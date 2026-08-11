// Unit tests for src/trading/brokers/BrokerModifyFields.h
//
// The untyped modify_order(mods) map is read under 32 distinct key names across
// 22 broker adapters. Every spelling mismatch reads as "absent" and transmits
// the type default — for a price field, 0. Two live examples this header exists
// to stop, both pinned as regression tests at the bottom of this file:
//
//   * IIFL read "limitPrice" while callers send "price"        -> limit price 0
//   * Shoonya/Flattrade/Tradejini read "triggerPrice" while
//     callers send "trigger_price"                             -> trgprc = 0
//
// The subtle invariant, and the reason first_present() exists at all:
// PRESENCE decides, not truthiness. A caller that deliberately sends price 0
// must get 0 back, not the next alias in the list and not the fallback.

#include "trading/brokers/BrokerModifyFields.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QTest>

using namespace fincept::trading::modify_fields;

class TstBrokerModifyFields : public QObject {
    Q_OBJECT

  private slots:
    void canonical_key_wins_over_alias();
    void alias_is_used_when_canonical_is_absent();
    void every_declared_alias_is_readable();
    void legitimate_zero_beats_a_later_alias();
    void missing_key_returns_caller_fallback();
    void numeric_strings_parse();
    void non_numeric_values_fall_back();
    void null_is_treated_as_absent();
    void undefined_is_treated_as_absent();
    void text_reads_and_empty_string_falls_back();
    void has_any_reports_presence_not_truthiness();
    void regression_iifl_price_modify();
    void regression_shoonya_trigger_price_modify();
};

// ── canonical vs alias ───────────────────────────────────────────────────────

void TstBrokerModifyFields::canonical_key_wins_over_alias() {
    // Priority comes from the KEY ARRAY order, not from the object's storage
    // order. QJsonObject keeps keys sorted, so "limitPrice" physically precedes
    // "price" here — if the implementation ever iterated the object instead of
    // the key set, this test is what catches it.
    QJsonObject mods;
    mods.insert(QStringLiteral("limitPrice"), 999.0);
    mods.insert(QStringLiteral("price"), 101.5);
    QCOMPARE(number(mods, kPrice, -1.0), 101.5);

    QJsonObject trig;
    trig.insert(QStringLiteral("auxPrice"), 77.0);
    trig.insert(QStringLiteral("stopPrice"), 66.0);
    trig.insert(QStringLiteral("trigger_price"), 50.0);
    QCOMPARE(number(trig, kTrigger, -1.0), 50.0);

    QJsonObject qty;
    qty.insert(QStringLiteral("qty"), 999.0);
    qty.insert(QStringLiteral("quantity"), 25.0);
    QCOMPARE(number(qty, kQuantity, -1.0), 25.0);

    QJsonObject typ;
    typ.insert(QStringLiteral("type"), QStringLiteral("MARKET"));
    typ.insert(QStringLiteral("orderType"), QStringLiteral("SL"));
    typ.insert(QStringLiteral("order_type"), QStringLiteral("LIMIT"));
    QCOMPARE(text(typ, kOrderType), QStringLiteral("LIMIT"));

    QJsonObject prod;
    prod.insert(QStringLiteral("productType"), QStringLiteral("NRML"));
    prod.insert(QStringLiteral("product"), QStringLiteral("MIS"));
    QCOMPARE(text(prod, kProduct), QStringLiteral("MIS"));
}

void TstBrokerModifyFields::alias_is_used_when_canonical_is_absent() {
    QJsonObject a;
    a.insert(QStringLiteral("limitPrice"), 250.25);
    QCOMPARE(number(a, kPrice, -1.0), 250.25);

    QJsonObject b;
    b.insert(QStringLiteral("limit_price"), 42.0);
    QCOMPARE(number(b, kPrice, -1.0), 42.0);

    // Last entry in the list must still be reachable.
    QJsonObject c;
    c.insert(QStringLiteral("auxPrice"), 12.5);
    QCOMPARE(number(c, kTrigger, -1.0), 12.5);

    QJsonObject d;
    d.insert(QStringLiteral("qty"), 7.0);
    QCOMPARE(number(d, kQuantity, -1.0), 7.0);

    QJsonObject e;
    e.insert(QStringLiteral("type"), QStringLiteral("SL-M"));
    QCOMPARE(text(e, kOrderType), QStringLiteral("SL-M"));

    QJsonObject f;
    f.insert(QStringLiteral("product_type"), QStringLiteral("CNC"));
    QCOMPARE(text(f, kProduct), QStringLiteral("CNC"));
}

