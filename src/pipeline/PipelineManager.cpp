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
//

#include <PipelineManager.h>
#include <Logger_macro.h>
#include <pbnjson.hpp>
#include <boost/fusion/adapted.hpp>
#include <Registry.h>
#include <list>
#include <string>

using namespace std;
using namespace pbnjson;
using namespace uMediaServer::Reg;
using namespace uMediaServer::DBI;

namespace uMediaServer {

PipelineManager::PipelineManager(IServiceReadyWatcher & watcher)
	: log(UMS_LOG_CONTEXT_PIPELINE_MANAGER)
	, process_pool(watcher, [this](const std::string & id){ processExited(id); })
	, default_debug_state("")
{}

PipelineManager::~PipelineManager()
{
	// each Pipelin(controller) will request associated Pipeline(process) to exit
	LOG_DEBUG(log, "Stopping all pipelines:");

	for (auto& it : pipelines) {
		try {
		  unload(it.first);
		} catch(const boost::bad_function_call &e) {
			LOG_ERROR(log, MSGERR_SERVICE_NOTREG, "%s", e.what());
		}
	}
}

// @f piplinePidUpdate
// @brief update callback function for Pipeline(controller) pid changes
//
//
void PipelineManager::pipelinePidUpdate(const string &appid, pid_t pid, bool exec) {
	pipeline_pid_update(appid, pid, exec);
}

void PipelineManager::pipelineLoadFailurehandler(const std::string& client_connection_id) {
	pipeline_load_failed(client_connection_id);
}

void PipelineManager::pipelineForegroundStatusUpdatehandler() {
	fg_pipeline_status_update();
}



// @f load
// @brief create new pipeline and load media
//
//
bool PipelineManager::load(std::string &pipeline_id, const std::string &type, const std::string &uri,
						   const std::string &payload, const std::string &appId,
						   UMSConnector *client_connector,bool isPreload) {
	Pipeline::ptr_t p = nullptr;

        if (!isPreload) {
          p = findPipeline(pipeline_id);
          if (p != nullptr &&p->getProcessState() == PIPELINE_MEDIA_PRELOADED) {
            LOG_DEBUG(log, "PRELOADED. connectionId. %s", pipeline_id.c_str());
            return p->load(appId, uri, payload, client_connector);
          }
        }

        if (p == nullptr) {
          LOG_DEBUG(log, "NO POOLED PIPELINE FOUND: creating new pipeline process.");
          p = std::unique_ptr<Pipeline>(new Pipeline(type, process_pool));
        }

	// connect to Pipeline Contoller PID events to track suspend/resume PID changes
	p->signal_pid.connect(bind(&PipelineManager::pipelinePidUpdate,
			this, placeholders::_1, placeholders::_2, placeholders::_3));

	p->signal_load_failed.connect(bind(&PipelineManager::pipelineLoadFailurehandler,
			this, placeholders::_1));

	p->signal_pipeline_status_changed.connect(bind(&PipelineManager::pipelineForegroundStatusUpdatehandler, this));

	p->stateChange(true); // connect client to Pipeline Controller subscription events

	pipeline_id = p->getID();
	pipelines[pipeline_id] = p;

	if (!default_debug_state.empty())
		setPipelineDebugState(pipeline_id, default_debug_state);

        if (isPreload) {
	  return p->preload(appId, uri, payload, client_connector);
        }
        return p->load(appId, uri, payload, client_connector);
}

// @f unload
// @brief pipeline move to unloaded state
//
//
bool PipelineManager::unload(const string &client_connection_id)
{
	auto pit = pipelines.find(client_connection_id);
	if (pit != pipelines.end()) {
		// notify pipeline removal for suspended pipeline
		try {
			if (0 == pit->second->getPID())
				pipeline_removed(client_connection_id);
		} catch(const boost::bad_function_call &e) {
			LOG_ERROR(log, MSGERR_NO_CONN_ID, "%s", e.what());
		}
		try {
			pit->second->unload();
		} catch(const boost::bad_any_cast &e) {
			LOG_ERROR(log, MSGERR_PIPELINE_FIND, "%s", e.what());
		}
		pipelines.erase(pit);
		return true;
	}

	return false;
}

// @f suspend
// @brief prevent restart of pipeline to yield resources
//
//
bool PipelineManager::suspend(const string &client_connection_id)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->suspend();
}

