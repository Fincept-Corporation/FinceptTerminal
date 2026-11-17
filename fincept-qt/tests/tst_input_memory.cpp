// Unit tests for the pure rules of src/ui/widgets/InputMemory.h
// (fincept::ui::input_memory_detail — header-inline, QtCore only) and the input
// signature it shares with ui::AutoLoadPane (ui::autoload::signature_of).
//
// The widget half of InputMemory (reading / applying combo, list, spin, text
// values; tracking; debounced writes through SettingsRepository) needs QtWidgets
// and the settings database and is verified in the running app. What is pinned
// here is everything a stored document depends on:
//   * the storage key and the stored JSON document round-trip,
//   * unreadable / newer-version documents are rejected (defaults, never a crash),
//   * the input signature is stable and key-order independent,
//   * the layout-signature guard for positional inputs,
//   * what a save writes (stored ∪ current ∪ pending, stale positional values dropped).

#include "ui/widgets/InputMemory.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSize>
#include <QTest>
#include <QTimeZone>
#include <QVariantList>
#include <QVariantMap>
#include <QtNumeric>

namespace imd = fincept::ui::input_memory_detail;

class TstInputMemory : public QObject {
    Q_OBJECT

  private slots:
    void storage_key_format();
    void positional_keys();
    void json_round_trip();
    void decode_rejects_bad_documents();
    void signature_is_key_order_independent();
    void signature_follows_values();
    void layout_guard();
    void merge_for_save_keeps_untracked_and_pending();
    void merge_for_save_drops_stale_positional_values();
    void symbol_value_round_trip();
    // Non-widget values (InputMemory::set_value / value) — since 2026-11-17 round 4.
    void value_normalisation();
    void value_survives_the_document();
    // Custom inputs (InputMemory::track with a getter / setter) — round 6.
    void custom_value_round_trip();
    void custom_value_signature_is_restart_stable();
};

// A custom input's QJsonValue is kept exactly as storage gives it back, and its setter
// receives the same JSON shape it produced.
void TstInputMemory::custom_value_round_trip() {
    bool ok = false;
    const QJsonArray chips{QStringLiteral("AAPL"), QStringLiteral("MSFT")};
    const QVariant kept = imd::custom_to_stored(QJsonValue(chips), &ok);
    QVERIFY(ok);
    QCOMPARE(imd::custom_from_stored(kept), QJsonValue(chips));
    const QJsonObject rule{{QStringLiteral("op"), QStringLiteral("and")},
                           {QStringLiteral("rules"), QJsonArray{QJsonObject{{QStringLiteral("field"), QStringLiteral("rsi")},
                                                                            {QStringLiteral("lt"), 30}}}}};
    const QVariant rule_kept = imd::custom_to_stored(QJsonValue(rule), &ok);
    QVERIFY(ok);
    QCOMPARE(imd::custom_from_stored(rule_kept), QJsonValue(rule));
    // Through the stored document and back.
    const imd::Decoded back = imd::decode(imd::encode({{QStringLiteral("rules"), rule_kept}}, QString()));
    QVERIFY(back.ok);
    QCOMPARE(imd::custom_from_stored(back.inputs.value(QStringLiteral("rules"))), QJsonValue(rule));
    // Scalars and an explicit null are values; Undefined is "nothing to remember".
    QCOMPARE(imd::custom_from_stored(imd::custom_to_stored(QJsonValue(QStringLiteral("x")), &ok)), QJsonValue(QStringLiteral("x")));
    QVERIFY(ok);
    imd::custom_to_stored(QJsonValue(QJsonValue::Null), &ok);
    QVERIFY(ok);
    QVERIFY(!imd::custom_to_stored(QJsonValue(QJsonValue::Undefined), &ok).isValid());
    QVERIFY(!ok);
}

// What a custom input reports now and what a restart reads give the same signature
// (a whole double and an integer, nested objects, key order).
void TstInputMemory::custom_value_signature_is_restart_stable() {
    bool ok = false;
    const QJsonObject live{{QStringLiteral("qty"), 2.0}, {QStringLiteral("legs"), QJsonArray{1.5, 3.0}}};
    const QVariant now = imd::custom_to_stored(QJsonValue(live), &ok);
    QVERIFY(ok);
    const QVariantMap session{{QStringLiteral("builder"), now}, {QStringLiteral("country"), QStringLiteral("US")}};
    const imd::Decoded back = imd::decode(imd::encode(session, QString()));
    QVERIFY(back.ok);
    QCOMPARE(imd::signature(back.inputs), imd::signature(session));
    // A real edit changes it.
    QJsonObject edited = live;
    edited.insert(QStringLiteral("qty"), 3);
    QVariantMap changed = session;
    changed.insert(QStringLiteral("builder"), imd::custom_to_stored(QJsonValue(edited), &ok));
    QVERIFY(imd::signature(changed) != imd::signature(session));
}

