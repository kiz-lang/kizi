/**
 * @file builtins_lib.hpp
 * @brief builtins库初始化函数声明
 * @author azhz1107cat
 * @date 2026-10-25
 */
#pragma once
#include "../../../src/models/models.hpp"
#include "builtin_functions.hpp"
#include "builtin_methods.hpp"

namespace builtins_lib {
model::Object* init_module(model::Object* self, const model::List* args);
}