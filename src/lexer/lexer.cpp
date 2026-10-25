/**
 * @file lexer.cpp
 * @brief 词法分析器（FSM）核心实现 - 基于UTF-8 UTF8Char/UTF8String
 * @author azhz1107cat
 * @date 2025-10-25 + 2026-01-31 重构 + 修复UTF8Char使用
 */
#include "lexer.hpp"
#include <algorithm>
#include <cassert>

#include "lexer.hpp"
#include "error/error_reporter.hpp"
#include <algorithm>

namespace kiz {

// 初始化关键字
void Lexer::init_keywords() {
    if (!keywords_.empty()) return;
    keywords_ = {
        {"if", TokenType::If},
        {"else", TokenType::Else},
        {"while", TokenType::While},
        {"for", TokenType::For},
        {"break", TokenType::Break},
        {"next", TokenType::Next},
        {"try", TokenType::Try},
        {"catch", TokenType::Catch},
        {"throw", TokenType::Throw},
        {"import", TokenType::Import},
        {"nonlocal", TokenType::Nonlocal},
        {"global", TokenType::Global},
        {"fn", TokenType::Func},
        {"object", TokenType::Object},
        {"return", TokenType::Return},
        {"end", TokenType::End},
        {"True", TokenType::True},
        {"False", TokenType::False},
        {"Nil", TokenType::Nil},
        {"and", TokenType::And},
        {"or", TokenType::Or},
        {"not", TokenType::Not},
        {"is", TokenType::Is},
        {"in", TokenType::In},
        {"at", TokenType::At}
    };
}

// 读取当前字符并移动位置
dep::UTF8Char Lexer::next() {
    if (char_pos_ >= src_.size()) return {'\0'};

    prev_lineno_ = lineno_;
    prev_col_ = col_;

    dep::UTF8Char ch = src_[char_pos_];
    char_pos_++;

    if (ch == '\n') {
        lineno_++;
        col_ = 1;
    } else if (ch != '\r') {
        col_++;
    }

    return ch;
}

// 处理字符串转义
std::string Lexer::handle_escape(const std::string& raw) {
    std::string res;
    res.reserve(raw.size());

    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\\' && i + 1 < raw.size()) {
            switch (raw[++i]) {
            case 'n': res += '\n'; break;
            case 't': res += '\t'; break;
            case 'r': res += '\r'; break;
            case '\\': res += '\\'; break;
            case '"': res += '"'; break;
            case '\'': res += '\''; break;
            case '0': res += '\0'; break;
            case 'u': {
                // \uXXXX：4位十六进制Unicode码点，编码为UTF-8
                if (i + 4 < raw.size()) {
                    uint32_t code_point = 0;
                    bool valid = true;
                    for (size_t j = 1; j <= 4; ++j) {
                        char h = raw[i + j];
                        code_point <<= 4;
                        if (h >= '0' && h <= '9') code_point |= static_cast<uint32_t>(h - '0');
                        else if (h >= 'a' && h <= 'f') code_point |= static_cast<uint32_t>(h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') code_point |= static_cast<uint32_t>(h - 'A' + 10);
                        else { valid = false; break; }
                    }
                    if (valid) {
                        // 拒绝UTF-16代理区码点（孤立代理编码出的是非法UTF-8）
                        if (code_point >= 0xD800 and code_point <= 0xDFFF) {
                            valid = false;
                        }
                    }
                    if (valid) {
                        // 按码点长度编码UTF-8
                        if (code_point < 0x80) {
                            res += static_cast<char>(code_point);
                        } else if (code_point < 0x800) {
                            res += static_cast<char>(0xC0 | (code_point >> 6));
                            res += static_cast<char>(0x80 | (code_point & 0x3F));
                        } else if (code_point < 0x10000) {
                            res += static_cast<char>(0xE0 | (code_point >> 12));
                            res += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
                            res += static_cast<char>(0x80 | (code_point & 0x3F));
                        } else {
                            res += static_cast<char>(0xF0 | (code_point >> 18));
                            res += static_cast<char>(0x80 | ((code_point >> 12) & 0x3F));
                            res += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
                            res += static_cast<char>(0x80 | (code_point & 0x3F));
                        }
                        i += 4;
                        break;
                    }
                }
                // 十六进制不完整：按原样保留
                res += '\\';
                res += 'u';
                break;
            }
            default:
                res += '\\';  // 保留转义符
                res += raw[i]; // 和未知字符
                break;
            }
        } else {
            res += raw[i];
        }
    }
    return res;
}

