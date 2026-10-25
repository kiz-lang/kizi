/**
 * @file handle_make.cpp
 * @brief 列表/字典构造与调用栈位置信息生成
 * @author azhz1107cat
 * @date 2026-10-25
 */
#include "vm.hpp"
#include "../models/models.hpp"
#include "opcode/opcode.hpp"

namespace kiz {

// 辅助函数
auto Vm::make_pos_info() -> std::vector<std::pair<std::string, err::PositionInfo>> {
    size_t frame_index = 0;
    std::vector<std::pair<std::string, err::PositionInfo>> positions;
    std::string path;
    for (const auto& frame: call_stack) {
        if (const auto m = dynamic_cast<model::Module*>(frame->owner)) {
            path = m->path;
        }
        err::PositionInfo pos{};
        bool is_last_frame = frame_index == call_stack.size() - 1;
        // pc下标钳制到合法范围：指令抛错时pc未推进（可能为0），pc-1会下溢；
        // 帧跑完未出栈时pc可能等于code.size()
        size_t safe_pc = frame->pc;
        if (safe_pc >= frame->code_object->code.size() && !frame->code_object->code.empty()) {
            safe_pc = frame->code_object->code.size() - 1;
        }
        if (is_last_frame) {
            pos = frame->code_object->code.at(safe_pc).pos;
        } else {
            pos = frame->code_object->code.at(safe_pc > 0 ? safe_pc - 1 : 0).pos;
        }
        positions.emplace_back(path, pos);
        ++frame_index;
    }
    return positions;
}

void Vm::make_list(size_t len) {
    size_t elem_count = len;
    assert(op_stack.size() >= elem_count);

    std::vector<model::Object*> elem_list;   // 借用指针或新拷贝，交给List构造统一持有
    std::vector<model::Object*> transferred; // 各元素从操作数栈移交来的引用，构造完成后统一归还
    elem_list.reserve(elem_count);

    for (size_t i = 0; i < elem_count; ++i) {
        auto elem = simple_get_and_pop_stack_top();
        if (auto pkg = dynamic_cast<model::Unpack*>(elem)) {
            // 解包成员：逐个驱动迭代器收集产出
            std::vector<model::Object*> unpack_list;
            while (true) {
                pkg->val->make_ref(); // 方法调用的self借用需临时持有
                call_method(pkg->val, "__next__", {});
                pkg->val->del_ref();
                auto res = simple_get_and_pop_stack_top();
                if (res == model::stop_iter_signal) {
                    break;
                }
                unpack_list.push_back(res); // 携带__next__结果压栈时的引用
            }
            elem->del_ref(); // 归还Unpack对象自身从操作数栈移交来的引用
            // 解包产出需先反转入栈顺序，与外层统一reverse后恢复原序
            for (auto u : std::ranges::reverse_view(unpack_list)) {
                elem_list.push_back(model::copy_if_mutable(u));
                transferred.push_back(u);
            }
        } else {
            elem_list.push_back(model::copy_if_mutable(elem));
            transferred.push_back(elem);
        }
    }
    std::ranges::reverse(elem_list); // 恢复原序

    auto list_obj = new model::List(elem_list); // 构造内部为每个元素make_ref
    push_to_stack(list_obj);

    for (auto t : transferred) {
        t->del_ref(); // 归还各元素从操作数栈移交来的引用
    }
}

void Vm::make_dict(size_t len) {
    size_t elem_count = len;
    const size_t total_elems = elem_count * 2;
    assert(op_stack.size() >= total_elems);

    std::vector<std::pair<dep::BigInt, std::pair<model::Object*, model::Object*>>> elem_list;
    std::vector<model::Object*> transferred; // key/value从操作数栈移交来的引用，构造完成后统一归还
    elem_list.reserve(elem_count);

    for (size_t i = 0; i < elem_count; ++i) {
        auto value = simple_get_and_pop_stack_top(); // 弹出 value
        auto key = simple_get_and_pop_stack_top();   // 弹出 key

        // 计算哈希
        call_method(key, "__hash__", {});
        auto hash_obj = simple_get_and_pop_stack_top();
        auto hashed_int = dynamic_cast<model::Int*>(hash_obj);
        if (!hashed_int) {
            hash_obj->del_ref();
            key->del_ref();
            value->del_ref();
            throw NativeFuncError("TypeError", "__hash__ must return an integer");
        }
        // value为可变对象时此处得到新拷贝，key保持借用，均由Dictionary构造统一持有
        elem_list.emplace_back(hashed_int->val, std::pair{key, model::copy_if_mutable(value)});
        hash_obj->del_ref(); // 归还哈希结果的引用

        transferred.push_back(key);
        transferred.push_back(value);
    }

    auto dict_obj = new model::Dictionary(dep::Dict(elem_list)); // 构造内部为每个key/value make_ref
    push_to_stack(dict_obj);

    for (auto t : transferred) {
        t->del_ref(); // 归还key/value从操作数栈移交来的引用
    }
}
}
