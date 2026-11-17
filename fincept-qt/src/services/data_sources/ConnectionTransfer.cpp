// ConnectionTransfer.cpp — see ConnectionTransfer.h.

#include "services/data_sources/ConnectionTransfer.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>

#include <openssl/evp.h>
#include <openssl/rand.h>

namespace fincept::services::datasources {

namespace {

constexpr const char* kFormatName = "fincept.connections";
constexpr int kFormatVersion = 2;
constexpr int kSaltLen = 16;
constexpr int kIvLen = 12;
constexpr int kTagLen = 16;
constexpr int kKeyLen = 32;
constexpr int kMinIterations = 100000;
constexpr const char* kAad = "fincept.connections.v2";
constexpr const char* kCheckText = "fincept-connections-check";

QString ct_tr(const char* text) {
    return QCoreApplication::translate("ConnectionTransfer", text);
}

QByteArray ct_random(int n) {
    QByteArray b(n, '\0');
    if (RAND_bytes(reinterpret_cast<unsigned char*>(b.data()), n) != 1)
        return {};
    return b;
}

QString ct_b64(const QByteArray& b) {
    return QString::fromLatin1(b.toBase64());
}

QByteArray ct_unb64(const QJsonValue& v) {
    return QByteArray::fromBase64(v.toString().toLatin1());
}

TransferConnection ct_connection_from_json(const QJsonObject& o) {
    TransferConnection c;
    c.id = o.value("id").toString();
    c.alias = o.value("alias").toString();
    c.display_name = o.value("display_name").toString();
    c.description = o.value("description").toString();
    c.type = o.value("type").toString();
    c.provider = o.value("provider").toString().trimmed();
    c.category = o.value("category").toString();
    c.tags = o.value("tags").toString();
    c.enabled = o.value("enabled").toBool(false);
    c.config = o.value("config").toObject();
    return c;
}

} // namespace

bool is_secret_config_key(const QString& key) {
    const QString k = key.toLower();
    for (const char* frag : {"password", "passwd", "secret", "token", "apikey", "api_key", "privatekey", "private_key",
                             "accesskey", "access_key", "accountkey", "account_key", "applicationkey", "appkey",
                             "app_key", "credential", "connectionstring"}) {
        if (k.contains(QLatin1String(frag)))
            return true;
    }
    return k == QLatin1String("key") || k == QLatin1String("headers");
}

QByteArray transfer_derive_key(const QString& passphrase, const QByteArray& salt, int iterations) {
    const QByteArray pass = passphrase.toUtf8();
    QByteArray key(kKeyLen, '\0');
    if (PKCS5_PBKDF2_HMAC(pass.constData(), int(pass.size()), reinterpret_cast<const unsigned char*>(salt.constData()),
                          int(salt.size()), iterations, EVP_sha256(), kKeyLen,
                          reinterpret_cast<unsigned char*>(key.data())) != 1)
        return {};
    return key;
}

QJsonObject transfer_encrypt(const QByteArray& key, const QByteArray& plain) {
    if (key.size() != kKeyLen)
        return {};
    const QByteArray iv = ct_random(kIvLen);
    if (iv.isEmpty())
        return {};
    QByteArray out(plain.size() + 16, '\0');
    QByteArray tag(kTagLen, '\0');
    bool ok = false;
    int written = 0;
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (ctx) {
        int len = 0;
        ok = EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1 &&
             EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kIvLen, nullptr) == 1 &&
             EVP_EncryptInit_ex(ctx, nullptr, nullptr, reinterpret_cast<const unsigned char*>(key.constData()),
                                reinterpret_cast<const unsigned char*>(iv.constData())) == 1 &&
             EVP_EncryptUpdate(ctx, nullptr, &len, reinterpret_cast<const unsigned char*>(kAad), int(qstrlen(kAad))) == 1 &&
             EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char*>(out.data()), &len,
                               reinterpret_cast<const unsigned char*>(plain.constData()), int(plain.size())) == 1;
        written = len;
        if (ok) {
            ok = EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(out.data()) + written, &len) == 1;
            written += len;
        }
        if (ok)
            ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, kTagLen, tag.data()) == 1;
        EVP_CIPHER_CTX_free(ctx);
    }
    if (!ok)
        return {};
    out.truncate(written);
    return QJsonObject{{"iv", ct_b64(iv)}, {"tag", ct_b64(tag)}, {"data", ct_b64(out)}};
}

