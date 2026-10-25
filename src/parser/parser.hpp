/**
 * @file parser.hpp
 * @brief 语法分析器（Parser）核心定义
 * @author azhz1107cat
 * @date 2025-10-25
 */

#pragma once
#include "ast.hpp"
#include "../lexer/lexer.hpp"

#include <memory>
#include <string>

namespace kiz {

class Parser {
    std::vector<Token> tokens_;
    size_t curr_tok_idx_ = 0;
    const std::string& file_path;
    // 嵌套深度为线程局部共享计数：f-string子表达式会新建Parser实例，
    // 每实例独立计数会被嵌套f-string绕过（每层都从0数起），共享计数才能封顶总深度
    static thread_local size_t nest_depth_;
    // 允许的最大嵌套深度
    static constexpr size_t max_nest_depth_ = 1000;

    // RAII嵌套深度守卫：超深嵌套报语法错误而不是打爆C++栈
    struct NestGuard {
        Parser& p;
        explicit NestGuard(Parser& parser) : p(parser) {
            ++nest_depth_;
            if (nest_depth_ > max_nest_depth_) {
                err::error_reporter(p.file_path, p.curr_token().pos, "SyntaxError",
                    "Nesting too deep");
            }
        }
        ~NestGuard() { --nest_depth_; }
        NestGuard(const NestGuard&) = delete;
        NestGuard& operator=(const NestGuard&) = delete;
    };
public:
    explicit Parser(const std::string& file_path) : file_path(file_path) {}
    ~Parser() = default;

    Token skip_token(const std::string& want_skip = "");
    void skip_end_of_lines();
    void skip_end_of_stmt();
    void skip_end();
    void skip_start_of_block();
    [[nodiscard]] Token curr_token() const;

    std::unique_ptr<BlockStmt> parse(const std::vector<Token>& tokens);

private:
    void skip_type_unit();

    void skip_fn_type_comment();
    void skip_var_type_comment();
    // parse stmt
    std::unique_ptr<Stmt> parse_stmt();
    std::unique_ptr<BlockStmt> parse_block(TokenType endswith = TokenType::End);
    std::unique_ptr<IfStmt> parse_if();

    // parse expr
    std::unique_ptr<Expr> parse_expression();
    std::unique_ptr<Expr> parse_and_or();
    std::unique_ptr<Expr> parse_and();
    std::unique_ptr<Expr> parse_in_is();
    std::unique_ptr<Expr> parse_comparison();
    std::unique_ptr<Expr> parse_add_sub();
    std::unique_ptr<Expr> parse_mul_div_mod();
    std::unique_ptr<Expr> parse_power();
    std::unique_ptr<Expr> parse_unary();
    std::unique_ptr<Expr> parse_factor();

    // parse factor
    std::unique_ptr<Expr> parse_primary();
    std::vector<std::unique_ptr<Expr>> parse_args(TokenType endswith);
};

} // namespace kiz