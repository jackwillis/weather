// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// TLS spike: resolve, connect, BearSSL handshake, HTTP/1.0 GET, dump body.
// usage: tlsfetch HOST PORT PATH     (logs to stderr, body to stdout)
#include <bearssl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include "ta.h"

static int sock_read(void *ctx, unsigned char *buf, size_t len)
{
	int fd = *static_cast<int *>(ctx);
	for (;;) {
		struct pollfd p = { fd, POLLIN, 0 };
		int r = poll(&p, 1, 15000);
		if (r == 0) { fprintf(stderr, "read timeout\n"); return -1; }
		if (r < 0) { if (errno == EINTR) continue; return -1; }
		ssize_t n = read(fd, buf, len);
		if (n < 0 && errno == EINTR) continue;
		return n <= 0 ? -1 : static_cast<int>(n);
	}
}

static int sock_write(void *ctx, const unsigned char *buf, size_t len)
{
	int fd = *static_cast<int *>(ctx);
	for (;;) {
		ssize_t n = write(fd, buf, len);
		if (n < 0 && errno == EINTR) continue;
		return n <= 0 ? -1 : static_cast<int>(n);
	}
}

int main(int argc, char **argv)
{
	if (argc != 4) { fprintf(stderr, "usage: %s host port path\n", argv[0]); return 64; }
	const char *host = argv[1], *port = argv[2], *path = argv[3];

	struct addrinfo hints, *res;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	int gai = getaddrinfo(host, port, &hints, &res);
	if (gai != 0) { fprintf(stderr, "dns: %s\n", gai_strerror(gai)); return 69; }
	int fd = -1;
	for (struct addrinfo *a = res; a; a = a->ai_next) {
		fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
		if (fd < 0) continue;
		if (connect(fd, a->ai_addr, a->ai_addrlen) == 0) break;
		close(fd); fd = -1;
	}
	freeaddrinfo(res);
	if (fd < 0) { fprintf(stderr, "connect failed: %s\n", strerror(errno)); return 69; }
	fprintf(stderr, "connected, system time %ld\n", static_cast<long>(time(NULL)));

	static br_ssl_client_context sc;
	static br_x509_minimal_context xc;
	static unsigned char iobuf[BR_SSL_BUFSIZE_BIDI];
	static br_sslio_context ioc;
	// WEATHER_NO_TA=1: empty trust set, to prove verification really rejects
	br_ssl_client_init_full(&sc, &xc, TAs, getenv("WEATHER_NO_TA") ? 0 : TAs_NUM);
	br_ssl_engine_set_buffer(&sc.eng, iobuf, sizeof iobuf, 1);
	if (!br_ssl_client_reset(&sc, host, 0)) {
		fprintf(stderr, "tls reset failed: err %d\n", br_ssl_engine_last_error(&sc.eng));
		return 70;
	}
	br_sslio_init(&ioc, &sc.eng, sock_read, &fd, sock_write, &fd);

	char req[512];
	int rl = snprintf(req, sizeof req,
	    "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: weather-spike/0\r\nConnection: close\r\n\r\n",
	    path, host);
	if (br_sslio_write_all(&ioc, req, rl) < 0 || br_sslio_flush(&ioc) < 0) {
		int e = br_ssl_engine_last_error(&sc.eng);
		fprintf(stderr, "tls write failed: engine err %d%s\n", e,
		    e == BR_ERR_X509_NOT_TRUSTED ? " (certificate not trusted)" : "");
		return 74;
	}
	br_ssl_session_parameters sp;
	br_ssl_engine_get_session_parameters(&sc.eng, &sp);
	fprintf(stderr, "handshake ok: version 0x%04x cipher 0x%04x\n", sp.version, sp.cipher_suite);

	char buf[1024];
	long total = 0;
	int n;
	while ((n = br_sslio_read(&ioc, buf, sizeof buf)) > 0) {
		fwrite(buf, 1, n, stdout);
		total += n;
	}
	int err = br_ssl_engine_last_error(&sc.eng);
	fprintf(stderr, "\nread %ld bytes, engine err %d (%s)\n", total, err,
	    err == BR_ERR_OK ? "ok" : err == BR_ERR_IO ? "io/closed" : "see bearssl.h");
	close(fd);
	return (total > 0 && (err == BR_ERR_OK || err == BR_ERR_IO)) ? 0 : 74;
}
