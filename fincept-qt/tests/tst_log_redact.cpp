// src/core/privacy/LogRedact.h — the privacy:: names forward to the logger's redaction
// (core/logging/LogRedact.h, defined in Logger.cpp). These are the forwarded rules as the
// call sites see them; tst_logger covers the logger's own helpers in depth.
#include "core/privacy/LogRedact.h"

#include <QtTest>

using namespace fincept::privacy;

class TestLogRedact : public QObject {
    Q_OBJECT
  private slots:
    void home_prefix_becomes_tilde() {
        const QString home = QStringLiteral("/home/ann");
        QCOMPARE(redact_home(QStringLiteral("/home/ann/.local/share/fincept/fincept.db"), home),
                 QStringLiteral("~/.local/share/fincept/fincept.db"));
        QCOMPARE(redact_home(QStringLiteral("/home/ann"), home), QStringLiteral("~"));
        QVERIFY(is_under_home(QStringLiteral("/home/ann/x"), home));
        QVERIFY(!is_under_home(QStringLiteral("/home/anna"), home));
    }

    void other_paths() {
        const QString home = QStringLiteral("/home/ann");
        // Stricter than before: another account's home is hidden as well.
        QCOMPARE(redact_home(QStringLiteral("/home/anna/x"), home), QStringLiteral("~/x"));
        QCOMPARE(redact_home(QStringLiteral("/opt/fincept/bin"), home), QStringLiteral("/opt/fincept/bin"));
        QCOMPARE(redact_home(QString(), home), QString());
        // A root home (service account) must not match every path.
        QCOMPARE(redact_home(QStringLiteral("/etc/x"), QStringLiteral("/")), QStringLiteral("/etc/x"));
    }

    void windows_spelling() {
        const QString home = QStringLiteral("C:/Users/Ann");
        // The rest of the path keeps the separators it was given.
        QCOMPARE(redact_home(QStringLiteral("C:\\Users\\Ann\\AppData\\Local\\x.db"), home),
                 QStringLiteral("~\\AppData\\Local\\x.db"));
        QCOMPARE(redact_text(QStringLiteral("cannot open C:\\Users\\Ann\\AppData\\x.db: denied")),
                 QStringLiteral("cannot open ~\\AppData\\x.db: denied"));
    }

    void url_drops_query_userinfo_fragment_and_third_party_paths() {
        // A third-party path can be the credential (webhooks, bot tokens): stricter than before.
        QCOMPARE(redact_url(QStringLiteral("https://user:pw@api.example.com:8443/v1/quotes?apikey=SECRET#frag")),
                 QStringLiteral("https://api.example.com:8443/<redacted>"));
        QCOMPARE(redact_url(QStringLiteral("https://api.fincept.in/market/quote?apikey=k")),
                 QStringLiteral("https://api.fincept.in/market/quote?<redacted>"));
        QCOMPARE(redact_url(QStringLiteral("http://example.com")), QStringLiteral("http://example.com"));
        QCOMPARE(redact_url(QStringLiteral("not a url")), QStringLiteral("<invalid url>"));
        // A relative API path keeps its path, never its query.
        QCOMPARE(redact_url(QStringLiteral("/relative/path?x=1")), QStringLiteral("/relative/path?<redacted>"));
        QCOMPARE(url_origin(QStringLiteral("http://bob:hunter2@proxy.corp:3128/")),
                 QStringLiteral("http://proxy.corp:3128"));
        QCOMPARE(url_origin(QStringLiteral("https://hooks.slack.com/services/T000/B000/XXXX")),
                 QStringLiteral("https://hooks.slack.com"));
    }

    void mask_never_leaks_the_value() {
        const QString ellipsis(QChar(0x2026));
        QCOMPARE(mask(QString()), QString());
        QCOMPARE(mask(QStringLiteral("sk-abcdef")), ellipsis);
        QCOMPARE(mask(QStringLiteral("x")), ellipsis);
        QCOMPARE(mask_id(QStringLiteral("AB12345678")), ellipsis + QStringLiteral("5678"));
        QCOMPARE(mask_id(QStringLiteral("ABCD")), ellipsis);
        QCOMPARE(mask_id(QString()), QString());
    }

    void text_redaction() {
        const QString in = QStringLiteral(
            "[WARN] [HTTP] GET https://api.example.com/v1/q?apikey=SECRET1 failed for bob@example.com "
            "token=abcd1234 password: \"p w\" Authorization: Bearer abcdefghijklmnop file=/home/ann/notes.txt "
            "jwt=eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiIxIn0.sig opaque 0123456789abcdef0123456789abcdef01");
        const QString out = redact_text(in);
        QVERIFY2(!out.contains(QStringLiteral("SECRET1")), qPrintable(out));
        QVERIFY2(!out.contains(QStringLiteral("/v1/q")), qPrintable(out));
        QVERIFY2(!out.contains(QStringLiteral("bob@example.com")), qPrintable(out));
        QVERIFY2(!out.contains(QStringLiteral("abcd1234")), qPrintable(out));
        QVERIFY2(!out.contains(QStringLiteral("p w")), qPrintable(out));
        QVERIFY2(!out.contains(QStringLiteral("abcdefghijklmnop")), qPrintable(out));
        QVERIFY2(!out.contains(QStringLiteral("/home/ann")), qPrintable(out));
        QVERIFY2(!out.contains(QStringLiteral("eyJhbGci")), qPrintable(out));
        QVERIFY2(!out.contains(QStringLiteral("0123456789abcdef0123456789abcdef01")), qPrintable(out));
        QVERIFY2(out.contains(QStringLiteral("https://api.example.com")), qPrintable(out));
        QVERIFY2(out.contains(QStringLiteral("~/notes.txt")), qPrintable(out));
        QVERIFY2(out.contains(QStringLiteral("<email>")), qPrintable(out));
        QVERIFY2(out.startsWith(QStringLiteral("[WARN] [HTTP] GET")), qPrintable(out));
        // Ordinary words and numbers survive.
        QCOMPARE(redact_text(QStringLiteral("Script failed script=x.py exit=1 duration_ms=420 tokens_in=12")),
                 QStringLiteral("Script failed script=x.py exit=1 duration_ms=420 tokens_in=12"));
        // Another account's folder is hidden too.
        QVERIFY(!redact_text(QStringLiteral("at /home/bob/x")).contains(QStringLiteral("bob")));
    }
};

QTEST_GUILESS_MAIN(TestLogRedact)
#include "tst_log_redact.moc"
