/**
 * @file models.hpp
 * @brief 虚拟机对象模型（VM Models）核心定义
 * @author azhz1107cat
 * @date 2025-10-25
 */

#pragma once

#include <format>
#include <fstream>
#include <functional>
#include <iomanip>
#include <ranges>
#include <utility>

#include "../kiz.hpp"
#include "../vm/vm.hpp"
#include "../../depends/hashmap.hpp"
#include "../../depends/bigint.hpp"
#include "../../depends/decimal.hpp"
#include "../../depends/dict.hpp"


namespace model {

namespace magic_name {
    constexpr auto add = "__add__";
    constexpr auto sub = "__sub__";
    constexpr auto mul = "__mul__";
    constexpr auto div = "__div__";
    constexpr auto pow = "__pow__";
    constexpr auto mod = "__mod__";
    constexpr auto eq = "__eq__";
    constexpr auto lt = "__lt__";
    constexpr auto gt = "__gt__";

    constexpr auto parent = "__parent__";
    constexpr auto call = "__call__";
    constexpr auto bool_of = "__bool__";
    constexpr auto str = "__str__";
    constexpr auto debug_str = "__dstr__";
    constexpr auto getitem = "__getitem__";
    constexpr auto setitem = "__setitem__";
    constexpr auto contains = "contains";  // contains不是魔术方法
    constexpr auto next_item = "__next__";
    constexpr auto hash = "__hash__";
}

// 工具函数ptr转为地址的字符串
template <typename T>
std::string ptr_to_string(T* m) {
    // 将指针转为 uintptr_t
    auto ptr_val = reinterpret_cast<uintptr_t>(m);

    // 格式化字符串
    std::stringstream ss;
    ss << "0x" << std::hex << std::setfill('0')
        << std::setw(sizeof(uintptr_t) * 2)
        << ptr_val;

    return ss.str();
}

struct UpValue {
    size_t distance_from_curr; // call_stack[call_stack.size() - distance_from_curr - 1]
    size_t idx;
};


struct ExceptionTable {
    size_t try_part_start_pc;
    size_t try_part_end_pc;
    dep::HashMap<size_t> handle_pc;
    size_t mismatch_pc;
};

class Object {
    size_t refc_ = 0;
    bool is_important = false; // 重要对象不参与make_refc/del_refc
public:
    dep::HashMap<Object*> attrs;

    // 对象类型枚举
    enum class ObjectType {
        Object, Nil, Bool, Int, String, Decimal,
        List, Dictionary, CodeObject, Function,
        NativeFunction, Module, Error, Unpack, Cell
    };

    void mark_as_important() {
        is_important = true;
    }

    // 获取实际类型的虚函数
    [[nodiscard]] virtual ObjectType get_type() const {
        return ObjectType::Object;
    }

    [[nodiscard]] size_t get_refc_() const {
        return refc_;
    }
    
    void make_ref() {
        if (is_important) return;
        refc_ += 1;
    }
    void del_ref() {
        if (is_important) return;
        // 引用计数不为正说明存在重复释放，直接暴露而不是静默吞掉
        assert(refc_ > 0);
        if (refc_ == 0) return;
        refc_ -= 1;
        if (refc_ == 0) {
            // std::cout << "deling object " << this->debug_string() << std::endl;
            delete this;
        }
    }

    void attrs_insert(const std::string& name, Object* o) {
        assert(o != nullptr);
        // insert返回被覆盖的旧值，插入后释放旧值的引用（一次哈希查找完成）
        o->make_ref();
        auto old_val = attrs.insert(name, o);
        if (old_val) old_val->del_ref();
    }

    [[nodiscard]] virtual std::string debug_string() const {
        return "<Object at " + ptr_to_string(this) + ">";
    }

    Object () = default;

    virtual ~Object() {
        // 直接遍历桶释放属性引用，避免to_vector的堆分配
        attrs.for_each_value([](Object* obj) {
            if (obj) obj->del_ref();
        });
    }
};

inline auto based_based_obj = new Object();
inline auto based_obj = new Object();
inline auto based_list = new Object();
inline auto based_function = new Object();
inline auto based_dict = new Object();
inline auto based_int = new Object();
inline auto based_bool = new Object();
inline auto based_str = new Object();
inline auto based_native_function = new Object();
inline auto based_error = new Object();
inline auto based_decimal = new Object();
inline auto based_module = new Object();
inline auto based_code_object = new Object();
inline auto based_file_handle = new Object();
inline auto based_range = new Object();
inline auto based_unpack = new Object();
inline auto stop_iter_signal = new Object();

class List;

class CodeObject : public Object {
public:
    std::vector<kiz::Instruction> code;

