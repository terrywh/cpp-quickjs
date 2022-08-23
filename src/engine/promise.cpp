#include "promise.h"
#include "context.h"
#include <quickjs.h>

namespace engine {

inline constexpr static JSValue& V(const value::fake_type& ft) {
    return *reinterpret_cast<JSValue*>(const_cast<value::fake_type*>(&ft));
}

promise::promise()
: ctx_(context_scope::get()) {
    V(p_) = JS_NewPromiseCapability(ctx_, reinterpret_cast<JSValue*>(f_));
}

promise::~promise() {
    JS_FreeValue(ctx_, V(p_));
    JS_FreeValue(ctx_, V(f_[0]));
    JS_FreeValue(ctx_, V(f_[1]));
}

void promise::resolve(value v) const {
    JSValue r = JS_Call(ctx_, V(f_[0]), JS_UNDEFINED, 1, v);
    JS_FreeValue(ctx_, r);
}

void promise::reject(value v) const {
    JSValue r = JS_Call(ctx_, V(f_[1]), JS_UNDEFINED, 1, v);
    JS_FreeValue(ctx_, r);
}

value promise::future() {
    value rv {ctx_, V(p_)};
    V(p_) = JS_UNDEFINED;
    return rv;
}

} // namespace engine