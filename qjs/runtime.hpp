#pragma once

#include "common.inc.hpp"
#include "detail.hpp"
#include "error.hpp"
#include "module_loader.hpp"

namespace qjs {

class runtime final {
public:
    runtime()
        : runtime_(JS_NewRuntime())
    {
        if (runtime_ == nullptr) {
            throw_error("JS_NewRuntime failed");
        }

        js_std_init_handlers(runtime_);
        install_default_module_loader();
    }

    runtime(const runtime&) = delete;
    runtime& operator=(const runtime&) = delete;

    runtime(runtime&& other) noexcept
        : runtime_(std::exchange(other.runtime_, nullptr))
    {
    }

    runtime& operator=(runtime&& other) noexcept
    {
        if (this != &other) {
            reset();
            runtime_ = std::exchange(other.runtime_, nullptr);
        }
        return *this;
    }

    ~runtime() { reset(); }

    [[nodiscard]] JSRuntime* raw() const noexcept { return runtime_; }

    void set_memory_limit(std::size_t bytes) noexcept { JS_SetMemoryLimit(runtime_, bytes); }
    void set_max_stack_size(std::size_t bytes) noexcept { JS_SetMaxStackSize(runtime_, bytes); }
    void run_gc() noexcept { JS_RunGC(runtime_); }

private:
    void install_default_module_loader() noexcept
    {
        JS_SetModuleLoaderFunc2(
            runtime_,
            nullptr,
            &module_loader::load_trampoline,
            &module_loader::check_attributes_trampoline,
            nullptr);
    }

    void reset() noexcept
    {
        if (runtime_ != nullptr) {
            js_std_free_handlers(runtime_);
            JS_FreeRuntime(runtime_);
            runtime_ = nullptr;
        }
    }

    JSRuntime* runtime_{};
};

} // namespace qjs
