#pragma once

#include "common.inc.hpp"
#include "call_args.hpp"
#include "runtime.hpp"

#include <atomic>
#include <cstdio>
#include <sstream>

namespace qjs {

class context;

class context_ref {
public:
    // Non-owning view over an existing JSContext. The raw context must have
    // been created by qjs::context(runtime&) and must outlive this wrapper.
    explicit context_ref(JSContext* ctx)
        : runtime_(nullptr), context_(ctx), cpp_error_ctor_(nullptr)
    {
        if (ctx == nullptr) {
            detail::throw_error("QuickJS context pointer is null");
        }
        auto* owner = static_cast<context_ref*>(JS_GetContextOpaque(ctx));
        if (owner == nullptr) {
            detail::throw_error("QuickJS context opaque pointer is not installed");
        }
        runtime_ = owner->runtime_;
        cpp_error_ctor_ = owner->cpp_error_ctor_;
    }

    context_ref(const context_ref&) noexcept = default;
    context_ref& operator=(const context_ref&) noexcept = default;

    [[nodiscard]] JSContext* raw() const noexcept { return context_; }
    [[nodiscard]] JSRuntime* raw_runtime() const noexcept { return runtime_->raw(); }
    [[nodiscard]] runtime& rt() const noexcept { return *runtime_; }

    [[nodiscard]] value eval(
        std::string_view code,
        std::string_view filename = "<eval>",
        int flags = JS_EVAL_TYPE_GLOBAL,
        std::source_location location = std::source_location::current())
    {
        auto res = try_eval(code, filename, flags, location);
        if (!res) {
            throw exception(std::move(res.error()));
        }
        return std::move(*res);
    }

    [[nodiscard]] result<value> try_eval(
        std::string_view code,
        std::string_view filename = "<eval>",
        int flags = JS_EVAL_TYPE_GLOBAL,
        std::source_location location = std::source_location::current())
    {
        value v(*this, JS_Eval(context_, code.data(), code.size(), std::string(filename).c_str(), flags));
        if (v.is_exception()) {
            return std::unexpected(error{exception_string(), location});
        }
        return v;
    }

    [[nodiscard]] value global() { return value(*this, JS_GetGlobalObject(context_)); }

    // ---------------------------------------------------------------------
    // Value factories (fundamental / primitive).
    // ---------------------------------------------------------------------

    [[nodiscard]] value make_undefined() noexcept { return value(*this, JS_UNDEFINED); }
    [[nodiscard]] value make_null() noexcept { return value(*this, JS_NULL); }

    [[nodiscard]] value make_object()
    {
        value v(*this, JS_NewObject(context_));
        if (v.is_exception()) {
            detail::throw_error(exception_string());
        }
        return v;
    }

    [[nodiscard]] value make_array()
    {
        value v(*this, JS_NewArray(context_));
        if (v.is_exception()) {
            detail::throw_error(exception_string());
        }
        return v;
    }

    // Explicit primitive constructors.
    [[nodiscard]] value make_integer(std::int64_t v)
    {
        return value(*this, JS_NewInt64(context_, v));
    }

    [[nodiscard]] value make_float(double v) { return value(*this, JS_NewFloat64(context_, v)); }

    [[nodiscard]] value make_string(std::string_view v)
    {
        value res(*this, JS_NewStringLen(context_, v.data(), v.size()));
        if (res.is_exception()) {
            detail::throw_error(exception_string());
        }
        return res;
    }

    [[nodiscard]] value make_bool(bool v) { return value(*this, JS_NewBool(context_, v)); }

    // Generic dispatcher: bridge any supported C++ value into a qjs::value.
    template <typename T>
    [[nodiscard]] value make_value(T&& v);

    // ---------------------------------------------------------------------
    // Value factories (function / class / wrapped object).
    // Each returns a stand-alone value; use context::set / value::set to
    // install it on the global scope or an object.
    // ---------------------------------------------------------------------

    // Build a JS closure value that wraps the given native_function.
    [[nodiscard]] value make_function(native_function fn, int length = 0)
    {
        return make_function_impl(next_closure_name(), std::move(fn), length);
    }

