// Unit tests for src/trading/MarketDataRequests.h (header-only, Qt Core only).
//
// These are the rules behind AccountDataStream's on-demand market data:
//   * LatestRequestGate — chart history is "latest request wins": a request made
//     while one is in flight is never dropped, and an answer for a symbol /
//     timeframe the chart has since left is never delivered as current. (The
//     stream used to drop the newer request outright, so a quick symbol or
//     timeframe switch left the chart on the old answer — or on nothing.)
//   * market_data_failure_reason — the reason a user sees when a fetch fails.
//   * market_symbols_match — one instrument spelled the WebSocket way and the
//     REST way ("NSE:SBIN-EQ" / "SBIN").

#include "trading/MarketDataRequests.h"

#include <QString>
#include <QTest>

using fincept::trading::LatestRequestGate;
using fincept::trading::market_data_failure_reason;
using fincept::trading::market_symbol_core;
using fincept::trading::market_symbols_match;
using fincept::trading::MarketDataKey;

namespace {
const MarketDataKey kA{QStringLiteral("RELIANCE"), QStringLiteral("15m")};
const MarketDataKey kB{QStringLiteral("TCS"), QStringLiteral("15m")};
const MarketDataKey kC{QStringLiteral("RELIANCE"), QStringLiteral("1h")}; // same symbol, other timeframe
constexpr qint64 kAbandon = LatestRequestGate::kAbandonAfterMs;
} // namespace

class TstMarketDataRequests : public QObject {
    Q_OBJECT

  private slots:
    // LatestRequestGate
    void first_request_is_issued_at_once();
    void request_while_in_flight_is_queued_not_dropped();
    void newest_queued_request_replaces_older_queued_one();
    void stale_answer_is_not_current_and_the_newest_is_issued();
    void same_symbol_other_timeframe_is_a_different_request();
    void switching_back_to_the_in_flight_key_needs_no_reissue();
    void duplicate_request_collapses_into_the_one_in_flight();
    void failure_answers_follow_the_same_rules();
    void silent_request_stops_blocking_after_the_abandon_window();
    void request_after_the_abandon_window_is_issued_at_once();
    void late_answer_of_an_abandoned_request_is_delivered_only_if_current();
    void unknown_ticket_changes_nothing();
    void outstanding_tracks_the_newest_request();
    void ms_until_expiry();

    // market_data_failure_reason
    void reason_spells_out_an_expired_session();
    void reason_for_no_data_is_the_callers_text();
    void reason_maps_missing_credentials();
    void reason_passes_broker_text_through();

    // market_symbols_match
    void symbols_match_across_spellings();
    void symbols_that_differ_do_not_match();
};

// ── LatestRequestGate ────────────────────────────────────────────────────────

void TstMarketDataRequests::first_request_is_issued_at_once() {
    LatestRequestGate g;
    const auto t = g.request(kA, 0);
    QVERIFY(t.has_value());
    QCOMPARE(t->key, kA);
    QVERIFY(t->id != 0);
    QVERIFY(g.outstanding());

    const auto done = g.complete(t->id, kA, 10);
    QVERIFY(done.current);
    QVERIFY(!done.next.has_value());
    QVERIFY(!g.outstanding());
}

void TstMarketDataRequests::request_while_in_flight_is_queued_not_dropped() {
    LatestRequestGate g;
    const auto a = g.request(kA, 0);
    QVERIFY(a.has_value());
    QVERIFY(!g.request(kB, 100).has_value()); // queued behind A...
    QCOMPARE(g.wanted(), kB);

    const auto done = g.complete(a->id, kA, 200);
    QVERIFY(!done.current);         // ...A's answer is stale...
    QVERIFY(done.next.has_value()); // ...and B is issued, not lost
    QCOMPARE(done.next->key, kB);

    QVERIFY(g.complete(done.next->id, kB, 300).current);
    QVERIFY(!g.outstanding());
}

