set_project("kiz")
-- 版本号单一事实源：仓库根目录的VERSION文件（用 scripts/bump_version.py 更新）
-- 描述域沙箱禁用io，只能借$(shell)读取；configvar的数值在配置期展开
set_version("$(shell cat VERSION)")
set_languages("c++20")

set_configvar("KIZ_VERSION", "$(shell cat VERSION)")
set_configvar("KIZ_VERSION_MAJOR", "$(shell cut -d. -f1 VERSION)")
set_configvar("KIZ_VERSION_MINOR", "$(shell cut -d. -f2 VERSION)")
set_configvar("KIZ_VERSION_PATCH", "$(shell cut -d. -f3 VERSION)")

target("kiz")
    set_kind("binary")

    add_files("src/main.cpp")
    add_files("src/test_runner.cpp")

    add_files("src/lexer/lexer.cpp")
    add_files("src/lexer/read_string.cpp")
    add_files("src/lexer/read_number.cpp")
    add_files("src/parser/parser.cpp")
    add_files("src/parser/parse_expr.cpp")
    add_files("src/parser/parse_stmt.cpp")

    add_files("src/ir_gen/ir_gen.cpp")
    add_files("src/ir_gen/gen_expr.cpp")
    add_files("src/ir_gen/gen_stmt.cpp")

    add_files("src/vm/vm.cpp")
    add_files("src/vm/entry_std_modules.cpp")
    add_files("src/vm/entry_builtins.cpp")
    add_files("src/vm/execute_unit.cpp")
    add_files("src/vm/handle_import.cpp")
    add_files("src/vm/handle_error.cpp")
    add_files("src/vm/handle_call.cpp")
    add_files("src/vm/handle_make.cpp")

    add_files("src/error/error_reporter.cpp")
    add_files("src/error/src_manager.cpp")
    add_files("src/repl/repl.cpp")
    add_files("src/repl/repl_readline.cpp")

    add_files("libs/builtins/bool_methods.cpp")
    add_files("libs/builtins/int_methods.cpp")
    add_files("libs/builtins/decimal_methods.cpp")
    add_files("libs/builtins/nil_methods.cpp")
    add_files("libs/builtins/str_methods.cpp")
    add_files("libs/builtins/list_methods.cpp")
    add_files("libs/builtins/dict_methods.cpp")
    add_files("libs/builtins/builtin_functions.cpp")
    add_files("libs/builtins/file_handle_methods.cpp")
    add_files("libs/builtins/builtins_lib.cpp")
    add_files("libs/builtins/object_methods.cpp")
    add_files("libs/os/os_lib.cpp")

    add_includedirs("src")
    add_includedirs("depends")
    add_includedirs("libs")

    set_optimize("fastest")

    if is_plat("macosx") then
        add_frameworks("CoreFoundation", "CoreGraphics")
    end
    if is_plat("windows") then
        add_syslinks("user32")
    end
