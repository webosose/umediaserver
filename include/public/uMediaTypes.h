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
// uMediaServer API C wrapper
//

#ifndef __UMEDIA_TYPES_H
#define __UMEDIA_TYPES_H

// TODO move all parameters and enums to "MediaObject.h" file
typedef enum AudioStreamClass {
	kFile,
	kMedia,
	kGapless,
	kCamera,
	kAudioStreamRingtone,
	kAudioStreamAlert,
	kAudioStreamMedia,
	kAudioStreamNotification,
	kAudioStreamFeedback,
	kAudioStreamFlash,
	kAudioStreamNavigation,
	kAudioStreamVoicedial,
	kAudioStreamVoip,
	kAudioStreamCalendar,
	kAudioStreamAlarm,
	kAudioStreamDefaultapp,
	kAudioStreamVvm,
	kAudioStreamAlsa,
	kAudioStreamFake,
	kAudioStreamNone
} AudioStreamClass;


typedef enum EaseType {
	kEaseTypeLinear,
	kEaseTypeInCubic,
	kEaseTypeOutCubic
} EaseType;

inline const char * ease_type_to_string(EaseType type) {
	switch (type) {
		case kEaseTypeLinear		: return "Linear";
		case kEaseTypeInCubic		: return "InCubic";
		case kEaseTypeOutCubic		: return "OutCubic";
		default				: return "Linear";
	}
}

inline EaseType string_to_ease_type(const char * type) {
	if (0 == strncmp(type, "Linear",             sizeof("Linear")))             return kEaseTypeLinear;
	if (0 == strncmp(type, "InCubic",              sizeof("InCubic")))              return kEaseTypeInCubic;
	if (0 == strncmp(type, "OutCubic",       sizeof("OutCubic")))       return kEaseTypeOutCubic;
	return kEaseTypeLinear;
}

struct VideoBounds;
typedef enum NetworkState {
	NETWORK_EMPTY,
	NETWORK_IDLE,
	NETWORK_LOADING,
	NETWORK_LOADED
} NetworkState;

typedef enum Error {
	NO_ERROR,
	INVALID_SOURCE_ERROR,
	NETWORK_ERROR,
	FORMAT_ERROR,
	DECODE_ERROR
} Error;

typedef enum VideoFitMode {
	VIDEO_FIT,
	VIDEO_FILL
} VideoFitMode;

typedef enum ReadyState {
	HAVE_NOTHING,
	HAVE_METADATA,
	HAVE_CURRENT_DATA,
	HAVE_FUTURE_DATA,
	HAVE_ENOUGH_DATA
} ReadyState;

// TODO : not supported in C clients at this time

struct VideoBounds {
	float left;
	float top;
	float width;
	float height;
};

#endif // __UMEDIA_TYPES_H
