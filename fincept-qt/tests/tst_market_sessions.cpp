// Unit tests for src/core/market/MarketSessions.{h,cpp} and
// src/core/market/InstrumentCurrency.{h,cpp} (Qt Core only): venue lookup by
// Yahoo symbol, session state across DST changes, lunch breaks, non-Sat/Sun
// weekends, holiday calendars and JSON overrides, bars per year, and
// session-aligned bar buckets.
//
// Every instant is an explicit UTC time, so the suite does not depend on the
// machine's clock or time zone.

#include "core/market/InstrumentCurrency.h"
#include "core/market/MarketSessions.h"
#include "ui/formatting/CurrencyTable.h"

#include <QDateTime>
#include <QSet>
#include <QString>
#include <QTest>
#include <QTimeZone>

using namespace fincept::core::market;

namespace {

QDateTime tms_utc(int y, int mo, int d, int h, int mi) {
    return QDateTime(QDate(y, mo, d), QTime(h, mi), QTimeZone::utc());
}

const Venue& tms_venue(const char* id) {
    const Venue* v = venue_by_code(QString::fromLatin1(id));
    if (!v)
        qFatal("venue %s missing from the table", id);
    return *v;
}

SessionState tms_state(const char* id, const QDateTime& utc) {
    return session_status(tms_venue(id), utc).state;
}

} // namespace

Q_DECLARE_METATYPE(fincept::core::market::SessionState)

class TstMarketSessions : public QObject {
    Q_OBJECT

  private slots:
    void cleanup();

    void venue_table_is_consistent();
    void symbol_to_venue_data();
    void symbol_to_venue();

    void state_across_dst_data();
    void state_across_dst();
    void next_change_crosses_weekend_and_dst();
    void lunch_breaks_data();
    void lunch_breaks();
    void lunch_break_next_change();
    void non_saturday_sunday_weekends_data();
    void non_saturday_sunday_weekends();
    void us_dst_linked_venues();
    void crypto_never_closes();

    void holiday_calendars_data();
    void holiday_calendars();
    void holiday_state_and_early_close();
    void holiday_coverage_report();
    void json_overrides();
    void json_override_rejects_bad_input();

    void trading_date_rolls();
    void session_minutes_and_bars_per_year();
    void bucket_nse_0915();
    void bucket_across_utc_midnight();
    void bucket_after_lunch_and_fx();

    void currency_for_symbol_cases_data();
    void currency_for_symbol_cases();
    void minor_units();
    void minor_units_agree_with_currency_table();
};

void TstMarketSessions::cleanup() {
    reset_holiday_overrides();
}

// ── Table ───────────────────────────────────────────────────────────────────

void TstMarketSessions::venue_table_is_consistent() {
    QSet<QString> ids;
    QSet<QString> mics;
    QSet<QString> suffixes;
    for (const Venue& v : all_venues()) {
        QVERIFY2(v.tz.isValid(), qPrintable(v.id + QLatin1String(" has an invalid time zone ") + v.tz_id));
        QVERIFY2(!ids.contains(v.id), qPrintable(v.id));
        ids.insert(v.id);
        if (!v.mic.isEmpty()) {
            QVERIFY2(!mics.contains(v.mic), qPrintable(v.mic));
            mics.insert(v.mic);
        }
        for (const QString& s : v.suffixes) {
            QVERIFY2(!suffixes.contains(s), qPrintable(QLatin1String("duplicate suffix ") + s));
            suffixes.insert(s);
        }
        QVERIFY2(v.pre_open_min <= v.open_min, qPrintable(v.id));
        QVERIFY2(v.open_min < v.close_min, qPrintable(v.id));
        QVERIFY2(v.close_min <= v.post_close_min, qPrintable(v.id));
        QVERIFY2(v.close_min - v.open_min <= 1440, qPrintable(v.id));
        if (v.has_lunch())
            QVERIFY2(v.open_min < v.lunch_start_min && v.lunch_end_min < v.close_min, qPrintable(v.id));
        QVERIFY2(venue_by_code(v.id) == &v, qPrintable(v.id));
        if (!v.mic.isEmpty())
            QVERIFY2(venue_by_code(v.mic) == &v, qPrintable(v.mic));
        QVERIFY(!venue_display_name(v).isEmpty());
    }
    // The venues the terminal is expected to know.
    for (const char* id : {"NYSE", "NASDAQ", "CME", "TSX", "B3", "BMV", "LSE", "XETRA", "FRA", "PARIS", "AMSTERDAM",
                           "SIX", "MILAN", "BME", "STOCKHOLM", "JSE", "TADAWUL", "DFM", "ADX", "TASE", "NSE", "BSE",
                           "SGX", "HKEX", "SSE", "SZSE", "TSE", "KRX", "TWSE", "ASX", "NZX", "IDX", "SET", "MYX",
                           "FX", "CRYPTO"})
        QVERIFY2(venue_by_code(QString::fromLatin1(id)), id);
    // Country codes name the main venue (the dashboard's region keys).
    QCOMPARE(venue_by_code(QStringLiteral("us"))->id, QStringLiteral("NYSE"));
    QCOMPARE(venue_by_code(QStringLiteral("UK"))->id, QStringLiteral("LSE"));
    QCOMPARE(venue_by_code(QStringLiteral("JP"))->id, QStringLiteral("TSE"));
    QCOMPARE(venue_by_code(QStringLiteral("CN"))->id, QStringLiteral("SSE"));
    QCOMPARE(venue_by_code(QStringLiteral("IN"))->id, QStringLiteral("NSE"));
    QCOMPARE(venue_by_code(QStringLiteral("XETR"))->id, QStringLiteral("XETRA"));
    QVERIFY(!venue_by_code(QStringLiteral("MCX")));
    QVERIFY(!venue_by_code(QString()));
}

// ── Symbol -> venue ─────────────────────────────────────────────────────────

