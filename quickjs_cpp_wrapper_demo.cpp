#include "qjs/qjs.hpp"

#include <iostream>
#include <string>

struct UserService {
    [[=qjs::expose{}]]
    std::string name{"demo-service"};

    [[=qjs::expose{}]]
    int add(int lhs, int rhs) { return lhs + rhs; }

    [[=qjs::expose{}]]
    std::string query(std::string key) const { return name + ":" + key; }
};

struct Calculator {
    // Renamed export: reachable from JS as `factor` even though the C++
    // identifier is `base`. Exercises the annotation's rename channel.
    [[=qjs::expose{"factor"}]]
    int base{100};

    [[=qjs::expose{}]]
    int scale(int factor) const { return base * factor; }

    // Unannotated helper: exercises that non-exposed methods stay hidden even
    // though they are public and non-special.
    int hidden_helper() const { return base + 1; }
};

// Free function to be exported via context::make_function<Fn>() using its C++ identifier.
int multiply(int lhs, int rhs) { return lhs * rhs; }

// Struct hosting a static member function; both should be exportable via context::make_function.
struct MathUtils {
    static int clamp(int v, int lo, int hi) {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    }
};

int main()
{
    try {
        qjs::runtime rt;
        qjs::context ctx(rt);
        qjs::context::scope scope(ctx);

        if (&qjs::context::current() != &ctx) {
            std::cerr << "QuickJS current context was not installed\n";
            return 1;
        }

        qjs::value object = ctx.eval("({ x: 41, add(a, b) { return a + b; } })");
        object["x"] = 42;

        int x = object["x"].as<int>();
        int js_sum = object.invoke("add", 20, 22).as<int>();

        ctx.set("nativeAdd", ctx.make_function("nativeAdd",
            [](qjs::call_args args) {
                // Use the explicit primitive factory to build the result value.
                return args.ctx().make_integer(
                    args[0].as<int>() + args[1].as<int>());
            }));

        int callback_sum = ctx.eval("nativeAdd(30, 12)").as<int>();

        UserService service;
        // Register the C++ class into JS under its C++ identifier.
        ctx.set("UserService", ctx.make_class<UserService>());
        ctx.set("service", ctx.make_object(service));

        // make_class overload: export Calculator to JS under a custom name "Calc".
        ctx.set("Calc", ctx.make_class<Calculator>("Calc"));

        std::string service_name = ctx.eval("service.name").as<std::string>();
        std::string service_query = ctx.eval("service.query('alpha')").as<std::string>();
        int service_sum = ctx.eval("service.add(7, 35)").as<int>();
        std::string constructed_query = ctx.eval("(new UserService()).query('beta')").as<std::string>();
        int constructed_sum = ctx.eval("(new UserService()).add(11, 31)").as<int>();

        // Constructor is registered under the alias, not the C++ type name.
        int aliased_scale = ctx.eval("(new Calc()).scale(6)").as<int>();
        bool alias_hides_cpp_name = ctx.eval("typeof Calculator === 'undefined'").as<bool>();
        std::string alias_ctor_name = ctx.eval("(new Calc()).constructor.name").as<std::string>();

        // make_function reflects the C++ identifier as the JS export name.
        ctx.set("multiply", ctx.make_function<&multiply>());
        // make_function overload: export a static member function under a custom JS name.
        ctx.set("clamp", ctx.make_function<&MathUtils::clamp>("clamp"));

        int free_fn_result = ctx.eval("multiply(6, 7)").as<int>();
        int free_fn_length = ctx.eval("multiply.length").as<int>();
        std::string free_fn_name = ctx.eval("multiply.name").as<std::string>();
        int static_fn_low = ctx.eval("clamp(-5, 0, 10)").as<int>();
        int static_fn_high = ctx.eval("clamp(42, 0, 10)").as<int>();
        int static_fn_pass = ctx.eval("clamp(7, 0, 10)").as<int>();

        // Compose namespace via `context::make_*` + `value::set(name, value)`.
        qjs::value ns = ctx.make_object();
        ns.set("multiply", ctx.make_function<&multiply>());
        ns.set("clamp", ctx.make_function<&MathUtils::clamp>("clamp"));
        ns.set("Calc", ctx.make_class<Calculator>("Calc"));
        ctx.set("ns", std::move(ns));

        int ns_multiply = ctx.eval("ns.multiply(6, 7)").as<int>();
        int ns_clamp = ctx.eval("ns.clamp(99, 0, 10)").as<int>();
        int ns_scale = ctx.eval("(new ns.Calc()).scale(4)").as<int>();
        bool ns_ctor_valid = ctx.eval("typeof ns.Calc === 'function'").as<bool>();
        std::string ns_ctor_name = ctx.eval("(new ns.Calc()).constructor.name").as<std::string>();

        // Explicit model: build a value carrying a
        // closure/reflected-function/class/wrapped-object, then set() it onto
        // context (global) or another value (namespace).
        ctx.set("sub", ctx.make_function("sub",
            [](qjs::call_args args) {
                // Generic dispatcher variant: make_value picks the right primitive factory.
                return args.ctx().make_value(
                    args[0].as<int>() - args[1].as<int>());
            }));

        ctx.set("mul", ctx.make_function<&multiply>("mul"));
        ctx.set("Calc2", ctx.make_class<Calculator>("Calc2"));

        UserService svc2;
        ctx.set("svc2", ctx.make_object(svc2));

        // Same explicit path onto a value (object as namespace).
        qjs::value api = ctx.make_object();
        api.set("mul", ctx.make_function<&multiply>("mul"));
        api.set("Calc", ctx.make_class<Calculator>("Calc"));
        ctx.set("api", std::move(api));

        int step_sub = ctx.eval("sub(50, 8)").as<int>();
        int step_mul = ctx.eval("mul(6, 7)").as<int>();
        int step_ctor = ctx.eval("(new Calc2()).scale(3)").as<int>();
        std::string step_svc_query = ctx.eval("svc2.query('gamma')").as<std::string>();
        int step_api_mul = ctx.eval("api.mul(3, 14)").as<int>();
        int step_api_ctor = ctx.eval("(new api.Calc()).scale(5)").as<int>();

        // Primitive factories: build stand-alone values, then set() them.
        ctx.set("answer", ctx.make_integer(42));
        ctx.set("pi", ctx.make_float(3.14));
        ctx.set("greeting", ctx.make_string("hello"));
        ctx.set("flag", ctx.make_bool(true));

        int prim_answer = ctx.eval("answer").as<int>();
        double prim_pi = ctx.eval("pi").as<double>();
        std::string prim_greeting = ctx.eval("greeting").as<std::string>();
        bool prim_flag = ctx.eval("flag").as<bool>();

        // C++ exception -> JS `CppError` (subclass of Error). JS-side code
        // identifies wrapper-originated failures via `instanceof CppError`.
        ctx.set("nativeThrow", ctx.make_function("nativeThrow",
            [](qjs::call_args) -> qjs::value {
                throw qjs::exception(qjs::error{
                    "boom-from-native",
                    std::source_location::current()});
            }));

        bool cpp_err_is_cpp_error = ctx.eval(
            "(() => { try { nativeThrow(); return false; }"
            "         catch (e) { return e instanceof CppError; } })()").as<bool>();
        bool cpp_err_is_error = ctx.eval(
            "(() => { try { nativeThrow(); return false; }"
            "         catch (e) { return e instanceof Error; } })()").as<bool>();
        std::string cpp_err_name = ctx.eval(
            "(() => { try { nativeThrow(); return ''; }"
            "         catch (e) { return e.name; } })()").as<std::string>();
        std::string cpp_err_message = ctx.eval(
            "(() => { try { nativeThrow(); return ''; }"
            "         catch (e) { return e.message; } })()").as<std::string>();
        bool cpp_err_has_filename = ctx.eval(
            "(() => { try { nativeThrow(); return false; }"
            "         catch (e) { return typeof e.fileName === 'string' && e.fileName.length > 0; } })()").as<bool>();
        bool cpp_err_has_line = ctx.eval(
            "(() => { try { nativeThrow(); return false; }"
            "         catch (e) { return Number.isInteger(e.lineNumber) && e.lineNumber > 0; } })()").as<bool>();
        // With stacktrace enabled the C++ frames are spliced ahead of the JS
        // stack (option B); we can't require any specific frame contents but
        // the `stack` property must at least be a non-empty string.
        bool cpp_err_has_stack = ctx.eval(
            "(() => { try { nativeThrow(); return false; }"
            "         catch (e) { return typeof e.stack === 'string' && e.stack.length > 0; } })()").as<bool>();
        // A regular JS-side Error must NOT satisfy `instanceof CppError`, so
        // the type check discriminates C++-originated failures precisely.
        bool js_err_not_cpp_error = ctx.eval(
            "(() => { try { throw new Error('js-only'); }"
            "         catch (e) { return !(e instanceof CppError) && (e instanceof Error); } })()").as<bool>();

        // Opt-in reflection: only members carrying `[[=qjs::expose{}]]` reach
        // the JS prototype. `Calculator::hidden_helper` is unannotated so it
        // must stay invisible; `Calculator::base` is annotated but renamed to
        // `factor`, so the C++ name must NOT leak while the alias resolves.
        bool hidden_field_absent = ctx.eval(
            "(new Calc()).base === undefined").as<bool>();
        bool renamed_field_visible = ctx.eval(
            "(new Calc()).factor === 100").as<bool>();
        bool hidden_method_absent = ctx.eval(
            "typeof (new Calc()).hidden_helper === 'undefined'").as<bool>();

        bool basic_value_checks =
            x == 42 && js_sum == 42 && callback_sum == 42 &&
            service_sum == 42 && constructed_sum == 42;
        bool naming_checks =
            service_name == "demo-service" &&
            service_query == "demo-service:alpha" &&
            constructed_query == "demo-service:beta" &&
            alias_hides_cpp_name && alias_ctor_name == "Calc" &&
            free_fn_name == "multiply" && ns_ctor_name == "Calc";
        bool function_checks =
            aliased_scale == 600 &&
            free_fn_result == 42 && free_fn_length == 2 &&
            static_fn_low == 0 && static_fn_high == 10 && static_fn_pass == 7;
        bool namespace_checks =
            ns_multiply == 42 && ns_clamp == 10 && ns_scale == 400 && ns_ctor_valid;
        bool explicit_set_checks =
            step_sub == 42 && step_mul == 42 && step_ctor == 300 &&
            step_svc_query == "demo-service:gamma" &&
            step_api_mul == 42 && step_api_ctor == 500;
        bool primitive_checks =
            prim_answer == 42 && prim_pi >= 3.13 && prim_pi <= 3.15 &&
            prim_greeting == "hello" && prim_flag;
        bool cpp_error_checks =
            cpp_err_is_cpp_error && cpp_err_is_error &&
            cpp_err_name == "CppError" && cpp_err_message == "boom-from-native" &&
            cpp_err_has_filename && cpp_err_has_line &&
            cpp_err_has_stack && js_err_not_cpp_error;
        bool reflection_checks =
            hidden_field_absent && renamed_field_visible && hidden_method_absent;

        if (!basic_value_checks || !naming_checks || !function_checks ||
            !namespace_checks || !explicit_set_checks || !primitive_checks ||
            !cpp_error_checks || !reflection_checks) {
            std::cerr << "QuickJS wrapper demo produced unexpected results\n";
            return 1;
        }

        std::cout << "QuickJS wrapper demo passed\n";
        std::cout << "eval/property/call/callback/object injection/JS construction/aliased class/reflected function/namespace object/two-step build+set/CppError bridge all behaved as expected\n";
        return 0;
    } catch (const qjs::exception& err) {
        std::cerr << "QuickJS wrapper error: " << err.what() << '\n';
        return 1;
    } catch (const std::exception& err) {
        std::cerr << "Unexpected error: " << err.what() << '\n';
        return 1;
    }
}
