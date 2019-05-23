#ifndef __WATCHNODE__
#define __WATCHNODE__

#include "winreglib.h"
#include <list>
#include <memory>
#include <mutex>
#include <uv.h>

namespace winreglib {

LOG_DEBUG_EXTERN_VARS

const DWORD filter = REG_NOTIFY_CHANGE_NAME |
					 REG_NOTIFY_CHANGE_ATTRIBUTES |
					 REG_NOTIFY_CHANGE_LAST_SET |
					 REG_NOTIFY_CHANGE_SECURITY;

/**
 * Represents a node in the watcher tree. Each node is responsible its corresponding registry key
 * and wiring up the change notification event.
 */
struct WatchNode {
	WatchNode() : env(NULL), hevent(NULL), hkey(NULL), name(L"ROOT"), parent(NULL) {}
	WatchNode(napi_env env) : env(env), hevent(NULL), hkey(NULL), name(L"ROOT"), parent(NULL) {}
	WatchNode(napi_env env, const std::wstring& name, HKEY hkey) : env(env), hevent(NULL), hkey(hkey), name(name), parent(NULL) {}

	WatchNode(napi_env env, const std::wstring& name, std::shared_ptr<WatchNode> parent) :
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

		load();
	}

	~WatchNode() {
		parent.reset();
		if (hevent) ::CloseHandle(hevent);
		if (hkey) ::RegCloseKey(hkey);
		std::lock_guard<std::mutex> lock(listenersLock);
		for (auto const& ref : listeners) ::napi_delete_reference(env, ref);
	}

	void addListener(napi_value listener) {
		napi_ref ref;
		if (::napi_create_reference(env, listener, 1, &ref) == napi_ok) {
			std::lock_guard<std::mutex> lock(listenersLock);
			listeners.push_back(ref);
		} else {
			napi_throw_error(env, NULL, "WatchNode::addListener: napi_create_reference failed");
		}
	}

	std::shared_ptr<WatchNode> addSubkey(const std::wstring& name, HKEY hkey) {
		std::shared_ptr<WatchNode> node = std::make_shared<WatchNode>(env, name, hkey);
		subkeys.insert(std::make_pair(name, node));
		return node;
	}

	std::shared_ptr<WatchNode> addSubkey(const std::wstring& name, std::shared_ptr<WatchNode> parent) {
		// note: parent == this, but we need the shared_ptr
		std::shared_ptr<WatchNode> node = std::make_shared<WatchNode>(env, name, parent);
		subkeys.insert(std::make_pair(name, node));
		return node;
	}

	void emit(const char* action) {
		// make a copy of listeners since the original list can be modified when a previous
		// listener callback has been called
		std::list<napi_ref> _listeners;
		{
			std::lock_guard<std::mutex> lock(listenersLock);
			if (listeners.empty()) {
				return;
			}
			_listeners = listeners;
		}

		LOG_DEBUG_2("WatchNode::emit", L"Calling \"%ls\" listeners: %lld", name.c_str(), _listeners.size())

		// construct the key
		std::wstring wkey = name;
		for (auto tmp = this->parent; tmp; tmp = tmp->parent) {
			wkey.insert(0, 1, '\\');
			wkey.insert(0, tmp->name);
		}
		std::u16string u16key(wkey.begin(), wkey.end());

		napi_handle_scope scope;
		napi_value global, resName, type, key, argv[2], listener, rval;
		napi_async_context ctx;
		napi_callback_scope callbackScope;

		NAPI_FATAL("WatchNode.emit", ::napi_open_handle_scope(env, &scope))
		NAPI_FATAL("WatchNode.emit", ::napi_get_global(env, &global))
		NAPI_FATAL("WatchNode.emit", ::napi_create_string_utf8(env, "winreglib.log", NAPI_AUTO_LENGTH, &resName))
		NAPI_FATAL("WatchNode.emit", ::napi_create_string_utf8(env, "change", NAPI_AUTO_LENGTH, &argv[0]))
		NAPI_FATAL("WatchNode.emit", ::napi_create_object(env, &argv[1]))
		NAPI_FATAL("WatchNode.emit", ::napi_create_string_utf16(env, u16key.c_str(), NAPI_AUTO_LENGTH, &key))
		NAPI_FATAL("WatchNode.emit", ::napi_create_string_utf8(env, action, NAPI_AUTO_LENGTH, &type))
		NAPI_FATAL("WatchNode.emit", ::napi_set_named_property(env, argv[1], "type", type))
		NAPI_FATAL("WatchNode.emit", ::napi_set_named_property(env, argv[1], "key", key))

		uint32_t count = 0;

		// fire the listener callback
		for (auto const& ref : _listeners) {
			NAPI_FATAL("WatchNode.emit", ::napi_get_reference_value(env, ref, &listener))
			if (listener != NULL) {
				// NAPI_FATAL("WatchNode.emit", ::napi_async_init(env, listener, resName, &ctx))
				// NAPI_FATAL("WatchNode.emit", ::napi_open_callback_scope(env, listener, ctx, &callbackScope))
				NAPI_FATAL("WatchNode.emit", ::napi_make_callback(env, NULL, global, listener, 2, argv, &rval))
				// NAPI_FATAL("WatchNode.emit", ::napi_close_callback_scope(env, callbackScope))
				// NAPI_FATAL("WatchNode.emit", ::napi_async_destroy(env, ctx))
			}
		}

		NAPI_FATAL("WatchNode.emit", ::napi_close_handle_scope(env, scope))
	}

