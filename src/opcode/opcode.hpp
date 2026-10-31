/**
 * @file opcode.hpp
 * @brief 虚拟机指令集(VM Opcode)核心定义
 * @author azhz1107cat
 * @date 2025-10-25
 */
#pragma once
#include <cstdint>
#include <string>

namespace kiz {

enum class Opcode : uint8_t {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV,
    OP_MOD, OP_POW, OP_NEG,
    OP_EQ, OP_GT, OP_LT,
    OP_GE, OP_LE, OP_NE,
    OP_NOT,
    OP_IS, OP_IN, OP_UNPACK,

    CALL, RET, CREATE_CLOSURE,
    GET_ATTR, SET_ATTR, CALL_METHOD,
    GET_ITEM, SET_ITEM,

    LOAD_VAR, LOAD_CONST,
    SET_GLOBAL, SET_LOCAL, SET_NONLOCAL,

    JUMP, JUMP_IF_FALSE, THROW, 
    MAKE_LIST, MAKE_DICT, MAKE_CELL, FRESH_CELL,
    IMPORT,
    LOAD_ERROR,
    CACHE_ITER, GET_ITER, POP_ITER, JUMP_IF_FINISH_ITER,

    IS_CHILD, CREATE_OBJECT, COPY_TOP,
    STOP, LOAD_FREE_VAR, LOAD_BUILTINS
};

inline std::string opcode_to_string(Opcode opc) {
    switch (opc) {
    // 算术运算
    case Opcode::OP_ADD:      return "OP_ADD";
    case Opcode::OP_SUB:      return "OP_SUB";
    case Opcode::OP_MUL:      return "OP_MUL";
    case Opcode::OP_DIV:      return "OP_DIV";
    case Opcode::OP_MOD:      return "OP_MOD";
    case Opcode::OP_POW:      return "OP_POW";
    case Opcode::OP_NEG:      return "OP_NEG";

    // 比较/逻辑运算
    case Opcode::OP_EQ:       return "OP_EQ";
    case Opcode::OP_GT:       return "OP_GT";
    case Opcode::OP_LT:       return "OP_LT";
    case Opcode::OP_GE:       return "OP_GE";
    case Opcode::OP_LE:       return "OP_LE";
    case Opcode::OP_NE:       return "OP_NE";
    case Opcode::OP_NOT:      return "OP_NOT";

    case Opcode::OP_IS:       return "OP_IS";
    case Opcode::OP_IN:       return "OP_IN";
    case Opcode::OP_UNPACK:   return "OP_UNPACK";

    // 函数调用/返回
    case Opcode::CALL:        return "CALL";
    case Opcode::RET:         return "RET";

    // 属性操作
    case Opcode::GET_ATTR:    return "GET_ATTR";
    case Opcode::SET_ATTR:    return "SET_ATTR";
    case Opcode::GET_ITEM:    return "GET_ITEM";
    case Opcode::SET_ITEM:    return "SET_ITEM";
    case Opcode::CALL_METHOD: return "CALL_METHOD";

    // 变量加载/存储
    case Opcode::LOAD_VAR:    return "LOAD_VAR";
    case Opcode::LOAD_CONST:  return "LOAD_CONST";
    case Opcode::SET_GLOBAL:  return "SET_GLOBAL";
    case Opcode::SET_LOCAL:   return "SET_LOCAL";
    case Opcode::SET_NONLOCAL:return "SET_NONLOCAL";
    case Opcode::LOAD_BUILTINS: return "LOAD_BUILTINS";
    case Opcode::LOAD_FREE_VAR: return "LOAD_FREE_VAR";
    case Opcode::CREATE_CLOSURE: return "CREATE_CLOSURE";

    // 流程控制
    case Opcode::JUMP:        return "JUMP";
    case Opcode::JUMP_IF_FALSE: return "JUMP_IF_FALSE";
    case Opcode::THROW:       return "THROW";

    // 容器创建
    case Opcode::MAKE_LIST:   return "MAKE_LIST";
    case Opcode::MAKE_DICT:   return "MAKE_DICT";
    case Opcode::MAKE_CELL:   return "MAKE_CELL";
    case Opcode::FRESH_CELL:  return "FRESH_CELL";

    case Opcode::CACHE_ITER: return "CACHE_ITER";
    case Opcode::GET_ITER:    return "GET_ITER";
    case Opcode::POP_ITER:     return "POP_ITER";
    case Opcode::JUMP_IF_FINISH_ITER:  return "JUMP_IF_FINISH_ITER";

    // 其他
    case Opcode::IMPORT:      return "IMPORT";
    case Opcode::LOAD_ERROR:  return "LOAD_ERROR";
    case Opcode::IS_CHILD: return "IS_CHILD";
    case Opcode::CREATE_OBJECT: return "CREATE_OBJECT";
    case Opcode::STOP:        return "STOP";
    case Opcode::COPY_TOP:    return "COPY_TOP";

    // 兜底
    default:                  return "UNKNOWN_OPCODE(" + std::to_string(static_cast<int>(opc)) + ")";
    }
}

} // namespace kiz