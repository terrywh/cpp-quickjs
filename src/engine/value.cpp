#include "value.h"
#include "context.h"
#include "arguments.h"
#include <quickjs.h>
#include <cstring>
#include <iostream>

namespace engine {

value_ref::operator value() const {
    switch (type_) {
    case type_null:
        return value(nullptr);
    case type_integer_32:
        return value(data_.i32);
    case type_integer_64:
        return value(data_.i64);
    case type_float_single:
        return value(data_.sfn);
    case type_float_double:
        return value(data_.dfn);
    case type_string:
        return value(data_.str);
    case type_function:
        return value(data_.fn);
    case type_object:
    case type_array:    
    default:
        return value{};
    }
}

value::value(JSContext* context, JSValue value, bool ref)
: context_(context)
, ref_(ref) {
    std::memcpy(&value_, &value, sizeof(JSValue));
}

JSValue& value::native() const {
    return *const_cast<JSValue*>(reinterpret_cast<const JSValue*>(&value_));
}

value::operator JSValue&() const {
    return *const_cast<JSValue*>(reinterpret_cast<const JSValue*>(&value_));
}

value::operator JSValue*() const {
    return const_cast<JSValue*>(reinterpret_cast<const JSValue*>(&value_));
}

JSValue value::clone() const {
    return JS_DupValue(context_, native());
}

JSValue value::release() {
    ref_ = false;
    return native();
}

value::value(const value& v)
: context_(v.context_)
, ref_(true) {
    native() = JS_DupValue(v.context_, v.native());
}

value::value(value&& v)
: context_(v.context_)
, ref_(v.ref_) {
    v.native() = v.native();
    v.ref_ = false;
}

value::~value() {
    if (ref_) JS_FreeValue(context(), native());
}

value::value()
: context_ { nullptr }
, ref_(false) {
    native() = JS_UNDEFINED;
}

value::value(std::nullptr_t)
: context_ { nullptr }
, ref_(false) {
    native() = JS_NULL;
}

value::value(int num)
: context_ { context_scope::get() }
, ref_(true) {
    native() = JS_NewInt32(context_, num);
}

value::value(int64_t num)
: context_ { context_scope::get() }
, ref_(true) {
    native() = JS_NewInt64(context_, num);
}

value::value(int64_t num, bool bigint)
: context_ {context_scope::get() }
, ref_(true) {
    native() = JS_NewBigInt64(context_, num);
}

value::value(float num)
: context_ { context_scope::get() }
, ref_(true) {
    native() = JS_NewFloat64(context_, num);
}

value::value(double num)
: context_ { context_scope::get() }
, ref_(true) {
    native() = JS_NewFloat64(context_, num);
}

value::value(const char* str, std::size_t len)
: context_ { context_scope::get() }
, ref_(true) {
    len > 0 ? native() = JS_NewStringLen(context_, str, len) : native() = JS_NewString(context_, str);
}

value::value(std::string_view str) 
: context_ { context_scope::get() }
, ref_(true) {
    native() = JS_NewStringLen(context_, str.data(), str.size());
}

value::value(value_ref::function_type fn)
: context_ { context_scope::get() }
, ref_(true) {
    native() = JS_NewCFunction(context_, fn, nullptr, 0);
}

JSValue value::fn_proxy(JSContext* ctx, JSValueConst that, int argc, JSValueConst* argv, int dc, JSValue* dv) {
    context_scope scope { ctx };
    auto* pfn = reinterpret_cast<std::function<value (const arguments&)>*>(JS_VALUE_GET_PTR(dv[0]));
    return pfn->operator()({ctx, argc, argv}).release();
}

value::value(std::function<value (const arguments&)> fn)
: context_ { context_scope::get() }
, ref_(true) {
    auto* pfn = new std::function<value (const arguments& args)>(fn);
    JSValue val = JS_MKPTR(/*JS_TAG_POINTER =*/8, pfn);
    native() = JS_NewCFunctionData(context_, fn_proxy, 0, 0, 1, &val);
    JS_FreeValue(context_, val);
}

value::value(std::initializer_list<value_ref> list)
: context_ { context_scope::get() }
, ref_(true) {
    if (is_object_init(list)) {
        native() = JS_NewObject(context_);
        for (auto &ref: list) {
            auto field = ref.data_.array.begin();
            auto value = field + 1;
            set(field->data_.str, *value);
        }
    }
    else {
        native() = JS_NewArray(context_);
        int index = 0;
        for (auto& ref: list) set(index++, ref);
    }
}

value value::get(std::string_view field) const {
    auto afield = JS_NewAtomLen(context_, field.data(), field.size());
    value rv {context_, JS_GetProperty(context_, native(), afield)};
    JS_FreeAtom(context_, afield);
    return rv;
}

void value::set(std::string_view field, const value& v) {
    // JS_SetPropertyStr(context_, native(), field.data(), JS_NewStringLen(context_, "abc", 3));
    auto afield = JS_NewAtomLen(context_, field.data(), field.size());
    JS_SetProperty(context_, native(), afield, v.clone());
    JS_FreeAtom(context_, afield);
}

void value::set(std::string_view field, value&& v) {
    // JS_SetPropertyStr(context_, native(), field.data(), JS_NewStringLen(context_, "abc", 3));
    auto afield = JS_NewAtomLen(context_, field.data(), field.size());
    JS_SetProperty(context_, native(), afield, v.release());
    JS_FreeAtom(context_, afield);
}

void value::set(int index, const value& v) {
    auto afield = JS_NewAtomUInt32(context_, index);
    JS_SetProperty(context_, native(), afield, v.clone());
    JS_FreeAtom(context_, afield);
}

value& value::operator =(const value& v) {
    value d {v};
    swap(*this, d);
}

value& value::operator =(value&& v) {
    value m {std::move(v)};
    swap(*this, m);
}

bool value::is_object_init(std::initializer_list<value_ref>& list) {
    int is_object = list.size();
    for (auto& ref: list) if (!ref.is_keyvalue_pair()) --is_object;
    return is_object == list.size();
}

void swap(value& a, value& b) {
    std::swap(a.context_, b.context_);
    std::swap(a.value_, b.value_);
}

} // namespace engine