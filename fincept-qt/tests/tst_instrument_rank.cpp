// Unit tests for src/core/instrument/InstrumentRank.{h,cpp} (the one ranking of
// instrument search hits) and the identity pieces it relies on: Instrument
// (Instrument.h: JSON, same_instrument) and the SymbolRef additions
// (core/symbol/SymbolRef.h: instrument_id / yahoo, same_instrument). Qt Core only.
//
// Ranking cases live in tests/data/symbol_vectors.json ("rank"), shared with the
// backend port; this file adds the order's properties (deterministic under input
// order, limit, de-duplication) and the edit distance.

#include "core/instrument/Instrument.h"
#include "core/instrument/InstrumentRank.h"
#include "core/instrument/VenueSymbols.h"
#include "core/symbol/SymbolRef.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QTimeZone>

#include <algorithm>

using namespace fincept::core::instrument;
using fincept::SymbolRef;

namespace {

QJsonObject tir_load_vectors() {
    QString path = qEnvironmentVariable("FINCEPT_SYMBOL_VECTORS");
    if (path.isEmpty())
        path = QFINDTESTDATA("data/symbol_vectors.json");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

const QJsonObject& tir_vectors() {
    static const QJsonObject v = tir_load_vectors();
    return v;
}

QVector<AssetClass> tir_classes(const QJsonValue& v) {
    QVector<AssetClass> out;
    for (const auto& x : v.toArray())
        out << asset_class_from_slug(x.toString());
    return out;
}

RankContext tir_context(const QJsonObject& v) {
    RankContext ctx;
    ctx.home_country = v.value("home").toString();
    ctx.class_priority = tir_classes(v.value("class_priority"));
    ctx.allowed_classes = tir_classes(v.value("allowed_classes"));
    ctx.include_inactive = v.value("include_inactive").toBool(true);
    return ctx;
}

QVector<Instrument> tir_instruments(const QJsonObject& v) {
    QVector<Instrument> out;
    for (const auto& x : v.value("instruments").toArray())
        out << Instrument::from_json(x.toObject());
    return out;
}

QStringList tir_symbols(const QVector<RankedInstrument>& hits) {
    QStringList out;
    for (const auto& h : hits)
        out << h.instrument.symbol;
    return out;
}

Instrument tir_full_record() {
    Instrument i;
    i.id = QStringLiteral("XNSE:TMPV");
    i.symbol = QStringLiteral("TMPV.NS");
    i.display = QStringLiteral("TMPV");
    i.name = QStringLiteral("Tata Motors Passenger Vehicles Ltd");
    i.asset_class = AssetClass::Equity;
    i.quote_type = QStringLiteral("EQUITY");
    i.venue = QStringLiteral("NSE");
    i.mic = QStringLiteral("XNSE");
    i.country = QStringLiteral("IN");
    i.currency = QStringLiteral("INR");
    i.isin = QStringLiteral("INE155A01022");
    i.figi = QStringLiteral("BBG000BXPW76");
    i.provider_symbols.insert(QStringLiteral("tv"), QStringLiteral("NSE:TMPV"));
    i.provider_symbols.insert(QStringLiteral("broker:zerodha"), QStringLiteral("NSE|TMPV|3456001"));
    i.status = InstrumentStatus::Active;
    i.aliases = {QStringLiteral("Tata Motors PV")};
    i.former_symbols = {QStringLiteral("TATAMOTORS")};
    i.primary_listing = true;
    i.rank_weight = 8;
    return i;
}

} // namespace

class TstInstrumentRank : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();

    void rank_vectors_data();
    void rank_vectors();
    void order_does_not_depend_on_input_order();
    void limit_and_dedupe();
    void match_tiers();
    void edit_distance_values();

    void instrument_json_round_trip();
    void instrument_json_tolerates_old_and_partial_records();
    void instrument_same_instrument();
    void asset_class_slugs();

    void symbolref_new_fields_are_json_optional();
    void symbolref_operator_eq_unchanged();
    void symbolref_same_instrument();
};

void TstInstrumentRank::initTestCase() {
    QVERIFY2(!tir_vectors().isEmpty(), "tests/data/symbol_vectors.json not found (set FINCEPT_SYMBOL_VECTORS)");
    QVERIFY(tir_vectors().value("rank").toArray().size() >= 20);
}

// ── Shared vectors ──────────────────────────────────────────────────────────

void TstInstrumentRank::rank_vectors_data() {
    QTest::addColumn<QJsonObject>("v");
    for (const auto& x : tir_vectors().value("rank").toArray()) {
        const QJsonObject v = x.toObject();
        QTest::newRow(qPrintable(v.value("id").toString())) << v;
    }
}

