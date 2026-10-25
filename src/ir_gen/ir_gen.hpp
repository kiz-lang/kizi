/**
 * @file ir_gen.hpp
 * @brief 中间代码生成器(IR Generator) 核心定义
 * 
 * @author azhz1107cat
 * @date 2025-10-25
 */

#pragma once
#include "../parser/ast.hpp"
#include "../models/models.hpp"

#include <memory>
#include <stack>
#include <vector>


namespace kiz {

struct LoopInfo {
    std::vector<size_t> break_pos;
    std::vector<size_t> continue_pos;
};


struct CodeChunk {
    std::vector<std::string> var_names;
    std::vector<std::string> attr_names;
    std::vector<std::string> free_names;

    std::vector<Instruction> code_list;
    std::vector<LoopInfo> loop_info_stack;
    std::vector<model::UpValue> upvalues;

    std::vector<model::ExceptionTable> exception_tables;
};

class IRGenerator {
    std::unique_ptr<BlockStmt> ast;
    std::vector<CodeChunk> code_chunks;
    const std::string& file_path;
public:
    explicit IRGenerator(const std::string& file_path) : file_path(file_path) {}
    model::CodeObject* gen(
        std::unique_ptr<BlockStmt> ast_into,
        const std::vector<std::string>& global_var_names_into = {}
        );

    static size_t get_or_add_name(std::vector<std::string>& names, const std::string& name);
    static size_t get_or_add_const(model::Object* obj);
    [[nodiscard]] static model::Module* gen_mod(
        const std::string& module_name, model::CodeObject* module_code
    );
    std::vector<std::string> get_global_var_names();

private:
    void gen_for(ForStmt* for_stmt);
    void gen_try(TryStmt* try_stmt);
    void gen_block(const BlockStmt* block);

    void gen_fn_call(CallExpr* expr);
    void gen_dict(DictExpr* expr);
    void gen_expr(Expr* expr);

    void gen_if(IfStmt* if_stmt);
    ///| 生成具名函数声明，返回函数名在所在代码块var_names中的下标
    size_t gen_fn_decl(NamedFuncDeclStmt* func);
    ///| 生成object语句，返回对象名在所在代码块var_names中的下标
    size_t gen_object_stmt(ObjectStmt* stmt);
    void gen_while(WhileStmt* while_stmt);

    static model::Int* make_int_obj(const NumberExpr* num_expr);
    static model::Decimal* make_decimal_obj(const DecimalExpr* dec_expr);
    static model::String* make_string_obj(const StringExpr* str_expr);
};

} // namespace kiz