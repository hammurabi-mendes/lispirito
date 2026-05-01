(define (map func lst)
	(cond
		((null? lst) '())
		(#t (cons (func (car lst)) (map func (cdr lst))))
	)
)

(define (foldl binfunc acc lst)
	(cond
		((null? lst) acc)
		(#t (foldl binfunc (binfunc (car lst) acc) (cdr lst)))
	)
)

(define (foldr binfunc acc lst)
	(cond
		((null? lst) acc)
		(#t (binfunc (car lst) (foldr binfunc acc (cdr lst))))
	)
)

(define (filter pred lst)
	(cond
		((null? lst) '())
		((pred (car lst)) (cons (car lst) (filter pred (cdr lst))))
		(#t (filter pred (cdr lst)))
	)
)