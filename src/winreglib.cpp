#include "winreglib.h"
#include "watchman.h"
#include <windows.h>
#include <memory>

namespace winreglib {
	winreglib::Watchman* watchman = NULL;

	const std::map<std::wstring, HKEY> rootKeys = {
		{ L"HKEY_CLASSES_ROOT",                HKEY_CLASSES_ROOT },
		{ L"HKEY_CURRENT_CONFIG",              HKEY_CURRENT_CONFIG },
		{ L"HKEY_CURRENT_USER",                HKEY_CURRENT_USER },
		{ L"HKEY_CURRENT_USER_LOCAL_SETTINGS", HKEY_CURRENT_USER_LOCAL_SETTINGS },
		{ L"HKEY_LOCAL_MACHINE",               HKEY_LOCAL_MACHINE },
		{ L"HKEY_PERFORMANCE_DATA",            HKEY_PERFORMANCE_DATA },
		{ L"HKEY_PERFORMANCE_NLSTEXT",         HKEY_PERFORMANCE_NLSTEXT },
		{ L"HKEY_PERFORMANCE_TEXT",            HKEY_PERFORMANCE_TEXT },
		{ L"HKEY_USERS",                       HKEY_USERS }
	};

	const std::map<std::wstring, std::wstring> rootMap = {
		{ L"HKCR", L"HKEY_CLASSES_ROOT" },
		{ L"HKCC", L"HKEY_CURRENT_CONFIG" },
		{ L"HKCU", L"HKEY_CURRENT_USER" },
		{ L"HKLM", L"HKEY_LOCAL_MACHINE" },
		{ L"HKU",  L"HKEY_USERS" }
	};

	std::wstring* resolveRootName(std::wstring& key) {
		auto it = rootMap.find(key);
		auto it2 = rootKeys.find(it == rootMap.end() ? key : it->second);
		return it2 == rootKeys.end() ? NULL : (std::wstring*)&it2->first;
	}

	HKEY resolveRootKey(napi_env env, std::wstring& key) {
		std::wstring* resolvedRoot = resolveRootName(key);

		if (!resolvedRoot) {
			THROW_ERROR_1("ERR_WINREG_INVALID_ROOT", L"Invalid registry root key \"%s\"", key.c_str())
			return NULL;
		}

		return rootKeys.find(*resolvedRoot)->second;
	}

	napi_ref logRef = NULL;
	LOG_DEBUG_VARS
}

/**
 * get() implementation for getting a value for the given key and valueName.
 */
