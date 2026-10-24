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
};

} // namespace wx
