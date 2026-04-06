set_project("kiz")
set_version("0.7.23")
set_languages("c++20")

rule("version")
    set_extensions(".in")
    on_build_file(function (target, source_file, opt)
        -- 读取文件内容
        local content = io.readfile(source_file)
        if content then
            -- 进行文本替换，例如替换版本号
            content = content:gsub("@KIZ_VERSION_MAJOR@", "0")
            content = content:gsub("@KIZ_VERSION_MINOR@", "7")
            content = content:gsub("@KIZ_VERSION_PATCH@", "11")
            content = content:gsub("@KIZ_VERSION@", "0.7.11")
            -- 将新内容写回文件（或写入新位置）
            local target_file = path.join("cmake-build-debug/include", string.sub(path.filename(source_file), 1, #path.filename(source_file) - 3)) -- 去掉 .in 后缀
            io.writefile(target_file, content)
        end
    end)


target("kiz")
    set_kind("binary")

    add_files("src/version.hpp.in", {rule = "version"}) -- 添加版本文件，并应用版本规则

    -- 入口文件
    add_files("src/main.cpp")

    -- 编译器模块
    add_files("src/lexer/lexer.cpp")
    add_files("src/lexer/read_string.cpp")
    add_files("src/lexer/read_number.cpp")
    add_files("src/parser/parser.cpp")
    add_files("src/parser/parse_expr.cpp")
    add_files("src/parser/parse_stmt.cpp")

    -- IR 生成模块
    add_files("src/ir_gen/ir_gen.cpp")
    add_files("src/ir_gen/gen_expr.cpp")
    add_files("src/ir_gen/gen_stmt.cpp")

    -- VM 核心模块
    add_files("src/vm/vm.cpp")
    add_files("src/vm/entry_std_modules.cpp")
    add_files("src/vm/entry_builtins.cpp")

    add_files("src/vm/execute_unit.cpp")
    add_files("src/vm/handle_import.cpp")
    add_files("src/vm/handle_error.cpp")
    add_files("src/vm/handle_call.cpp")
    add_files("src/vm/handle_make.cpp")

    -- 工具模块
    add_files("src/error/error_reporter.cpp")
    add_files("src/error/src_manager.cpp")
    add_files("src/repl/repl.cpp")
    add_files("src/repl/repl_readline.cpp")

    -- lib 模块
    add_files("libs/builtins/bool_methods.cpp")
    add_files("libs/builtins/int_methods.cpp")
    add_files("libs/builtins/decimal_methods.cpp")
    add_files("libs/builtins/nil_methods.cpp")
    add_files("libs/builtins/str_methods.cpp")
    add_files("libs/builtins/list_methods.cpp")
    add_files("libs/builtins/dict_methods.cpp")
    add_files("libs/builtins/builtin_functions.cpp")
    add_files("libs/os/os_lib.cpp")
    add_files("libs/builtins/builtins_lib.cpp")
    add_files("libs/builtins/file_handle_methods.cpp")
    add_files("libs/builtins/object_methods.cpp")

    -- 设置头文件搜索路径
    add_includedirs("src")
    add_includedirs("depends")
    add_includedirs("libs")
    add_includedirs("cmake-build-debug/include") -- 生成的version.hpp

    -- 设置编译选项
    set_optimize("fastest")
    if is_os("windows") then
        add_syslinks("user32")
    end
