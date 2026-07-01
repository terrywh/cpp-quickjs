#pragma once

#include "common.inc.hpp"
#include "detail.hpp"

namespace qjs {

class runtime final {
public:
    runtime()
        : runtime_(JS_NewRuntime())
    {
        if (runtime_ == nullptr) {
            detail::throw_error("JS_NewRuntime failed");
        }
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
    void reset() noexcept
    {
        if (runtime_ != nullptr) {
            JS_FreeRuntime(runtime_);
            runtime_ = nullptr;
        }
    }

    JSRuntime* runtime_{};
};

} // namespace qjs
