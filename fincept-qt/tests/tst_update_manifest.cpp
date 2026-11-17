// Unit tests for src/services/updater/UpdateManifest.{h,cpp} (Qt Core + the
// orlp/ed25519 C library only).
//
// The in-app updater trusts a release only through updates.json: it must carry
// an Ed25519 signature under the pinned key (UPDATE_SIGNING_PUBLIC_KEY_HEX),
// and only then is it parsed. These tests cover:
//   * verify_manifest_signature — a real signature made with the release key
//     verifies (base64 and hex forms); any change to the manifest, a signature
//     made with another key, and malformed signatures / keys are all rejected.
//   * parse_version / is_newer_version — strict X.Y.Z, numeric comparison.
//   * parse_manifest — schema 3 only, strict version, https-only assets (http
//     to this machine only when allowed), 64-hex sha256, optional size.
//
// The signed fixture below claims version 0.0.1, older than every release, so
// this signed file can never offer anyone an update. It was signed with
//   openssl pkeyutl -sign -rawin -inkey fincept-update-signing.key -in updates.json
// The "other key" signature was made with a throwaway key generated for this
// test (its private half was deleted).

#include "services/updater/UpdateManifest.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QTest>

using fincept::services::updater::is_newer_version;
using fincept::services::updater::ManifestResult;
using fincept::services::updater::parse_manifest;
using fincept::services::updater::parse_version;
using fincept::services::updater::UPDATE_SIGNING_PUBLIC_KEY_HEX;
using fincept::services::updater::verify_manifest_signature;

namespace {

// Exact bytes that were signed (LF line endings, trailing newline).
const char kFixtureManifest[] =
    "{\n"
    "  \"schema-version\": 3,\n"
    "  \"version\": \"0.0.1\",\n"
    "  \"release-url\": \"https://github.com/Fincept-Corporation/FinceptTerminal/releases/tag/v0.0.1\",\n"
    "  \"notes\": \"Signed test fixture of tst_update_manifest. Version 0.0.1 is older than every release, so it "
    "never offers an update.\",\n"
    "  \"assets\": {\n"
    "    \"linux-x64-appimage\": {\n"
    "      \"url\": \"https://example.invalid/FinceptTerminal-0.0.1-linux-x64-setup.run\",\n"
    "      \"sha256\": \"0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF\",\n"
    "      \"size\": 1048576\n"
    "    }\n"
    "  }\n"
    "}\n";
constexpr qsizetype kFixtureSize = 515;

// Release-key signature of kFixtureManifest.
const char kFixtureSignatureBase64[] =
    "RHcT7i+/arl+ulDPDUlEaQs8wB14RvRVfl/+oi8y5wX3AXkcWkzQhZvUPEhlQtOuZOoojWpWEWkTHL/0/5xlDw==";
const char kFixtureSignatureHex[] = "447713ee2fbf6ab97eba50cf0d4944690b3cc01d7846f4557e5ffea22f32e705"
                                    "f701791c5a4cd0859bd43c486542d3ae64ea288d6a561169131cbff4ff9c650f";

// A different key pair: its public key, and its signature of kFixtureManifest.
const char kOtherPublicKeyHex[] = "f40c1482bcdc697c9953f38f404bde3a842575064c1acc6d4bc9e2e67314955c";
const char kOtherSignatureBase64[] =
    "9IGTVnJSQnTvYWBmffVyn6EQQSd+DNPNUNQILer4OoY3qrq4q6Pq/jT1l2o3mju+JADp8DTwc2/QcbROWF89Cw==";

const QString kSha = QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

QByteArray fixture() {
    return QByteArray(kFixtureManifest, kFixtureSize);
}

QJsonObject asset_entry(const QString& url, const QString& sha256 = kSha, qint64 size = 1234) {
    QJsonObject entry;
    entry.insert(QStringLiteral("url"), url);
    entry.insert(QStringLiteral("sha256"), sha256);
    entry.insert(QStringLiteral("size"), size);
    return entry;
}

/// A well-formed schema-3 manifest with one linux-x64-deb asset.
QJsonObject base_manifest() {
    QJsonObject assets;
    assets.insert(QStringLiteral("linux-x64-deb"),
                  asset_entry(QStringLiteral("https://github.com/o/r/releases/download/v4.5.4/ft_4.5.4_amd64.deb")));
    QJsonObject root;
    root.insert(QStringLiteral("schema-version"), 3);
    root.insert(QStringLiteral("version"), QStringLiteral("4.5.4"));
    root.insert(QStringLiteral("release-url"), QStringLiteral("https://github.com/o/r/releases/tag/v4.5.4"));
    root.insert(QStringLiteral("notes"), QStringLiteral("  Faster charts.  "));
    root.insert(QStringLiteral("assets"), assets);
    return root;
}

QByteArray bytes(const QJsonObject& root) {
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QJsonObject with_asset(const QJsonValue& entry, const QString& key = QStringLiteral("linux-x64-deb")) {
    QJsonObject root = base_manifest();
    QJsonObject assets;
    assets.insert(key, entry);
    root.insert(QStringLiteral("assets"), assets);
    return root;
}

ManifestResult parse_deb(const QJsonObject& root, bool allow_loopback_http = false) {
    return parse_manifest(bytes(root), QStringLiteral("linux-x64-deb"), allow_loopback_http);
}

} // namespace

class TstUpdateManifest : public QObject {
    Q_OBJECT

