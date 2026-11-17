// tst_connection_transfer — the connections export / import file
// (services/data_sources/ConnectionTransfer): secrets left out by default, encrypted with a
// passphrase when included, wrong passphrases and damaged data rejected, version-1 files read.

#include "services/data_sources/ConnectionTransfer.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

using namespace fincept::services::datasources;

namespace {

TransferConnection pg_connection() {
    TransferConnection c;
    c.id = QStringLiteral("id-1");
    c.display_name = QStringLiteral("Prices DB");
    c.provider = QStringLiteral("postgresql");
    c.type = QStringLiteral("rest_api");
    c.enabled = true;
    c.config = QJsonObject{{"host", "db.local"}, {"port", 5432}, {"username", "analyst"}, {"password", "s3cr3t-pw"}};
    c.secret_fields = {QStringLiteral("password")};
    return c;
}

TransferConnection rest_connection() {
    TransferConnection c;
    c.id = QStringLiteral("id-2");
    c.display_name = QStringLiteral("Vendor API");
    c.provider = QStringLiteral("rest-api");
    c.config = QJsonObject{{"baseUrl", "https://api.example.com"},
                           {"apiKey", "k-123456"},
                           {"headers", "X-Token: t-999"},
                           {"authType", "apikey"}};
    return c;
}

} // namespace

class TestConnectionTransfer : public QObject {
    Q_OBJECT
  private slots:
    void secret_rule() {
        QVERIFY(is_secret_config_key(QStringLiteral("password")));
        QVERIFY(is_secret_config_key(QStringLiteral("apiKey")));
        QVERIFY(is_secret_config_key(QStringLiteral("secretAccessKey")));
        QVERIFY(is_secret_config_key(QStringLiteral("headers")));
        QVERIFY(is_secret_config_key(QStringLiteral("key")));
        QVERIFY(!is_secret_config_key(QStringLiteral("host")));
        QVERIFY(!is_secret_config_key(QStringLiteral("authType")));
    }

    void export_leaves_secrets_out_by_default() {
        const ExportResult r = export_connections_document({pg_connection(), rest_connection()}, ExportOptions{});
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.connections, 2);
        QCOMPARE(r.secrets_excluded, 3); // password, apiKey, headers
        QVERIFY(!r.json.contains("s3cr3t-pw"));
        QVERIFY(!r.json.contains("k-123456"));
        QVERIFY(!r.json.contains("t-999"));
        const QJsonObject doc = QJsonDocument::fromJson(r.json).object();
        QCOMPARE(doc.value("secrets").toString(), QStringLiteral("excluded"));
        QVERIFY(!doc.contains("kdf"));

