/**
 * @file bigint.hpp
 * @brief 无限精度整数（BigInt）核心定义
 *  * 采用逆序存储 digits（如 123 存储为 [3,2,1]），最小化进位/借位时的元素移动开销；
 * 支持 size_t 与合法数字字符串初始化，内置高效比较与 IO 操作。
 * @author azhz1107cat
 * @date 2025-10-25
 * @note 修复核心问题：
 *  1. div_mod_unsigned 中用二分查找替换逐次累加，避免无限循环
 *  2. Karatsuba 乘法添加基准阈值，减少递归深度
 *  3. shift_left 优化内存操作，避免 vector 开头插入的低效行为
 */

#pragma once
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <ostream>
#include <string>
#include <utility>
#include <vector>
#include <climits>  // 新增：用于 ULLONG_MAX

#include "../src/kiz.hpp"

namespace dep {

class BigInt {
    std::vector<uint8_t> digits_; // 逆序存储（低位在前），每个元素 0-9
    bool is_negative_;            // false=正，true=负；0恒为非负

    /**
     * @brief 移除前导零（逆序中为末尾零），确保数字表示唯一
     */
    void trim_leading_zeros() {
        while (digits_.size() > 1 && digits_.back() == 0) {
            digits_.pop_back();
        }
        if (digits_.size() == 1 && digits_[0] == 0) {
            is_negative_ = false;
        }
    }

    /**
     * @brief 绝对值比较（辅助减法/乘法）：返回 *this 的绝对值是否小于 other 的绝对值
     */
    [[nodiscard]] bool abs_less(const BigInt& other) const {
        if (digits_.size() != other.digits_.size()) {
            return digits_.size() < other.digits_.size();
        }
        // 从最高位（digits_末尾）逐位比
        for (auto it1 = digits_.rbegin(), it2 = other.digits_.rbegin(); it1 != digits_.rend(); ++it1, ++it2) {
            if (*it1 != *it2) {
                return *it1 < *it2;
            }
        }
        return false; // 绝对值相等
    }

    /**
     * @brief 核心辅助：计算 (dividend / divisor) 的商和余数（无符号，仅处理正整数）
     * @param dividend 被除数（非负）
     * @param divisor 除数（正，非零）
     * @return  pair<商, 余数>（均非负）
     * @note 修复：用二分查找替换逐次累加，避免无限循环
     */
    static std::pair<BigInt, BigInt> div_mod_unsigned(const BigInt& dividend, const BigInt& divisor) {
        BigInt quotient, remainder;
        quotient.digits_.clear();
        remainder.digits_.clear();
        remainder.is_negative_ = false;
        quotient.is_negative_ = false;

        // 从被除数最高位（digits_末尾）开始逐位构建余数
        for (auto it = dividend.digits_.rbegin(); it != dividend.digits_.rend(); ++it) {
            // 余数 = 余数 * 10 + 当前位（模拟手工除法）
            remainder = remainder * BigInt(10) + BigInt(static_cast<size_t>(*it));

            // 修复核心：二分查找最大的q_digit（0-9），避免逐次累加的无限循环
            uint8_t q_digit = 0;
            uint8_t left = 0, right = 9; // 单个数字位，范围固定0-9
            while (left <= right) {
                uint8_t mid = (left + right) / 2;
                BigInt temp = divisor * BigInt(static_cast<size_t>(mid));
                if (temp <= remainder) {
                    q_digit = mid;
                    left = mid + 1;
                } else {
                    right = mid - 1;
                }
            }

            // 商的当前位存入（注意商是正序构建，后续需逆序）
            quotient.digits_.push_back(q_digit);
            // 更新余数：remainder = remainder - (q_digit * divisor)
            if (q_digit > 0) {
                remainder -= (divisor * BigInt(static_cast<size_t>(q_digit)));
            }
        }

        // 商是正序存储（高位在前），需逆转为统一的低位在前格式
        std::reverse(quotient.digits_.begin(), quotient.digits_.end());
        quotient.trim_leading_zeros();
        remainder.trim_leading_zeros();

        return {quotient, remainder};
    }

