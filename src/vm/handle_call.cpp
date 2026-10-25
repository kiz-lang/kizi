/**
 * @file handle_call.cpp
 * @brief 函数调用/方法调用/真值判断/对象转字符串处理
 * @author azhz1107cat
 * @date 2026-10-25
 */
#include <utility>

#include "vm.hpp"
#include "../models/models.hpp"
#include "opcode/opcode.hpp"
#include <unordered_set>

namespace kiz {

// 带已访问集合的属性查找：防止__parent__成环导致无限递归栈溢出
static model::Object* get_attr_with_visited(model::Object* obj, const std::string& attr_name,
                                             std::unordered_set<const model::Object*>& visited) {
    assert(obj != nullptr);
    if (visited.count(obj)) {
        throw NativeFuncError("NameError",
            "Cyclic __parent__ chain detected while looking up attribute '" + attr_name + "'");
    }
    visited.insert(obj);

    const auto attr_it = obj->attrs.find(attr_name);
    if (attr_it) {
        return attr_it->value;
    }
    auto parent_it = obj->attrs.find("__parent__");
    if (parent_it) {
        return get_attr_with_visited(parent_it->value, attr_name, visited);
    }
    throw NativeFuncError("NameError",
        "Undefined attribute '" + attr_name + "'"
    );
}

model::Object* Vm::get_attr(model::Object* obj, const std::string& attr_name) {
    std::unordered_set<const model::Object*> visited;
    return get_attr_with_visited(obj, attr_name, visited);
}

bool Vm::is_true(model::Object* obj) {
    if (const auto bool_obj = dynamic_cast<const model::Bool*>(obj)) {
        return bool_obj->val==true;
    }
    if (dynamic_cast<const model::Nil*>(obj)) {
        return false;
    }

    // 自定义__bool__互相返回对方会无限递归，深度封顶后报错
    static thread_local int bool_depth = 0;
    struct DepthGuard {
        int& d;
        explicit DepthGuard(int& depth) : d(depth) {
            ++d;
            if (d > 128) {
                throw NativeFuncError("TypeError", "__bool__ recursion too deep (possible cyclic __bool__)");
            }
        }
        ~DepthGuard() { --d; }
    } depth_guard(bool_depth);

    call_method(obj, "__bool__", {});
    auto result = get_and_pop_stack_top(); // RAII释放结果引用
    bool ret = is_true(result.get());

    return ret;
}

model::Object* Vm::get_attr_current(model::Object* obj, const std::string& attr) {
    const auto attr_it = obj->attrs.find(attr);
    if (attr_it) {
        return attr_it->value;
    }
    throw NativeFuncError("NameError",
        "Undefined attribute '" + attr + "'" + " of current attributes table"
    );
}

void Vm::handle_call(model::Object* func_obj, model::Object* args_obj, model::Object* self){
    assert(func_obj != nullptr);
    assert(args_obj != nullptr);
    auto args_list = dynamic_cast<model::List*>(args_obj);

    assert(args_list && "CALL: 栈顶-1元素非List类型（参数必须封装为列表）");
    DEBUG_OUTPUT("start to call function");

    // 分类型处理函数调用（Function / NativeFunction）
    if (const auto cpp_func = dynamic_cast<model::NativeFunction*>(func_obj)) {
        // -------------------------- 处理 NativeFunction 调用 --------------------------
        // 每次分派先复位所有权标志，防止上一次异常路径遗留的置位误伤本次
        native_return_owns = false;
        model::Object* return_val = cpp_func->func(self, args_list);

        // 管理返回值引用计数：返回值压栈前必须 make_ref
        if (!return_val){
            // 若返回空，默认压入 Nil（避免栈异常）
            return_val = model::unique_nil;
        }

        // 返回值压入操作数栈；"移除并返回"的原生函数已把引用移交回来，不再额外+1
        if (native_return_owns) {
            native_return_owns = false;
            op_stack.push_back(return_val);
        } else {
            push_to_stack(return_val);
        }
    } else if (auto func = dynamic_cast<model::Function*>(func_obj)) {
        // -------------------------- 处理 Function 调用 --------------------------
        DEBUG_OUTPUT("call Function: " + func->name);

        // 校验参数数量
        const size_t required_argc = func->argc;
        size_t actual_argc;
        if (self and self->get_type() != model::Object::ObjectType::Module) {
            actual_argc = args_list->val.size() + 1;
        } else {
            actual_argc = args_list->val.size();
        }
        if (actual_argc != required_argc and !func->has_rest_params)
            throw NativeFuncError("ArgCountError", std::format(
                "expect {} arguments but got {} arguments", required_argc, actual_argc
            ));
        // 可变参数函数：除末尾rest参数外，其余必要参数必须给足
        if (func->has_rest_params and actual_argc < required_argc - 1)
            throw NativeFuncError("ArgCountError", std::format(
                "expect at least {} arguments but got {} arguments", required_argc - 1, actual_argc
            ));

        // 创建新调用帧
        func->make_ref();
        func->code->make_ref();
        auto new_frame = new CallFrame{
            .name = func->name,

            .owner = func,

            .pc = 0,
            .return_to_pc = call_stack.back()->pc + 1,
            .last_bp = call_stack.back()->bp,
            .bp = op_stack.size(),
            .code_object = func->code,

            .iters{},

            .curr_error = nullptr
        };
        op_stack.resize(op_stack.size() + func->code->locals_count);

        // 储存self
        if (self and self->get_type() != model::Object::ObjectType::Module) {
            self->make_ref();
            args_list->val.emplace(args_list->val.begin(), self);
        }

        if (func->has_rest_params) {
            for (size_t i = 0; i < required_argc; ++i) {
                std::string param_name = new_frame->code_object->var_names[i];
                model::Object* param_val;
                if (i == required_argc - 1 ) {
                    // 释放被丢弃前缀元素在参数List中的引用（其值已各自拷入局部区）
                    for (size_t j = 0; j < i; ++j) {
                        if (args_list->val[j]) args_list->val[j]->del_ref();
                    }
                    args_list->val.assign(args_list->val.begin() + i, args_list->val.end());
                    param_val = args_list;
                }
                else {
                    param_val = args_list->val[i];  // 从列表取参数
                }

                assert(param_val);
                param_val->make_ref();
                op_stack[new_frame->bp + i] = param_val;
            }

            call_stack.emplace_back(new_frame);
            return;
        }

        // 从参数列表中提取参数，存入locals
        for (size_t i = 0; i < required_argc; ++i) {
            std::string param_name = new_frame->code_object->var_names[i];
            model::Object* param_val = args_list->val[i];  // 从列表取参数

            // 校验参数非空
            assert(param_val != nullptr);

            // 增加参数引用计数（存入locals需持有引用）
            param_val->make_ref();
            op_stack[new_frame->bp + i] = param_val;
        }

        // 压入新调用帧，更新程序计数器
        call_stack.emplace_back(new_frame);

    // 处理对象魔术方法__call__
    } else {
        model::Object* callable;
        try {
            callable = get_attr(func_obj, "__call__");
        } catch (NativeFuncError& e) {
            throw NativeFuncError("TypeError", "try to call an uncallable object");
        }
        assert(callable);
        handle_call(callable, args_obj, func_obj);
    }
}

void Vm::call_function(model::Object* func_obj, std::vector<model::Object*> args, model::Object* self) {
    size_t old_call_stack_size = call_stack.size();

    // List构造持有每个参数的引用；temp_args自身+1后移交handle_call使用
    auto temp_args = new model::List(std::move(args));
    temp_args->make_ref();
    try {
        handle_call(func_obj, temp_args, self);
    } catch (...) {
        // 参数校验失败等场景下释放参数列表后继续抛出
        temp_args->del_ref();
        throw;
    }
    temp_args->del_ref();

    if (old_call_stack_size == call_stack.size()) return;

    while (running and !call_stack.empty() and call_stack.size() > old_call_stack_size) {
        auto curr_frame = call_stack.back();
        auto& frame_code = curr_frame->code_object;

        // 检查是否执行到代码末尾：按RET语义清理并销毁帧
        if (curr_frame->pc >= frame_code->code.size()) {
            call_stack.pop_back();
            if (!call_stack.empty()) {
                call_stack.back()->bp = curr_frame->last_bp;
                call_stack.back()->pc = curr_frame->return_to_pc;
            }
            unwind_op_stack_to(curr_frame->bp);
            destroy_frame(curr_frame);
            continue;
        }

        // 执行当前指令
        const Instruction& curr_inst = frame_code->code[curr_frame->pc];
        try {
            // RET由本函数代为处理：按RET语义完整清理帧，但不回写调用者pc
            // （调用者的pc推进交还给外层执行循环的ADVANCE_PC，避免跳过一条指令）
            if (curr_inst.opc == Opcode::RET and old_call_stack_size == call_stack.size() - 1) {
                assert(!call_stack.empty());
                auto frame = call_stack.back();
                call_stack.pop_back();

                auto return_val = get_and_pop_stack_top();
                unwind_op_stack_to(frame->bp);
                push_to_stack(return_val.get());

                if (frame->curr_error) frame->curr_error->del_ref();
                frame->owner->del_ref();
                frame->code_object->del_ref();
                for (auto it: frame->iters) {
                    if (it) it->del_ref();
                }
                delete frame;
                return;
            }
            execute_unit(curr_inst); // 调用VM的指令执行核心方法
        } catch (const NativeFuncError& e) {
            // 原生函数执行错误，抛出异常
            forward_to_handle_throw(e.name, e.msg);
            // 本函数管理的帧在解栈中被销毁（帧数跌破进入时的基线）：处理点在更外层
            if (call_stack.size() <= old_call_stack_size) {
                throw KizUnwindSignal();
            }
            continue; // pc已被重定向到handler起点，不再推进
        }

        // THROW等指令在C++层不抛异常即可完成解栈跳转：
        // 帧数跌破基线说明本函数管理的帧已被销毁，处理点在别的执行循环，用信号穿透C++栈
        if (call_stack.size() <= old_call_stack_size) {
            throw KizUnwindSignal();
        }

        ADVANCE_PC

    }
}

void Vm::call_method(model::Object* obj, const std::string& attr_name, std::vector<model::Object*> args) {
    assert(obj != nullptr);
    // 统一走get_attr：先查对象自身属性，再沿__parent__链查找
    // （此前魔术方法只查父链，导致实例自设的__bool__/__add__等与Nil自带方法被静默忽略）
    call_function(get_attr(obj, attr_name), args, obj);
}

std::string Vm::obj_to_str(model::Object* for_cast_obj) {
    DEBUG_OUTPUT("obj to str");
    try {
        call_method(for_cast_obj, "__str__", {});
    } catch (NativeFuncError& e) {
        call_method(for_cast_obj, "__dstr__", {});
    }
    auto res = get_and_pop_stack_top(); // RAII释放结果引用
    std::string val = model::cast_to_str(res.get())->val;
    return val;
}


std::string Vm::obj_to_debug_str(model::Object* for_cast_obj) {
    DEBUG_OUTPUT("obj to debug str");
    try {
        call_method(for_cast_obj, "__dstr__", {});
    } catch (NativeFuncError& e) {
        call_method(for_cast_obj, "__str__", {});
    }
    auto res = get_and_pop_stack_top(); // RAII释放结果引用
    std::string val = model::cast_to_str(res.get()) ->val;
    return val;
}
}
