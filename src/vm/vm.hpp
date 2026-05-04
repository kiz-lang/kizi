/**
 * @file vm.hpp
 * @brief 虚拟机(VM)核心定义
 * 执行IR
 * @author azhz1107cat
 * @date 2025-10-25
 */
#pragma once

#include <cassert>
#include <filesystem>

#include "../../depends/hashmap.hpp"

#include <stack>
#include <tuple>
#include <utility>

#include "../kiz.hpp"
#include "../error/error_reporter.hpp"

#define ADVANCE_PC \
    if (curr_inst.opc != Opcode::JUMP \
        && curr_inst.opc != Opcode::JUMP_IF_FALSE \
        && curr_inst.opc != Opcode::RET \
        && curr_inst.opc != Opcode::THROW \
        && curr_inst.opc != Opcode::JUMP_IF_FINISH_ITER\
    ) { \
                curr_frame->pc++; \
    }


namespace model {
class Module;
class CodeObject;
class Object;
class List;
class Int;
class Error;
}

namespace kiz {

enum class Opcode : uint8_t;

struct Instruction {
    Opcode opc;
    size_t opn;
    err::PositionInfo pos{};
    Instruction(Opcode o, size_t opn, err::PositionInfo& p) : opc(o), opn(opn), pos(p) {}
};

struct CallFrame {
    std::string name;

    model::Object* owner;

    size_t pc = 0;
    size_t return_to_pc;
    size_t last_bp;
    size_t bp;
    model::CodeObject* code_object;
    
    std::vector<model::Object*> iters;

    model::Object* curr_error;
    bool exec_ensure_stmt = false;
};

class StackRef {
    model::Object* obj;
public:
    explicit StackRef(model::Object* o) : obj(o) {}
    ~StackRef(); // 在vm.cpp中实现
    StackRef(const StackRef&) = delete;
    StackRef(StackRef&& other)  noexcept : obj(other.obj) { other.obj = nullptr; }
    [[nodiscard]] model::Object* get() const { return obj; }
    model::Object* release() { auto p = obj; obj = nullptr; return p; }
};

class Vm {
public:
    static dep::HashMap<model::Module*> modules_cache;
    static model::Module* main_module;

    static std::vector<model::Object*> op_stack;
    static std::vector<CallFrame*> call_stack;

    static model::Int* small_int_pool[201];
    static std::vector<model::Object*> const_pool;

    static std::vector<model::Object*> builtins;
    static std::vector<std::string> builtin_names;
    static dep::HashMap<model::Object*> std_modules;

    static bool running;
    static std::string main_file_path;

    explicit Vm(const std::string& file_path_);

    ///| 核心执行循环
    static void set_main_module(model::Module* src_module);
    static void exec_curr_code();
    static void reset_global_code(model::CodeObject* code_object);
    static void execute_unit(const Instruction& instruction);

    ///| 栈操作
    static CallFrame* get_frame();
    static StackRef get_and_pop_stack_top(); // 返回StackRef对象，参与RAII
    static model::Object* simple_get_and_pop_stack_top(); // 直接返回栈顶值, 需手动del_refc
    static void push_to_stack(model::Object* obj);
    static std::string get_attr_name_by_idx(size_t idx);

    ///| 如果新增了调用栈，执行循环仅处理新增的模块栈帧（call_stack.size() > old_stack_size），不影响原有调用栈
    static void call_function(model::Object* func_obj, std::vector<model::Object*> args, model::Object* self);

    ///| 运算符与普通方法分规则查找
    static void call_method(model::Object* obj, const std::string& attr_name, std::vector<model::Object*> args);

    ///| 如果用户函数则创建调用栈，如果内置函数则执行并压上返回值
    static void handle_call(model::Object* func_obj, model::Object* args_obj, model::Object* self=nullptr);

    ///| 处理import
    static void handle_import(const std::string& module_path);

    ///| 处理报错(设置到catch)或者进行traceback
    static void handle_throw();
    static void handle_ensure();

    ///| 注册内置对象
    static void entry_builtins();
    ///| 注册标注库
    static void entry_std_modules();

    ///| @utils
    static model::Object* get_attr(model::Object* obj, const std::string& attr);
    static model::Object* get_attr_current(model::Object* obj, const std::string& attr);
    static bool is_true(model::Object* obj);
    static std::string obj_to_str(model::Object* for_cast_obj);
    static std::string obj_to_debug_str(model::Object* for_cast_obj);
    static void forward_to_handle_throw(const std::string& name, const std::string& content);  // 转发到handle_throw函数

    static auto make_pos_info() -> std::vector<std::pair<std::string, err::PositionInfo>>;
    static void make_list(size_t len);
    static void make_dict(size_t len);

    ///| @utils: 供builtins检查参数
    static void assert_argc(size_t argc, const model::List* args);
    static void assert_argc(const std::vector<size_t>& argcs, const model::List* args);

    ///| @utils: 路径处理
    static std::filesystem::path get_exe_abs_dir();
    static std::filesystem::path get_current_file_path();
};

} // namespace kiz