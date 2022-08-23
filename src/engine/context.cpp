#include "context.h"
#include "runtime.h"
#include "value.h"
#include "error.h"
#include <quickjs.h>
#include <quickjs-libc.h>
#include <boost/assert.hpp>
#include <boost/system/system_error.hpp>
#include <iostream>

namespace engine {

context::context(JSRuntime* rt, value g)
: js_(JS_NewContext(rt)) {
    JS_SetContextOpaque(js_, this);
    js_std_add_helpers(js_, -1, nullptr);
    JS_EnqueueJob(js_, [] (JSContext* ctx, int argc, JSValueConst* argv) -> JSValue {
        boost::system::error_code error;
        static_cast<context*>(JS_GetContextOpaque(ctx))->io_.run(error);
        if (error)
            return JS_ThrowInternalError(ctx, "(%d) %s", error.value(), error.message().c_str());
        return JS_NULL;
    }, 0, nullptr);
}

context::context(JSRuntime* rt)
: context(rt, {}) {}

context::context()
: context(JS_NewRuntime(), {}) {}

context::~context() {
    if (js_) JS_FreeContext(js_);
}

value context::global() {
    return {*this, JS_GetGlobalObject(js_)};
}

value context::run(std::string_view script, boost::system::error_code& error) {
    JSValue rv = JS_Eval(js_, script.data(), script.size(), "root", 0);
    // TODO 抛出异常
    if (JS_IsException(rv)) {
        error = {error::eval_script_failed, error::get_quickjs_category()};
#ifdef QUICKJS_DEBUG_TRACE_EXCEPTION
        std::cerr << "<exception>: ";
        js_std_dump_error(js_);
#endif
    }

    JSContext* ctx;
    int err;
    do {
        err = JS_ExecutePendingJob(JS_GetRuntime(js_), &ctx);
    } while (err > 0);

    if (err < 0) {
        error = {error::exec_pending_job_failed, error::get_quickjs_category()};
#ifdef QUICKJS_DEBUG_TRACE_EXCEPTION
        std::cerr << "<exception> (pending): ";
        js_std_dump_error(ctx);
#endif
    }
    return {*this, rv};
}

value context::run(std::string_view script) {
    boost::system::error_code error;
    value rv = run(script, error);
    if (error) throw boost::system::system_error(error);
    return rv;
}

context_scope context::scope() const {
    return context_scope{js_};
}

thread_local JSContext* context_scope::ctx_;

context_scope::context_scope(JSContext* ctx) {
    BOOST_ASSERT(ctx_ == nullptr);
    ctx_ = ctx;
}

context_scope::~context_scope() {
    ctx_ = nullptr;
}

const context& context_scope::get() {
    BOOST_ASSERT(ctx_ != nullptr);
    return *reinterpret_cast<context*>(JS_GetContextOpaque(ctx_));
}

} // namespace engine