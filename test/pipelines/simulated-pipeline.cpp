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

#include <time.h>

#include <cstdarg>
#include <string>
#include <vector>
#include <map>
#include <utility>
#include <mutex>
#include <thread>
#include <unistd.h>
#include <algorithm>
#include <iterator>
#include <sstream>
#include <pbnjson.hpp>

#include <Logger_macro.h>
#include "UMSConnector.h"

#define us_to_ms         1000ULL
#define u1_000_000       1000000ULL
#define u1_000_000_000   1000000000ULL
#define u10_000_000_000  10000000000ULL

namespace {
    uMediaServer::Logger _log(UMS_LOG_CONTEXT_PIPELINE);
};

class Timer;
class PipelineWorker;
class PipelineContext;

// === Timer
class Timer {
private:
    bool running_;
    unsigned long long elapsed_;
    struct timespec mark_;

    void mark_time ();

public:
    explicit
    Timer(bool run = true);
    void start  ();
    void stop   ();

    void set (long long val);
    unsigned long long elapsed ();
};

// === Pipeline Worker
class PipelineWorker {
private:
    bool play_;
    long long duration_;

    Timer timer_;

    PipelineContext& ctx_;
    std::mutex m_op_;
    std::thread timer_thr_;
    bool stop_;

public:
    PipelineWorker (PipelineContext& c);
    ~PipelineWorker ();

    bool play  ();
    bool pause ();
    bool seek  (long long pos_ms);
};

// === Pipeline Context
class PipelineContext {
public:
	std::string         name;
    const char*         cname;
    PipelineWorker*     player;
    UMSConnector*       connector;
    bool                subscribed;
    std::vector<std::string> queue;

    PipelineContext(std::string& s);

    void addSubscriber (UMSConnectorHandle* sender, UMSConnectorMessage* message);
    bool load (const char* m);
    bool unload ();

    template<class... Args>
    void sendNotify (const char* type, Args&&... args);
    void sendStateUpdate (const char* state_name, const bool state_val);
};

// --- Timer impl
Timer::Timer(bool run) :
    running_(run), elapsed_(0)
{
    mark_time();
}

void Timer::mark_time () { clock_gettime(CLOCK_MONOTONIC, &mark_); }

void Timer::start () {          mark_time(); running_ = true;  }
void Timer::stop  () { elapsed_ = elapsed(); running_ = false; }

void Timer::set (long long val) { elapsed_ = val; }

unsigned long long Timer::elapsed ()
{
    struct timespec now;
    unsigned long long past = 0;

    if (running_) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        past = now.tv_sec - mark_.tv_sec;
        past *= u1_000_000_000;
        past += now.tv_nsec - mark_.tv_nsec;
    }

    return elapsed_ + past;
}

// --- Pipeline Worker impl
typedef std::lock_guard<std::mutex> _guard;
PipelineWorker::PipelineWorker (PipelineContext& c) :
    ctx_(c), timer_(false), play_(false), stop_(false), duration_(u10_000_000_000)
{
	usleep(20 * us_to_ms);
	ctx_.sendNotify("sourceInfo", duration_/u1_000_000);
	timer_thr_ = std::thread([this] {
        const int rate = 25; // Hz
        const int step_us = u1_000_000/rate;

        while (!stop_ && usleep(step_us) == 0) {
            _guard _(m_op_);
            if (play_) {
                if (timer_.elapsed() > duration_) {
                    play_ = false;
                    timer_.stop();
                    timer_.set(0);
                    ctx_.sendStateUpdate("endOfStream", true);
                    ctx_.sendStateUpdate("paused", true);
                } else {
                    ctx_.sendNotify("currentTime", "currentTime", timer_.elapsed()/u1_000_000);
                }
            }
        }
    });
	ctx_.sendStateUpdate("loadCompleted", true);
}

PipelineWorker::~PipelineWorker ()
{
    pause();
    stop_ = true;
    timer_thr_.join();
}

bool PipelineWorker::play  () { _guard _(m_op_); play_ = true;  timer_.start(); return true; }
bool PipelineWorker::pause () { _guard _(m_op_); play_ = false; timer_.stop();  return true; }
bool PipelineWorker::seek  (long long pos_ms)
{
    if (pos_ms < 0 || u1_000_000 * pos_ms > duration_)
        return false;

    _guard _(m_op_);
    timer_.set(u1_000_000 * pos_ms);
    timer_.start();
    return true;
}

