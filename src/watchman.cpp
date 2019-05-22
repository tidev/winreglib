#include "watchman.h"
#include <list>
#include <node_api.h>
#include <sstream>

using namespace winreglib;

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
	napi_value asyncWorkName;
	NAPI_STATUS_THROWS(::napi_create_string_utf8(env, "winreglib.runloop", NAPI_AUTO_LENGTH, &asyncWorkName));
	NAPI_STATUS_THROWS(::napi_create_async_work(
		env,
		NULL,
		asyncWorkName,
		[](napi_env env, void* data) { ((Watchman*)data)->run(); },
		[](napi_env env, napi_status status, void* data) { LOG_DEBUG("Watchman::complete", L"Worker thread exited") },
		this,
		&asyncWork
	));

	// wire up our dispatch change handler into Node's event loop, then unref it so that we don't
	// block Node from exiting
	uv_loop_t* loop;
	::napi_get_uv_event_loop(env, &loop);
	notifyChange.data = this;
	::uv_async_init(loop, &notifyChange, [](uv_async_t* handle) { ((Watchman*)handle->data)->dispatch(); });
	::uv_unref((uv_handle_t*)&notifyChange);
}

/**
 * Stops the background thread and closes all handles.
 */
Watchman::~Watchman() {
	::SetEvent(term);
	::napi_delete_async_work(env, asyncWork);
	::CloseHandle(term);
	::CloseHandle(refresh);
	::uv_close((uv_handle_t*)&notifyChange, NULL);
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
			LOG_DEBUG_2("Watchman::config", L"Parent \"%ls\" now has %lld subkeys", node->name.c_str(), node->subkeys.size())
		}
	}

	if (beforeCount == 0 && afterCount > 0) {
		LOG_DEBUG_THREAD_ID("Watchman::config", L"Starting background thread")
		NAPI_STATUS_THROWS(::napi_queue_async_work(env, asyncWork))
	} else if (beforeCount > 0 && afterCount == 0) {
		LOG_DEBUG_THREAD_ID("Watchman::config", L"Signalling term event")
		::SetEvent(term);
	} else if (beforeCount != afterCount) {
		LOG_DEBUG_THREAD_ID("Watchman::config", L"Signalling refresh event")
		::SetEvent(refresh);
	}

	// print the node tree for debugging
	std::wstring str;
	root->print(str);
	str.resize(str.length() - 1);
	LOG_DEBUG("Watchman::config", str.c_str())
}

/**
 * Emits debug log messages. This function is invoked by libuv on the main thread when a new log
 * message has arrived.
 */
void Watchman::dispatch() {
	LOG_DEBUG_THREAD_ID("Watchman::dispatch", L"Dispatching changes")

	DWORD count = 0;
	napi_handle_scope scope;
	napi_value global, key, argv[2], listener, rval;

	// initialize the scope and variables that won't change
	NAPI_FATAL("Watchman::dispatch", ::napi_open_handle_scope(env, &scope))
	NAPI_FATAL("Watchman::dispatch", ::napi_get_global(env, &global))
	NAPI_FATAL("Watchman::dispatch", ::napi_create_string_utf8(env, "change", NAPI_AUTO_LENGTH, &argv[0]))
	NAPI_FATAL("Watchman::dispatch", ::napi_create_object(env, &argv[1]))

	while (1) {
		std::shared_ptr<WatchNode> node;
		count = 0;

		// check if there are any changed nodes left...
		// the first time we loop, we know there's at least one
		{
			std::lock_guard<std::mutex> lock(changedNodesLock);

			if (changedNodes.empty()) {
				break;
			}

			count = changedNodes.size();
			node = changedNodes.front();
			changedNodes.pop_front();
		}

		LOG_DEBUG_2("Watchman::dispatch", L"Dispatching change event for \"%ls\" (%d pending)", node->name.c_str(), --count)

		// construct the key
		std::wstring wkey = node->name;
		std::shared_ptr<WatchNode> tmp(node);
		while (tmp = tmp->parent) {
			wkey.insert(0, 1, '\\');
			wkey.insert(0, tmp->name);
		}
		std::u16string u16key(wkey.begin(), wkey.end());

		NAPI_FATAL("Watchman::dispatch", ::napi_create_string_utf16(env, u16key.c_str(), NAPI_AUTO_LENGTH, &key))
		NAPI_FATAL("Watchman::dispatch", ::napi_set_named_property(env, argv[1], "key", key))

		// make a copy of listeners since the original list can be modified when a previous
		// listener callback has been called
		std::list<napi_ref> listeners;
		{
			std::lock_guard<std::mutex> lock(node->listenersLock);
			listeners = node->listeners;
		}

		// fire the listener callback
		for (auto const& ref : listeners) {
			NAPI_FATAL("Watchman::dispatch", ::napi_get_reference_value(env, ref, &listener))
			if (listener != NULL) {
				NAPI_FATAL("Watchman::dispatch", ::napi_make_callback(env, NULL, global, listener, 2, argv, &rval))
			}
		}
	}

	NAPI_FATAL("Watchman::dispatch", ::napi_close_handle_scope(env, scope))
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
		LOG_DEBUG_1("Watchman::run", L"Populating active copy (count=%lld)", active.size())
		activeCopy = active;
	}

	while (1) {
		if (handles != NULL) {
			delete[] handles;
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
						LOG_DEBUG_1("Watchman::run", L"Refreshing %lld handles", active.size())
						activeCopy = active;
					}
				} else {
					// something changed

					// first two indices are the term and refresh events, so skip those
					idx -= 2;

					if (auto node = activeCopy[idx].lock()) {
						LOG_DEBUG_2("Watchman::run", L"Detected change \"%ls\" [%ld]", node->name.c_str(), idx)
						node->watch();

						{
							std::lock_guard<std::mutex> lock(changedNodesLock);
							if (std::find(changedNodes.begin(), changedNodes.end(), node) != changedNodes.end()) {
								LOG_DEBUG_1("Watchman::run", L"Node \"%ls\" is already in the changed list", node->name.c_str())
							} else {
								LOG_DEBUG_1("Watchman::run", L"Adding node \"%ls\" to the changed list", node->name.c_str())
								changedNodes.push_back(node);
							}
						}

						::uv_async_send(&notifyChange);
					}
				}
			}
		}
	}

	delete[] handles;
}
