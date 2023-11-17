// Copyright (c) 2008-2021 LG Electronics, Inc.
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

#include "AppLifeManager.h"

#include <algorithm>
#include <Logger_id.h>
#include <Logger_macro.h>

namespace uMediaServer {

namespace {
Logger _log(UMS_LOG_CONTEXT_RESOURCE_MANAGER);
}

AppLifeManager::AppLifeManager(UMSConnector * umc)
	: connector(umc) {
	application_connections_t app;
	app.app_id = RESERVED_APP;
	app.status = AppLifeStatus::UNKNOWN;
	app.window_type = DEFAULT_TYPE;
	_apps[app.app_id] = app;
}

AppLifeManager::~AppLifeManager() {
}

void AppLifeManager::registerConnection(const std::string& app_id, const std::string& connection_id, bool reserved) {
	// specific pipeline's used without application
	if (app_id.empty()) {
		if (!reserved) {
			connection_status_changed_callback(connection_id, app_status_event_t::BACKGROUND, DEFAULT_TYPE);
		} else {
		    _apps[RESERVED_APP].connections.insert(connection_id);
			connection_status_changed_callback(connection_id, app_status_event_t::FOREGROUND, DEFAULT_TYPE);
		}
	} else {
		auto itr = _apps.find(app_id);
		if (itr != _apps.end()) {
			itr->second.connections.insert(connection_id);
			if (itr->second.status == AppLifeStatus::FOREGROUND) {
				connection_status_changed_callback(connection_id, app_status_event_t::FOREGROUND, itr->second.window_type);
			} else if  (itr->second.status == AppLifeStatus::LAUNCHING || itr->second.status == AppLifeStatus::RELAUNCHING) {
				connection_status_changed_callback(connection_id, app_status_event_t::DELAY, itr->second.window_type);
			} else {
				connection_status_changed_callback(connection_id, app_status_event_t::BACKGROUND, itr->second.window_type);
			}
		} else {
			application_connections_t app;
			app.app_id = app_id;
			app.status = AppLifeStatus::UNKNOWN;
			app.window_type = DEFAULT_TYPE;
			app.connections.insert(connection_id);
			_apps[app_id] = app;
		}
	}
	LOG_DEBUG(_log, "registerConnection done. app_id : %s, connection_id : %s, reserved : %d", app_id.c_str(), connection_id.c_str(), reserved);
}


void AppLifeManager::unregisterConnection(const std::string& connection_id) {
	typedef std::pair<std::string, application_connections_t> app_entry_t;

	auto func = [connection_id](const app_entry_t& p)->bool{
		return (p.second.connections.find(connection_id) != p.second.connections.end());
	};

	auto itr = std::find_if(_apps.begin(), _apps.end(), std::move(func));
	if (itr != _apps.end()) {
		itr->second.connections.erase(connection_id);
	}
}


AppLifeManager::application_connections_t* AppLifeManager::getAppConnection(const std::string& connection_id) {
	typedef std::pair<std::string, application_connections_t> app_entry_t;

	auto func = [connection_id](const app_entry_t& p)->bool{
		return (p.second.connections.find(connection_id) != p.second.connections.end());
	};

	auto itr = std::find_if(_apps.begin(), _apps.end(), func);
	if (itr == _apps.end()) {
		return nullptr;
	}

	return (&itr->second);
}

bool AppLifeManager::getForegroundAppsInfo(pbnjson::JValue& forground_app_info_out) {
	for (auto app : _apps) {
		if (app.second.status == AppLifeStatus::FOREGROUND) {
			pbnjson::JValue app_obj = pbnjson::Object();
			pbnjson::JValue conn_array = pbnjson::Array();
			app_obj.put("appId",app.second.app_id);
			app_obj.put("displayId",app.second.display_id);
			for (const auto &conn : app.second.connections) {
				pbnjson::JValue conn_obj = pbnjson::Object();
				if (pipeline_status_callback)
					pipeline_status_callback(conn, conn_obj);
				conn_array.append(conn_obj);
			}
			app_obj.put("pipelineInfo",conn_array);
			forground_app_info_out.append(app_obj);
		}
	}

	return true;
}

bool AppLifeManager::notifyForegroundAppsInfo() {
	lock_t l(mutex);

	pbnjson::JValue subscriptionEvent = pbnjson::Object();
	pbnjson::JValue foregroundAppInfoArray = pbnjson::Array();
	getForegroundAppsInfo(foregroundAppInfoArray);

	subscriptionEvent.put("subscribed", true);
	subscriptionEvent.put("returnValue", true);
	subscriptionEvent.put("foregroundAppInfo", foregroundAppInfoArray);
	connector->sendChangeNotificationJsonString(subscriptionEvent.stringify(), "getForegroundAppInfo");
	return true;
}


void AppLifeManager::updateAppStatus(const std::string app_id, AppLifeStatus status, const std::string& app_window_type, int32_t display_id, int32_t pid) {
	if (connection_status_changed_callback) {
		auto itr = _apps.find(app_id);
		if (itr != _apps.end()) {
			_apps[app_id].status = status;
			_apps[app_id].display_id = display_id;
			_apps[app_id].pid = pid;
			if (!app_window_type.empty()) {
				_apps[app_id].window_type = app_window_type;
			}

			switch (status) {
				case AppLifeStatus::FOREGROUND:
					for (auto conn: _apps[RESERVED_APP].connections) {
						connection_status_changed_callback(conn, app_status_event_t::BACKGROUND, _apps[RESERVED_APP].window_type);
					}
					for (auto conn: _apps[app_id].connections) {
						connection_status_changed_callback(conn, app_status_event_t::FOREGROUND, _apps[app_id].window_type);
					}
					break;
				case AppLifeStatus::LAUNCHING:
				case AppLifeStatus::RELAUNCHING:
					for (auto conn: _apps[app_id].connections) {
						connection_status_changed_callback(conn, app_status_event_t::DELAY, _apps[app_id].window_type);
					}
					break;
				case AppLifeStatus::BACKGROUND:
					for (const auto &conn: _apps[app_id].connections) {
						connection_status_changed_callback(conn, app_status_event_t::BACKGROUND, _apps[app_id].window_type);
					}
					break;
				case AppLifeStatus::CLOSING:
					break;
				case AppLifeStatus::STOP:
					for (auto conn: _apps[app_id].connections) {
						connection_status_changed_callback(conn, app_status_event_t::BACKGROUND, _apps[app_id].window_type);
					}
					_apps.erase(app_id);
					break;
				default:
					break;
			}
		} else {
			application_connections_t app;
			app.app_id = app_id;
			app.status = status;
			app.display_id = display_id;
			app.pid = pid;
			app.window_type = app_window_type;
			_apps[app_id] = std::move(app);
		}
	}
	notifyForegroundAppsInfo();
}

void AppLifeManager::updateConnectionStatus(const std::string& connection_id, AppLifeManager::app_status_event_t status)
{
	if (!connection_id.empty() && connection_status_changed_callback) {
		std::string app_id = getAppId(connection_id);
		connection_status_changed_callback(connection_id, status, _apps[app_id].window_type);
	}
}


void AppLifeManager::setConnectionStatusCallback(
		const std::function<bool(const std::string&, AppLifeManager::app_status_event_t, const std::string&)> & cb) {
	connection_status_changed_callback = cb;
}

void AppLifeManager::setPipelineStatusCallback(
		const std::function<bool(const std::string&, pbnjson::JValue& connection_info_out)> & cb) {
	pipeline_status_callback = cb;
}

bool AppLifeManager::isForeground(const std::string& app_id) {
	auto app = _apps.find(app_id);
	if (app != _apps.end() && _apps[app_id].status == AppLifeStatus::FOREGROUND) {
		return true;
	}
	return false;
}

bool AppLifeManager::setAppId(const std::string& connection_id, const std::string& new_app_id, bool reserved) {
	if (connection_id.empty()) {
		return false;
	}
	unregisterConnection(connection_id);
	registerConnection(new_app_id, connection_id, reserved);

	return true;
}

std::string AppLifeManager::getAppId(const std::string& connection_id)
{
	std::string ret = std::string();
	typedef std::pair<std::string, application_connections_t> app_entry_t;

	auto func = [connection_id](const app_entry_t& p)->bool{
		return (p.second.connections.find(connection_id) != p.second.connections.end());
	};

	auto itr = std::find_if(_apps.begin(), _apps.end(), std::move(func));
	if (itr != _apps.end()) {
		ret = itr->second.app_id;
	}

	return ret;
}

bool AppLifeManager::getDisplayId(const std::string& app_id, int32_t *display_id)
{
	*display_id = atoi (&app_id.back());
	return true;
}



} //namespace uMediaServer
