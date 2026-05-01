(define (length l)
	(foldl (lambda (first acc) (+ 1 acc)) 0 l)
)

(define (reverse l)
	(foldl cons '() l)
)

(define (append l1 l2)
	(foldr cons l2 l1)
)

(define (list . items)
	(foldl cons '() items)
)

(define (flatten lst)
	(cond
		((null? lst) '())
		((atom? (car lst)) (cons (car lst) (flatten (cdr lst))))
		(#t (append (flatten (car lst)) (flatten (cdr lst))))
	)
)

(define (list? input)
	(cond
		((atom? input) #f)
		((null? input) #t)
		(#t (list? (cdr input)))
	)
)
