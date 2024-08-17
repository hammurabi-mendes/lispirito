#ifndef LAMBDAS_H
#define LAMBDAS_H

constexpr int NUMBER_INITIAL_LAMBDAS = 6;

char *lambdas[] {

"(define map (lambda (func list)"
"    (cond ("
"        ((eq? list '()) '())"
"        (#t (cons (func (car list)) (map func (cdr list))))"
"    ))"
"))",

"(define foldl (lambda (binfunc acc list)"
"    (cond ("
"        ((eq? list '()) acc)"
"        (#t (foldl binfunc (binfunc (car list) acc) (cdr list)))"
"    ))"
"))",

"(define foldr (lambda (binfunc acc list)"
"    (cond ("
"        ((eq? list '()) acc)"
"        (#t (binfunc (car list) (foldr binfunc acc (cdr list))))"
"    ))"
"))",

"(define filter (lambda (pred list)"
"    (cond ("
"        ((eq? list '()) '())"
"        ((pred (car list)) (cons (car list) (filter pred (cdr list))))"
"        (#t (filter pred (cdr list)))"
"    ))"
"))",

"(define length (lambda (list)"
"    (cond ("
"        ((eq? list '()) 0)"
"        (#t (+ 1 (length (cdr list))))"
"    ))"
"))",

"(define apply (lambda (op . list) (foldl op (car list) (cdr list))))"
};

#endif /* LAMBDAS_H */