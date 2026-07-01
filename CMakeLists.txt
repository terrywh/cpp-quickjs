cmake_minimum_required(VERSION 3.31)

project(quickjs_cpp_wrapper_demo LANGUAGES CXX)

#---------------------------------------------------------------------------
# Configure-time compiler capability checks.
#---------------------------------------------------------------------------
include(CheckCXXSourceCompiles)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(_quickjs_required_flags "${CMAKE_REQUIRED_FLAGS}")
set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -std=c++26 -freflection")
unset(QUICKJS_CPP_WRAPPER_HAS_REQUIRED_CXX26 CACHE)

check_cxx_source_compiles([=[
#include <concepts>
#include <expected>
#include <functional>
#include <source_location>
#include <span>
#include <string_view>

#if __cplusplus < 202400L
#error "C++26 mode is required."
#endif

#if !defined(__cpp_lib_expected)
#error "std::expected is required."
#endif

#if !defined(__cpp_lib_move_only_function)
#error "std::move_only_function is required."
#endif

#if !defined(__cpp_static_reflection) && !defined(__cpp_impl_reflection)
#error "C++26 static reflection (__cpp_static_reflection or __cpp_impl_reflection) is required."
#endif

int main() { return 0; }
]=] QUICKJS_CPP_WRAPPER_HAS_REQUIRED_CXX26)

set(CMAKE_REQUIRED_FLAGS "${_quickjs_required_flags}")
unset(_quickjs_required_flags)

if(NOT QUICKJS_CPP_WRAPPER_HAS_REQUIRED_CXX26)
    message(FATAL_ERROR
        "QuickJS C++ wrapper requires C++26 with static reflection and the modern "
        "standard library facilities std::expected, std::span, std::source_location, "
        "and std::move_only_function. The compiler/standard library failed the "
        "configure-time probe, so this project refuses to configure. No lower-standard "
        "or no-reflection fallback is provided.")
endif()

#---------------------------------------------------------------------------
# Project-wide C++ settings.
#---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#---------------------------------------------------------------------------
# External QuickJS dependency.
#---------------------------------------------------------------------------
set(QUICKJS_DEPENDENCY_DIR "/data/vendor/quickjs" CACHE PATH "QuickJS/libquickjs dependency prefix")

find_package(qjs CONFIG REQUIRED PATHS "${QUICKJS_DEPENDENCY_DIR}")

#---------------------------------------------------------------------------
# Optional Boost.Stacktrace integration for qjs::error.
#
# When enabled (default), qjs::error captures a boost::stacktrace at the
# construction site and includes the frames in exception messages. When
# disabled, the wrapper is compiled with QJS_CPP_WRAPPER_DISABLE_STACKTRACE,
# no Boost dependency is looked up, and qjs::stacktrace becomes an empty
# placeholder — useful in environments without Boost or where unwind cost
# is unacceptable on error paths.
#---------------------------------------------------------------------------
option(QUICKJS_CPP_WRAPPER_ENABLE_STACKTRACE
    "Enable Boost.Stacktrace capture inside qjs::error." ON)

if(QUICKJS_CPP_WRAPPER_ENABLE_STACKTRACE)
    #-----------------------------------------------------------------------
    # External Boost dependency (headers + prebuilt boost::stacktrace backend).
    #
    # The vendor tree does not ship CMake config files, so we set up an imported
    # target manually: headers under <prefix>/include, static libs under
    # <prefix>/lib. We link the "basic" backend which relies on libdl for
    # symbol resolution and requires no additional runtime dependencies.
    #-----------------------------------------------------------------------
    set(BOOST_DEPENDENCY_DIR "/data/vendor/boost" CACHE PATH "Boost dependency prefix")

    set(_boost_include_dir "${BOOST_DEPENDENCY_DIR}/include")
    set(_boost_stacktrace_lib "${BOOST_DEPENDENCY_DIR}/lib/libboost_stacktrace_basic.a")

    if(NOT EXISTS "${_boost_include_dir}/boost/stacktrace.hpp")
        message(FATAL_ERROR
            "Boost headers not found at ${_boost_include_dir}. "
            "Set -DBOOST_DEPENDENCY_DIR=<prefix> to a Boost install that provides boost/stacktrace.hpp, "
            "or pass -DQUICKJS_CPP_WRAPPER_ENABLE_STACKTRACE=OFF to build without stacktrace support.")
    endif()

    if(NOT EXISTS "${_boost_stacktrace_lib}")
        message(FATAL_ERROR
            "Boost.Stacktrace backend not found at ${_boost_stacktrace_lib}. "
            "Expected libboost_stacktrace_basic.a in ${BOOST_DEPENDENCY_DIR}/lib, "
            "or pass -DQUICKJS_CPP_WRAPPER_ENABLE_STACKTRACE=OFF to build without stacktrace support.")
    endif()

    add_library(boost_stacktrace STATIC IMPORTED GLOBAL)
    add_library(Boost::stacktrace ALIAS boost_stacktrace)
    set_target_properties(boost_stacktrace PROPERTIES
        IMPORTED_LOCATION "${_boost_stacktrace_lib}"
        INTERFACE_INCLUDE_DIRECTORIES "${_boost_include_dir}"
        # BOOST_STACKTRACE_LINK toggles Boost.Stacktrace from header-only mode to
        # the linked backend that we ship as a static library.
        INTERFACE_COMPILE_DEFINITIONS "BOOST_STACKTRACE_LINK"
        INTERFACE_LINK_LIBRARIES "dl"
    )

    unset(_boost_include_dir)
    unset(_boost_stacktrace_lib)
endif()

#---------------------------------------------------------------------------
# Header-only QuickJS C++ wrapper library.
#---------------------------------------------------------------------------
file(GLOB QUICKJS_CPP_WRAPPER_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/qjs/*.hpp"
)

add_library(quickjs_cpp_wrapper INTERFACE)
add_library(quickjs::cpp_wrapper ALIAS quickjs_cpp_wrapper)

target_sources(quickjs_cpp_wrapper
    INTERFACE
        FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
        FILES ${QUICKJS_CPP_WRAPPER_HEADERS}
)

target_include_directories(quickjs_cpp_wrapper
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(quickjs_cpp_wrapper INTERFACE qjs)
if(QUICKJS_CPP_WRAPPER_ENABLE_STACKTRACE)
    target_link_libraries(quickjs_cpp_wrapper INTERFACE Boost::stacktrace)
else()
    target_compile_definitions(quickjs_cpp_wrapper INTERFACE QJS_CPP_WRAPPER_DISABLE_STACKTRACE)
endif()
target_compile_features(quickjs_cpp_wrapper INTERFACE cxx_std_26)
target_compile_options(quickjs_cpp_wrapper INTERFACE -freflection)

#---------------------------------------------------------------------------
# Demo executable.
#---------------------------------------------------------------------------
add_executable(quickjs_cpp_wrapper_demo
    quickjs_cpp_wrapper_demo.cpp
)

target_link_libraries(quickjs_cpp_wrapper_demo PRIVATE quickjs::cpp_wrapper)
