#include <stddef.h>
#include <stdint.h>

#define SYS_WRITE 1
#define SYS_MMAP 9
#define SYS_MPROTECT 10
#define SYS_MUNMAP 11
#define SYS_IOCTL 16
#define SYS_WRITEV 20
#define SYS_ARCH_PRCTL 158
#define SYS_SET_TID_ADDRESS 218
#define SYS_EXIT_GROUP 231

#define PROT_READ 1
#define PROT_WRITE 2
#define MAP_PRIVATE 2
#define MAP_ANONYMOUS 0x20
#define ARCH_SET_FS 0x1002
#define TIOCGWINSZ 0x5413

struct iovec64 { void *base; size_t len; };
struct winsize64 { uint16_t rows, cols, xpixel, ypixel; };

static long sc6(long n, long a, long b, long c, long d, long e, long f) {
    register long r10 __asm__("r10") = d;
    register long r8 __asm__("r8") = e;
    register long r9 __asm__("r9") = f;
    long out;
    __asm__ volatile ("syscall"
                      : "=a"(out)
                      : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8), "r"(r9)
                      : "rcx", "r11", "memory");
    return out;
}

static long sc3(long n, long a, long b, long c) { return sc6(n, a, b, c, 0, 0, 0); }
static long sc2(long n, long a, long b) { return sc6(n, a, b, 0, 0, 0, 0); }
static long sc1(long n, long a) { return sc6(n, a, 0, 0, 0, 0, 0); }

static size_t slen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
static void say(const char *s) { (void)sc3(SYS_WRITE, 1, (long)s, (long)slen(s)); }

__attribute__((noreturn)) void _start(void) {
    static const char prefix[] = "[smoke] syscall + ELF loader OK\n";
    static const char mapped_msg[] = "[smoke] mmap + writev + ioctl OK\n";
    struct winsize64 ws;

    say(prefix);
    if (sc3(SYS_IOCTL, 1, TIOCGWINSZ, (long)&ws) < 0) goto fail;

    long mapped = sc6(SYS_MMAP, 0, 4096, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapped < 0) goto fail;

    char *p = (char *)mapped;
    for (size_t i = 0; i < sizeof(mapped_msg); i++) p[i] = mapped_msg[i];
    struct iovec64 iov = { p, sizeof(mapped_msg) - 1 };
    if (sc3(SYS_WRITEV, 1, (long)&iov, 1) < 0) goto fail;
    if (sc3(SYS_MPROTECT, mapped, 4096, PROT_READ) < 0) goto fail;
    if (sc2(SYS_MUNMAP, mapped, 4096) < 0) goto fail;

    /* Exercise musl startup's two thread/TLS syscalls without touching FS after. */
    mapped = sc6(SYS_MMAP, 0, 4096, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapped < 0) goto fail;
    if (sc2(SYS_SET_TID_ADDRESS, mapped, 0) < 0) goto fail;
    if (sc2(SYS_ARCH_PRCTL, ARCH_SET_FS, mapped) < 0) goto fail;

    sc1(SYS_EXIT_GROUP, 0);
    for (;;) __asm__ volatile ("hlt");
fail:
    say("[smoke] FAIL\n");
    sc1(SYS_EXIT_GROUP, 2);
    for (;;) __asm__ volatile ("hlt");
}
