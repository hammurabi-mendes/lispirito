# Lispirito Tutorial

Welcome to Lispirito: ["quicker than a turtle, stronger than a mouse"](https://en.wikipedia.org/wiki/El_Chapul%C3%ADn_Colorado).

## Built-in Operators

These operators are always available — it is not necessary to load any definitions.

### McCarthy Core

```scheme
(quote x)          ; returns x unevaluated — shorthand: 'x
(car '(10 20 30))  ; => 10
(cdr '(10 20 30))  ; => (20 30)
(cons 10 '(20 30)) ; => (10 20 30)
(atom? 10)         ; => #t  (numbers, booleans, strings, and characters are atoms)
(atom? '(10 20))   ; => #f
(eq? 10 10)        ; => #t
(eq? 10 20)        ; => #f
```

`cond` evaluates each test in order and returns the consequent of the first passed test. Use `#t` as a trivially-passing test, serving the role of "else".

```scheme
(cond
    ((eq? 10 20) 'no)
    (#t 'yes))   ; => yes
```

### Association and Substitution

`assoc` looks up a key in an association list (a list of `(key value)` pairs):

```scheme
(define env '((a 10) (b 20) (c 30)))
(assoc 'b env)   ; => 20
(assoc 'd env)   ; => #f
```

`subst` replaces every occurrence of an atom in an expression with another:

```scheme
(subst 'x 99 '(x + x))   ; => (99 + 99)
```

### Type Predicates

```scheme
(null? '())        ; => #t  — the empty list
(null? '(10))      ; => #f
(pair? '(10 20))   ; => #t  — a non-empty list
(pair? '())        ; => #f
(atom? 10)         ; => #t
(char? #\a)        ; => #t
(boolean? #t)      ; => #t
(string? "Chaves") ; => #t
(number? 10)       ; => #t
(integer? 10)      ; => #t
(real? 3.14)       ; => #t
```

### Type Conversions

```scheme
(integer->real 10)          ; => 10.0
(real->integer 3.14)        ; => 3
(integer->char 65)          ; => #\A
(char->integer #\A)         ; => 65
(number->string 42)         ; => "42"
(string->number "42")       ; => 42
```

`string->data` and `data->string` reinterpret a string node as a raw memory block and vice versa.

### Arithmetic

All arithmetic operators are binary. For n-ary operations, use `foldl` or `foldr`:

```scheme
(+ 10 20)                ; => 30
(- 30 10)                ; => 20
(* 10 20)                ; => 200
(/ 30 10)                ; => 3
(foldl + '(10 20 30 40)) ; => 100
(foldl * '(10 20 30))    ; => 6000
```

Mixing an integer and a real promotes the integer:

```scheme
(+ 10 0.5)   ; => 10.5
```

### Arithmetic Comparisons

```scheme
(< 10 20)    ; => #t
(= 20 20)    ; => #t
(> 30 20)    ; => #t
(<= 10 10)   ; => #t
(>= 30 20)   ; => #t
```

### Logical Operators

`and` and `or` are n-ary, short-circuit, and tail-recursive. Zero-ary `and` returns `#t` (the identity for conjunction) and zero-ary `or` returns `#f` (the identity for disjunction):

```scheme
(and)               ; => #t
(or)                ; => #f
(and #t #t)         ; => #t
(and #t #f)         ; => #f
(and #t #t #t #t)   ; => #t
(or #f #t)          ; => #t
(or #f #f)          ; => #f
(or #f #f #t #f)    ; => #t
(not #f)            ; => #t
(not #t)            ; => #f
```

### Environment and Control

`define` adds a binding to the current environment:

```scheme
(define x 10)
(define (square n) (* n n))
(square 10)   ; => 100
```

`set!` updates an existing binding without extending the environment:

```scheme
(define counter 0)
(set! counter 10)
counter   ; => 10
```

`begin` evaluates a sequence of expressions and returns the last one. Defines inside `begin` are lexically scoped to it, as required by Scheme standards:

```scheme
(begin
    (define a 10)
    (define b 20)
    (+ a b))   ; => 30
```

`let` and `let*` bind local variables. `let` evaluates all right-hand sides in the environment *before* the `let`, so bindings cannot refer to each other. `let*` threads each binding into the next, so later bindings can see earlier ones:

```scheme
(define a 100)

(let ((a 10) (b a))   ; b sees the outer a (100), not the new a (10)
    b)   ; => 100

(let* ((a 10) (b a))  ; b sees the new a (10)
    b)   ; => 10

(let* ((a 10) (b (* a 2)))
    (+ a b))   ; => 30
```

`lambda` creates a closure. Closures capture their defining environment:

```scheme
(define (make-adder n)
    (lambda (x) (+ x n)))

(define add10 (make-adder 10))
(add10 20)   ; => 30
```

Variadic lambdas collect remaining arguments into a list using `.`:

```scheme
(define (sum . nums)
    (foldl + 0 nums))
(sum 10 20 30)   ; => 60
```

`macro` creates a macro — its arguments are substituted unevaluated into the body before evaluation. The built-in `if` (from `lib-utils.lsp`) is defined this way:

```scheme
(define if (macro (test if_clause else_clause)
    (cond (test if_clause) (#t else_clause))))

(if (= 10 10) 'equal 'different)   ; => equal
```

`eval` evaluates an expression in a given environment:

```scheme
(eval '(+ 10 20) (current-environment))   ; => 30
```

`current-environment` returns the environment as a first-class value (an association list of `(name value)` pairs), which you can pass to `eval` or inspect with `assoc`.

`apply` treats the last argument as a list of additional arguments:

```scheme
(apply + '(10 20 30))   ; => 60
```

### Display

```scheme
(display "Chaves")   ; prints: Chaves
(display 10)         ; prints: 10
(newline)            ; prints a newline character
```

### I/O

These operators are available on modern systems and on the Commodore 64. They let you work with files as first-class values:

```scheme
(define fd (open "data.txt" "r"))    ; open for reading; returns a file descriptor
(read fd)                            ; read and parse one expression from fd
(close fd)                           ; close the file descriptor

(define fd2 (open "data.txt" "w"))   ; open for writing; returns a file descriptor
(write "Seu Madruga" fd2)            ; write a value to out
(close fd2)                          ; close the file descriptor
```

`read` called on `stdin` reads from the interactive prompt. It returns `'()` at end of file.

`load!` redirects the interpreter's input to a file descriptor opened in `"r"` mode. It reads and evaluates expressions from that fd until end of file, then automatically reverts to the previous input source. To explicitly restore interactive input, pass any non-data-object value such as `'stdin`:

```scheme
(define fd (open "lib-list.lsp" "r"))
(load! fd)     ; evaluates lib-list.lsp, then reverts to previous input
(close fd)

(load! 'stdin) ; explicitly switch back to interactive input
```

`save!` redirects the interpreter's output to a file descriptor opened in `"w"` mode. Pass any non-data-object value such as `'stdout` to restore normal output:

```scheme
(define out (open "result.txt" "w"))
(save! out)    ; subsequent output goes to result.txt
(display 42)
(save! 'stdout) ; restore output to the terminal
(close out)
```

### Low-Level Memory

These operators give you direct access to memory, C-style. They are used by the string library internally and are useful for embedding data structures in raw memory:

```scheme
(define buf (mem-alloc 16))             ; allocate 16 bytes; returns a data pointer
(mem-fill buf (char->integer #\0) 16)   ; fill with '0' characters
(mem-addr buf)                          ; get the numeric address of buf
(mem-write (mem-addr buf) 65)           ; write byte 65 ('A') at that address
(mem-read (mem-addr buf))               ; => 65
(mem-copy dest src n)                   ; copy n bytes from src address to dest address
```

---

## Loading the Standard Libraries

Lispirito ships its standard library as a set of `.lsp` files. There are two ways to get them into your session.

**By pasting.** Open the file in a text editor, copy its contents, and paste them at the `>` prompt. This always works, even on bare-metal 6502 platforms.

**By using `load!`.** Open a file with `open` in `"r"` mode, pass the descriptor to `load!`, then close it:

```scheme
(define fd (open "lib-list.lsp" "r"))
(load! fd)
(close fd)
```

The recommended load order is: `lib-funct.lsp`, `lib-list.lsp`, `lib-utils.lsp`, `lib-assoc.lsp`, `lib-math.lsp`, `lib-strings.lsp`, `lib-streams.lsp`.

> **Note:** `load!` reads and evaluates each expression from the file in your current environment. Any `define` forms in the file extend the global environment, exactly as if you had typed them yourself.

---

## lib-funct.lsp

Load with:

```scheme
(define fd (open "lib-funct.lsp" "r"))
(load! fd)
(close fd)
```

### map

Applies a function to every element of a list:

```scheme
(map (lambda (x) (* x x)) '(10 20 30))   ; => (100 400 900)
```

### foldl

Left fold — reduces a list from left to right, accumulating a result. The binary function receives `(element accumulator)`:

```scheme
(foldl + 0 '(10 20 30))          ; => 60
(foldl cons '() '(10 20 30))     ; => (30 20 10)
```

### foldr

Right fold — reduces from right to left. The binary function receives `(element accumulator)`:

```scheme
(foldr cons '() '(10 20 30))     ; => (10 20 30)
(foldr + 0 '(10 20 30))          ; => 60
```

### filter

Keeps only the elements for which a predicate returns `#t`:

```scheme
(filter (lambda (x) (> x 20)) '(10 20 30 40 50))   ; => (30 40 50)
```

---

## lib-list.lsp

Depends on `lib-funct`. Load with:

```scheme
(define fd (open "lib-list.lsp" "r"))
(load! fd)
(close fd)
```

### length

```scheme
(length '(10 20 30))   ; => 3
(length '())           ; => 0
```

### reverse

```scheme
(reverse '(10 20 30))   ; => (30 20 10)
```

### append

Concatenates two lists:

```scheme
(append '(10 20) '(30 40 50))   ; => (10 20 30 40 50)
```

### list

Constructs a list from its arguments (variadic):

```scheme
(list 10 20 30)   ; => (10 20 30)
(list)            ; => ()
```

### flatten

Recursively flattens a nested list:

```scheme
(flatten '(10 (20 30) (40 (50 60))))   ; => (10 20 30 40 50 60)
```

### list?

Returns `#t` if the value is a proper list (including the empty list):

```scheme
(list? '(10 20 30))   ; => #t
(list? '())           ; => #t
(list? 10)            ; => #f
```

---

## lib-utils.lsp

Load with:

```scheme
(define fd (open "lib-utils.lsp" "r"))
(load! fd)
(close fd)
```

### if

`if` is a macro that expands to `cond`:

```scheme
(if (= 10 10) 'yes 'no)           ; => yes
(if (> 10 20) 'bigger 'smaller)   ; => smaller
```

---

## lib-assoc.lsp

Depends on `lib-funct` and `lib-utils`. Load with:

```scheme
(define fd (open "lib-assoc.lsp" "r"))
(load! fd)
(close fd)
```

### assoc-replace

Returns a new association list with the value for `key` replaced:

```scheme
(assoc-replace 'b 99 '((a 10) (b 20) (c 30)))   ; => ((a 10) (b 99) (c 30))
```

### assoc-delete

Returns a new association list with the entry for `key` removed:

```scheme
(assoc-delete 'b 20 '((a 10) (b 20) (c 30)))   ; => ((a 10) (c 30))
```

---

## lib-math.lsp

Depends on `lib-utils`. Load with:

```scheme
(define fd (open "lib-math.lsp" "r"))
(load! fd)
(close fd)
```

### abs

```scheme
(abs 10)         ; => 10
(abs (- 0 30))   ; => 30
```

### modulo

```scheme
(modulo 30 20)   ; => 10
(modulo 50 30)   ; => 20
```

---

## lib-strings.lsp

Depends on `lib-funct` and `lib-list`. Load with:

```scheme
(define fd (open "lib-strings.lsp" "r"))
(load! fd)
(close fd)
```

The examples use names from the cast of *Chaves* (known as *El Chavo del 8* in Spanish):

### make-string

Creates a new string of a given length, filled with a character:

```scheme
(make-string 5 #\-)   ; => "-----"
```

### string-length

```scheme
(string-length "Chiquinha")   ; => 9
(string-length "Kiko")        ; => 4
```

### string-ref

Returns the character at a zero-based index:

```scheme
(string-ref "Chaves" 0)   ; => #\c  (note: input is lowercased at the REPL)
(string-ref "Quico" 1)    ; => #\u
```

### string-set!

Modifies a character in place:

```scheme
(define name "Chaves")
(string-set! name 0 #\K)
name   ; => "Khaves"
```

### string-append

```scheme
(string-append "Seu " "Madruga")     ; => "Seu Madruga"
(string-append "Dona " "Florinda")   ; => "Dona Florinda"
```

### substring

Returns a slice from `start` up to and including `finish`:

```scheme
(substring "Professor Girafales" 0 8)   ; => "Professor"
(substring "Nhonho" 2 5)                ; => "onh"
```

### string->list

Converts a string to a list of characters:

```scheme
(string->list "Kiko")   ; => (#\k #\i #\k #\o)
```

### list->string

Converts a list of characters back to a string:

```scheme
(list->string '(#\C #\h #\a #\v #\e #\s))   ; => "Chaves"
```

---

## lib-streams.lsp

Load with:

```scheme
(define fd (open "lib-streams.lsp" "r"))
(load! fd)
(close fd)
```

Streams are lazy sequences: the tail of a stream is not evaluated until it is forced. This lets you work with sequences that are conceptually infinite. Refer to [test_streams.lsp](test_streams.lsp) for an usage example.

### delay and force

`delay` wraps an expression in a thunk without evaluating it. `force` evaluates the thunk:

```scheme
(define p (delay (+ 10 20)))   ; not evaluated yet
(force p)                      ; => 30
```

### stream-cons, stream-car, stream-cdr

`stream-cons` constructs a stream whose tail is delayed. `stream-car` returns the head; `stream-cdr` forces and returns the tail:

```scheme
(define s (stream-cons 10 (stream-cons 20 '())))
(stream-car s)         ; => 10
(stream-car (stream-cdr s))   ; => 20
```

### stream-range

Produces a stream of integers from `low` to `high` inclusive:

```scheme
(stream-range 1 5)   ; => (1 . #<thunk>)  — a lazy sequence 1 2 3 4 5
```

### stream-map

Applies a function to every element of a stream, lazily:

```scheme
(define evens (stream-map (lambda (x) (* x 2)) (stream-range 1 5)))
(stream-car evens)   ; => 2
```

### stream-filter

Keeps only the elements for which a predicate returns `#t`, lazily:

```scheme
(define odds (stream-filter odd? (stream-range 1 10)))
(stream-car odds)   ; => 1
```

### stream-and and stream-or

Short-circuit tests over a stream. `stream-and` returns `#t` if all elements are truthy; `stream-or` returns `#t` if any element is truthy:

```scheme
(stream-and (stream-range 1 5))   ; => #t  (all non-zero)
(stream-or  (stream-range 0 5))   ; => #t  (1 is truthy)
```

### stream-foreach

Applies a function to every element for its side effects:

```scheme
(stream-foreach display (stream-range 1 3))   ; prints: 123
```

### stream-yield

Applies a function to the first `n` elements of a stream for its side effects:

```scheme
(stream-yield display 3 (stream-range 10 99))   ; prints: 101112
```

---

> **Platform note:** `load!` (and the underlying `open`/`read`/`close` operators) requires I/O support compiled in: use `make clean; make TARGET_LIBC_IO=1`. On Commodore 64, use `make clean; make TARGET_6502=1 TARGET_C64=1`. On 6502 targets without a filesystem use `make clean; make TARGET_6502=1` and paste the library contents at the prompt instead.

---

## Recommended Load Order

Load them in this order:

```scheme
(define fd (open "lib-funct.lsp" "r"))
(load! fd)
(close fd)

(define fd (open "lib-list.lsp" "r"))
(load! fd)
(close fd)

(define fd (open "lib-utils.lsp" "r"))
(load! fd)
(close fd)

(define fd (open "lib-assoc.lsp" "r"))
(load! fd)
(close fd)

(define fd (open "lib-math.lsp" "r"))
(load! fd)
(close fd)

(define fd (open "lib-strings.lsp" "r"))
(load! fd)
(close fd)

(define fd (open "lib-streams.lsp" "r"))
(load! fd)
(close fd)
```

Have fun!
