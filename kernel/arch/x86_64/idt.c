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

extern void exception_ud(void);
extern void exception_df(void);
extern void exception_np(void);
extern void exception_ss(void);
extern void exception_gp(void);
extern void exception_pf(void);

static void set_gate(unsigned vector, void (*handler)(void)) {
    uint64_t addr = (uint64_t)(uintptr_t)handler;
    idt[vector] = (struct idt_entry){
        .offset_low = addr,
        .selector = 0x08,
        .type_attr = 0x8e,
        .offset_mid = addr >> 16,
        .offset_high = addr >> 32,
    };
}

void idt_init(void) {
    memset(idt, 0, sizeof(idt));
    set_gate(6, exception_ud);
    set_gate(8, exception_df);
    set_gate(11, exception_np);
    set_gate(12, exception_ss);
    set_gate(13, exception_gp);
    set_gate(14, exception_pf);

    struct idtr ptr = {
        .limit = sizeof(idt) - 1,
        .base = (uint64_t)(uintptr_t)idt,
    };
    __asm__ volatile ("lidt %0" : : "m"(ptr));
}
