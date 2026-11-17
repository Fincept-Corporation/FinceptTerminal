// Unit tests for src/core/instrument/SymbolInput.{h,cpp} — the one normaliser for
// symbol text (parse_symbol_input, canonical_symbol, list splitting, clean-up, name
// normalisation, ISIN check). Qt Core only.
//
// The cases live in tests/data/symbol_vectors.json, shared with the backend's Python
// port of the normaliser: change an expectation there, never here. This file adds the
// properties the vectors cannot state row by row (idempotence, no ISIN ever comes out
// as a symbol, thread safety, determinism).

#include "core/instrument/SymbolInput.h"
#include "core/instrument/VenueSymbols.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QTest>

#include <atomic>
#include <thread>
#include <vector>

using namespace fincept::core::instrument;

namespace {

QJsonObject tsi_load_vectors() {
    QString path = qEnvironmentVariable("FINCEPT_SYMBOL_VECTORS");
    if (path.isEmpty())
        path = QFINDTESTDATA("data/symbol_vectors.json");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

const QJsonObject& tsi_vectors() {
    static const QJsonObject v = tsi_load_vectors();
    return v;
}

QStringList tsi_strings(const QJsonValue& v) {
    QStringList out;
    for (const auto& x : v.toArray())
        out << x.toString();
    return out;
}

QString tsi_row_name(const QJsonObject& v, const char* text_key) {
    QString n = v.value("id").toString();
    if (!n.isEmpty())
        n += QLatin1Char(' ');
    n += v.value(QLatin1String(text_key)).toString();
    if (v.contains("venue_hint"))
        n += QStringLiteral(" @") + v.value("venue_hint").toString();
    if (v.contains("home"))
        n += QStringLiteral(" home=") + v.value("home").toString();
    return n;
}

} // namespace

class TstSymbolInput : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();

    void parse_vectors_data();
    void parse_vectors();
    void canonical_symbol_vectors_data();
    void canonical_symbol_vectors();

    void canonical_symbol_is_idempotent();
    void candidates_are_clean();
    void isin_is_never_a_symbol();
    void unknown_venue_never_guesses();
    void home_country_only_reorders();

    void clean_symbol_text_rules();
    void split_symbol_list_rules();
    void normalize_name_rules();
    void isin_check_digits();

    void deterministic_and_thread_safe();
};

void TstSymbolInput::initTestCase() {
    QVERIFY2(!tsi_vectors().isEmpty(), "tests/data/symbol_vectors.json not found (set FINCEPT_SYMBOL_VECTORS)");
    QCOMPARE(tsi_vectors().value("meta").toObject().value("schema").toInt(), 1);
    QVERIFY(tsi_vectors().value("parse").toArray().size() >= 150);
}

// ── Shared vectors ──────────────────────────────────────────────────────────

void TstSymbolInput::parse_vectors_data() {
    QTest::addColumn<QJsonObject>("v");
    for (const auto& x : tsi_vectors().value("parse").toArray()) {
        const QJsonObject v = x.toObject();
        QTest::newRow(qPrintable(tsi_row_name(v, "input"))) << v;
    }
}

void TstSymbolInput::parse_vectors() {
    QFETCH(QJsonObject, v);
    const QString input = v.value("input").toString();
    const QString hint = v.value("venue_hint").toString();
    const ParsedInput p = parse_symbol_input(input, hint, v.value("home").toString());
    if (v.contains("kind"))
        QCOMPARE(input_kind_slug(p.kind), v.value("kind").toString());
    if (v.contains("asset_class"))
        QCOMPARE(asset_class_slug(p.asset_class), v.value("asset_class").toString());
    if (v.contains("venue"))
        QCOMPARE(p.venue, v.value("venue").toString());
    if (v.contains("candidates"))
        QCOMPARE(p.candidates, tsi_strings(v.value("candidates")));
    if (v.contains("canonical"))
        QCOMPARE(p.canonical, v.value("canonical").toString());
    for (const QString& note : tsi_strings(v.value("notes_include")))
        QVERIFY2(p.notes.contains(note), qPrintable(note + QStringLiteral(" not in ") + p.notes.join(',')));
    if (v.contains("canonical_symbol"))
        QCOMPARE(canonical_symbol(input, hint), v.value("canonical_symbol").toString());
}

void TstSymbolInput::canonical_symbol_vectors_data() {
    QTest::addColumn<QJsonObject>("v");
    for (const auto& x : tsi_vectors().value("canonical_symbol").toArray()) {
        const QJsonObject v = x.toObject();
        QTest::newRow(qPrintable(tsi_row_name(v, "input"))) << v;
    }
}

void TstSymbolInput::canonical_symbol_vectors() {
    QFETCH(QJsonObject, v);
    const QString hint = v.value("venue_hint").toString();
    const QString got = canonical_symbol(v.value("input").toString(), hint);
    QCOMPARE(got, v.value("expected").toString());
    QCOMPARE(canonical_symbol(got, hint), got);
}

