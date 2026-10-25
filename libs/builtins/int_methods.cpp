/**
 * @file int_methods.cpp
 * @brief Int类型的魔术方法实现
 * @author azhz1107cat
 * @date 2026-10-25
 */
#include "../../depends/u8str.hpp"
#include "../../src/models/models.hpp"
#include "../../src/vm/vm.hpp"
#include "include/builtin_functions.hpp"

namespace model {

// Int.__call__
Object* int_call(Object* self, const List* args) {
    auto a = builtin::get_one_arg(args);
    dep::BigInt val(0);
    if (auto s = dynamic_cast<String*>(a)) {
        // 允许一个前导负号的整数字符串
        const std::string& text = s->val;
        size_t start = (text.size() > 0 && text[0] == '-') ? 1 : 0;
        bool is_digit = text.size() > start; // 至少要有一个数字
        for (size_t i = start; i < text.size(); ++i) {
            if (text[i] < '0' || text[i] > '9') is_digit = false;
        }
        if (is_digit) {
            val = dep::BigInt(text);
        } else {
            throw NativeFuncError("TypeError", "Cannot cast this string to Int");
        }
    }
    else if (auto i = dynamic_cast<Int*>(a)) {
        val = i->val;
    }
    else if (auto d = dynamic_cast<Decimal*>(a)) {
        val = d->val.integer_part();
    }
    else if (auto b = dynamic_cast<Bool*>(a)) {
        // True→1 / False→0，不能静默落回0
        val = dep::BigInt(b->val ? 1 : 0);
    }
    else {
        throw NativeFuncError("TypeError", "Cannot cast this type to Int");
    }
    return new Int(val);
}

// Int.__bool__
Object* int_bool(Object* self, const List* args) {
    const auto self_int = dynamic_cast<Int*>(self);
    if (self_int->val == dep::BigInt(0)) {
        return load_false();
    }
    return load_true();
}

// Int.__add__ 整数加法：self + args[0]（仅支持Int/Decimal）
Object* int_add(Object* self, const List* args) {
    kiz::Vm::assert_argc(1, args);

    const auto self_int = dynamic_cast<Int*>(self);
    assert(self_int!=nullptr);

    // 与Int相加
    auto another_int = dynamic_cast<Int*>(args->val[0]);
    if (another_int) {
        return new Int(self_int->val + another_int->val);
    }
    // 与Decimal相加（返回Decimal）
    auto another_dec = dynamic_cast<Decimal*>(args->val[0]);
    if (another_dec) {
        dep::Decimal left_dec(self_int->val);
        return new Decimal(left_dec + another_dec->val);
    }
    // 仅允许Int/Decimal
    throw NativeFuncError("TypeError", "function Int.add second arg need be Int or Decimal");
};

// Int.__sub__ 整数减法：self - args[0]（仅支持Int/Decimal）
Object* int_sub(Object* self, const List* args) {
    kiz::Vm::assert_argc(1, args);

    auto self_int = dynamic_cast<Int*>(self);
    // 与Int相减
    auto another_int = dynamic_cast<Int*>(args->val[0]);
    if (another_int) {
        return new Int(self_int->val - another_int->val);
    }
    // 与Decimal相减（返回Decimal）
    auto another_dec = dynamic_cast<Decimal*>(args->val[0]);
    if (another_dec) {
        dep::Decimal left_dec(self_int->val);
        return new Decimal(left_dec - another_dec->val);
    }
    // 仅允许Int/Decimal
    throw NativeFuncError("TypeError", "function Int.sub second arg need be Int or Decimal");
};

// Int.__mul__ 整数乘法：self * args[0]（仅支持Int/Decimal）
Object* int_mul(Object* self, const List* args) {
    kiz::Vm::assert_argc(1, args);

    auto self_int = dynamic_cast<Int*>(self);
    // 与Int相乘
    auto another_int = dynamic_cast<Int*>(args->val[0]);
    if (another_int) {
        return new Int(self_int->val * another_int->val);
    }
    // 与Decimal相乘（返回Decimal）
    auto another_dec = dynamic_cast<Decimal*>(args->val[0]);
    if (another_dec) {
        dep::Decimal left_dec(self_int->val);
        return new Decimal(left_dec * another_dec->val);
    }
    // 仅允许Int/Decimal
    throw NativeFuncError("TypeError", "function Int.mul second arg need be Int or Decimal");
};

// Int.__neg__ 取反
Object* int_neg(Object* self, const List* args) {
    auto self_int = dynamic_cast<Int*>(self);
    assert(self_int!=nullptr);

    auto new_int = dep::BigInt(0) - self_int->val;
    return new Int(new_int);
}

// Int.__div__ 整数除法 self / args[0]（仅支持Int/Decimal，返回Decimal）
Object* int_div(Object* self, const List* args) {
    kiz::Vm::assert_argc(1, args);

    auto self_int = dynamic_cast<Int*>(self);
    assert(self_int!=nullptr);
    // 与Int相除（返回Decimal，保留10位小数）
    auto another_int = dynamic_cast<Int*>(args->val[0]);
    if (another_int) {
        if (another_int->val == 0) throw NativeFuncError("CalculateError", "divisor cannot be zero");
        dep::Decimal left_dec(self_int->val);
        dep::Decimal right_dec(another_int->val);
        return new Decimal(left_dec.div(right_dec, 10));
    }
    // 与Decimal相除（返回Decimal）
    auto another_dec = dynamic_cast<Decimal*>(args->val[0]);
    if (another_dec) {
        if(another_dec->val == dep::Decimal(0)) throw NativeFuncError("CalculateError", "divisor cannot be zero");
        dep::Decimal left_dec(self_int->val);
        return new Decimal(left_dec.div(another_dec->val, 10));
    }
    // 仅允许Int/Decimal
    throw NativeFuncError("TypeError", "function Int.div second arg need be Int or Decimal");
};

// Int.__pow__ 整数幂运算：self ^ args[0]（self的args[0]次方，仅支持Int指数）
Object* int_pow(Object* self, const List* args) {
    kiz::Vm::assert_argc(1, args);

    auto self_int = dynamic_cast<Int*>(self);
    auto exp_int = dynamic_cast<Int*>(args->val[0]);
    if (! exp_int)
        throw NativeFuncError("TypeError", "function Int.pow second arg need be Int");

    // 结果规模上限：估算结果十进制位数，防止一行脚本算出数亿位大数耗尽内存
    dep::BigInt base_abs = self_int->val.abs();
    if (base_abs > dep::BigInt(1) and !exp_int->val.is_negative()) {
        dep::BigInt est_digits = dep::BigInt(base_abs.to_string().size()) * exp_int->val;
        if (est_digits > dep::BigInt(10000000)) {
            throw NativeFuncError("CalculateError", "Int.pow: result too large");
        }
    }

    // 指数非负时返回Int，负指数返回Decimal（扩展支持）
    if (exp_int->val.is_negative()) {
        dep::Decimal base_dec(self_int->val);
        dep::BigInt abs_exp = exp_int->val.abs();
        dep::Decimal res = base_dec.pow(abs_exp);
        // 负指数：1 / res
        dep::Decimal one(1);
        return new Decimal(one.div(res, 10));
    } else {
        return new Int(self_int->val.pow(exp_int->val));
    }
};

// Int.__mod__ 整数取模：self % args[0]
Object* int_mod(Object* self, const List* args) {
    kiz::Vm::assert_argc(1, args);

    auto another_int = dynamic_cast<Int*>(args->val[0]);
    // 与Decimal混合：转Decimal取模（与Int.__add__的跨类型行为一致）
    if (!another_int) {
        if (auto another_dec = dynamic_cast<Decimal*>(args->val[0])) {
            const auto self_int_mixed = dynamic_cast<Int*>(self);
            dep::Decimal left_dec(self_int_mixed->val);
            return new Decimal(left_dec.mod(another_dec->val));
        }
        throw NativeFuncError("TypeError", "function Int.mod second arg need be Int or Decimal");
    }

    if(another_int->val == dep::BigInt(0))
        throw NativeFuncError("CalculateError", "mod by zero");

    auto self_int = dynamic_cast<Int*>(self);
    dep::BigInt remainder = self_int->val % another_int->val;
    // 修正余数符号（确保与除数同号）
    if (remainder != dep::BigInt(0)
        and self_int->val < dep::BigInt(0) != another_int->val < dep::BigInt(0)
    ) {
        remainder += another_int->val;
    }
    return new Int(dep::BigInt(remainder));
};

// Int.__eq__ 相等判断：self == args[0]（仅支持Int/Decimal）
Object* int_eq(Object* self, const List* args) {
    kiz::Vm::assert_argc(1, args);

    auto self_int = dynamic_cast<Int*>(self);
    // 与Int比较
    auto another_int = dynamic_cast<Int*>(args->val[0]);
    if (another_int) {
        return load_bool(self_int->val == another_int->val);
    }
    // 与Decimal比较
    auto another_dec = dynamic_cast<Decimal*>(args->val[0]);
    if (another_dec) {
        dep::Decimal cmp_val(self_int->val);
        return load_bool(cmp_val == another_dec->val);
    }
    // 仅允许Int/Decimal
    throw NativeFuncError("TypeError", "function Int.eq second arg need be Int or Decimal");
};

// Int.__lt__ 小于判断：self < args[0]（仅支持Int/Decimal）
Object* int_lt(Object* self, const List* args) {
    kiz::Vm::assert_argc(1, args);

    auto self_int = dynamic_cast<Int*>(self);
    // 与Int比较
    auto another_int = dynamic_cast<Int*>(args->val[0]);
    if (another_int) {
        return load_bool(self_int->val < another_int->val);
    }
    // 与Decimal比较
    auto another_dec = dynamic_cast<Decimal*>(args->val[0]);
    if (another_dec) {
        dep::Decimal cmp_val(self_int->val);
        return load_bool(cmp_val < another_dec->val);
    }
    // 仅允许Int/Decimal
    throw NativeFuncError("TypeError", "function Int.lt second arg need be Int or Decimal");
};

// Int.__gt__ 大于判断：self > args[0]（仅支持Int/Decimal）
Object* int_gt(Object* self, const List* args) {
    kiz::Vm::assert_argc(1, args);

    auto self_int = dynamic_cast<Int*>(self);
    // 与Int比较
    auto another_int = dynamic_cast<Int*>(args->val[0]);
    if (another_int) {
        return load_bool(self_int->val > another_int->val);
    }
    // 与Decimal比较
    auto another_dec = dynamic_cast<Decimal*>(args->val[0]);
    if (another_dec) {
        dep::Decimal cmp_val(self_int->val);
        return load_bool(cmp_val > another_dec->val);
    }
    // 仅允许Int/Decimal
    throw NativeFuncError("TypeError", "function Int.gt second arg need be Int or Decimal");
};

// Int.__hash__
Object* int_hash(Object* self, const List* args) {
    auto self_int = dynamic_cast<Int*>(self);
    // 哈希带类型前缀：防止Int 1与Bool true/Str "1"等不同类型哈希撞车导致字典键混同
    return new Int(dep::BigInt(dep::hash_string("Int:" + self_int->val.to_string())));
}

Object* int_str(Object* self, const List* args) {
    auto self_int = dynamic_cast<Int*>(self);
    return new String(self_int->val.to_string());
}


}  // namespace model