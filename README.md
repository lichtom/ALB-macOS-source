# ALB Programming Language Reference — macOS Edition

> **Version 0.1.1** | ALB is a Lisp-dialect scripting language based on S-expressions, featuring concise syntax, strong expressiveness, and support for functional programming and modular development.  
> **This build is optimized exclusively for macOS.**

**[中文文档](README_zh.md)**

---

## Table of Contents

1. [Quick Start](#quick-start)
2. [Data Types](#data-types)
3. [Special Forms (Keywords)](#special-forms)
4. [Built-in Functions](#built-in-functions)
5. [Standard Libraries](#standard-libraries)
6. [Compiling to Executable](#compiling-to-executable)
7. [IDE Usage](#ide-usage)

---

## Quick Start

```lisp
; This is a comment (starts with ;)

; Print output
(print "Hello, ALB!")

; Define a variable
(let x 42)
(print x)

; Define a function
(let add (func (list a b) (+ a b)))
(print (add 3 5))   ; => 8

; Conditional
(if (> x 10)
  (print "big")
  (print "small"))

; Loop
(let i 0)
(while (< i 5)
  (print i)
  (set i (+ i 1)))
```

**How to install (macOS):**
1. Open the DMG installer and drag `ALB.app` / `ALB_IDE.app` to Applications
2. Right-click the app → **Open** (first time only, to bypass Gatekeeper)
3. The `ALB` command is automatically registered to `/usr/local/bin/` on first run

**How to run:**
- Double-click `ALB.app` to open a Terminal REPL
- Command line (after install): `ALB script.alb`
- Start REPL: `ALB`
- In the IDE: press **F5**

---

## Data Types

| Type | Description | Example |
|------|-------------|---------|
| `num` | Number (floating-point) | `42` `3.14` `-1` |
| `str` | String | `"hello"` `"world"` |
| `list` | List | `(list 1 2 3)` |
| `map` | Map (dictionary) | `(map "name" "ALB" "ver" "0.0.6")` |
| `fn` | Function | `(func (list x) (* x x))` |
| `nil` | Null value | `nil` |

**Booleans**: `true` = `1`, `false` = `0`. Any non-zero number and non-empty string/list/map is "truthy".

**Constants**: `pi` (3.14159...), `e` (2.71828...), `nil`, `true`, `false`

---

## Special Forms

### Variable Definition

```lisp
; let — define a local variable
(let x 10)
(let name "ALB")
(let items (list 1 2 3))

; set — modify an existing variable
(set x 20)

; const — define an immutable constant
(const MAX_SIZE 100)

; define — top-level definition (shorthand)
(define PI 3.14159)
(define (square x) (* x x))    ; => (let square (func (list x) (* x x)))

; Compound assignment
(+= x 5)    ; x = x + 5
(-= x 3)    ; x = x - 3
(*= x 2)    ; x = x * 2
(/= x 4)    ; x = x / 4
(+= name " world")  ; string concatenation
```

### Function Definition

Three equivalent forms:

```lisp
; Form 1: Standard func
(let add (func (list a b) (+ a b)))

; Form 2: Arrow syntax =>
(let add (list a b) => (+ a b))

; Form 3: Named func
(func add a b (+ a b))
```

**With docstring:**
```lisp
(let greet (func (list name) "Greets the user"
  (print (str "Hello, " name "!"))))
```

**Keyword arguments (v0.1.0):**
```lisp
(let draw-rect (func (list x y :width 100 :height 50 :color "red")
  (prints (str x " " y " " width " " height " " color))))

(draw-rect 10 20 :width 200)  ; x=10 y=20 width=200 height=50 color="red"
```

**Variadic arguments (v0.1.0):**
```lisp
(let sum (func (list &rest args)
  (fold + args 0)))
(sum 1 2 3 4)  ; => 10
```

**Multi-statement body:**
```lisp
(let factorial (func (list n) "Compute factorial"
  (if (< n 2)
    1
    (* n (factorial (- n 1))))))
```

### Conditionals

```lisp
; if — conditional branch
(if (> x 0)
  (print "positive")
  (print "non-positive"))

; when — execute only if condition is true (no else)
(when (> score 90)
  (print "excellent")
  (+= bonus 10))

; match — pattern matching
(match value
  1 (print "one")
  2 (print "two")
  _ (print "other"))

; Struct pattern matching (v0.1.0)
(match user
  (User name age _) (print (str name " is " age))
  _ (print "not a user"))

; cond — multi-way conditional
(cond
  ((> score 90) "A")
  ((> score 80) "B")
  ((> score 60) "C")
  (else "F"))
```

### Loops

```lisp
; while — conditional loop
(let i 0)
(while (< i 10)
  (print i)
  (+= i 1))

; do — execute multiple expressions in sequence
(do
  (print "step 1")
  (print "step 2")
  (print "done"))

; break — exit loop
(while true
  (if (> i 100) (break))
  (+= i 1))

; for-i — C-style indexed loop
(for-i (i 0 (< i 10) (+= i 1))
  (print i))
```

### List Construction

```lisp
; list — create a list
(let nums (list 1 2 3 4 5))
(let empty (list))

; quote — return expression unevaluated
(quote (a b c))   ; => (list 'a 'b 'c)

; Backtick/quasiquote — template with unquote (for macros)
`(if ,cond ,body)  ; substitutes ,cond and ,body, keeps rest as-is
`(a ,x ,@rest)    ; ,x = insert value, ,@rest = splice list

; String interpolation with backtick
`Hello, ${name}! You are ${age} years old.`

; Heredoc — multi-line string
(let html <<HTML
<div>
  <h1>Title</h1>
</div>
HTML
)
```

```lisp
; let* — sequential bindings (later can reference earlier)
(let* ((x 10)
       (y (+ x 5))
       (z (* x y)))
  (print z))   ; => 150

; letrec — recursive bindings (mutually recursive functions)
(letrec ((even? (func (list n) (if (== n 0) true (odd? (- n 1)))))
         (odd?  (func (list n) (if (== n 0) false (even? (- n 1))))))
  (even? 42))   ; => true
```

### Module Import

```lisp
; use — import standard or custom modules
(use "math")
(use "string")
(use "io")
(use "gui")

; Import with options
(use "math" :only (list pi sqrt abs))     ; import only specific functions
(use "string" :as "s")                     ; add prefix: s.str-trim
```

### Macros (v0.1.0)

```lisp
; defmacro — define a macro (code transformer)
(defmacro when (cond body)
  `(if ,cond ,body))

(defmacro unless (cond body)
  `(if ,cond nil ,body))

; Usage:
(unless (> x 10) (print "x is small"))
; Expands to: (if (> x 10) nil (print "x is small"))

; Backquote template syntax:
;   `  = quasiquote (template)
;   ,  = unquote (insert value)
;   ,@ = unquote-splicing (splice list)
(defmacro for-range (var start end body)
  `(for-i (,var ,start (< ,var ,end) (+= ,var 1))
     ,body))
(for-range i 1 10 (print i))  ; prints 1 to 9
```

### Module Export (v0.1.0)

```lisp
; export — control which names are visible when module is imported
(export greet add)
(let secret 42)             ; not exported
(let greet (func (list name) (str "Hello, " name)))
(let add (func (list a b) (+ a b)))
```

### Structs (v0.1.0)

```lisp
; struct — define a struct type
(struct Point (x y))

; Create instances
(let p (Point 10 20))

; Access fields
(Point-x p)      ; => 10
(Point-y p)      ; => 20
(Point? p)       ; => 1 (type predicate)

; Generic access
(struct-get p "x")         ; => 10
(struct-set! p "x" 30)     ; modify field
```

### Bytecode Compilation & Distribution (v0.1.1)

```bash
# Compile source to bytecode (.albc)
ALB compile hello.alb                    # → hello.albc
ALB compile hello.alb --obfuscate        # → hello.albc (with XOR obfuscation)

# Run bytecode (auto-detects .albc vs .alb)
ALB run hello.albc                       # run bytecode
ALB run hello.alb                        # run source (auto-detect)
ALB hello.albc                           # also works (auto-detect by extension/magic)

# Package into distribution zip
ALB package hello.alb                    # → hello.zip (bytecode + runtime + scripts)
ALB package hello.alb -o myapp.zip       # custom output name
ALB package hello.alb --obfuscate        # with obfuscation
```

**Bytecode file format (.albc):**
- 28-byte header: magic `ALBC`, format version, ALB version, flags, expression count, checksum
- Serialized AST in binary format (platform-independent)
- Optional XOR obfuscation for string constants
- Checksum verification for integrity

**Distribution package (.zip) contains:**
- `<name>.albc` — compiled bytecode
- `ALB` — macOS runtime interpreter
- `run.sh` — launcher script
- `manifest.json` — metadata (entry point, versions)

### Error Handling

```lisp
; try — catch exceptions
(try
  (/ 1 0)
  (catch err
    (print (str "Error: " (map-get err "message")))))

; try/catch/finally — guaranteed cleanup
(try
  (let f (file-open "data.txt"))
  (process f)
  (catch err
    (print (map-get err "message")))
  (finally
    (file-close f)))   ; always runs

; throw — raise an exception
(throw "Custom error")
(throw (error "TypeError" "Invalid argument type"))
```

---

## Built-in Functions

### I/O

| Function | Description | Example |
|----------|-------------|---------|
| `print` | Print with newline | `(print "hello" 42)` |
| `prints` | Print without newline | `(prints "loading...")` |
| `out` | Print args, no newline unless content has `\n` | `(out "a" "b")` → `a b` |
| `input` | Read a line from stdin (with optional prompt) | `(input "Enter: ")` |
| `read` | Read a line from stdin (no prompt) | `(read)` → number or string |
| `press` | Wait for keypress | `(press)` → key name; `(press "Return")` → 1/0 |

### Type & Meta

| Function | Description | Example |
|----------|-------------|---------|
| `type` | Return type name | `(type 42)` → `"num"` |
| `len` | Length of string/list/map | `(len "hello")` → `5` |
| `str` | Convert to string | `(str 42 "px")` → `"42px"` |
| `num` | Convert to number | `(num "3.14")` → `3.14` |
| `doc` | View function docs | `(doc print)` |
| `test` | Assertion test (compares str values) | `(test (+ 1 2) "3")` → PASS |
| `assert` | Assert condition is truthy | `(assert (> x 0) "x must be positive")` |
| `json-parse` | Parse JSON string to ALB value | `(json-parse "{\"x\":1}")` → map |
| `json-serialize` | Convert ALB value to JSON string | `(json-serialize (dict "x" 1))` → `"{\"x\":1}"` |

### Arithmetic

| Function | Description | Example |
|----------|-------------|---------|
| `+` | Addition / string concat | `(+ 1 2 3)` → `6` |
| `-` | Subtraction / negate | `(- 10 3)` → `7`, `(- 5)` → `-5` |
| `*` | Multiplication | `(* 2 3 4)` → `24` |
| `/` | Division | `(/ 10 3)` → `3.333...` |
| `%` | Modulo | `(% 10 3)` → `1` |
| `^` | Power | `(^ 2 10)` → `1024` |

### Comparison

| Function | Description | Example |
|----------|-------------|---------|
| `==` | Equal | `(== "abc" "abc")` → `1` |
| `!=` | Not equal | `(!= 1 2)` → `1` |
| `<` `>` `<=` `>=` | Numeric comparison | `(< 3 5)` → `1` |

### Logic

| Function | Description | Example |
|----------|-------------|---------|
| `and` | Logical AND (short-circuit) | `(and true true)` → `1` |
| `or` | Logical OR (short-circuit) | `(or false true)` → `1` |
| `not` | Logical NOT | `(not false)` → `1` |

### Math Functions

| Function | Description | Example |
|----------|-------------|---------|
| `sqrt` | Square root | `(sqrt 16)` → `4` |
| `abs` | Absolute value | `(abs -5)` → `5` |
| `floor` | Floor | `(floor 3.7)` → `3` |
| `ceil` | Ceiling | `(ceil 3.2)` → `4` |
| `round` | Round | `(round 3.5)` → `4` |
| `sin` `cos` `tan` | Trig (radians) | `(sin (/ pi 2))` → `1` |
| `asin` `acos` `atan` | Inverse trig | |
| `atan2` | Two-arg arctangent | `(atan2 y x)` |
| `log` | Natural logarithm | `(log e)` → `1` |
| `log2` | Log base 2 | `(log2 8)` → `3` |
| `log10` | Log base 10 | `(log10 100)` → `2` |
| `exp` | Exponential e^x | `(exp 1)` → `2.718...` |
| `min` | Minimum | `(min 3 7)` → `3` |
| `max` | Maximum | `(max 3 7)` → `7` |
| `clamp` | Clamp to range | `(clamp 15 0 10)` → `10` |
| `random` | Random float [0, n) | `(random 100)` |
| `random-int` | Random int [a, b] | `(random-int 1 6)` |

### List Operations

| Function | Description | Example |
|----------|-------------|---------|
| `car` | First element | `(car (list 1 2 3))` → `1` |
| `cdr` | Rest of elements | `(cdr (list 1 2 3))` → `(2 3)` |
| `cons` | Prepend element | `(cons 0 (list 1 2))` → `(0 1 2)` |
| `nth` | Get by index | `(nth (list 10 20 30) 1)` → `20` |
| `push` | Append to end | `(push (list 1 2) 3)` → `(1 2 3)` |
| `range` | Generate sequence | `(range 5)` → `(0 1 2 3 4)` |
| `range` | Specify range | `(range 1 6)` → `(1 2 3 4 5)` |
| `range` | Specify step | `(range 0 10 2)` → `(0 2 4 6 8)` |

### Higher-Order Functions

| Function | Description | Example |
|----------|-------------|--------|
| `apply` | Call with list args | `(apply + (list 1 2 3))` → `6` |
| `map` | Map over list | `(map (func (list x) (* x x)) (list 1 2 3))` → `(1 4 9)` |
| `filter` | Filter list | `(filter (func (list x) (> x 2)) (list 1 2 3 4))` → `(3 4)` |
| `fold` | Fold list | `(fold (func (list acc x) (+ acc x)) (list 1 2 3) 0)` → `6` |
| `sort` | Sort list | `(sort (list 3 1 2))` → `(1 2 3)` |
| `reverse` | Reverse list/string | `(reverse (list 1 2 3))` → `(3 2 1)` |
| `zip` | Zip two lists | `(zip (list 1 2) (list "a" "b"))` → `((1 "a") (2 "b"))` |
| `unzip` | Unzip pairs | `(unzip (list (list 1 "a") (list 2 "b")))` |
| `any` | Any element matches | `(any (func (list x) (> x 5)) (list 1 6 3))` → `1` |
| `all` | All elements match | `(all (func (list x) (> x 0)) (list 1 2 3))` → `1` |
| `compose` | Function composition | `(let inc-then-double (compose (func (list x) (* x 2)) (func (list x) (+ x 1))))` |
| `pipe` | Function pipe | `(let double-then-inc (pipe (func (list x) (* x 2)) (func (list x) (+ x 1))))` |
| `partial` | Partial application | `(let add5 (partial + 5))` then `(add5 3)` → `8` |
| `for`/`each` | Iterate (aliases, same function) | `(for (list 1 2 3) (func (list x) (print x)))` |
| `flatten` | Flatten nested lists | `(flatten (list 1 (list 2 3) 4))` → `(1 2 3 4)` |
| `uniq` | Remove duplicates | `(uniq (list 1 2 2 3 1))` → `(1 2 3)` |
| `take` | Take first n | `(take 2 (list 1 2 3 4))` → `(1 2)` |
| `drop` | Drop first n | `(drop 2 (list 1 2 3 4))` → `(3 4)` |
| `copy` | Deep copy | `(copy val)` |

### Vector (v0.1.0)

| Function | Description | Example |
|----------|-------------|--------|
| `vector` | Create vector | `(vector 1 2 3)` → `#[1 2 3]` |
| `vector-ref` | O(1) access | `(vector-ref v 0)` → `1` |
| `vector-set!` | O(1) modify | `(vector-set! v 0 10)` |
| `vector-push!` | Append | `(vector-push! v 4)` |
| `vector-pop!` | Pop last | `(vector-pop! v)` |
| `vector-size` | Size | `(vector-size v)` |
| `vector->list` | Convert to list | `(vector->list v)` |
| `list->vector` | Convert from list | `(list->vector (list 1 2 3))` |

### Map Operations (v0.1.0)

| Function | Description | Example |
|----------|-------------|--------|
| `dict` | Create map | `(dict "a" 1 "b" 2)` → `{a: 1, b: 2}` |
| `map-get` | Get value | `(map-get m "key")` |
| `map-set` | Set value (new map) | `(map-set m "key" 42)` |
| `map-del` | Delete key | `(map-del m "key")` |
| `map-has?` | Key exists? | `(map-has? m "key")` → `1`/`0` |
| `map-keys` | All keys | `(map-keys m)` → list |
| `map-values` | All values | `(map-values m)` → list |
| `map-merge` | Merge maps | `(map-merge m1 m2)` |

### Regex (v0.1.0)

| Function | Description | Example |
|----------|-------------|--------|
| `re-match` | Match list or nil | `(re-match "(\\d+)" "abc123")` → `("123" "123")` |
| `re-find` | First match | `(re-find "\\d+" "abc123")` → `"123"` |
| `re-replace` | Replace all | `(re-replace "\\d" "a*b" "X")` → `"aXb"` |
| `re-split` | Split by regex | `(re-split "\\s+" "a  b   c")` → `("a" "b" "c")` |
| `re-test?` | Test match | `(re-test? "^\\d+$" "123")` → `1` |

### Bit Operations (v0.1.0)

| Function | Description | Example |
|----------|-------------|--------|
| `bit-and` | Bitwise AND | `(bit-and 5 3)` → `1` |
| `bit-or` | Bitwise OR | `(bit-or 5 3)` → `7` |
| `bit-xor` | Bitwise XOR | `(bit-xor 5 3)` → `6` |
| `bit-not` | Bitwise NOT | `(bit-not 0)` → `-1` |
| `bit-lshift` | Left shift | `(bit-lshift 1 4)` → `16` |
| `bit-rshift` | Right shift | `(bit-rshift 16 2)` → `4` |

### Time (v0.1.0)

| Function | Description | Example |
|----------|-------------|--------|
| `time-now` | Unix timestamp (sec) | `(time-now)` |
| `time-now-ms` | Timestamp (ms) | `(time-now-ms)` |
| `time-format` | Format timestamp | `(time-format (time-now) "%Y-%m-%d")` |
| `time-parse` | Parse time string | `(time-parse "2024-01-01" "%Y-%m-%d")` |
| `time-diff` | Difference in seconds | `(time-diff a b)` |
| `time-sleep` | Sleep (ms) | `(time-sleep 1000)` |
| `time-local` | Local time map | `(time-local)` → `{year:2024 month:1 ...}` |

### System (v0.1.0)

| Function | Description | Example |
|----------|-------------|--------|
| `exit` | Exit program | `(exit 0)` |
| `sleep` | Sleep (ms) | `(sleep 500)` |
| `system` | Run command, return code | `(system "dir")` |
| `exec` | Run command, return output | `(exec "echo hello")` → `"hello\n"` |

### Network (v0.1.0)

| Function | Description | Example |
|----------|-------------|--------|
| `net-connect` | TCP connect | `(net-connect "127.0.0.1" 8080)` |
| `net-send` | Send data | `(net-send sock "hello")` |
| `net-recv` | Receive data | `(net-recv sock 4096)` |
| `net-close` | Close socket | `(net-close sock)` |
| `net-listen` | TCP listen | `(net-listen 8080)` |
| `net-accept` | Accept connection | `(net-accept sock)` |
| `http-get` | HTTP GET | `(http-get "http://example.com")` |
| `http-post` | HTTP POST | `(http-post "http://api/data" "key=val")` |

### String Operations

| Function | Description | Example |
|----------|-------------|---------|
| `substr` | Substring | `(substr "hello" 1 3)` → `"ell"` |
| `str-split` | Split | `(str-split "a,b,c" ",")` → `("a" "b" "c")` |
| `str-join` | Join | `(str-join (list "a" "b") "-")` → `"a-b"` |
| `str-replace` | Replace | `(str-replace "hello" "l" "r")` → `"herro"` |
| `str-find` | Find position | `(str-find "hello" "ll")` → `2` |
| `str-trim` | Trim whitespace | `(str-trim "  hi  ")` → `"hi"` |
| `str-upper` | To uppercase | `(str-upper "hello")` → `"HELLO"` |
| `str-lower` | To lowercase | `(str-lower "HELLO")` → `"hello"` |
| `str-starts?` | Starts with? | `(str-starts? "hello" "he")` → `1` |
| `str-ends?` | Ends with? | `(str-ends? "hello" "lo")` → `1` |
| `str-repeat` | Repeat string | `(str-repeat "ab" 3)` → `"ababab"` |
| `str-reverse` | Reverse | `(str-reverse "hello")` → `"olleh"` |
| `char-code` | Char → code | `(char-code "A")` → `65` |
| `code-char` | Code → char | `(code-char 65)` → `"A"` |

### File & Directory

| Function | Description | Example |
|----------|-------------|---------|
| `file-read` | Read file content | `(file-read "data.txt")` |
| `file-write` | Write to file | `(file-write "out.txt" "hello")` |
| `file-append` | Append to file | `(file-append "log.txt" "line\n")` |
| `file-exists?` | File exists? | `(file-exists? "a.txt")` → `1`/`0` |
| `file-delete` | Delete file | `(file-delete "tmp.txt")` |
| `file-lines` | Read as lines | `(file-lines "data.txt")` → list |
| `dir-list` | List directory | `(dir-list ".")` → list |
| `dir-exists?` | Directory exists? | `(dir-exists? "lib")` → `1`/`0` |
| `dir-create` | Create directory | `(dir-create "output")` |
| `path-join` | Join path parts | `(path-join "a" "b" "c.txt")` → `"a/b/c.txt"` |
| `path-ext` | Get extension | `(path-ext "file.txt")` → `"txt"` |
| `path-name` | Get filename (no ext) | `(path-name "a/b/file.txt")` → `"file"` |

### Serialization

| Function | Description | Example |
|----------|-------------|---------|
| `write` | Value → S-expr string | `(write (list 1 "hi"))` |
| `read-sexpr` | S-expr string → value | `(read-sexpr "(1 2 3)")` → `(1 2 3)` |

### Error Handling

| Function | Description | Example |
|----------|-------------|---------|
| `throw` | Raise exception | `(throw "Something went wrong")` |
| `error` | Create error object | `(error "TypeError" "Invalid type")` |

---

## Standard Libraries

ALB provides 5 standard library modules, imported via `(use "module-name")`.

### math — Math Library

```lisp
(use "math")
```

Provides: `PI` `E` `TAU` `PHI` `INF`, `lerp`, `inverse-lerp`, `remap`, `deg->rad`, `rad->deg`, `rand-float`, `rand-int`, `rand-bool`, `sign`, `lerp-angle`, `vec2-dot`, `vec2-len`, `vec2-dist`, `vec2-norm`

### string — String Library

```lisp
(use "string")
```

Provides: `str-contains?`, `str-empty?`, `str-blank?`, `str-count`, `str-at`, `str-take`, `str-drop`, `str-sub`, `str-slice`, `str-capitalize`, `str-title`, `str-pad-left`, `str-pad-right`, `str-center`, `str-chars`, `str-from-chars`, `words`, `lines`

### io — IO Utilities Library

```lisp
(use "io")
```

Provides: `file-copy`, `file-size`, `file-empty?`, `file-backup`, `file-touch`, `file-write-lines`, `file-filter-lines`, `file-map-lines`, `dir-ensure`, `dir-files`, `dir-dirs`, `dir-files-recursive`, `path-base`, `path-dir`, `path-change-ext`, `path-absolute?`, `csv-parse`, `csv-format`

### gui — GUI Library

```lisp
(use "gui")
```

Provides a complete graphical user interface: windows, widgets, layouts, menus, toolbars, and more.

**GUI Example:**
```lisp
(use "gui")

; Define button callback
(let on-click (func (list wid etype data extra)
  (msgbox "Button clicked!" "Info")))

; Create window
(gui-window "My App" 400 300)

; Add controls
(let btn (gui-button 0 "Click Me" "on-click"))
(gui-pack btn :side "top" :fill "x" :pad 10)

; Start event loop
(gui-run 1)
```

**GUI Widget Reference:**

| Function | Description |
|----------|-------------|
| `gui-window` | Create main window |
| `gui-label` | Label |
| `gui-button` | Button |
| `gui-entry` | Single-line input |
| `gui-text` | Multi-line text |
| `gui-checkbutton` | Checkbox |
| `gui-radiobutton` | Radio button |
| `gui-frame` | Frame container |
| `gui-canvas` | Canvas |
| `gui-combobox` | Dropdown |
| `gui-listbox` | Listbox |
| `gui-treeview` | Tree view |
| `gui-notebook` | Tab notebook |
| `gui-progressbar` | Progress bar |
| `gui-scale` | Slider |
| `gui-menubar` | Menu bar |
| `gui-toolbar` | Toolbar |
| `gui-statusbar` | Status bar |
| `gui-pack` / `gui-grid` / `gui-place` | Layout managers |
| `gui-run` | Start event loop |
| `msgbox` | Message dialog |
| `gui-choose-file` | File chooser dialog |

### agame — 2D Game & Interactive Animation Library

```lisp
(use "agame")
```

SDL2-based 2D game engine providing window management, graphics rendering, input handling, image loading, text rendering, and audio playback.

**Dependencies:**
- **Windows:** Download [SDL2 development libraries](https://libsdl.org), link with `-lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer`
- **Linux:** `sudo apt-get install libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev`
- **macOS:** `brew install sdl2 sdl2_image sdl2_ttf sdl2_mixer`

**Agame Example — Moving Ball:**
```lisp
(use "agame")

; Initialize window
(agame-init "Moving Ball" 640 480)

; Initial position
(let x 320)
(let y 240)
(let speed 5)

; Main game loop
(while (not (agame-should-quit))
  ; Clear screen
  (agame-set-color (list 0 0 0 255))
  (agame-clear)
  
  ; Draw ball
  (agame-set-color (list 255 100 100 255))
  (agame-fill-circle x y 20)
  
  ; Handle keyboard input
  (when (agame-key-pressed "left")  (set x (- x speed)))
  (when (agame-key-pressed "right") (set x (+ x speed)))
  (when (agame-key-pressed "up")    (set y (- y speed)))
  (when (agame-key-pressed "down")  (set y (+ y speed)))
  
  ; Boundary check
  (when (< x 20) (set x 20))
  (when (> x 620) (set x 620))
  (when (< y 20) (set y 20))
  (when (> y 460) (set y 460))
  
  ; Present frame
  (agame-present)
  (agame-delay 16))

; Cleanup
(agame-quit)
```

**Agame Function Reference:**

| Function | Description |
|----------|-------------|
| **Window Management** | |
| `agame-init` | Initialize window `(agame-init "title" width height)` |
| `agame-quit` | Close window and release resources |
| `agame-clear` | Clear screen |
| `agame-present` | Present frame |
| `agame-delay` | Delay in milliseconds |
| `agame-window-width` | Get window width |
| `agame-window-height` | Get window height |
| `agame-set-title` | Set window title |
| **Color** | |
| `agame-set-color` | Set current color `(agame-set-color (list r g b a))` |
| **Drawing** | |
| `agame-draw-point` | Draw point |
| `agame-draw-line` | Draw line |
| `agame-draw-rect` | Draw rectangle outline |
| `agame-fill-rect` | Fill rectangle |
| `agame-draw-circle` | Draw circle |
| `agame-fill-circle` | Fill circle |
| `agame-draw-triangle` | Draw triangle |
| `agame-fill-triangle` | Fill triangle |
| **Input** | |
| `agame-poll-event` | Poll events |
| `agame-should-quit` | Check if should quit |
| `agame-key-pressed` | Check if key is pressed |
| `agame-mouse-x` / `agame-mouse-y` | Mouse coordinates |
| `agame-mouse-pressed` | Mouse button state |
| **Images** | |
| `agame-load-image` | Load image |
| `agame-draw-image` | Draw image |
| `agame-free-image` | Free image |
| **Text** | |
| `agame-load-font` | Load font |
| `agame-draw-text` | Draw text |
| `agame-free-font` | Free font |
| **Audio** | |
| `agame-load-sound` | Load sound |
| `agame-play-sound` | Play sound |
| `agame-free-sound` | Free sound |
| **Constants** | |
| `agame-black` `agame-white` `agame-red` ... | Predefined colors |
| `agame-key-up` `agame-key-down` ... | Key constants |

---

## Compiling to Executable

ALB supports compiling `.alb` scripts into standalone macOS executables (Mach-O binary).

### Command-Line Compilation

```bash
# Normal compile (requires lib/ at runtime)
ALB -b script.alb -o script

# Embedded compile (libraries bundled into binary, fully standalone)
ALB -b script.alb --embed -o script
```

### IDE Compilation

- **F7** — Build Executable (normal compile)
- **F8** — Build with Libraries (embedded compile, all libs bundled in)

### Command-Line Help

```bash
ALB -h              # Show help
ALB -h --list       # List all built-in functions
ALB -h print        # View help for print function
ALB -h gui          # View help for gui library
```

---

## Building from Source (macOS)

Requires Xcode Command Line Tools:
```bash
xcode-select --install
```

Build:
```bash
make          # Compile ALB binary
make app      # Build ALB.app bundle
make dmg      # Build DMG installer (includes ALB.app + ALB_IDE.app + docs)
make install  # Install ALB to /usr/local/bin
make clean    # Clean build artifacts
```

---

## IDE Usage

The ALB IDE is a lightweight editor built with Python + Tkinter, featuring:

- Multi-tab editing
- Syntax highlighting
- Auto-completion (trigger with `Ctrl+Space`)
- Function signature hints (auto-shows after typing `(`)
- Smart indentation (auto-aligns after Enter based on parenthesis depth)
- Project management (`.albprj` files)
- One-click compile & run (F5 to run, F7/F8 to compile)

---

## Example Programs

### Number Guessing Game

```lisp
(let target (random-int 1 100))
(let attempts 0)
(print "Guess a number between 1 and 100!")

(while true
  (let guess (num (input "Your guess: ")))
  (+= attempts 1)
  (if (== guess target)
    (do
      (print (str "Congrats! You got it in " attempts " tries!"))
      (break))
    (if (< guess target)
      (print "Too low!")
      (print "Too high!"))))
```

### File Processing

```lisp
(use "io")

; Read and process a CSV file line by line
(let lines (file-lines "data.csv"))
(each line lines
  (let parts (str-split line ","))
  (print (str (nth parts 0) ": " (nth parts 1))))
```

### Functional Programming

```lisp
; Sum a list
(let nums (list 1 2 3 4 5))
(let total (fold + nums 0))    ; => 15

; Filter even numbers
(let evens (filter (func (list x) (== (% x 2) 0)) nums))  ; => (2 4)

; Map squares
(let squares (map (func (list x) (* x x)) nums))  ; => (1 4 9 16 25)
```

---

*ALB v0.1.1 — A concise, practical Lisp-dialect scripting language*
