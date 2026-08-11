// Unit tests for src/core/result/Result.h
//
// Result<T> is the error-handling contract for the whole codebase ("Use Result<T>
// for error handling" — CLAUDE.md Code Standards), so its edges are worth pinning
// precisely rather than assuming.
//
// The two behaviours most likely to surprise a caller, both asserted below:
//
//   1. Result<T>::value() on an error is NOT infallible. It is std::get<T> on a
//      variant holding the Error alternative, i.e. it throws std::bad_variant_access.
//      Same for error() on an ok result. Callers must branch on is_ok()/is_err()
//      first; there is no checked accessor and no default-returning overload.
//   2. Result<void> does NOT behave the same way: its error() is a plain member
//      read that returns an empty string on success. So `r.error().empty()` is a
//      safe (if wrong-headed) success test for Result<void> and a THROWING one
//      for Result<T> — and it is not even a correct success test for
//      Result<void>, because err("") is still an error.

#include "core/result/Result.h"

#include <QString>
#include <QTest>

#include <memory>
#include <string>
#include <variant>

using fincept::Result;

// Compare through QString so a failure prints the actual text rather than
// QTest's generic "Compared values are not the same".
static QString qs(const std::string& s) {
    return QString::fromStdString(s);
}

class TstResult : public QObject {
    Q_OBJECT

  private slots:
    void ok_construction();
    void err_construction();
    void value_is_mutable();
    void moves_a_move_only_payload();
    void copies_preserve_state();
    void empty_error_message_is_still_an_error();
    void value_on_error_throws();
    void error_on_ok_throws();
    void void_ok();
    void void_err();
    void void_error_on_ok_is_empty_not_throwing();
    void map_transforms_ok();
    void map_propagates_error();
    void map_changes_type();
};

// ── Result<T> ────────────────────────────────────────────────────────────────

void TstResult::ok_construction() {
    const auto r = Result<int>::ok(42);
    QVERIFY(r.is_ok());
    QVERIFY(!r.is_err());
    QCOMPARE(r.value(), 42);

    // A falsy payload is still a success — Result carries state, not truthiness.
    const auto zero = Result<int>::ok(0);
    QVERIFY(zero.is_ok());
    QCOMPARE(zero.value(), 0);

    const auto empty_str = Result<std::string>::ok(std::string());
    QVERIFY(empty_str.is_ok());
    QVERIFY(empty_str.value().empty());
}

void TstResult::err_construction() {
    const auto r = Result<int>::err("network unreachable");
    QVERIFY(r.is_err());
    QVERIFY(!r.is_ok());
    QCOMPARE(qs(r.error()), QStringLiteral("network unreachable"));
}

void TstResult::value_is_mutable() {
    auto r = Result<int>::ok(1);
    r.value() = 7;
    QCOMPARE(r.value(), 7);
    QVERIFY(r.is_ok());
}

void TstResult::moves_a_move_only_payload() {
    // ok() takes T by value and moves it into the variant, so a move-only payload
    // (unique_ptr, socket handle, …) round-trips without requiring a copy ctor.
    auto r = Result<std::unique_ptr<int>>::ok(std::make_unique<int>(5));
    QVERIFY(r.is_ok());
    QVERIFY(r.value() != nullptr);
    QCOMPARE(*r.value(), 5);

    const auto e = Result<std::unique_ptr<int>>::err("alloc failed");
    QVERIFY(e.is_err());
    QCOMPARE(qs(e.error()), QStringLiteral("alloc failed"));
}

void TstResult::copies_preserve_state() {
    const auto ok_src = Result<int>::ok(11);
    const Result<int> ok_copy = ok_src;
    QVERIFY(ok_copy.is_ok());
    QCOMPARE(ok_copy.value(), 11);

    const auto err_src = Result<int>::err("boom");
    const Result<int> err_copy = err_src;
    QVERIFY(err_copy.is_err());
    QCOMPARE(qs(err_copy.error()), QStringLiteral("boom"));
}

void TstResult::empty_error_message_is_still_an_error() {
    // is_err() is decided by which variant alternative is engaged, never by the
    // message. Using `error().empty()` as a proxy for failure is a bug.
    const auto r = Result<int>::err("");
    QVERIFY(r.is_err());
    QVERIFY(!r.is_ok());
    QVERIFY(r.error().empty());
}

void TstResult::value_on_error_throws() {
#ifdef QT_NO_EXCEPTIONS
    QSKIP("Qt built without exception support; std::get would terminate, not throw");
#else
    const auto r = Result<int>::err("nope");
    QVERIFY(r.is_err());
    QVERIFY_THROWS_EXCEPTION(std::bad_variant_access, (void)r.value());

    auto mutable_r = Result<int>::err("nope");
    QVERIFY_THROWS_EXCEPTION(std::bad_variant_access, (void)mutable_r.value());
#endif
}

void TstResult::error_on_ok_throws() {
#ifdef QT_NO_EXCEPTIONS
    QSKIP("Qt built without exception support; std::get would terminate, not throw");
#else
    const auto r = Result<int>::ok(1);
    QVERIFY(r.is_ok());
    QVERIFY_THROWS_EXCEPTION(std::bad_variant_access, (void)r.error());
#endif
}

// ── Result<void> ─────────────────────────────────────────────────────────────

void TstResult::void_ok() {
    const auto r = Result<void>::ok();
    QVERIFY(r.is_ok());
    QVERIFY(!r.is_err());
}

void TstResult::void_err() {
    const auto r = Result<void>::err("write failed");
    QVERIFY(r.is_err());
    QVERIFY(!r.is_ok());
    QCOMPARE(qs(r.error()), QStringLiteral("write failed"));

    // Same rule as Result<T>: an empty message is still a failure.
    const auto blank = Result<void>::err("");
    QVERIFY(blank.is_err());
    QVERIFY(blank.error().empty());
}

void TstResult::void_error_on_ok_is_empty_not_throwing() {
    // The asymmetry with Result<T>::error(), pinned deliberately: this one is a
    // plain member read, so it is safe on success and returns "".
    const auto r = Result<void>::ok();
    QVERIFY(r.error().empty());
}

// ── map() ────────────────────────────────────────────────────────────────────

void TstResult::map_transforms_ok() {
    const auto r = Result<int>::ok(21).map([](int v) { return v * 2; });
    QVERIFY(r.is_ok());
    QCOMPARE(r.value(), 42);
}

void TstResult::map_propagates_error() {
    bool called = false;
    const auto r = Result<int>::err("upstream failed").map([&called](int v) {
        called = true;
        return v * 2;
    });
    QVERIFY(r.is_err());
    QCOMPARE(qs(r.error()), QStringLiteral("upstream failed"));
    QVERIFY2(!called, "map() must not invoke the transform on an error result");
}

void TstResult::map_changes_type() {
    const auto r = Result<int>::ok(7).map([](int v) { return std::to_string(v) + "s"; });
    QVERIFY(r.is_ok());
    QCOMPARE(qs(r.value()), QStringLiteral("7s"));

    const auto e = Result<int>::err("bad").map([](int v) { return std::to_string(v); });
    QVERIFY(e.is_err());
    QCOMPARE(qs(e.error()), QStringLiteral("bad"));
}

QTEST_GUILESS_MAIN(TstResult)
#include "tst_result.moc"
