# kiz示例代码与讲解

本文通过一组由浅入深的可运行示例讲解 kiz 的核心特性。所有代码都可以保存为 `.kiz` 文件后用 `kiz run 文件名` 运行（`examples/` 目录下有更多现成示例）。

- [1. Hello World 与基本输出](#1-hello-world-与基本输出)
- [2. 变量、类型与精确小数](#2-变量类型与精确小数)
- [3. 控制流](#3-控制流)
- [4. 函数与闭包](#4-函数与闭包)
- [5. 列表与字典](#5-列表与字典)
- [6. 面向对象（原型链）](#6-面向对象原型链)
- [7. 运算符重载](#7-运算符重载)
- [8. 错误处理](#8-错误处理)
- [9. 模块](#9-模块)
- [10. 文件读写](#10-文件读写)
- [11. 综合练习：迭代器协议](#11-综合练习迭代器协议)

---

## 1. Hello World 与基本输出

```
print("Hello, kiz!")

name = "kiz"
version = 0.9
print(f"Hello, {name} v{version}!")

# 输出：
# Hello, kiz!
# Hello, kiz v0.9!
```

**讲解**：`print` 接受任意多个参数，以空格分隔打印并换行。f-string 用 `f"..."` 开头，`{表达式}` 插值，任意合法表达式都可以放进去。

## 2. 变量、类型与精确小数

```
a = 42                      # Int，无限精度
b = 10 ^ 40                 # 幂运算用 ^，照样无限精度
c = 0.1 + 0.2               # Decimal 精确小数
print(c == 0.3)             # True！没有浮点误差

print(1 / 3)                # 0.3333333333（除法产生Decimal）
print(Int(7 / 2))           # 3（要整数结果就转Int）
print(-7 % 3)               # 2（取模符号跟除数，同Python）

s = "中文ok"
print(len(s))               # 4（按UTF-8码点计数）
print(Int("123"), Str(88))  # 123 88
```

**讲解**：kiz 的整数没有大小上限；小数是十进制精确表示，`0.1 + 0.2` 严格等于 `0.3`。`/` 的结果可能是 Decimal，需要整型时显式 `Int()`。取模语义与 Python 一致。

## 3. 控制流

```
score = 77

if score >= 90
    print("优秀")
else if score >= 60
    print("及格")
else
    print("不及格")
end

# for 遍历列表
total = 0
for x in [10, 20, 30]
    total = total + x
end
print(total)                # 60

# while + next/break
i = 0
while i < 10
    i = i + 1
    if i % 2 == 0
        next                # next 相当于 continue
    end
    if i > 7
        break
    end
    print(i)                # 1 3 5 7
end
```

**讲解**：块以 `end` 结束；`else if` 不是 `elif`；`next` 是 continue。`for x in ...` 遍历任何实现了 `__next__` 的对象（列表、字符串、字典、range）。

## 4. 函数与闭包

```
fn fib(n)
    if n <= 1
        return n
    end
    return fib(n-1) + fib(n-2)
end
print(fib(10))              # 55

# 短lambda与多行匿名函数都是表达式
apply = |f, x| f(x)
print(apply(|x| x * 3, 5))  # 15

# 闭包：赋值自动写回捕获的变量（也可显式写 nonlocal n = n + 1）
make_counter = fn
    n = 0
    return fn
        n = n + 1
        return n
    end
end
c1 = make_counter()
c2 = make_counter()
print(c1(), c1(), c2())     # 1 2 1（各自独立）

# rest 参数
fn sum_all(first, ...rest)
    total = first
    for x in rest
        total = total + x
    end
    return total
end
print(sum_all(1, 2, 3, 4))  # 10
```

**讲解**：函数是一等对象。闭包捕获的是变量的**单元格（Cell）**：同一工厂函数产生的多个计数器各自独立（每次调用创建新绑定），而同一作用域创建的多个闭包**共享**同一绑定——`nonlocal n = ...` 与直接 `n = n + 1` 等效（赋值自动写回捕获的变量，Scheme/Lua 语义）。for 循环变量每轮是独立绑定，while 循环变量整轮共享。

## 5. 列表与字典

```
xs = [3, 1, 2]
xs.append(4)
print(xs.contains(2))       # True
print(xs.pop())             # 4（弹出末尾）
xs.reverse()
print(xs)                   # [2, 1, 3]

ys = [1, 2, 3]
print(ys.map(|x| x * x))            # [1, 4, 9]
print(ys.filter(|x| x > 1))         # [2, 3]
print(ys.find(|x| x % 2 == 0))      # 2
print(ys.count(2))                  # 1
print(["a", "b"].join("-"))         # a-b
print([0] * 3)                      # [0, 0, 0]

d = {"alice": 90, "bob": 60}
d["carol"] = 75
print(d.contains("bob"))    # True
print(d["alice"])           # 90
d.foreach(|k, v|
    print(k, v)
)
merged = {"a": 1} + {"b": 2}
print(merged)               # {a: 1, b: 2}
```

**讲解**：列表/字典方法链式可用。注意 kiz 是**值语义**：`b = a` 之后修改 `b` 不影响 `a`。字典键可以是任意可哈希类型（Str/Int/Decimal/Bool/Nil，不同类型不会互相混同）。

## 6. 面向对象（原型链）

```
object Animal
    name = ""
    fn init(this, name)
        this.name = name
        return this
    end
    fn speak(this)
        return this.name + " makes a sound"
    end
end

object Dog: Animal                  # Dog 的原型是 Animal
    fn speak(this)
        return this.name + " barks"
    end
end

a = create(Animal).init("Generic")
d = create(Dog).init("Rex")
print(a.speak())                    # Generic makes a sound
print(d.speak())                    # Rex barks（子对象覆盖父方法）
print(ischild(d, Animal))           # True（沿原型链判断）

# 方法内调用父实现：先取出父对象上的函数，再用变量调用（避免自动self绑定）
object Dog2: Animal
    fn speak(this)
        animal_speak = Animal.speak
        return animal_speak(this) + " (woof)"
    end
end
d2 = create(Dog2).init("Ace")
print(d2.speak())                    # Ace makes a sound (woof)
```

**讲解**：`object Name: Parent` 声明对象并设置原型（`__parent__`）。`create(对象)` 生成以该对象为原型的实例。方法查找沿原型链进行；子对象同名方法覆盖父方法。**调用父方法**的惯用法是先取出父对象上的函数再用变量调用：`f = Animal.speak` 然后 `f(实例)`——直接写 `this.__parent__.speak(...)` 会经过方法调用协议自动绑定 self（多传一个参数报 ArgCountError），而且方法内的 `this.__parent__` 指向的是实例的直接原型（这里是 Dog2 自己），容易写成自递归。

## 7. 运算符重载

```
object Vec
    fn init(this, x, y)
        this.x = x
        this.y = y
        return this
    end
    fn __add__(this, other)
        v = create(Vec)
        return v.init(this.x + other.x, this.y + other.y)
    end
    fn __str__(this)
        return f"({this.x}, {this.y})"
    end
end

v1 = create(Vec).init(1, 2)
v2 = create(Vec).init(3, 4)
print(v1 + v2)              # (4, 6)
```

**讲解**：实现 `__add__`/`__sub__`/`__mul__`/`__eq__`/`__lt__` 等魔术方法即可重载对应运算符；`__str__` 决定 `print`/`Str()` 的输出。魔术方法可以定义在 `object` 块里，也可以直接挂在实例上。

## 8. 错误处理

```
fn divide(a, b)
    if b == 0
        throw Error("DivideByZero", "除数不能为零")
    end
    return a / b
end

try
    print(divide(10, 2))    # 5
    print(divide(1, 0))
catch e (DivideByZero)
    print("捕获:", e)        # 捕获: Error(name=DivideByZero, msg=除数不能为零)
catch e (OtherError)
    print("不会到这里")
end

# 重抛
try
    try
        throw Error("Inner", "内部错误")
    catch e (Inner)
        print("内层处理，转外抛")
        throw e
    end
catch e2 (Inner)
    print("外层捕获:", e2.__msg__)
end
```

**讲解**：`catch e (名字)` 按错误名精确匹配，不匹配的继续向外传播；`throw e` 重抛当前错误对象。错误对象带 `__name__` 和 `__msg__` 属性。

## 9. 模块

```
# 文件 mymath.kiz
fn add(a, b)
    return a + b
end
PI = 3.14159
__secret = "带__private__前缀不导出"    # 惯例：双下划线前缀表示私有
```

```
# 文件 main.kiz
import mymath at "mymath.kiz"
print(mymath.add(1, 2))     # 3
print(mymath.PI)            # 3.14159

import os                    # 标准库
print(type(os))             # Module
```

**讲解**：`import 名字 at "路径"` 导入相对当前文件的模块，顶层变量成为模块属性。无 `at` 时依次查找标准库表、当前文件目录、可执行文件目录、`modules/` 目录。循环导入会报 `ImportError` 而不是崩溃。

## 10. 文件读写

```
w = open("demo.txt", "w")
w.write("第一行\n")
w.write("第二行\n")
w.close()

r = open("demo.txt", "r")
print(r.read())
r.close()
```

**讲解**：`open(路径, 模式)`，模式支持 `r w a r+ w+ a+`。相对路径以**当前脚本所在目录**为基准。`read()` 读全文，`readline(n)` 读第 n 行，`write` 会自动转换对象为字符串。用完记得 `close()`。

## 11. 综合练习：迭代器协议

任何实现了 `__next__` 的对象都能被 `for` 遍历。`__next__` 每次返回下一个元素，耗尽时返回 `__StopIterSignal__`：

```
object Countdown
    fn init(this, from)
        this.cur = from
        return this
    end
    fn __next__(this)
        if this.cur <= 0
            return __StopIterSignal__
        end
        this.cur = this.cur - 1
        return this.cur + 1
    end
end

cd = create(Countdown).init(3)
for n in cd
    print(n)                # 3 2 1
end

# 解包也走迭代协议
print([...cd])              # []（已耗尽）
fresh = create(Countdown).init(3)
print([...fresh])           # [3, 2, 1]
```

**讲解**：迭代协议是 `__next__` + 停止信号。列表/字符串/字典/range 都内置实现。`...x` 解包语法把可迭代对象展开进列表字面量。这个例子综合了原型链、方法、魔术方法与控制流。

---

配套阅读：[语法特性速览](语法特性速览.md)、[内置对象&库速览](内置对象&库速览.md)、[从Python过渡到Kiz](从Python过渡到Kiz.md)。

---
author: *azhz1107cat*
