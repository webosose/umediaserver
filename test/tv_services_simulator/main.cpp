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

#include <iostream>
#include <stdexcept>
#include <vector>
#include <sstream>
#include <stdio.h>
#include <cassert>
#include <string>
#include <exception>
#include <signal.h>
#include <execinfo.h>
#include <ucontext.h>
#include <boost/lexical_cast.hpp>  // for lexical_cast<string>(number)
#include <boost/filesystem/path.hpp>
#include <boost/noncopyable.hpp>
#include <functional>
#include <deque>
#include <mutex>
#include <condition_variable>

#include <pthread.h>
#include <thread>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cxxabi.h>
#include <sys/time.h>
#include <time.h>
#include <cstdlib> //std::system
#include <mcheck.h>
#include <stdint.h>
#include <glib.h>

#include <pbnjson.hpp>

#include <Logger.h>
#include <UMSConnector.h>

// 1) Create Service Files
// /usr/local/webos/usr/share/dbus-1/services
//    AND
// /usr/local/webos/usr/share/dbus-1/system-services
//
// Files:
// com.webos.service.videosinkmanager.service
// com.webos.service.tv.avblock.service
// com.webos.service.tv.display.service
// com.webos.service.tv.sound.service
//
// File Contents:
// Note: create for each service listed above.
//  more /usr/local/webos/usr/share/dbus-1/services/com.webos.service.tv.display.service
//
// [D-BUS Service]
// Name=com.webos.service.tv.display
// Exec=/usr/local/webos/usr/sbin/tvservices
// Type=static
//
// 2) Create Roles Files
// com.webos.service.videosinkmanager.json
// com.webos.service.tv.avblock
// com.webos.service.tv.display
// com.webos.service.tv.sound
//
// /usr/local/webos/usr/share/ls2/roles/prv
//           AND
// /usr/local/webos/usr/share/ls2/roles/pub
//
// File Contents:
// Note: create each file and update for the correct service name
//
//  /usr/local/webos/usr/share/ls2/roles/prv/com.webos.service.videosinkmanager.json
//
//  {
//      "role": {
//          "exeName":"/usr/local/webos/usr/sbin/tvservices",
//          "type": "regular",
//          "allowedNames": ["com.webos.service.videosinkmanager"]
//      },
//      "permissions": [
//          {
//              "service":"com.webos.service.videosinkmanager",
//              "inbound":["*"],
//              "outbound":["*"]
//          }
//      ]
//  }

using namespace std;
using namespace pbnjson;
using namespace uMediaServer;


#define MAX_MSG_SIZE 500
#define FLE (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define LOG_DEBUG(format, args...) do { \
		char msg[MAX_MSG_SIZE]; \
		sprintf(msg,format,##args);  \
		printf("<%s:%s(%d)> %s\n",FLE,__func__,__LINE__,msg); \
    } while(0)


static UMSConnector * connector_tv_display;
static UMSConnector * connector_tv_sound;
static UMSConnector * connector_tv_avblock;
static UMSConnector * connector_vsm;

// VSM Command
// Handle all VSM commands and report "success".
// TODO : later add option to report "failure"
//
// success: {"returnValue": true,"context : "pipeline_1"}
// failure: {
//     "returnValue": false,
//     "errorCode": "DISPLAY_ERROR_0000",
//     "errorText": "Invalid Context",
//     "context": "pipeline_1"
// }
//
bool vsmCommand(UMSConnectorHandle* sender,
		UMSConnectorMessage* message, void* ctxt)
{
	JDomParser parser;

	string cmd = connector_vsm->getMessageText(message);
	if (!parser.parse(cmd,  pbnjson::JSchema::AllSchema())) {
		return false;
	}

	JValue parsed = parser.getDom();
	string media_id = parsed["context"].asString();

	JValue response_obj = Object();

	// TODO allow this to be false for simulated error handling
	response_obj.put("context", media_id);
	response_obj.put("returnValue", true);

	string response = JGenerator::serialize(response_obj,  pbnjson::JSchema::AllSchema());

	LOG_DEBUG("VSM Simulator: response(%s)", response.c_str());

	connector_vsm->sendResponseObject(sender, message, response);
	return true;
}

