#pragma once

#include "ptr.hpp"
#include <cstdint>
#include <cstring>

template <typename T>
struct Vec {
    Ptr<T>  data;
    uint32_t len;
    uint32_t cap;

    /// 空构造
    constexpr Vec() noexcept : data(nullptr), len(0), cap(0) {}

    /// 从外部已分配内存绑定
    constexpr Vec(Ptr<T> buf, uint32_t buf_cap) noexcept
        : data(buf), len(0), cap(buf_cap)
    {}

    /// 判空
    constexpr auto is_empty() const noexcept -> bool {
        return len == 0;
    }

    /// 预留容量
    constexpr auto reserve(uint32_t new_cap) noexcept -> void {
        if (new_cap > cap) {
            cap = new_cap;
        }
    }

    // 尾插，外部保证 cap 足够
    constexpr auto push(const T& val) noexcept -> void {
        if (len < cap) {
            data[len++] = val;
        }
    }

    // 弹出末尾
    constexpr auto pop() noexcept -> void {
        if (len > 0) {
            len--;
        }
    }

    // 清空长度（不释放内存）
    constexpr auto clear() noexcept -> void {
        len = 0;
    }

    /// 下标读写
    constexpr auto operator[](uint32_t idx) noexcept -> T& {
        return data[idx];
    }
    constexpr auto operator[](uint32_t idx) const noexcept -> const T& {
        return data[idx];
    }

    /// 首尾元素
    constexpr auto front() noexcept -> T& {
        return data[0];
    }
    constexpr auto front() const noexcept -> const T& {
        return data[0];
    }

    constexpr auto back() noexcept -> T& {
        return data[len - 1];
    }
    constexpr auto back() const noexcept -> const T& {
        return data[len - 1];
    }

    /// 获取长度、容量
    constexpr auto size() const noexcept -> uint32_t {
        return len;
    }
    constexpr auto capacity() const noexcept -> uint32_t {
        return cap;
    }

    /// 获取底层指针
    constexpr auto get_data() noexcept -> Ptr<T> {
        return data;
    }
    constexpr auto get_data() const noexcept -> Ptr<T> {
        return data;
    }

    /// 重置绑定外部内存
    constexpr auto reset(Ptr<T> buf, uint32_t buf_cap) noexcept -> void {
        data = buf;
        len = 0;
        cap = buf_cap;
    }
};