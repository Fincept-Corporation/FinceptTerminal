// Unit tests for src/services/markets/FxRateMath.{h,cpp} — the pure half of
// FxRateService: leg symbols (fiat pairs, crypto / stablecoin "-USD"
// instruments), combining legs or a direct cross into a rate, minor units,
// staleness, the 7-day cap, "as of" = the market's time, and the stablecoin
// 1:1 fallback. Rates are injected: no hub, no network.

#include "services/markets/FxRateMath.h"

#include <QHash>
#include <QTest>
#include <QTimeZone>

#include <cmath>

using namespace fincept::services;
using namespace fincept::services::fxmath;

class TstFxRateMath : public QObject {
    Q_OBJECT

  private slots:
    void leg_symbols();
    void usd_per_unit();
    void combine_minor_units();
    void combine_triangulated();
    void combine_crypto_and_stablecoins();
    void stablecoin_without_rate_is_approximate();
    void staleness_and_hard_cap();
    void as_of_is_market_time();
    void direct_cross_preferred();
    void missing_legs_are_reported();
    void invalid_codes();
};

namespace {

constexpr qint64 kNow = 1'800'000'000'000; // epoch ms (2027-01-15)
constexpr qint64 kMin = 60'000;

struct FakeHub {
    QHash<QString, FxLegSample> samples;
    void put(const QString& sym, double price, qint64 age_ms = 10'000, qint64 market_time_s = 0) {
        samples.insert(sym, FxLegSample{price, kNow - age_ms, market_time_s});
    }
    FxLegLookup lookup() const {
        return [this](const QString& sym) -> std::optional<FxLegSample> {
            const auto it = samples.constFind(sym);
            if (it == samples.constEnd())
                return std::nullopt;
            return it.value();
        };
    }
};

FxCombine combine(const FakeHub& hub, const char* from, const char* to, qint64 stale_after_s = 30 * 60) {
    return fx_combine(QString::fromLatin1(from), QString::fromLatin1(to), hub.lookup(), kNow, stale_after_s);
}

} // namespace

void TstFxRateMath::leg_symbols() {
    QCOMPARE(fx_leg_symbol(QStringLiteral("EUR")), QStringLiteral("EURUSD=X")); // #130
    QCOMPARE(fx_leg_symbol(QStringLiteral("JPY")), QStringLiteral("USDJPY=X"));
    QCOMPARE(fx_leg_symbol(QStringLiteral("GBp")), QStringLiteral("GBPUSD=X"));
    QCOMPARE(fx_leg_symbol(QStringLiteral("USD")), QString());
    QCOMPARE(fx_leg_symbol(QStringLiteral("BTC")), QStringLiteral("BTC-USD")); // #131
    QCOMPARE(fx_leg_symbol(QStringLiteral("USDT")), QStringLiteral("USDT-USD"));
    QCOMPARE(fx_leg_symbol(QStringLiteral("XY")), QString());
    QCOMPARE(fx_leg_symbol(QStringLiteral("DOGE")), QStringLiteral("DOGE-USD"));
    // Gaps: the old form for crypto ("USDBTC=X") does not exist on Yahoo.
    QVERIFY(fx_leg_symbol(QStringLiteral("ETH")) != QStringLiteral("USDETH=X"));
    QCOMPARE(fx_leg_symbol(QStringLiteral("USX")), QString()); // US cents: a USD scale, no leg
    QCOMPARE(fx_leg_symbol(QStringLiteral("ZAc")), QStringLiteral("USDZAR=X"));
    QCOMPARE(fx_leg_symbol(QStringLiteral("AUD")), QStringLiteral("AUDUSD=X"));
    QCOMPARE(fx_leg_symbol(QStringLiteral("NZD")), QStringLiteral("NZDUSD=X"));
    QCOMPARE(fx_leg_symbol(QStringLiteral("KWF")), QStringLiteral("USDKWD=X"));
    QVERIFY(fx_valid_code(QStringLiteral("FDUSD")));
    QVERIFY(!fx_valid_code(QStringLiteral("usd")));
    // Metals: Yahoo has neither USDXAU=X nor XAUUSD=X (checked live) — no leg, no rate.
    QCOMPARE(fx_leg_symbol(QStringLiteral("XAU")), QString());
    QVERIFY(!fx_valid_code(QStringLiteral("XAG")));
}

void TstFxRateMath::usd_per_unit() {
    QCOMPARE(fx_usd_per_unit(QStringLiteral("EUR"), 1.1), 1.1);
    QCOMPARE(fx_usd_per_unit(QStringLiteral("JPY"), 160.0), 1.0 / 160.0);
    QCOMPARE(fx_usd_per_unit(QStringLiteral("BTC"), 65000.0), 65000.0);
    QCOMPARE(fx_usd_per_unit(QStringLiteral("USDT"), 0.999), 0.999);
}

