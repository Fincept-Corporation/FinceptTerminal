#pragma once
// MarketSessions — exchange venues, trading sessions and holiday calendars.
//
// A static table of the world's main venues (MIC, time zone, trading currency,
// regular / pre / post session in the exchange's OWN local time, lunch breaks,
// weekend days, Yahoo ticker suffixes) plus a holiday calendar per venue, and
// the session arithmetic built on it: open / closed state, the exchange's
// trading date, trading days and bars per year, and session-aligned intraday
// buckets.
//
// Qt Core only (no logging, no app services), so it is unit-tested on its own
// (tests/tst_market_sessions.cpp) and safe to call from any thread.
//
// Times
//   Every schedule value is minutes after local midnight of the TRADING DATE in
//   the venue's IANA time zone, so DST and half-hour offsets come out right.
//   A negative open means the session starts the previous calendar evening:
//   CME Globex (-7h = 17:00 CT the day before) and 24x5 FX (17:00 New York).
//
// Holidays
//   Built in (see holiday_coverage()):
//     rule-based  US (NYSE/Nasdaq), UK (LSE), DE (Xetra + German regional
//                 floors), EURONEXT (Paris/Amsterdam/Brussels/Lisbon), JP (TSE)
//     tables      IN (NSE/BSE) 2026; HK (HKEX) 2026-2027; CN (SSE/SZSE) 2026-2027
//   Every other venue has none: a weekday holiday reads as a trading day there.
//   load_holiday_overrides() merges a JSON file on top (format documented there).

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QTimeZone>

#include <vector>

namespace fincept::core::market {

enum class SessionState : quint8 {
    Closed,  ///< outside every session (night, weekend)
    Pre,     ///< pre-market / pre-open auction
    Open,    ///< regular session
    Lunch,   ///< midday break between the morning and afternoon sessions
    Post,    ///< post-market / closing auction / after-hours
    Holiday, ///< a weekday on which the venue is closed for a holiday
};

enum class VenueKind : quint8 { Equity, Futures, Fx, Crypto };

/// How complete a venue's holiday data is for one year.
enum class HolidayCoverage : quint8 {
    None,    ///< no holiday data — weekday holidays read as trading days
    Partial, ///< some holidays known (JSON override declared it partial)
    Full,    ///< every full-day closure the exchange announced / the rules give
};

/// One trading venue. Instances live in a static table — compare by pointer or id.
struct Venue {
    QString id;       ///< stable key: "NYSE", "NASDAQ", "NSE", "LSE", "TSE", "FX", "CRYPTO", ...
    QString mic;      ///< ISO 10383 MIC ("XNYS"); empty for the FX / crypto pseudo-venues
    QString name;     ///< English display name; use venue_display_name() in UI
    QString country;  ///< ISO 3166-1 alpha-2 ("US"); empty for global pseudo-venues
    QString tz_id;    ///< IANA time zone ("America/New_York")
    QString currency; ///< trading currency as quoted, minor units kept (GBp, ZAc, ILA); empty for FX/crypto
    QString calendar; ///< holiday calendar key ("US" for NYSE + Nasdaq, else usually the venue id)
    QStringList suffixes; ///< Yahoo ticker suffixes, upper case, no dot ("L", "IL")
    QStringList aliases;  ///< exchange names / codes accepted as an exchange hint (upper case)
    VenueKind kind = VenueKind::Equity;

    // Regular weekday schedule (minutes after local midnight of the trading date).
    // pre_open_min == open_min when there is no pre-market; post_close_min ==
    // close_min when there is no post-market; lunch_*_min are -1 without a break.
    int pre_open_min = 0;
    int open_min = 0;
    int lunch_start_min = -1;
    int lunch_end_min = -1;
    int close_min = 0;
    int post_close_min = 0;

    quint8 weekend_mask = 0x60; ///< bit (QDate::dayOfWeek() - 1) set = no regular session (0x60 = Sat+Sun)
    int trading_days_per_year = 252;

