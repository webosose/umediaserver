// Copyright (c) 2008-2020 LG Electronics, Inc.
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

#ifndef __APP_LIFE_MANAGER_H__
#define __APP_LIFE_MANAGER_H__

#include <GLibHelper.h>
#include <functional>
#include <UMSConnector.h>
#include <map>
#include <set>
#include <Logger_id.h>
#include <Logger_macro.h>
#include <pbnjson.hpp>

#include "uMediaTypes.h"

namespace uMediaServer {

class AppLifeManager {
public:
	enum class app_status_event_t {
		DELAY = 0,
		BACKGROUND,
		FOREGROUND,
		FOCUS
	};

	const std::string RESERVED_APP = "reserved";
	const std::string DEFAULT_TYPE = "card";

	AppLifeManager(UMSConnector * umc);
	~AppLifeManager();

	struct application_connections_t {
		std::string app_id;
		AppLifeStatus status;
		std::string window_type;
		int32_t display_id;
		int32_t pid;
		std::set<std::string> connections;

		application_connections_t()
						: app_id(""), status(AppLifeStatus::UNKNOWN),
						window_type(""), display_id(-1), pid(-1),
						connections() {}
	};

	void registerConnection(const std::string& app_id, const std::string& connection_id, bool reserved = false);
	void unregisterConnection(const std::string& connection_id);
	application_connections_t* getAppConnection(const std::string& connection_id);
	bool getForegroundAppsInfo(pbnjson::JValue& forground_app_info_out);
 	bool notifyForegroundAppsInfo();
	void updateAppStatus(const std::string app_id, AppLifeStatus status, const std::string& window_type, int32_t display_id, int32_t pid);
	void updateConnectionStatus(const std::string& connection_id, AppLifeManager::app_status_event_t status);

	bool isForeground(const std::string& app_id);

	bool setAppId(const std::string& connection_id, const std::string& new_app_id, bool reserved);

	bool getDisplayId(const std::string& app_id, int32_t *diplay_id);

	std::string getAppId(const std::string& connection_id);

	void setConnectionStatusCallback(
		const std::function<bool(const std::string&, app_status_event_t, const std::string& type)> & cb);

	void setPipelineStatusCallback(
		const std::function<bool(const std::string&, pbnjson::JValue& connection_info_out)> & cb);

private:
	AppLifeStatus getStatus(const std::string& app_id);

	UMSConnector * connector;
	std::map<std::string, application_connections_t> _apps;
	std::function<bool(const std::string& conenction_id,
		app_status_event_t app_status_event,
		const std::string& type)> connection_status_changed_callback;
	std::function<bool(const std::string&, pbnjson::JValue& connection_info_out)> pipeline_status_callback;
	std::recursive_mutex mutex;
	typedef std::lock_guard<std::recursive_mutex> lock_t;
};

}

#endif
