/**
 * @file repl.cpp
 * @brief 交互式终端（Read Evaluate Print Loop，简称 REPL）核心实现
 *
 * @author azhz1107cat
 * @date 2025-10-25
 */

#include "repl.hpp"

#include "../lexer/lexer.hpp"
#include "../vm/vm.hpp"
#include "../ir_gen/ir_gen.hpp"
#include "../parser/parser.hpp"
#include "../kiz.hpp"
#include "color.hpp"
#include "../error/src_manager.hpp"

namespace ui {

const std::string Repl::file_path = "<shell#>";

Repl::Repl(): is_running_(true), vm_(file_path) {
    std::cout << "This is the kiz REPL " << "v" << KIZ_VERSION << "\n" << std::endl;
}

Repl::~Repl() = default;

std::string Repl::read(const std::string& prompt) {
    std::cout << Color::BRIGHT_MAGENTA << prompt << Color::RESET;
    std::cout.flush();
    std::string result = get_whole_input(&std::cin, &std::cout);
    return result;
}

void Repl::loop() {
    DEBUG_OUTPUT("start repl loop");
    while (is_running_) {
        try {
            auto code = read(">>> ");
            if (code.empty()) continue;
            auto old_code_iterator = err::SrcManager::opened_files.find(file_path);
            if (old_code_iterator != err::SrcManager::opened_files.end()) {
                err::SrcManager::opened_files[file_path] = old_code_iterator->second + "\n" + code;
            } else {
                err::SrcManager::opened_files[file_path] = code;
            }

            size_t old_size = cmd_history_.size();
            for (const auto& line: err::SrcManager::splitlines(code)) {
                cmd_history_.push_back(line);
            }
            eval_and_print(code, old_size + 1);
        } catch (KizStopRunningSignal& e) {
            // 恢复VM状态：销毁残留的函数帧并截断操作数栈，保证下一行输入可继续执行
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
            if (std::string(e.what()).empty()) {
                continue;
            }
            std::cout << Color::BOLD <<
            Color::BRIGHT_RED << "A Panic!" << Color::RESET
            << Color::WHITE << " : " << e.what() << Color::RESET << "\n";
        }
    }
}

void Repl::eval_and_print(const std::string& cmd, const size_t startline) {
    DEBUG_OUTPUT("repl eval_and_print...");
    bool should_print = false;

    // init
    kiz::Parser parser(file_path);
    kiz::IRGenerator ir_gen(file_path);

    kiz::Lexer lexer(file_path);
    lexer.prepare(cmd, startline);
    const auto tokens = lexer.tokenize();

    auto ast = parser.parse(tokens);
    if (!ast->statements.empty() and
        dynamic_cast<kiz::ExprStmt*>(ast->statements.back().get())
    ) {  should_print = true; }

    const auto ir = ir_gen.gen(std::move(ast), last_global_var_names_);
    last_global_var_names_ = ir_gen.get_global_var_names();

    if (kiz::Vm::call_stack.empty()) {
        const auto module = kiz::IRGenerator::gen_mod(file_path, ir);
        kiz::Vm::set_main_module(module);
        kiz::Vm::exec_curr_code();
    } else {
        if (!ir) throw KizStopRunningSignal("No ir for run" );
        kiz::Vm::reset_global_code(ir);
        kiz::Vm::exec_curr_code();
    }

    DEBUG_OUTPUT("repl print");
    if (kiz::Vm::op_stack.empty()) {
        return;
    }
    auto stack_top = kiz::Vm::op_stack.back();
    if (stack_top) {
        if (!dynamic_cast<model::Nil*>(stack_top) and should_print) {
            std::cout << kiz::Vm::obj_to_debug_str(stack_top) << std::endl;
        }
    }
}

} // namespace ui