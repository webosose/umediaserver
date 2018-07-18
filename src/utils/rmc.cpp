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
// test client using Resource Manager API
//

#include <iostream>
#include <stdexcept>
#include <vector>
#include <sstream>
#include <stdio.h>
#include <cassert>
#include <string>
#include <algorithm>
#include <iterator>

#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <deque>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <Logger_macro.h>
#include <ResourceManagerClient.h>

#include <boost/lexical_cast.hpp>  // for lexical_cast<string>(number)
#include <boost/algorithm/string.hpp>

using namespace std;
using namespace boost;
using namespace uMediaServer;

ResourceManagerClient rmc; // {GenerateUniqueID()()}; // test as non-managed client
bool allowPolicy = true;

// resource + index
std::list<std::pair<std::string, size_t>> acquired_resources;

void acquireResources(const std::string & acquired) {
	std::stringstream json(acquired);
	boost::property_tree::ptree pt;
	boost::property_tree::read_json(json, pt);
	if (pt.get<bool>("state")) {
		for (const auto & res : pt.get_child("resources")) {
			acquired_resources.push_back({
				res.second.get<std::string>("resource"),
				res.second.get<size_t>("index")
			});
		}
	}
}

void releaseResources(const std::string & released) {
	std::stringstream json(released);
	boost::property_tree::ptree pt;
	std::stringstream fake; fake << "{\"root\":" << released << "}";
	boost::property_tree::read_json(fake, pt);
	for (const auto & requested : pt.get_child("root")) {
		std::string id = requested.second.get<std::string>("resource");
		size_t quantity = requested.second.get<size_t>("qty");
		size_t index = requested.second.get<size_t>("index");
		while(quantity--){
			for (auto it = acquired_resources.begin(); it != acquired_resources.end(); ++it) {
				if (it->first == id && (index == (size_t)(-1) || index == it->second)) {
					acquired_resources.erase(it);
					break;
				}
			}
		}
	}
}

void applyPolicy(const std::string & release) {
	std::stringstream json(release);
	// bloody ptree json parser
	std::stringstream fake; fake << "{\"root\":" << release << "}";
	std::stringstream request; request << "[";
	const char * sep = "";
	boost::property_tree::ptree pt;
	boost::property_tree::read_json(fake, pt);
	for (const auto & requested : pt.get_child("root")) {
		std::string id = requested.second.get<std::string>("resource");
		size_t quantity = requested.second.get<size_t>("qty");
		size_t index = requested.second.get<size_t>("index");
		while(quantity--) {
			for (auto it = acquired_resources.begin(); it != acquired_resources.end(); ++it) {
				if (it->first == id && (index == (size_t)(-1) || index == it->second)) {
					request << sep; sep = ",";
					request << "{\"resource\":\"" << id << "\",\"qty\":1,\"index\":" << it->second << "}";
					acquired_resources.erase(it);
					break;
				}
			}
		}
	}
	request << "]";
	rmc.release(request.str());
}

bool policyActionEvent(const char * action, const char * resources,
		const char * requestor_type, const char * requestor_name,
		const char * connection_id)
{
	printf("POLICY_ACTION: action=%s, resources=%s, requestor_type=%s,"
			"requestor_name=%s, connection_id=%s\n",
			action,resources,requestor_type,requestor_name,connection_id);

	if (allowPolicy) {
		applyPolicy(resources);
		printf("policy accepted - resources released\n");
	}
	else {
		printf("policy denied - resources not released\n");
	}
	return allowPolicy;
}

