#pragma once
#include <vector>

struct JSContext;
struct JSValue;

namespace engine {

class value;
class arguments {
    std::vector<value> args_;
    arguments(JSContext* ctx, int argc, JSValue* argv);

public:
    int size() const { return args_.size(); }
    const value& at(int index) const { return args_.at(index); }
    const value& operator[](int index) const { return args_.at(index); }

    std::vector<value>::iterator begin() { return args_.begin(); }
    std::vector<value>::iterator end() { return args_.begin(); }

    std::vector<value>::const_iterator begin() const { return args_.begin(); }
    std::vector<value>::const_iterator end() const { return args_.begin(); }

    friend class value;
};

} // namespace engine