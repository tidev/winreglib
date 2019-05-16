#ifndef __WATCHMAN__
#define __WATCHMAN__

#include "winreglib.h"
#include <list>
#include <mutex>
#include <queue>
#include <thread>
#include <uv.h>
#include <vector>

namespace winreglib {

extern const std::map<std::wstring, HKEY> rootKeys;

const DWORD filter = REG_NOTIFY_CHANGE_NAME |
                     REG_NOTIFY_CHANGE_ATTRIBUTES |
                     REG_NOTIFY_CHANGE_LAST_SET |
                     REG_NOTIFY_CHANGE_SECURITY;

struct Node {
	Node() : hevent(NULL), hkey(NULL), name(L"ROOT"), parent(NULL) {}

	Node(napi_env env2, const std::wstring& name, Node* parent) :
		env(env2),
		hevent(NULL),
		hkey(NULL),
		name(name),
		parent(parent)
	{
		parent->subkeys[name] = this;

		LOG_DEBUG(wprintf(L"Opening \"%ls\"\n", name.c_str()));

		LSTATUS status = ::RegOpenKeyExW(parent->hkey, name.c_str(), 0, KEY_NOTIFY, &hkey);
		if (status != ERROR_SUCCESS) {
			LOG_DEBUG_WIN32_ERROR("Node(): RegOpenKeyExW failed: ", status)
		}

		hevent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
		if (hevent == NULL) {
			LOG_DEBUG_WIN32_ERROR("Node(): CreateEvent failed: ", ::GetLastError())
		}

		if (hkey) {
			LSTATUS status = ::RegNotifyChangeKeyValue(hkey, FALSE, filter, hevent, TRUE);
			LOG_DEBUG_WIN32_ERROR("Node(): RegNotifyChangeKeyValue failed: ", status);
		}

		::printf("Node() named key getting global\n");
		napi_value global;
		if (::napi_get_global(env, &global) == napi_ok) {
			::printf("Node() named key got global!\n");
		} else {
			::printf("Node() named key get global errored!\n");
		}
	}

	Node(napi_env env, const std::wstring& name, HKEY hkey) :
		env(env),
		hevent(NULL),
		hkey(hkey),
		name(name),
		parent(NULL)
	{
		::printf("Node() root key getting global\n");
		napi_value global;
		if (::napi_get_global(env, &global) == napi_ok) {
			::printf("Node() root key got global!\n");
		} else {
			::printf("Node() root key get global errored!\n");
		}
	}

	~Node() {
		if (this->hevent) {
			::CloseHandle(this->hevent);
		}

		if (this->hkey) {
			::RegCloseKey(this->hkey);
		}

		for (auto const& it : this->subkeys) {
			delete it.second;
		}

		for (auto const ref : listeners) {
			::napi_delete_reference(env, ref);
		}
	}

	void addListener(napi_value callback) {
		napi_ref ref;
		if (::napi_create_reference(env, callback, 1, &ref) == napi_ok) {
			listeners.push_back(ref);
		} else {
			napi_throw_error(env, NULL, "napi_create_reference failed");
		}
	}

	napi_env env;
	std::wstring name;
	HANDLE hevent;
	HKEY hkey;
	Node* parent;
	std::list<napi_ref> listeners;
	std::map<std::wstring, Node*> subkeys;
};

class Watchman {
public:
	Watchman(napi_env env2) : env(env2), nextId(0), thread(NULL) {
		term = ::CreateEvent(NULL, FALSE, FALSE, NULL);
		refresh = ::CreateEvent(NULL, FALSE, FALSE, NULL);

		for (auto const& it : rootKeys) {
			root.subkeys[it.first] = new Node(env, it.first, it.second);
		}

		uv_loop_t* loop;
		::napi_get_uv_event_loop(env, &loop);
		notifyChange.data = this;
		::uv_async_init(loop, &notifyChange, Watchman::dispatchHelper);

		::printf("Watchman() getting global\n");
		napi_value global;
		if (::napi_get_global(env, &global) == napi_ok) {
			::printf("Watchman() got global!\n");
		} else {
			::printf("Watchman() get global errored!\n");
		}
	}

	~Watchman() {
		::SetEvent(term);

		if (thread) {
			if (thread->joinable()) {
				thread->join();
			}
			delete thread;
		}

		::CloseHandle(term);
		::CloseHandle(refresh);

		::uv_close((uv_handle_t*)&notifyChange, NULL);
	}

	int64_t add(napi_env env, const std::wstring& key, napi_value callback);
	void remove(int64_t id);
	void run();
	void dispatch();

	static void dispatchHelper(uv_async_t* handle) {
		Watchman* self = (Watchman*)handle->data;
		self->dispatch();
	}

private:
	napi_env env;
	Node root;
	HANDLE term;
	HANDLE refresh;
	std::mutex activeLock;
	std::vector<Node*> active;
	std::thread* thread;
	uv_async_t notifyChange;
	std::queue<Node*> changedNodes;
	std::mutex changedNodesLock;

	int64_t nextId;
};

}

#endif
