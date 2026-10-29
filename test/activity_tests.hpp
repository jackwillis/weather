// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
#include "app/activity.hpp"
#include <climits>
#include <initializer_list>

static void test_activity()
{
    ActivityLog log;
    CHECK(log.size() == 0 && log.at(0) == nullptr);
    CHECK(sizeof(ActivityEntry) <= 256 && sizeof(ActivityLog) <= 32768);
    for (size_t i = 0; i < ActivityLog::capacity * 3 + 17; ++i)
        log.record(ActivityEvent::RefreshStarted, {static_cast<long long>(i), true});
    CHECK(log.size() == ActivityLog::capacity);
    for (size_t i = 0; i < log.size(); ++i) {
        const ActivityEntry *entry = log.at(i);
        CHECK(entry && entry->time.utc == static_cast<long long>(ActivityLog::capacity * 2 + 17 + i));
        CHECK(entry && entry->severity == ActivitySeverity::Info);
        CHECK(entry && std::strcmp(entry->message, "Refreshing weather") == 0);
    }
    CHECK(log.at(log.size()) == nullptr && log.at(static_cast<size_t>(-1)) == nullptr);
    log.clear();
    CHECK(log.size() == 0 && log.at(0) == nullptr);
    log.record(ActivityEvent::PreferencesChanged, {}, okay());
    CHECK(log.at(0)->category == ActivityCategory::Preferences);
    log.clear();
    const char *private_text = "seed=PRIVATE-SEED; coordinates=44.5,-89.5; HTTP body=PRIVATE-BODY";
    log.record(ActivityEvent::RefreshFailed, {1735689600LL, true}, make(Err::Dns, private_text, 123456789));
    CHECK(std::strcmp(log.at(0)->message, "Weather update failed: name lookup failed") == 0);
    CHECK(std::strstr(log.at(0)->message, "PRIVATE") == nullptr);
    CHECK(std::strstr(log.at(0)->message, "123456789") == nullptr);
    CHECK(log.at(0)->severity == ActivitySeverity::Warning);
    const ActivityEvent events[] = {ActivityEvent::Startup, ActivityEvent::ForecastRestored,
        ActivityEvent::RefreshStarted, ActivityEvent::RefreshSucceeded, ActivityEvent::RefreshFailed,
        ActivityEvent::SourceFallback, ActivityEvent::StaleForecast, ActivityEvent::PreferencesChanged,
        ActivityEvent::Shutdown, static_cast<ActivityEvent>(999)};
    for (ActivityEvent event : events) {
        log.record(event, {}, make(Err::Tls, private_text));
        const ActivityEntry *entry = log.at(log.size() - 1);
        CHECK(entry && entry->message[0] && std::strlen(entry->message) < ActivityLog::message_capacity);
        CHECK(entry && !std::strchr(entry->message, '\n') && !std::strstr(entry->message, "PRIVATE"));
        CHECK(entry && !entry->time.trustworthy);
    }
    for (int error = static_cast<int>(Err::Ok); error <= static_cast<int>(Err::Cancelled); ++error) {
        log.record(ActivityEvent::RefreshFailed, {}, make(static_cast<Err>(error), private_text));
        CHECK(std::strstr(log.at(log.size() - 1)->message, "PRIVATE") == nullptr);
    }
    log.record(ActivityEvent::RefreshFailed, {}, make(Err::Cancelled, private_text));
    CHECK(log.at(log.size() - 1)->severity == ActivitySeverity::Info);
    char short_message[8] = {'X','X','X','X','X','X','X','!'};
    CHECK(!activity_message(ActivityEvent::RefreshStarted, Err::Ok, short_message, 7));
    CHECK(std::strcmp(short_message, "Refres") == 0 && short_message[7] == '!');
    CHECK(!activity_message(ActivityEvent::Startup, Err::Ok, nullptr, 0));
    char single[2] = {'X','!'};
    CHECK(!activity_message(ActivityEvent::Startup, Err::Ok, single, 1));
    CHECK(single[0] == 0 && single[1] == '!');
    struct TimeCase { long long utc; const char *text; };
    const TimeCase times[] = {{0,"1970-01-01T00:00:00Z"}, {1709164800LL,"2024-02-29T00:00:00Z"},
        {1735689600LL,"2025-01-01T00:00:00Z"}, {2147483648LL,"2038-01-19T03:14:08Z"},
        {4107542400LL,"2100-03-01T00:00:00Z"}, {253402300799LL,"9999-12-31T23:59:59Z"}};
    for (const TimeCase &test : times) {
        char timestamp[21];
        CHECK(activity_utc({test.utc, true}, timestamp, sizeof timestamp));
        CHECK(std::strcmp(timestamp, test.text) == 0);
    }
    for (ActivityTime invalid : {ActivityTime{}, ActivityTime{-1,true},
            ActivityTime{LLONG_MIN,true}, ActivityTime{LLONG_MAX,true}, ActivityTime{1735689600LL,false}}) {
        char timestamp[21];
        CHECK(activity_utc(invalid, timestamp, sizeof timestamp));
        CHECK(std::strcmp(timestamp, "TIME-UNKNOWN") == 0);
    }
    char tiny[3] = {'X','X','!'};
    CHECK(!activity_utc({0,true}, tiny, 2) && tiny[0] == 0 && tiny[2] == '!');
    CHECK(!activity_utc({0,true}, nullptr, 21));
}
