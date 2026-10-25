#pragma once
#include "app/cache_store.hpp"
#include <sys/types.h>

namespace wx {
// Narrow syscall seam permits native short-write/close/replace fault tests.
// Production always uses the default table; no environment/file activates it.
struct CacheFileOps {
	ssize_t (*read)(int,void *,size_t);
	ssize_t (*write)(int,const void *,size_t);
	int (*sync)(int);
	int (*close)(int);
	int (*replace)(const char *,const char *);
};
const CacheFileOps &posix_cache_file_ops();
struct PosixCache {
	CacheStore base;
	char path[1024] = {};
	const CacheFileOps *ops = nullptr;
};
Status posix_cache_init(PosixCache *,const char *path,const CacheFileOps *ops = nullptr);
}
