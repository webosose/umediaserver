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

#define BOOST_TEST_MODULE connector_test
#include <Logger_macro.h>
#include <unistd.h>
#include <sched.h>
#include "UMSConnector.h"
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include <sstream>
#include <sys/time.h>
#include <sys/resource.h>
#include <boost/test/included/unit_test.hpp>
#include <luna-service2/lunaservice.h>

using namespace uMediaServer;

const std::string mess[] = {"lol", "good cat", "add-stupid-results-for-you?", "exit"};
const std::string service_name = "com.webos.pipelinectrl.test";
UMSConnector* connector = NULL;

//--- Glib stuff starts here ---
bool flag = false;

void child_sa(int, siginfo_t*, void*) { flag = true; }

void ensure_child_sa_installed ()
{
    struct sigaction act;
    act.sa_sigaction = child_sa;
    act.sa_flags = SA_NOCLDWAIT;
    sigaction(SIGCHLD, &act, NULL);
}

gboolean source_prepare  (GSource *source, gint *timeout) { *timeout = 10; return flag; }
gboolean source_check    (GSource *source) { return flag; }
gboolean source_dispatch (GSource *source, GSourceFunc callback, gpointer user_data) { return callback(user_data); }

GSourceFuncs our_source = { source_prepare, source_check, source_dispatch, NULL };

GSource* g_childsig_watch_new (GSourceFunc cb, gpointer data, GDestroyNotify notify)
{
    GSource* src = g_source_new(&our_source, sizeof(GSource));
    g_source_set_callback(src, cb, data, notify);
    return src;
}

gboolean time_callback (gpointer data)
{
    g_main_loop_quit((GMainLoop*)data);
    BOOST_CHECK_MESSAGE(false, "test timeout");
    return true;
}

GMainLoop* create_gloop(GSourceFunc cb)
{
    GMainContext* ctx = g_main_context_new();
    GMainLoop* main_loop = g_main_loop_new(ctx, false);
    flag = false;
    ensure_child_sa_installed();
    g_source_attach(g_childsig_watch_new(cb, main_loop, (GDestroyNotify)g_main_loop_unref), ctx);
	GSource* src = g_timeout_source_new(5000);
	g_source_attach(src, ctx);
	g_source_set_callback(src, time_callback, main_loop, (GDestroyNotify)g_main_loop_unref);
    return main_loop;
}
//--- Glib stuff ends here ---

bool responseCallback (UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx) { return true; }

void client()
{
    std::thread t([] {
        std::string load_uri = service_name + "/load";
        for (int i = 0; i < sizeof(mess) / sizeof(*mess); ++i)
        {
            if (mess[i] == "exit") {
                connector->stop();
            } else {
                BOOST_CHECK(connector->sendMessage(load_uri, mess[i], responseCallback, connector));
                BOOST_CHECK(!connector->sendMessage(load_uri, "", responseCallback, connector));
            }
        }
    });
    connector->wait();
    t.join();
}

BOOST_AUTO_TEST_CASE (luna_register)
{
    connector = new UMSConnector(service_name); delete connector;
    connector = new UMSConnector(service_name); delete connector;
}

BOOST_AUTO_TEST_CASE (mainloop_racing)
{
    // as this is a race condition it can't be checked without some luck or underlying code changing
    for (int i = 0; i < 1000; ++i) {
        GMainContext *  ctx = g_main_context_new();
        GMainLoop    * loop = g_main_loop_new(ctx, false);
        g_main_context_unref(ctx);
        UMSConnector c(service_name, loop);
        g_main_loop_unref(loop);

        std::thread t([&] { c.wait(); });
        c.stop();
        t.join();
    }
    BOOST_CHECK("phase one");
    for (int i = 0; i < 1000; ++i) {
        GMainContext *  ctx = g_main_context_new();
        GMainLoop    * loop = g_main_loop_new(ctx, false);
        g_main_context_unref(ctx);
        UMSConnector c(service_name, loop);
        g_main_loop_unref(loop);

        std::thread t([&] { c.stop(); });
        c.wait();
        t.join();
    }
    BOOST_CHECK("phase two");
	// case with shared main context
	GMainContext * main_context = g_main_context_ref_thread_default();
	GMainLoop * main_loop = g_main_loop_new(main_context, false);
	std::thread runner([&](){
		g_main_loop_run(main_loop);
	});
	// wait until loop runs
	while(!g_main_loop_is_running(main_loop))
		sched_yield();
	std::vector<UMSConnector *> connectors;
	for (int i = 0; i < 100; ++i) {
		std::stringstream connector_service_name(service_name);
		connector_service_name << "_" << i;
		connectors.push_back(new UMSConnector(connector_service_name.str(), nullptr,
			this, UMS_CONNECTOR_PRIVATE_BUS, true));
	}
	for (auto connector : connectors) {
		delete connector;
	}
	// none of the attached connectors should stop main loop
	BOOST_CHECK(g_main_loop_is_running(main_loop));
	g_main_loop_quit(main_loop);
	g_main_context_unref(main_context);
	g_main_loop_unref(main_loop);
	runner.join();
}

