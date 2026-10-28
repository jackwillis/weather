// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
#include "cache_os9.hpp"
#include <Events.h>
#include <Processes.h>
#include <cstdio>
#include <cstring>

namespace wx {
namespace {
OSErr read_file(short fd,long *n,void *p) { return FSRead(fd,n,p); }
OSErr write_file(short fd,long *n,const void *p) { return FSWrite(fd,n,p); }
OSErr close_file(short fd) { return FSClose(fd); }
OSErr flush_volume(short volume) { return FlushVol(nullptr,volume); }
OSErr exchange_files(FSSpec *a,FSSpec *b) { return FSpExchangeFiles(a,b); }
OSErr rename_file(FSSpec *a,const unsigned char *name) { return FSpRename(a,name); }
OSErr remove_file(FSSpec *a) { return FSpDelete(a); }
const Os9CacheFileOps syscalls{read_file,write_file,close_file,flush_volume,exchange_files,rename_file,remove_file};
OSErr make_spec(short volume,long directory,const char *name,FSSpec &out)
{
	size_t n=std::strlen(name); if(n>255) return paramErr;
	unsigned char p[256]; p[0]=static_cast<unsigned char>(n); std::memcpy(p+1,name,n);
	return FSMakeFSSpec(volume,directory,p,&out);
}
Status remove(CacheStore &base)
{
    auto &store=*static_cast<Os9Cache *>(base.ctx);
    FSSpec file=store.file;
    OSErr err=store.ops->remove(&file);
    if(err==fnfErr) return okay();
    if(!err) err=store.ops->flush(store.file.vRefNum);
    return err ? make(Err::Io,"cannot remove saved forecast",err) : okay();
}
Status load(CacheStore &base,unsigned char *out,size_t cap,size_t *n)
{
	auto &store=*static_cast<Os9Cache *>(base.ctx); *n=0;
	short fd=0; OSErr err=FSpOpenDF(&store.file,fsRdPerm,&fd);
	if(err==fnfErr) return okay();
	if(err) return make(Err::Io,"cannot open forecast cache",err);
	long length=0; err=GetEOF(fd,&length);
	Status status;
	if(err) status=make(Err::Io,"cannot size forecast cache",err);
	else if(length<0 || static_cast<unsigned long>(length)>=cap) status=make(Err::BadData,"forecast cache too large");
	long count=length;
	if(status.ok() && count) {
		err=store.ops->read(fd,&count,out);
		if(err || count!=length) status=make(Err::Io,"cannot read forecast cache",err);
	}
	OSErr closed=store.ops->close(fd);
	if(status.ok() && closed) status=make(Err::Io,"cannot close forecast cache",closed);
	if(status.ok()) *n=static_cast<size_t>(count);
	return status;
}
Status save(CacheStore &base,const unsigned char *data,size_t n)
{
	auto &store=*static_cast<Os9Cache *>(base.ctx);
	if(n!=kForecastCacheBytes && n!=kForecastStoreBytes) return make(Err::BadData,"invalid forecast cache length");
	ProcessSerialNumber process;
	OSErr err=GetCurrentProcess(&process);
	if(err) return make(Err::Io,"cannot identify cache writer",err);
	char name[48];
	std::snprintf(name,sizeof name,"WXC%lx-%lx-%lx",static_cast<unsigned long>(process.highLongOfPSN),
	              static_cast<unsigned long>(process.lowLongOfPSN),static_cast<unsigned long>(TickCount()));
	FSSpec temp;
	err=make_spec(store.file.vRefNum,store.file.parID,name,temp);
	if(err!=fnfErr) return make(Err::Io,"cannot create unique forecast cache temporary file",err?err:static_cast<OSErr>(dupFNErr));
	err=FSpCreate(&temp,'Wthr','WXFC',0);
	if(err) return make(Err::Io,"cannot create forecast cache temporary file",err);
	short fd=0; err=FSpOpenDF(&temp,fsWrPerm,&fd);
	if(!err) {
		long count=static_cast<long>(n);
		err=store.ops->write(fd,&count,data);
		if(!err && count!=static_cast<long>(n)) err=ioErr;
		OSErr closed=store.ops->close(fd);
		if(!err) err=closed;
	}
	if(!err) err=store.ops->flush(temp.vRefNum);
	if(!err) {
		FInfo info; OSErr exists=FSpGetFInfo(&store.file,&info);
		if(exists==noErr) {
			FSSpec primary=store.file,replacement=temp;
			err=store.ops->exchange(&primary,&replacement);
		}
		else if(exists==fnfErr) {
			FSSpec moving=temp;
			err=store.ops->rename(&moving,store.file.name);
			// Rename consumed the temporary path; do not delete the new primary.
			if(!err) {
				OSErr flushed=store.ops->flush(store.file.vRefNum);
				return flushed?make(Err::Io,"cannot flush cache installation",flushed):okay();
			}
		} else err=exists;
	}
	if(!err) err=store.ops->flush(store.file.vRefNum);
	if(!err) err=store.ops->remove(&temp);
	if(!err) err=store.ops->flush(store.file.vRefNum);
	if(err) {
		// This path belongs only to this call, never the primary or any seed file.
		store.ops->remove(&temp);
		return make(Err::Io,"cannot replace forecast cache safely",err);
	}
	return okay();
}
}
const Os9CacheFileOps &os9_cache_file_ops() { return syscalls; }
Status os9_cache_init(Os9Cache *store,const char *path,const Os9CacheFileOps *ops)
{
	*store=Os9Cache{}; store->ops=ops?ops:&syscalls;
	OSErr err;
	if(path) err=make_spec(0,0,path,store->file);
	else {
		short volume=0; long directory=0;
		err=FindFolder(-32768,'pref',false,&volume,&directory);
		if(err) return make(Err::Io,"cannot locate Preferences for forecast cache",err);
		err=make_spec(volume,directory,"Weather Forecast Cache",store->file);
	}
	if(err && err!=fnfErr) return make(Err::Io,"cannot locate forecast cache",err);
	store->base.ctx=store; store->base.load=load; store->base.save=save; store->base.remove=remove;
	return okay();
}
}
