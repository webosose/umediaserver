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
// Media Display Controller Content Provider
//   Used by un-managed Media Content Provider Pipelines
//

#ifndef ____MDC_CONTENT_PROVIDER_H
#define ____MDC_CONTENT_PROVIDER_H

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
#include <dto_types.h>

class MDCContentProvider
{
public:
	typedef std::function<void(int32_t)> plane_id_callback_t;

	MDCContentProvider(const std::string &media_id);
	virtual ~MDCContentProvider();

	// @f mediaContentReady
	// @b Media Content Provider data is ready for display
	//
	bool mediaContentReady(bool state);

	// @f setVideoInfo
	// @b Media Video Meta Data
	// TODO: libndl-directMedia2 uses UMS_INTERNAP_API_VERSION 1.
	bool setVideoInfo(const ums::video_info_t &video_info);
	bool setVideoInfo(const uMediaServer::mdc::video_info_t &video_info);

	// @f registerPlaneIdCb
	// @b Media Video hardware rendering plane
	bool registerPlaneIdCallback(plane_id_callback_t && plane_id_cb);

private:
	UMSConnector * connector_;
	std::string media_id_;
	std::string mdc_uri_;
	GMainLoop * gmain_loop_;
	GMainContext* gmain_context_;
	pthread_t message_thread_;
	plane_id_callback_t plane_id_callback_;
	size_t subscribed_token_;
	static void * messageThread(void *arg);
	void subscribe();
	void unsubscribe();
	static bool notificationDispatch(UMSConnectorHandle *, UMSConnectorMessage *, void *);
};

#endif //  ____MDC_CONTENT_PROVIDER_H
