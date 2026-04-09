// DataSourcesTools.cpp — Data Sources tab MCP tools
// Covers: connection CRUD, connector discovery, connectivity test, stats.

#include "mcp/tools/DataSourcesTools.h"

#include "core/logging/Logger.h"
#include "screens/data_sources/ConnectorRegistry.h"
#include "screens/data_sources/DataSourceTypes.h"
#include "storage/repositories/DataSourceRepository.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>
#include <QUuid>

namespace fincept::mcp::tools {

using namespace fincept::screens::datasources;

// ── Helpers ──────────────────────────────────────────────────────────────────

static QJsonObject ds_to_json(const DataSource& ds) {
    return QJsonObject{
        {"id", ds.id},
        {"alias", ds.alias},
        {"display_name", ds.display_name},
        {"description", ds.description},
        {"type", ds.type},
        {"provider", ds.provider},
        {"category", ds.category},
        {"config", ds.config},
        {"enabled", ds.enabled},
        {"tags", ds.tags},
        {"created_at", ds.created_at},
        {"updated_at", ds.updated_at},
    };
}

static QJsonObject connector_to_json(const ConnectorConfig& c) {
    QJsonArray fields_arr;
    for (const auto& f : c.fields) {
        QString type_str;
        switch (f.type) {
            case FieldType::Text:
                type_str = "text";
                break;
            case FieldType::Password:
                type_str = "password";
                break;
            case FieldType::Number:
                type_str = "number";
                break;
            case FieldType::Url:
                type_str = "url";
                break;
            case FieldType::Select:
                type_str = "select";
                break;
            case FieldType::Textarea:
                type_str = "textarea";
                break;
            case FieldType::Checkbox:
                type_str = "checkbox";
                break;
            case FieldType::File:
                type_str = "file";
                break;
        }
        QJsonObject fo{
            {"name", f.name},
            {"label", f.label},
            {"type", type_str},
            {"required", f.required},
            {"default_value", f.default_value},
            {"placeholder", f.placeholder},
        };
        if (!f.options.isEmpty()) {
            QJsonArray opts;
            for (const auto& o : f.options)
                opts.append(QJsonObject{{"label", o.label}, {"value", o.value}});
            fo["options"] = opts;
        }
        fields_arr.append(fo);
    }
    return QJsonObject{
        {"id", c.id},
        {"name", c.name},
        {"type", c.type},
        {"category", category_str(c.category)},
        {"category_label", category_label(c.category)},
        {"icon", c.icon},
        {"description", c.description},
        {"testable", c.testable},
        {"requires_auth", c.requires_auth},
        {"fields", fields_arr},
    };
}

// ── Tool registration ─────────────────────────────────────────────────────────

std::vector<ToolDef> get_data_sources_tools() {
    std::vector<ToolDef> tools;

    // ── ds_list_connections ──────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ds_list_connections";
        t.description = "List all saved data source connections (name, provider, category, enabled status).";
        t.category = "data-sources";
        t.input_schema.properties = QJsonObject{};
        t.handler = [](const QJsonObject&) -> ToolResult {
            auto r = DataSourceRepository::instance().list_all();
            if (r.is_err())
                return ToolResult::fail("Failed to list connections: " + QString::fromStdString(r.error()));
            QJsonArray arr;
            for (const auto& ds : r.value())
                arr.append(ds_to_json(ds));
            return ToolResult::ok_data(arr);
        };
        tools.push_back(std::move(t));
    }

