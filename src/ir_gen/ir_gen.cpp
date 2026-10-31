/**
 * @file ir_gen.cpp
 * @brief 中间代码生成器（IR Generator）核心实现
 * 从AST生成IR
 * @author azhz1107cat
 * @date 2025-10-25
 */

#include "ir_gen.hpp"
#include "../parser/ast.hpp"
#include "../models/models.hpp"
#include <algorithm>
#include <cassert>

#include "../kiz.hpp"
#include "../vm/vm.hpp"
#include "../opcode/opcode.hpp"

namespace kiz {

size_t IRGenerator::get_or_add_name(std::vector<std::string>& names, const std::string& name) {
    auto it = std::ranges::find(names, name);
    if (it != names.end()) {
        return std::distance(names.begin(), it);
    }
    names.emplace_back(name);
    return names.size() - 1;
}

// 辅助函数：获取常量在curr_const中的索引（不存在则添加）
size_t IRGenerator::get_or_add_const(model::Object* obj) {
    obj->mark_as_important();
    const auto it = std::find(Vm::const_pool.begin(), Vm::const_pool.end(), obj);
    if (it != Vm::const_pool.end()) {
        return std::distance(Vm::const_pool.begin(), it);
    }
    Vm::const_pool.emplace_back(obj);
    return Vm::const_pool.size() - 1;
}

model::CodeObject* IRGenerator::gen(std::unique_ptr<BlockStmt> ast_into, const std::vector<std::string>& global_var_names_into) {
    ast = std::move(ast_into);
    DEBUG_OUTPUT("generating...");
    // 检查AST根节点有效性（默认模块根为BlockStmt）
    assert(ast && ast->ast_type == AstType::BlockStmt);
    const auto root_block = ast.get();

    // 处理模块顶层节点
    // 创建函数体
    code_chunks.emplace_back(CodeChunk());
    if (!global_var_names_into.empty()) {
        code_chunks.back().var_names = global_var_names_into;
    }
    gen_block(root_block);

    // 收尾装箱：模块级被捕获的全局槽位（共享闭包）
    finalize_chunk(root_block->pos);

    auto code_obj = new model::CodeObject(
        code_chunks.back().code_list,
        code_chunks.back().var_names,
        code_chunks.back().attr_names,
        code_chunks.back().free_names,
        code_chunks.back().upvalues,
        code_chunks.back().var_names.size(),
        code_chunks.back().exception_tables,
        0
    );

    return code_obj;
}

// 代码块收尾：为本作用域被捕获的局部槽位前插MAKE_CELL装箱指令，
// 并把代码区内全部绝对跳转与异常表pc统一偏移K（前插后所有目标平移K）
void IRGenerator::finalize_chunk(const err::PositionInfo& pos) {
    auto& chunk = code_chunks.back();
    const size_t K = chunk.captured_locals.size();
    if (K == 0) return;

    // 1. 前插装箱指令（槽位下标有序，装箱顺序无关紧要）
    std::vector<Instruction> prefix;
    prefix.reserve(K);
    err::PositionInfo cell_pos = pos; // Instruction构造收非常量引用，此处拷贝一份
    for (const size_t slot : chunk.captured_locals) {
        prefix.emplace_back(Opcode::MAKE_CELL, slot, cell_pos);
    }
    chunk.code_list.insert(chunk.code_list.begin(), prefix.begin(), prefix.end());

    // 2. 跳过前缀区，偏移区内全部绝对跳转目标
    for (size_t pc = K; pc < chunk.code_list.size(); ++pc) {
        auto& inst = chunk.code_list[pc];
        if (inst.opc == Opcode::JUMP or inst.opc == Opcode::JUMP_IF_FALSE
            or inst.opc == Opcode::JUMP_IF_FINISH_ITER) {
            inst.opn += K;
        }
    }

    // 3. 异常表全字段为绝对pc，统一偏移
    for (auto& table : chunk.exception_tables) {
        table.try_part_start_pc += K;
        table.try_part_end_pc += K;
        table.mismatch_pc += K;
        // to_vector返回拷贝，需用insert回写
        for (auto& [name, handle_pc] : table.handle_pc.to_vector()) {
            table.handle_pc.insert(name, handle_pc + K);
        }
    }
}

model::Int* IRGenerator::make_int_obj(const NumberExpr* num_expr) {
    DEBUG_OUTPUT("making int object...");
    assert(num_expr);
    auto the_num = dep::BigInt(num_expr->value);
    if (the_num >= 0 and the_num < 201) {
        auto obj = Vm::small_int_pool[the_num.to_unsigned_long_long()];
        return obj;
    }

    auto int_obj = new model::Int( the_num );
    return int_obj;
}


model::Decimal* IRGenerator::make_decimal_obj(const DecimalExpr* dec_expr) {
    DEBUG_OUTPUT("making rational object...");
    auto decimal_str = dep::Decimal(dec_expr->value);
    auto decimal_obj = new model::Decimal(decimal_str);
    return decimal_obj;
}

model::String* IRGenerator::make_string_obj(const StringExpr* str_expr) {
    DEBUG_OUTPUT("making string object...");
    assert(str_expr);
    auto str_obj = new model::String(str_expr->value);
    return str_obj;
}

std::vector<std::string> IRGenerator::get_global_var_names() {
    assert(!code_chunks.empty());
    return code_chunks.back().var_names;
}

} // namespace kiz