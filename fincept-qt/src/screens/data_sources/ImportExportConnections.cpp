// ImportExportConnections.cpp — JSON import/export and template dump for
// data-source connections.
//
// SECURITY NOTE: a connection's `config` blob holds whatever the connector
// schema declared — including every FieldType::Password value (DB passwords,
// API keys, API secrets, private keys, access tokens). Exporting it verbatim
// writes those secrets to a user-chosen file in plaintext, so export_connections
// redacts them by default and requires an explicit opt-in to include them.

#include "screens/data_sources/ImportExportConnections.h"

#include "core/logging/Logger.h"
#include "screens/data_sources/ConnectorRegistry.h"
#include "screens/data_sources/DataSourceTypes.h"
#include "screens/data_sources/DataSourcesHelpers.h"
#include "services/file_manager/FileManagerService.h"
#include "storage/repositories/DataSourceRepository.h"

#include <QByteArray>
#include <QFile>
#include <QFileDialog>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMap>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QStringList>
#include <QUuid>

namespace fincept::screens::datasources {

namespace {
constexpr const char* TAG = "DataSources";

/// Keys that are treated as secret when the connector has no registry entry
/// (imported/unknown providers), matched case-insensitively as substrings.
bool looks_like_secret_key(const QString& key) {
    const QString k = key.toLower();
    return k.contains(QLatin1String("password")) || k.contains(QLatin1String("passwd")) ||
           k.contains(QLatin1String("secret")) || k.contains(QLatin1String("token")) ||
           k.contains(QLatin1String("apikey")) || k.contains(QLatin1String("api_key")) ||
           k.contains(QLatin1String("privatekey")) || k.contains(QLatin1String("private_key")) ||
           k.contains(QLatin1String("accesskey")) || k.contains(QLatin1String("access_key")) ||
           k.contains(QLatin1String("credential")) || k.contains(QLatin1String("appkey")) ||
           k.contains(QLatin1String("connectionstring"));
}

/// Blank out every secret-bearing value in a connection's config object.
/// Authoritative source is the connector schema (FieldType::Password); the
/// name heuristic is the fallback for providers not in the registry.
/// Returns the number of values that were cleared.
int redact_config(QJsonObject& cfg, const QString& provider) {
    QSet<QString> secret_fields;
    if (const auto* connector = find_connector_config(provider)) {
        for (const auto& field : connector->fields) {
            if (field.type == FieldType::Password)
                secret_fields.insert(field.name);
        }
    }

    int cleared = 0;
    const QStringList keys = cfg.keys();
    for (const QString& key : keys) {
        if (!secret_fields.contains(key) && !looks_like_secret_key(key))
            continue;
        if (!cfg.value(key).isString() || cfg.value(key).toString().isEmpty())
            continue;
        cfg[key] = QString();
        ++cleared;
    }
    return cleared;
}

/// Count the secret-bearing values a connection would leak if exported raw.
int count_secrets(const QJsonObject& cfg, const QString& provider) {
    QJsonObject copy = cfg;
    return redact_config(copy, provider);
}

} // namespace

bool export_connections(QWidget* parent) {
    const auto all = DataSourceRepository::instance().list_all();
    if (all.is_err()) {
        QMessageBox::warning(parent, QObject::tr("Export Connections"),
                             QObject::tr("Could not read the connection store: %1")
                                 .arg(QString::fromStdString(all.error())));
        return false;
    }

    // How much credential material is actually at risk? Say so explicitly
    // rather than silently writing keys to a file the user may share.
    int secret_count = 0;
    for (const auto& ds : all.value())
        secret_count += count_secrets(QJsonDocument::fromJson(ds.config.toUtf8()).object(), ds.provider);

    bool include_secrets = false;
    if (secret_count > 0) {
        QMessageBox box(parent);
        box.setIcon(QMessageBox::Warning);
        box.setWindowTitle(QObject::tr("Export Connections"));
        box.setText(QObject::tr("%n stored secret(s) (passwords, API keys, tokens) are attached to these connections.",
                                "", secret_count));
        box.setInformativeText(
            QObject::tr("The export file is plain, unencrypted JSON. Anyone who opens it — or any tool that "
                        "reads your files — sees whatever it contains.\n\n"
                        "Redacted: connection settings are exported, secret values are blanked out and must "
                        "be re-entered after import.\n"
                        "With secrets: the file contains your live credentials."));
        auto* redacted_btn = box.addButton(QObject::tr("Export Redacted"), QMessageBox::AcceptRole);
        auto* raw_btn = box.addButton(QObject::tr("Include Secrets"), QMessageBox::DestructiveRole);
        auto* cancel_btn = box.addButton(QMessageBox::Cancel);
        box.setDefaultButton(redacted_btn);
        box.exec();
        if (box.clickedButton() == cancel_btn)
            return false;
        include_secrets = (box.clickedButton() == raw_btn);
    }

    const QString path =
        QFileDialog::getSaveFileName(parent, QObject::tr("Export Connections"), "fincept_connections.json",
                                     QObject::tr("JSON Files (*.json);;All Files (*)"));
    if (path.isEmpty())
        return false;

    QJsonArray arr;
    int redacted_total = 0;
    for (const auto& ds : all.value()) {
        QJsonObject config_obj = QJsonDocument::fromJson(ds.config.toUtf8()).object();
        if (!include_secrets)
            redacted_total += redact_config(config_obj, ds.provider);

        QJsonObject obj;
        obj["id"] = ds.id;
        obj["alias"] = ds.alias;
        obj["display_name"] = ds.display_name;
        obj["description"] = ds.description;
        obj["type"] = ds.type;
        obj["provider"] = ds.provider;
        obj["category"] = ds.category;
        obj["config"] = config_obj;
        obj["enabled"] = ds.enabled;
        obj["tags"] = ds.tags;
        obj["created_at"] = ds.created_at;
        obj["updated_at"] = ds.updated_at;
        obj["secrets_redacted"] = !include_secrets;
        arr.append(obj);
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_ERROR(TAG, QString("Export failed: cannot write to %1").arg(path));
        QMessageBox::warning(parent, QObject::tr("Export Connections"),
                             QObject::tr("Could not write to:\n%1\n\n%2").arg(path, file.errorString()));
        return false;
    }
    file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    file.close();
    LOG_INFO(TAG, QString("Exported %1 connections to %2 (%3)")
                      .arg(arr.size())
                      .arg(path)
                      .arg(include_secrets ? "secrets included" : "secrets redacted"));

    // Only index a redacted export in the file manager. A credential-bearing
    // file must not become discoverable through the file browser or the MCP
    // file tools that read from the same index.
    if (!include_secrets)
        services::FileManagerService::instance().import_file(path, "data_sources");

    QMessageBox::information(
        parent, QObject::tr("Export Connections"),
        include_secrets
            ? QObject::tr("Exported %n connection(s) INCLUDING plaintext secrets to:\n%1\n\n"
                          "Treat this file as a credential file — do not share or sync it.",
                          "", static_cast<int>(arr.size()))
                  .arg(path)
            : QObject::tr("Exported %n connection(s) to:\n%1\n\n%2 secret value(s) were blanked out and must be "
                          "re-entered after import.",
                          "", static_cast<int>(arr.size()))
                  .arg(path)
                  .arg(redacted_total));
    return true;
}

bool import_connections(QWidget* parent, int* imported_out, int* skipped_out) {
    const QString path = QFileDialog::getOpenFileName(parent, QObject::tr("Import Connections"), "",
                                                      QObject::tr("JSON Files (*.json);;All Files (*)"));
    if (path.isEmpty())
        return false;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_ERROR(TAG, QString("Import failed: cannot open %1").arg(path));
        QMessageBox::warning(parent, QObject::tr("Import Connections"),
                             QObject::tr("Could not open:\n%1\n\n%2").arg(path, file.errorString()));
        return false;
    }
    const QByteArray payload = file.readAll();
    file.close();

