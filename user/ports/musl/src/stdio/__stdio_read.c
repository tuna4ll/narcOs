#include "stdio_impl.h"
#include "syscall.h"

size_t __stdio_read(FILE *f, unsigned char *buf, size_t len)
{
	ssize_t cnt;
	size_t first_len;

	if (!f->buf_size) {
		cnt = syscall(SYS_read, f->fd, buf, len);
		if (cnt <= 0) {
			f->flags |= cnt ? F_ERR : F_EOF;
			return 0;
		}
		return cnt;
	}

	first_len = len - 1;
	cnt = first_len ? syscall(SYS_read, f->fd, buf, first_len) : 0;
	if (cnt < 0) {
		f->flags |= F_ERR;
		return 0;
	}
	if ((size_t)cnt == first_len) {
		ssize_t extra = syscall(SYS_read, f->fd, f->buf, f->buf_size);
		if (extra > 0) {
			f->rpos = f->buf;
			f->rend = f->buf + extra;
			buf[len - 1] = *f->rpos++;
			return len;
		}
		if (extra < 0) f->flags |= F_ERR;
		else f->flags |= F_EOF;
		return cnt;
	}
	if (cnt == 0) f->flags |= F_EOF;
	return cnt;
}
