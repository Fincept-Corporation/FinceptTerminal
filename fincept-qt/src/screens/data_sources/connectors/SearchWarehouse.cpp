// Search & Analytics (5) + Data Warehouses (4):
// Elasticsearch, OpenSearch, Solr, Algolia, MeiliSearch, BigQuery, Redshift, Databricks, Synapse
#include "screens/data_sources/ConnectorRegistry.h"

namespace fincept::screens::datasources {

static QVector<ConnectorConfig> search_warehouse_configs() {
    return {
        // ── Search & Analytics ──
        {"elasticsearch",
         "Elasticsearch",
         "elasticsearch",
         Category::SearchAnalytics,
         "E",
         "#005571",
         "Distributed search and analytics engine",
         true,
         false,
         {{"node", "Node URL", FieldType::Url, "http://localhost:9200", true, "", {}},
          {"username", "Username", FieldType::Text, "elastic", false, "", {}},
          {"password", "Password", FieldType::Password, "", false, "", {}},
          {"index", "Default Index", FieldType::Text, "my-index", false, "", {}}}},

        {"opensearch",
         "OpenSearch",
         "opensearch",
         Category::SearchAnalytics,
         "O",
         "#003B5C",
         "Open-source fork of Elasticsearch",
         true,
         false,
         {{"node", "Node URL", FieldType::Url, "https://localhost:9200", true, "", {}},
          {"username", "Username", FieldType::Text, "admin", false, "", {}},
          {"password", "Password", FieldType::Password, "", false, "", {}}}},

        {"solr",
         "Apache Solr",
         "solr",
         Category::SearchAnalytics,
         "S",
         "#D9411E",
         "Enterprise search platform",
         true,
         false,
         {{"host", "Host", FieldType::Url, "http://localhost:8983/solr", true, "", {}},
          {"collection", "Collection", FieldType::Text, "my-collection", false, "", {}}}},

        {"algolia",
         "Algolia",
         "algolia",
         Category::SearchAnalytics,
         "A",
         "#003DFF",
         "Search-as-a-Service platform",
         true,
         true,
         {{"appId", "Application ID", FieldType::Text, "your-app-id", true, "", {}},
          {"apiKey", "API Key", FieldType::Password, "your-api-key", true, "", {}},
          {"index", "Default Index", FieldType::Text, "my-index", false, "", {}}}},

        {"meilisearch",
         "MeiliSearch",
         "meilisearch",
         Category::SearchAnalytics,
         "M",
         "#FF5CAA",
         "Fast, open-source search engine",
         true,
         true,
         {{"host", "Host", FieldType::Url, "http://localhost:7700", true, "", {}},
          {"apiKey", "API Key", FieldType::Password, "your-master-key", true, "", {}},
          {"index", "Default Index", FieldType::Text, "my-index", false, "", {}}}},

        // ── Data Warehouses ──
        {"bigquery",
         "Google BigQuery",
         "bigquery",
         Category::DataWarehouse,
         "B",
         "#4285F4",
         "Google Cloud data warehouse",
         true,
         true,
         {{"projectId", "Project ID", FieldType::Text, "my-project", true, "", {}},
          {"dataset", "Dataset", FieldType::Text, "my_dataset", true, "", {}},
          {"credentials", "Service Account JSON", FieldType::Textarea, "Paste JSON credentials", true, "", {}}}},

        {"redshift",
         "Amazon Redshift",
         "redshift",
         Category::DataWarehouse,
         "R",
         "#8C4FFF",
         "AWS cloud data warehouse",
         true,
         true,
         {{"host", "Host", FieldType::Text, "cluster.region.redshift.amazonaws.com", true, "", {}},
          {"port", "Port", FieldType::Number, "5439", true, "5439", {}},
          {"database", "Database", FieldType::Text, "dev", true, "", {}},
          {"username", "Username", FieldType::Text, "admin", true, "", {}},
          {"password", "Password", FieldType::Password, "", true, "", {}}}},

        {"databricks",
         "Databricks",
         "databricks",
         Category::DataWarehouse,
         "D",
         "#FF3621",
         "Unified analytics platform",
         true,
         true,
         {{"serverHostname", "Server Hostname", FieldType::Text, "adb-xxx.azuredatabricks.net", true, "", {}},
          {"httpPath", "HTTP Path", FieldType::Text, "/sql/1.0/warehouses/xxx", true, "", {}},
          {"accessToken", "Access Token", FieldType::Password, "dapi...", true, "", {}}}},

        {"synapse",
         "Azure Synapse",
         "synapse",
         Category::DataWarehouse,
         "S",
         "#0078D4",
         "Azure analytics service",
         true,
         true,
         {{"server", "Server", FieldType::Text, "workspace.sql.azuresynapse.net", true, "", {}},
          {"database", "Database", FieldType::Text, "my_db", true, "", {}},
          {"username", "Username", FieldType::Text, "sqladmin", true, "", {}},
          {"password", "Password", FieldType::Password, "", true, "", {}}}},
    };
}

static bool registered = [] {
    for (auto& c : search_warehouse_configs())
        ConnectorRegistry::instance().add(std::move(c));
    return true;
}();

} // namespace fincept::screens::datasources