// @f resume
// @brief resumes suspended pipeline
//
//
bool PipelineManager::resume(const std::string &client_connection_id)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->resume();
}

} // namespace uMediaServer

// pipeline commands
namespace uMediaServer {

// @f play
// @brief set pipeline state to play
//
//
bool PipelineManager::play(const string &client_connection_id)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
         LOG_DEBUG(log, "%s pipeline is null", __FUNCTION__);
		return false;
	}

	p->resume();
	return p->play();
}

// @f pause
// @brief initiate subscription forwarding from the associated pipeline
//
//
bool PipelineManager::stateChange(const string &client_connection_id, bool subscribed)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		LOG_DEBUG(log, "processing stateChange request : '%s' Pipeline not ready,"
				"stateChange subscription will be set when pipeline is created",
				client_connection_id.c_str());
		return false;
	}

	return p->stateChange(subscribed);
}

// @f stop
// @brief set pipeline state to stop
//
//
bool PipelineManager::stop(const string &client_connection_id)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->play();
}

// @f pause
// @brief set pipeline state to pause
//
//
bool PipelineManager::pause(const string &client_connection_id)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->pause();
}

bool PipelineManager::seek(const string &client_connection_id,long long position)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->seek(position);
}

// @f selectTrack
// @brief select Track
//
bool PipelineManager::setUri(const std::string &client_connection_id, const std::string &uri, const std::string& option)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->setUri(uri, option);
}

// @f setPlayRate
// @brief set play back rate
//
bool PipelineManager::setPlayRate(const string &client_connection_id, double rate, bool audioOutput)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == NULL ) {
		return false;
	}

	return p->setPlayRate(rate, audioOutput);
}

// @f setSlave
// @brief set information to Slave device
//
bool PipelineManager::setSlave(const string &client_connection_id, const string &ip, int port, const string &basetime)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->setSlave(ip, port, basetime);
}

// @f setMaster
// @brief set information to Master device
//
bool PipelineManager::setMaster(const string &client_connection_id, const string &ip, int port, UMSConnectorEventFunction cb, void *ctx)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->setMaster(ip, port, cb, ctx);
}

bool PipelineManager::setPlane(const std::string &client_connection_id, int32_t plane_id) {
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->setPlane(plane_id);
}

// @f selectTrack
// @brief select Track
//
bool PipelineManager::selectTrack(const string &client_connection_id, const string &type, int32_t index)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->selectTrack(type,index);
}

// @f setSubtitleSoruce
// @brief set media streams subtitle source
//
bool PipelineManager::setSubtitleSource(const std::string &client_connection_id, const std::string &uri, std::string preferredEncodings)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->setSubtitleSource(uri, preferredEncodings);
}

// @f setSubtitleEnable
// @brief enable or disable subtitles
//
bool PipelineManager::setSubtitleEnable(const std::string &client_connection_id, bool enable)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->setSubtitleEnable(enable);
}

// @f setSubtitlePosition
// @brief position of subtitle text
//
bool PipelineManager::setSubtitlePosition(const std::string &client_connection_id, int32_t position)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->setSubtitlePosition(position);
}

// @f setSubtitleSync
// @brief adjust synchronization between video and subtitles
//
bool PipelineManager::setSubtitleSync(const std::string &client_connection_id, int32_t sync)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->setSubtitleSync(sync);
}

// @f setSubtitleFontSize
// @brief set subtitle font size
//
bool PipelineManager::setSubtitleFontSize(const std::string &client_connection_id, int32_t font_size)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->setSubtitleFontSize(font_size);
}

// @f setSubtitleColor
// @brief set subtitle color
//
bool PipelineManager::setSubtitleColor(const std::string &client_connection_id, int32_t color)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->setSubtitleColor(color);
}

