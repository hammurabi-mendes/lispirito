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
    "string-length",
    "string-append",
    "string-ref",
    "string-set!",
    "make-string",
    "substring",

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
    "read",
    "write",
    "current-environment"
};