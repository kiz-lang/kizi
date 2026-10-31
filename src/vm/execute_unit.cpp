/**
 * @file execute_unit.cpp
 * @brief VM核心执行单元（指令分发）
 * @author azhz1107cat
 * @date 2026-10-25
 */
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
        auto unpack_obj = new model::Unpack(obj); // 构造内部已make_ref
        obj->del_ref(); // 归还从操作数栈移交来的引用
        push_to_stack(unpack_obj);
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

    case Opcode::MAKE_CELL: {
        // 把局部槽位装箱为Cell（接管槽位对现有值的引用），槽位改为持有Cell
        // 幂等：已装箱则跳过（for循环每轮显式重装箱为新Cell实现逐轮独立）
        size_t offset = call_stack.back()->bp + instruction.opn;
        if (dynamic_cast<model::Cell*>(op_stack[offset])) {
            break;
        }
        auto cell = new model::Cell(op_stack[offset]); // 接管槽位对旧值的引用
        cell->make_ref();                              // 槽位持有Cell
        op_stack[offset] = cell;
        break;
    }

    case Opcode::FRESH_CELL: {
        // 为循环变量无条件换新空Cell：旧Cell若有闭包持有则完整保留（逐轮独立捕获）
        // 随后的SET_LOCAL向新Cell写入本轮值
        size_t offset = call_stack.back()->bp + instruction.opn;
        if (op_stack[offset]) {
            op_stack[offset]->del_ref();
        }
        auto cell = new model::Cell(nullptr);
        cell->make_ref();
        op_stack[offset] = cell;
        break;
    }

    case Opcode::CREATE_CLOSURE: {
        auto func_obj = dynamic_cast<model::Function*>(op_stack.back());
        assert(func_obj != nullptr);

        auto& upvalues = func_obj->code->upvalues;

        // 克隆出独立的Function对象持有本次捕获：各闭包有自己的free_vars列表，
        // 但捕获的是外层作用域装箱的Cell指针 → 兄弟闭包共享可变状态
        auto closure = new model::Function(func_obj->name, func_obj->code, func_obj->argc);
        closure->has_rest_params = func_obj->has_rest_params;

        std::vector<model::Object*> free_vars {};
        for (const auto& [distance_from_curr, idx] : upvalues) {
            // 按词法层级回溯目标帧：目标chunk深度 = 本闭包scope_depth - distance，
            // 从栈顶向栈底找最近的活动帧（递归重入时中间夹着更深的帧，固定栈偏移会指错）
            const size_t target_depth = func_obj->code->scope_depth - distance_from_curr;
            kiz::CallFrame* def_frame = nullptr;
            for (auto it = call_stack.rbegin(); it != call_stack.rend(); ++it) {
                if ((*it)->code_object->scope_depth == target_depth) {
                    def_frame = *it;
                    break;
                }
            }
            if (!def_frame) {
                throw NativeFuncError("NameError", "closure defining scope is not active");
            }

            auto var = op_stack[def_frame->bp + idx];
            // 被捕获槽位已在定义作用域入口装箱为Cell；递归自引用时
            // Cell的值经随后的SET_LOCAL回写自然指向本克隆，无需特判
            if (!dynamic_cast<model::Cell*>(var)) {
                throw NativeFuncError("NameError", "closure captured slot was not boxed");
            }
            var->make_ref();
            free_vars.push_back(var);
        }
        closure->free_vars = std::move(free_vars);

        op_stack.back()->del_ref(); // 释放const池共享函数对象在栈上的引用
        closure->make_ref();        // 闭包对象接替其栈槽位持有
        op_stack.back() = closure;
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
        auto frame = call_stack.back();
        call_stack.pop_back();

        // 模块顶层return：调用栈已空，直接结束执行（此前对空栈取back()是UB崩溃）
        if (call_stack.empty()) {
            auto return_val = get_and_pop_stack_top();
            unwind_op_stack_to(frame->bp);
            push_to_stack(return_val.get());
            destroy_frame(frame);
            running = false;
            break;
        }

        call_stack.back()->bp = frame->last_bp;
        call_stack.back()->pc = frame->return_to_pc;

        auto return_val = get_and_pop_stack_top();
        assert(return_val.get());

        // 清空该帧局部区与残留临时值（槽位可能为nullptr，需判空）
        while (frame->bp < op_stack.size()) {
            if (op_stack.back()) op_stack.back()->del_ref();
            op_stack.pop_back();
        }

        push_to_stack(return_val.get());

        if (frame->curr_error) frame->curr_error->del_ref();
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

        // get_attr返回借用指针，handle_call内部会自行持有引用
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

        // copy_if_mutable返回借用指针(可变值为新拷贝)，attrs_insert内部统一持有新值并释放旧值
        auto new_val = model::copy_if_mutable(attr_val.get());
        obj.get()->attrs_insert(attr_name, new_val);
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
        // 被捕获的槽位装的是Cell，穿透读内部值
        if (auto cell = dynamic_cast<model::Cell*>(val)) {
            val = cell->val;
        }
        // 声明过但尚未赋值的槽位：给出带变量名的可读错误而非晦涩的VM内部错误
        if (!val) {
            const auto& var_names = call_stack.back()->code_object->var_names;
            std::string name = instruction.opn < var_names.size() ? var_names[instruction.opn] : "<unknown>";
            throw NativeFuncError("NameError", "Variable '" + name + "' used before assignment");
        }
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
        // 捕获的是Cell，穿透读内部值
        auto fv = func->free_vars[ instruction.opn ];
        if (auto cell = dynamic_cast<model::Cell*>(fv)) {
            fv = cell->val;
        }
        if (!fv) {
            throw KizStopRunningSignal("Unable to load free var: value not assigned");
        }
        push_to_stack(fv);
        break;
    }

    case Opcode::SET_LOCAL: {
        auto value = get_and_pop_stack_top();

        size_t offset = call_stack.back()->bp + instruction.opn;
        auto new_val = model::copy_if_mutable(value.get());
        new_val->make_ref();

        // 被捕获的槽位装Cell：写穿透到Cell内部（兄弟闭包共享）
        if (auto cell = dynamic_cast<model::Cell*>(op_stack[offset])) {
            cell->set(new_val);
            break;
        }

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

        // 全局槽位被闭包捕获时同样是Cell，写穿透
        if (auto cell = dynamic_cast<model::Cell*>(op_stack[offset])) {
            cell->set(new_val);
            break;
        }

        if (op_stack[offset]) {
            op_stack[offset]->del_ref();
        }

        op_stack[offset] = new_val;
        break;
    }

    case Opcode::SET_NONLOCAL: {
        auto idx_of_upvalue = instruction.opn;
        auto func = dynamic_cast<model::Function*>(call_stack.back()->owner);
        // 直接写本闭包捕获的Cell：不依赖外层帧存活（修复闭包逃逸后写错帧的问题）
        if (!func or idx_of_upvalue >= func->free_vars.size()) {
            throw NativeFuncError("NameError", "nonlocal target variable was not captured");
        }
        auto cell = dynamic_cast<model::Cell*>(func->free_vars[idx_of_upvalue]);
        if (!cell) {
            throw NativeFuncError("NameError", "nonlocal target variable was not captured as a shared cell");
        }

        auto value = get_and_pop_stack_top();
        auto new_val = model::copy_if_mutable(value.get());
        new_val->make_ref(); // Cell接管此引用

        cell->set(new_val);
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
        // push_to_stack内部会make_ref，curr_error自身持有的引用不变
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
        obj->attrs_insert(model::magic_name::parent, model::based_obj);
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
