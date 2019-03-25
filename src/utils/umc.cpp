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
// test client using uMediaServer API
//
// Listen Only clients may use MediaChangeListener independently
//

#include <stdexcept>
#include <vector>
#include <sstream>
#include <stdio.h>
#include <cassert>
#include <string>
#include <algorithm>
#include <iterator>

#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <deque>
#include <list>

#include <pthread.h>
#include <pbnjson.hpp>
#include "uMediaClient.h"

#include <boost/lexical_cast.hpp>  // for lexical_cast<string>(number)
#include <boost/algorithm/string.hpp>

using namespace std;
using namespace boost;
using namespace uMediaServer;

typedef enum load_state { UNLOADED, LOADED } load_state_t;

//
// create media player client
//
class MediaPlayer : public uMediaClient {
	int buffer_range_reported;
	int total_bitrate_reported;
	int instant_bitrate_reported;
public:
	MediaPlayer() :
		state(UNLOADED),
		uMediaClient(false, UMS_CONNECTOR_PRIVATE_BUS),
		buffer_range_reported(0),
		total_bitrate_reported(0),
		instant_bitrate_reported(0) {}

	// override currentTimeEvent virtual method
	bool onCurrentTime(int64_t currentTime)	{
		//cout << "onCurrentTime =" << currentTime << endl;
		return true;
	}

	// override eosChanged virtual method
	bool onEndOfStream()	{
		cout << "onEndOfStream received" << endl;
		return true;
	}

	// override error virtual method
	bool onError(int64_t errorCode, const std::string &errorText)	{
		cout << "onError Code=" << errorCode << endl;
		cout << "onError Text=" << errorText << endl;
		unload();
		return true;
	}

	bool onSourceInfo(const source_info_t &sourceInfo)	{
		cout << "onSourceInfo.container = " 		<< sourceInfo.container << endl;
		cout << "onSourceInfo.numPrograms = " 	<< sourceInfo.numPrograms << endl;
		cout << "onSourceInfo.seekable = " 	<< sourceInfo.seekable << endl;
		cout << "onSourceInfo.trickable = " 	<< sourceInfo.trickable << endl;
		return true;
	}

	bool onStreamingInfo(const streaming_info_t &streamingInfo)	{
		if (total_bitrate_reported != streamingInfo.totalBitrate) {
			cout << "onStreamingInfo.totalBitrate = " 	<< streamingInfo.totalBitrate << endl;
			total_bitrate_reported = streamingInfo.totalBitrate;
		}
		if (instant_bitrate_reported != streamingInfo.instantBitrate) {
			cout << "onStreamingInfo.instantBitrate = " 	<< streamingInfo.instantBitrate << endl;
			instant_bitrate_reported = streamingInfo.instantBitrate;
		}
		return true;
	}

	bool onVideoInfo(const video_info_t &videoInfo)	{
		cout << "onVideoInfo.width = "            << videoInfo.width << endl;
		cout << "onVideoInfo.height = "           << videoInfo.height << endl;
		cout << "onVideoInfo.aspectRatio = "      << videoInfo.aspectRatio << endl;
		cout << "onVideoInfo.pixelAspectRatio = " << videoInfo.pixelAspectRatio << endl;
		cout << "onVideoInfo.frameRate = "        << videoInfo.frameRate << endl;
		cout << "onVideoInfo.mode3D = "           << videoInfo.mode3D << endl;
		cout << "onVideoInfo.actual3D = "         << videoInfo.actual3D << endl;
		cout << "onVideoInfo.scanType = "         << videoInfo.scanType << endl;

		if (videoInfo.isValidSeiInfo) {
			cout << "onVideoInfo.SEI.displayPrimariesX0 = "<< videoInfo.SEI.displayPrimariesX0 << endl;
			cout << "onVideoInfo.SEI.displayPrimariesX1 = "<< videoInfo.SEI.displayPrimariesX1 << endl;
			cout << "onVideoInfo.SEI.displayPrimariesX2 = "<< videoInfo.SEI.displayPrimariesX2 << endl;
			cout << "onVideoInfo.SEI.displayPrimariesY0 = "<< videoInfo.SEI.displayPrimariesY0 << endl;
			cout << "onVideoInfo.SEI.displayPrimariesY1 = "<< videoInfo.SEI.displayPrimariesY1 << endl;
			cout << "onVideoInfo.SEI.displayPrimariesY2 = "<< videoInfo.SEI.displayPrimariesY2 << endl;
			cout << "onVideoInfo.SEI.whitePointX = "<< videoInfo.SEI.whitePointX << endl;
			cout << "onVideoInfo.SEI.whitePointY = "<< videoInfo.SEI.whitePointY << endl;
			cout << "onVideoInfo.SEI.minDisplayMasteringLuminance = "<< videoInfo.SEI.minDisplayMasteringLuminance << endl;
			cout << "onVideoInfo.SEI.maxDisplayMasteringLuminance = "<< videoInfo.SEI.maxDisplayMasteringLuminance << endl;
			cout << "onVideoInfo.SEI.maxContentLightLevel = "<< videoInfo.SEI.maxContentLightLevel << endl;
			cout << "onVideoInfo.SEI.maxPicAverageLightLevel = "<< videoInfo.SEI.maxPicAverageLightLevel << endl;
		}

		if (videoInfo.isValidVuiInfo) {
			cout << "onVideoInfo.VUI.transferCharacteristics = "<< videoInfo.VUI.transferCharacteristics << endl;
			cout << "onVideoInfo.VUI.colorPrimaries =" << videoInfo.VUI.colorPrimaries << endl;
			cout << "onVideoInfo.VUI.matrixCoeffs = "<< videoInfo.VUI.matrixCoeffs << endl;
			cout << "onVideoInfo.VUI.videoFullRangeFlag = " << videoInfo.VUI.videoFullRangeFlag << endl;
		}

		return true;
	}

