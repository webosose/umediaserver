// Copyright (c) 2019 LG Electronics, Inc.
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

#ifndef __VideoOutputdConnector_H__
#define __VideoOutputdConnector_H__

#include <Logger.h>
#include <UMSConnector.h>
#include "interfaces.h"
#include <map>
#include <unordered_map>

// create static dispatch method to allow object methods to be used as call backs for UMSConnector events
#define UMS_RESPONSE_HANDLER(_class_, _cb_, _member_) \
	static bool _cb_(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx) { \
	_class_ *self = static_cast<_class_ *>(ctx); \
	bool rv = self->_member_(handle, message,ctx);   \
	return rv;  } \
	bool _member_(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx);

namespace uMediaServer {

	class VideooutputConnector : public mdc::ITVDisplay {
	public:
		VideooutputConnector(UMSConnector * umc, mdc::IConnectionPolicy & connection_policy);

		// ITVDisplay interface
		virtual void vsm_set_registration_callback(callback_t && callbak);
		virtual void vsm_set_connection_observer(callback_t && callback);
		virtual void sound_set_connection_observer(callback_t && callback);
		virtual void avblock_set_muted_callback(callback_t && callback);
		virtual void display_set_config_completed_callback(callback_t && callback);
		virtual void vsm_register(const std::string &id, const mdc::res_t & adec, const mdc::res_t & vdec);
		virtual void vsm_unregister(const std::string & id);

		virtual void vsm_connect(const std::string &id, int32_t sink);
		virtual void vsm_disconnect(const std::string &id);
		virtual void avblock_mute(const std::string &id, size_t channel);
		virtual void avblock_unmute(const std::string &id, size_t channel);
		virtual void display_set_window(const std::string &id, const mdc::display_out_t &display_out);
#if UMS_INTERNAL_API_VERSION == 2
		virtual void display_set_video_info(const std::string &id, const ums::video_info_t &video_info);
#else
		virtual void display_set_video_info(const std::string &id, const mdc::video_info_t &video_info);
#endif
		virtual void display_set_alpha(const std::string &id, double alpha);
		virtual void sound_connect(const std::string & id);
		virtual void sound_disconnect(const std::string & id);
		virtual int32_t get_plane_id(const std::string & id) const override;
		virtual void acquire_display_resource(const std::string & plane_name, const int32_t index, ums::disp_res_t & res) override;
		virtual void release_display_resource(const std::string & plane_name, const int32_t index) override;
		//TODO:  Internal implementation methods specific to TVDispalyConnector
		// remove from the ITVDisplay
		virtual void display_set_alpha(int32_t sink, double alpha) {};
		virtual void display_set_sub_overlay_mode(SubOverlayMode mode) {};
	private:
		static bool videoConnectResult_cb(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx);

		// video sink status change callback
		//bool videoSinkStatusChange(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx);
		UMS_RESPONSE_HANDLER(VideooutputConnector,videoSinkStatusChange_cb, videoSinkStatusChange);

		void mute_video_impl(const std::string & id, bool mute);
		static bool avmuted_cb(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx);
		static bool display_config_cb(UMSConnectorHandle *, UMSConnectorMessage * reply, void * ctx);
		static bool getparam_cb(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx);

		typedef struct {
			uint8_t alpha;
			uint8_t z;
			std::string id; // pipeline id. Set when connect called, cleared to "" on disconnect
			std::string name; // Sink name, main or sub
			bool connected; // True when sinkStatus reports connected
			bool acquired; // True when plane resource acquired by Resource Manager
			int32_t planeId;
			int32_t crtcId;
			int32_t connId;
		} video_state_t;

		typedef struct {
			mdc::res_t adec;
			mdc::res_t vdec;
		} registration_t;

		typedef struct{
			std::string id;
			VideooutputConnector* connector;
		} connect_context_t;

		registration_t* id_to_registration(const std::string& id);
		video_state_t* id_to_vsink(const std::string& id);
		video_state_t* name_to_vsink(const std::string& name);

		void notifyVideoConnectedChanged(video_state_t& vstate, bool connected);

		UMSConnector * connector;
		uMediaServer::Logger log;
		callback_t registration_callback;
		callback_t vsm_connection_observer;
		callback_t avblock_muted_callback;
		callback_t display_config_completed_callback;
		mdc::IConnectionPolicy & _connection_policy;
		std::unordered_map<std::string, registration_t> registrations;
		std::vector<video_state_t> video_states; // Main and sub
		uint8_t max_video_sink;
	};

}

#endif // __VideoOutputdConnector_H__
