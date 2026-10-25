/**
 * @file list_methods.cpp
 * @brief List类型的魔术方法与成员方法实现
 * @author azhz1107cat
 * @date 2026-10-25
 */
#include <format>

#include "../../src/models/models.hpp"
#include "include/builtin_functions.hpp"

namespace model {

// List.__call__
Object* list_call(Object* self, const List* args) {
    std::vector<Object*> list = {};
    if (args->val.empty()) {
        return new List({});
    }

    auto for_cast = builtin::get_one_arg(args);
    while (true) {
        kiz::Vm::call_method(for_cast, "__next__", {});
        auto res = kiz::Vm::simple_get_and_pop_stack_top();
        if (res == stop_iter_signal) {
            break;
        }
        list.push_back(res); // 携带压栈时的引用，构造完成后归还
    }
    auto new_list = new List(list); // 构造内部为每个元素make_ref
    for (auto e : list) {
        e->del_ref(); // 归还各元素从操作数栈移交来的引用
    }
    return new_list;
}

// List.__bool__
Object* list_bool(Object* self, const List* args) {
    const auto self_int = dynamic_cast<List*>(self);
    assert(self_int != nullptr);
    if (self_int->val.empty()) return load_false();
    return load_true();
}

//  List.__add__：拼接另一个List（self + 传入List，返回新List）
Object* list_add(Object* self, const List* args) {
    kiz::Vm::assert_argc(1, args);
    
    auto self_list = dynamic_cast<List*>(self);
    assert(self_list != nullptr);
    
    auto another_list = dynamic_cast<List*>(args->val[0]);
    if (!another_list)
        throw NativeFuncError("TypeError", "List.add only supports List type argument");
    
    // 浅拷贝
    std::vector<Object*> new_vals = self_list->val;
    new_vals.insert(new_vals.end(), another_list->val.begin(), another_list->val.end());
    
    return new List(std::move(new_vals));
};

// List.__mul__：重复自身n次 self * n
Object* list_mul(Object* self, const List* args) {
    kiz::Vm::assert_argc(1, args);
    
    auto self_list = dynamic_cast<List*>(self);
    assert(self_list != nullptr);
    
    auto times_int = dynamic_cast<Int*>(args->val[0]);
    if (! times_int)
        throw NativeFuncError("TypeError", "List.mul only supports Int type argument");
    if (times_int->val < dep::BigInt(0))
        throw NativeFuncError("TypeError", "List.mul requires non-negative integer argument");
    
    std::vector<Object*> new_vals;
    dep::BigInt times = times_int->val;
    for (dep::BigInt i = dep::BigInt(0); i < times; i+=dep::BigInt(1)) {
        new_vals.insert(new_vals.end(), self_list->val.begin(), self_list->val.end());
    }
    
    return new List(std::move(new_vals));
};

// List.__eq__：判断两个List是否相等
Object* list_eq(Object* self, const List* args) {
    kiz::Vm::assert_argc(1, args);
    
    auto self_list = dynamic_cast<List*>(self);
    assert(self_list != nullptr);
    
    auto another_list = dynamic_cast<List*>(args->val[0]);
    if (! another_list)
        throw NativeFuncError("TypeError", "List.eq only supports List type argument");
    
    // 比较元素个数，不同直接返回false
    if (self_list->val.size() != another_list->val.size()) {
        return load_false();
    }
    
    // 逐个比较元素（列表在自定义__eq__回调中可能被修改，逐次取size）
    for (size_t i = 0; i < self_list->val.size() and i < another_list->val.size(); ++i) {
        Object* self_elem = self_list->val[i];
        Object* another_elem = another_list->val[i];
        // 调用 __eq__
        kiz::Vm::call_method(
            self_elem, "__eq__", {another_elem}
        );
        const auto eq_result = kiz::Vm::simple_get_and_pop_stack_top(); // 移交引用

        // 解析比较结果
        const auto eq_bool = dynamic_cast<Bool*>(eq_result);
        if (! eq_bool) {
            eq_result->del_ref();
            throw NativeFuncError("TypeError", "__eq__ method must return Bool type");
        }

        // 任意元素不相等，返回 false
        if (!eq_bool->val) {
            eq_result->del_ref(); // 归还引用
            return load_false();
        }
        eq_result->del_ref(); // 归还引用
    }
    
    // 所有元素均相等，返回 true
    return load_true();
};

Object* list_str(Object* self, const List* args) {
    auto self_list = dynamic_cast<List*>(self);
    std::string result = "[";
    for (size_t i = 0; i < self_list->val.size(); ++i) {
        if (self_list->val[i]) {
            result += kiz::Vm::obj_to_str(self_list->val[i]);
        } else {
            result += "Nil";
        }
        if (i != self_list->val.size() - 1) {
            result += ", ";
        }
    }
    result += "]";
    return new String(result);
}

Object* list_dstr(Object* self, const List* args) {
    auto self_list = dynamic_cast<List*>(self);
    std::string result = "[";
    for (size_t i = 0; i < self_list->val.size(); ++i) {
        if (self_list->val[i] != nullptr) {
            result += kiz::Vm::obj_to_debug_str(self_list->val[i]);
        } else {
            result += "Nil";
        }
        if (i != self_list->val.size() - 1) {
            result += ", ";
        }
    }
    result += "]";
    return new String(result);
}

// List.contains：判断列表是否包含目标元素
Object* list_contains(Object* self, const List* args) {
    kiz::Vm::assert_argc(1, args);
    
    auto self_list = dynamic_cast<List*>(self);
    assert(self_list != nullptr);
    
    Object* target_elem = args->val[0];

    // 按下标动态读取：回调中修改列表不会使迭代器失效
    for (size_t i = 0; i < self_list->val.size(); ++i) {

        kiz::Vm::call_method(
            self_list->val[i], "__eq__", {target_elem}
        );
        const auto result = kiz::Vm::simple_get_and_pop_stack_top(); // 移交引用

        // 找到匹配元素，立即返回true
        bool matched = kiz::Vm::is_true(result);
        result->del_ref(); // 归还引用
        if (matched) return load_true();
    }
    
    // 遍历完未找到匹配元素，返回false
    return load_false();
};

// List.append：向列表尾部添加一个元素
Object* list_append(Object* self, const List* args) {
    kiz::Vm::assert_argc(1, args);
    
    auto self_list = dynamic_cast<List*>(self);
    assert(self_list != nullptr);
    
    Object* elem_to_add = args->val[0];

    // 添加元素到列表尾部
    self_list->val.push_back(elem_to_add);
    elem_to_add->make_ref();
    
    // 返回列表自身（借用指针，由handle_call压栈时持有），支持链式调用
    return self;
};

Object* list_next(Object* self, const List* args) {
    auto curr_idx_it = self->attrs.find("__current_index__");
    if(!curr_idx_it)
        throw NativeFuncError("TypeError", "List.next cannot find attribute '__current_index__' to get current index");

    auto curr_idx = curr_idx_it->value;

    auto index =  cast_to_int(curr_idx) ->val.to_unsigned_long_long();

    auto self_list = dynamic_cast<List*>(self);
    if (index < self_list->val.size()) {
        auto res = self_list->val[index];
        self->attrs_insert("__current_index__", new Int(index+1));
        return res;
    }
    self->attrs_insert("__current_index__", kiz::Vm::small_int_pool[0]);
    return load_stop_iter_signal();
}

Object* list_foreach(Object* self, const List* args) {
    auto func_obj = builtin::get_one_arg(args);

    auto self_list = dynamic_cast<List*>(self);
    assert(self_list != nullptr);

    // 按下标动态读取：回调中修改列表(append等)不会使迭代器失效
    for (size_t idx = 0; idx < self_list->val.size(); ++idx) {
        kiz::Vm::call_function(func_obj, {self_list->val[idx]}, nullptr);
        kiz::Vm::get_and_pop_stack_top(); // 丢弃并释放回调返回值
    }
    return load_nil();
}

Object* list_reverse(Object* self, const List* args) {
    const auto self_list = dynamic_cast<List*>(self);
    assert(self_list != nullptr);

    std::ranges::reverse(self_list->val);
    return load_nil();
}

Object* list_extend(Object* self, const List* args) {
    auto self_list = dynamic_cast<List*>(self);
    assert(self_list != nullptr);

    auto other_list_obj = builtin::get_one_arg(args);
    auto other_list = dynamic_cast<List*>(other_list_obj);
    if (!other_list)
        throw NativeFuncError("TypeError", "The first argument of List.extend must be List type");

    // 快照另一列表的元素：a.extend(a)时边遍历边追加会无限增长
    std::vector<Object*> other_elems(other_list->val.begin(), other_list->val.end());
    for (auto e: other_elems) {
        e->make_ref(); // self侧列表持有新元素的引用
        self_list->val.push_back(e);
    }
    return load_nil();
}

Object* list_pop(Object* self, const List* args) {
    auto self_list = dynamic_cast<List*>(self);
    assert(self_list != nullptr);

    if (self_list->val.empty()) {
        return load_nil();
    }
    auto back = self_list->val.back();
    self_list->val.pop_back();
    // 列表对该元素的引用随弹出移交给返回值（native_return_owns），
    // 若先del_ref再返回，元素可能refc归零被销毁，返回悬垂指针
    kiz::Vm::native_return_owns = true;
    return back;
}

Object* list_insert(Object* self, const List* args) {
    auto self_list = dynamic_cast<List*>(self);
    assert(self_list != nullptr);
    kiz::Vm::assert_argc(2, args);
    auto value_obj = args->val[0];
    auto idx_int = dynamic_cast<Int*>(args->val[1]);
    if (!idx_int)
        throw NativeFuncError("TypeError", "The index argument of List.insert must be Int type");
    auto idx = idx_int->val.to_unsigned_long_long();

    // 值语义：存储前深拷贝可变值（防止自引用结构）
    auto new_val = model::copy_if_mutable(value_obj);
    new_val->make_ref(); // 列表持有新元素
    if (idx < self_list->val.size()) {
        // 插入语义：在idx位置前插入，其余元素后移
        self_list->val.insert(self_list->val.begin() + idx, new_val);
    } else {
        // 越界等价于追加到末尾
        self_list->val.push_back(new_val);
    }
    return load_nil();
}

Object* list_setitem(Object* self, const List* args) {
    kiz::Vm::assert_argc(2, args);
    auto self_list = dynamic_cast<List*>(self);

    auto idx_obj = dynamic_cast<Int*>(args->val[0]);
    if (!idx_obj)
        throw NativeFuncError("TypeError", "The first argument of List.setitem must be Int type");

    auto index = idx_obj->val.to_unsigned_long_long();

    auto value_obj = args->val[1];

    if (index < self_list->val.size()) {
        // 值语义：存储前深拷贝可变值（防止a[0] = a构造自引用结构）
        auto new_val = model::copy_if_mutable(value_obj);
        self_list->val[index]->del_ref(); // 释放被覆盖的旧元素
        new_val->make_ref(); // 列表持有新元素
        self_list->val[index] = new_val;
        return load_nil();
    }
    throw NativeFuncError("SetItemError", std::format("index {} out of range", index));
}

Object* list_getitem(Object* self, const List* args) {
    auto self_list = dynamic_cast<List*>(self);
    auto idx_obj = dynamic_cast<Int*>(builtin::get_one_arg(args));
    if (!idx_obj)
        throw NativeFuncError("TypeError", "The first argument of List.getitem must be Int type");

    auto index = idx_obj->val.to_unsigned_long_long();
    if (index < self_list->val.size()) {
        return self_list->val[index];
    }
    throw NativeFuncError("GetItemError", std::format("index {} out of range", index));
}

Object* list_count(Object* self, const List* args) {
    const auto obj = builtin::get_one_arg(args);
    size_t count = 0;
    auto self_list = cast_to_list(self);

    for (size_t idx = 0; idx < self_list->val.size(); ++idx) {
        kiz::Vm::call_method(obj, "__eq__", {self_list->val[idx]});
        auto res = kiz::Vm::simple_get_and_pop_stack_top(); // 移交引用
        bool matched = false;
        if (const auto res_bool = dynamic_cast<Bool*>(res)) {
            matched = res_bool->val; // 比较结果为Bool值，而非指针本身是否为空
        }
        res->del_ref(); // 归还引用
        if (matched) {
            ++ count;
        }
    }
    return new Int(count);
}

Object* list_find(Object* self, const List* args) {
    auto func_obj = builtin::get_one_arg(args);

    auto self_list = dynamic_cast<List*>(self);
    assert(self_list != nullptr);

    for (size_t idx = 0; idx < self_list->val.size(); ++idx) {
        kiz::Vm::call_function(func_obj, {self_list->val[idx]}, nullptr);
        auto res = kiz::Vm::simple_get_and_pop_stack_top(); // 移交引用
        bool matched = kiz::Vm::is_true(res);
        res->del_ref(); // 归还谓词结果的引用
        if (matched) {
            return self_list->val[idx]; // 返回命中的元素（列表持有，借用返回）
        }
    }
    return load_nil();
}

Object* list_map(Object* self, const List* args) {
    auto func_obj = builtin::get_one_arg(args);

    auto self_list = dynamic_cast<List*>(self);
    assert(self_list != nullptr);

    std::vector<Object*> new_vec;

    for (size_t idx = 0; idx < self_list->val.size(); ++idx) {
        kiz::Vm::call_function(func_obj, {self_list->val[idx]}, nullptr);
        auto res = kiz::Vm::simple_get_and_pop_stack_top(); // 移交引用

        new_vec.push_back(res);
    }
    auto new_list = new List(new_vec); // 构造内部为每个元素make_ref
    for (auto r : new_vec) {
        r->del_ref(); // 归还各回调结果从操作数栈移交来的引用
    }
    return new_list;
}

Object* list_filter(Object* self, const List* args) {
    auto func_obj = builtin::get_one_arg(args);

    auto self_list = dynamic_cast<List*>(self);
    assert(self_list != nullptr);

    std::vector<Object*> new_vec;

    for (size_t idx = 0; idx < self_list->val.size(); ++idx) {
        kiz::Vm::call_function(func_obj, {self_list->val[idx]}, nullptr);
        auto res = kiz::Vm::simple_get_and_pop_stack_top(); // 移交引用
        bool matched = kiz::Vm::is_true(res);
        res->del_ref(); // 归还谓词结果的引用
        if (matched) {
            new_vec.push_back(self_list->val[idx]); // 保留命中元素，List构造时统一持有
        }
    }
    return new List(new_vec);
}

Object* list_len(Object* self, const List* args) {
    auto self_list = dynamic_cast<List*>(self);
    assert(self_list != nullptr);
    return new Int(dep::BigInt(self_list->val.size()));
}

Object* list_join(Object* self, const List* args) {
    auto self_list = dynamic_cast<List*>(self);
    assert(self_list != nullptr);

    auto sep = kiz::Vm::obj_to_str(builtin::get_one_arg(args));

    std::string text;
    size_t index = 0;
    // 按下标动态读取：自定义__str__回调可能修改列表
    for (size_t idx = 0; idx < self_list->val.size(); ++idx) {
        text += kiz::Vm::obj_to_str(self_list->val[idx]);
        if (index != self_list->val.size()-1) {
            text += sep;
        }
        ++index;
    }
    return new String(text);
}

}  // namespace model