void TstMarketSessions::symbol_to_venue_data() {
    QTest::addColumn<QString>("symbol");
    QTest::addColumn<QString>("hint");
    QTest::addColumn<QString>("venue"); // empty = nullptr

    const struct {
        const char* sym;
        const char* hint;
        const char* venue;
    } rows[] = {
        {"AAPL", "", "NYSE"},          {"AAPL", "NASDAQ", "NASDAQ"},   {"AAPL", "nms", "NASDAQ"},
        {"BRK-B", "", "NYSE"},         {"BRK.B", "", ""},             {"BRK.B", "NYSE", "NYSE"},
        {"RELIANCE", "NSE", "NSE"},    {"NSE:RELIANCE-EQ", "", "NSE"}, {"VOD.L", "", "LSE"},
        {"vod.l", "", "LSE"},          {"RELIANCE.NS", "", "NSE"},     {"500325.BO", "", "BSE"},
        {"7203.T", "", "TSE"},         {"0700.HK", "", "HKEX"},        {"600519.SS", "", "SSE"},
        {"000001.SZ", "", "SZSE"},     {"SAP.DE", "", "XETRA"},        {"SAP.F", "", "FRA"},
        {"AIR.PA", "", "PARIS"},       {"ASML.AS", "", "AMSTERDAM"},   {"SHOP.TO", "", "TSX"},
        {"BHP.AX", "", "ASX"},         {"PETR4.SA", "", "B3"},         {"005930.KS", "", "KRX"},
        {"2330.TW", "", "TWSE"},       {"D05.SI", "", "SGX"},          {"NPN.JO", "", "JSE"},
        {"NESN.SW", "", "SIX"},        {"ENI.MI", "", "MILAN"},        {"SAN.MC", "", "BME"},
        {"VOLV-B.ST", "", "STOCKHOLM"}, {"2222.SR", "", "TADAWUL"},    {"TEVA.TA", "", "TASE"},
        {"BBCA.JK", "", "IDX"},        {"PTT.BK", "", "SET"},          {"1155.KL", "", "MYX"},
        {"FPH.NZ", "", "NZX"},         {"WALMEX.MX", "", "BMV"},       {"EURUSD=X", "", "FX"},
        {"JPY=X", "", "FX"},           {"BTC-USD", "", "CRYPTO"},      {"ETH-USDT", "", "CRYPTO"},
        {"ES=F", "", "CME"},           {"GC=F", "", "CME"},            {"^GSPC", "", "NYSE"},
        {"^IXIC", "", "NASDAQ"},       {"^N225", "", "TSE"},           {"^NSEI", "", "NSE"},
        {"^BSESN", "", "BSE"},         {"^FTSE", "", "LSE"},           {"^HSI", "", "HKEX"},
        {"^GDAXI", "", "XETRA"},       {"^SET.BK", "", "SET"},         {"^NOSUCHINDEX", "", ""},
        {"EURUSD", "FOREX", "FX"},     {"", "", ""},                   {"GOLD", "MCX", ""},
    };
    for (const auto& r : rows) {
        QTest::newRow(qPrintable(QStringLiteral("%1|%2").arg(QString::fromLatin1(r.sym), QString::fromLatin1(r.hint))))
            << QString::fromLatin1(r.sym) << QString::fromLatin1(r.hint) << QString::fromLatin1(r.venue);
    }
}

void TstMarketSessions::symbol_to_venue() {
    QFETCH(QString, symbol);
    QFETCH(QString, hint);
    QFETCH(QString, venue);
    const Venue* v = venue_for_symbol(symbol, hint);
    QCOMPARE(v ? v->id : QString(), venue);
}

// ── Session state ───────────────────────────────────────────────────────────

void TstMarketSessions::state_across_dst_data() {
    QTest::addColumn<QString>("venue");
    QTest::addColumn<QDateTime>("utc");
    QTest::addColumn<SessionState>("expected");

    // US clocks go forward on 2026-03-08, UK on 2026-03-29.
    QTest::newRow("NYSE Fri 09:35 EST") << "NYSE" << tms_utc(2026, 3, 6, 14, 35) << SessionState::Open;
    QTest::newRow("NYSE Fri 08:35 EST pre") << "NYSE" << tms_utc(2026, 3, 6, 13, 35) << SessionState::Pre;
    QTest::newRow("NYSE Fri 09:25 EST pre") << "NYSE" << tms_utc(2026, 3, 6, 14, 25) << SessionState::Pre;
    QTest::newRow("NYSE Mon 09:35 EDT") << "NYSE" << tms_utc(2026, 3, 9, 13, 35) << SessionState::Open;
    QTest::newRow("NYSE Mon 15:59 EDT") << "NYSE" << tms_utc(2026, 3, 9, 19, 59) << SessionState::Open;
    QTest::newRow("NYSE Mon 16:05 EDT post") << "NYSE" << tms_utc(2026, 3, 9, 20, 5) << SessionState::Post;
    QTest::newRow("NYSE Mon 20:30 EDT closed") << "NYSE" << tms_utc(2026, 3, 10, 0, 30) << SessionState::Closed;
    QTest::newRow("NYSE Sat") << "NYSE" << tms_utc(2026, 3, 7, 15, 0) << SessionState::Closed;
    QTest::newRow("NYSE Nov after fall-back 09:35 EST") << "NYSE" << tms_utc(2026, 11, 2, 14, 35) << SessionState::Open;
    QTest::newRow("NYSE Nov after fall-back 09:00 EST") << "NYSE" << tms_utc(2026, 11, 2, 14, 0) << SessionState::Pre;
    QTest::newRow("LSE 08:05 GMT") << "LSE" << tms_utc(2026, 3, 16, 8, 5) << SessionState::Open;
    QTest::newRow("LSE 07:05 GMT closed") << "LSE" << tms_utc(2026, 3, 16, 7, 5) << SessionState::Closed;
    QTest::newRow("LSE 08:05 BST") << "LSE" << tms_utc(2026, 3, 30, 7, 5) << SessionState::Open;
    QTest::newRow("LSE 16:35 BST post") << "LSE" << tms_utc(2026, 3, 30, 15, 35) << SessionState::Post;
    QTest::newRow("LSE 16:35 GMT post") << "LSE" << tms_utc(2026, 3, 16, 16, 35) << SessionState::Post;
    QTest::newRow("XETRA 09:05 CEST") << "XETRA" << tms_utc(2026, 6, 15, 7, 5) << SessionState::Open;
    QTest::newRow("XETRA 09:05 CET") << "XETRA" << tms_utc(2026, 1, 15, 8, 5) << SessionState::Open;
    QTest::newRow("XETRA 08:05 CET closed") << "XETRA" << tms_utc(2026, 1, 15, 7, 5) << SessionState::Closed;
    QTest::newRow("NSE 09:20 IST") << "NSE" << tms_utc(2026, 3, 9, 3, 50) << SessionState::Open;
    QTest::newRow("NSE 09:10 IST pre") << "NSE" << tms_utc(2026, 3, 9, 3, 40) << SessionState::Pre;
    QTest::newRow("NSE 15:45 IST post") << "NSE" << tms_utc(2026, 3, 9, 10, 15) << SessionState::Post;
    // ASX opens 10:00 AEDT = 23:00 UTC the previous day; AEST (winter) = 00:00 UTC.
    QTest::newRow("ASX 10:30 AEDT") << "ASX" << tms_utc(2026, 1, 14, 23, 30) << SessionState::Open;
    QTest::newRow("ASX 09:30 AEST pre") << "ASX" << tms_utc(2026, 7, 14, 23, 30) << SessionState::Pre;
    QTest::newRow("ASX 10:30 AEST") << "ASX" << tms_utc(2026, 7, 15, 0, 30) << SessionState::Open;
}

