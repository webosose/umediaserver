#ifndef __GLIBHELPER_H__
#define __GLIBHELPER_H__

#include <functional>
#include <future>
#include <memory>
#include <type_traits>

#include <glib.h>

namespace uMediaServer {

class _GMainRouter_dispatcher {
    typedef std::function<void()> func_t;

    class _GMainRouter_helper {
        std::atomic_int cnt;
        func_t   func;
        GSource* g_src;

        static gboolean _callback (void* ctx) {
            _GMainRouter_helper* obj = static_cast<_GMainRouter_helper*>(ctx);

            if (obj->cnt > 1)
                (obj->func)();
            obj->unref();

            return false;
        }

    public:
        _GMainRouter_helper (guint timeout, std::function<void()>&& payload, GMainContext* ctx = NULL) :
            cnt(2),
            g_src(NULL),
            func(std::forward<func_t>(payload))
        {
            g_src = g_timeout_source_new(timeout);
            g_source_set_callback(g_src, _GMainRouter_helper::_callback, this, NULL);
            g_source_attach(g_src, ctx);
        }

        void ref () { ++cnt; }
        void unref () { if (! --cnt) delete this; }

        ~_GMainRouter_helper () { g_source_unref(g_src); }

        _GMainRouter_helper (_GMainRouter_helper&&) = delete;
        _GMainRouter_helper (const _GMainRouter_helper&) = delete;
        _GMainRouter_helper&& operator = (_GMainRouter_helper&&) = delete;
        _GMainRouter_helper&  operator = (const _GMainRouter_helper&) = delete;
    };

    _GMainRouter_helper* helper;

public:
    _GMainRouter_dispatcher (guint timeout, std::function<void()>&& payload, GMainContext* ctx = NULL) :
        helper(NULL)
	{
		helper = new _GMainRouter_helper(timeout, std::forward<func_t>(payload), ctx);
    }

    ~_GMainRouter_dispatcher () { if (helper) helper->unref(); }

    _GMainRouter_dispatcher (_GMainRouter_dispatcher&&) = delete;
    _GMainRouter_dispatcher (const _GMainRouter_dispatcher&) = delete;
    _GMainRouter_dispatcher&& operator = (_GMainRouter_dispatcher&&) = delete;
    _GMainRouter_dispatcher&  operator = (const _GMainRouter_dispatcher&) = delete;
};

class GMainTimer {
    std::unique_ptr<_GMainRouter_dispatcher> _impl;
public:
    GMainTimer (guint timeout, std::function<void()>&& payload, GMainContext* ctx = NULL) :
        _impl(new _GMainRouter_dispatcher(timeout, std::forward< decltype(payload) >(payload), ctx))
    { }

    GMainTimer () = default;
    GMainTimer (GMainTimer&&) = default;
    GMainTimer (const GMainTimer&) = default;
    GMainTimer& operator = (GMainTimer&&) = default;
    GMainTimer& operator = (const GMainTimer&) = default;
};

template <typename _Signature>
class GMainRouter {
    static_assert(std::is_function<_Signature>::value, "template argument must be of function type");
};
template <typename R, typename... Args>
class GMainRouter<R(Args...)> {
    std::function<R(Args...)> slot;
    GMainContext* g_ctx;
public:
    GMainRouter (const std::function<R(Args...)>& slot, GMainContext* ctx = NULL) :
        g_ctx(ctx),
        slot(std::forward<decltype(slot)>(slot))
    { }

    GMainRouter (GMainRouter&&) = default;
    GMainRouter (const GMainRouter&) = default;
    GMainRouter& operator = (GMainRouter&&) = default;
    GMainRouter& operator = (const GMainRouter&) = default;

    template <typename... _Args>
    R operator() (_Args&&... args) {
        auto task = std::make_shared< std::packaged_task<R()> >(std::bind(slot, std::forward<Args>(args)...));

        _GMainRouter_dispatcher watcher(0, [task] { (*task)(); }, g_ctx);

        return task->get_future().get();
    }
};

}

#endif // __GLIBHELPER_H__