void TstFxRateMath::combine_minor_units() {
    FakeHub hub;
    hub.put(QStringLiteral("GBPUSD=X"), 1.3183);
    const auto c = combine(hub, "GBp", "USD"); // #132
    QVERIFY(c.quote);
    QVERIFY(std::fabs(c.quote->rate - 0.013183) < 1e-12);
    QVERIFY(!c.quote->triangulated);
    QVERIFY(c.missing.isEmpty());

    const auto cents = combine(hub, "USD", "USX"); // #133
    QVERIFY(cents.quote);
    QCOMPARE(cents.quote->rate, 100.0);
    QCOMPARE(cents.quote->received_ms, qint64(0)); // no leg needed
    QVERIFY(cents.missing.isEmpty());

    // Gaps: pence -> US cents, and fils.
    const auto p2c = combine(hub, "GBp", "USX");
    QVERIFY(p2c.quote);
    QVERIFY(std::fabs(p2c.quote->rate - 1.3183) < 1e-12);
    hub.put(QStringLiteral("USDKWD=X"), 0.307);
    const auto fils = combine(hub, "KWF", "USD");
    QVERIFY(fils.quote);
    QVERIFY(std::fabs(fils.quote->rate - (1.0 / 0.307) / 1000.0) < 1e-12);
}

void TstFxRateMath::combine_triangulated() {
    FakeHub hub;
    hub.put(QStringLiteral("EURUSD=X"), 1.1001);
    hub.put(QStringLiteral("USDJPY=X"), 159.452);
    const auto c = combine(hub, "EUR", "JPY"); // #134
    QVERIFY(c.quote);
    QVERIFY(std::fabs(c.quote->rate - 175.4131452) < 1e-6);
    QVERIFY(c.quote->triangulated);
    // The inverse.
    const auto inv = combine(hub, "JPY", "EUR");
    QVERIFY(inv.quote);
    QVERIFY(std::fabs(inv.quote->rate - 1.0 / 175.4131452) < 1e-12);
    // Through USD only: USD legs are not triangulated.
    const auto direct = combine(hub, "EUR", "USD");
    QVERIFY(direct.quote);
    QVERIFY(!direct.quote->triangulated);
}

void TstFxRateMath::combine_crypto_and_stablecoins() {
    FakeHub hub;
    hub.put(QStringLiteral("USDT-USD"), 0.99903);
    hub.put(QStringLiteral("EURUSD=X"), 1.1001);
    hub.put(QStringLiteral("BTC-USD"), 64950.54);
    const auto c = combine(hub, "USDT", "EUR"); // #135
    QVERIFY(c.quote);
    QVERIFY(std::fabs(c.quote->rate - 0.99903 / 1.1001) < 1e-9);
    QVERIFY(c.quote->triangulated);
    QVERIFY(!c.quote->approximate);
    const auto btc = combine(hub, "BTC", "USD");
    QVERIFY(btc.quote);
    QCOMPARE(btc.quote->rate, 64950.54);
    const auto usd_btc = combine(hub, "USD", "BTC");
    QVERIFY(usd_btc.quote);
    QVERIFY(std::fabs(usd_btc.quote->rate - 1.0 / 64950.54) < 1e-15);
    const auto eth_in_btc = combine(hub, "BTC", "EUR");
    QVERIFY(eth_in_btc.quote);
    QVERIFY(std::fabs(eth_in_btc.quote->rate - 64950.54 / 1.1001) < 1e-6);
}

void TstFxRateMath::stablecoin_without_rate_is_approximate() {
    FakeHub hub;
    hub.put(QStringLiteral("EURUSD=X"), 1.1);
    const auto c = combine(hub, "USDC", "EUR");
    QVERIFY(c.quote);
    QVERIFY(c.quote->approximate);
    QVERIFY(c.quote->approx_reason.contains(QStringLiteral("USDC")));
    QVERIFY(std::fabs(c.quote->rate - 1.0 / 1.1) < 1e-12);
    QCOMPARE(c.missing, QStringList{QStringLiteral("USDC")}); // still requested
    // A non-stable crypto without a rate is never guessed.
    const auto btc = combine(hub, "BTC", "EUR");
    QVERIFY(!btc.quote);
    QCOMPARE(btc.missing, QStringList{QStringLiteral("BTC")});
}

