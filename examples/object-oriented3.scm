; check closure

(define selector-undefined-error 'selector-undefined-error)

(define object
  (lambda (selector) selector-undefined-error))

(define =>
  (lambda (instance method . args)
    (let ((_method (instance method)))
      (apply _method args))))

(class fruit (name)
  (super object)
  ((fetch (lambda (self) name))))

(define cons 
  (lambda (l1 l2)
    (cond ((ispa)))

(define apple (fruit "apple"))

; (display (apply + (list 1 2)))
(display (list (list 1 2) 12))
; (display (list 1 2))
(cons (list 1 2) (list 3 4))
; 
