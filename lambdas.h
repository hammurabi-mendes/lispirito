#ifndef LAMBDAS_H
#define LAMBDAS_H

constexpr int NUMBER_INITIAL_LAMBDAS = 20;

char *lambda_names[] {
    "map",
    "foldl",
    "foldr",
    "filter",
    "length",
    "reverse",
    "append",
    "list",
    "list?",
    "abs",
    "modulo",
    "list->string",
    "string->list",
    "apply",
    "string-ref",
    "string-set!",
    "make-string",
    "string-length",
    "string-append",
    "substring"
};

char *lambda_strings[] {
// map
"(lambda (func list)"
"    (cond ("
"        ((eq? list '()) '())"
"        (#t (cons (func (car list)) (map func (cdr list))))"
"    ))"
")",
// foldl
"(lambda (binfunc acc list)"
"    (cond ("
"        ((eq? list '()) acc)"
"        (#t (foldl binfunc (binfunc (car list) acc) (cdr list)))"
"    ))"
")",
// foldr
"(lambda (binfunc acc list)"
"    (cond ("
"        ((eq? list '()) acc)"
"        (#t (binfunc (car list) (foldr binfunc acc (cdr list))))"
"    ))"
")",
// filter
"(lambda (pred list)"
"    (cond ("
"        ((eq? list '()) '())"
"        ((pred (car list)) (cons (car list) (filter pred (cdr list))))"
"        (#t (filter pred (cdr list)))"
"    ))"
")",
// length
"(lambda (l) (foldl (lambda (first acc) (+ 1 acc)) 0 l))",
// reverse
"(lambda (l) (foldl cons '() l))",
// append
"(lambda (l1 l2) (foldr cons l2 l1))",
// list
"(lambda (. items) (foldl cons '() items))",
// list?
"(lambda (input)"
"    (cond ("
"        ((atom? input) #f)"
"        ((eq? input '()) #t)"
"        (#t (list? (cdr input)))"
"    ))"
")",
// abs
"(lambda (x) (if (> x 0) x (neg x)))",
// modulo
"(lambda (x m) (- x (* (/ x m) m)))",
// list->string
"(lambda (list)"
"    (foldl (lambda (first acc) (string-append (make-string 1 first) acc)) \"\" (reverse list))"
")",
// string->list
"(lambda (str)"
"    (cond ("
"        ((eq? str \"\") '())"
"        (#t (cons (string-ref str 0) (string->list (substring str 1 (string-length str)))))"
"    ))"
")",
// apply
"(lambda (op . list) (foldl op (car list) (cdr list)))",
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
"            (cond ("
"                ((eq? (mem-read (+ (mem-addr str) cur)) (integer->char 0)) 0)"
"                (#t (+ 1 (helper str (+ cur 1))))"
"            ))"
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
")"
};

#endif /* LAMBDAS_H */