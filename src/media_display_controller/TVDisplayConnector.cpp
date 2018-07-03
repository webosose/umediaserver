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

#include <luna-service2/lunaservice.h>
#include <pbnjson.hpp>
#include <Logger_macro.h>
#include "TVDisplayConnector.h"
#include "ControlInterface.h"

namespace uMediaServer {

namespace {
Logger _log(UMS_LOG_CONTEXT_MDC);
}

TVDisplayConnector::TVDisplayConnector(UMSConnector * umc, const mdc::IChannelConnection & channel)
	: connector(umc), video_channel(channel), sound_subscription(ControlInterface::sound_connection_state)
	, overlay_states{{mdc::sink_t::MAIN, {255U, 0U}},{mdc::sink_t::SUB, {255U, 1U}}} {
	// subscribe to sound connection observer
	pbnjson::JValue payload = pbnjson::JObject{{"subscribe", true}};
	std::string serialized;
	if(!pbnjson::JGenerator(nullptr).toString(payload, pbnjson::JSchema::AllSchema(), serialized)) {
		LOG_ERROR(_log, MSGERR_JSON_SERIALIZE, "json serialize error.");
	}
	sound_subscription.token = connector->subscribe(sound_subscription.call, serialized,
													sound_connection_status_hanler, this);
}

void TVDisplayConnector::vsm_set_registration_callback(callback_t && callback) {
	registration_callback = callback;
}

void TVDisplayConnector::vsm_set_connection_observer(callback_t && callback) {
	vsm_connection_observer = callback;
}

void TVDisplayConnector::vsm_register(const std::string &id, const mdc::res_t &adec, const mdc::res_t &vdec) {
	pbnjson::JArray resource_list;
	// do we have audio stream?
	if (adec)
		resource_list << pbnjson::JObject{{"type", adec.unit}, {"portNumber", adec.index}};
	if (vdec)
		resource_list << pbnjson::JObject{{"type", vdec.unit}, {"portNumber", vdec.index}};

	std::string audio_type = adec.unit == "PCMMC" ? "system_music" : "media";
	pbnjson::JValue payload = pbnjson::JObject{ {"context", id},
												{"audioType", audio_type},
												{"resourceList", resource_list}};
	std::string serialized;
	if(!pbnjson::JGenerator(nullptr).toString(payload, pbnjson::JSchema::AllSchema(), serialized)) {
		LOG_ERROR(_log, MSGERR_JSON_SERIALIZE, "json serialize error.");
		return;
	}

	connector->sendMessage(ControlInterface::register_media, serialized, vsm_register_reply_hanler, this);

	// subscribe for connection state
	payload = pbnjson::JObject{{"context", id}, {"subscribe", true}};

	if(!pbnjson::JGenerator(nullptr).toString(payload, pbnjson::JSchema::AllSchema(), serialized)) {
		LOG_ERROR(_log, MSGERR_JSON_SERIALIZE, "json serialize error.");
		return;
	}

	subscription_t subscription(ControlInterface::get_connection_state);
	subscription.token = connector->subscribe(subscription.call, serialized, vsm_subscription_handler, this);
	vsm_subscriptions[id] = subscription;
}

void TVDisplayConnector::vsm_unregister(const std::string &id) {
	pbnjson::JValue payload = pbnjson::JObject{{"context", id}};

	std::string serialized;
	if(!pbnjson::JGenerator(nullptr).toString(payload, pbnjson::JSchema::AllSchema(), serialized)) {
		LOG_ERROR(_log, MSGERR_JSON_SERIALIZE, "json serialize error.");
		return;
	}

	auto subscription_it = vsm_subscriptions.find(id);
	if (subscription_it != vsm_subscriptions.end()) {
		connector->unsubscribe(subscription_it->second.call, subscription_it->second.token);
		vsm_subscriptions.erase(subscription_it);
	}
	connector->sendMessage(ControlInterface::unregister_media, serialized, nullptr, nullptr);
}

void TVDisplayConnector::vsm_connect(const std::string &id, mdc::sink_t sink) {
	if (sink == mdc::sink_t::SUB)
		display_set_sub_overlay_mode(SubOverlayMode::PBP);
	pbnjson::JValue payload = pbnjson::JObject{{"context", id},
											   {"sinkType", (sink == mdc::sink_t::MAIN ? "MAIN" : "SUB")}};

	std::string serialized;
	if(!pbnjson::JGenerator(nullptr).toString(payload, pbnjson::JSchema::AllSchema(), serialized)) {
		LOG_ERROR(_log, MSGERR_JSON_SERIALIZE, "json serialize error.");
		return;
	}

	connector->sendMessage(ControlInterface::connect_media, serialized, nullptr, nullptr);
}

void TVDisplayConnector::vsm_disconnect(const std::string &id) {
	auto sink = video_channel.connected(id);
	if (sink == mdc::sink_t::SUB)
		display_set_sub_overlay_mode(SubOverlayMode::NONE);
	pbnjson::JValue payload = pbnjson::JObject{{"context", id}};

	std::string serialized;
	if(!pbnjson::JGenerator(nullptr).toString(payload, pbnjson::JSchema::AllSchema(), serialized)) {
		LOG_ERROR(_log, MSGERR_JSON_SERIALIZE, "json serialize error.");
		return;
	}

	connector->sendMessage(ControlInterface::disconnect_media, serialized, nullptr, nullptr);
}

void TVDisplayConnector::avblock_mute(const std::string &id, size_t channel) {
	pbnjson::JValue payload = pbnjson::JObject{{"context", id}, {"muteOwner", "MDC"},
											   {"video", bool(channel & ITVDisplay::VIDEO_CHANNEL)},
											   {"audio", bool(channel & ITVDisplay::AUDIO_CHANNEL)}};

	std::string serialized;
	if(!pbnjson::JGenerator(nullptr).toString(payload, pbnjson::JSchema::AllSchema(), serialized)) {
		LOG_ERROR(_log, MSGERR_JSON_SERIALIZE, "json serialize error.");
		return;
	}

	if (channel & ITVDisplay::VIDEO_CHANNEL)
		connector->sendMessage(ControlInterface::start_avmute, serialized, avblock_mute_handler, this);
	else
		connector->sendMessage(ControlInterface::start_avmute, serialized, nullptr, nullptr);
}

void TVDisplayConnector::avblock_unmute(const std::string &id, size_t channel) {
	pbnjson::JValue payload = pbnjson::JObject{{"context", id}, {"muteOwner", "MDC"},
											   {"video", bool(channel & ITVDisplay::VIDEO_CHANNEL)},
											   {"audio", bool(channel & ITVDisplay::AUDIO_CHANNEL)}};

	std::string serialized;
	if(!pbnjson::JGenerator(nullptr).toString(payload, pbnjson::JSchema::AllSchema(), serialized)) {
		LOG_ERROR(_log, MSGERR_JSON_SERIALIZE, "json serialize error.");
		return;
	}

	if (channel & ITVDisplay::VIDEO_CHANNEL)
		connector->sendMessage(ControlInterface::stop_avmute, serialized, avblock_unmute_handler, this);
	else
		connector->sendMessage(ControlInterface::stop_avmute, serialized, nullptr, nullptr);
}

void TVDisplayConnector::avblock_set_muted_callback(callback_t && callback) {
	avblock_muted_callback = callback;
}

void TVDisplayConnector::display_set_window(const std::string &id, const mdc::display_out_t &display_out) {
	const char * display_ctrl_uri = ControlInterface::display_window;

	pbnjson::JValue payload;
	pbnjson::JValue dst_win = pbnjson::JObject{
									{"positionX", display_out.out_rect.x},
									{"positionY", display_out.out_rect.y},
									{"width", display_out.out_rect.w},
									{"height", display_out.out_rect.h}
								};
	if (display_out.src_rect) {
		display_ctrl_uri = ControlInterface::display_custom_window;
		pbnjson::JValue src_win = pbnjson::JObject{
										{"positionX", display_out.src_rect.x},
										{"positionY", display_out.src_rect.y},
										{"width", display_out.src_rect.w},
										{"height", display_out.src_rect.h}
									};
		payload = pbnjson::JObject{
						{"context", id},
						{"displayOutput", dst_win},
						{"sourceInput", src_win}
					};
	} else {
		payload = pbnjson::JObject{{"context", id}, {"displayOutput", dst_win},
		{"fullScreen", display_out.fullscreen}};
	}

	std::string serialized;
	if(!pbnjson::JGenerator(nullptr).toString(payload, pbnjson::JSchema::AllSchema(), serialized)) {
		LOG_ERROR(_log, MSGERR_JSON_SERIALIZE, "json serialize error.");
		return;
	}

	connector->sendMessage(display_ctrl_uri, serialized, display_config_handler, this);
}
#if UMS_INTERNAL_API_VERSION == 2
void TVDisplayConnector::display_set_video_info(const std::string &id, const ums::video_info_t &video_info) {
	// UMS INTERNAL API VERSION 2 is only used on raspberrypi currently.
    // so we don't need to implement this function.
	return;
}
#else
void TVDisplayConnector::display_set_video_info(const std::string &id, const mdc::video_info_t &video_info) {
	// TV API : Video Info (meta data)
	// {
	//   "context" : "<MID>",
	//   "content" : "movie",
	//   "video" :
	//     {
	//       "width" : 800,
	//       "height" : 600,
	//       "frameRate" : 60,
	//       "scanType" : "VIDEO_PROGRESSIVE",
	//       "bitRate" : 2315496 ,
	//       "adaptive" : true,
	//       "path" : "file",
	//       "pixelAspectRatio" : {"width" : 1, "height" : 1},
	//       "data3D" : {
	//                     "originalPattern" : "side_side_half",
	//                     "currentPattern" : "side_side_half",
	//                     "typeLR" : "LR"
	//                   }
	//     }
	// }
	//

	pbnjson::JValue payload = pbnjson::JObject{
									{"context", id},
									{"content", video_info.content},
									{"video", pbnjson::JObject{
										{"width", video_info.width},
										{"height", video_info.height},
										{"frameRate", video_info.frame_rate},
										{"scanType", video_info.scan_type},
										{"adaptive", video_info.adaptive},
										{"bitRate", video_info.bit_rate},
										{"path", video_info.path},
										{"pixelAspectRatio", pbnjson::JObject{
											{"width", video_info.pixel_aspect_ratio.width},
											{"height", video_info.pixel_aspect_ratio.height}
										}},
										{"data3D", pbnjson::JObject{
											{"currentPattern", video_info.video3d.current},
											{"originalPattern", video_info.video3d.original},
											{"typeLR", video_info.video3d.type_lr}
										}}
									}}
								};

	std::string serialized;
	if(!pbnjson::JGenerator(nullptr).toString(payload, pbnjson::JSchema::AllSchema(), serialized)) {
		LOG_ERROR(_log, MSGERR_JSON_SERIALIZE, "json serialize error.");
		return;
	}

	connector->sendMessage(ControlInterface::set_video_info, serialized, nullptr, nullptr);
}
#endif

void TVDisplayConnector::display_set_alpha(const std::string &id, double alpha) {
	auto sink = video_channel.connected(id);
	display_set_alpha(sink, alpha);
}

void TVDisplayConnector::display_set_alpha(mdc::sink_t sink, double alpha) {
	static auto to8bit = [](double a) {
		return uint8_t((int)(a * 255. + .5));
	};
	// update internal state
	auto state_it = overlay_states.find(sink);
	if (state_it == overlay_states.end())
		return;
	uint8_t updated_alpha = to8bit(alpha);
	LOG_DEBUG(_log, "(sink:%d) update alpha from %d to %d",
			  (int)sink, state_it->second.alpha, updated_alpha);
	if (updated_alpha == state_it->second.alpha)
		return;
	state_it->second.alpha = updated_alpha;

	// commit updated state
	const auto & main = overlay_states.at(mdc::sink_t::MAIN);
	const auto & sub1 = overlay_states.at(mdc::sink_t::SUB);
	pbnjson::JValue payload = pbnjson::JObject{{"main_alpha", sub1.alpha}, {"main_zorder", sub1.z},
											   {"sub1_alpha", main.alpha}, {"sub1_zorder", main.z}};
	std::string serialized;
	if(!pbnjson::JGenerator(nullptr).toString(payload, pbnjson::JSchema::AllSchema(), serialized)) {
		LOG_ERROR(_log, MSGERR_JSON_SERIALIZE, "json serialize error.");
		return;
	}
	connector->sendMessage(ControlInterface::display_z_order, serialized, nullptr, nullptr);
}

void TVDisplayConnector::display_set_sub_overlay_mode(SubOverlayMode mode) {
#ifdef CONFIG_OVERLAY_MODE
	pbnjson::JValue payload = pbnjson::JObject{{"broadcastId", broadcastId},
											   {"setSubWindowMode", mode == SubOverlayMode::PBP ? "pbp" : "none"}};
	std::string payload_serialized;
	if(pbnjson::JGenerator(nullptr).toString(payload, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		connector->sendMessage(ControlInterface::display_sub_window_mode, payload_serialized, nullptr, nullptr);
	}
#endif
}
void TVDisplayConnector::display_set_config_completed_callback(callback_t && callback) {
	display_config_completed_callback = callback;
}

void TVDisplayConnector::sound_connect(const std::string & id){
	pbnjson::JValue payload = pbnjson::JObject{{"context", id}, {"sinkType", "main"}};

	std::string serialized;
	if(!pbnjson::JGenerator(nullptr).toString(payload, pbnjson::JSchema::AllSchema(), serialized)) {
		LOG_ERROR(_log, MSGERR_JSON_SERIALIZE, "json serialize error.");
		return;
	}

	connector->sendMessage(ControlInterface::connect_sound, serialized, nullptr, nullptr);
}

void TVDisplayConnector::sound_disconnect(const std::string & id){
	pbnjson::JValue payload = pbnjson::JObject{{"context", id}};

	std::string serialized;
	if(!pbnjson::JGenerator(nullptr).toString(payload, pbnjson::JSchema::AllSchema(), serialized)) {
		LOG_ERROR(_log, MSGERR_JSON_SERIALIZE, "json serialize error.");
		return;
	}

	connector->sendMessage(ControlInterface::disconnect_sound, serialized, nullptr, nullptr);
}

void TVDisplayConnector::sound_set_connection_observer(callback_t && callback) {
	sound_connection_observer = callback;
}

bool TVDisplayConnector::vsm_register_reply_hanler(UMSConnectorHandle *,
												   UMSConnectorMessage * reply, void * ctx) {
	TVDisplayConnector * self = static_cast<TVDisplayConnector *>(ctx);
	return self ? self->reply_handler(self->connector->getMessageText(reply), self->registration_callback) : false;
}

bool TVDisplayConnector::reply_handler(const std::string & response, callback_t &callback) {
	pbnjson::JDomParser parser;
	if(!parser.parse(response, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR(_log, MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ", response.c_str());
		return false;
	}
	pbnjson::JValue parsed = parser.getDom();
	if (!parsed.hasKey("context") || !parsed.hasKey("returnValue")) {
		LOG_ERROR(_log, MSGERR_JSON_PARSE, "malformed response");
		return false;
	}

	bool result = parsed["returnValue"].asBool();
	std::string mediaId = parsed["context"].asString();
	// store first registered media id as a key for sub overlay mode settings
	if (result && broadcastId.empty())
		broadcastId = mediaId;
	if (callback)
		callback(mediaId, result);
	return true;
}

//         VSM            vs.            SOUND
// 1. per pipeline                  per sink, don't track pipelines
// 2. reports context               don't report context
// 3. result as bool                result as "on"/"off" string
// 4. result as a single object     result as an array enclosed into object

bool TVDisplayConnector::vsm_subscription_handler(UMSConnectorHandle *, UMSConnectorMessage * reply, void * ctx) {
	TVDisplayConnector * self = static_cast<TVDisplayConnector *>(ctx);
	std::string message = self->connector->getMessageText(reply);
	pbnjson::JDomParser parser;
	if(!parser.parse(message, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR(_log, MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ", message.c_str());
		return false;
	}
	pbnjson::JValue parsed = parser.getDom();
	if (!parsed.hasKey("context") || !parsed.hasKey("connected")) {
		LOG_ERROR(_log, MSGERR_JSON_PARSE, "malformed response");
		return false;
	}
	std::string id = parsed["context"].asString();
	bool connected = parsed["connected"].asBool();
	auto sink = self->video_channel.connected(id);
	if (!connected && sink == mdc::sink_t::SUB)
		self->display_set_sub_overlay_mode(SubOverlayMode::NONE);
	if (self->vsm_connection_observer)
		self->vsm_connection_observer(id, connected);
	return true;
}

// reply format:
//{
//    "returnValue": true,
//    "physical": [
//        {
//            "audioType": "media",
//            "pipelineID": "_T1Ycy1CS3gLfEz"
//        }
//    ]
//}
bool TVDisplayConnector::sound_connection_status_hanler(UMSConnectorHandle *, UMSConnectorMessage * reply, void * ctx) {
	TVDisplayConnector * self = static_cast<TVDisplayConnector *>(ctx);
	std::string message = self->connector->getMessageText(reply);
	pbnjson::JDomParser parser;
	if(!parser.parse(message, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR(_log, MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ", message.c_str());
		return false;
	}
	pbnjson::JValue parsed = parser.getDom();
	if (!parsed.hasKey("physical")) {
		LOG_ERROR(_log, MSGERR_JSON_PARSE, "malformed response");
		return false;
	}
	const auto & physical = parsed["physical"];
	if (!physical.isArray() || !physical.arraySize()) {
		LOG_ERROR(_log, MSGERR_JSON_PARSE, "malformed response");
		return false;
	}
	const auto & status = physical[0];
	if (!status.isObject() || !status.hasKey("pipelineID")) {
		LOG_ERROR(_log, MSGERR_JSON_PARSE, "malformed response");
		return false;
	}
	std::string id = status["pipelineID"].asString();
	std::string type = status["audioType"].asString();
	if (type == "livetv") id = type;
	LOG_DEBUG(_log, "Sound Connection Observer: pipeline(%s).", id.c_str());
	// report only new connection
	if (!id.empty() && self->sound_connection_observer) {
		self->sound_connection_observer(id, true);
	}
	return true;
}


bool TVDisplayConnector::avblock_mute_handler(UMSConnectorHandle *, UMSConnectorMessage * reply, void * ctx) {
	TVDisplayConnector * self = static_cast<TVDisplayConnector *>(ctx);
	std::string message = self->connector->getMessageText(reply);
	pbnjson::JDomParser parser;
	if(!parser.parse(message, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR(_log, MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ", message.c_str());
		return false;
	}
	pbnjson::JValue parsed = parser.getDom();
	if (!parsed.hasKey("returnValue") || !parsed.hasKey("context")) {
		LOG_ERROR(_log, MSGERR_JSON_PARSE, "malformed response");
		return false;
	}
	if (parsed["returnValue"].asBool() && self->avblock_muted_callback) {
		self->avblock_muted_callback(parsed["context"].asString(), true);
		return true;
	}
	return false;
}

bool TVDisplayConnector::avblock_unmute_handler(UMSConnectorHandle *, UMSConnectorMessage * reply, void * ctx) {
	TVDisplayConnector * self = static_cast<TVDisplayConnector *>(ctx);
	std::string message = self->connector->getMessageText(reply);
	pbnjson::JDomParser parser;
	if(!parser.parse(message, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR(_log, MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ", message.c_str());
		return false;
	}
	pbnjson::JValue parsed = parser.getDom();
	if (!parsed.hasKey("returnValue") || !parsed.hasKey("context")) {
		LOG_ERROR(_log, MSGERR_JSON_PARSE, "malformed response");
		return false;
	}
	if (parsed["returnValue"].asBool() && self->avblock_muted_callback) {
		self->avblock_muted_callback(parsed["context"].asString(), false);
		return true;
	}
	return false;
}

bool TVDisplayConnector::display_config_handler(UMSConnectorHandle *, UMSConnectorMessage * reply, void * ctx) {
	TVDisplayConnector * self = static_cast<TVDisplayConnector *>(ctx);
	std::string message = self->connector->getMessageText(reply);
	pbnjson::JDomParser parser;
	if(!parser.parse(message, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR(_log, MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ", message.c_str());
		return false;
	}
	pbnjson::JValue parsed = parser.getDom();
	if (!parsed.hasKey("returnValue") || !parsed.hasKey("context")) {
		LOG_ERROR(_log, MSGERR_JSON_PARSE, "malformed response");
		return false;
	}
	if (parsed["returnValue"].asBool() && self->display_config_completed_callback) {
		self->display_config_completed_callback(parsed["context"].asString(), true);
		return true;
	}
	return false;
}

}
