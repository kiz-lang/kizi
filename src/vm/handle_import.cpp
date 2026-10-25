/**
 * @file handle_import.cpp
 * @brief import模块的搜索/解析/执行/缓存
 * @author azhz1107cat
 * @date 2026-10-25
 */
#include <cassert>
#include <fstream>

#include "../kiz.hpp"
#include "../models/models.hpp"
#include "vm.hpp"
#include "builtins/include/builtin_functions.hpp"
#include "ir_gen/ir_gen.hpp"
#include "lexer/lexer.hpp"
#include "opcode/opcode.hpp"
#include "parser/parser.hpp"
#include "error/src_manager.hpp"

#include <filesystem>
#include <string>
#include <stdexcept>
#include <cstddef>
#include <array>
#include <format>
#include <unordered_set>

// 跨平台兼容：处理Windows/Linux/macOS的编译差异
#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#include <limits.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <cstdint>
#endif

namespace fs = std::filesystem;

/**
 * @brief 跨平台获取EXE可执行文件的**完整绝对路径**（含exe文件名）
 * @return fs::path EXE的绝对路径（如Windows: C:/project/bin/Debug/app.exe，Linux: /home/user/bin/app）
 */
fs::path get_exe_abs_path() {
    fs::path exe_path;
#ifdef _WIN32
    // Windows平台：使用GetModuleFileNameW获取宽字符路径（避免中文路径乱码）
    wchar_t buf[MAX_PATH] = {0};
    DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    // 处理缓冲区不足的情况，动态扩容
    if (len == MAX_PATH) {
        do {
            std::wstring big_buf(len + MAX_PATH, L'\0');
            len = GetModuleFileNameW(nullptr, big_buf.data(), static_cast<DWORD>(big_buf.size()));
            if (len < big_buf.size()) {
                exe_path = big_buf.substr(0, len);
                break;
            }
        } while (true);
    } else if (len > 0) {
        exe_path = std::wstring(buf, len);
    } else {
        throw NativeFuncError("PathError","Windows GetModuleFileNameW failed, error code: " + std::to_string(GetLastError()));
    }
#elif defined(__linux__)
    // Linux平台：读取/proc/self/exe符号链接（指向当前进程的可执行文件）
    char buf[PATH_MAX] = {0};
    ssize_t len = readlink("/proc/self/exe", buf, PATH_MAX - 1);
    if (len == -1) {
        throw NativeFuncError("PathError", "Linux readlink /proc/self/exe failed");
    }
    exe_path = std::string(buf, len);
#elif defined(__APPLE__)
    // macOS平台：使用_NSGetExecutablePath获取可执行文件路径
    char buf[PATH_MAX] = {0};
    uint32_t buf_len = PATH_MAX;
    int ret = _NSGetExecutablePath(buf, &buf_len);
    // 缓冲区不足时扩容
    if (ret == -1) {
        std::string big_buf(buf_len, '\0');
        ret = _NSGetExecutablePath(big_buf.data(), &buf_len);
        if (ret != 0) {
            throw NativeFuncError("PathError", "macOS _NSGetExecutablePath failed");
        }
        exe_path = big_buf;
    } else {
        exe_path = std::string(buf);
    }
    // macOS需将相对路径转换为绝对路径
    exe_path = fs::absolute(exe_path);
#else
    throw KizStopRunningSignal("Unsupported platform");
#endif

    // 确保返回绝对路径（跨平台兜底）
    return fs::absolute(exe_path);
}

/**
 * @brief 跨平台获取EXE可执行文件的绝对路径所在目录
*/
fs::path kiz::Vm::get_exe_abs_dir() {
    return get_exe_abs_path().parent_path();
}

/**
 * @brief 路径安全拼接+规范化（自动解析.././、处理分隔符、跨平台）
 * @param base_path 基础路径（如EXE目录、任意绝对/相对路径）
 * @param path_fragments 待拼接的路径片段（支持多个，如"../conf"、"app.json"、"./data/log"）
 * @return fs::path 规范化后的绝对/相对路径（解析了.././，无冗余分隔符）
 * @note 若base_path是绝对路径，返回结果也为绝对路径；反之则为相对路径
 */
template <typename... Args>
fs::path path_combine(const fs::path& base_path, Args&&... path_fragments) {
    fs::path result = base_path;
    (result /= ... /= std::forward<Args>(path_fragments));
    return result.lexically_normal();
}

