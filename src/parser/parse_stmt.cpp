/**
 * @file parse_stmt.cpp
 * @brief 语法分析器解析语句部分核心实现
 * @author azhz1107cat
 * @date 2025-11-14
 */

#include "parser.hpp"

#include <algorithm>
#include <iostream>
#include <memory>
#include <vector>

#include "../kiz.hpp"

namespace kiz {

void Parser::skip_type_unit() {
    skip_token();
    if (curr_token().type == TokenType::LBracket) {
        skip_token("[");
        while (curr_token().type != TokenType::RBracket) {
            skip_type_unit();
            if (curr_token().type == TokenType::Comma) {
                skip_token(",");
                skip_end_of_lines();
            } else if (curr_token().type != TokenType::RBracket) {
                err::error_reporter(file_path, curr_token().pos, "SyntaxError", "Unclosed argument list");
            }
        }
        skip_token("]");
    }
}

void Parser::skip_var_type_comment() {
    if (curr_token().type == TokenType::Colon) {
        skip_token(":");
    } else {
        return;
    }
    skip_type_unit();
}


void Parser::skip_fn_type_comment() {
    DEBUG_OUTPUT("parsing type comment");
    if (curr_token().type == TokenType::ThinArrow) {
        skip_token("->");
    } else {
        return;
    }
    skip_type_unit();
    if (curr_token().type == TokenType::Backslash) {
        skip_token("\\");
        skip_token("(");
        while (curr_token().type != TokenType::RParen) {
            skip_token();
            if (curr_token().type == TokenType::Comma) {
                skip_token(",");
                skip_end_of_lines();
            } else if (curr_token().type != TokenType::RParen) {
                err::error_reporter(file_path, curr_token().pos, "SyntaxError", "Unclosed argument list");
            }
        }
        skip_token(")");
    }
}


// 需要end结尾的块
std::unique_ptr<BlockStmt> Parser::parse_block(TokenType endswith) {
    DEBUG_OUTPUT("parsing block (with end)");
    std::vector<std::unique_ptr<Stmt>> block_stmts;
    auto block_tok = curr_token();

    while (curr_token().type != TokenType::EndOfFile) {
        // 跳过前导空白
        skip_end_of_lines();
        while (curr_token().type == TokenType::TripleDot) {
            skip_token("...");
            skip_end_of_lines();
        }
        skip_end_of_lines();

        if (curr_token().type == endswith || curr_token().type == TokenType::End) {
            break;
        }

        if (curr_token().type == TokenType::EndOfFile) {
            err::error_reporter(file_path, curr_token().pos, "SyntaxError", "Block not terminated with 'end'");
        }

        auto stmt = parse_stmt();
        if (stmt) {
            block_stmts.push_back(std::move(stmt));
        } else {
            err::error_reporter(file_path, curr_token().pos, "SyntaxError", "Invalid syntax");
        }
    }

    return std::make_unique<BlockStmt>(block_tok.pos, std::move(block_stmts));
}

// parse_if实现
std::unique_ptr<IfStmt> Parser::parse_if() {
    DEBUG_OUTPUT("parsing if");
    // 解析if条件表达式
    auto cond_expr = parse_expression();
    if (cond_expr == nullptr)
        err::error_reporter(file_path,
            curr_token().pos,
            "SyntaxError",
            "Invalid if condition"
        );

    // 解析if体（无end的块）
    skip_start_of_block();
    auto if_tok = curr_token();
    auto if_block = parse_block(TokenType::Else);

    // 处理else分支
    std::unique_ptr<BlockStmt> else_block = nullptr;
    if (curr_token().type == TokenType::Else) {
        skip_token("else");
        skip_start_of_block();
        if (curr_token().type == TokenType::If) {
            // else if分支
            std::vector<std::unique_ptr<Stmt>> else_if_stmts;
            else_if_stmts.push_back(parse_stmt());
            else_block = std::make_unique<BlockStmt>(curr_token().pos, std::move(else_if_stmts));
        } else {
            // else分支（无end的块）
            else_block = parse_block();
        }
    }

    if (curr_token().type == TokenType::End) {
        skip_end();
    }

    return std::make_unique<IfStmt>(if_tok.pos, std::move(cond_expr), std::move(if_block), std::move(else_block));
}

// parse_stmt实现
std::unique_ptr<Stmt> Parser::parse_stmt() {
    DEBUG_OUTPUT("parsing stmt");
    const Token curr_tok = curr_token();

    // 解析if语句
    if (curr_tok.type == TokenType::If) {
        skip_token("if");
        return parse_if();  // 复用parse_if逻辑
    }

    // 解析while语句（适配end结尾）
    if (curr_tok.type == TokenType::While) {
        DEBUG_OUTPUT("parsing while");
        auto tok = skip_token("while");
        // 解析循环条件表达式
        auto cond_expr = parse_expression();
        if (cond_expr == nullptr)
            err::error_reporter(file_path, curr_token().pos, "SyntaxError", "Invalid if condition");
        skip_start_of_block();
        auto while_block = parse_block();
        skip_end();
        return std::make_unique<WhileStmt>(tok.pos, std::move(cond_expr), std::move(while_block));
    }

    // 解析函数定义（新语法：fn x() end）
    if (curr_tok.type == TokenType::Func) {
        DEBUG_OUTPUT("parsing function");
        auto tok = skip_token("fn");
        // 读取函数名
        const std::string func_name = skip_token().text;

        // 解析参数列表（()包裹，逻辑不变）
        std::vector<std::string> func_params;
        bool has_rest_params = false;
        if (curr_token().type == TokenType::LParen) {
            skip_token("(");
            while (curr_token().type != TokenType::RParen) {
                if (curr_token().type == TokenType::TripleDot) {
                    has_rest_params = true;
                    skip_token("...");
                    func_params.push_back(skip_token().text);
                    if (curr_token().type == TokenType::Comma) {
                        skip_token(",");
                    }
                    break;
                }
                func_params.push_back(skip_token().text);
                skip_var_type_comment();
                // 处理参数间的逗号
                if (curr_token().type == TokenType::Comma) {
                    skip_token(",");
                } else if (curr_token().type != TokenType::RParen) {
                    err::error_reporter(file_path, curr_token().pos, "SyntaxError", "Mismatched function parameters");
                }
            }
            skip_token(")");  // 跳过右括号
        }
        skip_fn_type_comment();

        // 解析函数体（无大括号，用end结尾）
        skip_start_of_block();  // 跳过参数后的换行
        auto func_body = parse_block();
        if (auto& stmt = func_body->statements;
            !stmt.empty()
        ) {
            if (auto expr_stmt = dynamic_cast<ExprStmt*>(stmt[stmt.size() - 1].get())) {
                func_body->statements[stmt.size() - 1] = std::make_unique<ReturnStmt>(
                    expr_stmt->pos,  std::move(expr_stmt->expr)
                );
            }
        }
        skip_end();

        // 生成函数定义语句节点
        return std::make_unique<NamedFuncDeclStmt>(
            tok.pos,
            func_name,
            std::move(func_params),
            std::move(func_body),
            has_rest_params
        );
    }


    // 解析return语句
    if (curr_tok.type == TokenType::Return) {
        DEBUG_OUTPUT("parsing return");
        auto tok = skip_token("return");
        // return后可跟表达式（也可无，视为返回nil）
        std::unique_ptr<Expr> return_expr = parse_expression();
        skip_end_of_stmt();
        return std::make_unique<ReturnStmt>(tok.pos, std::move(return_expr));
    }

    // 解析break语句
    if (curr_tok.type == TokenType::Break) {
        DEBUG_OUTPUT("parsing break");
        auto tok = skip_token("break");
        skip_end_of_stmt();
        return std::make_unique<BreakStmt>(tok.pos);
    }

    // 解析continue语句
    if (curr_tok.type == TokenType::Next) {
        DEBUG_OUTPUT("parsing next");
        auto tok = skip_token("next");
        skip_end_of_stmt();
        return std::make_unique<NextStmt>(tok.pos);
    }

    // 解析import语句
    if (curr_tok.type == TokenType::Import) {
        DEBUG_OUTPUT("parsing import");
        auto tok = skip_token("import");
        // 读取模块路径
        const std::string var_name = skip_token().text;
        std::string import_path = var_name;

        if (curr_token().type == TokenType::At) {
            skip_token("at");
            import_path = skip_token().text;
        }

        skip_end_of_stmt();
        return std::make_unique<ImportStmt>(tok.pos, import_path, var_name);
    }

    // 解析nonlocal语句
    if (curr_tok.type == TokenType::Nonlocal) {
        DEBUG_OUTPUT("parsing nonlocal");
        auto tok = skip_token("nonlocal");
        const std::string name = skip_token().text;
        skip_token("=");
        std::unique_ptr<Expr> expr = parse_expression();
        skip_end_of_stmt();
        return std::make_unique<NonlocalAssignStmt>(tok.pos, name, std::move(expr));
    }

    // 解析global语句
    if (curr_tok.type == TokenType::Global) {
        DEBUG_OUTPUT("parsing global");
        auto tok = skip_token("global");
        const std::string name = skip_token().text;
        skip_token("=");
        std::unique_ptr<Expr> expr = parse_expression();
        skip_end_of_stmt();
        return std::make_unique<GlobalAssignStmt>(tok.pos, name, std::move(expr));
    }

    // 解析object语句（适配end结尾）
    if (curr_tok.type == TokenType::Object) {
        DEBUG_OUTPUT("parsing object");
        auto tok = skip_token("object");
        const std::string name = skip_token().text;
        std::string parent_name;
        if (curr_token().type == TokenType::Colon) {
            skip_token(":");
            parent_name = skip_token().text;
        }
        skip_start_of_block();
        auto object_block = parse_block();
        skip_end();
        return std::make_unique<ObjectStmt>(tok.pos, name, parent_name, std::move(object_block));
    }
    
    // 解析throw语句
    if (curr_tok.type == TokenType::Throw) {
        DEBUG_OUTPUT("parsing throw");
        auto tok = skip_token("throw");
        std::unique_ptr<Expr> expr = parse_expression();
        skip_end_of_stmt();
        return std::make_unique<ThrowStmt>(tok.pos, std::move(expr));
    }

    // 解析for语句
    if (curr_tok.type == TokenType::For) {
        DEBUG_OUTPUT("parsing for");
        auto tok = skip_token("for");
        const std::string name = skip_token().text;
        skip_token("in");
        std::unique_ptr<Expr> expr = parse_expression();

        skip_start_of_block();
        auto for_block = parse_block();
        skip_end();
        return std::make_unique<ForStmt>(tok.pos, name, std::move(expr), std::move(for_block));
    }

    // 解析ensure语句
    if (curr_tok.type == TokenType::Ensure) {
        auto tok = skip_token("ensure");
        std::unique_ptr<Expr> expr = parse_expression();
        skip_end_of_stmt();
        return std::make_unique<EnsureStmt>(tok.pos, std::move(expr));
    }
    
    // 解析try语句
    if (curr_tok.type == TokenType::Try) {
        DEBUG_OUTPUT("parsing try");
        auto tok = skip_token("try");
        
        skip_start_of_block();
        auto try_block = parse_block(TokenType::Catch);
        if (curr_token().type != TokenType::Catch)
            err::error_reporter(file_path, curr_token().pos, "SyntaxError",
            "Try block without catch block");

        std::vector<std::unique_ptr<CatchStmt>> catch_blocks;
        auto block_tok = curr_token();

        while (curr_token().type == TokenType::Catch) {
            DEBUG_OUTPUT("parsing catch");
            auto catch_tok = skip_token("catch"); // 跳过catch关键字
            const std::string var_name = skip_token().text; // 捕获变量名（e）
            skip_token("("); // 跳过(
            std::string error_type = skip_token().text; // 捕获类型（Error）
            skip_token(")"); // 跳过)

            skip_start_of_block();
            auto catch_block = parse_block(TokenType::Catch);
            // 构建catch语句节点
            catch_blocks.push_back(std::make_unique<CatchStmt>(
                catch_tok.pos, std::move(error_type), var_name, std::move(catch_block)
            ));
        }
        // 必须存在end结束try块
        if (curr_token().type != TokenType::End) {
            err::error_reporter(file_path, curr_token().pos, "SyntaxError",
                                "Try block not terminated with 'end'");
        }
        skip_end(); // 跳过end关键字

        // 检查catch块非空
        if (catch_blocks.empty()) {
            err::error_reporter(file_path, tok.pos, "SyntaxError",
                                "Try block has no valid catch blocks");
        }

        // 构建TryStmt节点，返回给上层
        return std::make_unique<TryStmt>(tok.pos, std::move(try_block),
            std::move(catch_blocks)
        );
    }

    // 解析赋值语句（x = expr;）
    if (curr_tok.type == TokenType::Identifier
        and curr_tok_idx_ + 1 < tokens_.size()
        and tokens_[curr_tok_idx_ + 1].type == TokenType::Assign
    ) {
        DEBUG_OUTPUT("parsing assign");
        const auto name_tok = skip_token();
        skip_token("=");
        auto expr = parse_expression();
        skip_end_of_stmt();
        return std::make_unique<AssignStmt>(name_tok.pos, name_tok.text, std::move(expr));
    }

    if (curr_tok.type == TokenType::Identifier
        and curr_tok_idx_ + 1 < tokens_.size()
        and tokens_[curr_tok_idx_ + 1].type == TokenType::Colon
    ) {
        DEBUG_OUTPUT("parsing assign");
        const auto name_tok = skip_token();
        skip_var_type_comment();
        skip_token("=");
        auto expr = parse_expression();
        skip_end_of_stmt();
        return std::make_unique<AssignStmt>(name_tok.pos, name_tok.text, std::move(expr));
    }


    // 解析表达式语句
    auto expr = parse_expression();
    if (expr and curr_token().text == "=") {
        if (expr->ast_type == AstType::GetMemberExpr) {
            DEBUG_OUTPUT("parsing set member");
            skip_token("=");
            auto value = parse_expression();

            auto set_mem = std::make_unique<SetMemberStmt>(curr_token().pos, std::move(expr), std::move(value));
            skip_end_of_stmt();
            return set_mem;
        }
        if (expr->ast_type == AstType::GetItemExpr) {
            DEBUG_OUTPUT("parsing set item");
            skip_token("=");
            auto value = parse_expression();

            auto set_item = std::make_unique<SetItemStmt>(curr_token().pos, std::move(expr), std::move(value));
            skip_end_of_stmt();
            return set_item;
        }
        // 非成员访问表达式后不能跟 =
        err::error_reporter(file_path, curr_token().pos, "SyntaxError",
            "Invalid assignment target: expected member access");
    }

    if (expr) {
        skip_end_of_stmt();
        return std::make_unique<ExprStmt>(curr_token().pos, std::move(expr));
    }

    return nullptr;  // 无有效语句，返回空
}

}
