#include "watchman.h"
#include <list>
#include <node_api.h>
#include <sstream>

using namespace winreglib;

static void execute(napi_env env, void* data) {
	LOG_DEBUG("execute", L"Calling run()");
	((Watchman*)data)->run();
	LOG_DEBUG("execute", L"run() returned!");
}

static void complete(napi_env env, napi_status status, void* data) {
	if (status != napi_ok) {
		const napi_extended_error_info* error;
		::napi_get_last_error_info(env, &error);
		char msg[1024];
		::snprintf(msg, 1024, "Watchman::complete: Worker thread failed (status=%d) %s", status, error->error_message);
	} else {
		LOG_DEBUG_1("Watchman::complete", L"Worker thread exited (status=%d)", status)
	}
}

/**
 * Creates the default signal events, initializes the subkeys in the watcher tree, initializes the
 * background thread, and wires up the notification callback when a registry change occurs.
 */
Watchman::Watchman(napi_env env) : env(env) {
	// create the built-in events for controlling the background thread
	term = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	refresh = ::CreateEvent(NULL, FALSE, FALSE, NULL);

	// initialize the root subkeys
	root = std::make_shared<WatchNode>(env);
	for (auto const& it : rootKeys) {
		root->addSubkey(it.first, it.second);
	}

	// initialize the background thread that waits for win32 events
	LOG_DEBUG_THREAD_ID("Watchman", L"Initializing async work")
	napi_value name;
	NAPI_THROW("Watchman", "ERR_NAPI_CREATE_STRING", ::napi_create_string_utf8(
		env,
		"winreglib.runloop",
		NAPI_AUTO_LENGTH,
		&name
	))
	NAPI_THROW("Watchman", "ERR_NAPI_CREATE_ASYNC_WORK", ::napi_create_async_work(
		env,
		NULL,
		name,
		execute,
		complete,
		this,
		&asyncWork
	))
	NAPI_THROW("Watchman", "ERR_NAPI_QUEUE_ASYNC_WORK", ::napi_queue_async_work(env, asyncWork))

	// wire up our dispatch change handler into Node's event loop, then unref it so that we don't
	// block Node from exiting
	uv_loop_t* loop;
	::napi_get_uv_event_loop(env, &loop);

	notifyChange = new uv_async_t;
	notifyChange->data = (void*)this;
	::uv_async_init(loop, notifyChange, [](uv_async_t* handle) {
		LOG_DEBUG("Watchman::Watchman", L"uv_async_t callback")
		if (handle && handle->data) {
			LOG_DEBUG("Watchman::Watchman", L"Calling dispatch()")
			((Watchman*)handle->data)->dispatch();
			LOG_DEBUG("Watchman::Watchman", L"dispatch() success!")
		} else {
			LOG_DEBUG("Watchman::Watchman", L"No data in handle! Watchman is no more!")
		}
	});
	::uv_unref((uv_handle_t*)notifyChange);
}

/**
 * Stops the background thread and closes all handles.
 */
Watchman::~Watchman() {
	LOG_DEBUG("Watchman::~Watchman", L"Stopping background thread")

	::uv_close(reinterpret_cast<uv_handle_t*>(notifyChange), [](uv_handle_t* handle) {
		uv_async_t* async = reinterpret_cast<uv_async_t*>(handle);
		delete async;
	});

	::SetEvent(term);
	::napi_delete_async_work(env, asyncWork);
	::CloseHandle(term);
	::CloseHandle(refresh);
}

/**
 * Constructs the watcher tree and adds the listener callback to the watched node. Starts the
 * background thread that waits for win32 to signal an event.
 */
