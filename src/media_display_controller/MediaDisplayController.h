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

#ifndef _MEDIA_DISPLAY_CONTROLLER_H
#define _MEDIA_DISPLAY_CONTROLLER_H

#include <stdexcept>
#include <map>
#include <set>
#include <list>
#include <sstream>
#include <stdio.h>
#include <cassert>
#include <string>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include <signal.h>
#include <boost/noncopyable.hpp>
#include <boost/signals2.hpp>

#include <pbnjson.hpp>
#include <UMSConnector.h>
#include <Logger.h>
#include "ConnectionPolicy.h"
#include "AppObserver.h"
#include "LayoutManager.h"
#include "media_object.h"
#include "events.h"
#include "dto_types.h"

namespace uMediaServer {

typedef boost::signals2::signal<void(mdc::event::EventSignalType signal, const std::string & id,
									 const mdc::event::EventDataBaseType & data)> event_signal_t;

struct media_element_t {
	media_element_t(const std::string & app_id, const std::string & media_id,
					mdc::ITVDisplay & tv_display, mdc::IConnectionPolicy & connection_policy,
					mdc::ILayoutManager & layout_manager)
		: subscription {0, ""}
		, in_app_foreground(true)
		, is_fullScreen(false)
		, media(std::make_shared<mdc::MediaObject>(media_id, app_id, tv_display, connection_policy, layout_manager))
		, playback_state(PLAYBACK_STOPPED)
		, output_window {0,0,0,0} {}
	struct subscripion_t {
		size_t token;
		std::string call;
	} subscription;
	bool in_app_foreground;
	bool is_fullScreen;
	playback_state_t playback_state;
	std::shared_ptr<mdc::MediaObject> media;
	uMediaServer::rect_t output_window;
};
typedef std::map<std::string, media_element_t> media_element_map_t;

class MediaDisplayController final : boost::noncopyable
{
public:
	static MediaDisplayController* instance(UMSConnector * connector = nullptr);

#define MDC_EVENT_HANDLER(_member_)\
	static bool _member_##CallBack(UMSConnectorHandle* handle, UMSConnectorMessage* message, void*) {\
		return instance()->_member_(handle, message);\
	}\
	bool _member_(UMSConnectorHandle * handle, UMSConnectorMessage * message)

	// -- Media Frameworks interface (client side)
	MDC_EVENT_HANDLER(focus);
	MDC_EVENT_HANDLER(setDisplayWindow);
	MDC_EVENT_HANDLER(switchToFullScreen);
	MDC_EVENT_HANDLER(setVisibility);
	MDC_EVENT_HANDLER(getForegroundAppInfo);

	// -- Media Frameworks interface (pipeline side)
	MDC_EVENT_HANDLER(unregisterMedia);
	MDC_EVENT_HANDLER(setVideoInfo);
	MDC_EVENT_HANDLER(contentReady);

	// -- Pipeline Event handler
	// @f mediaContentReady
	// @b pipeline process has generated a frame of data and is "ready"
	//
	MDC_EVENT_HANDLER(pipelineEvents);

	// -- uMS interface
	//
	// @f registerMedia
	// @b register media object. link media_id with app_id passed in load
	//
	bool registerMedia(const std::string &media_id, const std::string &app_id);

	// @f unregisterMedia
	// @b unregister media object.
	//
	bool unregisterMedia(const std::string &id);

	// @f acquired
	// @b notify new Media Pipeline resources acquisition
	//
	bool acquired(const std::string & id, const std::string & service,
				  const mdc::res_info_t & resources);

	// @f released
	// @b notify Media Pipeline resources released
	//
	bool released(const std::string & id, const mdc::res_info_t & resources);

	// @f contentReady
	// @b update media content readiness
	//
	bool contentReady(const std::string &media_id, bool ready);

	// @f registerFocusEventNotify
	// @b register handler for focus events
	//
	void registerEventNotify(mdc::event::EventSignalType,
			const event_signal_t::slot_type& subscriber);

	// @f inAppForegroundEvent
	// @b notify app internal foreground state (e.g. browser tabs)
	//
	void inAppForegroundEvent(const std::string &id, bool foreground);

	// @f focus
	// @b set media focus
	//
	bool focus(const std::string &media_id);

	// @f getMediaElementState
	// @b query mdc related media element state
	//
	mdc::media_element_state_t getMediaElementState(const std::string & id);

	bool updatePipelineState(const std::string &media_id, playback_state_t state);

	int numberOfAutoLayoutedVideos() const;

	// @f acquireDisplayResource
	// @b acquire display resources including plane id, crtc id, connector id based on plane name and index
	//
	void acquireDisplayResource(const std::string & plane_name, const int32_t index, ums::disp_res_t & res);

	// @f releaseDisplayResource
	// @b release display resources including plane id, crtc id, connector id based on plane name and index
	//
	void releaseDisplayResource(const std::string & plane_name, const int32_t index);

	// @f getConnectedSinkname
	// @b query connected video/audio sink name based on media id
	//
	std::pair<std::string, std::string> getConnectedSinkname(const std::string & id);
private:
	MediaDisplayController(UMSConnector * connector);

	UMSConnector *connector_;

	AppObserver app_observer;
	std::unique_ptr<mdc::ITVDisplay> tv_display;
	ConnectionPolicy connection_policy;
	LayoutManager layout_manager;

	media_element_map_t media_elements_;
	std::set<std::string> _foreground_apps;

	std::map<mdc::event::EventSignalType, event_signal_t> event_signal;
	// audio connections: first => current; second => previous
	std::pair<std::string, std::string> audio_connections;
	std::list<std::string> focus_stack;

	// @f foregroundEvent
	// @b Foreground apps list from LSM passed via uMS
	//
	void foregroundEvent(const std::set<std::string> & fg_apps);

	// @f setVideoInfo
	// @b provide video meta information
	//
	bool setVideoInfo(const pbnjson::JValue & parsed);

	// @f notifyFocusEvent
	// @b notify observers of focus event on specific id
	//
	void notifyEvent(mdc::event::EventSignalType type,
			const std::string & id, const mdc::event::EventDataBaseType & data);

	void updateForegroundState(media_element_t & element);
	void notifyFocusChange(const media_element_t & element);
	void notifyActiveRegion(const std::string & id, const mdc::display_out_t & dpy_out);
	void reconnectSound(const std::string & id);


	void applyAutoLayout() const;
};

} // end namespace uMediaServer

#endif // MEDIA_DISPLAY_CONTROLLER
