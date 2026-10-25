/**
 * @file decimal.hpp
 * @brief 无限精度小数（Decimal）核心定义与实现（精确十进制表示）
 * @author azhz1107cat
 * @date 2026-10-25
 */
#pragma once
#include "bigint.hpp"
#include <string>
#include <cassert>
#include <algorithm>
#include <functional>

namespace dep {

class Decimal {
    BigInt mantissa_;   // 尾数（包含符号，归一化后无末尾零）
    int exponent_;      // 指数：value = mantissa_ * 10^exponent_

    /**
     * @brief 归一化：确保尾数无末尾零，保证表示唯一性
     * 例如：1200×10^-3 → 12×10^-1；-000 → 0×10^0
     */
    void normalize() {
        if (mantissa_ == BigInt(0)) {
            exponent_ = 0;
            return;
        }
        // 移除尾数末尾的零，同步调整指数
        BigInt ten(10);
        while (mantissa_ % ten == BigInt(0)) {
            mantissa_ /= ten;
            exponent_ += 1;
        }
    }

    /**
     * @brief 修复：对齐两个Decimal的指数（核心错误修复）
     * 正确逻辑：将两个数对齐到**更小的指数**（更低的量级），避免大数被缩小导致精度丢失
     * @param a 输入Decimal
     * @param b 输入Decimal
     * @param a_mant 输出：a对齐后的尾数
     * @param b_mant 输出：b对齐后的尾数
     * @return 公共指数（两个数中更小的那个）
     */
    static int align_exponent(const Decimal& a, const Decimal& b, BigInt& a_mant, BigInt& b_mant) {
        // 找到更小的指数（公共指数）
        int common_exp = std::min(a.exponent_, b.exponent_);
        // 用64位计算缩放差，避免int相减溢出UB
        long long a_scale = static_cast<long long>(a.exponent_) - common_exp;
        long long b_scale = static_cast<long long>(b.exponent_) - common_exp;
        // 巨大的指数差意味着要把尾数放大到10^上亿位，属于OOM/挂死向量，直接拒绝
        constexpr long long max_scale = 10000000;  // 允许的最大指数差（一千万位）
        if (a_scale > max_scale || b_scale > max_scale) {
            throw NativeFuncError("CalculateError", "Decimal exponent difference too large");
        }

        BigInt ten(10);
        // 放大尾数，使两者指数都等于common_exp
        a_mant = a.mantissa_ * BigInt::fast_pow_unsigned(ten, BigInt(a_scale));
        b_mant = b.mantissa_ * BigInt::fast_pow_unsigned(ten, BigInt(b_scale));

        return common_exp;
    }

    Decimal(const BigInt& mantissa, int exponent)
        : mantissa_(mantissa), exponent_(exponent) {
    }

public:
    // ========================= 构造函数 =========================
    Decimal() : mantissa_(0), exponent_(0) {}

    // 从BigInt构造（指数为0）
    explicit Decimal(const BigInt& mantissa) : mantissa_(mantissa), exponent_(0) {
        normalize();
    }

    // 修复：整数构造函数（排除char等字符类型误入；走有符号BigInt构造避免abs(INT_MIN) UB）
    template <typename T, typename = std::enable_if_t<
        std::is_integral_v<T> && !std::is_same_v<T, char> && !std::is_same_v<T, char8_t>
        && !std::is_same_v<T, signed char> && !std::is_same_v<T, unsigned char>>>
    explicit Decimal(T val) : mantissa_(BigInt(static_cast<long long>(val))), exponent_(0) {
        normalize();
    }

