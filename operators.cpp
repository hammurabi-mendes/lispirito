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
    "vm-call",
    "vm-cond",
    "vm-logic",
    "vm-define",
    "vm-begin",
    "vm-apply",
    "vm-eval-list",
    "vm-eval"
};