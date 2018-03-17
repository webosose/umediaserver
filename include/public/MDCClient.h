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
// MDCClient
//
// Media Display Controller Client
//   Used by un-managed media clients
//

#ifndef ____MDC_CLIENT_H
#define ____MDC_CLIENT_H

#include <iostream>
#include <stdexcept>
#include <stdio.h>
#include <string>
#include <functional>

#include <errno.h>
#include <pthread.h>
#include <glib.h>
#include <pbnjson.hpp>
#include <UMSConnector.h>
#include <Logger.h>
#include "dto_types.h"

struct window_t
{
	window_t(int _x = 0, int _y = 0, int _w = 0, int _h = 0) : x(_x), y(_y), w(_w), h(_h) {}
	operator bool () const { return w > 0 && h > 0; }
	friend std::ostream & operator << (std::ostream & os, const window_t & r) {
		return os << "(" << r.x << "," << r.y << "; " << r.w << "x" << r.h << ")";
	}
	int x, y, w, h;
};

class MDCClient
{
public:
	typedef std::function<void(const uMediaServer::rect_t &)> active_region_callback_t;

	MDCClient();
	virtual ~MDCClient();

	// @f registerMedia
	// @b register media object and application ID
	//
	bool registerMedia(const std::string &media_id,
			const std::string &app_id);

	// @f unregisterMedia
	// @b unregister media object and application ID
	//
	bool unregisterMedia();

	// @f setFocus
	// @b focus audio and video on media object
	//
	bool setFocus();

	// @f setDisplayWindow
	// @b set output display window
	//
	bool setDisplayWindow(const window_t &output);

	// @f setDisplayWindow
	// @b set input and output display window
	//
	bool setDisplayWindow(const window_t &input, const window_t &output);

	// @f switchToFullscreen
	// @b switch media object to fullscreen
	//
	bool switchToFullscreen();

	// @f switchToAutoLayout
	// @ b switch media positioning to autolayout
	bool switchToAutoLayout(active_region_callback_t && cb);

	// @f visibility
	// @b get current media object visibility state
	//
	bool visibility() const;

	// @f setVisibility
	// @b set current media object visibility state
	//
	bool setVisibility(bool visibility);

	// update the playback status for unmanaged pipelines
	bool updatePlaybackState(playback_state_t state);

private:
	UMSConnector * connector_;
	std::string media_id_;
	std::string app_id_;
	std::string mdc_uri_;
	bool visible;
	GMainLoop * gmain_loop_;
	GMainContext* gmain_context_;
	pthread_t message_thread_;
	active_region_callback_t active_region_callback_;
	size_t subscribed_token_;

	static void * messageThread(void *arg);
	pbnjson::JValue serializeWindow(const window_t &rect);
	void subscribe();
	void unsubscribe();
	static bool notificationDispatch(UMSConnectorHandle *, UMSConnectorMessage *, void *);
};

#endif // ____MDC_CLIENT_H
