/**
 * @file u8str.hpp
 * @brief UTF-8字符（UTF8Char）与UTF-8字符串（UTF8String）核心定义与实现
 * @author azhz1107cat
 * @date 2026-10-25
 */
#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>
#include <stdexcept>

namespace dep {

class UTF8Char {
    char buf_[4];
    int len_;

public:
    [[nodiscard]] int compare(const UTF8Char &other) const {
        return static_cast<int>(to_cod_point()) - static_cast<int>(other.to_cod_point());
    }

    UTF8Char() : len_(0) {
        memset(buf_, 0, 4);
    }

    UTF8Char(uint32_t code_point) : len_(0) {
        memset(buf_, 0, 4);
        if (code_point <= 0x7F) {
            len_ = 1;
            buf_[0] = static_cast<char>(code_point);
        } else if (code_point <= 0x7FF) {
            len_ = 2;
            buf_[0] = static_cast<char>(0xC0 | ((code_point >> 6) & 0x1F));
            buf_[1] = static_cast<char>(0x80 | (code_point & 0x3F));
        } else if (code_point <= 0xFFFF) {
            len_ = 3;
            buf_[0] = static_cast<char>(0xE0 | ((code_point >> 12) & 0x0F));
            buf_[1] = static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
            buf_[2] = static_cast<char>(0x80 | (code_point & 0x3F));
        } else if (code_point <= 0x10FFFF) {
            len_ = 4;
            buf_[0] = static_cast<char>(0xF0 | ((code_point >> 18) & 0x07));
            buf_[1] = static_cast<char>(0x80 | ((code_point >> 12) & 0x3F));
            buf_[2] = static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
            buf_[3] = static_cast<char>(0x80 | (code_point & 0x3F));
        } else {
            // Invalid code point, use replacement character
            *this = UTF8Char(static_cast<uint32_t>(0xFFFD));
        }
    }

    UTF8Char(const char *buf, int len) {
        // 钳制非法长度，防止len_记录越界值导致后续to_string栈越界读
        if (len < 0) len = 0;
        if (len > 4) len = 4;
        len_ = len;
        memset(buf_, 0, 4);
        if (buf && len > 0) {
            memcpy(buf_, buf, len);
        }
    }

    UTF8Char(char c) : len_(1) {
        memset(buf_, 0, 4);
        buf_[0] = c;
    }

    // Copy constructor
    UTF8Char(const UTF8Char &other) : len_(other.len_) {
        memcpy(buf_, other.buf_, 4);
    }

    // Assignment operator
    UTF8Char &operator=(const UTF8Char &other) {
        if (this != &other) {
            len_ = other.len_;
            memcpy(buf_, other.buf_, 4);
        }
        return *this;
    }

    [[nodiscard]] uint32_t to_cod_point() const {
        if (len_ == 0) return 0;

        unsigned char first_byte = static_cast<unsigned char>(buf_[0]);
        int cplen = 0;

        if ((first_byte & 0x80) == 0x00) cplen = 1;
        else if ((first_byte & 0xE0) == 0xC0) cplen = 2;
        else if ((first_byte & 0xF0) == 0xE0) cplen = 3;
        else if ((first_byte & 0xF8) == 0xF0) cplen = 4;
        else return 0xFFFD; // Invalid UTF-8, return replacement character

        if (cplen != len_) return 0xFFFD;

        // 校验续字节必须是10xxxxxx，非法序列统一回退替换字符
        for (int i = 1; i < cplen; ++i) {
            if ((static_cast<unsigned char>(buf_[i]) & 0xC0) != 0x80) {
                return 0xFFFD;
            }
        }

        uint32_t code_point = 0;
        if (cplen == 1) {
            code_point = first_byte;
        } else if (cplen == 2) {
            code_point = ((buf_[0] & 0x1F) << 6) | (buf_[1] & 0x3F);
        } else if (cplen == 3) {
            code_point = ((buf_[0] & 0x0F) << 12) | ((buf_[1] & 0x3F) << 6) | (buf_[2] & 0x3F);
        } else if (cplen == 4) {
            code_point = ((buf_[0] & 0x07) << 18) | ((buf_[1] & 0x3F) << 12) | ((buf_[2] & 0x3F) << 6) | (buf_[3] & 0x3F);
        }

        return code_point;
    }

    [[nodiscard]] std::string to_string() const {
        return std::string(buf_, len_);
    }

    [[nodiscard]] const char *data() const {
        return buf_;
    }

    [[nodiscard]] int bytesize() const {
        return len_;
    }

    [[nodiscard]] bool empty() const {
        return len_ == 0;
    }

