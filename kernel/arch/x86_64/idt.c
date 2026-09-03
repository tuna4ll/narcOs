#include <kernel/idt.h>
#include <kernel/string.h>
#include <stdint.h>

struct __attribute__((packed)) idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
};

struct __attribute__((packed)) idtr {
    uint16_t limit;
    uint64_t base;
};

static struct idt_entry idt[256];
extern void syscall_entry(void);
extern void exception_ud(void);
extern void exception_df(void);
extern void exception_np(void);
extern void exception_ss(void);
extern void exception_gp(void);
extern void exception_pf(void);

static void idt_set_gate(int vector, void (*handler)(void), uint8_t flags) {
    uint64_t addr = (uint64_t)(uintptr_t)handler;
    idt[vector].offset_low = addr & 0xffff;
    idt[vector].selector = 0x08;
    idt[vector].ist = 0;
    idt[vector].type_attr = flags;
    idt[vector].offset_mid = (addr >> 16) & 0xffff;
    idt[vector].offset_high = addr >> 32;
    idt[vector].zero = 0;
}

void idt_init(void) {
    memset(idt, 0, sizeof(idt));
    idt_set_gate(6, exception_ud, 0x8e);
    idt_set_gate(8, exception_df, 0x8e);
    idt_set_gate(11, exception_np, 0x8e);
    idt_set_gate(12, exception_ss, 0x8e);
    idt_set_gate(13, exception_gp, 0x8e);
    idt_set_gate(14, exception_pf, 0x8e);
    idt_set_gate(0x80, syscall_entry, 0xee); /* present, DPL=3, interrupt gate */
    struct idtr ptr = { .limit = sizeof(idt) - 1, .base = (uint64_t)(uintptr_t)idt };
    __asm__ volatile ("lidt %0" : : "m"(ptr));
}
