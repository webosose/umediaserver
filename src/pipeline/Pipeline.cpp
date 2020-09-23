// Copyright (c) 2008-2019 LG Electronics, Inc.
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

#include <chrono>
#include <GenerateUniqueID.h>
#include <Pipeline.h>
#include <Process.h>
#include <Logger_macro.h>
#include <pbnjson.hpp>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <umsTrace.h>

using namespace std;
using namespace pbnjson;

namespace uMediaServer {

namespace {

Logger _log(UMS_LOG_CONTEXT_PIPELINE_CONTROLLER);

JValue marshalllonglong(long long value) {
	JValue obj = (int64_t) value;
	return obj;
}

long long unmarshalllonglong(JValue value) {
	int64_t unmarshalled = 0;
	if ((!value.isNull()) && (CONV_OK != value.asNumber(unmarshalled))) {
		LOG_ERROR(_log, MSGERR_JSON_UNMARSHALL, "failure to unmarshall.");
	}
	return (long long) unmarshalled;
}

JValue marshallfloat(float value) {
	JValue obj =(double) value;
	return obj;
}

float unmarshallfloat(JValue value) {
	double unmarshalled = 0.0;
	if ((!value.isNull()) && (CONV_OK != value.asNumber(unmarshalled))) {
		LOG_ERROR(_log, MSGERR_JSON_UNMARSHALL, "failure to unmarshall.");
	}
	return (float) unmarshalled;
}

JValue marshalllong(long value) {
	JValue obj = (int32_t) value;
	return obj;
}

long unmarshalllong(JValue value) {
	int32_t unmarshalled = 0;
	if ((!value.isNull()) && (CONV_OK != value.asNumber(unmarshalled))) {
		LOG_ERROR(_log, MSGERR_JSON_UNMARSHALL, "failure to unmarshall.");
	}
	return (long) unmarshalled;
}

JValue marshallboolean(bool value) {
	JValue obj = (bool) value;
	return obj;
}

bool unmarshallboolean(JValue value) {
	bool ret = false;
	if ((!value.isNull()) && (CONV_OK != value.asBool(ret))) {
		LOG_ERROR(_log, MSGERR_JSON_UNMARSHALL, "failure to unmarshall.");
	}
	return ret;
}

JValue marshallPayload(const std::string & value) {
	JDomParser parser;
	if (!parser.parse(value, pbnjson::JSchema::AllSchema())) {
		return 0;
	}

	return parser.getDom();
}

JValue marshallstring(const std::string & value) {
	JValue obj = value;
	return obj;
}

std::string unmarshallstring(JValue value) {
	std::string unmarshalled;
	if (! value.isNull()) {
		unmarshalled = value.asString();
	}
	return unmarshalled;
}

}

std::map<std::string, std::unique_ptr<pbnjson::JSchema>> Pipeline::api_schema;

Pipeline::Pipeline(const string & type, ProcessPool & pool)
: log(UMS_LOG_CONTEXT_PIPELINE_CONTROLLER),
  type_(type),
  process_pool(pool),
  m_restarting(false),
  m_preloading(false),
  m_subscribed(false),
  m_process_starting(false),
  client_connector(NULL),
  m_pipeline_json_state(this)
{
	// TODO allow display to be routed to different display dynamically
	updatePipelineProcessState(PIPELINE_STOP);
	updatePipelineMediaState(MEDIA_STOP);

	id_ = GenerateUniqueID()();
	size_t offset = id_.length() > UMEDIASERVER_UNIQUE_ID_LENGTH ?
			id_.length() - UMEDIASERVER_UNIQUE_ID_LENGTH : 0;
	log.setUniqueId(id_.c_str() + offset);

	std::string connection_id = PIPELINE_CONTROLLER_CONNECTION_BASE_ID + id_;
	// Use default main loop and default context of uMS Core
	pipeline_connector = new UMSConnector(connection_id, nullptr,
			static_cast<void*>(this), UMS_CONNECTOR_PRIVATE_BUS, true);
}

Pipeline::~Pipeline()
{
	delete pipeline_connector;

	// send unloadComplete if we missed it
	if (m_subscribed && client_connector &&
			!m_pipeline_json_state.getJsonObj().hasKey("unloadCompleted")) {
		JValue unload_completed(Object()), payload_(Object());
		payload_.put("state", true);
		payload_.put("mediaId", id_);
		unload_completed.put("unloadCompleted", payload_);
		std::string json_message;
		JGenerator().toString(unload_completed,  pbnjson::JSchema::AllSchema(), json_message);
		try {
			client_connector->sendChangeNotificationJsonString(json_message, id_);
		} catch(const boost::bad_get &e) {
			LOG_ERROR(log, MSGERR_JSON_SCHEMA, "%s", e.what());
		}
	}
}

string Pipeline::getProcessState()
{
	JValue state = m_pipeline_json_state.getJsonObj();
	if (!state.hasKey("proc_state")) {
		LOG_ERROR(log, MSGERR_JSON_SCHEMA, "Missing proc_state field in state json object.");
		return "";
	}
	return unmarshallstring(state["proc_state"]);
}

void Pipeline::updatePipelineProcessState(string new_state)
{
	JValue procstate = Object();
	procstate.put("proc_state", marshallstring(new_state));
	m_pipeline_json_state.update(procstate);
	signal_pipeline_status_changed();
}

string Pipeline::getMediaState()
{
	JValue state = m_pipeline_json_state.getJsonObj();
	if (!state.hasKey("media_state")) {
		LOG_ERROR(log, MSGERR_JSON_SCHEMA, "Missing media_state field in state json object.");
		return "";
	}
	return unmarshallstring(state["media_state"]);
}

void Pipeline::updatePipelineMediaState(string new_state)
{
	JValue mediastate = Object();
	mediastate.put("media_state", marshallstring(new_state));
	m_pipeline_json_state.update(mediastate);
	signal_pipeline_status_changed();
}

void Pipeline::setSeekPos(long long seek_pos)
{
	JValue seekpos = Object();
	seekpos.put("seek_pos", marshalllonglong(seek_pos));
	m_pipeline_json_state.update(seekpos);
}

long long Pipeline::getSeekPos()
{
	JValue state = m_pipeline_json_state.getJsonObj();
	if (!state.hasKey("seek_pos")) {
		LOG_ERROR(log, MSGERR_JSON_SCHEMA, "Missing seek_pos field in state json object.");
		return 0;
	}
	return unmarshalllonglong(state["seek_pos"]);
}

void Pipeline::updateState(const string & msg)
{
	JDomParser parser;
	JValue stateupdate;

	if (!parser.parse(msg, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR(log, MSGERR_JSON_PARSE,
				"Failed to parse state update json string: %s", msg.c_str());
		return;
	}
	stateupdate = parser.getDom();

        if (stateupdate.hasKey("preloadCompleted"))
                processPreloadCompleted();
	else if (stateupdate.hasKey("loadCompleted"))
		processLoadCompleted();

	m_pipeline_json_state.update(stateupdate);

	return;
}

std::string Pipeline::updatePayloadForResume()
{
	JDomParser parser;
	JValue trans;
	JValue opt;

	if (! parser.parse(payload_, pbnjson::JSchema::AllSchema())) {
		return payload_;
	}

	JValue parsed = parser.getDom();

	if(parsed.hasKey("option")) {
		opt = parsed["option"];
		if(opt.hasKey("transmission")) {
			trans = parsed["option"]["transmission"];
		}
		else {
			trans = Object();
			opt.put("transmission",trans);
		}
	}
	else {
		opt = Object();
		parsed.put("option",opt);
		trans = Object();
		opt.put("transmission",trans);
	}

	long long current_time;
	JGenerator serializer(NULL);
	JValue time_ = Object();

	current_time = unmarshalllonglong(m_pipeline_json_state.getJsonObj()["currentTime"]["currentTime"]);
	time_.put("realTime",false);
	time_.put("start",(int64_t)current_time);
	trans.put("playTime",time_);

	std::string payload_serialized;

	if (! serializer.toString(parsed,  pbnjson::JSchema::AllSchema(), payload_serialized)) {
		return payload_;
	}
	return payload_serialized;
}

void Pipeline::startProcess() {
	m_process_starting = true;
        if (getProcessState() == PIPELINE_MEDIA_PRELOADED) {
          m_preloading = false;
          LOG_DEBUG(log, "%s preloaded, no create new process", __FUNCTION__);
          finishLoading(process_connection_id, process_handle);
        } else {
	  weak_ptr_t weak_this = shared_from_this();
	  process_pool.hire(type_, id_, [weak_this](const std::string & service, Process::ptr_t p){
		ptr_t shared_this = weak_this.lock();
		if (shared_this) {
			shared_this->finishLoading(service, p);
		}
	  });
       }
}

std::string Pipeline::serializeLoadArgs(const JValue & options) {
	auto serialize_load = [this](const JValue & load_options)->std::string {
		JValue args = Array();
		args.append(uri_);
		args.append(load_options);
		args.append(id_);

		JValue payload_jval = Object();   // push array of values into "arg" element
		payload_jval.put("args", args);
		m_pipeline_json_state.update(payload_jval);

		JGenerator serializer(NULL);   // serialize into string
		std::string payload_serialized;

		if (!serializer.toString(payload_jval, pbnjson::JSchema::AllSchema(), payload_serialized)) {
			LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failed to serialize payload.");
			return "";
		}
		return payload_serialized;
	};
	auto serialize_load_v2 = [this](const JValue & load_options)->std::string {
		pbnjson::JValue payload_jval = pbnjson::JObject{{"uri", uri_}, {"options", load_options}, {"id", id_}};
		pbnjson::JGenerator serializer(nullptr);
		std::string payload_serialized;
		if (!serializer.toString(payload_jval, pbnjson::JSchema::AllSchema(), payload_serialized)) {
			LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failed to serialize payload.");
			return "";
		}
		return payload_serialized;
	};
#if UMS_INTERNAL_API_VERSION == 2
	return serialize_load_v2(options);
#else
	return serialize_load(options);
#endif
}

void Pipeline::finishLoading (const string &service_name, Process::ptr_t process)
{
	process_handle = process;
	process_connection_id = service_name;
	m_process_starting = false;
	std::string load_exec = id_ + std::string("_load_exec");
	UMSTRACE_AFTER(load_exec.c_str());

	updatePipelineProcessState(PIPELINE_RUNNING);
	signal_pid(appid_, process_handle->pid(), true);

	// subscribe for regular state change updates
	std::string cmd = process_connection_id + "/stateChange";
	pipeline_connector->subscribe(cmd, "{}", pipelineProcessStateEvent, this);

	if (m_preloading) {
		// pass load command
		cmd = process_connection_id + "/preload";
	} else {
		// pass load command
		cmd = process_connection_id + "/load";
	}

	auto parse_options = [](const std::string & payload) {
		JDomParser parser;
		if(parser.parse(payload,  pbnjson::JSchema::AllSchema())) {
			return parser.getDom();
		} else {
			pbnjson::JValue empty_options = pbnjson::Object();
			return empty_options;
		}
	};

	JValue options;
	if (m_restarting)
		options = parse_options(updatePayloadForResume());
	else
		options = parse_options(payload_);

	auto payload_serialized = serializeLoadArgs(options);

	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);
	std::string load_load = id_ + std::string("_load_load");
	UMSTRACE_BEFORE(load_load.c_str());
}

bool Pipeline::processLoadCompleted()
{
	std::string load_load = id_ + std::string("_load_load");
	UMSTRACE_AFTER(load_load.c_str());
	string media_state = getMediaState();  // stash

	updatePipelineProcessState(PIPELINE_MEDIA_LOADED);

	LOG_DEBUG(log, "+++ processing deferred commands +++");
	JValue state = m_pipeline_json_state.getJsonObj();
	for ( size_t i = 0; i < m_command_queue.size() ; ++i ) {
		LOG_DEBUG(log, "+ command = %s, payload = %s ",
				m_command_queue[i].command.c_str(),
				m_command_queue[i].payload.c_str());

		if ( "unload" == m_command_queue[i].command ) {
			unload();
		}
		else if ( "play" == m_command_queue[i].command ) {
			updatePipelineMediaState(MEDIA_PLAY);
		}
		else if ( "pause" == m_command_queue[i].command ) {
			updatePipelineMediaState(MEDIA_PAUSE);
		}
		// TODO impossible to call setUri before load?
		else if ( "setUri" == m_command_queue[i].command ) {
			setUri(unmarshallstring(state["setUri"]["uri"]), unmarshallstring(state["setUri"]["option"]));
		}
		else if ( "setPlayRate" == m_command_queue[i].command ) {
			setPlayRate(unmarshallfloat(state["setPlayRate"]["playRate"]),unmarshallboolean(state["setPlayRate"]["audioOutput"]));
		}
		else if ( "selectTrack" == m_command_queue[i].command ) {
			selectTrack(unmarshallstring(state["selectTrack"]["type"]),
					unmarshalllong(state["selectTrack"]["index"]));
		}
		// TODO call sequence : FIFO is meaningful for subtitle?
		else if ( "setSubtitleSource" == m_command_queue[i].command ) {
			setSubtitleSource(unmarshallstring(state["setSubtitleSource"]["uri"]), unmarshallstring(state["setSubtitleSource"]["preferredEncodings"]));
		}
		else if ( "setSubtitleEnable" == m_command_queue[i].command ) {
			setSubtitleEnable(unmarshallboolean(state["setSubtitleEnable"]["enable"]));
		}
		else if ( "setSubtitlePosition" == m_command_queue[i].command ) {
			setSubtitlePosition(unmarshalllong(state["setSubtitlePosition"]["position"]));
		}
		else if ( "setSubtitleSync" == m_command_queue[i].command ) {
			setSubtitleSync(unmarshalllong(state["setSubtitleSync"]["sync"]));
		}
		else if ( "setSubtitleFontSize" == m_command_queue[i].command ) {
			setSubtitleFontSize(unmarshalllong(state["setSubtitleFontSize"]["fontSize"]));
		}
		else if ( "setSubtitleColor" == m_command_queue[i].command ) {
			setSubtitleColor(unmarshalllong(state["setSubtitleColor"]["color"]));
		}
		else if ( "setSubtitleEncoding" == m_command_queue[i].command ) {
			setSubtitleEncoding(unmarshallstring(state["setSubtitleEncoding"]["encoding"]));
		}
		else if ( "setSubtitlePresentationMode" == m_command_queue[i].command ) {
			setSubtitlePresentationMode(unmarshallstring(state["setSubtitlePresentationMode"]["presentationMode"]));
		}
		else if ( "setSubtitleCharacterColor" == m_command_queue[i].command ) {
			setSubtitleCharacterColor(unmarshallstring(state["setSubtitleCharacterColor"]["charColor"]));
		}
		else if ( "setSubtitleCharacterOpacity" == m_command_queue[i].command ) {
			setSubtitleCharacterOpacity(unmarshalllong(state["setSubtitleCharacterOpacity"]["charOpacity"]));
		}
		else if ( "setSubtitleCharacterFontSize" == m_command_queue[i].command ) {
			setSubtitleCharacterFontSize(unmarshallstring(state["setSubtitleCharacterFontSize"]["charFontSize"]));
		}
		else if ( "setSubtitleCharacterFont" == m_command_queue[i].command ) {
			setSubtitleCharacterFont(unmarshallstring(state["setSubtitleCharacterFont"]["charFont"]));
		}
		else if ( "setSubtitleBackgroundColor" == m_command_queue[i].command ) {
			setSubtitleBackgroundColor(unmarshallstring(state["setSubtitleBackgroundColor"]["bgColor"]));
		}
		else if ( "setSubtitleBackgroundOpacity" == m_command_queue[i].command ) {
			setSubtitleBackgroundOpacity(unmarshalllong(state["setSubtitleBackgroundOpacity"]["bgOpacity"]));
		}
		else if ( "setSubtitleCharacterEdge" == m_command_queue[i].command ) {
			setSubtitleCharacterEdge(unmarshallstring(state["setSubtitleCharacterEdge"]["charEdgeType"]));
		}
		else if ( "setSubtitleWindowColor" == m_command_queue[i].command ) {
			setSubtitleWindowColor(unmarshallstring(state["setSubtitleWindowColor"]["windowColor"]));
		}
		else if ( "setSubtitleWindowOpacity" == m_command_queue[i].command ) {
			setSubtitleWindowOpacity(unmarshalllong(state["setSubtitleWindowOpacity"]["windowOpacity"]));
		}
		else if ( "setUpdateInterval" == m_command_queue[i].command ) {
			setUpdateInterval(unmarshallstring(state["setUpdateInterval"]["key"]), unmarshalllong(state["setUpdateInterval"]["value"]));
		}
		else if ( "takeCameraSnapshot" == m_command_queue[i].command ) {
			takeCameraSnapshot(unmarshallstring(state["takeCameraSnapshot"]["location"]),
					unmarshallstring(state["takeCameraSnapshot"]["format"]),
					unmarshalllong(state["takeCameraSnapshot"]["width"]),
					unmarshalllong(state["takeCameraSnapshot"]["height"]),
					unmarshalllong(state["takeCameraSnapshot"]["pictureQuality"]));
		}
		else if ( "startCameraRecord" == m_command_queue[i].command ) {
			startCameraRecord(unmarshallstring(state["startCameraRecord"]["location"]),
					unmarshallstring(state["startCameraRecord"]["format"]));
		}
		else if ( "stopCameraRecord" == m_command_queue[i].command ) {
			stopCameraRecord();
		}
		else if ( "changeResolution" == m_command_queue[i].command ) {
			changeResolution(unmarshalllong(state["changeResolution"]["width"]), unmarshalllong(state["changeResolution"]["height"]));
		}
		else if ( "setStreamQuality" == m_command_queue[i].command ) {
			setStreamQuality(unmarshalllong(state["setStreamQuality"]["width"]),
					unmarshalllong(state["setStreamQuality"]["height"]),
					unmarshalllong(state["setStreamQuality"]["bitRate"]),
					unmarshallboolean(state["setStreamQuality"]["init"]));
		}
		else if ( "setProperty" == m_command_queue[i].command ) {
			setProperty(m_command_queue[i].payload);
		}
		else if ( "setDescriptiveVideoService" == m_command_queue[i].command ) {
			setDescriptiveVideoService(unmarshallboolean(state["setDescriptiveVideoService"]["enable"]));
		}
		else if ( "setVolume" == m_command_queue[i].command ) {
			setVolume(unmarshalllong(state["setVolume"]["volume"]),
					unmarshalllong(state["setVolume"]["ease"]["duration"]),
					string_to_ease_type(unmarshallstring(state["setVolume"]["ease"]["type"]).c_str()));
		}
		else if ("setMasterClock" == m_command_queue[i].command ) {
			setMasterClock(unmarshallstring(state["setMasterClock"]["ip"]), unmarshalllong(state["setMasterClock"]["port"]));
		}
		else if ("setSlaveClock" == m_command_queue[i].command ) {
			setSlaveClock(unmarshallstring(state["setSlaveClock"]["ip"]),
					unmarshalllong(state["setSlaveClock"]["port"]),
					unmarshalllonglong(state["setSlaveClock"]["baseTime"]));
		}
		else if ( "setAudioDualMono" == m_command_queue[i].command ) {
			setAudioDualMono(unmarshalllong(state["setAudioDualMono"]["audioMode"]));
		}
	}

	LOG_DEBUG(log, "+++ processing positional state +++");

	m_command_queue.clear();  // command queue holds commands sent before pipeline was ready to process.
	                          // pipeline is now able to process commands real time.

	m_restarting = false;
	long long seek_pos = getSeekPos();
	if (seek_pos != 0)
		seek(seek_pos);

	string mediastate = getMediaState();
	if (mediastate == MEDIA_PLAY) {
		LOG_DEBUG(log, "set state to play");
		play();
	} else if (mediastate == MEDIA_PAUSE) {
		LOG_DEBUG(log, "set state to pause");
		pause();
	} else {
		LOG_DEBUG(log, "media state unknown. state = %s", mediastate.c_str());
	}

	std::string load = id_ + std::string("_load");
	UMSTRACE_AFTER(load.c_str());
	return true;
}

bool Pipeline::processPreloadCompleted()
{
        updatePipelineProcessState(PIPELINE_MEDIA_PRELOADED);

        LOG_DEBUG(log, "+++ processing deferred commands +++");
        JValue state = m_pipeline_json_state.getJsonObj();
        for ( size_t i = 0; i < m_command_queue.size() ; ++i ) {
                LOG_DEBUG(log, "+ command = %s, payload = %s ",
                                m_command_queue[i].command.c_str(),
                                m_command_queue[i].payload.c_str());

                if ( "unload" == m_command_queue[i].command ) {
                        unload();
                }
        }

        return true;
}

// @f pipelineProcessStateEvent
// @b handle all state (ie. subscription) call back events
//
bool Pipeline::pipelineProcessStateEvent(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx)
{
	Pipeline *self = static_cast<Pipeline *>(ctx);
	string json_message = self->pipeline_connector->getMessageText(message);
	LOG_DEBUG(self->log, "pipelineStateEvent callback : %s, key = %s",
			json_message.c_str(), self->id_.c_str());
	int errCode = 0;
	self->updateState(json_message);   // track pipeline state

	pbnjson::JDomParser parser;
	if (parser.parse(json_message,  pbnjson::JSchema::AllSchema())) {
		pbnjson::JValue json = parser.getDom();
		if (json.hasKey("error") && json["error"].hasKey("errorCode"))
			errCode = json["error"]["errorCode"].asNumber<int>();

		if (601 == errCode) {
			return self->suspend();
		}
	}

	// forward subscription message to client
	if (self->m_subscribed && self->client_connector ) {
		LOG_TRACE(self->log,
				"Client subscriber present. Forwarding subscription messages to client \'%s\'",
				self->id_.c_str());

		self->client_connector->sendChangeNotificationJsonString(json_message, self->id_);

		if (104 == errCode)
		{
			self->signal_load_failed(self->id_);
		}
	}
	return true;
}

// @f load
// @b Pipeline(process/client) allocates all resources and routes needed for playback
//
// example command line directly to process
//luna-send -n 1 palm://com.palm.pipeline/load
//
bool Pipeline::load(const std::string & appid, const std::string & uri, const std::string & payload,
					UMSConnector * connector)
{
	uri_ = uri;
	payload_ = payload;
	appid_ = appid;

	// save client connector handle to forward subscription messages from Pipeline(process)
	client_connector = connector;
        m_preloading = false;
	startProcess();
	updatePipelineMediaState(MEDIA_LOAD);
	return true;
}

// @f preload
// @b Pipeline(process/client) allocates all resources and routes needed for playback
//
// example command line directly to process
//luna-send -n 1 palm://com.palm.pipeline/preload
//
bool Pipeline::preload(const std::string & appid, const std::string & uri, const std::string & payload,
                                        UMSConnector * connector)
{
        uri_ = uri;
        payload_ = payload;
        appid_ = appid;

        // save client connector handle to forward subscription messages from Pipeline(process)
        client_connector = connector;
        m_preloading = true;
        startProcess();
        updatePipelineMediaState(MEDIA_PRELOAD);
        return true;
}

// @f unload
// @b Pipeline(process/client) releases all resources and routes acquired during load
//
// example command line directly to process
//luna-send -n 1 palm://com.palm.pipeline/unload
//
bool Pipeline::unload()
{
	updatePipelineMediaState(MEDIA_UNLOAD);

	string process_state = getProcessState();
	if (process_state != PIPELINE_RUNNING &&
			process_state != PIPELINE_SUSPENDED &&
			process_state != PIPELINE_MEDIA_LOADED &&
                        process_state != PIPELINE_MEDIA_PRELOADED) {
		m_command_queue.emplace_back("unload");
		return true;
	}

	string cmd = process_connection_id + "/unload";
	pipeline_connector->sendMessage(cmd, uri_, nullptr, nullptr);

	if (process_handle) {
		signal_pid(appid_, process_handle->pid(), false);
		process_pool.retire(process_handle, process_connection_id);
		process_handle.reset();
	}

	return true;
}

// @f suspend
// @b Prevent restart of pipeline process to yield resources.
//
bool Pipeline::suspend()
{
	// We don't send any commands to the pipeline, since this is only a way to
	// mark the pipeline for non-restart as long as its resources are acquired
	// by another pipeline prcocess. It's up to the pipeline to decide (upon receipt of
	// a policyAction command) whether to exit, or to keep running but give up
	// the resources.
	updatePipelineProcessState(PIPELINE_SUSPENDED);

	if (process_handle) {
		signal_pid(appid_, process_handle->pid(), false);
		process_pool.retire(process_handle, process_connection_id);
		process_handle.reset();
	}

	return true;
}

// @f unsuspend
// @b Makes the pipeline restartable and starts it if necessary
//
bool Pipeline::resume()
{
	if (m_process_starting || process_handle)
		return true;

	// We don't call updatePipelineProcessState since that's done
	// in the readyEvent
	m_restarting = true; // This will force state restoration.

	std::string load = id_ + std::string("_load");
	std::string load_exec = id_ + std::string("_load_exec");
	UMSTRACE_BEFORE(load.c_str());
	UMSTRACE_BEFORE(load_exec.c_str());

	startProcess();

	// TODO: implement resume

	return true;
}

} // namespace uMediaServer