    /**
     * @brief Karatsuba乘法核心（无符号，仅处理正整数）
     * 时间复杂度 O(n^log3) ≈ O(n^1.58)，远快于普通O(n²)逐位乘
     * @note 修复：添加基准阈值，减少递归深度
     */
    static BigInt karatsuba_mul(const BigInt& a, const BigInt& b) {
        // 修复核心：基准条件优化，长度≤8时用普通乘法，减少递归深度
        const size_t BASE_THRESHOLD = 8;
        if (a.digits_.size() <= BASE_THRESHOLD || b.digits_.size() <= BASE_THRESHOLD) {
            BigInt res;
            res.digits_.resize(a.digits_.size() + b.digits_.size(), 0);
            // 逐位相乘，累加至对应位置
            for (size_t i = 0; i < a.digits_.size(); ++i) {
                uint32_t carry = 0; // 用32位存进位，避免溢出
                for (size_t j = 0; j < b.digits_.size(); ++j) {
                    uint32_t sum = res.digits_[i + j] + a.digits_[i] * b.digits_[j] + carry;
                    res.digits_[i + j] = static_cast<uint8_t>(sum % 10);
                    carry = sum / 10;
                }
                // 处理剩余进位（可能超过1位）
                size_t carry_pos = i + b.digits_.size();
                while (carry > 0) {
                    if (carry_pos >= res.digits_.size()) {
                        res.digits_.push_back(0);
                    }
                    uint32_t sum = res.digits_[carry_pos] + carry;
                    res.digits_[carry_pos] = static_cast<uint8_t>(sum % 10);
                    carry = sum / 10;
                    carry_pos++;
                }
            }
            res.trim_leading_zeros();
            return res;
        }

        // m 取较小长度的一半（向上取整）
        size_t min_len = std::min(a.digits_.size(), b.digits_.size());
        size_t m = (min_len + 1) / 2; // 向上取整，比如 min_len=5 → m=3，min_len=4→m=2

        BigInt a_low, a_high, b_low, b_high;

        // 拆分 a：a_low = a % 10^m，a_high = a / 10^m（逆序存储，前m位是低位）
        a_low.digits_ = std::vector<uint8_t>(a.digits_.begin(), a.digits_.begin() + std::min(m, a.digits_.size()));
        if (a.digits_.size() > m) {
            a_high.digits_ = std::vector<uint8_t>(a.digits_.begin() + m, a.digits_.end());
        } else {
            a_high.digits_ = {0}; // 不足高位补0
        }

        // 拆分 b（同 a）
        b_low.digits_ = std::vector<uint8_t>(b.digits_.begin(), b.digits_.begin() + std::min(m, b.digits_.size()));
        if (b.digits_.size() > m) {
            b_high.digits_ = std::vector<uint8_t>(b.digits_.begin() + m, b.digits_.end());
        } else {
            b_high.digits_ = {0};
        }

        // Karatsuba公式：z0*10^(2m) + (z1 - z0 - z2)*10^m + z2
        BigInt z0 = karatsuba_mul(a_low, b_low);
        BigInt z1 = karatsuba_mul(a_low + a_high, b_low + b_high);
        BigInt z2 = karatsuba_mul(a_high, b_high);

        // 按标准顺序计算（加法交换律下结果一致，但更易读）
        BigInt res = z2.shift_left(2 * m) + (z1 - z0 - z2).shift_left(m) + z0;
        res.trim_leading_zeros();
        return res;
    }

    /**
     * @brief 左移（乘以 10^k，逆序存储中为开头补k个0）
     * @note 修复：优化内存操作，避免 vector 开头 insert 的低效行为
     */
    [[nodiscard]] BigInt shift_left(size_t k) const {
        if (k == 0 || (digits_.size() == 1 && digits_[0] == 0)) {
            return *this;
        }
        BigInt res = *this;
        // 修复核心：先resize预分配，再后移元素，最后填充0，避免开头insert
        size_t old_size = res.digits_.size();
        res.digits_.resize(old_size + k, 0); // 扩展内存
        // 原有元素后移k位
        for (size_t i = old_size + k - 1; i >= k; --i) {
            res.digits_[i] = res.digits_[i - k];
        }
        // 前k位置0
        std::fill(res.digits_.begin(), res.digits_.begin() + k, 0);
        res.trim_leading_zeros(); // 补0后清理前导零
        return res;
    }

