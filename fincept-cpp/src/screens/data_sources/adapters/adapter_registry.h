// Adapter Registry — maps connector type strings to adapter instances
// Replaces old connector_registry + connection_tester with unified adapter system
#pragma once

#include "base_adapter.h"

// All 91 adapters (header-only)
// Relational (9)
#include "postgresql_adapter.h"
#include "mysql_adapter.h"
#include "mariadb_adapter.h"
#include "sqlite_adapter.h"
#include "oracle_adapter.h"
#include "mssql_adapter.h"
#include "cockroachdb_adapter.h"
#include "snowflake_adapter.h"
#include "vertica_adapter.h"
// NoSQL (15)
#include "mongodb_adapter.h"
#include "redis_adapter.h"
#include "cassandra_adapter.h"
#include "couchdb_adapter.h"
#include "dynamodb_adapter.h"
#include "neo4j_adapter.h"
#include "arangodb_adapter.h"
#include "memcached_adapter.h"
#include "rethinkdb_adapter.h"
#include "firestore_adapter.h"
#include "hbase_adapter.h"
#include "scylladb_adapter.h"
#include "orientdb_adapter.h"
#include "cosmosdb_adapter.h"
#include "etcd_adapter.h"
// TimeSeries (8)
#include "influxdb_adapter.h"
#include "questdb_adapter.h"
#include "timescaledb_adapter.h"
#include "prometheus_adapter.h"
#include "victoriametrics_adapter.h"
#include "opentsdb_adapter.h"
#include "kdb_adapter.h"
#include "clickhouse_adapter.h"
// File (10)
#include "csv_adapter.h"
#include "excel_adapter.h"
#include "json_adapter.h"
#include "parquet_adapter.h"
#include "xml_adapter.h"
#include "avro_adapter.h"
#include "orc_adapter.h"
#include "feather_adapter.h"
#include "ftp_adapter.h"
#include "sftp_adapter.h"
// API (6)
#include "rest_adapter.h"
#include "graphql_adapter.h"
#include "grpc_adapter.h"
#include "soap_adapter.h"
#include "odata_adapter.h"
#include "ms_graph_adapter.h"
// Streaming (5)
#include "websocket_adapter.h"
#include "mqtt_adapter.h"
#include "kafka_adapter.h"
#include "rabbitmq_adapter.h"
#include "nats_adapter.h"
// Cloud (10)
#include "s3_adapter.h"
#include "gcp_storage_adapter.h"
#include "azure_blob_adapter.h"
#include "do_spaces_adapter.h"
#include "minio_adapter.h"
#include "backblaze_b2_adapter.h"
#include "wasabi_adapter.h"
#include "cloudflare_r2_adapter.h"
#include "oracle_cloud_storage_adapter.h"
#include "ibm_cloud_storage_adapter.h"
// Market Data (19)
#include "yahoo_finance_adapter.h"
#include "alpha_vantage_adapter.h"
#include "finnhub_adapter.h"
#include "iex_cloud_adapter.h"
#include "twelve_data_adapter.h"
#include "quandl_adapter.h"
#include "binance_adapter.h"
#include "coinbase_adapter.h"
#include "kraken_adapter.h"
#include "coinmarketcap_adapter.h"
#include "coingecko_adapter.h"
#include "eod_historical_adapter.h"
#include "tiingo_adapter.h"
#include "intrinio_adapter.h"
#include "fincept_adapter.h"
#include "reuters_adapter.h"
#include "marketstack_adapter.h"
#include "finage_adapter.h"
#include "tradier_adapter.h"
// Search & Analytics (5)
#include "elasticsearch_adapter.h"
#include "opensearch_adapter.h"
#include "solr_adapter.h"
#include "algolia_adapter.h"
#include "meilisearch_adapter.h"
// Data Warehouses (4)
#include "bigquery_adapter.h"
#include "redshift_adapter.h"
#include "databricks_adapter.h"
#include "azure_synapse_adapter.h"

#include <memory>
#include <unordered_map>

namespace fincept::data_sources::adapters {

class AdapterRegistry {
public:
    static AdapterRegistry& instance() {
        static AdapterRegistry inst;
        return inst;
    }

    // Get adapter by type string (e.g., "postgresql", "binance")
    BaseAdapter* get(const std::string& type) {
        auto it = adapters_.find(type);
        return (it != adapters_.end()) ? it->second.get() : nullptr;
    }

    // Get all definitions (for gallery display)
    std::vector<DataSourceDef> get_all_definitions() {
        std::vector<DataSourceDef> defs;
        defs.reserve(adapters_.size());
        for (auto& [type, adapter] : adapters_) {
            defs.push_back(adapter->get_definition());
        }
        return defs;
    }

    // Test a connection by type
    TestResult test_connection(const std::string& type,
                                const std::map<std::string, std::string>& config) {
        auto* adapter = get(type);
        if (!adapter) return {false, "Unknown connector type: " + type, 0};
        return adapter->test_connection(config);
    }