// ── Properties over every vector input ──────────────────────────────────────

void TstSymbolInput::canonical_symbol_is_idempotent() {
    for (const auto& x : tsi_vectors().value("parse").toArray()) {
        const QJsonObject v = x.toObject();
        const QString hint = v.value("venue_hint").toString();
        const QString once = canonical_symbol(v.value("input").toString(), hint);
        QVERIFY2(canonical_symbol(once, hint) == once, qPrintable(v.value("input").toString() + " -> " + once));
        // A canonical symbol needs no hint any more.
        QVERIFY2(canonical_symbol(once) == once, qPrintable(v.value("input").toString() + " -> " + once));
        // Every candidate is already canonical.
        for (const QString& c : parse_symbol_input(v.value("input").toString(), hint).candidates)
            QVERIFY2(canonical_symbol(c) == c, qPrintable(c));
    }
}

void TstSymbolInput::candidates_are_clean() {
    for (const auto& x : tsi_vectors().value("parse").toArray()) {
        const QJsonObject v = x.toObject();
        const ParsedInput p =
            parse_symbol_input(v.value("input").toString(), v.value("venue_hint").toString(), v.value("home").toString());
        QStringList seen;
        for (const QString& c : p.candidates) {
            QVERIFY2(!c.isEmpty(), qPrintable(p.raw));
            QVERIFY2(!seen.contains(c), qPrintable(QStringLiteral("duplicate ") + c));
            QVERIFY2(!c.contains(QLatin1Char(' ')), qPrintable(c));
            QCOMPARE(c, c.toUpper());
            seen << c;
        }
        if (!p.canonical.isEmpty())
            QVERIFY2(p.candidates.contains(p.canonical), qPrintable(p.raw));
        if (p.kind == InputKind::Empty || p.kind == InputKind::Invalid || p.kind == InputKind::Name ||
            p.kind == InputKind::Isin)
            QVERIFY2(p.candidates.isEmpty(), qPrintable(p.raw));
        QCOMPARE(p.asset_class, p.preferred_classes.value(0, AssetClass::Unknown));
    }
}

void TstSymbolInput::isin_is_never_a_symbol() {
    for (const char* isin : {"US0378331005", "INE002A01018", "DE0007164600", "GB0007980591", "us0378331005"}) {
        const ParsedInput p = parse_symbol_input(QString::fromLatin1(isin));
        QCOMPARE(p.kind, InputKind::Isin);
        QVERIFY(p.candidates.isEmpty());
        QVERIFY(p.canonical.isEmpty());
        QCOMPARE(p.isin, QString::fromLatin1(isin).toUpper());
        QVERIFY(canonical_symbol(QString::fromLatin1(isin)).isEmpty());
        QVERIFY(to_yahoo(QString::fromLatin1(isin), QStringLiteral("NASDAQ")).isEmpty());
        QVERIFY(!from_yahoo(QString::fromLatin1(isin)).valid());
    }
}

void TstSymbolInput::unknown_venue_never_guesses() {
    // A bare ticker on an exchange we do not model would quote the US company of that name.
    for (const char* text : {"AAPL", "RELIANCE", "BTC", "NIFTY", "gold", "7203", "EURUSD"}) {
        const ParsedInput p = parse_symbol_input(QString::fromLatin1(text), QStringLiteral("NOSUCHVENUE"));
        QVERIFY2(p.candidates.isEmpty(), text);
        QVERIFY(p.has_note("unknown_venue"));
        QVERIFY(canonical_symbol(QString::fromLatin1(text), QStringLiteral("NOSUCHVENUE")).isEmpty());
    }
    QVERIFY(parse_symbol_input(QStringLiteral("XX9:AAPL")).candidates.isEmpty());
}

void TstSymbolInput::home_country_only_reorders() {
    for (const char* text : {"AAPL", "RELIANCE", "SAP", "7203", "BRK.B", "005930"}) {
        for (const char* home : {"US", "IN", "DE", "JP", "GB", "KR", "SE"}) {
            const ParsedInput a = parse_symbol_input(QString::fromLatin1(text));
            const ParsedInput b = parse_symbol_input(QString::fromLatin1(text), {}, QString::fromLatin1(home));
            QCOMPARE(b.kind, a.kind);
            for (const QString& c : a.candidates)
                QVERIFY2(b.candidates.contains(c), qPrintable(QString::fromLatin1(text) + " lost " + c));
        }
    }
}

// ── Clean-up / lists / names / ISIN ─────────────────────────────────────────

