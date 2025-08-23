#ifndef MACROS_H
#define MACROS_H

constexpr int NUMBER_INITIAL_MACROS = 2;

char *macro_names[] {
    "if",
    "let"
};

char *macro_strings[] {
// if
"(macro (test if_clause else_clause)"
"    (cond ((test if_clause) (#t else_clause)))"
")",
// let
"(macro (bindings expression)"
"   (begin"
"       (define new_env (foldr cons (current-environment) (quote bindings)))"
"       (eval (quote expression) new_env)"
"   )"
")"
};

#endif /* MACROS_H */