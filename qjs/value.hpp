#pragma once

#include "common.inc.hpp"
#include "detail.hpp"

namespace qjs {

class context;
class context_ref;
class property_ref;
class call_args;

class value_view {
public:
    constexpr value_view() noexcept = default;
    constexpr value_view(JSContext* ctx, JSValueConst raw) noexcept
        : ctx_(ctx), value_(raw)
    {
    }

    [[nodiscard]] JSContext* ctx() const noexcept { return ctx_; }
    [[nodiscard]] JSValueConst raw() const noexcept { return value_; }

    [[nodiscard]] bool is_number() const noexcept { return JS_IsNumber(value_); }
    [[nodiscard]] bool is_string() const noexcept { return JS_IsString(value_); }
    [[nodiscard]] bool is_object() const noexcept { return JS_IsObject(value_); }
    [[nodiscard]] bool is_array() const noexcept { return JS_IsArray(value_); }
    [[nodiscard]] bool is_function() const noexcept { return JS_IsFunction(ctx_, value_); }
    [[nodiscard]] bool is_exception() const noexcept { return JS_IsException(value_); }
    [[nodiscard]] bool is_undefined() const noexcept { return JS_IsUndefined(value_); }
    [[nodiscard]] bool is_null() const noexcept { return JS_IsNull(value_); }

    [[nodiscard]] std::string to_string() const
    {
        return detail::copy_c_string(ctx_, JS_ToCString(ctx_, value_));
    }

    template <typename T>
    [[nodiscard]] T as() const;

private:
    JSContext* ctx_{};
    JSValueConst value_{JS_UNDEFINED};
};

class value final {
public:
    // Default-constructed value is a context-less JS_UNDEFINED. It owns
    // nothing, so destruction is a no-op; assign / move-in a real value
    // once a context is available.
    value() noexcept
        : ctx_(nullptr), value_(JS_UNDEFINED)
    {
    }

    // Wrap an already-constructed raw JSValue (takes ownership). All other
    // ways to build a JS value flow through context::make_*() factories.
    value(context_ref& ctx, JSValue raw) noexcept;

    value(const value& other)
        : ctx_(other.ctx_)
        , value_(other.ctx_ == nullptr ? JS_UNDEFINED : JS_DupValue(other.ctx_, other.value_))
    {
    }

    value& operator=(const value& other)
    {
        if (this != &other) {
            reset();
            ctx_ = other.ctx_;
            value_ = other.ctx_ == nullptr ? JS_UNDEFINED : JS_DupValue(other.ctx_, other.value_);
        }
        return *this;
    }

    value(value&& other) noexcept
        : ctx_(std::exchange(other.ctx_, nullptr))
        , value_(std::exchange(other.value_, JS_UNDEFINED))
    {
    }

    value& operator=(value&& other) noexcept
    {
        if (this != &other) {
            reset();
            ctx_ = std::exchange(other.ctx_, nullptr);
            value_ = std::exchange(other.value_, JS_UNDEFINED);
        }
        return *this;
    }

    ~value() { reset(); }

    [[nodiscard]] JSContext* ctx() const noexcept { return ctx_; }
    [[nodiscard]] JSValue raw() const noexcept { return value_; }
    [[nodiscard]] value_view view() const noexcept { return value_view(ctx_, value_); }
    [[nodiscard]] JSValue release() noexcept
    {
        ctx_ = nullptr;
        return std::exchange(value_, JS_UNDEFINED);
    }

    [[nodiscard]] bool is_number() const noexcept { return view().is_number(); }
    [[nodiscard]] bool is_string() const noexcept { return view().is_string(); }
    [[nodiscard]] bool is_object() const noexcept { return view().is_object(); }
    [[nodiscard]] bool is_array() const noexcept { return view().is_array(); }
    [[nodiscard]] bool is_function() const noexcept { return view().is_function(); }
    [[nodiscard]] bool is_exception() const noexcept { return view().is_exception(); }
    [[nodiscard]] bool is_undefined() const noexcept { return view().is_undefined(); }
    [[nodiscard]] bool is_null() const noexcept { return view().is_null(); }

    [[nodiscard]] std::string to_string() const { return view().to_string(); }

    template <typename T>
    [[nodiscard]] T as() const
    {
        return view().template as<T>();
    }

    [[nodiscard]] value get(std::string_view name) const;

    void set(const char* name, value v);
    void set(std::string_view name, value v);

    [[nodiscard]] property_ref operator[](std::string_view name);
    [[nodiscard]] value operator[](std::string_view name) const { return get(name); }

    template <typename... Args>
    [[nodiscard]] value call(value_view this_value, Args&&... args) const;

    template <typename... Args>
    [[nodiscard]] value operator()(Args&&... args) const;

    template <typename... Args>
    [[nodiscard]] value invoke(std::string_view name, Args&&... args) const;

    [[nodiscard]] bool equals_loose(value_view other) const;
    [[nodiscard]] bool same_value(value_view other) const;

    friend bool operator==(const value& lhs, const value& rhs)
    {
        lhs.ensure_same_context(rhs.view());
        return JS_IsStrictEqual(lhs.ctx_, lhs.value_, rhs.value_);
    }

private:
    void reset() noexcept
    {
        if (ctx_ != nullptr) {
            JS_FreeValue(ctx_, value_);
            ctx_ = nullptr;
            value_ = JS_UNDEFINED;
        }
    }

    void ensure_same_context(value_view other) const
    {
        if (ctx_ != other.ctx()) {
            throw_error("QuickJS value used with a different context");
        }
    }

    JSContext* ctx_{};
    JSValue value_{JS_UNDEFINED};
};

} // namespace qjs
