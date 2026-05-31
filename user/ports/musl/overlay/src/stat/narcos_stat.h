#ifndef NARCOS_MUSL_STAT_BRIDGE_H
#define NARCOS_MUSL_STAT_BRIDGE_H

#include <stdint.h>
#include <string.h>
#include <sys/stat.h>

struct narcos_kstat {
	uint32_t size;
	uint32_t mode;
	uint32_t nlink;
	uint32_t uid;
	uint32_t gid;
	uint32_t rdev;
	uint64_t ino;
	uint64_t dev;
	uint64_t file_size;
	uint64_t blksize;
	uint64_t blocks;
	int64_t atime_sec;
	int64_t atime_nsec;
	int64_t mtime_sec;
	int64_t mtime_nsec;
	int64_t ctime_sec;
	int64_t ctime_nsec;
};

static void narcos_copy_stat(struct stat *st, const struct narcos_kstat *kst)
{
	memset(st, 0, sizeof(*st));
	st->st_dev = kst->dev;
	st->st_ino = kst->ino;
	st->st_mode = kst->mode;
	st->st_nlink = kst->nlink;
	st->st_uid = kst->uid;
	st->st_gid = kst->gid;
	st->st_rdev = kst->rdev;
	st->st_size = kst->file_size;
	st->st_blksize = kst->blksize;
	st->st_blocks = kst->blocks;
	st->st_atim.tv_sec = kst->atime_sec;
	st->st_atim.tv_nsec = kst->atime_nsec;
	st->st_mtim.tv_sec = kst->mtime_sec;
	st->st_mtim.tv_nsec = kst->mtime_nsec;
	st->st_ctim.tv_sec = kst->ctime_sec;
	st->st_ctim.tv_nsec = kst->ctime_nsec;
}

#endif