    std::vector<std::string> var_names;
    std::vector<std::string> attr_names;
    std::vector<std::string> free_names;

    std::vector<UpValue> upvalues;
    size_t locals_count;

    std::vector<ExceptionTable> exception_tables;
    // 本CodeObject在chunk栈中的词法层级（module=0，直接嵌套+1）；CREATE_CLOSURE按此回溯词法父帧
    size_t scope_depth = 0;

    static constexpr ObjectType TYPE = ObjectType::CodeObject;
    [[nodiscard]] ObjectType get_type() const override { return TYPE; }

    explicit CodeObject(const std::vector<kiz::Instruction>& c,
        const std::vector<std::string>& v_n,
        const std::vector<std::string>& a_n,
        const std::vector<std::string>& f_n,
        const std::vector<UpValue>& u_v,
        const size_t l_c,
        std::vector<ExceptionTable> et,
        const size_t s_d = 0)
            : code(c), var_names(v_n), attr_names(a_n), free_names(f_n), upvalues(u_v), locals_count(l_c),
                 exception_tables(std::move(et)), scope_depth(s_d) {}

    [[nodiscard]] std::string debug_string() const override {
        return "<CodeObject at " + ptr_to_string(this) + ">";
    }
};

class Module : public Object {
public:
    std::string path;
    CodeObject* code = nullptr;

    static constexpr ObjectType TYPE = ObjectType::Module;
    [[nodiscard]] ObjectType get_type() const override { return TYPE; }

    explicit Module(std::string name, CodeObject *code) : path(std::move(name)), code(code) {
        attrs_insert("__parent__", based_module);
        code->make_ref();
    }

    explicit Module(std::string name) : path(std::move(name)) {
        attrs_insert("__parent__", based_module);
    }

    [[nodiscard]] std::string debug_string() const override {
        return "<Module: path='" + path + "', attr=" + attrs.to_string() + ", at " + ptr_to_string(this) + ">";
    }

    ~Module() override {
        // 标准库模块不携带code，析构时需判空
        if (code) code->del_ref();
    }
};

class Function : public Object {
public:
    std::string name;
    CodeObject* code = nullptr;
    size_t argc = 0;
    bool has_rest_params = false;
    std::vector<Object*> free_vars;

    static constexpr ObjectType TYPE = ObjectType::Function;
    [[nodiscard]] ObjectType get_type() const override { return TYPE; }

    explicit Function(std::string name, CodeObject *code, const size_t argc
    ) : name(std::move(name)), code(code), argc(argc) {
        code->make_ref();
        attrs_insert("__parent__", based_function);
    }

    [[nodiscard]] std::string debug_string() const override {
        return "<Function: path='" + name + "', argc=" + std::to_string(argc) + " at " + ptr_to_string(this) + ">";
    }

    ~Function() override {
        code->del_ref();
        for (auto fv : free_vars) {
            if (fv) fv->del_ref();
        }
    }
};

class NativeFunction : public Object {
public:
    std::string name;
    std::function<Object*(Object*, List*)> func;

    static constexpr ObjectType TYPE = ObjectType::NativeFunction;
    [[nodiscard]] ObjectType get_type() const override { return TYPE; }

    explicit NativeFunction(std::function<Object*(Object*, List*)> func) : func(std::move(func)) {
        attrs_insert("__parent__", based_native_function);
    }
    [[nodiscard]] std::string debug_string() const override {
    return "<NativeFunction" +
           (name.empty() 
            ? "" 
            : ": name='" + name + "'"
            ) 
            + " at " + ptr_to_string(this) + ">";
}
};

class Int : public Object {
public:
    dep::BigInt val;

    static constexpr ObjectType TYPE = ObjectType::Int;
    [[nodiscard]] ObjectType get_type() const override { return TYPE; }

    explicit Int(dep::BigInt val) : val(std::move(val)) {
        attrs_insert("__parent__", based_int);
    }
    explicit Int() : val(dep::BigInt(0)) {
        attrs_insert("__parent__", based_int);
    }
    [[nodiscard]] std::string debug_string() const override {
        return val.to_string();
    }
};

class List : public Object {
public:
    std::vector<Object*> val;

    static constexpr ObjectType TYPE = ObjectType::List;
    [[nodiscard]] ObjectType get_type() const override { return TYPE; }