// ========== 新增：核心函数 get_file_name_by_path ==========
/**
 * @brief 从路径字符串中提取去掉「最后一个扩展名」的纯文件名
 * @param file_path 任意路径（绝对/相对，如"k/l/m.log"、"hhh/k.m.log"、"test.txt"）
 * @return std::string 纯文件名（无路径、无最后一个扩展名）
 * @note 边界情况处理：
 *       1. 无扩展名："a/b/c" → "c"；"test" → "test"
 *       2. 隐藏文件：".gitignore" → ".gitignore"（无最后一个扩展名，保留完整）
 *       3. 多分隔符/末尾分隔符："a//b/c.log/" → ""；"/" → ""
 *       4. 纯扩展名："a/b/.log" → ""
 */
std::string get_file_name_by_path(const std::string& file_path) {
    fs::path path_obj(file_path);
    // 先取文件名（带扩展名），再移除最后一个扩展名，最终转字符串
    return path_obj.filename().stem().string();
}

namespace kiz {

// 正在导入中的模块集合（规范化绝对路径），用于检测循环导入
static std::unordered_set<std::string> importing_modules;

// RAII守卫：无论模块执行成功还是抛异常，都从"导入中"集合移除
struct ImportingGuard {
    std::string key;
    explicit ImportingGuard(std::string k) : key(std::move(k)) {}
    ~ImportingGuard() { importing_modules.erase(key); }
};

void Vm::handle_import(const std::string& module_path) {
    std::string content;

    // 路径安全限制：禁止绝对路径与..上跳，防止脚本借import探测/读取任意文件
    {
        fs::path raw(module_path);
        if (raw.is_absolute()) {
            throw NativeFuncError("PathError", "Absolute import path is not allowed: '" + module_path + "'");
        }
        for (const auto& part : raw) {
            if (part == "..") {
                throw NativeFuncError("PathError", "Import path must not contain '..': '" + module_path + "'");
            }
        }
    }

    // 标准库模块优先解析：避免本地同名.kiz文件静默遮蔽os/builtins等标准模块
    if (std_modules.find(module_path)) {
        if (auto loaded_mod_it = modules_cache.find(module_path)) {
            push_to_stack(loaded_mod_it->value);
            return;
        }
        auto std_init_it = std_modules.find(module_path);
        auto std_init_func = dynamic_cast<model::NativeFunction*>(std_init_it->value);
        assert(std_init_func != nullptr);

        model::List* args_list = new model::List({});
        args_list->make_ref();
        model::Object* return_val = std_init_func->func(std_init_func, args_list);
        // 使用后释放临时参数列表
        args_list->del_ref();

        assert(return_val != nullptr);

        auto module_obj = dynamic_cast<model::Module*>(return_val);
        assert(module_obj != nullptr);

        push_to_stack(module_obj);

        module_obj->make_ref();
        modules_cache.insert(module_path, module_obj);
        return;
    }

    fs::path current_file_path = get_current_file_path();

    bool file_in_path = false;
    fs::path actually_found_path = "";
    std::vector for_search_paths = {
        fs::absolute(current_file_path.parent_path() / module_path),
        get_exe_abs_dir() / fs::path(module_path),
        get_exe_abs_dir().parent_path() / fs::path("modules") / fs::path(module_path) / fs::path("__main__.kiz"),
        get_exe_abs_dir().parent_path() / fs::path("modules") / fs::path(module_path)
    };

#ifdef __EMSCRIPTEN__
#else
    for (const auto& for_search_path : for_search_paths) {
        if (fs::is_regular_file( for_search_path )) {
            file_in_path = true;
            actually_found_path = for_search_path;
            break;
        }
    }
#endif

    if (!file_in_path) {
        throw NativeFuncError("PathError", std::format(
            "Failed to find module in path '{}', tried '{}', '{}', '{}', '{}'", module_path,
            for_search_paths[0].string(), for_search_paths[1].string(),
            for_search_paths[2].string(), for_search_paths[3].string()
        ));
    }

    // 以规范化绝对路径作为缓存键：同一文件的不同写法（a.kiz / ./a.kiz）共享同一模块
    std::string cache_key = fs::absolute(actually_found_path).lexically_normal().string();

    if (auto loaded_mod_it = modules_cache.find(cache_key)) {
        push_to_stack(loaded_mod_it->value);
        return;
    }

    // 循环导入检测：模块执行期间再次导入自身则报错，而不是无限递归崩溃
    if (!importing_modules.insert(cache_key).second) {
        throw NativeFuncError("ImportError", "Circular import detected: '" + module_path + "'");
    }
    ImportingGuard importing_guard(cache_key);

#ifdef __EMSCRIPTEN__
    // 不可能走到这
    assert(false);
#else
    content = err::SrcManager::get_file_by_path(actually_found_path.string());
#endif

    // 词法/语法/IR的错误报告键与SrcManager缓存键统一用实际命中的绝对路径，
    // 否则CWD≠脚本目录时报错上下文查不到缓存，原始错误被"打不开文件"吞掉
    const std::string found_path_str = actually_found_path.string();
    Lexer lexer(found_path_str);
    Parser parser(found_path_str);
    IRGenerator ir_gen(found_path_str);

    lexer.prepare(content);
    const auto tokens = lexer.tokenize();
    auto ast = parser.parse(tokens);
    const auto ir = ir_gen.gen(std::move(ast));
    auto module_obj = IRGenerator::gen_mod(module_path, ir);
    // 模块路径记录实际绝对路径：嵌套import的相对路径解析以此为基准才与CWD无关
    module_obj->path = found_path_str;

    module_obj->make_ref();  // 先被handle_import这个函数持有

    module_obj->make_ref();
    module_obj->code->make_ref();
    auto new_frame = new CallFrame{
        .name = module_path,

        .owner = module_obj,

        .pc = 0,
        .return_to_pc = module_obj->code->code.size() + 1,
        .last_bp = call_stack.back()->bp,
        .bp = op_stack.size(),
        .code_object = module_obj->code,

        .iters{},

        .curr_error = nullptr
    };

    size_t old_call_stack_size = call_stack.size();

    call_stack.emplace_back(new_frame);

    // 为模块帧的局部变量区扩容：缺了这一步，模块顶层SET_LOCAL会越界写操作数栈
    op_stack.resize(op_stack.size() + module_obj->code->locals_count);


    try {
        /// 执行新代码
        while (running and !call_stack.empty()) {
            auto curr_frame = call_stack.back();
            auto frame_code = curr_frame->code_object;

            // 检查是否执行到模块代码末尾：执行完毕则按RET语义清理并出栈
            if (curr_frame->pc >= frame_code->code.size()) {
                if (old_call_stack_size == call_stack.size() - 1) {
                    break;
                }
                call_stack.pop_back();
                unwind_op_stack_to(curr_frame->bp);
                destroy_frame(curr_frame);
                continue;
            }

            // 执行当前指令
            const Instruction& curr_inst = frame_code->code[curr_frame->pc];
            try {
                execute_unit(curr_inst); // 调用VM的指令执行核心方法
            } catch (const NativeFuncError& e) {
                // 原生函数执行错误，抛出异常
                forward_to_handle_throw(e.name, e.msg);
                if (call_stack.empty() or call_stack.back() != curr_frame) {
                    // 处理点在更外层：穿透本函数的清理逻辑回到主执行循环
                    throw KizUnwindSignal();
                }
                continue;
            }

            // 本帧被异常解栈销毁：帧数跌破基线，处理点在别的执行循环
            if (call_stack.size() <= old_call_stack_size) {
                throw KizUnwindSignal();
            }

            ADVANCE_PC

        }
    } catch (...) {
        // 模块内出现未捕获异常：清理本函数压入的帧与模块引用后继续上抛
        while (call_stack.size() > old_call_stack_size) {
            auto frame_to_clean = call_stack.back();
            call_stack.pop_back();
            unwind_op_stack_to(frame_to_clean->bp);
            destroy_frame(frame_to_clean);
        }
        module_obj->del_ref(); // 释放本函数对模块的持有
        throw;
    }

    for (size_t i = call_stack.back()->bp; i < call_stack.back()->bp + call_stack.back()->code_object->locals_count; ++i) {
        const auto local_object = op_stack[i];
        const auto name = call_stack.back()->code_object->var_names[i - call_stack.back()->bp];
        if (name.starts_with("__private__")) continue;
        if (!local_object) continue; // 未被赋值过的局部槽位不导出

        module_obj->attrs_insert(name, local_object);
    }


    /// 处理delete frame
    auto frame = call_stack.back();
    call_stack.pop_back();
    call_stack.back()->bp = frame->last_bp;

    unwind_op_stack_to(frame->bp);
    destroy_frame(frame);

    /// 储存module（以规范化路径为缓存键）
    push_to_stack(module_obj);

    module_obj->make_ref();
    modules_cache.insert(cache_key, module_obj);

    /// 抵消上一次的幽灵持有
    module_obj->del_ref();
}

}
