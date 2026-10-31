/**
 * @file builtin_functions.hpp
 * @brief 全局内置函数声明与ischild等通用工具
 * @author azhz1107cat
 * @date 2026-10-25
 */
#pragma once
#include <unordered_set>

#include "../../../src/models/models.hpp"

namespace builtin {

inline model::Object* get_one_arg(const model::List* args) {
    if (!args->val.empty()) {
        return args->val[0];
    }
    kiz::Vm::assert_argc(1, args);
}

inline model::Object* check_based_object_inner(
    model::Object* src_obj,
    model::Object* for_check_obj,
    std::unordered_set<model::Object*>& visited
) {
    if (src_obj == nullptr) return nullptr;
    // 闭环检测
    if (visited.contains(src_obj)) return model::load_false();
    visited.insert(src_obj);

    // 查找__parent__属性
    const auto it = src_obj->attrs.find("__parent__");
    if (it == nullptr) {
        return  model::load_false();
    }
    // 找到目标返回true，否则递归检查父对象
    if (it->value == for_check_obj) return  model::load_true();
    return check_based_object_inner(it->value, for_check_obj, visited);
}

// 对外接口
inline model::Object* check_based_object(model::Object* src_obj, model::Object* for_check_obj) {
    std::unordered_set<model::Object*> visited; // 每次调用新建集合
    return check_based_object_inner(src_obj, for_check_obj, visited);
}

// 内置函数
model::Object* print(model::Object* self, const model::List* args);
model::Object* input(model::Object* self, const model::List* args);
model::Object* ischild(model::Object* self, const model::List* args);
model::Object* help(model::Object* self, const model::List* args);
model::Object* breakpoint(model::Object* self, const model::List* args);
model::Object* range(model::Object* self, const model::List* args);
model::Object* cmd(model::Object* self, const model::List* args);
model::Object* now(model::Object* self, const model::List* args);
model::Object* setattr(model::Object* self, const model::List* args);
model::Object* getattr(model::Object* self, const model::List* args);
model::Object* delattr(model::Object* self, const model::List* args);
model::Object* hasattr(model::Object* self, const model::List* args);
model::Object* get_refc(model::Object* self, const model::List* args);
model::Object* create(model::Object* self, const model::List* args);
model::Object* type_of_obj(model::Object* self, const model::List* args);
model::Object* debug_str(model::Object* self, const model::List* args);
model::Object* attr(model::Object* self, const model::List* args);
model::Object* sleep(model::Object* self, const model::List* args);
model::Object* open(model::Object* self, const model::List* args);
model::Object* assert_(model::Object* self, const model::List* args);
model::Object* panic(model::Object* self, const model::List* args);
model::Object* repeat(model::Object* self, const model::List* args);
model::Object* len(model::Object* self, const model::List* args);
model::Object* chr(model::Object* self, const model::List* args);
model::Object* ord(model::Object* self, const model::List* args);

}