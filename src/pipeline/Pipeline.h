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
#ifndef _PIPELINE_H
#define _PIPELINE_H

#include <iostream>
#include <stdexcept>
#include <vector>
#include <sstream>
#include <stdio.h>
#include <cassert>
#include <string>

#include <thread>
#include <atomic>
#include <memory>

#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include <signal.h>
#include <execinfo.h>
#include <ucontext.h>
#include <boost/lexical_cast.hpp>  // for lexical_cast<string>(number)
#include <boost/thread.hpp>
#include <boost/signals2/signal.hpp>
#include <pbnjson.hpp>

#include <Logger.h>
#include <ProcessPool.h>
#include <boost/fusion/adapted.hpp>
#include <uMediaTypes.h>

#include "UMSConnector.h"
#include "GLibHelper.h"
#include "Registry.h"

namespace uMediaServer {

#define PIPELINE_STARTING		"starting"
#define PIPELINE_RESTARTING		"restarting"
#define PIPELINE_RUNNING		"running"
#define PIPELINE_MEDIA_LOADED           "media_loaded"
#define PIPELINE_MEDIA_PRELOADED        "media_preloaded"
#define PIPELINE_STOP			"stop"
#define PIPELINE_SUSPENDED		"suspended"

#define MEDIA_LOAD		"load"
#define MEDIA_PRELOAD           "preload"
#define MEDIA_UNLOAD            "unload"
#define MEDIA_PLAY		"play"
#define MEDIA_PAUSE		"pause"
#define MEDIA_STOP		"stop"

class Pipeline : public std::enable_shared_from_this<Pipeline>
{
public:
	typedef std::shared_ptr<Pipeline> ptr_t;
	typedef std::weak_ptr<Pipeline> weak_ptr_t;

	Pipeline(const std::string & type, ProcessPool & pool);
	virtual ~Pipeline();

	// media operations
	bool load(const std::string & appid, const std::string & uri, const std::string & payload,
			  UMSConnector * connector);
        bool preload(const std::string & appid, const std::string & uri, const std::string & payload,
                          UMSConnector * connector);
	bool unload();
	bool suspend();
	bool resume();

	bool play();
	bool stateChange(bool subscribed);
	bool pause();
	bool seek(long long position);

	bool setUri(const std::string &uri, const std::string& option);
	bool setPlayRate(double rate, bool audioOutput);
	bool selectTrack(const std::string &type, int32_t index);

	// set information for subtitle
	bool setSubtitleSource(const std::string &uri, std::string preferredEncodings);
	bool setSubtitleEnable(bool enable);
	bool setSubtitlePosition(int32_t position);
	bool setSubtitleSync(int32_t sync);
	bool setSubtitleFontSize(int32_t font_size);
	bool setSubtitleColor(int32_t color);
	bool setSubtitleEncoding(const std::string &encoding);
	bool setSubtitlePresentationMode(const std::string &presentationMode);
	bool setSubtitleCharacterColor(const std::string &charColor);
	bool setSubtitleCharacterOpacity(int32_t charOpacity);
	bool setSubtitleCharacterFontSize(const std::string &charFontSize);
	bool setSubtitleCharacterFont(const std::string &charFont);
	bool setSubtitleBackgroundColor(const std::string &bgColor);
	bool setSubtitleBackgroundOpacity(int32_t bgOpacity);
	bool setSubtitleCharacterEdge(const std::string &charEdgeType);
	bool setSubtitleWindowColor(const std::string &windowColor);
	bool setSubtitleWindowOpacity(int32_t windowOpacity);

	// set update interval for notification
	bool setUpdateInterval(int32_t current_time_interval,
			int32_t buffer_range_interval);
	bool setUpdateInterval(std::string key, int32_t value);

	bool takeSnapshot(const std::string &location, const std::string &format, int32_t width, int32_t height, int32_t pictureQuality);
	bool startRecord(const std::string &location, const std::string &format);
	bool stopRecord();
	bool changeResolution(int32_t width, int32_t height);
	bool setStreamQuality(int32_t width, int32_t height, int32_t bitRate, bool init);

	bool setProperty(const std::string &payload);
	bool setDescriptiveVideoService(bool enable);