    // Reflected free / static-member function.
    template <auto Fn>
    [[nodiscard]] value make_function();

    // Register a C++ class and return its constructor value.
    template <typename T>
    [[nodiscard]] value make_class();

    // Wrap a native C++ instance in a JS object. The four overloads mirror the
    // C++ ownership semantics that the JS object gains over the wrapped value:
    //   - const T&                : copy — JS owns an independent copy.
    //   - T&&                     : move — JS takes over the C++ instance.
    //   - std::unique_ptr<T, D>   : move — JS becomes the unique owner.
    //   - std::shared_ptr<T>      : share — JS shares ownership with the caller.
    template <typename T>
        requires (!detail::is_smart_ptr_v<detail::remove_cvref_t<T>>)
    [[nodiscard]] value make_object(const T& object);

    template <typename T>
        requires (!std::is_reference_v<T> &&
                  !detail::is_smart_ptr_v<detail::remove_cvref_t<T>>)
    [[nodiscard]] value make_object(T&& object);

    template <typename T>
    [[nodiscard]] value make_object(std::shared_ptr<T> object);

    template <typename T, typename D>
    [[nodiscard]] value make_object(std::unique_ptr<T, D> object);

    // Unified property setter: writes any value (closure / constructor / wrapped
    // object / plain data) as a named property on the global object.
    void set(const char* name, value v)
    {
        if (JS_SetPropertyStr(context_, global().raw(), name, v.release()) < 0) {
            detail::throw_error(exception_string());
        }
    }

    void set(std::string_view name, value v)
    {
        std::string owned_name{name};
        set(owned_name.c_str(), std::move(v));
    }

    [[nodiscard]] std::string exception_string()
    {
        value exc(*this, JS_GetException(context_));
        if (exc.is_undefined()) {
            return "QuickJS exception";
        }
        return exc.to_string();
    }

    // Raise a JS exception in this context that mirrors a `qjs::error`. The
    // returned raw handle is `JS_EXCEPTION` (QuickJS' pending-exception
    // sentinel) — native trampolines return it verbatim to signal failure.
    // On the JS side the thrown object is a `CppError` (a subclass of the
    // built-in `Error`), so scripts identify wrapper-originated failures via
    // `err instanceof CppError`.
    JSValue throw_cpp_error(const error& err)
    {
        try {
            value js_error = make_cpp_error(err);
            return JS_Throw(context_, js_error.release());
        } catch (...) {
            // Fallback: if constructing the structured error itself fails
            // (e.g. eval of the constructor bootstrap threw), fall back to
            // QuickJS' own InternalError so that we still surface *something*
            // rather than swallowing the C++ exception.
            return JS_ThrowInternalError(context_, "%s", err.message.c_str());
        }
    }

private:
    friend class value;
    friend class value_view;
    template <typename T>
    friend class type_builder;

    [[nodiscard]] value make_function_impl(std::string_view name, native_function fn, int length = 0)
    {
        auto* heap_function = new native_function(std::move(fn));
        std::string owned_name{name};
        value v(*this, JS_NewCClosure(
            context_,
            &native_trampoline,
            owned_name.c_str(),
            [](void* opaque) { delete static_cast<native_function*>(opaque); },
            length,
            0,
            heap_function));
        if (v.is_exception()) {
            delete heap_function;
            detail::throw_error(exception_string());
        }
        return v;
    }

    [[nodiscard]] static std::string next_closure_name()
    {
        static std::atomic_uint64_t counter{0};
        std::uint64_t id = counter.fetch_add(1, std::memory_order_relaxed) + 1;
        char buffer[sizeof("closure_") + 16]{};
        std::snprintf(buffer, sizeof(buffer), "closure_%06llu",
            static_cast<unsigned long long>(id));
        return buffer;
    }

    struct holder_base {
        virtual ~holder_base() = default;
        virtual void* pointer() noexcept = 0;
    };

