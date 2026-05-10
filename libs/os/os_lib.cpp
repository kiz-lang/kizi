#include "include/os_lib.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <filesystem>
#include "builtins/include/builtin_functions.hpp"

#ifdef _WIN32
#   include <windows.h>
#   include <direct.h>
#   define PATH_MAX MAX_PATH
    extern char** _environ;
#else
#   include <unistd.h>
#   include <climits>
    extern char** environ;
#endif

namespace os_lib {

model::Object* init_module(model::Object* self, const model::List* args) {
    auto mod = new model::Module("os");

    mod->attrs_insert("argv", model::create_nfunc(get_args));
    mod->attrs_insert("env", model::create_nfunc(get_env));
    mod->attrs_insert("exit", model::create_nfunc(exit_));
    mod->attrs_insert("cwd", model::create_nfunc(cwd));
    mod->attrs_insert("chdir_", model::create_nfunc(chdir_));
    mod->attrs_insert("mkdir", model::create_nfunc(mkdir_));
    mod->attrs_insert("rmdir", model::create_nfunc(rmdir));
    mod->attrs_insert("remove", model::create_nfunc(remove));

    return mod;
}

model::Object* get_args(model::Object* self, const model::List* args) {
    kiz::Vm::assert_argc(0, args);
    auto argv = new model::List({});
    for (auto c: rest_argv) {
        argv->val.push_back(new model::String(c));
    }
    return argv;
}

model::Object* get_env(model::Object* self, const model::List* args) {
    try {
        std::vector<std::pair<dep::BigInt, std::pair<model::Object*, model::Object*>>> elem_list;
        if (args->val.empty()) {

#if defined(_WIN32)
            // Windows 读取环境变量（通过_environ全局变量）
            char** env = _environ;
            while (*env != nullptr) {
                std::string env_str = *env;
                size_t eq_pos = env_str.find('=');
                if (eq_pos != std::string::npos) {
                    std::string key = env_str.substr(0, eq_pos);
                    std::string val = env_str.substr(eq_pos + 1);
                    elem_list.emplace_back(dep::hash_string(key),
                        std::pair {new model::String(key), new model::String(val)}
                    );
                }
                env++;
            }
#else
            // Linux/macOS 读取环境变量（通过environ全局变量）
            char** env = environ;
            while (*env != nullptr) {
                std::string env_str = *env;
                size_t eq_pos = env_str.find('=');
                if (eq_pos != std::string::npos) {
                    std::string key = env_str.substr(0, eq_pos);
                    std::string val = env_str.substr(eq_pos + 1);
                    elem_list.emplace_back(dep::hash_string(key),
                        std::pair {new model::String(key), new model::String(val)}
                    );
                }
                env++;
            }
#endif

            // 返回字典类型的Object（基于你定义的make_dict）
            return new model::Dictionary(dep::Dict(elem_list));
        }
        kiz::Vm::assert_argc(0, args);
    } catch (const std::exception& e) {
        throw NativeFuncError("SystemError",std::format("Error in get environment vars: {}", e.what()));
        return model::load_nil();
    }
}

model::Object* exit_(model::Object* self, const model::List* args) {
     size_t exit_code;
     if (!args->val.empty()) {
         exit_code= model::cast_to_int(builtin::get_one_arg(args)) ->val.to_unsigned_long_long();
     } else {
         exit_code= 0;
     }
    std::exit(exit_code);
}

model::Object* cwd(model::Object* self, const model::List* args) {
    // 跨平台获取当前工作目录
    char buf[PATH_MAX];
#if defined(_WIN32)
    if (_getcwd(buf, sizeof(buf)) == nullptr) {
#else
    if (getcwd(buf, sizeof(buf)) == nullptr) {
#endif
        throw NativeFuncError("SystemError", "Failed to get current working directory");
    }
    return new model::String(std::string(buf));
}

model::Object* chdir_(model::Object* self, const model::List* args) {
    auto path_obj = builtin::get_one_arg(args);
    auto path = model::cast_to_str(path_obj)->val;

    // 跨平台切换目录
#if defined(_WIN32)
    if (_chdir(path.c_str()) != 0) {
#else
    if (chdir(path.c_str()) != 0) {
#endif
        throw NativeFuncError("SystemError", "Failed to change directory: " + path);
    }
    return model::load_nil();
}

model::Object* mkdir_(model::Object* self, const model::List* args) {
    try {
        // 获取目录名称参数
        auto name_obj = builtin::get_one_arg(args);
        auto name = model::cast_to_str(name_obj)->val;

        // 创建目录（递归创建，如 mkdir -p）
        std::filesystem::create_directories(name);
        return model::load_nil();
    } catch (const std::exception& e) {
        throw NativeFuncError("SystemError", std::format("Error in mkdir: {}" , e.what()));
        return model::load_nil();
    }
}

model::Object* rmdir(model::Object* self, const model::List* args) {
    try {
        // 获取目录名称参数
        auto name_obj = builtin::get_one_arg(args);
        auto name = model::cast_to_str(name_obj)->val;

        // 检查目录是否存在且为空
        if (!std::filesystem::is_directory(name)) {
            throw NativeFuncError("SystemError", name + " is not a directory");
        }
        if (!std::filesystem::is_empty(name)) {
            throw NativeFuncError("SystemError", name + " is not empty");
        }

        // 删除空目录
        std::filesystem::remove(name);
        return model::load_nil();
    } catch (const std::exception& e) {
        throw NativeFuncError("SystemError", std::format("Error in rmdir: {}" , e.what()));
    }
}

model::Object* remove(model::Object* self, const model::List* args) {
    try {
        // 获取文件/目录名称参数
        auto name_obj = builtin::get_one_arg(args);
        auto name = model::cast_to_str(name_obj)->val;

        // 删除文件（若要删除目录，需用 remove_all，但需谨慎）
        if (std::filesystem::is_directory(name)) {
            throw NativeFuncError("SystemError",name + " is a directory (use rmdir instead)");
        }
        std::filesystem::remove(name);
        return model::load_nil();
    } catch (const std::exception& e) {
        throw NativeFuncError("SystemError", std::format("Error in remove: {}" , e.what()));
    }
}

}