	bool setVolume(int32_t volume, int32_t duration, EaseType type);
	// TODO: resolve against this API from drd4tv bool setVolume(int32_t volume, const std::string& ease);

	bool setMasterClock(const std::string& ip, int32_t port);
	bool setSlaveClock(const std::string& ip, int32_t port, int64_t baseTime);
	bool setAudioDualMono(int32_t audioMode);

	bool setPlane(int32_t plane_id);

	// state query operations
	std::string getPipelineState();
	void logPipelineState();

	void setPipelineDebugState(const std::string &state);

	const std::string& getID() const { return id_; }
	const std::string& getProcessConnectionID() const { return process_connection_id; }
	const std::string& getType() const { return type_; }
	const std::string& getURI() const { return uri_; }
	pid_t getPID() const { return process_handle ? process_handle->pid() : 0; }
	std::string getProcessState();
	std::string getMediaState();
	std::string getAppId() { return appid_; }
	void setLogLevel(const std::string & level) { log.setLogLevel(level); }

	// set information for Slave, Master device
	bool setSlave(const std::string &ip, int32_t port, const std::string basetime);
	bool setMaster(const std::string &ip, int32_t port, UMSConnectorEventFunction cb, void *ctx);

	// Events for PipelineManager observer
	boost::signals2::signal<void (const std::string &, pid_t, bool)> signal_pid;

private:
	static std::map<std::string, std::unique_ptr<pbnjson::JSchema>> api_schema;

	struct command_t {
		command_t( const std::string & command, const std::string & payload)
			: command(command), payload(payload) {}

		command_t( const std::string & command)
			: command(command) {}

		std::string command;
		std::string payload;
	};

	Logger log;

	// unique ID of Pipeline Controller <UID>
	std::string id_;

	// unique connection ID of pipeline process
	std::string process_connection_id; // format: pipeline_<PID>

	std::string type_;            // pipeline type (ref, sim, file, media, camera, ...)
	std::string uri_;             // eg: file://media/internal/rat.mp4
	std::string payload_;         // misc load options (start time, content type, user agent, etc.)
	std::string appid_;           // parent application id

	std::vector<command_t> m_command_queue;

	ProcessPool & process_pool;
	Process::ptr_t process_handle;

	bool m_restarting;
        bool m_preloading;
	bool m_process_starting;
	std::atomic_bool m_subscribed; // is this particular pipeline currently subscribed to by a client

	UMSConnector * pipeline_connector;
	UMSConnector * client_connector;

	// handle subscription message callbacks.
	static bool pipelineProcessStateEvent(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctxt);

	// subscribe to Pipeline(process) state change events and continue pipeline loading
	void startProcess();
	void finishLoading(const std::string & service_name, Process::ptr_t process);
	bool processLoadCompleted();
	bool processPreloadCompleted();

	// state management
	void updatePipelineProcessState(std::string state);
	void updatePipelineMediaState(std::string state);
	void updateState(const std::string & msg);
	void setSeekPos(long long);
	long long getSeekPos();
	static const pbnjson::JSchema & getSchema(const std::string & schema_path);
	std::string updatePayloadForResume(void);
	std::string serializeLoadArgs(const pbnjson::JValue &);

	class PipelineState
	{
	public:
		PipelineState(Pipeline *pipeline) : m_no_of_tabs(0) {
			m_pipeline = pipeline;
			m_state_object = pbnjson::Object();
		}
		~PipelineState() {}
		bool update(pbnjson::JValue update_arg);
		pbnjson::JValue getJsonObj() {std::lock_guard<std::mutex> lock(state_mutex); return m_state_object;}
		std::string getJsonString();
		void printState();
	private:
		uMediaServer::Logger log;
		Pipeline *m_pipeline;
		pbnjson::JValue m_state_object;
		int m_no_of_tabs; // PmLogLib doesn't print tabs so this is converted to spaces into m_indentation
		std::string m_indentation;
		std::mutex state_mutex;
		void updateFields(const pbnjson::JValue & update_arg, pbnjson::JValue state_arg);
		void printFields(const pbnjson::JValue & object);
	} m_pipeline_json_state;

	friend class PipelineState;
};

} // namespace uMediaServer

#endif  //_PIPELINE_H
