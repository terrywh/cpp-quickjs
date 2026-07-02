#pragma once

#include "common.inc.hpp"



namespace qjs {

// Thin C++ wrapper around QuickJS' module-loader callbacks. The trampolines
// are registered on qjs::runtime via JS_SetModuleLoaderFunc2; for now they
// forward to quickjs-libc's default js_module_loader / js_module_check_attributes.
struct module_loader {
    static JSModuleDef* load_trampoline(
        JSContext* ctx,
        const char* module_name,
        void* opaque,
        JSValueConst attributes) noexcept
    {
        return js_module_loader(ctx, module_name, opaque, attributes);
    }

    static int check_attributes_trampoline(
        JSContext* ctx,
        void* opaque,
        JSValueConst attributes) noexcept
    {
        return js_module_check_attributes(ctx, opaque, attributes);
    }
};

} // namespace qjs