  private slots:
    // verify_manifest_signature
    void fixture_is_intact();
    void valid_base64_signature_is_accepted();
    void valid_hex_signature_is_accepted();
    void wrapped_or_padded_signature_is_accepted();
    void any_modified_byte_is_rejected();
    void other_changes_to_the_manifest_are_rejected();
    void signature_from_another_key_is_rejected();
    void garbage_signatures_are_rejected();
    void malformed_public_keys_never_verify();
    void empty_manifest_is_rejected();
    void verified_fixture_parses_and_offers_no_update();

    // versions
    void parse_version_accepts_strict_triples();
    void parse_version_rejects_everything_else();
    void newer_version_comparison();

    // parse_manifest
    void well_formed_manifest_is_parsed();
    void missing_asset_key_gives_info_without_asset();
    void empty_asset_key_picks_nothing();
    void http_asset_url_is_rejected();
    void loopback_http_only_when_allowed();
    void other_url_schemes_are_rejected();
    void bad_sha256_is_rejected();
    void sha256_is_lower_cased();
    void size_is_optional_but_must_be_sane();
    void schema_version_must_be_3();
    void version_must_be_strict();
    void insecure_release_url_is_dropped();
    void malformed_documents_are_rejected();
};

// ── verify_manifest_signature ───────────────────────────────────────────────

void TstUpdateManifest::fixture_is_intact() {
    // If this fails the fixture text was edited: the signatures below no
    // longer match it. Re-sign or restore it rather than "fixing" this check.
    QCOMPARE(qsizetype(sizeof(kFixtureManifest) - 1), kFixtureSize);
    QVERIFY(!fixture().contains('\r'));
    QCOMPARE(QString::fromLatin1(UPDATE_SIGNING_PUBLIC_KEY_HEX),
             QStringLiteral("b4f20d945ef8588656e77841b2b7b84b8cf90ffb157656949ff0fde7013280e9"));
}

void TstUpdateManifest::valid_base64_signature_is_accepted() {
    QVERIFY(verify_manifest_signature(fixture(), QByteArray(kFixtureSignatureBase64)));
    // As fetched from updates.json.sig: usually with a trailing newline.
    QVERIFY(verify_manifest_signature(fixture(), QByteArray(kFixtureSignatureBase64) + "\n"));
    QVERIFY(verify_manifest_signature(fixture(), QByteArray(kFixtureSignatureBase64) + "\r\n"));
}

void TstUpdateManifest::valid_hex_signature_is_accepted() {
    QVERIFY(verify_manifest_signature(fixture(), QByteArray(kFixtureSignatureHex)));
    QVERIFY(verify_manifest_signature(fixture(), QByteArray(kFixtureSignatureHex).toUpper()));
    QVERIFY(verify_manifest_signature(fixture(), QByteArray(kFixtureSignatureHex) + "\n"));
}

void TstUpdateManifest::wrapped_or_padded_signature_is_accepted() {
    // `base64` without -w0 wraps at 76 columns.
    QByteArray wrapped(kFixtureSignatureBase64);
    wrapped.insert(76, '\n');
    QVERIFY(verify_manifest_signature(fixture(), wrapped));
    QVERIFY(verify_manifest_signature(fixture(), "  " + QByteArray(kFixtureSignatureBase64) + "  "));
    // The explicit key argument and the default are the same key.
    QVERIFY(verify_manifest_signature(fixture(), QByteArray(kFixtureSignatureBase64),
                                      QString::fromLatin1(UPDATE_SIGNING_PUBLIC_KEY_HEX).toUpper()));
}

void TstUpdateManifest::any_modified_byte_is_rejected() {
    const QByteArray original = fixture();
    const QByteArray signature(kFixtureSignatureBase64);
    for (qsizetype i = 0; i < original.size(); ++i) {
        for (const char mask : {'\x01', '\x80'}) {
            QByteArray tampered = original;
            tampered[i] = char(tampered[i] ^ mask);
            if (verify_manifest_signature(tampered, signature))
                QFAIL(qPrintable(QStringLiteral("byte %1 (mask %2) changed but the signature still verified")
                                     .arg(i)
                                     .arg(int(static_cast<unsigned char>(mask)))));
        }
    }
}

void TstUpdateManifest::other_changes_to_the_manifest_are_rejected() {
    const QByteArray signature(kFixtureSignatureBase64);
    QByteArray crlf = fixture();
    crlf.replace("\n", "\r\n");
    QVERIFY(!verify_manifest_signature(crlf, signature));
    QVERIFY(!verify_manifest_signature(fixture() + "\n", signature));
    QVERIFY(!verify_manifest_signature(fixture().chopped(1), signature));
    QVERIFY(!verify_manifest_signature(fixture().trimmed(), signature));
    QVERIFY(!verify_manifest_signature(" " + fixture(), signature));
    QByteArray newer = fixture();
    newer.replace("\"0.0.1\"", "\"9.9.9\"");
    QVERIFY(!verify_manifest_signature(newer, signature));
}

void TstUpdateManifest::signature_from_another_key_is_rejected() {
    const QString other_key = QString::fromLatin1(kOtherPublicKeyHex);
    // The other signature is genuine — under its own key...
    QVERIFY(verify_manifest_signature(fixture(), QByteArray(kOtherSignatureBase64), other_key));
    // ...but not under the pinned key, the default.
    QVERIFY(!verify_manifest_signature(fixture(), QByteArray(kOtherSignatureBase64)));
    // And the release signature does not verify under the other key.
    QVERIFY(!verify_manifest_signature(fixture(), QByteArray(kFixtureSignatureBase64), other_key));
}

void TstUpdateManifest::garbage_signatures_are_rejected() {
    const QByteArray m = fixture();
    QVERIFY(!verify_manifest_signature(m, QByteArray()));
    QVERIFY(!verify_manifest_signature(m, QByteArray("\n")));
    QVERIFY(!verify_manifest_signature(m, QByteArray("not a signature")));
    QVERIFY(!verify_manifest_signature(m, QByteArray(64, '\0').toBase64()));
    QVERIFY(!verify_manifest_signature(m, QByteArray(64, '\xff').toBase64()));
    QVERIFY(!verify_manifest_signature(m, QByteArray(128, '0')));
    QVERIFY(!verify_manifest_signature(m, QByteArray(128, 'z')));

    // Right encoding, wrong length.
    const QByteArray raw = QByteArray::fromBase64(QByteArray(kFixtureSignatureBase64));
    QCOMPARE(raw.size(), qsizetype(64));
    QVERIFY(!verify_manifest_signature(m, raw.left(63).toBase64()));
    QVERIFY(!verify_manifest_signature(m, (raw + 'x').toBase64()));
    QVERIFY(!verify_manifest_signature(m, QByteArray(kFixtureSignatureHex).chopped(2)));
    QVERIFY(!verify_manifest_signature(m, QByteArray(kFixtureSignatureHex) + "00"));
    // The raw 64 bytes are not an accepted encoding.
    QVERIFY(!verify_manifest_signature(m, raw));

    // One flipped bit anywhere in the signature.
    for (qsizetype i = 0; i < raw.size(); ++i) {
        QByteArray flipped = raw;
        flipped[i] = char(flipped[i] ^ 0x04);
        if (verify_manifest_signature(m, flipped.toBase64()))
            QFAIL(qPrintable(QStringLiteral("signature byte %1 changed but it still verified").arg(i)));
    }

    // Base64 with a character outside the alphabet, and hex with a non-hex digit.
    QByteArray bad_b64(kFixtureSignatureBase64);
    bad_b64[10] = '*';
    QVERIFY(!verify_manifest_signature(m, bad_b64));
    QByteArray bad_hex(kFixtureSignatureHex);
    bad_hex[5] = 'g';
    QVERIFY(!verify_manifest_signature(m, bad_hex));
}

void TstUpdateManifest::malformed_public_keys_never_verify() {
    const QByteArray m = fixture();
    const QByteArray sig(kFixtureSignatureBase64);
    const QString key = QString::fromLatin1(UPDATE_SIGNING_PUBLIC_KEY_HEX);
    QVERIFY(!verify_manifest_signature(m, sig, QString()));
    QVERIFY(!verify_manifest_signature(m, sig, key.left(62)));
    QVERIFY(!verify_manifest_signature(m, sig, key + QStringLiteral("00")));
    QString non_hex = key;
    non_hex[3] = QLatin1Char('x');
    QVERIFY(!verify_manifest_signature(m, sig, non_hex));
    QString spaced = key;
    spaced[10] = QLatin1Char(' ');
    QVERIFY(!verify_manifest_signature(m, sig, spaced));
    QVERIFY(!verify_manifest_signature(m, sig, QString(64, QLatin1Char('0'))));
}

void TstUpdateManifest::empty_manifest_is_rejected() {
    QVERIFY(!verify_manifest_signature(QByteArray(), QByteArray(kFixtureSignatureBase64)));
}

void TstUpdateManifest::verified_fixture_parses_and_offers_no_update() {
    QVERIFY(verify_manifest_signature(fixture(), QByteArray(kFixtureSignatureBase64)));
    const ManifestResult r = parse_manifest(fixture(), QStringLiteral("linux-x64-appimage"));
    QVERIFY2(r.info.has_value(), qPrintable(r.error));
    QCOMPARE(r.info->version, QStringLiteral("0.0.1"));
    QCOMPARE(r.info->release_url,
             QStringLiteral("https://github.com/Fincept-Corporation/FinceptTerminal/releases/tag/v0.0.1"));
    QVERIFY(r.info->has_asset());
    QCOMPARE(r.info->asset.url, QStringLiteral("https://example.invalid/FinceptTerminal-0.0.1-linux-x64-setup.run"));
    QCOMPARE(r.info->asset.sha256, QStringLiteral("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
    QCOMPARE(r.info->asset.size, qint64(1048576));
    QVERIFY(!is_newer_version(QStringLiteral("0.0.1"), r.info->version));
    QVERIFY(!is_newer_version(QStringLiteral("4.5.3"), r.info->version));
}

// ── versions ────────────────────────────────────────────────────────────────

void TstUpdateManifest::parse_version_accepts_strict_triples() {
    int a = -1, b = -1, c = -1;
    QVERIFY(parse_version(QStringLiteral("4.5.3"), a, b, c));
    QCOMPARE(a, 4);
    QCOMPARE(b, 5);
    QCOMPARE(c, 3);
    QVERIFY(parse_version(QStringLiteral("0.0.0"), a, b, c));
    QCOMPARE(a + b + c, 0);
    QVERIFY(parse_version(QStringLiteral("10.20.300"), a, b, c));
    QCOMPARE(c, 300);
    QVERIFY(parse_version(QStringLiteral("123456789.0.1"), a, b, c));
    QCOMPARE(a, 123456789);
}

void TstUpdateManifest::parse_version_rejects_everything_else() {
    const QStringList bad = {
        QString(),
        QStringLiteral("4"),
        QStringLiteral("4.5"),
        QStringLiteral("4.5.3.1"),
        QStringLiteral("v4.5.3"),
        QStringLiteral("4.5.3-beta"),
        QStringLiteral("4.5.3+build"),
        QStringLiteral(" 4.5.3"),
        QStringLiteral("4.5.3 "),
        QStringLiteral("4.5.3\n"),
        QStringLiteral("4..3"),
        QStringLiteral(".4.5"),
        QStringLiteral("4.5."),
        QStringLiteral("a.b.c"),
        QStringLiteral("-1.2.3"),
        QStringLiteral("+1.2.3"),
        QStringLiteral("1.-2.3"),
        QStringLiteral("1234567890.0.0"),           // would not fit
        QString::fromUtf8("\xef\xbc\x94.5.3"),       // full-width digit four
        QString::fromUtf8("\xd9\xa4.5.3"),           // Arabic-Indic digit four
        QStringLiteral("dev"),
        QStringLiteral("4.5.3-dev+abc123"),
    };
    for (const QString& s : bad) {
        int a = 7, b = 7, c = 7;
        if (parse_version(s, a, b, c))
            QFAIL(qPrintable(QStringLiteral("'%1' was accepted").arg(s)));
        QCOMPARE(a + b + c, 21); // outputs untouched on failure
    }
}

void TstUpdateManifest::newer_version_comparison() {
    QVERIFY(is_newer_version(QStringLiteral("4.5.3"), QStringLiteral("4.5.4")));
    QVERIFY(is_newer_version(QStringLiteral("4.5.3"), QStringLiteral("4.6.0")));
    QVERIFY(is_newer_version(QStringLiteral("4.5.3"), QStringLiteral("5.0.0")));
    QVERIFY(is_newer_version(QStringLiteral("4.5.9"), QStringLiteral("4.5.10"))); // numeric, not text
    QVERIFY(is_newer_version(QStringLiteral("4.9.9"), QStringLiteral("4.10.0")));
    QVERIFY(is_newer_version(QStringLiteral("0.0.0"), QStringLiteral("0.0.1")));

    QVERIFY(!is_newer_version(QStringLiteral("4.5.3"), QStringLiteral("4.5.3"))); // same
    QVERIFY(!is_newer_version(QStringLiteral("4.5.10"), QStringLiteral("4.5.9")));
    QVERIFY(!is_newer_version(QStringLiteral("5.0.0"), QStringLiteral("4.99.99")));
    QVERIFY(!is_newer_version(QStringLiteral("4.6.0"), QStringLiteral("4.5.99")));

    // Malformed on either side is never "newer".
    QVERIFY(!is_newer_version(QStringLiteral("dev"), QStringLiteral("9.9.9")));
    QVERIFY(!is_newer_version(QStringLiteral("4.5.3-dev"), QStringLiteral("9.9.9")));
    QVERIFY(!is_newer_version(QString(), QStringLiteral("9.9.9")));
    QVERIFY(!is_newer_version(QStringLiteral("4.5.3"), QStringLiteral("v9.9.9")));
    QVERIFY(!is_newer_version(QStringLiteral("4.5.3"), QStringLiteral("9.9")));
    QVERIFY(!is_newer_version(QStringLiteral("4.5.3"), QString()));
}

// ── parse_manifest ──────────────────────────────────────────────────────────

void TstUpdateManifest::well_formed_manifest_is_parsed() {
    const ManifestResult r = parse_deb(base_manifest());
    QVERIFY2(r.info.has_value(), qPrintable(r.error));
    QVERIFY(r.error.isEmpty());
    QCOMPARE(r.info->version, QStringLiteral("4.5.4"));
    QCOMPARE(r.info->release_url, QStringLiteral("https://github.com/o/r/releases/tag/v4.5.4"));
    QCOMPARE(r.info->notes, QStringLiteral("Faster charts."));
    QVERIFY(r.info->has_asset());
    QCOMPARE(r.info->asset.url, QStringLiteral("https://github.com/o/r/releases/download/v4.5.4/ft_4.5.4_amd64.deb"));
    QCOMPARE(r.info->asset.sha256, kSha);
    QCOMPARE(r.info->asset.size, qint64(1234));
}

void TstUpdateManifest::missing_asset_key_gives_info_without_asset() {
    const ManifestResult r = parse_manifest(bytes(base_manifest()), QStringLiteral("macos-arm64-dmg"));
    QVERIFY2(r.info.has_value(), qPrintable(r.error));
    QCOMPARE(r.info->version, QStringLiteral("4.5.4"));
    QVERIFY(!r.info->has_asset());
    QVERIFY(r.info->asset.sha256.isEmpty());

    // No "assets" at all, or "assets" of the wrong type: still a manifest.
    QJsonObject no_assets = base_manifest();
    no_assets.remove(QStringLiteral("assets"));
    QVERIFY(parse_deb(no_assets).info.has_value());
    QVERIFY(!parse_deb(no_assets).info->has_asset());
    QJsonObject wrong_assets = base_manifest();
    wrong_assets.insert(QStringLiteral("assets"), QJsonArray{1, 2});
    QVERIFY(parse_deb(wrong_assets).info.has_value());
    QVERIFY(!parse_deb(wrong_assets).info->has_asset());

    // An entry that is not an object.
    const ManifestResult str = parse_deb(with_asset(QStringLiteral("https://example.com/x.deb")));
    QVERIFY(str.info.has_value());
    QVERIFY(!str.info->has_asset());
}

void TstUpdateManifest::empty_asset_key_picks_nothing() {
    const ManifestResult r = parse_manifest(bytes(base_manifest()), QString());
    QVERIFY(r.info.has_value());
    QVERIFY(!r.info->has_asset());
}

void TstUpdateManifest::http_asset_url_is_rejected() {
    const QJsonObject root = with_asset(asset_entry(QStringLiteral("http://github.com/o/r/x.deb")));
    QVERIFY(parse_deb(root).info.has_value());
    QVERIFY(!parse_deb(root).info->has_asset());
    // Not even when loopback http is allowed: the host is not this machine.
    QVERIFY(!parse_deb(root, true).info->has_asset());
    QVERIFY(!parse_deb(with_asset(asset_entry(QStringLiteral("http://127.0.0.1.example.com/x.deb"))), true)
                 .info->has_asset());
    QVERIFY(!parse_deb(with_asset(asset_entry(QStringLiteral("http://localhost.example.com/x.deb"))), true)
                 .info->has_asset());
    QVERIFY(!parse_deb(with_asset(asset_entry(QStringLiteral("http://10.0.0.1/x.deb"))), true).info->has_asset());
}

void TstUpdateManifest::loopback_http_only_when_allowed() {
    const QStringList loopback = {
        QStringLiteral("http://127.0.0.1:8080/ft.deb"),
        QStringLiteral("http://localhost:8080/ft.deb"),
        QStringLiteral("http://LOCALHOST/ft.deb"),
        QStringLiteral("http://[::1]:9000/ft.deb"),
    };
    for (const QString& url : loopback) {
        const QJsonObject root = with_asset(asset_entry(url));
        if (parse_deb(root, false).info->has_asset())
            QFAIL(qPrintable(QStringLiteral("%1 accepted without allow_loopback_http").arg(url)));
        const ManifestResult allowed = parse_deb(root, true);
        if (!allowed.info->has_asset())
            QFAIL(qPrintable(QStringLiteral("%1 rejected with allow_loopback_http").arg(url)));
        QCOMPARE(allowed.info->asset.url, url);
    }
    // https to loopback is plain https: fine either way.
    QVERIFY(parse_deb(with_asset(asset_entry(QStringLiteral("https://127.0.0.1/ft.deb")))).info->has_asset());
}

void TstUpdateManifest::other_url_schemes_are_rejected() {
    const QStringList bad = {
        QStringLiteral("file:///etc/passwd"),
        QStringLiteral("ftp://example.com/x.deb"),
        QStringLiteral("javascript:alert(1)"),
        QStringLiteral("https://"),
        QStringLiteral("//example.com/x.deb"),
        QStringLiteral("/x.deb"),
        QString(),
    };
    for (const QString& url : bad) {
        if (parse_deb(with_asset(asset_entry(url)), true).info->has_asset())
            QFAIL(qPrintable(QStringLiteral("'%1' was accepted").arg(url)));
    }
    QJsonObject no_url = asset_entry(QStringLiteral("https://example.com/x.deb"));
    no_url.remove(QStringLiteral("url"));
    QVERIFY(!parse_deb(with_asset(no_url)).info->has_asset());
    QJsonObject numeric_url = asset_entry(QString());
    numeric_url.insert(QStringLiteral("url"), 42);
    QVERIFY(!parse_deb(with_asset(numeric_url)).info->has_asset());
}

void TstUpdateManifest::bad_sha256_is_rejected() {
    const QString url = QStringLiteral("https://example.com/x.deb");
    const QStringList bad = {
        QString(),
        kSha.left(63),
        kSha + QLatin1Char('a'),
        QString(63, QLatin1Char('a')) + QLatin1Char('g'),
        QString(64, QLatin1Char(' ')),
        QString(62, QLatin1Char('a')) + QStringLiteral("\xe9\xe9"),
        QStringLiteral("sha256:") + kSha.left(57),
    };
    for (const QString& sha : bad) {
        if (parse_deb(with_asset(asset_entry(url, sha))).info->has_asset())
            QFAIL(qPrintable(QStringLiteral("sha256 '%1' was accepted").arg(sha)));
    }
    QJsonObject no_sha = asset_entry(url);
    no_sha.remove(QStringLiteral("sha256"));
    QVERIFY(!parse_deb(with_asset(no_sha)).info->has_asset());
}

void TstUpdateManifest::sha256_is_lower_cased() {
    const QString upper = QStringLiteral("ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789");
    const ManifestResult r = parse_deb(with_asset(asset_entry(QStringLiteral("https://e.com/x.deb"), upper)));
    QVERIFY(r.info->has_asset());
    QCOMPARE(r.info->asset.sha256, upper.toLower());
}

void TstUpdateManifest::size_is_optional_but_must_be_sane() {
    const QString url = QStringLiteral("https://example.com/x.deb");
    QJsonObject no_size = asset_entry(url);
    no_size.remove(QStringLiteral("size"));
    ManifestResult r = parse_deb(with_asset(no_size));
    QVERIFY(r.info->has_asset());
    QCOMPARE(r.info->asset.size, qint64(0));

    QJsonObject big = asset_entry(url);
    big.insert(QStringLiteral("size"), qint64(5) * 1024 * 1024 * 1024);
    r = parse_deb(with_asset(big));
    QVERIFY(r.info->has_asset());
    QCOMPARE(r.info->asset.size, qint64(5) * 1024 * 1024 * 1024);

    QJsonObject negative = asset_entry(url);
    negative.insert(QStringLiteral("size"), -1);
    QVERIFY(!parse_deb(with_asset(negative)).info->has_asset());
    QJsonObject fraction = asset_entry(url);
    fraction.insert(QStringLiteral("size"), 1.5);
    QVERIFY(!parse_deb(with_asset(fraction)).info->has_asset());
    QJsonObject text = asset_entry(url);
    text.insert(QStringLiteral("size"), QStringLiteral("1234"));
    QVERIFY(!parse_deb(with_asset(text)).info->has_asset());
}

void TstUpdateManifest::schema_version_must_be_3() {
    for (const QJsonValue& v : {QJsonValue(2), QJsonValue(4), QJsonValue(3.5), QJsonValue(QStringLiteral("3")),
                                QJsonValue(QJsonValue::Null)}) {
        QJsonObject root = base_manifest();
        root.insert(QStringLiteral("schema-version"), v);
        const ManifestResult r = parse_deb(root);
        QVERIFY(!r.info.has_value());
        QVERIFY(!r.error.isEmpty());
    }
    QJsonObject v2 = base_manifest();
    v2.insert(QStringLiteral("schema-version"), 2);
    QVERIFY2(parse_deb(v2).error.contains(QLatin1String("2")), qPrintable(parse_deb(v2).error));

    QJsonObject missing = base_manifest();
    missing.remove(QStringLiteral("schema-version"));
    QVERIFY(!parse_deb(missing).info.has_value());

    // The old (schema 2) layout is not read at all.
    const QByteArray old_layout = R"({"updates":{"linux-x64":{"latest-version":"9.9.9",)"
                                  R"("download-url":"https://e.com/x","sha256":"aa"}}})";
    QVERIFY(!parse_manifest(old_layout, QStringLiteral("linux-x64-deb")).info.has_value());
}

void TstUpdateManifest::version_must_be_strict() {
    for (const QJsonValue& v : {QJsonValue(QStringLiteral("4.5")), QJsonValue(QStringLiteral("v4.5.4")),
                                QJsonValue(QStringLiteral("4.5.4-beta")), QJsonValue(QStringLiteral("")),
                                QJsonValue(4.5), QJsonValue(QJsonValue::Null)}) {
        QJsonObject root = base_manifest();
        root.insert(QStringLiteral("version"), v);
        const ManifestResult r = parse_deb(root);
        QVERIFY(!r.info.has_value());
        QVERIFY(!r.error.isEmpty());
    }
    QJsonObject missing = base_manifest();
    missing.remove(QStringLiteral("version"));
    QVERIFY(!parse_deb(missing).info.has_value());
}

void TstUpdateManifest::insecure_release_url_is_dropped() {
    for (const QString& url : {QStringLiteral("http://github.com/o/r/releases/tag/v4.5.4"),
                               QStringLiteral("javascript:alert(1)"), QStringLiteral("file:///tmp/x")}) {
        QJsonObject root = base_manifest();
        root.insert(QStringLiteral("release-url"), url);
        const ManifestResult r = parse_deb(root, true);
        QVERIFY(r.info.has_value()); // dropped, not fatal
        QVERIFY(r.info->release_url.isEmpty());
        QVERIFY(r.info->has_asset());
    }
    QJsonObject loopback = base_manifest();
    loopback.insert(QStringLiteral("release-url"), QStringLiteral("http://127.0.0.1/notes"));
    QVERIFY(parse_deb(loopback, true).info->release_url.isEmpty()); // loopback rule is for assets only
}

void TstUpdateManifest::malformed_documents_are_rejected() {
    for (const QByteArray& doc : {QByteArray(), QByteArray("not json"), QByteArray("[1,2,3]"), QByteArray("\"x\""),
                                  QByteArray("{\"schema-version\":3,\"version\":\"4.5.4\""), QByteArray("null")}) {
        const ManifestResult r = parse_manifest(doc, QStringLiteral("linux-x64-deb"));
        QVERIFY(!r.info.has_value());
        QVERIFY(!r.error.isEmpty());
    }
}

QTEST_GUILESS_MAIN(TstUpdateManifest)
#include "tst_update_manifest.moc"
