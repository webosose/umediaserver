// Copyright (c) 2015-2019 LG Electronics, Inc.
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

#ifndef __UMS_MDC_EVENTS_H__
#define __UMS_MDC_EVENTS_H__

#include <boost/statechart/event.hpp>
#include "dto_types.h"

namespace uMediaServer { namespace mdc { namespace event {

enum class EventSignalType {
	REGISTERED,
	FOCUS,
	FOREGROUND,
	MEDIA_CONTENT_READY,
	VIDEO_INFO,
	FULLSCREEN,
	VISIBLE,
	SOUND_DISCONNECTED,
	PLANE_ID
};

struct EventDataBaseType {}; // base carrier for data payload. downcast to send, upcast to decode

struct RegisteredEvent : EventDataBaseType {};

struct FocusEvent : EventDataBaseType {
	explicit FocusEvent(bool state) : state(state) {}
	bool state;
};

struct ForegroundEvent : EventDataBaseType {
	explicit ForegroundEvent(bool state) : state(state) {}
	bool state;
};

struct MediaContentReadyEvent : EventDataBaseType {
	explicit MediaContentReadyEvent(bool state) : state(state) {}
	bool state;
};

struct FullscreenEvent : EventDataBaseType {
	explicit FullscreenEvent(bool state) : state(state) {}
	bool state;
};

#if UMS_INTERNAL_API_VERSION == 2
struct VideoInfoEvent : EventDataBaseType {
	VideoInfoEvent(const ums::video_info_t &info) : video_info(info) {}
	ums::video_info_t video_info;
};
#else
struct VideoInfoEvent : EventDataBaseType {
	VideoInfoEvent(const mdc::video_info_t &info) : video_info(info) {}
	mdc::video_info_t video_info;
};
#endif

struct VisibilityEvent : EventDataBaseType {
	explicit VisibilityEvent(bool state) : state(state) {}
	bool state;
};

struct PlaneIdEvent : EventDataBaseType {
	PlaneIdEvent(int32_t id) : plane_id(id) {}
	int32_t plane_id;
};

// client events
struct SetDisplayWindow     : boost::statechart::event< SetDisplayWindow > {
	SetDisplayWindow(const rect_t & out, const rect_t & src = rect_t())
		: src_rect(src), out_rect(out) {}
	rect_t src_rect;
	rect_t out_rect;
};

struct SwitchToFullscreen   : boost::statechart::event< SwitchToFullscreen > {};
struct SwitchToAutoLayout   : boost::statechart::event< SwitchToAutoLayout > {};
struct SetFocus             : boost::statechart::event< SetFocus > {};
struct SetVisibility        : boost::statechart::event< SetVisibility > {
	explicit SetVisibility(bool visibility) : visible(visibility) {}
	bool visible;
};
struct SetOpacity           : boost::statechart::event< SetOpacity > {
	explicit SetOpacity(double a) : alpha(a) {}
	double alpha;
};

// pipeline events
#if UMS_INTERNAL_API_VERSION == 2
struct MediaVideoData       : boost::statechart::event< MediaVideoData > {
	MediaVideoData(const ums::video_info_t & info) : video_info(info) {}
        ums::video_info_t video_info;
};
#else
struct MediaVideoData       : boost::statechart::event< MediaVideoData > {
	MediaVideoData(const video_info_t & info) : video_info(info) {}
	video_info_t video_info;
};
#endif

struct Acquire              : boost::statechart::event< Acquire > {
	Acquire(const res_info_t & res) : resources(res) {}
	res_info_t resources;
};
struct Release              : boost::statechart::event< Release > {
	Release(const res_info_t & res) : resources(res) {}
	res_info_t resources;
};
struct MediaContentReady     : boost::statechart::event< MediaContentReady > {};
struct MediaContentNotReady  : boost::statechart::event< MediaContentNotReady > {};

// mdc internal commands
struct TryDisplay            : boost::statechart::event< TryDisplay > {};
struct Mute                  : boost::statechart::event< Mute > {};
struct DisconnectVideo       : boost::statechart::event< DisconnectVideo > {};
struct DisconnectAudio       : boost::statechart::event< DisconnectAudio > {};
struct LostFocus             : boost::statechart::event< LostFocus > {};

// lsm/sam events
struct ToForeground          : boost::statechart::event< ToForeground > {};
struct ToBackground          : boost::statechart::event< ToBackground > {};

// vsm events
struct Registered            : boost::statechart::event< Registered > {};
struct VideoConnected        : boost::statechart::event< VideoConnected > {
	VideoConnected(int32_t s) : sink(s) {}
	int32_t sink;
};
struct VideoDisconnected     : boost::statechart::event< VideoDisconnected > {};

// display events
struct DisplayConfigured     : boost::statechart::event< DisplayConfigured > {};

// avblock events
struct Muted                 : boost::statechart::event< Muted > {};

// sound events
struct AudioConnected        : boost::statechart::event< AudioConnected > {};
struct AudioDisconnected     : boost::statechart::event< AudioDisconnected > {};

}}} // uMediaServer::mdc::event

#endif // __UMS_MDC_EVENTS_H__