    /**
     * @brief 无符号减法：计算 |a| - |b|，要求 |a| >= |b|
     */
    static BigInt unsigned_subtract(const BigInt& a, const BigInt& b) {
        assert(!a.abs_less(b) && "unsigned_subtract requires |a| >= |b|");

        BigInt result;
        result.digits_.clear();
        result.is_negative_ = false;

        uint32_t borrow = 0;
        for (size_t i = 0; i < a.digits_.size(); ++i) {
            uint32_t a_digit = a.digits_[i];
            uint32_t b_digit = (i < b.digits_.size()) ? b.digits_[i] : 0;

            int32_t diff = static_cast<int32_t>(a_digit) - static_cast<int32_t>(b_digit) - borrow;

            if (diff < 0) {
                diff += 10;
                borrow = 1;
            } else {
                borrow = 0;
            }

            result.digits_.push_back(static_cast<uint8_t>(diff));
        }

        // 移除前导零
        while (result.digits_.size() > 1 && result.digits_.back() == 0) {
            result.digits_.pop_back();
        }

        return result;
    }

    /**
    * @brief 辅助函数：无符号快速幂（底数和指数均为非负整数）
    * 二分幂核心逻辑：a^b = (a^(b/2))^2 （b为偶数） / (a^(b/2))^2 * a （b为奇数）
    */
public:
    static BigInt fast_pow_unsigned(const BigInt& base, const BigInt& exp) {
        BigInt result(1); // 初始结果为 1（乘法单位元）
        BigInt current_base = base;
        BigInt current_exp = exp;
        const BigInt two(2);

        while (current_exp > BigInt(0)) {
            // 若当前指数为奇数，结果 *= 当前底数
            if (current_exp % two == BigInt(1)) {
                result = result * current_base; // 复用 Karatsuba 乘法，高效
            }
            // 底数平方，指数减半（整除）
            current_base = current_base * current_base;
            current_exp = current_exp / two; // 整数除法，向下取整
        }

        return result;
    }

    // ========================= 构造与析构 =========================
    BigInt() : is_negative_(false), digits_(1, 0) {}
    BigInt(size_t val) : is_negative_(false) {
        if (val == 0) { digits_.push_back(0); return; }
        while (val > 0) { digits_.push_back(static_cast<uint8_t>(val % 10)); val /= 10; }
    }
    // 精确匹配unsigned long long，避免与long long重载产生转换二义性
    BigInt(unsigned long long val) : is_negative_(false) {
        if (val == 0) { digits_.push_back(0); return; }
        while (val > 0) { digits_.push_back(static_cast<uint8_t>(val % 10)); val /= 10; }
    }
    // 有符号重载：防止负整数经隐式转换到size_t后变成巨大无符号数
    BigInt(long long val) : is_negative_(val < 0) {
        auto mag = is_negative_ ? static_cast<unsigned long long>(-(val + 1)) + 1 : static_cast<unsigned long long>(val);
        if (mag == 0) { digits_.push_back(0); return; }
        while (mag > 0) { digits_.push_back(static_cast<uint8_t>(mag % 10)); mag /= 10; }
    }
    BigInt(int val) : BigInt(static_cast<long long>(val)) {}
    BigInt(const std::string& s) : is_negative_(false) {
        if (s.empty()) { digits_.push_back(0); return; }
        size_t start_idx = 0;
        if (s[0] == '-') { is_negative_ = true; start_idx = 1; }
        // 仅一个负号没有数字：按0处理，避免构造出空digits的非法状态
        if (s.size() == start_idx) { digits_.push_back(0); is_negative_ = false; return; }
        for (auto it = s.rbegin(); it != s.rend() - start_idx; ++it) {
            if (*it < '0' || *it > '9') { digits_ = {0}; is_negative_ = false; return; }
            digits_.push_back(static_cast<uint8_t>(*it - '0'));
        }
        trim_leading_zeros();
    }
    BigInt(BigInt&& other) noexcept : digits_(std::move(other.digits_)), is_negative_(other.is_negative_) {
        // moved-from对象重置为合法的0状态（而非空digits），保证其后的比较/运算自洽
        other.digits_.assign(1, 0);
        other.is_negative_ = false;
    }
    BigInt& operator=(BigInt&& other) noexcept {
        if (this != &other) {
            digits_ = std::move(other.digits_);
            is_negative_ = other.is_negative_;
            other.digits_.assign(1, 0);
            other.is_negative_ = false;
        }
        return *this;
    }

    BigInt(const BigInt& other) = default;
    BigInt& operator=(const BigInt& other) = default;
    ~BigInt() = default;

