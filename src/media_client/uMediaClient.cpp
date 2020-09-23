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

//->Start of API documentation comment block
/**
@page com_media_client uMediaClient
@brief Provides media playback and record functionality
@{
@}
*/
//->End of API documentation comment block

#include "uMediaClient.h"

#include <UMSConnector.h>
#include <Logger_macro.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <GenerateUniqueID.h>

#define MEDIA_CLIENT_CONNECTION_BASE_ID "com.webos.media.client."
#define LOAD_TIMEOUT_SECONDS 10

using namespace uMediaServer;
using namespace pbnjson;
using namespace std;

namespace {
	Logger _log(UMS_LOG_CONTEXT_CLIENT);

JValue serialize_rect (const rect_t & rect) {
	JValue json_rect = Object();
	json_rect.put("x", rect.x);
	json_rect.put("y", rect.y);
	json_rect.put("width", rect.w);
	json_rect.put("height", rect.h);
	return json_rect;
}

}

// ------------------------------------------
// uMediaClient - controller + listener

uMediaClient::uMediaClient(bool rawEvents, UMSConnectorBusType bus, const std::string &appConnId)
	: m_umediaserver_connection_id(UMEDIASERVER_CONNECTION_ID)
	, m_app_connection_id(appConnId)
	, load_state(UMEDIA_CLIENT_UNLOADED), visible(true), _focus(false)
	, rawEventsFlag(rawEvents), bus(bus)
{
	//prefix of uid should be '-' when the client is created under app privileged case
	std::string uid = GenerateUniqueID()();
	if (!m_app_connection_id.empty()) {
		std::replace_if(uid.begin(), uid.begin() + 1, [](char c) { return c == '_'; }, '-');
		/*Umediaserver receiving app_id as app_id + display_id. So on LSRegister
		 * call it's failed for the same. So removing display_id from app_id*/
		m_app_connection_id = m_app_connection_id.substr(0,m_app_connection_id.length()-1);
	}

	_log.setUniqueId(uid);
	std::string process_connection_id =
		(!m_app_connection_id.empty() ? m_app_connection_id : MEDIA_CLIENT_CONNECTION_BASE_ID) + uid;
	LOG_INFO(_log, "connection-id", "create ums client with connection Id : %s\t app_id : %s",
			process_connection_id.c_str(), m_app_connection_id.c_str());

	context = g_main_context_new();
	gmain_loop = g_main_loop_new(context, false);
	connection = new UMSConnector(process_connection_id, gmain_loop,
			static_cast<void*>(this), bus, false, m_app_connection_id);

	pthread_cond_init(&load_state_cond,NULL);
	pthread_mutex_init(&mutex,NULL);
	pthread_mutex_init(&media_id_mutex, NULL);

	// start message handling thread
	pthread_create(&message_thread,NULL,messageThread,this);
}

uMediaClient::uMediaClient(const std::string &appConnId)
	: uMediaClient(false, UMS_CONNECTOR_PUBLIC_BUS, appConnId)
{
}

uMediaClient::~uMediaClient()
{
	connection->stop();
	pthread_join(message_thread,NULL);

	delete connection;
	g_main_context_unref(context);
	g_main_loop_unref(gmain_loop);

	pthread_cond_destroy(&load_state_cond);
	pthread_mutex_destroy(&mutex);
	pthread_mutex_destroy(&media_id_mutex);
}

void * uMediaClient::messageThread(void *arg)
{
	uMediaClient *self = static_cast<uMediaClient *>(arg);
	self->connection->wait();
	return nullptr;
}

std::string uMediaClient::getMediaId()
{
	std::string copied_media_id;
	pthread_mutex_lock(&media_id_mutex);
	copied_media_id = media_id;
	pthread_mutex_unlock(&media_id_mutex);
	return copied_media_id;
}

void uMediaClient::setMediaId(const std::string& new_media_id)
{
	if (getMediaId() != new_media_id) {
		pthread_mutex_lock(&media_id_mutex);
		media_id = new_media_id;
		pthread_mutex_unlock(&media_id_mutex);
	}
}

// @f subscribe
// @brief subscribe to uMediaServer state change events
//
void uMediaClient::subscribe()
{
	pbnjson::JValue args = pbnjson::Object();

	args.put("mediaId", getMediaId());	 // {"mediaId":<mediaId>}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(_log, MSGERR_JSON_SERIALIZE, "failure to serializer.toString()");
		return;
	}

	string cmd = m_umediaserver_connection_id + "/subscribe";
	connection->subscribe(cmd, payload_serialized, subscribeCallback, (void*)this);
}

// @f unsubscribe
// @brief unsubscribe to uMediaServer state change events
//
void uMediaClient::unsubscribe()
{
	pbnjson::JValue args = pbnjson::Object();

	args.put("mediaId", getMediaId());	 // {"mediaId":<mediaId>}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(_log, MSGERR_JSON_SERIALIZE, "failure to serializer.toString()");
		return;
	}

	string cmd = m_umediaserver_connection_id + "/unsubscribe";
	connection->sendMessage(cmd, payload_serialized, commandResponseCallback, (void*)this);
}

