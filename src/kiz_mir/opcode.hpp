#pragma once

namespace kmir {

#define $Op(x) x
enum class Opcode: uint8_t {
#   include "opcode_kind.def"
};
#undef $Op

#define $Op(x) case x: return #x
inline auto opcode_to_str(Opcode op) -> Str {
    switch op {
#       include "opcode_kind.def"
    }
}
#undef $Op

}