void TstBrokerModifyFields::every_declared_alias_is_readable() {
    // Table-driven guard: if someone typos or drops an entry while editing the
    // key arrays, the adapter that used that spelling starts silently sending 0.
    for (const char* k : kPrice) {
        QJsonObject m;
        m.insert(QString::fromLatin1(k), 123.0);
        QVERIFY2(has_any(m, kPrice), k);
        QCOMPARE(number(m, kPrice, -1.0), 123.0);
    }
    for (const char* k : kTrigger) {
        QJsonObject m;
        m.insert(QString::fromLatin1(k), 456.0);
        QVERIFY2(has_any(m, kTrigger), k);
        QCOMPARE(number(m, kTrigger, -1.0), 456.0);
    }
    for (const char* k : kQuantity) {
        QJsonObject m;
        m.insert(QString::fromLatin1(k), 10.0);
        QCOMPARE(number(m, kQuantity, -1.0), 10.0);
    }
    for (const char* k : kOrderType) {
        QJsonObject m;
        m.insert(QString::fromLatin1(k), QStringLiteral("LIMIT"));
        QCOMPARE(text(m, kOrderType), QStringLiteral("LIMIT"));
    }
    for (const char* k : kProduct) {
        QJsonObject m;
        m.insert(QString::fromLatin1(k), QStringLiteral("MIS"));
        QCOMPARE(text(m, kProduct), QStringLiteral("MIS"));
    }
}

// ── the subtle one: 0 is a value, not an absence ─────────────────────────────

void TstBrokerModifyFields::legitimate_zero_beats_a_later_alias() {
    // A caller that explicitly sends price 0 means 0. Truthiness-based lookup
    // would fall through to limitPrice and transmit 99 — a wrong-price order.
    QJsonObject p;
    p.insert(QStringLiteral("price"), 0.0);
    p.insert(QStringLiteral("limitPrice"), 99.0);
    QCOMPARE(number(p, kPrice, -1.0), 0.0);

    QJsonObject t;
    t.insert(QStringLiteral("trigger_price"), 0.0);
    t.insert(QStringLiteral("stopPrice"), 42.0);
    QCOMPARE(number(t, kTrigger, -1.0), 0.0);

    // ...and it must not degrade to the caller's fallback either.
    QJsonObject only_zero;
    only_zero.insert(QStringLiteral("price"), 0.0);
    QCOMPARE(number(only_zero, kPrice, 250.0), 0.0);

    // The primitive underneath says the same thing.
    const QJsonValue v = first_present(only_zero, kPrice);
    QVERIFY(!v.isUndefined());
    QCOMPARE(v.toDouble(-1.0), 0.0);
    QVERIFY(has_any(only_zero, kPrice));

    // Same rule for a numeric-string zero.
    QJsonObject zero_str;
    zero_str.insert(QStringLiteral("price"), QStringLiteral("0"));
    zero_str.insert(QStringLiteral("limitPrice"), 99.0);
    QCOMPARE(number(zero_str, kPrice, -1.0), 0.0);
}

// ── absence ──────────────────────────────────────────────────────────────────