// set_value() keeps what storage gives back, so a value reads the same before and
// after a restart; what JSON cannot hold is refused instead of stored as null.
void TstInputMemory::value_normalisation() {
    bool ok = false;
    QCOMPARE(imd::normalise_value(QVariant(QStringLiteral("SPX")), &ok), QVariant(QStringLiteral("SPX")));
    QVERIFY(ok);
    const QVariant whole = imd::normalise_value(QVariant(42), &ok);
    QVERIFY(ok);
    QCOMPARE(whole.toLongLong(), 42LL);
    const QVariant real = imd::normalise_value(QVariant(2.5), &ok);
    QVERIFY(ok);
    QCOMPARE(real.toDouble(), 2.5);
    QCOMPARE(imd::normalise_value(QVariant(true), &ok), QVariant(true));
    QVERIFY(ok);
    // A whole double comes back as a number equal to it (JSON text has no "2.0").
    QCOMPARE(imd::normalise_value(QVariant(2.0), &ok).toDouble(), 2.0);
    // Dates become ISO text — what a restart would read.
    const QDateTime when(QDate(2026, 11, 17), QTime(9, 15), QTimeZone::UTC);
    const QVariant date = imd::normalise_value(QVariant(when), &ok);
    QVERIFY(ok);
    QCOMPARE(date.typeId(), int(QMetaType::QString));
    QCOMPARE(QDateTime::fromString(date.toString(), Qt::ISODate), when);
    // Lists and maps element by element.
    const QVariantMap range{{QStringLiteral("from"), 1}, {QStringLiteral("to"), 5},
                            {QStringLiteral("ids"), QVariantList{QStringLiteral("a"), QStringLiteral("b")}}};
    const QVariant m = imd::normalise_value(QVariant(range), &ok);
    QVERIFY(ok);
    QCOMPARE(m.toMap().value(QStringLiteral("to")).toInt(), 5);
    QCOMPARE(m.toMap().value(QStringLiteral("ids")).toStringList(), QStringList({QStringLiteral("a"), QStringLiteral("b")}));
    // Idempotent: a normalised value normalises to itself.
    QCOMPARE(imd::normalise_value(m, &ok), m);
    // An explicit null is a value; invalid / NaN / unsupported are not.
    imd::normalise_value(QVariant(QMetaType::fromType<std::nullptr_t>()), &ok);
    QVERIFY(ok);
    QVERIFY(!imd::normalise_value(QVariant(), &ok).isValid());
    QVERIFY(!ok);
    imd::normalise_value(QVariant(qQNaN()), &ok);
    QVERIFY(!ok);
    imd::normalise_value(QVariant::fromValue(QSize(3, 4)), &ok);
    QVERIFY(!ok);
}

// Values share the document (and its version) with the tracked inputs.
void TstInputMemory::value_survives_the_document() {
    bool ok = false;
    const QVariant series = imd::normalise_value(QVariant(QStringLiteral("CPIAUCSL")), &ok);
    const QVariant span = imd::normalise_value(QVariant(QVariantMap{{QStringLiteral("years"), 5}}), &ok);
    const QVariantMap inputs{{QStringLiteral("country"), QStringLiteral("US")},   // a tracked combo
                             {QStringLiteral("series"), series},                   // set_value
                             {QStringLiteral("span"), span}};                      // set_value
    const QString text = imd::encode(inputs, QString());
    QVERIFY(text.contains(QStringLiteral("\"v\":1")));
    const imd::Decoded back = imd::decode(text);
    QVERIFY(back.ok);
    QCOMPARE(back.inputs.value(QStringLiteral("series")), series);
    QCOMPARE(back.inputs.value(QStringLiteral("span")), span);
    // The signature sees them like any input.
    QVariantMap other = inputs;
    other.insert(QStringLiteral("series"), QStringLiteral("GDP"));
    QVERIFY(imd::signature(inputs) != imd::signature(other));
}

