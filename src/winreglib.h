#ifndef __WINREGLIB__
#define __WINREGLIB__

#define NAPI_VERSION 3

#include <map>
#include <napi-macros.h>
#include <node_api.h>
#include <queue>
#include <string>

namespace winreglib {
	struct LogMessage {
		LogMessage() {}
		LogMessage(const std::string ns, const std::u16string msg) : ns(ns), msg(msg) {}
		std::string ns;
		std::u16string msg;
	};
}

#define WINREGLIB_URL "https://github.com/appcelerator/winreglib"

// enable the following line to bypass the message queue and print the raw debug log messages to stdout
// #define ENABLE_RAW_DEBUGGING

#define TRIM_EXTRA_LINES(str) \
	{ \
		char* nl = ::strchr(str, '\r'); \
		if (nl != NULL) { \
			*nl = '\0'; \
		} \
	}

#define NAPI_THROW_RETURN(ns, code, call, rval) \
	{ \
		napi_status _status = call; \
		if (_status != napi_ok) { \
			const napi_extended_error_info* error; \
			::napi_get_last_error_info(env, &error); \
			\
			char msg[1024]; \
			::snprintf(msg, 1024, "%s: " #call " failed (status=%d) %s", ns, _status, error->error_message); \
			\
			napi_value err, errCode, message; \
			NAPI_STATUS_THROWS(::napi_create_string_utf8(env, code, NAPI_AUTO_LENGTH, &errCode)) \
			NAPI_STATUS_THROWS(::napi_create_string_utf8(env, msg, ::strlen(msg), &message)) \
			\
			::napi_create_error(env, errCode, message, &err); \
			::napi_throw(env, err); \
			return rval; \
		} \
	}

#define NAPI_THROW(ns, code, call) NAPI_THROW_RETURN(ns, code, call, )

#define THROW_ERROR_PRE \
	napi_value error; \
	napi_value errCode; \
	napi_value message;

#define THROW_ERROR_POST(code, msg) \
	std::wstring wbuffer(msg); \
	std::u16string u16buffer(wbuffer.begin(), wbuffer.end()); \
	\
	NAPI_STATUS_THROWS(napi_create_string_utf8(env, code, NAPI_AUTO_LENGTH, &errCode)); \
	NAPI_STATUS_THROWS(napi_create_string_utf16(env, u16buffer.c_str(), u16buffer.length(), &message)); \
	\
	napi_create_error(env, errCode, message, &error); \
	napi_throw(env, error);

#define THROW_ERROR(code, msg) \
	{ \
		THROW_ERROR_PRE \
		THROW_ERROR_POST(code, msg) \
	}

#define THROW_ERROR_1(code, msg, arg1) \
	{ \
		THROW_ERROR_PRE \
		wchar_t buffer[1024]; \
		::swprintf(buffer, 1024, msg, arg1); \
		THROW_ERROR_POST(code, buffer) \
	}

#define THROW_ERROR_2(code, msg, arg1, arg2) \
	{ \
		THROW_ERROR_PRE \
		wchar_t buffer[1024]; \
		::swprintf(buffer, 1024, msg, arg1, arg2); \
		THROW_ERROR_POST(code, buffer) \
	}

#define NAPI_ARGV_U16CHAR(name, size, i) \
	char16_t name[size]; \
	size_t name##_len; \
	if (napi_get_value_string_utf16(env, argv[i], (char16_t*)&name, size, &name##_len) != napi_ok) { \
		napi_throw_error(env, "EINVAL", "Expected string"); \
		return NULL; \
	}

#define NAPI_ARGV_U16STRING(name, size, i) \
	NAPI_ARGV_U16CHAR(name##_char16, size, i) \
	std::u16string name(name##_char16);

#define NAPI_ARGV_WSTRING(name, size, i) \
	NAPI_ARGV_U16STRING(name##_str16, size, i) \
	std::wstring name(name##_str16.begin(), name##_str16.end());

#define NAPI_RETURN_UNDEFINED(ns) \
	{ \
		napi_value undef; \
		NAPI_THROW_RETURN(ns, "ERR_NAPI_GET_UNDEFINED", ::napi_get_undefined(env, &undef), NULL) \
		return undef; \
	}

#define NAPI_FATAL(ns, code) \
	{ \
		napi_status _status = code; \
		if (_status == napi_pending_exception) { \
			napi_value fatal; \
			napi_get_and_clear_last_exception(env, &fatal); \
			napi_fatal_exception(env, fatal); \
			return; \
		} else if (_status != napi_ok) { \
			const napi_extended_error_info* error; \
			::napi_get_last_error_info(env, &error); \
			\
			char msg[1024]; \
			::snprintf(msg, 1024, ns ": " #code " failed (status=%d) %s", _status, error->error_message); \
			\
			napi_value fatal; \
			if (::napi_create_string_utf8(env, msg, strlen(msg), &fatal) == napi_ok) { \
				::napi_fatal_exception(env, fatal); \
			} else { \
				::fprintf(stderr, msg); \
			} \
			return; \
		} \
	}

#define FORMAT_ERROR(status, code, message) \
	char msg[512]; \
	::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, status, NULL, (LPTSTR)&msg, 512, NULL); \
	TRIM_EXTRA_LINES(msg); \
	THROW_ERROR_2(code, message ": %hs (code %d)", msg, status);

#define ASSERT_WIN32_STATUS(status, code, message) \
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

#define LOG_DEBUG_VARS \
	std::mutex logLock; \
	uv_async_t logNotify; \
	std::queue<std::shared_ptr<winreglib::LogMessage>> logQueue;

#define LOG_DEBUG_EXTERN_VARS \
	extern std::mutex logLock; \
	extern uv_async_t logNotify; \
	extern std::queue<std::shared_ptr<winreglib::LogMessage>> logQueue;

#ifdef ENABLE_RAW_DEBUGGING
	#define LOG_DEBUG(ns, msg) \
		{ \
			std::wstring wbuffer(msg); \
			::wprintf(L"%hs: %s\n", ns, wbuffer.c_str()); \
		}

	#define WLOG_DEBUG(ns, wmsg) \
		::wprintf(L"%hs: %s\n", ns, wmsg.c_str());
#else
	#define LOG_DEBUG(ns, msg) \
		{ \
			std::wstring wbuffer(msg); \
			std::u16string u16buffer(wbuffer.begin(), wbuffer.end()); \
			std::shared_ptr<winreglib::LogMessage> obj = std::make_shared<winreglib::LogMessage>(ns, u16buffer); \
			std::lock_guard<std::mutex> lock(winreglib::logLock); \
			winreglib::logQueue.push(obj); \
			::uv_async_send(&winreglib::logNotify); \
		}

	#define WLOG_DEBUG(ns, wmsg) \
		{ \
			std::u16string u16buffer(wmsg.begin(), wmsg.end()); \
			std::shared_ptr<winreglib::LogMessage> obj = std::make_shared<winreglib::LogMessage>(ns, u16buffer); \
			std::lock_guard<std::mutex> lock(winreglib::logLock); \
			winreglib::logQueue.push(obj); \
			::uv_async_send(&winreglib::logNotify); \
		}
#endif

#define LOG_DEBUG_FORMAT(ns, code) \
	{ \
		wchar_t* buffer = new wchar_t[1024]; \
		::code; \
		LOG_DEBUG(ns, buffer) \
	}

#define LOG_DEBUG_1(ns, msg, a1)             LOG_DEBUG_FORMAT(ns, swprintf(buffer, 1024, msg, a1))
#define LOG_DEBUG_2(ns, msg, a1, a2)         LOG_DEBUG_FORMAT(ns, swprintf(buffer, 1024, msg, a1, a2))
#define LOG_DEBUG_3(ns, msg, a1, a2, a3)     LOG_DEBUG_FORMAT(ns, swprintf(buffer, 1024, msg, a1, a2, a3))
#define LOG_DEBUG_4(ns, msg, a1, a2, a3, a4) LOG_DEBUG_FORMAT(ns, swprintf(buffer, 1024, msg, a1, a2, a3, a4))

#define LOG_DEBUG_THREAD_ID(ns, msg) \
	LOG_DEBUG_1(ns, msg " (thread %zu)", std::hash<std::thread::id>{}(std::this_thread::get_id()))

#define LOG_DEBUG_WIN32_ERROR(ns, message, code) \
	if (code != ERROR_SUCCESS) { \
		char errorMsg[512]; \
		::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, code, NULL, (LPTSTR)&errorMsg, 512, NULL); \
		TRIM_EXTRA_LINES(errorMsg); \
		LOG_DEBUG_2(ns, message "%hs (code %d)", errorMsg, code); \
	}

#endif
