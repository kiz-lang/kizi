#pragma once

#include <cstdint>
#include <cstring>

/// 结果状态标记
enum class ResTag : uint8_t {
    Ok,
    Err
};

template <typename T, typename E>
struct Result {
    ResTag tag;

    union {
        T ok;
        E err;
    } as;

    /// 构造 Ok
    static constexpr auto ok(const T& val) noexcept -> Result {
        Result r{};
        r.tag = ResTag::Ok;
        r.as.ok = val;
        return r;
    }

    /// 构造 Err
    static constexpr auto err(const E& val) noexcept -> Result {
        Result r{};
        r.tag = ResTag::Err;
        r.as.err = val;
        return r;
    }

    /// 判断
    constexpr auto is_ok() const noexcept -> bool {
        return tag == ResTag::Ok;
    }

    constexpr auto is_err() const noexcept -> bool {
        return tag == ResTag::Err;
    }

    /// 取值 不做检查 使用者自己保证合法
    constexpr auto unwrap_ok() noexcept -> T& {
        return as.ok;
    }
    constexpr auto unwrap_ok() const noexcept -> const T& {
        return as.ok;
    }

    constexpr auto unwrap_err() noexcept -> E& {
        return as.err;
    }
    constexpr auto unwrap_err() const noexcept -> const E& {
        return as.err;
    }

    /// 兜底取值，出错给默认值
    constexpr auto unwrap_or(const T& def) const noexcept -> T {
        return is_ok() ? as.ok : def;
    }
};