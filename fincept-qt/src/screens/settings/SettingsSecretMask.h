#pragma once
// SettingsSecretMask.h — one "does this look like a secret?" policy shared by
// everything that renders stored settings/database values: the Settings →
// Storage SQL console (on screen) and the settings MCP tools (to an LLM).
//
// Secrets belong in SecureStorage. The plaintext tables still carry them in
// three situations: legacy installs that predate the encrypted store, the
// "SecureStorage unavailable — keeping the value in settings" fallback paths
// (LlmConfigRepository, McpServerRepository, NotificationsSection), and a few
// keys that never moved (telemetry.cloud_api_key). So any value read back from
// SQLite has to be treated as possibly secret before it is displayed.
//
// Header-only and free of widget dependencies on purpose; every symbol lives
// in its own namespace so unity builds cannot collide with it.

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

namespace fincept::settings_secret_mask {

/// How strongly a column / key name implies that its value is a secret.
///  - Strong: always hide a non-empty value ("password", "api_key", "webhook_url").
///  - Weak:   the bare word "token" — hide unless the value is numeric, because
///            oi_snapshots.token and similar columns hold instrument ids.
enum class SecretStrength { None = 0, Weak = 1, Strong = 2 };

/// Shown instead of a secret. Fixed width: it must not reveal the value's length.
inline QString placeholder() {
    return QStringLiteral("••••••••");
}

/// "Telemetry.Cloud-API-Key" → "telemetry_cloud_api_key".
inline QString normalise_name(const QString& name) {
    QString out;
    out.reserve(name.size());
    for (const QChar c : name)
        out.append(c.isLetterOrNumber() ? c.toLower() : QLatin1Char('_'));
    return out;
}

inline SecretStrength name_strength(const QString& raw) {
    if (raw.isEmpty())
        return SecretStrength::None;
    const QString n = normalise_name(raw);
    const QString padded = QLatin1Char('_') + n + QLatin1Char('_');

    // Distinctive fragments: a match anywhere is a strong signal ("smtppassword",
    // "client_secret", "api_secret").
    static const QLatin1String kAnywhere[] = {
        QLatin1String("password"),    QLatin1String("passwd"),     QLatin1String("passphrase"),
        QLatin1String("secret"),      QLatin1String("credential"), QLatin1String("mnemonic"),
        QLatin1String("bearer"),      QLatin1String("webhook"),    QLatin1String("ciphertext"),
        QLatin1String("apikey"),      QLatin1String("privatekey"), QLatin1String("accesstoken"),
        QLatin1String("authtoken"),   QLatin1String("refreshtoken"), QLatin1String("sessiontoken"),
    };
    for (const auto& f : kAnywhere) {
        if (padded.contains(f))
            return SecretStrength::Strong;
    }

    // Word-bounded fragments: only as whole "_word_" runs, so "whatsapp_key" does
    // not read as "app_key" and "spinner" does not read as "pin".
    static const QLatin1String kBounded[] = {
        QLatin1String("_api_key_"),        QLatin1String("_private_key_"),  QLatin1String("_access_key_"),
        QLatin1String("_signing_key_"),    QLatin1String("_encryption_key_"), QLatin1String("_master_key_"),
        QLatin1String("_client_key_"),     QLatin1String("_app_key_"),      QLatin1String("_user_key_"),
        QLatin1String("_routing_key_"),    QLatin1String("_auth_token_"),   QLatin1String("_access_token_"),
        QLatin1String("_refresh_token_"),  QLatin1String("_session_token_"), QLatin1String("_id_token_"),
        QLatin1String("_seed_phrase_"),    QLatin1String("_otp_"),          QLatin1String("_totp_"),
        QLatin1String("_cookie_"),         QLatin1String("_pin_"),          QLatin1String("_pass_"),
        QLatin1String("_pwd_"),            QLatin1String("_passcode_"),
    };
    for (const auto& f : kBounded) {
        if (padded.contains(f))
            return SecretStrength::Strong;
    }

    // Whole-name columns that carry secrets in this schema: mcp_servers.env
    // (environment variables, routinely API tokens) and data_mappings.headers
    // ("Authorization: Bearer …").
    if (n == QLatin1String("env") || n == QLatin1String("headers"))
        return SecretStrength::Strong;

    // "token" as a word — but instrument_token / exchange_token / broker_token
    // are market identifiers, and "tokens" (plural) is always a count.
    const QStringList words = n.split(QLatin1Char('_'), Qt::SkipEmptyParts);
    for (int i = 0; i < words.size(); ++i) {
        if (words.at(i) != QLatin1String("token"))
            continue;
        const QString prev = i > 0 ? words.at(i - 1) : QString();
        if (prev == QLatin1String("instrument") || prev == QLatin1String("exchange") ||
            prev == QLatin1String("broker"))
            continue;
        return SecretStrength::Weak;
    }
    return SecretStrength::None;
}

inline bool is_secret_name(const QString& name) {
    return name_strength(name) != SecretStrength::None;
}

/// Whether `value`, stored under a name of the given strength, must be hidden.
inline bool should_hide(SecretStrength s, const QString& value) {
    if (s == SecretStrength::None || value.trimmed().isEmpty())
        return false;
    if (s == SecretStrength::Weak) {
        bool numeric = false;
        value.trimmed().toLongLong(&numeric);
        return !numeric;
    }
    return true;
}

/// Replace well-known credential formats inside free text (a key pasted into a
/// note or a chat message). Conservative on purpose — only formats that are
/// unambiguous. Returns the number of replacements made.
inline int mask_token_patterns(QString& text) {
    if (text.size() < 16)
        return 0;
    static const QRegularExpression kPatterns(
        QStringLiteral("(?:\\bsk-(?:ant-)?[A-Za-z0-9_\\-]{20,}"   // OpenAI / Anthropic
                       "|\\bfk_user_[A-Za-z0-9_\\-]{20,}"         // Fincept API key
                       "|\\bgh[pousr]_[A-Za-z0-9]{30,}"            // GitHub
                       "|\\bxox[abprs]-[A-Za-z0-9\\-]{10,}"        // Slack
                       "|\\bAKIA[0-9A-Z]{16}\\b"                   // AWS access key id
                       "|\\bAIza[0-9A-Za-z_\\-]{35}"               // Google API key
                       "|\\bBearer\\s+[A-Za-z0-9._~+/\\-]{20,}=*"  // HTTP bearer credential
                       "|-----BEGIN [A-Z ]*PRIVATE KEY-----[\\s\\S]*?(?:-----END [A-Z ]*PRIVATE KEY-----|$))"));
    int count = 0;
    QString out;
    qsizetype last = 0;
    auto it = kPatterns.globalMatch(text);
    while (it.hasNext()) {
        const auto m = it.next();
        out += QStringView(text).mid(last, m.capturedStart() - last);
        out += placeholder();
        last = m.capturedEnd();
        ++count;
    }
    if (count > 0) {
        out += QStringView(text).mid(last);
        text = out;
    }
    return count;
}

/// Hide the values of secret-named keys anywhere inside a JSON document
/// ({"env":{"GITHUB_TOKEN":"…"}}, {"password":"…"}). Returns true if changed.
inline bool mask_json(QJsonValue& v, int depth = 0) {
    if (depth > 32)
        return false;
    bool changed = false;
    if (v.isObject()) {
        QJsonObject o = v.toObject();
        for (auto it = o.begin(); it != o.end(); ++it) {
            QJsonValue child = it.value();
            const SecretStrength s = name_strength(it.key());
            const QString as_text = child.isString()   ? child.toString()
                                    : child.isDouble() ? QString::number(child.toDouble(), 'g', 17)
                                                       : QString();
            if (s != SecretStrength::None && (child.isString() || child.isDouble()) && should_hide(s, as_text)) {
                it.value() = placeholder();
                changed = true;
                continue;
            }
            if (child.isObject() || child.isArray()) {
                if (mask_json(child, depth + 1)) {
                    it.value() = child;
                    changed = true;
                }
            } else if (child.isString()) {
                QString str = child.toString();
                if (mask_token_patterns(str) > 0) {
                    it.value() = str;
                    changed = true;
                }
            }
        }
        if (changed)
            v = o;
    } else if (v.isArray()) {
        QJsonArray a = v.toArray();
        for (qsizetype i = 0; i < a.size(); ++i) {
            QJsonValue child = a.at(i);
            if (child.isObject() || child.isArray()) {
                if (mask_json(child, depth + 1)) {
                    a.replace(i, child);
                    changed = true;
                }
            } else if (child.isString()) {
                QString str = child.toString();
                if (mask_token_patterns(str) > 0) {
                    a.replace(i, str);
                    changed = true;
                }
            }
        }
        if (changed)
            v = a;
    }
    return changed;
}

/// Free-form stored value → displayable text: secret keys inside JSON blobs are
/// hidden and well-known credential formats are replaced. `changed` (optional)
/// reports whether anything was masked.
inline QString redact_value(const QString& value, bool* changed = nullptr) {
    if (changed)
        *changed = false;
    QString out = value;
    const QString trimmed = value.trimmed();
    if ((trimmed.startsWith(QLatin1Char('{')) || trimmed.startsWith(QLatin1Char('['))) &&
        trimmed.size() < 4 * 1024 * 1024) {
        QJsonParseError err{};
        const QJsonDocument doc = QJsonDocument::fromJson(trimmed.toUtf8(), &err);
        if (err.error == QJsonParseError::NoError && !doc.isNull()) {
            QJsonValue root = doc.isObject() ? QJsonValue(doc.object()) : QJsonValue(doc.array());
            if (mask_json(root)) {
                out = QString::fromUtf8(root.isObject() ? QJsonDocument(root.toObject()).toJson(QJsonDocument::Compact)
                                                        : QJsonDocument(root.toArray()).toJson(QJsonDocument::Compact));
                if (changed)
                    *changed = true;
                return out;
            }
        }
    }
    if (mask_token_patterns(out) > 0 && changed)
        *changed = true;
    return out;
}

/// A stored key/value pair as it may be shown: the whole value is withheld when
/// the key names a secret; otherwise secrets embedded in the value are masked.
inline QString display_value_for_key(const QString& key, const QString& value, bool* masked = nullptr) {
    if (should_hide(name_strength(key), value)) {
        if (masked)
            *masked = true;
        return placeholder();
    }
    return redact_value(value, masked);
}

} // namespace fincept::settings_secret_mask