// A SymbolSearchBox tracked by name: its pick goes through the stored document and
// comes back with the instrument JSON intact (restored without a network call).
void TstInputMemory::symbol_value_round_trip() {
    imd::SymbolValue pick;
    pick.symbol = QStringLiteral("RELIANCE.NS");
    pick.text = QStringLiteral("RELIANCE.NS");
    pick.instrument = QJsonObject{{QStringLiteral("id"), QStringLiteral("eq:XNSE:RELIANCE")},
                                  {QStringLiteral("symbol"), QStringLiteral("RELIANCE.NS")},
                                  {QStringLiteral("display"), QStringLiteral("RELIANCE")},
                                  {QStringLiteral("currency"), QStringLiteral("INR")}};
    QVERIFY(pick.is_pick());
    QVariantMap inputs{{QStringLiteral("underlying"), imd::encode_symbol(pick)},
                       {QStringLiteral("spot"), 2950.5}};
    const imd::Decoded doc = imd::decode(imd::encode(inputs, QString()));
    QVERIFY(doc.ok);
    const imd::SymbolValue back = imd::decode_symbol(doc.inputs.value(QStringLiteral("underlying")));
    QCOMPARE(back.symbol, pick.symbol);
    QCOMPARE(back.text, pick.text);
    QCOMPARE(back.instrument, pick.instrument);
    QVERIFY(back.is_pick());
    // The signature follows the pick (another instrument = other inputs).
    imd::SymbolValue other = pick;
    other.symbol = QStringLiteral("TCS.NS");
    other.instrument.insert(QStringLiteral("symbol"), QStringLiteral("TCS.NS"));
    QVariantMap inputs2 = inputs;
    inputs2.insert(QStringLiteral("underlying"), imd::encode_symbol(other));
    QVERIFY(imd::signature(inputs) != imd::signature(inputs2));
    QCOMPARE(imd::signature(inputs), imd::signature(doc.inputs));
    // Text typed without a pick: only the text.
    imd::SymbolValue typed;
    typed.text = QStringLiteral("relia");
    const imd::SymbolValue typed_back = imd::decode_symbol(imd::encode_symbol(typed));
    QVERIFY(!typed_back.is_pick());
    QCOMPARE(typed_back.text, QStringLiteral("relia"));
    QVERIFY(!imd::encode_symbol(typed).toMap().contains(QStringLiteral("sym")));
    // A bare string (a line edit that held the symbol before) reads as that symbol.
    const imd::SymbolValue legacy = imd::decode_symbol(QVariant(QStringLiteral(" AAPL ")));
    QCOMPARE(legacy.symbol, QStringLiteral("AAPL"));
    QVERIFY(legacy.instrument.isEmpty());
    // Nothing usable: not a pick.
    QVERIFY(!imd::decode_symbol(QVariant()).is_pick());
    QVERIFY(!imd::decode_symbol(QVariant(42)).is_pick());
}

void TstInputMemory::storage_key_format() {
    QCOMPARE(imd::storage_key(QStringLiteral("derivatives"), QStringLiteral("bond")),
             QStringLiteral("inputs/derivatives/bond"));
    QCOMPARE(imd::storage_key(QStringLiteral("economics"), QString()), QStringLiteral("inputs/economics"));
    QCOMPARE(imd::settings_category(), QStringLiteral("inputs"));
}

void TstInputMemory::positional_keys() {
    QCOMPARE(imd::positional_key(QStringLiteral("QComboBox"), 2), QStringLiteral("@QComboBox#2"));
    QVERIFY(imd::is_positional_key(imd::positional_key(QStringLiteral("QSpinBox"), 0)));
    QVERIFY(!imd::is_positional_key(QStringLiteral("coupon")));
    QVERIFY(!imd::is_positional_key(QStringLiteral("QComboBox#0"))); // a name, not a position
}

