#include "services/prediction/PredictionCredentialStore.h"

#include "core/logging/Logger.h"
#include "storage/secure/SecureStorage.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace fincept::services::prediction {

namespace {

constexpr const char* kPolymarketKey = "prediction/polymarket/credentials";
constexpr const char* kKalshiKey = "prediction/kalshi/credentials";

QString json_string(const QJsonObject& obj, const char* key) {
    return obj.value(QLatin1String(key)).toString();
}

QJsonObject to_json(const PolymarketCredentials& c) {
    QJsonObject o;
    o.insert("private_key", c.private_key);
    o.insert("funder_address", c.funder_address);
    o.insert("signature_type", c.signature_type);
    o.insert("api_key", c.api_key);
    o.insert("api_secret", c.api_secret);
    o.insert("api_passphrase", c.api_passphrase);
    return o;
}

PolymarketCredentials from_json_polymarket(const QJsonObject& o) {
    PolymarketCredentials c;
    c.private_key = json_string(o, "private_key");
    c.funder_address = json_string(o, "funder_address");
    c.signature_type = o.value("signature_type").toInt(1);
    c.api_key = json_string(o, "api_key");
    c.api_secret = json_string(o, "api_secret");
    c.api_passphrase = json_string(o, "api_passphrase");
    return c;
}

QJsonObject to_json(const kalshi_ns::KalshiCredentials& c) {
    QJsonObject o;
    o.insert("api_key_id", c.api_key_id);
    o.insert("private_key_pem", c.private_key_pem);
    o.insert("use_demo", c.use_demo);
    return o;
}

kalshi_ns::KalshiCredentials from_json_kalshi(const QJsonObject& o) {
    kalshi_ns::KalshiCredentials c;
    c.api_key_id = json_string(o, "api_key_id");
    c.private_key_pem = json_string(o, "private_key_pem");
    c.use_demo = o.value("use_demo").toBool(false);
    return c;
}

QString encode(const QJsonObject& obj) {
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

std::optional<QJsonObject> decode(const QString& s) {
    if (s.isEmpty()) return std::nullopt;
    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(s.toUtf8(), &err);
    if (doc.isNull() || !doc.isObject()) return std::nullopt;
    return doc.object();
}

} // namespace

// ── Polymarket ──────────────────────────────────────────────────────────────

std::optional<PolymarketCredentials> PredictionCredentialStore::load_polymarket() {
    auto r = fincept::SecureStorage::instance().retrieve(QString::fromLatin1(kPolymarketKey));
    if (r.is_err()) return std::nullopt;
    auto obj = decode(r.value());
    if (!obj) return std::nullopt;
    auto creds = from_json_polymarket(*obj);
    if (!creds.is_valid()) return std::nullopt;
    return creds;
}

bool PredictionCredentialStore::save_polymarket(const PolymarketCredentials& creds) {
    if (!creds.is_valid()) return false;
    auto r = fincept::SecureStorage::instance().store(QString::fromLatin1(kPolymarketKey),
                                                      encode(to_json(creds)));
    if (r.is_err()) {
        LOG_ERROR("PredictionCreds",
                  QStringLiteral("SecureStorage.store(polymarket) failed: ") +
                      QString::fromStdString(r.error()));
        return false;
    }
    return true;
}

bool PredictionCredentialStore::clear_polymarket() {
    auto r = fincept::SecureStorage::instance().remove(QString::fromLatin1(kPolymarketKey));
    return r.is_ok();
}

// ── Kalshi ──────────────────────────────────────────────────────────────────

std::optional<kalshi_ns::KalshiCredentials> PredictionCredentialStore::load_kalshi() {
    auto r = fincept::SecureStorage::instance().retrieve(QString::fromLatin1(kKalshiKey));
    if (r.is_err()) return std::nullopt;
    auto obj = decode(r.value());
    if (!obj) return std::nullopt;
    auto creds = from_json_kalshi(*obj);
    if (!creds.is_valid()) return std::nullopt;
    return creds;
}

bool PredictionCredentialStore::save_kalshi(const kalshi_ns::KalshiCredentials& creds) {
    if (!creds.is_valid()) return false;
    auto r = fincept::SecureStorage::instance().store(QString::fromLatin1(kKalshiKey),
                                                      encode(to_json(creds)));
    if (r.is_err()) {
        LOG_ERROR("PredictionCreds",
                  QStringLiteral("SecureStorage.store(kalshi) failed: ") +
                      QString::fromStdString(r.error()));
        return false;
    }
    return true;
}

bool PredictionCredentialStore::clear_kalshi() {
    auto r = fincept::SecureStorage::instance().remove(QString::fromLatin1(kKalshiKey));
    return r.is_ok();
}

} // namespace fincept::services::prediction
