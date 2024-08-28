#ifndef OPERATORS_H
#define OPERATORS_H

enum LispOperation {
    OP_QUOTE,
    OP_CAR,
    OP_CDR,
    OP_ATOM_Q,
    OP_EQ_Q,
    OP_CONS,
    OP_COND,

    OP_ASSOC,
    OP_SUBST,

    OP_PAIR_Q,
    OP_CHAR_Q,
    OP_BOOLEAN_Q,
    OP_STRING_Q,
    OP_NUMBER_Q,
    OP_INTEGER_Q,
    OP_REAL_Q,
    OP_INTEGER_REAL,
    OP_REAL_INTEGER,
    OP_INTEGER_CHAR,
    OP_CHAR_INTEGER,
    OP_NUMBER_STRING,
    OP_STRING_NUMBER,
    OP_STRING_LENGTH,
    OP_STRING_APPEND,
    OP_STRING_REF,
    OP_STRING_SET_E,
    OP_MAKE_STRING,
    OP_SUBSTRING,

    OP_DISPLAY,
    OP_NEWLINE,

    OP_PLUS,
    OP_MINUS,
    OP_TIMES,
    OP_DIVIDE,

    OP_LESS,
    OP_EQUAL,
    OP_BIGGER,
    OP_LESS_EQUAL,
    OP_BIGGER_EQUAL,

    OP_AND,
    OP_OR,
    OP_NOT,

    OP_BEGIN,
    OP_DEFINE,
    OP_SET_E,
    OP_EVAL,
    OP_LAMBDA,
    OP_MACRO,
    OP_READ,
    OP_WRITE,
    OP_CURRENT_ENVIRONMENT
};

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

constexpr int NUMBER_BASIC_OPERATORS = OP_CURRENT_ENVIRONMENT + 1;

#endif /* OPERATORS_H */