void TstMarketSessions::state_across_dst() {
    QFETCH(QString, venue);
    QFETCH(QDateTime, utc);
    QFETCH(SessionState, expected);
    QCOMPARE(tms_state(qPrintable(venue), utc), expected);
}

void TstMarketSessions::next_change_crosses_weekend_and_dst() {
    const Venue& nyse = tms_venue("NYSE");
    // Friday 09:35 EST: open until 16:00 EST = 21:00 UTC, then post-market.
    SessionStatus st = session_status(nyse, tms_utc(2026, 3, 6, 14, 35));
    QCOMPARE(st.state, SessionState::Open);
    QCOMPARE(st.next_change_utc, tms_utc(2026, 3, 6, 21, 0));
    QCOMPARE(st.next_state, SessionState::Post);
    QCOMPARE(st.trading_date, QDate(2026, 3, 6));
    QCOMPARE(st.venue_local_now.time(), QTime(9, 35));

    // Saturday: next change is Monday's 04:00 pre-market, now on EDT (08:00 UTC).
    st = session_status(nyse, tms_utc(2026, 3, 7, 15, 0));
    QCOMPARE(st.state, SessionState::Closed);
    QCOMPARE(st.next_change_utc, tms_utc(2026, 3, 9, 8, 0));
    QCOMPARE(st.next_state, SessionState::Pre);

    // FX: Friday 16:30 EST still open, closes 17:00 EST for the weekend; the
    // Sunday 17:00 EST reopen is the next change after that.
    const Venue& fx = tms_venue("FX");
    st = session_status(fx, tms_utc(2026, 1, 16, 21, 30));
    QCOMPARE(st.state, SessionState::Open);
    QCOMPARE(st.next_change_utc, tms_utc(2026, 1, 16, 22, 0));
    QCOMPARE(st.next_state, SessionState::Closed);
    st = session_status(fx, tms_utc(2026, 1, 17, 12, 0));
    QCOMPARE(st.state, SessionState::Closed);
    QCOMPARE(st.next_change_utc, tms_utc(2026, 1, 18, 22, 0));
    // Tuesday: the 17:00 roll is not a close — next change is Friday's close.
    st = session_status(fx, tms_utc(2026, 1, 13, 12, 0));
    QCOMPARE(st.state, SessionState::Open);
    QCOMPARE(st.next_change_utc, tms_utc(2026, 1, 16, 22, 0));

    // CME Globex: daily maintenance break 16:00-17:00 CT.
    const Venue& cme = tms_venue("CME");
    QCOMPARE(tms_state("CME", tms_utc(2026, 1, 14, 22, 30)), SessionState::Closed); // 16:30 CST
    QCOMPARE(tms_state("CME", tms_utc(2026, 1, 14, 23, 30)), SessionState::Open);   // 17:30 CST
    st = session_status(cme, tms_utc(2026, 1, 18, 23, 30)); // Sunday 17:30 CST
    QCOMPARE(st.state, SessionState::Open);
    QCOMPARE(st.trading_date, QDate(2026, 1, 19));
    QCOMPARE(st.next_change_utc, tms_utc(2026, 1, 19, 22, 0));
}

void TstMarketSessions::lunch_breaks_data() {
    QTest::addColumn<QString>("venue");
    QTest::addColumn<QDateTime>("utc");
    QTest::addColumn<SessionState>("expected");

    QTest::newRow("TSE 11:00 JST") << "TSE" << tms_utc(2026, 1, 15, 2, 0) << SessionState::Open;
    QTest::newRow("TSE 12:00 JST lunch") << "TSE" << tms_utc(2026, 1, 15, 3, 0) << SessionState::Lunch;
    QTest::newRow("TSE 15:20 JST") << "TSE" << tms_utc(2026, 1, 15, 6, 20) << SessionState::Open;
    QTest::newRow("TSE 15:31 JST") << "TSE" << tms_utc(2026, 1, 15, 6, 31) << SessionState::Closed;
    QTest::newRow("HKEX 09:15 HKT pre") << "HKEX" << tms_utc(2026, 1, 15, 1, 15) << SessionState::Pre;
    QTest::newRow("HKEX 12:30 HKT lunch") << "HKEX" << tms_utc(2026, 1, 15, 4, 30) << SessionState::Lunch;
    QTest::newRow("HKEX 13:30 HKT") << "HKEX" << tms_utc(2026, 1, 15, 5, 30) << SessionState::Open;
    QTest::newRow("HKEX 16:05 HKT post") << "HKEX" << tms_utc(2026, 1, 15, 8, 5) << SessionState::Post;
    QTest::newRow("SSE 12:00 CST lunch") << "SSE" << tms_utc(2026, 1, 15, 4, 0) << SessionState::Lunch;
    QTest::newRow("SZSE 13:30 CST") << "SZSE" << tms_utc(2026, 1, 15, 5, 30) << SessionState::Open;
    QTest::newRow("SGX 12:30 SGT lunch") << "SGX" << tms_utc(2026, 1, 15, 4, 30) << SessionState::Lunch;
    // IDX: Mon-Thu break 12:00-13:30, Friday 11:30-14:00.
    QTest::newRow("IDX Thu 12:00 lunch") << "IDX" << tms_utc(2026, 1, 15, 5, 0) << SessionState::Lunch;
    QTest::newRow("IDX Thu 13:45") << "IDX" << tms_utc(2026, 1, 15, 6, 45) << SessionState::Open;
    QTest::newRow("IDX Fri 11:45 lunch") << "IDX" << tms_utc(2026, 1, 16, 4, 45) << SessionState::Lunch;
    QTest::newRow("IDX Fri 13:45 lunch") << "IDX" << tms_utc(2026, 1, 16, 6, 45) << SessionState::Lunch;
}

