/**
 * @file gc.hpp
 * @brief 垃圾回收器(GC)定义
 *  
 * @author azhz1107cat
 * @date 2026-5-3
 */

#pragma once

#include <vector>
#include <unordered_set>
#include <cstdlib>
#include "../models/models.hpp"

// ToDo: gc is developing...

namespace model {

class GC {
public:
    inline static std::unordered_set<Object*> obj_set;
    inline static std::unordered_set<Object*> marked;

    template<typename T, typename... Args>
    static T* alloc(Args&&... args) {
        // 在GC池中构造对象
        T* obj = new T(std::forward<Args>(args)...);
        obj_set.insert(obj);
        return obj;
    }

    /// mark
    static void mark_all(const std::vector<Object*>& root_set) {
        marked.clear();
        for (Object* root : root_set) {
            mark(root);
        }
    }

    static void mark(Object* obj) {
        if (!obj) return;
        if (marked.count(obj) || obj->is_important) return;

        marked.insert(obj);

        // 标记属性表子对象
        auto attrs_list = obj->attrs.to_vector();
        for (auto& p : attrs_list) {
            mark(p.second);
        }

        // 容器内部子对象标记
        mark_container(obj);
    }

    static void mark_container(Object* obj) {
        switch (obj->get_type()) {
        case Object::ObjectType::List: {
            auto list = static_cast<List*>(obj);
            for (auto e : list->val) mark(e);
            break;
        }
        case Object::ObjectType::Dictionary: {
            auto dict = static_cast<Dictionary*>(obj);
            auto kv_list = dict->val.to_vector();
            for (auto& p : kv_list) {
                mark(p.second.first);
                mark(p.second.second);
            }
            break;
        }
        case Object::ObjectType::Function: {
            auto fn = static_cast<Function*>(obj);
            mark(fn->code);
            for (auto fv : fn->free_vars) mark(fv);
            break;
        }
        case Object::ObjectType::Module: {
            auto m = static_cast<Module*>(obj);
            mark(m->code);
            break;
        }
        case Object::ObjectType::Unpack: {
            auto up = static_cast<Unpack*>(obj);
            mark(up->val);
            break;
        }
        default: break;
        }
    }

    /// sweep
    static void sweep() {
        std::vector<Object*> garbage;
        for (auto obj : obj_set) {
            if (!marked.contains(obj) && !obj->is_important) {
                garbage.push_back(obj);
            }
        }

        for (auto g : garbage) {
            obj_set.erase(g);
            delete g;
        }
    }

    /// 完整GC
    static void collect(const std::vector<Object*>& root_set) {
        mark_all(root_set);
        sweep();
    }

    static bool need_collect(size_t threshold = 1024) {
        return obj_set.size() >= threshold;
    }

    static void persist(Object* obj) {
        if (obj) obj->mark_as_important();
    }
};

} // namespace model