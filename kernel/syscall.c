#include <kernel/mm.h>
#include <kernel/syscall.h>
#include <kernel/vga.h>
#include <stddef.h>
#include <stdint.h>

#define SYS_READ             0
#define SYS_WRITE            1
#define SYS_CLOSE            3
#define SYS_MMAP             9
#define SYS_MPROTECT        10
#define SYS_MUNMAP          11
#define SYS_BRK             12
#define SYS_IOCTL           16
#define SYS_WRITEV          20
#define SYS_MREMAP          25
#define SYS_MADVISE         28
#define SYS_GETPID          39
#define SYS_EXIT            60
#define SYS_ARCH_PRCTL     158
#define SYS_SET_TID_ADDRESS 218
#define SYS_CLOCK_GETTIME   228
#define SYS_EXIT_GROUP     231
#define SYS_GETRANDOM      318

#define EBADF   9
#define EFAULT 14
#define EINVAL 22
#define ENOTTY 25
#define ENOSYS 38
#define ENOMEM 12

#define PROT_WRITE 0x2
#define MAP_FIXED  0x10
#define MAP_ANON   0x20
#define MAP_FIXED_NOREPLACE 0x100000
#define ARCH_SET_FS 0x1002
#define ARCH_GET_FS 0x1003
#define TIOCGWINSZ 0x5413
#define IA32_FS_BASE 0xc0000100u

#define USER_MMAP_BASE 0x0000100010000000ULL
#define USER_MMAP_END  0x0000100040000000ULL

struct iovec64 {
    uint64_t base;
    uint64_t len;
};

struct winsize64 {
    uint16_t rows;
    uint16_t cols;
    uint16_t xpixel;
    uint16_t ypixel;
};

struct timespec64 {
    int64_t sec;
    int64_t nsec;
};

static uint64_t mmap_next = USER_MMAP_BASE;
static uint64_t fs_base;
static uint64_t fake_clock_ns;

