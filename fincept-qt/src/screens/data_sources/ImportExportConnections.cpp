// ImportExportConnections.cpp — JSON import/export and template dump for
// data-source connections.

#include "screens/data_sources/ImportExportConnections.h"

#include "core/logging/Logger.h"
#include "screens/data_sources/ConnectorRegistry.h"
#include "screens/data_sources/DataSourceTypes.h"
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
#include <QStringList>
#include <QUuid>

namespace fincept::screens::datasources {

namespace {
constexpr const char* TAG = "DataSources";
} // namespace

bool export_connections(QWidget* parent) {
    const QString path = QFileDialog::getSaveFileName(
        parent, "Export Connections", "fincept_connections.json", "JSON Files (*.json);;All Files (*)");
    if (path.isEmpty())
        return false;

    const auto all = DataSourceRepository::instance().list_all();
    if (all.is_err())
        return false;

    QJsonArray arr;
    for (const auto& ds : all.value()) {
        QJsonObject obj;
        obj["id"]           = ds.id;
        obj["alias"]        = ds.alias;
        obj["display_name"] = ds.display_name;
        obj["description"]  = ds.description;
        obj["type"]         = ds.type;
        obj["provider"]     = ds.provider;
        obj["category"]     = ds.category;
        obj["config"]       = QJsonDocument::fromJson(ds.config.toUtf8()).object();
        obj["enabled"]      = ds.enabled;
        obj["tags"]         = ds.tags;
        obj["created_at"]   = ds.created_at;
        obj["updated_at"]   = ds.updated_at;
        arr.append(obj);
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_ERROR(TAG, QString("Export failed: cannot write to %1").arg(path));
        return false;
    }
    file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    file.close();
    LOG_INFO(TAG, QString("Exported %1 connections to %2").arg(arr.size()).arg(path));

    services::FileManagerService::instance().import_file(path, "data_sources");
    return true;
}

bool import_connections(QWidget* parent, int* imported_out, int* skipped_out) {
    const QString path = QFileDialog::getOpenFileName(
        parent, "Import Connections", "", "JSON Files (*.json);;All Files (*)");
    if (path.isEmpty())
        return false;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_ERROR(TAG, QString("Import failed: cannot open %1").arg(path));
        return false;
    }
    const QByteArray payload = file.readAll();
    QJsonParseError parse_err;
    const auto doc = QJsonDocument::fromJson(payload, &parse_err);
    if (parse_err.error != QJsonParseError::NoError || !doc.isArray()) {
        LOG_ERROR(TAG, QString("Import failed: invalid JSON — %1").arg(parse_err.errorString()));
        return false;
    }

    int imported = 0, skipped = 0;
    for (const auto& val : doc.array()) {
        const auto obj = val.toObject();
        const QString provider = obj["provider"].toString();
        if (provider.isEmpty()) {
            ++skipped;
            continue;
        }

        DataSource ds;
        ds.id           = QUuid::createUuid().toString(QUuid::WithoutBraces);
        ds.alias        = obj["alias"].toString();
        ds.display_name = obj["display_name"].toString();
        ds.description  = obj["description"].toString();
        ds.type         = obj["type"].toString();
        ds.provider     = provider;
        ds.category     = obj["category"].toString();
        ds.config       = QJsonDocument(obj["config"].toObject()).toJson(QJsonDocument::Compact);
        ds.enabled      = obj["enabled"].toBool(false);
        ds.tags         = obj["tags"].toString();

        if (ds.display_name.isEmpty()) ds.display_name = provider;
        if (ds.alias.isEmpty())        ds.alias = provider + "_" + ds.id.left(8);

        const auto result = DataSourceRepository::instance().save(ds);
        if (result.is_ok()) ++imported;
        else                ++skipped;
    }

    LOG_INFO(TAG, QString("Import complete: %1 imported, %2 skipped").arg(imported).arg(skipped));
    services::FileManagerService::instance().import_file(path, "data_sources");

    if (imported_out) *imported_out = imported;
    if (skipped_out)  *skipped_out  = skipped;
    return true;
}

bool download_connector_template(QWidget* parent) {
    const QString path = QFileDialog::getSaveFileName(
        parent, "Save Connector Template", "fincept_connections_template.json",
        "JSON Files (*.json);;All Files (*)");
    if (path.isEmpty())
        return false;

    const auto& all = ConnectorRegistry::instance().all();

    QMap<QString, QJsonArray> by_category;
    for (const auto& cfg : all) {
        QJsonObject config_obj;
        for (const auto& field : cfg.fields) {
            QString val = !field.default_value.isEmpty()
                              ? field.default_value
                              : (!field.placeholder.isEmpty() ? field.placeholder : "");
            config_obj[field.name] = val;
        }

        QJsonObject entry;
        entry["provider"]     = cfg.id;
        entry["display_name"] = cfg.name;
        entry["alias"]        = cfg.id + "_1";
        entry["description"]  = cfg.description;
        entry["type"]         = cfg.type;
        entry["category"]     = category_str(cfg.category);
        entry["enabled"]      = false;
        entry["tags"]         = "";
        entry["config"]       = config_obj;
        entry["_help"]        = QString("Fill in the 'config' fields and set 'enabled' to true. "
                                        "Remove this '_help' key before importing.");

        by_category[category_label(cfg.category)].append(entry);
    }

    QStringList cat_order = {"Databases",   "APIs",        "Streaming",          "File Sources",
                             "Cloud Storage", "Time Series", "Market Data", "Search & Analytics",
                             "Data Warehouses"};
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
        return false;
    }
    file.write(QJsonDocument(out).toJson(QJsonDocument::Indented));
    file.close();
    LOG_INFO(TAG, QString("Template written: %1 connectors to %2").arg(out.size()).arg(path));
    return true;
}

} // namespace fincept::screens::datasources
