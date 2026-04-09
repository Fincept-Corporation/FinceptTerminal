// Relational Database connectors (9): PostgreSQL, MySQL, MariaDB, SQLite, Oracle, MSSQL, CockroachDB, Snowflake,
// Vertica
#include "screens/data_sources/ConnectorRegistry.h"

namespace fincept::screens::datasources {

static const FieldDef F_HOST = {"host", "Host", FieldType::Text, "localhost", true, "", {}};
static const FieldDef F_USERNAME = {"username", "Username", FieldType::Text, "", true, "", {}};
static const FieldDef F_PASSWORD = {"password", "Password", FieldType::Password, "", true, "", {}};
static const FieldDef F_DATABASE = {"database", "Database", FieldType::Text, "mydb", true, "", {}};
static const FieldDef F_SSL = {"ssl", "Use SSL", FieldType::Checkbox, "", false, "false", {}};

static QVector<ConnectorConfig> relational_configs() {
    return {
        {"postgresql",
         "PostgreSQL",
         "postgresql",
         Category::Database,
         "P",
         "#336791",
         "Open-source relational database with advanced features",
         true,
         true,
         {F_HOST,
          {"port", "Port", FieldType::Number, "5432", true, "5432", {}},
          F_DATABASE,
          F_USERNAME,
          F_PASSWORD,
          F_SSL}},

        {"mysql",
         "MySQL",
         "mysql",
         Category::Database,
         "M",
         "#4479A1",
         "Popular open-source relational database",
         true,
         true,
         {F_HOST, {"port", "Port", FieldType::Number, "3306", true, "3306", {}}, F_DATABASE, F_USERNAME, F_PASSWORD}},

        {"mariadb",
         "MariaDB",
         "mariadb",
         Category::Database,
         "M",
         "#003545",
         "MySQL-compatible relational database fork",
         true,
         true,
         {F_HOST, {"port", "Port", FieldType::Number, "3306", true, "3306", {}}, F_DATABASE, F_USERNAME, F_PASSWORD}},

        {"sqlite",
         "SQLite",
         "sqlite",
         Category::Database,
         "S",
         "#003B57",
         "Lightweight embedded SQL database",
         false, // file-based: no network endpoint to test
         false,
         {{"filepath", "Database File Path", FieldType::Text, "/path/to/database.db", true, "", {}},
          {"readonly", "Read Only", FieldType::Checkbox, "", false, "false", {}}}},

        {"oracle",
         "Oracle Database",
         "oracle",
         Category::Database,
         "O",
         "#F80000",
         "Enterprise-grade relational database",
         true,
         true,
         {F_HOST,
          {"port", "Port", FieldType::Number, "1521", true, "1521", {}},
          {"serviceName", "Service Name", FieldType::Text, "ORCL", true, "", {}},
          F_USERNAME,
          F_PASSWORD}},

        {"mssql",
         "Microsoft SQL Server",
         "mssql",
         Category::Database,
         "S",
         "#CC2927",
         "Microsoft enterprise database system",
         true,
         true,
         {F_HOST, {"port", "Port", FieldType::Number, "1433", true, "1433", {}}, F_DATABASE, F_USERNAME, F_PASSWORD}},

        {"cockroachdb",
         "CockroachDB",
         "cockroachdb",
         Category::Database,
         "C",
         "#6933FF",
         "Distributed SQL database for cloud applications",
         true,
         true,
         {F_HOST,
          {"port", "Port", FieldType::Number, "26257", true, "26257", {}},
          F_DATABASE,
          F_USERNAME,
          {"password", "Password", FieldType::Password, "", false, "", {}}}},

        {"vertica",
         "Vertica",
         "vertica",
         Category::Database,
         "V",
         "#005A9C",
         "Columnar analytics database",
         true,
         true,
         {F_HOST, {"port", "Port", FieldType::Number, "5433", true, "5433", {}}, F_DATABASE, F_USERNAME, F_PASSWORD}},
    };
}

static bool registered = [] {
    for (auto& c : relational_configs())
        ConnectorRegistry::instance().add(std::move(c));
    return true;
}();

} // namespace fincept::screens::datasources
