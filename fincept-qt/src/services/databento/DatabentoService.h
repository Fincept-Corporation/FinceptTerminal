#pragma once
// DatabentoService — fetches institutional market data via databento_provider.py
// Follows P4 (PythonRunner), P6 (service layer), P8 (QPointer async), P11 (cache)

#include "screens/surface_analytics/SurfaceTypes.h"

#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <string>
#include <vector>

namespace fincept {

// Metadata types for the Surface Analytics control panel
struct DbPublisher {
    int publisher_id = 0;
    QString dataset;
    QString venue;
    QString description;
};

struct DbDatasetRange {
    QString dataset;
    QDate start;
    QDate end;
};

struct DbCostQuery {
    QString dataset;
    QStringList symbols;
    QString schema;
    QDate start;
    QDate end;
    QString stype_in = "raw_symbol";
};

struct DbCostResult {
    bool success = false;
    QString error;
    qint64 record_count = 0;
    double cost_usd = 0.0;
};

// Unified parameters for surface fetches. Replaces the per-method
// (symbol, spot, days, root_symbol, ...) overloads. The screen builds one
// of these from the SurfaceControlPanel state and passes it to fetch_with_params().
struct DatabentoFetchParams {
    QString chart_type;     // String form of ChartType, e.g. "Volatility"
    QString symbol;         // Underlying for option/futures surfaces
    QStringList basket;     // Multi-symbol basket for risk surfaces
    QString dataset;        // Optional override; capability default if empty
    QString schema;         // Optional override; capability default if empty
    QDate start_date;
    QDate end_date;
    int strike_window_pct = 20;
    int dte_min = 7;
    int dte_max = 365;
    QStringList venues;     // Optional OPRA-venue filter
    QString iv_method = "Brent";
    float spot_override = 0.0f;  // 0 = lookup via DataHub::peek
};

// Result of a Databento fetch — emitted via signals
struct DatabentoOhlcvResult {
    bool success = false;
    QString error;
    // symbol -> list of {date, open, high, low, close, volume}
    QHash<QString, QVector<QJsonObject>> data;
};

struct DatabentoVolSurfaceResult {
    bool success = false;
    QString error;
    surface::VolatilitySurfaceData vol;
    surface::GreeksSurfaceData delta, gamma, vega, theta;
    surface::SkewSurfaceData skew;
};

struct DatabentoFuturesResult {
    bool success = false;
    QString error;
    surface::CommodityForwardData forward;
    surface::ContangoData contango;
};

// Generic surface result — carries any strike×expiry or row×col surface
struct DatabentoSurfaceResult {
    bool success = false;
    QString error;
    QString type;                      // e.g. "local_vol", "liquidity", "implied_dividend", etc.
    std::vector<float> x_axis;         // strikes or row labels (numeric)
    std::vector<int> y_axis;           // expirations or col labels
    std::vector<std::string> x_labels; // string labels (if non-numeric)
    std::vector<std::string> y_labels; // string labels (if non-numeric)
    std::vector<std::vector<float>> z;
};

class DatabentoService : public QObject {
    Q_OBJECT
  public:
    static DatabentoService& instance();

    // API key management (stored in SecureStorage)
    bool has_api_key() const;
    void set_api_key(const QString& key);
    QString api_key() const;
    void clear_api_key();

    // Test connection
    void test_connection(); // emits connection_tested(bool ok, QString msg)

    // ── Metadata (callback-based; results cached in-memory) ─────────────────
    // The control panel uses these to populate dataset/schema/publisher pickers
    // and to suggest sane default date ranges before a fetch is dispatched.
    void list_datasets(std::function<void(QStringList)> cb);
    void list_schemas(const QString& dataset, std::function<void(QStringList)> cb);
    void list_publishers(const QString& dataset, std::function<void(QList<DbPublisher>)> cb);
    void get_dataset_range(const QString& dataset, std::function<void(DbDatasetRange)> cb);
    void get_cost(const DbCostQuery& q, std::function<void(DbCostResult)> cb);
    void search_symbols(const QString& query, const QString& dataset,
                        std::function<void(QStringList)> cb);