    QJsonParseError parse_err;
    const auto doc = QJsonDocument::fromJson(payload, &parse_err);
    if (parse_err.error != QJsonParseError::NoError || !doc.isArray()) {
        LOG_ERROR(TAG, QString("Import failed: invalid JSON — %1").arg(parse_err.errorString()));
        // Surface the parser's own message + offset, not a generic "failed".
        QMessageBox::warning(parent, QObject::tr("Import Connections"),
                             parse_err.error != QJsonParseError::NoError
                                 ? QObject::tr("The file is not valid JSON.\n\n%1 (at offset %2)")
                                       .arg(parse_err.errorString())
                                       .arg(parse_err.offset)
                                 : QObject::tr("The file must contain a JSON array of connections."));
        return false;
    }

    // An imported connection can point a connector at an arbitrary host and be
    // marked enabled, which the background reachability poll will then contact.
    // Ask before writing anything.
    const auto entries = doc.array();
    if (QMessageBox::question(parent, QObject::tr("Import Connections"),
                              QObject::tr("Import %n connection(s) from:\n%1\n\n"
                                          "Only import files you created or trust — an imported connection can "
                                          "point a connector at any host.",
                                          "", static_cast<int>(entries.size()))
                                  .arg(path),
                              QMessageBox::Yes | QMessageBox::Cancel,
                              QMessageBox::Cancel) != QMessageBox::Yes)
        return false;

