/**
 * @file hashmap.hpp
 * @brief 辅助容器（HashMap）核心定义与实现
 * 
 * @author azhz1107cat
 * @date 2025-10-25
 */

#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <sstream>
#include <utility>

namespace dep {

// 字符串哈希函数（FNV-1a算法）
inline size_t hash_string(const std::string& key) {
    constexpr size_t FNV_OFFSET = 14695981039346656037ULL;
    constexpr size_t FNV_PRIME = 1099511628211ULL;
    size_t hash = FNV_OFFSET;
    for (const char c : key) {
        hash ^= static_cast<size_t>(c);  // 异或当前字符
        hash *= FNV_PRIME;               // 乘以质数
    }
    return hash;
}

// 模板类：键为std::string，值为任意类型T的HashMap
template <typename VT>
class HashMap {
public:
    // 嵌套桶节点结构体（存储键值对、哈希值、链表指针）
    struct StringBucket {
        std::string key;
        VT value;
        size_t hash;
        std::shared_ptr<StringBucket> next;

        // 修复：先拷贝key，计算hash后再move，避免初始化顺序问题
        StringBucket(const std::string& k, VT val)
            : key(k),  // 先拷贝，不提前move
              value(std::move(val)),
              hash(hash_string(k)), // 直接用入参k计算hash，避免依赖成员变量初始化顺序
              next(nullptr) {}

        // 保留移动构造（优化性能）
        StringBucket(std::string&& k, VT&& val)
            : key(std::move(k)),
              value(std::move(val)),
              hash(hash_string(this->key)), // 此时key已move完成
              next(nullptr) {}
    };

    using Node = StringBucket;  // 简化节点类型名
    std::vector<std::shared_ptr<Node>> buckets_;  // 桶数组（每个元素是链表头指针）
    size_t elem_count_ = 0;                       // 元素总数
    const float load_factor_ = 0.7f;              // 负载因子（触发扩容的阈值）

    // 计算键对应的桶索引（利用位运算，要求桶大小为2的幂）
    [[nodiscard]] size_t getBucketIndex(const size_t hash) const {
        return hash & (this->buckets_.size() - 1);
    }

    // 扩容：桶数组大小翻倍，重新哈希所有元素
    void resize() {
        const size_t old_size = buckets_.size();
        const size_t new_size = old_size * 2;
        std::vector<std::shared_ptr<Node>> new_buckets(new_size, nullptr);

        for (size_t i = 0; i < old_size; ++i) {
            std::shared_ptr<Node> current = buckets_[i];
            while (current != nullptr) {
                const std::shared_ptr<Node> next = current->next;
                // 修复：使用节点自身缓存的hash计算新索引
                const size_t new_idx = current->hash & (new_size - 1);
                current->next = new_buckets[new_idx];
                new_buckets[new_idx] = current;
                current = next;
            }
        }

        buckets_.swap(new_buckets);
    }

    // 默认构造函数（初始桶大小为16，2的幂）
    explicit HashMap() {
        constexpr size_t init_size = 16;
        buckets_.resize(init_size, nullptr);
    }

    // 用键值对vector初始化
    explicit HashMap(const std::vector<std::pair<std::string, VT>>& vec) {
        // 计算初始桶大小（确保负载因子不超过阈值）
        size_t init_size = 16;
        while (init_size < (vec.size() / load_factor_)) {
            init_size *= 2;
        }
        buckets_.resize(init_size, nullptr);

        // 插入所有初始键值对
        for (const auto& [key, val] : vec) {
            insert(key, val);
        }
    }

    // 析构函数（默认即可，shared_ptr自动释放内存）
    ~HashMap() = default;

    // 插入/更新键值对（存在则更新并返回旧值，不存在则插入并返回默认值）
    VT insert(const std::string& key, VT val) {
        if (buckets_.empty()) {
            buckets_.resize(16, nullptr);
        }

        // 修复：先计算hash和bucket_idx（基于当前桶大小）
        const size_t hash = hash_string(key);
        size_t bucket_idx = getBucketIndex(hash);

        // 检查是否已存在（扩容前先查，避免扩容后索引变化）
        std::shared_ptr<Node> current = buckets_[bucket_idx];
        while (current != nullptr) {
            if (current->hash == hash && current->key == key) {
                VT old_val = std::move(current->value);
                current->value = std::move(val);
                return old_val; // 更新值，不递增计数
            }
            current = current->next;
        }

        // 检查负载因子，需要扩容则重新计算索引
        if (static_cast<float>(elem_count_) / buckets_.size() >= load_factor_) {
            this->resize();
            // 扩容后重新计算索引（基于新桶大小）
            bucket_idx = getBucketIndex(hash);
        }

        // 插入新节点（头插法）
        auto new_node = std::make_shared<Node>(key, std::move(val));
        new_node->next = buckets_[bucket_idx];
        buckets_[bucket_idx] = new_node;
        elem_count_++; // 仅插入新节点时递增

        return VT();
    }

    [[nodiscard]] std::shared_ptr<Node> find(const std::string& key) const {
        return find_in_current(key);
    }