	bool onAudioInfo(const audio_info_t &audioInfo)	{
		cout << "onAudioInfo.channels = " 	<< audioInfo.channels << endl;
		cout << "onAudioInfo.sampleRate = " 	<< audioInfo.sampleRate << endl;
		cout << "onAudioInfo.dualMono = " 	<< audioInfo.dualMono << endl;
		cout << "onAudioInfo.track = " 		<< audioInfo.track << endl;
		cout << "onAudioInfo.immersive = " 	<< audioInfo.immersive << endl;
		return true;
	}
	bool onTrackSelected(const std::string &type, int index) {
		cout << "onTrackSelected type = " << type << endl;
		cout << "onTrackSelected index = " << index << endl;
		return true;
	}

	bool onBufferRange(const buffer_range_t &bufferRange)	{
		if (bufferRange.percent != buffer_range_reported) {
			cout << "onBufferRange = " << bufferRange.percent << endl;
			buffer_range_reported = bufferRange.percent;
		}
		return true;
	}

	bool onBufferingStart()	{
		cout << "onBufferingStart received" << endl;
		return true;
	}

	bool onBufferingEnd()	{
		cout << "onBufferingEnd received" << endl;
		return true;
	}

	bool onVideoFrame()	{
		cout << "onVideoFrame received" << endl;
		return true;
	}

	bool onLoadCompleted() {
		cout << "\033[1;32monLoadCompleted\033[0m" << endl;
		state = LOADED;
		return true;
	}

	/* TODO:  Remove
	bool onPreloadCompleted() {
		cout << "\033[1;32monPreloadCompleted\033[0m" << endl;
		state = PRELOADED;
		return true;
	}
	*/

	bool onDetached() {
		cout << "\033[1;32monDetached\033[0m" << endl;
		state = UNLOADED;
		return true;
	}

	bool onUnloadCompleted() {
		cout << "\033[1;31monUnloadCompleted\033[0m" << endl;
		state = UNLOADED;
		return true;
	}

	bool onPaused() {
		cout << "onPaused" << endl;
		return true;
	}

	bool onSeekDone() {
		cout << "onSeekDone" << endl;
		return true;
	}

	bool onPlaying() {
		cout << "onPlaying" << endl;
		return true;
	}

	bool onSnapshotDone() {
		cout << "onSnapshotDone" << endl;
		return true;
	}

	bool onFileGenerated() {
		cout << "onFileGenerated" << endl;
		return true;
	}

	bool onRecordInfo(const record_info_t &recordInfo) {
		cout << "onRecordInfo.recordState = " 	<< recordInfo.recordState << endl;
		cout << "onRecordInfo.elapsedMiliSecond = " 	<< recordInfo.elapsedMiliSecond << endl;
		cout << "onRecordInfo.bitRate = " 	<< recordInfo.bitRate << endl;
		cout << "onRecordInfo.fileSize = " 	<< recordInfo.fileSize << endl;
		cout << "onRecordInfo.fps = " 	<< recordInfo.fps << endl;
		return true;
	}

