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
// uMediaServer API
//
// Listen Only clients may use MediaChangeListener independently
//

#ifndef __UMEDIA_CLIENT_H
#define __UMEDIA_CLIENT_H

#include <iostream>
#include <stdexcept>
#include <stdio.h>
#include <string>
#include <vector>

#include <errno.h>
#include <pthread.h>
#include <glib.h>
#include <pbnjson.hpp>
#include <UMSConnector.h>
#include <uMediaTypes.h>
#include <dto_types.h>

#if __cplusplus >= 199711L
#include <functional>
#define USE_CALLBACKS_API
#endif

#define UMEDIASERVER_CONNECTION_ID "com.webos.media"

class UMSConnector;
class UMSConnectorHandle;
class UMSConnectorMessage;

namespace uMediaServer {

typedef enum {
	UMEDIA_CLIENT_UNLOADED,
	UMEDIA_CLIENT_LOADING,
        UMEDIA_CLIENT_PRELOADING,
	UMEDIA_CLIENT_MEDIAID_VALID,
        UMEDIA_CLIENT_PRELOADED,
	UMEDIA_CLIENT_LOADED,
	UMEDIA_CLIENT_DETACHED
} umedia_client_load_state_t;

// create static dispatch method to allow object methods to be used as call backs for UMSConnector events
#define UMS_RESPONSE_HANDLER(_class_, _cb_, _member_) \
	static bool _cb_(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx) { \
	_class_ *self = static_cast<_class_ *>(ctx); \
	bool rv = self->_member_(handle, message,ctx);   \
	return rv;  } \
	bool _member_(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx);

// Data Class
//bufferRange structure
struct buffer_range_t {
	long int beginTime;
	long int endTime;
	long int remainingTime;
	long int percent;
};

//sourceInfo structure
struct audio_track_t {
	std::string language;
	std::string codec;
	std::string profile;
	std::string level;
	int bitRate;
	float sampleRate;
	int channels;
	int pid;
	int ctag;
	bool audioDescription;
	int audioType;
	int trackId;
	std::string role;
	int adaptationSetId;
};

struct video_track_t {
	int angleNumber;
	std::string codec;
	std::string profile;
	std::string level;
	int width;
	int height;
	std::string aspectRatio;
	std::string pixelAspectRatio;
	float frameRate;
	int bitRate;
	int pid;
	int ctag;
	bool progressive;
	int trackId;
	std::string role;
	int adaptationSetId;
	std::string orgCodec;
};

struct subtitle_track_t {
	std::string language;
	int pid;
	int ctag;
	int type;
	int compositionPageId;
	int ancilaryPageId;
	bool hearingImpared;
	int trackId;
};

struct program_info_t {
	long int duration;
	int parentControl;
	int numAudioTracks;
	std::vector<audio_track_t> audioTrackInfo;
	int numVideoTracks;
	std::vector<video_track_t> videoTrackInfo;
	int numSubtitleTracks;
	std::vector<subtitle_track_t> subtitleTrackInfo;
	std::string subtitleType;
};

struct downloadable_font_into_t {
	std::string descriptor;
	std::string url;
	std::string mimeType;
	std::string fontFamily;
};

struct source_info_t {
	std::string container;
	int numPrograms;
	bool seekable;
	bool trickable;
	std::vector<program_info_t> programInfo;
	int64_t startDate;
	int rotation;
	int numDownloadableFontInfos;
	std::vector<downloadable_font_into_t> downloadableFontInfo;
};

struct streaming_info_t {
	int instantBitrate;
	int totalBitrate;
};

struct sei_info_t {
	int displayPrimariesX0;
	int displayPrimariesX1;
	int displayPrimariesX2;
	int displayPrimariesY0;
	int displayPrimariesY1;
	int displayPrimariesY2;
	int whitePointX;
	int whitePointY;
	int minDisplayMasteringLuminance;
	int maxDisplayMasteringLuminance;
	int maxContentLightLevel;
	int maxPicAverageLightLevel;
};

struct vui_info_t {
	int transferCharacteristics;
	int colorPrimaries;
	int matrixCoeffs;
	bool videoFullRangeFlag;
};

struct video_info_t {
	int width;
	int height;
	std::string aspectRatio;
	std::string pixelAspectRatio;
	float frameRate;
	int bitRate;
	std::string mode3D;
	std::string actual3D;
	std::string scanType;
	bool isValidSeiInfo;
	sei_info_t SEI;
	bool isValidVuiInfo;
	vui_info_t VUI;
	std::string hdrType;
	int afd;
};

struct spectrum_info_t {
	int frequency;
	float magnitude;
	float phase;
};

struct audio_info_t {
	bool dualMono;
	int track;
	std::string immersive;
	float sampleRate;
	int channels;
	int band;
	std::vector<spectrum_info_t> spectrum;
};

struct record_info_t {
	bool recordState;
	int elapsedMiliSecond;
	int bitRate;
	int fileSize;
	int fps;
};

struct track_info_t{
	std::string description;
};

struct external_subtitle_track_info_t{
	std::string uri;
	int numSubtitleTracks;
	std::vector<track_info_t> tracks;
	std::string mediaId;
	std::string hitEncoding;
};

struct seekable_range_t {
	int64_t start;
	int64_t end;
};

struct seekable_ranges_info_t {
	int numRanges;
	std::vector<seekable_range_t> seekableRanges;
};

struct master_clock_info_t {
	bool result;
	int port;
	int64_t baseTime;
};

struct slave_clock_info_t {
	bool result;
};

struct subtitle_data_info_t {
	std::string subtitleData;
	uint64_t presentationTime;
};

// controller + change listener (subscription messages)
class uMediaClient {
public :

#ifdef USE_CALLBACKS_API
	typedef std::function<void()> generic_callback_t;
	typedef std::function<void(bool)> flag_callback_t;
	typedef std::function<void(const ums::error_t &)> error_callback_t;
#if UMS_INTERNAL_API_VERSION == 2
	typedef std::function<void(const ums::source_info_t &)> source_info_callback_t;
	typedef std::function<void(const ums::time_t &)> time_update_callback_t;
	typedef std::function<void(const ums::video_info_t &)> video_info_callback_t;
	typedef std::function<void(const ums::audio_info_t &)> audio_info_callback_t;
#else
	typedef std::function<void(const source_info_t &)> source_info_callback_t;
	typedef std::function<void(const long long &)> time_update_callback_t;
	typedef std::function<void(const video_info_t &)> video_info_callback_t;
	typedef std::function<void(const audio_info_t &)> audio_info_callback_t;
#endif // UMS_INTERNAL_API_VERSION == 2