namespace uMediaServer {

// @f play
// @b play Pipeline(process)
//
// example command line directly to process
//luna-send -n 1 palm://com.palm.pipeline/play "/media/internal/bolt.mp4"
//
bool Pipeline::play()
{
	updatePipelineMediaState(MEDIA_PLAY);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		m_command_queue.emplace_back("play");
		return true;
	}

	LOG_TRACE(log, "uri_=%s", uri_.c_str());
	LOG_TRACE(log, "process_connection_id=%s", process_connection_id.c_str());

	string cmd = process_connection_id + "/play";
	pipeline_connector->sendMessage(cmd, uri_, nullptr, nullptr);
	return true;
}

bool Pipeline::pause()
{
	updatePipelineMediaState(MEDIA_PAUSE);
	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		m_command_queue.emplace_back("pause");
		return true;
	}

	LOG_TRACE(log, "uri_=%s", uri_.c_str());
	LOG_TRACE(log, "process_connection_id=%s", process_connection_id.c_str());

	string cmd = process_connection_id + "/pause";
	pipeline_connector->sendMessage(cmd, uri_, nullptr, nullptr);
	return true;
}

bool Pipeline::seek(long long position)
{
	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		setSeekPos(position);
		m_command_queue.emplace_back("seek");
		return true;
	}

	LOG_TRACE(log, "uri_=%s", uri_.c_str());
	LOG_TRACE(log, "process_connection_id=%s", process_connection_id.c_str());

	string seek_pos = boost::lexical_cast<string>(position);
	string cmd = process_connection_id + "/seek";
	LOG_TRACE(log, "Pipeline::seek -> Now sending seek command = %s position = %s",
			cmd.c_str(), seek_pos.c_str());
	if (pipeline_connector->sendMessage(cmd, seek_pos, nullptr, nullptr)) {
		setSeekPos(0); // TODO should really be set in a "seekEvent" handler
		return true;
	}
	else {
		LOG_ERROR(log, MSGERR_PIPELINE_SEND,
				"seek UMSConnector sendMessage has failed, is the pipeline running?");
	}
	return false;

}