void TstFxRateMath::staleness_and_hard_cap() {
    FakeHub hub;
    hub.put(QStringLiteral("EURUSD=X"), 1.1, 31 * kMin); // #136
    const auto c = combine(hub, "EUR", "USD", 30 * 60);
    QVERIFY(c.quote);
    QVERIFY(c.quote->stale);
    QCOMPARE(c.quote->received_ms, kNow - 31 * kMin);
    const auto fresh = combine(hub, "EUR", "USD", 60 * 60);
    QVERIFY(fresh.quote);
    QVERIFY(!fresh.quote->stale);
    hub.put(QStringLiteral("EURUSD=X"), 1.1, 8LL * 24 * 60 * kMin); // #137
    const auto old = combine(hub, "EUR", "USD");
    QVERIFY(!old.quote);
    QCOMPARE(old.missing, QStringList{QStringLiteral("EUR")});
    // The oldest leg decides.
    hub.put(QStringLiteral("EURUSD=X"), 1.1, 5 * kMin);
    hub.put(QStringLiteral("USDJPY=X"), 160.0, 45 * kMin);
    const auto cross = combine(hub, "EUR", "JPY");
    QVERIFY(cross.quote);
    QVERIFY(cross.quote->stale);
    QCOMPARE(cross.quote->received_ms, kNow - 45 * kMin);
}

void TstFxRateMath::as_of_is_market_time() {
    FakeHub hub;
    const qint64 friday_close_s = (kNow - 2 * 24 * 60 * kMin) / 1000; // two days before "now"
    hub.put(QStringLiteral("EURUSD=X"), 1.1, 10'000, friday_close_s); // #138
    const auto c = combine(hub, "EUR", "USD");
    QVERIFY(c.quote);
    QCOMPARE(c.quote->as_of, QDateTime::fromSecsSinceEpoch(friday_close_s, QTimeZone::utc()));
    QVERIFY(!c.quote->stale); // freshness is judged on receipt, not on the market's time
    hub.put(QStringLiteral("EURUSD=X"), 1.1, 10'000, 0);
    const auto r = combine(hub, "EUR", "USD");
    QCOMPARE(r.quote->as_of, QDateTime::fromMSecsSinceEpoch(kNow - 10'000, QTimeZone::utc()));
}

void TstFxRateMath::direct_cross_preferred() {
    FakeHub hub;
    hub.put(QStringLiteral("EURUSD=X"), 1.10);
    hub.put(QStringLiteral("GBPUSD=X"), 1.32);
    hub.put(QStringLiteral("EURGBP=X"), 0.83427);
    const auto c = combine(hub, "EUR", "GBP");
    QVERIFY(c.quote);
    QCOMPARE(c.quote->rate, 0.83427);
    QVERIFY(!c.quote->triangulated);
    // The inverse direct pair is used inverted.
    const auto inv = combine(hub, "GBP", "EUR");
    QVERIFY(inv.quote);
    QVERIFY(std::fabs(inv.quote->rate - 1.0 / 0.83427) < 1e-12);
    // A stale direct cross loses to fresh legs.
    hub.put(QStringLiteral("EURGBP=X"), 0.80, 40 * kMin);
    const auto legs = combine(hub, "EUR", "GBP");
    QVERIFY(legs.quote);
    QVERIFY(std::fabs(legs.quote->rate - 1.10 / 1.32) < 1e-12);
    QVERIFY(legs.quote->triangulated);
    QVERIFY(!legs.quote->stale);
}

void TstFxRateMath::missing_legs_are_reported() {
    FakeHub hub;
    const auto c = combine(hub, "EUR", "INR");
    QVERIFY(!c.quote);
    QCOMPARE(c.missing, (QStringList{QStringLiteral("EUR"), QStringLiteral("INR")}));
    hub.put(QStringLiteral("EURUSD=X"), 1.1);
    const auto one = combine(hub, "EUR", "INR");
    QVERIFY(!one.quote);
    QCOMPARE(one.missing, QStringList{QStringLiteral("INR")});
    // Bad prices are ignored, never used.
    hub.put(QStringLiteral("USDINR=X"), 0.0);
    QVERIFY(!combine(hub, "EUR", "INR").quote);
    hub.put(QStringLiteral("USDINR=X"), std::nan(""));
    QVERIFY(!combine(hub, "EUR", "INR").quote);
}

void TstFxRateMath::invalid_codes() {
    FakeHub hub;
    QVERIFY(!combine(hub, "", "USD").quote);
    QVERIFY(!combine(hub, "EURO", "USD").quote);
    QVERIFY(combine(hub, "EURO", "USD").missing.isEmpty()); // nothing to subscribe for junk
    QVERIFY(!combine(hub, "US", "EUR").quote);
    const auto same = combine(hub, "GBp", "GBP");
    QVERIFY(same.quote);
    QCOMPARE(same.quote->rate, 0.01);
}

QTEST_GUILESS_MAIN(TstFxRateMath)
#include "tst_fx_rate_math.moc"
