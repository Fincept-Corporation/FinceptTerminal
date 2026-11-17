#include "services/updater/UpdateManifest.h"

// Qt Core only (plus the orlp/ed25519 C library): tests/tst_update_manifest.cpp
// links this file on its own, so no Logger, no Network, no Widgets here.

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QStringList>
#include <QUrl>

extern "C" {
#include "ed25519.h"
}

namespace fincept::services::updater {

namespace {

constexpr int kManifestSchemaVersion = 3;
constexpr int kEd25519PublicKeyBytes = 32;
constexpr int kEd25519SignatureBytes = 64;

bool manifest_is_hex_digit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/// Decode exactly `expected_bytes` bytes of hex. QByteArray::fromHex() silently
/// skips characters it does not understand, so the input is checked first: any
/// non-hex character or a wrong length gives an empty result.
QByteArray manifest_strict_hex(const QByteArray& hex, int expected_bytes) {
    if (hex.size() != qsizetype(expected_bytes) * 2)
        return {};
    for (const char c : hex) {
        if (!manifest_is_hex_digit(c))
            return {};
    }
    return QByteArray::fromHex(hex);
}

/// The signature file as published (base64, possibly wrapped by `base64`
/// without -w0, or 128 hex characters) → 64 raw bytes, or empty.
QByteArray manifest_decode_signature(const QByteArray& signature) {
    QByteArray compact;
    compact.reserve(signature.size());
    for (const char c : signature) {
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
            compact.append(c);
    }
    if (compact.size() == kEd25519SignatureBytes * 2) {
        const QByteArray raw = manifest_strict_hex(compact, kEd25519SignatureBytes);
        if (raw.size() == kEd25519SignatureBytes)
            return raw;
    }
    const auto decoded = QByteArray::fromBase64Encoding(compact, QByteArray::AbortOnBase64DecodingErrors);
    if (decoded.decodingStatus != QByteArray::Base64DecodingStatus::Ok)
        return {};
    if (decoded.decoded.size() != kEd25519SignatureBytes)
        return {};
    return decoded.decoded;
}

bool manifest_is_loopback_host(const QString& host) {
    return host == QLatin1String("127.0.0.1") || host == QLatin1String("::1") ||
           host.compare(QLatin1String("localhost"), Qt::CaseInsensitive) == 0;
}

/// https with a host — or, when allowed, plain http to this machine (local
/// test servers only).
bool manifest_is_acceptable_url(const QString& text, bool allow_loopback_http) {
    if (text.isEmpty())
        return false;
    const QUrl url(text);
    if (!url.isValid() || url.isRelative() || url.host().isEmpty())
        return false;
    const QString scheme = url.scheme().toLower();
    if (scheme == QLatin1String("https"))
        return true;
    return allow_loopback_http && scheme == QLatin1String("http") && manifest_is_loopback_host(url.host());
}

bool manifest_is_sha256_hex(const QString& s) {
    if (s.size() != 64)
        return false;
    for (const QChar c : s) {
        if (c.unicode() > 0x7f || !manifest_is_hex_digit(static_cast<char>(c.unicode())))
            return false;
    }
    return true;
}

bool manifest_parse_component(QStringView part, int& out) {
    if (part.isEmpty() || part.size() > 9) // 9 digits always fit an int
        return false;
    for (const QChar c : part) {
        if (c < QLatin1Char('0') || c > QLatin1Char('9'))
            return false;
    }
    bool ok = false;
    out = part.toInt(&ok);
    return ok;
}

} // namespace

// ── Signature ───────────────────────────────────────────────────────────────

bool verify_manifest_signature(const QByteArray& manifest, const QByteArray& signature,
                               const QString& public_key_hex) {
    if (manifest.isEmpty())
        return false;
    const QByteArray public_key = manifest_strict_hex(public_key_hex.trimmed().toLatin1(), kEd25519PublicKeyBytes);
    if (public_key.size() != kEd25519PublicKeyBytes)
        return false; // fail closed: no (valid) key means nothing verifies
    const QByteArray raw_signature = manifest_decode_signature(signature);
    if (raw_signature.size() != kEd25519SignatureBytes)
        return false;
    const int ok = ::ed25519_verify(reinterpret_cast<const unsigned char*>(raw_signature.constData()),
                                    reinterpret_cast<const unsigned char*>(manifest.constData()),
                                    static_cast<size_t>(manifest.size()),
                                    reinterpret_cast<const unsigned char*>(public_key.constData()));
    return ok == 1;
}

// ── Versions ────────────────────────────────────────────────────────────────

bool parse_version(const QString& s, int& major, int& minor, int& patch) {
    const QStringView view(s);
    const qsizetype first_dot = view.indexOf(QLatin1Char('.'));
    if (first_dot < 0)
        return false;
    const qsizetype second_dot = view.indexOf(QLatin1Char('.'), first_dot + 1);
    if (second_dot < 0 || view.indexOf(QLatin1Char('.'), second_dot + 1) >= 0)
        return false;
    int a = 0, b = 0, c = 0;
    if (!manifest_parse_component(view.first(first_dot), a) ||
        !manifest_parse_component(view.sliced(first_dot + 1, second_dot - first_dot - 1), b) ||
        !manifest_parse_component(view.sliced(second_dot + 1), c))
        return false;
    major = a;
    minor = b;
    patch = c;
    return true;
}

bool is_newer_version(const QString& local, const QString& remote) {
    int lmaj = 0, lmin = 0, lpat = 0;
    int rmaj = 0, rmin = 0, rpat = 0;
    if (!parse_version(local, lmaj, lmin, lpat) || !parse_version(remote, rmaj, rmin, rpat))
        return false;
    if (rmaj != lmaj)
        return rmaj > lmaj;
    if (rmin != lmin)
        return rmin > lmin;
    return rpat > lpat;
}

// ── Manifest ────────────────────────────────────────────────────────────────

ManifestResult parse_manifest(const QByteArray& verified_manifest, const QString& asset_key,
                              bool allow_loopback_http) {
    ManifestResult result;

    QJsonParseError parse_error{};
    const QJsonDocument doc = QJsonDocument::fromJson(verified_manifest, &parse_error);
    if (parse_error.error != QJsonParseError::NoError) {
        result.error = QStringLiteral("manifest is not valid JSON: %1 (at byte %2)")
                           .arg(parse_error.errorString())
                           .arg(parse_error.offset);
        return result;
    }
    if (!doc.isObject()) {
        result.error = QStringLiteral("manifest is not a JSON object");
        return result;
    }
    const QJsonObject root = doc.object();

    const QJsonValue schema = root.value(QLatin1String("schema-version"));
    if (!schema.isDouble()) {
        result.error = QStringLiteral("manifest has no numeric schema-version");
        return result;
    }
    if (schema.toInteger(-1) != kManifestSchemaVersion) {
        result.error = QStringLiteral("unsupported manifest schema-version %1 (this build reads %2)")
                           .arg(schema.toDouble())
                           .arg(kManifestSchemaVersion);
        return result;
    }

    const QJsonValue version_value = root.value(QLatin1String("version"));
    const QString version = version_value.toString();
    int major = 0, minor = 0, patch = 0;
    if (!version_value.isString() || !parse_version(version, major, minor, patch)) {
        result.error = QStringLiteral("manifest version '%1' is not of the form X.Y.Z").arg(version);
        return result;
    }

    UpdateInfo info;
    info.version = version;

    const QString release_url = root.value(QLatin1String("release-url")).toString().trimmed();
    if (manifest_is_acceptable_url(release_url, /*allow_loopback_http=*/false))
        info.release_url = release_url; // anything else is dropped, not fatal

    info.notes = root.value(QLatin1String("notes")).toString().trimmed();

    if (!asset_key.isEmpty()) {
        const QJsonValue entry_value = root.value(QLatin1String("assets")).toObject().value(asset_key);
        if (entry_value.isObject()) {
            const QJsonObject entry = entry_value.toObject();
            const QString url = entry.value(QLatin1String("url")).toString().trimmed();
            const QString sha256 = entry.value(QLatin1String("sha256")).toString().trimmed().toLower();
            const QJsonValue size_value = entry.value(QLatin1String("size"));

            // Optional; when present it must be a whole, non-negative number
            // (0 means "not stated", as in UpdateAsset).
            bool size_ok = true;
            qint64 size = 0;
            if (!size_value.isUndefined() && !size_value.isNull()) {
                size = size_value.isDouble() ? size_value.toInteger(-1) : -1;
                size_ok = size >= 0;
            }

            if (manifest_is_acceptable_url(url, allow_loopback_http) && manifest_is_sha256_hex(sha256) && size_ok) {
                info.asset.url = url;
                info.asset.sha256 = sha256;
                info.asset.size = size;
            }
        }
    }

    result.info = info;
    return result;
}

} // namespace fincept::services::updater