    int imported = 0, skipped = 0, needs_secrets = 0;
    for (const auto& val : entries) {
        const auto obj = val.toObject();
        const QString provider = obj["provider"].toString();
        if (provider.isEmpty()) {
            ++skipped;
            continue;
        }

        DataSource ds;
        ds.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        ds.alias = obj["alias"].toString();
        ds.display_name = obj["display_name"].toString();
        ds.description = obj["description"].toString();
        ds.type = obj["type"].toString();
        ds.provider = provider;
        ds.category = obj["category"].toString();
        ds.config = QJsonDocument(obj["config"].toObject()).toJson(QJsonDocument::Compact);
        ds.enabled = obj["enabled"].toBool(false);
        ds.tags = obj["tags"].toString();

        if (obj.value("secrets_redacted").toBool(false))
            ++needs_secrets;

        if (ds.display_name.isEmpty())
            ds.display_name = provider;
        if (ds.alias.isEmpty())
            ds.alias = provider + "_" + ds.id.left(8);

        const auto result = DataSourceRepository::instance().save(ds);
        if (result.is_ok())
            ++imported;
        else
            ++skipped;
    }

    LOG_INFO(TAG, QString("Import complete: %1 imported, %2 skipped").arg(imported).arg(skipped));
    services::FileManagerService::instance().import_file(path, "data_sources");

    QString summary = QObject::tr("Imported %1 connection(s), skipped %2.").arg(imported).arg(skipped);
    if (needs_secrets > 0)
        summary += QObject::tr("\n\n%1 of them came from a redacted export — open each one and re-enter its "
                               "password / API key before use.")
                       .arg(needs_secrets);
    QMessageBox::information(parent, QObject::tr("Import Connections"), summary);

    if (imported_out)
        *imported_out = imported;
    if (skipped_out)
        *skipped_out = skipped;
    return true;
}

bool download_connector_template(QWidget* parent) {
    const QString path = QFileDialog::getSaveFileName(parent, QObject::tr("Save Connector Template"),
                                                      "fincept_connections_template.json",
                                                      QObject::tr("JSON Files (*.json);;All Files (*)"));
    if (path.isEmpty())
        return false;

    const auto& all = ConnectorRegistry::instance().all();

    QMap<QString, QJsonArray> by_category;
    for (const auto& cfg : all) {
        QJsonObject config_obj;
        for (const auto& field : cfg.fields) {
            // Never seed a secret field with a placeholder that looks like a
            // real value — leave it empty so the template can't be mistaken
            // for a filled-in credential file.
            if (field.type == FieldType::Password) {
                config_obj[field.name] = QString();
                continue;
            }
            QString val = !field.default_value.isEmpty() ? field.default_value
                                                         : (!field.placeholder.isEmpty() ? field.placeholder : "");
            config_obj[field.name] = val;
        }

        QJsonObject entry;
        entry["provider"] = cfg.id;
        entry["display_name"] = cfg.name;
        entry["alias"] = cfg.id + "_1";
        entry["description"] = cfg.description;
        entry["type"] = cfg.type;
        entry["category"] = category_str(cfg.category);
        entry["enabled"] = false;
        entry["tags"] = "";
        entry["config"] = config_obj;
        entry["_help"] = QString("Fill in the 'config' fields and set 'enabled' to true. "
                                 "Remove this '_help' key before importing.");

        by_category[category_label(cfg.category)].append(entry);
    }

    QStringList cat_order = {"Databases",   "APIs",        "Streaming",          "File Sources",   "Cloud Storage",
                             "Time Series", "Market Data", "Search & Analytics", "Data Warehouses"};
    QJsonArray out;
    for (const auto& cat : cat_order) {
        if (by_category.contains(cat)) {
            for (const auto& entry : by_category[cat])
                out.append(entry);
        }
    }
    for (auto it = by_category.begin(); it != by_category.end(); ++it) {
        if (!cat_order.contains(it.key())) {
            for (const auto& entry : it.value())
                out.append(entry);
        }
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_ERROR(TAG, QString("Template write failed: %1").arg(path));
        QMessageBox::warning(parent, QObject::tr("Save Connector Template"),
                             QObject::tr("Could not write to:\n%1\n\n%2").arg(path, file.errorString()));
        return false;
    }
    file.write(QJsonDocument(out).toJson(QJsonDocument::Indented));
    file.close();
    LOG_INFO(TAG, QString("Template written: %1 connectors to %2").arg(out.size()).arg(path));
    return true;
}

} // namespace fincept::screens::datasources
