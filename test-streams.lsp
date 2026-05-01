;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Stream Tests
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define (displayln msg) (display msg) (newline))

(define (% x d) (- x (* (/ x d) d)))

(define (prime? n)
    (define (not-divisor? d) (not (= (% n d) 0)))
    (stream-and (stream-map not-divisor? (stream-range 2 (- n 1))))
)

(prime? 961)

(define prime-stream (stream-filter (lambda (x) (prime? x)) (stream-range 1 110)))
(stream-foreach displayln prime-stream)