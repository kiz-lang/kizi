/**
 * @file parse_expr.cpp
 * @brief 语法分析器解析表达式部分核心实现
 * @author azhz1107cat
 * @date 2025-11-14
 */

#include "parser.hpp"

#include <algorithm>
#include <memory>
#include <vector>

#include "cassert"
#include "../kiz.hpp"


namespace kiz {

// (可能返回nullptr)
std::unique_ptr<Expr> Parser::parse_expression() {
    DEBUG_OUTPUT("parse the expression...");
    return parse_and_or(); // 直接调用合并后的函数
}

// 处理 and/or（优先级相同，左结合）
std::unique_ptr<Expr> Parser::parse_and_or() {
    DEBUG_OUTPUT("parsing and/or expression...");
    auto node = parse_comparison();
    
    while (
        curr_token().type == TokenType::And
        or curr_token().type == TokenType::Or
        or curr_token().type == TokenType::Is
        or curr_token().type == TokenType::In
    ) {
        auto op_token = skip_token(curr_token().text);
        auto right = parse_comparison(); // 解析右侧比较表达式
        node = std::make_unique<BinaryExpr>(
            curr_token().pos,
            std::move(op_token.text),
            std::move(node),
            std::move(right)
        );
    }
    return node;
}

std::unique_ptr<Expr> Parser::parse_comparison() {
    DEBUG_OUTPUT("parsing comparison...");
    auto node = parse_add_sub();
    while (
        curr_token().type == TokenType::Equal
        or curr_token().type == TokenType::NotEqual
        or curr_token().type == TokenType::Greater
        or curr_token().type == TokenType::Less
        or curr_token().type == TokenType::GreaterEqual
        or curr_token().type == TokenType::LessEqual
    ) {
        auto tok = curr_token();
        auto op = skip_token().text;
        auto right = parse_add_sub();
        node = std::make_unique<BinaryExpr>(
            curr_token().pos,
            std::move(op),
            std::move(node),
            std::move(right)
        );
    }
    return node;
}

std::unique_ptr<Expr> Parser::parse_add_sub() {
    DEBUG_OUTPUT("parsing add/sub...");
    auto node = parse_mul_div_mod();
    while (
        curr_token().type == TokenType::Plus
        or curr_token().type == TokenType::Minus
    ) {
        auto tok = curr_token();
        auto op = skip_token().text;
        auto right = parse_mul_div_mod();
        node = std::make_unique<BinaryExpr>(curr_token().pos, std::move(op), std::move(node), std::move(right));
    }
    return node;
}

std::unique_ptr<Expr> Parser::parse_mul_div_mod() {
    DEBUG_OUTPUT("parsing mul/div/mod...");
    auto node = parse_power();
    while (
        curr_token().type == TokenType::Star
        or curr_token().type == TokenType::Slash
        or curr_token().type == TokenType::Percent
    ) {
        auto tok = curr_token();
        auto op = skip_token().text;
        auto right = parse_power();
        node = std::make_unique<BinaryExpr>(curr_token().pos, std::move(op), std::move(node), std::move(right));
    }
    return node;
}

std::unique_ptr<Expr> Parser::parse_power() {
    DEBUG_OUTPUT("parsing power...");
    auto node = parse_unary();
    if (curr_token().type == TokenType::Caret) {
        auto tok = curr_token();
        auto op = skip_token().text;
        auto right = parse_power();  // 右结合
        node = std::make_unique<BinaryExpr>(curr_token().pos, std::move(op), std::move(node), std::move(right));
    }
    return node;
}

std::unique_ptr<Expr> Parser::parse_unary() {
    DEBUG_OUTPUT("parsing unary...");
    if (curr_token().type == TokenType::Not) {
        auto op_token = skip_token(); // 跳过 not
        auto operand = parse_unary(); // 右结合
        return std::make_unique<UnaryExpr>(
            curr_token().pos,
            std::move(op_token.text),
            std::move(operand)
        );
    }
    if (curr_token().type == TokenType::Minus) {
        skip_token();
        auto operand = parse_unary();
        return std::make_unique<UnaryExpr>(curr_token().pos, "-", std::move(operand));
    }
    if (curr_token().type == TokenType::TripleDot) {
        skip_token();
        auto operand = parse_unary();
        return std::make_unique<UnaryExpr>(curr_token().pos, "...", std::move(operand));
    }
    return parse_factor();
}

std::unique_ptr<Expr> Parser::parse_factor() {
    DEBUG_OUTPUT("parsing factor...");
    auto node = parse_primary();
    if (!node) {
        err::error_reporter(file_path, curr_token().pos, "SyntaxError", "Invalid expression");
    }

    while (true) {
        if (curr_token().type == TokenType::TripleDot) {
            auto tok = curr_token();

            skip_token("...");
            auto end_range = parse_primary();
            std::vector<std::unique_ptr<Expr>> args;
            args.push_back(std::move(node));
            args.push_back(std::move(end_range));
            node = std::make_unique<CallExpr>(tok.pos,
                std::make_unique<IdentifierExpr>(tok.pos, "Range"),
                std::move(args)
            );
        }
        else if (curr_token().type == TokenType::Dot) {
            auto tok = curr_token();

            skip_token(".");
            auto child = std::make_unique<IdentifierExpr>(tok.pos, skip_token().text);
            node = std::make_unique<GetMemberExpr>(tok.pos, std::move(node),std::move(child));

        }
        else if (curr_token().type == TokenType::LBracket) {
            auto tok = curr_token();

            skip_token("[");
            auto param = parse_args(TokenType::RBracket);
            skip_token("]");
            node = std::make_unique<GetItemExpr>(tok.pos, std::move(node),std::move(param));
        }
        else if (curr_token().type == TokenType::LParen) {
            auto tok = curr_token();
            skip_token("(");
            auto param = parse_args(TokenType::RParen);
            skip_token(")");
            node = std::make_unique<CallExpr>(tok.pos, std::move(node),std::move(param));
        }
        else break;
    }
    return node;
}

std::unique_ptr<Expr> Parser::parse_primary() {
    DEBUG_OUTPUT("parsing primary...");
    const auto tok = skip_token();
    // 处理f-string解析
    if (tok.type == TokenType::FStringStart) {

        std::unique_ptr<Expr> combined_expr = nullptr;

        // 遍历f-string内部Token，直到FStringEnd
        while (curr_token().type != TokenType::FStringEnd) {
            if (curr_token().type == TokenType::String) {
                // 解析字符串片段
                auto str_tok = skip_token();
                auto str_expr = std::make_unique<StringExpr>(str_tok.pos, str_tok.text);

                // 拼接加法表达式
                if (combined_expr == nullptr) {
                    combined_expr = std::move(str_expr);
                } else {
                    combined_expr = std::make_unique<BinaryExpr>(
                        str_tok.pos,
                        "+",
                        std::move(combined_expr),
                        std::move(str_expr)
                    );
                }
            } else if (curr_token().type == TokenType::InsertExprStart) {
                // 解析插入表达式
                const auto insert_expr_start_tok = skip_token(); // 跳过InsertExprStart
                const auto insert_expr = skip_token();

                // 递归解析表达式（支持任意合法表达式）
                // 转Str
                Lexer lexer(file_path);
                Parser parser(file_path);
                lexer.prepare(insert_expr.text, insert_expr.pos.lno_start, insert_expr.pos.col_start);
                auto tokens = lexer.tokenize();
                auto ast = parser.parse(tokens);

                assert(ast != nullptr);
                auto expr_ast = dynamic_cast<ExprStmt*>(ast->statements.back().get());
                assert(expr_ast != nullptr);

                std::unique_ptr<Expr> sub_expr = std::move(expr_ast->expr);

                auto args = std::vector<std::unique_ptr<Expr>> ();
                args.emplace_back(std::move(sub_expr));
                auto expr = std::make_unique<CallExpr>(
                    insert_expr_start_tok.pos,
                    std::make_unique<IdentifierExpr>(insert_expr_start_tok.pos, "Str"),
                    std::move(args)
                );

                // 跳过InsertExprEnd
                if (curr_token().type != TokenType::InsertExprEnd) {
                    err::error_reporter(file_path, curr_token().pos, "SyntaxError", "Missing '}' in f-string");
                }
                skip_token();

                // 拼接加法表达式
                if (combined_expr == nullptr) {
                    combined_expr = std::move(expr);
                } else {
                    combined_expr = std::make_unique<BinaryExpr>(
                        expr->pos,
                        "+",
                        std::move(combined_expr),
                        std::move(expr)
                    );
                }
            } else {
                err::error_reporter(file_path, curr_token().pos, "SyntaxError", "Invalid token in f-string");
            }
        }

        // 跳过FStringEnd Token
        skip_token();

        // 空f-string返回空字符串
        if (combined_expr == nullptr) {
            return std::make_unique<StringExpr>(tok.pos, "");
        }

        return combined_expr;
    }
    if (tok.type == TokenType::Number) {
        return std::make_unique<NumberExpr>(tok.pos, tok.text);
    }
    if (tok.type == TokenType::Decimal) {
        return std::make_unique<DecimalExpr>(tok.pos, tok.text);
    }
    if (tok.type == TokenType::String) {
        return std::make_unique<StringExpr>(tok.pos, tok.text);
    }
    if (tok.type == TokenType::Nil) {
        return std::make_unique<NilExpr>(tok.pos);
    }
    if (tok.type == TokenType::True) {
        return std::make_unique<BoolExpr>(tok.pos, true);
    }
    if (tok.type == TokenType::False) {
        return std::make_unique<BoolExpr>(tok.pos, false);
    }
    if (tok.type == TokenType::Identifier) {
        return std::make_unique<IdentifierExpr>(tok.pos, tok.text);
    }
    if (tok.type == TokenType::Func) {
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
                    skip_token(")");  // 跳过右括号
                    break;
                }
                func_params.push_back(skip_token().text);
                // 处理参数间的逗号
                if (curr_token().type == TokenType::Comma) {
                    skip_token(",");
                    skip_end_of_lines();
                } else if (curr_token().type != TokenType::RParen) {
                    err::error_reporter(file_path, curr_token().pos, "SyntaxError", "Mismatched function parameters");
                }
            }
            skip_token(")");  // 跳过右括号
        }

        // 解析函数体（无大括号，用end结尾）
        skip_start_of_block();  // 跳过参数后的换行
        auto func_body = parse_block();
        skip_token("end");  // 特殊处理
        return std::make_unique<LambdaExpr>(curr_token().pos,
            "<lambda>",
            std::move(func_params),
            std::move(func_body),
            has_rest_params
            );
    }
    if (tok.type == TokenType::Pipe) {
        std::vector<std::string> params;
        while (curr_token().type != TokenType::Pipe) {
            params.emplace_back(skip_token().text);
            if (curr_token().type == TokenType::Comma) skip_token(",");
        }
        skip_token("|");
        skip_end_of_lines();
        auto expr = parse_expression();
        std::vector<std::unique_ptr<Stmt>> stmts;
        stmts.emplace_back(std::make_unique<ReturnStmt>(curr_token().pos, std::move(expr)));

        return std::make_unique<LambdaExpr>(
            curr_token().pos,
            "lambda",
            std::move(params),
            std::make_unique<BlockStmt>(curr_token().pos, std::move(stmts)),
            false
        );
    }
    if (tok.type == TokenType::LBrace) {
        skip_end_of_lines();

        decltype(DictExpr::elements) init_vec{};
        while (curr_token().type != TokenType::RBrace) {
            DEBUG_OUTPUT("parse dict item");
            auto key = parse_expression();
            skip_token(":");
            auto val = parse_expression();

            while(curr_token().type == TokenType::EndOfLine)
                skip_token(); // 直接跳过换行

            if (curr_token().type == TokenType::Comma) {
                skip_token(",");
                skip_end_of_lines();
            }
            else if (curr_token().type == TokenType::RBrace) {
                init_vec.emplace_back(std::move(key), std::move(val));
                break;
            }
            else err::error_reporter(file_path, curr_token().pos, "SyntaxError", "sep of dict must be ',' or ';'");

            init_vec.emplace_back(std::move(key), std::move(val));
        }
        skip_token("}");
        DEBUG_OUTPUT("finish parse dict");
        return std::make_unique<DictExpr>(curr_token().pos, std::move(init_vec));
    }
    if (tok.type == TokenType::LBracket) {
        auto param = parse_args(TokenType::RBracket);
        skip_token("]");
        return std::make_unique<ListExpr>(curr_token().pos, std::move(param));
    }
    if (tok.type == TokenType::LParen) {
        auto expr = parse_expression();
        skip_token(")");
        return expr;
    }
    return nullptr;
}

std::vector<std::unique_ptr<Expr>> Parser::parse_args(const TokenType endswith){
    std::vector<std::unique_ptr<Expr>> params;
    while (curr_token().type != endswith) {
        auto expr = parse_expression();
        if (! expr)
            err::error_reporter(file_path, curr_token().pos, "SyntaxError", "Unclosed argument list");
        params.emplace_back(std::move(expr));
        if (curr_token().type == TokenType::Comma) {
            skip_token(",");
            skip_end_of_lines();
        } else if (curr_token().type != endswith) {
            err::error_reporter(file_path, curr_token().pos, "SyntaxError", "Unclosed argument list");
        }
    }
    return params;
}

}
