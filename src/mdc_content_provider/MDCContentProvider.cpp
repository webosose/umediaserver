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


#include "MDCContentProvider.h"
#include <GenerateUniqueID.h>
#include "Logger_macro.h"

using namespace std;
using namespace pbnjson;
using namespace boost;
using namespace uMediaServer;
using namespace mdc;

#define MDC_URI "com.webos.media"
#define MDC_CLIENT_BASE_URI "com.webos.rm.client."

namespace {
	Logger _log(UMS_LOG_CONTEXT_CLIENT);
}

MDCContentProvider::MDCContentProvider(const std::string &media_id)  :
		mdc_uri_(MDC_URI), media_id_(media_id), subscribed_token_(-1)
{
	string uid = GenerateUniqueID()();
	string mdc_client_uri = MDC_CLIENT_BASE_URI + uid;
	gmain_context_ = g_main_context_new();
	gmain_loop_ = g_main_loop_new(gmain_context_, false);
	connector_ = new UMSConnector(mdc_client_uri, gmain_loop_,
			static_cast<void*>(this), UMS_CONNECTOR_PRIVATE_BUS);

	pthread_create(&message_thread_, NULL, messageThread, this);

#ifdef USE_AVOUTPUTD
	subscribe();
#endif

	LOG_DEBUG(_log, "ctor called");
}

MDCContentProvider::~MDCContentProvider()
{
	LOG_DEBUG(_log, "dtor called");

#ifdef USE_AVOUTPUTD
	unsubscribe();
#endif

	connector_->stop();
	pthread_join(message_thread_, NULL);

	delete connector_;
	g_main_context_unref(gmain_context_);
	g_main_loop_unref(gmain_loop_);
}

bool MDCContentProvider::mediaContentReady(bool state)
{
	JValue args = Object();
	args.put("mediaId", media_id_);
	args.put("state", state);

	JGenerator serializer(nullptr);
	string serialized;

	if (!serializer.toString(args,  pbnjson::JSchema::AllSchema(), serialized)) {
		LOG_ERROR(_log, MSGERR_JSON_SERIALIZE, "Failed to serialize request.");
		return false;
	}

	LOG_DEBUG(_log, "contentReady %s", serialized.c_str());
	string cmd = mdc_uri_ + "/contentReady";
	return connector_->sendMessage(cmd, serialized, nullptr, this);
}

bool MDCContentProvider::setVideoInfo(const ums::video_info_t &video_info)
{
	JGenerator serializer(nullptr);
	string serialized;

	JValue args = Object();
	JValue video_info_json = Object();
	JValue frame_rate = Object();

	video_info_json.put("mediaId", media_id_);
	video_info_json.put("width", (int32_t)video_info.width);
	video_info_json.put("height", (int32_t)video_info.height);
	frame_rate.put("num", (int32_t)video_info.frame_rate.num);
	frame_rate.put("den", (int32_t)video_info.frame_rate.den);
	video_info_json.put("frameRate", frame_rate);
	video_info_json.put("codec", video_info.codec);
	video_info_json.put("bitRate", (int64_t)video_info.bit_rate);

	args.put("videoInfo", video_info_json);

	if (!serializer.toString(args,  pbnjson::JSchema::AllSchema(), serialized)) {
		LOG_ERROR(_log, MSGERR_JSON_SERIALIZE, "Failed to serialize request.");
		return false;
	}

	LOG_DEBUG(_log, "mediaVideoData %s", serialized.c_str());
	string cmd = mdc_uri_ + "/setVideoInfo";
	return connector_->sendMessage(cmd, serialized, nullptr, this);
}

bool MDCContentProvider::setVideoInfo(const video_info_t &video_info)
{
	JGenerator serializer(nullptr);
	string serialized;

	JValue args = Object();
	JValue video_info_json = Object();
	video_info_json.put("mediaId", media_id_);
	video_info_json.put("width", video_info.width);
	video_info_json.put("height", video_info.height);
	video_info_json.put("frameRate", video_info.frame_rate);
	video_info_json.put("bitRate", video_info.bit_rate);
	video_info_json.put("adaptive", video_info.adaptive);
	video_info_json.put("path", video_info.path);
	video_info_json.put("content", video_info.content);
	video_info_json.put("actual3D", video_info.video3d.current);
	video_info_json.put("mode3D", video_info.video3d.original);
	video_info_json.put("currentType", video_info.video3d.type_lr);
	video_info_json.put("scanType", video_info.scan_type);

	// pixel aspect ratio: convert width and height to string "1:1" format
	string par_width = to_string(video_info.pixel_aspect_ratio.width);
	string par_height = to_string(video_info.pixel_aspect_ratio.height);
	string par = par_width + ":" + par_height;
	video_info_json.put("pixelAspectRatio", par);

	args.put("videoInfo", video_info_json);

	if (!serializer.toString(args,  pbnjson::JSchema::AllSchema(), serialized)) {
		LOG_ERROR(_log, MSGERR_JSON_SERIALIZE, "Failed to serialize request.");
		return false;
	}

	LOG_DEBUG(_log, "mediaVideoData %s", serialized.c_str());
	string cmd = mdc_uri_ + "/setVideoInfo";
	return connector_->sendMessage(cmd, serialized, nullptr, this);
}

bool MDCContentProvider::registerPlaneIdCallback(plane_id_callback_t &&plane_id_cb)
{
	plane_id_callback_ = plane_id_cb;
	return true;
}

void * MDCContentProvider::messageThread(void *arg)
{
	MDCContentProvider *self = static_cast<MDCContentProvider *>(arg);
	self->connector_->wait();
	return NULL;
}

void MDCContentProvider::subscribe() {
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

void MDCContentProvider::unsubscribe() {
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

bool MDCContentProvider::notificationDispatch(UMSConnectorHandle *, UMSConnectorMessage * message, void* context) {
	auto _this = static_cast<MDCContentProvider*>(context);
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

	if (event == "SetPlane" && _this->plane_id_callback_) {
		_this->plane_id_callback_(value["planeId"].asNumber<int>());
	}

	return true;
}
