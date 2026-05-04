/**
 * @file gen_expr.cpp
 * @brief 中间代码生成器（IR Generator）生成表达式部分核心实现
 * 从AST生成IR
 * @author azhz1107cat
 * @date 2026-4-30
 */

#include "../opcode/opcode.hpp"
#include "ir_gen.hpp"
#include "../parser/ast.hpp"
#include "../models/models.hpp"

namespace kiz {

void IRGenerator::gen_expr(Expr* expr) {
    if(!expr) throw KizStopRunningSignal("gen_expr: null expr node");
    switch (expr->ast_type) {
    case AstType::NumberExpr: {
        // 生成LOAD_CONST指令（加载字面量常量）
        auto const_obj = make_int_obj(dynamic_cast<NumberExpr*>(expr));
        size_t const_idx = get_or_add_const(const_obj);
        code_chunks.back().code_list.emplace_back(
            Opcode::LOAD_CONST,
            const_idx,
            expr->pos
        );
        break;
    }
    case AstType::StringExpr: {
        // 生成LOAD_CONST指令（加载字面量常量）
        auto const_obj = make_string_obj(dynamic_cast<StringExpr*>(expr));
        size_t const_idx = get_or_add_const(const_obj);
        code_chunks.back().code_list.emplace_back(
            Opcode::LOAD_CONST,
            const_idx,
            expr->pos
        );
        break;
    }
    case AstType::DecimalExpr: {
        // 生成LOAD_CONST指令（加载字面量常量）
        auto const_obj = make_decimal_obj(dynamic_cast<DecimalExpr*>(expr));
        size_t const_idx = get_or_add_const(const_obj);
        code_chunks.back().code_list.emplace_back(
            Opcode::LOAD_CONST,
            const_idx,
            expr->pos
        );
        break;
    }
    case AstType::IdentifierExpr: {
        // 标识符：生成LOAD_VAR指令（加载变量值）
        const auto ident = dynamic_cast<IdentifierExpr*>(expr);
        const auto name_idx_it = std::ranges::find(code_chunks.back().var_names, ident->name);
        if (name_idx_it != code_chunks.back().var_names.end()) {
            size_t name_idx = std::distance(code_chunks.back().var_names.begin(), name_idx_it);
            code_chunks.back().code_list.emplace_back(
                Opcode::LOAD_VAR,
                name_idx,
                expr->pos
            );
        } else {
            // 可能已经注册入free_vars
            auto may_be_free_it = std::ranges::find(code_chunks.back().free_names, ident->name);
            if (may_be_free_it != code_chunks.back().free_names.end()) {
                size_t name_idx = std::distance(code_chunks.back().free_names.begin(), may_be_free_it);
                code_chunks.back().code_list.emplace_back(
                    Opcode::LOAD_FREE_VAR,
                    name_idx,
                    expr->pos
                );
                break;
            }

            size_t i = 0;
            size_t name_idx = 0;
            bool find_free_var_it = false;
            for (auto& code_chunk : code_chunks | std::views::reverse) {
                auto free_it = std::ranges::find(code_chunk.var_names, ident->name);
                if (free_it != code_chunk.var_names.end()) {
                    name_idx = std::distance(code_chunk.var_names.begin(), free_it);
                    find_free_var_it = true;
                    break;
                }
                ++i;
            }
            if (!find_free_var_it) {
                auto builtin_it = std::ranges::find(Vm::builtin_names, ident->name);
                if (builtin_it != Vm::builtin_names.end()) {
                    code_chunks.back().code_list.emplace_back(
                        Opcode::LOAD_BUILTINS,
                        static_cast<size_t>(builtin_it - Vm::builtin_names.begin()),
                        expr->pos
                    );
                    break;
                }
                err::error_reporter(file_path, expr->pos, "NameError", "Undefined var '"+ident->name+"'");
            } else {
                code_chunks.back().free_names.push_back(ident->name);
                code_chunks.back().upvalues.push_back({i, name_idx});
                code_chunks.back().code_list.emplace_back(
                    Opcode::LOAD_FREE_VAR,
                    code_chunks.back().upvalues.size() - 1,
                    expr->pos
                );
            }
        }
        break;
    }
    case AstType::BinaryExpr: {
        // 二元运算：生成左表达式 -> 右表达式 -> 运算指令
        const auto bin_expr = dynamic_cast<BinaryExpr*>(expr);
        if (bin_expr->op == "and"){
            gen_expr(bin_expr->left.get());  // 左操作数

            code_chunks.back().code_list.emplace_back(Opcode::COPY_TOP, 0, expr->pos);

            size_t jump_if_false_idx = code_chunks.back().code_list.size();
            code_chunks.back().code_list.emplace_back(Opcode::JUMP_IF_FALSE, 0, expr->pos);

            gen_expr(bin_expr->right.get()); // 右操作数（栈中顺序：左在下，右在上）
            code_chunks.back().code_list[jump_if_false_idx].opn = code_chunks.back().code_list.size();
            break;
        }
        if (bin_expr->op == "or") {
            gen_expr(bin_expr->left.get());  // 左操作数

            code_chunks.back().code_list.emplace_back(Opcode::COPY_TOP, 0, expr->pos);

            code_chunks.back().code_list.emplace_back(Opcode::OP_NOT, 0, expr->pos);
            size_t jump_if_false_idx = code_chunks.back().code_list.size();
            code_chunks.back().code_list.emplace_back(Opcode::JUMP_IF_FALSE, 0, expr->pos);

            gen_expr(bin_expr->right.get()); // 右操作数（栈中顺序：左在下，右在上）
            code_chunks.back().code_list[jump_if_false_idx].opn = code_chunks.back().code_list.size();
            break;
        }
        gen_expr(bin_expr->left.get());  // 左操作数

        gen_expr(bin_expr->right.get()); // 右操作数（栈中顺序：左在下，右在上）

        // 映射运算符到 opcode
        Opcode opc;
        if (bin_expr->op == "+") opc = Opcode::OP_ADD;
        else if (bin_expr->op == "-") opc = Opcode::OP_SUB;
        else if (bin_expr->op == "*") opc = Opcode::OP_MUL;
        else if (bin_expr->op == "/") opc = Opcode::OP_DIV;
        else if (bin_expr->op == "%") opc = Opcode::OP_MOD;
        else if (bin_expr->op == "^") opc = Opcode::OP_POW;

        else if (bin_expr->op == "==") opc = Opcode::OP_EQ;
        else if (bin_expr->op == ">=") opc = Opcode::OP_GE;
        else if (bin_expr->op == "<=") opc = Opcode::OP_LE;
        else if (bin_expr->op == "!=") opc = Opcode::OP_NE;
        else if (bin_expr->op == ">") opc = Opcode::OP_GT;
        else if (bin_expr->op == "<") opc = Opcode::OP_LT;

        else if (bin_expr->op == "is") opc = Opcode::OP_IS;
        else if (bin_expr->op == "in") opc = Opcode::OP_IN;

        else assert(false);

        code_chunks.back().code_list.emplace_back(
            opc,
            0,
            expr->pos
        );
        break;
    }
    case AstType::UnaryExpr: {
        // 一元运算：生成操作数 -> 运算指令
        auto unary_expr = dynamic_cast<UnaryExpr*>(expr);
        gen_expr(unary_expr->operand.get());

        Opcode opc;
        if (unary_expr->op == "-") opc = Opcode::OP_NEG;
        else if (unary_expr->op == "not") opc = Opcode::OP_NOT;
        else if (unary_expr->op == "...") opc = Opcode::OP_UNPACK;
        else assert(false);

        code_chunks.back().code_list.emplace_back(
            opc,
            0,
            expr->pos
        );
        break;
    }
    case AstType::CallExpr:
        DEBUG_OUTPUT("gen fn call...");
        gen_fn_call(dynamic_cast<CallExpr*>(expr));
        break;
    case AstType::DictExpr:
        gen_dict(dynamic_cast<DictExpr*>(expr));
        break;
    case AstType::ListExpr: {
        auto list_expr = dynamic_cast<ListExpr*>(expr);
        for (const auto& e: list_expr->elements) {
            gen_expr(e.get());
        }
        // 生成 OP_MAKE_LIST 指令
        code_chunks.back().code_list.emplace_back(
            Opcode::MAKE_LIST,
            list_expr->elements.size(),
            expr->pos
       );
        break;
    }
    case AstType::GetMemberExpr: {
        // 获取成员：生成对象表达式 -> 加载属性名 -> GET_ATTR指令
        auto get_mem = dynamic_cast<GetMemberExpr*>(expr);
        gen_expr(get_mem->father.get()); // 生成对象IR
        size_t name_idx = get_or_add_name(code_chunks.back().attr_names, get_mem->child->name);
        code_chunks.back().code_list.emplace_back(
            Opcode::GET_ATTR,
            name_idx,
            expr->pos
        );
        break;
    }
    case AstType::GetItemExpr: {
        auto get_mem_expr = dynamic_cast<GetItemExpr*>(expr);
        size_t arg_count = get_mem_expr->params.size();

        for (auto& arg : get_mem_expr->params) {
            gen_expr(arg.get());
        }

        // 生成 OP_MAKE_LIST 指令：将栈顶 arg_count 个元素打包成参数列表，压回栈
        code_chunks.back().code_list.emplace_back(
            Opcode::MAKE_LIST,
            arg_count,
            get_mem_expr->pos
        );

        gen_expr(get_mem_expr->father.get());

        code_chunks.back().code_list.emplace_back(
            Opcode::GET_ITEM,
            0,
            get_mem_expr->pos
        );
        break;
    }
    case AstType::LambdaExpr: {
        // 匿名函数：同普通函数声明，生成函数对象后加载
        auto lambda = dynamic_cast<LambdaExpr*>(expr);

        // 创建函数体
        code_chunks.emplace_back(CodeChunk());
        // 添加参数到lambda变量表
        for (const auto& param : lambda->params) {
            get_or_add_name(code_chunks.back().var_names, param);
        }
        // 生成lambda函数体
        gen_block(lambda->body.get());

        // 确保lambda有返回值（无显式返回则返回Nil）
        if (code_chunks.back().code_list.empty() || code_chunks.back().code_list.back().opc != Opcode::RET) {
            const auto nil = model::load_nil();
            const size_t nil_idx = get_or_add_const(nil);
            code_chunks.back().code_list.emplace_back(
                Opcode::LOAD_CONST,
                nil_idx,
                expr->pos
            );
            code_chunks.back().code_list.emplace_back(
                Opcode::RET,
                0,
                expr->pos
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

        // 生成lambda函数体IR
        code_obj->make_ref();
        const auto lambda_fn = new model::Function(
            lambda->name.empty() ? "<lambda>" : lambda->name,
            code_obj,
            lambda->params.size()
        );
        lambda_fn->has_rest_params = lambda->has_rest_params;


        // 加载lambda函数对象
        const size_t fn_const_idx = get_or_add_const(lambda_fn);
        code_chunks.back().code_list.emplace_back(
            Opcode::LOAD_CONST,
            fn_const_idx,
            expr->pos
        );
        code_chunks.back().code_list.emplace_back(
            Opcode::CREATE_CLOSURE,
            0,
            expr->pos
        );
        break;
    }
    case AstType::NilExpr : {
        const auto nil = model::load_nil();
        const size_t nil_idx = get_or_add_const(nil);
        code_chunks.back().code_list.emplace_back(
            Opcode::LOAD_CONST,
            nil_idx,
            expr->pos
        );
        break;
    }
    case AstType::BoolExpr : {
        const auto bool_ast = dynamic_cast<BoolExpr*>(expr);
        assert(bool_ast!=nullptr);
        const auto bool_obj = model::load_bool(bool_ast->val);
        const size_t bool_idx = get_or_add_const(bool_obj);
        code_chunks.back().code_list.emplace_back(
            Opcode::LOAD_CONST,
            bool_idx,
            expr->pos
        );
        break;
    }
    default:
        assert(false && "gen_expr: 未处理的表达式类型");
    }
}

void IRGenerator::gen_fn_call(CallExpr* call_expr) {
    assert(call_expr && "gen_fn_call: 函数调用节点为空");
    size_t arg_count = call_expr->args.size();

    // 生成所有参数的IR，最终打包成List（与原逻辑一致）
    for (auto& arg : call_expr->args) {
        gen_expr(arg.get());
    }

    // 生成 OP_MAKE_LIST 指令：将栈顶 arg_count 个元素打包成参数列表，压回栈
    code_chunks.back().code_list.emplace_back(
        Opcode::MAKE_LIST,
        arg_count,
        call_expr->pos
    );

    // 判断 callee 是否为 GetMemberExpr
    if (auto member_expr = dynamic_cast<GetMemberExpr*>(call_expr->callee.get())) {
        gen_expr(member_expr->father.get()); 

        // 获取方法名的字符串常量池索引
        const std::string& method_name = member_expr->child->name;
        size_t method_name_idx = get_or_add_name(code_chunks.back().attr_names, method_name);

        // 生成 CALL_METHOD 指令：操作数为 方法名索引 + 参数个数（用于校验）
        code_chunks.back().code_list.emplace_back(
            Opcode::CALL_METHOD,
            method_name_idx,
            call_expr->pos
        );
    } else {
        // 普通函数调用：生成函数对象IR → 生成 CALL 指令
        gen_expr(call_expr->callee.get());
        code_chunks.back().code_list.emplace_back(
            Opcode::CALL,
            arg_count,
            call_expr->pos
        );
    }
}

void IRGenerator::gen_dict(DictExpr* expr) {
    assert(expr != nullptr);
    // 处理字典键值对
    for (auto& [key, val_expr] : expr->elements) {
        gen_expr(key.get());
        gen_expr(val_expr.get());
    }

    size_t dict_size = expr->elements.size();
    code_chunks.back().code_list.emplace_back(
        Opcode::MAKE_DICT,
        dict_size,
        expr->pos
    );
}

}
