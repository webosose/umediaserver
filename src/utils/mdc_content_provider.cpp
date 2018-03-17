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
#include "MDCContentProvider.h"

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

	try {
		printf("MDCContentProvider : START\n");

		if(argc < 2) {
			printf("ERROR : <media_id> required.\n");
			return false;
		}

		MDCContentProvider mdccp(argv[1]);

		while( !exit ) {
			printf("COMMANDS: contentReady <true/false>\n");
			printf("\tenter command : ");
			getline(cin, cmd);

			vector<string> args;
			args.clear();

				split(args, cmd, is_any_of(" "));   // boost string algorithm

			if(args[0] == "contentReady" ) {
				bool state;
				args[1] == "false" ? state=false: state=true;
				mdccp.mediaContentReady(state);
				printf("command :  %s\n",cmd.c_str());
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
