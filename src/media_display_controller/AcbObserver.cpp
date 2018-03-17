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

#include <pbnjson.hpp>
#include <Logger_macro.h>
#include "ControlInterface.h"
#include "AcbObserver.h"

namespace uMediaServer {

namespace {
	Logger _log;
}

AcbObserver::AcbObserver(UMSConnector * umc) : connector(umc) {
	connector->subscribe(ControlInterface::acb_service_observer, "{\"subscribe\":true}",
						 [](UMSConnectorHandle *, UMSConnectorMessage * message, void * ctx)->bool {
		AcbObserver * self = static_cast<AcbObserver *>(ctx);
		std::string event = self->connector->getMessageText(message);
		pbnjson::JDomParser parser;
		if(!parser.parse(event, pbnjson::JSchema::AllSchema())) {
			LOG_ERROR(_log, MSGERR_JSON_PARSE, "ERROR parser failure raw=%s ", event.c_str());
			return false;
		}
		pbnjson::JValue dom = parser.getDom();
		if(!dom.hasKey("appId") || !dom.hasKey("acbs")) {
			LOG_ERROR(_log, MSGERR_JSON_PARSE, "ERROR malformed json raw=%s ", event.c_str());
			return false;
		}
		if (dom["acbs"].arraySize()) {
			self->blacklist.insert(dom["appId"].asString());
		}
	}, this);
}

bool AcbObserver::check_blacklist(const std::string & app) const {
	return blacklist.find(app) != blacklist.end();
}

}
