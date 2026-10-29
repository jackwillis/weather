// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Session activity only: bounded storage and canonical messages, no diagnostic text.
#pragma once
#include <cstddef>
#include "core/status.hpp"

namespace wx {

struct ActivityTime { long long utc = 0; bool trustworthy = false; };
enum class ActivitySeverity { Info, Warning };
enum class ActivityCategory { Lifecycle, Weather, Preferences };
enum class ActivityEvent {
    Startup, ForecastRestored, RefreshStarted, RefreshSucceeded, RefreshFailed,
    SourceFallback, StaleForecast, PreferencesChanged, Shutdown
};

inline const char *activity_text(ActivityEvent event, Err error)
{
    switch (event) {
    case ActivityEvent::Startup: return "Weather started";
    case ActivityEvent::ForecastRestored: return "Saved forecast restored";
    case ActivityEvent::RefreshStarted: return "Refreshing weather";
    case ActivityEvent::RefreshSucceeded: return "Forecast updated";
    case ActivityEvent::SourceFallback: return "Trying another weather source";
    case ActivityEvent::StaleForecast: return "Showing an earlier forecast";
    case ActivityEvent::PreferencesChanged: return "Preferences updated";
    case ActivityEvent::Shutdown: return "Weather closing";
    case ActivityEvent::RefreshFailed:
        switch (error) {
        case Err::Cancelled: return "Weather update cancelled";
        case Err::Clock: return "Weather update failed: check the system clock";
        case Err::Dns: return "Weather update failed: name lookup failed";
        case Err::Connect: return "Weather update failed: connection failed";
        case Err::Timeout: return "Weather update failed: connection timed out";
        case Err::Tls: return "Weather update failed: secure connection failed";
        case Err::CertUntrusted: return "Weather update failed: certificate rejected";
        case Err::Config: case Err::Usage: return "Weather update failed: check preferences";
        case Err::HttpStatus: return "Weather update failed: server returned an error";
        case Err::Truncated: case Err::Parse: case Err::BadData:
            return "Weather update failed: unusable weather response";
        case Err::Io: return "Weather update failed: connection interrupted";
        case Err::NoSource: return "Weather update failed: no weather source available";
        case Err::Ok: break;
        }
        return "Weather update failed";
    }
    return "Weather activity";
}

// Returns false on truncation; a nonempty output buffer is always terminated.
inline bool activity_message(ActivityEvent event, Err error, char *out, size_t capacity)
{
    if (!out || !capacity) return false;
    const char *text = activity_text(event, error);
    size_t i = 0;
    while (text[i] && i + 1 < capacity) { out[i] = text[i]; ++i; }
    out[i] = 0;
    return !text[i];
}

inline bool activity_time_valid(ActivityTime time)
{
    return time.trustworthy && time.utc >= 0 && time.utc <= 253402300799LL;
}

// UTC for application-owned diagnostic text. Native viewers must convert the
// stored UTC to local time in their platform layer. Callers explicitly supply
// clock trust; a plausible numeric value alone never establishes it.
inline bool activity_utc(ActivityTime time, char *out, size_t capacity)
{
    if (!out || !capacity) return false;
    out[0] = 0;
    if (!activity_time_valid(time)) {
        constexpr char unknown[] = "TIME-UNKNOWN";
        if (capacity < sizeof unknown) return false;
        for (size_t i = 0; i < sizeof unknown; ++i) out[i] = unknown[i];
        return true;
    }
    if (capacity < 21) return false;
    long long days = time.utc / 86400;
    unsigned seconds = static_cast<unsigned>(time.utc % 86400);
    // Gregorian dates repeat every 400 years, including for a 1970 origin.
    unsigned year = 1970 + static_cast<unsigned>(days / 146097) * 400;
    unsigned remaining = static_cast<unsigned>(days % 146097);
    auto leap = [](unsigned y) { return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0); };
    while (remaining >= (leap(year) ? 366u : 365u)) {
        remaining -= leap(year) ? 366u : 365u;
        ++year;
    }
    const unsigned lengths[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    unsigned month = 0;
    while (remaining >= lengths[month] + (month == 1 && leap(year) ? 1u : 0u)) {
        remaining -= lengths[month] + (month == 1 && leap(year) ? 1u : 0u);
        ++month;
    }
    char text[] = "0000-00-00T00:00:00Z";
    for (int i = 3; i >= 0; --i) { text[i] = static_cast<char>('0' + year % 10); year /= 10; }
    auto pair = [&text](size_t at, unsigned value) {
        text[at] = static_cast<char>('0' + value / 10);
        text[at + 1] = static_cast<char>('0' + value % 10);
    };
    pair(5, month + 1); pair(8, remaining + 1);
    pair(11, seconds / 3600); pair(14, seconds / 60 % 60); pair(17, seconds % 60);
    for (size_t i = 0; i < sizeof text; ++i) out[i] = text[i];
    return true;
}

struct ActivityEntry {
    ActivityTime time;
    ActivitySeverity severity = ActivitySeverity::Info;
    ActivityCategory category = ActivityCategory::Lifecycle;
    char message[192] = {};
};

class ActivityLog {
public:
    static constexpr size_t capacity = 128;
    static constexpr size_t message_capacity = sizeof(ActivityEntry::message);
    size_t size() const { return count_; }
    // Borrowed until the next record/clear; callers must not retain entries
    // across event pumping. Single event-loop owner, no concurrent writers.
    const ActivityEntry *at(size_t oldest_first) const {
        return oldest_first < count_ ? &entries_[(first_ + oldest_first) % capacity] : nullptr;
    }
    void clear() {
        for (ActivityEntry &entry : entries_) entry = ActivityEntry{};
        first_ = count_ = 0;
    }
    // Never stores Status::msg/detail or arbitrary caller text. Fan-out and
    // duplicate-status suppression belong to the future event producer.
    void record(ActivityEvent event, ActivityTime time, Status result = okay()) {
        size_t slot = (first_ + count_) % capacity;
        if (count_ == capacity) first_ = (first_ + 1) % capacity;
        else ++count_;
        ActivityEntry &entry = entries_[slot];
        entry = ActivityEntry{};
        entry.time = activity_time_valid(time) ? time : ActivityTime{};
        entry.severity = (event == ActivityEvent::RefreshFailed && result.err != Err::Cancelled) ||
            event == ActivityEvent::SourceFallback || event == ActivityEvent::StaleForecast
                ? ActivitySeverity::Warning : ActivitySeverity::Info;
        entry.category = event == ActivityEvent::Startup || event == ActivityEvent::Shutdown
            ? ActivityCategory::Lifecycle : event == ActivityEvent::PreferencesChanged
                ? ActivityCategory::Preferences : ActivityCategory::Weather;
        activity_message(event, result.err, entry.message, message_capacity);
    }
private:
    ActivityEntry entries_[capacity] = {};
    size_t first_ = 0, count_ = 0;
};
static_assert(sizeof(ActivityEntry) <= 256, "activity entry exceeds its budget");
static_assert(sizeof(ActivityLog) <= 32768, "activity history exceeds 32 KiB");

} // namespace wx
