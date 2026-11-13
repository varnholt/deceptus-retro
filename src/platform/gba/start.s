.syntax unified
.cpu arm7tdmi
.arm
.section .gba_header,"ax",%progbits
.global _start
_start:
    b reset
    .space 188

.section .text.reset,"ax",%progbits
.balign 4
reset:
    @ initialize separate IRQ and system stacks, leaving interrupts disabled.
    mov r0, #0xd2
    msr cpsr_c, r0
    ldr sp, =0x03007fa0
    mov r0, #0xdf
    msr cpsr_c, r0
    ldr sp, =0x03007f00

    ldr r0, =__iwram_load
    ldr r1, =__iwram_start
    ldr r2, =__iwram_end
4:
    cmp r1, r2
    ldrlo r3, [r0], #4
    strlo r3, [r1], #4
    blo 4b
    ldr r0, =__data_load
    ldr r1, =__data_start
    ldr r2, =__data_end
1:
    cmp r1, r2
    ldrlo r3, [r0], #4
    strlo r3, [r1], #4
    blo 1b
    mov r0, #0
    ldr r1, =__bss_start
    ldr r2, =__bss_end
2:
    cmp r1, r2
    strlo r0, [r1], #4
    blo 2b
    bl main
3:
    b 3b
.ltorg
