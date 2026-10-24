// ~/.weatherrc on Unix. Saves atomically (temp file in the same directory, fsync, rename),
// keeps the file's mode, and follows a symlinked dotfile instead of replacing the link.
#include "settings_posix.hpp"
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace wx {

static Status px_load(SettingsStore &st, char *buf, size_t cap, size_t *n)
{
	PosixStore *p = reinterpret_cast<PosixStore *>(&st);
	*n = 0;
	FILE *f = fopen(p->path, "rb");
	if (!f) {
		if (errno == ENOENT && !p->must_exist) return okay();
		if (errno == ENOENT) return make(Err::Config, "cannot open settings file");
		return make(Err::Io, "cannot open settings file", errno);
	}
	size_t got = fread(buf, 1, cap - 1, f);
	bool big = !feof(f);
	bool bad = ferror(f);
	fclose(f);
	if (bad) return make(Err::Io, "cannot read settings file", errno);
	if (big) return make(Err::Config, "settings file too large");
	buf[got] = 0;
	*n = got;
	return okay();
}

static Status px_save(SettingsStore &st, const char *text, size_t n)
{
	PosixStore *p = reinterpret_cast<PosixStore *>(&st);
	char real[PATH_MAX];
	if (!realpath(p->path, real)) {            // not there yet: write at the plain path
		if (errno != ENOENT) return make(Err::Io, "cannot resolve settings path", errno);
		snprintf(real, sizeof real, "%s", p->path);
	}
	mode_t mode = 0644;
	struct stat sb;
	if (stat(real, &sb) == 0) mode = sb.st_mode & 0777;
	char tmp[PATH_MAX + 32];
	snprintf(tmp, sizeof tmp, "%s.tmp.%ld", real, static_cast<long>(getpid()));
	int fd = open(tmp, O_WRONLY | O_CREAT | O_EXCL, mode);
	if (fd < 0) return make(Err::Io, "cannot create temporary settings file", errno);
	size_t done = 0;
	while (done < n) {
		ssize_t w = write(fd, text + done, n - done);
		if (w < 0 && errno == EINTR) continue;
		if (w <= 0) { int e = errno; close(fd); unlink(tmp); return make(Err::Io, "cannot write settings file", e); }
		done += static_cast<size_t>(w);
	}
	if (fsync(fd) != 0) { int e = errno; close(fd); unlink(tmp); return make(Err::Io, "cannot sync settings file", e); }
	if (close(fd) != 0) { int e = errno; unlink(tmp); return make(Err::Io, "cannot close settings file", e); }
	if (rename(tmp, real) != 0) { int e = errno; unlink(tmp); return make(Err::Io, "cannot replace settings file", e); }
	return okay();
}

Status posix_settings_init(PosixStore *s, const char *path, bool must_exist)
{
	*s = PosixStore();
	s->base.load = px_load;
	s->base.save = px_save;
	s->must_exist = must_exist;
	if (path) {
		if (strlen(path) >= sizeof s->path) return make(Err::Config, "settings path too long");
		strcpy(s->path, path);
		return okay();
	}
	const char *home = getenv("HOME");
	if (!home || !*home) return make(Err::Config, "HOME is not set; use -c FILE");
	if (snprintf(s->path, sizeof s->path, "%s/.weatherrc", home) >= static_cast<int>(sizeof s->path))
		return make(Err::Config, "settings path too long");
	return okay();
}

} // namespace wx
