// Copyright (c) 2008-2018 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#ifndef LOGGER_MACRO_H
#define LOGGER_MACRO_H

#include <string.h>
#include <stdio.h>
#include <Logger.h>
#include <time.h>
#include "Logger_id.h"

#define MAX_CP_SIZE 256     // max codepoint "<file:func(line)>\0" size
#define MAX_TS_SIZE 32      // max timestamp "sec.nsec\0" size
#define MAX_FT_SIZE 512     // max size for free text formatted message
#define MAX_KV_SIZE 512     // max size for json formatted key-values

#define KVP_FORMAT_COMPLETE  "{\"%s\":\"%s\",\"%s\":%s,\"%s\":\"%s\"}"
#define KVP_FORMAT_TRUNCATED "{\"%s\":%s,\"%s\":\"%s\"}"

#define __EXTRACT_FILE(__path) (strrchr(__path, '/') ? strrchr(__path, '/') + 1 : __path)
#define __FLE __EXTRACT_FILE(__FILE__)
#define __CODEPOINT(__codepoint, __file, __func, __line) do {\
			snprintf(__codepoint, MAX_CP_SIZE, "<%s:%s(%d)>", __file, __func, __line);\
		} while(0)
#define __TIMESTAMP(__timestamp) do {\
			struct timespec __time;\
			clock_gettime(CLOCK_MONOTONIC, &__time);\
			snprintf(__timestamp, MAX_TS_SIZE, "%ld.%09ld", __time.tv_sec, __time.tv_nsec);\
		} while(0)
#define __MAKE_KVP(__ctx, __kvp, __offset) do {\
			char __timestamp[MAX_TS_SIZE]; __TIMESTAMP(__timestamp);\
			char __codepoint[MAX_CP_SIZE]; __CODEPOINT(__codepoint, __FLE, __func__, __LINE__);\
			if (__ctx.uid[0]) {\
				__offset = snprintf(__kvp, MAX_KV_SIZE, KVP_FORMAT_COMPLETE,\
									KVP_SESSION_ID, __ctx.uid,\
									KVP_TIMESTAMP, __timestamp,\
									KVP_CODE_POINT, __codepoint);\
			}\
			else {\
				__offset = snprintf(__kvp, MAX_KV_SIZE, KVP_FORMAT_TRUNCATED,\
									KVP_TIMESTAMP, __timestamp,\
									KVP_CODE_POINT, __codepoint);\
			}\
		} while(0)

#define LOG_PRINT(__ctx, __level, __msgid, __format, ...) do {\
			PmLogLevel __global_level;\
			if (PmLogGetContextLevel(__ctx.ctx, &__global_level) == kPmLogErr_None &&\
				__ctx.level >= __level && __global_level >= __level) {\
				int __offset;\
				char __kvp[MAX_KV_SIZE]; __MAKE_KVP(__ctx, __kvp, __offset);\
				char __message[MAX_FT_SIZE];\
				snprintf(__message, MAX_FT_SIZE, __format, ##__VA_ARGS__);\
				PmLogString(__ctx.ctx, __level, __msgid, __kvp, __message);\
			}\
		} while(0)
/**
 * Logs info level message
 * @param logging context
 * @param unique message tag
 * @param log string format
 * @param list of arguments for log message format
 */