void Watchman::config(const std::wstring& key, napi_value listener, WatchAction action) {
	if (action == Watch) {
		LOG_DEBUG_1("Watchman::config", L"Adding \"%ls\"", key.c_str())
	} else {
		LOG_DEBUG_1("Watchman::config", L"Removing \"%ls\"", key.c_str())
	}

	std::shared_ptr<WatchNode> node(root);
	std::wstring name;
	std::wstringstream wss(key);
	DWORD beforeCount = 0;
	DWORD afterCount = 0;

	{
		std::lock_guard<std::mutex> lock(activeLock);
		afterCount = beforeCount = active.size();
	}

	// parse the key while walking the watcher tree
	while (std::getline(wss, name, L'\\')) {
		auto it = node->subkeys.find(name);
		if (it == node->subkeys.end()) {
			// not found
			if (action == Watch) {
				// we're watching, so add the node
				node = node->addSubkey(name, node);
				++afterCount;
				std::lock_guard<std::mutex> lock(activeLock);
				active.push_back(std::weak_ptr<WatchNode>(node));
			} else {
				// node does not exist, nothing to remove
				LOG_DEBUG_1("Watchman::config", L"Node \"%ls\" does not exist", name.c_str())
				return;
			}
		} else {
			node = it->second;
		}
	}

	if (action == Watch) {
		// add the listener to the node
		node->addListener(listener);
	} else {
		// remove the listener from the node
		node->removeListener(listener);

		// prune the tree by blowing away an
		while (node->parent && node->listeners.size() == 0 && node->subkeys.size() == 0) {
			LOG_DEBUG_1("Watchman::config", L"Erasing node \"%ls\" from parent", node->name.c_str())

			// remove from the active list
			std::lock_guard<std::mutex> lock(activeLock);
			for (auto it = active.begin(); it != active.end(); ) {
				auto activeNode = (*it).lock();
				if (activeNode && activeNode != node) {
					++it;
				} else {
					it = active.erase(it);
					--afterCount;
				}
			}

			// remove the node from its parent's subkeys map
			node->parent->subkeys.erase(node->name);

			node = node->parent;
			LOG_DEBUG_2("Watchman::config", L"Parent \"%ls\" now has %ld subkeys", node->name.c_str(), (uint32_t)node->subkeys.size())
		}
	}

	// if (beforeCount == 0 && afterCount > 0) {
	// 	LOG_DEBUG_THREAD_ID("Watchman::config", L"Starting background thread")
	// 	NAPI_THROW("Watchman::config", "ERR_NAPI_QUEUE_ASYNC_WORK", ::napi_queue_async_work(env, asyncWork))
	// } else if (beforeCount > 0 && afterCount == 0) {
	// 	LOG_DEBUG_THREAD_ID("Watchman::config", L"Signaling term event")
	// 	::SetEvent(term);
	// } else if (beforeCount != afterCount) {
	// 	LOG_DEBUG_THREAD_ID("Watchman::config", L"Signaling refresh event")
	// 	::SetEvent(refresh);
	// }

	if ((beforeCount > 0 || afterCount > 0) && beforeCount != afterCount) {
		LOG_DEBUG_THREAD_ID("Watchman::config", L"Signaling refresh event")
		::SetEvent(refresh);
	}

	LOG_DEBUG("Watchman::config", L"Printing tree")
	printTree();
	LOG_DEBUG("Watchman::config", L"Done printing tree")
}

/**
 * Emits registry change events. This function is invoked by libuv on the main thread when a change
 * notification is sent from the background thread.
 */
void Watchman::dispatch() {
	LOG_DEBUG_THREAD_ID("Watchman::dispatch", L"Dispatching changes")

	while (1) {
		std::shared_ptr<WatchNode> node;
		DWORD remaining = 0;

		// check if there are any changed nodes left...
		// the first time we loop, we know there's at least one
		{
			LOG_DEBUG("Watchman::dispatch", L"Obtaining lock for changed nodes...")
			std::lock_guard<std::mutex> lock(changedNodesLock);

			if (changedNodes.empty()) {
				LOG_DEBUG("Watchman::dispatch", L"No changed nodes to dispatch, returning")
				return;
			}

			remaining = changedNodes.size();
			LOG_DEBUG_1("Watchman::dispatch", L"Found %ld changed nodes", remaining)
			node = changedNodes.front();
			LOG_DEBUG("Watchman::dispatch", L"Popping changed node...")
			changedNodes.pop_front();
			LOG_DEBUG("Watchman::dispatch", L"Popped changed node!")
		}

		LOG_DEBUG_2("Watchman::dispatch", L"Dispatching change event for \"%ls\" (%d remaining)", node->name.c_str(), --remaining)
		if (node->onChange()) {
			printTree();
		}
	}
}