void TstMarketSessions::lunch_breaks() {
    QFETCH(QString, venue);
    QFETCH(QDateTime, utc);
    QFETCH(SessionState, expected);
    QCOMPARE(tms_state(qPrintable(venue), utc), expected);
}

void TstMarketSessions::lunch_break_next_change() {
    const Venue& tse = tms_venue("TSE");
    SessionStatus st = session_status(tse, tms_utc(2026, 1, 15, 2, 0)); // 11:00 JST
    QCOMPARE(st.next_change_utc, tms_utc(2026, 1, 15, 2, 30));          // 11:30 JST
    QCOMPARE(st.next_state, SessionState::Lunch);
    st = session_status(tse, tms_utc(2026, 1, 15, 3, 0)); // 12:00 JST
    QCOMPARE(st.next_change_utc, tms_utc(2026, 1, 15, 3, 30));
    QCOMPARE(st.next_state, SessionState::Open);
    QCOMPARE(day_schedule(tse, QDate(2026, 1, 15)).lunch_start_utc, tms_utc(2026, 1, 15, 2, 30));
}

void TstMarketSessions::non_saturday_sunday_weekends_data() {
    QTest::addColumn<QString>("venue");
    QTest::addColumn<QDateTime>("utc");
    QTest::addColumn<SessionState>("expected");

    // Tadawul trades Sunday-Thursday, 10:00-15:00 AST (UTC+3).
    QTest::newRow("Tadawul Sun 11:00") << "TADAWUL" << tms_utc(2026, 1, 18, 8, 0) << SessionState::Open;
    QTest::newRow("Tadawul Thu 11:00") << "TADAWUL" << tms_utc(2026, 1, 22, 8, 0) << SessionState::Open;
    QTest::newRow("Tadawul Fri 11:00") << "TADAWUL" << tms_utc(2026, 1, 16, 8, 0) << SessionState::Closed;
    QTest::newRow("Tadawul Sat 11:00") << "TADAWUL" << tms_utc(2026, 1, 17, 8, 0) << SessionState::Closed;
    QTest::newRow("Tadawul Sun 15:10 post") << "TADAWUL" << tms_utc(2026, 1, 18, 12, 10) << SessionState::Post;
    // TASE moved to Monday-Friday in 2026; Friday closes at 13:50 IST (UTC+2).
    QTest::newRow("TASE Sun") << "TASE" << tms_utc(2026, 1, 18, 9, 0) << SessionState::Closed;
    QTest::newRow("TASE Thu 16:00") << "TASE" << tms_utc(2026, 1, 15, 14, 0) << SessionState::Open;
    QTest::newRow("TASE Fri 11:00") << "TASE" << tms_utc(2026, 1, 16, 9, 0) << SessionState::Open;
    QTest::newRow("TASE Fri 14:00") << "TASE" << tms_utc(2026, 1, 16, 12, 0) << SessionState::Closed;
    // DFM: Monday-Friday since 2022.
    QTest::newRow("DFM Fri 11:00") << "DFM" << tms_utc(2026, 1, 16, 7, 0) << SessionState::Open;
    QTest::newRow("DFM Sun 11:00") << "DFM" << tms_utc(2026, 1, 18, 7, 0) << SessionState::Closed;
}

void TstMarketSessions::non_saturday_sunday_weekends() {
    QFETCH(QString, venue);
    QFETCH(QDateTime, utc);
    QFETCH(SessionState, expected);
    QCOMPARE(tms_state(qPrintable(venue), utc), expected);
}

void TstMarketSessions::us_dst_linked_venues() {
    // B3 closes 18:00 BRT in US winter, 17:00 BRT in US summer; opens 10:00 either way.
    const Venue& b3 = tms_venue("B3");
    QCOMPARE(session_close_utc(b3, QDate(2026, 1, 15)), tms_utc(2026, 1, 15, 21, 0));
    QCOMPARE(session_close_utc(b3, QDate(2026, 7, 15)), tms_utc(2026, 7, 15, 20, 0));
    QCOMPARE(session_open_utc(b3, QDate(2026, 7, 15)), tms_utc(2026, 7, 15, 13, 0));
    // BMV keeps New York's 09:30: 08:30 CST in US winter, 07:30 CST in US summer.
    const Venue& bmv = tms_venue("BMV");
    QCOMPARE(session_open_utc(bmv, QDate(2026, 1, 15)), tms_utc(2026, 1, 15, 14, 30));
    QCOMPARE(session_open_utc(bmv, QDate(2026, 7, 15)), tms_utc(2026, 7, 15, 13, 30));
}

void TstMarketSessions::crypto_never_closes() {
    const Venue& c = tms_venue("CRYPTO");
    const SessionStatus st = session_status(c, tms_utc(2026, 1, 17, 3, 0)); // Saturday
    QCOMPARE(st.state, SessionState::Open);
    QVERIFY(!st.next_change_utc.isValid());
    QVERIFY(is_trading_day(c, QDate(2026, 1, 17)));
    QVERIFY(is_trading_day(c, QDate(2026, 12, 25)));
}

// ── Holidays ────────────────────────────────────────────────────────────────

