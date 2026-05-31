#define __SYSCALL_LL_E(x) \
((union { long long ll; long l[2]; }){ .ll = x }).l[0], \
((union { long long ll; long l[2]; }){ .ll = x }).l[1]
#define __SYSCALL_LL_O(x) __SYSCALL_LL_E((x))

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
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(n), "b"(a), "c"(b), "d"(c), "S"(d) : "memory");
	return ret;
}

static __inline long __syscall5(long n, long a, long b, long c, long d, long e)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(n), "b"(a), "c"(b), "d"(c), "S"(d), "D"(e) : "memory");
	return ret;
}

static __inline long __syscall6(long n, long a, long b, long c, long d, long e, long f)
{
	long ret;
	register long ebp __asm__("ebp") = f;
	__asm__ __volatile__("int $0x80"
		: "=a"(ret)
		: "a"(n), "b"(a), "c"(b), "d"(c), "S"(d), "D"(e), "r"(ebp)
		: "memory");
	return ret;
}