// @f setSubtitleEncoding
// @brief set media streams subtitle encoding
//
bool PipelineManager::setSubtitleEncoding(const std::string &client_connection_id, const std::string &encoding)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->setSubtitleEncoding(encoding);
}

// @f setSubtitlePresentationMode
// @brief set Subtitle Presentation Mode
//
bool PipelineManager::setSubtitlePresentationMode(const std::string &client_connection_id, const std::string &presentationMode)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->setSubtitlePresentationMode(presentationMode);
}

// @f setSubtitleCharacterColor
// @brief set subtitle CharacterColor
//
bool PipelineManager::setSubtitleCharacterColor(const std::string &client_connection_id, const std::string &charColor)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->setSubtitleCharacterColor(charColor);
}

// @f setSubtitleCharacterOpacity
// @brief set subtitle Character Opacity
//
bool PipelineManager::setSubtitleCharacterOpacity(const std::string &client_connection_id, int32_t charOpacity)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->setSubtitleCharacterOpacity(charOpacity);
}

// @f setSubtitleCharacterFontSize
// @brief set Subtitle Character FontSize
//
bool PipelineManager::setSubtitleCharacterFontSize(const std::string &client_connection_id, const std::string &charFontSize)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->setSubtitleCharacterFontSize(charFontSize);
}

// @f setSubtitleCharacterFont
// @brief set Subtitle Character Font
//
bool PipelineManager::setSubtitleCharacterFont(const std::string &client_connection_id, const std::string &charFont)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->setSubtitleCharacterFont(charFont);
}

// @f setSubtitleBackgroundColor
// @brief set Subtitle BackgroundColor
//
bool PipelineManager::setSubtitleBackgroundColor(const std::string &client_connection_id, const std::string &bgColor)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->setSubtitleBackgroundColor(bgColor);
}

// @f setSubtitleBackgroundOpacity
// @brief set Subtitle Background Opacity
//
bool PipelineManager::setSubtitleBackgroundOpacity(const std::string &client_connection_id, int32_t bgOpacity)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->setSubtitleBackgroundOpacity(bgOpacity);
}

// @f setSubtitleCharacterEdge
// @brief set Subtitle CharacterEdge
//
bool PipelineManager::setSubtitleCharacterEdge(const std::string &client_connection_id, const std::string &charEdgeType)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->setSubtitleCharacterEdge(charEdgeType);
}

// @f setSubtitleWindowColor
// @brief set Subtitle WindowColor
//
bool PipelineManager::setSubtitleWindowColor(const std::string &client_connection_id, const std::string &windowColor)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->setSubtitleWindowColor(windowColor);
}

// @f setSubtitleWindowOpacity
// @brief set Subtitle WindowOpacity
//
bool PipelineManager::setSubtitleWindowOpacity(const std::string &client_connection_id, int32_t windowOpacity)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->setSubtitleWindowOpacity(windowOpacity);
}

// @f setUpdateInterval
// @brief set update interval of currentTime message from media pipeline
//
bool PipelineManager::setUpdateInterval(const std::string &client_connection_id,
		int32_t current_time_interval, int32_t buffer_range_interval)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr  ) {
		return false;
	}

	return p->setUpdateInterval(current_time_interval,buffer_range_interval);
}

// @f setUpdateInterval
// @brief set update interval of currentTime message from media pipeline
//
bool PipelineManager::setUpdateInterval(const std::string &client_connection_id,
		std::string key, int32_t value)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->setUpdateInterval(key,value);
}

// @f takeCameraSnapshot
// @brief take still cut image
//
bool PipelineManager::takeCameraSnapshot(const std::string &client_connection_id,
		const std::string &location, const std::string &format,
		int32_t width, int32_t height, int32_t pictureQuality)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->takeCameraSnapshot(location, format, width, height, pictureQuality);
}

// @f startCameraRecord
// @brief start to record
//
bool PipelineManager::startCameraRecord(const std::string &client_connection_id,
		const std::string &location, const std::string &format, bool audio,
        const std::string &audioSrc)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->startCameraRecord(location, format, audio, audioSrc);
}

