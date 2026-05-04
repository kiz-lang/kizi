/**
 * @file gen_stmt.cpp
 * @brief 中间代码生成器（IR Generator）生成语句部分核心实现
 * 从AST生成IR
 * @author azhz1107cat
 * @date 2026-4-30
 */


#include <format>

#include "../kiz.hpp"
#include "../opcode/opcode.hpp"
#include "ir_gen.hpp"
#include "../parser/ast.hpp"
#include "../models/models.hpp"

namespace kiz {

model::Module* IRGenerator::gen_mod(
    const std::string& module_name,
    model::CodeObject* module_code
) {
    DEBUG_OUTPUT("code object created with code list len " + std::to_string(module_code->code.size()));
    assert(module_code != nullptr);
    const auto module_obj = new model::Module(
        module_name,
        module_code
    );
    return module_obj;
}

void IRGenerator::gen_block(const BlockStmt* block) {
    for (auto& stmt : block->statements) {
        switch (stmt->ast_type) {
        assert(!code_chunks.empty());
        case AstType::ImportStmt: {
            const auto import_stmt = dynamic_cast<ImportStmt*>(stmt.get());
            const size_t name_idx = get_or_add_name(code_chunks.back().attr_names, import_stmt->path);

            code_chunks.back().code_list.emplace_back(
                Opcode::IMPORT,
                name_idx,
                stmt->pos
            );

            const size_t local_name_idx = get_or_add_name(code_chunks.back().var_names, import_stmt->var_name);

            code_chunks.back().code_list.emplace_back(
                Opcode::SET_LOCAL,
                local_name_idx,
                stmt->pos
            );
            break;
        }
        case AstType::EnsureStmt: {
            auto ensure = dynamic_cast<EnsureStmt*>(stmt.get());
            size_t old_size = code_chunks.back().code_list.size();
            gen_expr(ensure->expr.get()); // 生成defer块的指令

            // 计算新生成的指令范围
            size_t new_size = code_chunks.back().code_list.size();
            if (new_size > old_size) {
                //正向复制指令
                std::vector<Instruction> defer_block;
                defer_block.reserve(new_size - old_size);
                for (size_t i = old_size; i < new_size; ++i) {
                    defer_block.push_back(code_chunks.back().code_list[i]);
                }

                // 删除原code_list中的这些指令（保留顺序删除）
                code_chunks.back().code_list.erase(
                    code_chunks.back().code_list.begin() + old_size,
                    code_chunks.back().code_list.end()
                );

                code_chunks.back().ensure_stmts.insert(
                    code_chunks.back().ensure_stmts.begin(),
                    defer_block.begin(),
                    defer_block.end());
            }
            break;
        }
        case AstType::AssignStmt: {
            // 变量声明：生成初始化表达式IR + 存储变量指令
            const auto var_decl = dynamic_cast<AssignStmt*>(stmt.get());
            gen_expr(var_decl->expr.get()); // 生成初始化表达式IR
            const size_t name_idx = get_or_add_name(code_chunks.back().var_names, var_decl->name);

            code_chunks.back().code_list.emplace_back(
                Opcode::SET_LOCAL,
                name_idx,
                stmt->pos
            );
            break;
        }
        case AstType::NonlocalAssignStmt: {
            // 变量声明：生成初始化表达式IR + 存储变量指令
            const auto var_decl = dynamic_cast<NonlocalAssignStmt*>(stmt.get());
            gen_expr(var_decl->expr.get()); // 生成初始化表达式IR

            // 可能已经注册入free_vars
            auto may_be_free_it = std::ranges::find(code_chunks.back().free_names, var_decl->name);
            if (may_be_free_it != code_chunks.back().free_names.end()) {
                size_t name_idx = std::distance(code_chunks.back().free_names.begin(), may_be_free_it);
                code_chunks.back().code_list.emplace_back(
                    Opcode::SET_NONLOCAL,
                    name_idx,
                    stmt->pos
                );
                break;
            }

            size_t i = 0;
            size_t name_idx = 0;
            bool find_free_var_it = false;
            for (auto& code_chuck : code_chunks | std::views::reverse) {
                auto free_it = std::ranges::find(code_chuck.var_names, var_decl->name);
                if (free_it != code_chuck.var_names.end()) {
                    name_idx = std::distance(code_chuck.var_names.begin(), free_it);
                    find_free_var_it = true;
                    break;
                }
                ++i;
            }
            if (find_free_var_it) {
                code_chunks.back().free_names.push_back(var_decl->name);
                code_chunks.back().upvalues.push_back({i, name_idx});
                code_chunks.back().code_list.emplace_back(
                    Opcode::SET_NONLOCAL,
                    code_chunks.back().upvalues.size() - 1,
                    stmt->pos
                );
            } else {
                err::error_reporter(file_path, stmt->pos, "NameError", "Undefined nonlocal var '"+var_decl->name+"'");
            }
            break;
        }

        case AstType::GlobalAssignStmt: {
            // 变量声明：生成初始化表达式IR + 存储变量指令
            const auto var_decl = dynamic_cast<GlobalAssignStmt*>(stmt.get());
            auto free_it = std::ranges::find(code_chunks.front().var_names, var_decl->name);
            if (free_it != code_chunks.front().var_names.end()) {
                size_t name_idx = std::distance(code_chunks.front().var_names.begin(), free_it);
                gen_expr(var_decl->expr.get());
                code_chunks.back().code_list.emplace_back(
                    Opcode::SET_GLOBAL,
                    name_idx,
                    stmt->pos
                );
                break;
            } else {
                err::error_reporter(file_path, stmt->pos, "NameError", "Undefined global var '"+var_decl->name+"'");
            }
            break;
        }
        case AstType::ObjectStmt: {
            gen_object_stmt(dynamic_cast<ObjectStmt*>(stmt.get()));
            break;
        }
        case AstType::ExprStmt: {
            auto expr_stmt = dynamic_cast<ExprStmt*>(stmt.get());
            gen_expr(expr_stmt->expr.get());
            break;
        }
        case AstType::IfStmt:
            gen_if(dynamic_cast<IfStmt*>(stmt.get()));
            break;
        case AstType::ForStmt:
            gen_for(dynamic_cast<ForStmt*>(stmt.get()));
            break;
        case AstType::WhileStmt:
            gen_while(dynamic_cast<WhileStmt*>(stmt.get()));
            break;
        case AstType::TryStmt:
            gen_try(dynamic_cast<TryStmt*>(stmt.get()));
            break;
        case AstType::ReturnStmt: {
            // 返回语句：生成返回值表达式IR + RET指令
            auto ret_stmt = dynamic_cast<ReturnStmt*>(stmt.get());
            if (ret_stmt->expr) {
                gen_expr(ret_stmt->expr.get());
            } else {
                // 无返回值时压入Nil常量
                auto nil = model::load_nil();
                const size_t const_idx = get_or_add_const(nil);
                code_chunks.back().code_list.emplace_back(
                    Opcode::LOAD_CONST,
                    const_idx,
                    stmt->pos
                );
            }
            code_chunks.back().code_list.emplace_back(
                Opcode::RET,
                0,
                stmt->pos
            );
            break;
        }
        case AstType::ThrowStmt: {
            auto throw_stmt = dynamic_cast<ThrowStmt*>(stmt.get());
            gen_expr(throw_stmt->expr.get());
            code_chunks.back().code_list.emplace_back(
                Opcode::THROW,
                0,
                stmt->pos
            );
            break;
        }
        case AstType::BreakStmt: {
            if (code_chunks.back().loop_info_stack.empty()) {
                err::error_reporter(file_path, stmt->pos, "SyntaxError", "Break statement cannot use freely (must in while/for block)");
            }
            code_chunks.back().loop_info_stack.back().break_pos.push_back(code_chunks.back().code_list.size());
            code_chunks.back().code_list.emplace_back(
                Opcode::JUMP,
                0,
                stmt->pos
            );
            break;
        }
        case AstType::NamedFuncDeclStmt: {
            gen_fn_decl(dynamic_cast<NamedFuncDeclStmt*>(stmt.get()));
            break;
        }
        case AstType::NextStmt: {
            if (code_chunks.back().loop_info_stack.empty()) {
                err::error_reporter(file_path, stmt->pos, "SyntaxError", "Next statement cannot use freely (must in while/for block)");
            }
            code_chunks.back().loop_info_stack.back().continue_pos.push_back(code_chunks.back().code_list.size());
            code_chunks.back().code_list.emplace_back(
                Opcode::JUMP,
                0,
                stmt->pos
            );
            break;
        }
        case AstType::SetMemberStmt: {
            // 设置成员：生成对象表达式 -> 生成值表达式 -> 加载属性名 -> SET_ATTR指令
            const auto set_mem = dynamic_cast<SetMemberStmt*>(stmt.get());
            const auto get_mem = dynamic_cast<GetMemberExpr*>(set_mem->g_mem.get());
            assert(get_mem != nullptr);
            gen_expr(get_mem->father.get()); // 生成对象IR
            gen_expr(set_mem->val.get());   // 生成值IR

            size_t name_idx = get_or_add_name(code_chunks.back().attr_names, get_mem->child->name);
            code_chunks.back().code_list.emplace_back(
                Opcode::SET_ATTR,
                name_idx,
                stmt->pos
            );
            break;
        }
        case AstType::SetItemStmt: {
            const auto set_item = dynamic_cast<SetItemStmt*>(stmt.get());
            const auto get_item = dynamic_cast<GetItemExpr*>(set_item->g_item.get());

            gen_expr(get_item->father.get()); // 生成对象IR
            gen_expr(get_item->params[0].get()); // 生成第一参数(仅支持一个参数)
            gen_expr(set_item->val.get());   // 生成值IR

            code_chunks.back().code_list.emplace_back(
                Opcode::SET_ITEM,
                0,
                stmt->pos
            );
            break;
        }
        default:
            assert(false && "gen_block: 未处理的语句类型");
        }
    }
}

void IRGenerator::gen_if(IfStmt* if_stmt) {
    assert(if_stmt && "gen_if: if节点为空");
    // 生成条件表达式IR
    gen_expr(if_stmt->condition.get());

    // 生成JUMP_IF_FALSE指令（目标先占位，后续填充）
    size_t jump_if_false_idx = code_chunks.back().code_list.size();
    code_chunks.back().code_list.emplace_back(
        Opcode::JUMP_IF_FALSE,
        0, // 占位目标索引
        if_stmt->pos
    );

    // 生成then块IR
    gen_block(if_stmt->thenBlock.get());

    // 生成JUMP指令（跳过else块，目标占位）
    size_t jump_else_idx = code_chunks.back().code_list.size();
    code_chunks.back().code_list.emplace_back(
        Opcode::JUMP,
        0, // 占位目标索引
        if_stmt->pos
    );

    // 填充JUMP_IF_FALSE的目标（else块开始位置）
    code_chunks.back().code_list[jump_if_false_idx].opn = code_chunks.back().code_list.size();

    // 生成else块IR（存在则生成）
    if (if_stmt->elseBlock) {
        gen_block(if_stmt->elseBlock.get());
    }

    // 填充JUMP的目标（if-else结束位置）
    code_chunks.back().code_list[jump_else_idx].opn = code_chunks.back().code_list.size();
}

void IRGenerator::gen_fn_decl(NamedFuncDeclStmt* func) {
    // 创建函数体
    code_chunks.back().var_names.push_back(func->name);
    code_chunks.emplace_back(CodeChunk());
    // 添加参数到变量表
    for (const auto& param : func->params) {
        get_or_add_name(code_chunks.back().var_names, param);
    }
    // 生成函数体
    gen_block(func->body.get());

    // 确保有返回值（无显式返回则返回Nil）
    if (code_chunks.back().code_list.empty() || code_chunks.back().code_list.back().opc != Opcode::RET) {
        const auto nil = model::load_nil();
        const size_t nil_idx = get_or_add_const(nil);
        code_chunks.back().code_list.emplace_back(
            Opcode::LOAD_CONST,
            nil_idx,
            func->pos
        );
        code_chunks.back().code_list.emplace_back(
            Opcode::RET,
            0,
            func->pos
        );
    }

    // std::cout << "== IR Result ==" << std::endl;
    // size_t i = 0;
    // for (const auto& inst : code_chunks.back().code_list) {
    //     std::string opn_text;
    //     for (auto opn : inst.opn_list) {
    //         opn_text += std::to_string(opn) + ",";
    //     }
    //     std::cout << i << ":" << opcode_to_string(inst.opc) << " " << opn_text << std::endl;
    //     ++i;
    // }
    // std::cout << "== End ==" << std::endl;
    // std::cout << "== VarName Result ==" << std::endl;
    // for (auto n: code_chunks.back().var_names) {
    //     std::cout << n << "\n";
    // }
    // std::cout << "== End ==" << std::endl;

    auto code_obj = new model::CodeObject(
        code_chunks.back().code_list,
        code_chunks.back().var_names,
        code_chunks.back().attr_names,
        code_chunks.back().free_names,
        code_chunks.back().upvalues,
        code_chunks.back().var_names.size(),
        code_chunks.back().exception_tables,
        code_chunks.back().ensure_stmts
    );
    code_chunks.pop_back();

    // 生成函数体IR
    code_obj->make_ref();
    const auto fn = new model::Function(
        func->name,
        code_obj,
        func->params.size()
    );
    fn->has_rest_params = func->has_rest_params;


    // 加载函数对象
    const size_t fn_const_idx = get_or_add_const(fn);
    code_chunks.back().code_list.emplace_back(
        Opcode::LOAD_CONST,
        fn_const_idx,
        func->pos
    );

    const size_t name_idx = get_or_add_name(code_chunks.back().var_names, func->name);

    code_chunks.back().code_list.emplace_back(
        Opcode::SET_LOCAL,
        name_idx,
        func->pos
    );

    code_chunks.back().code_list.emplace_back(
        Opcode::LOAD_VAR,
        name_idx,
        func->pos
    );

    code_chunks.back().code_list.emplace_back(
        Opcode::CREATE_CLOSURE,
        0,
        func->pos
    );
}

void IRGenerator::gen_object_stmt(ObjectStmt* obj_decl) {
    const size_t name_idx = get_or_add_name(code_chunks.back().var_names, obj_decl->name);

    code_chunks.back().code_list.emplace_back(
        Opcode::CREATE_OBJECT,
        0,
        obj_decl->pos
    );

    code_chunks.back().code_list.emplace_back(
        Opcode::SET_LOCAL,
        name_idx,
        obj_decl->pos
    );

    if (!obj_decl->parent_name.empty()) {
        const size_t parent_name_idx = get_or_add_name(code_chunks.back().var_names, obj_decl->parent_name);

        code_chunks.back().code_list.emplace_back(
            Opcode::LOAD_VAR,
            name_idx,
            obj_decl->pos
        );

        code_chunks.back().code_list.emplace_back(
            Opcode::LOAD_VAR,
            parent_name_idx,
            obj_decl->pos
        );

        const size_t parent_text_idx = get_or_add_name(code_chunks.back().attr_names, "__parent__");
        code_chunks.back().code_list.emplace_back(
            Opcode::SET_ATTR,
            parent_text_idx,
            obj_decl->pos
        );
    }

    for (const auto& sub_assign: obj_decl->body->statements) {
        if (const auto sub_assign_stmt = dynamic_cast<AssignStmt*>(sub_assign.get())) {
            code_chunks.back().code_list.emplace_back(
                Opcode::LOAD_VAR,
                name_idx,
                obj_decl->pos
            );
            assert(sub_assign_stmt->expr.get());
            gen_expr(sub_assign_stmt->expr.get());

            const size_t sub_name_idx = get_or_add_name(code_chunks.back().attr_names, sub_assign_stmt->name);

            code_chunks.back().code_list.emplace_back(
                Opcode::SET_ATTR,
                sub_name_idx,
                obj_decl->pos
            );
        } else if (auto f_decl = dynamic_cast<NamedFuncDeclStmt*>(sub_assign.get())) {
            gen_fn_decl(f_decl);
            // 定位到set local指令
            auto set_func_instr = code_chunks.back().code_list[code_chunks.back().code_list.size() - 3];
            code_chunks.back().code_list.emplace_back(
                Opcode::LOAD_VAR,
                name_idx,
                set_func_instr.pos
            );

            code_chunks.back().code_list.emplace_back(
                Opcode::LOAD_VAR,
                set_func_instr.opn,
                set_func_instr.pos
            );

            auto sub_func_name = code_chunks.back().var_names[set_func_instr.opn];
            auto sub_func_name_idx = get_or_add_name(code_chunks.back().attr_names, sub_func_name);
            code_chunks.back().code_list.emplace_back(
                Opcode::SET_ATTR,
                sub_func_name_idx,
                set_func_instr.pos
            );
        } else {
            err::error_reporter(file_path, obj_decl->pos,
                "SyntaxError",
                "Object Statement cannot include other code or other object statement (only assign and function statement support)"
            );
        }
    }
}

void IRGenerator::gen_while(WhileStmt* while_stmt) {
    assert(while_stmt && "gen_while: while节点为空");
    // 记录循环入口（条件判断开始位置）→ continue跳这里
    size_t loop_entry_idx = code_chunks.back().code_list.size();

    // 生成循环条件IR
    gen_expr(while_stmt->condition.get());

    // 生成JUMP_IF_FALSE指令（目标：循环结束位置，先占位）
    const size_t jump_if_false_idx = code_chunks.back().code_list.size();
    code_chunks.back().code_list.emplace_back(
        Opcode::JUMP_IF_FALSE,
        0, // 占位，后续填充为循环结束位置
        while_stmt->pos
    );

    auto loop_info = LoopInfo{{}, {}};
    code_chunks.back().loop_info_stack.push_back(loop_info);

    // 生成循环体IR
    gen_block(while_stmt->body.get());

    // 生成JUMP指令，跳回循环入口
    code_chunks.back().code_list.emplace_back(
        Opcode::JUMP,
        loop_entry_idx,
        while_stmt->pos
    );

    // 填充JUMP_IF_FALSE的目标（循环结束位置 = 当前代码列表长度）
    size_t loop_exit_idx = code_chunks.back().code_list.size();
    code_chunks.back().code_list[jump_if_false_idx].opn = loop_exit_idx;

    for (const auto break_pos : code_chunks.back().loop_info_stack.back().break_pos) {
        code_chunks.back().code_list[break_pos].opn = loop_exit_idx;
    }

    for (const auto continue_pos : code_chunks.back().loop_info_stack.back().continue_pos) {
        code_chunks.back().code_list[continue_pos].opn = loop_entry_idx;
    }

    code_chunks.back().loop_info_stack.pop_back();
}

void IRGenerator::gen_for(ForStmt* for_stmt) {
    assert(for_stmt);

    // 生成循环iter IR
    gen_expr(for_stmt->iter.get());

    code_chunks.back().code_list.emplace_back(
        Opcode::CACHE_ITER,
        0,
        for_stmt->pos
    );

    // 记录循环入口（条件判断开始位置）→ continue跳这里
    size_t loop_entry_idx = code_chunks.back().code_list.size();

    code_chunks.back().code_list.emplace_back(
        Opcode::MAKE_LIST,
        0,
        for_stmt->pos
    );

    code_chunks.back().code_list.emplace_back(
        Opcode::GET_ITER,
        0,
        for_stmt->pos
    );

    size_t name_idx = get_or_add_name(code_chunks.back().attr_names, "__next__");

    code_chunks.back().code_list.emplace_back(
        Opcode::CALL_METHOD,
        name_idx,
        for_stmt->pos
    );

    size_t var_name_idx = get_or_add_name(code_chunks.back().var_names, for_stmt->item_var_name);
    code_chunks.back().code_list.emplace_back(
        Opcode::SET_LOCAL,
        var_name_idx,
        for_stmt->pos
    );

    code_chunks.back().code_list.emplace_back(
        Opcode::LOAD_VAR,
        var_name_idx,
        for_stmt->pos
    );

    // 生成JUMP_IF_FALSE指令（目标：循环结束位置，先占位）
    const size_t jump_if_false_idx = code_chunks.back().code_list.size();
    code_chunks.back().code_list.emplace_back(
        Opcode::JUMP_IF_FINISH_ITER,
        0, // 占位，后续填充为循环结束位置
        for_stmt->pos
    );

    auto loop_info = LoopInfo{{}, {}};
    code_chunks.back().loop_info_stack.emplace_back(loop_info);

    // 生成循环体IR
    gen_block(for_stmt->body.get());

    // 生成JUMP指令，跳回循环入口
    code_chunks.back().code_list.emplace_back(
        Opcode::JUMP,
        loop_entry_idx,
        for_stmt->pos
    );

    // 填充JUMP_IF_FALSE的目标（循环结束位置 = 当前代码列表长度）
    size_t loop_exit_idx = code_chunks.back().code_list.size();
    code_chunks.back().code_list[jump_if_false_idx].opn = loop_exit_idx;

    code_chunks.back().code_list.emplace_back(
        Opcode::POP_ITER,
        0,
        for_stmt->pos
    );

    for (const auto break_pos : code_chunks.back().loop_info_stack.back().break_pos) {
        code_chunks.back().code_list[break_pos].opn = loop_exit_idx;
    }

    for (const auto continue_pos : code_chunks.back().loop_info_stack.back().continue_pos) {
        code_chunks.back().code_list[continue_pos].opn = loop_entry_idx;
    }

    code_chunks.back().loop_info_stack.pop_back();

}

void IRGenerator::gen_try(TryStmt* try_stmt) {
    assert(try_stmt);
    size_t try_start_idx = code_chunks.back().code_list.size();

    // 生成 try 块的语句
    gen_block(try_stmt->try_block.get());

    size_t jump_to_finally_idx = code_chunks.back().code_list.size();
    code_chunks.back().code_list.emplace_back(
        Opcode::JUMP,
        0,
        try_stmt->pos
    );

    std::vector<size_t> catch_start_pc;
    model::ExceptionTable exception_table;
    exception_table.try_part_start_pc = try_start_idx;
    exception_table.try_part_end_pc = code_chunks.back().code_list.size();

    std::vector<size_t> catch_jump_to_finally_pcs;
    for (const auto& catch_stmt : try_stmt->catch_blocks) {
        exception_table.handle_pc.insert(catch_stmt->error_text, code_chunks.back().code_list.size());

        size_t name_idx = get_or_add_name(code_chunks.back().var_names, catch_stmt->var_name);

        code_chunks.back().code_list.emplace_back(
            Opcode::LOAD_ERROR,
            0,
            try_stmt->pos
        );

        code_chunks.back().code_list.emplace_back(
            Opcode::SET_LOCAL,
            name_idx,
            try_stmt->pos
        );
        gen_block(catch_stmt->catch_block.get());

        catch_jump_to_finally_pcs.push_back(code_chunks.back().code_list.size());
        code_chunks.back().code_list.emplace_back(
            Opcode::JUMP,
            0,
            try_stmt->pos
        );
    }

    exception_table.mismatch_pc = code_chunks.back().code_list.size();
    code_chunks.back().code_list.emplace_back( Opcode::LOAD_ERROR, 0, try_stmt->pos);
    code_chunks.back().code_list.emplace_back(Opcode::THROW, 0, try_stmt->pos);

    code_chunks.back().exception_tables.push_back(exception_table);

    // 回填跳转到finally
    code_chunks.back().code_list[jump_to_finally_idx].opn = code_chunks.back().code_list.size();
    for (auto catch_jump_to_finally_pc : catch_jump_to_finally_pcs) {
        code_chunks.back().code_list[catch_jump_to_finally_pc].opn = code_chunks.back().code_list.size();
    }

}

}
