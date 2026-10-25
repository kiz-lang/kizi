/**
 * @file object_methods.cpp
 * @brief Object/Range/Error/Function/Module等类型的方法实现
 * @author azhz1107cat
 * @date 2026-10-25
 */
#include "../../src/models/models.hpp"
#include "include/builtin_functions.hpp"

namespace model {

// Object类型
Object* object_str(Object* self, const List* args) {
    return new String("<Object at " + model::ptr_to_string(self) + ">");
}

// 缺省真值：任何对象默认为真（否则 if obj 会因缺__bool__直接报错）
Object* object_bool(Object* self, const List* args) {
    return load_true();
}

Object* object_eq(Object* self, const List* args) {
    const auto other_obj = builtin::get_one_arg(args);
    return model::load_bool(self == other_obj);
}

Object* object_setitem(Object* self, const List* args) {
    kiz::Vm::assert_argc(2, args);
    auto attr = args->val[0];
    auto attr_str = dynamic_cast<model::String*>(attr);
    // 下标必须是Str：此前是assert断言，非法输入会直接abort整个解释器
    if (!attr_str)
        throw NativeFuncError("TypeError", "Object index must be a String");
    self->attrs_insert(attr_str->val, args->val[1]);
    return self;
}

Object* object_getitem(Object* self, const List* args) {
    auto attr = builtin::get_one_arg(args);
    auto attr_str = dynamic_cast<model::String*>(attr);
    // 下标必须是Str：此前是assert断言，非法输入会直接abort整个解释器
    if (!attr_str)
        throw NativeFuncError("TypeError", "Object index must be a String");
    return kiz::Vm::get_attr(self, attr_str->val);
}


// Range类型
Object* range_call(Object* self, const List* args) {
    auto arg_vector = args->val;
    dep::BigInt start_int = 0;
    dep::BigInt step_int = 1;
    dep::BigInt end_int = 1;
    auto r_obj = new Object();
    r_obj->attrs_insert("__parent__", self);

    if (arg_vector.size() == 1) {
        end_int = cast_to_int(arg_vector[0])->val.to_unsigned_long_long();
    }
    else if (arg_vector.size() == 2) {
        start_int = cast_to_int(arg_vector[0])->val.to_unsigned_long_long();
        end_int = cast_to_int(arg_vector[1])->val.to_unsigned_long_long();
    }
    else if (arg_vector.size() == 3) {
        start_int = cast_to_int(arg_vector[0])->val.to_unsigned_long_long();
        step_int = cast_to_int(arg_vector[1])->val.to_unsigned_long_long();
        end_int = cast_to_int(arg_vector[2])->val.to_unsigned_long_long();

    } else kiz::Vm::assert_argc({1,2,3}, args);

    r_obj->attrs_insert("current", new Int(start_int));
    r_obj->attrs_insert("start", new Int(start_int));
    r_obj->attrs_insert("step", new Int(step_int));
    r_obj->attrs_insert("end", new Int(end_int));
    return r_obj;
}

Object* range_next(Object* self, const List* args) {
    Int* start_obj = cast_to_int(kiz::Vm::get_attr_current(self, "start"));
    Int* step_obj = cast_to_int(kiz::Vm::get_attr_current(self, "step"));
    Int* end_obj = cast_to_int(kiz::Vm::get_attr_current(self, "end"));
    Int* current_obj = cast_to_int(kiz::Vm::get_attr_current(self, "current"));

    dep::BigInt& start_int = start_obj->val;
    dep::BigInt& step_int = step_obj->val;
    dep::BigInt& end_int = end_obj->val;
    dep::BigInt& current = current_obj->val;

    bool is_end = false;
    if (step_int > 0) {
        is_end = current >= end_int;
    } else if (step_int < 0) {
        // 负步长：向下计数到end
        is_end = current <= end_int;
    } else {
        // 步长为0永不产出
        return load_stop_iter_signal();
    }

    if (is_end) {
        return load_stop_iter_signal();
    }

    dep::BigInt old_val = current;
    current += step_int;

    // 返回迭代值
    return new Int(old_val);
}

Object* range_str(Object* self, const List* args) {
    dep::BigInt start_int = cast_to_int(kiz::Vm::get_attr_current(self, "start"))->val;
    dep::BigInt step_int = cast_to_int(kiz::Vm::get_attr_current(self, "step"))->val;
    dep::BigInt end_int = cast_to_int(kiz::Vm::get_attr_current(self, "end"))->val;
    dep::BigInt current = cast_to_int(kiz::Vm::get_attr_current(self, "current"))->val;

    return new String(std::format("Range(start={}, step={}, end={}, current={})", start_int.to_string(),
        step_int.to_string(), end_int.to_string(), current.to_string()));
}

// Error类型
Object* error_str(Object* self, const List* args) {
    auto name = kiz::Vm::obj_to_debug_str(kiz::Vm::get_attr_current(self, "__name__"));
    auto msg = kiz::Vm::obj_to_debug_str(kiz::Vm::get_attr_current(self, "__msg__"));
    return new String(std::format("Error(name={}, msg={})", name, msg));
}

Object* error_call(Object* self, const List* args) {
    kiz::Vm::assert_argc(2, args);
    auto err_name = args->val[0];
    auto err_msg = args->val[1];

    auto err = new Error(kiz::Vm::make_pos_info());
    err->attrs_insert("__name__", err_name);
    err->attrs_insert("__msg__", err_msg);
    return err;
}

// Function类型
Object* function_str(Object* self, const List* args) {
    auto self_fn = dynamic_cast<Function*>(self);
    return new String(
        "<Function: path='" + self_fn->name + "', argc=" + std::to_string(self_fn->argc) + " at " + ptr_to_string(self_fn) + ">"
    );
}

// NativeFunction类型
Object* native_function_str(Object* self, const List* args) {
    auto self_nfn = dynamic_cast<NativeFunction*>(self);
    return new model::String(
     "<NativeFunction" +
         (self_nfn->name.empty()
         ? ""
         : ": name='" + self_nfn->name + "'"
         )
         + " at " + ptr_to_string(self_nfn) + ">"
    );
}

// Module类型
Object* module_str(Object* self, const List* args) {
    auto self_mod = dynamic_cast<model::Module*>(self);
    return new model::String(
        "<Module: path='" + self_mod->path + "', attr=" + self_mod->attrs.to_string() + ", at " + ptr_to_string(self_mod) + ">"
    );
}

// Unpack类型
Object* unpack_str(Object* self, const List* args) {
    auto self_unpack = dynamic_cast<model::Unpack*>(self);
    return new model::String(
        "__Unpack of " + kiz::Vm::obj_to_str(self_unpack->val)
    );
}

}