NAPI_METHOD(get) {
	NAPI_ARGV(2)
	NAPI_ARGV_WSTRING(key, 1024, 0)
	NAPI_ARGV_WSTRING(valueName, 256, 1)

	std::string::size_type p = key.find('\\');
	if (p == std::string::npos) {
		napi_throw_error(env, "ERR_NO_SUBKEY", "Expected key to contain both a root and subkey");
		return NULL;
	}

	std::wstring subkey = key.substr(p + 1);
	key.erase(p);

	LOG_DEBUG_3("get", L"key=\"%ls\" subkey=\"%ls\" valueName=\"%ls\"", key.c_str(), subkey.c_str(), valueName.c_str())

	HKEY hroot = winreglib::resolveRootKey(env, key);
	DWORD keyType = 0;
	DWORD dataSize = 0;

	LSTATUS status = RegGetValueW(hroot, subkey.c_str(), valueName.c_str(), RRF_RT_ANY, &keyType, NULL, &dataSize);
	ASSERT_WIN32_STATUS(status, "ERR_WINREG_GET_VALUE", L"RegGetValue() failed")

	LOG_DEBUG_2("get", L"Type=%ld Size=%ld", keyType, dataSize);

	BYTE* data = new BYTE[dataSize];
	status = RegGetValueW(hroot, subkey.c_str(), valueName.c_str(), RRF_RT_ANY, NULL, data, &dataSize);
	if (status != ERROR_SUCCESS) {
		FORMAT_ERROR(status, "ERR_WINREG_GET_VALUE", L"RegGetValue() failed")
		delete[] data;
		return NULL;
	}

	napi_value rval;

	switch (keyType) {
		case REG_SZ:
		case REG_EXPAND_SZ:
		case REG_LINK:
			{
				std::wstring data_wstr(reinterpret_cast<wchar_t*>(data));
				std::u16string data_str16(data_wstr.begin(), data_wstr.end());
				NAPI_THROW_RETURN("get", "ERR_NAPI_CREATE_STRING", napi_create_string_utf16(env, data_str16.c_str(), NAPI_AUTO_LENGTH, &rval), NULL)
				break;
			}

		case REG_DWORD: // same as REG_DWORD_LITTLE_ENDIAN
			NAPI_THROW_RETURN("get", "ERR_NAPI_CREATE_UINT32", napi_create_uint32(env, *((uint32_t*)data), &rval), NULL)
			break;

		case REG_DWORD_BIG_ENDIAN:
			{
				uint32_t in = *((uint32_t*)data);
				uint32_t out = ((in & 0x000000FF) << 16) |
					((in & 0x0000FF00) << 8) |
					((in & 0x00FF0000) >> 8) |
					((in & 0xFF000000) >> 16);
				NAPI_THROW_RETURN("get", "ERR_NAPI_CREATE_UINT32", napi_create_uint32(env, out, &rval), NULL)
				break;
			}

		case REG_QWORD: // same as REG_QWORD_LITTLE_ENDIAN
			NAPI_THROW_RETURN("get", "ERR_NAPI_CREATE_INT64", napi_create_int64(env, *((uint64_t*)data), &rval), NULL)
			break;

		case REG_BINARY:
		case REG_FULL_RESOURCE_DESCRIPTOR:
		case REG_RESOURCE_LIST:
		case REG_RESOURCE_REQUIREMENTS_LIST:
			NAPI_THROW_RETURN("get", "ERR_NAPI_CREATE_BUFFER", napi_create_buffer(env, dataSize, (void**)data, &rval), NULL)
			break;

		case REG_NONE:
			NAPI_THROW_RETURN("get", "ERR_NAPI_GET_NULL", napi_get_null(env, &rval), NULL)
			break;

		case REG_MULTI_SZ:
			{
				BYTE* ptr = data;
				napi_value push;

				NAPI_THROW_RETURN("get", "ERR_NAPI_CREATE_ARRAY", napi_create_array(env, &rval), NULL)
				NAPI_THROW_RETURN("get", "ERR_NAPI_GET_NAMED_PROPERTY", napi_get_named_property(env, rval, "push", &push), NULL)

				while (*ptr) {
					std::wstring data_wstr(reinterpret_cast<wchar_t*>(ptr));
					size_t len = data_wstr.length();
					if (len == 0) {
						break;
					}
					std::u16string data_str16(data_wstr.begin(), data_wstr.end());
					napi_value str;
					NAPI_THROW_RETURN("get", "ERR_NAPI_CREATE_STRING", napi_create_string_utf16(env, data_str16.c_str(), data_str16.length(), &str), NULL)
					NAPI_THROW_RETURN("get", "ERR_NAPI_CALL_FUNCTION", napi_call_function(env, rval, push, 1, &str, NULL), NULL)
					ptr += len + 1;
				}
			}
			break;

		default:
			THROW_ERROR_1("ERR_WINREG_UNSUPPORTED_KEY_TYPE", L"Unsupported key type: %d", keyType)
	}

	delete[] data;

	return rval;
}

/**
 * Emits queued log messages.
 */
static void dispatchLog(uv_async_t* handle) {
	napi_env env = (napi_env)handle->data;
	napi_handle_scope scope;
	napi_value global, logFn, rval;

	NAPI_FATAL("dispatchLog", napi_open_handle_scope(env, &scope))

	if (!winreglib::logRef) {
		return;
	}

	NAPI_FATAL("dispatchLog", napi_get_reference_value(env, winreglib::logRef, &logFn))

	if (logFn == NULL) {
		return;
	}

	NAPI_FATAL("dispatchLog", napi_get_global(env, &global))

	std::lock_guard<std::mutex> lock(winreglib::logLock);

	while (!winreglib::logQueue.empty()) {
		std::shared_ptr<winreglib::LogMessage> obj = winreglib::logQueue.front();
		winreglib::logQueue.pop();
		napi_value argv[2];

		if (obj->ns.length()) {
			NAPI_FATAL("dispatchLog", napi_create_string_utf8(env, obj->ns.c_str(), obj->ns.length(), &argv[0]))
		} else {
			NAPI_FATAL("dispatchLog", napi_get_null(env, &argv[0]))
		}
		NAPI_FATAL("dispatchLog", napi_create_string_utf16(env, obj->msg.c_str(), obj->msg.length(), &argv[1]))

		// we have to create an async context to prevent domain.enter error
		napi_value resName;
		NAPI_FATAL("dispatchLog", napi_create_string_utf8(env, "winreglib.log", NAPI_AUTO_LENGTH, &resName))
		napi_async_context ctx;
		NAPI_FATAL("dispatchLog", napi_async_init(env, NULL, resName, &ctx))

		// emit the log message
		napi_status status = napi_make_callback(env, ctx, global, logFn, 2, argv, &rval);

		napi_async_destroy(env, ctx);

		NAPI_FATAL("dispatchLog", status)
	}

	NAPI_FATAL("dispatchLog", napi_close_handle_scope(env, scope))
}

