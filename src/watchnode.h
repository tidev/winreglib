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
		LOG_DEBUG_1("WatchNode", L"Opening \"%ls\"", name.c_str())

		LSTATUS status = ::RegOpenKeyExW(parent->hkey, name.c_str(), 0, KEY_NOTIFY, &hkey);
		if (status != ERROR_SUCCESS) {
			LOG_DEBUG_WIN32_ERROR("WatchNode", L"RegOpenKeyExW failed: ", status)
		}

		hevent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
		if (hevent == NULL) {
			LOG_DEBUG_WIN32_ERROR("WatchNode", L"CreateEvent failed: ", ::GetLastError())
		}

		watch();
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
			napi_throw_error(env, NULL, "WatchNode(): napi_create_reference failed");
		}
	}

	void print(std::wstring& str, uint8_t indent = 0) {
		if (indent > 0) {
			for (uint8_t i = 1; i < indent; ++i) {
				str += L"  ";
			}
			str += L"- ";
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

	void watch() {
		if (hkey) {
			LSTATUS status = ::RegNotifyChangeKeyValue(hkey, FALSE, filter, hevent, TRUE);
			if (status == 1018) {
				// key has been marked for deletion
				::RegCloseKey(hkey);
				hkey = NULL;
			} else {
				LOG_DEBUG_WIN32_ERROR("WatchNode", L"RegNotifyChangeKeyValue failed: ", status);
			}
		}
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
