/**
 * @file str_methods.cpp
 * @brief String类型的魔术方法与成员方法实现
 * @author azhz1107cat
 * @date 2026-10-25
 */
#include "../depends/u8str.hpp"
#include "../../src/models/models.hpp"
#include "include/builtin_functions.hpp"

namespace model {

// String.__call__
Object* str_call(Object* self, const List* args) {
    std::string val;
    if (args->val.empty()) {
        val = "";
    } else {
        val = kiz::Vm::obj_to_str(args->val[0]);
    }
    return new String(val);
}

// String.__bool__
Object* str_bool(Object* self, const List* args) {
    const auto self_int = dynamic_cast<String*>(self);
    if (self_int->val.empty()) return load_false();
    return load_true();
}

// String.__add__：字符串拼接（self + 传入String，返回新String，不修改原对象）
Object* str_add(Object* self, const List* args) {
    kiz::Vm::assert_argc(1, args);
    auto self_str = dynamic_cast<String*>(self);
    assert(self_str != nullptr);
    
    auto another_str = dynamic_cast<String*>(args->val[0]);
    if (!another_str)
        throw NativeFuncError("TypeError", "String.add only supports String type argument");
    
    // 拼接并返回新String
    return new String(self_str->val + another_str->val);
};

// String.__mul__：字符串重复n次（self * n，返回新String，n为非负整数）
Object* str_mul(Object* self, const List* args) {
    kiz::Vm::assert_argc(1, args);
    auto self_str = dynamic_cast<String*>(self);
    assert(self_str != nullptr);
    
    auto times_int = dynamic_cast<Int*>(args->val[0]);
    if (!times_int)
        throw NativeFuncError("TypeError","String.mul only supports Int type argument");
    if(times_int->val < dep::BigInt(0))
        throw NativeFuncError("TypeError", "String.mul requires non-negative integer argument");
    
    std::string result;
    dep::BigInt times = times_int->val;
    for (dep::BigInt i = dep::BigInt(0); i < times; i+=dep::BigInt(1)) {
        result += self_str->val;
    }
    
    return new String(std::move(result));
};

// String.__eq__：判断两个字符串是否相等 self == x
Object* str_eq(Object* self, const List* args) {
    kiz::Vm::assert_argc(1, args);
    
    auto self_str = dynamic_cast<String*>(self);
    assert(self_str != nullptr);
    
    auto another_str = dynamic_cast<String*>(args->val[0]);
    if (! another_str)
        throw NativeFuncError("TypeError","String.eq only supports String type argument");
    
    return load_bool(self_str->val == another_str->val);
};

// String.__lt__：字典序小于（补齐字符串的大小比较能力）
Object* str_lt(Object* self, const List* args) {
    kiz::Vm::assert_argc(1, args);

    auto self_str = dynamic_cast<String*>(self);
    assert(self_str != nullptr);

    auto another_str = dynamic_cast<String*>(args->val[0]);
    if (! another_str)
        throw NativeFuncError("TypeError","String.lt only supports String type argument");

    return load_bool(self_str->val < another_str->val);
};

// String.__gt__：字典序大于
Object* str_gt(Object* self, const List* args) {
    kiz::Vm::assert_argc(1, args);

    auto self_str = dynamic_cast<String*>(self);
    assert(self_str != nullptr);

    auto another_str = dynamic_cast<String*>(args->val[0]);
    if (! another_str)
        throw NativeFuncError("TypeError","String.gt only supports String type argument");

    return load_bool(self_str->val > another_str->val);
};

// String.find：查找子串首个出现的码点下标，未找到返回-1
Object* str_find(Object* self, const List* args) {
    kiz::Vm::assert_argc(1, args);

    auto self_str = cast_to_str(self);
    auto target = dynamic_cast<String*>(args->val[0]);
    if (! target)
        throw NativeFuncError("TypeError","String.find only supports String type argument");

    dep::UTF8String source(self_str->val);
    dep::UTF8String needle(target->val);
    if (needle.size() == 0) return new Int(0);
    if (needle.size() > source.size()) return new Int(-1);

    for (size_t i = 0; i + needle.size() <= source.size(); ++i) {
        bool matched = true;
        for (size_t j = 0; j < needle.size(); ++j) {
            if (!(source[i + j] == needle[j])) {
                matched = false;
                break;
            }
        }
        if (matched) return new Int(i);
    }
    return new Int(-1);
};

// String.__contains__：判断是否包含子字符串 x in self
Object* str_contains(Object* self, const List* args) {
    kiz::Vm::assert_argc(1, args);
    
    auto self_str = dynamic_cast<String*>(self);
    assert(self_str != nullptr);
    
    auto sub_str = dynamic_cast<String*>(args->val[0]);
    if(! sub_str)
        throw NativeFuncError("TypeError", "String.contains only supports String type argument");
    
    bool exists = self_str->val.find(sub_str->val) != std::string::npos;
    return load_bool(exists);
};

// String.__hash__
Object* str_hash(Object* self, const List* args) {
    auto self_str = dynamic_cast<String*>(self);
    assert(self_str != nullptr);
    // 哈希带类型前缀：防止"123"与Int 123等跨类型哈希撞车
    auto hashed_str = dep::hash_string("Str:" + self_str->val);
    return new Int(dep::BigInt(hashed_str));
}

Object* str_next(Object* self, const List* args) {
    auto curr_idx_it = self->attrs.find("__current_index__");
    assert(curr_idx_it != nullptr);

    auto curr_idx = curr_idx_it->value;

    auto index = cast_to_int(curr_idx) ->val.to_unsigned_long_long();

    auto self_str = dynamic_cast<String*>(self);
    assert(self_str != nullptr);

    if (index < self_str->val.size()) {
        auto res = dep::UTF8String(self_str->val)[index];
        self->attrs_insert("__current_index__", new Int(index+1));
        return new String(res.to_string());
    }
    self->attrs_insert("__current_index__", kiz::Vm::small_int_pool[0]);
    return load_stop_iter_signal();
}

Object* str_str(Object* self, const List* args) {
    auto self_str = dynamic_cast<String*>(self);
    assert(self_str != nullptr);
    return new String(self_str->val);
}

Object* str_dstr(Object* self, const List* args) {
    auto self_str = dynamic_cast<String*>(self);
    assert(self_str != nullptr);
    return new String("\"" + self_str->val + "\"");
}

Object* str_getitem(Object* self, const List* args) {
    auto self_str = dynamic_cast<String*>(self);
    auto idx_obj = cast_to_int(builtin::get_one_arg(args));
    auto index = idx_obj->val.to_unsigned_long_long();
    auto text = dep::UTF8String(self_str->val);

    if (index >= text.size()) {
        throw NativeFuncError("GetItemError", std::format("index {} out of range", index));
    }
    return new String( text[index] .to_string() );
}

Object* str_foreach(Object* self, const List* args) {
    const auto func_obj = builtin::get_one_arg(args);

    auto self_str = cast_to_str(self);

    dep::BigInt idx = 0;
    for (const auto& e : dep::UTF8String(self_str->val)) {
        kiz::Vm::call_function(func_obj, {
            new String(e.to_string())
        }, nullptr);
        kiz::Vm::get_and_pop_stack_top(); // 丢弃并释放回调返回值
        idx += 1;
    }
    return load_nil();
}

Object* str_count(Object* self, const List* args) {
    const auto obj = builtin::get_one_arg(args);
    size_t count = 0;
    auto self_str = cast_to_str(self);

    for (const auto& c : dep::UTF8String(self_str->val)) {
        kiz::Vm::call_method(obj, "__eq__", {
            new String(c.to_string())
        });
        auto res = kiz::Vm::get_and_pop_stack_top();
        if (const auto res_bool = dynamic_cast<Bool*>(res.get())) {
            if (res_bool->val) { // 比较结果为Bool值，而非指针本身是否为空
                ++ count;
            }
        }
    }
    return new Int(count);
}


Object* str_startswith(Object* self, const List* args) {
    auto self_str = cast_to_str(self)->val;
    auto prefix = cast_to_str(
        builtin::get_one_arg(args)
    ) -> val;

    if (prefix.empty()) {
        return load_true();
    }
    if (prefix.size() > self_str.size()) {
        return load_false();
    }
    // 比较原字符串的前prefix.size()个字符与前缀是否相等
    return load_bool(self_str.compare(0, prefix.size(), prefix) == 0);
}

Object* str_endswith(Object* self, const List* args) {
    auto self_str = cast_to_str(self)->val;
    auto suffix = cast_to_str(
        builtin::get_one_arg(args)
    ) -> val;

    if (suffix.empty()) {
        return load_true();
    }
    if (suffix.size() > self_str.size()) {
        return load_false();
    }

    size_t start_pos = self_str.size() - suffix.size();
    return load_bool(self_str.compare(start_pos, suffix.size(), suffix) == 0);

}

Object* str_len(Object* self, const List* args) {
    auto self_str = cast_to_str(self);

    return new Int(dep::UTF8String(self_str->val).size());
}

Object* str_is_alpha(Object* self, const List* args) {
    auto self_str = cast_to_str(self);
    auto str = dep::UTF8String(self_str->val);
    if (str.empty()) return load_false(); // 空串按约定不为纯字母
    bool is_alpha = true;
    for (const auto& c : str) {
        if (!c.is_alpha()) is_alpha = false;
    }
    return load_bool(is_alpha);
}

Object* str_is_digit(Object* self, const List* args) {
    auto self_str = cast_to_str(self);
    auto str = dep::UTF8String(self_str->val);
    if (str.empty()) return load_false(); // 空串按约定不为纯数字
    bool is_digit = true;
    for (const auto& c : str) {
        if (!c.is_digit()) is_digit = false;
    }
    return load_bool(is_digit);

}

Object* str_substr(Object* self, const List* args) {
    auto self_str = cast_to_str(self);
    auto args_vec = args->val;
    kiz::Vm::assert_argc({1,2}, args);

    size_t pos = cast_to_int(args_vec[0])->val.to_unsigned_long_long();
    auto source = dep::UTF8String(self_str->val);
    size_t len = source.size() > pos ? source.size() - pos : 0; // 默认取到末尾
    if (args_vec.size() > 1) {
        len = cast_to_int(args_vec[1])->val.to_unsigned_long_long();
    }

    return new String(source.substr(pos, len).to_string());
}

Object* str_to_lower(Object* self, const List* args) {
    auto self_str = cast_to_str(self);

    return new String(dep::UTF8String(self_str->val).to_lower().to_string());
}

Object* str_to_upper(Object* self, const List* args) {
    auto self_str = cast_to_str(self);

    return new String(dep::UTF8String(self_str->val).to_upper().to_string());

}

Object* str_format(Object* self, const List* args) {
    auto format_str = cast_to_str(self)->val;
    std::vector<std::string> str_vec;
    for (auto item : args->val) {
        str_vec.push_back(kiz::Vm::obj_to_str(item));
    }

    std::string result;
    size_t arg_index = 0;  // 用于遍历str_vec的索引
    size_t pos = 0;        // 用于遍历format_str的位置

    // 遍历格式化字符串，查找{}占位符并替换
    while (pos < format_str.size()) {
        // 查找下一个{}的起始位置
        size_t brace_pos = format_str.find("{}", pos);
        if (brace_pos == std::string::npos) {
            // 没有找到更多占位符，将剩余字符串追加到结果
            result += format_str.substr(pos);
            break;
        }

        // 将{}之前的普通字符串追加到结果
        result += format_str.substr(pos, brace_pos - pos);

        // 替换占位符：如果str_vec还有元素则替换，否则保留{}
        if (arg_index < str_vec.size()) {
            result += str_vec[arg_index];
            arg_index++;
        } else {
            result += "{}";  // 参数不足时保留占位符
        }

        // 移动到{}之后的位置继续遍历
        pos = brace_pos + 2;
    }

    return new String(result);
}


}  // namespace model