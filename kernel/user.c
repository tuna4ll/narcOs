#include <kernel/mm.h>
#include <kernel/string.h>
#include <kernel/task.h>
#include <kernel/console.h>
#include <stddef.h>
#include <stdint.h>

#define USER_MIN        0x0000000000400000ULL
#define USER_IMAGE_END  0x0000000010000000ULL
#define USER_STACK_TOP  0x0000100008000000ULL
#define USER_STACK_SIZE (64ULL * 1024ULL)

#define EI_NIDENT 16
#define ET_EXEC 2
#define EM_X86_64 62
#define PT_LOAD 1
#define PT_PHDR 6
#define PF_W 2

#define AT_NULL   0
#define AT_PHDR   3
#define AT_PHENT  4
#define AT_PHNUM  5
#define AT_PAGESZ 6
#define AT_ENTRY  9
#define AT_RANDOM 25
#define AT_EXECFN 31

struct __attribute__((packed)) elf64_ehdr {
    unsigned char ident[EI_NIDENT];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
};

struct __attribute__((packed)) elf64_phdr {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
};

extern const unsigned char user_blob_start[];
extern const unsigned char user_blob_end[];

static uint64_t align_down(uint64_t x) {
    return x & ~(PAGE_SIZE - 1);
}

static uint64_t align_up(uint64_t x) {
    return (x + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

static __attribute__((noreturn)) void user_panic(const char *msg) {
    console_puts("[panic] user loader: ");
    console_puts(msg);
    console_puts("\n");
    for (;;) __asm__ volatile ("cli; hlt");
}

static void user_copy_out(struct address_space *space, uint64_t dst, const void *src, size_t len) {
    const uint8_t *s = src;
    while (len) {
        uint64_t phys = vmm_user_phys(space, dst);
        if (!phys) user_panic("copy to unmapped page");
        size_t chunk = PAGE_SIZE - (size_t)(dst & (PAGE_SIZE - 1));
        if (chunk > len) chunk = len;
        memcpy(phys_to_virt(phys), s, chunk);
        dst += chunk;
        s += chunk;
        len -= chunk;
    }
}

static void user_zero(struct address_space *space, uint64_t dst, size_t len) {
    while (len) {
        uint64_t phys = vmm_user_phys(space, dst);
        if (!phys) user_panic("zero of unmapped page");
        size_t chunk = PAGE_SIZE - (size_t)(dst & (PAGE_SIZE - 1));
        if (chunk > len) chunk = len;
        memset(phys_to_virt(phys), 0, chunk);
        dst += chunk;
        len -= chunk;
    }
}

static void map_range(struct address_space *space, uint64_t start, uint64_t end) {
    for (uint64_t va = align_down(start); va < align_up(end); va += PAGE_SIZE) {
        if (vmm_user_phys(space, va)) continue;
        uint64_t phys = pmm_alloc_page();
        if (!phys) user_panic("out of physical memory");
        if (vmm_map_user(space, va, phys, VMM_WRITE) != 0) user_panic("failed to map image");
    }
}

static uint64_t stack_put(struct address_space *space, uint64_t *sp, const void *src, size_t len) {
    if (*sp < USER_STACK_TOP - USER_STACK_SIZE + len) user_panic("initial stack overflow");
    *sp -= len;
    user_copy_out(space, *sp, src, len);
    return *sp;
}

static uint64_t find_phdr_addr(const struct elf64_ehdr *eh, const struct elf64_phdr *ph) {
    for (uint16_t i = 0; i < eh->phnum; i++) {
        if (ph[i].type == PT_PHDR) return ph[i].vaddr;
    }

    uint64_t bytes = (uint64_t)eh->phnum * eh->phentsize;
    for (uint16_t i = 0; i < eh->phnum; i++) {
        if (ph[i].type != PT_LOAD) continue;
        if (eh->phoff >= ph[i].offset &&
            eh->phoff + bytes <= ph[i].offset + ph[i].filesz) {
            return ph[i].vaddr + (eh->phoff - ph[i].offset);
        }
    }
    return 0;
}

static uint64_t build_linux_stack(struct address_space *space,
                                  const struct elf64_ehdr *eh, uint64_t phdr_addr) {
    static const char arg0[] = "hello";
    static const uint8_t random_bytes[16] = {
        0x54, 0x75, 0x72, 0x6b, 0x4f, 0x53, 0x64, 0x65,
        0x76, 0x2d, 0x6d, 0x75, 0x73, 0x6c, 0x21, 0x7f,
    };

    for (uint64_t va = USER_STACK_TOP - USER_STACK_SIZE; va < USER_STACK_TOP; va += PAGE_SIZE) {
        uint64_t phys = pmm_alloc_page();
        if (!phys) user_panic("out of physical memory for stack");
        if (vmm_map_user(space, va, phys, VMM_WRITE) != 0) user_panic("failed to map stack");
    }

    uint64_t sp = USER_STACK_TOP;
    uint64_t execfn = stack_put(space, &sp, arg0, sizeof(arg0));
    uint64_t randomp = stack_put(space, &sp, random_bytes, sizeof(random_bytes));
    sp &= ~0xfULL;

    const uint64_t words[] = {
        1,
        execfn,
        0,
        0,
        AT_PHDR, phdr_addr,
        AT_PHENT, eh->phentsize,
        AT_PHNUM, eh->phnum,
        AT_PAGESZ, PAGE_SIZE,
        AT_ENTRY, eh->entry,
        AT_RANDOM, randomp,
        AT_EXECFN, execfn,
        AT_NULL, 0,
    };

    uint64_t bytes = sizeof(words);
    sp = (sp - bytes) & ~0xfULL;
    user_copy_out(space, sp, words, sizeof(words));
    return sp;
}

void user_start(void) {
    size_t blob_size = (size_t)(user_blob_end - user_blob_start);
    if (blob_size < sizeof(struct elf64_ehdr)) user_panic("ELF is truncated");

    const struct elf64_ehdr *eh = (const struct elf64_ehdr *)user_blob_start;
    if (eh->ident[0] != 0x7f || eh->ident[1] != 'E' || eh->ident[2] != 'L' || eh->ident[3] != 'F' ||
        eh->ident[4] != 2 || eh->ident[5] != 1 || eh->type != ET_EXEC || eh->machine != EM_X86_64 ||
        eh->phentsize != sizeof(struct elf64_phdr) || !eh->phnum) {
        user_panic("unsupported ELF64 executable");
    }

    uint64_t ph_bytes = (uint64_t)eh->phnum * eh->phentsize;
    if (eh->phoff > blob_size || ph_bytes > blob_size - eh->phoff) user_panic("bad program headers");
    const struct elf64_phdr *ph = (const struct elf64_phdr *)(user_blob_start + eh->phoff);

    uint64_t phdr_addr = find_phdr_addr(eh, ph);
    if (!phdr_addr) user_panic("cannot locate runtime program headers");

    for (size_t n = 0; n < 2; n++) {
        struct task *task = task_create();
        if (!task) user_panic("cannot create task");
        struct address_space *space = task_space(task);

        for (uint16_t i = 0; i < eh->phnum; i++) {
            if (ph[i].type != PT_LOAD) continue;
            if (ph[i].filesz > ph[i].memsz || ph[i].offset > blob_size ||
                ph[i].filesz > blob_size - ph[i].offset)
                user_panic("invalid PT_LOAD");
            if (ph[i].vaddr < USER_MIN || ph[i].vaddr >= USER_IMAGE_END ||
                ph[i].memsz > USER_IMAGE_END - ph[i].vaddr)
                user_panic("PT_LOAD outside userspace image window");
            if (!ph[i].memsz) continue;

            map_range(space, ph[i].vaddr, ph[i].vaddr + ph[i].memsz);
            user_copy_out(space, ph[i].vaddr, user_blob_start + ph[i].offset,
                          (size_t)ph[i].filesz);
            if (ph[i].memsz > ph[i].filesz)
                user_zero(space, ph[i].vaddr + ph[i].filesz,
                          (size_t)(ph[i].memsz - ph[i].filesz));
        }

        for (uint16_t i = 0; i < eh->phnum; i++) {
            if (ph[i].type != PT_LOAD || !ph[i].memsz) continue;
            for (uint64_t va = align_down(ph[i].vaddr);
                 va < align_up(ph[i].vaddr + ph[i].memsz); va += PAGE_SIZE) {
                if (vmm_protect_user(space, va, (ph[i].flags & PF_W) ? VMM_WRITE : 0) != 0)
                    user_panic("failed to protect PT_LOAD");
            }
        }

        uint64_t stack = build_linux_stack(space, eh, phdr_addr);
        task_set_entry(task, eh->entry, stack);
    }

    console_puts("[user] entering ring 3\n");
    task_start();
}