	bool onSeekableRanges(const seekable_ranges_info_t& rangesInfo) {
		for (int i = 0; i < rangesInfo.numRanges; i++) {
			cout << "onSeekableRanges.seekableRanges[" << i << "].start = " << rangesInfo.seekableRanges[i].start << endl;
			cout << "onSeekableRanges.seekableRanges[" << i << "].end = " << rangesInfo.seekableRanges[i].end << endl;
		}
		return true;
	}

	bool onSetMasterClockResult(const master_clock_info_t& masterClockInfo) {
		cout << "onSetMasterClockResult.result = " 	<< masterClockInfo.result << endl;
		cout << "onSetMasterClockResult.port = "  << masterClockInfo.port << endl;
		cout << "onSetMasterClockResult.baseTime = " 	<< masterClockInfo.baseTime << endl;
		return true;
	}

	bool onSetSlaveClockResult(const slave_clock_info_t& slaveClockInfo) {
		cout << "onSetMasterClockResult.result = " 	<< slaveClockInfo.result << endl;
		return true;
	}

	bool onSubtitleData(const subtitle_data_info_t& subtitleDataInfo) {
		cout << "onSubtitleData.subtitleData = " 	<< subtitleDataInfo.subtitleData << endl;
		cout << "onSubtitleData.presentationTime = " << subtitleDataInfo.presentationTime << endl;
		return true;
	}

	bool onValidData(bool state) {
		cout << "onValidData.state = " << std::boolalpha << state << std::noboolalpha << endl;
		return true;
	}

	bool onSubscribe(const char * message) {
		cout << "onSubscribe result : " << message << endl;
		return true;
	}

	bool onUnsubscribe(const char * message) {
		cout << "onunSubscribe result : " << message << endl;
		return true;

	}

