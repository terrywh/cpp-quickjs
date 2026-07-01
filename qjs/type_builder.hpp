#pragma once

#include "common.inc.hpp"
#include "context.hpp"
#include "expose.hpp"

namespace qjs {

namespace detail {

template <typename T>
consteval auto reflected_members()
{
    return std::define_static_array(std::meta::members_of(^^T, std::meta::access_context::unchecked()));
}

// True iff `member` carries a `qjs::expose` annotation.
consteval bool is_exposed(std::meta::info member)
{
    return !std::meta::annotations_of_with_type(member, ^^expose).empty();
}

// Precondition: `is_exposed(member)` is true. Returns the `qjs::expose`
// annotation value verbatim so callers can inspect the inline rename buffer.
//
// The `expose` payload is intentionally shaped as an aggregate of
// `char[N] + size_t` — a structural type free of pointers — which is the
// exact contract `std::meta::extract` needs to succeed on GCC 16 (raw
// `const char*` fields would trip "reflect_constant failed" here).
consteval expose exposed_annotation(std::meta::info member)
{
    return std::meta::extract<expose>(
        std::meta::annotations_of_with_type(member, ^^expose).front());
}

} // namespace detail

template <typename T>
class type_builder final {
private:
    friend class context;
    friend class value;

    type_builder(context& ctx, std::string_view js_name)
        : context_(&ctx), js_name_(js_name), class_id_(ctx.ensure_class<T>(js_name))
    {
        prototype_ = ctx.make_object();
        JS_SetClassProto(ctx.raw(), class_id_, JS_DupValue(ctx.raw(), prototype_.raw()));
    }

    // Populate prototype with reflected members and produce the constructor
    // value. The caller is responsible for installing it on a target object /
    // the global scope via context::set / value::set.
    value build()
    {
        define_public_members();
        return build_constructor();
    }

    void define_public_members()
    {
        static_assert(
            QJS_CPP_WRAPPER_STATIC_REFLECTION_VERSION >= 1,
            "context::make_class<T>() requires standardized C++26 static reflection support.");
        template for (constexpr auto member : detail::reflected_members<T>()) {
            // Opt-in export: only members annotated with `[[=qjs::expose{...}]]`
            // are reflected into JS. Access level is intentionally NOT checked
            // — annotating a private member is a deliberate act of the class
            // author and should just work.
            if constexpr (detail::is_exposed(member)) {
                if constexpr (std::meta::has_identifier(member)) {
                    // Materialise the annotation as a `static constexpr`
                    // aggregate: this both dodges libstdc++'s debug-checked
                    // `optional` machinery and gives the inline `data`
                    // buffer program-lifetime storage, so a `string_view`
                    // fabricated from it below is valid indefinitely.
                    //
                    // We deliberately construct the string_view *inline* at
                    // each dispatch site rather than binding it to a
                    // `constexpr` variable — `std::string_view` is
                    // non-structural (private members) and GCC 16's
                    // `reflect_constant` refuses to promote it to a constexpr
                    // initialiser. Passing it as a plain runtime rvalue keeps
                    // that path clean.
                    static constexpr expose ann = detail::exposed_annotation(member);
                    if constexpr (std::meta::is_nonstatic_data_member(member)) {
                        if constexpr (ann.size == 0) {
                            property(std::meta::identifier_of(member), &[:member:]);
                        } else {
                            property(std::string_view{ann.data, ann.size}, &[:member:]);
                        }
                    } else if constexpr (
                        std::meta::is_function(member) &&
                        std::meta::is_class_member(member) &&
                        !std::meta::is_static_member(member) &&
                        !std::meta::is_special_member_function(member) &&
                        !std::meta::is_operator_function(member)) {
                        if constexpr (ann.size == 0) {
                            method<&[:member:]>(std::meta::identifier_of(member));
                        } else {
                            method<&[:member:]>(std::string_view{ann.data, ann.size});
                        }
                    } else {
                        static_assert(detail::dependent_false<T>,
                            "qjs::expose only supports non-static data members "
                            "and non-static, non-special, non-operator member functions.");
                    }
                }
            }
        }
    }
    value build_constructor()
    {
        if constexpr (std::default_initializable<T>) {
            value constructor(*context_, JS_NewCFunction2(
                context_->raw(),
                &constructor_trampoline,
                js_name_.c_str(),
                0,
                JS_CFUNC_constructor,
                0));
            if (constructor.is_exception()) {
                detail::throw_error(context_->exception_string());
            }
            if (JS_SetConstructor(context_->raw(), constructor.raw(), prototype_.raw()) < 0) {
                detail::throw_error(context_->exception_string());
            }
            return constructor;
        } else {
            return context_->make_undefined();
        }
    }

