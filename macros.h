#ifndef MACROS_H
#define MACROS_H

constexpr int NUMBER_INITIAL_MACROS = 2;

char *macros[] {

"(define if (macro (test if_clause else_clause)"
"    (cond ((test if_clause) (#t else_clause)))"
"))",

"(define let (macro (bindings expression)"
"   (begin"
"       (define new_env (foldr cons (current-environment) (quote bindings)))"
"       (eval expression new_env)"
"   )"
"))"

};

#endif /* MACROS_H */