bool transfer_decrypt(const QByteArray& key, const QJsonObject& box, QByteArray* plain) {
    const QByteArray iv = ct_unb64(box.value("iv"));
    QByteArray tag = ct_unb64(box.value("tag"));
    const QByteArray data = ct_unb64(box.value("data"));
    if (key.size() != kKeyLen || iv.size() != kIvLen || tag.size() != kTagLen)
        return false;
    QByteArray out(data.size() + 16, '\0');
    bool ok = false;
    int written = 0;
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (ctx) {
        int len = 0;
        ok = EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1 &&
             EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kIvLen, nullptr) == 1 &&
             EVP_DecryptInit_ex(ctx, nullptr, nullptr, reinterpret_cast<const unsigned char*>(key.constData()),
                                reinterpret_cast<const unsigned char*>(iv.constData())) == 1 &&
             EVP_DecryptUpdate(ctx, nullptr, &len, reinterpret_cast<const unsigned char*>(kAad), int(qstrlen(kAad))) == 1 &&
             EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char*>(out.data()), &len,
                               reinterpret_cast<const unsigned char*>(data.constData()), int(data.size())) == 1;
        written = len;
        // The tag must be set before EVP_DecryptFinal_ex, which verifies it.
        if (ok)
            ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, kTagLen, tag.data()) == 1;
        if (ok) {
            ok = EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(out.data()) + written, &len) == 1;
            written += len;
        }
        EVP_CIPHER_CTX_free(ctx);
    }
    if (!ok)
        return false;
    out.truncate(written);
    *plain = out;
    return true;
}

