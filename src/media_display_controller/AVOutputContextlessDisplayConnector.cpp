// Copyright (c) 2016-2018 LG Electronics, Inc.
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
#include "AVOutputContextlessDisplayConnector.h"

namespace uMediaServer {

namespace {
	Logger _log(UMS_LOG_CONTEXT_MDC);
}

namespace AVoutputd
{
	// Video
	static const char* video_get_status = "palm://com.webos.service.avoutput/video/getStatus";
	const char* video_connect = "palm://com.webos.service.avoutput/video/connect";
	const char* video_disconnect = "palm://com.webos.service.avoutput/video/disconnect";
	const char* video_blank = "palm://com.webos.service.avoutput/video/blankVideo";
	const char* video_set_display_window = "palm://com.webos.service.avoutput/video/display/setDisplayWindow";
	const char* video_set_media_data = "palm://com.webos.service.avoutput/video/setMediaData";
	const char* display_set_compositing = "palm://com.webos.service.avoutput/video/display/setCompositing";

	// Audio
	static const char* audio_get_status = "palm://com.webos.service.avoutput/audio/getStatus";
	const char* audio_connect = "palm://com.webos.service.avoutput/audio/connect";
	const char* audio_disconnect = "palm://com.webos.service.avoutput/audio/disconnect";
	const char* audio_mute = "palm://com.webos.service.avoutput/audio/mute";

