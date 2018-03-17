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

#include <iostream>
#include <string>
#include <iomanip>
#include <thread>
#include <pbnjson.h>
#include <pbnjson.hpp>
#include <UMSConnector.h>
#include <GenerateUniqueID.h>
#include <boost/lexical_cast.hpp>
#include <list>

using namespace std;
using namespace pbnjson;
using namespace uMediaServer;

static const char* UMS_TOOLS_CONNECTION_BASE_ID = "com.webos.ums-status.";
static const char* CMD_GETACTIVE_PIPELINES = "palm://com.webos.media/getActivePipelines";
UMSConnector* connector;
#define ALIGN_INDENT 30

// This pretty_print function comes from luna-service component.
#define INDENT_INCREMENT 4
static void pretty_print(jvalue_ref object, int first_indent, int indent)
{
	if (!object) {
		printf("%*s<NULL>", first_indent, "");
		return;
	}

	if (!jis_array(object) && !jis_object(object))
	{
		printf("%*s%s", first_indent, "", jvalue_tostring_simple(object));
	}
	else if (jis_array(object))
	{
		int len = jarray_size(object);
		int i;
		printf("%*s[", first_indent, "");
		bool first = true;
		for (i=0;i<len;i++) {
			if (first) {
				printf("\n");
				first = false;
			} else {
				printf(",\n");
			}
			pretty_print(jarray_get(object, i), indent + INDENT_INCREMENT, indent + INDENT_INCREMENT);
		}
		printf("\n%*s]", indent, "");
	}
	else if (jis_object(object))
	{
		printf("%*s{", first_indent, "");
		bool first = true;
		jobject_iter it;
		(void)jobject_iter_init(&it, object);/* TODO: handle appropriately */
		jobject_key_value keyval;
		while (jobject_iter_next(&it, &keyval)) {
			if (first) {
				printf("\n");
				first = false;
			} else {
				printf(",\n");
			}
			// FIXME: contents of key are not being escaped
			raw_buffer key = jstring_get_fast(keyval.key);
			printf("%*s\"%.*s\": ", indent+INDENT_INCREMENT, "", (int)key.m_len, key.m_str);
			pretty_print(keyval.value, 0, indent + INDENT_INCREMENT);
		}
		printf("\n%*s}", indent, "");
	}
	else
	{
		printf("%*s<unknown json type>", first_indent, "");
	}
}


void print_left(int width, const string& text)
{
	ios::fmtflags f( cout.flags() );
	cout << std::left << std::setw(width) << text;
	cout.flags( f );
}


void print_right(int width, const string& text)
{
	ios::fmtflags f( cout.flags() );
	cout << std::right << std::setw(width) << text;
	cout.flags( f );
}

