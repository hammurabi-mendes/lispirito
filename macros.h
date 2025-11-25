#ifndef MACROS_H
#define MACROS_H

constexpr int NUMBER_INITIAL_MACROS = 4;

const char *macro_names[] {
    "if",
    "let",
    "let*",
    "letrec"
};

const char *macro_strings[] {
// if
"(macro (test if_clause else_clause)"
"    (cond (test if_clause) (#t else_clause))"
")",
// let
"(macro (bindings expression)"
"    (begin"
"        (define old_env (current-environment))"
"        (define (appender binding cur_env) (begin"
"            (define p1 (car binding))"
"            (define p2 (car (cdr binding)))"
"            (cons (pair p1 (eval p2 old_env)) cur_env)"
"        ))"
"        (define new_env (foldl appender old_env (quote bindings)))"
"        (eval (quote expression) new_env)"
"    )"
")",
// let*
"(macro (bindings expression)"
"    (begin"
"        (define old_env (current-environment))"
"        (define (appender binding cur_env) (begin"
"            (define p1 (car binding))"
"            (define p2 (car (cdr binding)))"
"            (cons (pair p1 (eval p2 cur_env)) cur_env) "
"        ))"
"        (define new_env (foldl appender old_env (quote bindings)))"
"        (eval (quote expression) new_env)"
"    )"
")",
// letrec
"(macro (bindings expression)"
"    (begin"
"        (define old_env (current-environment))"
"        (define (appender binding cur_env) (begin"
"            (define p1 (car binding))"
"            (define p2 (car (cdr binding)))"
"            (cons (pair p1 p2) cur_env) "
"        ))"
"        (define (setter binding cur_env) (begin"
"            (define p1 (car binding))"
"            (define p2 (car (cdr binding)))"
"            (assoc-replace p1 (eval p2 cur_env) cur_env)"
"        ))"
"        (define tmp_env (foldl appender old_env (quote bindings)))"
"        (define new_env (foldl setter tmp_env (quote bindings)))"
"        (eval (quote expression) new_env)"
"    )"
")"
};

#endif /* MACROS_H */