	bool load(bool fromChange = false) {
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
				unload(fromChange);
				result = true;
			}
		} else {
			LOG_DEBUG_1("WatchNode::load", L"Opening \"%ls\"", name.c_str())
			LSTATUS status = ::RegOpenKeyExW(parent->hkey, name.c_str(), 0, KEY_NOTIFY, &hkey);
			if (status == ERROR_SUCCESS) {
				LOG_DEBUG_1("WatchNode::load", L"Key \"%ls\" was just created, registering watcher", name.c_str())

				watch(fromChange);

				for (auto const& it : subkeys) {
					if (it.second->load(fromChange)) {
						result = true;
					}
				}

				if (fromChange) {
					emit("add");
				}

				result = true;
			} else {
				hkey = NULL;
				LOG_DEBUG_WIN32_ERROR("WatchNode::load", L"RegOpenKeyExW failed: ", status)
			}
		}

		return result;
	}

	bool onChange() {
		// if our hkey is good, then a registry subkey or value was changed
		// if our hkey is bad, then this node's key has been deleted and we can assume the listeners have been notified

		bool changed = false;

		if (hkey) {
			if (watch(true)) {
				emit("change");
			}

			for (auto const& it : subkeys) {
				if (it.second->load(true)) {
					changed = true;
				}
			}
		}

		return changed;
	}

	void print(std::wstring& str, uint8_t indent = 0) {
		if (indent > 0) {
			for (uint8_t i = 1; i < indent; ++i) {
				str += L"  ";
			}
			str += hkey ? L"+ " : L"- ";
		}

		str += name + L" (" + std::to_wstring(listeners.size()) + L" listener" + (listeners.size() == 1 ? L")\n" : L"s)\n");

		for (auto const& it : subkeys) {
			it.second->print(str, indent + 1);
		}
	}

	void removeListener(napi_value listener) {
		std::lock_guard<std::mutex> lock(listenersLock);
		for (auto it = listeners.begin(); it != listeners.end(); ) {
			napi_value listener;
			NAPI_STATUS_THROWS(::napi_get_reference_value(env, *it, &listener))

			bool same;
			NAPI_STATUS_THROWS(::napi_strict_equals(env, listener, listener, &same))

			if (same) {
				LOG_DEBUG_1("WatchNode::removeListener", L"Removing listener from \"%ls\"", name.c_str())
				it = listeners.erase(it);
			} else {
				++it;
			}
		}
		LOG_DEBUG_2("WatchNode::removeListener", L"Node \"%ls\" now has %lld listeners", name.c_str(), listeners.size())
	}

	void unload(bool fromChange = false) {
		for (auto const& it : subkeys) {
			it.second->unload(fromChange);
		}

		if (hkey) {
			LOG_DEBUG_1("WatchNode::unload", L"Unloading \"%ls\" hkey", name.c_str())

			::RegCloseKey(hkey);
			hkey = NULL;

			if (fromChange) {
				emit("delete");
			}
		}
	}

	bool watch(bool fromChange = false) {
		if (hkey) {
			LSTATUS status = ::RegNotifyChangeKeyValue(hkey, FALSE, filter, hevent, TRUE);
			if (status == ERROR_SUCCESS) {
				return true;
			}
			if (status == 1018) {
				// key has been marked for deletion
				unload(fromChange);
			} else {
				LOG_DEBUG_WIN32_ERROR("WatchNode::watch", L"RegNotifyChangeKeyValue failed: ", status);
			}
		}
		return false;
	}

	napi_env env;
	std::wstring name;
	HANDLE hevent;
	HKEY hkey;
	std::shared_ptr<WatchNode> parent;
	std::mutex listenersLock;
	std::list<napi_ref> listeners;
	std::map<std::wstring, std::shared_ptr<WatchNode>> subkeys;
};

}

#endif