	void set_load_completed_callback(generic_callback_t && handler) {
		_load_completed_callback = handler;
	}
	void set_unload_completed_callback(generic_callback_t && handler) {
		_unload_completed_callback = handler;
	}
	void set_playing_callback(generic_callback_t && handler) {
		_playing_callback = handler;
	}
	void set_paused_callback(generic_callback_t && handler) {
		_paused_callback = handler;
	}
	void set_eos_callback(generic_callback_t && handler) {
		_eos_callback = handler;
	}
	void set_focus_callback(flag_callback_t && handler) {
		_focus_callback = handler;
	}
	void set_source_info_callback(source_info_callback_t && handler) {
		_source_info_callback = handler;
	}
	void set_stream_time_callback(time_update_callback_t && handler) {
		_stream_time_callback = handler;
	}
	void set_video_info_callback(video_info_callback_t && handler) {
		_video_info_callback = handler;
	}
	void set_audio_info_callback(audio_info_callback_t && handler) {
		_audio_info_callback = handler;
	}
	void set_error_callback(error_callback_t && handler) {
		_error_callback = handler;
	}

#endif // USE_CALLBACKS_API


	uMediaClient(bool rawEvents = false, UMSConnectorBusType bus = UMS_CONNECTOR_PUBLIC_BUS);
	virtual ~ uMediaClient();

