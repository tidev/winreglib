#include "watchnode.h"
#include "winreglib.h"
#include <uv.h>

using namespace winreglib;

/**
 * Creates the event handle for when this node registers for change notifications and attempts to
 * load the registry key.
 */
WatchNode::WatchNode(napi_env env, const std::wstring& name, std::shared_ptr<WatchNode> parent) :
	env(env),
	hevent(NULL),
	hkey(NULL),
	name(name),
	parent(parent)
{
	hevent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	if (hevent == NULL) {
		LOG_DEBUG_WIN32_ERROR("WatchNode", L"CreateEvent failed: ", ::GetLastError())
	}

	load(NULL);
}

/**
 * Shutsdown the node.
 */
WatchNode::~WatchNode() {
	LOG_DEBUG_1("WatchNode::~WatchNode", L"Destroying node \"%ls\"", name.c_str())
	parent.reset();
	if (hevent) ::CloseHandle(hevent);
	if (hkey) ::RegCloseKey(hkey);
	std::lock_guard<std::mutex> lock(listenersLock);
	for (auto const& ref : listeners) ::napi_delete_reference(env, ref);
}

/**
 * Adds a JS listener function to the list.
 */
void WatchNode::addListener(napi_value listener) {
	napi_ref ref;
	if (::napi_create_reference(env, listener, 1, &ref) == napi_ok) {
		std::lock_guard<std::mutex> lock(listenersLock);
		listeners.push_back(ref);
	} else {
		napi_throw_error(env, NULL, "WatchNode::addListener: napi_create_reference failed");
	}
}

/**
 * Adds a subkey by name and the key handle. This is used to wire up the built-in root keys.
 */
std::shared_ptr<WatchNode> WatchNode::addSubkey(const std::wstring& name, HKEY hkey) {
	std::shared_ptr<WatchNode> node = std::make_shared<WatchNode>(env, name, hkey);
	subkeys.insert(std::make_pair(name, node));
	return node;
}

/**
 * Adds a subkey by name.
 */
std::shared_ptr<WatchNode> WatchNode::addSubkey(const std::wstring& name, std::shared_ptr<WatchNode> parent) {
	// note: parent == this, but we need the shared_ptr
	std::shared_ptr<WatchNode> node = std::make_shared<WatchNode>(env, name, parent);
	subkeys.insert(std::make_pair(name, node));
	return node;
}

/**
 * Walks parent nodes to construct the full key.
 */
std::u16string WatchNode::getKey() {
	std::wstring wkey = name;
	for (auto tmp = parent; tmp; tmp = tmp->parent) {
		wkey.insert(0, 1, '\\');
		wkey.insert(0, tmp->name);
	}
	std::u16string u16key(wkey.begin(), wkey.end());
	return u16key;
}

/**
 * Attempts to open this node's registry key and watch it.
 */
bool WatchNode::load(CallbackQueue* pending) {
	bool result = false;

	if (!parent) {
		return result;
	}

	if (hkey) {
		// verify this node is still valid
		HKEY tmp;
		LSTATUS status = ::RegOpenKeyW(parent->hkey, name.c_str(), &tmp);
		if (status == ERROR_SUCCESS) {
			// we're still good
			::RegCloseKey(tmp);
			LOG_DEBUG_1("WatchNode::load", L"\"%ls\" hkey is still valid", name.c_str())
		} else {
			// no longer valid!
			LOG_DEBUG_1("WatchNode::load", L"\"%ls\" hkey is no longer valid", name.c_str())
			unload(pending);
			result = true;
		}
	} else {
		LOG_DEBUG_1("WatchNode::load", L"Opening \"%ls\"", name.c_str())
		LSTATUS status = ::RegOpenKeyExW(parent->hkey, name.c_str(), 0, KEY_NOTIFY, &hkey);
		if (status == ERROR_SUCCESS) {
			LOG_DEBUG_1("WatchNode::load", L"Key \"%ls\" was just created, registering watcher", name.c_str())

			watch(pending);

			if (pending) {
				PUSH_CALLBACK(*pending, "add", getKey(), listeners);
			}

			for (auto const& it : subkeys) {
				if (it.second->load(pending)) {
					result = true;
				}
			}

			result = true;
		} else {
			hkey = NULL;
			LOG_DEBUG_WIN32_ERROR("WatchNode::load", L"RegOpenKeyExW failed: ", status)
		}
	}

	return result;
}

/**
 * This function is called on the main thread when a registry key changes. It rewatches the changed
 * key and walks the subkeys to see if any subkeys were added or deleted.
 */
