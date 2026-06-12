#define __SYSCALL_LL_E(x) (x)
#define __SYSCALL_LL_O(x) (x)

static __inline long __syscall0(long n)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(n) : "memory");
	return ret;
}

static __inline long __syscall1(long n, long a)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(n), "b"(a) : "memory");
	return ret;
}

static __inline long __syscall2(long n, long a, long b)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(n), "b"(a), "c"(b) : "memory");
	return ret;
}

static __inline long __syscall3(long n, long a, long b, long c)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
	return ret;
}

static __inline long __syscall4(long n, long a, long b, long c, long d)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(n), "b"(a), "c"(b), "d"(c), "D"(d) : "memory");
	return ret;
}

static __inline long __syscall5(long n, long a, long b, long c, long d, long e)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(n), "b"(a), "c"(b), "d"(c), "D"(d), "S"(e) : "memory");
	return ret;
}

static __inline long __syscall6(long n, long a, long b, long c, long d, long e, long f)
{
	long ret;
	register long r8 __asm__("r8") = f;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(n), "b"(a), "c"(b), "d"(c), "D"(d), "S"(e), "r"(r8) : "memory");
	return ret;
}
