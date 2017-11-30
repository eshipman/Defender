@ xorshift random number generator
.global xorshift
xorshift:
        ldr r1, [r0]    @ Load the integer into r1

        @ XOR with shift by 13
        mov r2, r1, lsl #13
        eor r1, r1, r2

        @ XOR with shift by 17
        mov r2, r1, lsr #17
        eor r1, r2, r1

        @ XOR with shift by 5
        mov r2, r1, lsl #5
        eor r1, r1, r2

        @ Store it back
        str r1, [r0]

        mov pc, lr
