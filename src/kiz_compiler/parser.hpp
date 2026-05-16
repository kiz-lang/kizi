#pragma once

namespace kizc {

class Parser {
    Lexer& lexer;
    Codegen& codegen;
    Vec<Vec<Symbol>> symtable;

    /// parse expr
    auto parse_expr() -> void;

    /// parse stmt
    auto parse_stmt() -> void;
public:
    Parser(Lexer& l, Codegen& cg)
        : lexer(l), codegen(cg), symtable({{}}) {}
    
    /// parse
    auto parse(Str txt) -> void;
}

}