// @f stateChange
// @b client subscribes here to receive Pipeline(process) state change events
//
// Function is used by a client who wants to subscribe to have state change
// messages forwarded from a target pipeline.
//
bool Pipeline::stateChange(bool subscribed)
{
	LOG_TRACE(log, "process_connection_id = %s, uri_=%s", uri_.c_str(),
			process_connection_id.c_str());

	m_subscribed = subscribed;
	return true;
}

// @f setUri
// @b set uri
//
bool Pipeline::setUri(const std::string &uri, const std::string& option)
{
	pbnjson::JValue args = pbnjson::Object();

	args.put("uri",marshallstring(uri));

	if ( !option.empty() )
		args.put("option", marshallstring(option));

	JValue stateupdate = Object();
	stateupdate.put("setUri", args);
	m_pipeline_json_state.update(stateupdate);

	uri_ = uri;

	// TODO check if the following is necessary
	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_DEBUG(log, "caching setUri : uri - %s", uri_.c_str());
		m_command_queue.emplace_back("setUri");
		return true;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failed serializer.toString()");
		return false;
	}

	LOG_DEBUG(log, "uri_=%s", uri_.c_str());

	string cmd = process_connection_id + "/setUri";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);

	return true;
}

// @f setPlaykRate
// @b set play back rate
//
//
bool Pipeline::setPlayRate(double rate, bool audioOutput)
{
	JValue args = Object();
	args.put("playRate", marshallfloat(rate));
	args.put("audioOutput", marshallboolean(audioOutput));

	JValue stateupdate = Object();
	stateupdate.put("setPlayRate", args);

	m_pipeline_json_state.update(stateupdate);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_DEBUG(log, "caching playRate = %lf, audioOutput = %d", rate, audioOutput);
		m_command_queue.emplace_back("setPlayRate");
		return true;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failed serializer.toString()");
		return false;
	}

	LOG_DEBUG(log, "rate=%lf, audioOutput=%d", rate, audioOutput);

	string cmd = process_connection_id + "/setPlayRate";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);

	return true;
}