    explicit Decimal(const std::string& s) {
        // 空字符串直接报错
        if (s.empty()) {
            throw NativeFuncError("ParseError", "Empty string is not a valid decimal");
        }

        std::string str = s;
        bool is_neg = false;
        size_t start = 0;

        // 处理可选的正负号
        if (str[0] == '-') {
            is_neg = true;
            start = 1;
        } else if (str[0] == '+') {
            start = 1;
        }

        // 如果只有符号没有数字部分，报错
        if (start >= str.length()) {
            throw NativeFuncError("ParseError", "Sign only without digits");
        }

        // 检查指数部分（e/E）的位置
        size_t exp_pos = str.find_first_of("eE", start);
        std::string mantissa_part;   // 尾数部分（整数+小数，不含指数）
        std::string exp_part;        // 指数部分（字符串形式）

        if (exp_pos != std::string::npos) {
            // 指数符号 e/E 后面必须有内容
            if (exp_pos + 1 >= str.length()) {
                throw NativeFuncError("ParseError", "Exponent symbol without following digits");
            }
            mantissa_part = str.substr(start, exp_pos - start);
            exp_part = str.substr(exp_pos + 1);
            // 指数部分允许以 + 或 - 开头
            size_t exp_start = 0;
            if (exp_part[0] == '+' || exp_part[0] == '-') {
                exp_start = 1;
            }
            // 指数部分至少有一个数字
            if (exp_start >= exp_part.length()) {
                throw NativeFuncError("ParseError", "Exponent has sign but no digits");
            }
            // 指数部分只能包含数字
            for (size_t i = exp_start; i < exp_part.length(); ++i) {
                if (!std::isdigit(static_cast<unsigned char>(exp_part[i]))) {
                    throw NativeFuncError("ParseError", "Invalid character in exponent");
                }
            }
        } else {
            mantissa_part = str.substr(start);
        }

        // 检查尾数部分（小数点前后）
        if (mantissa_part.empty()) {
            throw NativeFuncError("ParseError", "No digits before exponent");
        }

        size_t dot_pos = mantissa_part.find('.');
        std::string int_part, frac_part;

        if (dot_pos != std::string::npos) {
            // 不允许出现多个小数点
            if (mantissa_part.find('.', dot_pos + 1) != std::string::npos) {
                throw NativeFuncError("ParseError", "Multiple decimal points");
            }
            int_part = mantissa_part.substr(0, dot_pos);
            frac_part = mantissa_part.substr(dot_pos + 1);

            // 整数部分可以为空（如 ".123"），但至少小数部分要有数字
            if (!int_part.empty()) {
                for (char c : int_part) {
                    if (!std::isdigit(static_cast<unsigned char>(c))) {
                        throw NativeFuncError("ParseError", "Invalid character in integer part");
                    }
                }
            }
            // 小数部分不能为空（否则小数点无意义）
            if (frac_part.empty()) {
                throw NativeFuncError("ParseError", "Decimal point without fractional digits");
            }
            for (char c : frac_part) {
                if (!std::isdigit(static_cast<unsigned char>(c))) {
                    throw NativeFuncError("ParseError", "Invalid character in fractional part");
                }
            }
        } else {
            // 没有小数点，整个部分必须是数字
            int_part = mantissa_part;
            for (char c : int_part) {
                if (!std::isdigit(static_cast<unsigned char>(c))) {
                    throw NativeFuncError("ParseError", "Invalid character in integer part");
                }
            }
            // 整数部分不能为空（已在前面检查 mantissa_part 非空）
        }

        // 解析指数（如果存在）
        int exp = 0;
        if (!exp_part.empty()) {
            try {
                exp = std::stoi(exp_part);
            } catch (const std::exception& e) {
                throw NativeFuncError("ParseError", "Invalid exponent format");
            }
        }

        // 解析出的指数量级校验（stoi 合法但巨大的值在此拦截）
        if (exp > 10000000 || exp < -10000000) {
            throw NativeFuncError("ParseError", "Exponent magnitude too large");
        }

        // 构造尾数和指数（沿用原有逻辑）
        if (dot_pos == std::string::npos) {
            // 无小数点：整数
            mantissa_ = BigInt(int_part);
            exponent_ = exp;
        } else {
            // 有小数点：拼接整数和小数部分
            std::string mant_str = (int_part.empty() ? "0" : int_part) + frac_part;
            mantissa_ = BigInt(mant_str);
            // 用64位计算避免int溢出UB
            long long exp_ll = static_cast<long long>(exp) - static_cast<long long>(frac_part.size());
            constexpr long long max_exp = 10000000;  // 指数量级上限，防OOM
            if (exp_ll > max_exp || exp_ll < -max_exp) {
                throw NativeFuncError("ParseError", "Exponent magnitude too large");
            }
            exponent_ = static_cast<int>(exp_ll);
        }

        // 应用符号
        if (is_neg) {
            mantissa_ = BigInt(0) - mantissa_;
        }

        normalize();
    }

    // 移动/拷贝构造（默认）
    Decimal(const Decimal& other) = default;
    Decimal(Decimal&& other) noexcept = default;
    Decimal& operator=(const Decimal& other) = default;
    Decimal& operator=(Decimal&& other) noexcept = default;
    ~Decimal() = default;

    // ========================= 基础方法 =========================
    /// 取绝对值
    [[nodiscard]] Decimal abs() const {
        Decimal res = *this;
        res.mantissa_ = res.mantissa_.abs();
        return res;
    }

