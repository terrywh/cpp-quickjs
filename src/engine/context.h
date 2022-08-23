#pragma once
#include <boost/system/error_code.hpp>
#include <boost/asio/io_context.hpp>
#include <string_view>

struct JSRuntime;
struct JSContext;

namespace engine {

class runtime;
class value_ref;
class value;

class context_scope;
template <class Handler>
class context_handler;

class context {
    boost::asio::io_context io_;
    JSContext *js_ = nullptr;

public:
    context(JSRuntime* rt, value global);
    context(JSRuntime* rt);
    context();
    
    ~context();

    inline boost::asio::io_context& io() { return io_; }
    inline JSContext* js() const { return js_; }
    inline operator JSContext*() const { return js_; }
    inline operator boost::asio::io_context&() { return io_; }

    value global();
    value run(std::string_view script);
    value run(std::string_view script, boost::system::error_code& error);

    context_scope scope() const;
    friend class context_scope;
};

class context_scope {
    static thread_local JSContext* ctx_;
public:
    explicit context_scope(JSContext* ctx);
    ~context_scope();
    operator bool() const { 
        static int once = 0;
        return ++once <= 1;
    }
    static const context& get();
};

template <class Handler>
class context_handler {
    context& ctx_;
    Handler fn_;

public:
    context_handler(context& ctx, Handler&& fn)
    : ctx_(ctx)
    , fn_(std::move(fn)) {}

    template <class... Args>
    void operator()(Args&&... args) {
        auto scope = ctx_.scope();
        fn_(std::forward<Args>(args)...);
    }
};
} // namespace engine