// @f setSlave
// @b set information to Slave device
//
//
bool Pipeline::setSlave(const std::string &ip, int32_t port, const std::string basetime)
{
	JValue args = Object();
	args.put("ip", marshallstring(ip));
	args.put("port", marshalllong(port));
	args.put("basetime", marshallstring(basetime));

	JValue stateupdate = Object();
	stateupdate.put("setSlave", args);

	m_pipeline_json_state.update(stateupdate);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_ERROR(log, "CLOCK_SYNC", "Pipeline is not loaded yet.");
		return false;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failed serializer.toString()");
		return false;
	}

	LOG_DEBUG(log, "ip=%s, port = %d, basetime = %s", ip.c_str(), port, basetime.c_str());

	string cmd = process_connection_id + "/setSlave";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, (void*)this);

	return true;
}

// @f setMaster
// @b set information to Master device
//
//
bool Pipeline::setMaster(const std::string &ip, int32_t port, UMSConnectorEventFunction cb, void *ctx)
{
	JValue args = Object();
	args.put("ip", marshallstring(ip));
	args.put("port", marshalllong(port));

	JValue stateupdate = Object();
	stateupdate.put("setMaster", args);

	m_pipeline_json_state.update(stateupdate);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_ERROR(log, "CLOCK_SYNC", "Pipeline is not loaded yet.");
		return false;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failed serializer.toString()");
		return false;
	}

	LOG_DEBUG(log, "ip=%s, port = %d", ip.c_str(), port);

	string cmd = process_connection_id + "/setMaster";
	pipeline_connector->sendMessage(cmd, payload_serialized, cb, ctx);

	return true;
}