void TstMarketDataRequests::newest_queued_request_replaces_older_queued_one() {
    LatestRequestGate g;
    const auto a = g.request(kA, 0);
    QVERIFY(!g.request(kB, 10).has_value());
    QVERIFY(!g.request(kC, 20).has_value()); // B was never issued and never will be

    const auto done = g.complete(a->id, kA, 30);
    QVERIFY(!done.current);
    QCOMPARE(done.next->key, kC);
    QVERIFY(g.complete(done.next->id, kC, 40).current);
}

void TstMarketDataRequests::stale_answer_is_not_current_and_the_newest_is_issued() {
    // The defect this gate exists for: the answer for the previous symbol must not
    // be painted under the new one.
    LatestRequestGate g;
    const auto a = g.request(kA, 0);
    g.request(kB, 5);
    const auto done = g.complete(a->id, kA, 9);
    QVERIFY(!done.current);
    QVERIFY(done.next.has_value());
}

void TstMarketDataRequests::same_symbol_other_timeframe_is_a_different_request() {
    LatestRequestGate g;
    const auto a = g.request(kA, 0);    // RELIANCE 15m
    QVERIFY(!g.request(kC, 1).has_value()); // RELIANCE 1h
    const auto done = g.complete(a->id, kA, 2);
    QVERIFY(!done.current);
    QCOMPARE(done.next->key, kC);
}

void TstMarketDataRequests::switching_back_to_the_in_flight_key_needs_no_reissue() {
    LatestRequestGate g;
    const auto a = g.request(kA, 0);
    g.request(kB, 1);
    g.request(kA, 2); // A -> B -> A before A answered

    const auto done = g.complete(a->id, kA, 3);
    QVERIFY(done.current);          // A is what is wanted again
    QVERIFY(!done.next.has_value()); // and B is not fetched for nothing
    QVERIFY(!g.outstanding());
}

void TstMarketDataRequests::duplicate_request_collapses_into_the_one_in_flight() {
    LatestRequestGate g;
    const auto a = g.request(kA, 0);
    QVERIFY(!g.request(kA, 1).has_value());
    const auto done = g.complete(a->id, kA, 2);
    QVERIFY(done.current);
    QVERIFY(!done.next.has_value());
}

void TstMarketDataRequests::failure_answers_follow_the_same_rules() {
    // The gate does not care whether the answer is data or a failure: a failure for
    // a stale key is not shown either, and the newest request still goes out.
    LatestRequestGate g;
    const auto a = g.request(kA, 0);
    g.request(kB, 1);
    const auto failed_a = g.complete(a->id, kA, 2); // e.g. "No data" for RELIANCE
    QVERIFY(!failed_a.current);
    QVERIFY(failed_a.next.has_value());
    const auto failed_b = g.complete(failed_a.next->id, kB, 3);
    QVERIFY(failed_b.current); // TCS's failure IS shown
}

void TstMarketDataRequests::silent_request_stops_blocking_after_the_abandon_window() {
    LatestRequestGate g;
    const auto a = g.request(kA, 0);
    QVERIFY(!g.request(kB, kAbandon - 1).has_value()); // A is merely slow: queue
    QVERIFY(!g.expire(kAbandon - 1).has_value());

    const auto b = g.expire(kAbandon); // A has been silent too long
    QVERIFY(b.has_value());
    QCOMPARE(b->key, kB);
    QVERIFY(b->id != a->id);
    QVERIFY(!g.expire(kAbandon * 3).has_value()); // nothing else waiting

    QVERIFY(g.complete(b->id, kB, kAbandon + 10).current);
}

void TstMarketDataRequests::request_after_the_abandon_window_is_issued_at_once() {
    LatestRequestGate g;
    QVERIFY(g.request(kA, 0).has_value());
    const auto b = g.request(kB, kAbandon);
    QVERIFY(b.has_value());
    QCOMPARE(b->key, kB);
}