void TstInstrumentRank::rank_vectors() {
    QFETCH(QJsonObject, v);
    const QVector<RankedInstrument> hits =
        rank_instruments(tir_instruments(v), v.value("query").toString(), tir_context(v));
    QStringList expected;
    for (const auto& x : v.value("expected").toArray())
        expected << x.toString();
    QCOMPARE(tir_symbols(hits), expected);
    const QJsonObject match = v.value("match").toObject();
    for (auto it = match.constBegin(); it != match.constEnd(); ++it) {
        const auto hit = std::find_if(hits.cbegin(), hits.cend(),
                                      [&it](const RankedInstrument& h) { return h.instrument.symbol == it.key(); });
        QVERIFY2(hit != hits.cend(), qPrintable(it.key()));
        QCOMPARE(match_kind_slug(hit->match), it.value().toString());
    }
    if (v.contains("former")) {
        QStringList former;
        for (const auto& h : hits)
            if (h.via_former_symbol)
                former << h.instrument.symbol;
        QStringList want;
        for (const auto& x : v.value("former").toArray())
            want << x.toString();
        QCOMPARE(former, want);
    }
}

// ── Properties ──────────────────────────────────────────────────────────────

void TstInstrumentRank::order_does_not_depend_on_input_order() {
    for (const auto& x : tir_vectors().value("rank").toArray()) {
        const QJsonObject v = x.toObject();
        QVector<Instrument> insts = tir_instruments(v);
        const RankContext ctx = tir_context(v);
        const QStringList base = tir_symbols(rank_instruments(insts, v.value("query").toString(), ctx));
        std::reverse(insts.begin(), insts.end());
        QCOMPARE(tir_symbols(rank_instruments(insts, v.value("query").toString(), ctx)), base);
        if (insts.size() > 2) {
            std::rotate(insts.begin(), insts.begin() + 1, insts.end());
            QCOMPARE(tir_symbols(rank_instruments(insts, v.value("query").toString(), ctx)), base);
        }
    }
}

void TstInstrumentRank::limit_and_dedupe() {
    QVector<Instrument> insts;
    for (const char* s : {"RELIANCE.NS", "RELIANCE.BO", "RELIANCE.NS", "RELIANCE", "RELIANCE.L"}) {
        Instrument i = instrument_skeleton(QString::fromLatin1(s));
        i.name = QStringLiteral("Reliance Industries");
        insts << i;
    }
    RankContext ctx;
    ctx.home_country = QStringLiteral("IN");
    QCOMPARE(rank_instruments(insts, QStringLiteral("RELIANCE"), ctx).size(), 4); // one RELIANCE.NS
    ctx.limit = 2;
    QCOMPARE(tir_symbols(rank_instruments(insts, QStringLiteral("RELIANCE"), ctx)),
             (QStringList{QStringLiteral("RELIANCE.NS"), QStringLiteral("RELIANCE.BO")}));
    QVERIFY(rank_instruments(insts, QString(), ctx).isEmpty());
    QVERIFY(rank_instruments({}, QStringLiteral("RELIANCE"), ctx).isEmpty());
}

void TstInstrumentRank::match_tiers() {
    Instrument tmpv = instrument_skeleton(QStringLiteral("TMPV.NS"));
    tmpv.name = QStringLiteral("Tata Motors Passenger Vehicles Ltd");
    tmpv.former_symbols = {QStringLiteral("TATAMOTORS")};
    tmpv.aliases = {QStringLiteral("Tata Motors PV")};
    tmpv.isin = QStringLiteral("INE155A01022");
    auto m = [&tmpv](const char* text) { return match_instrument(tmpv, parse_symbol_input(QString::fromUtf8(text))); };
    QCOMPARE(m("TMPV.NS").match, MatchKind::ExactSymbol);
    QCOMPARE(m("TMPV").match, MatchKind::ExactListingCode);
    QCOMPARE(m("NSE:TMPV").match, MatchKind::ExactSymbol);
    const RankedInstrument former = m("TATAMOTORS");
    QCOMPARE(former.match, MatchKind::ExactAlias);
    QVERIFY(former.via_former_symbol);
    QCOMPARE(former.matched_text, QStringLiteral("TATAMOTORS"));
    QCOMPARE(m("tata motors pv").match, MatchKind::ExactAlias);
    QCOMPARE(m("INE155A01022").match, MatchKind::ExactIsin);
    QCOMPARE(m("US0378331005").match, MatchKind::None); // an ISIN never matches by name / prefix
    QCOMPARE(m("TMP").match, MatchKind::Prefix);
    QVERIFY(m("TMP").code_prefix);
    QCOMPARE(m("tata mot").match, MatchKind::Prefix);
    QVERIFY(!m("tata mot").code_prefix);
    QCOMPARE(m("passenger").match, MatchKind::NameContains);
    const RankedInstrument fuzzy = m("TATAMOTRS");
    QCOMPARE(fuzzy.match, MatchKind::Fuzzy);
    QCOMPARE(fuzzy.distance, 1);
    QCOMPARE(m("ZZZZZZZZ").match, MatchKind::None);
    QCOMPARE(m("").match, MatchKind::None);
    QCOMPARE(m("AAPL@#").match, MatchKind::None);
}