	// virtual methods:  Override messages/events as needed. See test.cpp for example.
	virtual bool onLoadCompleted() { return true; }
        virtual bool onPreloadCompleted() { return true; }
	virtual bool onUnloadCompleted() { return true; }
	virtual bool onDetached() { return true; }
	virtual bool onCurrentTime(long long currentTime) { return true; }
	virtual bool onEndOfStream() { return true; }
	virtual bool onPaused() { return true; }
	virtual bool onPlaying() { return true; }
	virtual bool onSeekDone() { return true; }
	virtual bool onBufferingStart() { return true; }
	virtual bool onBufferingEnd() { return true; }
	virtual bool onTrackSelected(const std::string &type, int index) { return true; }
	virtual bool onBufferRange(const buffer_range_t &bufferRange) { return true; }
	virtual bool onVideoFrame() { return true; }
#if UMS_INTERNAL_API_VERSION == 2
	virtual bool onSourceInfo(const ums::source_info_t &sourceInfo) { return true; }
	virtual bool onVideoInfo(const ums::video_info_t &videoInfo) { return true; }
	virtual bool onAudioInfo(const ums::audio_info_t &audioInfo) { return true; }
#else
	virtual bool onSourceInfo(const source_info_t &sourceInfo) { return true; }
	virtual bool onVideoInfo(const video_info_t &videoInfo) { return true; }
	virtual bool onAudioInfo(const audio_info_t &audioInfo) { return true; }
#endif // UMS_INTERNAL_API_VERSION == 2
	virtual bool onStreamingInfo(const streaming_info_t &streamingInfo) { return true; }
	virtual bool onActiveRegion(const rect_t &activeRegion) { return true; }
	virtual bool onError(long long onPlayingerrorCode, const std::string &errorText) { return true; }
	virtual bool onSnapshotDone() { return true; }
	virtual bool onFileGenerated() { return true; }
	virtual bool onRecordInfo(const record_info_t &recordInfo) { return true; }
	virtual bool onExternalSubtitleTrackInfo(const external_subtitle_track_info_t&) {return true;}
	virtual bool onActivePipelines(const std::string &active_pipelines) {return true;}
	virtual bool onFocusChanged(bool focus) { return true; }
	virtual bool onSeekableRanges(const seekable_ranges_info_t& rangesInfo) { return true; }
	virtual bool onSetMasterClockResult(const master_clock_info_t& masterClockInfo) { return true; }
	virtual bool onSetSlaveClockResult(const slave_clock_info_t& slaveClockInfo) { return true; }
	virtual bool onSubtitleData(const subtitle_data_info_t& subtitleDataInfo) { return true; }
	virtual bool onVsmResourceInfo(const std::string& vsmResourceInfo) { return true; }
	virtual bool onValidData(bool state) { return true; }

	// media pipelines can return any state. Untracked/user defined data
	virtual bool onUserDefinedChanged(const char * message) { return true; }

	// API
	bool loadAsync(const std::string& uri, const std::string& type, const std::string& payload = std::string());
	bool load(const std::string& uri, const std::string& type, const std::string& payload = std::string());
	bool loadAsync(std::string uri,AudioStreamClass audioClass, std::string payload=std::string());
	bool load(std::string uri,AudioStreamClass audioClass, std::string payload=std::string());
        bool preload(const std::string& uri, const std::string& type, const std::string& payload = std::string());
        bool preload(std::string uri,AudioStreamClass audioClass, std::string payload=std::string());
	bool attach(const std::string& mediaId);
	bool play();
	bool pause();
	bool seek(long long position);
	bool unload();
	bool notifyForeground();
	bool notifyBackground();
	bool setPlayRate(double rate, bool audioOutput=true);
	bool setUri(std::string& uri, const std::string& option = std::string());
	bool selectTrack(std::string& type, int32_t index);
	bool setSubtitleSource(std::string& uri, std::string preferredEncodings=std::string());
	bool setSubtitleEnable(bool enable);
	bool setSubtitlePosition(int32_t position);
	bool setSubtitleSync(int32_t sync);
	bool setSubtitleFontSize(int32_t fontSize);
	bool setSubtitleColor(int32_t color);
	bool setSubtitleEncoding(std::string& encoding);
	bool setSubtitlePresentationMode(std::string& presentationMode);
	bool setSubtitleCharacterColor(std::string& charColor);
	bool setSubtitleCharacterOpacity(int32_t charOpacity);
	bool setSubtitleCharacterFontSize(std::string& charFontSize);
	bool setSubtitleCharacterFont(std::string& charFont);
	bool setSubtitleBackgroundColor(std::string& bgColor);
	bool setSubtitleBackgroundOpacity(int32_t bgOpacity);
	bool setSubtitleCharacterEdge(std::string& charEdgeType);
	bool setSubtitleWindowColor(std::string& windowColor);
	bool setSubtitleWindowOpacity(int32_t windowOpacity);
	bool setUpdateInterval(int32_t currentTimeInterval, int32_t bufferRangeInterval);
	bool setUpdateInterval(std::string &key, int32_t value);
	bool takeSnapshot(std::string& location, std::string& format,
					int32_t width, int32_t height, int32_t pictureQuality);
	bool startRecord(std::string& location,  std::string& format);
	bool stopRecord();
	bool changeResolution(int32_t width, int32_t height);
	bool setStreamQuality(int32_t width, int32_t height, int32_t bitRate, bool init);
	bool setDescriptiveVideoService(bool enable);

	bool setVolume(const int32_t volume);
	bool setVolume(const int32_t volume, const int32_t duration);
	bool setVolume(const int32_t volume, const int32_t duration, const std::string type);
	bool setVolume(const int32_t volume, const int32_t duration, const EaseType type);