/**
 * Prints the watcher tree for debugging.
 */
void Watchman::printTree() {
	std::wstringstream wss(L"");
	std::wstring line;
	root->print(wss);
	while (std::getline(wss, line, L'\n')) {
		WLOG_DEBUG("Watchman::printTree", line)
	}
}

/**
 * The background thread that waits for an event (e.g. registry change) to be signaled and notifies
 * the main thread of changed nodes to emit events for.
 */
void Watchman::run() {
	LOG_DEBUG_THREAD_ID("Watchman::run", L"Initializing run loop")

	HANDLE* handles = NULL;
	DWORD count = 0;
	DWORD idx = 0;
	std::vector<std::weak_ptr<WatchNode>> activeCopy;

	// make a copy of all active nodes since we need to preserve the order to know which node
	// changed based on the handle index
	{
		std::lock_guard<std::mutex> lock(activeLock);
		LOG_DEBUG_1("Watchman::run", L"Populating active copy (count=%ld)", (uint32_t)active.size())
		activeCopy = active;
	}

	while (1) {
		if (handles != NULL) {
			LOG_DEBUG("Watchman::run", L"Deleting handles 1")
			delete[] handles;
			LOG_DEBUG("Watchman::run", L"Deleted handles 1")
		}

		// WaitForMultipleObjects() wants an array of handles, so we must construct one
		idx = 0;
		count = activeCopy.size() + 2;
		handles = new HANDLE[count];
		handles[idx++] = term;
		handles[idx++] = refresh;
		for (auto it = activeCopy.begin(); it != activeCopy.end(); ) {
			if (auto node = (*it).lock()) {
				handles[idx++] = node->hevent;
				++it;
			} else {
				it = activeCopy.erase(it);
				--count;
			}
		}

		LOG_DEBUG_1("Watchman::run", L"Waiting on %ld objects...", count)
		DWORD result = ::WaitForMultipleObjects(count, handles, FALSE, INFINITE);
		LOG_DEBUG("Watchman::run", L"Done waiting on objects")

		if (result == WAIT_OBJECT_0) {
			// terminate
			LOG_DEBUG("Watchman::run", L"Received terminate signal")
			break;
		}

		if (result == WAIT_FAILED) {
			LOG_DEBUG_WIN32_ERROR("Watchman::run", L"WaitForMultipleObjects failed: ", ::GetLastError())
		} else {
			idx = result - WAIT_OBJECT_0;
			if (idx > 0 && idx < count) {
				if (result == WAIT_OBJECT_0 + 1) {
					// doing a refresh
					{
						std::lock_guard<std::mutex> lock(activeLock);
						LOG_DEBUG_1("Watchman::run", L"Refreshing %ld handles", (uint32_t)active.size())
						activeCopy = active;
					}
				} else {
					// something changed

					// first two indices are the term and refresh events, so skip those
					idx -= 2;

					if (auto node = activeCopy[idx].lock()) {
						LOG_DEBUG_2("Watchman::run", L"Detected change \"%ls\" [%ld]", node->name.c_str(), idx)

						{
							std::lock_guard<std::mutex> lock(changedNodesLock);
							if (std::find(changedNodes.begin(), changedNodes.end(), node) != changedNodes.end()) {
								LOG_DEBUG_1("Watchman::run", L"Node \"%ls\" is already in the changed list", node->name.c_str())
							} else {
								LOG_DEBUG_1("Watchman::run", L"Adding node \"%ls\" to the changed list", node->name.c_str())
								changedNodes.push_back(node);
							}
						}

						LOG_DEBUG("Watchman::run", L"Sending notification to main thread")
						::uv_async_send(notifyChange);
						LOG_DEBUG("Watchman::run", L"Done sending notification to main thread")
					}
				}
			}
		}
	}

	LOG_DEBUG("Watchman::run", L"Deleting handles 2")
	delete[] handles;
	LOG_DEBUG("Watchman::run", L"Deleted handles 2")
}