// @f stopCameraRecord
// @brief stop recording
//
bool PipelineManager::stopCameraRecord(const std::string &client_connection_id)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->stopCameraRecord();
}

// @f changeResolution
// @brief change resoulution of input source
//
bool PipelineManager::changeResolution(const std::string &client_connection_id, int32_t width, int32_t height)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->changeResolution(width, height);
}

// @f setStreamQuality
// @brief set stream quality among several streams options for adaptive streaming cases
//
bool PipelineManager::setStreamQuality(const std::string &client_connection_id, int32_t width, int32_t height, int32_t bitRate, bool init)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->setStreamQuality(width, height, bitRate, init);
}

// @f setProperty
// @brief set Property
//
bool PipelineManager::setProperty(const string &client_connection_id, const string &payload)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->setProperty(payload);
}

// @f setDescriptiveVideoService
// @brief set Descriptive Video Service
//
bool PipelineManager::setDescriptiveVideoService(const string &client_connection_id, bool enable)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->setDescriptiveVideoService(enable);
}

// @f setVolume
// @brief set volume to control input gain
//
bool PipelineManager::setVolume(const string& client_connection_id, int32_t volume, int32_t duration, EaseType type)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->setVolume(volume, duration, type);
}

} // namespace uMediaServer

namespace uMediaServer {

// @f setMasterClock
// @brief set master clock to master pipeline
//
bool PipelineManager::setMasterClock(const std::string& client_connection_id, const std::string& ip, int32_t port)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->setMasterClock(ip, port);
}

// @f setSlaveClock
// @brief set master clock to slave pipeline for sync
//
bool PipelineManager::setSlaveClock(const std::string& client_connection_id, const std::string& ip, int32_t port, int64_t baseTime)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->setSlaveClock(ip, port, baseTime);
}

// @f setAudioDualMono
// @brief set audio mode(0: L+R, 1: L+L, 2: R+R, 3: MIX)
//
bool PipelineManager::setAudioDualMono(const std::string& client_connection_id, int32_t audioMode)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	return p->setAudioDualMono(audioMode);
}

// @f getPipelineState
// @brief get tracked Pipeline state
//
bool PipelineManager::getPipelineState(const string &client_connection_id,
		string &state)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	state = p->getPipelineState();
	return true;
}

// @f logPipelineState
// @brief dump tracked Pipeline state to the log
//
bool PipelineManager::logPipelineState(const string &client_connection_id)
{
	Pipeline::ptr_t p = findPipeline(client_connection_id);
	if( p == nullptr ) {
		return false;
	}

	p->logPipelineState();
	return true;
}

bool PipelineManager::getActivePipeline(const std::string &client_connection_id, std::string &active_pipeline_out)
{
	auto it = pipelines.find(client_connection_id);
	if (it == pipelines.end()) {
		return false;
	}

	JValue active_pipeline = Object();
	active_pipeline.put("mediaId", it->second->getID().c_str());
	active_pipeline.put("uri", it->second->getURI().c_str());
	active_pipeline.put("pid", it->second->getPID());
	active_pipeline.put("processState", it->second->getProcessState().c_str());
	active_pipeline.put("mediaState",it->second->getMediaState().c_str());
	active_pipeline.put("appId", it->second->getAppId().c_str());

	JGenerator serializer(NULL);

	if (!serializer.toString(active_pipeline,  pbnjson::JSchema::AllSchema(), active_pipeline_out)) {
		LOG_ERROR(log, MSGERR_JSON_PARSE, "ERROR JDomParser.parse.");
		return false;
	}

	return true;
}

