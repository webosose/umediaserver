// Copyright (c) 2015-2018 LG Electronics, Inc.
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

#ifndef __TVDISPLAYCONNECTOR_H__
#define __TVDISPLAYCONNECTOR_H__

#include <Logger.h>
#include <UMSConnector.h>
#include "interfaces.h"
#include <map>

namespace uMediaServer {

class TVDisplayConnector : public mdc::ITVDisplay {
public:
	TVDisplayConnector(UMSConnector * umc, const mdc::IChannelConnection & channel);

	// ITVDisplay interface
	virtual void vsm_set_registration_callback(callback_t && callback);
	virtual void vsm_set_connection_observer(callback_t && callback);
	virtual void vsm_register(const std::string &id, const mdc::res_t & adec, const mdc::res_t & vdec);
	virtual void vsm_unregister(const std::string &id);
	virtual void vsm_connect(const std::string &id, mdc::sink_t sink);
	virtual void vsm_disconnect(const std::string &id);
	virtual void avblock_mute(const std::string &id, size_t channel);
	virtual void avblock_unmute(const std::string &id, size_t channel);
	virtual void avblock_set_muted_callback(callback_t && callback);
	virtual void display_set_window(const std::string &id, const mdc::display_out_t &display_out);
	virtual void display_set_video_info(const std::string &id, const mdc::video_info_t &video_info);
	virtual void display_set_alpha(const std::string &id, double alpha);
	virtual void display_set_alpha(mdc::sink_t sink, double alpha);
	virtual void display_set_config_completed_callback(callback_t && callback);
	virtual void display_set_sub_overlay_mode(SubOverlayMode mode);
	virtual void sound_connect(const std::string & id);
	virtual void sound_disconnect(const std::string & id);
	virtual void sound_set_connection_observer(callback_t && callback);

private:
	struct overlay_state_t {
		uint8_t alpha;
		uint8_t z;
	};

	bool reply_handler(const std::string & message, callback_t & callback);
	static bool vsm_register_reply_hanler(UMSConnectorHandle *, UMSConnectorMessage * reply, void * ctx);
	static bool sound_connection_status_hanler(UMSConnectorHandle *, UMSConnectorMessage * reply, void * ctx);
	static bool vsm_subscription_handler(UMSConnectorHandle *, UMSConnectorMessage * reply, void * ctx);
	static bool avblock_mute_handler(UMSConnectorHandle *, UMSConnectorMessage * reply, void * ctx);
	static bool avblock_unmute_handler(UMSConnectorHandle *, UMSConnectorMessage * reply, void * ctx);
	static bool display_config_handler(UMSConnectorHandle *, UMSConnectorMessage * reply, void * ctx);
	UMSConnector * connector;
	const mdc::IChannelConnection & video_channel;
	std::map<mdc::sink_t, overlay_state_t> overlay_states;
	// TODO: it is much better way to have one callmap with message tokens but UMC hides call tokens from user
	struct subscription_t {
		subscription_t (const char * c = nullptr) : call(c?c:""), token(0) {}
		std::string call;
		size_t token;
	};
	subscription_t sound_subscription;
	std::map<std::string, subscription_t> vsm_subscriptions;
	std::string broadcastId;
	callback_t registration_callback;
	callback_t vsm_connection_observer;
	callback_t sound_connection_observer;
	callback_t avblock_muted_callback;
	callback_t display_config_completed_callback;
};

}

#endif // __TVDISPLAYCONNECTOR_H__
