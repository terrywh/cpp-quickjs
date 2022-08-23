#include <quickjs.h>
#include <quickjs-libc.h>
#include "runtime.h"

namespace engine {

runtime::runtime()
: runtime_(JS_NewRuntime()) {
    js_std_init_handlers(runtime_);
} // namespace engine

runtime::~runtime() {
    js_std_free_handlers(runtime_);
    JS_FreeRuntime(runtime_);
}

} // namespace engine