	const std::string LOGGER_NAME = "ums.avoutput";
}

AVOutputContextlessDisplayConnector::AVOutputContextlessDisplayConnector(UMSConnector * umc, const mdc::IChannelConnection &)
	: connector(umc)
	, log(AVoutputd::LOGGER_NAME)
{
	video_states[0].name = "MAIN";
	video_states[0].z = 0;
	video_states[0].alpha = 255;
	video_states[0].connected = false;
	video_states[0].id = "";
	video_states[1].name = "SUB";
	video_states[1].z = 1;
	video_states[1].alpha = 255;
	video_states[1].connected = false;
	video_states[1].id = "";

	// subscribe to video sink status
	size_t token = connector->subscribe(AVoutputd::video_get_status,
	                                    pbnjson::JObject{{"subscribe", true}}.stringify(),
	                                    videoSinkStatusChange_cb, this);
	if (!token)
	{
		LOG_ERROR(_log, MSGERR_AVOUTPUT_SUBSCRIBE, "Failed to subscribe to video getStatus");
	}

	// subscribe to audio sink status
	token = connector->subscribe(AVoutputd::audio_get_status,
                                 pbnjson::JObject{{"subscribe", true}}.stringify(),
                                 audioConnectionStatusChange_cb, this);
	if (!token)
	{
		LOG_ERROR(_log, MSGERR_AVOUTPUT_SUBSCRIBE, "Failed to subscribe to audio getStatus");
	}
}

void AVOutputContextlessDisplayConnector::vsm_set_registration_callback(callback_t&& callbak)
{
	registration_callback = callbak;
}

void AVOutputContextlessDisplayConnector::vsm_set_connection_observer(callback_t && callback) {
	vsm_connection_observer = callback;
}

void AVOutputContextlessDisplayConnector::sound_set_connection_observer(callback_t && callback) {
	sound_connection_observer = callback;
}

void AVOutputContextlessDisplayConnector::avblock_set_muted_callback(callback_t && callback) {
	avblock_muted_callback = callback;
}

void AVOutputContextlessDisplayConnector::display_set_config_completed_callback(callback_t && callback) {
	display_config_completed_callback = callback;
}

void AVOutputContextlessDisplayConnector::vsm_register(const std::string &id,
                                                       const mdc::res_t & adec, const mdc::res_t & vdec)
{
	registrations.emplace(id, registration_t{adec, vdec});
	if (registration_callback)
		registration_callback(id, true);
}

void AVOutputContextlessDisplayConnector::vsm_unregister(const std::string& id)
{
	// Disconnect audio and video
	video_state_t* video = id_to_vsink(id);
	audio_connection_t* audio = id_to_audio_connection(id);

	if (video && video->connected)
	{
		vsm_disconnect(id);
	}

	if (audio && audio->connected)
	{
		sound_disconnect(id);
	}

	if (registrations.erase(id) != 1)
	{
		LOG_ERROR(_log, MSGERR_MEDIA_ID_NOT_REGISTERD, "Media ID not registered with AvoutputDisplayConnector");
		return;
	}

	if (registration_callback)
		registration_callback(id, false);
}

void AVOutputContextlessDisplayConnector::vsm_connect(const std::string &id, mdc::sink_t sink) {

	registration_t* reg = id_to_registration(id);
	if (!reg)
	{
		LOG_ERROR(_log, MSGERR_MEDIA_ID_NOT_REGISTERD, "Media ID not registered with AvoutputDisplayConnector");
		return;
	}

	video_state_t* sink_state;
	std::string source_name;
	int source_port;

	if (sink == mdc::sink_t::MAIN)
	{
		sink_state = &video_states[0];
	}
	else if (sink == mdc::sink_t::SUB)
	{
		sink_state = &video_states[1];
	}
	else
	{
		LOG_ERROR(_log, MSGERR_AVOUTPUTD_INVALID_PARAMS, "Invalid parameters for video connect: sink");
		return;
	}

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
		LOG_ERROR(_log, MSGERR_AVOUTPUTD_INVALID_PARAMS, "Invalid parameters for video connect: source name not recognized");
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

	connector->sendMessage(AVoutputd::video_connect,
	                       pbnjson::JObject{{"source",     source_name},
	                                        {"sourcePort", source_port},
	                                        {"sink",       sink_state->name},
	                                        {"outputMode",      "DISPLAY"}}
			                       .stringify(),
	                       videoConnectResult_cb, context);
	// Now wait for message response, see next method.
}

int32_t AVOutputContextlessDisplayConnector::get_plane_id(const std::string & id) const {
    if (video_states[0].id == id) return video_states[0].planeId;
    if (video_states[1].id == id) return video_states[1].planeId;
    return -1;
}

bool AVOutputContextlessDisplayConnector::videoConnectResult_cb(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx)
{
	connect_context_t* context = static_cast<connect_context_t*>(ctx);
	AVOutputContextlessDisplayConnector* self = context->connector;
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

void AVOutputContextlessDisplayConnector::vsm_disconnect(const std::string &id) {

	video_state_t* vstate = id_to_vsink(id);
	LOG_DEBUG(log, "Call vsm_disconnect. mediaId=%s", id.c_str());

	if (vstate)
	{
		connector->sendMessage(AVoutputd::video_disconnect,
		                       pbnjson::JObject{{"sink",  vstate->name}}
				                       .stringify(),
		                       nullptr, nullptr);
		// Now wait for subscription update and send disconnected notification
	}
}

void AVOutputContextlessDisplayConnector::sound_connect(const std::string & id){

	registration_t* reg = id_to_registration(id);
	if (!reg)
	{
		LOG_ERROR(_log, MSGERR_MEDIA_ID_NOT_REGISTERD, "Media ID not registered with AvoutputDisplayConnector");
		return;
	}

	if (reg->adec.unit != "ADEC")
	{
		LOG_ERROR(_log, MSGERR_AVOUTPUTD_INVALID_PARAMS, "Invalid parameters for sound connect");
		return;
	}

	// If exists will reuse existing entry
	audio_connection_t& connection = (*audio_connections.emplace(id, audio_connection_t()).first).second;

	connection.id = id;
	connection.sink = "MAIN";
	connection.source = "ADEC";
	connection.sourcePort = reg->adec.index;

	connector->sendMessage(AVoutputd::audio_connect,
	                       pbnjson::JObject{{"sink", connection.sink},
	                                        {"source", connection.source},
	                                        {"sourcePort", connection.sourcePort},
	                                        {"outputMode", "tv_speaker"},
	                                        {"audioType", "media"}}
			                       .stringify(),
	                       nullptr, nullptr);

	// Now wait for subscription update and send connected notification
}

void AVOutputContextlessDisplayConnector::sound_disconnect(const std::string & id){

	audio_connection_t* connection = id_to_audio_connection(id);

	if (!connection)
	{
		return;
	}

	connector->sendMessage(AVoutputd::audio_disconnect,
	                       pbnjson::JObject{{"source",  connection->source},
	                                        {"sourcePort", connection->sourcePort},
	                                        {"sink",  connection->sink}}
			                       .stringify(),
	                       nullptr, nullptr);

	audio_connections.erase(id);
	// Note that no disconnection notification for sound is sent to observer
}

bool AVOutputContextlessDisplayConnector::videoSinkStatusChange(UMSConnectorHandle* , UMSConnectorMessage* message, void*)
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
		LOG_ERROR(_log, MSGERR_JSON_PARSE, "malformed response");
		return false;
	}

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

