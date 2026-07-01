#pragma once

#include "common.inc.hpp"
#include "value.hpp"

namespace qjs {

class property_ref final {
public:
    property_ref(value& obj, std::string_view name)
        : object_(&obj), name_(name)
    {
    }

    [[nodiscard]] operator value() const { return object_->get(name_); }

    template <typename T>
    property_ref& operator=(T&& v)
    {
        context& ctx = context::from_raw(object_->ctx());
        object_->set(name_, ctx.make_value(std::forward<T>(v)));
        return *this;
    }

    template <typename T>
    [[nodiscard]] T as() const
    {
        return static_cast<value>(*this).template as<T>();
    }

private:
    value* object_;
    std::string name_;
};

} // namespace qjs
