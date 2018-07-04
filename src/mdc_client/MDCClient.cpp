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

#include "MDCClient.h"
#include <GenerateUniqueID.h>
#include "Logger_macro.h"

using namespace std;
using namespace pbnjson;
using namespace boost;
using namespace uMediaServer;

#define MDC_URI "com.webos.media"
#define MDC_CLIENT_BASE_URI "com.webos.rm.client."

namespace {
	Logger _log(UMS_LOG_CONTEXT_CLIENT);
}

MDCClient::MDCClient() : mdc_uri_(MDC_URI), visible(true), subscribed_token_(-1)
{
	string uid = GenerateUniqueID()();
	string mdc_client_uri = MDC_CLIENT_BASE_URI + uid;
	gmain_context_ = g_main_context_new();
	gmain_loop_ = g_main_loop_new(gmain_context_, false);
	connector_ = new UMSConnector(mdc_client_uri, gmain_loop_,
			static_cast<void*>(this), UMS_CONNECTOR_PUBLIC_BUS);

	pthread_create(&message_thread_, NULL, messageThread, this);
	LOG_DEBUG(_log, "ctor called: MDDClient");
}

MDCClient::~MDCClient()
{
	LOG_DEBUG(_log, "dtor called: MDDClient");
	unsubscribe();

	connector_->stop();
	pthread_join(message_thread_, NULL);

	delete connector_;
	g_main_context_unref(gmain_context_);
	g_main_loop_unref(gmain_loop_);
}

bool MDCClient::registerMedia(const std::string &media_id,
		const std::string &app_id)
{
	JValue args = Object();
	args.put("mediaId", media_id);
	args.put("appId", app_id);

	JGenerator serializer(nullptr);
	string serialized;

	if (!serializer.toString(args,  pbnjson::JSchema::AllSchema(), serialized)) {
		LOG_ERROR(_log, MSGERR_JSON_SERIALIZE, "Failed to serialize request.");
		return false;
	}

	media_id_ = media_id;
	app_id_ = app_id;

	LOG_DEBUG(_log, "registerMedia %s", serialized.c_str());
	string cmd = mdc_uri_ + "/registerMedia";
	connector_->sendMessage(cmd, serialized, nullptr, this);

	subscribe();

	return true;
}

bool MDCClient::unregisterMedia()
{
	unsubscribe();

	JValue args = Object();
	args.put("mediaId", media_id_);

	JGenerator serializer(nullptr);
	string serialized;

	if (!serializer.toString(args,  pbnjson::JSchema::AllSchema(), serialized)) {
		LOG_ERROR(_log, MSGERR_JSON_SERIALIZE, "Failed to serialize request.");
		return false;
	}

	LOG_DEBUG(_log, "unregisterMedia %s", serialized.c_str());
	string cmd = mdc_uri_ + "/unregisterMedia";
	return connector_->sendMessage(cmd, serialized, nullptr, this);
}

bool MDCClient::setDisplayWindow(const window_t &output)
{
	JValue args = Object();
	args.put("mediaId", media_id_);
	args.put("destination", serializeWindow(output));

	JGenerator serializer(nullptr);
	string serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), serialized)) {
		LOG_ERROR(_log, MSGERR_JSON_SERIALIZE, "Failed to serialize request.");
		return false;
	}

	LOG_DEBUG(_log, "setDisplayWindow %s", serialized.c_str());
	string cmd = mdc_uri_ + "/setDisplayWindow";
	return connector_->sendMessage(cmd, serialized, nullptr, this);
}

bool MDCClient::setDisplayWindow(const window_t & source, const window_t & output)
{
	JValue args = Object();
	args.put("mediaId", media_id_);
	args.put("destination", serializeWindow(output));
	args.put("source", serializeWindow(source));

	JGenerator serializer(nullptr);
	string serialized;

	if (!serializer.toString(args,  pbnjson::JSchema::AllSchema(), serialized)) {
		LOG_ERROR(_log, MSGERR_JSON_SERIALIZE, "Failed to serialize request.");
		return false;
	}

	LOG_DEBUG(_log, "setDisplayWindow %s", serialized.c_str());
	string cmd = mdc_uri_ + "/setDisplayWindow";
	return connector_->sendMessage(cmd, serialized, nullptr, this);
}

bool MDCClient::switchToFullscreen()
{
	JValue args = Object();
	args.put("mediaId", media_id_);

	JGenerator serializer(nullptr);
	string serialized;

	if (!serializer.toString(args,  pbnjson::JSchema::AllSchema(), serialized)) {
		LOG_ERROR(_log, MSGERR_JSON_SERIALIZE, "Failed to serialize request.");
		return false;
	}

	LOG_DEBUG(_log, "switchToFullScreen %s", serialized.c_str());
	string cmd = mdc_uri_ + "/switchToFullScreen";
	return connector_->sendMessage(cmd, serialized, nullptr, this);
}

