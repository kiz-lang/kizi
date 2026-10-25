/**
 * @file handle_error.cpp
 * @brief 异常处理（错误转发与调用栈解退）
 * @author azhz1107cat
 * @date 2026-10-25
 */
#include <ranges>

#include "vm.hpp"
#include "builtins/include/builtin_functions.hpp"
#include "opcode/opcode.hpp"

namespace kiz {

// -------------------------- 异常处理 --------------------------

void Vm::forward_to_handle_throw(const std::string& name, const std::string& content) {
    const auto err_name = new model::String(name);
    const auto err_msg = new model::String(content);
    const auto err_obj = new model::Error(make_pos_info());

    err_obj->attrs_insert("__name__", err_name);
    err_obj->attrs_insert("__msg__", err_msg);

    // 替换全局curr_error前，释放旧错误对象
    if (call_stack.back()->curr_error) {
        call_stack.back()->curr_error->del_ref();
    }
    err_obj->make_ref();
    call_stack.back()->curr_error = err_obj;
    handle_throw();
}

void Vm::handle_throw() {
    assert(call_stack.back()->curr_error);

    // 提取错误对象的 __name__ 和 __msg__
    auto err = call_stack.back()->curr_error;
    err->make_ref(); // 借用持有：函数任意路径退出前必须del_ref一次
    auto err_name_it = err->attrs.find("__name__");
    auto err_msg_it = err->attrs.find("__msg__");

    if (!err_name_it or !err_msg_it) {
        err->del_ref();
        throw KizStopRunningSignal(
            "Undefined attribute '__name__' '__msg__' of " + obj_to_debug_str(err) + " (when try to throw it)");
    }
    auto error_name = obj_to_str(err_name_it->value);
    auto error_msg = obj_to_str(err_msg_it->value);

    // 逆序遍历调用栈，寻找第一个能处理该异常的try块
    CallFrame* handler_frame = nullptr;
    size_t handler_pc = 0;
    size_t frames_to_pop = 0; // 需要从栈顶弹出销毁的帧数
    for (auto frame : std::ranges::reverse_view(call_stack)) {
        for (const auto& table : frame->code_object->exception_tables) {
            // 检查当前 pc 是否在此 try 块的范围内
            size_t current_pc = frame->pc;
            if (table.try_part_start_pc <= current_pc and current_pc < table.try_part_end_pc) {
                // 寻找匹配的 catch 块
                if (auto catch_start_pc_it = table.handle_pc.find(error_name)) {
                    handler_pc = catch_start_pc_it->value;
                } else {
                    handler_pc = table.mismatch_pc;
                }
                handler_frame = frame;
                break;
            }
        }
        if (handler_frame) break;
        ++frames_to_pop;
    }

    // 没有找到任何能处理该异常的 try 块：打印错误信息并终止执行
    if (!handler_frame) {
        if (const auto err_obj = dynamic_cast<model::Error*>(err)) {
            std::cout << Color::BRIGHT_RED << "\nTrace Back: " << Color::RESET << std::endl;
            for (auto& [_path, _pos] : err_obj->positions) {
                err::context_printer(_path, _pos);
            }
        }

        std::cout << Color::BOLD <<
            Color::BRIGHT_RED << error_name << Color::RESET
            << Color::WHITE << " : " << error_msg << Color::RESET << "\n";

        std::cout << std::endl;

        err->del_ref();
        // 释放抛出点帧上错误对象的引用，交给外层(REPL等)做帧清理
        if (call_stack.back()->curr_error) {
            call_stack.back()->curr_error->del_ref();
        }
        call_stack.back()->curr_error = nullptr;
        throw KizStopRunningSignal();
    }

    // 回退操作数栈：丢弃异常路径上各帧的临时值，保留handler帧的局部变量区
    unwind_op_stack_to(handler_frame->bp + handler_frame->code_object->locals_count);

    // 从栈顶逐个销毁被展开的帧（操作数栈已截到handler帧局部区，帧内引用在此统一释放）
    for (size_t i = 0; i < frames_to_pop; ++i) {
        auto frame = call_stack.back();
        call_stack.pop_back();
        destroy_frame(frame);
    }
    assert(call_stack.back() == handler_frame);

    // handler帧持有错误对象引用；释放本函数开头的借用引用
    if (handler_frame->curr_error) {
        handler_frame->curr_error->del_ref();
    }
    err->make_ref();
    handler_frame->curr_error = err;
    err->del_ref();
    handler_frame->pc = handler_pc;
}

}
