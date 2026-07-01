#pragma once

#if __cplusplus < 202400L
#error "quickjs_wrapper requires C++26 or newer. Build with -std=c++26/-std=c++2c; lower standards are unsupported."
#endif

// -----------------------------------------------------------------------------
// common.inc.hpp
// -----------------------------------------------------------------------------
// Aggregate "batteries-included" header for the QuickJS C++ wrapper.
//
// Any wrapper header that would otherwise pull in a large slice of the C++
// standard library and <quickjs.h> should include THIS file directly instead
// of listing every standard header on its own. The intent is:
//
//   * Reduce include-boilerplate at every call-site.
//   * Guarantee that any wrapper header which uses standard facilities or
//     QuickJS types depends on them *directly* (via this aggregate), not
//     transitively through some other wrapper header. If a sibling wrapper
//     header stops re-exporting some symbol tomorrow, this file (owned by us)
//     still guarantees availability.
//
// Rules:
//   * Every wrapper *.hpp that uses standard-library or QuickJS symbols must
//     `#include "common.inc.hpp"` itself.
//   * Wrapper *.hpp files must NOT rely on OTHER wrapper *.hpp files to
//     transitively provide standard-library / QuickJS declarations.
//   * This file is the single source of truth for capability probes, common
//     STL includes, and the <quickjs.h> import.
// -----------------------------------------------------------------------------

// ---- Feature-test probe support --------------------------------------------
#include <version>

// ---- Common C++ standard library headers used across the wrapper -----------
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <expected>
#include <functional>
#include <memory>
#include <meta>
#include <optional>
#include <source_location>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

// ---- Feature-test guards ---------------------------------------------------
#if !defined(__cpp_lib_expected)
#error "quickjs_wrapper requires std::expected from the C++ standard library."
#endif

#if !defined(__cpp_lib_move_only_function)
#error "quickjs_wrapper requires std::move_only_function from the C++ standard library."
#endif

#if defined(__cpp_static_reflection)
#define QJS_CPP_WRAPPER_STATIC_REFLECTION_VERSION __cpp_static_reflection
#elif defined(__cpp_impl_reflection)
#define QJS_CPP_WRAPPER_STATIC_REFLECTION_VERSION __cpp_impl_reflection
#else
#error "quickjs_wrapper requires C++26 static reflection (__cpp_static_reflection or __cpp_impl_reflection). No no-reflection fallback is provided."
#endif

// ---- QuickJS C API ---------------------------------------------------------
extern "C" {
#include <quickjs.h>
}

// ---- Optional Boost.Stacktrace integration ---------------------------------
//
// Boost.Stacktrace is used by qjs::error to capture the call stack at the
// failure site. Downstream consumers who cannot afford the extra dependency
// (or the runtime cost of unwinding on every error) can opt out by defining
// QJS_CPP_WRAPPER_DISABLE_STACKTRACE at compile time; a size-zero placeholder
// type is substituted instead so that qjs::error's aggregate layout and
// existing initialiser syntax remain unchanged.
//
// The macro QJS_CPP_WRAPPER_HAS_STACKTRACE (1 when enabled, 0 otherwise) is
// exposed so callers can gate stack-related logic without repeating the
// negation of the disable switch.
#if defined(QJS_CPP_WRAPPER_DISABLE_STACKTRACE)
#define QJS_CPP_WRAPPER_HAS_STACKTRACE 0
#else
#define QJS_CPP_WRAPPER_HAS_STACKTRACE 1
#endif

#if QJS_CPP_WRAPPER_HAS_STACKTRACE
#include <boost/stacktrace/stacktrace.hpp>
#endif

#include <ostream>

namespace qjs {

#if QJS_CPP_WRAPPER_HAS_STACKTRACE
using stacktrace = boost::stacktrace::stacktrace;
#else
// Empty stand-in used when stacktrace capture is disabled at build time.
// It is a trivially default-constructible aggregate so that qjs::error's
// default member initialiser (`stacktrace{}`) keeps compiling unchanged, and
// its stream operator prints an explicit "disabled" marker for logs.
struct stacktrace {
    friend std::ostream& operator<<(std::ostream& os, const stacktrace&) {
        return os << "<stacktrace disabled at build time>";
    }
};
#endif

} // namespace qjs
