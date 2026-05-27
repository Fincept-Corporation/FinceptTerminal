#include "services/workflow/nodes/DataSourceNodes.h"

#include "services/workflow/NodeRegistry.h"
#include "screens/data_sources/ConnectorRegistry.h"
#include "screens/data_sources/DataSourceTypes.h"
#include "storage/repositories/DataSourceRepository.h"
#include "python/PythonRunner.h"

#include <QRegularExpression>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTcpSocket>
#include <QDateTime>
#include <QUrl>
#include <QtConcurrent/QtConcurrent>
#include <QHash>

namespace fincept::workflow {

using fincept::python::PythonRunner;
using fincept::python::PythonResult;
using fincept::python::extract_json;

void register_datasource_nodes(NodeRegistry& registry) {
    // ── Register Data Source connectors dynamically ────────────────
    // Ensure all connectors are registered first
    fincept::screens::datasources::register_all_connectors();

    const auto& connectors = fincept::screens::datasources::ConnectorRegistry::instance().all();
    for (const auto& c : connectors) {
        // Construct ParamDefs from connector fields
        QVector<ParamDef> parameters;
        // First parameter: Saved Connection selector
        parameters.append({
            .key = "connection_id",
            .label = "Saved Connection",
            .type = "datasource_connection_select",
            .default_value = "",
            .options = {},
            .placeholder = c.id, // pass the connector ID here so the widget filters by it
            .required = false
        });

        // Add each field def as a parameter
        for (const auto& f : c.fields) {
            ParamDef p;
            p.key = f.name;
            p.label = f.label;
            p.placeholder = f.placeholder;
            p.required = false; // let connection settings handle requirements, make overrides optional
            p.default_value = f.default_value;

            // Map FieldType -> string type
            switch (f.type) {
                case fincept::screens::datasources::FieldType::Number:
                    p.type = "number";
                    break;
                case fincept::screens::datasources::FieldType::Checkbox:
                    p.type = "boolean";
                    break;
                case fincept::screens::datasources::FieldType::Select:
                    p.type = "select";
                    for (const auto& opt : f.options) {
                        p.options.append(opt.value); // use raw value
                    }
                    break;
                case fincept::screens::datasources::FieldType::File:
                    p.type = "file_managed";
                    break;
                default:
                    p.type = "string";
                    break;
            }
            parameters.append(std::move(p));
        }

        // Register the dynamic node type
        registry.register_type({
            .type_id = "datasource." + c.id,
            .display_name = c.name,
            .category = "Data Source: " + fincept::screens::datasources::category_label(c.category),
            .description = c.description,
            .icon_text = c.icon.isEmpty() ? "DS" : c.icon,
            .accent_color = c.color.isEmpty() ? "#10b981" : c.color,
            .version = 1,
            .inputs = {{"input_trigger", "Trigger In", PortDirection::Input, ConnectionType::Main, false}},
            .outputs = {{"output_main", "Data Out", PortDirection::Output, ConnectionType::Main}},
            .parameters = std::move(parameters),
            .execute = [connector_id = c.id, connector_name = c.name](const QJsonObject& params, const QVector<QJsonValue>&,
                       std::function<void(bool, QJsonValue, QString)> cb) {
                // Determine connection config
                QJsonObject final_config;
                QString conn_id = params.value("connection_id").toString();

                if (!conn_id.isEmpty()) {
                    auto res = DataSourceRepository::instance().get(conn_id);
                    if (res.is_ok()) {
                        const auto& ds = res.value();
                        QJsonDocument doc = QJsonDocument::fromJson(ds.config.toUtf8());
                        if (doc.isObject()) {
                            final_config = doc.object();
                        }
                    }
                }

                // Merge in parameter overrides (anything that has a non-empty value and is not connection_id)
                for (auto it = params.begin(); it != params.end(); ++it) {
                    if (it.key() != "connection_id" && !it.value().isNull() && !it.value().toString().isEmpty()) {
                        final_config[it.key()] = it.value();
                    }
                }

                // Define standard script execution naming via static QHash instead of if/else
                static const QHash<QString, QString> script_mappings = {
                    {"yahoo-finance", "yfinance_data.py"},
                    {"alpha-vantage", "alphavantage_data.py"},
                    {"eod-historical", "eodhd_data.py"}
                };

                QString script_name = script_mappings.value(connector_id);
                if (script_name.isEmpty()) {
                    // check if standard file exists
                    QString clean_id = connector_id;
                    clean_id.replace("-", "_");
                    QString candidate1 = clean_id + "_data.py";
                    QString candidate2 = clean_id + ".py";
                    QString candidate3 = connector_id + "_data.py";
                    QString candidate4 = connector_id + ".py";

                    // check which one exists in scripts directory using cross-platform QDir::filePath
                    QString scripts_dir = PythonRunner::instance().scripts_dir();
                    QDir dir(scripts_dir);
                    if (QFileInfo::exists(dir.filePath(candidate1))) script_name = candidate1;
                    else if (QFileInfo::exists(dir.filePath(candidate2))) script_name = candidate2;
                    else if (QFileInfo::exists(dir.filePath(candidate3))) script_name = candidate3;
                    else if (QFileInfo::exists(dir.filePath(candidate4))) script_name = candidate4;
                }

                if (!script_name.isEmpty()) {
                    // Execute specific script
                    QString args_json = QString::fromUtf8(QJsonDocument(final_config).toJson(QJsonDocument::Compact));
                    PythonRunner::instance().run(script_name, {"--args", args_json}, [cb, connector_name](const PythonResult& res) {
                        if (!res.success) {
                            cb(false, {}, res.error.isEmpty() ? "Script execution failed" : res.error);
                            return;
                        }
                        QString out = fincept::python::extract_json(res.output).trimmed();
                        auto doc = QJsonDocument::fromJson(out.toUtf8());
                        if (doc.isNull()) {
                            cb(false, {}, "Invalid JSON response: " + res.output.left(200));
                            return;
                        }
                        cb(true, doc.isObject() ? QJsonValue(doc.object()) : QJsonValue(doc.array()), {});
                    });
                } else {
                    // Fallback to simulated data fetch or connectivity test
                    // Let's perform a live TCP connectivity test or return resolved config
                    // Try to resolve host/port
                    QString host = final_config.value("host").toString();
                    int port = final_config.value("port").toInt(0);
                    if (host.isEmpty()) {
                        // try to extract from url
                        QString url = final_config.value("url").toString();
                        if (!url.isEmpty()) {
                            QUrl qurl(url);
                            host = qurl.host();
                            port = qurl.port(qurl.scheme() == "https" ? 443 : 80);
                        }
                    }

                    if (!host.isEmpty() && port > 0) {
                        // Run TCP reachability check asynchronously
                        (void)QtConcurrent::run([host, port, final_config, connector_name, cb]() {
                            QTcpSocket sock;
                            sock.connectToHost(host, static_cast<quint16>(port));
                            bool ok = sock.waitForConnected(3000);
                            sock.disconnectFromHost();

                            QJsonObject out;
                            out["connector"] = connector_name;
                            out["host"] = host;
                            out["port"] = port;
                            out["status"] = ok ? "connected" : "failed";
                            out["message"] = ok ? "Successfully connected to data source server." : "Failed to connect to data source server: " + sock.errorString();
                            out["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
                            out["config_applied"] = final_config;
                            cb(ok, out, ok ? QString{} : out["message"].toString());
                        });
                    } else {
                        // Return configuration directly
                        QJsonObject out;
                        out["connector"] = connector_name;
                        out["status"] = "mocked";
                        out["message"] = "No server endpoint configured. Returning applied config parameters.";
                        out["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
                        out["config_applied"] = final_config;
                        cb(true, out, {});
                    }
                }
            }
        });
    }
}

} // namespace fincept::workflow