void TstMarketSessions::holiday_calendars_data() {
    QTest::addColumn<QString>("venue");
    QTest::addColumn<QDate>("date");
    QTest::addColumn<bool>("trading");

    // US (rule-based)
    QTest::newRow("NYSE Thanksgiving 2026") << "NYSE" << QDate(2026, 11, 26) << false;
    QTest::newRow("NASDAQ Thanksgiving 2026") << "NASDAQ" << QDate(2026, 11, 26) << false;
    QTest::newRow("NYSE Good Friday 2026") << "NYSE" << QDate(2026, 4, 3) << false;
    QTest::newRow("NYSE Good Friday 2027") << "NYSE" << QDate(2027, 3, 26) << false;
    QTest::newRow("NYSE Jul 3 2026 observed") << "NYSE" << QDate(2026, 7, 3) << false;
    QTest::newRow("NYSE Juneteenth 2027 observed Fri") << "NYSE" << QDate(2027, 6, 18) << false;
    QTest::newRow("NYSE Jul 5 2027 observed") << "NYSE" << QDate(2027, 7, 5) << false;
    QTest::newRow("NYSE Christmas 2027 observed Fri") << "NYSE" << QDate(2027, 12, 24) << false;
    QTest::newRow("NYSE Dec 31 2027 open (New Year on Sat)") << "NYSE" << QDate(2027, 12, 31) << true;
    QTest::newRow("NYSE MLK 2028") << "NYSE" << QDate(2028, 1, 17) << false;
    QTest::newRow("NYSE Carter mourning 2025") << "NYSE" << QDate(2025, 1, 9) << false;
    QTest::newRow("NYSE ordinary Monday") << "NYSE" << QDate(2026, 11, 16) << true;
    // UK
    QTest::newRow("LSE Boxing Day substitute 2026") << "LSE" << QDate(2026, 12, 28) << false;
    QTest::newRow("LSE Easter Monday 2026") << "LSE" << QDate(2026, 4, 6) << false;
    QTest::newRow("LSE Christmas substitute 2027") << "LSE" << QDate(2027, 12, 27) << false;
    QTest::newRow("LSE Boxing substitute 2027") << "LSE" << QDate(2027, 12, 28) << false;
    QTest::newRow("LSE Spring bank 2026") << "LSE" << QDate(2026, 5, 25) << false;
    // Germany / Euronext
    QTest::newRow("Xetra Christmas Eve 2026") << "XETRA" << QDate(2026, 12, 24) << false;
    QTest::newRow("Xetra Whit Monday 2026 trades") << "XETRA" << QDate(2026, 5, 25) << true;
    QTest::newRow("Frankfurt Labour Day 2026") << "FRA" << QDate(2026, 5, 1) << false;
    QTest::newRow("Paris Boxing Day 2028") << "PARIS" << QDate(2028, 12, 26) << false;
    // Japan
    QTest::newRow("TSE Jan 2 2026") << "TSE" << QDate(2026, 1, 2) << false;
    QTest::newRow("TSE Golden Week substitute 2026") << "TSE" << QDate(2026, 5, 6) << false;
    QTest::newRow("TSE Citizens' holiday 2026") << "TSE" << QDate(2026, 9, 22) << false;
    QTest::newRow("TSE equinox substitute 2027") << "TSE" << QDate(2027, 3, 22) << false;
    QTest::newRow("TSE Sports Day 2028") << "TSE" << QDate(2028, 10, 9) << false;
    QTest::newRow("TSE ordinary day") << "TSE" << QDate(2026, 1, 5) << true;
    // India
    QTest::newRow("NSE civic polls 2026") << "NSE" << QDate(2026, 1, 15) << false;
    QTest::newRow("BSE Diwali Balipratipada 2026") << "BSE" << QDate(2026, 11, 10) << false;
    QTest::newRow("NSE day after Balipratipada") << "NSE" << QDate(2026, 11, 11) << true;
    // Hong Kong / China
    QTest::newRow("HKEX day after Easter Monday 2026") << "HKEX" << QDate(2026, 4, 7) << false;
    QTest::newRow("HKEX first weekday after Christmas 2027") << "HKEX" << QDate(2027, 12, 27) << false;
    QTest::newRow("SSE Spring Festival 2026") << "SSE" << QDate(2026, 2, 20) << false;
    QTest::newRow("SSE reopens 2026-02-24") << "SSE" << QDate(2026, 2, 24) << true;
    QTest::newRow("SZSE National Day 2027") << "SZSE" << QDate(2027, 10, 7) << false;
}

void TstMarketSessions::holiday_calendars() {
    QFETCH(QString, venue);
    QFETCH(QDate, date);
    QFETCH(bool, trading);
    QCOMPARE(is_trading_day(tms_venue(qPrintable(venue)), date), trading);
}

void TstMarketSessions::holiday_state_and_early_close() {
    const Venue& nyse = tms_venue("NYSE");
    const SessionStatus st = session_status(nyse, tms_utc(2026, 11, 26, 15, 0));
    QCOMPARE(st.state, SessionState::Holiday);
    QCOMPARE(st.next_change_utc, tms_utc(2026, 11, 27, 9, 0)); // Friday 04:00 EST pre-market
    QVERIFY(!holiday_name(nyse, QDate(2026, 11, 26)).isEmpty());

    // Day after Thanksgiving: 13:00 EST close, after-hours until 17:00.
    const DaySchedule half = day_schedule(nyse, QDate(2026, 11, 27));
    QVERIFY(half.trading);
    QVERIFY(half.early_close);
    QCOMPARE(half.close_utc, tms_utc(2026, 11, 27, 18, 0));
    QCOMPARE(half.post_close_utc, tms_utc(2026, 11, 27, 22, 0));
    QCOMPARE(tms_state("NYSE", tms_utc(2026, 11, 27, 18, 30)), SessionState::Post);
    QCOMPARE(session_minutes(nyse, QDate(2026, 11, 27)), 210.0);

    // HKEX half day: morning session only, no lunch, closing auction to 12:10.
    const DaySchedule hk = day_schedule(tms_venue("HKEX"), QDate(2026, 12, 24));
    QVERIFY(hk.trading && hk.early_close);
    QVERIFY(!hk.lunch_start_utc.isValid());
    QCOMPARE(hk.close_utc, tms_utc(2026, 12, 24, 4, 0));
    QCOMPARE(hk.post_close_utc, tms_utc(2026, 12, 24, 4, 10));

    // LSE and Euronext Christmas Eve half days, in local time.
    QCOMPARE(session_close_utc(tms_venue("LSE"), QDate(2026, 12, 24)), tms_utc(2026, 12, 24, 12, 30));
    QCOMPARE(session_close_utc(tms_venue("PARIS"), QDate(2026, 12, 24)), tms_utc(2026, 12, 24, 13, 5));
    QCOMPARE(session_close_utc(tms_venue("LISBON"), QDate(2026, 12, 24)), tms_utc(2026, 12, 24, 13, 5));

    // A holiday on a weekend is just a weekend.
    QCOMPARE(tms_state("XETRA", tms_utc(2027, 12, 25, 10, 0)), SessionState::Closed);
    QVERIFY(!session_open_utc(nyse, QDate(2026, 11, 26)).isValid());
}

