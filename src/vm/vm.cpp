/**
 * @file vm.cpp
 * @brief 虚拟机（VM）核心实现
 * 用于运行IR
 * @author azhz1107cat
 * @date 2025-10-25
 */

#include "vm.hpp"

#include "../models/models.hpp"
#include "../opcode/opcode.hpp"

#include <algorithm>
#include <cassert>
#include <format>

#include "../kiz.hpp"

#include "../libs/builtins/include/builtin_methods.hpp"
#include "../libs/builtins/include/builtin_functions.hpp"

namespace kiz {

std::vector<model::Object*> Vm::builtins {};
std::vector<std::string> Vm::builtin_names {};
dep::HashMap<model::Module*> Vm::modules_cache {};
model::Module* Vm::main_module;
std::vector<model::Object*> Vm::op_stack {};
std::vector<CallFrame*> Vm::call_stack {};
model::Int* Vm::small_int_pool[201] {};
bool Vm::running = false;
std::string Vm::main_file_path;
thread_local bool Vm::native_return_owns = false;
std::vector<model::Object*> Vm::const_pool {};
dep::HashMap<model::Object*> Vm::std_modules {};

StackRef::~StackRef() { if (obj) obj->del_ref(); }

Vm::Vm(const std::string& file_path_) {
    main_file_path = file_path_;
    DEBUG_OUTPUT("entry builtin functions...");
    model::unique_nil->mark_as_important();
    model::unique_false->mark_as_important();
    model::unique_true->mark_as_important();

    model::based_based_obj->mark_as_important();
    model::based_obj->mark_as_important();
    model::based_list->mark_as_important();
    model::based_function->mark_as_important();
    model::based_dict->mark_as_important();
    model::based_int->mark_as_important();
    model::based_bool->mark_as_important();
    model::based_str->mark_as_important();
    model::based_native_function->mark_as_important();
    model::based_error->mark_as_important();
    model::based_decimal->mark_as_important();
    model::based_module->mark_as_important();
    model::stop_iter_signal->mark_as_important();
    model::based_file_handle->mark_as_important();
    model::based_code_object->mark_as_important();
    model::based_unpack->mark_as_important();
    model::based_range->mark_as_important();

    for (dep::BigInt i = 0; i < 201; i+= 1) {
        auto int_obj = new model::Int{i};
        int_obj->mark_as_important();
        small_int_pool[i.to_unsigned_long_long()] = int_obj;
    }
    entry_builtins();
    entry_std_modules();
}

void Vm::set_main_module(model::Module* src_module) {
    // 合法性校验
    assert(src_module != nullptr);
    assert(src_module->code != nullptr);

    // 注册为main module
    main_module = src_module;
    src_module->make_ref();

    // 创建模块级调用帧（CallFrame）：模块是顶层执行单元，对应一个顶层调用帧
    op_stack.resize(src_module->code->locals_count);
    src_module->make_ref(); // owner
    src_module->code->make_ref();
    auto module_call_frame = new CallFrame{
        .name = src_module->path,

        .owner = src_module,

        .pc = 0,
        .return_to_pc = src_module->code->code.size(),
        .last_bp = 0,
        .bp = 0,
        .code_object = src_module->code,

        .iters{},

        .curr_error = nullptr
    };

    // 将调用帧压入VM的调用栈
    call_stack.emplace_back(module_call_frame);

    // 初始化VM执行状态：标记为"就绪"
    running = true; // 标记VM为运行状态（等待exec触发执行）
}

void Vm::exec_curr_code() {
    // 循环执行当前调用帧下的所有指令
    while (!call_stack.empty() && running) {
        auto curr_frame = call_stack.back();
        // 检查当前帧是否执行完毕
        if (curr_frame->pc >= curr_frame->code_object->code.size()) {
            // 非模块帧则按RET语义清理并弹出，模块帧则退出循环
            if (call_stack.size() == 1) {
                break;
            }
            call_stack.pop_back();
            unwind_op_stack_to(curr_frame->bp);
            destroy_frame(curr_frame);
            continue;
        }

        // 执行当前指令
        const Instruction& curr_inst = curr_frame->code_object->code[curr_frame->pc];
        try {
            // std::cout << "current instr: " << opcode_to_string(curr_inst.opc) << std::endl;
            // std::cout << "Stack:" << std::endl;
            // size_t j = 0;
            // for (auto st_mem: op_stack) {
            //     if (st_mem)
            //         std::cout << j << ": " << st_mem->debug_string() << std::endl;
            //     else
            //         std::cout << j << ": " << "<nullptr>" << std::endl;
            //     ++j;
            // }
            // std::cout << "==End==" << std::endl;
            execute_unit(curr_inst);
            // std::cout << "finish handle instr: " << opcode_to_string(curr_inst.opc) << std::endl;
            // std::cout << "Stack:" << std::endl;
            // j = 0;
            // for (auto st_mem: op_stack) {
            //     if (st_mem)
            //         std::cout << j << ": " << st_mem->debug_string() << std::endl;
            //     else
            //         std::cout << j << ": " << "<nullptr>" << std::endl;
            //     ++j;
            // }
            // std::cout << "==End==" << std::endl;
        } catch (NativeFuncError& e) {
            forward_to_handle_throw(e.name, e.msg);
            continue;
        } catch (KizUnwindSignal&) {
            // 异常已重定向到本循环管理的帧的catch块，pc已就位，直接继续执行
            continue;
        } catch (const KizStopRunningSignal&) {
            // 未捕获的kiz错误：traceback已由handle_throw打印，就地终止执行
            // （不再向宿主层抛C++异常——wasm/REPL等宿主的清理与状态均保持正常）
            running = false;
            return;
        }
        ADVANCE_PC
    }
}

void Vm::destroy_frame(CallFrame* frame) {
    // 调用前必须保证操作数栈已截断到frame->bp以下
    if (frame->curr_error) frame->curr_error->del_ref();
    frame->owner->del_ref();
    frame->code_object->del_ref();
    for (auto it : frame->iters) {
        if (it) it->del_ref();
    }
    delete frame;
}

void Vm::unwind_op_stack_to(size_t target_size) {
    // 局部槽位可能为nullptr，需判空后释放
    while (op_stack.size() > target_size) {
        if (op_stack.back()) op_stack.back()->del_ref();
        op_stack.pop_back();
    }
}

CallFrame* Vm::get_frame() {
    if ( !call_stack.empty() ) {
        return call_stack.back();
    }
    throw KizStopRunningSignal("Unable to fetch current frame");
}

StackRef Vm::get_and_pop_stack_top() {
    if(op_stack.empty()) throw KizStopRunningSignal("Unable to fetch top of stack");
    auto stack_top = op_stack.back();
    if (!stack_top) throw KizStopRunningSignal("Top of stack is free");
    op_stack.pop_back();
    return StackRef(stack_top);
}

model::Object* Vm::simple_get_and_pop_stack_top() {
    if(op_stack.empty()) throw KizStopRunningSignal("Unable to fetch top of stack");
    auto stack_top = op_stack.back();
    if (!stack_top) throw KizStopRunningSignal("Top of stack is free");
    op_stack.pop_back();
    return stack_top;
}


void Vm::push_to_stack(model::Object* obj) {
    if (obj == nullptr) return;
    obj->make_ref(); // 栈成为新持有者，增加引用计数
    op_stack.push_back(obj);
}

void Vm::reset_global_code(model::CodeObject* code_object) {
    assert(code_object != nullptr);
    assert(!call_stack.empty());
    assert(call_stack.size() == 1);

    // 获取全局模块级调用帧（REPL 共享同一个帧）
    auto frame = call_stack.back();
    // 对原有CodeObject调用del_ref()，释放CallFrame的持有权
    if (frame->code_object) {
        frame->code_object->del_ref();
    }

    code_object->make_ref();
    frame->code_object = code_object;
    frame->pc = 0;

    // 同步操作数栈局部区大小：截断旧值(释放引用)或补空槽，避免新代码局部下标越界
    if (op_stack.size() > code_object->locals_count) {
        unwind_op_stack_to(code_object->locals_count);
    } else {
        op_stack.resize(code_object->locals_count, nullptr);
    }
    exec_curr_code();
}

std::string Vm::get_attr_name_by_idx(const size_t idx) {
    auto frame = get_frame();
    return frame->code_object->attr_names[idx];
}


void Vm::assert_argc(size_t argc, const model::List* args) {
    if (argc == args->val.size()) {
        return;
    }
    throw NativeFuncError("ArgCountError", std::format(
        "expect {} arguments but got {} arguments", argc, args->val.size()
    ));
}

void Vm::assert_argc(const std::vector<size_t>& argcs, const model::List* args) {
    auto actually_count = args->val.size();
    for (size_t i : argcs) {
        if (i == actually_count) {
            return;
        }
    }

    std::string argc_str;
    size_t i = 0;
    for (size_t argc : argcs) {
        argc_str += std::to_string(argc);
        if (i != argcs.size()-1) {
            argc_str += " or ";
        }
        ++ i;
    }

    throw NativeFuncError("ArgCountError", std::format(
        "expect {} arguments but got {} arguments", argc_str, actually_count
    ));
}

std::filesystem::path Vm::get_current_file_path() {
    std::filesystem::path current_file_path = "";
    if (main_file_path == "<shell#>") return current_file_path;
    // 取调用栈中最顶部（最近执行）的模块帧，嵌套import时相对路径以当前模块目录为基准
    for (const auto& frame: std::ranges::reverse_view(call_stack)) {
        if (frame->owner->get_type() == model::Object::ObjectType::Module) {
            const auto m = dynamic_cast<model::Module*>(frame->owner);
            current_file_path = m->path;
            break;
        }
    }
    return current_file_path;
}

} // namespace kiz
