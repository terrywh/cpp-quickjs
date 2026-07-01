#pragma once

#include "common.inc.hpp"

#include <sstream>
#include <string>

namespace qjs {

// Error payload for `qjs::result<T>` and `qjs::exception`.
//
// The `stacktrace` member (see common.inc.hpp) captures the call stack at the
// point where the `error` value is aggregate-initialised, which — for the
// `std::unexpected(error{...})` / `throw exception(error{...})` idioms used
// throughout the wrapper — coincides with the actual failure site. When the
// wrapper is built with QJS_CPP_WRAPPER_DISABLE_STACKTRACE, the field becomes
// an empty placeholder and stack frames are omitted from `to_string()`.
struct error {
    std::string message;
    std::source_location location;
    qjs::stacktrace stacktrace{};

    // Human-readable one-shot dump: message + source location (+ stack trace
    // when the stacktrace capability is enabled at build time).
    [[nodiscard]] std::string to_string() const {
        std::ostringstream os;
        os << message
           << " [" << location.file_name() << ':' << location.line()
           << " in " << location.function_name() << "]";
#if QJS_CPP_WRAPPER_HAS_STACKTRACE
        os << '\n' << stacktrace;
#endif
        return std::move(os).str();
    }
};

template <typename T>
using result = std::expected<T, error>;

class exception final : public std::runtime_error {
public:
    explicit exception(error err)
        : std::runtime_error(err.to_string()), error_(std::move(err)) {}

    [[nodiscard]] const error& info() const noexcept { return error_; }

private:
    error error_;
};

} // namespace qjs
