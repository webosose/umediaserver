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
//

#include <algorithm>
#include <iterator>
#include <MediaDisplayController.h>
#include <Logger_macro.h>
#include <boost/algorithm/string.hpp>
#include "ControlInterface.h"
#ifdef USE_AVOUTPUTD
#include "AVOutputContextlessDisplayConnector.h"
typedef ::uMediaServer::AVOutputContextlessDisplayConnector WEBOS_CONNECTOR_IMPLEMENTATION;
#else
#include "TVDisplayConnector.h"
typedef ::uMediaServer::TVDisplayConnector WEBOS_CONNECTOR_IMPLEMENTATION;
#endif

// -------------------
// Main MDC logic to control output state of display
//
// VSM interface requires operations to happen in a specific order
//  while all inputs required can arrive in any order. To force
//  correct order MDC synchronizes on incoming inputs.
//
// VSM Required Order
// 1) registration with Media Content Providers allocated resources
// 2) Connect Media Object (media_id) to a display (MAIN/SUB)
// 3) blank
// 3) Set all parameters
//    i) Display Window dimensions
//    ii) Video Info Meta data
// 4) unblank
//

using namespace std;
using namespace pbnjson;
using namespace boost;

namespace uMediaServer {

using namespace mdc;
using namespace mdc::event;

namespace {
	Logger log(UMS_LOG_CONTEXT_MDC);
}

#define RETURN_IF(exp,rv,msgid,format,args...) \
{ if(exp) { \
	LOG_ERROR(log, msgid, format, ##args); \
	return rv; \
} \
}

MediaDisplayController * MediaDisplayController::instance(UMSConnector *connector)
{
	static MediaDisplayController* instance = nullptr;
	if (!instance)
		instance = new MediaDisplayController(connector);

	return instance;
}

MediaDisplayController::MediaDisplayController(UMSConnector *connector)
	: connector_(connector), acb_spy(connector), layout_manager(connection_policy, 1920, 1080)
	, app_observer(connector, [this](const std::set<std::string> & fg){ foregroundEvent(fg); })
	, tv_display(new WEBOS_CONNECTOR_IMPLEMENTATION(connector_, connection_policy.video()))
	, connection_policy(acb_spy, audio_connections)
{
	LOG_DEBUG(log, "[MediaDisplayController]");

	connector_->addEventHandler("focus", focusCallBack, UMS_CONNECTOR_PUBLIC_BUS);
	connector_->addEventHandler("setDisplayWindow", setDisplayWindowCallBack, UMS_CONNECTOR_PUBLIC_BUS);
	connector_->addEventHandler("switchToFullScreen", switchToFullScreenCallBack, UMS_CONNECTOR_PUBLIC_BUS);
	connector_->addEventHandler("switchToAutoLayout", switchToAutoLayoutCallBack, UMS_CONNECTOR_PUBLIC_BUS);
	connector_->addEventHandler("setVisibility", setVisibilityCallBack, UMS_CONNECTOR_PUBLIC_BUS);
	connector_->addEventHandler("getForegroundAppInfo", getForegroundAppInfoCallBack, UMS_CONNECTOR_PUBLIC_BUS);

	connector_->addEventHandler("unregisterMedia", unregisterMediaCallBack, UMS_CONNECTOR_PUBLIC_BUS);
	connector_->addEventHandler("setVideoInfo", setVideoInfoCallBack, UMS_CONNECTOR_PRIVATE_BUS);
	connector_->addEventHandler("contentReady", contentReadyCallBack, UMS_CONNECTOR_PRIVATE_BUS);
	connector_->addEventHandler("updatePipelineStateEvent", pipelineEventsCallBack, UMS_CONNECTOR_PUBLIC_BUS);

	// TEST event handlers
	connector_->addEventHandler("testPipelineEvent", pipelineEventsCallBack, UMS_CONNECTOR_PRIVATE_BUS);
	connector_->addEventHandler("enableDebug", enableDebugCallBack, UMS_CONNECTOR_PRIVATE_BUS);

	tv_display->vsm_set_registration_callback([this](const std::string & id, bool result){
		auto it = media_elements_.find(id);
		if (it != media_elements_.end()) {
			if (result)
				it->second.media->process_event(event::Registered());
			else
				LOG_WARNING(log, MSGERR_VSM_REGISTER_ERR, "failed to register with vsm : %s", id.c_str());
		} else
			LOG_WARNING(log, MSGERR_UNHANDLED_REPLY, "unhandled VSM register reply : %s", id.c_str());
	});
	tv_display->sound_set_connection_observer([this](const std::string & id, bool connected){
		// update connection pair
		audio_connections.second = audio_connections.first;
		audio_connections.first = id;
		auto media_element_out = connection_policy.connected(mdc::sink_t::SOUND);
		auto it = media_elements_.find(id);
		if (it != media_elements_.end()) {
			auto & media_element = it->second.media;
			if (connected)
				media_element->process_event(event::AudioConnected());
			else {
				media_element->process_event(event::AudioDisconnected());
				if (!media_element->hasVideo() && media_element->hasAudio())
					notifyEvent(EventSignalType::SOUND_DISCONNECTED, id, EventDataBaseType());
			}
		}
		// notify disconnect to previously connected media element
		if (connected && media_element_out && media_element_out->id() != id) {
			auto imo_out = std::static_pointer_cast<mdc::MediaObject>
					(std::const_pointer_cast<mdc::IMediaObject>(media_element_out));
			imo_out->process_event(event::AudioDisconnected());
			if (!imo_out->hasVideo() && imo_out->hasAudio())
				notifyEvent(EventSignalType::SOUND_DISCONNECTED, imo_out->id(), EventDataBaseType());
		}
	});
	tv_display->vsm_set_connection_observer([this](const std::string & id, bool connected){
		auto it = media_elements_.find(id);
		if (it != media_elements_.end()) {
			auto & media_element = it->second.media;
			if (connected) {
				auto plane_id = tv_display->get_plane_id(id);
				notifyEvent(EventSignalType::PLANE_ID, id, PlaneIdEvent(plane_id));
				auto sink = connection_policy.video().requested(id);
				media_element->process_event(event::VideoConnected(sink));
			}
			else
				media_element->process_event(event::VideoDisconnected());
		}
	});
	tv_display->avblock_set_muted_callback([this](const std::string & id, bool muted){
		if (muted) {
			auto it = media_elements_.find(id);
			if (it != media_elements_.end()) {
				auto & media_element = it->second.media;
				media_element->process_event(event::Muted());
			}
		}

	});
	tv_display->display_set_config_completed_callback([this](const std::string & id, bool){
		auto it = media_elements_.find(id);
		if (it != media_elements_.end()) {
			auto & media_element = it->second.media;
			media_element->process_event(event::DisplayConfigured());
		}
	});

	layout_manager.set_layout_change_callback([this](const std::string & id, const mdc::display_out_t & d){
		notifyActiveRegion(id, d);
	});

#if !USE_RPI_RESOURCE
	LOG_DEBUG(log, "subscribe to TV service for screen saver Events");
	connector_->subscribe("palm://com.webos.service.tvpower/power/registerScreenSaverRequest",
						  "{\"clientName\":\"com.webos.media\", \"subscribe\":true}",
						  screenSaverEventCallBack, (void*)this);
        connector_->sendMessage(ControlInterface::get_timer_info, "{\"subscribe\":true}", nopTimerInfoCallBack, (void*)this);
#endif

}

//------------------------------------------------
// @f nopTimerInfo
// @b nopTimerInfo callback for getting noptimer events.
//------------------------------------------------

bool MediaDisplayController::nopTimerInfo(UMSConnectorHandle* sender,
               UMSConnectorMessage* message)
{
	JDomParser parser;

	string event = connector_->getMessageText(message);
	RETURN_IF(!parser.parse(event,  pbnjson::JSchema::AllSchema()), false,
		      MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ", event.c_str());
	JValue parsed = parser.getDom();
	JValue timers_array = parsed["timers"];
	if (!timers_array.isArray())
	{
		LOG_ERROR(log, MSGERR_JSON_PARSE, "malformed response");
		return false;
	}
        for (int i = 0; i < timers_array.arraySize(); i++)
        {
		JValue timers_object = timers_array[i];
		std::string timer_id;
		timer_id = timers_object["type"].asString();
		if (timer_id.find("[TIMER]ScreenSaver") != std::string::npos) {
			screenSaverTimerId_ = timers_object["type"].asString();
			LOG_DEBUG(log, "screenSaverTimerId_ : %s", screenSaverTimerId_.c_str());
		}
	}
        return true;
}


//------------------------------------------------
// @f resetScreenSaverTimer
// @b resetScreenSaverTimer API to reset the screen saver timer.
//------------------------------------------------

bool MediaDisplayController::resetScreenSaverTimer()
{
	LOG_DEBUG (log, "resetScreenSaverTimer");
	if (screenSaverTimerId_.empty()) {
		LOG_DEBUG(log, "screenSaverTimerId_.empty()");
		return false;
	}
	JValue responseObject = Object();
	JGenerator serializer(NULL);
	string payload_serialized;
	responseObject.put("timerId", screenSaverTimerId_.c_str());
	responseObject.put("option", "reset");
	serializer.toString(responseObject,  pbnjson::JSchema::AllSchema(), payload_serialized);
	LOG_DEBUG (log, "sending resetScreenSaverTimer: %s ", payload_serialized.c_str());
	connector_->sendMessage(ControlInterface::remove_nop_timer, payload_serialized, NULL, (void*)this);
	return true;
}

/* callback function to get Events from Tv power service */
bool MediaDisplayController::screenSaverEvent(UMSConnectorHandle* sender,
		UMSConnectorMessage* message)
{
	bool ack = true;
	JDomParser parser;

	string event = connector_->getMessageText(message);
	RETURN_IF(!parser.parse(event,  pbnjson::JSchema::AllSchema()), false,
			MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ", event.c_str());

	JValue parsed = parser.getDom();
	string cmd = "palm://com.webos.service.tvpower/power/responseScreenSaverRequest";
	if (parsed.hasKey("timestamp")) {
		string timestamp = parsed["timestamp"].asString();
		JValue responseObject = Object();
		JGenerator serializer(NULL);
		string payload_serialized;
		if ( media_elements_.size() != 0 ) {
			ack = canLaunchScreenSaver();
		}
		responseObject.put("clientName", "com.webos.media");
		responseObject.put("timestamp", timestamp);
		responseObject.put("ack", ack);

		serializer.toString(responseObject,  pbnjson::JSchema::AllSchema(), payload_serialized);
		LOG_DEBUG (log, "sending responseScreenSaverRequest: %s ", payload_serialized.c_str());
		connector_->sendMessage(cmd,payload_serialized, NULL, (void*)this);
	}
	return true;
}

bool MediaDisplayController::canLaunchScreenSaver() const {
	LOG_DEBUG (log, "canLaunchScreenSaver");
	for (const auto & i : media_elements_) {
		if (i.second.media->hasVideo()) {
			LOG_DEBUG (log, "canLaunchScreenSaver: playback_state %d, is_fullScreen %d",i.second.playback_state,i.second.is_fullScreen);
			if (i.second.playback_state == PLAYBACK_PLAYING && i.second.is_fullScreen) {
				LOG_DEBUG (log, "Playing Video pipeline in is in full screen : [media_id : %s ] ", i.first.c_str());
				return false;
			}
		}
	}

	return true;
}

void MediaDisplayController::applyAutoLayout() const {
	std::for_each(media_elements_.begin(), media_elements_.end(),
				  [this](decltype(*media_elements_.begin()) me) {
		if ((me.second.media->autoLayout() == true) && (me.second.media->foreground() == true)) {
			me.second.media->process_event(event::SwitchToAutoLayout());
		}
	});
}

//------------------------------------------------
// @f turnOnScreen
// @b turnOnScreen API to remove the screen saver.
//------------------------------------------------

void MediaDisplayController::turnOnScreen() const {
	LOG_DEBUG(log,"turnOnScreen");
	JValue responseObject = Object();
	JGenerator serializer(NULL);
	string payload_serialized;
	serializer.toString(responseObject,  pbnjson::JSchema::AllSchema(), payload_serialized);
	LOG_DEBUG (log, "Sending turnOnScreen: %s ", payload_serialized.c_str());
	connector_->sendMessage(ControlInterface::turn_on_screen, payload_serialized, NULL, (void*)this);
}

//----------------------------------------
//  event and command call back handlers
// ---------------------------------------

void MediaDisplayController::foregroundEvent(const std::set<std::string> & fg_apps) {
	std::set<std::string> in /* poped to fg */, out /* went to bg */;
	std::set_difference(fg_apps.begin(), fg_apps.end(), _foreground_apps.begin(), _foreground_apps.end(),
						std::inserter(in, in.begin()));
	std::set_difference(_foreground_apps.begin(), _foreground_apps.end(), fg_apps.begin(), fg_apps.end(),
						std::inserter(out, out.begin()));
	_foreground_apps = fg_apps;

	// notifying background pipelines
	for (const auto & app_id : out) {
		std::for_each(media_elements_.begin(), media_elements_.end(),
					  [this, app_id](decltype(*media_elements_.begin()) me) {
			if (me.second.media->appId() == app_id) {
				updateForegroundState(me.second);
			}
		});
	}
	// notifying foreground pipelines
	for (const auto & app_id : in) {
		std::for_each(media_elements_.begin(), media_elements_.end(),
					  [this, app_id](decltype(*media_elements_.begin()) me) {
			if (me.second.media->appId() == app_id) {
				updateForegroundState(me.second);
			}
		});
	}
	// try display foreground pipelines
	for (const auto & app_id : in) {
		std::for_each(media_elements_.begin(), media_elements_.end(),
					  [this, app_id](decltype(*media_elements_.begin()) me) {
			if (me.second.media->appId() == app_id) {
				me.second.media->process_event(event::TryDisplay());
			}
		});
	}
}

bool MediaDisplayController::updatePipelineState(const std::string &media_id, playback_state_t state)
{
	auto e = media_elements_.find(media_id);
	RETURN_IF(e == media_elements_.end(), false, MSGERR_INVALID_ARG,
			  "invalid media_id=%s", media_id.c_str());

	e->second.playback_state = state;
	LOG_DEBUG (log, "mediaId = %s, status= %d ", media_id.c_str(), state);

	// update sound connection for audio only pipeline
	if (!e->second.media->hasVideo() && e->second.media->hasAudio()) {
		if (state == PLAYBACK_PLAYING) {
			e->second.media->process_event(event::TryDisplay());
		} else {
			e->second.media->process_event(event::DisconnectAudio());
			reconnectSound(e->second.media->id());
		}
	}

	return true;
}

int MediaDisplayController::numberOfAutoLayoutedVideos() const
{
	static int number_of_autolayouted_videos;
	number_of_autolayouted_videos = 0;
	std::for_each(media_elements_.begin(), media_elements_.end(),
				  [this](decltype(*media_elements_.begin()) me) {
		if ((me.second.media->autoLayout() == true) && (me.second.media->foreground() == true)) {
			number_of_autolayouted_videos++;
		}
	});
	return number_of_autolayouted_videos;
}

// @f pipelineEvents
// @b monitor managed Pipeline Events to detec mediaContentReady and videoInfo events
//
// Incoming Events
//
// Media Content Ready
// {
//   "mediaContentReady": {"state":<bool>, "mediaId":"<MID>"}
// }
//
// Video Info from Pipeline
//
//  {"videoInfo":
//    {"width":320,
//     "height":240,
//     "aspectRatio":"4:3",
//     "frameRate":15,
//     "bitRate":0,
//     "mode3D":"2d",
//     "actual3D":"2d",
//     "scanType":"progressive",
//     "pixelAspectRatio":"1:1",
//     "mediaId":"_4MR93JBwuRtXGz"}
//   }
//
bool MediaDisplayController::pipelineEvents(UMSConnectorHandle* handle, UMSConnectorMessage* message)
{
	string event = connector_->getMessageText(message);

	JDomParser parser;
	RETURN_IF(!parser.parse(event,  pbnjson::JSchema::AllSchema()), false,
			  MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ", event.c_str());

	JValue parsed = parser.getDom();
	if (parsed.hasKey("playing")) {
		LOG_DEBUG(log, "Pipeline Event: event(%s).", event.c_str());
		string mediaId = parsed["playing"]["mediaId"].asString();
		auto e = media_elements_.find(mediaId);
		if(e != media_elements_.end() && e->second.is_fullScreen)
#if !USE_RPI_RESOURCE
			turnOnScreen();
#endif
		updatePipelineState(mediaId, PLAYBACK_PLAYING);
	} else if (parsed.hasKey("paused")){
		LOG_DEBUG(log, "Pipeline Event: event(%s).", event.c_str());
		string mediaId = parsed["paused"]["mediaId"].asString();
		updatePipelineState(mediaId, PLAYBACK_PAUSED);
	}else if(parsed.hasKey("endOfStream")){
		LOG_DEBUG(log, "Pipeline Event: event(%s).", event.c_str());
		string mediaId = parsed["endOfStream"]["mediaId"].asString();
		resetScreenSaverTimer();
		updatePipelineState(mediaId, PLAYBACK_STOPPED);
	}

	if( parsed.hasKey("mediaContentReady") ) {
		string media_id = parsed["mediaContentReady"]["mediaId"].asString();
		bool ready = parsed["mediaContentReady"]["state"].asBool();
		contentReady(media_id, ready);
		LOG_DEBUG(log, "Pipeline Event: event(%s).", event.c_str());
	}
	else if ( parsed.hasKey("videoInfo") ) {
		setVideoInfo(parsed);
		LOG_DEBUG(log, "Pipeline Event: event(%s).", event.c_str());
	}

	return true;
}

// @f enableDebug
// @b enable debug statements in MDC Boost::StateCharts core
//
//
bool MediaDisplayController::enableDebug(UMSConnectorHandle* handle, UMSConnectorMessage* message)
{
	string event = connector_->getMessageText(message);

	JDomParser parser;
	RETURN_IF(!parser.parse(event,  pbnjson::JSchema::AllSchema()), false,
			  MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ", event.c_str());

	JValue parsed = parser.getDom();
	if( parsed.hasKey("state") ) {
		bool state = parsed["state"].asBool();
		for (auto & m : media_elements_) {
			m.second.media->enableDebug(state);
		}
		LOG_DEBUG(log, "MDC debug state(%s).", (state ? "true" : "false"));
	}

	connector_->sendSimpleResponse(handle, message, true);
	return true;
}

//----------------------------------------
// END : event and command call back handlers
// ---------------------------------------

// @f focus
// @b focus service interface to obtain audio and video focus for media element
//
//  { "mediaId":"<MID>" }
//
bool MediaDisplayController::focus(UMSConnectorHandle* handle, UMSConnectorMessage* message)
{
	string cmd = connector_->getMessageText(message);

	JDomParser parser;
	RETURN_IF(!parser.parse(cmd,  pbnjson::JSchema::AllSchema()), false,
			  MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ", cmd.c_str());

	JValue parsed = parser.getDom();
	RETURN_IF(!parsed.hasKey("mediaId"), false, MSGERR_NO_MEDIA_ID, "mediaId must be specified");

	string media_id = parsed["mediaId"].asString();

	connector_->sendSimpleResponse(handle, message, focus(media_id));
	return true;
}

// @f setDisplayWindow
// @b setDisplayWindow service interface to set input and output media element display windows
//
//   {
//     "mediaId":"<MID>",
//     "source":{"x" : 0, "y" : 0, "width" : 1920, "height" : 1080},
//     "destination":{"x" : 0, "y" : 0, "width" : 1920, "height" : 1080}
//   }
//
//
bool MediaDisplayController::setDisplayWindow(UMSConnectorHandle* handle, UMSConnectorMessage* message)
{
	string cmd = connector_->getMessageText(message);
	LOG_DEBUG(log, "cmd = %s", cmd.c_str());

	JDomParser parser;
	RETURN_IF(!parser.parse(cmd,  pbnjson::JSchema::AllSchema()), false,
			  MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ", cmd.c_str());

	JValue parsed = parser.getDom();
	RETURN_IF(!parsed.hasKey("mediaId"), false, MSGERR_NO_MEDIA_ID, "mediaId must be specified");

	string media_id = parsed["mediaId"].asString();
	auto e = media_elements_.find(media_id);

	if (e == media_elements_.end())
	{
		LOG_ERROR(log, MSGERR_INVALID_ARG, "invalid mediaId=%s", media_id.c_str());
		JValue response = JObject{{"returnValue",false}, \
		                          {"errorMessage", "invalid mediaId"},
		                          {"errorCode", 1}}; //TODO: setup error codes list
		connector_->sendResponseObject(handle, message, response.stringify());
		return true;
	}

	// ----
	// TV services API
	//   {
	//     "context":"<MID>",
	//     "sourceInput":{"positionX" : 0, "positionY" : 0, "width" : 1920, "height" : 1080},
	//     "displayOutput":{"positionX" : 0, "positionY" : 0, "width" : 1920, "height" : 1080}
	//   }
	//
	rect_t src_rect;
	e->second.output_window = { parsed["destination"]["x"].asNumber<int32_t>(),
				parsed["destination"]["y"].asNumber<int32_t>(),
				parsed["destination"]["width"].asNumber<int32_t>(),
				parsed["destination"]["height"].asNumber<int32_t>()};
	if (parsed.hasKey("source")) {
		src_rect = { parsed["source"]["x"].asNumber<int32_t>(),
					 parsed["source"]["y"].asNumber<int32_t>(),
					 parsed["source"]["width"].asNumber<int32_t>(),
					 parsed["source"]["height"].asNumber<int32_t>() };
	}
	e->second.media->process_event(event::SetDisplayWindow(e->second.output_window, src_rect));

	//e->second.output_window = out_rect;
	LOG_DEBUG(log, "output_window size for media_id[%s]: %d, %d, %d, %d", e->first.c_str(), e->second.output_window.x, e->second.output_window.y, e->second.output_window.w, e->second.output_window.h);
	e->second.is_fullScreen = false;
	notifyEvent(EventSignalType::FULLSCREEN, media_id,
			static_cast<const EventDataBaseType&>(FullscreenEvent(false)) );

	connector_->sendSimpleResponse(handle, message, true);
	return true;
}

bool MediaDisplayController::getForegroundAppInfo(UMSConnectorHandle* handle, UMSConnectorMessage* message)
{
	JValue retObject = Object();
	JGenerator serializer(NULL);
	string retObjectString;
	JValue arrayObject = Object();
	pbnjson::JArray array_info;
	std::string pipelineId;
	std::string playStateNow;
	std::string playStateNext;
	bool isFullScreen = false;
	int positionX = 0;
	int positionY = 0;
	int width = 0;
	int height = 0;
	bool returnValue = true;
	std::string appId = "";

	string cmd = connector_->getMessageText(message);
	LOG_DEBUG(log, "cmd = %s", cmd.c_str());

	JDomParser parser;
	RETURN_IF(!parser.parse(cmd,  pbnjson::JSchema::AllSchema()), false,
			  MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ", cmd.c_str());

	for (const auto & i : media_elements_) {
		appId = i.second.media->appId();
		if (i.second.media->hasVideo() && i.second.in_app_foreground == true) {
			LOG_DEBUG(log, "foreground app media_id = %s has video", i.first.c_str());
			pipelineId =  i.first.c_str();
			playStateNow = i.second.playback_state == PLAYBACK_PLAYING ? "playing" : "paused";
			isFullScreen = i.second.is_fullScreen;
			positionX = i.second.output_window.x;
			positionY = i.second.output_window.y;
			width = i.second.output_window.w;
			height = i.second.output_window.h;
			break;
		}
	}
	arrayObject.put("pipelineId", pipelineId);
	arrayObject.put("playStateNow", playStateNow);
	arrayObject.put("playStateNext", playStateNow);
	arrayObject.put("isFullScreen", isFullScreen);
	arrayObject.put("positionX", positionX);
	arrayObject.put("positionY", positionY);
	arrayObject.put("width", width);
	arrayObject.put("height", height);
	array_info << arrayObject;
	retObject.put("returnValue", returnValue);
	retObject.put("appId", appId);
	retObject.put("acbs", array_info);

	serializer.toString(retObject,  pbnjson::JSchema::AllSchema(), retObjectString);
	LOG_DEBUG(log, "retObjectString =  %s", retObjectString.c_str());
	connector_->sendResponseObject(handle,message,retObjectString);
	return true;
}

// @f switchToFullScreen
// @b Set media element to full screen
//
//  command:
//   {"mediaId":"<MID>"}
//
//  responses:
//  success = {"returnValue": true}
//  failure = {
//     "returnValue": false,
//     "errorCode": "DISPLAY_ERROR_0000",
//     "errorText": "Invalid Context",
//     "context": "pipeline_1"
//  }
//
bool MediaDisplayController::switchToFullScreen(UMSConnectorHandle* handle, UMSConnectorMessage* message)
{
	string cmd = connector_->getMessageText(message);
	LOG_DEBUG(log, "cmd = %s", cmd.c_str());

	JDomParser parser;
	RETURN_IF(!parser.parse(cmd,  pbnjson::JSchema::AllSchema()), false,
			  MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ", cmd.c_str());

	JValue parsed = parser.getDom();
	RETURN_IF(!parsed.hasKey("mediaId"), false, MSGERR_NO_MEDIA_ID, "mediaId must be specified");


	string media_id = parsed["mediaId"].asString();
	auto e = media_elements_.find(media_id);
	RETURN_IF(e == media_elements_.end(), false, MSGERR_INVALID_ARG,
			  "invalid media_id=%s", media_id.c_str());

	e->second.media->process_event(event::SwitchToFullscreen());
	LOG_DEBUG(log, "media-id( %s ) switch to full screen", media_id.c_str());
	e->second.is_fullScreen = true;

	notifyEvent(EventSignalType::FULLSCREEN, media_id,
			static_cast<const EventDataBaseType&>(FullscreenEvent(true)) );

	connector_->sendSimpleResponse(handle, message, true);
	return true;
}

// @f switchToAutoLayout
// @b Delegate media element layout to MDC
//
//  command:
//   {"mediaId":"<MID>"}
//
//  responses:
//  success = {"returnValue": true}
//  failure = {
//     "returnValue": false,
//     "errorCode": "DISPLAY_ERROR_0000",
//     "errorText": "Invalid Context",
//     "context": "pipeline_1"
//  }
//
bool MediaDisplayController::switchToAutoLayout(UMSConnectorHandle* handle, UMSConnectorMessage* message)
{
	string cmd = connector_->getMessageText(message);
	LOG_DEBUG(log, "cmd = %s", cmd.c_str());

	JDomParser parser;
	RETURN_IF(!parser.parse(cmd,  pbnjson::JSchema::AllSchema()), false,
			  MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ", cmd.c_str());

	JValue parsed = parser.getDom();
	RETURN_IF(!parsed.hasKey("mediaId"), false, MSGERR_NO_MEDIA_ID, "mediaId must be specified");


	string media_id = parsed["mediaId"].asString();
	auto e = media_elements_.find(media_id);
	RETURN_IF(e == media_elements_.end(), false, MSGERR_INVALID_ARG,
			  "invalid media_id=%s", media_id.c_str());

	e->second.media->process_event(event::SwitchToAutoLayout());

	if (numberOfAutoLayoutedVideos() > 1 ) {
		applyAutoLayout();
	}

	connector_->sendSimpleResponse(handle, message, true);
	return true;
}

// @f setVisibility
// @b Media element visibility hint
//
//  command:
//   {"mediaId":"<MID>", "visible":true}
//
//  responses:
//  success = {"returnValue": true}
//  failure = {
//     "returnValue": false,
//     "errorCode": "DISPLAY_ERROR_0000",
//     "errorText": "Invalid Context",
//     "context": "pipeline_1"
//  }
//
bool MediaDisplayController::setVisibility(UMSConnectorHandle* handle, UMSConnectorMessage* message)
{
	string cmd = connector_->getMessageText(message);
	LOG_DEBUG(log, "cmd = %s", cmd.c_str());

	JDomParser parser;
	RETURN_IF(!parser.parse(cmd,  pbnjson::JSchema::AllSchema()), false,
			  MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ", cmd.c_str());

	JValue parsed = parser.getDom();
	RETURN_IF(!parsed.hasKey("mediaId"), false, MSGERR_NO_MEDIA_ID, "mediaId must be specified");
	RETURN_IF(!parsed.hasKey("visible"), false, MSGERR_NO_MEDIA_ID, "visible must be specified");


	string media_id = parsed["mediaId"].asString();
	auto e = media_elements_.find(media_id);
	RETURN_IF(e == media_elements_.end(), false, MSGERR_INVALID_ARG,
			  "invalid media_id=%s", media_id.c_str());

	bool is_visible = parsed["visible"].asBool();
	e->second.media->process_event(event::SetVisibility(is_visible));
	connector_->sendSimpleResponse(handle, message, true);

	notifyEvent(EventSignalType::VISIBLE, media_id,
				static_cast<const EventDataBaseType&>(VisibilityEvent(is_visible)) );

	return true;
}

// @f unregisterMedia
// @b unregister umnanaged media element
//
//  command:
//   {"mediaId":"<MID>"}
//
//  responses:
//  success = {"returnValue": true,"context : "pipeline_1"}
//  failure = {
//     "returnValue": false,
//     "errorCode": "DISPLAY_ERROR_0000",
//     "errorText": "Invalid Context",
//     "context": "pipeline_1"
//  }
//
bool MediaDisplayController::unregisterMedia(UMSConnectorHandle* handle, UMSConnectorMessage* message)
{
	string cmd = connector_->getMessageText(message);
	LOG_DEBUG(log, "cmd = %s", cmd.c_str());

	JDomParser parser;
	RETURN_IF(!parser.parse(cmd,  pbnjson::JSchema::AllSchema()), false,
			  MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ", cmd.c_str());

	JValue parsed = parser.getDom();
	RETURN_IF(!parsed.hasKey("mediaId"), false, MSGERR_NO_MEDIA_ID, "mediaId must be specified");

	string media_id = parsed["mediaId"].asString();

        resetScreenSaverTimer();

	connector_->sendSimpleResponse(handle, message, unregisterMedia(media_id));
	return true;
}

// @f setVideoInfo
// @b store video meta information
//
//  command:
//   {"mediaId":"<MID>", "videoInfo":"{video info}"}
//
//  responses:
//  success = {"returnValue": true,"context : "pipeline_1"}
//  failure = {
//     "returnValue": false,
//     "errorCode": "DISPLAY_ERROR_0000",
//     "errorText": "Invalid Context",
//     "context": "pipeline_1"
//  }
//
bool MediaDisplayController::setVideoInfo(UMSConnectorHandle* handle, UMSConnectorMessage* message)
{
	string cmd = connector_->getMessageText(message);
	LOG_DEBUG(log, "cmd = %s", cmd.c_str());

	JDomParser parser;
	RETURN_IF(!parser.parse(cmd,  pbnjson::JSchema::AllSchema()), false,
			  MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ", cmd.c_str());

	JValue parsed = parser.getDom();
	connector_->sendSimpleResponse(handle, message, setVideoInfo(parsed));
	return true;
}

// @f contentReady
// @b inform that pipeline is ready for display
//
//  command:
//   {"mediaId":"<MID>", "state", <bool>}
//
//  responses:
//  success = {"returnValue": true,"context : "pipeline_1"}
//  failure = {
//     "returnValue": false,
//     "errorCode": "DISPLAY_ERROR_0000",
//     "errorText": "Invalid Context",
//     "context": "pipeline_1"
//  }
//
bool MediaDisplayController::contentReady(UMSConnectorHandle* handle, UMSConnectorMessage* message)
{
	string cmd = connector_->getMessageText(message);
	LOG_DEBUG(log, "cmd = %s", cmd.c_str());

	JDomParser parser;
	RETURN_IF(!parser.parse(cmd,  pbnjson::JSchema::AllSchema()), false,
			  MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ", cmd.c_str());

	JValue parsed = parser.getDom();
	string media_id = parsed["mediaId"].asString();
	bool ready = parsed["state"].asBool();
	connector_->sendSimpleResponse(handle, message, contentReady(media_id, ready));
	return true;
}

// ----------------------------------------------------------
// uMediaServer API
//
// @f registerMedia
// @b Begin tracking app_id with LSM to update FG and FOCUS state
//
bool MediaDisplayController::registerMedia(const string &media_id, const string &app_id)
{
	if (media_id.empty() || app_id.empty()) {
		LOG_DEBUG(log, "failed registration because of empty string: media_id=%s, app_id=%s",
				media_id.c_str(), app_id.c_str());
		return false;
	}

	auto e = media_elements_.find(media_id);
	if(e != media_elements_.end()) {
		LOG_DEBUG(log, "existing media id. failed registration. media_id=%s, app_id=%s",
				  media_id.c_str(), app_id.c_str());
		return false;
	}
	auto it = media_elements_.emplace(std::piecewise_construct, std::forward_as_tuple(media_id),
									  std::forward_as_tuple(app_id, media_id, *tv_display,
															connection_policy, layout_manager));
	LOG_INFO(log, MSGNFO_MDC_REGISTRATION, "media_id(%s), app_id(%s): registered.",
			 media_id.c_str(), app_id.c_str());
	notifyEvent(EventSignalType::REGISTERED, media_id, RegisteredEvent());
	if (_foreground_apps.find(app_id) != _foreground_apps.end()) {
		it.first->second.media->process_event(event::ToForeground());
		notifyEvent(EventSignalType::FOREGROUND, media_id, ForegroundEvent(true));
	}
	return true;
}

// @f unregisterMedia
// @b Begin tracking app_id with LSM to update FG and FOCUS state
//
bool MediaDisplayController::unregisterMedia(const std::string &media_id)
{
	LOG_DEBUG(log, "media_id(%s): unregistered.", media_id.c_str());

	auto it = media_elements_.find(media_id);
	if (it != media_elements_.end()) {
		auto & subscription = it->second.subscription;
		if (subscription.token)
			connector_->unsubscribe(subscription.call, subscription.token);
		subscription.token = 0;
		tv_display->display_set_alpha(media_id, 1.);
		// pass focus by stack
		if (it->second.media->focus()) {
			auto focus_it = std::find_if(focus_stack.begin(), focus_stack.end(), [this](const std::string & id) {
				auto me = media_elements_.find(id);
				return me != media_elements_.end() && me->second.media->foreground();
			});
			if (focus_it == focus_stack.end()) {
				if (!focus_stack.empty())
					focus(focus_stack.front());
			} else
				focus(*focus_it);
		}

		reconnectSound(media_id);

		media_elements_.erase(it);
	}

	focus_stack.remove_if([&media_id](const std::string & id){ return id == media_id; });

	return true;
}

// @f acquired
// @b notify new Media Pipeline resources acquisition
//
// {"resources":
//   [
//     {"qty":1,"resource":"VDEC","index":1},
//     {"qty":1,"resource":"ADEC","index":1}
//   ],
//   "state":true,"connectionId":"_ZWXaAE3IXmXFQr"
//  }
//
bool MediaDisplayController::acquired(const std::string & id, const std::string & service,
									  const res_info_t & resources)
{
	auto e = media_elements_.find(id);
	RETURN_IF(e == media_elements_.end(), false, MSGERR_INVALID_ARG,
			  "invalid media_id=%s", id.c_str());

	e->second.media->process_event(event::Acquire(resources));

	// subscribe to Pipeline(process) state change events
	// TODO: why we subscribe here?
	auto & subscription = e->second.subscription;
	std::string call = service + "/stateChange";
	if (subscription.call != call) {
		if (subscription.token)
			connector_->unsubscribe(subscription.call, subscription.token);
		subscription.call = call;
		subscription.token = connector_->subscribe(subscription.call, "{}",
												   pipelineEventsCallBack, (void*)this);
		LOG_TRACE(log, "subscribe to Pipeline media_id(%s) for state change events. cmd(%s)",
				  id.c_str(), subscription.call.c_str());
	}

	return true;
}

bool MediaDisplayController::released(const std::string & id, const res_info_t & resources)
{
	auto e = media_elements_.find(id);
	RETURN_IF(e == media_elements_.end(), false, MSGERR_INVALID_ARG,
			  "invalid media_id=%s", id.c_str());

	e->second.media->process_event(event::Release(resources));

	return true;
}

bool MediaDisplayController::contentReady(const std::string &media_id, bool ready) {
	auto e = media_elements_.find(media_id);
	RETURN_IF(e == media_elements_.end(), false, MSGERR_INVALID_ARG,
			  "invalid media_id=%s", media_id.c_str());

	LOG_DEBUG(log, "media_id(%s): content_ready(%s).",
			  media_id.c_str(), (ready ? "true" : "false"));

	if (ready) {
		e->second.media->process_event(event::MediaContentReady());
		notifyEvent(EventSignalType::FULLSCREEN, media_id,
				static_cast<const EventDataBaseType&>(MediaContentReadyEvent(true)) );
	}
	else {
		e->second.media->process_event(event::MediaContentNotReady());
		notifyEvent(EventSignalType::FULLSCREEN, media_id,
				static_cast<const EventDataBaseType&>(MediaContentReadyEvent(false)) );
	}

	return true;
}

void MediaDisplayController::inAppForegroundEvent(const std::string &id, bool foreground) {
	auto e = media_elements_.find(id);
	if (e != media_elements_.end()) {
		auto & element = e->second;
		element.in_app_foreground = foreground;
		updateForegroundState(element);
		if (foreground)
			element.media->process_event(event::TryDisplay());
	}
}

bool MediaDisplayController::focus(const std::string &media_id) {
	auto e = media_elements_.find(media_id);
	RETURN_IF(e == media_elements_.end(), false, MSGERR_INVALID_ARG,
			  "invalid media_id=%s", media_id.c_str());

	LOG_DEBUG(log, "media_id(%s): focus.", media_id.c_str());

	for (auto & m : media_elements_) {
		if (m.second.media->focus()) {
			focus_stack.remove_if([&m](const std::string & id){ return id == m.second.media->id(); });
			focus_stack.push_front(m.second.media->id());
			m.second.media->process_event(event::LostFocus());
			notifyFocusChange(m.second);
			break;
		}
	}

	e->second.media->process_event(event::SetFocus());
	notifyFocusChange(e->second);

	notifyEvent(EventSignalType::FOCUS, media_id,
			static_cast<const EventDataBaseType&>(FocusEvent(true)) );

	focus_stack.remove_if([&media_id](const std::string & id){ return id == media_id; });

	return true;
}

mdc::media_element_state_t MediaDisplayController::getMediaElementState(const std::string & id) {
	auto it = media_elements_.find(id);
	if (it == media_elements_.end())
		return mdc::media_element_state_t();
	return mdc::media_element_state_t(id, it->second.media->getStates(),
									 {connection_policy.video().connected(id),
									  connection_policy.audio().connected(id)});
}

// ----------------------------------------------------------
// INTERNAL API
//
// NO LOCKING !!! no locking whatsoever...
//

bool MediaDisplayController::setVideoInfo(const pbnjson::JValue & parsed) {
	string media_id;
	media_id = parsed["videoInfo"]["mediaId"].asString();

	auto e = media_elements_.find(media_id);
	RETURN_IF(e == media_elements_.end(), false, MSGERR_INVALID_ARG,
			  "invalid media_id=%s", media_id.c_str());

	// parse and store video info.
	// Note: accepts partial updates of videoInfo
	// Note: Why is this a structure and not a json string like all the
	//        other settings. This was to allow default values and partial
	//        json updates. PBNJson will not update the video_info.<parameter>
	//        if the field doesn't exist in the JSON string.
	//
	video_info_t video_info;
	parsed["videoInfo"]["width"].asNumber(video_info.width);
	parsed["videoInfo"]["height"].asNumber(video_info.height);
	parsed["videoInfo"]["frameRate"].asNumber(video_info.frame_rate);
	parsed["videoInfo"]["bitRate"].asNumber(video_info.bit_rate);
	parsed["videoInfo"]["adaptive"].asBool(video_info.adaptive);
	parsed["videoInfo"]["path"].asString(video_info.path);
	parsed["videoInfo"]["content"].asString(video_info.content);

	// 3D
	parsed["videoInfo"]["actual3D"].asString(video_info.video3d.current);
	parsed["videoInfo"]["mode3D"].asString(video_info.video3d.original);
	parsed["videoInfo"]["currentType"].asString(video_info.video3d.type_lr);

	// format: "VIDEO_<SCAN_TYPE>"; all upper case
	parsed["videoInfo"]["scanType"].asString(video_info.scan_type);
	video_info.scan_type.insert(0, "video_");
	transform(video_info.scan_type.begin(), video_info.scan_type.end(),
			  video_info.scan_type.begin(), ::toupper);

	// format: "1:1". slit and store in data structure
	vector<std::string> ratios; std::string aspect;
	if (::CONV_OK == parsed["videoInfo"]["pixelAspectRatio"].asString(aspect)) {
		split(ratios, aspect, is_any_of(":"));
		if (ratios.size() == 2) {
			video_info.pixel_aspect_ratio.width = stoi(ratios[0]);
			video_info.pixel_aspect_ratio.height = stoi(ratios[1]);
		}
	}

	static auto validate_video_info = [](const video_info_t & vi) {
		return vi.width && vi.height;
	};

	auto video_info_valid = validate_video_info(video_info);

	if (video_info_valid) {
		e->second.media->process_event(event::MediaVideoData(video_info));
		notifyEvent(EventSignalType::VIDEO_INFO, media_id,
					static_cast<const EventDataBaseType&>(VideoInfoEvent(video_info)) );
	}

	return video_info_valid;
}

void MediaDisplayController::registerEventNotify( EventSignalType type,
		const event_signal_t::slot_type& subscriber)
{
	event_signal[type].connect(subscriber);
}

void MediaDisplayController::notifyEvent(EventSignalType type,
		const std::string& id, const EventDataBaseType &data)
{
	event_signal[type](type, id, data);
}

void MediaDisplayController::updateForegroundState(media_element_t & element) {
	bool updated_state = element.in_app_foreground &&
			_foreground_apps.find(element.media->appId()) != _foreground_apps.end();
	bool current_state = element.media->foreground();
	if (updated_state != current_state) {
		if (updated_state) {
			notifyEvent(EventSignalType::FOREGROUND, element.media->id(), ForegroundEvent(true));
			element.media->process_event(event::ToForeground());
		}
		else {
			notifyEvent(EventSignalType::FOREGROUND, element.media->id(), ForegroundEvent(false));
			element.media->process_event(event::ToBackground());
		}
	}
}

void MediaDisplayController::notifyFocusChange(const media_element_t & element) {
	pbnjson::JValue event = pbnjson::JObject {
			{"focusChanged", pbnjson::JObject {
				{"focus", element.media->focus()},
				{"mediaId", element.media->id()}}
	}};
	std::string serialized;
	if(!pbnjson::JGenerator(nullptr).toString(event, pbnjson::JSchema::AllSchema(), serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "json serialize error.");
		return;
	}
	connector_->sendChangeNotificationJsonString(serialized, element.media->id());
}

void MediaDisplayController::notifyActiveRegion(const std::string & id, const mdc::display_out_t & dpy_out) {
	pbnjson::JValue event = pbnjson::JObject {
			{"activeRegion", pbnjson::JObject {
				{"x", dpy_out.out_rect.x},
				{"y", dpy_out.out_rect.y},
				{"width", dpy_out.out_rect.w},
				{"height", dpy_out.out_rect.h},
				{"mediaId", id}}
	}};
	std::string serialized;
	if(!pbnjson::JGenerator(nullptr).toString(event, pbnjson::JSchema::AllSchema(), serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "json serialize error.");
		return;
	}
	connector_->sendChangeNotificationJsonString(serialized, id);
}

void MediaDisplayController::reconnectSound(const std::string & id) {
	auto it = media_elements_.find(id);
	if (it != media_elements_.end()) {
		if (audio_connections.first == it->second.media->id() &&
			!it->second.media->hasVideo() && audio_connections.second != "livetv") {
			// TODO: make this **** more generic
			tv_display->sound_connect(audio_connections.second);
			LOG_DEBUG(log, "Audio connection restoring from %s to %s", id.c_str(), audio_connections.second.c_str());
		}
	}
}

} // namespace uMediaServer
