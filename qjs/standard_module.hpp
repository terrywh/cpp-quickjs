#pragma once

#include "common.inc.hpp"

namespace qjs {

// Descriptor for a quickjs-libc built-in ES module (e.g. "qjs:std").
// console / print / scriptArgs (js_std_add_helpers) are always installed by
// qjs::context and are not standard_module values.
struct standard_module {
    [[nodiscard]] const char* module_name() const noexcept { return name_; }

    friend constexpr bool operator==(standard_module lhs, standard_module rhs) noexcept
    {
        return lhs.name_ == rhs.name_;
    }

    static const standard_module std;
    static const standard_module os;
    static const standard_module bjson;

private:
    friend class context;

    using init_func = JSModuleDef* (*)(JSContext*, const char*);

    constexpr standard_module(const char* name, init_func init, unsigned bit) noexcept
        : name_{name}
        , init_{init}
        , bit_{bit}
    {
    }

    const char* name_;
    init_func init_;
    unsigned bit_;
};

inline const standard_module standard_module::std{"qjs:std", js_init_module_std, 1u << 0};
inline const standard_module standard_module::os{"qjs:os", js_init_module_os, 1u << 1};
inline const standard_module standard_module::bjson{"qjs:bjson", js_init_module_bjson, 1u << 2};

} // namespace qjs
