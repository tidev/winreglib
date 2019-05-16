#ifndef __WINREGLIB__
#define __WINREGLIB__

#define NAPI_VERSION 3

#include <map>
#include <napi-macros.h>
#include <node_api.h>
#include <string>

#define TRIM_EXTRA_LINES(str) \
	{ \
		char* nl = ::strchr(str, '\r'); \
		if (nl != NULL) { \
			*nl = '\0'; \
		} \
	}

#define THROW_ERROR(code, msg) \
	{ \
		napi_value error; \
		napi_value errCode; \
		napi_value message; \
		\
		std::wstring wbuffer(msg); \
		std::u16string u16buffer(wbuffer.begin(), wbuffer.end()); \
		\
		NAPI_STATUS_THROWS(napi_create_string_utf8(env, code, ::strlen(code) + 1, &errCode)); \
		NAPI_STATUS_THROWS(napi_create_string_utf16(env, u16buffer.c_str(), u16buffer.length(), &message)); \
		\
		napi_create_error(env, errCode, message, &error); \
		napi_throw(env, error); \
	}

#define THROW_ERROR_1(code, msg, arg1) \
	{ \
		napi_value error; \
		napi_value errCode; \
		napi_value message; \
		\
		wchar_t buffer[1024]; \
		::swprintf(buffer, 1024, msg, arg1); \
		\
		std::wstring wbuffer(buffer); \
		std::u16string u16buffer(wbuffer.begin(), wbuffer.end()); \
		\
		NAPI_STATUS_THROWS(napi_create_string_utf8(env, code, ::strlen(code) + 1, &errCode)); \
		NAPI_STATUS_THROWS(napi_create_string_utf16(env, u16buffer.c_str(), u16buffer.length(), &message)); \
		\
		napi_create_error(env, errCode, message, &error); \
		napi_throw(env, error); \
	}

#define THROW_ERROR_2(code, msg, arg1, arg2) \
	{ \
		napi_value error; \
		napi_value errCode; \
		napi_value message; \
		\
		wchar_t buffer[1024]; \
		::swprintf(buffer, 1024, msg, arg1, arg2); \
		\
		std::wstring wbuffer(buffer); \
		std::u16string u16buffer(wbuffer.begin(), wbuffer.end()); \
		\
		NAPI_STATUS_THROWS(napi_create_string_utf8(env, code, ::strlen(code) + 1, &errCode)); \
		NAPI_STATUS_THROWS(napi_create_string_utf16(env, u16buffer.c_str(), u16buffer.length(), &message)); \
		\
		napi_create_error(env, errCode, message, &error); \
		napi_throw(env, error); \
	}

#define __NAPI_ARGV_U16CHAR(name, size, i) \
	char16_t name[size]; \
	size_t name##_len; \
	if (napi_get_value_string_utf16(env, argv[i], (char16_t*)&name, size, &name##_len) != napi_ok) { \
		napi_throw_error(env, "EINVAL", "Expected string"); \
		return NULL; \
	}

#define __NAPI_ARGV_U16STRING(name, size, i) \
	__NAPI_ARGV_U16CHAR(name##_char16, size, i) \
	std::u16string name(name##_char16);

#define __NAPI_ARGV_WSTRING(name, size, i) \
	__NAPI_ARGV_U16STRING(name##_str16, size, i) \
	std::wstring name(name##_str16.begin(), name##_str16.end());

#define FORMAT_ERROR(status, code, message) \
	char msg[512]; \
	::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, status, NULL, (LPTSTR)&msg, 512, NULL); \
	TRIM_EXTRA_LINES(msg); \
	THROW_ERROR_2(code, message ": %hs (code %d)", msg, status);

#define ASSERT_STATUS(status, code, message) \
	if (status != ERROR_SUCCESS) { \
		if (status == 2) { \
			THROW_ERROR("ERR_WINREG_NOT_FOUND", L"Registry key or value not found") \
		} else { \
			FORMAT_ERROR(status, code, message) \
		} \
		return NULL; \
	}

#define SET_FROM_REGISTRY(fn, key, idx, buffer, maxSize, code, dest, push) \
	DWORD size = maxSize; \
	LSTATUS status = fn(key, idx, buffer, &size, NULL, NULL, NULL, NULL); \
	if (status != ERROR_SUCCESS) { \
		FORMAT_ERROR(status, code, L"" #fn " failed") \
		throw; \
	} \
	\
	std::wstring subkey_wstr(buffer); \
	std::u16string subkey_str16(subkey_wstr.begin(), subkey_wstr.end()); \
	\
	napi_value result; \
	if (napi_create_string_utf16(env, subkey_str16.c_str(), NAPI_AUTO_LENGTH, &result) != napi_ok) { \
		napi_throw_error(env, NULL, "napi_create_string_utf16 failed"); \
		throw; \
	} \
	if (napi_call_function(env, dest, push, 1, &result, NULL) != napi_ok) { \
		napi_throw_error(env, NULL, "napi_call_function failed"); \
		throw; \
	}

#define SET_PROP_FROM_WSTRING(obj, prop, str) \
	{ \
		std::wstring* tmp = str; \
		std::u16string tmp2(tmp->begin(), tmp->end()); \
		napi_value value; \
		NAPI_STATUS_THROWS(napi_create_string_utf16(env, tmp2.c_str(), NAPI_AUTO_LENGTH, &value)) \
		NAPI_STATUS_THROWS(napi_set_named_property(env, obj, prop, value)) \
	}

#define LOG_NAPI_STATUS_ERROR(call, err) \
	if ((call) != napi_ok) { \
		printf(err "\n"); \
		return; \
	}

#define LOG_DEBUG_WIN32_ERROR(message, code) \
	if (code != ERROR_SUCCESS) { \
		char msg[512]; \
		::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, code, NULL, (LPTSTR)&msg, 512, NULL); \
		TRIM_EXTRA_LINES(msg); \
		::printf(message "%s (code %d)\n", msg, code); \
	}

#define LOG_DEBUG_THREAD_ID(msg) \
	::printf(msg " (thread %zu)\n", std::hash<std::thread::id>{}(std::this_thread::get_id()));

#define LOG_DEBUG(code) { code; }

#endif