bool load_callback (UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx)
{
    std::string m = connector->getMessageText(message);
	std::cout << "load request: " << m << std::endl;
    static int i = 0;
    BOOST_CHECK_EQUAL(m, mess[i++]);
    BOOST_CHECK(connector->sendResponseObject(handle, message, m));
    return true;
}

gboolean conn_stop (gpointer data)
{
    connector->stop();
    return false;
}

struct ForkingChildFixture
{
    pid_t pid;

	ForkingChildFixture () {
		pid = fork();
		connector = new UMSConnector(pid ? service_name : "", create_gloop(conn_stop),
									 nullptr, UMS_CONNECTOR_PRIVATE_BUS);
    }
	~ForkingChildFixture () {
        delete connector;
        if (!pid)
            exit(0);
    }
};

BOOST_FIXTURE_TEST_CASE(handler_test, ForkingChildFixture)
{
    if (pid) {
        BOOST_CHECK(connector->addEventHandler("load", load_callback));
        BOOST_CHECK(connector->wait());
    }
    else
        client();
}

const std::string notification = "you shouldn't see this";

bool subscribeCallback(UMSConnectorHandle* subscriber, UMSConnectorMessage* message, void* ctx)
{
	std::cout << "got subscription request: " << connector->getMessageText(message) << std::endl;
    BOOST_CHECK((connector->addSubscriber(subscriber, message, "")));
	BOOST_CHECK(connector->sendChangeNotificationJsonString(notification, ""));
    return true;
}

static size_t token = 0;
static std::string uri = service_name + "/subscribe";

bool responseCallback_t (UMSConnectorHandle*, UMSConnectorMessage* message, void* ctx)
{
	std::string m = connector->getMessageText(message);
	std::cout << "notification is: " << m << std::endl;
	BOOST_CHECK_NE(m, notification);
	connector->unsubscribe(uri, token);
    connector->stop();
    return true;
}

BOOST_FIXTURE_TEST_CASE (client_test, ForkingChildFixture)
{
    if (pid) {
        BOOST_CHECK(connector->addEventHandler("subscribe", subscribeCallback));
        connector->wait();
	} else {
		token = connector->subscribe(uri, "{}", responseCallback_t, connector);
		BOOST_CHECK(token);
        connector->wait();
    }
}

const int expected_cb_calls = 10;

void service_status_callback() {
	static int cb_calls = 0;
	++cb_calls;
	if (cb_calls == expected_cb_calls)
		connector->stop();
}

BOOST_AUTO_TEST_CASE (service_status) {
	const int TIMEOUT = 500000; /* 500 ms */
	pid_t pid = fork();
	if (pid) { /* service*/
		std::thread shooter([](){
			for (int c = 0; c < expected_cb_calls; ++c) {
				usleep(TIMEOUT);
				delete connector;
			}
		});
		for (int c = 0; c < expected_cb_calls; ++c ) {
			connector = new UMSConnector(service_name, create_gloop(conn_stop), nullptr, UMS_CONNECTOR_PRIVATE_BUS);
			connector->wait();
		}
		shooter.join();
	} else { /* client */
		connector = new UMSConnector("", create_gloop(conn_stop), nullptr, UMS_CONNECTOR_PRIVATE_BUS);
		connector->subscribeServiceReady(service_name, service_status_callback);
		connector->wait();
	}
}