		video_state_t* state = this->name_to_vsink(sink);

		if (state)
		{
			// Read back current opacity and zorder
			state->z = (uint8_t)video["zOrder"].asNumber<int>();
			state->alpha = (uint8_t) video["opacity"].asNumber<int>();

			// Notify only disconnected, connected is notified via connect callback.
			if (connected == false)
			{
				notifyVideoConnectedChanged(*state, connected);
			}
		}
	}

	return true;
}

void AVOutputContextlessDisplayConnector::notifyVideoConnectedChanged(
		AVOutputContextlessDisplayConnector::video_state_t& state,
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

bool AVOutputContextlessDisplayConnector::audioConnectionStatusChange(UMSConnectorHandle*, UMSConnectorMessage* message, void*)
{
	const char* msg= connector->getMessageText(message);
	pbnjson::JDomParser parser;
	if(!parser.parse(msg, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR(_log, MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ", msg);
		return false;
	}

	pbnjson::JValue parsed = parser.getDom();
	pbnjson::JValue audio_array = parsed["audio"];

	if (!audio_array.isArray())
	{
		LOG_ERROR(_log, MSGERR_JSON_PARSE, "malformed response");
		return false;
	}

	// Report all other connections as not connected
	for (auto& connection: audio_connections)
	{
		connection.second.processed = false;
	}

	for (int i = 0; i < audio_array.arraySize(); i++)
	{
		pbnjson::JValue audio = audio_array[i];
		std::string sink;
		std::string source;
		int sourcePort;

		auto error = audio["sink"].asString(sink);
		error |= audio["source"].asString(source);
		error |= audio["sourcePort"].asNumber(sourcePort);
		if (error)
			break;

		audio_connection_t* connection = this->name_to_audio_connection(source.c_str(), sourcePort, sink.c_str());
		if (connection)
		{
			LOG_DEBUG(_log, "Audio connection - connected: %s,%d->%s", source.c_str(), sourcePort, sink.c_str());

			connection->processed = true;
			if (!connection->connected)
			{
				connection->connected = true;
				if (sound_connection_observer)
					sound_connection_observer(connection->id, true);
			}
		}
		else
		{
			LOG_WARNING(_log, MSGWARN_UNEXPECTED_AUDIO_CONNECTION, "Unknown audio connection %s,%d->%s", source.c_str(), sourcePort, sink.c_str());
		}
	}

	// Mark all other connections as not connected
	for (auto& connection: audio_connections)
	{
		if (connection.second.connected && !connection.second.processed)
		{
			LOG_DEBUG(_log, "Audio connection - disconnected: %s", connection.second.id.c_str());
			connection.second.connected = false;
			//TODO: Not doing this becuase of hacks in MediaDisplayController.
			/*
			if (sound_connection_observer)
				sound_connection_observer(connection.second.id, false);
			 */
		}
	}

	return true;
}

void AVOutputContextlessDisplayConnector::avblock_mute(const std::string &id, size_t channel)
{
	if (channel & mdc::ITVDisplay::VIDEO_CHANNEL)
	{
		mute_video_impl(id, true);
	}
	if (channel & mdc::ITVDisplay::AUDIO_CHANNEL)
	{
		mute_audio_impl(id, true);
	}
}

void AVOutputContextlessDisplayConnector::avblock_unmute(const std::string &id, size_t channel)
{
	if (channel & mdc::ITVDisplay::VIDEO_CHANNEL)
	{
		mute_video_impl(id, false);
	}
	if (channel & mdc::ITVDisplay::AUDIO_CHANNEL)
	{
		mute_audio_impl(id, false);
	}
}

void AVOutputContextlessDisplayConnector::mute_video_impl(const std::string & id, bool mute)
{
	video_state_t* video_state = id_to_vsink(id);
        // FIXME: mute video may be called before connect
        std::string sink_name = "MAIN";
        if (video_state)
	{
                sink_name = video_state->name;
	}
	connect_context_t* context = new connect_context_t();
	context->id = id;
	context->connector = this;
	if (mute)
	{
		connector->sendMessage(AVoutputd::video_blank,
                                       pbnjson::JObject{{"sink",  sink_name},
		                                        {"blank", mute}}.stringify(),
		                       avmuted_cb, context);
	}
	else
	{
		connector->sendMessage(AVoutputd::video_blank,
                                       pbnjson::JObject{{"sink",  sink_name},
		                                        {"blank", mute}}.stringify(),
		                       nullptr, nullptr);
	}
}

bool AVOutputContextlessDisplayConnector::avmuted_cb(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx)
{
	connect_context_t* context = static_cast<connect_context_t*>(ctx);
	AVOutputContextlessDisplayConnector* self = context->connector;
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
	self->avblock_muted_callback(id, success);
	return true;
}

void AVOutputContextlessDisplayConnector::mute_audio_impl(const std::string & id, bool mute)
{
	audio_connection_t* audio_state = id_to_audio_connection(id);
	if (!audio_state)
	{
		LOG_ERROR(_log, MSGERR_MEDIA_ID_NOT_CONNECTED, "Muting audio but id not connected");
		return;
	}

	connector->sendMessage(AVoutputd::audio_mute,
	                       pbnjson::JObject{{"source",  audio_state->source},
	                                        {"sink", audio_state->sink},
	                                        {"sourcePort", audio_state->sourcePort},
	                                        {"mute", mute}}
			                       .stringify(),
	                       nullptr, nullptr);
}

void AVOutputContextlessDisplayConnector::display_set_window(const std::string &id,
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
	connector->sendMessage(AVoutputd::video_set_display_window,  payload.stringify(), display_config_cb, context);
}

bool AVOutputContextlessDisplayConnector::display_config_cb(UMSConnectorHandle *, UMSConnectorMessage * message, void * ctx) {

	connect_context_t* context = static_cast<connect_context_t*>(ctx);
	AVOutputContextlessDisplayConnector* self = context->connector;
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

void AVOutputContextlessDisplayConnector::display_set_video_info(const std::string &id, const mdc::video_info_t &video_info) {
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

	connector->sendMessage(AVoutputd::video_set_media_data, payload.stringify(), nullptr, nullptr);
}

void AVOutputContextlessDisplayConnector::display_set_alpha(const std::string &id, double alpha) {

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

	connector->sendMessage(AVoutputd::display_set_compositing,
	                       pbnjson::JObject{{"mainOpacity", video_states[0].alpha}, {"mainZOrder", video_states[0].z},
	                                        {"subOpacity", video_states[1].alpha}, {"subZOrder", video_states[1].z}}
			                       .stringify(),
	                       nullptr, nullptr);
}

AVOutputContextlessDisplayConnector::registration_t* AVOutputContextlessDisplayConnector::id_to_registration(const std::string& id)
{
	auto iter = registrations.find(id);
	if (iter != registrations.end())
	{
		return &(*iter).second;
	}

	return nullptr;
}

AVOutputContextlessDisplayConnector::video_state_t* AVOutputContextlessDisplayConnector::id_to_vsink(const std::string& id)
{
	for (int i = 0; i < 2; i ++)
	{
		if (video_states[i].id == id)
		{
			return &video_states[i];
		}
	}
	return nullptr;
}

AVOutputContextlessDisplayConnector::video_state_t* AVOutputContextlessDisplayConnector::name_to_vsink(const std::string& name)
{
	for (int i = 0; i < 2; i ++)
	{
		if (video_states[i].name == name)
		{
			return &video_states[i];
		}
	}
	return nullptr;
}

AVOutputContextlessDisplayConnector::audio_connection_t* AVOutputContextlessDisplayConnector::id_to_audio_connection(const std::string& id)
{
	auto iter = audio_connections.find(id);
	if (iter != audio_connections.end())
	{
		return &(*iter).second;
	}

	return nullptr;
}

AVOutputContextlessDisplayConnector::audio_connection_t* AVOutputContextlessDisplayConnector::name_to_audio_connection(const std::string& sourceName, int sourcePort, const std::string& sinkName)
{
	for (auto& pair: audio_connections)
	{
		if (pair.second.source == sourceName && pair.second.sourcePort == sourcePort && pair.second.sink == sinkName)
		{
			return &pair.second;
		}
	}
	return nullptr;
}
}
