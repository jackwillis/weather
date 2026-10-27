// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
#include "cache_posix.hpp"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace wx {
namespace {
const CacheFileOps syscalls{::read,::write,::fsync,::close,::rename};
Status load(CacheStore &base,unsigned char *out,size_t cap,size_t *n)
{
	auto &store=*static_cast<PosixCache *>(base.ctx); *n=0;
	int flags=O_RDONLY|O_NONBLOCK;
#ifdef O_NOFOLLOW
	flags|=O_NOFOLLOW;
#endif
	int fd=open(store.path,flags);
	if(fd<0) return errno==ENOENT?okay():make(Err::Io,"cannot open forecast cache",errno);
	struct stat info;
	if(fstat(fd,&info) || !S_ISREG(info.st_mode)) { store.ops->close(fd); return make(Err::Io,"cache is not a readable regular file"); }
	Status status;
	while(*n<cap) {
		ssize_t count=store.ops->read(fd,out+*n,cap-*n);
		if(count<0 && errno==EINTR) continue;
		if(count<0) { status=make(Err::Io,"cannot read forecast cache",errno); break; }
		if(!count) break;
		*n+=static_cast<size_t>(count);
	}
	int closed=store.ops->close(fd);
	if(status.ok() && closed) status=make(Err::Io,"cannot close forecast cache",errno);
	return status;
}
Status save(CacheStore &base,const unsigned char *data,size_t n)
{
	auto &store=*static_cast<PosixCache *>(base.ctx);
	char temp[1100];
	if(std::snprintf(temp,sizeof temp,"%s.tmp.XXXXXX",store.path)>=static_cast<int>(sizeof temp)) return make(Err::Io,"cache path too long");
	int fd=mkstemp(temp);
	if(fd<0) return make(Err::Io,"cannot create forecast cache temporary file",errno);
	Status status; size_t done=0;
	while(done<n) {
		ssize_t count=store.ops->write(fd,data+done,n-done);
		if(count<0 && errno==EINTR) continue;
		if(count<=0) { status=make(Err::Io,"cannot write forecast cache",errno); break; }
		done+=static_cast<size_t>(count);
	}
	if(status.ok() && store.ops->sync(fd)) status=make(Err::Io,"cannot flush forecast cache",errno);
	if(store.ops->close(fd) && status.ok()) status=make(Err::Io,"cannot close forecast cache",errno);
	if(status.ok() && store.ops->replace(temp,store.path)) status=make(Err::Io,"cannot replace forecast cache",errno);
	if(!status.ok()) { unlink(temp); return status; }
	// Rename already left a complete valid new record. Failure to flush the
	// parent is still reported; do not claim durable success or roll back.
	char parent[1024]; std::strcpy(parent,store.path);
	char *slash=std::strrchr(parent,'/');
	if(!slash) std::strcpy(parent,".");
	else if(slash==parent) slash[1]=0;
	else *slash=0;
	int directory=open(parent,O_RDONLY|O_DIRECTORY);
	if(directory<0) return make(Err::Io,"cannot open cache directory",errno);
	if(store.ops->sync(directory)) status=make(Err::Io,"cannot flush cache directory",errno);
	if(store.ops->close(directory) && status.ok()) status=make(Err::Io,"cannot close cache directory",errno);
	return status;
}
}
const CacheFileOps &posix_cache_file_ops() { return syscalls; }
Status posix_cache_init(PosixCache *store,const char *path,const CacheFileOps *ops)
{
	*store=PosixCache{};
	if(!path || !*path || std::strlen(path)>=sizeof store->path) return make(Err::Config,"forecast cache path missing or too long");
	std::strcpy(store->path,path); store->ops=ops?ops:&syscalls;
	store->base.ctx=store; store->base.load=load; store->base.save=save;
	return okay();
}
}
