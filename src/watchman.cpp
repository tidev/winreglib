#include "watchman.h"
#include <node_api.h>
#include <sstream>

using namespace winreglib;

/**
 * Creates the default signal events, initializes the subkeys in the watcher tree, initializes the
 * background thread, and wires up the notification callback when a registry change occurs.
 */
Watchman::Watchman(napi_env env) : env(env) {
	term = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	refresh = ::CreateEvent(NULL, FALSE, FALSE, NULL);

	root = std::make_shared<WatchNode>(env);
	for (auto const& it : rootKeys) {
		root->addSubkey(it.first, it.second);
	}

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
void Watchman::config(const std::wstring& key, napi_value emit, WatchAction action) {
	char* ns = action == Watch ? "Watchman::add" : "Watchman::remove";
	if (action == Watch) {
		LOG_DEBUG_1(ns, L"Adding \"%ls\"", key.c_str())
	} else {
		LOG_DEBUG_1(ns, L"Removing \"%ls\"", key.c_str())
	}

	std::shared_ptr<WatchNode> node = root;
	std::wstring name;
	std::wstringstream wss(key);
	DWORD beforeCount = 0;
	DWORD afterCount = 0;

	{
		std::lock_guard<std::mutex> lock(activeLock);
		afterCount = beforeCount = active.size();
	}

	while (std::getline(wss, name, L'\\')) {
		auto it = node->subkeys.find(name);
		if (it == node->subkeys.end()) {
			if (action == Watch) {
				node = node->addSubkey(name, node);
				++afterCount;
				std::lock_guard<std::mutex> lock(activeLock);
				active.push_back(std::weak_ptr<WatchNode>(node));
			} else {
				// node does not exist, nothing to remove
				LOG_DEBUG_1(ns, L"Node \"%ls\" does not exist", name.c_str())
				return;
			}
		} else {
			node = it->second;
		}
	}

	if (action == Watch) {
		node->addListener(emit);
	} else {
		node->removeListener(emit);

		while (node->parent && node->listeners.size() == 0 && node->subkeys.size() == 0) {
			LOG_DEBUG_1(ns, L"Erasing node \"%ls\" from parent", node->name.c_str())

			std::shared_ptr<WatchNode> parent = node->parent;
			parent->subkeys.erase(node->name);

			// remove from active list
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

			node = parent;
		}
	}

	if (beforeCount == 0 && afterCount > 0) {
		LOG_DEBUG_THREAD_ID(ns, L"Starting background thread")
		NAPI_STATUS_THROWS(::napi_queue_async_work(env, asyncWork))
	} else if (beforeCount > 0 && afterCount == 0) {
		LOG_DEBUG_THREAD_ID(ns, L"Signalling term event")
		::SetEvent(term);
	} else if (beforeCount != afterCount) {
		LOG_DEBUG_THREAD_ID(ns, L"Signalling refresh event")
		::SetEvent(refresh);
	}
}

/**
 * Emits debug log messages. This function is invoked by libuv on the main thread when a new log
 * message has arrived.
 */
void Watchman::dispatch() {
	LOG_DEBUG_THREAD_ID("Watchman::dispatch", L"Dispatching changes")

	napi_handle_scope scope;
	NAPI_FATAL("Watchman::dispatch", ::napi_open_handle_scope(env, &scope))

	napi_value global;
	NAPI_FATAL("Watchman::dispatch", ::napi_get_global(env, &global))

	while (1) {
		std::weak_ptr<WatchNode> weak;
		DWORD count = 0;

		{
			std::lock_guard<std::mutex> lock(changedNodesLock);
			if (changedNodes.empty()) {
				break;
			}

			count = changedNodes.size();
			weak = changedNodes.front();
			changedNodes.pop_front();
		}

		auto node = weak.lock();
		if (!node) {
			continue;
		}

		LOG_DEBUG_1("Watchman::dispatch", L"Pending changes = %d", count)
		LOG_DEBUG_1("Watchman::dispatch", L"Node = \"%ls\"", node->name.c_str())

		napi_value argv[2];
		NAPI_FATAL("Watchman::dispatch", ::napi_create_string_utf8(env, "change", NAPI_AUTO_LENGTH, &argv[0]))
		NAPI_FATAL("Watchman::dispatch", ::napi_create_object(env, &argv[1]))

		std::wstring wkey = node->name;
		std::shared_ptr<WatchNode> tmp(node);
		while (tmp = tmp->parent) {
			wkey.insert(0, 1, '\\');
			wkey.insert(0, tmp->name);
		}

		std::u16string u16key(wkey.begin(), wkey.end());
		napi_value key;

		NAPI_FATAL("Watchman::dispatch", ::napi_create_string_utf16(env, u16key.c_str(), NAPI_AUTO_LENGTH, &key))
		NAPI_FATAL("Watchman::dispatch", ::napi_set_named_property(env, argv[1], "key", key))

		for (napi_ref ref : node->listeners) {
			napi_value emit;
			::napi_get_reference_value(env, ref, &emit);
			napi_value rval;
			NAPI_FATAL("Watchman::dispatch", ::napi_make_callback(env, NULL, global, emit, 2, argv, &rval))
		}
	}

	::napi_close_handle_scope(env, scope);
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

	{
		std::lock_guard<std::mutex> lock(activeLock);
		LOG_DEBUG_1("Watchman::run", L"Populating active copy (count=%lld)", active.size())
		activeCopy = active;
	}

	while (1) {
		count = activeCopy.size() + 2;
		if (handles != NULL) {
			delete[] handles;
		}
		handles = new HANDLE[count];

		idx = 0;
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
							bool found = false;

							for (auto weak : changedNodes) {
								auto changedNode = weak.lock();
								if (changedNode && changedNode == node) {
									found = true;
									break;
								}
							}

							if (!found) {
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