	// MDC API extension
	bool setDisplayWindow(const rect_t & output_rect);
	bool setDisplayWindow(const rect_t & source_rect, const rect_t & output_rect);
	bool switchToFullscreen();
	bool switchToAutoLayout();

	bool focus() const;
	bool setFocus();

	bool visibility() const;
	bool setVisibility(bool visibility);

	bool setMasterClock(const std::string& ip = std::string("0.0.0.0"), int32_t port = 5637);
	bool setSlaveClock(const std::string& ip, int32_t port, int64_t baseTime);
	bool setAudioDualMono(int audioMode = 0);

	std::string getMediaId() { return media_id; }

	bool setProperty(std::string& payload);

	bool getPipelineState();
	bool logPipelineState();

	bool setPipelineDebugState(std::string& debug_state);

	void run();
	void stop();

protected :
	UMSConnector *connection;
	std::string media_id;       // passed back from uMediaServer load command

	pbnjson::JValue marshallstring(const std::string& a);
	std::string unmarshallstring(pbnjson::JValue value);
	pbnjson::JValue marshallPayload(const std::string& a);
	const char* marshallAudioStreamClass(AudioStreamClass a);
	AudioStreamClass unmarshallAudioStreamClass(pbnjson::JValue value);
	pbnjson::JValue marshallfloat(float a);
	float unmarshallfloat(pbnjson::JValue value);
	pbnjson::JValue marshalllonglong(long long value);
	long long unmarshalllonglong(pbnjson::JValue value);
	pbnjson::JValue marshalllong(long a);
	long unmarshalllong(pbnjson::JValue value);
	pbnjson::JValue marshallboolean(bool a);
	bool unmarshallboolean(pbnjson::JValue value);
	pbnjson::JValue marshallnumber(int a);
	int unmarshallnumber(pbnjson::JValue value);

	pbnjson::JValue marshallError(Error a);
	Error unmarshallError(pbnjson::JValue value);
	pbnjson::JValue marshallNetworkState(NetworkState a);
	NetworkState unmarshallNetworkState(pbnjson::JValue value);
	pbnjson::JValue marshallReadyState(ReadyState a);
	ReadyState unmarshallReadyState(pbnjson::JValue value);
	pbnjson::JValue marshallVideoFitMode(VideoFitMode a);
	VideoFitMode unmarshallVideoFitMode(pbnjson::JValue value);

	bool getStateData(const std::string & message, std::string &name, pbnjson::JValue &value);

private :
	Logger log;
	UMSConnectorBusType bus;

	std::string process_connection_id;
	umedia_client_load_state_t load_state;

	bool visible;
	bool _focus;

	std::string m_umediaserver_connection_id;

	GMainLoop * gmain_loop;
	GMainContext * context;

	bool rawEventsFlag; // set means deliver only unparsed raw events to the client
	pthread_cond_t load_state_cond;
	pthread_mutex_t mutex;

	std::vector<std::pair<std::string, pbnjson::JValue> > message_queue;
	void dispatchCall(const std::string & method, const pbnjson::JValue & args);
	void invokeCall(const std::string & method, const pbnjson::JValue & args,
					bool (*cb)(UMSConnectorHandle*, UMSConnectorMessage*, void*) = commandResponseCallback);

	void subscribe();
	void unsubscribe();

	// handle subscribed events
	UMS_RESPONSE_HANDLER(uMediaClient,subscribeCallback,stateChange);

	// specifically handle load response to obtain media_id
	UMS_RESPONSE_HANDLER(uMediaClient,loadResponseCallback,loadResponse);

        // specifically handle preload response to obtain media_id
        UMS_RESPONSE_HANDLER(uMediaClient,preloadResponseCallback,preloadResponse);

	// specifically handle attach response to start subscribing
	UMS_RESPONSE_HANDLER(uMediaClient,attachResponseCallback,attachResponse);

	// handle other command responses
	UMS_RESPONSE_HANDLER(uMediaClient,commandResponseCallback,commandResponse);


	pthread_t message_thread;
	static void * messageThread(void *arg);

#ifdef USE_CALLBACKS_API
	generic_callback_t		_load_completed_callback;
	generic_callback_t		_unload_completed_callback;
	generic_callback_t		_playing_callback;
	generic_callback_t		_paused_callback;
	generic_callback_t		_eos_callback;
	flag_callback_t			_focus_callback;
	source_info_callback_t	_source_info_callback;
	time_update_callback_t	_stream_time_callback;
	video_info_callback_t	_video_info_callback;
	audio_info_callback_t _audio_info_callback;
	error_callback_t		_error_callback;
#endif

};
} // namespace uMediaServer

#endif  // __UMEDIA_CLIENT_H
