.data
    valA:   .word 10   # Address 0x00
    valB:   .word 40   # Address 0x04
    valC:   .word 25   # Address 0x08
    maxVal: .word 0    # Address 0x0C
    result: .word 0    # Address 0x10

.text
.global main

main:
    # 1. Load valA and valB
    lw x1, 0(x0)        # x1 = 10
    lw x2, 4(x0)        # x2 = 40

    # 2. Compare A < B
    blt x1, x2, PICK_B

    # Case: A >= B (A is current max)
    sw x1, 12(x0)       # Store A into 'maxVal' slot
    beq x0, x0, CHECK_C # Jump to next check

PICK_B:
    # Case: A < B (B is current max)
    sw x2, 12(x0)       # Store B into 'maxVal' slot

CHECK_C:
    # 3. Load current max and valC
    lw x3, 12(x0)       # x3 = Current Max (between A and B)
    lw x4, 8(x0)        # x4 = 25

    # 4. Compare Max < C
    blt x3, x4, PICK_C

    # Case: Current Max >= C. We are done finding max.
    beq x0, x0, CALC

PICK_C:
    # Case: Max < C. Update Max.
    sw x4, 12(x0)       # Store C into 'maxVal' slot

CALC:
    # 5. Load the final Max value
    lw x5, 12(x0)       # x5 = Absolute Max (should be 40)

    # 6. Test SLLI: Multiply by 4 (Shift Left 2)
    slli x6, x5, 2      # x6 = 40 * 4 = 160

    # 7. Test SLT & SLL: Multiply by 2 more
    # We need to shift by 1, but we can't use ADDI to make a '1'.
    # TRICK: x0 is 0. x5 is 40. 
    # slt x7, x0, x5 -> Checks if 0 < 40. True! Sets x7 = 1.
    slt x7, x0, x5      
    
    # Now shift x6 (160) left by x7 (1)
    sll x8, x6, x7      # x8 = 160 << 1 = 320
    
    # 8. Store Final Result
    sw x8, 16(x0)       # Store 320 at address 0x10