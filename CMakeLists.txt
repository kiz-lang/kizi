cmake_minimum_required(VERSION 3.10)

project(kiz
        VERSION 0.7.23
        LANGUAGES CXX)

add_compile_options(-Ofast)

# ===================== 基础配置 =====================
# 设置C++标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 版本号定义
set(KIZ_VERSION_MAJOR 0)
set(KIZ_VERSION_MINOR 7)
set(KIZ_VERSION_PATCH 23)
set(KIZ_VERSION "${KIZ_VERSION_MAJOR}.${KIZ_VERSION_MINOR}.${KIZ_VERSION_PATCH}")

# 配置版本头文件（生成到构建目录）
# 注意：如果 version.hpp.in 实际在 include/ 下，改为 ${PROJECT_SOURCE_DIR}/include/version.hpp.in
configure_file(
        ${PROJECT_SOURCE_DIR}/src/version.hpp.in  # 按你实际路径调整
        ${CMAKE_CURRENT_BINARY_DIR}/include/version.hpp
        @ONLY
)

# ===================== 手动枚举所有 .cpp 文件（仅列cpp，无hpp） =====================
set(SRC_FILES
        # 编译器模块
        ${PROJECT_SOURCE_DIR}/src/lexer/lexer.cpp
        ${PROJECT_SOURCE_DIR}/src/lexer/read_string.cpp
        ${PROJECT_SOURCE_DIR}/src/lexer/read_number.cpp
        ${PROJECT_SOURCE_DIR}/src/lexer/lexer.cpp
        ${PROJECT_SOURCE_DIR}/src/parser/parser.cpp
        ${PROJECT_SOURCE_DIR}/src/parser/parse_expr.cpp
        ${PROJECT_SOURCE_DIR}/src/parser/parse_stmt.cpp

        # IR 生成模块
        ${PROJECT_SOURCE_DIR}/src/ir_gen/ir_gen.cpp
        ${PROJECT_SOURCE_DIR}/src/ir_gen/gen_expr.cpp
        ${PROJECT_SOURCE_DIR}/src/ir_gen/gen_stmt.cpp

        # VM 核心模块
        ${PROJECT_SOURCE_DIR}/src/vm/vm.cpp
        ${PROJECT_SOURCE_DIR}/src/vm/entry_std_modules.cpp
        ${PROJECT_SOURCE_DIR}/src/vm/entry_builtins.cpp
        ${PROJECT_SOURCE_DIR}/src/vm/handle_import.cpp
        ${PROJECT_SOURCE_DIR}/src/vm/execute_unit.cpp
        ${PROJECT_SOURCE_DIR}/src/vm/handle_call.cpp
        ${PROJECT_SOURCE_DIR}/src/vm/handle_error.cpp
        ${PROJECT_SOURCE_DIR}/src/vm/handle_make.cpp

        # 报错模块
        ${PROJECT_SOURCE_DIR}/src/error/error_reporter.cpp
)

# lib 模块（仅列cpp）
set(LIB_SRC_FILES
        ${PROJECT_SOURCE_DIR}/libs/builtins/bool_methods.cpp
        ${PROJECT_SOURCE_DIR}/libs/builtins/int_methods.cpp
        ${PROJECT_SOURCE_DIR}/libs/builtins/decimal_methods.cpp
        ${PROJECT_SOURCE_DIR}/libs/builtins/nil_methods.cpp
        ${PROJECT_SOURCE_DIR}/libs/builtins/str_methods.cpp
        ${PROJECT_SOURCE_DIR}/libs/builtins/list_methods.cpp
        ${PROJECT_SOURCE_DIR}/libs/builtins/dict_methods.cpp
        ${PROJECT_SOURCE_DIR}/libs/builtins/builtin_functions.cpp
        ${PROJECT_SOURCE_DIR}/libs/builtins/file_handle_methods.cpp
        ${PROJECT_SOURCE_DIR}/libs/builtins/builtins_lib.cpp
        ${PROJECT_SOURCE_DIR}/libs/builtins/object_methods.cpp
        ${PROJECT_SOURCE_DIR}/libs/os/os_lib.cpp
)

set(CLI_FILES
        # CLI_FILES
        ${PROJECT_SOURCE_DIR}/src/error/src_manager.cpp
        ${PROJECT_SOURCE_DIR}/src/repl/repl.cpp
        ${PROJECT_SOURCE_DIR}/src/repl/repl_readline.cpp
        ${PROJECT_SOURCE_DIR}/src/main.cpp

)

# ===================== 文件存在性检查（仅检查cpp） =====================
foreach(src_file ${SRC_FILES})
    if(NOT EXISTS ${src_file})
        message(FATAL_ERROR "[错误] 未找到src目录文件：${src_file}，请检查路径/文件名是否正确！")
    endif()
endforeach()

foreach(lib_file ${LIB_SRC_FILES})
    if(NOT EXISTS ${lib_file})
        message(FATAL_ERROR "[错误] 未找到库文件：${lib_file}，请检查路径/文件名是否正确！")
    endif()
endforeach()

option(BUILD_WASM "Build for WebAssembly" OFF)

