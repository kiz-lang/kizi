/**
 * @file ast.hpp
 * @brief 抽象语法树（AST）核心定义
 *  
 * @author azhz1107cat
 * @date 2025-10-25
 */

#pragma once
#include <memory>
#include <string>
#include <vector>

#include "../error/error_reporter.hpp"

namespace kiz {

enum class AstType {
    // 表达式类型（对应 Expr 子类）
    NilExpr, BoolExpr,
    StringExpr, NumberExpr, DecimalExpr, ListExpr, IdentifierExpr,
    BinaryExpr, UnaryExpr,
    CallExpr,
    GetMemberExpr, GetItemExpr,
    LambdaExpr, DictExpr,

    // 语句类型（对应 Stmt 子类）
    AssignStmt, NonlocalAssignStmt, GlobalAssignStmt,
    SetMemberStmt, SetItemStmt,
    BlockStmt, IfStmt, WhileStmt,
    ReturnStmt, ImportStmt, ForStmt, TryStmt, CatchStmt,
    NullStmt, ExprStmt,
    BreakStmt, NextStmt, ThrowStmt, ObjectStmt, NamedFuncDeclStmt
};

// AST 基类
struct ASTNode {
    err::PositionInfo pos{};
    AstType ast_type = AstType::NullStmt;