bool cb_(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx)
{
	string msg_received = connector->getMessageText(message);

	JDomParser parser;
	typedef struct {
		int i;
		int64_t timestamp;
	} index_time_map_t;
	std::list<index_time_map_t> time_index_map;

	auto mru_to_lru = [&](const index_time_map_t a,
			const index_time_map_t b)->bool {
		return (a.timestamp > b.timestamp);
	};

	if (!parser.parse(msg_received, pbnjson::JSchema::AllSchema())) {
		cout << "JDomParse.parse Error." << msg_received << endl;
		return false;
	}

	JValue parsed = parser.getDom();

	// no pipeline
	if (!parsed.isArray()) {
		cout << "No pipelines" << endl;
		return true;
	}

	for (int i=0; i < parsed.arraySize(); ++i) {
		index_time_map_t map_entry;
		map_entry.i = i;
		map_entry.timestamp = parsed[i]["timestamp"].asNumber<int64_t>();
		time_index_map.push_back(map_entry);
	}

	time_index_map.sort(mru_to_lru);

	int k = 1;
	for (auto it : time_index_map)
	{
		string id = parsed[it.i]["id"].asString();
		string type = parsed[it.i]["type"].asString();
		bool is_managed = parsed[it.i]["is_managed"].asBool();
		bool is_foreground = parsed[it.i]["is_foreground"].asBool();
		bool is_focus = parsed[it.i]["is_focus"].asBool();
		bool selected_by_policy = parsed[it.i]["selected_by_policy"].asBool();
		JValue resource = parsed[it.i]["resource"];

		cout << endl << "[ Pipeline : " << k++ << " ]" << endl << endl;

		print_right(ALIGN_INDENT, "id : "); print_left(ALIGN_INDENT, id);
		cout << endl;
		print_right(ALIGN_INDENT, "type : "); print_left(ALIGN_INDENT, type);
		cout << endl;
		print_right(ALIGN_INDENT, "is_managed : "); print_left(ALIGN_INDENT, (is_managed ? "true" : "false"));
		cout << endl;
		print_right(ALIGN_INDENT, "is_foreground : "); print_left(ALIGN_INDENT, (is_foreground ? "true" : "false"));
		cout << endl;
		print_right(ALIGN_INDENT, "is_focus: "); print_left(ALIGN_INDENT, (is_focus? "true" : "false"));
		cout << endl;
		print_right(ALIGN_INDENT, "selected_by_policy : "); print_left(ALIGN_INDENT, (selected_by_policy ? "true" : "false"));
		cout << endl;

		// print resources
		print_right(ALIGN_INDENT, "resources : ");
		std::stringstream stream;
		if (resource.isArray()) {
			for (int j=0; j < resource.arraySize(); ++j) {
				stream << resource[j]["resource"].asString() << "[" << resource[j]["index"].asNumber<int32_t>() << "] ";
			}
			print_left(ALIGN_INDENT, stream.str());
		}
		cout << endl; cout << endl;

		if (is_managed) {
			string uri = parsed[it.i]["uri"].asString();
			int32_t pid = parsed[it.i]["pid"].asNumber<int32_t>();
			string processState = parsed[it.i]["processState"].asString();
			string mediaState = parsed[it.i]["mediaState"].asString();
			string appId = parsed[it.i]["appId"].asString();

			print_right(ALIGN_INDENT, "uri : "); print_left(ALIGN_INDENT, uri);
			cout << endl;
			print_right(ALIGN_INDENT, "pid : "); print_left(ALIGN_INDENT, boost::lexical_cast<string>(pid));
			cout << endl;
			print_right(ALIGN_INDENT, "processState : "); print_left(ALIGN_INDENT, processState);
			cout << endl;
			print_right(ALIGN_INDENT, "mediaState : "); print_left(ALIGN_INDENT, mediaState);
			cout << endl;
			print_right(ALIGN_INDENT, "appId : "); print_left(ALIGN_INDENT, appId);
			cout << endl;
		}
	}

	return true;
}


int main(int argc, char *argv[])
{
	string input = "";

	string uid = "";

	try {
		uid = GenerateUniqueID()();
	}
	catch (const std::exception& e) {
		cout << "Exception while calling GenerateUniqueID: " << e.what() << endl;
	}

	string connection_id = UMS_TOOLS_CONNECTION_BASE_ID + uid;
	connector = new UMSConnector(connection_id, NULL, NULL, UMS_CONNECTOR_PRIVATE_BUS);

	try {
		std::thread([&] {
			connector->wait();
		}).detach();
	}
	catch (const std::exception& e) {
		cout << "Exception while calling detach: " << e.what() << endl;
	}

	pbnjson::JValue args = pbnjson::Object();
	JGenerator serializer(NULL);
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		cout << "failed serializer.toString()" << endl;
		return 0;
	}

	cout << "Test program start. type \"q\" for quit" << endl << endl;

	while (true) {
		bool rv = connector->sendMessage(CMD_GETACTIVE_PIPELINES, payload_serialized, cb_, NULL /*ctx*/);
		if (false == rv) {
			cout << "msg send fail" << endl;
			continue;
		}

		try {
			getline(cin, input);
		}
		catch (const std::exception& e)
		{
			cout << "Exception caught calling getline: " << e.what() << endl;
		}

		if (input == "exit" || input == "quit" || input == "q") {
			connector->stop();
			break;
		}
		cin.clear();
	}

    cout << "Test program end" << endl;

    return 0;
}

