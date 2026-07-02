#pragma once

#include "common.inc.hpp"

namespace qjs {

// Error payload for `qjs::result<T>`; also the base of `qjs::exception`.
//
// The `stacktrace` member (see common.inc.hpp) captures the call stack at the
// point where the `error` value is aggregate-initialised, which — for the
// `std::unexpected(error{...})` / `throw exception(error{...})` idioms used
// throughout the wrapper — coincides with the actual failure site. When the
// wrapper is built with QJS_CPP_WRAPPER_DISABLE_STACKTRACE, the field becomes
// an empty placeholder and stack frames are omitted from `to_string()`.
struct error {
    std::string message;
    std::source_location at;
    qjs::stacktrace stacktrace;

    // Human-readable one-shot dump: message + source location (+ stack trace
    // when the stacktrace capability is enabled at build time).
    [[nodiscard]] std::string to_string() const {
        std::ostringstream os;
        os << message
           << " [" << at.file_name() << ':' << at.line()
           << " in " << at.function_name() << "]";
#if QJS_CPP_WRAPPER_HAS_STACKTRACE
        os << '\n' << stacktrace;
#endif
        return std::move(os).str();
    }
};

template <typename T>
using result = std::expected<T, error>;

class exception final : public error, public std::exception {
public:
    explicit exception(error err)
        : error(std::move(err))
    {
        
    }

    [[nodiscard]] const char* what() const noexcept override {
        return message.c_str();
    }



private:
};

// Throw a qjs::exception with message, source location, and (when enabled)
// a C++ stack trace that excludes throw_error itself.
[[noreturn]] inline void throw_error(
    const char* message,
    std::source_location at = std::source_location::current())
{
    throw exception(error{std::move(message), at,{}});
}

[[noreturn]] inline void throw_error(
    std::string_view message,
    std::source_location at = std::source_location::current())
{
    throw exception(error{std::string{message}, at, {}});
}

[[noreturn]] inline void throw_error(
    const std::string& message,
    std::source_location at = std::source_location::current())
{
    throw exception(error{message, at, {}});
}

} // namespace qjs