if(BUILD_WASM)
    # 尝试自动查找Emscripten
    if(NOT EMSCRIPTEN_ROOT_PATH)
        # 检查常见位置
        if(EXISTS "$ENV{HOME}/emsdk/upstream/emscripten")
            set(EMSCRIPTEN_ROOT_PATH "$ENV{HOME}/emsdk/upstream/emscripten")
        elseif(EXISTS "$ENV{USERPROFILE}/emsdk/upstream/emscripten")
            set(EMSCRIPTEN_ROOT_PATH "$ENV{USERPROFILE}/emsdk/upstream/emscripten")
        elseif(EXISTS "C:/emsdk/upstream/emscripten")
            set(EMSCRIPTEN_ROOT_PATH "C:/emsdk/upstream/emscripten")
        else()
            # 尝试在当前项目目录查找
            if(EXISTS "${CMAKE_SOURCE_DIR}/emsdk/upstream/emscripten")
                set(EMSCRIPTEN_ROOT_PATH "${CMAKE_SOURCE_DIR}/emsdk/upstream/emscripten")
            else()
                message(FATAL_ERROR "Emscripten not found. Please set EMSCRIPTEN_ROOT_PATH or install Emscripten")
            endif()
        endif()
    endif()

    message(STATUS "Emscripten path: ${EMSCRIPTEN_ROOT_PATH}")

    # 设置Emscripten工具链
    set(CMAKE_TOOLCHAIN_FILE "${EMSCRIPTEN_ROOT_PATH}/cmake/Modules/Platform/Emscripten.cmake")

    # 添加Emscripten包含目录
    include_directories(${EMSCRIPTEN_ROOT_PATH}/system/include)

    # 对于WASM，排除CLI相关文件
    set(WASM_SRC_FILES
            ${SRC_FILES}
            ${LIB_SRC_FILES}
            ${PROJECT_SOURCE_DIR}/src/wasm_interface.cpp
    )

    # 创建WASM库
    add_library(kiz_wasm STATIC ${WASM_SRC_FILES})

    target_include_directories(kiz_wasm
            PRIVATE
            ${PROJECT_SOURCE_DIR}/src
            ${PROJECT_SOURCE_DIR}/depends
            ${PROJECT_SOURCE_DIR}/libs
            ${CMAKE_CURRENT_BINARY_DIR}/include
            ${EMSCRIPTEN_ROOT_PATH}/system/include
    )

    # 添加预处理器定义
    target_compile_definitions(kiz_wasm PRIVATE EMSCRIPTEN __EMSCRIPTEN__)

    # 创建WASM可执行文件
    add_executable(kiz_wasm_module ${PROJECT_SOURCE_DIR}/src/wasm_interface.cpp)
    target_link_libraries(kiz_wasm_module kiz_wasm)

    # Emscripten编译选项
    set_target_properties(kiz_wasm_module PROPERTIES
            LINK_FLAGS "-s WASM=1 -s MODULARIZE=1 -s EXPORT_ES6=1 -s NO_DISABLE_EXCEPTION_CATCHING -s ALLOW_MEMORY_GROWTH=1 -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','UTF8ToString','stringToUTF8'] -s EXPORTED_FUNCTIONS=['_malloc','_free','_run_code','_allocate_memory','_free_memory','_get_last_error'] -s ENVIRONMENT=web -s EXPORT_NAME='createKizModule'"
    )

    # 设置输出名称
    set_target_properties(kiz_wasm_module PROPERTIES
            OUTPUT_NAME "kiz_wasm"
            SUFFIX ".js"
    )

    message(STATUS "Building for WebAssembly")

else()
    # 原有的本地构建配置
    set(ALL_SRC_FILES ${SRC_FILES} ${LIB_SRC_FILES} ${CLI_FILES})
    add_executable(kiz ${ALL_SRC_FILES})

    target_include_directories(kiz
            PRIVATE
            ${PROJECT_SOURCE_DIR}/src
            ${PROJECT_SOURCE_DIR}/depends
            ${PROJECT_SOURCE_DIR}/libs
            ${CMAKE_CURRENT_BINARY_DIR}/include
    )

    if (APPLE)
        target_link_libraries(kiz PRIVATE
                "-framework CoreFoundation"
                "-framework CoreGraphics"
        )
    endif()

    if(CMAKE_SYSTEM_NAME MATCHES "Windows")
        set_target_properties(kiz PROPERTIES SUFFIX ".exe")
    else()
        set_target_properties(kiz PROPERTIES SUFFIX ".elf")
    endif()
endif()

# ===================== 编译信息打印 =====================
if(BUILD_WASM)
    message(STATUS "======================================")
    message(STATUS "  Kiz WASM 构建配置")
    message(STATUS "======================================")
    message(STATUS "✅ 目标：WebAssembly")
    message(STATUS "✅ 工具链：Emscripten")
    message(STATUS "✅ 输出：kiz_wasm.js + kiz_wasm.wasm")
else()
    list(LENGTH ALL_SRC_FILES TOTAL_SRC_COUNT)
    list(LENGTH SRC_FILES SRC_DIR_COUNT)
    list(LENGTH LIB_SRC_FILES LIB_FILES_COUNT)

    message(STATUS "======================================")
    message(STATUS "  项目 kiz v${PROJECT_VERSION} 编译配置")
    message(STATUS "======================================")
    message(STATUS "✅ C++标准：C++20")
    message(STATUS "✅ 源文件总数：${TOTAL_SRC_COUNT}")
    message(STATUS "  - src目录：${SRC_DIR_COUNT} 个 .cpp 文件")
    message(STATUS "  - libs：${LIB_FILES_COUNT} 个 .cpp 文件")
    message(STATUS "✅ 目标文件路径：${CMAKE_BINARY_DIR}/kiz$<TARGET_FILE_SUFFIX:kiz>")
endif()
message(STATUS "======================================")