// --- Pipeline Context impl
PipelineContext::PipelineContext(std::string& s) : subscribed(false)
{
    _log.setUniqueId(name);
    connector = new UMSConnector(s, NULL, this, UMS_CONNECTOR_PRIVATE_BUS);
}

void PipelineContext::addSubscriber (UMSConnectorHandle* sender, UMSConnectorMessage* message)
{
    connector->addSubscriber(sender, message);
    subscribed = true;
    for (auto& m : queue)
        connector->sendChangeNotificationJsonString(m.c_str());
    queue.clear();
}

bool PipelineContext::load (const char* m) {
	pbnjson::JDomParser parser;
	parser.parse(m, pbnjson::JSchema::AllSchema());

#ifdef UMS_INTERNAL_API_VERSION
#if UMS_INTERNAL_API_VERSION == 2
	name = parser.getDom()["id"].asString();
#endif
#else
	name = parser.getDom()["args"][2].asString();
#endif

	cname = name.c_str();

	player = new PipelineWorker(*this);
	return true;
}
bool PipelineContext::unload ()            { delete player; return true; }

template<class... Args>
void PipelineContext::sendNotify (const char* type, Args&&... args)
{
    static std::map<std::string, const char*> formats = {
        { "state",          "{ \"%3$s\" : {\"%2$s\":%4$s,\"mediaId\":\"%1$s\"} }" },
        { "currentTime",    "{ \"%3$s\" : {\"%2$s\":%4$lld,\"mediaId\":\"%1$s\"} }" },
        { "sourceInfo",     "{ \"%2$s\" : {\"container\":\"\",\"numPrograms\":1,\"programInfo\":[{\"duration\":%3$lld,\"numAudioTracks\":0,\"numVideoTracks\":0,\"numSubtitleTracks\":0}],\"mediaId\":\"%1$s\"} }" },
    };
    char buf[1024];
    snprintf(buf, 1023, formats[type], cname, type, args...);
    if (subscribed)
        connector->sendChangeNotificationJsonString(buf);
    else
        queue.push_back(buf);
}

void PipelineContext::sendStateUpdate (const char* state_name, const bool state_val)
{
    sendNotify("state", state_name, state_val ? "true" : "false");
}

namespace {
	// pipeline exit delay in ms
	size_t g_exit_delay = 0;
    bool g_send_unload_completed = true;
}

void segfault (int* ptr)
{
    *ptr = 13;
}

std::map<std::string, void (*) (const std::vector<std::string> &)> debugHandlers = {
	{ "segfault",   [] (const std::vector<std::string> &) { segfault(NULL); } },
	{ "exit_delay", [] (const std::vector<std::string> & tokens) {
			if (tokens.size() > 1) {
				try { g_exit_delay = std::stoul(tokens[1]); } catch (...) {}
			}
		}
	},
	{ "hang",       [] (const std::vector<std::string> &) { g_exit_delay = -1; } },
	{ "unhang",     [] (const std::vector<std::string> &) { g_exit_delay = 0; } },
	{ "skip_uc",    [] (const std::vector<std::string> &) { g_send_unload_completed = false; } },
};

// ---
// API message handlers
bool loadEvent (UMSConnectorHandle* sender, UMSConnectorMessage* message, void* ctx)
{
    PipelineContext* pctx = static_cast<PipelineContext*>(ctx);

    const char* msg = pctx->connector->getMessageText(message);
    LOG_DEBUG(_log, "load msg=%s", msg);

    pctx->connector->sendSimpleResponse(sender, message, pctx->load(msg));
    return true;
}

bool unloadEvent (UMSConnectorHandle* sender, UMSConnectorMessage* message, void* ctx)
{
    PipelineContext* pctx = static_cast<PipelineContext*>(ctx);

    const char* msg = pctx->connector->getMessageText(message);
    LOG_DEBUG(_log, "unload msg=%s", msg);

    bool unloaded = pctx->unload();
    if (g_send_unload_completed)
        pctx->sendStateUpdate("unloadCompleted", unloaded);

    pctx->connector->sendSimpleResponse(sender, message, true);
	std::thread([pctx](){ usleep(g_exit_delay * us_to_ms); pctx->connector->stop(); }).detach();
    return true;
}

