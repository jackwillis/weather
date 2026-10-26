// The app model: everything a UI needs to decide, with no drawing and no OS calls.
// Phases: Idle -> Fetching -> Ok | Failed. The UI owns the (blocking, yielding) fetch:
//
//     if (m.tick(now)) {                      // true = start a fetch now (phase is Fetching)
//         Status s = fetch_forecast(...);
//         m.finish(now, s, &forecast);
//     }
//
// While Fetching, tick() never starts another fetch and request_refresh() is a no-op, so a
// click on Refresh during a fetch (the UI keeps pumping events through Net::yield) is harmless.
#pragma once
#include "core/forecast.hpp"
#include "core/status.hpp"
#include "core/text.hpp"

namespace wx {

enum class Phase { Idle, Fetching, Ok, Failed };

struct ModelConfig {
	bool auto_refresh = true;
	long interval_s = 900;         // 15 min: Open-Meteo updates its "current" data every 15 min
	long backoff_base_s = 60;      // after a failure retry in 60 s, 120 s, 240 s ... capped at interval_s
};

class Model {
public:
	explicit Model(const ModelConfig &c = ModelConfig(), long long now = 0) : cfg_(c) { start(now); }

	void start(long long now) { phase_ = Phase::Idle; next_at_ = now; want_ = true; }

	// Manual refresh. Ignored while a fetch is running.
	void request_refresh() { if (phase_ != Phase::Fetching) want_ = true; }

	// Pausing automatic updates does not move the pending success/retry deadline.
	// On resume, tick() immediately fetches if that deadline has passed.
	void set_auto_refresh(bool on, long long /*now*/)
	{
		cfg_.auto_refresh = on;
	}

	// The location changed: the old forecast describes somewhere else. Forget it and refetch.
	void location_changed()
	{
		have_ = false;
		failures_ = 0;
		if (phase_ != Phase::Fetching) { phase_ = Phase::Idle; want_ = true; }
		else pending_reset_ = true;
	}

	// Call regularly (e.g. once a second, or on a timer). True means: run the fetch now.
	bool tick(long long now)
	{
		if (phase_ == Phase::Fetching) return false;
		// A clock that jumped backwards must not postpone the next refresh by years.
		if (next_at_ > now + cfg_.interval_s) next_at_ = now + cfg_.interval_s;
		bool due = want_ || (cfg_.auto_refresh && now >= next_at_);
		if (!due) return false;
		want_ = false;
		prev_ = phase_;
		phase_ = Phase::Fetching;
		return true;
	}

	void finish(long long now, const Status &s, const Forecast *f)
	{
		if (pending_reset_) {
			// The location changed while this fetch ran: its answer is about somewhere else.
			pending_reset_ = false;
			have_ = false;
			phase_ = Phase::Idle;
			want_ = true;
			return;
		}
		if (s.ok() && f) {
			fc_ = *f; have_ = true; at_ = now; failures_ = 0;
			phase_ = Phase::Ok; last_ = okay();
			next_at_ = now + cfg_.interval_s;
		} else if (s.err == Err::Cancelled) {
			// Not a failure: do not back off, do not hammer. Return to what we showed before.
			phase_ = have_ ? (prev_ == Phase::Failed ? Phase::Failed : Phase::Ok) : Phase::Idle;
			next_at_ = now + cfg_.interval_s;
		} else {
			failures_++;
			last_ = s;
			phase_ = Phase::Failed;
			long wait = cfg_.interval_s;
			if (retryable(s)) {
				wait = cfg_.backoff_base_s;
				for (int i = 1; i < failures_ && wait < cfg_.interval_s; i++) wait *= 2;
				if (wait > cfg_.interval_s) wait = cfg_.interval_s;
			}   // a permanent error (bad certificate, bad data) is not retried faster than normal
			next_at_ = now + wait;
		}
	}

	Phase phase() const { return phase_; }
	bool has_forecast() const { return have_; }
	const Forecast &forecast() const { return fc_; }
	const Status &last_error() const { return last_; }
	int failures() const { return failures_; }
	long long next_refresh_at() const { return next_at_; }

	// Seconds since the data was fetched; -1 if there is none. Never negative: see age_unknown().
	long age(long long now) const { return have_ ? (now >= at_ ? static_cast<long>(now - at_) : 0) : -1; }
	// Showing data that may no longer be true: the last refresh failed, it is over two intervals old, or the clock was
	// stepped back past the moment it was fetched (its age is then unknown).
	bool stale(long long now) const { return have_ && (phase_ == Phase::Failed || age_unknown(now) || age(now) > 2 * cfg_.interval_s); }
	// Seconds until the next automatic refresh; -1 if automatic refresh is off or a fetch is running.
	long seconds_until_refresh(long long now) const
	{
		if (!cfg_.auto_refresh || phase_ == Phase::Fetching) return -1;
		return next_at_ > now ? static_cast<long>(next_at_ - now) : 0;
	}

	// One line for the window's status bar.
	template <size_t N>
	void status_line(Buf<N> &o, long long now) const
	{
		switch (phase_) {
		case Phase::Idle:
			o.add("Waiting to load"); break;
		case Phase::Fetching:
			o.add("Fetching"); if (have_) { o.add(" (showing data from "); add_data_age(o, now); o.add(")"); } else o.add("..."); break;
		case Phase::Ok:
			o.add("Updated "); if (age_unknown(now)) o.add("at "); add_data_age(o, now); break;
		case Phase::Failed:
			o.add("Update failed: ").add(last_.msg);
			if (have_) { o.add(". Showing data from "); add_data_age(o, now); }
			break;
		}
		long left = seconds_until_refresh(now);
		if (left >= 0 && phase_ != Phase::Idle) {
			o.add(". Next update in ");
			add_dur(o, left);
		}
	}

	// How old the data is, in words; "an unknown time" if the clock was stepped back since it was fetched.
	template <size_t N>
	void add_data_age(Buf<N> &o, long long now) const
	{
		if (age_unknown(now)) o.add("an unknown time, clock changed");
		else add_age(o, age(now));
	}

	template <size_t N>
	static void add_dur(Buf<N> &o, long secs)
	{
		if (secs < 60) o.add_int(secs).add(" s");
		else if (secs < 3600) o.add_int((secs + 30) / 60).add(" min");
		else { o.add_int(secs / 3600).add(" h"); if (secs % 3600 >= 60) o.add(' ').add_int(secs % 3600 / 60).add(" min"); }
	}
	template <size_t N>
	static void add_age(Buf<N> &o, long secs)
	{
		if (secs < 30) o.add("just now"); else { add_dur(o, secs); o.add(" ago"); }
	}

private:
	// The clock now reads earlier than when the data was fetched, so how old it is cannot be known.
	bool age_unknown(long long now) const { return have_ && now < at_; }

	ModelConfig cfg_;
	Phase phase_ = Phase::Idle, prev_ = Phase::Idle;
	Forecast fc_;
	bool have_ = false, want_ = true, pending_reset_ = false;
	long long at_ = 0, next_at_ = 0;
	int failures_ = 0;
	Status last_;
};

} // namespace wx