void TstMarketSessions::holiday_coverage_report() {
    QCOMPARE(holiday_coverage(tms_venue("NYSE"), 2027), HolidayCoverage::Full);
    QCOMPARE(holiday_coverage(tms_venue("LSE"), 2028), HolidayCoverage::Full);
    QCOMPARE(holiday_coverage(tms_venue("TSE"), 2028), HolidayCoverage::Full);
    QCOMPARE(holiday_coverage(tms_venue("NSE"), 2026), HolidayCoverage::Full);
    QCOMPARE(holiday_coverage(tms_venue("NSE"), 2027), HolidayCoverage::None);
    QCOMPARE(holiday_coverage(tms_venue("HKEX"), 2027), HolidayCoverage::Full);
    QCOMPARE(holiday_coverage(tms_venue("HKEX"), 2028), HolidayCoverage::None);
    QCOMPARE(holiday_coverage(tms_venue("SSE"), 2027), HolidayCoverage::Full);
    QCOMPARE(holiday_coverage(tms_venue("ASX"), 2026), HolidayCoverage::None);
    QCOMPARE(holiday_coverage(tms_venue("CME"), 2026), HolidayCoverage::None);
}

void TstMarketSessions::json_overrides() {
    const Venue& asx = tms_venue("ASX");
    QVERIFY(is_trading_day(asx, QDate(2026, 1, 26)));

    const QByteArray json = R"({
      "calendars": {
        "asx": {
          "years": [2026],
          "days": [ {"date": "2026-01-26", "name": "Australia Day"},
                    {"date": "2026-12-24", "close": "14:10"} ]
        },
        "IN": {
          "days": [ {"date": "2026-11-08", "open": "18:00", "close": "19:15", "name": "Muhurat Trading"},
                    {"date": "2026-11-10", "remove": true},
                    {"date": "2027-01-26", "name": "Republic Day"} ]
        }
      }
    })";
    QString error;
    QVERIFY2(load_holiday_overrides_json(json, &error), qPrintable(error));

    QVERIFY(!is_trading_day(asx, QDate(2026, 1, 26)));
    QCOMPARE(holiday_name(asx, QDate(2026, 1, 26)), QStringLiteral("Australia Day"));
    QCOMPARE(holiday_coverage(asx, 2026), HolidayCoverage::Full);
    QCOMPARE(session_close_utc(asx, QDate(2026, 12, 24)), tms_utc(2026, 12, 24, 3, 10)); // 14:10 AEDT

    // Special Sunday session on NSE; removed entry trades again; 2027 now partial.
    const Venue& nse = tms_venue("NSE");
    QVERIFY(is_trading_day(nse, QDate(2026, 11, 8)));
    QCOMPARE(tms_state("NSE", tms_utc(2026, 11, 8, 12, 45)), SessionState::Open); // 18:15 IST
    QCOMPARE(tms_state("NSE", tms_utc(2026, 11, 8, 14, 0)), SessionState::Closed);
    QVERIFY(is_trading_day(nse, QDate(2026, 11, 10)));
    QCOMPARE(holiday_coverage(nse, 2027), HolidayCoverage::Partial);
    QVERIFY(!is_trading_day(tms_venue("BSE"), QDate(2027, 1, 26))); // shared "IN" calendar

    reset_holiday_overrides();
    QVERIFY(is_trading_day(asx, QDate(2026, 1, 26)));
    QVERIFY(!is_trading_day(nse, QDate(2026, 11, 10)));
}

