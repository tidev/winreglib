#define NAPI_VERSION 3

#include <map>
#include <napi-macros.h>
#include <node_api.h>
#include <string>
#include <windows.h>

#define TRIM_EXTRA_LINES(str) \
	{ \
		char* nl = strchr(str, '\r'); \
		if (nl != NULL) { \
			*nl = '\0'; \
		} \
	}

#define THROW_ERROR_1(code, msg, arg1) \
	{ \
		char buffer[1024]; \
		snprintf(buffer, 1024, msg, arg1); \
		napi_throw_error(env, code, buffer); \
	}

#define THROW_ERROR_2(code, msg, arg1, arg2) \
	{ \
		char buffer[1024]; \
		snprintf(buffer, 1024, msg, arg1, arg2); \
		napi_throw_error(env, code, buffer); \
	}

#define NAPI_ARGV_WSTRING(name, size, i) \
	char16_t name##_char16[size]; \
	size_t name##_len; \
	if (napi_get_value_string_utf16(env, argv[i], (char16_t*)&name##_char16, size, &name##_len) != napi_ok) { \
		napi_throw_error(env, "EINVAL", "Expected string"); \
		return NULL; \
	} \
	std::u16string name##_str16(name##_char16); \
	std::wstring name(name##_str16.begin(), name##_str16.end());

#define FORMAT_ERROR(status, code, message) \
	char msg[512]; \
	::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, status, NULL, (LPTSTR)&msg, 512, NULL); \
	TRIM_EXTRA_LINES(msg); \
	THROW_ERROR_2(code, message ": %s (code %d)", msg, status);

#define ASSERT_STATUS(status, code, message) \
	if (status != ERROR_SUCCESS) { \
		if (status == 2) { \
			THROW_ERROR_1("ERR_WINREG_NOT_FOUND", "Registry key or value not found (code %d)", status) \
		} else { \
			FORMAT_ERROR(status, code, message) \
		} \
		return NULL; \
	}

#define SET_FROM_REGISTRY(fn, buffer, maxSize, code, dest, push) \
	DWORD size = maxSize; \
	LSTATUS status = ::fn(key, i, buffer, &size, NULL, NULL, NULL, NULL); \
	if (status != ERROR_SUCCESS) { \
		FORMAT_ERROR(status, code, #fn " failed") \
		throw; \
	} \
	std::wstring subkey_wstr(buffer); \
	std::u16string subkey_str16(subkey_wstr.begin(), subkey_wstr.end()); \
	napi_value result; \
	if (napi_create_string_utf16(env, subkey_str16.c_str(), NAPI_AUTO_LENGTH, &result) != napi_ok) { \
		napi_throw_error(env, NULL, "napi_create_string_utf16 failed"); \
		throw; \
	} \
	if (napi_call_function(env, dest, push, 1, &result, NULL) != napi_ok) { \
		napi_throw_error(env, NULL, "napi_call_function failed"); \
		throw; \
	}

struct cmp_str {
	bool operator()(char const *a, char const *b) const {
		return std::strcmp(a, b) < 0;
	}
};

const std::map<const char*, const char*, cmp_str> rootMap = {
	{ "HKCR", "HKEY_CLASSES_ROOT" },
	{ "HKCC", "HKEY_CURRENT_CONFIG" },
	{ "HKCU", "HKEY_CURRENT_USER" },
	{ "HKLM", "HKEY_LOCAL_MACHINE" },
	{ "HKU",  "HKEY_USERS" }
};

const std::map<const char*, HKEY, cmp_str> rootKeys = {
	{ "HKEY_CLASSES_ROOT",                HKEY_CLASSES_ROOT },
	{ "HKEY_CURRENT_CONFIG",              HKEY_CURRENT_CONFIG },
	{ "HKEY_CURRENT_USER",                HKEY_CURRENT_USER },
	{ "HKEY_CURRENT_USER_LOCAL_SETTINGS", HKEY_CURRENT_USER_LOCAL_SETTINGS },
	{ "HKEY_LOCAL_MACHINE",               HKEY_LOCAL_MACHINE },
	{ "HKEY_PERFORMANCE_DATA",            HKEY_PERFORMANCE_DATA },
	{ "HKEY_PERFORMANCE_NLSTEXT",         HKEY_PERFORMANCE_NLSTEXT },
	{ "HKEY_PERFORMANCE_TEXT",            HKEY_PERFORMANCE_TEXT },
	{ "HKEY_USERS",                       HKEY_USERS }
};

const char* resolveRootName(const char* key) {
	auto it = rootMap.find(key);
	return it == rootMap.end() ? key : it->second;
}

HKEY resolveRootKey(napi_env env, const char* key) {
	auto it = rootKeys.find(resolveRootName(key));
	if (it == rootKeys.end()) {
		THROW_ERROR_1("ERR_WINREG_INVALID_KEY", "Invalid registry root key \"%s\"", key)
		return NULL;
	}
	return it->second;
}

NAPI_METHOD(get) {
	NAPI_ARGV(3)
	NAPI_ARGV_UTF8(rootName, 32, 0)
	NAPI_ARGV_WSTRING(subkey, 256, 1)
	NAPI_ARGV_WSTRING(valueName, 256, 2)

	HKEY root = resolveRootKey(env, rootName);
	DWORD keyType = 0;
	DWORD dataSize = 0;

	LSTATUS status = ::RegGetValueW(root, subkey.c_str(), valueName.c_str(), RRF_RT_ANY, &keyType, NULL, &dataSize);
	ASSERT_STATUS(status, "ERR_WINREG_GET_VALUE", "RegGetValue() failed")

	// printf("Type=%ld Size=%ld\n", keyType, dataSize);

	BYTE* data = new BYTE[dataSize];
	status = ::RegGetValueW(root, subkey.c_str(), valueName.c_str(), RRF_RT_ANY, NULL, data, &dataSize);
	if (status != ERROR_SUCCESS) {
		FORMAT_ERROR(status, "ERR_WINREG_GET_VALUE", "RegGetValue() failed")
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
			THROW_ERROR_1("ERR_WINREG_UNSUPPORTED_KEY_TYPE", "Unsupported key type: %d", keyType);
	}

	delete[] data;

	return rval;
}

NAPI_METHOD(list) {
	NAPI_ARGV(2);
	NAPI_ARGV_UTF8(rootName, 32, 0)
	NAPI_ARGV_WSTRING(subkey, 256, 1)

	napi_value resolvedRoot;
	napi_value rval;
	napi_value subkeys;
	napi_value subkeysPush;
	napi_value values;
	napi_value valuesPush;

	NAPI_STATUS_THROWS(napi_create_string_utf8(env, resolveRootName(rootName), NAPI_AUTO_LENGTH, &resolvedRoot))
	NAPI_STATUS_THROWS(napi_create_object(env, &rval))
	NAPI_STATUS_THROWS(napi_create_array(env, &subkeys))
	NAPI_STATUS_THROWS(napi_create_array(env, &values))
	NAPI_STATUS_THROWS(napi_set_named_property(env, rval, "root", resolvedRoot))
	NAPI_STATUS_THROWS(napi_set_named_property(env, rval, "path", argv[1]))
	NAPI_STATUS_THROWS(napi_set_named_property(env, rval, "subkeys", subkeys))
	NAPI_STATUS_THROWS(napi_set_named_property(env, rval, "values", values))
	NAPI_STATUS_THROWS(napi_get_named_property(env, subkeys, "push", &subkeysPush))
	NAPI_STATUS_THROWS(napi_get_named_property(env, values, "push", &valuesPush))

	HKEY root = resolveRootKey(env, rootName);
	HKEY key;
	LSTATUS status = ::RegOpenKeyExW(root, subkey.c_str(), 0, KEY_READ, &key);
	ASSERT_STATUS(status, "ERR_WINREG_OPEN_KEY", "RegOpenKeyEx() failed")

	DWORD numSubkeys = 0;
	DWORD maxSubkeyLength = 0;
	DWORD numValues = 0;
	DWORD maxValueLength = 0;
	status = ::RegQueryInfoKeyW(key, NULL, NULL, NULL, &numSubkeys, &maxSubkeyLength, NULL, &numValues, &maxValueLength, NULL, NULL, NULL);
	if (status != ERROR_SUCCESS) {
		FORMAT_ERROR(status, "ERR_WINREG_QUERY_INFO_KEY", "RegQueryInfoKey failed")
		::RegCloseKey(key);
		return NULL;
	}

	// printf("%d keys (max %d), %d values (max %d)\n", numSubkeys, maxSubkeyLength, numValues, maxValueLength);

	DWORD maxSize = (maxSubkeyLength > maxValueLength ? maxSubkeyLength : maxValueLength) + 1;
	wchar_t* buffer = new wchar_t[maxSize];

	try {
		for (DWORD i = 0; i < numSubkeys; ++i) {
			SET_FROM_REGISTRY(RegEnumKeyExW, buffer, maxSize, "ERR_WINREG_ENUM_KEY", subkeys, subkeysPush)
		}

		for (DWORD i = 0; i < numValues; ++i) {
			SET_FROM_REGISTRY(RegEnumValueW, buffer, maxSize, "ERR_WINREG_ENUM_VALUE", values, valuesPush)
		}
	} catch (...) {
		rval = NULL;
	}

	delete[] buffer;
	::RegCloseKey(key);
	return rval;
}

NAPI_METHOD(watch) {
	NAPI_ARGV(2);
	NAPI_ARGV_UTF8(rootName, 32, 0)
	NAPI_ARGV_WSTRING(subkey, 256, 1)

	HKEY root = resolveRootKey(env, rootName);
	HKEY key;
	LSTATUS status = ::RegOpenKeyExW(root, subkey.c_str(), 0, KEY_READ, &key);
	ASSERT_STATUS(status, "ERR_WINREG_OPEN_KEY", "RegOpenKeyEx() failed")

	// LSTATUS status = ::RegNotifyChangeKeyValue(key)

	return 0;
}

NAPI_METHOD(unwatch) {
	NAPI_ARGV(1);
	NAPI_ARGV_UINT32(id, 0);

	return 0;
}

NAPI_INIT() {
	NAPI_EXPORT_FUNCTION(get);
	NAPI_EXPORT_FUNCTION(list);
	NAPI_EXPORT_FUNCTION(watch);
	NAPI_EXPORT_FUNCTION(unwatch);
}

/*
HKEY hKey;

  //Check if the registry exists
  DWORD lRv = RegOpenKeyEx(
	HKEY_CURRENT_USER,
	L"Software\\Zahid",
	0,
	KEY_READ,
	&hKey
  );

  if (lRv == ERROR_SUCCESS)
  {
	DWORD BufferSize = sizeof(DWORD);
	DWORD dwRet;
	DWORD cbData;
	DWORD cbVal = 0;

	dwRet = RegQueryValueEx(
	  hKey,
	  L"Something",
	  NULL,
	  NULL,
	  (LPBYTE)&cbVal,
	  &cbData
	);

	if( dwRet == ERROR_SUCCESS )
	  cout<<"\nValue of Something is " << cbVal << endl;
	else cout<<"\nRegQueryValueEx failed " << dwRet << endl;
  }
  else
  {
	cout<<"RegOpenKeyEx failed " << lRv << endl;
  }
  */
