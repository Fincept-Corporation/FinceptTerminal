// Time Series Database connectors (8): InfluxDB, QuestDB, TimescaleDB, Prometheus,
// VictoriaMetrics, OpenTSDB, kdb+, ClickHouse
#include "screens/data_sources/ConnectorRegistry.h"

namespace fincept::screens::datasources {

static QVector<ConnectorConfig> timeseries_configs() {
    return {
        {"influxdb",
         "InfluxDB",
         "influxdb",
         Category::TimeSeries,
         "I",
         "#22ADF6",
         "Time series database for metrics and events",
         true,
         true,
         {{"url", "URL", FieldType::Url, "http://localhost:8086", true, "", {}},
          {"token", "Token", FieldType::Password, "your-token", true, "", {}},
          {"organization", "Organization", FieldType::Text, "my-org", true, "", {}},
          {"bucket", "Bucket", FieldType::Text, "my-bucket", true, "", {}}}},

        {"questdb",
         "QuestDB",
         "questdb",
         Category::TimeSeries,
         "Q",
         "#FF9D00",
         "High-performance time series database",
         true,
         false,
         {{"host", "Host", FieldType::Text, "localhost", true, "", {}},
          {"port", "HTTP Port", FieldType::Number, "9000", true, "9000", {}},
          {"username", "Username", FieldType::Text, "admin", false, "", {}},
          {"password", "Password", FieldType::Password, "", false, "", {}}}},

        {"timescaledb",
         "TimescaleDB",
         "timescaledb",
         Category::TimeSeries,
         "T",
         "#FDB515",
         "PostgreSQL extension for time series",
         true,
         true,
         {{"host", "Host", FieldType::Text, "localhost", true, "", {}},
          {"port", "Port", FieldType::Number, "5432", true, "5432", {}},
          {"database", "Database", FieldType::Text, "tsdb", true, "", {}},
          {"username", "Username", FieldType::Text, "postgres", true, "", {}},
          {"password", "Password", FieldType::Password, "", true, "", {}}}},

        {"prometheus",
         "Prometheus",
         "prometheus",
         Category::TimeSeries,
         "P",
         "#E6522C",
         "Monitoring system and time series database",
         true,
         false,
         {{"url", "URL", FieldType::Url, "http://localhost:9090", true, "", {}},
          {"username", "Username", FieldType::Text, "", false, "", {}},
          {"password", "Password", FieldType::Password, "", false, "", {}}}},

        {"victoriametrics",
         "VictoriaMetrics",
         "victoriametrics",
         Category::TimeSeries,
         "V",
         "#621CEA",
         "Fast and cost-effective time series database",
         true,
         false,
         {{"url", "URL", FieldType::Url, "http://localhost:8428", true, "", {}}}},

        {"opentsdb",
         "OpenTSDB",
         "opentsdb",
         Category::TimeSeries,
         "O",
         "#44B4E5",
         "Distributed time series database on HBase",
         true,
         false,
         {{"url", "URL", FieldType::Url, "http://localhost:4242", true, "", {}}}},

        {"kdb",
         "kdb+",
         "kdb",
         Category::TimeSeries,
         "K",
         "#0069B4",
         "High-performance time series database for financial data",
         true,
         true,
         {{"host", "Host", FieldType::Text, "localhost", true, "", {}},
          {"port", "Port", FieldType::Number, "5000", true, "5000", {}},
          {"username", "Username", FieldType::Text, "user", false, "", {}},
          {"password", "Password", FieldType::Password, "", false, "", {}}}},

        {"clickhouse",
         "ClickHouse",
         "clickhouse",
         Category::TimeSeries,
         "C",
         "#FFCC00",
         "Fast open-source columnar database",
         true,
         true,
         {{"host", "Host", FieldType::Text, "localhost", true, "", {}},
          {"port", "HTTP Port", FieldType::Number, "8123", true, "8123", {}},
          {"database", "Database", FieldType::Text, "default", true, "", {}},
          {"username", "Username", FieldType::Text, "default", true, "", {}},
          {"password", "Password", FieldType::Password, "", false, "", {}}}},
    };
}

static bool registered = [] {
    for (auto& c : timeseries_configs())
        ConnectorRegistry::instance().add(std::move(c));
    return true;
}();

} // namespace fincept::screens::datasources
