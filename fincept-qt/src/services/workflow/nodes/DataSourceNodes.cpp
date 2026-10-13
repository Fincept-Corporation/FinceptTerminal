#include "services/workflow/nodes/DataSourceNodes.h"

#include "screens/data_sources/ConnectorRegistry.h"
#include "screens/data_sources/DataSourceTypes.h"
#include "services/connectors/ConnectorCatalog.h"
#include "services/connectors/ConnectorService.h"
#include "services/workflow/NodeRegistry.h"
#include "storage/repositories/DataSourceRepository.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>

namespace fincept::workflow {

namespace {

/// Data Sources connection types that have a live Python connector in the catalogue
/// (resources/data_connectors.json). Every other connection type (databases, brokers' data
/// feeds, cloud storage, ...) has no fetch implementation behind its workflow node yet.
struct DsNodeLiveConnector {
    const char* node_id;
    const char* connector;
};
constexpr DsNodeLiveConnector kDsNodeLiveConnectors[] = {
    {"yahoo-finance", "data_yfinance"}, {"alpha-vantage", "data_alphavantage"},  {"eod-historical", "data_eodhd"},
    {"coingecko", "data_coingecko"},    {"coinmarketcap", "data_coinmarketcap"}, {"finnhub", "data_finnhub"},
    {"intrinio", "data_intrinio"},      {"marketstack", "data_marketstack"},
    {"openfigi", "data_openfigi"},      {"polygon-io", "data_polygon_io"},       {"tiingo", "data_tiingo"},
    {"twelve-data", "data_twelve"},     {"quandl", "data_quandl_nasdaq"},
};

QString ds_node_connector_for(const QString& node_id) {
    for (const auto& m : kDsNodeLiveConnectors)
        if (node_id == QLatin1String(m.node_id))
            return QString::fromLatin1(m.connector);
    return {};
}

/// Symbols configured on the node or its saved connection, used as the default arguments
/// when the node's `args` parameter is empty (e.g. Yahoo Finance's "Default Symbols").
QStringList ds_node_symbols(const QJsonObject& config) {
    for (const char* key : {"symbols", "symbol", "ticker", "tickers"}) {
        const QString v = config.value(QLatin1String(key)).toString().trimmed();
        if (v.isEmpty())
            continue;
        QStringList out;
        for (const QString& s : v.split(QLatin1Char(','), Qt::SkipEmptyParts))
            if (!s.trimmed().isEmpty())
                out << s.trimmed();
        return out;
    }
    return {};
}

} // namespace

void register_datasource_nodes(NodeRegistry& registry) {
    // Ensure all connection types are registered first.
    fincept::screens::datasources::register_all_connectors();
    auto& catalog = fincept::services::connectors::ConnectorCatalog::instance();
    const bool catalog_ok = catalog.ensure_loaded();

    const auto& connectors = fincept::screens::datasources::ConnectorRegistry::instance().all();
    for (const auto& c : connectors) {
        const QString live_name = catalog_ok ? ds_node_connector_for(c.id) : QString();
        const auto* live = live_name.isEmpty() ? nullptr : catalog.find(live_name);

        QVector<ParamDef> parameters;
        // First parameter: Saved Connection selector
        parameters.append({.key = "connection_id",
                           .label = "Saved Connection",
                           .type = "datasource_connection_select",
                           .default_value = "",
                           .options = {},
                           .placeholder = c.id, // pass the connector ID here so the widget filters by it
                           .required = false});

        if (live) {
            // What to fetch: the catalogue connector's commands, with the default preselected.
            parameters.append({.key = "command",
                               .label = "Command",
                               .type = "select",
                               .default_value = live->default_command,
                               .options = live->command_names(),
                               .placeholder = "",
                               .required = false});
            parameters.append({.key = "args",
                               .label = "Arguments (space-separated)",
                               .type = "string",
                               .default_value = live->default_args.join(QLatin1Char(' ')),
                               .options = {},
                               .placeholder = live->args_hint(live->default_command),
                               .required = false});
        }

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

        QString description = c.description;
        if (live)
            description += QStringLiteral(" Fetches live data through the %1 connector; the API key comes from "
                                          "Settings › Credentials (saving a connection stores its key there).")
                               .arg(live->name);
        else
            description += QStringLiteral(" (No live data connector behind this node yet.)");

        // Register the dynamic node type
        registry.register_type(
            {.type_id = "datasource." + c.id,
             .display_name = c.name,
             .category = "Data Source: " + fincept::screens::datasources::category_label(c.category),
             .description = description,
             .icon_text = c.icon.isEmpty() ? "DS" : c.icon,
             .accent_color = c.color.isEmpty() ? "#10b981" : c.color,
             .version = 1,
             .inputs = {{"input_trigger", "Trigger In", PortDirection::Input, ConnectionType::Main, false}},
             .outputs = {{"output_main", "Data Out", PortDirection::Output, ConnectionType::Main}},
             .parameters = std::move(parameters),
             .execute = [connector_name = c.name, live_name](const QJsonObject& params, const QVector<QJsonValue>&,
                                                             std::function<void(bool, QJsonValue, QString)> cb) {
                 namespace conn = fincept::services::connectors;
                 if (live_name.isEmpty()) {
                     // This used to answer {"status": "mocked"} as a SUCCESS with no data, so a
                     // workflow silently carried on with nothing. Fail loudly instead.
                     cb(false, {},
                        QStringLiteral("No live connector for %1 yet — use a node backed by the Data Sources › "
                                       "Catalog connectors (or a Python node) to fetch this data.")
                            .arg(connector_name));
                     return;
                 }
                 const conn::ConnectorInfo* info = conn::ConnectorCatalog::instance().find(live_name);
                 if (!info) {
                     cb(false, {}, QStringLiteral("Connector %1 is missing from the catalogue").arg(live_name));
                     return;
                 }

                 // Saved connection config (non-secret fields such as default symbols), with the
                 // node's own parameter overrides on top.
                 QJsonObject config;
                 const QString conn_id = params.value("connection_id").toString();
                 if (!conn_id.isEmpty()) {
                     const auto res = DataSourceRepository::instance().get(conn_id);
                     if (res.is_ok()) {
                         const QJsonDocument doc = QJsonDocument::fromJson(res.value().config.toUtf8());
                         if (doc.isObject())
                             config = doc.object();
                     }
                 }
                 for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
                     if (it.key() != QLatin1String("connection_id") && !it.value().isNull() &&
                         !it.value().toString().isEmpty())
                         config[it.key()] = it.value();
                 }

                 QString command = config.value("command").toString().trimmed();
                 if (command.isEmpty())
                     command = info->default_command;
                 QStringList args = QProcess::splitCommand(config.value("args").toString().trimmed());
                 if (args.isEmpty()) {
                     args = ds_node_symbols(config);
                     if (args.size() > 1 && command == QLatin1String("quote") && info->has_command("batch_quotes"))
                         command = QStringLiteral("batch_quotes");
                 }

                 conn::ConnectorService::instance().run(info->name, command, args, /*force=*/false,
                                                        [cb](const conn::ConnectorResult& r) {
                                                            if (r.success)
                                                                cb(true, r.data, {});
                                                            else
                                                                cb(false, {}, r.error);
                                                        });
             }});
    }
}

} // namespace fincept::workflow
