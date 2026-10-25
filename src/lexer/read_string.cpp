/**
 * @file read_string.cpp
 * @brief 词法分析器读取字符串部分
 * @author azhz1107cat
 * @date 2026-2-8
 */

#include "lexer.hpp"

namespace kiz {
void Lexer::read_string() {
    dep::UTF8Char quote_char = src_[char_pos_];

    size_t start_char = char_pos_;
    size_t start_lno = lineno_;
    size_t start_col = col_;

    next(); // 跳过引号

    bool closed = false;
    dep::UTF8String raw_str;

    // 消费字符串内容
    while (char_pos_ < src_.size()) {
        dep::UTF8Char c = src_[char_pos_];

        // 普通字符串允许跨行

        if (c == quote_char) {
            closed = true;  // 取消注释这行
            next(); // 跳过闭合引号
            break;
        }

        if (c == '\\' && char_pos_ + 1 < src_.size()) {
            raw_str += c;
            next(); // 跳过转义符
            c = src_[char_pos_];
            raw_str += c;
            next(); // 消费转义后的字符
            continue;
        }

        raw_str += c;
        next();
    }

    std::string raw = raw_str.to_string();
    std::string content = handle_escape(raw);

    if (!closed) {
            err::error_reporter(file_path_, {start_lno, prev_lineno_, start_col, prev_col_},
                              "SyntaxError", "Unclosed string literal");
    }

    tokens_.emplace_back(TokenType::String, content, start_lno, lineno_, start_col, col_ - 1);
    curr_state_ = LexState::Start;
}

void Lexer::read_fstring() {
    size_t start_char = char_pos_;
    size_t start_lno = lineno_;
    size_t start_col = col_;

    dep::UTF8Char prefix = src_[char_pos_]; // 'f' 或 'F'
    next(); // 消费 'f' 或 'F'

    dep::UTF8Char quote_char = src_[char_pos_]; // 外部f-string的引号
    next(); // 消费引号

    // 生成 FStringStart token
    size_t fstring_start_char = start_char;
    size_t fstring_end_char = char_pos_; // 引号之后的位置
    emit_token(TokenType::FStringStart, fstring_start_char, fstring_end_char,
               start_lno, start_col, lineno_, col_ - 1);

    bool in_expr = false;
    bool fstring_closed = false; // 外层引号是否闭合
    size_t expr_start_char = 0;
    size_t expr_start_lno = 0;
    size_t expr_start_col = 0;
    int brace_depth = 0;
    bool in_string = false;        // 标记是否在表达式内的字符串中
    dep::UTF8Char string_quote;    // 表达式内字符串的引号类型
    bool escape_next = false;      // 标记下一个字符是否转义

    while (char_pos_ < src_.size()) {
        dep::UTF8Char c = src_[char_pos_];

        // 如果在表达式内的字符串中
        if (in_expr && in_string) {
            if (escape_next) {
                // 转义字符，跳过
                escape_next = false;
                next();
                continue;
            }

            if (c == '\\') {
                // 遇到转义符
                escape_next = true;
                next();
                continue;
            }

            if (c == string_quote) {
                // 字符串结束
                in_string = false;
                next();
                continue;
            }

            // 字符串内容，正常消费
            next();
            continue;
        }

        // 检查是否结束外部f-string
        if (c == quote_char && !in_expr) {
            // 生成 FStringEnd token
            size_t end_char = char_pos_;
            emit_token(TokenType::FStringEnd, end_char, end_char + 1,
                       lineno_, col_, lineno_, col_);
            next(); // 消费结束引号
            fstring_closed = true;
            break;
        }

        // 表达式内部的转义符由字符串状态机自行处理，这里无需干预

        // 检查是否进入表达式
        if (c == '{' && !in_expr) {
            // 生成 InsertExprStart token
            size_t expr_start = char_pos_;
            emit_token(TokenType::InsertExprStart, expr_start, expr_start + 1,
                       lineno_, col_, lineno_, col_);
            next(); // 消费 '{'

            in_expr = true;
            brace_depth = 1;
            expr_start_char = char_pos_; // 表达式内容的开始
            expr_start_lno = lineno_;
            expr_start_col = col_;

            continue;
        }

        // 检查是否结束表达式
        if (c == '}' && in_expr && brace_depth == 1 && !in_string) {
            // 生成表达式标识符token
            if (expr_start_char < char_pos_) { // 表达式不为空
                std::string expr_text;
                for (size_t i = expr_start_char; i < char_pos_; i++) {
                    expr_text += src_[i].to_string();
                }

                // 生成 Identifier token
                emit_token(TokenType::Identifier, expr_start_char, char_pos_,
                           expr_start_lno, expr_start_col, lineno_, col_ - 1);
            }

            // 生成 InsertExprEnd token
            size_t expr_end = char_pos_;
            emit_token(TokenType::InsertExprEnd, expr_end, expr_end + 1,
                       lineno_, col_, lineno_, col_);
            next(); // 消费 '}'

            in_expr = false;
            brace_depth = 0;
            continue;
        }

        // 处理表达式内的字符串引号
        if (in_expr && (c == '"' || c == '\'')) {
            // 进入表达式内的字符串
            in_string = true;
            string_quote = c;
            next(); // 消费引号
            continue;
        }

        // 处理表达式内的嵌套 '{'
        if (c == '{' && in_expr) {
            brace_depth++;
            next();
            continue;
        }

        // 处理表达式内的嵌套 '}'
        if (c == '}' && in_expr) {
            brace_depth--;
            next();
            continue;
        }

        // 普通字符串内容（不在表达式中）
        if (!in_expr) {
            size_t str_start = char_pos_;
            size_t str_start_lno = lineno_;
            size_t str_start_col = col_;

            // 收集直到下一个 '{' 或引号；转义序列成对纳入原文，稍后统一解释
            while (char_pos_ < src_.size()) {
                dep::UTF8Char ch = src_[char_pos_];
                if (ch == quote_char || ch == '{') {
                    break;
                }
                if (ch == '\\') {
                    next(); // 反斜杠
                    if (char_pos_ < src_.size()) {
                        next(); // 转义字符原样保留
                    }
                    continue;
                }
                next();
            }

            // 如果有字符串内容，生成 String token（转义序列在此解释）
            if (char_pos_ > str_start) {
                std::string str_text;
                for (size_t i = str_start; i < char_pos_; i++) {
                    str_text += src_[i].to_string();
                }

                tokens_.emplace_back(TokenType::String, handle_escape(str_text),
                    str_start_lno, lineno_, str_start_col, col_ - 1);
            }
        } else {
            // 在表达式中（不在字符串中），继续读取
            next();
        }
    }

    // 到达EOF仍未闭合f-string本体：与普通字符串一样在词法层给出明确报错
    if (!fstring_closed) {
        err::error_reporter(file_path_, {start_lno, prev_lineno_, start_col, prev_col_},
                          "SyntaxError", "Unclosed f-string literal");
    }

    // 检查未闭合的表达式
    if (in_expr) {
        err::error_reporter(file_path_, {expr_start_lno, prev_lineno_, expr_start_col, prev_col_},
                          "SyntaxError", "Unclosed f-string expression");
    }

    curr_state_ = LexState::Start;
}

}