void TstBrokerModifyFields::missing_key_returns_caller_fallback() {
    QJsonObject unrelated;
    unrelated.insert(QStringLiteral("order_id"), QStringLiteral("ABC123"));

    QCOMPARE(number(unrelated, kPrice, 7.5), 7.5);
    QCOMPARE(number(unrelated, kTrigger, -1.0), -1.0);
    QCOMPARE(number(unrelated, kQuantity), 0.0); // documented default fallback
    QCOMPARE(text(unrelated, kProduct, QStringLiteral("MIS")), QStringLiteral("MIS"));
    QVERIFY(text(unrelated, kOrderType).isEmpty()); // documented default fallback
    QVERIFY(!has_any(unrelated, kPrice));
    QVERIFY(first_present(unrelated, kPrice).isUndefined());

    const QJsonObject empty;
    QCOMPARE(number(empty, kPrice, 3.25), 3.25);
    QVERIFY(!has_any(empty, kTrigger));
}

void TstBrokerModifyFields::null_is_treated_as_absent() {
    QJsonObject m;
    m.insert(QStringLiteral("price"), QJsonValue(QJsonValue::Null));
    QCOMPARE(number(m, kPrice, 12.0), 12.0);
    QVERIFY(!has_any(m, kPrice));

    // A null canonical key must not shadow a real alias.
    QJsonObject shadow;
    shadow.insert(QStringLiteral("price"), QJsonValue(QJsonValue::Null));
    shadow.insert(QStringLiteral("limitPrice"), 88.0);
    QCOMPARE(number(shadow, kPrice, -1.0), 88.0);

    // Same behaviour when the null arrives through a real JSON parse.
    QJsonParseError perr{};
    const QJsonDocument doc =
        QJsonDocument::fromJson(QByteArray(R"({"trigger_price": null, "stopPrice": 55.5})"), &perr);
    QVERIFY(perr.error == QJsonParseError::NoError);
    QCOMPARE(number(doc.object(), kTrigger, -1.0), 55.5);
}

void TstBrokerModifyFields::undefined_is_treated_as_absent() {
    // Whether Qt stores an Undefined value or drops the key on insert, the
    // observable contract is identical: the reader must skip it.
    QJsonObject m;
    m.insert(QStringLiteral("price"), QJsonValue(QJsonValue::Undefined));
    QCOMPARE(number(m, kPrice, 31.0), 31.0);
    QVERIFY(!has_any(m, kPrice));

    QJsonObject shadow;
    shadow.insert(QStringLiteral("price"), QJsonValue(QJsonValue::Undefined));
    shadow.insert(QStringLiteral("limit_price"), 64.0);
    QCOMPARE(number(shadow, kPrice, -1.0), 64.0);
}

// ── numeric strings (the Zerodha bug) ────────────────────────────────────────

void TstBrokerModifyFields::numeric_strings_parse() {
    // Why number() goes through toVariant() rather than a typed accessor:
    // QJsonValue::toString() on a NUMBER returns an empty string, which is how
    // Zerodha's adapter turned a perfectly good price into "".
    QCOMPARE(QJsonValue(101.25).toString(), QString());

    QJsonObject s;
    s.insert(QStringLiteral("price"), QStringLiteral("101.25"));
    QCOMPARE(number(s, kPrice, -1.0), 101.25);

    QJsonObject q;
    q.insert(QStringLiteral("quantity"), QStringLiteral("50"));
    QCOMPARE(number(q, kQuantity, -1.0), 50.0);

    QJsonObject neg;
    neg.insert(QStringLiteral("trigger_price"), QStringLiteral("-12.5"));
    QCOMPARE(number(neg, kTrigger, 0.0), -12.5);

    // ...and a JSON number still reads as a number.
    QJsonObject n;
    n.insert(QStringLiteral("price"), 101.25);
    QCOMPARE(number(n, kPrice, -1.0), 101.25);
}

void TstBrokerModifyFields::non_numeric_values_fall_back() {
    QJsonObject junk;
    junk.insert(QStringLiteral("price"), QStringLiteral("not-a-number"));
    QCOMPARE(number(junk, kPrice, 9.0), 9.0);

    QJsonObject inner;
    inner.insert(QStringLiteral("v"), 1.0);
    QJsonObject structured;
    structured.insert(QStringLiteral("price"), inner);
    QCOMPARE(number(structured, kPrice, 9.0), 9.0);

    QJsonArray list;
    list.append(1.0);
    list.append(2.0);
    QJsonObject arr;
    arr.insert(QStringLiteral("quantity"), list);
    QCOMPARE(number(arr, kQuantity, 9.0), 9.0);
}