	bool onUserDefinedChanged(const char * message)	{
		//cout << "onUserDefinedChanged " << message << endl;

		std::string name;
		pbnjson::JValue value = pbnjson::Object();
		bool rv = getStateData(message,name,value);

		if (name == "currentTime") {
			if (!value.hasKey("currentTime"))
				return false;
			long long currentTime = unmarshalllonglong(value["currentTime"]);

			cout << "onUserDefinedChanged: currentTime ="	<< currentTime << endl;
		}
		else if (name == "sourceInfo") {
			source_info_t sourceInfo = {};
			sourceInfo.container = unmarshallstring(value["container"]);
			sourceInfo.numPrograms = unmarshalllong(value["numPrograms"]);
			sourceInfo.seekable= unmarshallboolean(value["seekable"]);
			sourceInfo.trickable= unmarshallboolean(value["trickable"]);
			pbnjson::JValue programInfoArray = value["programInfo"];

			if (programInfoArray.arraySize() == sourceInfo.numPrograms) {
				for (int i = 0; i < sourceInfo.numPrograms; i++) {
					program_info_t programInfo = {};

					pbnjson::JValue programInfoValue = programInfoArray[i];
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
							subtitleTrackInfo.trackId 	= unmarshallboolean(programInfoValue["subtitleTrackInfo"][j]["trackId"]);
							programInfo.subtitleTrackInfo.push_back(subtitleTrackInfo);
						}
					}

					sourceInfo.programInfo.push_back(programInfo);
				}
			}
			else {
				cout << "numprograms differs from the number of real program info structure" << endl;
				return false;
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

			cout << "onUserDefinedChanged: sourceInfo.container = "		<< sourceInfo.container << endl;
			cout << "onUserDefinedChanged: sourceInfo.numPrograms = "	<< sourceInfo.numPrograms << endl;
			cout << "onUserDefinedChanged: sourceInfo.seekable = "		<< sourceInfo.seekable << endl;
			cout << "onUserDefinedChanged: sourceInfo.trickable = "		<< sourceInfo.trickable << endl;
		}
		else if (name == "streamingInfo") {
			streaming_info_t streamingInfo = {};
			streamingInfo.totalBitrate = unmarshalllong(value["totalBitrate"]);
			streamingInfo.instantBitrate = unmarshalllong(value["instantBitrate"]);

			cout << "onUserDefinedChanged: streamingInfo.totalBitrate = "       << streamingInfo.totalBitrate << endl;
			cout << "onUserDefinedChanged: streamingInfo.instantBitrate = "     << streamingInfo.instantBitrate << endl;
		}
		else if (name == "videoInfo") {
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
			pbnjson::JValue seiInfoValue = value["SEI"];
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

			pbnjson::JValue vuiInfoValue = value["VUI"];
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

			cout << "onUserDefinedChanged: videoInfo.width = "				<< videoInfo.width << endl;
			cout << "onUserDefinedChanged: videoInfo.height = "				<< videoInfo.height << endl;
			cout << "onUserDefinedChanged: videoInfo.aspectRatio = "		<< videoInfo.aspectRatio << endl;
			cout << "onUserDefinedChanged: videoInfo.pixelAspectRatio = "	<< videoInfo.pixelAspectRatio << endl;
			cout << "onUserDefinedChanged: videoInfo.frameRate = "			<< videoInfo.frameRate << endl;
			cout << "onUserDefinedChanged: videoInfo.mode3D = "				<< videoInfo.mode3D << endl;
			cout << "onUserDefinedChanged: videoInfo.actual3D = "			<< videoInfo.actual3D << endl;
			cout << "onUserDefinedChanged: videoInfo.scanType = "			<< videoInfo.scanType << endl;

			if (videoInfo.isValidSeiInfo) {
				cout << "onUserDefinedChanged: videoInfo.SEI.displayPrimariesX0 = "<< videoInfo.SEI.displayPrimariesX0 << endl;
				cout << "onUserDefinedChanged: videoInfo.SEI.displayPrimariesX1 = "<< videoInfo.SEI.displayPrimariesX1 << endl;
				cout << "onUserDefinedChanged: videoInfo.SEI.displayPrimariesX2 = "<< videoInfo.SEI.displayPrimariesX2 << endl;
				cout << "onUserDefinedChanged: videoInfo.SEI.displayPrimariesY0 = "<< videoInfo.SEI.displayPrimariesY0 << endl;
				cout << "onUserDefinedChanged: videoInfo.SEI.displayPrimariesY1 = "<< videoInfo.SEI.displayPrimariesY1 << endl;
				cout << "onUserDefinedChanged: videoInfo.SEI.displayPrimariesY2 = "<< videoInfo.SEI.displayPrimariesY2 << endl;
				cout << "onUserDefinedChanged: videoInfo.SEI.whitePointX = "<< videoInfo.SEI.whitePointX << endl;
				cout << "onUserDefinedChanged: videoInfo.SEI.whitePointY = "<< videoInfo.SEI.whitePointY << endl;
				cout << "onUserDefinedChanged: videoInfo.SEI.minDisplayMasteringLuminance = "<< videoInfo.SEI.minDisplayMasteringLuminance << endl;
				cout << "onUserDefinedChanged: videoInfo.SEI.maxDisplayMasteringLuminance = "<< videoInfo.SEI.maxDisplayMasteringLuminance << endl;
				cout << "onUserDefinedChanged: videoInfo.SEI.maxContentLightLevel = "<< videoInfo.SEI.maxContentLightLevel << endl;
				cout << "onUserDefinedChanged: videoInfo.SEI.maxPicAverageLightLevel = "<< videoInfo.SEI.maxPicAverageLightLevel << endl;
			}

			if (videoInfo.isValidVuiInfo) {
				cout << "onUserDefinedChanged: videoInfo.VUI.transferCharacteristics = "<< videoInfo.VUI.transferCharacteristics << endl;
				cout << "onUserDefinedChanged: videoInfo.VUI.colorPrimaries =" << videoInfo.VUI.colorPrimaries << endl;
				cout << "onUserDefinedChanged: videoInfo.VUI.matrixCoeffs = "<< videoInfo.VUI.matrixCoeffs << endl;
				cout << "onUserDefinedChanged: videoInfo.VUI.videoFullRangeFlag = " << videoInfo.VUI.videoFullRangeFlag << endl;
			}

			cout << "onUserDefinedChanged: hdrType = " << videoInfo.hdrType << endl;

		}
		else if (name == "audioInfo") {
			audio_info_t audioInfo = {};
			audioInfo.channels = unmarshalllong(value["channels"]);
			audioInfo.sampleRate = unmarshallfloat(value["sampleRate"]);
			audioInfo.dualMono = unmarshallboolean(value["dualMono"]);
			audioInfo.track = unmarshalllong(value["track"]);
			audioInfo.immersive = unmarshallstring(value["immersive"]);

			cout << "onUserDefinedChanged: audioInfo.channels = "		<< audioInfo.channels << endl;
			cout << "onUserDefinedChanged: audioInfo.sampleRate = "		<< audioInfo.sampleRate << endl;
			cout << "onUserDefinedChagned: audioInfo.dualMono = " 		<< audioInfo.dualMono << endl;
			cout << "onUserDefinedChanged: audioInfo.track = " 		<< audioInfo.track << endl;
			cout << "onUserDefinedChanged: audioInfo.immersive = " 	<< audioInfo.immersive << endl;
		}
		else if (name == "trackSelected") {
			const std::string& type = unmarshallstring(value["type"]);
			int index = unmarshalllong(value["index"]);

			cout << "onUserDefinedChanged: trackSelected type = " << type << endl;
			cout << "onUserDefinedChanged: trackSelected index = " << index << endl;
		}
		else if (name == "endOfStream") {
			cout << "onUserDefinedChanged: endOfStream received" << endl;
		}
		else if (name == "paused") {
			cout << "onUserDefinedChanged: paused" << endl;
		}
		else if (name == "playing") {
			cout << "onUserDefinedChanged: playing" << endl;
		}
		else if (name == "seekDone") {
			cout << "onUserDefinedChanged: seekDone" << endl;
		}
		else if (name == "bufferRange") {
			buffer_range_t bufferRange = {};
			bufferRange.beginTime = unmarshalllonglong(value["beginTime"]);
			bufferRange.endTime = unmarshalllonglong(value["endTime"]);
			bufferRange.remainingTime = unmarshalllonglong(value["remainingTime"]);
			bufferRange.percent = unmarshalllonglong(value["percent"]);

			cout << "onUserDefinedChanged: bufferRange = " << bufferRange.percent << endl;
		}
		else if (name == "bufferingStart") {
			cout << "onUserDefinedChanged: bufferingStart" << endl;
		}
		else if (name == "bufferingEnd") {
			cout << "onUserDefinedChanged: bufferingEnd" << endl;
		}
		else if (name == "videoFrame") {
			cout << "onVideoFrame received" << endl;
		}
		else if (name == "loadCompleted") {
			cout << "\033[1;32monLoadCompleted\033[0m" << endl;
			state = LOADED;
		}
		else if (name == "unloadCompleted") {
			cout << "\033[1;31monUserDefinedChanged: unloadCompleted\033[0m" << endl;
			state = UNLOADED;
		}
		/* TODO : Remove
		else if (name == "preloadCompleted") {
			cout << "\033[1;32monUserDefinedChanged: preloadCompleted\033[0m" << endl;
			state = PRELOADED;
		}
		*/
		else if (name == "detached") {
			cout << "\033[1;32monUserDefinedChanged: detached\033[0m" << endl;
			state = UNLOADED;
		}
		else if (name == "snapshotDone") {
			cout << "onUserDefinedChanged: snapshotDone" << endl;
		}
		else if (name == "fileGenerated") {
			cout << "onUserDefinedChanged: fileGenerated" << endl;
		}
		else if (name == "recordInfo") {
			record_info_t recordInfo = {};
			recordInfo.recordState = unmarshallboolean(value["recordState"]);
			recordInfo.elapsedMiliSecond = unmarshalllong(value["elapsedMiliSecond"]);
			recordInfo.bitRate = unmarshalllong(value["bitRate"]);
			recordInfo.fileSize = unmarshalllong(value["fileSize"]);
			recordInfo.fps = unmarshalllong(value["fps"]);

			cout << "onUserDefinedChanged: recordInfo.recordState = "   << recordInfo.recordState << endl;
			cout << "onUserDefinedChanged: recordInfo.elapsedMiliSecond = "     << recordInfo.elapsedMiliSecond << endl;
			cout << "onUserDefinedChanged: recordInfo.bitRate = "       << recordInfo.bitRate << endl;
			cout << "onUserDefinedChanged: recordInfo.fileSize = "      << recordInfo.fileSize << endl;
			cout << "onUserDefinedChanged: recordInfo.fps = "   << recordInfo.fps << endl;
		}
		else if (name == "error") {
			long long errorCode = unmarshalllonglong(value["errorCode"]);
			const std::string& errorText = unmarshallstring(value["errorText"]);

			cout << "onUserDefinedChanged: error Code=" << errorCode << endl;
			cout << "onUserDefinedChanged: error Text=" << errorText << endl;
			unload();
		}
		else if (name == "externalSubtitleTrackInfo"){
			external_subtitle_track_info_t extsubtrackInfo = {};
			extsubtrackInfo.uri=unmarshallstring(value["uri"]);
			extsubtrackInfo.numSubtitleTracks=unmarshalllong(value["numSubtitleTracks"]);
			pbnjson::JValue tracksInfoArray = value["tracks"];
			if (tracksInfoArray.arraySize() == extsubtrackInfo.numSubtitleTracks) {
				for (int i = 0; i < extsubtrackInfo.numSubtitleTracks; i++) {
					track_info_t trackInfo = {};

					pbnjson::JValue trackInfoValue = tracksInfoArray[i];
					trackInfo.description = unmarshallstring(trackInfoValue["description"]);
					extsubtrackInfo.tracks.push_back(trackInfo);
				}
			}
			extsubtrackInfo.hitEncoding=unmarshallstring(value["hitEncoding"]);

			cout << "onUserDefinedChanged: externalSubtitleTrackInfo.uri = " << extsubtrackInfo.uri << endl;
			cout << "onUserDefinedChanged: externalSubtitleTrackInfo.numSubtitleTracks = " << extsubtrackInfo.numSubtitleTracks << endl;
		}
		else if (name == "seekableRanges") {
			seekable_ranges_info_t seekableRangesInfo = {};
			pbnjson::JValue rangesInfoArray = value["seekableRanges"];

			seekableRangesInfo.numRanges = rangesInfoArray.arraySize();
			for (int i = 0; i < seekableRangesInfo.numRanges; i++) {
				seekable_range_t rangeInfo = {};
				rangeInfo.start = (int64_t)unmarshalllonglong(rangesInfoArray[i]["start"]);
				rangeInfo.end = (int64_t)unmarshalllonglong(rangesInfoArray[i]["end"]);

				seekableRangesInfo.seekableRanges.push_back(rangeInfo);
			}

			for (int i = 0; i < seekableRangesInfo.numRanges; i++) {
				cout << "onUserDefinedChanged: seekableRanges.seekableRanges[" << i << "].start = " << seekableRangesInfo.seekableRanges[i].start << endl;
				cout << "onUserDefinedChanged: seekableRanges.seekableRanges[" << i << "].end = " << seekableRangesInfo.seekableRanges[i].end << endl;
			}
		}
		else if (name == "setMasterClockResult") {
			master_clock_info_t masterClockInfo = {};
			masterClockInfo.result = unmarshallboolean(value["result"]);
			masterClockInfo.port = unmarshalllong(value["port"]);
			masterClockInfo.baseTime = unmarshalllonglong(value["baseTime"]);

			cout << "onUserDefinedChanged: masterClockInfo.result = "   << masterClockInfo.result << endl;
			cout << "onUserDefinedChanged: masterClockInfo.port = "     << masterClockInfo.port << endl;
			cout << "onUserDefinedChanged: masterClockInfo.baseTime = "     << masterClockInfo.baseTime << endl;
		}
		else if (name == "setSlaveClockResult") {
			slave_clock_info_t slaveClockInfo = {};
			slaveClockInfo.result = unmarshallboolean(value["result"]);

			cout << "onUserDefinedChanged: slaveClockInfo.result = " << slaveClockInfo.result << endl;
		}
		else if (name == "subtitleData") {
			subtitle_data_info_t subtitleDataInfo = {};
			subtitleDataInfo.subtitleData = unmarshallstring(value["subtitleData"]);
			subtitleDataInfo.presentationTime = (uint64_t)unmarshalllonglong(value["presentationTime"]);

			cout << "onUserDefinedChanged: subtitleData.subtitleData = " << subtitleDataInfo.subtitleData << endl;
			cout << "onUserDefinedChanged: subtitleData.presentationTime = " << subtitleDataInfo.presentationTime << endl;
		}
		else if (name == "validData") {
			bool state = unmarshallboolean(value["state"]);

			cout << "onUserDefinedChagned: validData state = " << state << endl;
		}
		else if (name == "subscription") {
			onSubscribe(message);
		}
		else if (name == "errorCode") {
			cout << "Command Result : " << message << endl;
		}
		else {
			cout << "Unknown stateChange event.  Passing raw message to client." << endl;
		}