    bool operator<(const UTF8Char &other) const { return compare(other) < 0; }
    bool operator>(const UTF8Char &other) const { return compare(other) > 0; }
    bool operator<=(const UTF8Char &other) const { return compare(other) <= 0; }
    bool operator>=(const UTF8Char &other) const { return compare(other) >= 0; }
    bool operator==(const UTF8Char &other) const { return compare(other) == 0; }
    bool operator!=(const UTF8Char &other) const { return compare(other) != 0; }

    // UTF8Char 类中的 is_alpha() 函数修改后版本
    [[nodiscard]] bool is_alpha() const {
        uint32_t cp = to_cod_point();

        // 英文字母 (基本拉丁字母)
        if ((cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z')) {
            return true;
        }

        // 中文字符范围 (CJK统一表意文字)
        //    基本汉字: 0x4E00 - 0x9FFF
        //    扩展A: 0x3400 - 0x4DBF
        //    扩展B: 0x20000 - 0x2A6DF (需要4字节UTF-8)
        //    扩展C: 0x2A700 - 0x2B73F
        //    扩展D: 0x2B740 - 0x2B81F
        //    扩展E: 0x2B820 - 0x2CEAF
        //    扩展F: 0x2CEB0 - 0x2EBEF
        if ((cp >= 0x4E00 && cp <= 0x9FFF) ||        // 基本汉字
            (cp >= 0x3400 && cp <= 0x4DBF) ||        // 扩展A
            (cp >= 0x20000 && cp <= 0x2A6DF) ||      // 扩展B
            (cp >= 0x2A700 && cp <= 0x2B73F) ||      // 扩展C
            (cp >= 0x2B740 && cp <= 0x2B81F) ||      // 扩展D
            (cp >= 0x2B820 && cp <= 0x2CEAF) ||      // 扩展E
            (cp >= 0x2CEB0 && cp <= 0x2EBEF)) {      // 扩展F
            return true;
            }

        // 日文假名
        //    平假名: 0x3040 - 0x309F
        //    片假名: 0x30A0 - 0x30FF
        //    片假名音标扩展: 0x31F0 - 0x31FF
        if ((cp >= 0x3040 && cp <= 0x309F) ||        // 平假名
            (cp >= 0x30A0 && cp <= 0x30FF) ||        // 片假名
            (cp >= 0x31F0 && cp <= 0x31FF)) {        // 片假名音标扩展
            return true;
            }

        // 韩文字母 (Hangul)
        //    韩文音节: 0xAC00 - 0xD7AF
        //    韩文字母: 0x1100 - 0x11FF
        //    韩文兼容字母: 0x3130 - 0x318F
        if ((cp >= 0xAC00 && cp <= 0xD7AF) ||        // 韩文音节
            (cp >= 0x1100 && cp <= 0x11FF) ||        // 韩文字母
            (cp >= 0x3130 && cp <= 0x318F)) {        // 韩文兼容字母
            return true;
            }

        // 其他常见字母系统
        //    拉丁字母补充 (带重音符号): 0x00C0 - 0x00FF
        //    希腊字母: 0x0370 - 0x03FF
        //    西里尔字母: 0x0400 - 0x04FF
        //    阿拉伯字母: 0x0600 - 0x06FF
        //    希伯来字母: 0x0590 - 0x05FF
        //    泰文字母: 0x0E00 - 0x0E7F
        //    梵文字母: 0x0900 - 0x097F
        if ((cp >= 0x00C0 && cp <= 0x00FF && cp != 0x00D7 && cp != 0x00F7) || // 拉丁字母补充，排除乘除号
            (cp >= 0x0370 && cp <= 0x03FF) ||        // 希腊字母
            (cp >= 0x0400 && cp <= 0x04FF) ||        // 西里尔字母
            (cp >= 0x0600 && cp <= 0x06FF) ||        // 阿拉伯字母
            (cp >= 0x0590 && cp <= 0x05FF) ||        // 希伯来字母
            (cp >= 0x0E00 && cp <= 0x0E7F) ||        // 泰文字母
            (cp >= 0x0900 && cp <= 0x097F)) {        // 梵文字母
            return true;
            }

        //全角字母 (全角英文字母)
        if ((cp >= 0xFF21 && cp <= 0xFF3A) ||        // 全角大写字母
            (cp >= 0xFF41 && cp <= 0xFF5A)) {        // 全角小写字母
            return true;
            }

        // 特殊字母字符
        // 字母数字符号 (如罗马数字): 0x2160 - 0x2188
        // 带圈字母数字: 0x2460 - 0x24FF
        if ((cp >= 0x2160 && cp <= 0x2188) ||        // 罗马数字等
            (cp >= 0x2460 && cp <= 0x24FF)) {        // 带圈字母数字
            return true;
            }

        return false;
    }

    [[nodiscard]] bool is_digit() const {
        uint32_t cp = to_cod_point();
        return cp >= '0' && cp <= '9';
    }

    [[nodiscard]] bool is_alnum() const {
        return is_alpha() || is_digit();
    }

    [[nodiscard]] bool is_space() const {
        uint32_t cp = to_cod_point();
        return cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' || cp == '\f' || cp == '\v';
    }

    [[nodiscard]] bool is_punct() const {
        uint32_t cp = to_cod_point();
        // Basic ASCII punctuation
        return (cp >= 33 && cp <= 47) || (cp >= 58 && cp <= 64) ||
               (cp >= 91 && cp <= 96) || (cp >= 123 && cp <= 126);
    }

    [[nodiscard]] bool is_symbol() const {
        uint32_t cp = to_cod_point();
        // Symbols are punctuation excluding quotes, parentheses, brackets, braces
        if (!is_punct()) return false;

        // Exclude specific characters
        static const char excluded[] = "\"#;()[]{}'`";
        for (char c : excluded) {
            if (cp == static_cast<uint32_t>(c)) return false;
        }
        return true;
    }

    [[nodiscard]] UTF8Char to_lower() const {
        uint32_t cp = to_cod_point();
        if (cp >= 'A' && cp <= 'Z') {
            return UTF8Char(cp + 32); // Convert to lowercase
        }
        return *this;
    }

    [[nodiscard]] UTF8Char to_upper() const {
        uint32_t cp = to_cod_point();
        if (cp >= 'a' && cp <= 'z') {
            return UTF8Char(cp - 32); // Convert to uppercase
        }
        return *this;
    }
};

class UTF8String {
    std::vector<UTF8Char> arr_;
    uint32_t actual_size_;

