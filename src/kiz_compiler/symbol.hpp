#pragma once

namespace kizc {

enum class SymKind: uint8_t {
    Const, Var, Function
};

struct Symbol {
    Str name;
    SymKind symkind;
};

enum class TypeKind: uint8_t {
    Builtin_Int, Builtin_Decimal, Built_Str,
    Struct, Function, Alias, Enum
};

struct Type {
    uint32_t typeid;
    TypeKind typekind;
};


}