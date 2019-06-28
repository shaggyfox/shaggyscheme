
(define assoc_list_left (lambda (l)
                          (if (eq? '() l)
                              '()
                              (cons (car(car l)) (assoc_list_left (cdr l))))))
(define assoc_list_right (lambda (l)
                           (if (eq? '() l)
                               '()
                               (cons (car(cdr(car l))) (assoc_list_right (cdr l))))))


;(macro (let form) `(lambda ,(assoc_list_left (car (cdr form))))

(macro (let form)
       `(
         (lambda
           ,(assoc_list_left (car(cdr form)))
           ,(car(cdr(cdr form))))
         ,@(assoc_list_right (car(cdr form)))))

(display (let ((x 1)) "bla"))
(newline)
(display "mmmm\n")
