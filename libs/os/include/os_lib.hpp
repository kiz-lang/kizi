/**
 * @file os_lib.hpp
 * @brief os标准模块核心定义
 * @author azhz1107cat
 * @date 2026-10-25
 */
#pragma once
#include "models/models.hpp"

namespace os_lib {

inline std::vector<char*> rest_argv;

model::Object* init_module(model::Object* self, const model::List* args);

model::Object* get_args(model::Object* self, const model::List* args);
model::Object* get_env(model::Object* self, const model::List* args);
model::Object* exit_(model::Object* self, const model::List* args);
model::Object* cwd(model::Object* self, const model::List* args);

model::Object* chdir_(model::Object* self, const model::List* args);
model::Object* mkdir_(model::Object* self, const model::List* args);
model::Object* rmdir(model::Object* self, const model::List* args);

model::Object* remove(model::Object* self, const model::List* args);

}