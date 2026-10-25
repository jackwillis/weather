// OS-independent controller; Backend owns fixed-lifetime OT buffers and callbacks.
#pragma once
#include <cstdint>
#include "core/status.hpp"

namespace wx {
template<class Backend> class DnsResolver {
	enum class Phase { Idle, Opening, Resolving, Quarantined };
	Backend &b_;
	Phase phase_ = Phase::Idle;

	Status wait(bool opening, uint32_t start, uint32_t budget)
	{
		for (;;) {
			// Foreground policy: cancellation, deadline, completion, in that order.
			if (!b_.yield()) return make(Err::Cancelled, "cancelled while resolving host");
			if (static_cast<uint32_t>(b_.now() - start) >= budget)
				return make(Err::Timeout, "host name lookup timed out");
			if (opening ? b_.opened() : b_.resolved()) return okay();
			b_.sleep();
		}
	}

	Status close()
	{
		int e = b_.close();
		phase_ = e ? Phase::Quarantined : Phase::Idle;
		return e ? make(Err::Dns, "cannot close name service", e) : okay();
	}

public:
	explicit DnsResolver(Backend &b) : b_(b) {}
	Status resolve(const char *host, uint32_t budget)
	{
		uint32_t start = b_.now();
		if (phase_ == Phase::Quarantined) return make(Err::Dns, "name service cleanup failed; restart required");
		// A previously abandoned async open still owns its callback context. Drain
		// it before resetting any buffers or opening another provider.
		if (phase_ == Phase::Opening) {
			Status s = wait(true, start, budget);
			if (!s.ok()) return s;
			s = close();
			if (!s.ok()) return s;
		}
		if (!b_.yield()) return make(Err::Cancelled, "cancelled before host lookup");
		if (static_cast<uint32_t>(b_.now() - start) >= budget)
			return make(Err::Timeout, "host name lookup timed out");
		phase_ = Phase::Opening;
		int e = b_.open(host);
		if (e) { phase_ = Phase::Idle; return make(Err::Dns, "cannot open name service", e); }
		Status s = wait(true, start, budget);
		if (!s.ok()) return s; // retain pending open and its storage
		e = b_.open_result();
		if (e) { close(); return make(Err::Dns, "cannot open name service", e); }
		phase_ = Phase::Resolving;
		e = b_.query();
		if (e) { close(); return make(Err::Dns, "cannot start host lookup", e); }
		s = wait(false, start, budget);
		if (s.ok()) {
			e = b_.query_result();
			if (e || !b_.has_address()) s = make(Err::Dns, "cannot resolve host name", e);
		}
		Status cleanup = close();
		return s.ok() ? cleanup : s;
	}
};
} // namespace wx
