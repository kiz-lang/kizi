/**
 * @file nil_methods.cpp
 * @brief Nil类型的魔术方法实现
 * @author azhz1107cat
 * @date 2026-10-25
 */
#include "../../src/models/models.hpp"

namespace model {

// Nil.__eq__ 相等判断：仅当另一个对象也是Nil时返回true
Object* nil_eq(Object* self, const List* args) {
    kiz::Vm::assert_argc(1, args);
    // Nil仅与自身相等
    auto another_nil = dynamic_cast<Nil*>(args->val[0]);
    return load_bool(another_nil != nullptr);
}

// Nil.__hash__
Object* nil_hash(Object* self, const List* args) {
    // 哈希带类型前缀：防止Nil与Int 0混同为同一字典键
    return new Int(dep::BigInt(dep::hash_string("Nil")));
}

Object* nil_str(Object* self, const List* args) {
    return new model::String("Nil");
}

}  // namespace model