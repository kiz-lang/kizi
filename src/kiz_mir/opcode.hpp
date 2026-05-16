#pragma once

namespace kmir {

#define $Op(x, y) x,
enum class Opcode: uint8_t {
#   include "opcode_kind.def"
};
#undef $Op

#define $Op(x, y) case x: return Str(#x);
inline auto opcode_to_str(Opcode op) -> Str {
    switch op {
#       include "opcode_kind.def"
    }
}
#undef $Op

}