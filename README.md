# lispirito

A portable LISP implementation for memory-constrained systems. It works from MOS 6502 to modern ARM/Intel 64-bit machines.

## Project Goals

- Binary size smaller than 31.5K on MOS 6502, yet capable with modern architectures.
- Macro expansion support for syntatic sugar.
- Depend on a minimal set of `libc` functions.
- The code should be small and pedagogical, *easy to understand*.

## Notably missing features

- Although a good subset of R7RS-small functions are covered, many are left out due to space (notably vectors and maps). It should be fairly straightfoward to extend the implementation to add them, but that would increase the footprint past our size goal of 31.5K.

## Getting started

If you are compiling **lispirito** in a modern system, just a simple `make clean; make install` should work.

To include debugging, add `-DDEBUG=1` to your build command.
To include some standard lambdas and macros, add `-DINITIAL_ENVIROMENT=1` to your build command. Make sure you have heap memory for this! If you are on 6502 systems,
you likely/unfortunately do not. In that case, you can either:

- Type the definitions you want in the REPL, maximally saving space; or
- Edit the `lambdas.h` and `macros.h` files to include only the definitions you need.

If you are compiling **lispirito** for MOS 6502 (in particular Ben Eater's machine), first download the LLVM-MOS SDK
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

You can change the default definitions of Integral and Real in `extra.h` to best accomodate the build to your system.

Have fun!