    ;[[nodiscard]] int compare(const UTF8String& other) const {
        size_t min_size = std::min(size(), other.size());
        for (size_t i = 0; i < min_size; i++) {
            int cmp = arr_[i].compare(other.arr_[i]);
            if (cmp != 0) return cmp;
        }
        return static_cast<int>(size()) - static_cast<int>(other.size());
    }

public:
    UTF8String() : actual_size_(0) {}

    UTF8String(const char* str) : actual_size_(0) {
        if (!str) return;
        *this = UTF8String(str, strlen(str));
    }

    // 带显式长度的构造：支持内嵌NUL字节的数据
    UTF8String(const char* str, size_t len) : actual_size_(0) {
        if (!str) return;

        for (size_t i = 0; i < len; ) {
            auto byte = static_cast<unsigned char>(str[i]);
            int cplen = 0;

            if ((byte & 0x80) == 0x00) cplen = 1;
            else if ((byte & 0xE0) == 0xC0) cplen = 2;
            else if ((byte & 0xF0) == 0xE0) cplen = 3;
            else if ((byte & 0xF8) == 0xF0) cplen = 4;
            else {
                // Invalid UTF-8, skip this byte
                cplen = 1;
            }

            if (i + cplen > len) {
                // Not enough bytes, treat as invalid
                cplen = 1;
            }

            arr_.emplace_back(str + i, cplen);
            actual_size_ += cplen;
            i += cplen;
        }
    }

    // 用data()+size()构造，保留内嵌NUL字节（c_str()会在NUL处截断丢数据）
    UTF8String(const std::string& str) : UTF8String(str.data(), str.size()) {}

    UTF8String(char c) : actual_size_(0) {
        if (c != '\0') {
            arr_.emplace_back(c);
            actual_size_ = 1;
        }
    }

    UTF8String(const UTF8Char& ch) : actual_size_(0) {
        if (!ch.empty()) {
            arr_.push_back(ch);
            actual_size_ = ch.bytesize();
        }
    }

    // Copy constructor
    UTF8String(const UTF8String& other) : arr_(other.arr_), actual_size_(other.actual_size_) {}

    // Move constructor
    UTF8String(UTF8String&& other) noexcept : arr_(std::move(other.arr_)), actual_size_(other.actual_size_) {
        other.actual_size_ = 0;
    }

    // Assignment operator
    UTF8String& operator=(const UTF8String& other) {
        if (this != &other) {
            arr_ = other.arr_;
            actual_size_ = other.actual_size_;
        }
        return *this;
    }

    // Move assignment operator
    UTF8String& operator=(UTF8String&& other) noexcept {
        if (this != &other) {
            arr_ = std::move(other.arr_);
            actual_size_ = other.actual_size_;
            other.actual_size_ = 0;
        }
        return *this;
    }

