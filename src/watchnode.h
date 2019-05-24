#ifndef __WATCHNODE__
#define __WATCHNODE__

#include "winreglib.h"
#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>

namespace winreglib {

LOG_DEBUG_EXTERN_VARS

const DWORD filter = REG_NOTIFY_CHANGE_NAME |
					 REG_NOTIFY_CHANGE_ATTRIBUTES |
					 REG_NOTIFY_CHANGE_LAST_SET |
					 REG_NOTIFY_CHANGE_SECURITY;

#define PUSH_CALLBACK(list, evtType, key, listeners) \
	if (listeners.size() > 0) { \
		char* type = evtType; \
		(list).push(std::make_shared<Callback>(type, key, listeners)); \
	}

/**
 * Holds everything needed to emit a change event for a given node.
 */
struct Callback {
	Callback(char* type, std::u16string key, std::list<napi_ref> listeners) :
		type(type), key(key), listeners(listeners) {}

	char* type;
	std::u16string key;
	std::list<napi_ref> listeners;
};

/**
 * Datatype for passing the callback list around easier.
 */
typedef std::queue<std::shared_ptr<Callback>> CallbackQueue;

/**
 * Represents a node in the watcher tree. Each node is responsible its corresponding registry key
 * and wiring up the change notification event.
 */
class WatchNode {
public:
	WatchNode() : env(NULL), hevent(NULL), hkey(NULL), name(L"ROOT"), parent(NULL) {}
	WatchNode(napi_env env) : env(env), hevent(NULL), hkey(NULL), name(L"ROOT"), parent(NULL) {}
	WatchNode(napi_env env, const std::wstring& name, HKEY hkey) : env(env), hevent(NULL), hkey(hkey), name(name), parent(NULL) {}
	WatchNode(napi_env env, const std::wstring& name, std::shared_ptr<WatchNode> parent);
	~WatchNode();

	void addListener(napi_value listener);
	std::shared_ptr<WatchNode> addSubkey(const std::wstring& name, HKEY hkey);
	std::shared_ptr<WatchNode> addSubkey(const std::wstring& name, std::shared_ptr<WatchNode> parent);
	bool onChange();
	void print(std::wstringstream& wss, uint8_t indent = 0);
	void removeListener(napi_value listener);

private:
	std::u16string getKey();
	bool load(CallbackQueue* pending);
	void unload(CallbackQueue* pending);
	bool watch(CallbackQueue* pending);

public:
	HANDLE hevent;
	std::wstring name;
	std::map<std::wstring, std::shared_ptr<WatchNode>> subkeys;
	std::shared_ptr<WatchNode> parent;
	std::list<napi_ref> listeners;

private:
	napi_env env;
	HKEY hkey;
	std::mutex listenersLock;
};

}

#endif
