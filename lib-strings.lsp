(define (list->string lst)
	(foldl (lambda (first acc) (string-append (make-string 1 first) acc)) "" (reverse lst))
)

(define (string->list str)
	(cond
		((eq? str "") '())
		(#t (cons (string-ref str 0) (string->list (substring str 1 (string-length str)))))
	)
)

(define (string-ref str pos)
	(mem-read (+ (mem-addr str) pos))
)

(define (string-set! str pos chr)
	(mem-write (+ (mem-addr str) pos) chr)
)

(define (make-string size init)
	(begin
		(define result (mem-fill (mem-alloc (+ size 1)) init size))
		(mem-write (+ (mem-addr result) size) 0)
		(data->string result)
	)
)

(define (string-length input)
	(begin
		(define (helper str cur)
			(cond
				((eq? (mem-read (+ (mem-addr str) cur)) (integer->char 0)) 0)
				(#t (+ 1 (helper str (+ cur 1))))
			)
		)
		(helper input 0)
	)
)

(define (string-append str1 str2)
	(begin
		(define len1 (string-length str1))
		(define len2 (string-length str2))
		(define result (make-string (+ len1 len2) 0))
		(mem-copy (mem-addr result) (mem-addr str1) len1)
		(mem-copy (+ (mem-addr result) len1) (mem-addr str2) len2)
		result
	)
)

(define (substring str start finish)
	(begin
		(define size (+ (- finish start) 1))
		(define result (make-string size 0))
		(mem-copy (mem-addr result) (+ (mem-addr str) start) size)
		result
	)
)
