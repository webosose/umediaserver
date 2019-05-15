// Copyright (c) 2019 LG Electronics, Inc.
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
#include "VideooutputConnector.h"

namespace uMediaServer {

namespace {
	Logger _log(UMS_LOG_CONTEXT_MDC);
}

namespace Videooutputd
{
	// Video
	const char* video_get_status = "palm://com.webos.service.videooutput/getStatus";
	const char* video_register = "palm://com.webos.service.videooutput/register";
	const char* video_unregister = "palm://com.webos.service.videooutput/unregister";
	const char* video_connect = "palm://com.webos.service.videooutput/connect";
	const char* video_disconnect = "palm://com.webos.service.videooutput/disconnect";
	const char* video_blank = "palm://com.webos.service.videooutput/blankVideo";
	const char* video_getparam = "palm://com.webos.service.videooutput/display/getParam";
	const char* video_set_display_window = "palm://com.webos.service.videooutput/display/setDisplayWindow";
	const char* video_set_video_data = "palm://com.webos.service.videooutput/setVideoData";
	const char* display_set_compositing = "palm://com.webos.service.videooutput/display/setCompositing";
	const char* display_get_capability = "palm://com.webos.service.videooutput/display/getOutputCapabilities";

	const std::string LOGGER_NAME = "ums.videooutput";
}

VideooutputConnector::VideooutputConnector(UMSConnector * umc, mdc::IConnectionPolicy & connection_policy)
	: connector(umc)
	, log(Videooutputd::LOGGER_NAME)
	, max_video_sink(0)
	, _connection_policy(connection_policy)
{
	// subscribe to video sink status
	size_t token = connector->subscribe(Videooutputd::video_get_status,
	                                    pbnjson::JObject{{"subscribe", true}}.stringify(),
	                                    videoSinkStatusChange_cb, this);
	if (!token)
	{
		LOG_ERROR(_log, MSGERR_VIDEOOUTPUT_SUBSCRIBE, "Failed to subscribe to video getStatus");
	}
}

void VideooutputConnector::vsm_set_registration_callback(callback_t&& callbak)
{
	registration_callback = callbak;
}

void VideooutputConnector::vsm_set_connection_observer(callback_t && callback) {
	vsm_connection_observer = callback;
}

void VideooutputConnector::sound_set_connection_observer(callback_t && callback) {
	// DEPRECATED_API
}

void VideooutputConnector::avblock_set_muted_callback(callback_t && callback) {
	avblock_muted_callback = callback;
}

void VideooutputConnector::display_set_config_completed_callback(callback_t && callback) {
	display_config_completed_callback = callback;
}

void VideooutputConnector::vsm_register(const std::string &id,
                                                       const mdc::res_t & adec, const mdc::res_t & vdec)
{
	registrations.emplace(id, registration_t{adec, vdec});
	if (registration_callback)
		registration_callback(id, true);
}

void VideooutputConnector::vsm_unregister(const std::string& id)
{
	// Disconnect video
	video_state_t* video = id_to_vsink(id);

	if (video && video->connected)
	{
		vsm_disconnect(id);
	}

	if (registrations.erase(id) != 1)
	{
		LOG_ERROR(_log, MSGERR_MEDIA_ID_NOT_REGISTERD, "Media ID not registered with videooutputDisplayConnector");
		return;
	}

	if (registration_callback)
		registration_callback(id, false);
}

void VideooutputConnector::vsm_connect(const std::string &id, int32_t sink) {

	registration_t* reg = id_to_registration(id);
	if (!reg)
	{
		LOG_ERROR(_log, MSGERR_MEDIA_ID_NOT_REGISTERD, "Media ID not registered with videooutputDisplayConnector");
		return;
	}

	video_state_t* sink_state;
	std::string source_name;
	int source_port;

	if ( sink >= 0 )
	{
		sink_state = &video_states[sink];
	}
	else
	{
		LOG_ERROR(_log, MSGERR_VIDEOOUTPUTD_INVALID_PARAMS, "Invalid parameters for video connect: sink");
		return;
	}

	LOG_DEBUG(log, "sink id:%d, name:%s, reg size:%d", sink, sink_state->name.c_str(), registrations.size());

	if (reg->vdec.unit == "HDMI_INPUT")
	{
		source_name = "HDMI";
		source_port = reg->vdec.index;
	}
	else if (reg->vdec.unit == "IMG_DEC")
	{
		source_name = "IDEC";
		source_port = reg->vdec.index;
	}
	else if (reg->vdec.unit == "VDEC")
	{
		source_name = "VDEC";
		source_port = reg->vdec.index;
	}
	else if (reg->vdec.unit == "VADC")
	{
		source_name = "RGB";
		source_port = reg->vdec.index;
	}
	else
	{
		LOG_ERROR(_log, MSGERR_VIDEOOUTPUTD_INVALID_PARAMS, "Invalid parameters for video connect: source name not recognized");
		return;
	}

	LOG_DEBUG(log, "Call vsm_connect. mediaId=%s", id.c_str());

	if (sink_state->id != id)
	{
		// send disconnected notification if currently connected
		notifyVideoConnectedChanged(*sink_state, false);
	}

	sink_state->id = id;
	sink_state->connected = false;

	connect_context_t* context = new connect_context_t();
	context->id = id;
	context->connector = this;

	connector->sendMessage(Videooutputd::video_connect,
			pbnjson::JObject{{"source",     source_name},
			{"sourcePort", source_port},
			{"sink",       sink_state->name},
			{"outputMode",      "DISPLAY"}}
			.stringify(),
			videoConnectResult_cb, context);
	// Now wait for message response, see next method.
}

int32_t VideooutputConnector::get_plane_id(const std::string & id) const {
	for(int i = 0; i < max_video_sink; i++) {
		if (video_states[i].id == id) {
			LOG_DEBUG(_log, "idx:%d, plane_id:%d", i, video_states[i].planeId);
			return video_states[i].planeId;
		}
	}
	return -1;
}

void VideooutputConnector::acquire_display_resource(const std::string & plane_name, const int32_t index, ums::disp_res_t & res) {
	int idx = 0;
	LOG_DEBUG(_log, "requested plane name : %s", plane_name.c_str());
	for(int i = 0; i < max_video_sink; i++) {
		if (video_states[i].name.find(plane_name) != std::string::npos) {
			if (!video_states[i].acquired && (idx == index)) {
				LOG_DEBUG(_log, "return corresponding plane id %d", video_states[i].planeId);
				video_states[i].acquired = true;
				res.plane_id = video_states[i].planeId;
				res.crtc_id = video_states[i].crtcId;
				res.conn_id = video_states[i].connId;
				break;
			} else {
				LOG_DEBUG(_log, "already acquired or index in not matched(%d/%d), find other candidates",idx,index);
			}
			idx++;
		}
	}
}

void VideooutputConnector::release_display_resource(const std::string & plane_name, const int32_t index) {
	int idx = 0;
	LOG_DEBUG(_log, "requested plane name : %s", plane_name.c_str());
	for(int i = 0; i < max_video_sink; i++) {
		if (video_states[i].name.find(plane_name) != std::string::npos) {
			if (video_states[i].acquired && (idx == index)) {
				LOG_DEBUG(_log, "acquired status for plane id %d is changed to false", video_states[i].planeId);
				video_states[i].acquired = false;
				break;
			} else {
				LOG_DEBUG(_log, "already released or index is not matched(%d/%d), find other candidates",idx,index);
			}
			idx++;
		}
	}
}

bool VideooutputConnector::videoConnectResult_cb(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx)
{
	connect_context_t* context = static_cast<connect_context_t*>(ctx);
	VideooutputConnector* self = context->connector;
	std::string id = context->id;
	delete context;

	LOG_DEBUG(self->log, "Return vsm_connect. mediaId=%s", id.c_str());

	const char* msg = self->connector->getMessageText(message);
	pbnjson::JDomParser parser;
	if (!parser.parse(msg, pbnjson::JSchema::AllSchema()))
	{
		LOG_ERROR(_log,
		          MSGERR_JSON_PARSE,
		          "ERROR JDomParser.parse. raw=%s ",
		          msg);
		return false;
	}

	pbnjson::JValue parsed = parser.getDom();
	bool success = parsed.isObject() && parsed["returnValue"].asBool();
	auto* state = self->id_to_vsink(id);

	if (state)
	{
		if (success)
		{
			state->planeId = parsed["planeID"].asNumber<int32_t>();
			self->notifyVideoConnectedChanged(*state, true);
		}
		else
		{
			//Force notify disconnected
			state->planeId = -1;
			state->connected = true;
			self->notifyVideoConnectedChanged(*state, false);
		}
	}

	return true;
}

void VideooutputConnector::vsm_disconnect(const std::string &id) {
	video_state_t* vstate = id_to_vsink(id);
	if (vstate)
	{
		LOG_DEBUG(log, "Call vsm_disconnect. mediaId=%s, sinkname:%s", id.c_str(), vstate->name.c_str());
		connector->sendMessage(Videooutputd::video_disconnect,
		                       pbnjson::JObject{{"sink",  vstate->name}}
				                       .stringify(),
		                       nullptr, nullptr);
		// Now wait for subscription update and send disconnected notification
	}
}

void VideooutputConnector::sound_connect(const std::string & id){
	// DEPRECATED_API
}

void VideooutputConnector::sound_disconnect(const std::string & id){
	// DEPRECATED_API
}

bool VideooutputConnector::videoSinkStatusChange(UMSConnectorHandle* , UMSConnectorMessage* message, void*)
{
	const char* msg = connector->getMessageText(message);
	pbnjson::JDomParser parser;
	if (!parser.parse(msg, pbnjson::JSchema::AllSchema()))
	{
		LOG_ERROR(_log,
		          MSGERR_JSON_PARSE,
		          "ERROR JDomParser.parse. raw=%s ",
		          msg);
		return false;
	}

	pbnjson::JValue parsed = parser.getDom();
	pbnjson::JValue video_array = parsed["video"];

	if (!video_array.isArray())
	{
		LOG_ERROR(_log, MSGERR_JSON_PARSE, "malformed response (%s)", msg);
		return false;
	}

	max_video_sink = video_array.arraySize();
	bool is_video_states_empty = video_states.empty();
	for (int i = 0; i < video_array.arraySize(); i++)
	{
		pbnjson::JValue video = video_array[i];
		std::string sink;
		// when connectedSource = null, connected = false
		// when connectedSource is a sink name, connected = true
		bool connected = !video["connectedSource"].isNull();

		auto error = video["sink"].asString(sink);
		if (error || !video["zOrder"].isNumber() || !video["opacity"].isNumber())
			break;

		uint8_t zorder = (uint8_t)video["zOrder"].asNumber<int>();
		uint8_t opacity = (uint8_t)video["opacity"].asNumber<int>();
		if (is_video_states_empty)
		{
			video_state_t state = {opacity, zorder, "", sink, connected, false, 0, 0, 0};
			video_states.push_back(state);
			_connection_policy.set_video_object(i, sink);

			video_state_t* sink_state = &video_states[i];

			connect_context_t* context = new connect_context_t();
			context->connector = this;
			// get display resource info
			LOG_DEBUG(_log, "video_getparam sink:%s", sink_state->name.c_str());
			connector->sendMessage(Videooutputd::video_getparam,
					pbnjson::JObject{{"command", "DRMResources"}, {"sink", sink_state->name.c_str()}}
					.stringify(),
					getparam_cb, context);
		}
		else
		{
			video_state_t* state = this->name_to_vsink(sink);
			if (state)
			{
				// Read back current opacity and zorder
				state->z = zorder;
				state->alpha = opacity;
			}
		}
		// Notify only disconnected, connected is notified via connect callback.
		if (connected == false)
		{
			video_state_t* state = this->name_to_vsink(sink);
			if (state)
			{
				notifyVideoConnectedChanged(*state, connected);
			}
		}
	}

	for (int i = 0; i < video_array.arraySize(); i++)
	{
		LOG_DEBUG(_log, "video states %d name : %s, planeid : %d, id : %s, connected : %d, acquired : %d", i, \
		video_states[i].name.c_str(), video_states[i].planeId, video_states[i].id.c_str(), \
		video_states[i].connected, video_states[i].acquired);
	}

	return true;
}

void VideooutputConnector::notifyVideoConnectedChanged(
		VideooutputConnector::video_state_t& state,
		bool connected)
{
	if (state.connected != connected && state.id != "")
	{
		state.connected = connected;
		if (vsm_connection_observer)
		{
			LOG_DEBUG(log, "Notify controller connected changed, mediaId=%s, conected=%d", state.id.c_str(), connected);
			vsm_connection_observer(state.id, connected);
		}
		//Disassociate the sink from id when going connected->disconnected.
		if (connected == false)
		{
			state.id = "";
		}
	}
}

void VideooutputConnector::avblock_mute(const std::string &id, size_t channel)
{
	if (channel & mdc::ITVDisplay::VIDEO_CHANNEL)
	{
		mute_video_impl(id, true);
	}
}

void VideooutputConnector::avblock_unmute(const std::string &id, size_t channel)
{
	if (channel & mdc::ITVDisplay::VIDEO_CHANNEL)
	{
		mute_video_impl(id, false);
	}
}

void VideooutputConnector::mute_video_impl(const std::string & id, bool mute)
{
	video_state_t* video_state = id_to_vsink(id);
	// FIXME: mute video may be called before connect
	std::string sink_name = "DISP0_MAIN";
	if (video_state)
	{
		sink_name = video_state->name;
	}

	connect_context_t* context = new connect_context_t();
	context->id = id;
	context->connector = this;
	if (mute)
	{
		connector->sendMessage(Videooutputd::video_blank,
				pbnjson::JObject{{"sink",  sink_name},
				{"blank", mute}}.stringify(),
				avmuted_cb, context);
	}
	else
	{
		connector->sendMessage(Videooutputd::video_blank,
				pbnjson::JObject{{"sink",  sink_name},
				{"blank", mute}}.stringify(),
				nullptr, nullptr);
        // Context is usually deleted in the callback function
        // If the callback is not set, the context variable must be deleted
		delete context;
	}
}

bool VideooutputConnector::avmuted_cb(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx)
{
	connect_context_t* context = static_cast<connect_context_t*>(ctx);
	VideooutputConnector* self = context->connector;
	std::string id = context->id;
	delete context;

	LOG_DEBUG(self->log, "Return avmuted. mediaId=%s", id.c_str());

	const char* msg = self->connector->getMessageText(message);
	pbnjson::JDomParser parser;
	if (!parser.parse(msg, pbnjson::JSchema::AllSchema()))
	{
		LOG_ERROR(_log,
		          MSGERR_JSON_PARSE,
		          "ERROR JDomParser.parse. raw=%s ",
		          msg);
		return false;
	}

	pbnjson::JValue parsed = parser.getDom();
	bool success = parsed.isObject() && parsed["returnValue"].asBool();
	if (!success)
	{
		LOG_ERROR(_log, "AV_MUTE_ERROR", "ERROR : video blank return false, media object status will not be changed");
	}
	self->avblock_muted_callback(id, success);
	return true;
}

void VideooutputConnector::display_set_window(const std::string &id,
                                            const mdc::display_out_t &display_out) {
	video_state_t* video_state = id_to_vsink(id);
	if (!video_state)
	{
		LOG_ERROR(_log, MSGERR_MEDIA_ID_NOT_CONNECTED, "Cannot set display window, id not connected");
		return;
	}

	pbnjson::JValue dst_win = pbnjson::JObject{
			{"x", display_out.out_rect.x},
			{"y", display_out.out_rect.y},
			{"width", display_out.out_rect.w},
			{"height", display_out.out_rect.h}};
	pbnjson::JValue payload = pbnjson::JObject{{"sink", video_state->name},
	                                           {"displayOutput", dst_win},
	                                           {"fullScreen", display_out.fullscreen}};

	if (display_out.src_rect) {
		pbnjson::JValue src_win = pbnjson::JObject{
				{"x", display_out.src_rect.x},
				{"y", display_out.src_rect.y},
				{"width", display_out.src_rect.w},
				{"height", display_out.src_rect.h}};
		payload.put("sourceInput", src_win);
	}

	connect_context_t* context = new connect_context_t();
	context->id = id;
	context->connector = this;
	connector->sendMessage(Videooutputd::video_set_display_window,  payload.stringify(), display_config_cb, context);
}

bool VideooutputConnector::display_config_cb(UMSConnectorHandle *, UMSConnectorMessage * message, void * ctx) {

	connect_context_t* context = static_cast<connect_context_t*>(ctx);
	VideooutputConnector* self = context->connector;
	std::string id = context->id;
	delete context;
	const char* msg = self->connector->getMessageText(message);
	pbnjson::JDomParser parser;
	if (!parser.parse(msg, pbnjson::JSchema::AllSchema()))
	{
		LOG_ERROR(_log,
		          MSGERR_JSON_PARSE,
		          "ERROR JDomParser.parse. raw=%s ",
		          msg);
		return false;
	}

	pbnjson::JValue parsed = parser.getDom();
	bool success = parsed.isObject() && parsed["returnValue"].asBool();
	if (success && self->display_config_completed_callback) {
                self->display_config_completed_callback(id, true);
		return true;
	}
	return false;
}
#if UMS_INTERNAL_API_VERSION == 2
void VideooutputConnector::display_set_video_info(const std::string &id, const ums::video_info_t &video_info) {
	video_state_t* video_state = id_to_vsink(id);
	if (!video_state)
	{
		LOG_ERROR(_log, MSGERR_MEDIA_ID_NOT_CONNECTED, "Cannot set video info, id not connected");
		return;
	}

	// bitRate and codec information doesn't needed from videooutputD.
	pbnjson::JValue payload = pbnjson::JObject{
			{"sink", video_state->name},
			{"frameRate", (double)video_info.frame_rate.num / (double)video_info.frame_rate.den},
			{"width", (int32_t)video_info.width},
			{"height", (int32_t)video_info.height},
			{"codec", video_info.codec},
			{"bitRate", (int64_t)video_info.bit_rate}
	};

	connector->sendMessage(Videooutputd::video_set_video_data, payload.stringify(), nullptr, nullptr);
}
#else
void VideooutputConnector::display_set_video_info(const std::string &id, const mdc::video_info_t &video_info) {
	video_state_t* video_state = id_to_vsink(id);
	if (!video_state)
	{
		LOG_ERROR(_log, MSGERR_MEDIA_ID_NOT_CONNECTED, "Cannot set video info, id not connected");
		return;
	}

	pbnjson::JValue payload = pbnjson::JObject{
			{"sink", video_state->name},
			{"contentType", video_info.content},
			{"frameRate", video_info.frame_rate},
			{"width", video_info.width},
			{"height", video_info.height},
			{"scanType", video_info.scan_type},
			{"adaptive", video_info.adaptive}
	};

	connector->sendMessage(Videooutputd::video_set_video_data, payload.stringify(), nullptr, nullptr);
}
#endif

void VideooutputConnector::display_set_alpha(const std::string &id, double alpha) {

	video_state_t* video_state = id_to_vsink(id);
	if (!video_state)
	{
		LOG_ERROR(_log, MSGERR_MEDIA_ID_NOT_CONNECTED, "Cannot set alpha, id not connected");
		return;
	}

	uint8_t int_alpha = uint8_t((int)(alpha * 255. + .5));
	uint8_t* target_alpha = &video_state->alpha;

	LOG_DEBUG(_log, "(sink:%s) update alpha from %d to %d", video_state->name.c_str(), *target_alpha, int_alpha);

	if (int_alpha == *target_alpha)
		return;

	*target_alpha = int_alpha;

	connector->sendMessage(Videooutputd::display_set_compositing,
	                       pbnjson::JObject{{"mainOpacity", video_states[0].alpha}, {"mainZOrder", video_states[0].z},
	                                        {"subOpacity", video_states[1].alpha}, {"subZOrder", video_states[1].z}}
			                       .stringify(),
	                       nullptr, nullptr);
}

bool VideooutputConnector::getparam_cb(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx)
{
	connect_context_t* context = static_cast<connect_context_t*>(ctx);
	VideooutputConnector* self = context->connector;
	std::string sink;
	delete context;

	LOG_DEBUG(self->log, "Return getParam.");

	const char* msg = self->connector->getMessageText(message);
	pbnjson::JDomParser parser;
	if (!parser.parse(msg, pbnjson::JSchema::AllSchema()))
	{
		LOG_ERROR(_log,
				MSGERR_JSON_PARSE,
				"ERROR JDomParser.parse. raw=%s ",
				msg);
		return false;
	}

	pbnjson::JValue parsed = parser.getDom();
	bool success = parsed.isObject() && parsed["returnValue"].asBool();
	if (!success)
	{
		LOG_ERROR(_log, "VIDEOOUTPUTD_GET_PARAM_ERROR", "ERROR : getParam return false, fail to get connId, crtcId, planeId");
	}
	else
	{
		auto error = parsed["sink"].asString(sink);
		if (error || !parsed["planeId"].isNumber() || !parsed["crtcId"].isNumber() || !parsed["connId"].isNumber())
		{
			LOG_ERROR(_log, "VIDEOOUTPUTD_GET_PARAM_ERROR", "ERROR : fail to get valid sink, connId, crtcId, planeId");
		}
		else
		{
			video_state_t* state = self->name_to_vsink(sink);
			if (state)
			{
				state->planeId = (uint8_t)parsed["planeId"].asNumber<int>();
				state->crtcId = (uint8_t)parsed["crtcId"].asNumber<int>();
				state->connId = (uint8_t)parsed["connId"].asNumber<int>();
				LOG_DEBUG(self->log, "sink:%s, planeId:%d, crtcId:%d, connId:%d", sink.c_str(), state->planeId, state->crtcId, state->connId);
			}
		}
	}
	return true;
}

VideooutputConnector::registration_t* VideooutputConnector::id_to_registration(const std::string& id)
{
	auto iter = registrations.find(id);
	if (iter != registrations.end())
	{
		return &(*iter).second;
	}

	return nullptr;
}

VideooutputConnector::video_state_t* VideooutputConnector::id_to_vsink(const std::string& id)
{
	for (int i = 0; i < max_video_sink; i ++)
	{
		if (video_states[i].id == id)
		{
			return &video_states[i];
		}
	}
	return nullptr;
}

VideooutputConnector::video_state_t* VideooutputConnector::name_to_vsink(const std::string& name)
{
	for (int i = 0; i < max_video_sink; i ++)
	{
		if (video_states[i].name == name)
		{
			return &video_states[i];
		}
	}
	return nullptr;
}
}