    virtual ~ASTNode() = default;
};

// 表达式基类
struct Expr :  ASTNode {};

// 语句基类
struct Stmt :  ASTNode {};

// 字符串字面量
struct StringExpr final :  Expr {
    std::string value;
    explicit StringExpr(const err::PositionInfo& pos, std::string v)
        : value(std::move(v)) {
        this->pos = pos;
        this->ast_type = AstType::StringExpr;
    }
};

// 数字字面量
struct NumberExpr final :  Expr {
    std::string value;
    explicit NumberExpr(const err::PositionInfo& pos, std::string v)
        : value(std::move(v)) {
        this->pos = pos;
        this->ast_type = AstType::NumberExpr;
    }
};

struct DecimalExpr final :  Expr {
    std::string value;
    explicit DecimalExpr(const err::PositionInfo& pos, std::string v)
        : value(std::move(v)) {
        this->pos = pos;
        this->ast_type = AstType::DecimalExpr;
    }
};

// 空值字面量
struct NilExpr final : Expr {
    explicit NilExpr(const err::PositionInfo& pos) {
        this->pos = pos;
        this->ast_type = AstType::NilExpr;
    }
};

// 布尔值字面量
struct BoolExpr final : Expr {
    bool val;
    explicit BoolExpr(const err::PositionInfo& pos, bool v) : val(v) {
        this->pos = pos;
        this->ast_type = AstType::BoolExpr;
    }
};

// 数组字面量
struct ListExpr final :  Expr {
    std::vector<std::unique_ptr<Expr>> elements;
    explicit ListExpr(const err::PositionInfo& pos, std::vector<std::unique_ptr<Expr>> elems)
        : elements(std::move(elems)) {
        this->pos = pos;
        this->ast_type = AstType::ListExpr;
    }
};

// 字典字面量
struct DictExpr final :  Expr {
    std::vector<std::pair<std::unique_ptr<Expr>, std::unique_ptr<Expr>>> elements;
    explicit DictExpr(const err::PositionInfo& pos,
         decltype(elements) elems
    ) : elements(std::move(elems)) {
        this->pos = pos;
        this->ast_type = AstType::DictExpr;
    }
};

// 标识符
struct IdentifierExpr final :  Expr {
    std::string name;
    explicit IdentifierExpr(const err::PositionInfo& pos, std::string n)
        : name(std::move(n)) {
        this->pos = pos;
        this->ast_type = AstType::IdentifierExpr;
    }
};

// 二元运算
struct BinaryExpr final :  Expr {
    std::string op;
    std::unique_ptr<Expr> left, right;
    BinaryExpr(const err::PositionInfo& pos, std::string o, std::unique_ptr<Expr> l, std::unique_ptr<Expr> r)
        : op(std::move(o)), left(std::move(l)), right(std::move(r)) {
        this->pos = pos;
        this->ast_type = AstType::BinaryExpr;
    }
};

// 一元运算
struct UnaryExpr final :  Expr {
    std::string op;
    std::unique_ptr<Expr> operand;
    UnaryExpr(const err::PositionInfo& pos, std::string o, std::unique_ptr<Expr> e)
        : op(std::move(o)), operand(std::move(e)) {
        this->pos = pos;
        this->ast_type = AstType::UnaryExpr;
    }
};

// 赋值
struct AssignStmt final :  Stmt {
    std::string name;
    std::unique_ptr<Expr> expr;
    AssignStmt(const err::PositionInfo& pos, std::string n, std::unique_ptr<Expr> e)
        : name(std::move(n)), expr(std::move(e)) {
        this->pos = pos;
        this->ast_type = AstType::AssignStmt;
    }
};

// nonlocal赋值
struct NonlocalAssignStmt final :  Stmt {
    std::string name;
    std::unique_ptr<Expr> expr;
    NonlocalAssignStmt(const err::PositionInfo& pos, std::string n, std::unique_ptr<Expr> e)
        : name(std::move(n)), expr(std::move(e)) {
        this->pos = pos;
        this->ast_type = AstType::NonlocalAssignStmt;
    }
};

// global赋值
struct GlobalAssignStmt final :  Stmt {
    std::string name;
    std::unique_ptr<Expr> expr;
    GlobalAssignStmt(const err::PositionInfo& pos, std::string n, std::unique_ptr<Expr> e)
        : name(std::move(n)), expr(std::move(e)) {
        this->pos = pos;
        this->ast_type = AstType::GlobalAssignStmt;
    }
};

// 复合语句块
struct BlockStmt final :  Stmt {
    std::vector<std::unique_ptr<Stmt>> statements{};
    explicit BlockStmt(const err::PositionInfo& pos, std::vector<std::unique_ptr<Stmt>> s)
        : statements(std::move(s)) {
        this->pos = pos;
        this->ast_type = AstType::BlockStmt;
    }
};

// if 语句
struct IfStmt final :  Stmt {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<BlockStmt> thenBlock;
    std::unique_ptr<BlockStmt> elseBlock;
    IfStmt(const err::PositionInfo& pos, std::unique_ptr<Expr> cond, std::unique_ptr<BlockStmt> thenB, std::unique_ptr<BlockStmt> elseB)
        : condition(std::move(cond)), thenBlock(std::move(thenB)), elseBlock(std::move(elseB)) {
        this->pos = pos;
        this->ast_type = AstType::IfStmt;
    }
};

// while 语句
struct WhileStmt final :  Stmt {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<BlockStmt> body;
    WhileStmt(const err::PositionInfo& pos, std::unique_ptr<Expr> cond, std::unique_ptr<BlockStmt> b)
        : condition(std::move(cond)), body(std::move(b)) {
        this->pos = pos;
        this->ast_type = AstType::WhileStmt;
    }
};

// throw语句
struct ThrowStmt final :  Stmt{
    std::unique_ptr<Expr> expr;
    explicit ThrowStmt(const err::PositionInfo& pos, std::unique_ptr<Expr> e)
        : expr(std::move(e)) {
        this->pos = pos;
        this->ast_type = AstType::ThrowStmt;
    }
};

// for语句
struct ForStmt final :  Stmt {
    std::string item_var_name;
    std::unique_ptr<Expr> iter;
    std::unique_ptr<BlockStmt> body;
    explicit ForStmt(const err::PositionInfo& pos,
        std::string iv,
        std::unique_ptr<Expr> i,
        std::unique_ptr<BlockStmt> b
    ) : item_var_name(std::move(iv)), iter(std::move(i)), body(std::move(b)) {
        this->pos = pos;
        this->ast_type = AstType::ForStmt;
    }
};

// catch语句
struct CatchStmt final :  Stmt {
    std::string error_text;
    std::string var_name;
    std::unique_ptr<BlockStmt> catch_block;
    explicit CatchStmt(const err::PositionInfo& pos,
        std::string e,
        std::string v,
        std::unique_ptr<BlockStmt> c
    ) : error_text(std::move(e)), var_name(std::move(v)), catch_block(std::move(c)) {
        this->pos = pos;
        this->ast_type = AstType::CatchStmt;
    }
};

// try语句
struct TryStmt final :  Stmt {
    std::unique_ptr<BlockStmt> try_block;
    std::vector<std::unique_ptr<CatchStmt>> catch_blocks;
    explicit TryStmt(const err::PositionInfo& pos,
        std::unique_ptr<BlockStmt> t,
        std::vector<std::unique_ptr<CatchStmt>> c
    ) : try_block(std::move(t)), catch_blocks(std::move(c)){
        this->pos = pos;
        this->ast_type = AstType::TryStmt;
    }
};

// 设置成员
struct SetMemberStmt final :  Stmt {
    std::unique_ptr<Expr> g_mem;
    std::unique_ptr<Expr> val;
    SetMemberStmt(const err::PositionInfo& pos, std::unique_ptr<Expr> g_mem, std::unique_ptr<Expr> val)
        : g_mem(std::move(g_mem)), val(std::move(val)) {
        this->pos = pos;
        this->ast_type = AstType::SetMemberStmt;
    }
};

// 设置项
struct SetItemStmt final :  Stmt {
    std::unique_ptr<Expr> g_item;
    std::unique_ptr<Expr> val;
    SetItemStmt(const err::PositionInfo& pos, std::unique_ptr<Expr> g_mem, std::unique_ptr<Expr> val)
        : g_item(std::move(g_mem)), val(std::move(val)) {
        this->pos = pos;
        this->ast_type = AstType::SetItemStmt;
    }
};

// 函数调用
struct CallExpr final :  Expr {
    std::unique_ptr<Expr> callee;
    std::vector<std::unique_ptr<Expr>> args;
    CallExpr(const err::PositionInfo& pos, std::unique_ptr<Expr> c, std::vector<std::unique_ptr<Expr>> a)
        : callee(std::move(c)), args(std::move(a)) {
        this->pos = pos;
        this->ast_type = AstType::CallExpr;
    }
};

// 获取成员
struct GetMemberExpr final :  Expr {
    std::unique_ptr<Expr> father;
    std::unique_ptr<IdentifierExpr> child;
    GetMemberExpr(const err::PositionInfo& pos, std::unique_ptr<Expr> f, std::unique_ptr<IdentifierExpr> c)
        : father(std::move(f)), child(std::move(c)) {
        this->pos = pos;
        this->ast_type = AstType::GetMemberExpr;
    }
};

// 获取项
struct GetItemExpr final :  Expr {
    std::unique_ptr<Expr> father;
    std::vector<std::unique_ptr<Expr>> params;
    GetItemExpr(const err::PositionInfo& pos, std::unique_ptr<Expr> f, std::vector<std::unique_ptr<Expr>> p)
        : father(std::move(f)), params(std::move(p)) {
        this->pos = pos;
        this->ast_type = AstType::GetItemExpr;
    }
};

// 声明匿名函数
struct LambdaExpr final :  Expr {
    std::string name;
    std::vector<std::string> params;
    std::unique_ptr<BlockStmt> body;
    bool has_rest_params = false;
    LambdaExpr(const err::PositionInfo& pos, std::string n, std::vector<std::string> p, std::unique_ptr<BlockStmt> b, bool has_rest_params)
        : name(std::move(n)), params(std::move(p)), body(std::move(b)), has_rest_params(has_rest_params) {
        this->pos = pos;
        this->ast_type = AstType::LambdaExpr;
    }
};

struct NamedFuncDeclStmt final :  Stmt {
    std::string name;
    std::vector<std::string> params;
    std::unique_ptr<BlockStmt> body;
    bool has_rest_params = false;
    NamedFuncDeclStmt(const err::PositionInfo& pos, std::string n, std::vector<std::string> p, std::unique_ptr<BlockStmt> b, bool has_rest_params)
        : name(std::move(n)), params(std::move(p)), body(std::move(b)), has_rest_params(has_rest_params) {
        this->pos = pos;
        this->ast_type = AstType::NamedFuncDeclStmt;
    }
};

// return 语句
struct ReturnStmt final :  Stmt {
    std::unique_ptr<Expr> expr;
    explicit ReturnStmt(const err::PositionInfo& pos, std::unique_ptr<Expr> e)
        : expr(std::move(e)) {
        this->pos = pos;
        this->ast_type = AstType::ReturnStmt;
    }
};

// import 语句
struct ImportStmt final :  Stmt {
    std::string path;
    std::string var_name;
    explicit ImportStmt(const err::PositionInfo& pos, std::string p, std::string v)
        : path(std::move(p)), var_name(std::move(v)) {
        this->pos = pos;
        this->ast_type = AstType::ImportStmt;
    }
};

// object语句
struct ObjectStmt final :  Stmt {
    std::string name;
    std::string parent_name;
    std::unique_ptr<BlockStmt> body;
    explicit ObjectStmt(
        const err::PositionInfo& pos,
        std::string n, std::string p,
        std::unique_ptr<BlockStmt> b
    ) : name(std::move(n)), parent_name(std::move(p)), body(std::move(b)) {
        this->pos = pos;
        this->ast_type = AstType::ObjectStmt;
    }
};

// 空语句
struct NullStmt final :  Stmt {
    explicit NullStmt(const err::PositionInfo& pos) {
        this->pos = pos;
        this->ast_type = AstType::NullStmt;
    }
};

// break 语句
struct BreakStmt final :  Stmt {
    explicit BreakStmt(const err::PositionInfo& pos) {
        this->pos = pos;
        this->ast_type = AstType::BreakStmt;
    }
};

// continue 语句
struct NextStmt final :  Stmt {
    explicit NextStmt(const err::PositionInfo& pos) {
        this->pos = pos;
        this->ast_type = AstType::NextStmt;
    }
};

// 表达式语句
struct ExprStmt final :  Stmt {
    std::unique_ptr<Expr> expr;
    explicit ExprStmt(const err::PositionInfo& pos, std::unique_ptr<Expr> e)
        : expr(std::move(e)) {
        this->pos = pos;
        this->ast_type = AstType::ExprStmt;
    }
};

} // namespace kiz