// 生成Token：按字符索引范围取原始字节
void Lexer::emit_token(TokenType type, size_t start_char, size_t end_char,
                       size_t start_lno, size_t start_col,
                       size_t end_lno, size_t end_col) {
    dep::UTF8String substr;
    for (size_t i = start_char; i < end_char && i < src_.size(); ++i) {
        substr += src_[i];
    }

    std::string text = substr.to_string();
    tokens_.emplace_back(type, text, start_lno, end_lno, start_col, end_col);
}

// 快速生成单字符Token
void Lexer::emit_single_cp_token(TokenType type, size_t char_index) {
    // 注意：char_index应该是消费前的索引
    size_t current_char = char_index;
    emit_token(type, current_char, current_char + 1,
               lineno_, col_ - 1, lineno_, col_ - 1);
}

// 核心：有限自动状态机 词法分析
std::vector<Token> Lexer::tokenize() {
    while (char_pos_ < src_.size()) {
        dep::UTF8Char current_char = src_[char_pos_];

        switch (curr_state_) {
        // ======================================
        // 初始状态：核心分支
        // ======================================
        case LexState::Start: {
            if (current_char.is_space()) {
                // 空白符
                if (current_char == '\n') {
                    bool has_bs = !tokens_.empty() && tokens_.back().type == TokenType::Backslash;
                    if (!has_bs) {
                        emit_single_cp_token(TokenType::EndOfLine, char_pos_);
                    } else {
                        tokens_.pop_back(); // 移除续行符
                    }
                }
                next(); // 消费空白符
            }
            else if ((current_char == 'f' || current_char == 'F') &&
                    char_pos_ + 1 < src_.size() &&
                    (src_[char_pos_ + 1] == '"' || src_[char_pos_ + 1] == '\'')) {
                curr_state_ = LexState::FString;
             }
            else if (current_char.is_alpha() || current_char.to_cod_point() == U'_') {
                curr_state_ = LexState::Identifier;
            }
            else if (current_char.is_digit() || (current_char == '.' && char_pos_ + 1 < src_.size() &&
                     src_[char_pos_ + 1].is_digit() )) {
                curr_state_ = LexState::Number;
            }
            else if (current_char == '#') {
                curr_state_ = LexState::SingleComment;
            }
            else if (current_char == '/' && char_pos_ + 1 < src_.size() &&
                     src_[char_pos_ + 1] == '*') {
                curr_state_ = LexState::BlockComment;
            }
            else if (current_char == '"' || current_char == '\'') {
                curr_state_ = LexState::String;
            }
            else if (current_char == '=' || current_char == '!' ||
                     current_char == '<' || current_char == '>' ||
                     current_char == '-' || current_char == ':') {
                curr_state_ = LexState::Operator;
            }
            else {
                // 单字符Token处理
                size_t start_pos = char_pos_;
                if (current_char == '(') {
                    next();
                    emit_token(TokenType::LParen, start_pos, char_pos_,
                              lineno_, col_ - 1, lineno_, col_ - 1);
                }
                else if (current_char == ')') {
                    next();
                    emit_token(TokenType::RParen, start_pos, char_pos_,
                              lineno_, col_ - 1, lineno_, col_ - 1);
                }
                else if (current_char == '{') {
                    next();
                    emit_token(TokenType::LBrace, start_pos, char_pos_,
                              lineno_, col_ - 1, lineno_, col_ - 1);
                }
                else if (current_char == '}') {
                    next();
                    emit_token(TokenType::RBrace, start_pos, char_pos_,
                              lineno_, col_ - 1, lineno_, col_ - 1);
                }
                else if (current_char == '[') {
                    next();
                    emit_token(TokenType::LBracket, start_pos, char_pos_,
                              lineno_, col_ - 1, lineno_, col_ - 1);
                }
                else if (current_char == ']') {
                    next();
                    emit_token(TokenType::RBracket, start_pos, char_pos_,
                              lineno_, col_ - 1, lineno_, col_ - 1);
                }
                else if (current_char == ',') {
                    next();
                    emit_token(TokenType::Comma, start_pos, char_pos_,
                              lineno_, col_ - 1, lineno_, col_ - 1);
                }
                else if (current_char == ';') {
                    next();
                    emit_token(TokenType::Semicolon, start_pos, char_pos_,
                              lineno_, col_ - 1, lineno_, col_ - 1);
                }
                else if (current_char == '+') {
                    next();
                    emit_token(TokenType::Plus, start_pos, char_pos_,
                              lineno_, col_ - 1, lineno_, col_ - 1);
                }
                else if (current_char == '*') {
                    next();
                    emit_token(TokenType::Star, start_pos, char_pos_,
                              lineno_, col_ - 1, lineno_, col_ - 1);
                }
                else if (current_char == '\\') {
                    next();
                    emit_token(TokenType::Backslash, start_pos, char_pos_,
                              lineno_, col_ - 1, lineno_, col_ - 1);
                }
                else if (current_char == '%') {
                    next();
                    emit_token(TokenType::Percent, start_pos, char_pos_,
                              lineno_, col_ - 1, lineno_, col_ - 1);
                }
                else if (current_char == '^') {
                    next();
                    emit_token(TokenType::Caret, start_pos, char_pos_,
                              lineno_, col_ - 1, lineno_, col_ - 1);
                }
                else if (current_char == '|') {
                    next();
                    emit_token(TokenType::Pipe, start_pos, char_pos_,
                              lineno_, col_ - 1, lineno_, col_ - 1);
                }
                else if (current_char == '/') {
                    next();
                    emit_token(TokenType::Slash, start_pos, char_pos_,
                              lineno_, col_ - 1, lineno_, col_ - 1);
                }
                else if (current_char == '.') {
                    next(); // 消费第一个点
                    if (char_pos_ < src_.size() && src_[char_pos_] == '.' &&
                        char_pos_ + 1 < src_.size() && src_[char_pos_ + 1] == '.') {
                        next(); // 消费第二个点
                        next(); // 消费第三个点
                        emit_token(TokenType::TripleDot, start_pos, char_pos_,
                                  lineno_, col_ - 3, lineno_, col_);
                    } else {
                        emit_token(TokenType::Dot, start_pos, char_pos_,
                                      lineno_, col_ - 1, lineno_, col_ - 1);
                    }
                }
                else {
                    // 未知字符：错误报告
                    err::error_reporter(file_path_, {lineno_, lineno_, col_, col_},
                                      "SyntaxError", "Unknown character");
                    next();
                    emit_token(TokenType::Unknown, start_pos, char_pos_,
                              lineno_, col_ - 1, lineno_, col_ - 1);
                }
            }
            break;
        }

        // 模板字符串状态 f"/F"
        case LexState::FString: {
            read_fstring();
            break;
        }

        // 普通字符串状态：""/''
        case LexState::String: {
            read_string();
            break;
        }

        // 数字状态：整数/小数/科学计数法
        case LexState::Number: {
            read_num();
            break;
        }

        // 标识符/关键字状态
        case LexState::Identifier: {
            size_t start_char = char_pos_;
            size_t start_lno = lineno_;
            size_t start_col = col_;

            dep::UTF8String ident_str;

            // 消费第一个字符（已经在当前状态）
            ident_str += src_[char_pos_];
            next();

            // 消费标识符后续字符
            while (char_pos_ < src_.size() &&
                   (src_[char_pos_].is_alnum() || src_[char_pos_].to_cod_point() == U'_')) {
                ident_str += src_[char_pos_];
                next();
            }

            std::string ident = ident_str.to_string();
            TokenType type = keywords_.contains(ident) ? keywords_[ident] : TokenType::Identifier;

            emit_token(type, start_char, char_pos_, start_lno, start_col, lineno_, col_ - 1);
            curr_state_ = LexState::Start;
            break;
        }

        // 运算符状态：处理双字符运算符
        case LexState::Operator: {
            size_t start_char = char_pos_;
            size_t start_lno = lineno_;
            size_t start_col = col_;

            dep::UTF8Char c1 = src_[char_pos_];
            next(); // 消费第一个字符

            TokenType type = TokenType::Unknown;
            bool manual_emitted = false; // :=别名分支手工产出token的标志
            size_t token_end = char_pos_;

            // 检查第二个字符
            if (char_pos_ < src_.size()) {
                dep::UTF8Char c2 = src_[char_pos_];

                // 匹配双字符运算符
                if (c1 == '=' && c2 == '>') {
                    type = TokenType::FatArrow;
                    next(); // 消费第二个字符
                    token_end = char_pos_;
                }
                else if (c1 == '-' && c2 == '>') {
                    type = TokenType::ThinArrow;
                    next(); // 消费第二个字符
                    token_end = char_pos_;
                }
                else if (c1 == '=' && c2 == '=') {
                    type = TokenType::Equal;
                    next(); // 消费第二个字符
                    token_end = char_pos_;
                }
                else if (c1 == '!' && c2 == '=') {
                    type = TokenType::NotEqual;
                    next(); // 消费第二个字符
                    token_end = char_pos_;
                }
                else if (c1 == '<' && c2 == '=') {
                    type = TokenType::LessEqual;
                    next(); // 消费第二个字符
                    token_end = char_pos_;
                }
                else if (c1 == '>' && c2 == '=') {
                    type = TokenType::GreaterEqual;
                    next(); // 消费第二个字符
                    token_end = char_pos_;
                }
                else if (c1 == ':' && c2 == '=') {
                    // := 是 = 的别名写法：token文本统一为"="，与语法层按文本匹配保持一致
                    next(); // 消费第二个字符
                    tokens_.emplace_back(TokenType::Assign, "=", start_lno, start_col);
                    manual_emitted = true; // 已手工产token，跳过通用emit
                }
            }

            // 单字符运算符（:=已手工产出时跳过）
            if (type == TokenType::Unknown and !manual_emitted) {
                token_end = char_pos_; // 只消费了一个字符
                if (c1 == '=') type = TokenType::Assign;
                else if (c1 == '!') type = TokenType::ExclamationMark;
                else if (c1 == '<') type = TokenType::Less;
                else if (c1 == '>') type = TokenType::Greater;
                else if (c1 == ':') type = TokenType::Colon;
                else if (c1 == '-') type = TokenType::Minus;
            }

            if (!manual_emitted) {
                emit_token(type, start_char, token_end, start_lno, start_col, lineno_, col_ - 1);
            }
            curr_state_ = LexState::Start;
            break;
        }

        // 单行注释状态：# 至行尾
        case LexState::SingleComment: {
            next(); // 跳过#

            // 消费至行尾
            while (char_pos_ < src_.size() && src_[char_pos_] != '\n') {
                next();
            }

            curr_state_ = LexState::Start;
            break;
        }

        // 块注释状态：/* */ 支持跨行
        case LexState::BlockComment: {
            const size_t comment_start_lno = lineno_;
            const size_t comment_start_col = col_;
            next(); // 跳过/
            next(); // 跳过*

            bool comment_closed = false;
            // 消费至*/
            while (char_pos_ < src_.size()) {
                if (char_pos_ + 1 < src_.size() &&
                    src_[char_pos_] == '*' &&
                    src_[char_pos_ + 1] == '/') {
                    next(); // 跳过*
                    next(); // 跳过/
                    comment_closed = true;
                    break;
                }
                next();
            }

            // 到达EOF仍未闭合：显式报错，不能静默吞掉剩余源码
            if (!comment_closed) {
                err::error_reporter(file_path_, {comment_start_lno, prev_lineno_, comment_start_col, prev_col_},
                    "SyntaxError", "Unclosed block comment");
            }

            curr_state_ = LexState::Start;
            break;
        }
        }
    }

    // 生成EOF Token
    tokens_.emplace_back(TokenType::EndOfFile, "", lineno_, col_);
    return tokens_;
}

} // namespace kiz