    // Async test
    std::future<TestResult> test_connection_async(const std::string& type,
                                                    const std::map<std::string, std::string>& config) {
        auto* adapter = get(type);
        if (!adapter) {
            std::promise<TestResult> p;
            p.set_value({false, "Unknown connector type: " + type, 0});
            return p.get_future();
        }
        return adapter->test_connection_async(config);
    }

    // Check if adapter exists
    bool has_adapter(const std::string& type) const {
        return adapters_.find(type) != adapters_.end();
    }

    // Get supported types
    std::vector<std::string> get_supported_types() const {
        std::vector<std::string> types;
        types.reserve(adapters_.size());
        for (auto& [type, _] : adapters_) types.push_back(type);
        return types;
    }

    size_t count() const { return adapters_.size(); }

    AdapterRegistry(const AdapterRegistry&) = delete;
    AdapterRegistry& operator=(const AdapterRegistry&) = delete;

private:
    AdapterRegistry() { register_all(); }

    template <typename T>
    void reg() {
        auto adapter = std::make_unique<T>();
        auto def = adapter->get_definition();
        adapters_[def.type] = std::move(adapter);
    }

    void register_all() {
        // Relational (9)
        reg<PostgreSQLAdapter>();
        reg<MySQLAdapter>();
        reg<MariaDBAdapter>();
        reg<SQLiteAdapter>();
        reg<OracleAdapter>();
        reg<MSSQLAdapter>();
        reg<CockroachDBAdapter>();
        reg<SnowflakeAdapter>();
        reg<VerticaAdapter>();
        // NoSQL (15)
        reg<MongoDBAdapter>();
        reg<RedisAdapter>();
        reg<CassandraAdapter>();
        reg<CouchDBAdapter>();
        reg<DynamoDBAdapter>();
        reg<Neo4jAdapter>();
        reg<ArangoDBAdapter>();
        reg<MemcachedAdapter>();
        reg<RethinkDBAdapter>();
        reg<FirestoreAdapter>();
        reg<HBaseAdapter>();
        reg<ScyllaDBAdapter>();
        reg<OrientDBAdapter>();
        reg<CosmosDBAdapter>();
        reg<EtcdAdapter>();
        // TimeSeries (8)
        reg<InfluxDBAdapter>();
        reg<QuestDBAdapter>();
        reg<TimescaleDBAdapter>();
        reg<PrometheusAdapter>();
        reg<VictoriaMetricsAdapter>();
        reg<OpenTSDBAdapter>();
        reg<KDBAdapter>();
        reg<ClickHouseAdapter>();
        // File (10)
        reg<CSVAdapter>();
        reg<ExcelAdapter>();
        reg<JSONAdapter>();
        reg<ParquetAdapter>();
        reg<XMLAdapter>();
        reg<AvroAdapter>();
        reg<ORCAdapter>();
        reg<FeatherAdapter>();
        reg<FTPAdapter>();
        reg<SFTPAdapter>();
        // API (6)
        reg<RESTAdapter>();
        reg<GraphQLAdapter>();
        reg<GRPCAdapter>();
        reg<SOAPAdapter>();
        reg<ODataAdapter>();
        reg<MSGraphAdapter>();
        // Streaming (5)
        reg<WebSocketAdapter>();
        reg<MQTTAdapter>();
        reg<KafkaAdapter>();
        reg<RabbitMQAdapter>();
        reg<NATSAdapter>();
        // Cloud (10)
        reg<S3Adapter>();
        reg<GCPStorageAdapter>();
        reg<AzureBlobAdapter>();
        reg<DOSpacesAdapter>();
        reg<MinIOAdapter>();
        reg<BackblazeB2Adapter>();
        reg<WasabiAdapter>();
        reg<CloudflareR2Adapter>();
        reg<OracleCloudStorageAdapter>();
        reg<IBMCloudStorageAdapter>();
        // Market Data (19)
        reg<YahooFinanceAdapter>();
        reg<AlphaVantageAdapter>();
        reg<FinnhubAdapter>();
        reg<IEXCloudAdapter>();
        reg<TwelveDataAdapter>();
        reg<QuandlAdapter>();
        reg<BinanceAdapter>();
        reg<CoinbaseAdapter>();
        reg<KrakenAdapter>();
        reg<CoinMarketCapAdapter>();
        reg<CoinGeckoAdapter>();
        reg<EODHistoricalAdapter>();
        reg<TiingoAdapter>();
        reg<IntrinioAdapter>();
        reg<FinceptAdapter>();
        reg<ReutersAdapter>();
        reg<MarketstackAdapter>();
        reg<FinageAdapter>();
        reg<TradierAdapter>();
        // Search & Analytics (5)
        reg<ElasticsearchAdapter>();
        reg<OpenSearchAdapter>();
        reg<SolrAdapter>();
        reg<AlgoliaAdapter>();
        reg<MeiliSearchAdapter>();
        // Data Warehouses (4)
        reg<BigQueryAdapter>();
        reg<RedshiftAdapter>();
        reg<DatabricksAdapter>();
        reg<AzureSynapseAdapter>();
    }

    std::unordered_map<std::string, std::unique_ptr<BaseAdapter>> adapters_;
};

} // namespace fincept::data_sources::adapters