/**
 * init() implementation for wiring up the log message notification handler and printing the
 * winreglib banner.
 */
NAPI_METHOD(init) {
	NAPI_ARGV(1);
	napi_value logFn = argv[0];

	// wire up the log notification handler
	uv_loop_t* loop;
	napi_get_uv_event_loop(env, &loop);
	winreglib::logNotify.data = env;
	uv_async_init(loop, &winreglib::logNotify, &dispatchLog);
	uv_unref((uv_handle_t*)&winreglib::logNotify);

	// create the reference for the emit log callback so it doesn't get GC'd
	NAPI_THROW_RETURN("init", "ERR_NAPI_CREATE_REFERENCE", napi_create_reference(env, logFn, 1, &winreglib::logRef), NULL)

	// print the banner
	napi_value global, result, args[2];
	uint32_t apiVersion;
	char banner[128];
	const napi_node_version* ver;
	NAPI_THROW_RETURN("init", "ERR_NAPI_GET_GLOBAL", napi_get_global(env, &global), NULL)
	NAPI_THROW_RETURN("init", "ERR_NAPI_GET_NULL", napi_get_null(env, &args[0]), NULL)
	NAPI_THROW_RETURN("init", "ERR_NAPI_GET_VERSION", napi_get_version(env, &apiVersion), NULL)
	NAPI_THROW_RETURN("init", "ERR_NAPI_GET_NODE_VERSION", napi_get_node_version(env, &ver), NULL)
	snprintf(banner, 128, "v" WINREGLIB_VERSION " <" WINREGLIB_URL "> (%s %d.%d.%d/n-api %d)", ver->release, ver->major, ver->minor, ver->patch, apiVersion);
	NAPI_THROW_RETURN("init", "ERR_NAPI_CREATE_STRING", napi_create_string_utf8(env, banner, strlen(banner), &args[1]), NULL)
	NAPI_THROW_RETURN("init", "ERR_NAPI_CALL_FUNCTION", napi_call_function(env, global, logFn, 2, args, &result), NULL)

	NAPI_RETURN_UNDEFINED("init")
}

/**
 * list() implementation for retrieving all subkeys and values for a given key.
 */
NAPI_METHOD(list) {
	NAPI_ARGV(1);
	NAPI_ARGV_WSTRING(key, 1024, 0)

	std::string::size_type p = key.find('\\');
	if (p == std::string::npos) {
		napi_throw_error(env, "ERR_NO_SUBKEY", "Expected key to contain both a root and subkey");
		return NULL;
	}

	std::wstring root = key.substr(0, p);
	std::wstring subkey = key.substr(p + 1);

	LOG_DEBUG_2("list", L"key=\"%ls\" subkey=\"%ls\"", root.c_str(), subkey.c_str())

	HKEY hroot = winreglib::resolveRootKey(env, root);
	HKEY hkey;
	LSTATUS status = RegOpenKeyExW(hroot, subkey.c_str(), 0, KEY_READ, &hkey);
	ASSERT_WIN32_STATUS(status, "ERR_WINREG_OPEN_KEY", L"RegOpenKeyEx() failed")

	std::wstring* resolvedRoot = winreglib::resolveRootName(root);
	if (!resolvedRoot) {
		THROW_ERROR_1("ERR_WINREG_INVALID_ROOT", L"Invalid registry root key \"%s\"", key.c_str())
		return NULL;
	}

	// add back the resolved root
	key = *resolvedRoot + L'\\' + subkey;

	napi_value rval;
	napi_value subkeys;
	napi_value subkeysPush;
	napi_value values;
	napi_value valuesPush;

	NAPI_THROW_RETURN("list", "ERR_NAPI_CREATE_OBJECT", napi_create_object(env, &rval), NULL)
	NAPI_THROW_RETURN("list", "ERR_NAPI_CREATE_ARRAY", napi_create_array(env, &subkeys), NULL)
	NAPI_THROW_RETURN("list", "ERR_NAPI_CREATE_ARRAY", napi_create_array(env, &values), NULL)
	SET_PROP_FROM_WSTRING(rval, "resolvedRoot", resolvedRoot)
	SET_PROP_FROM_WSTRING(rval, "key", &key)
	NAPI_THROW_RETURN("list", "ERR_NAPI_SET_NAMED_PROPERTY", napi_set_named_property(env, rval, "subkeys", subkeys), NULL)
	NAPI_THROW_RETURN("list", "ERR_NAPI_SET_NAMED_PROPERTY", napi_set_named_property(env, rval, "values", values), NULL)
	NAPI_THROW_RETURN("list", "ERR_NAPI_GET_NAMED_PROPERTY", napi_get_named_property(env, subkeys, "push", &subkeysPush), NULL)
	NAPI_THROW_RETURN("list", "ERR_NAPI_GET_NAMED_PROPERTY", napi_get_named_property(env, values, "push", &valuesPush), NULL)

	DWORD numSubkeys = 0;
	DWORD maxSubkeyLength = 0;
	DWORD numValues = 0;
	DWORD maxValueLength = 0;

	status = RegQueryInfoKeyW(hkey, NULL, NULL, NULL, &numSubkeys, &maxSubkeyLength, NULL, &numValues, &maxValueLength, NULL, NULL, NULL);
	if (status != ERROR_SUCCESS) {
		FORMAT_ERROR(status, "ERR_WINREG_QUERY_INFO_KEY", L"RegQueryInfoKey() failed")
		RegCloseKey(hkey);
		return NULL;
	}

	LOG_DEBUG_4("list", L"%d keys (max %d), %d values (max %d)", numSubkeys, maxSubkeyLength, numValues, maxValueLength)

	DWORD maxSize = (maxSubkeyLength > maxValueLength ? maxSubkeyLength : maxValueLength) + 1;
	wchar_t* buffer = new wchar_t[maxSize];

	try {
		for (DWORD i = 0; i < numSubkeys; ++i) {
			SET_FROM_REGISTRY(RegEnumKeyExW, hkey, i, buffer, maxSize, "ERR_WINREG_ENUM_KEY", subkeys, subkeysPush)
		}

		for (DWORD i = 0; i < numValues; ++i) {
			SET_FROM_REGISTRY(RegEnumValueW, hkey, i, buffer, maxSize, "ERR_WINREG_ENUM_VALUE", values, valuesPush)
		}
	} catch (...) {
		rval = NULL;
	}

	delete[] buffer;
	RegCloseKey(hkey);
	return rval;
}

