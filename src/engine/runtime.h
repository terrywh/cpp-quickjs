#pragma once

struct JSRuntime;

namespace engine {

class runtime {
    JSRuntime *runtime_;

public:
    runtime();
    ~runtime();
    inline JSRuntime* native() { return runtime_; }
    operator JSRuntime*() const { return runtime_; }
};

} // namespace engine