void TstMarketSessions::json_override_rejects_bad_input() {
    QString error;
    QVERIFY(!load_holiday_overrides_json("not json", &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!load_holiday_overrides_json(R"({"calendars": {"US": {"days": [{"date": "2026-13-40"}]}}})", &error));
    QVERIFY(!load_holiday_overrides_json(R"({"calendars": {"US": {"days": [{"date": "2026-11-16", "close": "1pm"}]}}})",
                                         &error));
    // A rejected file changes nothing.
    QVERIFY(is_trading_day(tms_venue("NYSE"), QDate(2026, 11, 16)));
    QVERIFY(load_holiday_overrides(QStringLiteral("/definitely/not/here/market_holidays.json"), &error));
}

// ── Calendar arithmetic ─────────────────────────────────────────────────────

void TstMarketSessions::trading_date_rolls() {
    // FX rolls at 17:00 New York: Friday 17:30 EST is already Saturday's date.
    QCOMPARE(trading_date(tms_venue("FX"), tms_utc(2026, 1, 16, 22, 30)), QDate(2026, 1, 17));
    QCOMPARE(trading_date(tms_venue("FX"), tms_utc(2026, 1, 16, 21, 30)), QDate(2026, 1, 16));
    // Equities use the local calendar date: 23:00 EST Thursday is still Thursday.
    QCOMPARE(trading_date(tms_venue("NYSE"), tms_utc(2026, 1, 16, 4, 0)), QDate(2026, 1, 15));
    // 08:00 JST Friday is Thursday 23:00 UTC.
    QCOMPARE(trading_date(tms_venue("TSE"), tms_utc(2026, 1, 15, 23, 0)), QDate(2026, 1, 16));
    QCOMPARE(venue_timezone(tms_venue("NSE")).id(), QByteArray("Asia/Kolkata"));
}

void TstMarketSessions::session_minutes_and_bars_per_year() {
    const Venue& nyse = tms_venue("NYSE");
    QCOMPARE(session_minutes(nyse), 390.0);
    QCOMPARE(session_minutes(tms_venue("NSE")), 375.0);
    QCOMPARE(session_minutes(tms_venue("TSE")), 330.0); // 150 + 180
    QCOMPARE(session_minutes(tms_venue("HKEX")), 330.0);
    QCOMPARE(session_minutes(tms_venue("FX")), 1440.0);
    QCOMPARE(session_minutes(tms_venue("CME")), 1380.0);
    QCOMPARE(session_minutes(nyse, QDate(2026, 11, 26)), 0.0);
    // TASE Friday is short.
    QCOMPARE(session_minutes(tms_venue("TASE"), QDate(2026, 1, 16)), 230.0);

    QCOMPARE(trading_days_per_year(nyse), 252.0);
    QCOMPARE(trading_days_per_year(tms_venue("CRYPTO")), 365.0);
    QCOMPARE(trading_days_per_year(tms_venue("FX")), 260.0);

    QCOMPARE(bars_per_year(nyse, 60), 252.0 * 390.0);
    QCOMPARE(bars_per_year(nyse, 3600), 252.0 * 7.0); // 09:30, 10:30, ... 15:30
    QCOMPARE(bars_per_year(nyse, 86400), 252.0);
    QCOMPARE(bars_per_year(nyse, 2 * 86400), 126.0);
    QVERIFY(qAbs(bars_per_year(nyse, 7 * 86400) - 52.1775) < 1e-3);
    QCOMPARE(bars_per_year(tms_venue("CRYPTO"), 3600), 365.0 * 24.0);
    QCOMPARE(bars_per_year(tms_venue("FX"), 86400), 260.0);
    QCOMPARE(bars_per_year(tms_venue("TSE"), 3600), 245.0 * 6.0); // 3 morning + 3 afternoon
    QCOMPARE(bars_per_year(tms_venue("HKEX"), 60), 247.0 * 330.0);
    QCOMPARE(bars_per_year(nyse, 0), 0.0);
}

void TstMarketSessions::bucket_nse_0915() {
    const Venue& nse = tms_venue("NSE");
    // 10:30 IST -> the 10:15 hourly bar (04:45 UTC).
    QCOMPARE(session_bucket_start(nse, tms_utc(2026, 1, 15, 5, 0), 3600), tms_utc(2026, 1, 15, 4, 45));
    // 09:20 IST -> 09:15 (03:45 UTC).
    QCOMPARE(session_bucket_start(nse, tms_utc(2026, 1, 15, 3, 50), 3600), tms_utc(2026, 1, 15, 3, 45));
    // 15m: 10:37 IST -> 10:30.
    QCOMPARE(session_bucket_start(nse, tms_utc(2026, 1, 15, 5, 7), 900), tms_utc(2026, 1, 15, 5, 0));
    // Pre-open 09:05 IST extends the grid backwards: 1h bucket 08:15.
    QCOMPARE(session_bucket_start(nse, tms_utc(2026, 1, 15, 3, 35), 3600), tms_utc(2026, 1, 15, 2, 45));
    // Daily -> the session open.
    QCOMPARE(session_bucket_start(nse, tms_utc(2026, 1, 15, 8, 0), 86400), tms_utc(2026, 1, 15, 3, 45));
    // The UTC-epoch grid would have said 05:00 / 04:00.
    QCOMPARE(session_bucket_start(nse, tms_utc(2026, 1, 15, 5, 0), 0), tms_utc(2026, 1, 15, 5, 0));
}

void TstMarketSessions::bucket_across_utc_midnight() {
    // ASX in January opens 10:00 AEDT = 23:00 UTC on the previous UTC date.
    const Venue& asx = tms_venue("ASX");
    // 13:30 AEDT on 15 Jan (02:30 UTC): the 4h bar opened 10:00 AEDT = 14 Jan 23:00 UTC.
    QCOMPARE(session_bucket_start(asx, tms_utc(2026, 1, 15, 2, 30), 4 * 3600), tms_utc(2026, 1, 14, 23, 0));
    // 14:30 AEDT: second 4h bar (14:00 AEDT = 03:00 UTC).
    QCOMPARE(session_bucket_start(asx, tms_utc(2026, 1, 15, 3, 30), 4 * 3600), tms_utc(2026, 1, 15, 3, 0));
    // Hourly across the UTC date line.
    QCOMPARE(session_bucket_start(asx, tms_utc(2026, 1, 15, 0, 30), 3600), tms_utc(2026, 1, 15, 0, 0));
    QCOMPARE(session_bucket_start(asx, tms_utc(2026, 1, 14, 23, 59), 3600), tms_utc(2026, 1, 14, 23, 0));
    // Daily bucket = the open, on the previous UTC date.
    QCOMPARE(session_bucket_start(asx, tms_utc(2026, 1, 15, 4, 0), 86400), tms_utc(2026, 1, 14, 23, 0));
    QCOMPARE(session_status(asx, tms_utc(2026, 1, 14, 23, 30)).trading_date, QDate(2026, 1, 15));
}

void TstMarketSessions::bucket_after_lunch_and_fx() {
    // HKEX restarts the grid at the 13:00 afternoon open.
    const Venue& hk = tms_venue("HKEX");
    QCOMPARE(session_bucket_start(hk, tms_utc(2026, 1, 15, 2, 45), 3600), tms_utc(2026, 1, 15, 2, 30)); // 10:30
    QCOMPARE(session_bucket_start(hk, tms_utc(2026, 1, 15, 5, 30), 3600), tms_utc(2026, 1, 15, 5, 0));  // 13:00
    // FX 4h bars align to the 17:00 New York open (22:00 UTC in winter).
    const Venue& fx = tms_venue("FX");
    QCOMPARE(session_bucket_start(fx, tms_utc(2026, 1, 18, 23, 30), 4 * 3600), tms_utc(2026, 1, 18, 22, 0));
    QCOMPARE(session_bucket_start(fx, tms_utc(2026, 1, 19, 3, 0), 4 * 3600), tms_utc(2026, 1, 19, 2, 0));
    // Crypto: UTC midnight, i.e. the epoch grid.
    QCOMPARE(session_bucket_start(tms_venue("CRYPTO"), tms_utc(2026, 1, 17, 5, 30), 4 * 3600),
             tms_utc(2026, 1, 17, 4, 0));
}

// ── InstrumentCurrency ──────────────────────────────────────────────────────

void TstMarketSessions::currency_for_symbol_cases_data() {
    QTest::addColumn<QString>("symbol");
    QTest::addColumn<QString>("hint");
    QTest::addColumn<QString>("ccy");

    const struct {
        const char* sym;
        const char* hint;
        const char* ccy;
    } rows[] = {
        {"AAPL", "", "USD"},         {"BRK-B", "", "USD"},       {"BRK.B", "", ""},         {"VOD.L", "", "GBp"},
        {"NPN.JO", "", "ZAc"},       {"TEVA.TA", "", "ILA"},     {"RELIANCE.NS", "", "INR"}, {"7203.T", "", "JPY"},
        {"SAP.DE", "", "EUR"},       {"NESN.SW", "", "CHF"},     {"PETR4.SA", "", "BRL"},   {"2222.SR", "", "SAR"},
        {"EMAAR.AE", "", "AED"},     {"PKO.WA", "", "PLN"},      {"CEZ.PR", "", "CZK"},     {"OPAP.AT", "", "EUR"},
        {"XYZ.IL", "", ""},          {"BTC-USD", "", "USD"},     {"SOL-USDT", "", "USDT"},  {"ETH-EUR", "", "EUR"},
        {"BTC-USDC", "", "USDC"},    {"ETH-BTC", "", "BTC"},     {"ETH-FDUSD", "", "FDUSD"},
        {"EURUSD=X", "", "USD"},     {"USDJPY=X", "", "JPY"},    {"JPY=X", "", "JPY"},      {"GC=F", "", ""},
        {"^GSPC", "", ""},           {"RELIANCE", "NSE", "INR"}, {"NSE:SBIN-EQ", "", "INR"}, {"GOLD", "MCX", ""},
        {"", "", ""},
    };
    for (const auto& r : rows)
        QTest::newRow(qPrintable(QStringLiteral("%1|%2").arg(QString::fromLatin1(r.sym), QString::fromLatin1(r.hint))))
            << QString::fromLatin1(r.sym) << QString::fromLatin1(r.hint) << QString::fromLatin1(r.ccy);
}

void TstMarketSessions::currency_for_symbol_cases() {
    QFETCH(QString, symbol);
    QFETCH(QString, hint);
    QFETCH(QString, ccy);
    QCOMPARE(currency_for_symbol(symbol, hint), ccy);
}

void TstMarketSessions::minor_units() {
    QCOMPARE(minor_unit_divisor(QStringLiteral("GBp")), 100.0);
    QCOMPARE(minor_unit_divisor(QStringLiteral("GBX")), 100.0);
    QCOMPARE(minor_unit_divisor(QStringLiteral("GBP")), 1.0); // case matters
    QCOMPARE(minor_unit_divisor(QStringLiteral("ZAc")), 100.0);
    QCOMPARE(minor_unit_divisor(QStringLiteral("ILA")), 100.0);
    QCOMPARE(minor_unit_divisor(QStringLiteral("KWF")), 1000.0);
    QCOMPARE(minor_unit_divisor(QString()), 1.0);
    QVERIFY(is_minor_unit(QStringLiteral("USX")));
    QVERIFY(!is_minor_unit(QStringLiteral("USD")));
    QCOMPARE(major_currency(QStringLiteral("GBp")), QStringLiteral("GBP"));
    QCOMPARE(major_currency(QStringLiteral("ZAc")), QStringLiteral("ZAR"));
    QCOMPARE(major_currency(QStringLiteral("ILA")), QStringLiteral("ILS"));
    QCOMPARE(major_currency(QStringLiteral(" eur ")), QStringLiteral("EUR"));
    QCOMPARE(major_currency(QString()), QString());
    QCOMPARE(to_major_units(12345.0, QStringLiteral("GBp")), 123.45);
}

void TstMarketSessions::minor_units_agree_with_currency_table() {
    // core/market keeps its own minor-unit list (core cannot include ui); it
    // must say exactly what the formatter's currency table says (currency
    // design case 129).
    namespace uifmt = fincept::ui::formatting;
    const QStringList codes = {"GBp", "GBX", "GBx", "ZAc", "ZAC", "ILA", "ILa", "KWF", "USX", "USc", "GBP", "USD",
                               "JPY", "EUR", "INR", "USDT", "BTC"};
    for (const QString& c : codes) {
        QVERIFY2(fincept::core::market::minor_unit_divisor(c) == double(uifmt::minor_unit_divisor(c)), qPrintable(c));
        QVERIFY2(fincept::core::market::major_currency(c) == uifmt::major_currency(c), qPrintable(c));
        QVERIFY2(fincept::core::market::is_minor_unit(c) == uifmt::is_minor_unit(c), qPrintable(c));
    }
    // Every minor unit of the table is known to core.
    for (const QString& c : uifmt::known_currency_codes())
        if (uifmt::is_minor_unit(c))
            QVERIFY2(fincept::core::market::is_minor_unit(c), qPrintable(c));
    // A stablecoin the symbol parser returns is a crypto row of the table.
    for (const char* sym : {"X-USDT", "X-USDC", "X-DAI", "X-FDUSD", "X-TUSD", "X-BUSD"}) {
        const QString ccy = currency_for_symbol(QString::fromLatin1(sym));
        QVERIFY2(uifmt::is_stablecoin(ccy), sym);
    }
}

QTEST_GUILESS_MAIN(TstMarketSessions)
#include "tst_market_sessions.moc"