void TstMarketDataRequests::late_answer_of_an_abandoned_request_is_delivered_only_if_current() {
    LatestRequestGate g;
    const auto a1 = g.request(kA, 0);
    const auto a2 = g.request(kA, kAbandon); // retried after A1 went silent
    QVERIFY(a2.has_value());

    // A1 finally answers: still what is wanted, so it is delivered...
    const auto late = g.complete(a1->id, kA, kAbandon + 5);
    QVERIFY(late.current);
    QVERIFY(!late.next.has_value());
    QVERIFY(g.outstanding()); // ...and A2 is still in flight, untouched

    QVERIFY(g.complete(a2->id, kA, kAbandon + 6).current);
    QVERIFY(!g.outstanding());

    // A late answer for a key that is no longer wanted is dropped.
    LatestRequestGate h;
    const auto x = h.request(kA, 0);
    const auto y = h.request(kB, kAbandon);
    QVERIFY(y.has_value());
    QVERIFY(!h.complete(x->id, kA, kAbandon + 1).current);
}

void TstMarketDataRequests::unknown_ticket_changes_nothing() {
    LatestRequestGate g;
    const auto a = g.request(kA, 0);
    g.request(kB, 1);
    const auto bogus = g.complete(a->id + 1000, kB, 2); // never issued
    QVERIFY(bogus.current);          // it is for the wanted key, so deliverable...
    QVERIFY(!bogus.next.has_value()); // ...but the in-flight request is not disturbed
    QVERIFY(g.outstanding());
    QVERIFY(!g.complete(0, kA, 3).current); // id 0 is never a ticket; kA is not wanted
    const auto real = g.complete(a->id, kA, 4);
    QCOMPARE(real.next->key, kB);
}

void TstMarketDataRequests::outstanding_tracks_the_newest_request() {
    // AccountDataStream::adopt_subscriptions re-issues wanted() on a replacement
    // stream when outstanding() is true.
    LatestRequestGate g;
    QVERIFY(!g.outstanding());
    QVERIFY(!g.has_wanted());
    const auto a = g.request(kA, 0);
    QVERIFY(g.outstanding());
    g.request(kB, 1);
    QVERIFY(g.outstanding());
    QCOMPARE(g.wanted(), kB);
    const auto done = g.complete(a->id, kA, 2);
    QVERIFY(g.outstanding()); // B is in flight now
    g.complete(done.next->id, kB, 3);
    QVERIFY(!g.outstanding());
    QCOMPARE(g.wanted(), kB); // remembered, but answered
}

void TstMarketDataRequests::ms_until_expiry() {
    LatestRequestGate g;
    QCOMPARE(g.ms_until_expiry(0), qint64(-1)); // nothing waiting
    g.request(kA, 1000);
    QCOMPARE(g.ms_until_expiry(1000), qint64(-1)); // in flight, nothing queued
    g.request(kB, 1500);
    QCOMPARE(g.ms_until_expiry(1500), kAbandon - 500);
    QCOMPARE(g.ms_until_expiry(1000 + kAbandon + 99), qint64(0));
}

// ── market_data_failure_reason ───────────────────────────────────────────────

void TstMarketDataRequests::reason_spells_out_an_expired_session() {
    const QString nd = QStringLiteral("no data text");
    const QString r = market_data_failure_reason(QStringLiteral("[TOKEN_EXPIRED] Invalid access token"), nd);
    QVERIFY2(r.contains(QLatin1String("session has expired")), qPrintable(r));
    QVERIFY2(r.contains(QLatin1String("ACCOUNTS")), qPrintable(r));
    QVERIFY2(r.contains(QLatin1String("(Invalid access token)")), qPrintable(r)); // the broker's own words
    QVERIFY(!r.contains(QLatin1String("[TOKEN_EXPIRED]")));

    // Separators after the tag are dropped; a bare tag adds no empty "()".
    QVERIFY(market_data_failure_reason(QStringLiteral("[TOKEN_EXPIRED]: TokenException"), nd)
                .endsWith(QLatin1String("(TokenException)")));
    const QString bare = market_data_failure_reason(QStringLiteral("  [TOKEN_EXPIRED]  "), nd);
    QVERIFY(bare.contains(QLatin1String("session has expired")));
    QVERIFY(!bare.contains(QLatin1Char('(')));
}

