#include "engine/runtime.h"
#include "engine/context.h"
#include "engine/value.h"
#include "engine/promise.h"
#include <iostream>
#include <chrono>
#include <boost/asio/steady_timer.hpp>

int main(int argc, char* argv[]) {
    engine::runtime rt;
    engine::context ctx { rt };
    while (auto scope = ctx.scope()) {
        ctx.global().set("hello", {"hello"});
        ctx.global().set("world", {"world"});
        ctx.global().set("array", {"abc", "123", 456});
        ctx.global().set("object", {{"abc",123}, {"xyz","xxyyzz"}});
        ctx.global().set("test", {[&ctx] (const engine::arguments&) -> engine::value {
            std::cout << "test\n";
            auto promise = std::make_shared<engine::promise>();
            auto timer = std::make_shared<boost::asio::steady_timer>(ctx.io());
            timer->expires_after(std::chrono::seconds(5));
            timer->async_wait(engine::context_handler(ctx, [promise, timer] (const boost::system::error_code& error) {
                promise->resolve({ 
                    std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()});
            }));
            return promise->future();
        }});
    }
    ctx.run(R"JS("use strict"; 
    print("> starting ...");
    (async function () {
        print('> before await');
        let x = await test();
        print('> after await');
        print(x);
    })();
    print(hello, world, array, object);
    print("> finished.");
)JS");
    return 0;
}