#pragma once
#include <cstdint>
#include <functional>
#include <string_view>
#include <type_traits>

struct JSValue;
#define JSValueConst JSValue
struct JSContext;

namespace engine {

class context;
class value;
class object;
class array;
class arguments;

// 惰性创建（用于初始化列表）
class value_ref {
public:
    enum type_ref {
        type_null,
        type_integer_32,
        type_integer_64,
        type_float_single,
        type_float_double,
        type_string,
        type_function,
        type_object,
        type_array,
    };
    using function_type = JSValue (*)(JSContext* ctx, JSValueConst self, int argc, JSValueConst* argv);
    int type_;
    union {
        std::nullptr_t  null;
        std::int32_t     i32;
        std::int64_t     i64;
        float            sfn;
        double           dfn;
        std::string_view str;
        function_type     fn;
        //
        std::initializer_list<value_ref> array;
    } data_;
    
    value_ref(std::nullptr_t): type_(type_null), data_ {.null = nullptr} {}
    value_ref(int num): type_(type_integer_32), data_{.i32 = num} {}
    value_ref(int64_t num): type_(type_integer_64), data_{.i64 = num} {}
    value_ref(float sfn): type_(type_float_single), data_{.sfn = sfn} {}
    value_ref(double dfn): type_(type_float_double), data_{.dfn = dfn} {}
    value_ref(const char* str, std::size_t len = 0): type_(type_string), data_{.str = len > 0 ? std::string_view(str, len) : std::string_view(str)} {}
    value_ref(std::string_view str): type_(type_string), data_{.str = str} {}
    value_ref(function_type fn): type_(type_function), data_{.fn = fn} {}
    // object
    value_ref(std::initializer_list<value_ref> array): type_(type_array), data_{.array = array} {}
    
    operator value() const;

private:
    inline bool is_keyvalue_pair() const {
        return type_ == type_array && data_.array.size() == 2 && data_.array.begin()->type_ == type_string;
    }
    friend class value;
};

class value {
public:
    struct fake_type {
        void * _1, *_2;
    };

protected:
    mutable JSContext* context_ = nullptr;
    fake_type value_;
    bool ref_;

public:
    JSValue& native() const;
    operator JSValue&() const;
    operator JSValue*() const;
    inline JSContext* context() { return context_; }
    JSValue clone() const;
    JSValue release(); // 将持有的 JS_Value 分离出来

    value(JSContext* ctx, JSValue v, bool ref = true);
    value(const value& v);
    value(value&& v);
    ~value();

    value();
    value(std::nullptr_t);
    value(int num);
    value(int64_t num);
    value(int64_t num, bool bigint);
    value(float num);
    value(double num);
    value(const char* str, std::size_t len = 0);
    value(std::string_view str);
    value(value_ref::function_type fn);
    value(std::function<value (const arguments&)> fn);
    value(std::initializer_list<value_ref> list);

    value get(std::string_view field) const;
    void set(std::string_view field, const value& v);
    void set(std::string_view field, value&& v);
    // void set(std::string_view field, std::function<value (const arguments&)> fn) { set(field, value{fn}); }
    void set(int index, const value& v);
    // void set(int index, std::function<value (const arguments&)> fn) { set(index, value{fn}); }
    value& operator =(const value& v);
    value& operator =(value&& v);

private:
    static bool is_object_init(std::initializer_list<value_ref>& list);
    static JSValue fn_proxy(JSContext* ctx, JSValueConst self, int argc, JSValueConst* argv, int dc, JSValue* dv);

    friend void swap(value& a, value& b);
    friend class context;
};

} // namespace engine