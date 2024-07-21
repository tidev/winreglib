#ifndef __WATCHMAN__
#define __WATCHMAN__

#include "winreglib.h"
#include "watchnode.h"
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <uv.h>
#include <vector>

namespace winreglib {

LOG_DEBUG_EXTERN_VARS
extern const std::map<std::wstring, HKEY> rootKeys;

enum WatchAction { Watch, Unwatch };

/**
 * Maintains state for the nodes being watched and emits change events.
 */
class Watchman {
public:
	Watchman(napi_env env);
	~Watchman();

	void config(const std::wstring& key, napi_value listener, WatchAction action);
	void run();
	void startWorker();
	void stopWorker();

private:
	void dispatch();
	void printTree();

	napi_env env;
	napi_async_work asyncWork;
	std::shared_ptr<WatchNode> root;
	HANDLE term;
	HANDLE refresh;
	std::mutex activeLock;
	std::vector<std::weak_ptr<WatchNode>> active;
	uv_async_t* notifyChange;
	std::deque<std::shared_ptr<WatchNode>> changedNodes;
	std::mutex changedNodesLock;
};

}

#endif
