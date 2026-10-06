; c5xdis round-trip fixture: exercises the currently-decoded format classes
; (no-operand 0x00, single-word memory 0x01, direct branch 0x02) plus raw data.
        .text
start:
        abs                     ; 0x00 no-operand
        apac
        addb
        addc   0x10             ; 0x01 memory, direct dma
        addh   *+,ar2           ; 0x01 memory, indirect + next-ARP
        adds   *
        addt   *-
        andi   0x7f
        b      0x0820           ; 0x02 branch, 16-bit target
        call   0x0830
        .word  0x1234           ; data: stays a .word
        .word  0xabcd
        abs