    // One weekday may run a different schedule (TASE Friday, IDX Friday lunch).
    int alt_weekday = 0;          ///< 1..7, 0 = none
    int alt_close_min = -1;       ///< -1 = same as close_min
    int alt_lunch_start_min = -1; ///< -1 = same as lunch_start_min
    int alt_lunch_end_min = -1;   ///< -1 = same as lunch_end_min

    // Venues that move their hours with US daylight saving (B3, BMV) shift the
    // open / close by these minutes while New York observes DST.
    int us_dst_open_shift_min = 0;
    int us_dst_close_shift_min = 0;

    QTimeZone tz; ///< resolved tz_id

    bool has_lunch() const { return lunch_start_min >= 0 && lunch_end_min > lunch_start_min; }
    bool is_24x7() const { return kind == VenueKind::Crypto; }
};

/// The resolved schedule of one trading date, in UTC. Times are filled from the
/// venue's weekday schedule even when `trading` is false (weekend / holiday), so
/// they can still serve as a reference grid.
struct DaySchedule {
    QDate date;                ///< the trading date (exchange calendar)
    bool trading = false;      ///< a session runs on this date
    bool holiday = false;      ///< closed (or special) because of a calendar entry, not the weekend
    bool early_close = false;  ///< closes before the regular close (half day)
    bool special = false;      ///< a special session on an otherwise non-trading day (e.g. Muhurat)
    QString holiday_name;      ///< calendar entry name (translated), empty if none
    QDateTime pre_open_utc;    ///< == open_utc without a pre-market
    QDateTime open_utc;
    QDateTime lunch_start_utc; ///< invalid without a midday break
    QDateTime lunch_end_utc;   ///< invalid without a midday break
    QDateTime close_utc;
    QDateTime post_close_utc;  ///< == close_utc without a post-market
};

struct SessionStatus {
    SessionState state = SessionState::Closed;
    QDateTime next_change_utc;                 ///< when `state` ends; invalid when it does not change within ~2 weeks (crypto)
    SessionState next_state = SessionState::Closed;
    QDateTime venue_local_now;                 ///< `now` in the venue's time zone
    QDate trading_date;                        ///< session date `now` belongs to (trading_date() when closed)
};

// ── Venue lookup ─────────────────────────────────────────────────────────────

/// Every venue in the built-in table.
const std::vector<Venue>& all_venues();

/// Venue by id ("NSE"), MIC ("XNSE") or alias ("NSI", "NASDAQ", "XETRA", "HKG"), case-insensitive.
/// ISO country codes resolve to the country's main venue ("US" NYSE, "UK"/"GB" LSE,
/// "JP" TSE, "CN" SSE, "IN" NSE, "HK" HKEX, "DE" XETRA, ...).
const Venue* venue_by_code(const QString& code);

/// Venue for a Yahoo suffix ("L", ".L", "NS"), case-insensitive. nullptr if unknown.
const Venue* venue_for_suffix(const QString& suffix);

/// Venue a (Yahoo-style) symbol trades on.
///   "EURUSD=X" -> FX, "BTC-USD" -> CRYPTO, "ES=F" -> CME, "^N225" -> TSE,
///   "VOD.L" -> LSE, "NSE:RELIANCE-EQ" -> NSE (prefix acts as the hint).
/// A suffix wins over `exchange_hint`; the hint decides for bare symbols and
/// unknown suffixes. A bare symbol without a hint is a US listing (NYSE); with
/// a hint naming an exchange not in the table (MCX) it is nullptr, as are
/// unknown `^` indices and unknown suffixes without a hint.
const Venue* venue_for_symbol(const QString& symbol, const QString& exchange_hint = {});

/// Translated display name of a venue.
QString venue_display_name(const Venue& venue);

/// Stable English key of a state ("OPEN", "PRE", "LUNCH", "POST", "CLOSED", "HOLIDAY").
QString session_state_key(SessionState state);
/// Translated, user-visible label of a state.
QString session_state_label(SessionState state);

// ── Clock & calendar ────────────────────────────────────────────────────────

QTimeZone venue_timezone(const Venue& venue);

/// The exchange's calendar date for `utc`. For venues whose session starts the
/// previous evening (CME, FX) the date rolls at that evening open (17:00
/// local), so Sunday 18:00 New York is Monday for FX. Equity venues: local date.
QDate trading_date(const Venue& venue, const QDateTime& utc);

/// True if a session runs on `date` (weekday, not a full-day holiday, or a special session).
bool is_trading_day(const Venue& venue, QDate date);

/// Resolved schedule of `date` (holidays, early closes, weekday variants, US-DST shifts applied).
DaySchedule day_schedule(const Venue& venue, QDate date);

/// Open / closed state of `venue` at `now_utc`.
SessionStatus session_status(const Venue& venue, const QDateTime& now_utc);

/// Regular-session open / close of `date` in UTC; invalid if `date` is not a trading day.
QDateTime session_open_utc(const Venue& venue, QDate date);
QDateTime session_close_utc(const Venue& venue, QDate date);

/// Name of the holiday / special day on `date` (translated), empty if none.
QString holiday_name(const Venue& venue, QDate date);

// ── Annualisation & bar alignment ───────────────────────────────────────────

/// Regular-session length in minutes, lunch excluded. Without a date: the
/// venue's standard weekday schedule (US standard time for B3/BMV). With a
/// date: that date's actual schedule (0 when not a trading day).
double session_minutes(const Venue& venue, QDate date = {});

/// Typical trading days per year: ~252 US equities, 242-255 elsewhere, 260 FX, 365 crypto.
double trading_days_per_year(const Venue& venue);

/// Bars per year for annualising (Sharpe, volatility). Intraday bars count the
/// bars a session-aligned feed prints per day (each continuous segment rounded
/// up: NYSE 1h = 7, HKEX 1h = 3 + 3) times trading_days_per_year(); daily-to-
/// weekly bars scale trading days; weekly and longer bars use calendar time.
double bars_per_year(const Venue& venue, int bar_seconds);

/// Start of the bar containing `ts_utc` when bars are aligned to the session
/// open (NSE 1h: 09:15, 10:15, ...) instead of the UTC epoch. After a midday
/// break the grid restarts at the afternoon open (HKEX 13:00). Pre/post-market
/// stamps extend the same grid. bar_seconds >= 86400 returns the open of the
/// trading date. bar_seconds <= 0 returns ts_utc unchanged.
QDateTime session_bucket_start(const Venue& venue, const QDateTime& ts_utc, int bar_seconds);

// ── Holiday data ────────────────────────────────────────────────────────────

HolidayCoverage holiday_coverage(const Venue& venue, int year);

/// Merge holiday data from JSON on top of the built-in table (later loads win).
/// Format:
///   {
///     "calendars": {
///       "IN": {                                   // calendar key (Venue::calendar)
///         "years": [2027],                        // years now fully covered
///         "partial_years": [2028],                // years with some data
///         "replace_years": true,                  // drop built-in entries of "years" first
///         "days": [
///           {"date": "2027-01-26", "name": "Republic Day"},                    // closed all day
///           {"date": "2027-12-24", "close": "13:00"},                           // early close
///           {"date": "2027-11-07", "open": "18:00", "close": "19:00",
///            "name": "Muhurat Trading"},                                        // special session
///           {"date": "2027-03-03", "remove": true}                              // drop an entry
///         ]
///       }
///     }
///   }
/// Times are the venue's local wall-clock time. Returns false (and leaves the
/// data unchanged) on a parse error; `error` gets a short reason.
bool load_holiday_overrides_json(const QByteArray& json, QString* error = nullptr);

/// load_holiday_overrides_json() from a file. A missing file is not an error
/// (returns true). The app calls this at startup with
/// AppPaths::data() + "/market_holidays.json".
bool load_holiday_overrides(const QString& path, QString* error = nullptr);

/// Drop every override loaded so far (back to the built-in table). For tests.
void reset_holiday_overrides();

} // namespace fincept::core::market