// @f getStateName
// @brief return state change name
//
bool uMediaClient::getStateData(const string & message, string &name, JValue &value)
{
	JDomParser parser;
	if (!parser.parse(message, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR(_log, MSGERR_JSON_PARSE, "JDomParse. input=%s",
				message.c_str());
		return false;
	}

	JValue state = parser.getDom();
	if ( !(*state.begin()).first.isString() ) {
		LOG_ERROR(_log, MSGERR_JSON_SCHEMA, "error. stateChange name != string");
		return false;
	}
	name = (*state.begin()).first.asString();
	value = state[name];

	return value.isObject() ? true : false;
}

// @f stateChange
// @brief handle state change messages.  Parse known states and call associated virtual method
//
// FIXME: remove third argument once we can safely change API
bool uMediaClient::stateChange(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx)
{
	const char *msg = connection->getMessageText(message);

	std::string name;
	JValue value = Object();
	bool rv = getStateData(msg,name,value);
	if (!rv)
		LOG_WARNING(_log, MSGERR_JSON_PARSE, "Invalid value type detected");

	// First: do internal signaling
	if (name == "loadCompleted") {
                load_state = UMEDIA_CLIENT_LOADED;
	}
        else if (name == "preloadCompleted") {
                load_state = UMEDIA_CLIENT_PRELOADED;
        }
	else if (name == "detached") {
		load_state = UMEDIA_CLIENT_DETACHED;
	};

	// Second: if client requested raw events pass events to him
	if (rawEventsFlag)
		return onUserDefinedChanged(msg);

	// Third: do normal notification dispatching
	if (name == "currentTime") {
		int64_t currentTime = value.asNumber<int64_t>();
		if (_stream_time_callback) {
			_stream_time_callback(currentTime);
			return true;
		}
		return onCurrentTime(currentTime);
	}
	else if (name == "sourceInfo") {
#if UMS_INTERNAL_API_VERSION == 2
		ums::source_info_t source_info = {};
		source_info.container = value["container"].asString();
		source_info.duration = value["duration"].asNumber<int64_t>();

		source_info.seekable = value["seekable"].asBool();
		for (size_t p = 0; p < value["programs"].arraySize(); p++) {
			const auto & program = value["programs"][p];
			source_info.programs.push_back(ums::program_info_t { (uint32_t)program["video_stream"].asNumber<int32_t>(),
																 (uint32_t)program["audio_stream"].asNumber<int32_t>() });
		}

		for (size_t v = 0; v < value["video_streams"].arraySize(); ++v) {
			const auto & vs = value["video_streams"][v];
			ums::video_info_t video_info;

			video_info.codec = vs["video"]["codec"].asString();
			video_info.bit_rate = vs["video"]["bit_rate"].asNumber<int64_t>();
			video_info.width = vs["video"]["width"].asNumber<int32_t>();
			video_info.height = vs["video"]["height"].asNumber<int32_t>();
			video_info.frame_rate = ums::rational_t { vs["video"]["frame_rate"]["num"].asNumber<int32_t>(),
													  vs["video"]["frame_rate"]["den"].asNumber<int32_t>() };
			source_info.video_streams.push_back(video_info);
		}
		for (size_t v = 0; v < value["audio_streams"].arraySize(); ++v) {
			const auto & as = value["audio_streams"][v];
			ums::audio_info_t audio_info;
			audio_info.codec = as["codec"].asString();
			audio_info.bit_rate = as["bit_rate"].asNumber<int64_t>();
			audio_info.sample_rate = as["sample_rate"].asNumber<int32_t>();
			source_info.audio_streams.push_back(audio_info);
		}
		if (_source_info_callback)
			_source_info_callback(source_info);
		return true;
#else
		source_info_t sourceInfo = {};
		sourceInfo.container = unmarshallstring(value["container"]);
		sourceInfo.numPrograms = unmarshalllong(value["numPrograms"]);
		sourceInfo.seekable= unmarshallboolean(value["seekable"]);
		sourceInfo.trickable= unmarshallboolean(value["trickable"]);
		sourceInfo.rotation = unmarshalllong(value["rotation"]);
		JValue programInfoArray = value["programInfo"];

		if (programInfoArray.arraySize() == sourceInfo.numPrograms) {
			for (int i = 0; i < sourceInfo.numPrograms; i++) {
				program_info_t programInfo = {};

				JValue programInfoValue = programInfoArray[i];
				programInfo.duration = unmarshalllonglong(programInfoValue["duration"]);
				programInfo.parentControl = unmarshalllong(programInfoValue["parentControl"]);
				//extract audio track info
				if(programInfoValue.hasKey("numAudioTracks")) {
					programInfo.numAudioTracks = unmarshalllong(programInfoValue["numAudioTracks"]);
					for(int j = 0; j < programInfo.numAudioTracks; j++) {
						audio_track_t audioTrackInfo = {};
						audioTrackInfo.language 	= unmarshallstring(programInfoValue["audioTrackInfo"][j]["language"]);
						audioTrackInfo.codec 		= unmarshallstring(programInfoValue["audioTrackInfo"][j]["codec"]);
						audioTrackInfo.profile 		= unmarshallstring(programInfoValue["audioTrackInfo"][j]["profile"]);
						audioTrackInfo.level 		= unmarshallstring(programInfoValue["audioTrackInfo"][j]["level"]);
						audioTrackInfo.bitRate 		= unmarshalllong(programInfoValue["audioTrackInfo"][j]["bitRate"]);
						audioTrackInfo.sampleRate 	= unmarshallfloat(programInfoValue["audioTrackInfo"][j]["sampleRate"]);
						audioTrackInfo.channels 	= unmarshalllong(programInfoValue["audioTrackInfo"][j]["channels"]);
						audioTrackInfo.pid 			= unmarshalllong(programInfoValue["audioTrackInfo"][j]["pid"]);
						audioTrackInfo.ctag 		= unmarshalllong(programInfoValue["audioTrackInfo"][j]["ctag"]);
						audioTrackInfo.audioDescription = unmarshallboolean(programInfoValue["audioTrackInfo"][j]["audioDescription"]);
						audioTrackInfo.audioType 		= unmarshalllong(programInfoValue["audioTrackInfo"][j]["audioType"]);
						audioTrackInfo.trackId		= unmarshalllong(programInfoValue["audioTrackInfo"][j]["trackId"]);
						audioTrackInfo.role		= unmarshallstring(programInfoValue["audioTrackInfo"][j]["role"]);
						audioTrackInfo.adaptationSetId	= unmarshalllong(programInfoValue["audioTrackInfo"][j]["adaptationSetId"]);
						programInfo.audioTrackInfo.push_back(audioTrackInfo);
					}
				}

				//extract video track info
				if(programInfoValue.hasKey("numVideoTracks")) {
					programInfo.numVideoTracks = unmarshalllong(programInfoValue["numVideoTracks"]);
					for(int j = 0; j < programInfo.numVideoTracks; j++) {
						video_track_t videoTrackInfo = {};
						videoTrackInfo.angleNumber	= unmarshalllong(programInfoValue["videoTrackInfo"][j]["angleNumber"]);
						videoTrackInfo.codec 		= unmarshallstring(programInfoValue["videoTrackInfo"][j]["codec"]);
						videoTrackInfo.profile 		= unmarshallstring(programInfoValue["videoTrackInfo"][j]["profile"]);
						videoTrackInfo.level 		= unmarshallstring(programInfoValue["videoTrackInfo"][j]["level"]);
						videoTrackInfo.width 		= unmarshalllong(programInfoValue["videoTrackInfo"][j]["width"]);
						videoTrackInfo.height 		= unmarshalllong(programInfoValue["videoTrackInfo"][j]["height"]);
						videoTrackInfo.aspectRatio	= unmarshallstring(programInfoValue["videoTrackInfo"][j]["aspectRatio"]);
						videoTrackInfo.pixelAspectRatio	= unmarshallstring(programInfoValue["videoTrackInfo"][j]["pixelAspectRatio"]);
						videoTrackInfo.bitRate 		= unmarshalllong(programInfoValue["videoTrackInfo"][j]["bitRate"]);
						videoTrackInfo.frameRate 	= unmarshallfloat(programInfoValue["videoTrackInfo"][j]["frameRate"]);
						videoTrackInfo.pid 			= unmarshalllong(programInfoValue["videoTrackInfo"][j]["pid"]);
						videoTrackInfo.ctag 		= unmarshalllong(programInfoValue["videoTrackInfo"][j]["ctag"]);
						videoTrackInfo.progressive	= unmarshallboolean(programInfoValue["videoTrackInfo"][j]["progressive"]);
						videoTrackInfo.trackId		= unmarshalllong(programInfoValue["videoTrackInfo"][j]["trackId"]);
						videoTrackInfo.role		= unmarshallstring(programInfoValue["videoTrackInfo"][j]["role"]);
						videoTrackInfo.adaptationSetId	= unmarshalllong(programInfoValue["videoTrackInfo"][j]["adaptationSetId"]);
						videoTrackInfo.orgCodec 	= unmarshallstring(programInfoValue["videoTrackInfo"][j]["orgCodec"]);
						programInfo.videoTrackInfo.push_back(videoTrackInfo);
					}
				}

				//extract subtitle track info
				if(programInfoValue.hasKey("numSubtitleTracks")) {
					programInfo.subtitleType = unmarshallstring(programInfoValue["subtitleType"]);
					programInfo.numSubtitleTracks = unmarshalllong(programInfoValue["numSubtitleTracks"]);
					for(int j = 0; j < programInfo.numSubtitleTracks; j++) {
						subtitle_track_t subtitleTrackInfo = {};
						subtitleTrackInfo.language			= unmarshallstring(programInfoValue["subtitleTrackInfo"][j]["language"]);
						subtitleTrackInfo.pid 				= unmarshalllong(programInfoValue["subtitleTrackInfo"][j]["pid"]);
						subtitleTrackInfo.ctag 				= unmarshalllong(programInfoValue["subtitleTrackInfo"][j]["ctag"]);
						subtitleTrackInfo.type 				= unmarshalllong(programInfoValue["subtitleTrackInfo"][j]["type"]);
						subtitleTrackInfo.compositionPageId = unmarshalllong(programInfoValue["subtitleTrackInfo"][j]["compositionPageId"]);
						subtitleTrackInfo.ancilaryPageId 	= unmarshalllong(programInfoValue["subtitleTrackInfo"][j]["ancilaryPageId"]);
						subtitleTrackInfo.hearingImpared 	= unmarshallboolean(programInfoValue["subtitleTrackInfo"][j]["hearingImpared"]);
						subtitleTrackInfo.trackId			= unmarshalllong(programInfoValue["subtitleTrackInfo"][j]["trackId"]);
						programInfo.subtitleTrackInfo.push_back(subtitleTrackInfo);
					}
				}
				sourceInfo.programInfo.push_back(programInfo);
			}
		}

		sourceInfo.startDate = (int64_t)unmarshalllonglong(value["startDate"]);

		if (value.hasKey("numDownloadableFontInfos")) {
			sourceInfo.numDownloadableFontInfos = unmarshalllong(value["numDownloadableFontInfos"]);
			for (int i = 0; i < sourceInfo.numDownloadableFontInfos; i++) {
				downloadable_font_into_t downloadableFontInfo = {};

				downloadableFontInfo.descriptor = unmarshallstring(value["downloadableFontInfo"][i]["descriptor"]);
				downloadableFontInfo.url = unmarshallstring(value["downloadableFontInfo"][i]["url"]);
				downloadableFontInfo.mimeType = unmarshallstring(value["downloadableFontInfo"][i]["mimeType"]);
				downloadableFontInfo.fontFamily = unmarshallstring(value["downloadableFontInfo"][i]["fontFamily"]);
				sourceInfo.downloadableFontInfo.push_back(downloadableFontInfo);
			}
		}

		if (_source_info_callback) {
			_source_info_callback(sourceInfo);
			return true;
		}

		return onSourceInfo(sourceInfo);
#endif // UMS_INTERNAL_API_VERSION == 2
	}
	else if (name == "streamingInfo") {
		streaming_info_t streamingInfo = {};
		streamingInfo.totalBitrate = unmarshalllong(value["totalBitrate"]);
		streamingInfo.instantBitrate = unmarshalllong(value["instantBitrate"]);
		return onStreamingInfo(streamingInfo);
	}
	else if (name == "videoInfo") {
#if UMS_INTERNAL_API_VERSION == 2
		ums::video_info_t video_info = {};
		video_info.codec = value["video"]["codec"].asString();
		video_info.bit_rate = value["video"]["bitrate"].asNumber<int64_t>();
		video_info.width = value["video"]["width"].asNumber<int32_t>();
		video_info.height = value["video"]["height"].asNumber<int32_t>();
		video_info.frame_rate.num = value["video"]["frame_rate"]["num"].asNumber<int32_t>();
		video_info.frame_rate.den = value["video"]["frame_rate"]["den"].asNumber<int32_t>();
		LOG_INFO(_log, "video_info", "codec=%s, bitrate=%lld, width=%d, height=%d, framerate = %d/%d",
				video_info.codec.c_str(), video_info.bit_rate, video_info.width, video_info.height, video_info.frame_rate.num, video_info.frame_rate.den);

		if (_video_info_callback) {
			_video_info_callback(video_info);
			return true;
		}

		return onVideoInfo(video_info);
#else
		video_info_t videoInfo = {};
		videoInfo.width = unmarshalllong(value["width"]);
		videoInfo.height = unmarshalllong(value["height"]);
		videoInfo.aspectRatio = unmarshallstring(value["aspectRatio"]);
		videoInfo.pixelAspectRatio = unmarshallstring(value["pixelAspectRatio"]);
		videoInfo.frameRate = unmarshallfloat(value["frameRate"]);
		videoInfo.bitRate = unmarshalllong(value["bitRate"]);
		videoInfo.mode3D = unmarshallstring(value["mode3D"]);
		videoInfo.actual3D = unmarshallstring(value["actual3D"]);
		videoInfo.scanType = unmarshallstring(value["scanType"]);

		//SEI meta-info
		JValue seiInfoValue = value["SEI"];
		if (seiInfoValue.isObject()) {
			videoInfo.isValidSeiInfo = true;
			videoInfo.SEI.displayPrimariesX0 = unmarshalllong(seiInfoValue["displayPrimariesX0"]);
			videoInfo.SEI.displayPrimariesX1 = unmarshalllong(seiInfoValue["displayPrimariesX1"]);
			videoInfo.SEI.displayPrimariesX2 = unmarshalllong(seiInfoValue["displayPrimariesX2"]);
			videoInfo.SEI.displayPrimariesY0 = unmarshalllong(seiInfoValue["displayPrimariesY0"]);
			videoInfo.SEI.displayPrimariesY1 = unmarshalllong(seiInfoValue["displayPrimariesY1"]);
			videoInfo.SEI.displayPrimariesY2 = unmarshalllong(seiInfoValue["displayPrimariesY2"]);
			videoInfo.SEI.whitePointX = unmarshalllong(seiInfoValue["whitePointX"]);
			videoInfo.SEI.whitePointY = unmarshalllong(seiInfoValue["whitePointY"]);
			videoInfo.SEI.minDisplayMasteringLuminance = unmarshalllong(seiInfoValue["minDisplayMasteringLuminance"]);
			videoInfo.SEI.maxDisplayMasteringLuminance = unmarshalllong(seiInfoValue["maxDisplayMasteringLuminance"]);
			videoInfo.SEI.maxContentLightLevel = unmarshalllong(seiInfoValue["maxContentLightLevel"]);
			videoInfo.SEI.maxPicAverageLightLevel = unmarshalllong(seiInfoValue["maxPicAverageLightLevel"]);
		}
		else {
			videoInfo.isValidSeiInfo = false;
		}

		//VUI meta-info
		JValue vuiInfoValue = value["VUI"];
		if (vuiInfoValue.isObject()) {
			videoInfo.isValidVuiInfo = true;
			videoInfo.VUI.transferCharacteristics = unmarshalllong(vuiInfoValue["transferCharacteristics"]);
			videoInfo.VUI.colorPrimaries = unmarshalllong(vuiInfoValue["colorPrimaries"]);
			videoInfo.VUI.matrixCoeffs = unmarshalllong(vuiInfoValue["matrixCoeffs"]);
			videoInfo.VUI.videoFullRangeFlag = unmarshallboolean(vuiInfoValue["videoFullRangeFlag"]);
		}
		else {
			videoInfo.isValidVuiInfo = false;
		}

		videoInfo.hdrType = unmarshallstring(value["hdrType"]);

		if (value.hasKey("afd")) {
			videoInfo.afd = unmarshalllong(value["afd"]);
		} else {
			videoInfo.afd = -1;
		}

		if (_video_info_callback) {
			_video_info_callback(videoInfo);
			return true;
		}

		return onVideoInfo(videoInfo);
#endif
	}

	else if (name == "audioInfo") {
#if UMS_INTERNAL_API_VERSION == 2
		ums::audio_info_t audio_info = {};
		audio_info.codec = value["codec"].asString();
		audio_info.bit_rate = value["bitrate"].asNumber<int64_t>();
		audio_info.sample_rate = value["sample_rate"].asNumber<int32_t>();

		LOG_INFO(_log, "audio_info", "codec=%s, bitrate=%lld, samplerate = %d",
				audio_info.codec.c_str(), audio_info.bit_rate, audio_info.sample_rate);

		if (_audio_info_callback) {
			_audio_info_callback(audio_info);
			return true;
		}

		return onAudioInfo(audio_info);
#else
		audio_info_t audioInfo = {};
		audioInfo.dualMono = unmarshallboolean(value["dualMono"]);
		audioInfo.track = unmarshalllong(value["track"]);
		audioInfo.immersive = unmarshallstring(value["immersive"]);
		audioInfo.channels = unmarshalllong(value["channels"]);
		audioInfo.sampleRate = unmarshallfloat(value["sampleRate"]);

		if (value.hasKey("band")) {
			audioInfo.band = unmarshalllong(value["band"]);
			for (int i = 0; i < audioInfo.band; i++) {
				spectrum_info_t spectrumInfo = {};

				spectrumInfo.frequency = unmarshalllong(value["spectrum"][i]["frequency"]);
				spectrumInfo.magnitude = unmarshallfloat(value["spectrum"][i]["magnitude"]);
				spectrumInfo.phase = unmarshallfloat(value["spectrum"][i]["phase"]);
				audioInfo.spectrum.push_back(spectrumInfo);
			}
		} else {
			audioInfo.band = -1;
		}

		return onAudioInfo(audioInfo);
#endif
	}

	else if (name == "activeRegion") {
		rect_t active_rc {
			unmarshalllong(value["x"]),
			unmarshalllong(value["y"]),
			unmarshalllong(value["width"]),
			unmarshalllong(value["height"])
		};
		return onActiveRegion(active_rc);
	}
	else if (name == "trackSelected") {
		return onTrackSelected(unmarshallstring(value["type"]), unmarshalllong(value["index"]));
	}
	else if (name == "endOfStream") {
		if (_eos_callback) {
			_eos_callback();
			return true;
		}
		return onEndOfStream();
	}
	else if (name == "paused") {
		if (_paused_callback) {
			_paused_callback();
			return true;
		}
		return onPaused();
	}
	else if (name == "playing") {
		if (_playing_callback) {
			_playing_callback();
			return true;
		}
		return onPlaying();
	}
	else if (name == "seekDone") {
		return onSeekDone();
	}
	else if (name == "bufferRange") {
		buffer_range_t bufferRange = {};
		bufferRange.beginTime = unmarshalllonglong(value["beginTime"]);
		bufferRange.endTime = unmarshalllonglong(value["endTime"]);
		bufferRange.remainingTime = unmarshalllonglong(value["remainingTime"]);
		bufferRange.percent = unmarshalllonglong(value["percent"]);
		return onBufferRange(bufferRange);
	}
	else if (name == "bufferingStart") {
		return onBufferingStart();
	}
	else if (name == "bufferingEnd") {
		return onBufferingEnd();
	}
	else if (name == "videoFrame") {
		return onVideoFrame();
	}
	else if (name == "loadCompleted") {
		if (_load_completed_callback) {
			_load_completed_callback();
			return true;
		}
		return onLoadCompleted();
	}
        else if (name == "preloadCompleted") {
                return onPreloadCompleted();
        }
	else if (name == "unloadCompleted") {
		if (_unload_completed_callback) {
			_unload_completed_callback();
			return true;
		}
		return onUnloadCompleted();
	}
	else if (name == "detached") {
		unsubscribe();
		return onDetached();
	}
	else if (name == "snapshotDone") {
		return onSnapshotDone();
	}
	else if (name == "fileGenerated") {
		return onFileGenerated();
	}
	else if (name == "recordInfo") {
		record_info_t recordInfo = {};
		recordInfo.recordState = unmarshallboolean(value["recordState"]);
		recordInfo.elapsedMiliSecond = unmarshalllong(value["elapsedMiliSecond"]);
		recordInfo.bitRate = unmarshalllong(value["bitRate"]);
		recordInfo.fileSize = unmarshalllong(value["fileSize"]);
		recordInfo.fps = unmarshalllong(value["fps"]);
		return onRecordInfo(recordInfo);
	}
	else if (name == "error") {
		uint32_t error_code = value["errorCode"].asNumber<int32_t>();
		std::string error_text = value["errorText"].asString();
		if (_error_callback) {
			_error_callback({error_code, error_text});
			return true;
		}
		return onError(error_code, error_text);
	}
	else if (name == "externalSubtitleTrackInfo"){
		external_subtitle_track_info_t extsubtrackInfo = {};
		extsubtrackInfo.uri=unmarshallstring(value["uri"]);
		extsubtrackInfo.numSubtitleTracks=unmarshalllong(value["numSubtitleTracks"]);
		JValue tracksInfoArray = value["tracks"];
		if (tracksInfoArray.arraySize() == extsubtrackInfo.numSubtitleTracks) {
			for (int i = 0; i < extsubtrackInfo.numSubtitleTracks; i++) {
				track_info_t trackInfo = {};

				JValue trackInfoValue = tracksInfoArray[i];
				trackInfo.description = unmarshallstring(trackInfoValue["description"]);
				extsubtrackInfo.tracks.push_back(trackInfo);
			}
		}
		extsubtrackInfo.hitEncoding=unmarshallstring(value["hitEncoding"]);
		return onExternalSubtitleTrackInfo(extsubtrackInfo);
	}
	else if (name == "focusChanged" && value.hasKey("focus")){
        _focus = value["focus"].asBool();
		if (_focus_callback) {
			_focus_callback(_focus);
			return true;
		}
        return onFocusChanged(_focus);
    }
	else if (name == "seekableRanges") {
		seekable_ranges_info_t seekableRangesInfo = {};
		JValue rangesInfoArray = value["seekableRanges"];
		seekableRangesInfo.numRanges = rangesInfoArray.arraySize();
		for (int i = 0; i < seekableRangesInfo.numRanges; i++) {
			seekable_range_t rangeInfo = {};
			rangeInfo.start = (int64_t)unmarshalllonglong(rangesInfoArray[i]["start"]);
			rangeInfo.end = (int64_t)unmarshalllonglong(rangesInfoArray[i]["end"]);

			seekableRangesInfo.seekableRanges.push_back(rangeInfo);
		}
		return onSeekableRanges(seekableRangesInfo);
	}
	else if (name == "setMasterClockResult") {
		master_clock_info_t masterClockInfo = {};
		masterClockInfo.result = unmarshallboolean(value["result"]);
		masterClockInfo.port = unmarshalllong(value["port"]);
		masterClockInfo.baseTime = unmarshalllonglong(value["baseTime"]);
		return onSetMasterClockResult(masterClockInfo);
	}
	else if (name == "setSlaveClockResult") {
		slave_clock_info_t slaveClockInfo = {};
		slaveClockInfo.result = unmarshallboolean(value["result"]);
		return onSetSlaveClockResult(slaveClockInfo);
	}
	else if (name == "subtitleData") {
		subtitle_data_info_t subtitleDataInfo = {};
		subtitleDataInfo.subtitleData = unmarshallstring(value["subtitleData"]);
		subtitleDataInfo.presentationTime = (uint64_t)unmarshalllonglong(value["presentationTime"]);
		return onSubtitleData(subtitleDataInfo);
	}
	else if ( name == "vsmResourceInfo" ) {
		JGenerator serializer(NULL);   // serialize into string
		string payload_serialized;

		if (!serializer.toString(value, pbnjson::JSchema::AllSchema(), payload_serialized)) {
			LOG_ERROR(_log, MSGERR_JSON_SERIALIZE, "failure to serializer.toString()");
			return false;
		}

		return onVsmResourceInfo(payload_serialized);
	}
	else if (name == "validData") {
		bool state = unmarshallboolean(value["state"]);
		return onValidData(state);
	}
	else {
		LOG_DEBUG(_log, "Unknown stateChange event.  Passing raw message to client.");
		return onUserDefinedChanged(msg);
	}

	return true;
}


// ---------------------------------
// ---  uMediaClient API

// -------------------------------------------------
// API command handlers

//->Start of API documentation comment block
/**
@page com_media_client uMediaClient
@{
@section com_media_client_load load

Requests the media server to load a new media object for the specified URI.
This function is synchronous.

@par Parameters
Name | Required | Type | Description
-----|--------|------|----------
uri     | yes | String | Location of media file
type    | yes | Enum   | Pipeline type to launch
payload | yes | String | JSON object containing pipeline specific parameters

@par Returns(Call)
Required. Boolean. Returns true if successful, false otherwise.

@par Returns(Subscription)
None
@}
*/
//->End of API documentation comment block

bool uMediaClient::load(std::string uri,AudioStreamClass audioClass, std::string mediaPayload)
{
	return load(uri, marshallAudioStreamClass(audioClass), mediaPayload);
}

bool uMediaClient::load(const std::string& uri, const std::string& type, const std::string& mediaPayload)
{
	pthread_mutex_lock(&mutex);
	bool result = loadAsync(uri, type, mediaPayload);
	if (result) {
		struct timespec ts;
		struct timeval tp;
		gettimeofday(&tp, NULL);
		// Convert from timeval to timespec + 10 sec timeout
		ts.tv_sec  = tp.tv_sec + LOAD_TIMEOUT_SECONDS;
		ts.tv_nsec = tp.tv_usec * 1000;
		auto ret = pthread_cond_timedwait(&load_state_cond, &mutex, &ts);
		if (ret == ETIMEDOUT) {
			LOG_ERROR(_log, MSGERR_COND_TIMEDWAIT, "Load timeout.");
			result = false;
		}
	}
	pthread_mutex_unlock(&mutex);
	return result;
}

//->Start of API documentation comment block
/**
@page com_media_client uMediaClient
@{
@section com_media_client_loadAsync loadAsync

Requests the media server to load a new media object for the specified URI.
This function is asynchronous.
loadCompleted called when load is completed

@par Parameters
Name | Required | Type | Description
-----|--------|------|----------
uri     | yes | String | Location of media file
type    | yes | Enum   | Pipeline type to launch
payload | yes | String | JSON object containing pipeline specific parameters

@par Returns(Call)
Required. Boolean. Returns true if successful, false otherwise.

@par Returns(Subscription)
None
@}
*/
//->End of API documentation comment block

bool uMediaClient::loadAsync(std::string uri,AudioStreamClass audioClass, std::string mediaPayload)
{
	return loadAsync(uri, marshallAudioStreamClass(audioClass), mediaPayload);
}

bool uMediaClient::loadAsync(const std::string& uri, const std::string& type, const std::string& mediaPayload)
{
	LOG_DEBUG(_log, "payload of loadAsync : %s",mediaPayload.c_str());
	JValue args = pbnjson::Object();   // append all values into JSON array
	args.put("uri", marshallstring(uri));
	args.put("type", marshallstring(type));
	args.put("payload", marshallPayload(mediaPayload));

        if (!getMediaId().empty()) {
          args.put("mediaId", getMediaId());
        } else {
          // Note: uMS will detect additional loads on this object and will
          //     automatically unload the previous media pipeline.
          setMediaId("<invalid mediaId>");
        }

        load_state = UMEDIA_CLIENT_LOADING;
	invokeCall("/load", args, loadResponseCallback);

	return true;
}

// -------------------------------------------------
// API command handlers

//->Start of API documentation comment block
/**
@page com_media_client uMediaClient
@{
@section com_media_client_load load

Requests the media server to load a new media object for the specified URI.
This function is synchronous.
Currently not supported.

@par Parameters
Name | Required | Type | Description
-----|--------|------|----------
uri     | yes | String | Location of media file
type    | yes | Enum   | Pipeline type to launch
payload | yes | String | JSON object containing pipeline specific parameters

@par Returns(Call)
Required. Boolean. Returns true if successful, false otherwise.

@par Returns(Subscription)
None
@}
*/
//->End of API documentation comment block

bool uMediaClient::preload(std::string uri,AudioStreamClass audioClass, std::string mediaPayload)
{
        return true;
}

bool uMediaClient::preload(const std::string& uri, const std::string& type, const std::string& mediaPayload)
{
        return true;
}

// @f attach
// @brief attach client to existing pipeline
//
bool uMediaClient::attach(const std::string& mediaId)
{
	setMediaId("<invalid mediaId>");
	JValue args = pbnjson::Object();   // append all values into JSON array
	args.put("mediaId", marshallstring(mediaId));

	invokeCall("/attach", args, attachResponseCallback);

	return true;
}

//->Start of API documentation comment block
/**
@page com_media_client uMediaClient
@{
@section com_media_client_play play

Plays the media object.

@par Parameters
None

@par Returns(Call)
Required. Boolean. Returns true if successful, false otherwise.

@par Returns(Subscription)
None
@}
*/
//->End of API documentation comment block
bool uMediaClient::play()
{
	pbnjson::JValue args = pbnjson::Object();
	args.put("mediaId", getMediaId()); // {"mediaId":<mediaId>}

	dispatchCall("/play", args);

	return true;
}

//->Start of API documentation comment block
/**
@page com_media_client uMediaClient
@{
@section com_media_client_pause pause

Pauses playback.

@par Parameters
None

@par Returns(Call)
Required. Boolean. Returns true if successful, false otherwise.

@par Returns(Subscription)
None
@}
*/
//->End of API documentation comment block
bool uMediaClient::pause()
{
	pbnjson::JValue args = pbnjson::Object();
	args.put("mediaId", getMediaId()); // {"mediaId":<mediaId>}

	dispatchCall("/pause", args);

	return true;
}

//->Start of API documentation comment block
/**
@page com_media_client uMediaClient
@{
@section com_media_client_seek seek

Seeks to specified time position.
seekDone called when seek is completed.

@par Parameters
Name | Required | Type | Description
-----|--------|------|----------
position | yes | Integer | position in milliseconds from the start to seek to.

@par Returns(Call)
Required. Boolean. Returns true if successful, false otherwise.

@par Returns(Subscription)
None
@}
*/
//->End of API documentation comment block
bool uMediaClient::seek(long long position)
{
	pbnjson::JValue args = pbnjson::Object();
	args.put("mediaId", getMediaId());
	args.put("position",marshalllonglong(position));

	dispatchCall("/seek", args);

	return true;
}

// @f unload
// @brief unload requested media
//
bool uMediaClient::unload()
{
	if (load_state == UMEDIA_CLIENT_UNLOADED) {
		return true;
	}

	pbnjson::JValue args = pbnjson::Object();
	args.put("mediaId", getMediaId());

	dispatchCall("/unload", args);

	return true;
}

// @f notifyForeground
// @brief notify client is in foreground/visible state
//
bool uMediaClient::notifyForeground() {
	pbnjson::JValue args = pbnjson::Object();
	args.put("connectionId", getMediaId());

	dispatchCall("/notifyForeground", args);

	return true;
}

// @f notifyForeground
// @brief notify client is in foreground/visible state
//
bool uMediaClient::notifyBackground() {
	pbnjson::JValue args = pbnjson::Object();
	args.put("connectionId", getMediaId());

	dispatchCall("/notifyBackground", args);

	return true;
}

//->Start of API documentation comment block
/**
@page com_media_client uMediaClient
@{
@section com_media_client_setPlayRate setPlayRate

Change play rate.

@par Parameters
Name | Required | Type | Description
-----|--------|------|----------
playRate    | yes | Integer | rate for playback.
audioOutput | yes | Boolean | determine to mute audio

@par Returns(Call)
Required. Boolean. Returns true if successful, false otherwise.

@par Returns(Subscription)
None
@}
*/
//->End of API documentation comment block
bool uMediaClient::setPlayRate(double playRate, bool audioOutput)
{
	pbnjson::JValue args = pbnjson::Object();
	args.put("mediaId", getMediaId());
	args.put("playRate",marshallfloat(playRate));
	args.put("audioOutput",marshallboolean(audioOutput));

	dispatchCall("/setPlayRate", args);

	return true;
}

//->Start of API documentation comment block
/**
@page com_media_client uMediaClient
@{
@section com_media_client_selectTrack selectTrack

Selects Track : Not supported

@par Parameters
Name | Required | Type | Description
-----|--------|------|----------
type     | yes | String  | track type: video, audio and subtitle.
index    | yes | Integer  | track index to select.

@par Returns(Call)
Required. Boolean. Returns true if successful, false otherwise.

@par Returns(Subscription)
None
@}
*/
//->End of API documentation comment block
bool uMediaClient::selectTrack(string &type, int32_t index)
{
  return true;
}

//->Start of API documentation comment block
/**
@page com_media_client uMediaClient
@{
@section com_media_client_startCameraRecord startCameraRecord

start to record

@par Parameters
Name | Required | Type | Description
-----|--------|------|----------
location        | yes | String  | location to record media
format          | yes | String  | format to be stored

@par Returns(Call)
Required. Boolean. Returns true if successful, false otherwise.

@par Returns(Subscription)
None
@}
*/
//->End of API documentation comment block
bool uMediaClient::startCameraRecord(std::string& location, std::string& format)
{
  pbnjson::JValue args = pbnjson::Object();
  args.put("mediaId",getMediaId());
  args.put("location", marshallstring(location));
  args.put("format", marshallstring(format));

  dispatchCall("/startCameraRecord", args);

  return true;
}

//->Start of API documentation comment block
/**
@page com_media_client uMediaClient
@{
@section com_media_client_stopCameraRecord stopCameraRecord

stop recording

@par Parameters
Name | Required | Type | Description
-----|--------|------|----------

@par Returns(Call)
Required. Boolean. Returns true if successful, false otherwise.

@par Returns(Subscription)
None
@}
*/
//->End of API documentation comment block
bool uMediaClient::stopCameraRecord()
{
  pbnjson::JValue args = pbnjson::Object();
  args.put("mediaId",getMediaId());

  dispatchCall("/stopCameraRecord", args);

  return true;
}

//->Start of API documentation comment block
/**
@page com_media_client uMediaClient
@{
@sction com_media_client_takeCameraSnapshot takeCameraSnapshot

take still image

@par Parameters
Name | Required | Type | Description
-----|--------|------|----------
location        | yes | String  | location to store still image
format          | yes | String  | format to be stored
width           | yes | Integer | width for still image
height          | yes | Integer | height for still image
pictureQuality  | yes | Integer | pictureQuality for still image

@par Returns(Call)
Required. Boolean. Returns true if successful, false otherwise.

@par Returns(Subscription)
None
@}
*/
//->End of API documentation comment block
bool uMediaClient::takeCameraSnapshot(std::string& location, std::string& format, int32_t width, int32_t height, int32_t pictureQuality)
{
  pbnjson::JValue args = pbnjson::Object();
  args.put("mediaId",getMediaId());
  args.put("location", marshallstring(location));
  args.put("format", marshallstring(format));
  args.put("width", marshalllong(width));
  args.put("height", marshalllong(height));
  args.put("pictureQuality", marshalllong(pictureQuality));

  dispatchCall("/takeCameraSnapshot", args);

  return true;
}

//->Start of API documentation comment block
/**
@page com_media_client uMediaClient
@{
@section com_media_client_setVolume setVolume

control input gain

@par Parameters
Name | Required | Type | Description
-----|--------|------|----------
volume   | yes | integer | value of input gain
duration | no  | integer | duration time for easing effect
type     | no  | string  | easing mode

@par Returns(Call)
Required. Boolean. Returns true if successful, false otherwise.

@par Returns(Subscription)
None
@}
*/
//->End of API documentation comment block

bool uMediaClient::setVolume(const int32_t volume)
{
	setVolume(volume, 0, kEaseTypeLinear);
	return true;
}

bool uMediaClient::setVolume(const int32_t volume, const int32_t duration)
{
	setVolume(volume, duration, kEaseTypeLinear);
	return true;
}

bool uMediaClient::setVolume(const int32_t volume, const int32_t duration, const std::string type)
{
	setVolume(volume, duration, string_to_ease_type(type.c_str()));
	return true;
}

bool uMediaClient::setVolume(const int32_t volume, const int32_t duration, const EaseType type)
{
	pbnjson::JValue args = pbnjson::Object();
	args.put("mediaId", getMediaId());
	args.put("volume", marshalllong(volume));

	pbnjson::JValue ease = pbnjson::Object();
	ease.put("duration", marshalllong(duration));
	ease.put("type", marshallstring(ease_type_to_string(type)));
	args.put("ease", ease);

	dispatchCall("/setVolume", args);

	return true;
}

//->Start of API documentation comment block
/**
@page com_media_client uMediaClient
@{
@section com_media_client_setDisplayWindow setDisplayWindow

Specifies desired output window position and dimensions.
Sets display to windowed mode.

@par Parameters
Name           | Required | Type           | Description
---------------|--------  |----------------|-----------------------------------
output_rect    | yes      | const rect_t & | Desired output window rectangle

@par Returns(Call)
Required. Boolean. Returns true if successful, false otherwise.

None
@}
*/
//->End of API documentation comment block
bool uMediaClient::setDisplayWindow(const rect_t & output_rect) {

	JValue args = Object();
	args.put("mediaId", getMediaId());
	args.put("destination", serialize_rect(output_rect));

	dispatchCall("/setDisplayWindow", args);

	return true;
}

//->Start of API documentation comment block
/**
@page com_media_client uMediaClient
@{
@section com_media_client_setDisplayWindow setDisplayWindow

Specifies desired output window position and dimensions.
Specifies part of the video frame to crop and fit.
Sets display to windowed mode.

@par Parameters
Name           | Required | Type           | Description
---------------|--------  |----------------|-----------------------------------
source_rect    | yes      | const rect_t & | Part of the input frame to display
output_rect    | yes      | const rect_t & | Desired output window rectangle

@par Returns(Call)
Required. Boolean. Returns true if successful, false otherwise.

None
@}
*/
//->End of API documentation comment block
bool uMediaClient::setDisplayWindow(const rect_t & source_rect, const rect_t & output_rect) {
	JValue args = Object();
	args.put("mediaId", getMediaId());
	args.put("destination", serialize_rect(output_rect));
	args.put("source", serialize_rect(source_rect));

	dispatchCall("/setDisplayWindow", args);

	return true;
}

//->Start of API documentation comment block
/**
@page com_media_client uMediaClient
@{
@section com_media_client_switchToFullscreen switchToFullscreen

Switch video output to fullscreen.

@par Parameters
None

@par Returns(Call)
Required. Boolean. Returns true if successful, false otherwise.

None
@}
*/
//->End of API documentation comment block
bool uMediaClient::switchToFullscreen() {
	JValue args = Object();
	args.put("mediaId", getMediaId());

	dispatchCall("/switchToFullScreen", args);

	return true;
}

//->Start of API documentation comment block
/**
@page com_media_client uMediaClient
@{
@section com_media_client_switchToAutoLayout switchToAutoLayout

Delegate video output layout to MDC.
Not supported. Using setDisplayWindow is recommended.

@par Parameters
None

@par Returns(Call)
Required. Boolean. Returns true if successful, false otherwise.

None
@}
*/
//->End of API documentation comment block
bool uMediaClient::switchToAutoLayout() {

	return true;
}

//->Start of API documentation comment block
/**
@page com_media_client uMediaClient
@{
@section com_media_client_setFocus setFocus

Request media focus.

@par Parameters
None

@par Returns(Call)
Required. Boolean. Returns true if successful, false otherwise.

None
@}
*/
//->End of API documentation comment block
bool uMediaClient::setFocus() {
	_focus = true;

	JValue args = Object();
	args.put("mediaId", getMediaId());

	dispatchCall("/focus", args);

	return true;
}

//->Start of API documentation comment block
/**
@page com_media_client uMediaClient
@{
@section com_media_client_focus focus

Query whether current media has focus.

@par Parameters
None

@par Returns(Call)
Required. Boolean. Returns true if media's in focus, false otherwise.

None
@}
*/
//->End of API documentation comment block
bool uMediaClient::focus() const {
	return _focus;
}

//->Start of API documentation comment block
/**
@page com_media_client uMediaClient
@{
@section com_media_client_visibility visibility

Queries media object visibility state.

@par Parameters
None

@par Returns(Call)
Required. Boolean. Returns true if visible, false otherwise.

None
@}
*/
//->End of API documentation comment block
bool uMediaClient::visibility() const {
	return visible;
}

//->Start of API documentation comment block
/**
@page com_media_client uMediaClient
@{
@section com_media_client_setVisibility setVisibility

Sets visibility state for current media object.

@par Parameters
	Name    | Required |   Type  | Description
------------|----------|---------|------------------------------------
 visibility |    yes   | Boolean | true for visible, false for hidden.

@par Returns(Call)
Required. Boolean. Returns true if successful, false otherwise.

None
@}
*/
//->End of API documentation comment block

bool uMediaClient::setVisibility(bool visibility) {
	if (visible != visibility) {
		visible = visibility;
		dispatchCall("/setVisibility", JObject{{"mediaId", getMediaId()}, {"visible", visible}});
	}
	return true;
}

void uMediaClient::run()
{
	connection->wait();
}

void uMediaClient::stop()
{
	connection->stop();
}

// -----------------------------------------------
// --- utility functions to create JSON arguments

bool uMediaClient::loadResponse(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx)
{
	JDomParser parser;

	const char *status = connection->getMessageText(message);

	if (!parser.parse(status, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR(_log, MSGERR_JSON_PARSE,"JDomParser.parse. status=%s ", status);
		return false;
	}

	JValue parsed = parser.getDom();
	if (!parsed.hasKey("mediaId")) {
		LOG_ERROR(_log, MSGERR_JSON_SCHEMA, "load failed. status=%s", status);
		return false;
	}

	setMediaId(parsed["mediaId"].asString()); // save mediaId issued by uMediaServer

	pthread_mutex_lock(&mutex);
	load_state = UMEDIA_CLIENT_MEDIAID_VALID;

	// TODO: optimize by squashing some events. Eg play,play,pause,play => play
	// or seek, seek, seek => seek
	// fire off message stash
	std::string copied_media_id(getMediaId());
	for (auto & msg : message_queue) {
		msg.second.put("mediaId", copied_media_id);
		invokeCall(msg.first, msg.second);
	}
	message_queue.clear();

	subscribe();  // subscribe to state change messages

	_log.setUniqueId(copied_media_id);

	pthread_cond_signal(&load_state_cond);
	pthread_mutex_unlock(&mutex);

	return true;
}

bool uMediaClient::attachResponse(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx)
{
	JDomParser parser;

	const char *status = connection->getMessageText(message);

	if (!parser.parse(status, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR(_log, MSGERR_JSON_PARSE,"JDomParser.parse. status=%s ", status);
		return 1;
	}

	JValue parsed = parser.getDom();
	if (!parsed.hasKey("mediaId")) {
		LOG_ERROR(_log, MSGERR_JSON_SCHEMA, "load failed. status=%s", status);
		return false;
	}

	pthread_mutex_lock(&mutex);
	setMediaId(parsed["mediaId"].asString()); // save mediaId issued by uMediaServer
	load_state = UMEDIA_CLIENT_LOADED;
	pthread_mutex_unlock(&mutex);

	_log.setUniqueId(getMediaId());

	subscribe();  // subscribe to state change messages

	return true;
}

bool uMediaClient::commandResponse(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx)
{
	// not used for normal commands but left in place for future uMediaServer return status
	const char *msg = connection->getMessageText(message);
	std::string name;
	JValue value = Object();
	bool rv = getStateData(msg,name,value);
	if (!rv)
		LOG_WARNING(_log, MSGERR_JSON_PARSE, "Invalid value type detected");
	return onUnsubscribe(msg);
}

void uMediaClient::dispatchCall(const std::string & method, const pbnjson::JValue & args) {
	// stash call for later use
	pthread_mutex_lock(&mutex);
	if (load_state < UMEDIA_CLIENT_MEDIAID_VALID) {
		message_queue.push_back({method, args});
	} else {
		invokeCall(method, args);
	}
	pthread_mutex_unlock(&mutex);
}

void uMediaClient::invokeCall(const std::string & method, const pbnjson::JValue & args,
							  bool (*cb)(UMSConnectorHandle*, UMSConnectorMessage*, void*)) {
	JGenerator serializer(NULL);
	std::string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(_log, MSGERR_JSON_SERIALIZE, "failure to serializer.toString()");
	} else {
		std::string cmd = m_umediaserver_connection_id + method;
		connection->sendMessage(cmd, payload_serialized, cb, this);
	}
}

JValue uMediaClient::marshallstring(const std::string& value) {
	JValue obj = value;
	return obj;
}

std::string uMediaClient::unmarshallstring(JValue value) {
	std::string unmarshalled;
	if (! value.isNull()) {
		unmarshalled = value.asString();
	}
	return unmarshalled;
}

JValue uMediaClient::marshallPayload(const std::string& value) {
	JDomParser parser;
	if (!parser.parse(value, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR(_log, MSGERR_JSON_PARSE, "failure to parse from %s", __FUNCTION__);
		return 0;
	}
	JValue parsed = parser.getDom();
	return parsed;
}

const char* uMediaClient::marshallAudioStreamClass(AudioStreamClass value) {
	switch(value) {
	case kFile:
		return "file";
	case kMedia:
		return "media";
	case kGapless:
		return "gapless";
	case kCamera:
		return "camera";
	case kAudioStreamRingtone:
		return "kAudioStreamRingtone";
	case kAudioStreamAlert:
		return "kAudioStreamAlert";
	case kAudioStreamMedia:
		return "kAudioStreamMedia";
	case kAudioStreamNotification:
		return "kAudioStreamNotification";
	case kAudioStreamFeedback:
		return "kAudioStreamFeedback";
	case kAudioStreamFlash:
		return "kAudioStreamFlash";
	case kAudioStreamNavigation:
		return "kAudioStreamNavigation";
	case kAudioStreamVoicedial:
		return "kAudioStreamVoicedial";
	case kAudioStreamVoip:
		return "kAudioStreamVoip";
	case kAudioStreamCalendar:
		return "kAudioStreamCalendar";
	case kAudioStreamAlarm:
		return "kAudioStreamAlarm";
	case kAudioStreamDefaultapp:
		return "kAudioStreamDefaultapp";
	case kAudioStreamVvm:
		return "kAudioStreamVvm";
	case kAudioStreamAlsa:
		return "kAudioStreamAlsa";
	case kAudioStreamFake:
		return "ref";
	case kAudioStreamNone:
		return "kAudioStreamNone";
	}
	return NULL;
}

AudioStreamClass uMediaClient::unmarshallAudioStreamClass(JValue value) {
	string s = value.asString();
	//AudioStreamClass e;
	if (s == "kAudioStreamRingtone") return kAudioStreamRingtone;
	if (s == "kAudioStreamAlert") return kAudioStreamAlert;
	if (s == "kAudioStreamMedia") return kAudioStreamMedia;
	if (s == "kAudioStreamNotification") return kAudioStreamNotification;
	if (s == "kAudioStreamFeedback") return kAudioStreamFeedback;
	if (s == "kAudioStreamFlash") return kAudioStreamFlash;
	if (s == "kAudioStreamNavigation") return kAudioStreamNavigation;
	if (s == "kAudioStreamVoicedial") return kAudioStreamVoicedial;
	if (s == "kAudioStreamVoip") return kAudioStreamVoip;
	if (s == "kAudioStreamCalendar") return kAudioStreamCalendar;
	if (s == "kAudioStreamAlarm") return kAudioStreamAlarm;
	if (s == "kAudioStreamDefaultapp") return kAudioStreamDefaultapp;
	if (s == "kAudioStreamVvm") return kAudioStreamVvm;
	if (s == "kAudioStreamAlsa") return kAudioStreamAlsa;
	if (s == "kAudioStreamFake") return kAudioStreamFake;
	if (s == "kAudioStreamNone") return kAudioStreamNone;
	if (s == "media") return kMedia;
	if (s == "gapless") return kGapless;
	if (s == "camera") return kCamera;
	return (AudioStreamClass)-1;
}

JValue uMediaClient::marshalllonglong(long long value) {
	JValue obj = (int64_t) value;
	return obj;
}

long long uMediaClient::unmarshalllonglong(JValue value) {
	int64_t unmarshalled = 0;
	if ((!value.isNull()) && (CONV_OK != value.asNumber(unmarshalled))) {
		LOG_ERROR(_log, MSGERR_JSON_UNMARSHALL, "failure to unmarshall.");
	}
	return (long long) unmarshalled;
}

JValue uMediaClient::marshallfloat(float value) {
	JValue obj =(double) value;
	return obj;
}

float uMediaClient::unmarshallfloat(JValue value) {
	double unmarshalled = 0.0;
	if ((!value.isNull()) && (CONV_OK != value.asNumber(unmarshalled))) {
		LOG_ERROR(_log, MSGERR_JSON_UNMARSHALL, "failure to unmarshall.");
	}
	return (float) unmarshalled;
}

JValue uMediaClient::marshalllong(long value) {
	JValue obj = (int32_t) value;
	return obj;
}

long uMediaClient::unmarshalllong(JValue value) {
	int32_t unmarshalled = 0;
	if ((!value.isNull()) && (CONV_OK != value.asNumber(unmarshalled))) {
		LOG_ERROR(_log, MSGERR_JSON_UNMARSHALL, "failure to unmarshall.");
	}
	return (long) unmarshalled;
}

JValue uMediaClient::marshallboolean(bool value) {
	JValue obj = value;
	return obj;
}

bool uMediaClient::unmarshallboolean(JValue value) {
	bool ret = false;
	if ((!value.isNull()) && (CONV_OK != value.asBool(ret))) {
		LOG_ERROR(_log, MSGERR_JSON_UNMARSHALL, "failure to unmarshall.");
	}
	return ret;
}

JValue uMediaClient::marshallnumber(int value) {
	JValue obj = (bool) value;
	return obj;
}

int uMediaClient::unmarshallnumber(JValue value) {
	int ret = false;
	if ((!value.isNull()) && (CONV_OK != value.asNumber(ret))) {
		LOG_ERROR(_log, MSGERR_JSON_UNMARSHALL, "failure to unmarshall.");
	}
	return ret;
}

JValue uMediaClient::marshallError(Error value) {
	switch(value) {
	case NO_ERROR:
		return "NO_ERROR";
	case INVALID_SOURCE_ERROR:
		return "INVALID_SOURCE_ERROR";
	case NETWORK_ERROR:
		return "NETWORK_ERROR";
	case FORMAT_ERROR:
		return "FORMAT_ERROR";
	case DECODE_ERROR:
		return "DECODE_ERROR";
	}
	return JValue();
}

Error uMediaClient::unmarshallError(JValue value) {
	string s = value.asString();
	//Error e;
	if (s == "NO_ERROR") return NO_ERROR;
	if (s == "INVALID_SOURCE_ERROR") return INVALID_SOURCE_ERROR;
	if (s == "NETWORK_ERROR") return NETWORK_ERROR;
	if (s == "FORMAT_ERROR") return FORMAT_ERROR;
	if (s == "DECODE_ERROR") return DECODE_ERROR;
	return (Error)-1;
}

JValue uMediaClient::marshallNetworkState(NetworkState value) {
	switch(value) {
	case NETWORK_EMPTY:
		return "NETWORK_EMPTY";
	case NETWORK_IDLE:
		return "NETWORK_IDLE";
	case NETWORK_LOADING:
		return "NETWORK_LOADING";
	case NETWORK_LOADED:
		return "NETWORK_LOADED";
	}
	return JValue();
}

NetworkState uMediaClient::unmarshallNetworkState(JValue value) {
	string s = value.asString();
	//NetworkState e;
	if (s == "NETWORK_EMPTY") return NETWORK_EMPTY;
	if (s == "NETWORK_IDLE") return NETWORK_IDLE;
	if (s == "NETWORK_LOADING") return NETWORK_LOADING;
	if (s == "NETWORK_LOADED") return NETWORK_LOADED;
	return (NetworkState)-1;
}

JValue uMediaClient::marshallReadyState(ReadyState value) {
	switch(value) {
	case HAVE_NOTHING:
		return "HAVE_NOTHING";
	case HAVE_METADATA:
		return "HAVE_METADATA";
	case HAVE_CURRENT_DATA:
		return "HAVE_CURRENT_DATA";
	case HAVE_FUTURE_DATA:
		return "HAVE_FUTURE_DATA";
	case HAVE_ENOUGH_DATA:
		return "HAVE_ENOUGH_DATA";
	}
	return JValue();
}

ReadyState uMediaClient::unmarshallReadyState(JValue value) {
	string s = value.asString();
	//ReadyState e;
	if (s == "HAVE_NOTHING") return HAVE_NOTHING;
	if (s == "HAVE_METADATA") return HAVE_METADATA;
	if (s == "HAVE_CURRENT_DATA") return HAVE_CURRENT_DATA;
	if (s == "HAVE_FUTURE_DATA") return HAVE_FUTURE_DATA;
	if (s == "HAVE_ENOUGH_DATA") return HAVE_ENOUGH_DATA;
	return (ReadyState)-1;
}

JValue uMediaClient::marshallVideoFitMode(VideoFitMode value) {
	switch(value) {
	case VIDEO_FIT:
		return "VIDEO_FIT";
	case VIDEO_FILL:
		return "VIDEO_FILL";
	}
	return JValue();
}

VideoFitMode uMediaClient::unmarshallVideoFitMode(JValue value) {
	string s = value.asString();
	//VideoFitMode e;
	if (s == "VIDEO_FIT") return VIDEO_FIT;
	if (s == "VIDEO_FILL") return VIDEO_FILL;
	return (VideoFitMode)-1;
}
