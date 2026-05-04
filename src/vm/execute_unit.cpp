#include "vm.hpp"
#include "../../libs/builtins/include/builtin_functions.hpp"
#include "../opcode/opcode.hpp"

///| 核心执行单元
namespace kiz {
void Vm::execute_unit(const Instruction& instruction) {
    switch (instruction.opc) {
    case Opcode::OP_ADD: {
        auto b = get_and_pop_stack_top();
        auto a = get_and_pop_stack_top();
        call_method(a.get(), "__add__", {b.get()});
        break;
    }

    case Opcode::OP_SUB: {
        auto b = get_and_pop_stack_top();
        auto a = get_and_pop_stack_top();
        call_method(a.get(), "__sub__", {b.get()});
        break;
    }

    case Opcode::OP_MUL: {
        auto b = get_and_pop_stack_top();
        auto a = get_and_pop_stack_top();
        call_method(a.get(), "__mul__", {b.get()});
        break;
    }

    case Opcode::OP_DIV: {
        auto b = get_and_pop_stack_top();
        auto a = get_and_pop_stack_top();
        call_method(a.get(), "__div__", {b.get()});
        break;
    }

    case Opcode::OP_MOD: {
        auto b = get_and_pop_stack_top();
        auto a = get_and_pop_stack_top();
        call_method(a.get(), "__mod__", {b.get()});
        break;
    }

    case Opcode::OP_POW: {
        auto b = get_and_pop_stack_top();
        auto a = get_and_pop_stack_top();
        call_method(a.get(), "__pow__", {b.get()});
        break;
    }

    case Opcode::OP_NEG: {
        auto a = get_and_pop_stack_top();
        call_method(a.get(), "__neg__", {});
        break;
    }

    case Opcode::OP_EQ: {
        auto b = get_and_pop_stack_top();
        auto a = get_and_pop_stack_top();

        call_method(a.get(), "__eq__", {b.get()});
        break;
    }

    case Opcode::OP_GT: {
        auto b = get_and_pop_stack_top();
        auto a = get_and_pop_stack_top();

        call_method(a.get(), "__gt__", {b.get()});
        break;
    }

    case Opcode::OP_LT: {
        auto b = get_and_pop_stack_top();
        auto a = get_and_pop_stack_top();

        call_method(a.get(), "__lt__", {b.get()});
        break;
    }

    case Opcode::OP_GE: {
        auto b = get_and_pop_stack_top();
        auto a = get_and_pop_stack_top();

        call_method(a.get(), "__eq__", {b.get()});
        call_method(a.get(), "__gt__", {b.get()});

        // 统一使用封装的栈操作获取结果
        auto gt_result = get_and_pop_stack_top();
        auto eq_result = get_and_pop_stack_top();

        // 压入最终结果
        if (is_true(gt_result.get()) or is_true(eq_result.get())) {
            push_to_stack(model::load_true());
        } else {
            push_to_stack(model::load_false());
        }
        break;
    }

    case Opcode::OP_LE: {
        auto b = get_and_pop_stack_top();
        auto a = get_and_pop_stack_top();

        // 调用__eq__方法
        call_method(a.get(), "__eq__", {b.get()});

        // 调用__lt__方法
        call_method(a.get(), "__lt__", {b.get()});

        // 获取结果
        auto lt_result = get_and_pop_stack_top();
        auto eq_result = get_and_pop_stack_top();

        // 压入最终结果
        if (is_true(lt_result.get()) or is_true(eq_result.get())) {
            push_to_stack(model::load_true());
        } else {
            push_to_stack(model::load_false());
        }
        break;
    }

    case Opcode::OP_NE: {
        auto b = get_and_pop_stack_top();
        auto a = get_and_pop_stack_top();


        // 调用__eq__方法
        call_method(a.get(), "__eq__", {b.get()});

        // 获取比较结果
        auto eq_result = get_and_pop_stack_top();

        // 压入取反结果
        push_to_stack(model::load_bool(
            ! is_true(eq_result.get())
        ));
        break;
    }

    case Opcode::OP_NOT: {
        auto a = get_and_pop_stack_top();
        bool result = !is_true(a.get());
        push_to_stack(model::load_bool(result));
        break;
    }

    case Opcode::OP_IS: {
        auto b = get_and_pop_stack_top();
        auto a = get_and_pop_stack_top();
        push_to_stack(model::load_bool(a.get() == b.get()));
        break;
    }

    case Opcode::OP_IN: {
        auto for_check = get_and_pop_stack_top();
        auto item = get_and_pop_stack_top();

        // 调用contains方法，参数为item
        call_method(for_check.get(), "contains", {item.get()});
        break;
    }

    case Opcode::OP_UNPACK: {
        auto obj = simple_get_and_pop_stack_top();
        push_to_stack(new model::Unpack(obj));
        break;
    }

    case Opcode::MAKE_LIST: {
        make_list(instruction.opn);
        break;
    }

    case Opcode::MAKE_DICT: {
        make_dict(instruction.opn);
        break;
    }

    case Opcode::CREATE_CLOSURE: {
        auto func_obj = dynamic_cast<model::Function*>(op_stack.back());

        auto& upvalues = func_obj->code->upvalues;
        std::vector<model::Object*> free_vars {};

        for (const auto& [distance_from_curr, idx] : upvalues) {
            auto frame = call_stack[ call_stack.size() - distance_from_curr];
            size_t loc_based = frame->bp;

            auto var = op_stack[loc_based + idx];
            var->make_ref();
            free_vars.push_back( var );
        }

        func_obj->free_vars = free_vars;
        break;
    }


    case Opcode::CALL: {
        auto func_obj = get_and_pop_stack_top();
        // 弹出栈顶-1元素 : 参数列表
        auto args_obj = get_and_pop_stack_top();
        handle_call(func_obj.get(), args_obj.get(), nullptr);
        break;
    }

    case Opcode::RET: {
        // 执行ensure确保资源被释放
        handle_ensure();

        auto frame = call_stack.back();
        call_stack.pop_back();
        call_stack.back()->bp = frame->last_bp;
        call_stack.back()->pc = frame->return_to_pc;

        auto return_val = get_and_pop_stack_top();
        assert(return_val.get());

        while (frame->bp < op_stack.size()) {
            op_stack.back()->del_ref();
            op_stack.pop_back();
        }

        push_to_stack(return_val.get());

        frame->owner->del_ref();
        frame->code_object->del_ref();
        for (auto it: frame->iters) {
            if (it) it->del_ref();
        }

        delete frame;
        break;
    }

    case Opcode::CALL_METHOD: {
        auto obj = get_and_pop_stack_top();

        // 弹出栈顶-1元素 : 参数列表
        auto args_obj = get_and_pop_stack_top();

        std::string attr_name = get_attr_name_by_idx(instruction.opn);

        auto func_obj = get_attr(obj.get(), attr_name);

        func_obj->make_ref();
        handle_call(func_obj, args_obj.get(), obj.get());
        break;
    }

    case Opcode::GET_ATTR: {
        auto obj = get_and_pop_stack_top();
         std::string attr_name = get_attr_name_by_idx(instruction.opn);

        model::Object* attr_val = get_attr(obj.get(), attr_name);
        push_to_stack(attr_val);
        break;
    }

    case Opcode::SET_ATTR: {
        auto attr_val = get_and_pop_stack_top();
        auto obj = get_and_pop_stack_top();
        std::string attr_name = get_attr_name_by_idx(instruction.opn);

        if (std::ranges::find(builtins, obj.get()) != std::ranges::end(builtins)) {
            throw NativeFuncError("SetattrError", "Cannot reset or add attribute for builtin object");
        }

        auto new_val = model::copy_if_mutable(attr_val.get());
        auto old_it = obj.get()->attrs.find(attr_name);   // 获取旧值（若有）
        obj.get()->attrs_insert(attr_name, new_val);      // 插入新值，内部 make_ref

        if (old_it) old_it->value->del_ref();       // 释放旧值
        break;
    }

    case Opcode::GET_ITEM: {
        auto obj = get_and_pop_stack_top();
        auto args_list = get_and_pop_stack_top();

        call_method(obj.get(), "__getitem__", model::cast_to_list(
            args_list.get()
        ) -> val);
        break;
    }

    case Opcode::SET_ITEM: {
        auto value = get_and_pop_stack_top();
        auto arg = get_and_pop_stack_top();
        auto obj = get_and_pop_stack_top();

        // 获取对象自身的 __setitem__
        call_method(obj.get(), "__setitem__", {arg.get(), value.get()});
        break;
    }

    case Opcode::LOAD_VAR: {
        auto val = op_stack[call_stack.back()->bp + instruction.opn];
        push_to_stack(val);
        break;
    }

    case Opcode::LOAD_CONST: {
        size_t const_idx = instruction.opn;
        model::Object* const_val = const_pool[const_idx];
        push_to_stack(const_val);
        break;
    }

    case Opcode::LOAD_BUILTINS: {
        auto obj = builtins[ instruction.opn ];
        push_to_stack(obj);
        break;
    }

    case Opcode::LOAD_FREE_VAR: {
        auto func = dynamic_cast<model::Function*>(call_stack.back()->owner);
        assert(func != nullptr);
        push_to_stack(func->free_vars[ instruction.opn ]);
        break;
    }

    case Opcode::SET_LOCAL: {
        auto value = get_and_pop_stack_top();

        size_t offset = call_stack.back()->bp + instruction.opn;
        auto new_val = model::copy_if_mutable(value.get());
        new_val->make_ref();

        if (op_stack[offset]) {
            op_stack[offset]->del_ref();
        }
        op_stack[offset] = new_val;
        break;
    }


    case Opcode::SET_GLOBAL: {
        auto offset = instruction.opn;
        auto value = get_and_pop_stack_top();

        auto new_val = model::copy_if_mutable(value.get());
        new_val->make_ref();
        if (op_stack[offset]) {
            op_stack[offset]->del_ref();
        }

        op_stack[offset] = new_val;
        break;
    }

    case Opcode::SET_NONLOCAL: {
        auto idx_of_upvalue = instruction.opn;
        auto upvalue = call_stack.back()->code_object->upvalues[ idx_of_upvalue ];
        auto frame = call_stack[ call_stack.size() - upvalue.distance_from_curr - 1]; // 区别于CREATE_CLOSURE指令, 这里在函数中要多减一
        size_t loc_based = frame->bp;

        auto value = get_and_pop_stack_top();

        auto new_val = model::copy_if_mutable(value.get());
        new_val->make_ref();

        if (op_stack[loc_based + upvalue.idx]) {
            op_stack[loc_based + upvalue.idx]->del_ref();
        }

        op_stack[loc_based + upvalue.idx] = new_val;

        // 更新闭包
        if (auto f = dynamic_cast<model::Function*>(call_stack.back()->owner)) {
            f->free_vars[idx_of_upvalue] = new_val;
        }
        break;
    }


    case Opcode::THROW: {
        auto top = get_and_pop_stack_top();
        if (call_stack.back()->curr_error) call_stack.back()->curr_error->del_ref();
        call_stack.back()->curr_error = top.get();
        top.get()->make_ref();     // 使 curr_error 持有引用
        handle_throw();
        break;
    }

    case Opcode::LOAD_ERROR: {
        if (!call_stack.back()->curr_error) {
            throw KizStopRunningSignal("Unable to load error");
        }
        call_stack.back()->curr_error->make_ref();
        push_to_stack(call_stack.back()->curr_error);
        break;
    }

    case Opcode::JUMP: {
        size_t target_pc = instruction.opn;
        call_stack.back()->pc = target_pc;
        break;
    }

    case Opcode::JUMP_IF_FALSE: {
        auto cond = get_and_pop_stack_top();
        if (! is_true(cond.get())) {
            // 跳转逻辑
            call_stack.back()->pc = instruction.opn;
        } else {
            call_stack.back()->pc++;
        }
        break;
    }

    case Opcode::IS_CHILD: {
        auto b = get_and_pop_stack_top();
        auto a = get_and_pop_stack_top();
        push_to_stack(builtin::check_based_object(a.get(), b.get()));
        break;
    }

    case Opcode::CREATE_OBJECT: {
        auto obj = new model::Object();
        obj->attrs_insert("__parent__", model::based_obj);
        push_to_stack(obj);
        break;
    }

    case Opcode::IMPORT: {
        std::string module_path = get_attr_name_by_idx(instruction.opn);
        handle_import(module_path);
        break;
    }

    case Opcode::CACHE_ITER: {
        auto iter = op_stack.back();
        iter->make_ref();

        call_stack.back()->iters.push_back(iter);
        break;
    }

    case Opcode::GET_ITER: {
        push_to_stack(
            call_stack.back()->iters.back()
        );
        break;
    }

    case Opcode::POP_ITER: {
        auto iter_obj = call_stack.back()->iters.back();
        iter_obj->del_ref();
        call_stack.back()->iters.pop_back();
        break;

    }

    case Opcode::JUMP_IF_FINISH_ITER: {
        auto obj = get_and_pop_stack_top();
        size_t target_pc = instruction.opn;
        if (obj.get() == model::stop_iter_signal) {
            call_stack.back()->pc = target_pc;
            return;
        }
        call_stack.back()->pc ++;
        break;
    }

    case Opcode::COPY_TOP: {
        auto obj = get_and_pop_stack_top();
        push_to_stack(obj.get());
        push_to_stack(obj.get());
        break;
    }

    case Opcode::STOP: {
        running = false;
        break;
    }

    default: throw NativeFuncError("FutureError", "execute_instruction meet unknown opcode");
    }

}
}