#define LOG_INFO(__ctx, __msgid, __format, ...)\
	LOG_PRINT(__ctx, kPmLogLevel_Info, __msgid, __format, ##__VA_ARGS__)
/**
 * Logs warning level message
 * @param logging context
 * @param unique message tag
 * @param log string format
 * @param list of arguments for log message format
 */
#define LOG_WARNING(__ctx, __msgid, __format, ...)\
	LOG_PRINT(__ctx, kPmLogLevel_Warning, __msgid, __format, ##__VA_ARGS__)
/**
 * Logs error level message
 * @param logging context
 * @param unique message tag
 * @param log string format
 * @param list of arguments for log message format
 */
#define LOG_ERROR(__ctx, __msgid, __format, ...)\
	LOG_PRINT(__ctx, kPmLogLevel_Error, __msgid, __format, ##__VA_ARGS__)
/**
 * Logs critical level message
 * @param logging context
 * @param unique message tag
 * @param log string format
 * @param list of arguments for log message format
 */
#define LOG_CRITICAL(__ctx, __msgid, __format, ...)\
	LOG_PRINT(__ctx, kPmLogLevel_Critical, __msgid, __format, ##__VA_ARGS__)
/**
 * Logs debug level message
 * @param logging context
 * @param log string format
 * @param list of arguments for log message format
 */
#define LOG_DEBUG(__ctx, __format, ...) do {\
			PmLogLevel __global_level;\
			if (PmLogGetContextLevel(__ctx.ctx, &__global_level) == kPmLogErr_None &&\
				__ctx.level == kPmLogLevel_Debug && __global_level == kPmLogLevel_Debug) {\
				int __offset;\
				char __message[MAX_KV_SIZE]; __MAKE_KVP(__ctx, __message, __offset);\
				if (__offset + 1 < MAX_FT_SIZE) {\
					__message[__offset] = ' ';\
					snprintf(__message + __offset + 1, MAX_FT_SIZE - __offset -1,\
								__format, ##__VA_ARGS__);\
				}\
				PmLogString(__ctx.ctx, kPmLogLevel_Debug, NULL, NULL, __message);\
			}\
		} while(0)

#ifndef NDEBUG
  /**
   * Logs trace message. Disabled in the release build.
   * @param logging context
   * @param log string format
   * @param list of arguments for log message format
   */
  #define LOG_TRACE(__ctx, __format, ...)\
	LOG_DEBUG(__ctx, __format, ##__VA_ARGS__)
#else
  #define LOG_TRACE(__ctx, __format, ...)
#endif

#ifdef __cplusplus
#include <map>
#include <string>
#include <sstream>
#include <boost/variant.hpp>

typedef const char * kvp_key_t;
typedef boost::variant<
		bool,
		int,
		unsigned int,
		double,
		const char *,
		std::string
> kvp_val_t;
typedef std::map<kvp_key_t, kvp_val_t> __KV;

template <typename... Args>
inline void __log_print_ex(const uMediaServer::LogContext & ctx, PmLogLevel level,
		const char * msgid, const __KV & kvp,
		const char * file, const char * func, const int & line,
		const char * format, const Args& ... args) {
	PmLogLevel global_level;
	if (PmLogGetContextLevel(ctx.ctx, &global_level) == kPmLogErr_None &&
		ctx.level >= level && global_level >= level) {
		char codepoint[MAX_CP_SIZE]; __CODEPOINT(codepoint, file, func, line);
		char timestamp[MAX_TS_SIZE]; __TIMESTAMP(timestamp);
		std::stringstream kvpstream;
		kvpstream << "{";
		if (ctx.uid[0])
			kvpstream << "\"" << KVP_SESSION_ID << "\":\"" << ctx.uid << "\",";
		kvpstream << "\"" << KVP_TIMESTAMP << "\":" << timestamp << ",";
		kvpstream << "\"" << KVP_CODE_POINT << "\":\"" << codepoint << "\"";
		for (const auto & kv : kvp) {
			kvpstream << ",\"" << kv.first << "\":";
			switch(kv.second.which()) {
				case 0: /* boolean */
					kvpstream << (boost::get<bool>(kv.second) ? "true" : "false"); break;
				case 1: case 2: case 3: /* number */
					kvpstream << kv.second; break;
				default: /* string */
					kvpstream << "\"" << kv.second << "\""; break;
			}
		}
		kvpstream << "}";
		char message[MAX_FT_SIZE]; snprintf(message, MAX_FT_SIZE, format, args...);
		const char * str = kvpstream.str().c_str();
		PmLogString(ctx.ctx, level, msgid, kvpstream.str().c_str(), message);
	}
}

/**
 * Logs info level message with set of provided key-value pairs
 * @param logging context
 * @param unique message tag
 * @param std::map<kvp_key_t, kvp_val_t> that represents set of key-value pairs
 * @param log string format
 * @param list of arguments for log message format
 */
#define LOG_INFO_EX(__ctx, __msgid, __kvp, __format, ...)\
		__log_print_ex(__ctx, kPmLogLevel_Info, __msgid, __kvp,\
				__FLE, __func__, __LINE__, __format, ##__VA_ARGS__);
/**
 * Logs warning level message with set of provided key-value pairs
 * @param logging context
 * @param unique message tag
 * @param std::map<kvp_key_t, kvp_val_t> that represents set of key-value pairs
 * @param log string format
 * @param list of arguments for log message format
 */
#define LOG_WARNING_EX(__ctx, __msgid, __kvp, __format, ...)\
		__log_print_ex(__ctx, kPmLogLevel_Warning, __msgid, __kvp,\
				__FLE, __func__, __LINE__, __format, ##__VA_ARGS__);
/**
 * Logs error level message with set of provided key-value pairs
 * @param logging context
 * @param unique message tag
 * @param std::map<kvp_key_t, kvp_val_t> that represents set of key-value pairs
 * @param log string format
 * @param list of arguments for log message format
 */
#define LOG_ERROR_EX(__ctx, __msgid, __kvp, __format, ...)\
		__log_print_ex(__ctx, kPmLogLevel_Error, __msgid, __kvp,\
				__FLE, __func__, __LINE__, __format, ##__VA_ARGS__);
/**
 * Logs critical level message with set of provided key-value pairs
 * @param logging context
 * @param unique message tag
 * @param std::map<kvp_key_t, kvp_val_t> that represents set of key-value pairs
 * @param log string format
 * @param list of arguments for log message format
 */
#define LOG_CRITICAL_EX(__ctx, __msgid, __kvp, __format, ...)\
		__log_print_ex(__ctx, kPmLogLevel_Critical, __msgid, __kvp,\
				__FLE, __func__, __LINE__, __format, ##__VA_ARGS__);

#endif //____cplusplus

#endif //LOGGER_MACRO_H