void TstInputMemory::json_round_trip() {
    QVariantMap in;
    in.insert(QStringLiteral("coupon"), 5.25);
    in.insert(QStringLiteral("frequency"),
              QVariantMap{{QStringLiteral("d"), 2}, {QStringLiteral("t"), QStringLiteral("Semi-Annual")}});
    in.insert(QStringLiteral("active"), true);
    in.insert(QStringLiteral("name"), QStringLiteral("Bond A — 5% 2029"));
    in.insert(QStringLiteral("since"), QStringLiteral("2026-01-02T00:00:00"));
    in.insert(QStringLiteral("@QSpinBox#0"), 7);
    in.insert(QStringLiteral("countries"),
              QVariantMap{{QStringLiteral("sel"),
                           QVariantList{QVariantMap{{QStringLiteral("d"), QStringLiteral("US")},
                                                    {QStringLiteral("t"), QStringLiteral("United States")}}}}});
    const QString sig = imd::layout_signature({QStringLiteral("QSpinBox")});

    const QString text = imd::encode(in, sig);
    QVERIFY(!text.contains(QLatin1Char('\n'))); // compact: one settings value
    const imd::Decoded out = imd::decode(text);
    QVERIFY(out.ok);
    QCOMPARE(out.layout_sig, sig);
    QCOMPARE(out.inputs.size(), in.size());
    QCOMPARE(out.inputs.value(QStringLiteral("coupon")).toDouble(), 5.25);
    const QVariantMap freq = out.inputs.value(QStringLiteral("frequency")).toMap();
    QCOMPARE(freq.value(QStringLiteral("d")).toInt(), 2);
    QCOMPARE(freq.value(QStringLiteral("t")).toString(), QStringLiteral("Semi-Annual"));
    QCOMPARE(out.inputs.value(QStringLiteral("active")).toBool(), true);
    QCOMPARE(out.inputs.value(QStringLiteral("name")).toString(), QStringLiteral("Bond A — 5% 2029"));
    QCOMPARE(out.inputs.value(QStringLiteral("since")).toString(), QStringLiteral("2026-01-02T00:00:00"));
    QCOMPARE(out.inputs.value(QStringLiteral("@QSpinBox#0")).toInt(), 7);
    const QVariantList sel =
        out.inputs.value(QStringLiteral("countries")).toMap().value(QStringLiteral("sel")).toList();
    QCOMPARE(sel.size(), 1);
    QCOMPARE(sel.first().toMap().value(QStringLiteral("d")).toString(), QStringLiteral("US"));

    // Canonical: a decoded document encodes to the same text (no drift across sessions).
    QCOMPARE(imd::encode(out.inputs, out.layout_sig), text);
    // The signature survives the round trip too.
    QCOMPARE(imd::signature(out.inputs), imd::signature(in));
}

void TstInputMemory::decode_rejects_bad_documents() {
    QVERIFY(!imd::decode(QString()).ok);
    QVERIFY(!imd::decode(QStringLiteral("   ")).ok);
    QVERIFY(!imd::decode(QStringLiteral("not json")).ok);
    QVERIFY(!imd::decode(QStringLiteral("[1,2,3]")).ok);
    QVERIFY(!imd::decode(QStringLiteral("{\"v\":1}")).ok);                         // no inputs
    QVERIFY(!imd::decode(QStringLiteral("{\"v\":1,\"inputs\":[1]}")).ok);           // inputs not an object
    QVERIFY(!imd::decode(QStringLiteral("{\"inputs\":{\"a\":1}}")).ok);             // no version
    QVERIFY(!imd::decode(QStringLiteral("{\"v\":99,\"inputs\":{\"a\":1}}")).ok);    // a newer format
    const imd::Decoded ok = imd::decode(QStringLiteral("{\"v\":1,\"inputs\":{\"a\":1}}"));
    QVERIFY(ok.ok);
    QCOMPARE(ok.layout_sig, QString()); // no positional inputs saved
    QCOMPARE(ok.inputs.value(QStringLiteral("a")).toInt(), 1);
}

void TstInputMemory::signature_is_key_order_independent() {
    QVariantMap a;
    a.insert(QStringLiteral("zeta"), 1);
    a.insert(QStringLiteral("alpha"), QVariantMap{{QStringLiteral("y"), 2}, {QStringLiteral("x"), 1}});
    const QVariantMap b = QJsonDocument::fromJson(R"({"alpha":{"x":1,"y":2},"zeta":1})").object().toVariantMap();
    const QVariantMap c = QJsonDocument::fromJson(R"({"zeta":1,"alpha":{"y":2,"x":1}})").object().toVariantMap();
    const QString sa = imd::signature(a);
    QCOMPARE(sa.size(), 16);
    QCOMPARE(imd::signature(b), sa);
    QCOMPARE(imd::signature(c), sa);
    QCOMPARE(sa, fincept::ui::autoload::signature_of(a)); // the same signature AutoLoadPane compares
}