    explicit List(const std::vector<Object*>& val_){
        for (auto v: val_) {
            v->make_ref();
            val.push_back(v);
        }
        attrs_insert("__parent__", based_list);
        auto zero = kiz::Vm::small_int_pool[0];
        attrs_insert("__current_index__", zero);
    }
    [[nodiscard]] std::string debug_string() const override {
        std::string result = "[";
        for (size_t i = 0; i < val.size(); ++i) {
            if (val[i] != nullptr) {
                result += val[i]->debug_string();  // 递归调用元素的 to_string
            } else {
                result += "Nil";
            }
            if (i != val.size() - 1) {
                result += ", ";
            }
        }
        result += "]";
        return result;
    }

    ~List() override {
        for (auto elem : val) {
            if (elem) elem->del_ref();
        }
    }
};

class Decimal : public Object {
public:
    dep::Decimal val;
    static constexpr ObjectType TYPE = ObjectType::Decimal;
    [[nodiscard]] ObjectType get_type() const override { return TYPE; }
    explicit Decimal(dep::Decimal val) : val(std::move(val)) {
        attrs_insert("__parent__", based_decimal);
    }
    [[nodiscard]] std::string debug_string() const override {
        return val.to_string();
    }
};

class String : public Object {
public:
    std::string val;

    static constexpr ObjectType TYPE = ObjectType::String;
    [[nodiscard]] ObjectType get_type() const override { return TYPE; }

    explicit String(std::string val) : val(std::move(val)) {
        attrs_insert("__parent__", based_str);
        auto zero = kiz::Vm::small_int_pool[0];
        attrs_insert("__current_index__", zero);
    }
    [[nodiscard]] std::string debug_string() const override {
        return '"'+val+'"';
    }
};

class Dictionary : public Object {
public:
    dep::Dict<std::pair<Object*, Object*>> val;
    static constexpr ObjectType TYPE = ObjectType::Dictionary;
    [[nodiscard]] ObjectType get_type() const override { return TYPE; }

    explicit Dictionary(dep::Dict<std::pair<Object*, Object*>> val_) : val(std::move(val_)) {
        // 移动完成后遍历自身存储，为每个键值对补上字典持有的引用
        auto kv_list = val.to_vector();
        for (auto& kv_pair : kv_list | std::views::values) {
            if (kv_pair.first) kv_pair.first->make_ref();
            if (kv_pair.second) kv_pair.second->make_ref();
        }
        attrs_insert("__parent__", based_dict);
        auto zero = kiz::Vm::small_int_pool[0];
        attrs_insert("__current_index__", zero);
    }
    explicit Dictionary() {
        attrs_insert("__parent__", based_dict);
        auto zero = kiz::Vm::small_int_pool[0];
        attrs_insert("__current_index__", zero);
    }

    [[nodiscard]] std::string debug_string() const override {
        std::string result = "{";
        auto kv_list = val.to_vector();
        size_t i = 0;
        for (auto& kv_pair : kv_list | std::views::values) {
            result += kv_pair.first->debug_string() + ": " + kv_pair.second->debug_string();
            if (i != kv_list.size() - 1) {
                result += ", ";
            }
            ++i;
        }
        result += "}";
        return result;
    }

    ~Dictionary() override {
        // 直接遍历桶释放键值引用，避免to_vector的堆分配
        val.for_each_value([](std::pair<Object*, Object*>& kv_pair) {
            if (kv_pair.first) kv_pair.first->del_ref();
            if (kv_pair.second) kv_pair.second->del_ref();
        });
    }
};

class Bool : public Object {
public:
    bool val;

    static constexpr ObjectType TYPE = ObjectType::Bool;
    [[nodiscard]] ObjectType get_type() const override { return TYPE; }

    explicit Bool(const bool val) : val(val) {
        attrs_insert("__parent__", based_bool);
    }
    [[nodiscard]] std::string debug_string() const override {
        return val ? "True" : "False";
    }
};

class Nil : public Object {
public:

    static constexpr ObjectType TYPE = ObjectType::Nil;
    [[nodiscard]] ObjectType get_type() const override { return TYPE; }

    explicit Nil() : Object() {}
    [[nodiscard]] std::string debug_string() const override {
        return "Nil";
    }
};

class Error : public Object {
public:
    std::vector<std::pair<std::string, err::PositionInfo>> positions;
    static constexpr ObjectType TYPE = ObjectType::Error;
    [[nodiscard]] ObjectType get_type() const override { return TYPE; }

    explicit Error(std::vector<std::pair<std::string, err::PositionInfo>> p) {
        positions = std::move(p);
        attrs_insert("__parent__", based_error);
    }

    explicit Error() {
        attrs_insert("__parent__", based_error);
    }