static uint64_t align_up(uint64_t x) {
    if (x > UINT64_MAX - (PAGE_SIZE - 1)) return 0;
    return (x + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

static void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t lo = (uint32_t)value;
    uint32_t hi = (uint32_t)(value >> 32);
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

static int copy_from_user(void *dst, uint64_t src, size_t len) {
    uint8_t *d = dst;
    if (!vmm_user_range_ok(src, len, 0)) return -1;
    while (len) {
        uint64_t phys = vmm_user_phys(src);
        if (!phys) return -1;
        size_t chunk = PAGE_SIZE - (size_t)(src & (PAGE_SIZE - 1));
        if (chunk > len) chunk = len;
        const uint8_t *s = phys_to_virt(phys);
        for (size_t i = 0; i < chunk; i++) d[i] = s[i];
        src += chunk;
        d += chunk;
        len -= chunk;
    }
    return 0;
}

static int copy_to_user(uint64_t dst, const void *src, size_t len) {
    const uint8_t *s = src;
    if (!vmm_user_range_ok(dst, len, 1)) return -1;
    while (len) {
        uint64_t phys = vmm_user_phys(dst);
        if (!phys) return -1;
        size_t chunk = PAGE_SIZE - (size_t)(dst & (PAGE_SIZE - 1));
        if (chunk > len) chunk = len;
        uint8_t *d = phys_to_virt(phys);
        for (size_t i = 0; i < chunk; i++) d[i] = s[i];
        dst += chunk;
        s += chunk;
        len -= chunk;
    }
    return 0;
}

static long sys_write(uint64_t fd, uint64_t buf, uint64_t len) {
    if (fd != 1 && fd != 2) return -EBADF;
    if (!vmm_user_range_ok(buf, len, 0)) return -EFAULT;

    uint64_t left = len;
    uint64_t ptr = buf;
    while (left) {
        uint64_t phys = vmm_user_phys(ptr);
        if (!phys) return -EFAULT;
        size_t chunk = PAGE_SIZE - (size_t)(ptr & (PAGE_SIZE - 1));
        if ((uint64_t)chunk > left) chunk = (size_t)left;
        vga_write((const char *)phys_to_virt(phys), chunk);
        ptr += chunk;
        left -= chunk;
    }
    return (long)len;
}

static long sys_writev(uint64_t fd, uint64_t iov_addr, uint64_t count) {
    if (fd != 1 && fd != 2) return -EBADF;
    if (count > 1024) return -EINVAL;
    if (count && !vmm_user_range_ok(iov_addr, count * sizeof(struct iovec64), 0)) return -EFAULT;

    long total = 0;
    for (uint64_t i = 0; i < count; i++) {
        struct iovec64 iov;
        if (copy_from_user(&iov, iov_addr + i * sizeof(struct iovec64), sizeof(iov)) != 0) return -EFAULT;
        long r = sys_write(fd, iov.base, iov.len);
        if (r < 0) return r;
        total += r;
    }
    return total;
}

static long sys_mmap(uint64_t addr, uint64_t len, uint64_t prot, uint64_t flags,
                     uint64_t fd, uint64_t offset) {
    (void)offset;
    if (!len) return -EINVAL;
    if (!(flags & MAP_ANON) || (int64_t)fd != -1) return -ENOSYS;

    uint64_t size = align_up(len);
    if (!size) return -ENOMEM;

    uint64_t base;
    if (flags & (MAP_FIXED | MAP_FIXED_NOREPLACE)) {
        if (addr & (PAGE_SIZE - 1)) return -EINVAL;
        base = addr;
    } else {
        base = align_up(mmap_next);
    }

    if (base < USER_MMAP_BASE || size > USER_MMAP_END - base) return -ENOMEM;

    for (uint64_t off = 0; off < size; off += PAGE_SIZE) {
        uint64_t va = base + off;
        if (vmm_user_phys(va)) {
            if (flags & MAP_FIXED_NOREPLACE) return -ENOMEM;
            if (!(flags & MAP_FIXED)) return -ENOMEM;
            vmm_unmap_user(va);
        }
        uint64_t phys = pmm_alloc_page();
        if (!phys) return -ENOMEM;
        vmm_map_user(va, phys, (prot & PROT_WRITE) ? VMM_WRITE : 0);
    }

    if (!(flags & (MAP_FIXED | MAP_FIXED_NOREPLACE))) mmap_next = base + size;
    return (long)base;
}

static long sys_mprotect(uint64_t addr, uint64_t len, uint64_t prot) {
    if ((addr & (PAGE_SIZE - 1)) || !len) return -EINVAL;
    uint64_t size = align_up(len);
    if (!size) return -EINVAL;
    for (uint64_t off = 0; off < size; off += PAGE_SIZE) {
        if (vmm_protect_user(addr + off, (prot & PROT_WRITE) ? VMM_WRITE : 0) != 0) return -ENOMEM;
    }
    return 0;
}

static long sys_munmap(uint64_t addr, uint64_t len) {
    if ((addr & (PAGE_SIZE - 1)) || !len) return -EINVAL;
    uint64_t size = align_up(len);
    if (!size) return -EINVAL;
    for (uint64_t off = 0; off < size; off += PAGE_SIZE) {
        (void)vmm_unmap_user(addr + off);
    }
    return 0;
}

static long sys_ioctl(uint64_t fd, uint64_t request, uint64_t arg) {
    if (fd != 1 && fd != 2) return -EBADF;
    if (request != TIOCGWINSZ) return -ENOTTY;
    struct winsize64 ws = {
        .rows = vga_rows(), .cols = vga_cols(), .xpixel = 0, .ypixel = 0,
    };
    return copy_to_user(arg, &ws, sizeof(ws)) == 0 ? 0 : -EFAULT;
}

static long sys_arch_prctl(uint64_t code, uint64_t addr) {
    if (code == ARCH_SET_FS) {
        if (addr > 0x00007fffffffffffULL) return -EINVAL;
        fs_base = addr;
        wrmsr(IA32_FS_BASE, addr);
        return 0;
    }
    if (code == ARCH_GET_FS) {
        return copy_to_user(addr, &fs_base, sizeof(fs_base)) == 0 ? 0 : -EFAULT;
    }
    return -EINVAL;
}

static long dispatch_linux(uint64_t nr, uint64_t a1, uint64_t a2, uint64_t a3,
                           uint64_t a4, uint64_t a5, uint64_t a6) {
    switch (nr) {
    case SYS_READ:
        return a1 == 0 ? 0 : -EBADF;
    case SYS_WRITE:
        return sys_write(a1, a2, a3);
    case SYS_CLOSE:
        return a1 <= 2 ? 0 : -EBADF;
    case SYS_MMAP:
        return sys_mmap(a1, a2, a3, a4, a5, a6);
    case SYS_MPROTECT:
        return sys_mprotect(a1, a2, a3);
    case SYS_MUNMAP:
        return sys_munmap(a1, a2);
    case SYS_BRK:
        return 0; /* makes musl fall back to mmap-backed allocation */
    case SYS_IOCTL:
        return sys_ioctl(a1, a2, a3);
    case SYS_WRITEV:
        return sys_writev(a1, a2, a3);
    case SYS_MREMAP:
        return -ENOSYS;
    case SYS_MADVISE:
        return 0;
    case SYS_GETPID:
        return 1;
    case SYS_ARCH_PRCTL:
        return sys_arch_prctl(a1, a2);
    case SYS_SET_TID_ADDRESS:
        return 1;
    case SYS_CLOCK_GETTIME: {
        if (a1 > 7) return -EINVAL;
        struct timespec64 ts = {
            .sec = (int64_t)(fake_clock_ns / 1000000000ULL),
            .nsec = (int64_t)(fake_clock_ns % 1000000000ULL),
        };
        fake_clock_ns += 1000000ULL;
        return copy_to_user(a2, &ts, sizeof(ts)) == 0 ? 0 : -EFAULT;
    }
    case SYS_GETRANDOM: {
        if (!vmm_user_range_ok(a1, a2, 1)) return -EFAULT;
        uint64_t state = 0x6a09e667f3bcc909ULL ^ a1 ^ a2;
        uint64_t ptr = a1;
        uint64_t left = a2;
        while (left) {
            uint64_t phys = vmm_user_phys(ptr);
            if (!phys) return -EFAULT;
            size_t chunk = PAGE_SIZE - (size_t)(ptr & (PAGE_SIZE - 1));
            if ((uint64_t)chunk > left) chunk = (size_t)left;
            uint8_t *out = phys_to_virt(phys);
            for (size_t i = 0; i < chunk; i++) {
                state ^= state << 13;
                state ^= state >> 7;
                state ^= state << 17;
                out[i] = (uint8_t)state;
            }
            ptr += chunk;
            left -= chunk;
        }
        return (long)a2;
    }
    case SYS_EXIT:
    case SYS_EXIT_GROUP:
        vga_puts("[kernel] userspace exited\n");
        __asm__ volatile ("outl %0, %1" : : "a"((uint32_t)((a1 & 0xff) << 1 | 1)), "Nd"((uint16_t)0xf4));
        for (;;) __asm__ volatile ("cli; hlt");
    default:
        return -ENOSYS;
    }
}

void syscall_dispatch_fast(struct fast_syscall_frame *frame) {
    long ret = dispatch_linux(frame->rax, frame->rdi, frame->rsi, frame->rdx,
                              frame->r10, frame->r8, frame->r9);
    frame->rax = (uint64_t)ret;
}

void syscall_dispatch(struct syscall_frame *frame) {
    long ret = dispatch_linux(frame->rax, frame->rdi, frame->rsi, frame->rdx,
                              frame->rcx, frame->r8, frame->r9);
    frame->rax = (uint64_t)ret;
}