// ── text() ───────────────────────────────────────────────────────────────────

void TstBrokerModifyFields::text_reads_and_empty_string_falls_back() {
    QJsonObject ok;
    ok.insert(QStringLiteral("product"), QStringLiteral("NRML"));
    QCOMPARE(text(ok, kProduct, QStringLiteral("MIS")), QStringLiteral("NRML"));

    // An empty string is treated as "no usable value" and yields the fallback.
    QJsonObject blank;
    blank.insert(QStringLiteral("product"), QStringLiteral(""));
    QCOMPARE(text(blank, kProduct, QStringLiteral("MIS")), QStringLiteral("MIS"));

    // Documented asymmetry vs number(): an empty CANONICAL string does not fall
    // through to a later alias — first_present() already committed to the key,
    // and text() then applies the caller's fallback. Pinned so a future change
    // to either half is a deliberate one.
    QJsonObject blank_then_alias;
    blank_then_alias.insert(QStringLiteral("product"), QStringLiteral(""));
    blank_then_alias.insert(QStringLiteral("productType"), QStringLiteral("NRML"));
    QCOMPARE(text(blank_then_alias, kProduct, QStringLiteral("MIS")), QStringLiteral("MIS"));
}

void TstBrokerModifyFields::has_any_reports_presence_not_truthiness() {
    QJsonObject zero;
    zero.insert(QStringLiteral("quantity"), 0.0);
    QVERIFY(has_any(zero, kQuantity));

    QJsonObject blank;
    blank.insert(QStringLiteral("product"), QStringLiteral(""));
    QVERIFY(has_any(blank, kProduct)); // present, even though text() falls back

    QJsonObject nulled;
    nulled.insert(QStringLiteral("product"), QJsonValue(QJsonValue::Null));
    QVERIFY(!has_any(nulled, kProduct));

    QVERIFY(!has_any(QJsonObject(), kProduct));
}

// ── regressions, named for the incidents ─────────────────────────────────────

void TstBrokerModifyFields::regression_iifl_price_modify() {
    // Caller-side spelling. IIFL read "limitPrice" and so transmitted
    // modifiedLimitPrice = 0 for every price modify.
    QJsonObject mods;
    mods.insert(QStringLiteral("price"), 145.5);
    mods.insert(QStringLiteral("quantity"), 50.0);

    QCOMPARE(number(mods, kPrice, 0.0), 145.5);
    QVERIFY2(!qFuzzyIsNull(number(mods, kPrice, 0.0)), "the IIFL bug transmitted 0 here");
    QCOMPARE(number(mods, kQuantity, 0.0), 50.0);
}

void TstBrokerModifyFields::regression_shoonya_trigger_price_modify() {
    // Shoonya / Flattrade / Tradejini read "triggerPrice" and so transmitted
    // trgprc = 0 for every stop-loss modify.
    QJsonObject mods;
    mods.insert(QStringLiteral("trigger_price"), 99.5);
    mods.insert(QStringLiteral("price"), 100.0);

    QCOMPARE(number(mods, kTrigger, 0.0), 99.5);
    QVERIFY2(!qFuzzyIsNull(number(mods, kTrigger, 0.0)), "the Shoonya bug transmitted trgprc=0 here");
    QCOMPARE(number(mods, kPrice, 0.0), 100.0);

    // And the mirror image: an adapter-spelled payload read by canonical name.
    QJsonObject adapter_spelled;
    adapter_spelled.insert(QStringLiteral("triggerPrice"), 99.5);
    QCOMPARE(number(adapter_spelled, kTrigger, 0.0), 99.5);
}

QTEST_GUILESS_MAIN(TstBrokerModifyFields)
#include "tst_broker_modify_fields.moc"
