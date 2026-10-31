# 从 Python 过渡到 Kiz

本文面向有 Python 基础的开发者，帮助你快速上手 kiz。重点讲**相同之处**与**必须注意的差异**。

- [心智模型](#心智模型)
- [语法速查表](#语法速查表)
- [分主题对照](#分主题对照)
    - [变量与基本类型](#变量与基本类型)
    - [容器](#容器)
    - [控制流](#控制流)
    - [函数与闭包](#函数与闭包)
    - [面向对象](#面向对象)
    - [错误处理](#错误处理)
    - [模块](#模块)
    - [输入输出](#输入输出)
- [语义差异陷阱清单](#语义差异陷阱清单)

---

## 心智模型

kiz 与 Python 的相似点：动态类型、鸭子类型、一切皆对象、f-string、try/catch 异常、大量内置方法命名接近。

开始前先记住三个核心差异：

1. **赋值是值语义**：`b = a` 会深拷贝可变对象（List/Dict/Object），之后两者互不影响。Python 是引用语义。
2. **块用 `end` 结束，不用缩进**：缩进只是习惯，`if`/`while`/`for`/`fn`/`object`/`try` 都以 `end` 收尾。
3. **强类型**：跨类型运算直接报错，没有隐式转换。`1 + "1"`、`1 == True` 都是 TypeError。

## 语法速查表

| Python | kiz | 说明 |
|---|---|---|
| `x = 1` | `x = 1` | 相同，无需关键字 |
| `None` | `Nil` | |
| `True` / `False` | `True` / `False` | |
| `if/elif/else` | `if ... / else if ... / else ... / end` | 无冒号，无 elif 关键字 |
| `while ...:` | `while ... ... end` | |
| `for x in xs:` | `for x in xs ... end` | |
| `continue` | `next` | **关键字不同** |
| `break` | `break` | |
| `def f(a, b):` | `fn f(a, b) ... end` | |
| `lambda a: a+1` | `\|a\| a+1` | 短 lambda |
| `def f(a, *rest)` | `fn f(a, ...rest)` | rest 参数 |
| `class Dog(Dad):` | `object Dog: Dad ... end` | 原型链而非类 |
| `try/except/finally` | `try ... catch e (Name) ... end` | 无 finally |
| `raise ValueError("x")` | `throw Error("ValueError", "x")` | |
| `import os` | `import os` | 标准库同名 |
| `from x import y` | `import x at "路径"` 后用 `x.y` | 无 from 形式 |
| `d = {"a": 1}` | `d = {"a": 1}` | 冒号相同 |
| `f"{x}"` | `f"{x}"` | 相同 |
| `x ** 2` | `x ^ 2` | 幂运算符不同 |
| `a // b` | 无整除运算符 | 用 `Int(a / b)` |
| `a, b = b, a` | 不支持 | 用临时变量 |

## 分主题对照

### 变量与基本类型

```
x = 42                # 无限精度整数，没有溢出这回事
y = 3.14              # 精确十进制 Decimal（不是二进制浮点！）
s = "你好"             # UTF-8 字符串，支持中文标识符
b = True
n = Nil
big = 10 ^ 100        # 幂运算符是 ^
```

要点：

- **小数是 Decimal**：`0.1 + 0.2 == 0.3` 为 True，没有浮点误差。
- **除法**：`1 / 3` 得 `0.3333333333`（Decimal，默认 10 位精度）。要整型结果用 `Int(7 / 2)` 得 `3`。
- **取模与 Python 一致**（结果符号跟除数）：`-7 % 3` 是 `2`，`7 % -3` 是 `-2`。
- 转换：`Int("42")`、`Int("-5")`、`Int(2.9)`（截断为 2）、`Decimal("1.5e3")`、`Str(9)` 得 `"9"`。非法输入抛 TypeError，不会静默得 0。
- 转义：`\n` `\t` `\\` `\"` `\u4e2d`（Unicode 码点）。

### 容器

```
xs = [1, 2, 3]
d = {"a": 1, "b": [2, 3]}     # 任意可哈希类型可作键

xs.append(4)
xs.contains(3)                # 相当于 Python 的 3 in xs
xs.pop()
xs.reverse()
xs.extend([5, 6])
xs2 = xs.map(|x| x * 2)
xs3 = xs.filter(|x| x > 1)
d.contains("a")               # 相当于 "a" in d
len(xs)                       # len 对 List/Str/Dict 通用
```

**最大差异：值语义**

```
a = [1, 2]
b = a          # Python：b 与 a 是同一个列表；kiz：深拷贝
b[0] = 99
print(a)       # Python 打印 [99, 2]；kiz 打印 [1, 2]
```

传参同理，函数内修改列表参数不影响调用方。需要共享可变状态时，把数据放进 Object、通过方法修改（方法内经 `this` 访问真实对象）：

```
counter = create()
counter.n = 0
counter.incr = fn(this)
    this.n = this.n + 1
end
counter.incr()
counter.incr()
print(counter.n)         # 2
```

### 控制流

```
if x > 3
    print("big")
else if x > 1            # 不是 elif，是 else if
    print("mid")
else
    print("small")
end

while i < 10
    if i == 3
        next             # 相当于 continue
    end
    if i == 8
        break
    end
    i = i + 1
end

for item in [10, 20, 30]
    print(item)
end

for i in range(5)        # [0,1,2,3,4]
    print(i)
end

for i in range(5, -1, 2) # 负步长向下计数：5,4,3
    print(i)
end
```

### 函数与闭包

```
fn add(a, b)
    return a + b
end

fn greet(name, ...rest)      # rest 参数收集多余实参为 List
    return name + Str(rest)
end

double = |x| x * 2           # 短 lambda（单表达式）

make_adder = fn(n)           # 多行匿名函数也是表达式
    return |x| x + n         # 闭包捕获
end
add5 = make_adder(5)
print(add5(1))               # 6
```

与 Python 的差异：

- **方法第一个形参是 self**：任何"通过对象调用"的函数，第一个形参接收调用者对象，惯用名 `this`。
- **闭包共享捕获变量的单元格(Cell)**——同一作用域创建的闭包共享可变状态："”
  ```
  fns = []
  i = 0
  while i < 3
      fn mk
          return i
      end
      fns.append(mk)
      i = i + 1
  end
  print(fns[0]())    # 0（for 循环变量每轮独立，Python 里同一写法常得 3）
  print(fns[2]())    # 20
  ```
- **赋值写共享单元格**：闭包内对捕获变量的赋值自动写回外层（Scheme/Lua 语义），`nonlocal` 关键字可省略（保留兼容）：
  ```
  fn make_counter
      n = 0
      fn inc
          n = n + 1       # 直接写，等效 nonlocal n = n + 1
          return n
      end
      fn get
          return n        # 与 inc 共享同一个 n
      end
      return [inc, get]
  end
  c = make_counter()
  c[0]()                  # 1
  print(c[1]())           # 1（get 看到了 inc 的写入）
  ```
- **while 循环变量被捕获时共享**（整个循环一个绑定，Lua 风格）；**for 循环变量每轮独立**。若要在 while 中逐轮独立，把值拷进循环体内的一个新名字。
- `global x = 1` / `nonlocal x = 1` 用于跨作用域赋值声明（显式形式，可省略）。

### 面向对象

kiz 是**原型链**模型，没有类：

```
object Dog
    name = "default"
    fn init(this, name)        # 惯用构造方法（名字任意）
        this.name = name
        return this
    end
    fn bark(this)
        print(this.name + " is barking")
    end
end

dog = create(Dog).init("Tim")
dog.bark()

object Puppy: Dog              # 继承
    fn bark(this)
        print("puppy!")
        this.__parent__.bark() # 显式调用父方法
    end
end
```

也可以完全手工（kiz 中一切皆是对象）：

```
Point = create()
Point.__call__ = fn(this, x, y)    # 定义 __call__ 后对象可像函数一样调用
    o = create(this)
    o.x = x
    o.y = y
    return o
end
p = Point(3, 5)
```

常用魔术方法：`__call__` `__str__` `__eq__` `__lt__` `__gt__` `__add__` `__mul__` `__getitem__` `__setitem__` `__next__` `__bool__` `__hash__`。运算符重载即实现对应魔术方法，**挂在对象自身或其原型上均可**。

- 继承判断用 `ischild(dog, Dog)`（True）。
- `is` 是严格同一性判断，**不是**继承判断。
- 对象嵌套：`object Outer ... object Inner ... end end`，内层成为外层的属性 `Outer.Inner`。

### 错误处理

```
try
    risky()
catch e (ValueError)          # 只捕获指定名字的错误
    print("值错误:", e)
catch e (KeyError)
    print("键错误:", e)
end

throw Error("MyError", "自定义信息")

try
    throw Error("A", "x")
catch e (A)
    throw e                   # 重抛：e 就是错误对象
end
```

与 Python 差异：

- catch 必须带错误名 `(Name)`；名字不匹配的错误继续向外传播。
- 错误对象有 `__name__` 与 `__msg__` 属性，`Str(e)` 可读。
- **没有 finally/ensure**。"离开作用域必做"的逻辑要显式写在正常路径与 catch 之后，或封装成函数。

### 模块

```
import os                     # 标准库（os、builtins）
import mylib at "mylib.kiz"   # 相对当前文件的路径
import mylib                  # 无 at 时依次搜索：当前文件目录、可执行文件目录、modules/
```

差异与限制：

- 导入路径**不允许绝对路径与 `..`**（安全限制）。
- **循环导入会被检测并报 ImportError**，不会崩溃。
- 同一文件多次导入只执行一次（按规范化路径缓存）。
- 模块顶层不带 `__private__` 前缀的变量导出为模块属性：`mylib.foo`。

### 输入输出

```
print("a", 1, [2, 3])         # 空格分隔，自动换行
name = input("你的名字: ")     # 返回 Str

f = open("data.txt", "r")     # 模式: r w a r+ w+ a+
text = f.read()
f.close()

g = open("out.txt", "w")
g.write("hello")
g.close()
```

- `open` 的相对路径以**当前脚本所在目录**为基准（不是进程工作目录）。
- 文件用完记得 `close()`（句柄析构时也会关闭，但及时关闭是好习惯）。
- `input()` 遇到 EOF（管道结束/Ctrl-D）会抛 `EOFError`。

## 语义差异陷阱清单

从 Python 迁移最容易踩的坑，按出现频率排序：

1. **忘了 `end`**：每个块都要收尾。
2. **写了 `continue`**：kiz 是 `next`。
3. **写了 `elif`**：kiz 是 `else if`。
4. **字典字面量写 `=`**：kiz 用冒号 `{"a": 1}`。
5. **以为 `b = a` 是引用**：kiz 是深拷贝；共享可变状态用 Object。
6. **以为 `1 == True`**：kiz 强类型，跨类型 `==` 抛 TypeError。真值判断直接 `if x`。
7. **以为 `/` 是整除**：`/` 结果可能是 Decimal；整型结果用 `Int(a / b)`。
8. **幂运算写 `**`**：kiz 是 `^`。
9. **依赖 finally**：没有，重构逻辑。
10. **用负索引**：`xs[-1]` 报错；用 `xs.pop()` 或先 `len(xs)`。
11. **`"a" in "abc"`**：可用；字典用 `d.contains(k)`。跨类型 `in`（如 `"x" in [1,2]`）抛 TypeError 而不是返回 False。
12. **多变量交换**：`a, b = b, a` 不支持，用临时变量。

---

更多细节见 [语法特性速览](语法特性速览.md) 与 [内置对象&库速览](内置对象&库速览.md)，示例见 [kiz示例代码与讲解](kiz示例代码与讲解.md)。

---
author: *azhz1107cat*