void TstInputMemory::signature_follows_values() {
    const QVariantMap a{{QStringLiteral("strike"), 100}, {QStringLiteral("type"), QStringLiteral("Call")}};
    QVariantMap b = a;
    b.insert(QStringLiteral("strike"), 105);
    QVERIFY(imd::signature(a) != imd::signature(b));
    QVariantMap c = a;
    c.insert(QStringLiteral("extra"), true);
    QVERIFY(imd::signature(a) != imd::signature(c));
    QCOMPARE(imd::signature(QVariantMap()), imd::signature(QVariantMap())); // stable for "no inputs"
}

void TstInputMemory::layout_guard() {
    const QString saved =
        imd::layout_signature({QStringLiteral("QComboBox"), QStringLiteral("QDoubleSpinBox"), QStringLiteral("QLineEdit")});
    QCOMPARE(saved, QStringLiteral("QComboBox,QDoubleSpinBox,QLineEdit"));
    QVERIFY(imd::positional_restore_allowed(saved, saved));
    // An input added, removed or reordered by an app update: positional values would land in the
    // wrong fields, so they are not restored.
    QVERIFY(!imd::positional_restore_allowed(
        saved, imd::layout_signature({QStringLiteral("QComboBox"), QStringLiteral("QDoubleSpinBox")})));
    QVERIFY(!imd::positional_restore_allowed(
        saved, imd::layout_signature({QStringLiteral("QDoubleSpinBox"), QStringLiteral("QComboBox"),
                                      QStringLiteral("QLineEdit")})));
    // Nothing positional now (or nothing saved): nothing to restore by position.
    QVERIFY(!imd::positional_restore_allowed(saved, QString()));
    QVERIFY(!imd::positional_restore_allowed(QString(), QString()));
    QVERIFY(!imd::positional_restore_allowed(QString(), saved));
}

void TstInputMemory::merge_for_save_keeps_untracked_and_pending() {
    const QVariantMap stored{{QStringLiteral("name"), QStringLiteral("old")},
                             {QStringLiteral("@QSpinBox#0"), 3},
                             {QStringLiteral("lazy_tab_field"), QStringLiteral("keep me")}};
    const QVariantMap current{{QStringLiteral("name"), QStringLiteral("new")},
                              {QStringLiteral("@QSpinBox#0"), 5},
                              {QStringLiteral("country"), QStringLiteral("first item while filling")}};
    const QVariantMap pending{{QStringLiteral("country"), QVariantMap{{QStringLiteral("d"), QStringLiteral("DE")}}}};
    const QVariantMap out = imd::merge_for_save(stored, /*positional_ok=*/true, current, pending);
    QCOMPARE(out.value(QStringLiteral("name")).toString(), QStringLiteral("new"));       // current wins
    QCOMPARE(out.value(QStringLiteral("@QSpinBox#0")).toInt(), 5);
    QCOMPARE(out.value(QStringLiteral("lazy_tab_field")).toString(), QStringLiteral("keep me")); // not tracked now
    // A value still waiting for its combo's items beats the combo's placeholder first item.
    QCOMPARE(out.value(QStringLiteral("country")).toMap().value(QStringLiteral("d")).toString(), QStringLiteral("DE"));
    QCOMPARE(out.size(), 4);
}

void TstInputMemory::merge_for_save_drops_stale_positional_values() {
    const QVariantMap stored{{QStringLiteral("@QSpinBox#0"), 3},
                             {QStringLiteral("@QSpinBox#1"), 9},
                             {QStringLiteral("named"), 1}};
    const QVariantMap current{{QStringLiteral("@QComboBox#0"), QVariantMap{{QStringLiteral("t"), QStringLiteral("A")}}}};
    // The layout changed: the stored positional values belong to another form and are not carried on.
    const QVariantMap out = imd::merge_for_save(stored, /*positional_ok=*/false, current, QVariantMap());
    QVERIFY(!out.contains(QStringLiteral("@QSpinBox#0")));
    QVERIFY(!out.contains(QStringLiteral("@QSpinBox#1")));
    QVERIFY(out.contains(QStringLiteral("@QComboBox#0")));
    QCOMPARE(out.value(QStringLiteral("named")).toInt(), 1); // named values never depend on the layout
}

QTEST_GUILESS_MAIN(TstInputMemory)
#include "tst_input_memory.moc"