    /// 取整数部分（截断小数部分）
    [[nodiscard]] BigInt integer_part() const {
        if (exponent_ >= 0) {
            // 指数非负：尾数 × 10^exponent
            return mantissa_ * BigInt::fast_pow_unsigned(BigInt(10), BigInt(exponent_));
        } else {
            // 指数为负：尾数 ÷ 10^(-exponent)
            BigInt ten_pow = BigInt::fast_pow_unsigned(BigInt(10), BigInt(-exponent_));
            return mantissa_ / ten_pow;
        }
    }

    /// 修复：to_string() 方法（小数点位置计算错误）
    [[nodiscard]] std::string to_string() const {
        if (mantissa_ == BigInt(0)) {
            return "0";
        }

        std::string mant_str = mantissa_.abs().to_string();
        bool is_neg = mantissa_.is_negative();
        int exp = exponent_;

        // 修复核心：正确计算小数点位置
        // 逻辑：数值 = mant_str × 10^exp → 等价于 mant_str 的小数点向左（exp负）/右（exp正）移|exp|位
        std::string res;
        if (exp >= 0) {
            // 指数非负：尾数后补exp个零（整数）
            res = mant_str + std::string(exp, '0');
        } else {
            // 指数为负：需要插入小数点
            int abs_exp = std::abs(exp);
            if (abs_exp >= static_cast<int>(mant_str.size())) {
                // 小数点在最前面，补零：0.000...mant_str
                res = "0." + std::string(abs_exp - mant_str.size(), '0') + mant_str;
            } else {
                // 小数点在中间：拆分整数和小数部分
                res = mant_str.substr(0, mant_str.size() - abs_exp) + "." + mant_str.substr(mant_str.size() - abs_exp);
            }
        }

        // 移除末尾的零和多余的小数点
        if (res.find('.') != std::string::npos) {
            res.erase(res.find_last_not_of('0') + 1, std::string::npos);
            if (res.back() == '.') {
                res.pop_back();
            }
        }

        // 应用符号
        if (is_neg) {
            res = "-" + res;
        }

        return res;
    }

    /// 哈希函数
    [[nodiscard]] BigInt hash() const {
        size_t mant_hash = std::hash<std::string>()(mantissa_.to_string());
        size_t exp_hash = std::hash<int>()(exponent_);

        BigInt mant_big(mant_hash);
        BigInt exp_big(exp_hash);
        BigInt shift_base(1);
        shift_base = shift_base.pow(BigInt(64));

        return mant_big * shift_base + exp_big;
    }

    // ========================= 比较运算符（逻辑正确，无需修改） =========================
    bool operator==(const Decimal& other) const {
        if (exponent_ != other.exponent_) {
            BigInt a_mant, b_mant;
            align_exponent(*this, other, a_mant, b_mant);
            return a_mant == b_mant;
        }
        return mantissa_ == other.mantissa_;
    }

    bool operator!=(const Decimal& other) const {
        return !(*this == other);
    }

    bool operator<(const Decimal& other) const {
        BigInt a_mant, b_mant;
        align_exponent(*this, other, a_mant, b_mant);
        return a_mant < b_mant;
    }

    bool operator>(const Decimal& other) const {
        return other < *this;
    }

    bool operator<=(const Decimal& other) const {
        return *this < other || *this == other;
    }

    bool operator>=(const Decimal& other) const {
        return *this > other || *this == other;
    }

    // ========================= 算术运算符（保持原有） =========================
    Decimal operator+(const Decimal& other) const {
        BigInt a_mant, b_mant;
        int exp = align_exponent(*this, other, a_mant, b_mant);
        BigInt sum_mant = a_mant + b_mant;
        Decimal res;  // 使用默认构造函数
        res.mantissa_ = sum_mant;
        res.exponent_ = exp;
        res.normalize();  // 只在最后 normalize 一次
        return res;
    }

    Decimal operator-(const Decimal& other) const {
        BigInt a_mant, b_mant;
        int exp = align_exponent(*this, other, a_mant, b_mant);
        BigInt sub_mant = a_mant - b_mant;
        Decimal res;
        res.mantissa_ = sub_mant;
        res.exponent_ = exp;
        res.normalize();
        return res;
    }

