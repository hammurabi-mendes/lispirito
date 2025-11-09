(define delay (macro (exp) (lambda () exp)))
(define force (macro (exp) (exp)))

(define stream-cons (macro (a b)
    (cons a (delay b))
))

(define stream-car (macro (s)
    (car s)
))

(define stream-cdr (macro (s)
    (force (cdr s))
))

(define (stream-range low high)
    (cond 
        ((> low high) '())
        (#t (stream-cons low (stream-range (+ low 1) high)))
    )
)

(define (stream-map func s)
    (cond 
        ((eq? s '()) '())
        (#t (stream-cons (func (stream-car s)) (stream-map func (stream-cdr s))))
    )
)

(define (stream-filter pred s)
    (cond
        ((eq? s '()) '())
        ((pred (stream-car s)) (stream-cons (stream-car s) (stream-filter pred (stream-cdr s))))
        (#t (stream-filter pred (stream-cdr s)))
    )
)

(define (stream-and s)
    (cond
        ((eq? s '()) #t)
        ((not (stream-car s)) #f)
        (#t (stream-and (stream-cdr s)))
    )
)

(define (stream-or s)
    (cond
        ((eq? s '()) #f)
        ((stream-car s) #t)
        (#t (stream-or (stream-cdr s)))
    )
)

(define (stream-foreach func s)
    (cond
        ((eq? s '()) '())
        (#t 
            (begin
                (func (stream-car s))
                (stream-foreach func (stream-cdr s))
            )
        )
    )
)