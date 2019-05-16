#include "winreglib.h"
#include "watchman.h"
#include <windows.h>

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
}

using namespace winreglib;

NAPI_METHOD(get) {
	NAPI_ARGV(2)
	__NAPI_ARGV_WSTRING(key, 1024, 0)
	__NAPI_ARGV_WSTRING(valueName, 256, 1)

	std::string::size_type p = key.find('\\');
	if (p == std::string::npos) {
		napi_throw_error(env, "ERR_NO_SUBKEY", "Expected key to contain both a root and subkey");
		return NULL;
	}

	std::wstring subkey = key.substr(p + 1);
	key.erase(p);

	// wprintf(L"key=%ls\n", key.c_str());
	// wprintf(L"subkey=%ls\n", subkey.c_str());
	// wprintf(L"valueName=%ls\n", valueName.c_str());

	HKEY hroot = resolveRootKey(env, key);
	DWORD keyType = 0;
	DWORD dataSize = 0;

	LSTATUS status = RegGetValueW(hroot, subkey.c_str(), valueName.c_str(), RRF_RT_ANY, &keyType, NULL, &dataSize);
	ASSERT_STATUS(status, "ERR_WINREG_GET_VALUE", L"RegGetValue() failed")

	// printf("Type=%ld Size=%ld\n", keyType, dataSize);

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
				NAPI_STATUS_THROWS(napi_create_string_utf16(env, data_str16.c_str(), NAPI_AUTO_LENGTH, &rval))
				break;
			}

		case REG_DWORD: // same as REG_DWORD_LITTLE_ENDIAN
			NAPI_STATUS_THROWS(napi_create_uint32(env, *((uint32_t*)data), &rval))
			break;

		case REG_DWORD_BIG_ENDIAN:
			{
				uint32_t in = *((uint32_t*)data);
				uint32_t out = ((in & 0x000000FF) << 16) |
					((in & 0x0000FF00) << 8) |
					((in & 0x00FF0000) >> 8) |
					((in & 0xFF000000) >> 16);
				NAPI_STATUS_THROWS(napi_create_uint32(env, out, &rval))
				break;
			}

		case REG_QWORD: // same as REG_QWORD_LITTLE_ENDIAN
			NAPI_STATUS_THROWS(napi_create_int64(env, *((uint64_t*)data), &rval))
			break;

		case REG_BINARY:
		case REG_FULL_RESOURCE_DESCRIPTOR:
		case REG_RESOURCE_LIST:
		case REG_RESOURCE_REQUIREMENTS_LIST:
			NAPI_STATUS_THROWS(napi_create_buffer(env, dataSize, (void**)data, &rval))
			break;

		case REG_NONE:
			NAPI_STATUS_THROWS(napi_get_null(env, &rval))
			break;

		case REG_MULTI_SZ:
			{
				BYTE* ptr = data;
				napi_value push;

				if (napi_create_array(env, &rval) != napi_ok) {
					napi_throw_error(env, NULL, "napi_create_array failed");
					break;
				}

				if (napi_get_named_property(env, rval, "push", &push) != napi_ok) {
					napi_throw_error(env, NULL, "napi_get_named_property failed");
					break;
				}

				while (*ptr) {
					std::wstring data_wstr(reinterpret_cast<wchar_t*>(ptr));
					size_t len = data_wstr.length();
					if (len == 0) {
						break;
					}
					std::u16string data_str16(data_wstr.begin(), data_wstr.end());
					napi_value str;
					if (napi_create_string_utf16(env, data_str16.c_str(), NAPI_AUTO_LENGTH, &str) != napi_ok) {
						napi_throw_error(env, NULL, "napi_create_string_utf16 failed");
						break;
					}
					if (napi_call_function(env, rval, push, 1, &str, NULL) != napi_ok) {
						napi_throw_error(env, NULL, "napi_call_function failed");
						break;
					}
					ptr += len + 1;
				}
			}
			break;

		default:
			THROW_ERROR_1("ERR_WINREG_UNSUPPORTED_KEY_TYPE", L"Unsupported key type: %d", keyType);
	}

	delete[] data;

	return rval;
}