    // ── ds_get_connection ────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ds_get_connection";
        t.description = "Get full details (including config JSON) for a saved connection by ID.";
        t.category = "data-sources";
        t.input_schema.properties = QJsonObject{
            {"id", QJsonObject{{"type", "string"}, {"description", "Connection ID (UUID)"}}},
        };
        t.input_schema.required = {"id"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString id = args["id"].toString().trimmed();
            if (id.isEmpty())
                return ToolResult::fail("Missing 'id'");
            auto r = DataSourceRepository::instance().get(id);
            if (r.is_err())
                return ToolResult::fail("Connection not found: " + QString::fromStdString(r.error()));
            return ToolResult::ok_data(ds_to_json(r.value()));
        };
        tools.push_back(std::move(t));
    }

    // ── ds_add_connection ────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ds_add_connection";
        t.description = "Create a new saved data source connection. "
                        "'provider' must be a valid connector ID from ds_list_connectors. "
                        "'config' is a JSON object with the connector's required fields.";
        t.category = "data-sources";
        t.input_schema.properties = QJsonObject{
            {"display_name", QJsonObject{{"type", "string"}, {"description", "Human-readable connection name"}}},
            {"provider",
             QJsonObject{{"type", "string"}, {"description", "Connector ID (e.g. 'postgresql', 'yahoo_finance')"}}},
            {"config", QJsonObject{{"type", "object"}, {"description", "Key-value map of connector field values"}}},
            {"description", QJsonObject{{"type", "string"}, {"description", "Optional description"}}},
            {"tags", QJsonObject{{"type", "string"}, {"description", "Optional comma-separated tags"}}},
            {"enabled",
             QJsonObject{{"type", "boolean"}, {"description", "Whether to enable immediately (default: true)"}}},
        };
        t.input_schema.required = {"display_name", "provider", "config"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString name = args["display_name"].toString().trimmed();
            QString provider = args["provider"].toString().trimmed();
            if (name.isEmpty())
                return ToolResult::fail("Missing 'display_name'");
            if (provider.isEmpty())
                return ToolResult::fail("Missing 'provider'");

            // Validate provider exists
            const auto* cfg = ConnectorRegistry::instance().get(provider);
            if (!cfg)
                return ToolResult::fail("Unknown provider '" + provider +
                                        "'. Use ds_list_connectors to see valid IDs.");

            QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
            QString alias = provider + "_" + uuid.left(8);

            // Serialize config
            QJsonObject config_obj = args["config"].toObject();
            QString config_json = QString::fromUtf8(QJsonDocument(config_obj).toJson(QJsonDocument::Compact));

            DataSource ds;
            ds.id = uuid;
            ds.alias = alias;
            ds.display_name = name;
            ds.description = args["description"].toString();
            ds.type = "rest_api"; // default; user can update via ds_update_connection
            ds.provider = provider;
            ds.category = category_str(cfg->category);
            ds.config = config_json;
            ds.enabled = args["enabled"].toBool(true);
            ds.tags = args["tags"].toString();

            auto r = DataSourceRepository::instance().save(ds);
            if (r.is_err())
                return ToolResult::fail("Failed to save connection: " + QString::fromStdString(r.error()));

            return ToolResult::ok("Connection created: " + name,
                                  QJsonObject{{"id", uuid}, {"alias", alias}, {"provider", provider}});
        };
        tools.push_back(std::move(t));
    }

    // ── ds_update_connection ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ds_update_connection";
        t.description = "Update an existing saved connection. Only supplied fields are changed.";
        t.category = "data-sources";
        t.input_schema.properties = QJsonObject{
            {"id", QJsonObject{{"type", "string"}, {"description", "Connection ID to update"}}},
            {"display_name", QJsonObject{{"type", "string"}, {"description", "New display name"}}},
            {"config", QJsonObject{{"type", "object"}, {"description", "Updated config fields"}}},
            {"description", QJsonObject{{"type", "string"}, {"description", "Updated description"}}},
            {"tags", QJsonObject{{"type", "string"}, {"description", "Updated tags"}}},
            {"enabled", QJsonObject{{"type", "boolean"}, {"description", "Enable or disable"}}},
        };
        t.input_schema.required = {"id"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString id = args["id"].toString().trimmed();
            if (id.isEmpty())
                return ToolResult::fail("Missing 'id'");

            auto r = DataSourceRepository::instance().get(id);
            if (r.is_err())
                return ToolResult::fail("Connection not found: " + QString::fromStdString(r.error()));

            DataSource ds = r.value();
            if (args.contains("display_name") && !args["display_name"].toString().isEmpty())
                ds.display_name = args["display_name"].toString();
            if (args.contains("description"))
                ds.description = args["description"].toString();
            if (args.contains("tags"))
                ds.tags = args["tags"].toString();
            if (args.contains("enabled"))
                ds.enabled = args["enabled"].toBool();
            if (args.contains("config") && args["config"].isObject()) {
                ds.config = QString::fromUtf8(QJsonDocument(args["config"].toObject()).toJson(QJsonDocument::Compact));
            }

            auto sr = DataSourceRepository::instance().save(ds);
            if (sr.is_err())
                return ToolResult::fail("Failed to update: " + QString::fromStdString(sr.error()));

            return ToolResult::ok("Connection updated: " + ds.display_name, QJsonObject{{"id", id}});
        };
        tools.push_back(std::move(t));
    }

    // ── ds_delete_connection ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ds_delete_connection";
        t.description = "Delete a saved data source connection by ID.";
        t.category = "data-sources";
        t.input_schema.properties = QJsonObject{
            {"id", QJsonObject{{"type", "string"}, {"description", "Connection ID to delete"}}},
        };
        t.input_schema.required = {"id"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString id = args["id"].toString().trimmed();
            if (id.isEmpty())
                return ToolResult::fail("Missing 'id'");
            auto r = DataSourceRepository::instance().remove(id);
            if (r.is_err())
                return ToolResult::fail("Failed to delete: " + QString::fromStdString(r.error()));
            return ToolResult::ok("Connection deleted");
        };
        tools.push_back(std::move(t));
    }

    // ── ds_set_enabled ───────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ds_set_enabled";
        t.description = "Enable or disable a saved data source connection.";
        t.category = "data-sources";
        t.input_schema.properties = QJsonObject{
            {"id", QJsonObject{{"type", "string"}, {"description", "Connection ID"}}},
            {"enabled", QJsonObject{{"type", "boolean"}, {"description", "true to enable, false to disable"}}},
        };
        t.input_schema.required = {"id", "enabled"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString id = args["id"].toString().trimmed();
            if (id.isEmpty())
                return ToolResult::fail("Missing 'id'");
            if (!args.contains("enabled"))
                return ToolResult::fail("Missing 'enabled'");
            bool enabled = args["enabled"].toBool();
            auto r = DataSourceRepository::instance().set_enabled(id, enabled);
            if (r.is_err())
                return ToolResult::fail("Failed: " + QString::fromStdString(r.error()));
            return ToolResult::ok(QString("Connection %1").arg(enabled ? "enabled" : "disabled"),
                                  QJsonObject{{"id", id}, {"enabled", enabled}});
        };
        tools.push_back(std::move(t));
    }

    // ── ds_list_connectors ───────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ds_list_connectors";
        t.description = "List all available connector types with their ID, name, category, "
                        "auth requirement, and testability. Use this to discover valid provider IDs.";
        t.category = "data-sources";
        t.input_schema.properties = QJsonObject{};
        t.handler = [](const QJsonObject&) -> ToolResult {
            const auto& all = ConnectorRegistry::instance().all();
            QJsonArray arr;
            for (const auto& c : all) {
                arr.append(QJsonObject{
                    {"id", c.id},
                    {"name", c.name},
                    {"category", category_str(c.category)},
                    {"category_label", category_label(c.category)},
                    {"icon", c.icon},
                    {"requires_auth", c.requires_auth},
                    {"testable", c.testable},
                    {"field_count", static_cast<int>(c.fields.size())},
                });
            }
            return ToolResult::ok_data(arr);
        };
        tools.push_back(std::move(t));
    }

    // ── ds_get_connector ─────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ds_get_connector";
        t.description = "Get full connector details including all required fields and their types. "
                        "Use this before ds_add_connection to know what config fields to supply.";
        t.category = "data-sources";
        t.input_schema.properties = QJsonObject{
            {"id",
             QJsonObject{{"type", "string"}, {"description", "Connector ID (e.g. 'postgresql', 'yahoo_finance')"}}},
        };
        t.input_schema.required = {"id"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString id = args["id"].toString().trimmed();
            if (id.isEmpty())
                return ToolResult::fail("Missing 'id'");
            const auto* cfg = ConnectorRegistry::instance().get(id);
            if (!cfg)
                return ToolResult::fail("Connector '" + id + "' not found. Use ds_list_connectors to see valid IDs.");
            return ToolResult::ok_data(connector_to_json(*cfg));
        };
        tools.push_back(std::move(t));
    }

    // ── ds_search_connectors ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ds_search_connectors";
        t.description = "Search available connector types by name, description, or category keyword.";
        t.category = "data-sources";
        t.input_schema.properties = QJsonObject{
            {"query",
             QJsonObject{{"type", "string"}, {"description", "Search keyword (e.g. 'postgres', 'kafka', 'market')"}}},
        };
        t.input_schema.required = {"query"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString q = args["query"].toString().trimmed().toLower();
            if (q.isEmpty())
                return ToolResult::fail("Missing 'query'");
            const auto& all = ConnectorRegistry::instance().all();
            QJsonArray arr;
            for (const auto& c : all) {
                bool match = c.id.toLower().contains(q) || c.name.toLower().contains(q) ||
                             c.description.toLower().contains(q) || category_str(c.category).contains(q) ||
                             category_label(c.category).toLower().contains(q);
                if (match) {
                    arr.append(QJsonObject{
                        {"id", c.id},
                        {"name", c.name},
                        {"category", category_str(c.category)},
                        {"icon", c.icon},
                        {"description", c.description},
                        {"requires_auth", c.requires_auth},
                    });
                }
            }
            return ToolResult::ok(QString("Found %1 connector(s)").arg(arr.size()), arr);
        };
        tools.push_back(std::move(t));
    }

    // ── ds_connectors_by_category ────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ds_connectors_by_category";
        t.description = "List connector types filtered by category. "
                        "Valid categories: database, api, file, streaming, cloud, "
                        "timeseries, market-data, search, warehouse, alt-data, open-banking.";
        t.category = "data-sources";
        t.input_schema.properties = QJsonObject{
            {"category", QJsonObject{{"type", "string"},
                                     {"description", "Category string: database | api | file | streaming | cloud | "
                                                     "timeseries | market-data | search | warehouse"}}},
        };
        t.input_schema.required = {"category"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString cat = args["category"].toString().trimmed().toLower();
            if (cat.isEmpty())
                return ToolResult::fail("Missing 'category'");

            // Map string -> enum
            Category cat_enum;
            if (cat == "database")
                cat_enum = Category::Database;
            else if (cat == "api")
                cat_enum = Category::Api;
            else if (cat == "file")
                cat_enum = Category::File;
            else if (cat == "streaming")
                cat_enum = Category::Streaming;
            else if (cat == "cloud")
                cat_enum = Category::Cloud;
            else if (cat == "timeseries")
                cat_enum = Category::TimeSeries;
            else if (cat == "market-data")
                cat_enum = Category::MarketData;
            else if (cat == "search")
                cat_enum = Category::SearchAnalytics;
            else if (cat == "warehouse")
                cat_enum = Category::DataWarehouse;
            else if (cat == "alt-data")
                cat_enum = Category::AlternativeData;
            else if (cat == "open-banking")
                cat_enum = Category::OpenBanking;
            else
                return ToolResult::fail("Unknown category '" + cat +
                                        "'. Valid: database, api, file, streaming, cloud, timeseries, market-data, "
                                        "search, warehouse, alt-data, open-banking");

            auto filtered = ConnectorRegistry::instance().by_category(cat_enum);
            QJsonArray arr;
            for (const auto& c : filtered) {
                arr.append(QJsonObject{
                    {"id", c.id},
                    {"name", c.name},
                    {"icon", c.icon},
                    {"description", c.description},
                    {"requires_auth", c.requires_auth},
                    {"testable", c.testable},
                    {"field_count", static_cast<int>(c.fields.size())},
                });
            }
            return ToolResult::ok_data(arr);
        };
        tools.push_back(std::move(t));
    }

    // ── ds_test_connection ───────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ds_test_connection";
        t.description = "Test TCP/HTTP reachability for a saved connection. "
                        "Returns success/failure and latency. Note: only confirms network "
                        "reachability — does not validate API keys or credentials.";
        t.category = "data-sources";
        t.input_schema.properties = QJsonObject{
            {"id", QJsonObject{{"type", "string"}, {"description", "Connection ID to test"}}},
        };
        t.input_schema.required = {"id"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString id = args["id"].toString().trimmed();
            if (id.isEmpty())
                return ToolResult::fail("Missing 'id'");

            auto r = DataSourceRepository::instance().get(id);
            if (r.is_err())
                return ToolResult::fail("Connection not found: " + QString::fromStdString(r.error()));

            const DataSource& ds = r.value();
            const auto* cfg = ConnectorRegistry::instance().get(ds.provider);
            if (!cfg)
                return ToolResult::fail("Connector definition not found for provider: " + ds.provider);
            if (!cfg->testable)
                return ToolResult::fail("This connector does not support connectivity testing.");

            // Parse config JSON
            QJsonObject config_obj;
            {
                QJsonParseError pe;
                auto doc = QJsonDocument::fromJson(ds.config.toUtf8(), &pe);
                if (!doc.isNull() && doc.isObject())
                    config_obj = doc.object();
            }

            // Resolve test host/port from config
            QString host;
            int port = 80;

            // Try explicit URL fields first
            for (const QString& key : {"url", "baseUrl", "endpoint", "wsdlUrl", "serviceRoot", "jobManagerUrl"}) {
                QString val = config_obj[key].toString().trimmed();
                if (!val.isEmpty()) {
                    QUrl u(val);
                    if (u.isValid() && !u.host().isEmpty()) {
                        host = u.host();
                        port = u.port(u.scheme() == "https" ? 443 : 80);
                        break;
                    }
                }
            }

            // Fallback: direct host/port fields
            if (host.isEmpty()) {
                QString h = config_obj["host"].toString().trimmed();
                if (!h.isEmpty()) {
                    host = h;
                    port = config_obj["port"].toInt(80);
                }
            }

            // Fallback: connectionString (MongoDB style)
            if (host.isEmpty()) {
                QString cs = config_obj["connectionString"].toString();
                QUrl u(cs);
                if (u.isValid() && !u.host().isEmpty()) {
                    host = u.host();
                    port = u.port(27017);
                }
            }

            // Fallback: comma-separated host lists (kafka brokers, nats servers,
            //           cassandra contactPoints, etcd hosts, memcached servers)
            if (host.isEmpty()) {
                for (const QString& key : {"brokers", "servers", "contactPoints", "hosts", "zkQuorum"}) {
                    QString val = config_obj[key].toString().trimmed();
                    if (!val.isEmpty()) {
                        // Take the first entry — "host:port" or just "host"
                        QString first = val.split(',').first().trimmed();
                        int colon = first.lastIndexOf(':');
                        if (colon > 0) {
                            host = first.left(colon);
                            port = first.mid(colon + 1).toInt();
                        } else {
                            host = first;
                            port = config_obj["port"].toInt(80);
                        }
                        break;
                    }
                }
            }

            // Fallback: bolt/neo4j URI in "uri" field
            if (host.isEmpty()) {
                QString uri = config_obj["uri"].toString().trimmed();
                if (!uri.isEmpty()) {
                    QUrl u(uri);
                    if (u.isValid() && !u.host().isEmpty()) {
                        host = u.host();
                        port = u.port(7687);
                    }
                }
            }

            if (host.isEmpty())
                return ToolResult::fail("No testable endpoint found in connection config. "
                                        "Ensure required fields (host, url, etc.) are filled in.");

            // TCP probe (synchronous — MCP handlers run off UI thread via McpService)
            QTcpSocket sock;
            sock.connectToHost(host, static_cast<quint16>(port));
            bool ok = sock.waitForConnected(5000);
            sock.disconnectFromHost();

            if (ok) {
                return ToolResult::ok(
                    QString("Connection reachable: %1:%2").arg(host).arg(port),
                    QJsonObject{{"host", host},
                                {"port", port},
                                {"reachable", true},
                                {"note", "TCP reachability confirmed. API key validity is not verified."}});
            } else {
                return ToolResult::fail(QString("Cannot reach %1:%2 — %3").arg(host).arg(port).arg(sock.errorString()));
            }
        };
        tools.push_back(std::move(t));
    }

    // ── ds_stats ─────────────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "ds_stats";
        t.description = "Return summary statistics for the Data Sources tab: "
                        "total connector types, configured connections, active (enabled) connections, "
                        "and how many connector types require authentication.";
        t.category = "data-sources";
        t.input_schema.properties = QJsonObject{};
        t.handler = [](const QJsonObject&) -> ToolResult {
            const auto& all_connectors = ConnectorRegistry::instance().all();
            int total_connectors = all_connectors.size();
            int auth_required = 0;
            for (const auto& c : all_connectors)
                if (c.requires_auth)
                    ++auth_required;

            auto cr = DataSourceRepository::instance().list_all();
            int configured = 0;
            int active = 0;
            if (cr.is_ok()) {
                configured = cr.value().size();
                for (const auto& ds : cr.value())
                    if (ds.enabled)
                        ++active;
            }

            return ToolResult::ok_data(QJsonObject{
                {"total_connector_types", total_connectors},
                {"auth_required_connectors", auth_required},
                {"configured_connections", configured},
                {"active_connections", active},
                {"disabled_connections", configured - active},
            });
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
