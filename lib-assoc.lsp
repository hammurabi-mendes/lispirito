(define (assoc-replace key nval lst)
	(foldr (lambda (cur acc) (if (eq? (car cur) key) (cons (pair key nval) acc) (cons cur acc))) '() lst)
)

(define (assoc-delete key nval lst)
	(foldr (lambda (cur acc) (if (eq? (car cur) key) acc (cons cur acc))) '() lst)
)
