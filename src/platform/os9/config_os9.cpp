#include "config_os9.hpp"
#include <Files.h>
#include <cstring>

namespace wx {
namespace {
Status load(SettingsStore &store, char *buf, size_t cap, size_t *n)
{
	*n = 0;
	FSSpec spec;
	const char *path=static_cast<const char *>(store.ctx);
	size_t length_name=std::strlen(path);
	if(length_name>255) return make(Err::Config,"settings path too long");
	unsigned char name[256]; name[0]=static_cast<unsigned char>(length_name);
	std::memcpy(name+1,path,length_name);
	OSErr err = FSMakeFSSpec(0,0,name,&spec);
	if (err == fnfErr) return okay();
	if (err) return make(Err::Config,"cannot locate weather.conf",err);
	short ref = 0;
	err = FSpOpenDF(&spec,fsRdPerm,&ref);
	if (err) return make(Err::Config,"cannot open weather.conf",err);
	long length = 0;
	err = GetEOF(ref,&length);
	if (err || length < 0 || static_cast<unsigned long>(length) >= cap) {
		FSClose(ref);
		return make(Err::Config,err ? "cannot size weather.conf" : "weather.conf too large (max 4095 bytes)",err);
	}
	long count = length;
	if (count) err = FSRead(ref,&count,buf);
	OSErr closed = FSClose(ref);
	if (err || closed || count != length) return make(Err::Config,"cannot read weather.conf",err ? err : closed);
	*n = static_cast<size_t>(count);
	return okay();
}
}
SettingsStore os9_config_store(const char *path)
{
	SettingsStore store; store.ctx=const_cast<char *>(path); store.load = load; return store;
}
}