void TstSymbolInput::clean_symbol_text_rules() {
    QCOMPARE(clean_symbol_text(QStringLiteral("  aapl  ")), QStringLiteral("aapl"));
    QCOMPARE(clean_symbol_text(QStringLiteral("\tAAPL\n")), QStringLiteral("AAPL"));
    QCOMPARE(clean_symbol_text(QStringLiteral(" AAPL ")), QStringLiteral("AAPL"));
    QCOMPARE(clean_symbol_text(QStringLiteral("AA​PL")), QStringLiteral("AAPL"));
    QCOMPARE(clean_symbol_text(QStringLiteral("﻿AAPL")), QStringLiteral("AAPL"));
    QCOMPARE(clean_symbol_text(QStringLiteral("ＡＡＰＬ")), QStringLiteral("AAPL"));
    QCOMPARE(clean_symbol_text(QStringLiteral("$AAPL")), QStringLiteral("AAPL"));
    QCOMPARE(clean_symbol_text(QStringLiteral("'AAPL")), QStringLiteral("AAPL"));
    QCOMPARE(clean_symbol_text(QStringLiteral("\"'$AAPL'\"")), QStringLiteral("AAPL"));
    QCOMPARE(clean_symbol_text(QStringLiteral("“tata   motors”")), QStringLiteral("tata motors"));
    QCOMPARE(clean_symbol_text(QStringLiteral("Nestlé")), QStringLiteral("Nestlé"));
    QCOMPARE(clean_symbol_text(QStringLiteral("Nestlé")), QStringLiteral("Nestlé")); // NFC composed
    QCOMPARE(clean_symbol_text(QString()), QString());
}

void TstSymbolInput::split_symbol_list_rules() {
    QCOMPARE(split_symbol_list(QStringLiteral("AAPL, MSFT;tata motors\nBTC/USDT\t^NSEI\r\n")),
             (QStringList{"AAPL", "MSFT", "tata motors", "BTC/USDT", "^NSEI"}));
    QCOMPARE(split_symbol_list(QStringLiteral(" , ;\n")), QStringList());
    QCOMPARE(split_symbol_list(QStringLiteral("NIFTY 50")), QStringList{QStringLiteral("NIFTY 50")});
    QCOMPARE(split_symbol_list(QStringLiteral("S&P 500; Dow Jones")),
             (QStringList{QStringLiteral("S&P 500"), QStringLiteral("Dow Jones")}));
}

void TstSymbolInput::normalize_name_rules() {
    QCOMPARE(normalize_name(QStringLiteral("Nestlé S.A.")), QStringLiteral("nestle s a"));
    QCOMPARE(normalize_name(QStringLiteral("  APPLE   Inc. ")), QStringLiteral("apple inc"));
    QCOMPARE(normalize_name(QStringLiteral("AT&T Inc.")), QStringLiteral("at&t inc"));
    QCOMPARE(normalize_name(QStringLiteral("Société Générale")), QStringLiteral("societe generale"));
    QCOMPARE(normalize_name(QStringLiteral("腾讯控股")), QStringLiteral("腾讯控股"));
    QCOMPARE(normalize_name(QStringLiteral("Ｔｅｓｌａ")), QStringLiteral("tesla"));
    QCOMPARE(normalize_name(QStringLiteral("Mahindra & Mahindra")), QStringLiteral("mahindra & mahindra"));
}

void TstSymbolInput::isin_check_digits() {
    for (const char* good : {"US0378331005", "INE002A01018", "DE0007164600", "US5949181045", "GB0007980591"})
        QVERIFY2(is_valid_isin(QString::fromLatin1(good)), good);
    for (const char* bad : {"US0378331006", "INE002A01019", "US037833100", "US03783310055", "0S0378331005",
                            "ABCDEFGHIJKL", "", "us0378331005"})
        QVERIFY2(!is_valid_isin(QString::fromLatin1(bad)), bad);
}

// ── Determinism / threads ───────────────────────────────────────────────────

void TstSymbolInput::deterministic_and_thread_safe() {
    QStringList inputs, hints, expected;
    for (const auto& x : tsi_vectors().value("parse").toArray()) {
        const QJsonObject v = x.toObject();
        inputs << v.value("input").toString();
        hints << v.value("venue_hint").toString();
    }
    auto render = [&inputs, &hints](int i) {
        const ParsedInput p = parse_symbol_input(inputs.at(i), hints.at(i));
        return input_kind_slug(p.kind) + QLatin1Char('|') + p.candidates.join(',') + QLatin1Char('|') +
               canonical_symbol(inputs.at(i), hints.at(i)) + QLatin1Char('|') +
               to_yahoo(p.base, hints.at(i).isEmpty() ? QStringLiteral("NSE") : hints.at(i));
    };
    for (int i = 0; i < inputs.size(); ++i)
        expected << render(i);
    std::atomic<int> failures{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&, t]() {
            for (int round = 0; round < 20; ++round)
                for (int i = 0; i < inputs.size(); ++i)
                    if (render((i + t * 7) % inputs.size()) != expected.at((i + t * 7) % inputs.size()))
                        failures.fetch_add(1);
        });
    }
    for (auto& th : threads)
        th.join();
    QCOMPARE(failures.load(), 0);
}

QTEST_GUILESS_MAIN(TstSymbolInput)
#include "tst_symbol_input.moc"
