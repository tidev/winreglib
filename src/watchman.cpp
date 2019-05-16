#include "watchman.h"
#include <node_api.h>
#include <sstream>

using namespace winreglib;

int64_t Watchman::add(napi_env env, const std::wstring& key, napi_value callback) {
	if (!thread) {
		LOG_DEBUG_THREAD_ID("Watchman::add(): Initializing thread")
		thread = new std::thread(&Watchman::run, this);
	}

	LOG_DEBUG(wprintf(L"Watchman::add(): Adding \"%ls\"\n", key.c_str()))

	Node* node = &root;
	std::wstring token;
	std::wstringstream wss(key);
	DWORD count = active.size();

	while (std::getline(wss, token, L'\\')) {
		auto it = node->subkeys.find(token);
		if (it == node->subkeys.end()) {
			node = new Node(env, token, node);
			std::lock_guard<std::mutex> lock(activeLock);
			active.push_back(node);
		} else {
			node = it->second;
		}
	}

	node->addListener(callback);

	if (active.size() != count) {
		::SetEvent(refresh);
	}

	::printf("Watchman::add() getting global\n");
	napi_value global;
	if (::napi_get_global(env, &global) == napi_ok) {
		::printf("Watchman::add() got global!\n");
	} else {
		::printf("Watchman::add() get global errored!\n");
	}

	// TODO: fix this
	int64_t id = nextId++;
	return id;
}

void Watchman::remove(int64_t id) {
	// TODO: STOP!
	printf("Stopping %lld\n", id);
}

void Watchman::run() {
	LOG_DEBUG_THREAD_ID("Watchman::run(): Initializing run loop")

	HANDLE* handles = NULL;
	DWORD count = 0;
	DWORD idx = 0;
	std::vector<Node*> activeCopy;

	{
		std::lock_guard<std::mutex> lock(activeLock);
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
		for (Node* node : activeCopy) {
			handles[idx++] = node->hevent;
		}

		LOG_DEBUG(printf("Watchman::run(): Waiting on %ld objects...\n", count))
		DWORD result = ::WaitForMultipleObjects(count, handles, FALSE, INFINITE);

		if (result == WAIT_OBJECT_0) {
			// terminate
			break;
		}

		if (result == WAIT_FAILED) {
			LOG_DEBUG_WIN32_ERROR("Watchman::run(): WaitForMultipleObjects failed: ", ::GetLastError())
		} else {
			idx = result - WAIT_OBJECT_0;
			if (idx > 0 && idx < count) {
				if (result == WAIT_OBJECT_0 + 1) {
					// doing a refresh
					LOG_DEBUG(::printf("Watchman::run(): Refreshing handles\n"));
					{
						std::lock_guard<std::mutex> lock(activeLock);
						activeCopy = active;
					}
				} else {
					// something changed!
					idx -= 2;
					LOG_DEBUG(::wprintf(L"Watchman::run(): %ls changed! (%ld)\n", activeCopy[idx]->name.c_str(), idx))

					// TODO: need to re-watch whatever fired

					{
						std::lock_guard<std::mutex> lock(changedNodesLock);
						changedNodes.push(activeCopy[idx]);
					}

					::uv_async_send(&notifyChange);
				}
			}
		}
	}

	delete[] handles;
	delete thread;
	thread = NULL;
}

void Watchman::dispatch() {
	Node* node;
	DWORD count;

	LOG_DEBUG_THREAD_ID("Watchman::dispatch(): Dispatching change")

	::printf("Watchman::dispatch() getting global\n");
	napi_value global;
	if (::napi_get_global(this->env, &global) == napi_ok) {
		::printf("Watchman::dispatch() got global!\n");
	} else {
		::printf("Watchman::dispatch() get global errored!\n");
	}

	while (1) {
		count = 0;

		{
			std::lock_guard<std::mutex> lock(changedNodesLock);
			if (changedNodes.empty()) {
				return;
			}

			count = changedNodes.size();
			node = changedNodes.front();
			changedNodes.pop();
		}

		LOG_DEBUG(::printf("Watchman::dispatch(): Pending changes = %d\n", count))

		napi_value global;
		LOG_NAPI_STATUS_ERROR(::napi_get_global(this->env, &global), "Watchman::dispatch(): Failed to get global object")

		printf("GOT GLOBAL!\n");

		napi_value rval;
		LOG_NAPI_STATUS_ERROR(::napi_create_object(node->env, &rval), "Watchman::dispatch(): Failed to create result object")

		std::wstring wkey = node->name;
		while (node = node->parent) {
			wkey.insert(0, 1, '\\');
			wkey.insert(0, node->name);
		}

		std::u16string u16key(wkey.begin(), wkey.end());
		napi_value key;
		LOG_NAPI_STATUS_ERROR(::napi_create_string_utf16(node->env, u16key.c_str(), NAPI_AUTO_LENGTH, &key), "Watchman::dispatch(): Failed to create key string")
		LOG_NAPI_STATUS_ERROR(::napi_set_named_property(node->env, rval, "key", key), "Watchman::dispatch(): Failed to set key property")

		for (auto const ref : node->listeners) {
			napi_value callback;
			::napi_get_reference_value(node->env, ref, &callback);
			LOG_NAPI_STATUS_ERROR(::napi_call_function(node->env, global, callback, 1, &rval, NULL), "Watchman::dispatch(): Failed to invoke callback")
		}
	}
}
