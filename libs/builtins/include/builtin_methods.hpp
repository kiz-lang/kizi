/**
 * @file builtin_methods.hpp
 * @brief 各内置类型的魔术方法声明
 * @author azhz1107cat
 * @date 2026-10-25
 */
#pragma once
#include <functional>

#include "../../../src/models/models.hpp"

namespace model {

// Object类型
Object* object_str(Object* self, const List* args);
Object* object_bool(Object* self, const List* args);
Object* object_eq(Object* self, const List* args);
Object* object_setitem(Object* self, const List* args);
Object* object_getitem(Object* self, const List* args);

// Int 类型原生函数
Object* int_add(Object* self, const List* args);
Object* int_sub(Object* self, const List* args);
Object* int_mul(Object* self, const List* args);
Object* int_div(Object* self, const List* args);
Object* int_pow(Object* self, const List* args);
Object* int_mod(Object* self, const List* args);
Object* int_neg(Object* self, const List* args);
Object* int_eq(Object* self, const List* args);
Object* int_lt(Object* self, const List* args);
Object* int_gt(Object* self, const List* args);
Object* int_bool(Object* self, const List* args);
Object* int_call(Object* self, const List* args);
Object* int_hash(Object* self, const List* args);
Object* int_str(Object* self, const List* args);

// Decimal类型原生函数
Object* decimal_add(Object* self, const List* args);
Object* decimal_sub(Object* self, const List* args);
Object* decimal_mul(Object* self, const List* args);
Object* decimal_div(Object* self, const List* args);
Object* decimal_mod(Object* self, const List* args);
Object* decimal_pow(Object* self, const List* args);
Object* decimal_neg(Object* self, const List* args);
Object* decimal_eq(Object* self, const List* args);
Object* decimal_lt(Object* self, const List* args);
Object* decimal_gt(Object* self, const List* args);
Object* decimal_bool(Object* self, const List* args);
Object* decimal_call(Object* self, const List* args);
Object* decimal_hash(Object* self, const List* args);
Object* decimal_str(Object* self, const List* args);
Object* decimal_limit_div(Object* self, const List* args);
Object* decimal_round_div(Object* self, const List* args);
Object* decimal_approx(Object* self, const List* args);

// Nil 类型原生函数
Object* nil_eq(Object* self, const List* args);
Object* nil_hash(Object* self, const List* args);
Object* nil_str(Object* self, const List* args);

// Bool 类型原生函数
Object* bool_eq(Object* self, const List* args);
Object* bool_call(Object* self, const List* args);
Object* bool_hash(Object* self, const List* args);
Object* bool_str(Object* self, const List* args);

// String 类型原生函数
Object* str_eq(Object* self, const List* args);
Object* str_lt(Object* self, const List* args);
Object* str_gt(Object* self, const List* args);
Object* str_add(Object* self, const List* args);
Object* str_mul(Object* self, const List* args);
Object* str_contains(Object* self, const List* args);
Object* str_find(Object* self, const List* args);
Object* str_replace(Object* self, const List* args);
Object* str_call(Object* self, const List* args);
Object* str_bool(Object* self, const List* args);
Object* str_hash(Object* self, const List* args);
Object* str_next(Object* self, const List* args);
Object* str_getitem(Object* self, const List* args);
Object* str_str(Object* self, const List* args);
Object* str_dstr(Object* self, const List* args);
// 普通方法
Object* str_foreach(Object* self, const List* args);
Object* str_count(Object* self, const List* args);
Object* str_startswith(Object* self, const List* args);
Object* str_endswith(Object* self, const List* args);
Object* str_len(Object* self, const List* args);
Object* str_substr(Object* self, const List* args);
Object* str_is_alpha(Object* self, const List* args);
Object* str_is_digit(Object* self, const List* args);
Object* str_to_lower(Object* self, const List* args);
Object* str_to_upper(Object* self, const List* args);
Object* str_format(Object* self, const List* args);

// Dict 类型原生函数
Object* dict_eq(Object* self, const List* args);
Object* dict_add(Object* self, const List* args);
Object* dict_contains(Object* self, const List* args);
Object* dict_setitem(Object* self, const List* args);
Object* dict_getitem(Object* self, const List* args);
Object* dict_str(Object* self, const List* args);
Object* dict_dstr(Object* self, const List* args);
Object* dict_foreach(Object* self, const List* args);
Object* dict_next(Object* self, const List* args);
Object* dict_len(Object* self, const List* args);

// List 类型原生函数
Object* list_eq(Object* self, const List* args);
Object* list_add(Object* self, const List* args);
Object* list_mul(Object* self, const List* args);
Object* list_call(Object* self, const List* args);
Object* list_bool(Object* self, const List* args);
Object* list_next(Object* self, const List* args);
Object* list_setitem(Object* self, const List* args);
Object* list_getitem(Object* self, const List* args);
Object* list_str(Object* self, const List* args);
Object* list_dstr(Object* self, const List* args);
// 普通方法
Object* list_contains(Object* self, const List* args);
Object* list_append(Object* self, const List* args);
Object* list_foreach(Object* self, const List* args);
Object* list_reverse(Object* self, const List* args);
Object* list_extend(Object* self, const List* args);
Object* list_pop(Object* self, const List* args);
Object* list_insert(Object* self, const List* args);
Object* list_find(Object* self, const List* args);
Object* list_map(Object* self, const List* args);
Object* list_count(Object* self, const List* args);
Object* list_len(Object* self, const List* args);
Object* list_filter(Object* self, const List* args);
Object* list_join(Object* self, const List* args);

// FileHandle类型
Object* file_handle_read(Object* self, const List* args);
Object* file_handle_flush(Object* self, const List* args);
Object* file_handle_write(Object* self, const List* args);
Object* file_handle_readline(Object* self, const List* args);
Object* file_handle_close(Object* self, const List* args);

// Range类型
Object* range_call(Object* self, const List* args);
Object* range_next(Object* self, const List* args);
Object* range_str(Object* self, const List* args);

// Error类型
Object* error_str(Object* self, const List* args);
Object* error_call(Object* self, const List* args);

// Function类型
Object* function_str(Object* self, const List* args);

// NativeFunction类型
Object* native_function_str(Object* self, const List* args);

// Module类型
Object* module_str(Object* self, const List* args);

// __Unpack类型
Object* unpack_str(Object* self, const List* args);

}
