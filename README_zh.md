# ALB 编程语言参考手册

> **版本 0.1.1** | ALB 是一门基于 S-表达式的 Lisp 方言脚本语言，语法简洁、表达力强，支持函数式编程与模块化开发。

**[English](README.md)**

---

## 目录

1. [快速开始](#快速开始)
2. [数据类型](#数据类型)
3. [特殊形式（关键字）](#特殊形式)
4. [内置函数](#内置函数)
5. [标准库](#标准库)
6. [编译为可执行文件](#编译为可执行文件)
7. [IDE 使用](#ide-使用)

---

## 快速开始

```lisp
; 这是注释（以 ; 开头）

; 打印输出
(print "Hello, ALB!")

; 定义变量
(let x 42)
(print x)

; 定义函数
(let add (func (list a b) (+ a b)))
(print (add 3 5))   ; => 8

; 条件判断
(if (> x 10)
  (print "大")
  (print "小"))

; 循环
(let i 0)
(while (< i 5)
  (print i)
  (set i (+ i 1)))
```

**运行方式：**
- 双击 `.alb` 文件（IDE 打开）
- 命令行：
  - **Windows:** `ALB.exe script.alb`
  - **Linux/macOS:** `./ALB script.alb`
- IDE 中按 F5 运行
- 启动 REPL：`ALB.exe`（Windows）或 `./ALB`（Linux/macOS）

---

## 数据类型

| 类型 | 说明 | 示例 |
|------|------|------|
| `num` | 数字（浮点数） | `42` `3.14` `-1` |
| `str` | 字符串 | `"hello"` `"世界"` |
| `list` | 列表 | `(list 1 2 3)` |
| `map` | 映射（字典） | `(map "name" "ALB" "ver" "0.0.6")` |
| `fn` | 函数 | `(func (list x) (* x x))` |
| `nil` | 空值 | `nil` |

**布尔值**：`true` = `1`，`false` = `0`。任何非零数字和非空字符串/列表/映射都为"真"。

**常量**：`pi`（3.14159...）、`e`（2.71828...）、`nil`、`true`、`false`

---

## 特殊形式

### 变量定义

```lisp
; let — 定义局部变量
(let x 10)
(let name "ALB")
(let items (list 1 2 3))

; set — 修改已有变量
(set x 20)

; const — 定义不可变常量
(const MAX_SIZE 100)

; define — 顶层定义（简写形式）
(define PI 3.14159)
(define (square x) (* x x))    ; => (let square (func (list x) (* x x)))

; 复合赋值
(+= x 5)    ; x = x + 5
(-= x 3)    ; x = x - 3
(*= x 2)    ; x = x * 2
(/= x 4)    ; x = x / 4
(+= name " world")  ; 字符串拼接
```

### 函数定义

三种等价写法：

```lisp
; 方式1：标准 func
(let add (func (list a b) (+ a b)))

; 方式2：箭头语法 =>
(let add (list a b) => (+ a b))

; 方式3：命名 func
(func add a b (+ a b))
```

**带 docstring 的函数：**
```lisp
(let greet (func (list name) "向用户打招呼"
  (print (str "Hello, " name "!"))))
```

**关键字参数 (v0.1.0)：**
```lisp
(let draw-rect (func (list x y :width 100 :height 50 :color "red")
  (prints (str x " " y " " width " " height " " color))))

(draw-rect 10 20 :width 200)  ; x=10 y=20 width=200 height=50 color="red"
```

**可变参数 (v0.1.0)：**
```lisp
(let sum (func (list &rest args)
  (fold + args 0)))
(sum 1 2 3 4)  ; => 10
```

**多语句函数体：**
```lisp
(let factorial (func (list n) "计算阶乘"
  (if (< n 2)
    1
    (* n (factorial (- n 1))))))
```

### 条件控制

```lisp
; if — 条件分支
(if (> x 0)
  (print "正数")
  (print "非正数"))

; when — 仅当条件为真时执行（无 else）
(when (> score 90)
  (print "优秀")
  (+= bonus 10))

; match — 模式匹配
(match value
  1 (print "一")
  2 (print "二")
  _ (print "其他"))

; 结构体模式匹配 (v0.1.0)
(match user
  (User name age _) (print (str name " 今年 " age " 岁"))
  _ (print "不是用户"))

; cond — 多路条件
(cond
  ((> score 90) "A")
  ((> score 80) "B")
  ((> score 60) "C")
  (else "F"))
```

### 循环

```lisp
; while — 条件循环
(let i 0)
(while (< i 10)
  (print i)
  (+= i 1))

; do — 顺序执行多个表达式
(do
  (print "step 1")
  (print "step 2")
  (print "done"))

; break — 跳出循环
(while true
  (if (> i 100) (break))
  (+= i 1))

; for-i — C 风格索引循环
(for-i (i 0 (< i 10) (+= i 1))
  (print i))
```

### 列表构造

```lisp
; list — 创建列表
(let nums (list 1 2 3 4 5))
(let empty (list))

; quote — 不求值（直接返回表达式本身）
(quote (a b c))   ; => (list 'a 'b 'c)

; 反引号/准引用 — 模板与反引用（用于宏）
`(if ,cond ,body)  ; 替换 ,cond 和 ,body，其余保持不变
`(a ,x ,@rest)    ; ,x = 插入值，,@rest = 拼接列表

; 字符串插值（反引号）
`你好，${name}！你今年 ${age} 岁了。`

; Heredoc — 多行字符串
(let html <<HTML
<div>
  <h1>标题</h1>
</div>
HTML
)
```

```lisp
; let* — 顺序绑定（后面的可以引用前面的）
(let* ((x 10)
       (y (+ x 5))
       (z (* x y)))
  (print z))   ; => 150

; letrec — 递归绑定（互递归函数）
(letrec ((even? (func (list n) (if (== n 0) true (odd? (- n 1)))))
         (odd?  (func (list n) (if (== n 0) false (even? (- n 1))))))
  (even? 42))   ; => true
```

### 模块导入

```lisp
; use — 导入标准库或自定义模块
(use "math")
(use "string")
(use "io")
(use "gui")

; 带选项导入
(use "math" :only (list pi sqrt abs))     ; 仅导入指定函数
(use "string" :as "s")                     ; 加前缀：s.str-trim
```

### 宏系统 (v0.1.0)

```lisp
; defmacro — 定义宏（代码转换器）
(defmacro when (cond body)
  `(if ,cond ,body))

(defmacro unless (cond body)
  `(if ,cond nil ,body))

; 用法:
(unless (> x 10) (print "x 很小"))
; 展开为: (if (> x 10) nil (print "x 很小"))

; 反引号模板语法：
;   `  = 准引用（模板）
;   ,  = 反引用（插入值）
;   ,@ = 反引用拼接（拼接列表）
(defmacro for-range (var start end body)
  `(for-i (,var ,start (< ,var ,end) (+= ,var 1))
     ,body))
(for-range i 1 10 (print i))  ; 打印 1 到 9
```

### 模块导出 (v0.1.0)

```lisp
; export — 控制模块导入时哪些名称可见
(export greet add)
(let secret 42)             ; 不导出
(let greet (func (list name) (str "Hello, " name)))
(let add (func (list a b) (+ a b)))
```

### 结构体 (v0.1.0)

```lisp
; struct — 定义结构体类型
(struct Point (x y))

; 创建实例
(let p (Point 10 20))

; 访问字段
(Point-x p)      ; => 10
(Point-y p)      ; => 20
(Point? p)       ; => 1（类型谓词）

; 通用访问
(struct-get p "x")         ; => 10
(struct-set! p "x" 30)     ; 修改字段
```

### 字节码编译与分发 (v0.1.1)

```bash
# 编译源码为字节码 (.albc)
ALB compile hello.alb                    # → hello.albc
ALB compile hello.alb --obfuscate        # → hello.albc（带 XOR 混淆）

# 运行字节码（自动识别 .albc 和 .alb）
ALB run hello.albc                       # 运行字节码
ALB run hello.alb                        # 运行源码（自动识别）
ALB hello.albc                           # 也可以（按扩展名/魔数自动识别）

# 打包为分发包
ALB package hello.alb                    # → hello.zip（字节码 + 运行时 + 脚本）
ALB package hello.alb -o myapp.zip       # 自定义输出名
ALB package hello.alb --obfuscate        # 带混淆打包
```

**字节码文件格式 (.albc)：**
- 28 字节头部：魔数 `ALBC`、格式版本、ALB 版本、标志位、表达式数、校验和
- 二进制序列化的 AST（平台无关）
- 可选 XOR 字符串常量混淆
- 校验和完整性验证

**分发包 (.zip) 内容：**
- `<name>.albc` — 编译后的字节码
- `ALB.exe` — 运行时解释器
- `run.bat` / `run.sh` — 启动脚本
- `manifest.json` — 元数据（入口文件、版本信息）

### 错误处理

```lisp
; try — 捕获异常
(try
  (/ 1 0)
  (catch err
    (print (str "错误: " (map-get err "message")))))

; try/catch/finally — 保证清理
(try
  (let f (file-open "data.txt"))
  (process f)
  (catch err
    (print (map-get err "message")))
  (finally
    (file-close f)))   ; 始终执行

; throw — 抛出异常
(throw "自定义错误")
(throw (error "TypeError" "参数类型不正确"))
```

---

## 内置函数

### I/O 输入输出

| 函数 | 说明 | 示例 |
|------|------|------|
| `print` | 打印并换行 | `(print "hello" 42)` |
| `prints` | 打印不换行 | `(prints "loading...")` |
| `out` | 打印参数，内容含 `\n` 时才换行 | `(out "a" "b")` → `a b` |
| `input` | 读取一行输入（可带提示符） | `(input "请输入: ")` |
| `read` | 读取一行输入（无提示符） | `(read)` → 数字或字符串 |
| `press` | 等待按键 | `(press)` → 按键名；`(press "Return")` → 1/0 |

### 类型与元信息

| 函数 | 说明 | 示例 |
|------|------|------|
| `type` | 返回值的类型名 | `(type 42)` → `"num"` |
| `len` | 字符串/列表/映射长度 | `(len "hello")` → `5` |
| `str` | 转为字符串 | `(str 42 "px")` → `"42px"` |
| `num` | 转为数字 | `(num "3.14")` → `3.14` |
| `doc` | 查看函数文档 | `(doc print)` |
| `test` | 断言测试（比较字符串值） | `(test (+ 1 2) "3")` → PASS |
| `assert` | 断言条件为真 | `(assert (> x 0) "x 必须为正数")` |
| `json-parse` | 解析 JSON 字符串为 ALB 值 | `(json-parse "{\"x\":1}")` → map |
| `json-serialize` | 将 ALB 值转为 JSON 字符串 | `(json-serialize (dict "x" 1))` → `"{\"x\":1}"` |

### 算术运算

| 函数 | 说明 | 示例 |
|------|------|------|
| `+` | 加法 / 字符串拼接 | `(+ 1 2 3)` → `6` |
| `-` | 减法 / 取负 | `(- 10 3)` → `7`，`(- 5)` → `-5` |
| `*` | 乘法 | `(* 2 3 4)` → `24` |
| `/` | 除法 | `(/ 10 3)` → `3.333...` |
| `%` | 取模 | `(% 10 3)` → `1` |
| `^` | 幂运算 | `(^ 2 10)` → `1024` |

### 比较运算

| 函数 | 说明 | 示例 |
|------|------|------|
| `==` | 等于 | `(== "abc" "abc")` → `1` |
| `!=` | 不等于 | `(!= 1 2)` → `1` |
| `<` `>` `<=` `>=` | 数值比较 | `(< 3 5)` → `1` |

### 逻辑运算

| 函数 | 说明 | 示例 |
|------|------|------|
| `and` | 逻辑与（短路） | `(and true true)` → `1` |
| `or` | 逻辑或（短路） | `(or false true)` → `1` |
| `not` | 逻辑非 | `(not false)` → `1` |

### 数学函数

| 函数 | 说明 | 示例 |
|------|------|------|
| `sqrt` | 平方根 | `(sqrt 16)` → `4` |
| `abs` | 绝对值 | `(abs -5)` → `5` |
| `floor` | 向下取整 | `(floor 3.7)` → `3` |
| `ceil` | 向上取整 | `(ceil 3.2)` → `4` |
| `round` | 四舍五入 | `(round 3.5)` → `4` |
| `sin` `cos` `tan` | 三角函数（弧度） | `(sin (/ pi 2))` → `1` |
| `asin` `acos` `atan` | 反三角函数 | |
| `atan2` | 反正切（双参数） | `(atan2 y x)` |
| `log` | 自然对数 | `(log e)` → `1` |
| `log2` | 以2为底对数 | `(log2 8)` → `3` |
| `log10` | 以10为底对数 | `(log10 100)` → `2` |
| `exp` | 指数 e^x | `(exp 1)` → `2.718...` |
| `min` | 最小值 | `(min 3 7)` → `3` |
| `max` | 最大值 | `(max 3 7)` → `7` |
| `clamp` | 限制范围 | `(clamp 15 0 10)` → `10` |
| `random` | 随机浮点数 [0, n) | `(random 100)` |
| `random-int` | 随机整数 [a, b] | `(random-int 1 6)` |

### 列表操作

| 函数 | 说明 | 示例 |
|------|------|------|
| `car` | 取第一个元素 | `(car (list 1 2 3))` → `1` |
| `cdr` | 取剩余元素 | `(cdr (list 1 2 3))` → `(2 3)` |
| `cons` | 前插元素 | `(cons 0 (list 1 2))` → `(0 1 2)` |
| `nth` | 按索引取值 | `(nth (list 10 20 30) 1)` → `20` |
| `push` | 尾部追加 | `(push (list 1 2) 3)` → `(1 2 3)` |
| `range` | 生成数字序列 | `(range 5)` → `(0 1 2 3 4)` |
| `range` | 指定范围 | `(range 1 6)` → `(1 2 3 4 5)` |
| `range` | 指定步长 | `(range 0 10 2)` → `(0 2 4 6 8)` |

### 高阶函数

| 函数 | 说明 | 示例 |
|------|------|------|
| `apply` | 用列表参数调用函数 | `(apply + (list 1 2 3))` → `6` |
| `map` | 映射列表 | `(map (func (list x) (* x x)) (list 1 2 3))` → `(1 4 9)` |
| `filter` | 过滤列表 | `(filter (func (list x) (> x 2)) (list 1 2 3 4))` → `(3 4)` |
| `fold` | 折叠列表 | `(fold (func (list acc x) (+ acc x)) (list 1 2 3) 0)` → `6` |
| `sort` | 排序 | `(sort (list 3 1 2))` → `(1 2 3)` |
| `reverse` | 反转列表/字符串 | `(reverse (list 1 2 3))` → `(3 2 1)` |
| `zip` | 拉链合并 | `(zip (list 1 2) (list "a" "b"))` → `((1 "a") (2 "b"))` |
| `unzip` | 拆链 | `(unzip (list (list 1 "a") (list 2 "b")))` |
| `any` | 任一元素满足条件 | `(any (func (list x) (> x 5)) (list 1 6 3))` → `1` |
| `all` | 所有元素满足条件 | `(all (func (list x) (> x 0)) (list 1 2 3))` → `1` |
| `compose` | 函数组合 | `(compose f g)` — 先 g 后 f |
| `pipe` | 函数管道 | `(pipe f g)` — 先 f 后 g |
| `partial` | 偏应用 | `(let add5 (partial + 5))` 然后 `(add5 3)` → `8` |
| `for`/`each` | 遍历执行（别名，同一函数） | `(for (list 1 2 3) (func (list x) (print x)))` |
| `flatten` | 扁平化嵌套列表 | `(flatten (list 1 (list 2 3) 4))` → `(1 2 3 4)` |
| `uniq` | 去重 | `(uniq (list 1 2 2 3 1))` → `(1 2 3)` |
| `take` | 取前 n 个 | `(take 2 (list 1 2 3 4))` → `(1 2)` |
| `drop` | 丢弃前 n 个 | `(drop 2 (list 1 2 3 4))` → `(3 4)` |
| `copy` | 深拷贝 | `(copy val)` |

### 向量 (v0.1.0)

| 函数 | 说明 | 示例 |
|------|------|------|
| `vector` | 创建向量 | `(vector 1 2 3)` → `#[1 2 3]` |
| `vector-ref` | O(1) 随机访问 | `(vector-ref v 0)` → `1` |
| `vector-set!` | O(1) 原地修改 | `(vector-set! v 0 10)` |
| `vector-push!` | 末尾追加 | `(vector-push! v 4)` |
| `vector-pop!` | 末尾弹出 | `(vector-pop! v)` |
| `vector-size` | 大小 | `(vector-size v)` |
| `vector->list` | 转为列表 | `(vector->list v)` |
| `list->vector` | 从列表转换 | `(list->vector (list 1 2 3))` |

### Map 操作 (v0.1.0)

| 函数 | 说明 | 示例 |
|------|------|------|
| `dict` | 创建 map | `(dict "a" 1 "b" 2)` → `{a: 1, b: 2}` |
| `map-get` | 获取值 | `(map-get m "key")` |
| `map-set` | 设置值（返回新 map） | `(map-set m "key" 42)` |
| `map-del` | 删除键 | `(map-del m "key")` |
| `map-has?` | 键是否存在 | `(map-has? m "key")` → `1`/`0` |
| `map-keys` | 所有键 | `(map-keys m)` → 列表 |
| `map-values` | 所有值 | `(map-values m)` → 列表 |
| `map-merge` | 合并 | `(map-merge m1 m2)` |

### 正则表达式 (v0.1.0)

| 函数 | 说明 | 示例 |
|------|------|------|
| `re-match` | 匹配列表或 nil | `(re-match "(\\d+)" "abc123")` → `("123" "123")` |
| `re-find` | 首个匹配 | `(re-find "\\d+" "abc123")` → `"123"` |
| `re-replace` | 替换所有 | `(re-replace "\\d" "a*b" "X")` → `"aXb"` |
| `re-split` | 正则分割 | `(re-split "\\s+" "a  b   c")` → `("a" "b" "c")` |
| `re-test?` | 测试匹配 | `(re-test? "^\\d+$" "123")` → `1` |

### 位运算 (v0.1.0)

| 函数 | 说明 | 示例 |
|------|------|------|
| `bit-and` | 按位与 | `(bit-and 5 3)` → `1` |
| `bit-or` | 按位或 | `(bit-or 5 3)` → `7` |
| `bit-xor` | 按位异或 | `(bit-xor 5 3)` → `6` |
| `bit-not` | 按位取反 | `(bit-not 0)` → `-1` |
| `bit-lshift` | 左移 | `(bit-lshift 1 4)` → `16` |
| `bit-rshift` | 右移 | `(bit-rshift 16 2)` → `4` |

### 时间 (v0.1.0)

| 函数 | 说明 | 示例 |
|------|------|------|
| `time-now` | Unix 时间戳（秒） | `(time-now)` |
| `time-now-ms` | 时间戳（毫秒） | `(time-now-ms)` |
| `time-format` | 格式化时间 | `(time-format (time-now) "%Y-%m-%d")` |
| `time-parse` | 解析时间字符串 | `(time-parse "2024-01-01" "%Y-%m-%d")` |
| `time-diff` | 时间差（秒） | `(time-diff a b)` |
| `time-sleep` | 暂停（毫秒） | `(time-sleep 1000)` |
| `time-local` | 本地时间 map | `(time-local)` → `{year:2024 month:1 ...}` |

### 系统 (v0.1.0)

| 函数 | 说明 | 示例 |
|------|------|------|
| `exit` | 退出程序 | `(exit 0)` |
| `sleep` | 暂停（毫秒） | `(sleep 500)` |
| `system` | 执行命令，返回退出码 | `(system "dir")` |
| `exec` | 执行命令，返回输出 | `(exec "echo hello")` → `"hello\n"` |

### 网络 (v0.1.0)

| 函数 | 说明 | 示例 |
|------|------|------|
| `net-connect` | TCP 连接 | `(net-connect "127.0.0.1" 8080)` |
| `net-send` | 发送数据 | `(net-send sock "hello")` |
| `net-recv` | 接收数据 | `(net-recv sock 4096)` |
| `net-close` | 关闭套接字 | `(net-close sock)` |
| `net-listen` | TCP 监听 | `(net-listen 8080)` |
| `net-accept` | 接受连接 | `(net-accept sock)` |
| `http-get` | HTTP GET | `(http-get "http://example.com")` |
| `http-post` | HTTP POST | `(http-post "http://api/data" "key=val")` |

### 字符串操作

| 函数 | 说明 | 示例 |
|------|------|------|
| `substr` | 子串 | `(substr "hello" 1 3)` → `"ell"` |
| `str-split` | 分割 | `(str-split "a,b,c" ",")` → `("a" "b" "c")` |
| `str-join` | 连接 | `(str-join (list "a" "b") "-")` → `"a-b"` |
| `str-replace` | 替换 | `(str-replace "hello" "l" "r")` → `"herro"` |
| `str-find` | 查找位置 | `(str-find "hello" "ll")` → `2` |
| `str-trim` | 去空白 | `(str-trim "  hi  ")` → `"hi"` |
| `str-upper` | 转大写 | `(str-upper "hello")` → `"HELLO"` |
| `str-lower` | 转小写 | `(str-lower "HELLO")` → `"hello"` |
| `str-starts?` | 是否以…开头 | `(str-starts? "hello" "he")` → `1` |
| `str-ends?` | 是否以…结尾 | `(str-ends? "hello" "lo")` → `1` |
| `str-repeat` | 重复字符串 | `(str-repeat "ab" 3)` → `"ababab"` |
| `str-reverse` | 反转 | `(str-reverse "hello")` → `"olleh"` |
| `char-code` | 字符→编码 | `(char-code "A")` → `65` |
| `code-char` | 编码→字符 | `(code-char 65)` → `"A"` |

### 文件与目录

| 函数 | 说明 | 示例 |
|------|------|------|
| `file-read` | 读取文件内容 | `(file-read "data.txt")` |
| `file-write` | 写入文件 | `(file-write "out.txt" "hello")` |
| `file-append` | 追加写入 | `(file-append "log.txt" "line\n")` |
| `file-exists?` | 文件是否存在 | `(file-exists? "a.txt")` → `1`/`0` |
| `file-delete` | 删除文件 | `(file-delete "tmp.txt")` |
| `file-lines` | 按行读取 | `(file-lines "data.txt")` → 列表 |
| `dir-list` | 列出目录 | `(dir-list ".")` → 列表 |
| `dir-exists?` | 目录是否存在 | `(dir-exists? "lib")` → `1`/`0` |
| `dir-create` | 创建目录 | `(dir-create "output")` |
| `path-join` | 拼接路径 | `(path-join "a" "b" "c.txt")` → `"a/b/c.txt"` |
| `path-ext` | 取扩展名 | `(path-ext "file.txt")` → `"txt"` |
| `path-name` | 取文件名（无扩展名） | `(path-name "a/b/file.txt")` → `"file"` |

### 序列化

| 函数 | 说明 | 示例 |
|------|------|------|
| `write` | 值→S-表达式字符串 | `(write (list 1 "hi"))` |
| `read-sexpr` | S-表达式字符串→值 | `(read-sexpr "(1 2 3)")` → `(1 2 3)` |

### 错误处理

| 函数 | 说明 | 示例 |
|------|------|------|
| `throw` | 抛出异常 | `(throw "出错了")` |
| `error` | 创建错误对象 | `(error "TypeError" "类型错误")` |

---

## 标准库

ALB 提供 5 个标准库模块，通过 `(use "模块名")` 导入。

### math — 数学库

```lisp
(use "math")
```

提供：`PI` `E` `TAU` `PHI` `INF`、`lerp`、`inverse-lerp`、`remap`、`deg->rad`、`rad->deg`、`rand-float`、`rand-int`、`rand-bool`、`sign`、`lerp-angle`、`vec2-dot`、`vec2-len`、`vec2-dist`、`vec2-norm`

### string — 字符串库

```lisp
(use "string")
```

提供：`str-contains?`、`str-empty?`、`str-blank?`、`str-count`、`str-at`、`str-take`、`str-drop`、`str-sub`、`str-slice`、`str-capitalize`、`str-title`、`str-pad-left`、`str-pad-right`、`str-center`、`str-chars`、`str-from-chars`、`words`、`lines`

### io — IO 工具库

```lisp
(use "io")
```

提供：`file-copy`、`file-size`、`file-empty?`、`file-backup`、`file-touch`、`file-write-lines`、`file-filter-lines`、`file-map-lines`、`dir-ensure`、`dir-files`、`dir-dirs`、`dir-files-recursive`、`path-base`、`path-dir`、`path-change-ext`、`path-absolute?`、`csv-parse`、`csv-format`

### gui — 图形界面库

```lisp
(use "gui")
```

提供 GUI 窗口、控件、布局、菜单、工具栏等完整图形界面功能。

**GUI 示例：**
```lisp
(use "gui")

; 定义按钮回调
(let on-click (func (list wid etype data extra)
  (msgbox "按钮被点击了！" "提示")))

; 创建窗口
(gui-window "我的应用" 400 300)

; 添加控件
(let btn (gui-button 0 "点击我" "on-click"))
(gui-pack btn :side "top" :fill "x" :pad 10)

; 启动事件循环
(gui-run 1)
```

**GUI 控件一览：**

| 函数 | 说明 |
|------|------|
| `gui-window` | 创建主窗口 |
| `gui-label` | 标签 |
| `gui-button` | 按钮 |
| `gui-entry` | 单行输入框 |
| `gui-text` | 多行文本框 |
| `gui-checkbutton` | 复选框 |
| `gui-radiobutton` | 单选按钮 |
| `gui-frame` | 容器框架 |
| `gui-canvas` | 画布 |
| `gui-combobox` | 下拉框 |
| `gui-listbox` | 列表框 |
| `gui-treeview` | 树形视图 |
| `gui-notebook` | 标签页 |
| `gui-progressbar` | 进度条 |
| `gui-scale` | 滑块 |
| `gui-menubar` | 菜单栏 |
| `gui-toolbar` | 工具栏 |
| `gui-statusbar` | 状态栏 |
| `gui-pack` / `gui-grid` / `gui-place` | 布局方式 |
| `gui-run` | 启动事件循环 |
| `msgbox` | 消息对话框 |
| `gui-choose-file` | 文件选择对话框 |

### agame — 2D 游戏与交互动画库

```lisp
(use "agame")
```

基于 SDL2 的 2D 游戏引擎，提供窗口管理、图形绘制、输入处理、图像加载、文字渲染和音效播放。

**依赖安装：**
- **Windows:** 下载 [SDL2 开发库](https://libsdl.org)，编译时链接 `-lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer`
- **Linux:** `sudo apt-get install libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev`
- **macOS:** `brew install sdl2 sdl2_image sdl2_ttf sdl2_mixer`

**Agame 示例 — 移动小球：**
```lisp
(use "agame")

; 初始化窗口
(agame-init "移动小球" 640 480)

; 初始位置
(let x 320)
(let y 240)
(let speed 5)

; 游戏主循环
(while (not (agame-should-quit))
  ; 清屏
  (agame-set-color (list 0 0 0 255))
  (agame-clear)
  
  ; 绘制小球
  (agame-set-color (list 255 100 100 255))
  (agame-fill-circle x y 20)
  
  ; 处理键盘输入
  (when (agame-key-pressed "left")  (set x (- x speed)))
  (when (agame-key-pressed "right") (set x (+ x speed)))
  (when (agame-key-pressed "up")    (set y (- y speed)))
  (when (agame-key-pressed "down")  (set y (+ y speed)))
  
  ; 边界检测
  (when (< x 20) (set x 20))
  (when (> x 620) (set x 620))
  (when (< y 20) (set y 20))
  (when (> y 460) (set y 460))
  
  ; 刷新显示
  (agame-present)
  (agame-delay 16))

; 退出
(agame-quit)
```

**Agame 函数一览：**

| 函数 | 说明 |
|------|------|
| **窗口管理** | |
| `agame-init` | 初始化窗口 `(agame-init "标题" 宽 高)` |
| `agame-quit` | 关闭窗口并释放资源 |
| `agame-clear` | 清屏 |
| `agame-present` | 刷新显示 |
| `agame-delay` | 延时毫秒 |
| `agame-window-width` | 获取窗口宽度 |
| `agame-window-height` | 获取窗口高度 |
| `agame-set-title` | 设置窗口标题 |
| **颜色** | |
| `agame-set-color` | 设置当前颜色 `(agame-set-color (list r g b a))` |
| **绘制** | |
| `agame-draw-point` | 画点 |
| `agame-draw-line` | 画线 |
| `agame-draw-rect` | 画矩形框 |
| `agame-fill-rect` | 填充矩形 |
| `agame-draw-circle` | 画圆 |
| `agame-fill-circle` | 填充圆 |
| `agame-draw-triangle` | 画三角形 |
| `agame-fill-triangle` | 填充三角形 |
| **输入** | |
| `agame-poll-event` | 轮询事件 |
| `agame-should-quit` | 检查是否应该退出 |
| `agame-key-pressed` | 检查按键是否按下 |
| `agame-mouse-x` / `agame-mouse-y` | 鼠标坐标 |
| `agame-mouse-pressed` | 鼠标按键状态 |
| **图像** | |
| `agame-load-image` | 加载图像 |
| `agame-draw-image` | 绘制图像 |
| `agame-free-image` | 释放图像 |
| **文字** | |
| `agame-load-font` | 加载字体 |
| `agame-draw-text` | 绘制文字 |
| `agame-free-font` | 释放字体 |
| **音效** | |
| `agame-load-sound` | 加载音效 |
| `agame-play-sound` | 播放音效 |
| `agame-free-sound` | 释放音效 |
| **常量** | |
| `agame-black` `agame-white` `agame-red` ... | 预定义颜色 |
| `agame-key-up` `agame-key-down` ... | 按键常量 |

---

## 编译为可执行文件

ALB 支持将 `.alb` 脚本编译为独立的可执行文件。

- **Windows:** `.exe`
- **Linux:** ELF 可执行文件（无扩展名）
- **macOS:** Mach-O 可执行文件（无扩展名）

### 命令行编译

```bash
; 普通编译（运行时需 lib/ 目录）
ALB -b script.alb -o script      # Linux/macOS
ALB.exe -b script.alb -o script.exe  # Windows

; 打包编译（库嵌入，完全独立）
ALB -b script.alb --embed -o script      # Linux/macOS
ALB.exe -b script.alb --embed -o script.exe  # Windows
```

### IDE 编译

- **F7** — Build Executable（普通编译）
- **F8** — Build with Libraries（打包编译，DLL 和库源码全部嵌入）

### 命令行帮助

```bash
ALB.exe -h              ; 显示帮助
ALB.exe -h --list       ; 列出所有内置函数
ALB.exe -h print        ; 查看 print 函数帮助
ALB.exe -h gui          ; 查看 gui 库帮助
```

---

## IDE 使用

ALB IDE 是基于 Python + Tkinter 的轻量级编辑器，支持：

- 多标签页编辑
- 语法高亮
- 自动补全（`Ctrl+Space` 触发）
- 函数签名提示（输入 `(` 后自动显示）
- 智能缩进（Enter 后自动对齐）
- 项目管理（`.albprj` 文件）
- 一键编译运行（F5 运行，F7/F8 编译）

---

## 示例代码

### 猜数字游戏

```lisp
(let target (random-int 1 100))
(let attempts 0)
(print "猜一个 1-100 的数字！")

(while true
  (let guess (num (input "你的猜测: ")))
  (+= attempts 1)
  (if (== guess target)
    (do
      (print (str "恭喜！你用了 " attempts " 次猜对了！"))
      (break))
    (if (< guess target)
      (print "太小了！")
      (print "太大了！"))))
```

### 文件处理

```lisp
(use "io")

; 读取文件并按行处理
(let lines (file-lines "data.csv"))
(each line lines
  (let parts (str-split line ","))
  (print (str (nth parts 0) ": " (nth parts 1))))
```

### 函数式编程

```lisp
; 列表求和
(let nums (list 1 2 3 4 5))
(let total (fold + nums 0))    ; => 15

; 过滤偶数
(let evens (filter (func (list x) (== (% x 2) 0)) nums))  ; => (2 4)

; 映射平方
(let squares (map (func (list x) (* x x)) nums))  ; => (1 4 9 16 25)
```

---

*ALB v0.1.1 — 一门简洁、实用的 Lisp 方言脚本语言*