void TstInstrumentRank::edit_distance_values() {
    QCOMPARE(edit_distance(u"RELAINCE", u"RELIANCE", 2), 1); // adjacent swap
    QCOMPARE(edit_distance(u"MICROSFT", u"MICROSOFT", 2), 1);
    QCOMPARE(edit_distance(u"ABC", u"ABC", 0), 0);
    QCOMPARE(edit_distance(u"", u"", 0), 0);
    QCOMPARE(edit_distance(u"", u"AB", 2), 2);
    QCOMPARE(edit_distance(u"KITTEN", u"SITTING", 5), 3);
    QCOMPARE(edit_distance(u"KITTEN", u"SITTING", 2), 3); // capped at max + 1
    QCOMPARE(edit_distance(u"AB", u"ABCDEF", 2), 3);       // length gap alone exceeds max
    QCOMPARE(edit_distance(u"CA", u"ABC", 3), 3);          // OSA, not full Damerau
}

// ── Instrument record ───────────────────────────────────────────────────────

void TstInstrumentRank::instrument_json_round_trip() {
    const Instrument full = tir_full_record();
    const QJsonObject o = full.to_json();
    QCOMPARE(o.value("asset_class").toString(), QStringLiteral("equity"));
    QVERIFY(!o.contains("status")); // Active is the default and is not written
    QCOMPARE(Instrument::from_json(o), full);
    Instrument renamed = full;
    renamed.status = InstrumentStatus::Renamed;
    renamed.successor_id = QStringLiteral("XNSE:TMCV");
    QCOMPARE(renamed.to_json().value("status").toString(), QStringLiteral("renamed"));
    QCOMPARE(Instrument::from_json(renamed.to_json()), renamed);
    // Through text, too (what the local index and the backend store).
    const QByteArray text = QJsonDocument(full.to_json()).toJson(QJsonDocument::Compact);
    QCOMPARE(Instrument::from_json(QJsonDocument::fromJson(text).object()), full);
    QVERIFY(Instrument().to_json().isEmpty());
}

void TstInstrumentRank::instrument_json_tolerates_old_and_partial_records() {
    const QJsonObject partial{{"symbol", "AAPL"}, {"asset_class", "STOCK"}, {"unknown_key", 42},
                              {"aliases", QJsonArray{"Apple", "", 7}}, {"status", "weird"}};
    const Instrument i = Instrument::from_json(partial);
    QCOMPARE(i.symbol, QStringLiteral("AAPL"));
    QCOMPARE(i.asset_class, AssetClass::Equity);
    QCOMPARE(i.aliases, QStringList{QStringLiteral("Apple")});
    QCOMPARE(i.status, InstrumentStatus::Active);
    QVERIFY(i.is_valid());
    QCOMPARE(i.provider_symbol(QStringLiteral("yahoo")), QStringLiteral("AAPL"));
    QVERIFY(!Instrument::from_json(QJsonObject()).is_valid());
}

void TstInstrumentRank::instrument_same_instrument() {
    Instrument a = instrument_skeleton(QStringLiteral("RELIANCE.NS"));
    Instrument b = a;
    b.name = QStringLiteral("other name");
    QVERIFY(a.same_instrument(b));
    QVERIFY(!(a == b));
    Instrument bo = instrument_skeleton(QStringLiteral("RELIANCE.BO"));
    bo.isin = a.isin = QStringLiteral("INE002A01018");
    QVERIFY(!a.same_instrument(bo)); // same ISIN, different listing
    Instrument no_id;
    no_id.symbol = QStringLiteral("RELIANCE.NS");
    QVERIFY(a.same_instrument(no_id)); // falls back to the canonical symbol
    Instrument other_id = a;
    other_id.id = QStringLiteral("XNSE:SOMETHINGELSE");
    QVERIFY(!a.same_instrument(other_id)); // ids decide when both have one
    QVERIFY(!Instrument().same_instrument(Instrument()));
    QCOMPARE(a.id, QStringLiteral("XNSE:RELIANCE"));
    QCOMPARE(instrument_skeleton(QStringLiteral("AAPL")).id, QStringLiteral("US:AAPL"));
    QCOMPARE(instrument_skeleton(QStringLiteral("INR=X")).id, QStringLiteral("FX:USDINR"));
}