bool MDCClient::switchToAutoLayout(active_region_callback_t && cb) {
	active_region_callback_ = cb;

	JValue args = Object();
	args.put("mediaId", media_id_);

	JGenerator serializer(nullptr);
	string serialized;

	if (!serializer.toString(args,  pbnjson::JSchema::AllSchema(), serialized)) {
		LOG_ERROR(_log, MSGERR_JSON_SERIALIZE, "Failed to serialize request.");
		return false;
	}

	string cmd = mdc_uri_ + "/switchToAutoLayout";
	return connector_->sendMessage(cmd, serialized, nullptr, this);
}

bool MDCClient::setFocus()
{
	JValue args = Object();
	args.put("mediaId", media_id_);

	JGenerator serializer(nullptr);
	string serialized;

	if (!serializer.toString(args,  pbnjson::JSchema::AllSchema(), serialized)) {
		LOG_ERROR(_log, MSGERR_JSON_SERIALIZE, "Failed to serialize request.");
		return false;
	}

	LOG_DEBUG(_log, "focus %s", serialized.c_str());
	string cmd = mdc_uri_ + "/focus";
	return connector_->sendMessage(cmd, serialized, nullptr, this);
}

bool MDCClient::visibility() const
{
	return visible;
}

bool MDCClient::setVisibility(bool visibility)
{
	if (visibility != visible) {
		visible = visibility;
		JValue args = JObject{{"mediaId", media_id_}, {"visible", visible}};

		JGenerator serializer(nullptr);
		std::string serialized;

		if (!serializer.toString(args,  pbnjson::JSchema::AllSchema(), serialized)) {
			LOG_ERROR(_log, MSGERR_JSON_SERIALIZE, "Failed to serialize request.");
			return false;
		}
		std::string cmd = mdc_uri_ + "/setVisibility";
		return connector_->sendMessage(cmd, serialized, nullptr, this);
	}
	return true;
}

bool MDCClient::updatePlaybackState(playback_state_t state)
{
	JValue args = Object();
	JValue media_id_json = Object();
	media_id_json.put("mediaId", media_id_);

	if (state == PLAYBACK_PLAYING) {
		args.put("playing", media_id_json);
	} else if (state == PLAYBACK_STOPPED) {
		args.put("endOfStream", media_id_json);
	} else {
		args.put("paused", media_id_json);
	}

	JGenerator serializer(nullptr);
	string serialized;

	if (!serializer.toString(args,  pbnjson::JSchema::AllSchema(), serialized)) {
		LOG_ERROR(_log, MSGERR_JSON_SERIALIZE, "Failed to serialize request.");
		return false;
	}

	LOG_DEBUG(_log, "updatePlaybackStatus %s", serialized.c_str());
	string cmd = mdc_uri_ + "/updatePipelineStateEvent";
	return connector_->sendMessage(cmd, serialized, nullptr, this);
}

// ------------
// PRIVATE

void * MDCClient::messageThread(void *arg)
{
	MDCClient *self = static_cast<MDCClient *>(arg);
	self->connector_->wait();
	return NULL;
}

JValue MDCClient::serializeWindow(const window_t &window)
{
	JValue json_rect = Object();
	json_rect.put("x", window.x);
	json_rect.put("y", window.y);
	json_rect.put("width", window.w);
	json_rect.put("height", window.h);
	return json_rect;
}

void MDCClient::subscribe() {
	pbnjson::JValue args = pbnjson::Object();

	args.put("mediaId", media_id_);

	JGenerator serializer(NULL);
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(_log, MSGERR_JSON_SERIALIZE, "failed to serialize payload");
		return;
	}

	string cmd = mdc_uri_ + "/subscribe";
	subscribed_token_ = connector_->subscribe(cmd, payload_serialized, notificationDispatch, (void*)this);
}

void MDCClient::unsubscribe() {
	pbnjson::JValue args = pbnjson::Object();

	args.put("mediaId", media_id_);

	JGenerator serializer(NULL);
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(_log, MSGERR_JSON_SERIALIZE, "failed to serialize payload");
		return;
	}

	string cmd = mdc_uri_ + "/unsubscribe";
	connector_->unsubscribe(cmd, subscribed_token_);
}

bool MDCClient::notificationDispatch(UMSConnectorHandle *, UMSConnectorMessage * message, void* context) {
	auto _this = static_cast<MDCClient*>(context);
	std::string msg = _this->connector_->getMessageText(message);

	JDomParser parser;
	if (!parser.parse(msg, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR(_log, MSGERR_JSON_PARSE, "JDomParse. input=%s", msg.c_str());
		return false;
	}

	JValue dom = parser.getDom();

	if ( !(*dom.begin()).first.isString() ) {
		LOG_ERROR(_log, MSGERR_JSON_PARSE, "JDomParse. input=%s", msg.c_str());
		return false;
	}

	std::string event = (*dom.begin()).first.asString();
	JValue value = dom[event];

	if (event == "activeRegion" && _this->active_region_callback_) {
		rect_t active_rc {
			value["x"].asNumber<int>(),
			value["y"].asNumber<int>(),
			value["width"].asNumber<int>(),
			value["height"].asNumber<int>()
		};
		_this->active_region_callback_(active_rc);
	}

	return true;
}