    [[nodiscard]] bool is_negative() const {
        return is_negative_;
    }

    // ========================= 绝对值 =========================
    [[nodiscard]] BigInt abs() const {
        BigInt res = *this;
        res.is_negative_ = false;
        return res;
    }

    // ========================= 比较运算符 =========================
    bool operator==(const BigInt& other) const {
        if (is_negative_ != other.is_negative_ || digits_.size() != other.digits_.size()) return false;
        return digits_ == other.digits_;
    }
    bool operator!=(const BigInt& other) const { return !(*this == other); }
    bool operator<(const BigInt& other) const {
        if (is_negative_ != other.is_negative_) return is_negative_;
        if (digits_.size() != other.digits_.size()) return is_negative_ ? (digits_.size() > other.digits_.size()) : (digits_.size() < other.digits_.size());
        for (auto it1 = digits_.rbegin(), it2 = other.digits_.rbegin(); it1 != digits_.rend(); ++it1, ++it2) {
            if (*it1 != *it2) return is_negative_ ? (*it1 > *it2) : (*it1 < *it2);
        }
        return false;
    }
    bool operator>(const BigInt& other) const { return other < *this; }
    bool operator<=(const BigInt& other) const { return *this < other || *this == other; }
    bool operator>=(const BigInt& other) const { return *this > other || *this == other; }

    // ========================= 核心运算：加法 =========================
    BigInt operator+(const BigInt& other) const {
        BigInt res;
        res.digits_.clear(); // 清空默认的[0]

        // 情况1：同号（都正或都负）→ 绝对值相加，符号不变
        if (is_negative_ == other.is_negative_) {
            res.is_negative_ = is_negative_;
            uint32_t carry = 0; // 进位（用32位避免溢出）
            size_t max_len = std::max(digits_.size(), other.digits_.size());

            for (size_t i = 0; i < max_len || carry > 0; ++i) {
                // 取当前位（不足补0）
                uint32_t a = (i < digits_.size()) ? digits_[i] : 0;
                uint32_t b = (i < other.digits_.size()) ? other.digits_[i] : 0;
                uint32_t sum = a + b + carry;

                res.digits_.push_back(static_cast<uint8_t>(sum % 10));
                carry = sum / 10;
            }
        }
        // 情况2：异号（一正一负）→ 绝对值相减，符号取绝对值大的
        else {
            if (abs_less(other)) {
                // this绝对值 < other绝对值
                res = unsigned_subtract(other, *this);
                res.is_negative_ = other.is_negative_;
            } else {
                // this绝对值 >= other绝对值
                res = unsigned_subtract(*this, other);
                res.is_negative_ = is_negative_;
            }
        }

        res.trim_leading_zeros();
        return res;
    }

    BigInt& operator+=(const BigInt& other) {
        *this = *this + other;
        return *this;
    }

    // ========================= 核心运算：减法 =========================
    BigInt operator-(const BigInt& other) const {
        // 特殊情况：减自己 → 0（保留，提升效率）
        if (*this == other) {
            return {0};
        }
        // 数学本质：a - b = a + (-b) （核心修复，复用加法的正确逻辑）
        BigInt other_neg = other;       // 复制被减数
        other_neg.is_negative_ = !other_neg.is_negative_; // 取反被减数的符号（得到相反数）
        return *this + other_neg;       // 调用正确的加法运算符，完成所有计算
    }


    BigInt& operator-=(const BigInt& other) {
        *this = *this - other;
        return *this;
    }

    // ========================= 核心运算：乘法 =========================
    BigInt operator*(const BigInt& other) const {
        if ((digits_.size() == 1 && digits_[0] == 0) || (other.digits_.size() == 1 && other.digits_[0] == 0)) {
            return {0};
        }

        // 先计算无符号乘法
        BigInt res = karatsuba_mul(this->abs(), other.abs());
        // 再设置符号（异或运算：同号得正，异号得负）
        res.is_negative_ = is_negative_ ^ other.is_negative_;
        res.trim_leading_zeros();
        return res;
    }

    BigInt& operator*=(const BigInt& other) {
        *this = *this * other;
        return *this;
    }

