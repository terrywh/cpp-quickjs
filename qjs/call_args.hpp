#pragma once

#include "common.inc.hpp"
#include "value.hpp"

namespace qjs {

class context;
class context_ref;
class value_view;
class value;

class call_args final {
public:
    call_args(context_ref& ctx, JSValueConst this_value, int argc, JSValueConst* argv) noexcept;

    [[nodiscard]] context_ref& ctx() const noexcept;
    [[nodiscard]] value_view this_value() const noexcept { return this_; }
    [[nodiscard]] std::size_t size() const noexcept { return static_cast<std::size_t>(argc_); }
    [[nodiscard]] bool empty() const noexcept { return argc_ == 0; }
    [[nodiscard]] std::span<const JSValueConst> raw_values() const noexcept
    {
        return std::span<const JSValueConst>(argv_, static_cast<std::size_t>(argc_));
    }

    [[nodiscard]] value_view operator[](std::size_t index) const;

private:
    context_ref* context_;
    value_view this_;
    int argc_;
    JSValueConst* argv_;
};

using native_function = std::move_only_function<value(call_args)>;

} // namespace qjs
