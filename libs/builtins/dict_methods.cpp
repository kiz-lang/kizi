/**
 * @file dict_methods.cpp
 * @brief Dict类型的魔术方法实现
 * @author azhz1107cat
 * @date 2026-10-25
 */
#include "../../src/models/models.hpp"
#include "include/builtin_functions.hpp"

namespace model {

dep::BigInt hash_object(Object* key_obj) {
    // hash对象
    kiz::Vm::call_method(key_obj, "__hash__", {});

    const auto result = kiz::Vm::get_and_pop_stack_top();

    const auto result_int = dynamic_cast<Int*>(result.get());
    if (!result_int)
        throw NativeFuncError("TypeError", "Object's hash method return a value which type isn't Int");
    dep::BigInt key_hash_val = result_int->val;
    return key_hash_val;
}

// Dictionary.__add__
Object* dict_add(Object* self, const List* args) {
    kiz::Vm::assert_argc(1, args);
    
    auto self_dict = dynamic_cast<Dictionary*>(self);
    assert(self_dict != nullptr);
    
    auto another_dict = dynamic_cast<Dictionary*>(args->val[0]);
    if (! another_dict)
        throw NativeFuncError("TypeError", "Dict.add first argument must be Dict type");

    auto self_dict_to_vec = self_dict->val.to_vector();
    auto another_dict_to_vec = another_dict->val.to_vector();

    self_dict_to_vec.insert(
        self_dict_to_vec.end(),
        another_dict_to_vec.begin(),
        another_dict_to_vec.end()
    );
    
    auto new_dict = new Dictionary(dep::Dict(
        self_dict_to_vec
    )); // 构造内部为每个key/value make_ref

    // 以借用指针返回，由handle_call压栈时持有
    return new_dict;
};

// Dictionary.contains：判断是否包含指定键（key: String），返回Bool
Object* dict_contains(Object* self, const List* args) {
    kiz::Vm::assert_argc(1, args);
    
    auto self_dict = dynamic_cast<Dictionary*>(self);
    assert(self_dict != nullptr);
    
    // 键
    auto key_obj = args->val[0];
    dep::BigInt key_hash_val = hash_object(key_obj);

    auto found_pair_it = self_dict->val.find(
        key_hash_val
    );

    if (found_pair_it) {
        return load_true();
    }
    return load_false();
};

Object* dict_setitem(Object* self, const List* args) {
    kiz::Vm::assert_argc(2, args);
    auto self_dict = dynamic_cast<Dictionary*>(self);
    auto key_obj = args->val[0];
    auto value_obj = args->val[1];
    dep::BigInt key_hash_val = hash_object(key_obj);

    // 值语义：存储前深拷贝可变值（防止d["k"] = d构造自引用结构导致打印/拷贝无限递归）
    value_obj = model::copy_if_mutable(value_obj);
    // 字典持有新的键值对
    key_obj->make_ref();
    value_obj->make_ref();
    auto old_pair = self_dict->val.insert(
        key_hash_val,
        std::pair{key_obj, value_obj}
    );
    // 覆盖同名键时释放旧键值对
    if (old_pair.first) old_pair.first->del_ref();
    if (old_pair.second) old_pair.second->del_ref();
    return load_nil();
}

Object* dict_getitem(Object* self, const List* args) {
    auto self_dict = dynamic_cast<Dictionary*>(self);
    auto key_obj = builtin::get_one_arg(args);

    dep::BigInt key_hash_val = hash_object(key_obj);

    auto found_pair_it = self_dict->val.find(key_hash_val);
    if (found_pair_it) {
        return found_pair_it->value.second;
    }

    throw NativeFuncError("KeyError",
            "Undefined key " + key_obj->debug_string() + " in Dictionary object " + self->debug_string()
    );
}


Object* dict_str(Object* self, const List* args) {
    auto self_dict = dynamic_cast<Dictionary*>(self);
    std::string result = "{";
    auto kv_list = self_dict->val.to_vector();
    size_t i = 0;
    for (auto& kv_pair : kv_list | std::views::values) {
        result += kiz::Vm::obj_to_str(kv_pair.first) + ": " + kiz::Vm::obj_to_str(kv_pair.second);
        if (i != kv_list.size() - 1) {
            result += ", ";
        }
        ++i;
    }
    result += "}";
    return new String(result);
}

Object* dict_dstr(Object* self, const List* args) {
    auto self_dict = dynamic_cast<Dictionary*>(self);
    std::string result = "{";
    auto kv_list = self_dict->val.to_vector();
    size_t i = 0;
    for (auto& kv_pair : kv_list | std::views::values) {
        result += kiz::Vm::obj_to_debug_str(kv_pair.first) + ": " + kiz::Vm::obj_to_debug_str(kv_pair.second);
        if (i != kv_list.size() - 1) {
            result += ", ";
        }
        ++i;
    }
    result += "}";
    return new String(result);
}

Object* dict_foreach(Object* self, const List* args) {
    auto func_obj = builtin::get_one_arg(args);

    auto self_dict = dynamic_cast<Dictionary*>(self);
    assert(self_dict != nullptr);

    dep::BigInt idx = 0;
    for (auto pair : self_dict->val.to_vector() | std::views::values) {
        kiz::Vm::call_function(func_obj, {pair.first, pair.second}, nullptr);
        kiz::Vm::get_and_pop_stack_top(); // 丢弃并释放回调返回值
        idx += 1;
    }
    return load_nil();
}

Object* dict_next(Object* self, const List* args) {
    auto curr_idx_it = self->attrs.find("__current_index__");
    if(!curr_idx_it)
        throw NativeFuncError("TypeError", "Dict.next cannot find attribute '__current_index__' to get current item");

    auto curr_idx = curr_idx_it->value;

    auto index =  cast_to_int(curr_idx) ->val.to_unsigned_long_long();

    auto self_dict = dynamic_cast<Dictionary*>(self);
    if (index < self_dict->val.size()) {
        auto res = self_dict->val.to_vector()[index].second.second;
        self->attrs_insert("__current_index__", new Int(index+1));
        return res;
    }
    self->attrs_insert("__current_index__", kiz::Vm::small_int_pool[0]);
    return load_stop_iter_signal();
}

Object* dict_len(Object* self, const List* args) {
    auto self_dict = dynamic_cast<Dictionary*>(self);
    return new Int(self_dict->val.to_vector().size());
}

}  // namespace model