// @f selectTrack
// @b select one of tracks
//
//
bool Pipeline::selectTrack(const string &type, int32_t index)
{
	// validate input
	auto valid_type = [](const string & t)->bool {
		return t == "video" || t == "audio" || t == "text" || t=="externalText";
	};
	auto valid_index = [](int32_t idx)->bool {
		return idx >= 0;
	};
	if (!(valid_type(type) && valid_index(index)))
		return false;

	JValue args = Object();
	args.put("type",marshallstring(type));
	args.put("index",marshalllong(index));

	JValue stateupdate = Object();
	stateupdate.put("selectTrack", args);
	m_pipeline_json_state.update(stateupdate);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_DEBUG(log, "caching track info : type - %s, index - %d", type.c_str(), index);
		m_command_queue.emplace_back("selectTrack");
		return true;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failed serializer.toString()");
		return false;
	}

	LOG_DEBUG(log, "type=%s, index=%d", type.c_str(), index);

	string cmd = process_connection_id + "/selectTrack";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);

	return true;
}

// @f setSubtitleSource
// @b set uri for subtitle
//
//
bool Pipeline::setSubtitleSource(const std::string &uri, std::string preferredEncodings)
{
	pbnjson::JValue args = pbnjson::Object();

	args.put("uri",marshallstring(uri));
	args.put("preferredEncodings",marshallstring(preferredEncodings));

	JValue stateupdate = Object();
	stateupdate.put("setSubtitleSource", args);
	m_pipeline_json_state.update(stateupdate);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_DEBUG(log, "caching subtitle info : uri - %s", uri.c_str());
		m_command_queue.emplace_back("setSubtitleSource");
		return true;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failed serializer.toString()");
		return false;
	}

	LOG_TRACE(log, "uri = %s", uri.c_str());

	string cmd = process_connection_id + "/setSubtitleSource";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);

	return true;
}

// @f setSubtitleEnable
// @b determine to display subtitle
//
//
bool Pipeline::setSubtitleEnable(bool enable)
{
	pbnjson::JValue args = pbnjson::Object();

	args.put("enable",marshallboolean(enable));

	JValue stateupdate = Object();
	stateupdate.put("setSubtitleEnable", args);
	m_pipeline_json_state.update(stateupdate);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_TRACE(log, "caching subtitle info : display_enable - %d", enable);
		m_command_queue.emplace_back("setSubtitleEnable");
		return true;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failed serializer.toString()");
		return false;
	}

	LOG_DEBUG(log, "enable to display subtilte = %d", enable);

	string cmd = process_connection_id + "/setSubtitleEnable";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);

	return true;
}

// @f setSubtitlePosition
// @b set subtitle position
//
//
bool Pipeline::setSubtitlePosition(int32_t position)
{
	pbnjson::JValue args = pbnjson::Object();

	args.put("position",marshalllong(position));

	JValue stateupdate = Object();
	stateupdate.put("setSubtitlePosition", args);
	m_pipeline_json_state.update(stateupdate);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_DEBUG(log, "caching subtitle info : position - %d", position);
		m_command_queue.emplace_back("setSubtitlePosition");
		return true;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failed serializer.toString()");
		return false;
	}

	LOG_DEBUG(log, "set subtitle position = %d", position);

	string cmd = process_connection_id + "/setSubtitlePosition";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);

	return true;
}

// @f setSubtitleSync
// @b set subtitle sync
//
//
bool Pipeline::setSubtitleSync(int32_t sync)
{
	pbnjson::JValue args = pbnjson::Object();

	args.put("sync",marshalllong(sync));

	JValue stateupdate = Object();
	stateupdate.put("setSubtitleSync", args);
	m_pipeline_json_state.update(stateupdate);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_DEBUG(log, "caching subtitle info : sync - %d", sync);
		m_command_queue.emplace_back("setSubtitleSync");
		return true;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failed serializer.toString()");
		return false;
	}

	LOG_DEBUG(log, "set subtitle sync = %d", sync);

	string cmd = process_connection_id + "/setSubtitleSync";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);

	return true;
}

// @f setSubtitleFontSize
// @b set subtitle font size
//
//
bool Pipeline::setSubtitleFontSize(int32_t font_size)
{
	pbnjson::JValue args = pbnjson::Object();

	args.put("fontSize",marshalllong(font_size));

	JValue stateupdate = Object();
	stateupdate.put("setSubtitleFontSize", args);
	m_pipeline_json_state.update(stateupdate);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_DEBUG(log, "caching subtitle info : fontSize - %d", font_size);
		m_command_queue.emplace_back("setSubtitleFontSize");
		return true;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failed serializer.toString()");
		return false;
	}

	LOG_DEBUG(log, "set subtitle font size = %d", font_size);

	string cmd = process_connection_id + "/setSubtitleFontSize";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);

	return true;
}

// @f setSubtitleColor
// @b set subtitle color
//
//
bool Pipeline::setSubtitleColor(int32_t color)
{
	pbnjson::JValue args = pbnjson::Object();

	args.put("color",marshalllong(color));

	JValue stateupdate = Object();
	stateupdate.put("setSubtitleColor", args);
	m_pipeline_json_state.update(stateupdate);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_DEBUG(log, "caching subtitle info : color - %d", color);
		m_command_queue.emplace_back("setSubtitleColor");
		return true;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failed serializer.toString()");
		return false;
	}

	LOG_DEBUG(log, "set subtitle color = %d", color);

	string cmd = process_connection_id + "/setSubtitleColor";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);

	return true;
}

// @f setSubtitleEncoding
// @b set encoding for subtitle
//
//
bool Pipeline::setSubtitleEncoding(const std::string &encoding)
{
	pbnjson::JValue args = pbnjson::Object();

	args.put("encoding",marshallstring(encoding));

	JValue stateupdate = Object();
	stateupdate.put("setSubtitleEncoding", args);
	m_pipeline_json_state.update(stateupdate);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_DEBUG(log, "caching subtitle info : encoding - %s", encoding.c_str());
		m_command_queue.emplace_back("setSubtitleEncoding");
		return true;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failed serializer.toString()");
		return false;
	}

	LOG_TRACE(log, "encoding = %s", encoding.c_str());

	string cmd = process_connection_id + "/setSubtitleEncoding";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);

	return true;
}

// @f setSubtitlePresentationMode
// @b set Subtitle Presentation Mode
//
//
bool Pipeline::setSubtitlePresentationMode(const std::string &presentationMode)
{
	pbnjson::JValue args = pbnjson::Object();

	args.put("presentationMode",marshallstring(presentationMode));

	JValue stateupdate = Object();
	stateupdate.put("setSubtitlePresentationMode", args);
	m_pipeline_json_state.update(stateupdate);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_DEBUG(log, "caching subtitle info : presentationMode - %s", presentationMode.c_str());
		m_command_queue.emplace_back("setSubtitlePresentationMode");
		return true;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failed serializer.toString()");
		return false;
	}

	LOG_TRACE(log, "presentationMode = %s", presentationMode.c_str());

	string cmd = process_connection_id + "/setSubtitlePresentationMode";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);

	return true;
}

// @f setSubtitleCharacterColor
// @b set Subtitle Character Color
//
//
bool Pipeline::setSubtitleCharacterColor(const std::string &charColor)
{
	pbnjson::JValue args = pbnjson::Object();

	args.put("charColor",marshallstring(charColor));

	JValue stateupdate = Object();
	stateupdate.put("setSubtitleCharacterColor", args);
	m_pipeline_json_state.update(stateupdate);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_DEBUG(log, "caching subtitle info : charColor - %s", charColor.c_str());
		m_command_queue.emplace_back("setSubtitleCharacterColor");
		return true;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failed serializer.toString()");
		return false;
	}

	LOG_TRACE(log, "charColor = %s", charColor.c_str());

	string cmd = process_connection_id + "/setSubtitleCharacterColor";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);

	return true;
}

// @f setSubtitleCharacterOpacity
// @b set Subtitle Character Opacity
//
//
bool Pipeline::setSubtitleCharacterOpacity(int32_t charOpacity)
{
	pbnjson::JValue args = pbnjson::Object();

	args.put("charOpacity",marshalllong(charOpacity));

	JValue stateupdate = Object();
	stateupdate.put("setSubtitleCharacterOpacity", args);
	m_pipeline_json_state.update(stateupdate);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_DEBUG(log, "caching subtitle info : charOpacity - %d", charOpacity);
		m_command_queue.emplace_back("setSubtitleCharacterOpacity");
		return true;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failed serializer.toString()");
		return false;
	}

	LOG_TRACE(log, "charOpacity = %d", charOpacity);

	string cmd = process_connection_id + "/setSubtitleCharacterOpacity";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);

	return true;
}

