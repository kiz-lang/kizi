/**
 * @file parser.cpp
 * @brief 语法分析器（Parser）核心实现
 * 从Token列表生成AST（适配函数定义新语法：fn x() end）
 * @author azhz1107cat
 * @date 2025-10-25
 */

#include "parser.hpp"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <iostream>
#include <memory>
#include <vector>

#include "../kiz.hpp"
#include "../repl/color.hpp"

namespace kiz {

thread_local size_t Parser::nest_depth_ = 0;

Token Parser::skip_token(const std::string& want_skip) {
    DEBUG_OUTPUT("skipping token: index " + std::to_string(curr_tok_idx_));

    // 边界检查
    if (curr_tok_idx_ >= tokens_.size()) {
        throw KizStopRunningSignal("skip_token: Index out of range");
    }

    const Token& curr_tok = tokens_[curr_tok_idx_];
    // 无目标文本：直接跳过当前 Token
    if (want_skip.empty()) {
        ++curr_tok_idx_;
        return curr_tok;
    }

    // 优先按文本匹配
    if (curr_tok.text == want_skip) {
        ++curr_tok_idx_;
        return curr_tok;
    }

    // 严格报错
    err::error_reporter(file_path, curr_token().pos, "SyntaxError", "Invalid grammar: " + curr_token().text);
}

// curr_token实现
Token Parser::curr_token() const {
    if (curr_tok_idx_ < tokens_.size()) {
        return tokens_.at(curr_tok_idx_);
    }
    auto end_of_file = tokens_.back();
    assert(end_of_file.type == TokenType::EndOfFile);
    return end_of_file;
}

// skip_end_of_stmt实现
void Parser::skip_end_of_stmt() {
    DEBUG_OUTPUT("skipping end of line...");
    const Token curr_tok = curr_token();
    // 支持分号或换行作为语句结束符
    if (curr_tok.type == TokenType::Semicolon) {
        while (curr_token().type == TokenType::Semicolon) {
            skip_token(";");
        }
        return;
    }
    if (curr_tok.type == TokenType::EndOfLine) {
        skip_end_of_lines();
        return;
    }
    // 到达文件末尾也视为合法结束
    if (curr_tok.type == TokenType::EndOfFile) {
        DEBUG_OUTPUT("end of the file");
        skip_token();
        return;
    }
    err::error_reporter(file_path, curr_tok.pos, "SyntaxError", "Invalid statement terminator");
}

// skip_end实现
void Parser::skip_end() {
    DEBUG_OUTPUT("skipping end of line...");
    skip_token("end");
    const Token curr_tok = curr_token();
    // 支持分号或换行作为语句结束符
    if (curr_tok.type == TokenType::Semicolon) {
        while (curr_token().type == TokenType::Semicolon) {
            skip_token(";");
        }
        return;
    }
    if (curr_tok.type == TokenType::EndOfLine) {
        skip_end_of_lines();
        return;
    }
    // 到达文件末尾也视为合法结束
    if (curr_tok.type == TokenType::EndOfFile) {
        DEBUG_OUTPUT("end of the file");
        skip_token();
        return;
    }
}

// skip_start_of_block实现 处理函数体前置换行
void Parser::skip_start_of_block() {
    DEBUG_OUTPUT("skipping start of block...");
    const Token curr_tok = curr_token();
    // if (curr_tok.type == TokenType::Colon) {
    //     skip_token(":");
    //     return;
    // }
    if (curr_tok.type == TokenType::EndOfLine) {
        skip_end_of_lines();
        return;
    }
}

void Parser::skip_end_of_lines() {
    while (curr_token().type == TokenType::EndOfLine) {
        skip_token("\n");
    }
}

// parse_program实现（解析整个程序
std::unique_ptr<BlockStmt> Parser::parse(const std::vector<Token>& tokens) {
    tokens_ = tokens;
    curr_tok_idx_ = 0;
    DEBUG_OUTPUT("parsing...");
    std::vector<std::unique_ptr<Stmt>> program_stmts;

    // 打印 Token 序列（保留调试逻辑）
    // std::cout << "=== 所有 Token 序列 ===" << std::endl;
    // for (size_t i = 0; i < tokens.size(); i++) {
    //     const auto& tok = tokens[i];
    //     std::cout << (
    //         "Token[" + std::to_string(i) + "]: type=" + std::to_string(static_cast<int>(tok.type))
    //         + ", text='" + tok.text + "', line=" + std::to_string(tok.pos.lno_start)
    //     ) << std::endl;
    // }
    // std::cout << "=== Token 序列结束 ===" << std::endl;

    // 全局块解析：直到 EOF
    while (curr_token().type != TokenType::EndOfFile) {
        // 跳过前置换行（仅清理，不处理语句）
        skip_end_of_lines();
        while (curr_token().type == TokenType::TripleDot) {
            skip_token("...");
            skip_end_of_lines();
        }
        skip_end_of_lines();
        if (curr_token().type == TokenType::EndOfFile) break;
        auto stmt = parse_stmt();
        if (stmt) {
            program_stmts.push_back(std::move(stmt));
        } else {
            err::error_reporter(file_path, curr_token().pos, "SyntaxError","Invalid syntax");
        }
    }

    DEBUG_OUTPUT("end parsing");
    return std::make_unique<BlockStmt>(tokens_.front().pos, std::move(program_stmts));
}

} // namespace kiz