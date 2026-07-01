#pragma once

#include "common.inc.hpp"
#include "error.hpp"

namespace qjs {

class context;
class value;
class value_view;
class property_ref;

template <typename T>
class type_builder;

namespace detail {

template <typename>
inline constexpr bool dependent_false = false;

inline std::string copy_c_string(JSContext* ctx, const char* text)
{
    if (text == nullptr) {
        return {};
    }
    std::string out{text};
    JS_FreeCString(ctx, text);
    return out;
}

[[noreturn]] inline void throw_error(
    std::string message,
    std::source_location location = std::source_location::current())
{
    throw exception(error{std::move(message), location});
}

template <typename T>
struct member_pointer_traits;

template <typename C, typename R, typename... Args>
struct member_pointer_traits<R (C::*)(Args...)> {
    using class_type = C;
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
};

template <typename C, typename R, typename... Args>
struct member_pointer_traits<R (C::*)(Args...) const> {
    using class_type = C;
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
};

template <typename C, typename R, typename... Args>
struct member_pointer_traits<R (C::*)(Args...) noexcept> : member_pointer_traits<R (C::*)(Args...)> {};

template <typename C, typename R, typename... Args>
struct member_pointer_traits<R (C::*)(Args...) const noexcept> : member_pointer_traits<R (C::*)(Args...) const> {};

template <typename T>
struct function_pointer_traits;

template <typename R, typename... Args>
struct function_pointer_traits<R (*)(Args...)> {
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
    static constexpr std::size_t arity = sizeof...(Args);
};

template <typename R, typename... Args>
struct function_pointer_traits<R (*)(Args...) noexcept>
    : function_pointer_traits<R (*)(Args...)> {};

template <typename T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

template <typename T>
inline constexpr bool is_smart_ptr_v = false;

template <typename T, typename D>
inline constexpr bool is_smart_ptr_v<std::unique_ptr<T, D>> = true;

template <typename T>
inline constexpr bool is_smart_ptr_v<std::shared_ptr<T>> = true;

} // namespace detail
} // namespace qjs
