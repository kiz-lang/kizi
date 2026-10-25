# kiz库开发指南

**kiz** 当前库比较缺乏，邀请您一起为 kiz 开发库。本文覆盖两条开发路径：用 C++ 写原生库（性能好、可访问系统 API），以及直接用 kiz 写纯 kiz 库（零门槛）。

- [使用C++为kiz开发库](#使用C为kiz开发库)
    - [库函数签名](#库函数签名)
    - [参数校验与类型转换](#参数校验与类型转换)
    - [抛出错误与返回值](#抛出错误与返回值)
    - [注册库](#注册库)
    - [接入构建系统](#接入构建系统)
    - [实战演练：一个完整的小库](#实战演练一个完整的小库)
    - [对象模型速览（库作者须知）](#对象模型速览库作者须知)
- [使用kiz为kiz开发库](#使用kiz为kiz开发库)

---

## 使用C++为kiz开发库

### 库函数签名

以库名+lib命名空间，所有对外函数使用统一签名：

```cpp
#include "models.hpp"   // 必备，定义了对象模型
#include "kiz.hpp"      // 可选，用于库内抛出异常
#include "vm.hpp"       // 可选，包含 call_method/call_function/is_true/obj_to_str/obj_to_debug_str/assert_argc 等实用函数

namespace xx_lib {

model::Object* foo(model::Object* self, const model::List* args) {
    // self 是调用者对象（作为类型方法注册时有效；作为全局函数注册时通常为空）
    // args 是参数列表；返回值为新建对象或借用指针
    return model::load_nil();
}

}
```

**您不需要在库函数中管理引用计数**：参数列表由 VM 持有；返回借用指针（新建对象引用计数为 0 也可以），VM 会在压栈时统一 `make_ref`。

### 参数校验与类型转换

```cpp
// 固定参数个数
kiz::Vm::assert_argc(2, args);
// 多种合法个数
kiz::Vm::assert_argc({1, 2, 3}, args);

// 类型转换（失败自动抛 TypeError，traceback 会指向调用处）
auto i = model::cast_to_int(args->val[0]);    // -> Int*
auto s = model::cast_to_str(args->val[1]);    // -> String*
auto l = model::cast_to_list(args->val[2]);   // -> List*

// 取第一个参数（空则报 ArgCountError）
auto first = builtin::get_one_arg(args);
```

常用类型与构造：

```cpp
new model::Int(dep::BigInt("123"))                 // 大整数
new model::Decimal(dep::Decimal("1.5"))
new model::String("text")
model::load_true() / model::load_false() / model::load_nil()
model::load_bool(cond)
new model::List(std::vector<model::Object*>{...})  // 构造时自动持有元素引用
```

### 抛出错误与返回值

```cpp
// 抛出 kiz 层可见的错误（可被 catch e (MyLibError) 捕获）
throw NativeFuncError("MyLibError", "详细描述");

// 切勿抛 KizStopRunningSignal——那是停机信号，会静默终止脚本
```

返回值约定：**返回借用指针**。返回容器内已有元素、类型对象等，直接返回指针；返回新建对象时 `new` 出来直接返回（引用计数保持 0，由 VM 接管）。

### 注册库

三种注册方式，按需求选择（注册代码都在 `src/vm/` 下）：

**方式一：注册为某类型的成员方法**（在 `entry_builtins.cpp` 中）：

```cpp
// 挂到 Str 类型上：之后 "abc".foo() 可用
model::based_str->attrs_insert("foo", model::create_nfunc(model::str_foo, "foo"));
```

**方式二：注册为全局内置函数**（同文件末尾）：

```cpp
builtin_insert("myfunc", model::create_nfunc(builtin::myfunc, "myfunc"));
// builtin_insert 是该文件内的lambda：负责 make_ref 并登记名字表
```

**方式三：注册为 import 可导入的标准模块**（推荐给功能较多的库，完整范例见 `libs/os/os_lib.cpp`）：

```cpp
// libs/xx/xx_lib.cpp
model::Object* init_module(model::Object* self, const model::List* args) {
    auto mod = new model::Module("xx");
    mod->attrs_insert("foo", model::create_nfunc(foo, "foo"));
    mod->attrs_insert("bar", model::create_nfunc(bar, "bar"));
    return mod;
}
```

```cpp
// src/vm/entry_std_modules.cpp 中登记初始化函数
std_modules_insert("xx", model::create_nfunc(xx_lib::init_module, "__init__"));
```

之后脚本里 `import xx` 即可使用 `xx.foo()`。

### 接入构建系统

| 构建系统 | 操作 |
|---|---|
| Make | 无需操作（`libs/**/*.cpp` 自动全量编译） |
| xmake | `xmake.lua` 中 `add_files("libs/xx/xx_lib.cpp")` |
| CMake | `CMakeLists.txt` 的 `LIB_SRC_FILES` 列表中追加路径 |

头文件声明放进 `libs/xx/include/xx_lib.hpp`，在实现文件与注册文件中 include。

### 实战演练：一个完整的小库

目标：实现 `math2` 模块，提供 `max(list)` 与 `clamp(v, lo, hi)`。

```cpp
// libs/math2/math2_lib.cpp
#include "../../src/models/models.hpp"
#include "../../src/vm/vm.hpp"
#include "include/math2_lib.hpp"

namespace math2_lib {

model::Object* max(model::Object* self, const model::List* args) {
    auto lst = model::cast_to_list(builtin::get_one_arg(args));
    if (lst->val.empty()) {
        throw NativeFuncError("ValueError", "max of empty list");
    }
    model::Object* best = lst->val[0];
    for (size_t i = 1; i < lst->val.size(); ++i) {
        // 借用 VM 的比较能力：call_method 会把结果压栈
        kiz::Vm::call_method(best, "__lt__", {lst->val[i]});
        auto cmp = kiz::Vm::get_and_pop_stack_top();   // RAII自动释放
        if (model::cast_to_bool(cmp.get())->val) {
            best = lst->val[i];
        }
    }
    return best;   // 列表内元素，借用返回
}

model::Object* clamp(model::Object* self, const model::List* args) {
    kiz::Vm::assert_argc(3, args);
    auto v = model::cast_to_int(args->val[0]);
    auto lo = model::cast_to_int(args->val[1]);
    auto hi = model::cast_to_int(args->val[2]);
    if (v->val < lo->val) return lo;
    if (v->val > hi->val) return hi;
    return v;
}

model::Object* init_module(model::Object* self, const model::List* args) {
    auto mod = new model::Module("math2");
    mod->attrs_insert("max", model::create_nfunc(max, "max"));
    mod->attrs_insert("clamp", model::create_nfunc(clamp, "clamp"));
    return mod;
}

}
```

在 `entry_std_modules.cpp` 注册并加入构建后：

```
import math2
print(math2.max([3, 1, 4, 1, 5]))   # 5
print(math2.clamp(9, 0, 5))          # 5
```

### 对象模型速览（库作者须知）

- **一切皆对象**：`model::Object` 基类持有 `attrs`（属性表）与引用计数。类型枚举 `get_type()` 区分 Int/String/List/Dictionary/Function/Object 等。
- **原型链**：对象的 `__parent__` 属性指向原型。属性查找（`Vm::get_attr`）先查自身再沿链上溯，带环检测。
- **魔术方法**：`__add__` `__eq__` `__str__` `__next__` 等只是普通属性；运算符会被翻译成对它们的调用（先查对象自身，再查原型链）。
- **引用计数纪律**（库函数内）：默认不动计数——返回借用指针；只有把对象**存进自己长期持有的结构**时才 `make_ref`，移除时 `del_ref`。`attrs_insert` 与 List/Dict 构造内部已自动持有。
- **错误传播**：库函数抛 `NativeFuncError` 会被 VM 转成 kiz 层错误对象（带调用栈位置）供用户 catch；库内回调用户函数（`call_function`）时产生的 kiz 错误由 VM 自动解栈，C++ 循环无需干预。

## 使用kiz为kiz开发库

纯 kiz 库无需编译，放到 `modules/` 目录即可被发现：

1. 创建库文件夹 `modules/YourLibName`
2. 主逻辑放在 `modules/YourLibName/__main__.kiz` 中
3. 在其他 kiz 文件中使用：

```
import YourLibName
YourLibName.foo()
```

`__main__.kiz` 顶层定义的函数与变量自动成为模块属性。也可以用相对路径导入任意位置的 kiz 文件：`import x at "路径.kiz"`（不允许绝对路径与 `..`，循环导入会被检测并报错）。

---
author : *azhz1107cat*
