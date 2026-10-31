/**
 * @file wasm_interface.cpp
 * @brief WASM接口文件 - 简化版本
 *
 * 提供JavaScript可以调用的C接口
 */

#include "ir_gen/ir_gen.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "error/src_manager.hpp"
#include "vm/vm.hpp"

#include <string>
#include <cstring>
#include <cstdio>

extern "C" {
    // 上一次run_code的错误信息（JS侧通过_get_last_error读取后须立即拷贝）
    static std::string last_error_text;

    // 获取上一次run_code的错误信息；无错误返回空串
    const char* get_last_error() {
        return last_error_text.c_str();
    }

    // 清理运行残留状态：销毁调用栈中多余的帧并释放操作数栈引用
    static void cleanup_after_run() {
        while (kiz::Vm::call_stack.size() > 1) {
            auto frame = kiz::Vm::call_stack.back();
            kiz::Vm::call_stack.pop_back();
            kiz::Vm::unwind_op_stack_to(frame->bp);
            kiz::Vm::destroy_frame(frame);
        }
        if (!kiz::Vm::call_stack.empty()) {
            auto main_frame = kiz::Vm::call_stack.back();
            kiz::Vm::unwind_op_stack_to(main_frame->bp + main_frame->code_object->locals_count);
        }
    }

    // 运行代码并返回结果
    // 单参数接口：按NUL截断取代码（页面JS侧只传code，UTF-8由cstring完整携带）
    const char* run_code(const char* code) {
        static std::string result;
        static std::string current_path = "<playground#>";

        last_error_text.clear();
        // RAII：无论正常返回还是异常穿出，都保证VM状态清理
        struct CleanupGuard {
            ~CleanupGuard() { cleanup_after_run(); }
        } cleanup_guard;
        try {
            std::string content(code);
            // Vm构造必须先于IR生成：entry_builtins在构造时注册内置函数表，
            // 顺序颠倒会让Identifier解析时builtin_names为空（print等全部NameError）
            kiz::Vm vm(current_path);
            kiz::Lexer lexer(current_path);
            kiz::Parser parser(current_path);
            kiz::IRGenerator ir_gen(current_path);

            lexer.prepare(content, 1);
            const auto tokens = lexer.tokenize();
            auto ast = parser.parse(tokens);
            const auto ir = ir_gen.gen(std::move(ast));
            const auto module = kiz::IRGenerator::gen_mod(current_path, ir);

            kiz::Vm::set_main_module(module);
            kiz::Vm::exec_curr_code();

            // wasm下musl的stdout是全缓冲：C++层的flush只把数据送进musl缓冲，
            // 必须fflush C层stdout，print输出才会经fd_write到达JS的print回调
            std::fflush(stdout);

            result = "Success";
        } catch (const KizStopRunningSignal& e) {
            result = std::string("A Panic! : ") + e.what();
            last_error_text = e.what();
        } catch (...) {
            result = "Unknown error";
            last_error_text = "Unknown error";
        }

        return result.c_str();
    }
}