    // pow 方法也需要修复
    [[nodiscard]] Decimal pow(const BigInt& exp) const {
        if (exp.is_negative())
            throw NativeFuncError("CalculateError", "Decimal pow: negative exponent not supported");
        if (exp == BigInt(0)) {
            return Decimal(BigInt(1));
        }
        BigInt mant_pow = BigInt::fast_pow_unsigned(mantissa_.abs(), exp);
        if (mantissa_.is_negative() && (exp % BigInt(2) == BigInt(1))) {
            mant_pow = BigInt(0) - mant_pow;
        }
        Decimal res;
        res.mantissa_ = mant_pow;
        // 指数乘法用64位计算并检查范围，防止截断与溢出UB
        unsigned long long exp_ull = exp.to_unsigned_long_long();
        long long new_exp = static_cast<long long>(exponent_) * static_cast<long long>(exp_ull);
        constexpr long long max_exp = 10000000;  // 指数量级上限，防OOM
        if (new_exp > max_exp || new_exp < -max_exp) {
            throw NativeFuncError("CalculateError", "Decimal pow result exponent too large");
        }
        res.exponent_ = static_cast<int>(new_exp);
        res.normalize();
        return res;
    }

    // 在Decimal类中添加一个新的div_round方法
    [[nodiscard]] Decimal div_round(const Decimal& other, int n=10) const {
        if (other.mantissa_ == BigInt(0)) {
            throw NativeFuncError("CalculateError", "Division by zero");
        }
        if (n < 0) {
            throw NativeFuncError("CalculateError", "n must be non-negative");
        }

        // 先计算到n+1位小数
        Decimal temp = this->div(other, n + 1);

        // 将结果转换为字符串
        std::string str = temp.to_string();

        // 找到小数点
        size_t dot_pos = str.find('.');

        if (dot_pos == std::string::npos) {
            return temp;  // 没有小数部分
        }

        // 分离整数和小数部分
        std::string int_part = str.substr(0, dot_pos);
        std::string frac_part = str.substr(dot_pos + 1);

        // 确保小数部分至少有n+1位
        if (frac_part.size() < static_cast<size_t>(n + 1)) {
            frac_part.append(n + 1 - frac_part.size(), '0');
        }

        // 检查第n+1位
        if (frac_part[n] >= '5') {
            // 需要进位
            // 构建前n位小数
            std::string new_frac = (n > 0) ? frac_part.substr(0, n) : "";

            // 对前n位进行进位操作
            bool carry = true;
            for (int i = n - 1; i >= 0 && carry; --i) {
                if (new_frac[i] == '9') {
                    new_frac[i] = '0';
                } else {
                    new_frac[i] += 1;
                    carry = false;
                }
            }

            // 处理整数部分的进位
            BigInt int_val(int_part);
            if (carry) {
                if (int_part[0] == '-') {
                    int_val = int_val - BigInt(1);
                } else {
                    int_val = int_val + BigInt(1);
                }
                int_part = int_val.to_string();
            }

            // 构建结果字符串
            std::string result_str = int_part;
            if (n > 0 && new_frac != std::string(n, '0')) {
                result_str += "." + new_frac;
            }

            return Decimal(result_str);
        } else {
            // 直接截断到n位
            if (n == 0) {
                return Decimal(int_part);
            } else {
                std::string result_str = int_part + "." + frac_part.substr(0, n);
                return Decimal(result_str);
            }
        }
    }

    /**
     * @brief 判断两个Decimal的小数部分是否相等到指定精度
     * @param other 要比较的另一个Decimal
     * @param n 比较到小数点后第n位
     * @return 如果从小数点第1位到第n位都相等，返回true
     */
    [[nodiscard]] bool decimal_weekeq(const Decimal& other, int n) const {
        // 负精度无意义，直接报错（release下assert失效会引发巨型运算挂死）
        if (n < 0) {
            throw NativeFuncError("CalculateError", "decimal_weekeq: precision must be non-negative");
        }

        // 如果两个数完全相等，直接返回true
        if (*this == other) {
            return true;
        }

        // 获取两个数的整数部分
        BigInt this_int = this->integer_part();
        BigInt other_int = other.integer_part();

        // 整数部分必须相等
        if (this_int != other_int) {
            return false;
        }

        // 计算10^n用于缩放
        BigInt scale = BigInt::fast_pow_unsigned(BigInt(10), BigInt(n));

        // 分离小数部分
        Decimal this_fraction = *this - Decimal(this_int);
        Decimal other_fraction = other - Decimal(other_int);

        // 将小数部分放大10^n倍并取整
        BigInt this_scaled = (this_fraction * Decimal(scale)).integer_part();
        BigInt other_scaled = (other_fraction * Decimal(scale)).integer_part();

        return this_scaled == other_scaled;
    }

