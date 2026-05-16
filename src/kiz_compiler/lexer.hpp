#pragma once

namespace kizc {

enum class TokenKind: uint8_t {
    OpAdd, OpSub, OpMul, OpDiv, OpMod, OpEq,
    OpNe, OpGt, OpLt, OpLe, OpGe,
    KwAnd, KwOr, KwNot, KwIf, KwElif, KwElse,
    KwWhile, KwLoop, KwBreak, KwReturn, KwNext,
    KwType, KwWith, KwImpl, KwAbstract, KwMove,
    KwRef, KwMutRef, kwLet, KwPub, KwUse, KwMod,
    KwFun, KwTry, KwWhen, KwOn,
    At, LParen, RParen, LBrace, RBrace, LBracket, RBracket,
    Comma, Semi, Colon, Dot, FatArrow, ThinArrow, Assign,
    Ident, String, Int, Decimal, Eof, Invaild
};

struct Token {
    TokenKind tokenkind;
    Str text;
    Span span;
};

class Lexer {
    Str text;
    uint32_t pos;
    uint32_t lineno;
    uint8_t col;
    Token next_cache;
public:
    Lexer(Str s)
        : text(s), pos(0), lineno(1), col(1) {}
    /// next
    auto next() -> Token;
    auto peek() -> Token;
}

}