    // Comparison operators
    bool operator<(const UTF8String& other) const { return compare(other) < 0; }
    bool operator>(const UTF8String& other) const { return compare(other) > 0; }
    bool operator<=(const UTF8String& other) const { return compare(other) <= 0; }
    bool operator>=(const UTF8String& other) const { return compare(other) >= 0; }
    bool operator==(const UTF8String& other) const { return compare(other) == 0; }
    bool operator!=(const UTF8String& other) const { return compare(other) != 0; }

    // Concatenation
    UTF8String operator+(const UTF8String &other) const {
        UTF8String result = *this;
        result += other;
        return result;
    }

    UTF8String& operator+=(const UTF8String &other) {
        arr_.insert(arr_.end(), other.arr_.begin(), other.arr_.end());
        actual_size_ += other.actual_size_;
        return *this;
    }

    UTF8String& operator+=(const UTF8Char &ch) {
        arr_.push_back(ch);
        actual_size_ += ch.bytesize();
        return *this;
    }

    UTF8String& operator+=(char c) {
        return *this += UTF8Char(c);
    }

    // Indexing
    const UTF8Char& operator[](size_t index) const {
        if (index >= arr_.size()) {
            static UTF8Char null_char;
            return null_char;
        }
        return arr_[index];
    }

    UTF8Char& operator[](size_t index) {
        // 与const版本一致做边界检查，越界返回静态哑字符而不是堆越界写
        if (index >= arr_.size()) {
            static UTF8Char null_char;
            return null_char;
        }
        return arr_[index];
    }

    // Size methods
    [[nodiscard]] size_t size() const {
        return arr_.size();
    }

    [[nodiscard]] size_t bytesize() const {
        return actual_size_;
    }

    [[nodiscard]] bool empty() const {
        return arr_.empty();
    }

    // Content check
    [[nodiscard]] bool contains(const UTF8Char &element) const {
        for (const auto &ch : arr_) {
            if (ch == element) return true;
        }
        return false;
    }

    [[nodiscard]] bool contains(const UTF8String &substr) const {
        if (substr.empty()) return true;
        if (substr.size() > size()) return false;

        for (size_t i = 0; i <= size() - substr.size(); i++) {
            bool found = true;
            for (size_t j = 0; j < substr.size(); j++) {
                if (arr_[i + j] != substr[j]) {
                    found = false;
                    break;
                }
            }
            if (found) return true;
        }
        return false;
    }

    // String conversion
    [[nodiscard]] std::string to_string() const {
        std::string result;
        result.reserve(actual_size_);
        for (const auto &ch : arr_) {
            result.append(ch.data(), ch.bytesize());
        }
        return result;
    }

    [[nodiscard]] const char* c_str() const {
        // Note: This requires storing the string contiguously
        static thread_local std::string temp;
        temp = to_string();
        return temp.c_str();
    }

    // Substring
    [[nodiscard]] UTF8String substr(size_t pos, size_t len = std::string::npos) const {
        if (pos >= size()) return {};

        UTF8String result;
        size_t end = (len == std::string::npos) ? size() : std::min(pos + len, size());

        for (size_t i = pos; i < end; i++) {
            result += arr_[i];
        }
        return result;
    }

    // Case conversion
    [[nodiscard]] UTF8String to_lower() const {
        UTF8String result;
        for (const auto &ch : arr_) {
            result += ch.to_lower();
        }
        return result;
    }

    [[nodiscard]] UTF8String to_upper() const {
        UTF8String result;
        for (const auto &ch : arr_) {
            result += ch.to_upper();
        }
        return result;
    }

    // Trimming
    [[nodiscard]] UTF8String trim() const {
        if (empty()) return *this;

        size_t start = 0;
        while (start < size() && arr_[start].is_space()) {
            start++;
        }

        if (start == size()) return {};

        size_t end = size() - 1;
        while (end > start && arr_[end].is_space()) {
            end--;
        }

        return substr(start, end - start + 1);
    }

    // Iterators
    auto begin() { return arr_.begin(); }
    auto end() { return arr_.end(); }
    auto begin() const { return arr_.begin(); }
    auto end() const { return arr_.end(); }
    auto cbegin() const { return arr_.cbegin(); }
    auto cend() const { return arr_.cend(); }
    auto rbegin() { return arr_.rbegin(); }
    auto rend() { return arr_.rend(); }
    auto rbegin() const { return arr_.rbegin(); }
    auto rend() const { return arr_.rend(); }
};



// Stream operators
inline std::ostream &operator<<(std::ostream &os, const UTF8String &str) {
    return os << str.to_string();
}

inline std::istream &operator>>(std::istream &is, UTF8String &str) {
    std::string temp;
    is >> temp;
    str = UTF8String(temp);
    return is;
}

}