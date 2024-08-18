#ifndef LAMBDAS_H
#define LAMBDAS_H

constexpr int NUMBER_INITIAL_LAMBDAS = 12;

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

"(define length (lambda (l) (foldl (lambda (first acc) (+ 1 acc)) 0 l)))",

"(define reverse (lambda (l) (foldl cons '() l)))",

"(define append (lambda (l1 l2) (foldr cons l2 l1)))",

"(define list (lambda (. items) (foldl cons '() items)))",

"(define list? (lambda (input)"
"    (cond ("
"        ((atom? input) #f)"
"        ((eq? input '()) #t)"
"        (#t (list? (cdr input)))"
"    ))"
"))",

"(define apply (lambda (op . list) (foldl op (car list) (cdr list))))",

"(define abs (lambda (x) (if (> x 0) x (neg x))))",
"(define modulo (lambda (x m) (- x (* (/ x m) m))))"

};

#endif /* LAMBDAS_H */