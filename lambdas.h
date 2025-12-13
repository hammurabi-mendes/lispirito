#ifndef LAMBDAS_H
#define LAMBDAS_H

constexpr int NUMBER_INITIAL_LAMBDAS = 23;

const char *lambda_names[] {
    "map",
    "foldl",
    "foldr",
    "filter",
    "length",
    "reverse",
    "append",
    "list",
    "flatten",
    "list?",
    "abs",
    "modulo",
    "list->string",
    "string->list",
    "string-ref",
    "string-set!",
    "make-string",
    "string-length",
    "string-append",
    "substring",
    "pair",
    "assoc-replace",
    "assoc-delete"
};

const char *lambda_strings[] {
// map
"(lambda (func lst)"
"    (cond"
"        ((null? lst) '())"
"        (#t (cons (func (car lst)) (map func (cdr lst))))"
"    )"
")",
// foldl
"(lambda (binfunc acc lst)"
"    (cond"
"        ((null? lst) acc)"
"        (#t (foldl binfunc (binfunc (car lst) acc) (cdr lst)))"
"    )"
")",
// foldr
"(lambda (binfunc acc lst)"
"    (cond"
"        ((null? lst) acc)"
"        (#t (binfunc (car lst) (foldr binfunc acc (cdr lst))))"
"    )"
")",
// filter
"(lambda (pred lst)"
"    (cond"
"        ((null? lst) '())"
"        ((pred (car lst)) (cons (car lst) (filter pred (cdr lst))))"
"        (#t (filter pred (cdr lst)))"
"    )"
")",
// length
"(lambda (l) (foldl (lambda (first acc) (+ 1 acc)) 0 l))",
// reverse
"(lambda (l) (foldl cons '() l))",
// append
"(lambda (l1 l2) (foldr cons l2 l1))",
// list
"(lambda (. items) (foldl cons '() items))",
// flatten
"(lambda (lst)"
"    (cond"
"        ((null? lst) '())"
"        ((atom? (car lst)) (cons (car lst) (flatten (cdr lst))))"
"        (#t (append (flatten (car lst)) (flatten (cdr lst))))"
"    )"
")",
// list?
"(lambda (input)"
"    (cond"
"        ((atom? input) #f)"
"        ((null? input) #t)"
"        (#t (list? (cdr input)))"
"    )"
")",
// abs
"(lambda (x) (if (> x 0) x (neg x)))",
// modulo
"(lambda (x m) (- x (* (/ x m) m)))",
// list->string
"(lambda (lst)"
"    (foldl (lambda (first acc) (string-append (make-string 1 first) acc)) \"\" (reverse lst))"
")",
// string->list
"(lambda (str)"
"    (cond"
"        ((eq? str \"\") '())"
"        (#t (cons (string-ref str 0) (string->list (substring str 1 (string-length str)))))"
"    )"
")",
// "string-ref"
"(lambda (str pos)"
"    (mem-read (+ (mem-addr str) pos))"
")",
// "string-set!"
"(lambda (str pos chr)"
"    (mem-write (+ (mem-addr str) pos) chr)"
")",
// "make-string"
"(lambda (size init)"
"    (begin"
"        (define result (mem-fill (mem-alloc (+ size 1)) init size))"
"        (mem-write (+ (mem-addr result) size) 0)"
"        (data->string result)"
"    )"
")",
// "string-length"
"(lambda (input)"
"    (begin"
"        (define (helper str cur)"
"            (cond"
"                ((eq? (mem-read (+ (mem-addr str) cur)) (integer->char 0)) 0)"
"                (#t (+ 1 (helper str (+ cur 1))))"
"            )"
"        )"
"        (helper input 0)"
"    )"
")",
// "string-append"
"(lambda (str1 str2)"
"    (begin"
"        (define len1 (string-length str1))"
"        (define len2 (string-length str2))"
"        (define result (make-string (+ len1 len2) 0))"
"        (mem-copy (mem-addr result) (mem-addr str1) len1)"
"        (mem-copy (+ (mem-addr result) len1) (mem-addr str2) len2)"
"        result"
"    )"
")",
// "substring"
"(lambda (str start finish)"
"    (begin"
"        (define size (+ (- finish start) 1))"
"        (define result (make-string size 0))"
"        (mem-copy (mem-addr result) (+ (mem-addr str) start) size)"
"        result"
"    )"
")",
// pair
"(lambda (a b) (cons a (cons b '())))",
// assoc-replace
"(lambda (key nval lst)"
"    (foldr (lambda (cur acc) (if (eq? (car cur) key) (cons (pair key nval) acc) (cons cur acc))) '() lst)"
")",
// assoc-delete
"(lambda (key nval lst)"
"    (foldr (lambda (cur acc) (if (eq? (car cur) key) acc (cons cur acc))) '() lst)"
")"
};

#endif /* LAMBDAS_H */