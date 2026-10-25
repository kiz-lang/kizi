/**
 * @file entry_std_modules.cpp
 * @brief C++实现的标准库模块注册入口
 * @author azhz1107cat
 * @date 2026-10-25
 */
#include "../models/models.hpp"
#include "builtins/include/builtins_lib.hpp"
#include "os/include/os_lib.hpp"

namespace kiz {

void Vm::entry_std_modules() {
    auto std_modules_insert = [](const std::string& name, model::NativeFunction* o){
        o->make_ref();
        std_modules.insert(name, o);
    };
    std_modules_insert("builtins", model::create_nfunc(builtins_lib::init_module, "__init__"));
    std_modules_insert("os", model::create_nfunc(os_lib::init_module, "__init__"));
}
} // namespace model