void TstMarketDataRequests::reason_for_no_data_is_the_callers_text() {
    const QString nd = QStringLiteral("The broker returned no price history for TCS on the 1m timeframe.");
    QCOMPARE(market_data_failure_reason(QString(), nd), nd);
    QCOMPARE(market_data_failure_reason(QStringLiteral("   "), nd), nd);
    QCOMPARE(market_data_failure_reason(QStringLiteral("No data"), nd), nd); // HistoricalDataService's word
    QCOMPARE(market_data_failure_reason(QStringLiteral("no DATA"), nd), nd);
}

void TstMarketDataRequests::reason_maps_missing_credentials() {
    const QString r = market_data_failure_reason(QStringLiteral("No credentials"), QStringLiteral("x"));
    QVERIFY2(r.contains(QLatin1String("credentials")), qPrintable(r));
    QVERIFY2(r.contains(QLatin1String("ACCOUNTS")), qPrintable(r));
}

void TstMarketDataRequests::reason_passes_broker_text_through() {
    QCOMPARE(market_data_failure_reason(QStringLiteral("  HTTP 429: Too many requests  "), QStringLiteral("x")),
             QStringLiteral("HTTP 429: Too many requests"));
    // "No data available for the given range" is the broker's own sentence, not the bare token.
    QCOMPARE(market_data_failure_reason(QStringLiteral("No data available for the given range"), QStringLiteral("x")),
             QStringLiteral("No data available for the given range"));
}

// ── market_symbols_match ─────────────────────────────────────────────────────

void TstMarketDataRequests::symbols_match_across_spellings() {
    QVERIFY(market_symbols_match(QStringLiteral("NSE:SBIN-EQ"), QStringLiteral("SBIN")));
    QVERIFY(market_symbols_match(QStringLiteral("SBIN"), QStringLiteral("NSE:SBIN-EQ")));
    QVERIFY(market_symbols_match(QStringLiteral("sbin"), QStringLiteral("SBIN")));
    QVERIFY(market_symbols_match(QStringLiteral("NSE:BAJAJ-AUTO-EQ"), QStringLiteral("BAJAJ-AUTO")));
    QVERIFY(market_symbols_match(QStringLiteral("BSE:M&M"), QStringLiteral("M&M")));
    QCOMPARE(market_symbol_core(QStringLiteral("NSE:RELIANCE-EQ")), QStringLiteral("RELIANCE"));
    // The exchange prefix is dropped: the selected-symbol filter is single-symbol,
    // not per-exchange (pinned so a change is deliberate).
    QVERIFY(market_symbols_match(QStringLiteral("NSE:RELIANCE"), QStringLiteral("BSE:RELIANCE")));
}

void TstMarketDataRequests::symbols_that_differ_do_not_match() {
    QVERIFY(!market_symbols_match(QStringLiteral("TCS"), QStringLiteral("INFY")));
    QVERIFY(!market_symbols_match(QStringLiteral("NSE:SBIN-EQ"), QStringLiteral("SBI")));
    QVERIFY(!market_symbols_match(QStringLiteral("BAJAJ-AUTO"), QStringLiteral("BAJAJ")));
    // Only the upper-case equity series suffix is a suffix (pinned).
    QVERIFY(!market_symbols_match(QStringLiteral("sbin-eq"), QStringLiteral("SBIN")));
}

QTEST_GUILESS_MAIN(TstMarketDataRequests)
#include "tst_market_data_requests.moc"
