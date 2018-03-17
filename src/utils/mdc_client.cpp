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
// test client using uMediaServer API
//
// Listen Only clients may use MediaChangeListener independently
//

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
#include <list>

#include <pthread.h>
#include "MDCClient.h"

#include <boost/lexical_cast.hpp>  // for lexical_cast<string>(number)
#include <boost/algorithm/string.hpp>

using namespace std;
using namespace boost;
using namespace uMediaServer;

// ---
// MAIN

int main(int argc, char *argv[])
{
	string cmd;
	bool exit = false;

	MDCClient mdc_client;

	printf("MDCClient : START\n");

	try {

		while( !exit ) {
			printf("COMMANDS: registerMedia <media_id> <app_id>, unregisterMedia setFocus, setAudioFocus, setVideoFocus,"
					" setDisplayWindow <output>, setDisplayWindow <input> <output>, switchToFullscreen\n");
			printf("\tenter command : ");
			getline(cin, cmd);

			// split command into arguments
			vector<string> args;
			args.clear();
			split(args, cmd, is_any_of(" "));   // boost string algorithm

			if(args[0] == "registerMedia" ) {
				printf("command :  %s\n",cmd.c_str());
				if(args.size() < 3) {
					printf("ERROR :  <media_id> <app_id> required.\n");
					continue;
				}

				mdc_client.registerMedia(args[1], args[2]);
			}
			else if(args[0] == "unregisterMedia" ) {
				printf("command :  %s\n",cmd.c_str());
				mdc_client.unregisterMedia();
			}
			else if(args[0] == "switchToFullScreen" ) {
				printf("command :  %s\n",cmd.c_str());
				mdc_client.switchToFullscreen();
			}
			else if(args[0] == "setDisplayWindow" ) {
				printf("command :  %s\n",cmd.c_str());
				int x, y, width, height;
				if(args.size() == 5) {
					x = boost::lexical_cast<float>(args[1]);
					y = boost::lexical_cast<float>(args[2]);
					width = boost::lexical_cast<float>(args[3]);
					height = boost::lexical_cast<float>(args[4]);

					window_t output(x, y, width, height);
					mdc_client.setDisplayWindow(output);
				}
				else if(args.size() == 9) {
					x = boost::lexical_cast<float>(args[1]);
					y = boost::lexical_cast<float>(args[2]);
					width = boost::lexical_cast<float>(args[3]);
					height = boost::lexical_cast<float>(args[4]);
					window_t output(x, y, width, height);

					x = boost::lexical_cast<float>(args[5]);
					y = boost::lexical_cast<float>(args[6]);
					width = boost::lexical_cast<float>(args[7]);
					height = boost::lexical_cast<float>(args[8]);
					window_t input(x, y, width, height);

					mdc_client.setDisplayWindow(input, output);
				}
				else {
					printf("ERROR: usage<input and output>:\n"
							" setDisplayWindow 0 0 400 300 0 0 600 400  (input and output windows)\n"
							"       OR\n"
							" setDisplayWindow 0 0 400 300 (output window)\n");
					continue;
				}
			}
			else if(args[0] == "setFocus" ) {
				printf("command :  %s\n",cmd.c_str());
				mdc_client.setFocus();
			}
			else if(args[0] == "exit"
					|| args[0] == "quit"
					|| args[0] == "q") {
				exit = true;
			}
		}
	}
	catch (std::exception& e) {
		printf("Exception Received: %s\n", e.what());
	}

	printf("\nMDC Client exiting :  '%s'\n",cmd.c_str());

	return 0;

}