void TstInstrumentRank::asset_class_slugs() {
    for (AssetClass c : {AssetClass::Equity, AssetClass::Etf, AssetClass::Fund, AssetClass::Index, AssetClass::Fx,
                         AssetClass::Crypto, AssetClass::Future, AssetClass::Option, AssetClass::Bond,
                         AssetClass::Commodity})
        QCOMPARE(asset_class_from_slug(asset_class_slug(c)), c);
    QCOMPARE(asset_class_slug(AssetClass::Unknown), QString());
    QCOMPARE(asset_class_from_slug(QStringLiteral(" CryptoCurrency ")), AssetClass::Crypto);
    QCOMPARE(asset_class_from_slug(QStringLiteral("MUTUALFUND")), AssetClass::Fund);
    QCOMPARE(asset_class_from_slug(QStringLiteral("cfd")), AssetClass::Unknown);
    QVERIFY(is_equity_like(AssetClass::Etf));
    QVERIFY(!is_equity_like(AssetClass::Index));
    QCOMPARE(instrument_status_from_slug(QStringLiteral("DELISTED")), InstrumentStatus::Delisted);
}

// ── SymbolRef additions ─────────────────────────────────────────────────────

void TstInstrumentRank::symbolref_new_fields_are_json_optional() {
    // A file written before the fields existed loads unchanged.
    const QJsonObject old{{"symbol", "RELIANCE"}, {"asset_class", "equity"}, {"exchange", "NSE"}};
    const SymbolRef r = SymbolRef::from_json(old);
    QVERIFY(r.instrument_id.isEmpty());
    QVERIFY(r.yahoo.isEmpty());
    QCOMPARE(r.to_json(), old); // and is written back byte-for-byte the same
    SymbolRef n = r;
    n.instrument_id = QStringLiteral("XNSE:RELIANCE");
    n.yahoo = QStringLiteral("RELIANCE.NS");
    const QJsonObject o = n.to_json();
    QCOMPARE(o.value("instrument_id").toString(), QStringLiteral("XNSE:RELIANCE"));
    QCOMPARE(o.value("yahoo").toString(), QStringLiteral("RELIANCE.NS"));
    const SymbolRef back = SymbolRef::from_json(o);
    QCOMPARE(back.instrument_id, n.instrument_id);
    QCOMPARE(back.yahoo, n.yahoo);
    QCOMPARE(SymbolRef::equity(QStringLiteral("aapl")).symbol, QStringLiteral("AAPL"));
}

void TstInstrumentRank::symbolref_operator_eq_unchanged() {
    SymbolRef a = SymbolRef::equity(QStringLiteral("AAPL"));
    SymbolRef b = a;
    b.instrument_id = QStringLiteral("US:AAPL"); // not part of operator== (behaviour kept)
    b.yahoo = QStringLiteral("AAPL");
    QVERIFY(a == b);
    b.selection = QStringLiteral("row-3");
    QVERIFY(a != b);
}

void TstInstrumentRank::symbolref_same_instrument() {
    auto ref = [](const char* sym, const char* cls, const char* ex) {
        SymbolRef r;
        r.symbol = QString::fromLatin1(sym);
        r.asset_class = QString::fromLatin1(cls);
        r.exchange = QString::fromLatin1(ex);
        return r;
    };
    // Same AAPL pinned from two screens (audit C26): different class slugs / time range / selection.
    SymbolRef a = ref("AAPL", "equity", "");
    SymbolRef b = ref("aapl", "", "NASDAQ");
    b.time_range_start = QDateTime(QDate(2026, 1, 1), QTime(0, 0), QTimeZone::utc());
    b.selection = QStringLiteral("x");
    QVERIFY(a.same_instrument(b));
    QVERIFY(ref("SPY", "etf", "").same_instrument(ref("SPY", "equity", "ARCA")));
    QVERIFY(!ref("AAPL", "equity", "").same_instrument(ref("AAPL", "option", "")));
    QVERIFY(!ref("RELIANCE", "equity", "NSE").same_instrument(ref("RELIANCE", "equity", "BSE")));
    // Different spellings meet through `yahoo` or the instrument id.
    SymbolRef broker = ref("RELIANCE", "equity", "NSE");
    broker.yahoo = QStringLiteral("RELIANCE.NS");
    QVERIFY(broker.same_instrument(ref("RELIANCE.NS", "equity", "")));
    QVERIFY(!broker.same_instrument(ref("RELIANCE.BO", "equity", "")));
    SymbolRef x = ref("BTC/USDT", "crypto", "binance");
    SymbolRef y = ref("BTC-USD", "crypto", "");
    x.instrument_id = y.instrument_id = QStringLiteral("CRYPTO:BTC-USD");
    QVERIFY(x.same_instrument(y));
    y.instrument_id = QStringLiteral("CRYPTO:ETH-USD");
    QVERIFY(!x.same_instrument(y));
    QVERIFY(!SymbolRef().same_instrument(SymbolRef()));
}

QTEST_GUILESS_MAIN(TstInstrumentRank)
#include "tst_instrument_rank.moc"
