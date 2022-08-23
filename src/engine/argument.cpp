#include "arguments.h"
#include "value.h"
#include <quickjs.h>

namespace engine {

arguments::arguments(JSContext* ctx, int argc, JSValue* argv) {
    JSValueConst* args = reinterpret_cast<JSValueConst*>(argv);
    args_.reserve(argc);
    for (int i=0;i<argc;++i) {
        args_.emplace_back(ctx, argv[i], false);
    }
}


} // namespace engine