/**
 * Common watch/unwatch boilerplate.
 */
napi_value watchHelper(napi_env env, napi_callback_info info, winreglib::WatchAction action) {
	NAPI_ARGV(2);
	NAPI_ARGV_WSTRING(key, 1024, 0)
	napi_value listener = argv[1];

	std::string::size_type p = key.find('\\');
	if (p == std::string::npos) {
		napi_throw_error(env, "ERR_NO_SUBKEY", "Expected key to contain both a root and subkey");
		return NULL;
	}

	std::wstring root = key.substr(0, p);
	auto it = winreglib::rootMap.find(root);
	if (it != winreglib::rootMap.end()) {
		root = it->second;
	}
	auto it2 = winreglib::rootKeys.find(root);
	if (it2 == winreglib::rootKeys.end()) {
		THROW_ERROR_1("ERR_WINREG_INVALID_ROOT", L"Invalid registry root key \"%s\"", root.c_str())
		return NULL;
	}
	key = root + key.substr(p);

	char* ns = action == winreglib::Watch ? "watch" : "unwatch";
	LOG_DEBUG_1(ns, L"key=\"%ls\"", key.c_str())

	winreglib::watchman->config(key, listener, action);

	NAPI_RETURN_UNDEFINED(ns)
}

/**
 * watch() implementation to watch a key for changes.
 */
NAPI_METHOD(watch) {
	return watchHelper(env, info, winreglib::Watch);
}

/**
 * unwatch() implementation to stop watching a key for changes.
 */
NAPI_METHOD(unwatch) {
	return watchHelper(env, info, winreglib::Unwatch);
}

/**
 * Destroys the Watchman instance and closes open handles.
 */
static void cleanup(void* arg) {
	napi_env env = (napi_env)arg;

	if (winreglib::watchman) {
		LOG_DEBUG("cleanup", L"Deleting watchman")
		delete winreglib::watchman;
		winreglib::watchman = NULL;
	}

	uv_close((uv_handle_t*)&winreglib::logNotify, NULL);

	if (winreglib::logRef) {
		napi_delete_reference(env, winreglib::logRef);
		winreglib::logRef = NULL;
	}
}

/**
 * Wire up the public API and cleanup handler and creates the Watchman instance.
 */
NAPI_INIT() {
	NAPI_EXPORT_FUNCTION(get);
	NAPI_EXPORT_FUNCTION(init);
	NAPI_EXPORT_FUNCTION(list);
	NAPI_EXPORT_FUNCTION(watch);
	NAPI_EXPORT_FUNCTION(unwatch);

	NAPI_THROW("init", "ERR_NAPI_ADD_ENV_CLEANUP_HOOK", napi_add_env_cleanup_hook(env, cleanup, env))

	winreglib::watchman = new winreglib::Watchman(env);
}
