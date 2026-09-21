.section .text
.balign 2048
.global aarch64_vectors
aarch64_vectors:
    b aarch64_bad
    .balign 128
    b aarch64_bad
    .balign 128
    b aarch64_bad
    .balign 128
    b aarch64_bad
    .balign 128
    b aarch64_bad
    .balign 128
    b aarch64_bad
    .balign 128
    b aarch64_bad
    .balign 128
    b aarch64_bad
    .balign 128
    b aarch64_sync_entry
    .balign 128
    b aarch64_irq_entry
    .balign 128
    b aarch64_bad
    .balign 128
    b aarch64_bad
    .balign 128
    b aarch64_bad
    .balign 128
    b aarch64_bad
    .balign 128
    b aarch64_bad
    .balign 128
    b aarch64_bad

.macro save_frame
    sub sp, sp, #272
    stp x0, x1, [sp, #0]
    stp x2, x3, [sp, #16]
    stp x4, x5, [sp, #32]
    stp x6, x7, [sp, #48]
    stp x8, x9, [sp, #64]
    stp x10, x11, [sp, #80]
    stp x12, x13, [sp, #96]
    stp x14, x15, [sp, #112]
    stp x16, x17, [sp, #128]
    stp x18, x19, [sp, #144]
    stp x20, x21, [sp, #160]
    stp x22, x23, [sp, #176]
    stp x24, x25, [sp, #192]
    stp x26, x27, [sp, #208]
    stp x28, x29, [sp, #224]
    str x30, [sp, #240]
    mrs x9, CurrentEL
    cmp x9, #8
    b.eq 1f
    mrs x10, sp_el0
    mrs x11, elr_el1
    mrs x12, spsr_el1
    b 2f
1:
    mrs x10, sp_el0
    mrs x11, elr_el2
    mrs x12, spsr_el2
2:
    stp x10, x11, [sp, #248]
    str x12, [sp, #264]
.endm

.macro restore_frame
    ldp x10, x11, [sp, #248]
    ldr x12, [sp, #264]
    msr sp_el0, x10
    mrs x9, CurrentEL
    cmp x9, #8
    b.eq 1f
    msr elr_el1, x11
    msr spsr_el1, x12
    b 2f
1:
    msr elr_el2, x11
    msr spsr_el2, x12
2:
    ldp x0, x1, [sp, #0]
    ldp x2, x3, [sp, #16]
    ldp x4, x5, [sp, #32]
    ldp x6, x7, [sp, #48]
    ldp x8, x9, [sp, #64]
    ldp x10, x11, [sp, #80]
    ldp x12, x13, [sp, #96]
    ldp x14, x15, [sp, #112]
    ldp x16, x17, [sp, #128]
    ldp x18, x19, [sp, #144]
    ldp x20, x21, [sp, #160]
    ldp x22, x23, [sp, #176]
    ldp x24, x25, [sp, #192]
    ldp x26, x27, [sp, #208]
    ldp x28, x29, [sp, #224]
    ldr x30, [sp, #240]
    add sp, sp, #272
.endm

aarch64_sync_entry:
    save_frame
    mov x0, sp
    bl aarch64_sync
    restore_frame
    eret

aarch64_irq_entry:
    save_frame
    mov x0, sp
    bl aarch64_irq
    restore_frame
    eret

aarch64_bad:
    msr daifset, #0xf
1:  wfi
    b 1b

.global task_enter
.type task_enter, %function
task_enter:
    mov x30, x0
    ldp x10, x11, [x30, #248]
    ldr x12, [x30, #264]
    msr sp_el0, x10
    mrs x9, CurrentEL
    cmp x9, #8
    b.eq 1f
    msr elr_el1, x11
    msr spsr_el1, x12
    b 2f
1:
    msr elr_el2, x11
    msr spsr_el2, x12
2:
    ldp x2, x3, [x30, #16]
    ldp x4, x5, [x30, #32]
    ldp x6, x7, [x30, #48]
    ldp x8, x9, [x30, #64]
    ldp x10, x11, [x30, #80]
    ldp x12, x13, [x30, #96]
    ldp x14, x15, [x30, #112]
    ldp x16, x17, [x30, #128]
    ldp x18, x19, [x30, #144]
    ldp x20, x21, [x30, #160]
    ldp x22, x23, [x30, #176]
    ldp x24, x25, [x30, #192]
    ldp x26, x27, [x30, #208]
    ldp x28, x29, [x30, #224]
    ldp x0, x1, [x30, #0]
    ldr x30, [x30, #240]
    eret

.global arch_task_state_save
.type arch_task_state_save, %function
arch_task_state_save:
    stp q0, q1, [x0, #0]
    stp q2, q3, [x0, #32]
    stp q4, q5, [x0, #64]
    stp q6, q7, [x0, #96]
    stp q8, q9, [x0, #128]
    stp q10, q11, [x0, #160]
    stp q12, q13, [x0, #192]
    stp q14, q15, [x0, #224]
    stp q16, q17, [x0, #256]
    stp q18, q19, [x0, #288]
    stp q20, q21, [x0, #320]
    stp q22, q23, [x0, #352]
    stp q24, q25, [x0, #384]
    stp q26, q27, [x0, #416]
    stp q28, q29, [x0, #448]
    stp q30, q31, [x0, #480]
    mrs x1, fpcr
    mrs x2, fpsr
    str x1, [x0, #512]
    str x2, [x0, #520]
    mrs x1, tpidr_el0
    str x1, [x0, #528]
    ret

.global arch_task_state_restore
.type arch_task_state_restore, %function
arch_task_state_restore:
    ldp q0, q1, [x0, #0]
    ldp q2, q3, [x0, #32]
    ldp q4, q5, [x0, #64]
    ldp q6, q7, [x0, #96]
    ldp q8, q9, [x0, #128]
    ldp q10, q11, [x0, #160]
    ldp q12, q13, [x0, #192]
    ldp q14, q15, [x0, #224]
    ldp q16, q17, [x0, #256]
    ldp q18, q19, [x0, #288]
    ldp q20, q21, [x0, #320]
    ldp q22, q23, [x0, #352]
    ldp q24, q25, [x0, #384]
    ldp q26, q27, [x0, #416]
    ldp q28, q29, [x0, #448]
    ldp q30, q31, [x0, #480]
    ldr x1, [x0, #512]
    ldr x2, [x0, #520]
    msr fpcr, x1
    msr fpsr, x2
    ldr x1, [x0, #528]
    msr tpidr_el0, x1
    ret