    template <auto MemberFunction>
    void method(std::string_view name)
    {
        using traits = detail::member_pointer_traits<decltype(MemberFunction)>;
        using C = typename traits::class_type;
        static_assert(std::same_as<C, T>, "method() member pointer must belong to the reflected type");

        context* ctx = context_;
        value fn = context_->make_function(name, [ctx](call_args args) -> value {
            T* object = ctx->template opaque_this<T>(args.this_value().raw());
            if (object == nullptr) {
                detail::throw_error("JS method called with incompatible this object");
            }
            return invoke_member<MemberFunction>(*ctx, *object, args);
        }, 0);
        prototype_.set(name, std::move(fn));
    }

    template <typename Member>
    void property(std::string_view name, Member T::*member)
    {
        context* ctx = context_;
        auto getter = [ctx, member](call_args args) -> value {
            T* object = ctx->template opaque_this<T>(args.this_value().raw());
            if (object == nullptr) {
                detail::throw_error("JS getter called with incompatible this object");
            }
            return ctx->make_value(object->*member);
        };

        value getter_value = context_->make_function(name, std::move(getter), 0);
        std::string owned_name{name};
        JSAtom atom = JS_NewAtomLen(context_->raw(), owned_name.data(), owned_name.size());
        if (atom == JS_ATOM_NULL) {
            detail::throw_error(context_->exception_string());
        }
        int rc = JS_DefinePropertyGetSet(
            context_->raw(),
            prototype_.raw(),
            atom,
            getter_value.release(),
            JS_UNDEFINED,
            JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(context_->raw(), atom);
        if (rc < 0) {
            detail::throw_error(context_->exception_string());
        }
    }

    template <auto MemberFunction, typename Traits = detail::member_pointer_traits<decltype(MemberFunction)>>
    static value invoke_member(context& ctx, T& object, call_args args)
    {
        using args_tuple = typename Traits::args_tuple;
        return invoke_member_impl<MemberFunction>(ctx, object, args, std::make_index_sequence<std::tuple_size_v<args_tuple>>{});
    }

    template <auto MemberFunction, std::size_t... I>
    static value invoke_member_impl(context& ctx, T& object, call_args args, std::index_sequence<I...>)
    {
        using traits = detail::member_pointer_traits<decltype(MemberFunction)>;
        using args_tuple = typename traits::args_tuple;
        using return_type = typename traits::return_type;

        if (args.size() < sizeof...(I)) {
            detail::throw_error("JS method received too few arguments");
        }

        if constexpr (std::same_as<return_type, void>) {
            std::invoke(MemberFunction, object, args[I].template as<detail::remove_cvref_t<std::tuple_element_t<I, args_tuple>>>()...);
            return ctx.make_undefined();
        } else {
            return ctx.make_value(std::invoke(MemberFunction, object, args[I].template as<detail::remove_cvref_t<std::tuple_element_t<I, args_tuple>>>()...));
        }
    }

    static JSValue constructor_trampoline(JSContext* ctx, JSValueConst, int, JSValueConst*)
    {
        try {
            context& c = context::from_raw(ctx);
            return c.make_object(std::make_unique<T>()).release();
        } catch (const exception& err) {
            return context::from_raw(ctx).throw_cpp_error(err.info());
        } catch (const std::exception& err) {
            return context::from_raw(ctx).throw_cpp_error(
                error{err.what(), std::source_location::current()});
        } catch (...) {
            return context::from_raw(ctx).throw_cpp_error(
                error{"unknown C++ exception", std::source_location::current()});
        }
    }

    context* context_;
    std::string js_name_;
    JSClassID class_id_;
    value prototype_;
};

// -----------------------------------------------------------------------------
// context: class / object factories (definitions rely on type_builder & holders).
// -----------------------------------------------------------------------------

template <typename T>
inline value context::make_class(std::string_view js_name)
{
    return type_builder<T>(*this, js_name).build();
}

template <typename T>
inline value context::make_class()
{
    return make_class<T>(std::meta::identifier_of(^^T));
}

template <typename T>
    requires (!detail::is_smart_ptr_v<detail::remove_cvref_t<T>>)
inline value context::make_object(const T& object)
{
    auto id = ensure_class<T>(typeid(T).name());
    value v(*this, JS_NewObjectClass(context_, id));
    if (v.is_exception()) {
        detail::throw_error(exception_string());
    }
    JS_SetOpaque(v.raw(), new copy_holder<T>(object));
    return v;
}

template <typename T>
    requires (!std::is_reference_v<T> &&
              !detail::is_smart_ptr_v<detail::remove_cvref_t<T>>)
inline value context::make_object(T&& object)
{
    auto id = ensure_class<T>(typeid(T).name());
    value v(*this, JS_NewObjectClass(context_, id));
    if (v.is_exception()) {
        detail::throw_error(exception_string());
    }
    JS_SetOpaque(v.raw(), new owned_holder<T>(std::move(object)));
    return v;
}

template <typename T>
inline value context::make_object(std::shared_ptr<T> object)
{
    auto id = ensure_class<T>(typeid(T).name());
    value v(*this, JS_NewObjectClass(context_, id));
    if (v.is_exception()) {
        detail::throw_error(exception_string());
    }
    JS_SetOpaque(v.raw(), new shared_holder<T>(std::move(object)));
    return v;
}

template <typename T>
inline value context::make_object(std::unique_ptr<T> object)
{
    auto id = ensure_class<T>(typeid(T).name());
    value v(*this, JS_NewObjectClass(context_, id));
    if (v.is_exception()) {
        detail::throw_error(exception_string());
    }
    JS_SetOpaque(v.raw(), new unique_holder<T>(std::move(object)));
    return v;
}

// -----------------------------------------------------------------------------
// Reflected free-function invocation used by context::make_function<Fn>().
// -----------------------------------------------------------------------------

namespace detail {

template <auto Fn, std::size_t... I>
inline value invoke_free_function_impl(context& ctx, call_args args, std::index_sequence<I...>)
{
    using traits = function_pointer_traits<decltype(Fn)>;
    using args_tuple = typename traits::args_tuple;
    using return_type = typename traits::return_type;

    if (args.size() < sizeof...(I)) {
        throw_error("JS function received too few arguments");
    }

    if constexpr (std::same_as<return_type, void>) {
        Fn(args[I].template as<remove_cvref_t<std::tuple_element_t<I, args_tuple>>>()...);
        return ctx.make_undefined();
    } else {
        return ctx.make_value(Fn(args[I].template as<remove_cvref_t<std::tuple_element_t<I, args_tuple>>>()...));
    }
}

template <auto Fn>
inline value invoke_free_function(context& ctx, call_args args)
{
    using traits = function_pointer_traits<decltype(Fn)>;
    using args_tuple = typename traits::args_tuple;
    return invoke_free_function_impl<Fn>(
        ctx, args, std::make_index_sequence<std::tuple_size_v<args_tuple>>{});
}

} // namespace detail

template <auto Fn>
inline value context::make_function(std::string_view js_name)
{
    static_assert(
        std::is_pointer_v<decltype(Fn)> && std::is_function_v<std::remove_pointer_t<decltype(Fn)>>,
        "context::make_function<Fn>() requires a free function or static member function pointer");

    using traits = detail::function_pointer_traits<decltype(Fn)>;
    context* self = this;
    return make_function(js_name,
        [self](call_args args) -> value {
            return detail::invoke_free_function<Fn>(*self, args);
        },
        static_cast<int>(traits::arity));
}

template <auto Fn>
inline value context::make_function()
{
    static_assert(
        QJS_CPP_WRAPPER_STATIC_REFLECTION_VERSION >= 1,
        "context::make_function<Fn>() without a name argument requires standardized C++26 static reflection support.");
    return make_function<Fn>(std::meta::identifier_of(std::meta::reflect_function(*Fn)));
}

// -----------------------------------------------------------------------------
// context::make_value<T> — generic C++ -> qjs::value dispatcher.
// -----------------------------------------------------------------------------

template <typename T>
inline value context::make_value(T&& v)
{
    using U = detail::remove_cvref_t<T>;
    if constexpr (std::same_as<U, value>) {
        return std::forward<T>(v);
    } else if constexpr (std::same_as<U, value_view>) {
        if (v.ctx() != context_) {
            detail::throw_error("QuickJS value belongs to a different context");
        }
        return value(*this, JS_DupValue(context_, v.raw()));
    } else if constexpr (std::same_as<U, std::string>) {
        return make_string(v);
    } else if constexpr (std::constructible_from<std::string_view, T>) {
        return make_string(std::string_view(v));
    } else if constexpr (std::same_as<U, bool>) {
        return make_bool(v);
    } else if constexpr (std::integral<U> && std::is_signed_v<U>) {
        return make_integer(static_cast<std::int64_t>(v));
    } else if constexpr (std::floating_point<U>) {
        return make_float(static_cast<double>(v));
    } else {
        static_assert(detail::dependent_false<U>, "Type is not convertible to qjs::value");
    }
}

} // namespace qjs
