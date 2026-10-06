; c5xasm PoC seed test - validated C5x words
; expected: B94A B801 EF00 (+ directive/label exercise)
        .text
start:  lacl    #4ah            ; -> B94A
        add     #1h             ; -> B801
        ret                     ; -> EF00
VAL     .set    1234h
        .word   VAL, start      ; -> 1234, 0000 (start = address 0)
