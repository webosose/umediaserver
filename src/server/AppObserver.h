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

#ifndef __APPOBSERVER_H__
#define __APPOBSERVER_H__

#include <GLibHelper.h>
#include <set>
#include <functional>
#include <UMSConnector.h>
#include <uMediaTypes.h>

namespace uMediaServer {
class AppObserver {
public:
	typedef std::function<void(const std::string& app_id, AppLifeStatus status, std::string app_window_type, int32_t display_id, int32_t pid)> app_state_callback_t;
	AppObserver(UMSConnector * umc, app_state_callback_t && fg_state_change_cb);

  AppLifeStatus unmarshallAppLifeStatus(const std::string& value);
  std::string convertAppWindowType(const std::string& value);

  bool isEnable();

private:
	static bool foregroundAppsCallback(UMSConnectorHandle*, UMSConnectorMessage*, void*);
	static bool appLifeStatusCallback(UMSConnectorHandle*, UMSConnectorMessage*, void*);

	UMSConnector * connector;
	app_state_callback_t fg_state_callback;
	std::set<std::string> fg_apps;
	GMainTimer app_state_subscribe_timer;
};
}

#endif
