(define if (macro (test if_clause else_clause)
	(cond
		(test if_clause)
		(#t else_clause)
	)
))