bool PipelineManager::getActivePipeline(const std::string &client_connection_id, pbnjson::JValue &active_pipeline_out, bool show_app_id)
{
	auto it = pipelines.find(client_connection_id);
	if (it == pipelines.end()) {
		return false;
	}
	//pbnjson::JValue active_pipeline = pbnjson::Object();
	active_pipeline_out.put("mediaId", it->second->getID().c_str());
	active_pipeline_out.put("uri", it->second->getURI().c_str());
	active_pipeline_out.put("pid", it->second->getPID());
	active_pipeline_out.put("processState", it->second->getProcessState().c_str());
	active_pipeline_out.put("mediaState",it->second->getMediaState().c_str());
	if (show_app_id)
		active_pipeline_out.put("appId", it->second->getAppId().c_str());
	return true;
}


// @f getActivePipelines
// @b get list of active pipeline associated with application ID
//
std::set<std::string> PipelineManager::getActivePipelines(const std::string &app_id)
{
	std::set<std::string> active_pipelines;

	for (auto &i : pipelines) {
		if( app_id == i.second->getAppId() ) {
			active_pipelines.insert(i.second->getID());
		}
	}

	return active_pipelines;
}

bool PipelineManager::getActivePipelines(JValue &active_pipelines_out, bool app_tracking)
{
	if( pipelines.empty()) {
		LOG_DEBUG(log, "pipelines map: EMPTY");
		return false;
	}

	JValue active_pipelines = pbnjson::Array();

	for (auto& i : pipelines) {
		//Memory Manager wants to know just real processes, not suspended
		if (app_tracking && i.second->getProcessState() == PIPELINE_SUSPENDED)
			continue;

		JValue pipeline = Object();
		pipeline.put("pid",i.second->getPID());
		pipeline.put("appId", i.second->getAppId().c_str());
		if (!app_tracking) {
			pipeline.put("mediaId",i.second->getID().c_str());
			pipeline.put("uri",i.second->getURI().c_str());
			pipeline.put("processState",i.second->getProcessState().c_str());
			pipeline.put("mediaState",i.second->getMediaState().c_str());
		}
		active_pipelines.append(pipeline);
	}

	active_pipelines_out = active_pipelines;

	return true;
}

bool PipelineManager::getActivePipelines(pipeline_map_t &pipelines_out)
{
	if( pipelines.empty()) {
		LOG_DEBUG(log, "pipelines map: EMPTY");
		return false;
	}

	pipelines_out = pipelines;
	return true;
}

int PipelineManager::getActivePipelineCount()
{
	return pipelines.size();
}

bool PipelineManager::setPipelineDebugState(const string &client_connection_id,
		const string &state)
{
	if (client_connection_id.empty()) {
		default_debug_state = state;
		LOG_DEBUG(log, "default_debug_state : %s", default_debug_state.c_str());
	}
	else {
		Pipeline::ptr_t p = findPipeline(client_connection_id);
		if( p == NULL ) {
			return false;
		}

		LOG_DEBUG(log, "id : %s, debug_state : %s", client_connection_id.c_str(), state.c_str());
		p->setPipelineDebugState(state);
	}

	return true;
}

std::string PipelineManager::getPipelineServiceName(const std::string &client_connection_id) {
	auto p = findPipeline(client_connection_id);
	if (p == nullptr)
		return "";

	return p->getProcessConnectionID();
}

Pipeline::ptr_t PipelineManager::findPipeline(const string &name)
{
	auto it = pipelines.find(name);
	if(it != pipelines.end()) {
		return it->second;
	}
	LOG_ERROR_EX(log, MSGERR_PIPELINE_FIND, __KV({{KVP_PIPELINE_ID, name.c_str()}}),
			"'%s' Pipeline NOT FOUND", name.c_str());
	return NULL;
}

void PipelineManager::processExited(const std::string & id) {
	auto pipeline = findPipeline(id);
	if (pipeline) {
		pipeline_exited(id);
		pipeline->suspend();
	} else {
		pipeline_removed(id);
	}
}

void PipelineManager::setLogLevelPipeline(const string &client_connection_id, const string& level)
{
	Pipeline::ptr_t p  = findPipeline(client_connection_id);
	if( p ) {
		// TODO set associated Pipeline(process) debug level by sending message
		p->setLogLevel(level);
	}
	return;
}

} // namespace uMediaServer