    [[nodiscard]] std::string debug_string() const override {
        return "Error";
    }
};

class FileHandle : public Object {
public:
    std::fstream* file_handle = nullptr;
    bool is_closed = false;
    size_t readline_cursor = 0; // readline顺序读的游标（已读到的行号）
    explicit FileHandle() {
        attrs_insert("__parent__", based_file_handle);
    }
    ~FileHandle() override {
        is_closed = true;
        if (file_handle) {
            file_handle->close();
            delete file_handle;
        }
    }
    [[nodiscard]] std::string debug_string() const override {
        return "fileHandle";
    }

};

class Cell : public Object {
public:
    Object* val;

    static constexpr ObjectType TYPE = ObjectType::Cell;
    [[nodiscard]] ObjectType get_type() const override { return TYPE; }

    // 接管调用方持有的引用（不make_ref）；val可为nullptr表示未赋值
    explicit Cell(Object* adopted) : val(adopted) {}
    [[nodiscard]] std::string debug_string() const override {
        return "<Cell of " + (val ? val->debug_string() : "Nil") + ">";
    }
    // 释放旧值引用并接管新值引用
    void set(Object* adopted) {
        if (val) val->del_ref();
        val = adopted;
    }
    ~Cell() override {
        if (val) val->del_ref();
    }
};

class Unpack : public Object {
public:
    Object* val;

    static constexpr ObjectType TYPE = ObjectType::Unpack;
    [[nodiscard]] ObjectType get_type() const override { return TYPE; }

    explicit Unpack(Object* val_) : val(val_) {
        val->make_ref();
        attrs_insert("__parent__", based_unpack);
    }
    [[nodiscard]] std::string debug_string() const override {
        return "unpack";
    }
    ~Unpack() override {
        val->del_ref();
    }
};

inline auto unique_nil = new Nil();
inline auto unique_false = new Bool(false);
inline auto unique_true = new Bool(true);

inline auto load_nil() {
    return unique_nil;
}
inline auto load_false() {
    return unique_false;
}
inline auto load_true() {
    return unique_true;
}

inline auto load_bool(bool b) {
    if (b) { return load_true(); }
    return load_false();
}

inline auto load_stop_iter_signal() {
    stop_iter_signal->make_ref();
    return stop_iter_signal;
}

inline auto create_nfunc(const std::function<Object*(Object*, List*)>& func, const std::string& name="<unnamed>") {
    auto o = new NativeFunction(func);
    o->name = name;
    return o;
}


inline auto cast_to_int(Object* o) {
    auto obj = dynamic_cast<Int*>(o);
    if (!obj)
        throw NativeFuncError("TypeError", std::format(
            "fail to cast {} to Int", kiz::Vm::obj_to_debug_str(o)));
    return obj;
}

inline auto cast_to_str(Object* o) {
    auto obj = dynamic_cast<String*>(o);
    if (!obj)
        throw NativeFuncError("TypeError", std::format(
            "fail to cast {} to Str", kiz::Vm::obj_to_debug_str(o)));
    return obj;
}

inline auto cast_to_bool(Object* o) {
    auto obj = dynamic_cast<Bool*>(o);
    if (!obj)
        throw NativeFuncError("TypeError", std::format(
            "fail to cast {} to Bool", kiz::Vm::obj_to_debug_str(o)));    return obj;
}

inline auto cast_to_list(Object* o) {
    auto obj = dynamic_cast<List*>(o);
    if (!obj)
        throw NativeFuncError("TypeError", std::format(
            "fail to cast {} to List", kiz::Vm::obj_to_debug_str(o)));
    return obj;
}

inline auto copy_if_mutable(Object* obj) -> Object* {
    // 统一返回“借用指针”：不可变对象原样返回（引用计数不变），可变对象返回全新拷贝(refc=0)
    // 持有责任由调用方的存储点自行make_ref承担
    switch (obj->get_type()) {

    case Object::ObjectType::List: {
        std::vector<Object*> new_val;
        for (auto val : cast_to_list(obj)->val) {
            new_val.push_back(copy_if_mutable(val));
        }
        return new List(new_val);
    }

    case Object::ObjectType::Dictionary: {
        std::vector<std::pair<
            dep::BigInt, std::pair< Object*, Object* >
        >> elem_list;
        auto dict_obj = dynamic_cast<Dictionary*>(obj);
        assert(dict_obj != nullptr);
        for (auto& [_, kv_pair] : dict_obj->val.to_vector()) {
            // key是hashable value, 也就是不可变对象, 可以引用传递, 应该没有神人为可变对象重载__hash__方法的
            elem_list.emplace_back(_, std::pair{
                kv_pair.first, copy_if_mutable(kv_pair.second)
            });
        }
        auto new_dict_obj = new Dictionary(dep::Dict(elem_list));
        return new_dict_obj;
    }

    default: {
        return obj;
    }

    }
}

};