    // JS owns a C++ instance stored inline in the holder, constructed either
    // by copy (const T&) or by move (T&&).
    template <typename T>
    struct value_holder final : holder_base {
        explicit value_holder(const T& v) : value(v) {}
        explicit value_holder(T&& v) noexcept(std::is_nothrow_move_constructible_v<T>)
            : value(std::move(v)) {}
        void* pointer() noexcept override { return &value; }
        T value;
    };

    // JS becomes the unique owner of the pointee.
    template <typename Pointer>
    struct unique_holder final : holder_base {
        explicit unique_holder(Pointer v) noexcept : value(std::move(v)) {}
        void* pointer() noexcept override { return value.get(); }
        Pointer value;
    };

    // JS shares ownership with the caller.
    template <typename Pointer>
    struct shared_holder final : holder_base {
        explicit shared_holder(Pointer v) noexcept : value(std::move(v)) {}
        void* pointer() noexcept override { return value.get(); }
        Pointer value;
    };

    template <typename T>
    static void object_finalizer(JSRuntime*, JSValue v)
    {
        auto id = mutable_class_id<T>();
        delete static_cast<holder_base*>(JS_GetOpaque(v, id));
    }

    template <typename T>
    static JSClassID& mutable_class_id()
    {
        static JSClassID id = 0;
        return id;
    }

    template <typename T>
    static std::string& class_name()
    {
        static std::string name;
        return name;
    }

    template <typename T>
    JSClassID ensure_class(std::string_view js_name)
    {
        JSClassID& id = mutable_class_id<T>();
        if (id == 0) {
            JS_NewClassID(raw_runtime(), &id);
            class_name<T>() = std::string(js_name);
            JSClassDef def{};
            def.class_name = class_name<T>().c_str();
            def.finalizer = &object_finalizer<T>;
            if (JS_NewClass(raw_runtime(), id, &def) < 0) {
                detail::throw_error("JS_NewClass failed for " + class_name<T>());
            }
        }
        return id;
    }

    template <typename T>
    T* opaque_this(JSValueConst this_value)
    {
        auto* holder = static_cast<holder_base*>(JS_GetOpaque2(context_, this_value, mutable_class_id<T>()));
        if (holder == nullptr) {
            return nullptr;
        }
        return static_cast<T*>(holder->pointer());
    }

    void ensure_context(value_view v) const
    {
        if (v.ctx() != context_) {
            detail::throw_error("QuickJS value belongs to a different context");
        }
    }

    static JSValue native_trampoline(JSContext* ctx, JSValueConst this_value, int argc, JSValueConst* argv, int, void* opaque)
    {
        try {
            context_ref c(ctx);
            call_args args(c, this_value, argc, argv);
            value result = (*static_cast<native_function*>(opaque))(args);
            if (result.ctx() != ctx) {
                return JS_ThrowTypeError(ctx, "native callback returned a value from a different context");
            }
            return result.release();
        } catch (const exception& err) {
            return context_ref(ctx).throw_cpp_error(err.info());
        } catch (const std::exception& err) {
            return context_ref(ctx).throw_cpp_error(
                error{err.what(), std::source_location::current()});
        } catch (...) {
            return context_ref(ctx).throw_cpp_error(
                error{"unknown C++ exception", std::source_location::current()});
        }
    }