// @f setSubtitleCharacterFontSize
// @b set Subtitle Character FontSize
//
//
bool Pipeline::setSubtitleCharacterFontSize(const std::string &charFontSize)
{
	pbnjson::JValue args = pbnjson::Object();

	args.put("charFontSize",marshallstring(charFontSize));

	JValue stateupdate = Object();
	stateupdate.put("setSubtitleCharacterFontSize", args);
	m_pipeline_json_state.update(stateupdate);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_DEBUG(log, "caching subtitle info : charFontSize - %s", charFontSize.c_str());
		m_command_queue.emplace_back("setSubtitleCharacterFontSize");
		return true;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failed serializer.toString()");
		return false;
	}

	LOG_TRACE(log, "charFontSize = %s", charFontSize.c_str());

	string cmd = process_connection_id + "/setSubtitleCharacterFontSize";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);

	return true;
}

// @f setSubtitleCharacterFont
// @b set Subtitle Character Font
//
//
bool Pipeline::setSubtitleCharacterFont(const std::string &charFont)
{
	pbnjson::JValue args = pbnjson::Object();

	args.put("charFont",marshallstring(charFont));

	JValue stateupdate = Object();
	stateupdate.put("setSubtitleCharacterFont", args);
	m_pipeline_json_state.update(stateupdate);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_DEBUG(log, "caching subtitle info : charFont - %s", charFont.c_str());
		m_command_queue.emplace_back("setSubtitleCharacterFont");
		return true;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failed serializer.toString()");
		return false;
	}

	LOG_TRACE(log, "charFont = %s", charFont.c_str());

	string cmd = process_connection_id + "/setSubtitleCharacterFont";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);

	return true;
}

// @f setSubtitleBackgroundColor
// @b set Subtitle BackgroundColor
//
//
bool Pipeline::setSubtitleBackgroundColor(const std::string &bgColor)
{
	pbnjson::JValue args = pbnjson::Object();

	args.put("bgColor",marshallstring(bgColor));

	JValue stateupdate = Object();
	stateupdate.put("setSubtitleBackgroundColor", args);
	m_pipeline_json_state.update(stateupdate);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_DEBUG(log, "caching subtitle info : bgColor - %s", bgColor.c_str());
		m_command_queue.emplace_back("setSubtitleBackgroundColor");
		return true;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failed serializer.toString()");
		return false;
	}

	LOG_TRACE(log, "bgColor = %s", bgColor.c_str());

	string cmd = process_connection_id + "/setSubtitleBackgroundColor";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);

	return true;
}

// @f setSubtitleBackgroundOpacity
// @b set Subtitle Background Opacity
//
//
bool Pipeline::setSubtitleBackgroundOpacity(int32_t bgOpacity)
{
	pbnjson::JValue args = pbnjson::Object();

	args.put("bgOpacity",marshalllong(bgOpacity));

	JValue stateupdate = Object();
	stateupdate.put("setSubtitleBackgroundOpacity", args);
	m_pipeline_json_state.update(stateupdate);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_DEBUG(log, "caching subtitle info : bgOpacity - %d", bgOpacity);
		m_command_queue.emplace_back("setSubtitleBackgroundOpacity");
		return true;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failed serializer.toString()");
		return false;
	}

	LOG_TRACE(log, "bgOpacity = %d", bgOpacity);

	string cmd = process_connection_id + "/setSubtitleBackgroundOpacity";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);

	return true;
}

// @f setSubtitleCharacterEdge
// @b set Subtitle CharacterEdge
//
//
bool Pipeline::setSubtitleCharacterEdge(const std::string &charEdgeType)
{
	pbnjson::JValue args = pbnjson::Object();

	args.put("charEdgeType",marshallstring(charEdgeType));

	JValue stateupdate = Object();
	stateupdate.put("setSubtitleCharacterEdge", args);
	m_pipeline_json_state.update(stateupdate);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_DEBUG(log, "caching subtitle info : charEdgeType - %s", charEdgeType.c_str());
		m_command_queue.emplace_back("setSubtitleCharacterEdge");
		return true;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failed serializer.toString()");
		return false;
	}

	LOG_TRACE(log, "charEdgeType = %s", charEdgeType.c_str());

	string cmd = process_connection_id + "/setSubtitleCharacterEdge";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);

	return true;
}

// @f setSubtitleWindowColor
// @b set Subtitle WindowColor
//
//
bool Pipeline::setSubtitleWindowColor(const std::string &windowColor)
{
	pbnjson::JValue args = pbnjson::Object();

	args.put("windowColor",marshallstring(windowColor));

	JValue stateupdate = Object();
	stateupdate.put("setSubtitleWindowColor", args);
	m_pipeline_json_state.update(stateupdate);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_DEBUG(log, "caching subtitle info : windowColor - %s", windowColor.c_str());
		m_command_queue.emplace_back("setSubtitleWindowColor");
		return true;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failed serializer.toString()");
		return false;
	}

	LOG_TRACE(log, "windowColor = %s", windowColor.c_str());

	string cmd = process_connection_id + "/setSubtitleWindowColor";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);

	return true;
}

// @f setSubtitleWindowOpacity
// @b set Subtitle Window Opacity
//
//
bool Pipeline::setSubtitleWindowOpacity(int32_t windowOpacity)
{
	pbnjson::JValue args = pbnjson::Object();

	args.put("windowOpacity",marshalllong(windowOpacity));

	JValue stateupdate = Object();
	stateupdate.put("setSubtitleWindowOpacity", args);
	m_pipeline_json_state.update(stateupdate);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_DEBUG(log, "caching subtitle info : windowOpacity - %d", windowOpacity);
		m_command_queue.emplace_back("setSubtitleWindowOpacity");
		return true;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failed serializer.toString()");
		return false;
	}

	LOG_TRACE(log, "windowOpacity = %d", windowOpacity);

	string cmd = process_connection_id + "/setSubtitleWindowOpacity";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);

	return true;
}

// @f setUpdateInterval
// @b set update interval for current time and buffer range
//
//
bool Pipeline::setUpdateInterval(int32_t current_time_interval, int32_t buffer_range_interval)
{
	// validate input
	if (current_time_interval < 0 || buffer_range_interval < 0)
		return false;

	pbnjson::JValue args = pbnjson::Object();

	args.put("currentTimeInterval",marshalllong(current_time_interval));
	args.put("bufferRangeInterval",marshalllong(buffer_range_interval));

	JValue stateupdate = Object();
	stateupdate.put("setUpdateInterval", args);
	m_pipeline_json_state.update(stateupdate);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_DEBUG(log, "caching update interval info : current_time - %d, buffer_range - %d",
				current_time_interval,
				buffer_range_interval);
		m_command_queue.emplace_back("setUpdateInterval");
		return true;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failed serializer.toString()");
		return false;
	}

	LOG_DEBUG(log, "update interval info : current_time - %d, buffer_range - %d",
			current_time_interval,
			buffer_range_interval);

	string cmd = process_connection_id + "/setUpdateInterval";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);

	return true;
}

// @f setUpdateIntervalKV
// @b set update interval for key.
// @arg key value to be updated
// @arg value value to set key
//
bool Pipeline::setUpdateInterval(std::string key, int32_t value)
{
	pbnjson::JValue args = pbnjson::Object();

	args.put("key",marshallstring(key));
	args.put("value",marshalllong(value));

	JValue stateupdate = Object();
	stateupdate.put("setUpdateIntervalKV", args);
	m_pipeline_json_state.update(stateupdate);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_DEBUG(log, "caching update interval info : key - %s, value - %d",
				key.c_str(),
				value);
		m_command_queue.emplace_back("setUpdateInterval");
		return true;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failed serializer.toString()");
		return false;
	}

	LOG_DEBUG(log, "update interval info : key - %s, value - %d",
			key.c_str(),
			value);

	string cmd = process_connection_id + "/setUpdateIntervalKV";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);

	return true;
}

