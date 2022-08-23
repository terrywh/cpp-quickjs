#pragma once
#include "value.h"

namespace engine {

class promise {
    JSContext* ctx_;
    value::fake_type f_[2];
    value::fake_type p_;
public:
     promise();
    ~promise();
    void resolve(value v) const;
    void reject(value v) const;
    value future();
    operator value() { return future(); }
};

} // namespace engine