    // Build a JS `CppError` instance (subclass of `Error`) that mirrors the
    // fields of `qjs::error`. Called only from throw_cpp_error(); it does
    // NOT install a pending exception on the JS side by itself.
    value make_cpp_error(const error& err)
    {
        ensure_cpp_error_ctor();

        // `new CppError(message)` — inherits from Error, so `instanceof Error`
        // and `instanceof CppError` both hold, and QuickJS auto-populates
        // `.stack` with the JS-side backtrace.
        std::string message = err.message;
        JSValue msg_raw = JS_NewStringLen(context_, message.data(), message.size());
        if (JS_IsException(msg_raw)) {
            detail::throw_error(exception_string());
        }
        JSValue argv[1] = { msg_raw };
        JSValue instance_raw = JS_CallConstructor(context_, cpp_error_ctor_->raw(), 1, argv);
        JS_FreeValue(context_, msg_raw);
        if (JS_IsException(instance_raw)) {
            detail::throw_error(exception_string());
        }
        value instance(*this, instance_raw);

        // Attach structured metadata; use plain string properties so JS code
        // can read them without extra decoding.
        instance.set("fileName", make_string(err.location.file_name()));
        instance.set("lineNumber", make_integer(static_cast<std::int64_t>(err.location.line())));
        instance.set("columnNumber", make_integer(static_cast<std::int64_t>(err.location.column())));
        instance.set("functionName", make_string(err.location.function_name()));

#if QJS_CPP_WRAPPER_HAS_STACKTRACE
        // Serialise the C++ stack once and expose it verbatim as `cppStack`,
        // then splice it in front of QuickJS' native `stack` so a single
        // `err.stack` read returns both C++ and JS frames (option B).
        std::ostringstream cpp_stack_stream;
        cpp_stack_stream << err.stacktrace;
        std::string cpp_stack = std::move(cpp_stack_stream).str();
        instance.set("cppStack", make_string(cpp_stack));

        value existing_stack = instance.get("stack");
        std::string combined = "C++ stack:\n" + cpp_stack;
        if (existing_stack.is_string()) {
            combined += "\nJS stack:\n";
            combined += existing_stack.to_string();
        }
        instance.set("stack", make_string(combined));
#endif

        return instance;
    }

    // Bootstrap and cache the `CppError` constructor for this context. The
    // class is also exposed as `globalThis.CppError` so scripts can perform
    // `err instanceof CppError` in any scope.
    void ensure_cpp_error_ctor()
    {
        if (!cpp_error_ctor_->is_undefined()) {
            return;
        }
        // Note: eval'ing the class expression returns the constructor value;
        // assigning it to globalThis makes the identifier reachable to user
        // scripts running under this context.
        static constexpr std::string_view bootstrap =
            "globalThis.CppError = class CppError extends Error {"
            "  constructor(message) {"
            "    super(message);"
            "    this.name = 'CppError';"
            "  }"
            "};"
            "globalThis.CppError";
        value ctor(*this, JS_Eval(
            context_,
            bootstrap.data(),
            bootstrap.size(),
            "<qjs:CppError>",
            JS_EVAL_TYPE_GLOBAL));
        if (ctor.is_exception()) {
            detail::throw_error(exception_string());
        }
        *cpp_error_ctor_ = std::move(ctor);
    }

protected:
    context_ref(runtime* rt, JSContext* ctx, value* cpp_error_ctor) noexcept
        : runtime_(rt), context_(ctx), cpp_error_ctor_(cpp_error_ctor)
    {
    }

    runtime* runtime_;
    JSContext* context_;
    value* cpp_error_ctor_;
};

class context final : public context_ref {
public:
    explicit context(runtime& rt)
        : context_ref(&rt, JS_NewContext(rt.raw()), &cpp_error_ctor_storage_)
    {
        if (context_ == nullptr) {
            detail::throw_error("JS_NewContext failed");
        }
        JS_SetContextOpaque(context_, static_cast<context_ref*>(this));
    }

    context(const context&) = delete;
    context& operator=(const context&) = delete;

    context(context&&) = delete;
    context& operator=(context&&) = delete;

    ~context()
    {
        // Release cached JS handles that reference this context BEFORE the
        // underlying JSContext is freed; otherwise their embedded ctx pointer
        // would dangle when the value's destructor runs after JS_FreeContext.
        cpp_error_ctor_storage_ = value{};
        if (context_ != nullptr) {
            JS_SetContextOpaque(context_, nullptr);
            JS_FreeContext(context_);
        }
    }

private:
    // Lazily-populated cache of the JS `CppError` constructor used to bridge
    // C++ exceptions into the JS side (see ensure_cpp_error_ctor()). Kept
    // per-context so that multiple contexts sharing a runtime each own their
    // own CppError class object.
    value cpp_error_ctor_storage_{};
};

} // namespace qjs