    Decimal operator*(const Decimal& other) const {
        BigInt mul_mant = mantissa_ * other.mantissa_;
        Decimal res(mul_mant, exponent_ + other.exponent_);
        res.normalize();
        return res;
    }

    Decimal operator/(const Decimal& other) const {
        return this->div(other, 10); // 默认保留10位小数
    }

    [[nodiscard]] Decimal div(const Decimal& other, int n=10) const { // n改为int类型
        if (other.mantissa_ == BigInt(0)) {
            // 除零必须抛可报告的错误，绝不能用停机信号代替（否则脚本静默退出）
            throw NativeFuncError("CalculateError", "decimal_div: division by zero");
        }
        if (n < 0)
            throw NativeFuncError("CalculateError", "n must be non-negative");

        BigInt a_mant, b_mant;
        // 对齐指数（抵消a/b的指数影响）
        align_exponent(*this, other, a_mant, b_mant);

        BigInt ten(10);
        // 补零扩展被除数：乘以 10^n，用于保留n位小数
        BigInt scale = BigInt::fast_pow_unsigned(ten, BigInt(n));
        BigInt dividend = a_mant * scale;

        // 整数除法（截断余数）
        BigInt quotient = dividend / b_mant;

        // 修复核心：先创建空对象，避免构造函数提前normalize
        Decimal res; // 空构造：mantissa_=0，exponent_=0
        res.mantissa_ = quotient; // 原始商（如5000000000），未被normalize
        res.exponent_ = -n; // 设置指数为 -n（如-10）
        res.normalize(); // 此时normalize会正确简化：5000000000 → 5，exponent_-10→-1

        return res;
    }

    /// 取模（floor语义，与Int.mod一致）：r = a - b*floor(a/b)
    [[nodiscard]] Decimal mod(const Decimal& other) const {
        if (other.mantissa_ == BigInt(0)) {
            throw NativeFuncError("CalculateError", "decimal_mod: division by zero");
        }
        if (mantissa_ == BigInt(0)) {
            return Decimal(BigInt(0));
        }

        BigInt a_mant, b_mant;
        int common_exp = align_exponent(*this, other, a_mant, b_mant);

        // 截断商与余数（BigInt为C截断语义）
        BigInt q = a_mant / b_mant;
        BigInt r = a_mant % b_mant;
        // floor调整：余数非零且商为负方向时，商再减一
        if (r != BigInt(0) && (a_mant.is_negative() != b_mant.is_negative())) {
            q -= BigInt(1);
        }

        // r = a - b*q（对齐后的尾数直接相减）
        Decimal res;
        res.mantissa_ = a_mant - b_mant * q;
        res.exponent_ = common_exp;
        res.normalize();
        return res;
    }

    // ========================= 算术运算符（Decimal与BigInt） =========================
    Decimal operator+(const BigInt& other) const {
        return *this + Decimal(other);
    }

    Decimal operator-(const BigInt& other) const {
        return *this - Decimal(other);
    }

    Decimal operator*(const BigInt& other) const {
        return *this * Decimal(other);
    }

    Decimal operator/(const BigInt& other) const {
        return *this / Decimal(other);
    }

    // ========================= 赋值运算符 =========================
    Decimal& operator+=(const Decimal& other) {
        *this = *this + other;
        return *this;
    }

    Decimal& operator-=(const Decimal& other) {
        *this = *this - other;
        return *this;
    }

    Decimal& operator*=(const Decimal& other) {
        *this = *this * other;
        return *this;
    }

    Decimal& operator/=(const Decimal& other) {
        *this = *this / other;
        return *this;
    }

    Decimal& operator+=(const BigInt& other) {
        return *this += Decimal(other);
    }

    Decimal& operator-=(const BigInt& other) {
        return *this -= Decimal(other);
    }

    Decimal& operator*=(const BigInt& other) {
        return *this *= Decimal(other);
    }

    Decimal& operator/=(const BigInt& other) {
        return *this /= Decimal(other);
    }
};

// ========================= 全局函数（BigInt与Decimal运算） =========================
inline Decimal operator+(const BigInt& lhs, const Decimal& rhs) {
    return rhs + lhs;
}

inline Decimal operator-(const BigInt& lhs, const Decimal& rhs) {
    return Decimal(lhs) - rhs;
}

inline Decimal operator*(const BigInt& lhs, const Decimal& rhs) {
    return rhs * lhs;
}

inline Decimal operator/(const BigInt& lhs, const Decimal& rhs) {
    return Decimal(lhs) / rhs;
}

} // namespace dep