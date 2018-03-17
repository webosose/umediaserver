// Copyright (c) 2017-2018 LG Electronics, Inc.
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

#include <pbnjson.hpp>
#include <Logger_macro.h>
#include "PowerManager.h"

namespace uMediaServer { namespace pwr {

Logger _log(UMS_LOG_CONTEXT_SERVER);

struct PowerCallbackInfo {
	PowerCallbackInfo(const char * reg, const char * res) : register_uri(reg), response_uri(res) {}
	const char * register_uri;
	const char * response_uri;
	PowerManager * mgr = nullptr;
}	suspend {
	"palm://com.webos.service.tvpower/power/registerPrepareSuspend",
	"palm://com.webos.service.tvpower/power/responsePrepareSuspend"
},	standby {
	"palm://com.webos.service.tvpower/power/registerPrepareActiveStandby",
	"palm://com.webos.service.tvpower/power/responsePrepareActiveStandby"
};

PowerManager::PowerManager(UMSConnector * connector) : connector_(connector) {
	auto subscribe = [this](PowerCallbackInfo & cb_nfo) -> bool {
		pbnjson::JValue args = pbnjson::JObject {{"clientName", "com.webos.media"}, {"subscribe", true}};
		pbnjson::JGenerator gen(nullptr);
		std::string payload;
		if (!gen.toString(args, pbnjson::JSchema::AllSchema(), payload)) {
			LOG_ERROR(_log, MSGERR_JSON_SERIALIZE, "failed to serialize payload.");
			return false;
		}
		cb_nfo.mgr = this;
		return connector_->subscribe(cb_nfo.register_uri, payload,
									 [] (UMSConnectorHandle *, UMSConnectorMessage * msg, void * ctx) {
			PowerCallbackInfo * nfo = static_cast<PowerCallbackInfo *>(ctx);
			pbnjson::JDomParser parser;
			bool parse_result = parser.parse(nfo->mgr->connector_->getMessageText(msg), pbnjson::JSchema::AllSchema());
			if (!parse_result) {
				LOG_ERROR(_log, MSGERR_JSON_PARSE, "failed to parse json. raw=%s",
						  nfo->mgr->connector_->getMessageText(msg));
				return false;
			}
			std::string timestamp = parser.getDom()["timestamp"].asString();
			if (timestamp.empty()) return false;

			if (nfo->mgr->shutdown_callback_)
				nfo->mgr->shutdown_callback_();

			pbnjson::JValue args = pbnjson::JObject{{"clientName", "com.webos.media"},
													{"timestamp", timestamp},
													{"ack", true}};
			pbnjson::JGenerator gen(nullptr);
			std::string payload;
			if (!gen.toString(args, pbnjson::JSchema::AllSchema(), payload)) {
				LOG_ERROR(_log, MSGERR_JSON_SERIALIZE, "failed to serialize payload.");
				return false;
			}
			return nfo->mgr->connector_->sendMessage(nfo->response_uri, payload, nullptr, nullptr);
		}, & cb_nfo);
	};
	subscribe(suspend);
	subscribe(standby);
}

void PowerManager::set_shutdown_handler(callback_t && callback) {
	shutdown_callback_ = callback;
}

}} // namespace uMediaServer::pwr
