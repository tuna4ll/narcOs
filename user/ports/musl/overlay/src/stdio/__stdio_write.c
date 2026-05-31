#include "stdio_impl.h"
#include "syscall.h"

static ssize_t write_all(int fd, const unsigned char *buf, size_t len)
{
	size_t done = 0;

	while (done < len) {
		ssize_t cnt = syscall(SYS_write, fd, buf + done, len - done);
		if (cnt <= 0) return done ? (ssize_t)done : cnt;
		done += cnt;
	}
	return done;
}

size_t __stdio_write(FILE *f, const unsigned char *buf, size_t len)
{
	size_t buffered = f->wpos - f->wbase;
	ssize_t cnt;

	cnt = buffered ? write_all(f->fd, f->wbase, buffered) : 0;
	if (cnt != (ssize_t)buffered) {
		f->wpos = f->wbase = f->wend = 0;
		f->flags |= F_ERR;
		return 0;
	}

	cnt = len ? write_all(f->fd, buf, len) : 0;
	if (cnt != (ssize_t)len) {
		f->wpos = f->wbase = f->wend = 0;
		f->flags |= F_ERR;
		return cnt > 0 ? (size_t)cnt : 0;
	}

	f->wend = f->buf + f->buf_size;
	f->wpos = f->wbase = f->buf;
	return len;
}
