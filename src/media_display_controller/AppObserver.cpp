// Copyright (c) 2015-2018 LG Electronics, Inc.
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
const std::string lsm_foreground_app_info_call = "palm://com.webos.surfacemanager/getForegroundAppInfo";
const std::string sam_foreground_app_info_call = "palm://com.webos.applicationManager/getForegroundAppInfo";
const std::string sam_app_life_status_call     = "palm://com.webos.applicationManager/getAppLifeStatus";
const std::string subscription_message         = "{\"subscribe\":true}";
const std::string lsm_subscription_key         = "foregroundAppInfo";
Logger _log(UMS_LOG_CONTEXT_MDC);
}

AppObserver::AppObserver(UMSConnector * umc, fg_state_callback_t && fg_state_change_cb)
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

	if (!parsed.hasKey("appId")) {
		LOG_ERROR(_log, MSGERR_FG_RESUBSCRIBE, "Failed reply from getForegroundAppInfo: %s", event.c_str());
		return false;
	}

	self->fg_apps.insert(parsed["appId"].asString());

	if (self->fg_state_callback)
		self->fg_state_callback(self->fg_apps);

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
		self->connector->sendMessage(sam_foreground_app_info_call, "{}", foregroundAppsCallback, self);
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

	std::string app_id = parsed["appId"].asString();
	std::string status = parsed["status"].asString();

	if (status == "closing" || status == "stop" || status == "background") {
		auto result = self->fg_apps.erase(app_id);
		if (result && self->fg_state_callback)
			self->fg_state_callback(self->fg_apps);
	} else if (status == "launching" || status == "relaunching" || status == "foreground") {
		auto result = self->fg_apps.insert(app_id);
		if (result.second && self->fg_state_callback)
			self->fg_state_callback(self->fg_apps);
	}

	return true;
}

}
