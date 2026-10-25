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

extern "C" {
    // 上一次run_code的错误信息（JS侧通过_get_last_error读取后须立即拷贝）
    static std::string last_error_text;

    // 获取上一次run_code的错误信息；无错误返回空串
    const char* _get_last_error() {
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
    const char* run_code(const char* code, int length) {
        static std::string result;
        static std::string current_path = "<playground#>";

        last_error_text.clear();
        try {
            std::string content(code, length);
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

            cleanup_after_run(); // 清理残留帧与栈引用，保证下次run_code从干净状态开始

            result = "Success";
        } catch (const KizStopRunningSignal& e) {
            cleanup_after_run();
            result = std::string("A Panic! : ") + e.what();
            last_error_text = e.what();
        } catch (...) {
            cleanup_after_run();
            result = "Unknown error";
            last_error_text = "Unknown error";
        }

        return result.c_str();
    }
}