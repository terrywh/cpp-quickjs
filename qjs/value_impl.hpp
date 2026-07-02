#pragma once

#include "common.inc.hpp"
#include "context.hpp"
#include "property_ref.hpp"

namespace qjs {

inline call_args::call_args(context_ref& ctx, JSValueConst this_value, int argc, JSValueConst* argv) noexcept
    : context_(&ctx), this_(ctx.raw(), this_value), argc_(argc), argv_(argv)
{
}

inline context_ref& call_args::ctx() const noexcept
{
    return *context_;
}

inline value_view call_args::operator[](std::size_t index) const
{
    if (index >= size()) {
        detail::throw_error("QuickJS callback argument index is out of range");
    }
    return value_view(context_->raw(), argv_[index]);
}

inline value::value(context_ref& ctx, JSValue raw) noexcept
    : ctx_(ctx.raw()), value_(raw)
{
}

inline value value::get(std::string_view name) const
{
    std::string owned_name{name};
    context_ref ctx(ctx_);
    value v(ctx, JS_GetPropertyStr(ctx_, value_, owned_name.c_str()));
    if (v.is_exception()) {
        detail::throw_error(ctx.exception_string());
    }
    return v;
}

inline void value::set(const char* name, value v)
{
    context_ref ctx(ctx_);
    if (JS_SetPropertyStr(ctx_, value_, name, v.release()) < 0) {
        detail::throw_error(ctx.exception_string());
    }
}

inline void value::set(std::string_view name, value v)
{
    std::string owned_name{name};
    set(owned_name.c_str(), std::move(v));
}

inline property_ref value::operator[](std::string_view name)
{
    return property_ref(*this, name);
}

template <typename... Args>
inline value value::call(value_view this_value, Args&&... args) const
{
    context_ref ctx(ctx_);
    ctx.ensure_context(this_value);

    std::vector<value> owned_args;
    owned_args.reserve(sizeof...(Args));
    (owned_args.emplace_back(ctx.make_value(std::forward<Args>(args))), ...);

    std::vector<JSValueConst> raw_args;
    raw_args.reserve(owned_args.size());
    for (const value& arg : owned_args) {
        raw_args.push_back(arg.raw());
    }

    value result(ctx, JS_Call(ctx_, value_, this_value.raw(), static_cast<int>(raw_args.size()), raw_args.data()));
    if (result.is_exception()) {
        detail::throw_error(ctx.exception_string());
    }
    return result;
}

template <typename... Args>
inline value value::operator()(Args&&... args) const
{
    return call(value_view(ctx_, JS_UNDEFINED), std::forward<Args>(args)...);
}

template <typename... Args>
inline value value::invoke(std::string_view name, Args&&... args) const
{
    value function = get(name);
    return function.call(view(), std::forward<Args>(args)...);
}

inline bool value::equals_loose(value_view other) const
{
    ensure_same_context(other);
    int r = JS_IsEqual(ctx_, value_, other.raw());
    if (r < 0) {
        detail::throw_error(context_ref(ctx_).exception_string());
    }
    return r != 0;
}

inline bool value::same_value(value_view other) const
{
    ensure_same_context(other);
    return JS_IsSameValue(ctx_, value_, other.raw());
}

template <typename T>
inline T value_view::as() const
{
    using U = detail::remove_cvref_t<T>;
    if constexpr (std::same_as<U, std::int32_t> || std::same_as<U, int>) {
        std::int32_t out{};
        if (JS_ToInt32(ctx_, &out, value_) < 0) {
            detail::throw_error(context_ref(ctx_).exception_string());
        }
        return static_cast<T>(out);
    } else if constexpr (std::same_as<U, std::int64_t>) {
        std::int64_t out{};
        if (JS_ToInt64(ctx_, &out, value_) < 0) {
            detail::throw_error(context_ref(ctx_).exception_string());
        }
        return out;
    } else if constexpr (std::same_as<U, double>) {
        double out{};
        if (JS_ToFloat64(ctx_, &out, value_) < 0) {
            detail::throw_error(context_ref(ctx_).exception_string());
        }
        return out;
    } else if constexpr (std::same_as<U, bool>) {
        int out = JS_ToBool(ctx_, value_);
        if (out < 0) {
            detail::throw_error(context_ref(ctx_).exception_string());
        }
        return out != 0;
    } else if constexpr (std::same_as<U, std::string>) {
        return to_string();
    } else if constexpr (std::same_as<U, value>) {
        context_ref ctx(ctx_);
        return value(ctx, JS_DupValue(ctx_, value_));
    } else {
        static_assert(detail::dependent_false<U>, "Type is not convertible from qjs::value_view");
    }
}

} // namespace qjs
