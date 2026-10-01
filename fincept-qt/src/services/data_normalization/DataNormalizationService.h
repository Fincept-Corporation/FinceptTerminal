#pragma once
#include "core/result/Result.h"
#include "storage/repositories/DataMappingRepository.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>
#include <optional>

namespace fincept::services {

struct NormalizedRecord {
    QString id;
    QString mapping_id;
    QString source_id;
    QString schema_name;
    QJsonObject normalized; // validated, schema-conformant data
    QJsonObject raw;        // original API response
    QStringList errors;     // validation errors (empty = clean)
    QString extracted_at;
    bool from_cache = false; // served from normalized_data within the mapping's cache TTL (no HTTP call)
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
    /// When the mapping has caching enabled and a record younger than its TTL
    /// exists, that record is returned (from_cache = true) without a request —
    /// pass force_refresh to skip that. On failure the record's `errors` carries
    /// the reason (it is never empty when ok == false).
    void fetch_and_normalize(const DataMapping& mapping, NormalizeCallback cb, bool force_refresh = false);

    /// Normalize already-fetched raw JSON (no HTTP call). Useful for testing.
    NormalizedRecord normalize_raw(const DataMapping& mapping, const QJsonDocument& raw);

    /// Load the most recent normalized record for a mapping.
    std::optional<NormalizedRecord> latest_for_mapping(const QString& mapping_id);

    /// Load all normalized records for a schema (e.g. all OHLCV records).
    QVector<NormalizedRecord> records_for_schema(const QString& schema_name);

    // ── Request helpers (shared with the Data Mapping screen's TEST API button) ──

    /// Per-request HTTP headers for a mapping: the AUTH VALUE rendered for its
    /// auth type (Bearer/OAuth2 -> Authorization: Bearer, API Key -> X-API-Key,
    /// Basic Auth -> Authorization: Basic) followed by the "Name: value" lines
    /// of the HEADERS box, which win on conflict. Malformed lines are skipped.
    /// SECURITY: values are credentials — never log the result.
    static QMap<QByteArray, QByteArray> build_request_headers(const QString& headers_text, const QString& auth_type,
                                                              const QString& auth_token);

    /// base + endpoint with exactly one '/' between them; an endpoint that is
    /// already an absolute http(s) URL is returned as-is.
    static QString join_url(const QString& base, const QString& endpoint);

    /// True for absolute http:// or https:// URLs. HttpClient treats anything
    /// else as relative to the Fincept API and would attach the user's Fincept
    /// session headers to it, so mappings must be rejected before dispatch.
    static bool is_http_url(const QString& url);

    /// True when `expression` parses with the built-in JSONPath subset (empty and
    /// "$" count as valid — they select the whole document).
    static bool expression_supported(const QString& expression);

    /// True when `transform` is empty or one of the built-in transform names.
    static bool transform_supported(const QString& transform);

  signals:
    void normalization_complete(const NormalizedRecord& record);
    void normalization_failed(const QString& mapping_id, const QString& error);

  private:
    DataNormalizationService();

    // ── Field extraction ───────────────────────────────────────────────────
    // Extract a value from a JSON document using a JSONPath-style expression.
    // Supports: $.key, $.a.b, $[0].key, ['quoted key'], [-1], and projection
    // through [*] / .* (returns an array of the remaining path applied to every
    // element), plus a trailing ~ for property names. Anything else (recursive
    // descent, filters, slices) yields Undefined rather than a wrong node.
    static QJsonValue extract_jsonpath(const QJsonValue& root, const QString& path);

    // ── Transforms ────────────────────────────────────────────────────────
    // Apply a named transform function to a raw extracted value (element-wise
    // when the value is an array).
    // Supported: to_number, to_string, unix_ms_to_iso, unix_s_to_iso,
    // unix_to_iso (seconds or ms, auto-detected), upper, lower, abs_value
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
};

} // namespace fincept::services
