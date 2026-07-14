.section ".text"
.global _start
_start:
    mrs     x1, mpidr_el1
    and     x1, x1, #3
    cbz     x1, master

proc_hang:
    wfe
    b       proc_hang

master:
    ldr     x1, =_stack_top
    mov     sp, x1

    ldr     x1, =__bss_start
    ldr     x2, =__bss_end
    
    bss_clear_loop:
        cmp     x1, x2
        b.ge    bss_clear_done
        str     xzr, [x1], #8
        b       bss_clear_loop
        
    bss_clear_done:
        bl      main
        b       proc_hang
