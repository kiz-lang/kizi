/**
 * @file entry_builtins.cpp
 * @brief 内置类型方法与全局内置函数的注册入口
 * @author azhz1107cat
 * @date 2026-10-25
 */
#include <format>

#include "vm.hpp"
#include "../models/models.hpp"
#include "builtins/include/builtin_functions.hpp"
#include "builtins/include/builtin_methods.hpp"

namespace kiz {

void Vm::entry_builtins() {
    // 幂等保护：多Vm实例(测试循环等)重复注册会导致builtins向量线性膨胀与泄漏
    if (!builtins.empty()) return;

    model::based_bool->attrs_insert("__parent__", model::based_obj);
    model::based_int->attrs_insert("__parent__", model::based_obj);
    model::unique_nil->attrs_insert("__parent__", model::based_obj);
    model::based_function->attrs_insert("__parent__", model::based_obj);
    model::based_decimal->attrs_insert("__parent__", model::based_obj);
    model::based_module->attrs_insert("__parent__", model::based_obj);
    model::based_dict->attrs_insert("__parent__", model::based_obj);
    model::based_list->attrs_insert("__parent__", model::based_obj);
    model::based_native_function->attrs_insert("__parent__", model::based_obj);
    model::based_error->attrs_insert("__parent__", model::based_obj);
    model::based_str->attrs_insert("__parent__", model::based_obj);
    model::stop_iter_signal->attrs_insert("__parent__", model::based_obj);
    model::based_code_object->attrs_insert("__parent__", model::based_obj);
    model::based_file_handle->attrs_insert("__parent__", model::based_obj);
    model::based_range->attrs_insert("__parent__", model::based_obj);
    model::based_unpack->attrs_insert("__parent__", model::based_obj);

    // Object 基类 方法
    model::based_obj->attrs_insert("__parent__", model::based_based_obj);
    model::based_based_obj->attrs_insert("__eq__", model::create_nfunc(model::object_eq));
    model::based_based_obj->attrs_insert("__str__", model::create_nfunc(model::object_str));
    // 缺省真值：任意对象默认为真
    model::based_based_obj->attrs_insert("__bool__", model::create_nfunc(model::object_bool));
    model::based_based_obj->attrs_insert("__getitem__", model::create_nfunc(model::object_getitem));
    model::based_based_obj->attrs_insert("__setitem__", model::create_nfunc(model::object_setitem));

    // Bool 类型魔法方法
    model::based_bool->attrs_insert("__eq__", model::create_nfunc(model::bool_eq));
    model::based_bool->attrs_insert("__call__", model::create_nfunc(model::bool_call));
    model::based_bool->attrs_insert("__hash__", model::create_nfunc(model::bool_hash));
    model::based_bool->attrs_insert("__str__", model::create_nfunc(model::bool_str));

    // Nil 类型魔法方法
    model::unique_nil->attrs_insert("__eq__", model::create_nfunc(model::nil_eq));
    model::unique_nil->attrs_insert("__hash__", model::create_nfunc(model::nil_hash));
    model::unique_nil->attrs_insert("__str__", model::create_nfunc(model::nil_str));

    // Int 类型魔法方法
    model::based_int->attrs_insert("__add__", model::create_nfunc(model::int_add));
    model::based_int->attrs_insert("__sub__", model::create_nfunc(model::int_sub));
    model::based_int->attrs_insert("__mul__", model::create_nfunc(model::int_mul));
    model::based_int->attrs_insert("__div__", model::create_nfunc(model::int_div));
    model::based_int->attrs_insert("__mod__", model::create_nfunc(model::int_mod));
    model::based_int->attrs_insert("__pow__", model::create_nfunc(model::int_pow));
    model::based_int->attrs_insert("__neg__", model::create_nfunc(model::int_neg));
    model::based_int->attrs_insert("__gt__", model::create_nfunc(model::int_gt));
    model::based_int->attrs_insert("__lt__", model::create_nfunc(model::int_lt));
    model::based_int->attrs_insert("__eq__", model::create_nfunc(model::int_eq));
    model::based_int->attrs_insert("__call__", model::create_nfunc(model::int_call));
    model::based_int->attrs_insert("__bool__", model::create_nfunc(model::int_bool));
    model::based_int->attrs_insert("__hash__", model::create_nfunc(model::int_hash));
    model::based_int->attrs_insert("__str__", model::create_nfunc(model::int_str));

    // Decimal类型魔术方法
    model::based_decimal->attrs_insert("__add__", model::create_nfunc(model::decimal_add));
    model::based_decimal->attrs_insert("__sub__", model::create_nfunc(model::decimal_sub));
    model::based_decimal->attrs_insert("__mul__", model::create_nfunc(model::decimal_mul));
    model::based_decimal->attrs_insert("__div__", model::create_nfunc(model::decimal_div));
    model::based_decimal->attrs_insert("__mod__", model::create_nfunc(model::decimal_mod));
    model::based_decimal->attrs_insert("__pow__", model::create_nfunc(model::decimal_pow));
    model::based_decimal->attrs_insert("__neg__", model::create_nfunc(model::decimal_neg));
    model::based_decimal->attrs_insert("__gt__", model::create_nfunc(model::decimal_gt));
    model::based_decimal->attrs_insert("__lt__", model::create_nfunc(model::decimal_lt));
    model::based_decimal->attrs_insert("__eq__", model::create_nfunc(model::decimal_eq));
    model::based_decimal->attrs_insert("__call__", model::create_nfunc(model::decimal_call));
    model::based_decimal->attrs_insert("__bool__", model::create_nfunc(model::decimal_bool));
    model::based_decimal->attrs_insert("__hash__", model::create_nfunc(model::decimal_hash));
    model::based_decimal->attrs_insert("__str__", model::create_nfunc(model::decimal_str));
    model::based_decimal->attrs_insert("limit_div", model::create_nfunc(model::decimal_limit_div));
    model::based_decimal->attrs_insert("round_div", model::create_nfunc(model::decimal_round_div));
    model::based_decimal->attrs_insert("approx", model::create_nfunc(model::decimal_approx));

    // Dictionary 类型魔法方法
    model::based_dict->attrs_insert("__add__", model::create_nfunc(model::dict_add));
    // 注册名与OP_IN调用的方法名统一为contains（此前注册成__contains__导致 in 对Dict不可用）
    model::based_dict->attrs_insert("contains", model::create_nfunc(model::dict_contains));
    model::based_dict->attrs_insert("__getitem__", model::create_nfunc(model::dict_getitem));
    model::based_dict->attrs_insert("__str__", model::create_nfunc(model::dict_str));
    model::based_dict->attrs_insert("__dstr__", model::create_nfunc(model::dict_dstr));
    model::based_dict->attrs_insert("__setitem__", model::create_nfunc(model::dict_setitem));
    model::based_dict->attrs_insert("__next__", model::create_nfunc(model::dict_next));
    model::based_dict->attrs_insert("foreach", model::create_nfunc(model::dict_foreach));
    model::based_dict->attrs_insert("len", model::create_nfunc(model::dict_len));

    // List 类型魔法方法
    model::based_list->attrs_insert("__add__", model::create_nfunc(model::list_add));
    model::based_list->attrs_insert("__mul__", model::create_nfunc(model::list_mul));
    model::based_list->attrs_insert("__eq__", model::create_nfunc(model::list_eq));
    model::based_list->attrs_insert("__call__", model::create_nfunc(model::list_call));
    model::based_list->attrs_insert("__bool__", model::create_nfunc(model::list_bool));
    model::based_list->attrs_insert("__next__", model::create_nfunc(model::list_next));
    model::based_list->attrs_insert("__getitem__", model::create_nfunc(model::list_getitem));
    model::based_list->attrs_insert("__setitem__", model::create_nfunc(model::list_setitem));
    model::based_list->attrs_insert("__str__", model::create_nfunc(model::list_str));
    model::based_list->attrs_insert("__dstr__", model::create_nfunc(model::list_dstr));

    model::based_list->attrs_insert("append", model::create_nfunc(model::list_append));
    model::based_list->attrs_insert("contains", model::create_nfunc(model::list_contains));
    model::based_list->attrs_insert("foreach", model::create_nfunc(model::list_foreach));
    model::based_list->attrs_insert("reverse", model::create_nfunc(model::list_reverse));
    model::based_list->attrs_insert("extend", model::create_nfunc(model::list_extend));
    model::based_list->attrs_insert("pop", model::create_nfunc(model::list_pop));
    model::based_list->attrs_insert("insert", model::create_nfunc(model::list_insert));
    model::based_list->attrs_insert("find", model::create_nfunc(model::list_find));
    model::based_list->attrs_insert("map", model::create_nfunc(model::list_map));
    model::based_list->attrs_insert("count", model::create_nfunc(model::list_count));
    model::based_list->attrs_insert("filter", model::create_nfunc(model::list_filter));
    model::based_list->attrs_insert("len", model::create_nfunc(model::list_len));
    model::based_list->attrs_insert("join", model::create_nfunc(model::list_join));

    // String 类型魔法方法
    model::based_str->attrs_insert("__add__", model::create_nfunc(model::str_add));
    model::based_str->attrs_insert("__mul__", model::create_nfunc(model::str_mul));
    model::based_str->attrs_insert("__eq__", model::create_nfunc(model::str_eq));
    model::based_str->attrs_insert("__lt__", model::create_nfunc(model::str_lt));
    model::based_str->attrs_insert("__gt__", model::create_nfunc(model::str_gt));
    model::based_str->attrs_insert("__call__", model::create_nfunc(model::str_call));
    model::based_str->attrs_insert("__bool__", model::create_nfunc(model::str_bool));
    model::based_str->attrs_insert("__hash__", model::create_nfunc(model::str_hash));
    model::based_str->attrs_insert("__getitem__", model::create_nfunc(model::str_getitem));
    model::based_str->attrs_insert("__str__", model::create_nfunc(model::str_str));
    model::based_str->attrs_insert("__dstr__", model::create_nfunc(model::str_dstr));
    model::based_str->attrs_insert("__next__", model::create_nfunc(model::str_next));

    model::based_str->attrs_insert("contains", model::create_nfunc(model::str_contains));
    model::based_str->attrs_insert("find", model::create_nfunc(model::str_find));
    model::based_str->attrs_insert("replace", model::create_nfunc(model::str_replace));
    model::based_str->attrs_insert("count", model::create_nfunc(model::str_count));
    model::based_str->attrs_insert("foreach", model::create_nfunc(model::str_foreach));
    model::based_str->attrs_insert("startswith", model::create_nfunc(model::str_startswith));
    model::based_str->attrs_insert("endswith", model::create_nfunc(model::str_endswith));
    model::based_str->attrs_insert("substr", model::create_nfunc(model::str_substr));
    model::based_str->attrs_insert("len", model::create_nfunc(model::str_len));
    model::based_str->attrs_insert("isalpha", model::create_nfunc(model::str_is_alpha));
    model::based_str->attrs_insert("isdigit", model::create_nfunc(model::str_is_digit));
    model::based_str->attrs_insert("tolower", model::create_nfunc(model::str_to_lower));
    model::based_str->attrs_insert("toupper", model::create_nfunc(model::str_to_upper));
    model::based_str->attrs_insert("format", model::create_nfunc(model::str_format));

    // FileHandle类型
    model::based_file_handle->attrs_insert("read", model::create_nfunc(model::file_handle_read));
    model::based_file_handle->attrs_insert("flush", model::create_nfunc(model::file_handle_flush));
    model::based_file_handle->attrs_insert("write", model::create_nfunc(model::file_handle_write));
    model::based_file_handle->attrs_insert("readline", model::create_nfunc(model::file_handle_readline));
    model::based_file_handle->attrs_insert("close", model::create_nfunc(model::file_handle_close));

    // Range类型
    model::based_range->attrs_insert("__call__", model::create_nfunc(model::range_call));
    model::based_range->attrs_insert("__str__", model::create_nfunc(model::range_str));
    model::based_range->attrs_insert("__next__", model::create_nfunc(model::range_next));

    // Error类型
    model::based_error->attrs_insert("__call__", model::create_nfunc(model::error_call));
    model::based_error->attrs_insert("__str__", model::create_nfunc(model::error_str));

    // Module类型
    model::based_module->attrs_insert("__str__", model::create_nfunc(model::module_str));
    // Function类型
    model::based_function->attrs_insert("__str__", model::create_nfunc(model::function_str));
    // NativeFunction类型
    model::based_native_function->attrs_insert("__str__", model::create_nfunc(model::native_function_str));
    // __Unpack类型
    model::based_unpack->attrs_insert("__str__", model::create_nfunc(model::unpack_str));

    auto builtin_insert = [](const std::string& name,  model::Object* f) {
        f->make_ref();
        builtins.push_back(f);
        builtin_names.push_back(name);
    };
    builtin_insert("print", model::create_nfunc(builtin::print, "print"));
    builtin_insert("input", model::create_nfunc(builtin::input, "input"));
    builtin_insert("ischild", model::create_nfunc(builtin::ischild, "ischild"));
    builtin_insert("create", model::create_nfunc(builtin::create, "create"));
    builtin_insert("now", model::create_nfunc(builtin::now, "now"));
    builtin_insert("get_refc", model::create_nfunc(builtin::get_refc, "get_refc"));
    builtin_insert("breakpoint", model::create_nfunc(builtin::breakpoint, "breakpoint"));
    builtin_insert("cmd", model::create_nfunc(builtin::cmd, "cmd"));
    builtin_insert("help", model::create_nfunc(builtin::help, "help"));
    builtin_insert("delattr", model::create_nfunc(builtin::delattr, "delattr"));
    builtin_insert("setattr", model::create_nfunc(builtin::setattr, "setattr"));
    builtin_insert("getattr", model::create_nfunc(builtin::getattr, "getattr"));
    builtin_insert("hasattr", model::create_nfunc(builtin::hasattr, "hasattr"));
    builtin_insert("range", model::create_nfunc(builtin::range, "range"));
    builtin_insert("type_of", model::create_nfunc(builtin::type_of_obj, "type_of"));
    builtin_insert("type", model::create_nfunc(builtin::type_of_obj, "type"));
    builtin_insert("len", model::create_nfunc(builtin::len, "len"));
    builtin_insert("chr", model::create_nfunc(builtin::chr, "chr"));
    builtin_insert("ord", model::create_nfunc(builtin::ord, "ord"));
    builtin_insert("debug_str", model::create_nfunc(builtin::debug_str, "debug_str"));
    builtin_insert("attr", model::create_nfunc(builtin::attr, "attr"));
    builtin_insert("sleep", model::create_nfunc(builtin::sleep, "sleep"));
    builtin_insert("open", model::create_nfunc(builtin::open, "open"));
    builtin_insert("assert", model::create_nfunc(builtin::assert_, "assert"));
    builtin_insert("panic", model::create_nfunc(builtin::panic, "panic"));
    builtin_insert("repeat", model::create_nfunc(builtin::repeat, "repeat"));

    builtin_insert("__BasedObject", model::based_based_obj);
    builtin_insert("Object", model::based_obj);
    builtin_insert("Int", model::based_int);
    builtin_insert("Bool", model::based_bool);
    builtin_insert("Decimal", model::based_decimal);
    builtin_insert("List", model::based_list);
    builtin_insert("Dict", model::based_dict);
    builtin_insert("Str", model::based_str);
    builtin_insert("Func", model::based_function);
    builtin_insert("NFunc", model::based_native_function);
    builtin_insert("Error", model::based_error);
    builtin_insert("Module", model::based_module);
    builtin_insert("FileHandle", model::based_file_handle);
    builtin_insert("Range", model::based_range);
    builtin_insert("__Unpack", model::based_unpack);
    builtin_insert("__CodeObject", model::based_code_object);
    builtin_insert("__StopIterSignal__", model::stop_iter_signal);
}
}
