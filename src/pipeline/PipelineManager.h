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

#ifndef _PIPELINE_MGR_H
#define _PIPELINE_MGR_H

#include <iostream>
#include <stdexcept>
#include <vector>
#include <sstream>
#include <stdio.h>
#include <cassert>
#include <unordered_set>
#include <mutex>
#include <set>
#include <Logger.h>
#include <Pipeline.h>
#include <boost/noncopyable.hpp>
#include <boost/lexical_cast.hpp>  // for lexical_cast<string>(number)
#include <dbi.h>
#include <pbnjson.hpp>

namespace uMediaServer {

typedef std::map<std::string, Pipeline::ptr_t> pipeline_map_t;

class PipelineManager : private boost::noncopyable
{
public:
	PipelineManager(IServiceReadyWatcher & watcher);
	~PipelineManager();

	bool load(std::string &pipeline_id, const std::string &type, const std::string &uri,
			  const std::string &payload, const std::string &appId, UMSConnector *client_connector, bool isPreload = false);
	bool unload(const std::string &client_connection_id);

	bool suspend(const std::string &client_connection_id);
	bool resume(const std::string &client_connection_id);


	bool play(const std::string &client_connection_id);
	bool stateChange(const std::string &client_connection_id, bool subscribed);
	bool stop(const std::string &client_connection_id);
	bool pause(const std::string &client_connection_id);
	bool seek(const std::string &client_connection_id,long long pos);

	bool setUri(const std::string &client_connection_id, const std::string &uri, const std::string& option);
	bool getType(const std::string &client_connection_id, std::string &type);
	bool setPlayRate(const std::string &client_connection_id, double rate, bool audioOutput);
	bool selectTrack(const std::string &client_connection_id, const std::string &type, int32_t index);
	bool setSubtitleSource(const std::string &client_connection_id, const std::string &uri, std::string preferredEncodings);
	bool setSubtitleEnable(const std::string &client_connection_id, bool enable);
	bool setSubtitlePosition(const std::string &client_connection_id, int32_t position);
	bool setSubtitleSync(const std::string &client_connection_id, int32_t sync);
	bool setSubtitleFontSize(const std::string &client_connection_id, int32_t font_size);
	bool setSubtitleColor(const std::string &client_connection_id, int32_t color);
	bool setSubtitleEncoding(const std::string &client_connection_id, const std::string &encoding);
	bool setSubtitlePresentationMode(const std::string &client_connection_id, const std::string &presentationMode);
	bool setSubtitleCharacterColor(const std::string &client_connection_id, const std::string &charColor);
	bool setSubtitleCharacterOpacity(const std::string &client_connection_id, int32_t charOpacity);
	bool setSubtitleCharacterFontSize(const std::string &client_connection_id, const std::string &charFontSize);
	bool setSubtitleCharacterFont(const std::string &client_connection_id, const std::string &charFont);
	bool setSubtitleBackgroundColor(const std::string &client_connection_id, const std::string &bgColor);
	bool setSubtitleBackgroundOpacity(const std::string &client_connection_id, int32_t bgOpacity);
	bool setSubtitleCharacterEdge(const std::string &client_connection_id, const std::string &charEdgeType);
	bool setSubtitleWindowColor(const std::string &client_connection_id, const std::string &windowColor);
	bool setSubtitleWindowOpacity(const std::string &client_connection_id, int32_t windowOpacity);

	bool setUpdateInterval(const std::string &client_connection_id,
			int32_t current_time_interval, int32_t buffer_range_interval);

	bool setUpdateInterval(const std::string &client_connection_id,
			std::string key, int32_t value);

	bool takeCameraSnapshot(const std::string &client_connection_id, const std::string &location, const std::string &format,
													int32_t width, int32_t height, int32_t pictureQuality);
	bool startCameraRecord(const std::string &client_connection_id, const std::string &location, const std::string &format);
	bool stopCameraRecord(const std::string &client_connection_id);
	bool changeResolution(const std::string &client_connection_id, int32_t width, int32_t height);
	bool setStreamQuality(const std::string &client_connection_id, int32_t width, int32_t height, int32_t bitRate, bool init);
	bool setProperty(const std::string &client_connection_id, const std::string &payload);
	bool setDescriptiveVideoService(const std::string &client_connection_id, bool enable);

	bool setVolume(const std::string& client_connection_id, int32_t volume, int32_t duration, EaseType type);
	// TODO: Resolve this API against drd4tv bool setVolume(const std::string& client_connection_id, int32_t volume, const std::string& ease);

	bool setMasterClock(const std::string& client_connection_id, const std::string& ip, int32_t port);
	bool setSlaveClock(const std::string& client_connection_id, const std::string& ip, int32_t port, int64_t baseTime);
	bool setAudioDualMono(const std::string& client_connection_id, int32_t audioMode);

	void pipelinePidUpdate(const std::string &appid, pid_t pid, bool exec);
	void pipelineLoadFailurehandler(const std::string& client_connection_id);
	void pipelineForegroundStatusUpdatehandler();
	void setLogLevel(const std::string & level) { log.setLogLevel(level); }
	void setLogLevelPipeline(const std::string &client_connection_id, const std::string& level);

	// state query operations
	bool getPipelineState(const std::string &client_connection_id,
			std::string &state);
	bool logPipelineState(const std::string &client_connection_id);

	bool getActivePipeline(const std::string &client_connection_id, std::string &active_pipeline_out);
	bool getActivePipeline(const std::string &client_connection_id, pbnjson::JValue &active_pipeline_out, bool show_app_id = true);
	bool getActivePipelines(pbnjson::JValue &active_pipelines_out, bool app_tracking = false);
	bool getActivePipelines(pipeline_map_t &pipelines_out);
	int getActivePipelineCount();
	std::set<std::string> getActivePipelines(const std::string &app_id);
	bool setPipelineDebugState(const std::string &client_connection_id,
			const std::string &state);
	std::string getPipelineServiceName(const std::string &client_connection_id);

	// set sync for slave, master device
	bool setSlave(const std::string &client_connection_id, const std::string &ip, int port, const std::string &basetime);
	bool setMaster(const std::string &client_connection_id, const std::string &ip, int port, UMSConnectorEventFunction cb, void *ctx);

	// set renderer plane id
	bool setPlane(const std::string &client_connection_id, int32_t plane_id);

	// pipeline exited signal
	boost::signals2::signal<void (const std::string &id)> pipeline_exited;
	boost::signals2::signal<void (const std::string &id)> pipeline_removed;
	boost::signals2::signal<void (const std::string &id)> pipeline_load_failed;
	boost::signals2::signal<void (const std::string &, pid_t, bool)> pipeline_pid_update;
	boost::signals2::signal<void ()> fg_pipeline_status_update;

private:
	Logger log;
	ProcessPool process_pool;

	// pipeline map management
	pipeline_map_t pipelines;

	void processExited(const std::string & id);
	Pipeline::ptr_t findPipeline(const std::string &name);

	std::string default_debug_state;
};
} // namespace uMediaServer

#endif  //_PIPELINE_MGR_H
