#pragma once

#include "common.inc.hpp"
#include "context.hpp"
#include "value.hpp"

namespace qjs {

class property_ref final {
public:
    property_ref(value& obj, std::string_view name)
        : object_(&obj), name_(name)
    {
    }

    [[nodiscard]] operator value() const { return object_->get(name_); }

    property_ref& operator=(const value& v)
    {
        object_->set(name_, v);
        return *this;
    }

    property_ref& operator=(value&& v)
    {
        object_->set(name_, std::move(v));
        return *this;
    }

    template <typename T>
        requires (!std::same_as<detail::remove_cvref_t<T>, value>)
    property_ref& operator=(T&& v)
    {
        context_ref ctx(object_->ctx());
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
