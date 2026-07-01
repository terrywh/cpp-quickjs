#pragma once

#include "common.inc.hpp"

namespace qjs {

// -----------------------------------------------------------------------------
// qjs::expose — opt-in annotation for members that should be reflected into JS.
// -----------------------------------------------------------------------------
//
// `context::make_class<T>()` walks T's members via C++26 static reflection and
// exports ONLY the members carrying a `qjs::expose` annotation (attached with
// C++26 `[[=expr]]` syntax). Everything else stays invisible to JS regardless
// of access level, giving authors an explicit allow-list rather than a
// "public == exported" default.
//
// Usage:
//
//     struct Calc {
//         [[=qjs::expose{}]]              // export as "scale" (identifier)
//         int scale(int factor) const { ... }
//
//         [[=qjs::expose{"factor"}]]      // export as "factor" (renamed)
//         int base{};
//
//         void internal_helper();         // NOT exported
//     };
//
// Design notes on the on-wire representation:
//
//   * `[[=expr]]` requires the annotation type to be a *structural type*
//     (P1907), which rules out anything with private members — hence no
//     `std::string` / `std::string_view` field.
//   * GCC 16's `std::meta::extract` refuses to rematerialise aggregates that
//     carry raw `const char*` pointers ("reflect_constant failed"), so we
//     store the rename as an inline `char[]` buffer instead of a pointer/size
//     pair. A plain fixed-size char array is trivially structural AND fully
//     re-materialisable through `reflect_constant`.
//   * The buffer is intentionally sized generously; JS property names longer
//     than `max_name_size - 1` are refused at compile time via a `constexpr`
//     precondition so overflow is impossible by construction.
struct expose {
    // Upper bound for the inline JS name. Chosen large enough for realistic
    // property identifiers while staying tiny enough that copying the tag is
    // cheap during reflection. One byte is reserved for the null terminator.
    static constexpr std::size_t max_name_size = 64;

    // Inline storage for the JS-visible name. `size == 0` means "no override —
    // fall back to the C++ identifier of the annotated member".
    char data[max_name_size]{};
    std::size_t size{0};

    constexpr expose() = default;

    // Deducing N from a string literal keeps the natural call site
    // `[[=qjs::expose{"jsName"}]]` free of explicit length arguments. The
    // literal's length (excluding its trailing '\0') must fit into `data`;
    // failing that requirement is a hard compile-time error rather than a
    // silent truncation.
    template <std::size_t N>
    constexpr expose(const char (&literal)[N])
    {
        static_assert(N > 0, "qjs::expose: name literal cannot be empty (missing terminator)");
        static_assert(N <= max_name_size,
            "qjs::expose: rename literal exceeds max_name_size — bump the buffer or shorten the name");
        // Copy N-1 bytes so `data` remains null-terminated (the trailing '\0'
        // was already zero-initialised by the default member initialiser).
        for (std::size_t i = 0; i + 1 < N; ++i) {
            data[i] = literal[i];
        }
        size = N - 1;
    }

    // View the rename as a string_view. Intentionally NOT `constexpr`-usable
    // as an annotation payload (string_view is non-structural); this accessor
    // exists purely for the reflection consumer inside `type_builder`.
    [[nodiscard]] constexpr std::string_view name() const noexcept
    {
        return std::string_view{data, size};
    }
};

} // namespace qjs
