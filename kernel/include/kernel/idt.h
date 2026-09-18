#pragma once
void idt_init(void);
void idt_set_gate(unsigned vector, void (*handler)(void));