NAPI_METHOD(list) {
	NAPI_ARGV(1);
	__NAPI_ARGV_WSTRING(key, 1024, 0)

	std::string::size_type p = key.find('\\');
	if (p == std::string::npos) {
		napi_throw_error(env, "ERR_NO_SUBKEY", "Expected key to contain both a root and subkey");
		return NULL;
	}

	std::wstring subkey = key.substr(p + 1);
	key.erase(p);

	// wprintf(L"key=%ls\n", key.c_str());
	// wprintf(L"subkey=%ls\n", subkey.c_str());

	HKEY hroot = resolveRootKey(env, key);
	HKEY hkey;
	LSTATUS status = RegOpenKeyExW(hroot, subkey.c_str(), 0, KEY_READ, &hkey);
	ASSERT_STATUS(status, "ERR_WINREG_OPEN_KEY", L"RegOpenKeyEx() failed")

	std::wstring* resolvedRoot = resolveRootName(key);
	if (!resolvedRoot) {
		THROW_ERROR_1("ERR_WINREG_INVALID_ROOT", L"Invalid registry root key \"%s\"", key.c_str())
		return NULL;
	}

	napi_value rval;
	napi_value subkeys;
	napi_value subkeysPush;
	napi_value values;
	napi_value valuesPush;

	NAPI_STATUS_THROWS(napi_create_object(env, &rval))
	NAPI_STATUS_THROWS(napi_create_array(env, &subkeys))
	NAPI_STATUS_THROWS(napi_create_array(env, &values))
	SET_PROP_FROM_WSTRING(rval, "root", resolvedRoot)
	SET_PROP_FROM_WSTRING(rval, "path", &subkey)
	NAPI_STATUS_THROWS(napi_set_named_property(env, rval, "subkeys", subkeys))
	NAPI_STATUS_THROWS(napi_set_named_property(env, rval, "values", values))
	NAPI_STATUS_THROWS(napi_get_named_property(env, subkeys, "push", &subkeysPush))
	NAPI_STATUS_THROWS(napi_get_named_property(env, values, "push", &valuesPush))

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

	// printf("%d keys (max %d), %d values (max %d)\n", numSubkeys, maxSubkeyLength, numValues, maxValueLength);

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

NAPI_METHOD(watch) {
	NAPI_ARGV(2);
	__NAPI_ARGV_WSTRING(key, 1024, 0)
	napi_value callback = argv[1];

	std::string::size_type p = key.find('\\');
	if (p != std::string::npos) {
		std::wstring root = key.substr(0, p);
		auto it = rootMap.find(root);
		if (it != rootMap.end()) {
			root = it->second;
		}
		auto it2 = rootKeys.find(root);
		if (it2 == rootKeys.end()) {
			THROW_ERROR_1("ERR_WINREG_INVALID_ROOT", L"Invalid registry root key \"%s\"", root.c_str())
			return NULL;
		}
		key = root + key.substr(p);
	}

	// wprintf(L"key=%ls\n", key.c_str());

	const int64_t id64 = watchman->add(env, key, callback);

	napi_value id;
	napi_create_int64(env, id64, &id);
	return id;
}

NAPI_METHOD(unwatch) {
	NAPI_ARGV(1);

	int64_t id = 0;
	if (napi_get_value_int64(env, argv[0], &id) != napi_ok) {
		napi_throw_error(env, "EINVAL", "Expected id to be an unsigned number");
		return NULL;
	}

	watchman->remove(id);

	napi_value rval;
	NAPI_STATUS_THROWS(napi_get_undefined(env, &rval));
	return rval;
}

void cleanup(void* arg) {
	if (watchman) {
		delete watchman;
		watchman = NULL;
	}
}

NAPI_INIT() {
	NAPI_EXPORT_FUNCTION(get);
	NAPI_EXPORT_FUNCTION(list);
	NAPI_EXPORT_FUNCTION(watch);
	NAPI_EXPORT_FUNCTION(unwatch);

	NAPI_STATUS_THROWS(napi_add_env_cleanup_hook(env, cleanup, NULL));

	watchman = new Watchman(env);
}
