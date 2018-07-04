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

#ifndef LOGGER_H
#define LOGGER_H

#include <string.h>

#ifdef __cplusplus
#include <iostream>
#include <cassert>
#include <string>
#else //__cplusplus
#include <assert.h>
#endif

#include <PmLogLib.h>

#define UMS_LOG_CONTEXT                           "ums.default"
#define UMS_LOG_CONTEXT_SERVER                    "ums.server"
#define UMS_LOG_CONTEXT_RESOURCE_MANAGER          "ums.resource_mgr"
#define UMS_LOG_CONTEXT_RESOURCE_MANAGER_CLIENT   "ums.resource_mgr_client"
#define UMS_LOG_CONTEXT_PIPELINE_MANAGER          "ums.pipeline_mgr"
#define UMS_LOG_CONTEXT_PIPELINE                  "ums.pipeline"
#define UMS_LOG_CONTEXT_PIPELINE_CONTROLLER       "ums.pipeline_ctrl"
#define UMS_LOG_CONTEXT_PROCESS_CONTROLLER        "ums.process_ctrl"
#define UMS_LOG_CONTEXT_CLIENT                    "ums.client"
#define UMS_LOG_CONTEXT_CONNECTOR                 "ums.connector"
#define UMS_LOG_CONTEXT_MDC                       "ums.media_display_controller"
#define UMS_LOG_CONTEXT_AUDIOD                    "ums.audiod"

#define LOG_LEVEL_CRITICAL  "CRIT"
#define LOG_LEVEL_ERROR     "ERR"
#define LOG_LEVEL_WARNING   "WARN"
#define LOG_LEVEL_INFO      "INFO"
#define LOG_LEVEL_DEBUG     "DEBUG"

#ifdef __cplusplus
namespace uMediaServer {
#endif //__cplusplus

#define UMEDIASERVER_UNIQUE_ID_LENGTH 15

typedef PmLogLevel LogLevel;
/**
 * Logging context with bound unique id
 */
typedef struct {
	PmLogContext ctx;
	char         uid[UMEDIASERVER_UNIQUE_ID_LENGTH + 1];
	LogLevel     level;
} LogContext;


/**
 * Sets the logging level for specified context
 * @param logging context pointer
 * @param desired logging level
 * @return error code
 */
inline PmLogErr SetLogLevel(LogContext * context, LogLevel level) {
	if (context == NULL)
		return kPmLogErr_InvalidParameter;
	context->level = level;
	return kPmLogErr_None;
};

/**
 * Sets the logging level for specified context
 * @param logging context pointer
 * @param string alias for desired logging level
 * @return error code
 */
inline PmLogErr SetLogLevelStr(LogContext * context, const char * level) {
	if (context == NULL)
		return kPmLogErr_InvalidParameter;
	PmLogErr error = kPmLogErr_InvalidParameter;
	if (0 == strncmp(level, LOG_LEVEL_CRITICAL, strlen(LOG_LEVEL_CRITICAL))) {
		error = SetLogLevel(context, kPmLogLevel_Critical);
	}
	else if (0 == strncmp(level, LOG_LEVEL_ERROR, strlen(LOG_LEVEL_ERROR))) {
		error = SetLogLevel(context, kPmLogLevel_Error);
	}
	else if (0 == strncmp(level, LOG_LEVEL_WARNING, strlen(LOG_LEVEL_WARNING))) {
		error = SetLogLevel(context, kPmLogLevel_Warning);
	}
	else if (0 == strncmp(level, LOG_LEVEL_INFO, strlen(LOG_LEVEL_INFO))) {
		error = SetLogLevel(context, kPmLogLevel_Info);
	}
	else if (0 == strncmp(level, LOG_LEVEL_DEBUG, strlen(LOG_LEVEL_DEBUG))) {
		error = SetLogLevel(context, kPmLogLevel_Debug);
	}
	return error;
};

/**
 * Sets the unique id bound to specified context
 * @param logging context pointer
 * @param unique id
 * @return error code
 */
inline PmLogErr SetUniqueId(LogContext * context, const char * uniqueId) {
	if (context == NULL)
		return kPmLogErr_InvalidParameter;
	strncpy(context->uid, uniqueId, UMEDIASERVER_UNIQUE_ID_LENGTH);
	return kPmLogErr_None;
};

/**
 * Acquires logging context
 * @param logging context name
 * @param pointer to logging context to be assigned
 * @return error code
 */
inline PmLogErr AcquireLogContext(const char * contextName, LogContext * context) {
	if (context == NULL)
		return kPmLogErr_InvalidParameter;
	memset(context->uid, 0, UMEDIASERVER_UNIQUE_ID_LENGTH + 1);
	PmLogErr error = PmLogGetContext(contextName, &context->ctx);
	if (error != kPmLogErr_None)
		return error;
	error = SetLogLevel(context, kPmLogLevel_Debug);
	return error;
};

#ifdef __cplusplus
/**
 * Wrapper for logging context
 */
struct Logger : LogContext {
	/**
	 * Aquires context and optionaly sets unique id
	 * @param logging context name
	 * @param unique id
	 */
	Logger(const std::string & logContext = UMS_LOG_CONTEXT, const std::string & uniqueId = "") {
		ctx = NULL;
		AcquireLogContext(logContext.c_str(), this);
		SetUniqueId(this, uniqueId.c_str());
	};
	/**
	 * Sets logging level
	 * @param desired logging level
	 */
	void setLogLevel(LogLevel level) {
		SetLogLevel(this, level);
	};
	/**
	 * Sets logging level by name
	 * @param string alias for desired logging level
	 */
	void setLogLevel(const std::string & level) {
		SetLogLevelStr(this, level.c_str());
	};
	/**
	 * Sets unique id
	 * @param unique id to bound to context
	 */
	void setUniqueId(const std::string & uniqueId) {
		SetUniqueId(this, uniqueId.c_str());
	};
};

} // name space uMediaServer
#endif //__cplusplus

#endif // LOGGER_H