    // ── Unified surface fetch ───────────────────────────────────────────────
    // Routes to the right Python command based on params.chart_type.
    // Spot for option surfaces is read from DataHub::peek("market:quote:<sym>")
    // unless params.spot_override > 0. Result arrives through the existing
    // vol_surface_ready / ohlcv_ready / futures_ready / surface_ready signals
    // matching the request's chart family.
    void fetch_with_params(const DatabentoFetchParams& params);

    // Fetch historical OHLCV for a list of symbols (used for correlation, PCA, drawdown, beta)
    void fetch_ohlcv(const QStringList& symbols, int days = 60);

    // Fetch options chain → vol surface + greeks + skew for one underlying
    void fetch_options_surface(const QString& symbol, float spot);

    // Fetch futures term structure → commodity forwards + contango
    void fetch_futures_term_structure(const QStringList& commodities);

    // ── Extended surface fetches ──────────────────────────────────────────
    void fetch_local_vol(const QString& symbol, float spot);
    void fetch_implied_dividend(const QString& symbol, float spot);
    void fetch_liquidity(const QString& symbol, float spot);
    void fetch_commodity_vol(const QString& root_symbol);
    void fetch_crack_spread();
    void fetch_stress_test(const QStringList& symbols);
    void fetch_yield_curve();
    void fetch_forward_rate();
    void fetch_rate_path();
    void fetch_fx_forward_points();

    // Cache access (returns last fetched data, empty if never fetched)
    const DatabentoOhlcvResult& last_ohlcv() const { return cached_ohlcv_; }
    const DatabentoVolSurfaceResult& last_vol() const { return cached_vol_; }
    const DatabentoFuturesResult& last_futures() const { return cached_futures_; }
    QDateTime last_fetch_time() const { return last_fetch_time_; }

  signals:
    void connection_tested(bool ok, const QString& message);
    void ohlcv_ready(const DatabentoOhlcvResult& result);
    void vol_surface_ready(const DatabentoVolSurfaceResult& result);
    void futures_ready(const DatabentoFuturesResult& result);
    void surface_ready(const DatabentoSurfaceResult& result);
    void fetch_started(const QString& description);
    void fetch_failed(const QString& error);
    // Emits the raw Python stdout for the most recent fetch so the data
    // inspector can show it in the "View raw response" modal.
    void raw_response(const QString& script_command, const QString& raw_stdout);

  private:
    DatabentoService();
    void run_script(const QStringList& args, std::function<void(bool, const QJsonObject&)> cb);

    static surface::VolatilitySurfaceData parse_vol_surface(const QJsonObject& j, const std::string& symbol,
                                                            float spot);
    static surface::GreeksSurfaceData parse_greek_surface(const QJsonObject& j, const std::string& greek,
                                                          const std::string& symbol, float spot);
    static surface::SkewSurfaceData parse_skew(const QJsonObject& j, const std::string& symbol);
    static surface::CommodityForwardData parse_commodity_forward(const QJsonObject& j);
    static surface::ContangoData parse_contango(const QJsonObject& j);

    DatabentoOhlcvResult cached_ohlcv_;
    DatabentoVolSurfaceResult cached_vol_;
    DatabentoFuturesResult cached_futures_;
    QDateTime last_fetch_time_;

    // Metadata caches — populated lazily on first request, valid for app session
    QStringList cached_datasets_;
    QHash<QString, QStringList> cached_schemas_;     // dataset → schemas
    QHash<QString, QList<DbPublisher>> cached_publishers_;
    QHash<QString, DbDatasetRange> cached_ranges_;

    static constexpr const char* SECURE_KEY = "databento.api_key";
    static constexpr const char* SCRIPT = "databento_provider.py";
};

} // namespace fincept
