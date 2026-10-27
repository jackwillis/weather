// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// A data source: where to ask, what to ask, how to read the answer.
#pragma once
#include "forecast.hpp"
#include "status.hpp"

namespace wx {

struct Query {
	long lat_e4 = 0;   // degrees * 10^4
	long lon_e4 = 0;
	bool metric = false;
};

struct Provider {
	const char *name;
	const char *host;
	const char *port;
	// Write the request path (with query) into out; false if it does not fit.
	bool (*path)(const Query &, char *out, size_t cap);
	// Turn a response body into a Forecast.
	Status (*parse)(sv body, const Query &, Forecast *);
	// The service's terms require identifying ourselves: we refuse to ask until 'contact' is set.
	bool needs_contact = false;
};

// "open-meteo,other" -> pointers into `all`, in order. Unknown names are a usage error.
inline Status resolve_providers(const char *list, const Provider *const *all, size_t nall,
    const Provider **out, size_t cap, size_t *n)
{
	*n = 0;
	const char *p = list;
	while (*p) {
		const char *e = p;
		while (*e && *e != ',') e++;
		sv name = trim(sv(p, static_cast<size_t>(e - p)));
		if (!name.empty()) {
			const Provider *found = nullptr;
			for (size_t i = 0; i < nall; i++)
				if (name == sv(all[i]->name)) found = all[i];
			if (!found) return make(Err::Usage, "unknown data source");
			if (*n >= cap) return make(Err::Usage, "too many data sources");
			out[(*n)++] = found;
		}
		p = *e ? e + 1 : e;
	}
	if (*n == 0) return make(Err::Usage, "no data source given");
	return okay();
}

} // namespace wx