bool WatchNode::onChange() {
	// if our hkey is good, then a registry subkey or value was changed
	//
	// if our hkey is bad, then this node's key has been deleted and we can assume the listeners
	// have been notified

	bool changed = false;
	CallbackQueue pending;

	if (hkey) {
		if (watch(&pending)) {
			PUSH_CALLBACK(pending, "change", getKey(), listeners);
		}

		for (auto const& it : subkeys) {
			if (it.second->load(&pending)) {
				changed = true;
			}
		}
	}

	// this node and it's subkeys are processed, now fire the callbacks for each change we discovered

	napi_handle_scope scope;
	napi_value global, type, key, argv[2], listener, rval;

	NAPI_THROW_RETURN("WatchNode::onChange", "ERR_NAPI_OPEN_HANDLE_SCOPE", ::napi_open_handle_scope(env, &scope), false)
	NAPI_THROW_RETURN("WatchNode::onChange", "ERR_NAPI_GET_GLOBAL", ::napi_get_global(env, &global), false)
	NAPI_THROW_RETURN("WatchNode::onChange", "ERR_NAPI_CREATE_STRING", ::napi_create_string_utf8(env, "change", NAPI_AUTO_LENGTH, &argv[0]), false)
	NAPI_THROW_RETURN("WatchNode::onChange", "ERR_NAPI_CREATE_OBJECT", ::napi_create_object(env, &argv[1]), false)

	LOG_DEBUG_1("WatchNode::onChange", L"Calling %lld callbacks", pending.size())

	while (!pending.empty()) {
		auto cb = pending.front();
		pending.pop();

		NAPI_THROW_RETURN("WatchNode::onChange", "ERR_NAPI_CREATE_STRING", ::napi_create_string_utf8(env, cb->type, NAPI_AUTO_LENGTH, &type), false)
		NAPI_THROW_RETURN("WatchNode::onChange", "ERR_NAPI_CREATE_STRING", ::napi_create_string_utf16(env, cb->key.c_str(), NAPI_AUTO_LENGTH, &key), false)
		NAPI_THROW_RETURN("WatchNode::onChange", "ERR_NAPI_SET_NAMED_PROPERTY", ::napi_set_named_property(env, argv[1], "type", type), false)
		NAPI_THROW_RETURN("WatchNode::onChange", "ERR_NAPI_SET_NAMED_PROPERTY", ::napi_set_named_property(env, argv[1], "key", key), false)

		LOG_DEBUG_1("WatchNode::onChange", L"Calling %lld listeners", cb->listeners.size())

		for (auto const& ref : cb->listeners) {
			NAPI_THROW_RETURN("WatchNode::onChange", "ERR_NAPI_GET_REFERENCE_VALUE", ::napi_get_reference_value(env, ref, &listener), false)
			if (listener != NULL) {
				NAPI_THROW_RETURN("WatchNode::onChange", "ERROR_NAPI_MAKE_CALLBACK", ::napi_make_callback(env, NULL, global, listener, 2, argv, &rval), false)
			}
		}
	}

	NAPI_THROW_RETURN("WatchNode::onChange", "ERR_NAPI_CLOSE_HANDLE_SCOPE", ::napi_close_handle_scope(env, scope), false)

	return changed;
}

/**
 * Prints this node's info and calls its subkeys to print their info.
 */
void WatchNode::print(std::wstringstream& wss, uint8_t indent) {
	if (indent > 0) {
		for (uint8_t i = 1; i < indent; ++i) {
			wss << L"  ";
		}
		if (hkey) {
			wss << L"+ ";
		} else {
			wss << L"- ";
		}
	}

	wss << name << " (" << std::to_wstring(listeners.size()) << " listener" << (listeners.size() == 1 ? ")\n" : "s)\n");

	for (auto const& it : subkeys) {
		it.second->print(wss, indent + 1);
	}
}

/**
 * Removes a JS listener function from the list.
 */
void WatchNode::removeListener(napi_value listener) {
	std::lock_guard<std::mutex> lock(listenersLock);
	for (auto it = listeners.begin(); it != listeners.end(); ) {
		napi_value listener;
		NAPI_THROW("WatchNode::removeListener", "ERR_NAPI_GET_REFERENCE_VALUE", ::napi_get_reference_value(env, *it, &listener))

		bool same;
		NAPI_THROW("WatchNode::removeListener", "ERR_NAPI_STRICT_EQUALS", ::napi_strict_equals(env, listener, listener, &same))

		if (same) {
			LOG_DEBUG_1("WatchNode::removeListener", L"Removing listener from \"%ls\"", name.c_str())
			it = listeners.erase(it);
		} else {
			++it;
		}
	}
	LOG_DEBUG_2("WatchNode::removeListener", L"Node \"%ls\" now has %lld listeners", name.c_str(), listeners.size())
}

/**
 * Closes this node's registry key handle and its subkey's key handles.
 */
void WatchNode::unload(CallbackQueue* pending) {
	for (auto const& it : subkeys) {
		it.second->unload(pending);
	}

	if (hkey) {
		LOG_DEBUG_1("WatchNode::unload", L"Unloading \"%ls\" hkey", name.c_str())

		::RegCloseKey(hkey);
		hkey = NULL;

		if (pending) {
			PUSH_CALLBACK(*pending, "delete", getKey(), listeners);
		}
	}
}

/**
 * Wires up the change notification event.
 */
bool WatchNode::watch(CallbackQueue* pending) {
	if (hkey) {
		LSTATUS status = ::RegNotifyChangeKeyValue(hkey, FALSE, filter, hevent, TRUE);
		if (status == ERROR_SUCCESS) {
			return true;
		}
		if (status == 1018) {
			// key has been marked for deletion
			unload(pending);
		} else {
			LOG_DEBUG_WIN32_ERROR("WatchNode::watch", L"RegNotifyChangeKeyValue failed: ", status);
		}
	}
	return false;
}
