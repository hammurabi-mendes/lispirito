#include "operators.h"

const char *operator_names[] = {
    // McCarthy
    "quote",
    "car",
    "cdr",
    "atom?",
    "eq?",
    "cons",
    "cond",

    // Association and substitution
    "assoc",
    "subst",

    // Type support
    "null?",
    "pair?",
    "char?",
    "boolean?",
    "string?",
    "number?",
    "integer?",
    "real?",
    "integer->real",
    "real->integer",
    "integer->char",
    "char->integer",
    "number->string",
    "string->number",
    "string->data",
    "data->string",

    // Display support
    "display",
    "newline",

    // Arithmetic
    "+",
    "-",
    "*",
    "/",

    // Arithmetic comparison
    "<",
    "=",
    ">",
    "<=",
    ">=",

    // Logic
    "and",
    "or",
    "not",

    // Environment and Lambda support
    "begin",
    "define",
    "set!",
    "eval",
    "lambda",
    "macro",
    "closure",
    "apply",
    "read",
    "write",
    "current-environment",

    // Low-level memory handling
    "mem-alloc",
    "mem-read",
    "mem-write",
    "mem-fill",
    "mem-copy",
    "mem-addr",

    // Dynamic definition load/unload
    "load",
    "unload",

    // VM operators
    "vm-first",
    "vm-normal",
    "vm-quote",
    "vm-cond",
    "vm-logic",
    "vm-define",
    "vm-begin",
    "vm-apply",
    "vm-eval",
    "vm-load",
    "vm-call",
    "vm-eval-list"
};

ReduceMode operator_reduce_modes[] = {
    // McCarthy
    SpecialQuote,
    Normal1,
    Normal1,
    Normal1,
    Normal2,
    Normal2,
    SpecialCond,

    // Association and substitution
    Normal2,
    Normal3,

    // Type support
    Normal1,
    Normal1,
    Normal1,
    Normal1,
    Normal1,
    Normal1,
    Normal1,
    Normal1,
    Normal1,
    Normal1,
    Normal1,
    Normal1,
    Normal1,
    Normal1,
    Normal1,
    Normal1,

    // Display support
    Normal1,
    Normal0,

    // Arithmetic
    Normal2,
    Normal2,
    Normal2,
    Normal2,

    // Arithmetic comparison
    Normal2,
    Normal2,
    Normal2,
    Normal2,
    Normal2,

    // Logic
    SpecialLogic,
    SpecialLogic,
    Normal1,

    // Environment and Lambda support
    SpecialBegin,
    SpecialDefine,
    SpecialDefine,
    SpecialEval,
    ImmediateLambda,
    ImmediateMacro,
    ImmediateClosure,
    NormalX,
    Normal0,
    Normal1,
    Normal0,

    // Low-level memory handling
    Normal1,
    Normal1,
    Normal2,
    Normal3,
    Normal3,
    Normal1,

    // Dynamic definition load/unload
    SpecialLoad,
    SpecialLoad,

    // VM operators
    VM,
    VM,
    VM,
    VM,
    VM,
    VM,
    VM,
    VM,
    VM,
    VM,
    VM,
    VM
};