    // ========================= 核心运算：取模 =========================
    BigInt operator%(const BigInt& other) const {
        // 断言：除数不能为0
        if(other.digits_.size() == 1 && other.digits_[0] == 0)
            throw NativeFuncError("CalculateError", "BigInt mod: divisor cannot be zero");

        // 特殊情况：被除数为0→结果为0
        if (digits_.size() == 1 && digits_[0] == 0) {
            return BigInt(0);
        }

        // 计算被除数、除数的绝对值
        BigInt a_abs = this->abs();
        BigInt b_abs = other.abs();

        // 无符号取模（得到非负余数）
        BigInt remainder = div_mod_unsigned(a_abs, b_abs).second;

        // 截断语义（与C一致）：余数符号与被除数一致
        if (this->is_negative_ && remainder != BigInt(0)) {
            remainder.is_negative_ = true;
        }

        remainder.trim_leading_zeros();
        return remainder;
    }

    BigInt& operator%=(const BigInt& other) {
        *this = *this % other;
        return *this;
    }

    // ========================= 核心运算：除法 =========================
    BigInt operator/(const BigInt& other) const {
        // 断言：除数不能为0
        if(other.digits_.size() == 1 && other.digits_[0] == 0)
            throw NativeFuncError("CalculateError", "divisor cannot be zero");

        // 特殊情况：被除数为0→结果为0
        if (digits_.size() == 1 && digits_[0] == 0) {
            return {0};
        }

        // 符号：同号为正，异号为负（异或运算）
        const bool res_neg = is_negative_ ^ other.is_negative_;

        // 无符号除法取商
        BigInt quotient = div_mod_unsigned(this->abs(), other.abs()).first;
        quotient.is_negative_ = res_neg;
        quotient.trim_leading_zeros();

        return quotient;
    }

    BigInt& operator/=(const BigInt& other) {
        *this = *this / other;
        return *this;
    }

    // ========================= 核心运算：幂运算 =========================
    BigInt pow(const BigInt& other) const {
        // 指数必须为非负整数
        if(other.is_negative_)
            throw NativeFuncError("CalculateError", "use negative int into Bigint.pow is unsupported");

        const BigInt& exp = other;
        const BigInt zero(0);
        const BigInt one(1);

        // 边界情况：指数为 0 → 结果为 1
        if (exp == zero) {
            return one;
        }

        // 边界情况：底数为 0 → 结果为 0（指数为正）
        if (*this == zero) {
            return zero;
        }

        // 提取底数的绝对值（后续统一计算正数幂，最后处理符号）
        BigInt base_abs = this->abs();
        // 快速幂计算（处理正整数幂）
        BigInt result_abs = fast_pow_unsigned(base_abs, exp);
        // 确定结果符号（仅当底数为负且指数为奇数时，结果为负）
        bool is_result_neg = this->is_negative_ && (exp % BigInt(2) == one);
        result_abs.is_negative_ = is_result_neg;

        result_abs.trim_leading_zeros();
        return result_abs;
    }

    // ========================= 字符串/数值转换 =========================
    [[nodiscard]] std::string to_string() const {
        std::string result;

        // 边界处理：若 digits_ 为空（未初始化/零值），直接返回 "0"
        if (digits_.empty()) {
            return "0";
        }

        // 处理负数符号
        if (is_negative_) {
            result += '-';
        }

        // 逆序遍历
        for (auto it = digits_.rbegin(); it != digits_.rend(); ++it) {
            // *it 是 0-9 的数字，+'0' 转换为对应的 ASCII 字符（如 3 → '3'）
            result += static_cast<char>('0' + *it);
        }

        return result;
    }

    [[nodiscard]] unsigned long long to_unsigned_long_long() const {
        // 检查是否为负数
        if (is_negative_) {
            throw NativeFuncError("CalculateError","BigInt is negative, cannot convert to unsigned long long");
        }

        // 检查是否超出范围（显式走size_t构造，避免与long long重载二义）
        const BigInt ull_max_big(static_cast<size_t>(ULLONG_MAX));
        if (*this > ull_max_big) {
            throw NativeFuncError("CalculateError","BigInt value exceeds ULLONG_MAX");
        }

        unsigned long long result = 0;
        // 修复核心：逆序遍历 digits_（从高位到低位）
        for (auto it = digits_.rbegin(); it != digits_.rend(); ++it) {
            unsigned char digit = *it;
            result = result * 10 + digit;
        }

        return result;
    }

    // ========================= 友元：输出运算符 =========================
    friend std::ostream& operator<<(std::ostream& os, const BigInt& num) {
        os << num.to_string();
        return os;
    }
};

} // namespace dep