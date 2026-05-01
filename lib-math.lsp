(define (abs x)
	(if (> x 0) x (neg x))
)

(define (modulo x m)
	(- x (* (/ x m) m))
)
