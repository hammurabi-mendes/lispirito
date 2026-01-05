# Lispirito

A portable LISP implementation for memory-constrained systems. It works from MOS 6502 to modern ARM/Intel 64-bit machines.

## Project goals

- Binary size smaller than 31.5K on MOS 6502 (on a minimal build), yet capable with modern architectures.
- Macro expansion support for syntactic sugar.
- Depend on a minimal set of `libc` functions.
- The code should be small, portable, and pedagogical, *easy to understand*.
- The code prioritizes the reduction of binary image and making evident the evaluator's meta-circular property instead of performance.

This project is meant as a "software continuation" of a 6502 breadboard computer (such as the projects discussed in the 6502 Forum and Ben Eater's machine).
Lispirito indents to show how to overcome limitations inherent to the 6502 such as the 256-byte hardware stack (with function frames in the heap), how to implement
garbage collection and recycling using reference counting, how to overcome the problem that closures refer to their own environment and could create a reference counting loop,
and other similar implementation issues.

In addition -- and importantly -- the code should make evident the intricate the so-called meta-circular relationship between the `eval` (Evaluation) 
and `apply` (Function Application) subroutines in the code. **Hence, the interpreter is written in a way to not trade clarity for performance:** the code should be pedagogical and stylish. There are always rough edges to be revised, but as much thought will be given to style and clarity as to efficiency.

## Supported features

We support a a good subset of the Scheme R7RS-small specification:

- "McCarthy" operators: `quote`, `car`, `cdr`, `atom?`, `eq?`, `cons`, `cond`, `lambda`, `eval`, `define`
- Association and substitution: `assoc`, `subst`
- Type support:
    - `pair?`, `char?`, `boolean?`, `string?`, `number?`, `integer?`, `real?`
    - `integer->real`, `real->integer`, `integer->char`, `char->integer`, `number->string`, `string->number`
- Arithmetic operators: `+`, `-`, `*`, `/`
    - If you want an n-ary operator, use `apply` together with the operators above
- Arithmetic comparison operators: `<`, `=`, `>`, `<=`, `>=`
- Logical operators: `and`, `or`, `not`
    - If you want an n-ary `and`/`or`, use `apply` together with `and`/`or`
- Environment and macro support: `begin`, `set!`, `macro`, `read`, `write`, `current-environment`
- Low-level memory operations (C-style): `mem-alloc`, `mem-read`, `mem-write`, `mem-fill`, `mem-copy`, `mem-addr`

If you compile with `INITIAL_ENVIRONMENT=1`, you can use many of the expected functions like `map`, `filter` by loading them with `(load 'map)`, `(load 'filter)`, etc. Alternatively, you can **download the minimal release and type/paste the definitions of the functions in  [environment.lsp](environment.lsp).** All functions are still available in the minimal release, you just have to type/paste them from [environment.lsp](environment.lsp).
  - Functional operators: `map`, `foldl`, `foldr`, `filter`
  - List operations: `length`, `reverse`, `append`, `list`, `list?`
  - Other arithmetic operators: `abs`, `modulo`
  - String support: `list->string`, `string->list`, `string-length`, `string-append`, `string-ref`, `string-set!`, `make-string`, `substring`
  - Display support: `display`, `newline`
  - Function application operator: `apply`
  - Scope and control operators: `if`, `let`
  
Lambda definitions create *closures*, and `cond`, `and/or`, and `begin` are all tail-recursive (just make sure to recur in [tail-position](https://en.wikipedia.org/wiki/Tail_call)).

## Notably missing features

- Although a good subset of the Scheme R7RS-small specification is covered, many operators or data structures are left out due to space (notably vectors and maps). It should be fairly straightforward to extend the implementation to add them, but that would increase the footprint past our size goal of 31.5K.

## Future plans

- In the next versions, I plan to include `call/cc` and `let*` (for now you can use `begin` and `define`).
- I will also make versions for DOS, Amiga, and (hopefully) BSD 2.11 on a PDP-11. Building and running on modern systems should already be trivial.

## Building

If you are building **Lispirito** in a modern system, just a simple `make clean; make install` should work.
To include debugging, use `make DEBUG=1` as your build command.

If you are building for 6502 platforms, use `make clean; make TARGET_6502=1`. To include some standard lambdas and macros, use `make clean; make TARGET_6502=1 INITIAL_ENVIROMENT=1` as your build command. Make sure you have heap memory for this! If you do not, you can exclude the initial environment and:

- Type the definitions you want in the REPL, maximally saving space; or
- Edit the `lambdas.h` and `macros.h` files to include only the definitions you need.

If you are compiling **Lispirito** for MOS 6502 (in particular Ben Eater's machine), first download the LLVM-MOS SDK
in the link below, and place it alongside this project directory. You might want to adjust the `CXX` location in your
Makefile depending on where your SDK is. You can find the SDK here:

- [Linux](https://github.com/llvm-mos/llvm-mos-sdk/releases/latest/download/llvm-mos-linux.tar.xz)
- [Mac OS](https://github.com/llvm-mos/llvm-mos-sdk/releases/latest/download/llvm-mos-macos.tar.xz)
- [Windows](https://github.com/llvm-mos/llvm-mos-sdk/releases/latest/download/llvm-mos-windows.7z)

If you are using `macOS`, make sure to remove the quarantine of the file before extracting it.

```shell
$ xattr -d com.apple.quarantine llvm-mos-macos.tar.xz
```

## Tweaks

You can change the default definitions of Integral and Real in `extra.h` to best accommodate the build to your system.

Have fun!