// @f takeCameraSnapshot
// @b take still cut image
//
//
bool Pipeline::takeCameraSnapshot(const std::string &location, const std::string &format, int32_t width, int32_t height, int32_t picture_quality)
{
	pbnjson::JValue args = pbnjson::Object();

	args.put("location",marshallstring(location));
	args.put("format",marshallstring(format));
	args.put("width",marshalllong(width));
	args.put("height",marshalllong(height));
	args.put("pictureQuality",marshalllong(picture_quality));

	JValue stateupdate = Object();
	stateupdate.put("takeCameraSnapshot", args);
	m_pipeline_json_state.update(stateupdate);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_DEBUG(log, "caching takeCameraSnapshot info : location - %s, format - %s, width - %d, height - %d, pq - %d",
				location.c_str(),
				format.c_str(),
				width,
				height,
				picture_quality);
		m_command_queue.emplace_back("takeCameraSnapshot");
		return true;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failed serializer.toString()");
		return false;
	}

	LOG_DEBUG(log, "takeCameraSnapshot info : location - %s, format - %s, width - %d, height - %d, pq - %d",
			location.c_str(),
			format.c_str(),
			width,
			height,
			picture_quality);

	string cmd = process_connection_id + "/takeCameraSnapshot";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);

	return true;
}

// @f startCameraRecord
// @b start to record
//
//
bool Pipeline::startCameraRecord(const std::string &location, const std::string &format)
{
	pbnjson::JValue args = pbnjson::Object();

	args.put("location",marshallstring(location));
	args.put("format",marshallstring(format));

	JValue stateupdate = Object();
	stateupdate.put("startCameraRecord", args);
	m_pipeline_json_state.update(stateupdate);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_DEBUG(log, "caching startCameraRecord info : location - %s, format - %s", location.c_str(), format.c_str());
		m_command_queue.emplace_back("startCameraRecord");
		return true;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE,"failed serializer.toString()");
		return false;
	}

	LOG_DEBUG(log, "startCameraRecord info : location - %s, format - %s", location.c_str(), format.c_str());

	string cmd = process_connection_id + "/startCameraRecord";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);

	return true;
}

// @f stopCameraRecord
// @b stop recording
//
//
bool Pipeline::stopCameraRecord()
{
	pbnjson::JValue args = pbnjson::Object();

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_DEBUG(log, "caching stopCameraRecord");
		m_command_queue.emplace_back("stopCameraRecord");
		return true;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failed serializer.toString()");
		return false;
	}

	LOG_DEBUG(log, "stopCameraRecord");

	string cmd = process_connection_id + "/stopCameraRecord";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);
	return true;
}

// @f changeResolution
// @b change resolution of input source
//
//
bool Pipeline::changeResolution(int32_t width, int32_t height)
{
	pbnjson::JValue args = pbnjson::Object();

	args.put("width",marshalllong(width));
	args.put("height",marshalllong(height));

	JValue stateupdate = Object();
	stateupdate.put("changeResolution", args);
	m_pipeline_json_state.update(stateupdate);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_DEBUG(log, "caching changeResolution info : width - %d, height - %d",
				width,
				height);
		m_command_queue.emplace_back("changeResolution");
		return true;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE,"failed serializer.toString()");
		return false;
	}

	LOG_DEBUG(log, "changeResolution info : width - %d, height - %d", width, height);

	string cmd = process_connection_id + "/changeResolution";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);

	return true;
}

// @f setStreamQuality
// @b set stream quality among several streams options for adaptive streaming cases
//
//
bool Pipeline::setStreamQuality(int32_t width, int32_t height, int32_t bitRate, bool init)
{
	pbnjson::JValue args = pbnjson::Object();

	args.put("width",marshalllong(width));
	args.put("height",marshalllong(height));
	args.put("bitRate",marshalllong(bitRate));
	args.put("init",marshallboolean(init));

	JValue stateupdate = Object();
	stateupdate.put("setStreamQuality", args);
	m_pipeline_json_state.update(stateupdate);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_DEBUG(log, "caching setStreamQuality info : width - %d, height - %d, bitRate - %d, init - %d",
				width,
				height,
				bitRate,
				init);
		m_command_queue.emplace_back("setStreamQuality");
		return true;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE,"failed serializer.toString()");
		return false;
	}

	LOG_DEBUG(log, "setStreamQuality info : width - %d, height - %d, bitRate - %d, init - %d", width, height, bitRate, init);

	string cmd = process_connection_id + "/setStreamQuality";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);

	return true;
}

bool Pipeline::setProperty(const string &payload)
{
	JValue args = Object();
	args.put("payload",marshallstring(payload));

	JValue stateupdate = Object();
	stateupdate.put("setProperty", args);
	m_pipeline_json_state.update(stateupdate);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_DEBUG(log, "caching property : payload - %s", payload.c_str());
		m_command_queue.emplace_back("setProperty", payload);
		return true;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failed serializer.toString()");
		return false;
	}

	LOG_DEBUG(log, "payload=%s", payload.c_str());

	string cmd = process_connection_id + "/setProperty";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);

	return true;
}

bool Pipeline::setDescriptiveVideoService(bool enable)
{
	JValue args = Object();

	args.put("enable",marshallboolean(enable));

	JValue stateupdate = Object();
	stateupdate.put("setDescriptiveVideoService", args);
	m_pipeline_json_state.update(stateupdate);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_TRACE(log, "caching Descriptive Video Service info : enable - %d", enable);
		m_command_queue.emplace_back("setDescriptiveVideoService");
		return true;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failed serializer.toString()");
		return false;
	}

	LOG_DEBUG(log, "Descriptive Video Service = %d", enable);

	string cmd = process_connection_id + "/setDescriptiveVideoService";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);

	return true;
}

bool Pipeline::setVolume(int32_t volume, int32_t duration, EaseType type)
{
	JValue args = Object();

	args.put("volume", marshalllong(volume));

	JValue ease = Object();
	ease.put("duration", marshalllong(duration));
	ease.put("type", marshallstring(ease_type_to_string(type)));
	args.put("ease", ease);

	JValue stateupdate = Object();
	stateupdate.put("setVolume", args);
	m_pipeline_json_state.update(stateupdate);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_TRACE(log, "caching volume : volume - %d, ease duration - %d and type - %s",
				volume, duration, ease_type_to_string(type));
		m_command_queue.emplace_back("setVolume");
		return true;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failed serializer.toString()");
		return false;
	}

	LOG_DEBUG(log, "volume = %d, ease duration = %d and type = %s", volume, duration, ease_type_to_string(type));

	string cmd = process_connection_id + "/setVolume";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);

	return true;
}

} // namespace uMediaServer

namespace uMediaServer {

bool Pipeline::setMasterClock(const std::string& ip, int32_t port)
{
	pbnjson::JValue args = pbnjson::Object();

	args.put("ip",marshallstring(ip));
	args.put("port",marshalllong(port));

	JValue stateupdate = Object();
	stateupdate.put("setMasterClock", args);
	m_pipeline_json_state.update(stateupdate);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_DEBUG(log, "caching master clock : ip - %s, port - %d",
				ip.c_str(),
				port);
		m_command_queue.emplace_back("setMasterCock");
		return true;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE,"failed serializer.toString()");
		return false;
	}

	LOG_DEBUG(log, "masterClock info : ip - %s, port - %d", ip.c_str(), port);

	string cmd = process_connection_id + "/setMasterClock";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);

	return true;
}