ExportResult export_connections_document(const QVector<TransferConnection>& connections, const ExportOptions& options) {
    ExportResult res;
    QByteArray key;
    QByteArray salt;
    const int iterations = qMax(options.iterations, kMinIterations);
    if (options.include_secrets) {
        if (options.passphrase.size() < 8) {
            res.error = ct_tr("The passphrase must have at least 8 characters");
            return res;
        }
        salt = ct_random(kSaltLen);
        key = salt.isEmpty() ? QByteArray() : transfer_derive_key(options.passphrase, salt, iterations);
        if (key.isEmpty()) {
            res.error = ct_tr("The encryption library failed; nothing was exported");
            return res;
        }
    }

    QJsonArray arr;
    for (const TransferConnection& c : connections) {
        QJsonObject cfg = c.config;
        QJsonObject secrets;
        QStringList secret_fields;
        for (auto it = c.config.constBegin(); it != c.config.constEnd(); ++it) {
            const bool secret = c.secret_fields.contains(it.key()) || is_secret_config_key(it.key());
            if (!secret || !it.value().isString() || it.value().toString().isEmpty())
                continue;
            secrets.insert(it.key(), it.value());
            secret_fields << it.key();
            cfg.insert(it.key(), QString());
        }
        QJsonObject o{{"id", c.id},
                      {"alias", c.alias},
                      {"display_name", c.display_name},
                      {"description", c.description},
                      {"type", c.type},
                      {"provider", c.provider},
                      {"category", c.category},
                      {"tags", c.tags},
                      {"enabled", c.enabled},
                      {"config", cfg},
                      {"secret_fields", QJsonArray::fromStringList(secret_fields)}};
        if (!secrets.isEmpty()) {
            if (options.include_secrets) {
                const QJsonObject box =
                    transfer_encrypt(key, QJsonDocument(secrets).toJson(QJsonDocument::Compact));
                if (box.isEmpty()) {
                    res.error = ct_tr("The encryption library failed; nothing was exported");
                    return res;
                }
                o.insert("secrets", box);
                res.secrets_encrypted += int(secrets.size());
            } else {
                res.secrets_excluded += int(secrets.size());
            }
        }
        arr.append(o);
    }

    QJsonObject doc{{"format", QString::fromLatin1(kFormatName)},
                    {"version", kFormatVersion},
                    {"exported_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
                    {"secrets", options.include_secrets ? QStringLiteral("encrypted") : QStringLiteral("excluded")},
                    {"connections", arr}};
    if (options.include_secrets) {
        doc.insert("kdf", QJsonObject{{"name", QStringLiteral("PBKDF2-HMAC-SHA256")},
                                      {"iterations", iterations},
                                      {"salt", ct_b64(salt)}});
        doc.insert("cipher", QStringLiteral("AES-256-GCM"));
        const QJsonObject check = transfer_encrypt(key, QByteArray(kCheckText));
        if (check.isEmpty()) {
            res.error = ct_tr("The encryption library failed; nothing was exported");
            return res;
        }
        doc.insert("check", check);
    }
    res.json = QJsonDocument(doc).toJson(QJsonDocument::Indented);
    res.connections = int(arr.size());
    res.ok = true;
    return res;
}

ImportResult read_connections_document(const QByteArray& bytes, const QString& passphrase, bool skip_secrets) {
    ImportResult res;
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &pe);
    if (pe.error != QJsonParseError::NoError) {
        res.error = ct_tr("The file is not valid JSON: %1 (at offset %2)").arg(pe.errorString()).arg(pe.offset);
        return res;
    }

    // Version 1: a plain array (earlier exports and the connector template).
    if (doc.isArray()) {
        res.format_version = 1;
        for (const QJsonValue& v : doc.array()) {
            const QJsonObject o = v.toObject();
            ImportedConnection ic;
            ic.connection = ct_connection_from_json(o);
            if (ic.connection.provider.isEmpty()) {
                ++res.skipped;
                continue;
            }
            for (auto it = ic.connection.config.constBegin(); it != ic.connection.config.constEnd(); ++it) {
                if (!is_secret_config_key(it.key()) || !it.value().isString())
                    continue;
                if (it.value().toString().isEmpty()) {
                    if (o.value("secrets_redacted").toBool(false))
                        ic.missing_secrets << it.key();
                } else {
                    ++res.secrets_restored;
                }
            }
            res.secrets_missing += int(ic.missing_secrets.size());
            res.connections.append(ic);
        }
        res.ok = true;
        return res;
    }

    const QJsonObject root = doc.object();
    if (!doc.isObject() || root.value("format").toString() != QLatin1String(kFormatName)) {
        res.error = ct_tr("This is not a Fincept connections file");
        return res;
    }
    res.format_version = root.value("version").toInt();
    if (res.format_version > kFormatVersion) {
        res.error = ct_tr("This file was written by a newer version of the terminal (format %1)").arg(res.format_version);
        return res;
    }
    const QJsonArray arr = root.value("connections").toArray();
    for (const QJsonValue& v : arr)
        if (v.toObject().contains("secrets"))
            res.has_encrypted_secrets = true;

    QByteArray key;
    if (res.has_encrypted_secrets && !skip_secrets) {
        if (passphrase.isEmpty()) {
            res.needs_passphrase = true;
            res.error = ct_tr("This file holds encrypted secrets: enter the passphrase it was exported with");
            return res;
        }
        const QJsonObject kdf = root.value("kdf").toObject();
        const QByteArray salt = ct_unb64(kdf.value("salt"));
        const int iterations = kdf.value("iterations").toInt();
        if (salt.size() != kSaltLen || iterations < kMinIterations || iterations > 10000000 ||
            kdf.value("name").toString() != QLatin1String("PBKDF2-HMAC-SHA256")) {
            res.error = ct_tr("The file's encryption header is damaged");
            return res;
        }
        key = transfer_derive_key(passphrase, salt, iterations);
        QByteArray check;
        if (key.isEmpty() || !transfer_decrypt(key, root.value("check").toObject(), &check) ||
            check != QByteArray(kCheckText)) {
            res.wrong_passphrase = true;
            res.error = ct_tr("Wrong passphrase — nothing was imported");
            return res;
        }
    }

    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        ImportedConnection ic;
        ic.connection = ct_connection_from_json(o);
        if (ic.connection.provider.isEmpty()) {
            ++res.skipped;
            continue;
        }
        const QStringList fields = [&o] {
            QStringList out;
            for (const QJsonValue& f : o.value("secret_fields").toArray())
                out << f.toString();
            return out;
        }();
        if (o.contains("secrets") && !key.isEmpty()) {
            QByteArray plain;
            if (!transfer_decrypt(key, o.value("secrets").toObject(), &plain)) {
                res.error = ct_tr("The secrets of \"%1\" are damaged — nothing was imported").arg(ic.connection.display_name);
                res.connections.clear();
                return res;
            }
            const QJsonObject secrets = QJsonDocument::fromJson(plain).object();
            for (auto it = secrets.constBegin(); it != secrets.constEnd(); ++it) {
                ic.connection.config.insert(it.key(), it.value());
                ++res.secrets_restored;
            }
            for (const QString& f : fields)
                if (!secrets.contains(f))
                    ic.missing_secrets << f;
        } else {
            ic.missing_secrets = fields;
        }
        res.secrets_missing += int(ic.missing_secrets.size());
        res.connections.append(ic);
    }
    res.ok = true;
    return res;
}

} // namespace fincept::services::datasources