// VSM Command
// Handle all VSM commands and report "success".
// TODO : later add option to report "failure"
//
// success: {"returnValue": true,"context : "pipeline_1"}
// failure: {
//     "returnValue": false,
//     "errorCode": "DISPLAY_ERROR_0000",
//     "errorText": "Invalid Context",
//     "context": "pipeline_1"
// }
//
bool tvCommand(UMSConnectorHandle* sender,
		UMSConnectorMessage* message, void* ctxt)
{
	JDomParser parser;
	string cmd = connector_vsm->getMessageText(message);
	if (!parser.parse(cmd,  pbnjson::JSchema::AllSchema())) {
		return false;
	}

	JValue parsed = parser.getDom();
	string media_id = parsed["context"].asString();
	JValue response_obj = Object();

	// TODO allow this to be false for simulated error handling
	response_obj.put("context", media_id);
	response_obj.put("returnValue", true);

	string response = JGenerator::serialize(response_obj,  pbnjson::JSchema::AllSchema());
	LOG_DEBUG("TV Simulator: response(%s)", response.c_str());
	connector_vsm->sendResponseObject(sender, message, response);
	return true;
}

int main(int argc, char** argv)
{
	LOG_DEBUG("BEGIN: TV Services Simulator");

	try {

		// setup TV and VSM simulated service interfaces
		connector_tv_display = new UMSConnector("com.webos.service.tv.display",
				nullptr, nullptr, UMS_CONNECTOR_DUAL_BUS);

		connector_tv_sound = new UMSConnector("com.webos.service.tv.sound",
				nullptr, nullptr, UMS_CONNECTOR_DUAL_BUS);

		connector_tv_avblock = new UMSConnector("com.webos.service.tv.avblock",
				nullptr, nullptr, UMS_CONNECTOR_DUAL_BUS);

		connector_vsm = new UMSConnector("com.webos.service.videosinkmanager",
				nullptr, nullptr, UMS_CONNECTOR_DUAL_BUS);

		// VSM
		// palm://com.webos.service.videosinkmanager/private/register
		// palm://com.webos.service.videosinkmanager/private/unregister
		// palm://com.webos.service.videosinkmanager/private/connect
		// palm://com.webos.service.videosinkmanager/private/disconnect
		connector_vsm->addEventHandler("register", vsmCommand, "private");
		connector_vsm->addEventHandler("unregister", vsmCommand, "private");
		connector_vsm->addEventHandler("connect", vsmCommand);
		connector_vsm->addEventHandler("disconnect", vsmCommand);

		// TV Display
		// luna://com.webos.service.tv.display/private/setDisplayWindow
		// luna://com.webos.service.tv.display/private/setCustomDisplayWindow
		// luna://com.webos.service.tv.display/private/setMediaVideoData
		connector_tv_display->addEventHandler("setDisplayWindow", tvCommand);
		connector_tv_display->addEventHandler("setCustomDisplayWindow", tvCommand);
		connector_tv_display->addEventHandler("setMediaVideoData", tvCommand, "private");
		connector_tv_display->addEventHandler("getScreenStatus", tvCommand);

		// TV Sound
		// luna://com.webos.service.tv.sound/private/connect
		connector_tv_sound->addEventHandler("connect", tvCommand, "private");

		// TV AV Block
		// luna://com.webos.service.tv.avblock/startAvMute
		// luna://com.webos.service.tv.avblock/startMute
		// luna://com.webos.service.tv.avblock/stopMute
		connector_tv_avblock->addEventHandler("startAvMute", tvCommand);
		connector_tv_avblock->addEventHandler("startMute", tvCommand);
		connector_tv_avblock->addEventHandler("stopMute", tvCommand);

		thread vsm_wait([&]()->void { connector_vsm->wait(); });
		thread tv_display_wait([&]()->void { connector_tv_display->wait(); });
		thread tv_sound_wait([&]()->void { connector_tv_sound->wait(); });
		thread tv_avblock_wait([&]()->void { connector_tv_avblock->wait(); });

		vsm_wait.join();
		tv_display_wait.join();
		tv_sound_wait.join();
		tv_avblock_wait.join();

	}
	catch (std::exception& e) {
		printf("Exception Received: %s\n", e.what());
	}

	LOG_DEBUG("END: TV Services Simulator");
	return true;
}