bool playEvent (UMSConnectorHandle* sender, UMSConnectorMessage* message, void* ctx)
{
    PipelineContext* pctx = static_cast<PipelineContext*>(ctx);

    const char* msg = pctx->connector->getMessageText(message);
    LOG_DEBUG(_log, "play msg=%s", msg);

    pctx->sendStateUpdate("playing", pctx->player->play());

    pctx->connector->sendSimpleResponse(sender, message, true);
    return true;
}

bool pauseEvent (UMSConnectorHandle* sender, UMSConnectorMessage* message, void* ctx)
{
    PipelineContext* pctx = static_cast<PipelineContext*>(ctx);

    const char* msg = pctx->connector->getMessageText(message);
    LOG_DEBUG(_log, "pause msg=%s", msg);

    pctx->sendStateUpdate("paused", pctx->player->pause());

    pctx->connector->sendSimpleResponse(sender, message, true);
    return true;
}

bool seekEvent (UMSConnectorHandle* sender, UMSConnectorMessage* message, void* ctx)
{
    PipelineContext* pctx = static_cast<PipelineContext*>(ctx);

    const char* msg = pctx->connector->getMessageText(message);
    LOG_DEBUG(_log, "seek msg=%s", msg);

    pctx->sendStateUpdate("seekDone", pctx->player->seek(atoll(msg)));

    pctx->connector->sendSimpleResponse(sender, message, true);
    return true;
}

bool stateChangeEvent (UMSConnectorHandle* sender, UMSConnectorMessage* message, void* ctx)
{
    PipelineContext* pctx = static_cast<PipelineContext*>(ctx);

    const char* msg = pctx->connector->getMessageText(message);
    LOG_DEBUG(_log, "stateChange msg=%s", msg);

    pctx->addSubscriber(sender, message);

    return true;
}

// Debug message format: "function args..." (space separated)
bool debugEvent (UMSConnectorHandle* sender, UMSConnectorMessage* message, void* ctx)
{
    PipelineContext* pctx = static_cast<PipelineContext*>(ctx);

    const char* msg = pctx->connector->getMessageText(message);
    LOG_DEBUG(_log, "debug msg=%s", msg);
	std::cerr << "debug msg = " << msg << std::endl;

	std::stringstream iss(msg);
	std::vector<std::string> tokens{std::istream_iterator<std::string>{iss},
									std::istream_iterator<std::string>{}};
	if (tokens.size()) {
		const auto e = debugHandlers.find(tokens[0]);
		if (e != debugHandlers.cend())
			(e->second)(tokens);
	}

    pctx->connector->sendSimpleResponse(sender, message, true);
    return true;
}

std::vector <std::pair<std::string, bool (*) (UMSConnectorHandle*, UMSConnectorMessage*, void*)>> eventHandlers = {
    { "load",           loadEvent           },
    { "unload",         unloadEvent         },
    { "play",           playEvent           },
    { "pause",          pauseEvent          },
    { "seek",           seekEvent           },
    { "stateChange",    stateChangeEvent    },
    { "debug",          debugEvent          },
};

int main (int argc, char** argv, char ** envp)
{
    int c = 0;
    std::string* service_name = NULL;

	// prinitng environment
	std::cerr << "initialized with environment: " << std::endl;
	char ** env;
	for (env = envp; *env != 0; ++env) {
		char * e = *env;
		std::cerr << e << std::endl;
	}

	while ((c = getopt(argc, argv, "s:")) != -1) {
        switch (c) {
            case 's':
                service_name = new std::string(optarg);
                break;
			case '?':
				std::cout << "usage: pipeline process [-#]\n  -s#: specifies UMSConnector service name.\n";
        }
    }

    if (service_name) {
        LOG_DEBUG(_log, "-s %s", service_name->c_str());
    } else {
        service_name = new std::string("com.webos.pipeline._ZZZZZZZZZZZZZZZ");
        LOG_DEBUG(_log, "default service_name=%s", service_name->c_str());
    }

    LOG_DEBUG(_log, "SIMULATED-PIPELINE BEGIN");

    PipelineContext* pctx;

    try {
        pctx = new PipelineContext(*service_name);
    }
    catch (...) {
        LOG_ERROR(_log, MSGERR_INIT_FAILED, "Initialization failure");
        return -1;
    }

    // ---
    // add API event handlers.
    for(auto& i : eventHandlers)
        pctx->connector->addEventHandler(i.first, i.second);

    pctx->connector->wait();

    LOG_DEBUG(_log, "SIMULATED-PIPELINE END");

    return 0;
}
