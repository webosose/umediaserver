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


#include <uMediaserver.h>

// uMS project utilities
#include <Logger_macro.h>
#include <PipelineManager.h>
#include <Pipeline.h>
#include <memory>
#include "UMSConnector.h"

using namespace std;
using namespace uMediaServer;

namespace {
	Logger _log(UMS_LOG_CONTEXT_SERVER);
}

//
// ------------- Let it begin ----------------
//
int main(int argc, char** argv)
{
	GError *gerror = NULL;
	char *conf_file =  NULL;
	static GOptionEntry opt_entries[] = {
		{"conf", 'c', 0, G_OPTION_ARG_FILENAME, &conf_file, "MANDATORY: Path to umediaserver resource file",
			"/some/path/umediaserver_resource_config.txt"},
		{NULL}
	};
	GOptionContext *opt_context = g_option_context_new("- uMediaserver");
	g_option_context_add_main_entries(opt_context, opt_entries, NULL);
	if (!g_option_context_parse(opt_context, &argc, &argv, &gerror)) {
		LOG_CRITICAL_EX(_log, MSGERR_CLARG_PARSE, __KV({{KVP_ERROR, gerror->message}}),
			"Error processing commandline args: \"%s\"", gerror->message);
		g_error_free(gerror);
		exit(EXIT_FAILURE);
	}
	g_option_context_free(opt_context);
	if (NULL == conf_file) {
		LOG_CRITICAL(_log, MSGERR_NO_RES_CONF,
			"Mandatory uMediaserver resource file (-c/--conf) not provided!");
		exit(EXIT_FAILURE);
	}

	LOG_TRACE(_log, "BEGIN: umediaserver");

	// block SIGPIPE that could be eventually raised by luna-bus
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGPIPE);
	pthread_sigmask(SIG_BLOCK, &set, NULL);

	try {
		std::unique_ptr<uMediaserver> ums(uMediaserver::instance(conf_file));
		ums->wait();
	}
	catch (const std::exception & e) {
		LOG_CRITICAL(_log, MSGERR_SRV_OPERATE, "MediaServer failure: %s", e.what());
		return -1;
	}
	catch (...) {
		LOG_CRITICAL(_log, MSGERR_SRV_OPERATE, "MediaServer failure: unexpected error");
		return -1;
	}

	LOG_TRACE(_log, "END : umediaserver");
	return 0;
}
