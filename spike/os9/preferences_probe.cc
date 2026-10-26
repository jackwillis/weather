// Seed-free File Manager acceptance in a newly owned disposable directory.
#include "platform/os9/settings_os9.hpp"
#include "app/preferences.hpp"
#include <Events.h>
#include <cstdio>
#include <cstring>

namespace {
enum class Fault { None, Read, Write, Close, PreFlush, Exchange, PostFlush, Remove, FinalFlush, Rename };
Fault fault=Fault::None;
int failures=0,closes=0,flushes=0,removes=0;
FILE *logfile;
void check(bool ok,const char *label) {
	std::fprintf(logfile,"%s %s\n",ok?"ok":"FAIL",label);
	if(!ok) ++failures;
	std::fflush(logfile);
}
OSErr spec(const char *path,FSSpec &out) {
	unsigned char p[256]; size_t n=std::strlen(path); if(n>255) return paramErr;
	p[0]=static_cast<unsigned char>(n); std::memcpy(p+1,path,n); return FSMakeFSSpec(0,0,p,&out);
}
OSErr read(short fd,long *n,void *p) { if(fault==Fault::Read) return ioErr; return wx::os9_settings_file_ops().read(fd,n,p); }
OSErr write(short fd,long *n,const void *p) { if(fault==Fault::Write && *n) --*n; return wx::os9_settings_file_ops().write(fd,n,p); }
OSErr close(short fd) { OSErr e=wx::os9_settings_file_ops().close(fd); return fault==Fault::Close && ++closes==1?static_cast<OSErr>(ioErr):e; }
OSErr flush(short volume) {
	++flushes;
	if((fault==Fault::PreFlush && flushes==1) || (fault==Fault::PostFlush && flushes==2) || (fault==Fault::FinalFlush && flushes==3)) return ioErr;
	return wx::os9_settings_file_ops().flush(volume);
}
OSErr exchange(FSSpec *a,FSSpec *b) { return fault==Fault::Exchange?static_cast<OSErr>(ioErr):wx::os9_settings_file_ops().exchange(a,b); }
OSErr rename(FSSpec *a,const unsigned char *name) { return fault==Fault::Rename?static_cast<OSErr>(ioErr):wx::os9_settings_file_ops().rename(a,name); }
OSErr remove(FSSpec *a) { return fault==Fault::Remove && ++removes==1?static_cast<OSErr>(ioErr):wx::os9_settings_file_ops().remove(a); }
}
int main() {
	logfile=std::fopen("MacOS9Lives:weather-dev:preferences-probe.log","w"); if(!logfile) return 2;
	char folder[180],path[220];
	std::snprintf(folder,sizeof folder,"MacOS9Lives:weather-dev:preferences-check-%lu",static_cast<unsigned long>(TickCount()));
	FSSpec dir; long id=0;
	bool owned=spec(folder,dir)==fnfErr && FSpDirCreate(&dir,0,&id)==noErr;
	check(owned,"create private diagnostic directory");
	if(!owned) { std::fclose(logfile); return 1; }
	std::snprintf(path,sizeof path,"%s:settings",folder);
    const char *before="latitude=1\n# old\n", *after="latitude=2\n# new\n";
    char loaded[4096]; size_t length=0;
	wx::Os9Settings store;
	check(wx::os9_settings_init(&store,path).ok(),"initialize diagnostic preferences only");
	
	check(store.base.load(store.base,loaded,sizeof loaded,&length).ok() && length==0,"missing preferences is harmless");
	wx::Os9SettingsFileOps ops{read,write,close,flush,exchange,rename,remove};
	fault=Fault::Rename;
	check(wx::os9_settings_init(&store,path,&ops).ok(),"initialize rename fault");
	check(!store.base.save(store.base,before,std::strlen(before)).ok(),"rename failure is reported");
	fault=Fault::None;
	check(wx::os9_settings_init(&store,path).ok(),"initialize normal preferences");
	check(store.base.save(store.base,before,std::strlen(before)).ok(),"initial installation");
	
	check(store.base.save(store.base,after,std::strlen(after)).ok(),"replace through same adapter instance");
	check(store.base.load(store.base,loaded,sizeof loaded,&length).ok() && wx::sv(loaded,length)==wx::sv(after),"same adapter reads replacement");
    // Storage boundaries, corrupt configuration and an OS-level lock on our own file.
    char large[4096]; std::memset(large,' ',sizeof large);
    check(!store.base.save(store.base,large,sizeof large).ok(),"4096-byte save rejected");
    check(store.base.save(store.base,large,sizeof large-1).ok(),"4095-byte boundary saved");
    check(store.base.load(store.base,loaded,sizeof loaded,&length).ok() && length==4095,"4095-byte boundary read");
    check(!store.base.load(store.base,loaded,4095,&length).ok() && length==0,"small reader cannot accept truncated settings");
    const char invalid[]="units=invalid\n";
    check(store.base.save(store.base,invalid,sizeof invalid-1).ok(),"install corrupt owned configuration");
    wx::SettingsStore empty; empty.load=[](wx::SettingsStore &,char *,size_t,size_t *n) { *n=0; return wx::okay(); };
    wx::Config unchanged; unchanged.lat_e4=123;
    check(!wx::load_preferences(store.base,empty,unchanged).ok() && unchanged.lat_e4==123,"corrupt preferences do not partially apply or fall back");
    check(store.base.save(store.base,before,std::strlen(before)).ok(),"restore baseline after corruption");
    FSSpec locked; bool can_lock=spec(path,locked)==noErr && FSpSetFLock(&locked)==noErr;
    check(can_lock,"lock owned Preferences file");
    if(can_lock) {
        check(!store.base.save(store.base,after,std::strlen(after)).ok(),"locked file replacement reports failure");
        check(FSpRstFLock(&locked)==noErr,"unlock owned Preferences file");
        check(store.base.load(store.base,loaded,sizeof loaded,&length).ok() && wx::sv(loaded,length)==wx::sv(before),"locked file retains original contents");
    }
	Fault faults[]={Fault::Write,Fault::Close,Fault::PreFlush,Fault::Exchange,Fault::PostFlush,Fault::Remove,Fault::FinalFlush};
	for(Fault f:faults) {
		std::fprintf(logfile,"fault=%d\n",static_cast<int>(f));
		fault=Fault::None;
		check(wx::os9_settings_init(&store,path).ok(),"open baseline writer"); 
		check(store.base.save(store.base,before,std::strlen(before)).ok(),"write baseline");
		fault=f; closes=flushes=removes=0;
		check(wx::os9_settings_init(&store,path,&ops).ok(),"open faulted writer"); 
		check(!store.base.save(store.base,after,std::strlen(after)).ok(),"injected failure is reported");
		fault=Fault::None;
		check(wx::os9_settings_init(&store,path).ok(),"reopen after failure");
		check(store.base.load(store.base,loaded,sizeof loaded,&length).ok(),"valid primary survives");
		check(wx::sv(loaded,length)==wx::sv(f==Fault::PostFlush || f==Fault::Remove || f==Fault::FinalFlush?after:before),"old or complete new record as expected");
	}
	for(Fault f:{Fault::Read,Fault::Close}) {
		fault=f; closes=0; check(wx::os9_settings_init(&store,path,&ops).ok(),"open faulted reader");
		check(!store.base.load(store.base,loaded,sizeof loaded,&length).ok() && length==0,"read/close failure is not a preferences hit");
	}
	fault=Fault::None;
	FSSpec file; check(spec(path,file)==noErr && FSpDelete(&file)==noErr,"remove diagnostic primary");
	check(FSpDelete(&dir)==noErr,"remove empty diagnostic directory");
	std::fprintf(logfile,"finished: %d failures\n",failures); std::fclose(logfile);
	return failures?1:0;
}
