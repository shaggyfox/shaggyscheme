(define reverse (lambda (l)
                  (begin
                    (define reverse2 (lambda (l out)
                                       (if (eq? '() l)
                                           out
                                           (reverse2 (cdr l) (cons (car l) out)))))
                    (reverse2 l '()))))

(define primes (lambda (m)
                 (begin
                   (define isprime (lambda (p)
                                     (begin
                                       (define isprime2 (lambda (p c)
                                                          (if (= c 1)
                                                              #t
                                                              (if (< 0 (modulo p c))
                                                                  (isprime2 p (- c 1))
                                                                  #f))))
                                       (if (= p 1)
                                           #t
                                           (isprime2 p (- p 1))))))
                   (define primes2 (lambda (m c l)
                                     (if (<= c m)
                                         (if (isprime c)
                                             (primes2 m (+ c 1) (cons c l))
                                             (primes2 m (+ c 1) l))
                                         l )))
                   (primes2 m 1 '()))))

(display (reverse (primes 1024)))
(newline)