        const ImportResult in = read_connections_document(r.json, QString());
        QVERIFY2(in.ok, qPrintable(in.error));
        QCOMPARE(in.connections.size(), 2);
        QCOMPARE(in.secrets_restored, 0);
        QCOMPARE(in.secrets_missing, 3);
        QCOMPARE(in.connections[0].missing_secrets, QStringList{QStringLiteral("password")});
        QCOMPARE(in.connections[0].connection.config.value("host").toString(), QStringLiteral("db.local"));
        QCOMPARE(in.connections[0].connection.config.value("password").toString(), QString());
    }

    void encrypted_round_trip() {
        ExportOptions o;
        o.include_secrets = true;
        o.passphrase = QStringLiteral("correct horse battery");
        const ExportResult r = export_connections_document({pg_connection(), rest_connection()}, o);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.secrets_encrypted, 3);
        QVERIFY(!r.json.contains("s3cr3t-pw"));
        QVERIFY(!r.json.contains("k-123456"));
        const QJsonObject doc = QJsonDocument::fromJson(r.json).object();
        QCOMPARE(doc.value("secrets").toString(), QStringLiteral("encrypted"));
        QCOMPARE(doc.value("cipher").toString(), QStringLiteral("AES-256-GCM"));
        QVERIFY(doc.value("kdf").toObject().value("iterations").toInt() >= 100000);

        // No passphrase: asked for, nothing returned.
        ImportResult in = read_connections_document(r.json, QString());
        QVERIFY(!in.ok);
        QVERIFY(in.needs_passphrase);
        QVERIFY(in.connections.isEmpty());

        // Wrong passphrase: rejected before anything is returned.
        in = read_connections_document(r.json, QStringLiteral("wrong passphrase"));
        QVERIFY(!in.ok);
        QVERIFY(in.wrong_passphrase);
        QVERIFY(in.connections.isEmpty());

        // Right passphrase: every secret restored.
        in = read_connections_document(r.json, o.passphrase);
        QVERIFY2(in.ok, qPrintable(in.error));
        QCOMPARE(in.secrets_restored, 3);
        QCOMPARE(in.secrets_missing, 0);
        QCOMPARE(in.connections[0].connection.config.value("password").toString(), QStringLiteral("s3cr3t-pw"));
        QCOMPARE(in.connections[1].connection.config.value("apiKey").toString(), QStringLiteral("k-123456"));
        QCOMPARE(in.connections[1].connection.config.value("headers").toString(), QStringLiteral("X-Token: t-999"));

        // Without secrets on purpose: settings only, the secret fields listed.
        in = read_connections_document(r.json, QString(), true);
        QVERIFY(in.ok);
        QCOMPARE(in.secrets_restored, 0);
        QCOMPARE(in.secrets_missing, 3);
    }

    void short_passphrase_rejected() {
        ExportOptions o;
        o.include_secrets = true;
        o.passphrase = QStringLiteral("short");
        const ExportResult r = export_connections_document({pg_connection()}, o);
        QVERIFY(!r.ok);
        QVERIFY(r.json.isEmpty());
    }

    void tampered_secrets_rejected() {
        ExportOptions o;
        o.include_secrets = true;
        o.passphrase = QStringLiteral("correct horse battery");
        const ExportResult r = export_connections_document({pg_connection()}, o);
        QJsonObject doc = QJsonDocument::fromJson(r.json).object();
        QJsonArray conns = doc.value("connections").toArray();
        QJsonObject c0 = conns[0].toObject();
        QJsonObject box = c0.value("secrets").toObject();
        QByteArray data = QByteArray::fromBase64(box.value("data").toString().toLatin1());
        data[0] = char(data[0] ^ 0x01);
        box.insert("data", QString::fromLatin1(data.toBase64()));
        c0.insert("secrets", box);
        conns[0] = c0;
        doc.insert("connections", conns);
        const ImportResult in = read_connections_document(QJsonDocument(doc).toJson(), o.passphrase);
        QVERIFY(!in.ok);
        QVERIFY(in.connections.isEmpty());
    }

    void primitives() {
        const QByteArray salt(16, 'x');
        const QByteArray key = transfer_derive_key(QStringLiteral("passphrase-1"), salt, 100000);
        QCOMPARE(key.size(), 32);
        QCOMPARE(transfer_derive_key(QStringLiteral("passphrase-1"), salt, 100000), key);
        QVERIFY(transfer_derive_key(QStringLiteral("passphrase-2"), salt, 100000) != key);
        const QJsonObject box = transfer_encrypt(key, QByteArray("hello"));
        QVERIFY(!box.isEmpty());
        QByteArray plain;
        QVERIFY(transfer_decrypt(key, box, &plain));
        QCOMPARE(plain, QByteArray("hello"));
        QVERIFY(!transfer_decrypt(transfer_derive_key(QStringLiteral("other"), salt, 100000), box, &plain));
        // Two encryptions of the same text differ (fresh IV each time).
        QVERIFY(transfer_encrypt(key, QByteArray("hello")).value("data") != box.value("data") ||
                transfer_encrypt(key, QByteArray("hello")).value("iv") != box.value("iv"));
    }

    void version_one_files() {
        const QJsonArray v1{QJsonObject{{"provider", "postgresql"},
                                        {"display_name", "Old"},
                                        {"config", QJsonObject{{"host", "h"}, {"password", "plain-pw"}}},
                                        {"secrets_redacted", false}},
                            QJsonObject{{"provider", "rest-api"},
                                        {"display_name", "Redacted"},
                                        {"config", QJsonObject{{"baseUrl", "https://x"}, {"apiKey", ""}}},
                                        {"secrets_redacted", true}},
                            QJsonObject{{"display_name", "no provider"}}};
        const ImportResult in = read_connections_document(QJsonDocument(v1).toJson(), QString());
        QVERIFY2(in.ok, qPrintable(in.error));
        QCOMPARE(in.format_version, 1);
        QCOMPARE(in.connections.size(), 2);
        QCOMPARE(in.skipped, 1);
        QCOMPARE(in.secrets_restored, 1);
        QCOMPARE(in.connections[1].missing_secrets, QStringList{QStringLiteral("apiKey")});
    }

    void not_a_connections_file() {
        QVERIFY(!read_connections_document(QByteArray("{\"hello\": 1}"), QString()).ok);
        QVERIFY(!read_connections_document(QByteArray("not json"), QString()).ok);
    }
};

QTEST_GUILESS_MAIN(TestConnectionTransfer)
#include "tst_connection_transfer.moc"
