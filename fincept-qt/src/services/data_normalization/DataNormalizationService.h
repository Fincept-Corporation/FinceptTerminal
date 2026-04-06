#pragma once
#include "core/result/Result.h"
#include "storage/repositories/DataMappingRepository.h"

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVector>

#include <functional>

namespace fincept::services {

struct NormalizedRecord {
    QString id;
    QString mapping_id;
    QString source_id;
    QString schema_name;
    QJsonObject normalized;   // validated, schema-conformant data
    QJsonObject raw;          // original API response
    QStringList errors;       // validation errors (empty = clean)
    QString extracted_at;
};

/// Fetches raw data from a provider using a DataMapping config,
/// applies JSONPath field extraction + transforms, validates against schema,
/// and persists the result to the normalized_data table.
class DataNormalizationService : public QObject {
    Q_OBJECT
  public:
    static DataNormalizationService& instance();

    using NormalizeCallback = std::function<void(bool ok, NormalizedRecord result)>;

    /// Fetch from provider, normalize, persist, and invoke callback.
    void fetch_and_normalize(const DataMapping& mapping, NormalizeCallback cb);

    /// Normalize already-fetched raw JSON (no HTTP call). Useful for testing.
    NormalizedRecord normalize_raw(const DataMapping& mapping, const QJsonDocument& raw);

    /// Load the most recent normalized record for a mapping.
    std::optional<NormalizedRecord> latest_for_mapping(const QString& mapping_id);

    /// Load all normalized records for a schema (e.g. all OHLCV records).
    QVector<NormalizedRecord> records_for_schema(const QString& schema_name);

  signals:
    void normalization_complete(const NormalizedRecord& record);
    void normalization_failed(const QString& mapping_id, const QString& error);

  private:
    DataNormalizationService();

    // ── Field extraction ───────────────────────────────────────────────────
    // Extract a value from a JSON document using a simple JSONPath expression.
    // Supports: $.key, $.key.subkey, $[0].key, $[*][N] (array index projection).
    static QJsonValue extract_jsonpath(const QJsonValue& root, const QString& path);

    // ── Transforms ────────────────────────────────────────────────────────
    // Apply a named transform function to a raw extracted value.
    // Supported: to_number, to_string, unix_ms_to_iso, unix_s_to_iso, upper, lower, abs_value
    static QJsonValue apply_transform(const QJsonValue& val, const QString& transform);

    // ── Schema validation ─────────────────────────────────────────────────
    // Check that all required fields for the schema are present and non-null.
    // Returns list of validation error strings (empty = valid).
    static QStringList validate_schema(const QJsonObject& data, const QString& schema_name);

    // ── Persistence ───────────────────────────────────────────────────────
    static void persist(const DataMapping& mapping, const NormalizedRecord& record);

    // ── HTTP helpers ──────────────────────────────────────────────────────
    // Build the full request URL from base_url + endpoint.
    static QString build_url(const DataMapping& mapping);
    // Apply auth_type config to HttpClient before making a request.
    void apply_auth(const DataMapping& mapping);
};

} // namespace fincept::services