		return true;
	}

	void trackAppProcesses() {
		connection->subscribe("com.webos.media/trackAppProcesses", "{}", trackAppProcessesCallback, (void*)this);
	}

	load_state_t state;
private:
	UMS_RESPONSE_HANDLER(MediaPlayer, trackAppProcessesCallback, appPidUpdate)


};

bool MediaPlayer::appPidUpdate(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx)
{
	const char *msg = connection->getMessageText(message);

	cout << "\033[1;31m" << msg << "\033[0m" << endl;

	return true;
}

// ---
// MAIN

int main(int argc, char *argv[])
{
	MediaPlayer mp;

	string cmd;
	bool exit = false;

	printf("uMediaClient : START\n");
	printf("COMMANDS: load file:////media_files/rat.mp4 ref {\"option\":{\"appId\":\"app1\"}}, play, pause, unload, exit :  \n");
	printf("\tenter command : ");
	while( !exit ) {
		try {
			getline(cin, cmd);
		}
		catch (const std::exception& e)
		{
			cout << "Exception caught calling getline: " << e.what() << endl;
			exit;
		}

		// split command into arguments
		vector<string> args;
		args.clear();
		split(args, cmd, is_any_of(" "));   // boost string algorithm

		//load uri type payload
		if(args[0] == "load" ) {
			printf("command :  %s\n",cmd.c_str());
			mp.state = LOADED;

			if(args.size() < 3) {
				printf("ERROR :  must specify pipeline type in load command.\n");
				continue;
			}

			mp.load(args[1], args[2], args[3]);
			printf("load media_id :  %s\n",mp.getMediaId().c_str());
		}
		else if(args[0] == "attach" ) {
			printf("command :  %s\n",cmd.c_str());
			mp.state = LOADED;

			if(args.size() < 1) {
				printf("ERROR :  must specify mediaId in attach command.\n");
				continue;
			}

			mp.attach(args[1]);
			printf("attach media_id :  %s\n",mp.getMediaId().c_str());
		}
		else if(args[0] == "loadAsync" ) {
			printf("command :  %s\n",cmd.c_str());
			mp.state = LOADED;

			if(args.size() < 3) {
				printf("ERROR :  must specify pipeline type in load command.\n");
				continue;
			}

			mp.loadAsync(args[1], args[2], args[3]);
			printf("load media_id :  %s\n",mp.getMediaId().c_str());
		}
		else if(args[0] == "loadAsyncBadClient" ) {
			// send commands immediately after loadAsync
			// to simulate misbehaving loadAsync client.
			//
			printf("command :  %s\n",cmd.c_str());
			mp.state = LOADED;

			if(args.size() < 3) {
				printf("ERROR :  must specify pipeline type in load command.\n");
				continue;
			}

			mp.loadAsync(args[1], args[2], args[3]);
			mp.play();
			printf("load media_id :  %s\n",mp.getMediaId().c_str());
		}
		else if( args[0] == "play") {
			printf("command :  %s\n",cmd.c_str());
			mp.play();
		}
		else if( args[0] == "pause") {
			if( UNLOADED == mp.state ) {
				printf("load content first !\n");
				continue;
			}
			printf("command :  %s\n",cmd.c_str());
			mp.pause();
		}
		else if( args[0] == "seek") {
			if( UNLOADED == mp.state ) {
				printf("load content first !\n");
				continue;
			}
			printf("command :  %s \n",cmd.c_str());

			long long pos = 0;       //default to zero
			if( args.size() > 1 ) {
				pos = boost::lexical_cast<int>(args[1]);
			}

			mp.seek(pos);
		}
		else if( args[0] == "setPlayRate") {
			if( UNLOADED == mp.state ) {
				printf("load content first !\n");
				continue;
			}
			printf("command :  %s \n",cmd.c_str());

			float playRate = 1.0;       //default to zero
			if( args.size() == 2 ) {
				playRate = boost::lexical_cast<float>(args[1]);
				mp.setPlayRate(playRate);
			} else if ( args.size() == 3 ) {
				bool audioOutput = true;
				playRate = boost::lexical_cast<float>(args[1]);
				audioOutput = boost::lexical_cast<int>(args[2]);
				mp.setPlayRate(playRate, audioOutput);
			}
			else {
				printf("setPlayRate - usage :  playRate audioOutput \n");
				continue;
			}
		}
		else if(args[0] == "selectTrack" ) {

			if( UNLOADED == mp.state ) {
				printf("load content first !\n");
				continue;
			}

			if( args.size() != 3 ) {
				printf("selectTrack - usage :  selectTrack type index \n");
				continue;
			}

			string type = args[1];
			long index =  boost::lexical_cast<int>(args[2]);

			printf("command :  %s\n",cmd.c_str());
			mp.selectTrack(type, index);
		}
		else if(args[0] == "setVolume" ) {
			if (UNLOADED == mp.state) {
				printf("load content first !\n");
				continue;
			}

			if (args.size() < 2) {
				printf("setVolume - usage : setVolume volume [duration] [type]\n");
				printf(" [type] - usage :  Linear, InCubic, OutCubic \n");
				continue;
			}
			long volume =  boost::lexical_cast<int>(args[1]);
			long duration = 0;
			EaseType type = kEaseTypeLinear;
			if (args.size() == 4) {
				duration = boost::lexical_cast<int>(args[2]);
				type = string_to_ease_type(args[3].c_str());
			}
			mp.setVolume(volume, duration, type);
		}
		else if(args[0] == "startCameraRecord" ) {
			if( UNLOADED == mp.state ) {
				printf("load content first !\n");
				continue;
			}

			if( args.size() != 3 ) {
				printf("startCameraRecord - usage :  startCameraRecord location format \n");
				continue;
			}

			printf("command :  %s\n",cmd.c_str());
			mp.startCameraRecord(args[1],args[2]);
		}
		else if(args[0] == "stopCameraRecord" ) {
			if( UNLOADED == mp.state ) {
				printf("load content first !\n");
				continue;
			}

			printf("command :  %s\n",cmd.c_str());
			mp.stopCameraRecord();
		}
		else if(args[0] == "takeCameraSnapshot" ) {
			if( UNLOADED == mp.state ) {
				printf("load content first !\n");
				continue;
			}

			if( args.size() != 6 ) {
				printf("takeCameraSnapshot - usage :  takeCameraSnapshot location format width height pictureQuality \n");
				continue;
			}

			long width =  boost::lexical_cast<int>(args[3]);
			long height =  boost::lexical_cast<int>(args[4]);
			long pq =  boost::lexical_cast<int>(args[5]);

			printf("command :  %s\n",cmd.c_str());
			mp.takeCameraSnapshot(args[1],args[2],width,height,pq);
		}
		else if( args[0] == "unload") {
			if( UNLOADED == mp.state ) {
				printf("load content first !\n");
				continue;
			}
			printf("command :  %s\n",cmd.c_str());
			mp.unload();
		}
		else if( args[0] == "notifyForeground" || args[0] == "fg") {
			if( UNLOADED == mp.state ) {
				printf("load content first !\n");
				continue;
			}
			printf("command : %s\n", cmd.c_str());
			mp.notifyForeground();
		}
		else if( args[0] == "notifyBackground" || args[0] == "bg") {
			if( UNLOADED == mp.state ) {
				printf("load content first !\n");
				continue;
			}
			printf("command : %s\n", cmd.c_str());
			mp.notifyBackground();
		}
		else if( args[0] == "exit") {
			printf("command :  %s\n",cmd.c_str());
			if( LOADED == mp.state ) {
				mp.unload();
			}
			exit = true;
		}
		else if( args[0] == "trackAppProcesses") {
			printf("command :  %s\n",cmd.c_str());
			mp.trackAppProcesses();
		}
		else if ( args[0] == "setDisplayWindow" ) {
			printf("command :  %s\n",cmd.c_str());
			if (args.size() > 4) {
				int dx  = lexical_cast<int>(args[1]);
				int dy  = lexical_cast<int>(args[2]);
				int dw  = lexical_cast<int>(args[3]);
				int dh  = lexical_cast<int>(args[4]);
				if (args.size() > 8) {
					int sx = lexical_cast<int>(args[5]);
					int sy = lexical_cast<int>(args[6]);
					int sw = lexical_cast<int>(args[7]);
					int sh = lexical_cast<int>(args[8]);
					mp.setDisplayWindow({sx, sy, sw, sh}, {dx, dy, dw, dh});
				} else {
					mp.setDisplayWindow({dx, dy, dw, dh});
				}
			}
		}
		else if ( args[0] == "switchToFullscreen" ) {
			printf("command :  %s\n",cmd.c_str());
			mp.switchToFullscreen();
		}
		else if ( args[0] == "focus" ) {
			printf("command :  %s\n",cmd.c_str());
			mp.setFocus();
		}
		else if (args[0] == "subscribe") {
			printf("command :  %s\n",cmd.c_str());
			mp.subscribe();
		}
		else if (args[0] == "unsubscribe") {
			printf("command :  %s\n",cmd.c_str());
			mp.unsubscribe();
		}
		else {
			printf("UNKNOWN COMMAND :  '%s'\n",cmd.c_str());
		}
	}

	printf("\nuMedia Client API exiting :  '%s'\n",cmd.c_str());

	return 0;

}