int main(int argc, char *argv[])
{
	string cmd;
       bool result = false;
	bool exit = false;

	printf("ResourceManagerClient : START\n");

	rmc.registerPolicyActionHandler(policyActionEvent);

	while( !exit ) {
		printf("COMMANDS:\n"
				"\t'registerPipeline <type>\n"
				"\t'unregisterPipeline'\n"
				"\t'acquire <resource_list>' eg: acquire [{\"resource\":\"VDEC\",\"qty\":1}]\n"
				"\t'tryAcquire <resource_list>' eg: tryAcquire [{\"resource\":\"VDEC\",\"qty\":1}]\n"
                "\t'release <indexed_resource_list>' eg: release [{\"resource\":\"VDEC\",\"qty\":1,\"index\":0}]\n"
                "\t'policy (a)llow|(d)eny\n"
                "\t'notifyForeground\n"
                "\t'notifyBackground\n"
                "\t'notifyActivity\n"
				"\t'exit' :  \n\n");

		printf("\t enter command : ");
		getline(cin, cmd);

		// split command into arguments
		vector<string> args;
		args.clear();
		split(args, cmd, is_any_of(" "));   // boost string algorithm

		if(args[0] == "registerPipeline"
				|| args[0] == "rp" ) {
			printf("command :  %s\n",cmd.c_str());

			string type = "ref";  // default to file
			if( args.size() > 1 ) {
				type = args[1];
			}
			result = rmc.registerPipeline(type);
		}
		else if( args[0] == "unregisterPipline" || args[0] == "up" ) {
			printf("command :  %s\n",cmd.c_str());
			result = rmc.unregisterPipeline();
		}
		else if( args[0] == "acquire" || args[0] == "a" ) {
			printf("command :  %s\n",cmd.c_str());
			if( args.size() <= 1 ) {
				printf(" ERROR: acquire command requires resource list.\n");
				continue;
			}
			try {
				std::string request = args[1];
				std::string response;
				result = rmc.acquire(request,response);
				acquireResources(response);
				printf("ACQUIRE RESPONSE :  %s\n",response.c_str());
			} catch(const std::exception & e) {
				printf(" ERROR: acquire command failed: %s\n", e.what());
			}

		}
		else if( args[0] == "tryAcquire" || args[0] == "ta" ) {
			printf("command :  %s\n",cmd.c_str());
			if( args.size() <= 1 ) {
				printf(" ERROR: tryAcquire command requires resource list.\n");
				continue;
			}
			try {
				std::string request = args[1];
				std::string response;
				result = rmc.tryAcquire(request,response);
				acquireResources(response);
				printf("ACQUIRE RESPONSE :  %s\n",response.c_str());
			} catch(const std::exception & e) {
				printf(" ERROR: acquire command failed: %s\n", e.what());
			}

		}
		else if( args[0] == "release" || args[0] == "r" ) {
			printf("command :  %s\n",cmd.c_str());
			if( args.size() <= 1 ) {
				printf(" ERROR: release command requires resource list.\n");
				continue;
			}
			try {
				std::string request = args[1];
				result = rmc.release(request);
				releaseResources(request);
			} catch (const std::exception & e) {
				printf(" ERROR: release command failed: %s\n", e.what());
			}
		}
		else if( args[0] == "policy" || args[0] == "p" ) {
			printf("command :  %s\n",cmd.c_str());
			if( args.size() <= 1 ) {
				printf(" ERROR: policy command requires switch (a)llow or (d)eny.\n");
				continue;
			}
			if ( args[1][0] == 'd' /*deny*/ )
				allowPolicy = false;
			else if ( args[1][0] == 'a' /*allow*/ )
				allowPolicy = true;
			else
				printf(" ERROR: policy command requires switch (a)llow or (d)eny.\n");
		}
		else if( args[0] == "notifyActivity" || args[0] == "na" ) {
			printf("command :  %s\n",cmd.c_str());
			try {
				result = rmc.notifyActivity();
			} catch (const std::exception & e) {
				printf(" ERROR: notifyActivity command failed: %s\n", e.what());
			}
		}
		else if( args[0] == "notifyForeground" || args[0] == "fg" ) {
			printf("command :  %s\n",cmd.c_str());
			try {
				result = rmc.notifyForeground();
			} catch (const std::exception & e) {
				printf(" ERROR: notifyForeground command failed: %s\n", e.what());
			}
		}
		else if( args[0] == "notifyBackground" || args[0] == "bg" ) {
			printf("command :  %s\n",cmd.c_str());
			try {
				result = rmc.notifyBackground();
			} catch (const std::exception & e) {
				printf(" ERROR: notifyBackground command failed: %s\n", e.what());
			}
		}
		else if( args[0] == "exit") {
			printf("command :  %s\n",cmd.c_str());
			exit = true;
		}
		else {
			printf("UNKNOWN COMMAND :  '%s'\n",cmd.c_str());
                       continue;
		}

               printf("result : %s\n", result?"true":"false");
	}

	printf("\nResource Manager Client API exiting :  '%s'\n",cmd.c_str());

	return 0;

}