bool Pipeline::setSlaveClock(const std::string& ip, int32_t port, int64_t baseTime)
{
	pbnjson::JValue args = pbnjson::Object();

	args.put("ip",marshallstring(ip));
	args.put("port",marshalllong(port));
	args.put("baseTime",marshalllonglong(baseTime));

	JValue stateupdate = Object();
	stateupdate.put("setSlaveClock", args);
	m_pipeline_json_state.update(stateupdate);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_DEBUG(log, "caching slave clock : ip - %s, port - %d, baseTime - %lld",
				ip.c_str(),
				port,
				baseTime);
		m_command_queue.emplace_back("setSlaveClock");
		return true;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE,"failed serializer.toString()");
		return false;
	}

	LOG_DEBUG(log, "slaveClock info : ip - %s, port - %d, baseTime - %lld", ip.c_str(), port, baseTime);

	string cmd = process_connection_id + "/setSlaveClock";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);

	return true;
}

bool Pipeline::setAudioDualMono(int32_t audioMode)
{
	pbnjson::JValue args = pbnjson::Object();

	args.put("audioMode",marshalllong(audioMode));

	JValue stateupdate = Object();
	stateupdate.put("setAudioDualMono", args);
	m_pipeline_json_state.update(stateupdate);

	if (getProcessState() != PIPELINE_MEDIA_LOADED) {
		LOG_DEBUG(log, "caching audio mode : audioMode - %d", audioMode);
		m_command_queue.emplace_back("setAudioDualMono");
		return true;
	}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE,"failed serializer.toString()");
		return false;
	}

	LOG_DEBUG(log, "audio mode info : audioMode - %d", audioMode);

	string cmd = process_connection_id + "/setAudioDualMono";
	pipeline_connector->sendMessage(cmd, payload_serialized, nullptr, nullptr);

	return true;
}

bool Pipeline::setPlane(int32_t plane_id) {
	pbnjson::JValue args = pbnjson::JObject {{"planeID", plane_id}};
	std::string message;

	if (pbnjson::JGenerator(nullptr).toString(args, pbnjson::JSchema::AllSchema(), message)) {
		std::string cmd = process_connection_id + "/setPlane";
		pipeline_connector->sendMessage(cmd, message, nullptr, nullptr);
		return true;
	} else {
		return false;
	}
}

string Pipeline::getPipelineState()
{
	return m_pipeline_json_state.getJsonString();
}

void Pipeline::logPipelineState()
{
	m_pipeline_json_state.printState();
}

void Pipeline::setPipelineDebugState(const string& state)
{
	LOG_DEBUG(log, "debug_state=%s", state.c_str());
	string cmd = process_connection_id + "/setPipelineDebugState";
	pipeline_connector->sendMessage(cmd, state, NULL,NULL);
	return;
}

bool Pipeline::PipelineState::update(JValue update_arg) {
	lock_guard<mutex> lock(state_mutex);
	JValue tempstate = m_state_object.duplicate();
	updateFields(update_arg, tempstate);

	// Since pbnjson doesn't have a validate function and it doesn't valideate
	// against schema when serializing, we need to first serialize and then
	// parse in order to validate. See GF-2746 and GF-2836
	JGenerator serializer;
	JDomParser parser;
	string statestring;

	auto registry = Reg::Registry::instance();
	pipeline_cfg_t config;
	// TODO: error handling
	registry->get("pipelines", m_pipeline->type_, config);
	const auto & schema = getSchema(config.schema_file);

	if (serializer.toString(tempstate, schema, statestring)) {
		if (parser.parse(statestring, schema)) {
			// All good
			m_state_object = tempstate;
			LOG_TRACE(log, "Json state: %s", statestring.c_str());
		}
		else {
			LOG_ERROR(log, MSGERR_JSON_SCHEMA,
					"Validation against schema failed. State not updated");
			return false;
		}
	}
	else {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "Failed to stringify");
		return false;
	}
	return true;
}

void Pipeline::PipelineState::updateFields(const JValue & update_arg, JValue state_arg) {
	for (auto iterator = update_arg.begin(); iterator != update_arg.end(); ++iterator) {
		if ((*iterator).second.isObject()) {
			if (state_arg.hasKey((*iterator).first.asString())) {
				updateFields((*iterator).second, state_arg[(*iterator).first.asString()]);
			}
			else {
				state_arg.put((*iterator).first, (*iterator).second);
			}
		}
		// Arrays can only be "over-write"
		else {
			state_arg.put((*iterator).first, (*iterator).second);
		}
	}
}

string Pipeline::PipelineState::getJsonString() {
	// TODO: handle error
	auto registry = Reg::Registry::instance();
	pipeline_cfg_t config;
	registry->get("pipelines", m_pipeline->type_, config);
	const auto & schema = getSchema(config.schema_file);

	JGenerator serializer;
	string statestring;
	if (!serializer.toString(m_state_object, schema, statestring)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE,
				"Failed to generate json state string, returning empty object");
		statestring = "{}";
	}
	return statestring;
}

const pbnjson::JSchema & Pipeline::getSchema(const std::string & schema_path) {
	auto & schema_ptr = Pipeline::api_schema[schema_path];
	if (!schema_ptr) {
		try {
			schema_ptr.reset(new pbnjson::JSchemaFile(schema_path));
			if (!schema_ptr->isInitialized())
				throw std::runtime_error("corrupted schema file");
		} catch (...) {
			schema_ptr.reset(new pbnjson::JSchema(pbnjson::JSchema::AllSchema()));
		}
	}
	return *schema_ptr;
}

void Pipeline::PipelineState::printState() {
	LOG_DEBUG(log, " pipeLineState : {");
	printFields(m_state_object);
	LOG_DEBUG(log, " }");
}

void Pipeline::PipelineState::printFields(const JValue & object) {
	string comma;
	m_no_of_tabs++;
	m_indentation.resize(m_no_of_tabs*4, ' ');
	if(object.isArray()) {
		int array_elems = object.arraySize();
		for (int i  = 0; i < array_elems; i++) {
			LOG_DEBUG(log, "%s{", m_indentation.c_str());
			printFields(object[i]);
			comma = ((i + 1) != array_elems) ? "," : "";
			LOG_DEBUG(log, "%s}%s", m_indentation.c_str(), comma.c_str());
		}
	}
	else if (object.isObject()) {
		string openbracket, closebracket;
		bool subfields;
		for (auto iterator = object.begin(); iterator != object.end(); iterator++) {
			if ((*iterator).second.isObject()) {
				openbracket = "{";
				closebracket = "}";
				subfields = true;
			}
			else if((*iterator).second.isArray()) {
				openbracket = "[";
				closebracket = "]";
				subfields = true;
			}
			else {
				subfields = false;
			}

			if (subfields) {
				LOG_DEBUG(log, "%s\"%s\" : %s", m_indentation.c_str(), (*iterator).first.asString().c_str(), openbracket.c_str());
				printFields((*iterator).second);
				comma = ((iterator + 1) != object.end()) ? "," : "";
				LOG_DEBUG(log, "%s%s%s", m_indentation.c_str(), closebracket.c_str(), comma.c_str());

			}
			else {
				stringstream ss;
				if ((*iterator).second.isBoolean())
					ss << (*iterator).second.asBool() ? "true" : "false";
				else if ((*iterator).second.isNumber())
					ss << (*iterator).second.asNumber<double>();
				else
					ss << "\"" << (*iterator).second.asString() << "\"";
				comma = ((iterator + 1) != object.end()) ? "," : "";
				LOG_DEBUG(log, "%s\"%s\" : %s%s", m_indentation.c_str(), (*iterator).first.asString().c_str(), ss.str().c_str(), comma.c_str());
			}
		}
	}
	else {
		stringstream ss;
		if (object.isBoolean()) {
			ss << object.asBool() ? "true" : "false";
		}
		else if (object.isNumber()) {
			ss << object.asNumber<double>();
		}
		else {
			ss << "\"" << object.asString().c_str() << "\"";
		}
		LOG_DEBUG(log, "%s%s", m_indentation.c_str(), ss.str().c_str());
	}
	m_no_of_tabs--;
	m_indentation.resize(m_no_of_tabs*4, ' ');
}

} // namespace uMediaServer
