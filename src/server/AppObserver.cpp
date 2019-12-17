// Copyright (c) 2015-2019 LG Electronics, Inc.
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
//

#include <pbnjson.hpp>
#include <Logger_id.h>
#include <Logger_macro.h>
#include "AppObserver.h"

namespace uMediaServer {

namespace {
const std::string sam_foreground_app_info_call = "palm://com.webos.applicationManager/getForegroundAppInfo";
const std::string sam_app_life_status_call     = "palm://com.webos.applicationManager/getAppLifeStatus";
const std::string subscription_message         = "{\"subscribe\":true}";
const std::string foreground_app_info_message  = "{\"extraInfo\":true, \"subscribe\":false}";
const std::string lsm_subscription_key         = "foregroundAppInfo";
Logger _log(UMS_LOG_CONTEXT_SERVER);
}

AppObserver::AppObserver(UMSConnector * umc, app_state_callback_t && fg_state_change_cb)
	: connector(umc), fg_state_callback(fg_state_change_cb) {
	connector->subscribe(sam_app_life_status_call, subscription_message, appLifeStatusCallback, this);
}

bool AppObserver::foregroundAppsCallback(UMSConnectorHandle*, UMSConnectorMessage* msg, void* ctx) {
	AppObserver * self = static_cast<AppObserver*>(ctx);
	pbnjson::JDomParser parser;
	std::string event = self->connector->getMessageText(msg);

	if (!parser.parse(event, pbnjson::JSchema::AllSchema()))
	{
		LOG_ERROR(_log, MSGERR_JSON_PARSE, "Failed to parse from AppObserver::getForegroundAppInfo: %s", event.c_str());
		return false;
	}

	pbnjson::JValue parsed = parser.getDom();

	LOG_INFO(_log, MSGNFO_APP_STATUS_UPDATE, "fgAppsEvt: %s", event.c_str());

	if (!parsed.hasKey("foregroundAppInfo")) {
		LOG_ERROR(_log, MSGERR_FG_RESUBSCRIBE, "Failed reply from getForegroundAppInfo: %s", event.c_str());
		return false;
	}

	if (self->fg_state_callback) {
		for (int i = 0; i < parsed["foregroundAppInfo"].arraySize(); i++) {
			AppLifeStatus app_life_status = self->unmarshallAppLifeStatus("foreground");
			std::string app_window_type = self->convertAppWindowType(parsed["foregroundAppInfo"][i]["windowType"].asString());
			int32_t displayId = parsed["foregroundAppInfo"][i]["displayId"].asNumber<int32_t>();
			int32_t pid = parsed["foregroundAppInfo"][i]["processId"].asNumber<int32_t>();
			self->fg_state_callback(parsed["foregroundAppInfo"][i]["appId"].asString(), app_life_status, app_window_type, displayId, pid);
		}
	}

	return true;
}

bool AppObserver::appLifeStatusCallback(UMSConnectorHandle*, UMSConnectorMessage* msg, void* ctx) {
	AppObserver * self = static_cast<AppObserver*>(ctx);
	pbnjson::JDomParser parser;
	std::string event = self->connector->getMessageText(msg);

	if (! parser.parse(event, pbnjson::JSchema::AllSchema()))
	{
		LOG_ERROR(_log, MSGERR_JSON_PARSE, "Failed to parse from AppObserver::appLifeStatusCallback: %s", event.c_str());
		return false;
	}

	pbnjson::JValue parsed = parser.getDom();

	LOG_INFO(_log, MSGNFO_APP_STATUS_UPDATE, "appLifeStatusEvt: %s", event.c_str());

	if (parsed.hasKey("subscribed") && parsed["subscribed"].asBool()) {
		return true;
	}

	if (!parsed.hasKey("appId") || !parsed.hasKey("status")) {
		LOG_WARNING(_log, MSGERR_FG_RESUBSCRIBE,
					"Failed subscription reply from getAppLifeStatus: %s", event.c_str());
		self->app_state_subscribe_timer = GMainTimer(100, [self] {
			self->connector->subscribe(sam_app_life_status_call, subscription_message, appLifeStatusCallback, self);
		});
		return false;
	}

	if (parsed["status"] == "foreground")
		self->connector->sendMessage(sam_foreground_app_info_call, foreground_app_info_message, foregroundAppsCallback, self);
	else {
		std::string app_id = parsed["appId"].asString();
		std::string status = parsed["status"].asString();
		std::string window_type = parsed["windowType"].asString();

		if (app_id.empty() || status.empty()) {
			LOG_WARNING(_log, MSGERR_INVALID_ARG, "Empty value. appId[%s], status[%s], windowType[%s]", app_id.c_str(), status.c_str(), window_type.c_str());
			return false;
		}
		AppLifeStatus app_life_status = self->unmarshallAppLifeStatus(status);
		std::string app_window_type = self->convertAppWindowType(window_type);
		if (self->fg_state_callback) {
			self->fg_state_callback(app_id, app_life_status, app_window_type, 0, -1);
		}
	}
	return true;
}

AppLifeStatus AppObserver::unmarshallAppLifeStatus(const std::string& value) {
	if (value == "launching") {
		return AppLifeStatus::LAUNCHING;
	} else if (value == "relaunching") {
	return AppLifeStatus::RELAUNCHING;
	} else if (value == "foreground") {
		return AppLifeStatus::FOREGROUND;
	} else if (value == "closing") {
		return AppLifeStatus::CLOSING;
	} else if (value == "stop") {
		return AppLifeStatus::STOP;
	} else if (value == "background") {
		return AppLifeStatus::BACKGROUND;
	} else {
		return AppLifeStatus::UNKNOWN;
	}
}

std::string AppObserver::convertAppWindowType(const std::string& value) {
	std::string app_type = "card";

	static struct {
		const std::string app_type;
		const std::string config_type;
	} appTypeMap[] = {
		{ "card",          "_WEBOS_WINDOW_TYPE_CARD" },
		{ "floating",      "_WEBOS_WINDOW_TYPE_FLOATING" },
		{ "overlay",       "_WEBOS_WINDOW_TYPE_OVERLAY" }
	};

	for (auto itr = std::begin(appTypeMap); itr != std::end(appTypeMap); itr++) {
		if (itr->config_type == value) {
			app_type = itr->app_type;
		}
	}
	return app_type;
}

bool AppObserver::isEnable() {
	return (connector != nullptr);
}

}
