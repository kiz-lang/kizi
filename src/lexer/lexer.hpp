/**
 * @file lexer.hpp
 * @brief 词法分析器（FSM有限自动状态机）- 基于UTF-8 UTF8Char/UTF8String
 * @author azhz1107cat
 * @date 2025-10-25 + 2026-01-31 重构
 */
#pragma once
#include <string>
#include <vector>
#include <utility>
#include <cstddef>
#include <unordered_map>
#include "../../depends/u8str.hpp"
#include "../error/error_reporter.hpp"

namespace kiz {

/// Token 类型
enum class TokenType {
    // 关键字
    Func, If, Else, While, Return, Import, Break, Object,
    True, False, Nil, End, Next, Nonlocal, Global, Try, Catch, For, Throw,
    // 标识符
    Identifier,
    // 赋值运算符
    Assign,
    // 字面量
    Number, Decimal, String,

    // F-String
    FStringStart,    // f-string 起始标记
    InsertExprStart, // 插入表达式开始 {
    InsertExprEnd,   // 插入表达式结束 }
    FStringEnd,      // f-string 结束标记

    // 分隔符
    LParen, RParen, LBrace, RBrace, LBracket, RBracket,
    Comma, Dot, TripleDot, Semicolon,
    // 运算符
    ExclamationMark, Plus, Minus, Star, Slash, Backslash,
    Percent, Caret, Bang, Equal, NotEqual,
    Less, LessEqual, Greater, GreaterEqual, Pipe,
    FatArrow, ThinArrow, Colon,
    Not, And, Or, Is, In, At,
    // 特殊标记
    EndOfFile, EndOfLine, Unknown
};

/// Token定义：保持原有结构，pos为const
struct Token {
    TokenType type;
    std::string text;
    err::PositionInfo pos{};

    explicit Token(
        TokenType tp,
        std::string t,
        size_t lno_start, size_t lno_end,
        size_t col_start, size_t col_end
    ) : type(tp), text(std::move(t)), pos{lno_start, lno_end, col_start, col_end} {}

    explicit Token(
        TokenType tp,
        std::string t,
        size_t lno, size_t col
    ) : type(tp), text(std::move(t)), pos{lno, lno, col, col} {}

    explicit Token(
        TokenType tp,
        std::string t,
        const err::PositionInfo& pos_info
    ) : type(tp), text(std::move(t)), pos(pos_info) {}
};

/// 有限自动状态机 状态枚举
enum class LexState {
    Start,          // 初始状态
    Identifier,     // 标识符/关键字
    Number,         // 数字/小数/科学计数法
    Operator,       // 双字符运算符（=>/->/==/!=等）
    String,         // 普通字符串（""/''）
    SingleComment,  // 单行注释（#）
    BlockComment,   // 块注释（/* */）
    FString,        // f-string 解析状态
};

// 词法分析器类：重构为FSM，基于UTF8Char/UTF8String
class Lexer {
    const std::string& file_path_;  // 文件名（用于错误报告）
    dep::UTF8String src_;           // 源文件UTF-8字符串
    std::vector<Token> tokens_;     // 生成的Token列表
    std::unordered_map<std::string, TokenType> keywords_; // 关键字映射

    // FSM核心状态变量
    LexState curr_state_ = LexState::Start; // 当前状态
    size_t char_pos_ = 0;                   // 当前字符索引
    size_t lineno_ = 1;                     // 当前行号
    size_t col_ = 1;                        // 当前列号

    size_t prev_lineno_ = 1;
    size_t prev_col_ = 1;

    /// 初始化关键字（仅执行一次）
    void init_keywords();

    /// 生成Token并添加到列表
    void emit_token(TokenType type, size_t start_cp, size_t end_cp,
                   size_t start_lno, size_t start_col,
                   size_t end_lno, size_t end_col);

    /// 快速生成单码点Token
    void emit_single_cp_token(TokenType type, size_t char_index);

    /// 处理字符串转义（普通/跨行通用）
    static std::string handle_escape(const std::string& raw);

    /// 预读下一个码点（不移动cp_pos_）
    dep::UTF8Char peek(size_t offset = 1) const {
        return src_[char_pos_ + offset];
    }

    /// 读取当前码点并移动cp_pos_（更新行号/列号）
    dep::UTF8Char next();

public:
    explicit Lexer(const std::string& file_path)
    : file_path_(file_path) {
        init_keywords();
    }
    void prepare(const std::string& src, size_t lineno_start = 1, size_t col_start = 1) {
        // 初始化状态
        src_ = dep::UTF8String(src);
        tokens_.clear();
        curr_state_ = LexState::Start;
        char_pos_ = 0;
        lineno_ = lineno_start;
        col_ = col_start;
        // 跳过UTF-8 BOM（U+FEFF），Windows编辑器常见
        if (src_.size() > 0 and src_[0].to_cod_point() == 0xFEFF) {
            char_pos_ = 1;
        }
    }
    std::vector<Token> tokenize();
    void read_string();
    void read_fstring();
    void read_num();
};

}  // namespace kiz