    // 直接遍历所有值（不构造中间vector，析构等热路径使用）
    template<typename Func>
    void for_each_value(Func&& f) const {
        for (const auto& bucket_head : buckets_) {
            auto current = bucket_head;
            while (current != nullptr) {
                f(current->value);
                current = current->next;
            }
        }
    }

    // 仅在当前HashMap查找键（不递归父结构体）
    [[nodiscard]] std::shared_ptr<Node> find_in_current(const std::string& key) const {
        if (buckets_.empty()) {
            return nullptr;
        }

        const size_t hash = hash_string(key);
        const size_t bucket_idx = getBucketIndex(hash);
        if (bucket_idx >= buckets_.size()) {
            return nullptr;
        }

        // 遍历桶对应的链表查找
        std::shared_ptr<Node> current = buckets_[bucket_idx];
        while (current != nullptr) {
            if (current->hash == hash && current->key == key) {
                return current;
            }
            current = current->next;
        }

        return nullptr;
    }

    bool del(const std::string& attr_name) {
        // 空桶数组直接返回删除失败
        if (buckets_.empty()) {
            return false;
        }

        // 计算键的哈希值和桶索引
        const size_t hash = hash_string(attr_name);
        const size_t bucket_idx = getBucketIndex(hash);
        if (bucket_idx >= buckets_.size()) {
            return false;
        }

        // 遍历链表查找目标节点（需要前驱节点指针）
        std::shared_ptr<Node> prev = nullptr;
        std::shared_ptr<Node> current = buckets_[bucket_idx];

        while (current != nullptr) {
            // 哈希值+键双重校验（避免哈希碰撞）
            if (current->hash == hash && current->key == attr_name) {
                // 要删除的是头节点
                if (prev == nullptr) {
                    buckets_[bucket_idx] = current->next;
                }
                // 要删除的是中间/尾节点
                else {
                    prev->next = current->next;
                }

                // 释放当前节点（shared_ptr自动管理，断开链接后计数减1）
                current.reset();
                elem_count_--; // 减少元素计数
                return true;   // 删除成功
            }

            prev = current;
            current = current->next;
        }

        // 遍历完未找到对应键，删除失败
        return false;
    }

    // 转换为字符串（需T支持to_string()成员函数）
    [[nodiscard]] std::string to_string() const {
        std::stringstream ss;
        ss << "{ ";

        size_t current_idx = 0;
        size_t total_count = this->elem_count_; // 直接用已统计的数量，无需遍历

        for (const auto& bucket_head : buckets_) {
            auto current = bucket_head;
            while (current != nullptr) {
                ss << current->key << ": ";
                if constexpr (std::is_pointer_v<VT>) {
                    ss << static_cast<void*>(current->value);
                } else {
                    ss << current->value.to_string();
                }
                if (current_idx < total_count - 1) {
                    ss << ", ";
                }
                current = current->next;
                current_idx++;
            }
        }

        ss << " }";
        return ss.str();
    }

    // 转换为键值对vector
    [[nodiscard]] std::vector<std::pair<std::string, VT>> to_vector() const {
        std::vector<std::pair<std::string, VT>> vec;
        for (const auto& bucket_head : buckets_) {
            auto current = bucket_head;
            while (current != nullptr) {
                vec.emplace_back(current->key, current->value);
                current = current->next;
            }
        }
        return vec;
    }

    // 深拷贝构造函数
    HashMap(const HashMap& other)
        : load_factor_(other.load_factor_) {
        // 获取源HashMap的所有键值对
        auto all_kv = other.to_vector();
        // 计算初始桶大小
        size_t init_size = 16;
        while (init_size < (all_kv.size() / load_factor_)) {
            init_size *= 2;
        }
        buckets_.resize(init_size, nullptr);

        // 插入所有键值对（深拷贝）
        for (auto& [key, val] : all_kv) {
            this->insert(key, val);
        }
        this->elem_count_ = other.elem_count_;
    }

    HashMap(HashMap&& other) noexcept
    : buckets_(std::move(other.buckets_)),
      elem_count_(other.elem_count_),
      load_factor_(other.load_factor_) {
        other.elem_count_ = 0; // 置空原对象计数
        other.buckets_.clear(); // 置空原对象桶数组
    }

    HashMap& operator=(const HashMap& other) {
        if (this == &other) return *this;

        buckets_.clear();
        elem_count_ = 0; // 重置计数

        auto all_kv = other.to_vector();
        size_t init_size = 16;
        while (init_size < (all_kv.size() / load_factor_)) {
            init_size *= 2;
        }
        buckets_.resize(init_size, nullptr);

        // 修复：手动计数，避免insert自动递增的误差
        for (auto& [key, val] : all_kv) {
            this->insert(key, val);
        }
        this->elem_count_ = all_kv.size(); // 直接赋值正确的数量

        return *this;
    }

    // 修复移动赋值运算符
    HashMap& operator=(HashMap&& other) noexcept {
        if (this == &other) return *this; // 自赋值检查

        // 释放当前资源
        buckets_.clear();
        elem_count_ = 0;

        // 转移其他对象的资源（删除 load_factor_ = other.load_factor_）
        buckets_ = std::move(other.buckets_);
        elem_count_ = other.elem_count_;

        // 置空原对象
        other.elem_count_ = 0;
        other.buckets_.clear();

        return *this;
    }
};

} // namespace dep