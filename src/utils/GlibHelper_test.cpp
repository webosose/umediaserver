/* g++ -std=c++0x GLibHelper_test.cpp -g -o GLibHelper_test $(pkg-config --cflags --libs glib-2.0) -pthread */

#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <glib.h>

#include <boost/signals2/signal.hpp>

#include "GLibHelper.h"
using namespace uMediaServer;

struct timespec start;

void _time (struct timespec* res) {
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    res->tv_sec = now.tv_sec - start.tv_sec;
    if ((res->tv_nsec = now.tv_nsec - start.tv_nsec) < 0) {
        res->tv_sec--;
        res->tv_nsec += 1000000000;
    }
}

inline void LOG (const char* msg, ...) {
    va_list list;
    char fmt[256];
    struct timespec pass;

    _time(&pass);
    snprintf(fmt, 256, "[%3d.%06u] %u: %s\n", (int)pass.tv_sec, (unsigned)pass.tv_nsec/1000, (unsigned)pthread_self(), msg);
    va_start(list, msg);
    vfprintf(stderr, fmt, list);
    va_end(list);
}

struct A { };

gboolean _quit (void* data) {
    LOG("quitting from loop");
    g_main_loop_quit((GMainLoop*)data);
    return FALSE;
}

void* main2 (void* _) {
    GMainContext* ctx = g_main_context_new();
    GMainLoop* loop = g_main_loop_new(ctx, FALSE);

    LOG("starting second loop");

    //auto test = [&] (int n, int m) { LOG("t(%d,%d)", n, m); };
    //GMainRouter<int> x(test);
    //GMainRouter<void,int,int> x(test);
    //GMainRouter<void(int)> x(test);
    //GMainRouter<void(int,int)> x(test);
    //x(3);
    //x(loop);
    //x(3,4,5);

    A a;
    GMainRouter<void(A&)> x([] (A& v) { LOG("x: %p", &v); });
    GMainRouter<void(A*)> y([] (A* v) { LOG("y: %p",  v); }, ctx);
    x( a);
    y(&a);

    auto test_f = [&] (int n, int m) { LOG("s(%d,%d)", n, m); };
    boost::signals2::signal<void(int,int)> sig;
    sig.connect(GMainRouter<void(int,int)>(test_f));
    sig.connect(GMainRouter<void(int,int)>(test_f, ctx));

    boost::signals2::signal<int(int)> sig_r;
    sig_r.connect(GMainRouter<int(int)>([] (int a) { return (a + 666) % 10; }, ctx));

    GMainTimer w[] = {
        { 3000, [&] { LOG("w1");                                } },
        { 5000, [&] { LOG("w2"); sig(13, 666);                  } },
        { 6000, [&] { LOG("w3"); sig(14, 777);                  } },
        { 7000, [&] { LOG("w4"); LOG("sig_r: %d", *sig_r(13));  } },
        { 8000, [&] { LOG("w5"); sig(15, 999);                  } },
        { 9000, [&] { LOG("w6"); _quit(loop);                   }, ctx},
    };

    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    return NULL;
}

int main () {
    clock_gettime(CLOCK_MONOTONIC, &start);
    LOG("starting first loop");

    pthread_t thr_main2;
    pthread_create(&thr_main2, NULL, main2, NULL);

    GMainLoop* loop = g_main_loop_new(NULL, 0);
    g_timeout_add_seconds(12, _quit, loop);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    pthread_join(thr_main2, NULL);
    return 0;
}
