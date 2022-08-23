set_project("ark_proxy")
set_languages("c17","cxx17")

local vendor = {
    ["boost"]  = "/data/vendor/boost-1.80",
    ["gsl"] = os.scriptdir() .. "/vendor/gsl",
    ["quickjs"] = os.scriptdir() .. "/vendor/quickjs",
}
-- 
option("vendor-boost")
    set_default(vendor["boost"])
    set_showmenu(true)
    after_check(function(option)
        option:add("includedirs","$(vendor-boost)/include", {public = true})
        option:add("linkdirs","$(vendor-boost)/lib")
        option:add("links", "boost_json", "boost_log", "boost_filesystem", "boost_context", "boost_thread", "boost_system")
    end)
--
target("gsl")
    set_kind("headeronly")
    add_headerfiles("vendor/gsl/include/(gsl/**)")
    add_includedirs(vendor["gsl"] .. "/include", {public = true})
-- 
-- package("quickjs")
--     set_sourcedir(vendor["quickjs"])
--     add_links("quickjs")
--     on_install(function(package)
--         local old = os.cd(vendor["quickjs"])
--         if not os.exists("libquickjs.a") then
--             cprint("${green bright}(vendor) ${clear}build ${magenta bright}quickjs ${clear} ...")
--             os.execv("sed", {"s/prefix=\\/usr\\/local/prefix=\\" .. package:installdir() .."/g", "s/CFLAGS=\\-g/CFLAGS=\\-fPIC \\-g/g", "Makefile"}, {stdout = "Makefile.fpic"})
--             os.execv("make", {"-f", "Makefile.fpic"})
--             os.execv("make", {"-f", "Makefile.fpic", "install"})
--         end
--         os.cd(old)
--     end)
--     on_test(function(package)
--         assert(package:has_cfuncs("JS_NewRuntime", {includes = "quickjs.h"}))
--     end)

target("quickjs")
    set_kind("static")
    on_build(function(target)
        local old = os.cd(vendor["quickjs"])
        if not os.exists("libquickjs.a") then
            cprint("${green bright}(vendor) ${clear}build ${magenta bright}quickjs ${clear} ...")
            os.execv("sed", {"s/CFLAGS=\\-g/CFLAGS=\\-fPIC \\-g/g", "Makefile"}, {stdout = "Makefile.fpic"})
            os.execv("make", {"-f", "Makefile.fpic", "-j", "4", "libquickjs.a"})
            -- os.execv("make", {"-f", "Makefile.fpic", "install"})
        end
        os.cd(old)
    end)
    on_clean(function(target)
        local old = os.cd(vendor["quickjs"])
        if not os.exists("libquickjs.a") then
            cprint("${green bright}(vendor) ${clear}clean ${magenta bright}quickjs ${clear} ...")
            os.execv("sed", {"s/CFLAGS=\\-g/CFLAGS=\\-fPIC \\-g/g", "Makefile"}, {stdout = "Makefile.fpic"})
            os.execv("make", {"-f", "Makefile.fpic", "clean"})
        end
        os.cd(old)
    end)
    add_includedirs(vendor["quickjs"], {public = true})
    -- add_links(vendor["quickjs"] .. "/libquickjs.a")


option("trace-exception")
    set_default(true)
    add_defines("QUICKJS_DEBUG_TRACE_EXCEPTION")

target("cpp_quickjs")
    set_kind("shared")
    add_rules("mode.debug", "mode.release", "mode.releasedbg")
    add_options("vendor-boost")
    add_options("trace-exception")
    add_deps("gsl")
    add_deps("quickjs")
    add_syslinks("pthread", "dl")
    add_linkdirs(vendor["quickjs"])
    add_links("quickjs")
    -- add_defines("BOOST_ASIO_DISABLE_STD_ALIGNED_ALLOC")
    add_cxxflags("-fPIC")
    add_files("src/**.cpp")

    add_headerfiles("src/(engine/*.hpp)")

target("example")
    set_default(false)
    set_kind("binary")
    add_rules("mode.debug")
    add_options("vendor-boost")
    add_deps("cpp_quickjs")
    add_syslinks("pthread")
    add_includedirs("src")
    add_files("example/**.cpp")
