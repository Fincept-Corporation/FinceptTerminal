#pragma once
// DatabentoService — fetches institutional market data via databento_provider.py
// Follows P4 (PythonRunner), P6 (service layer), P8 (QPointer async), P11 (cache)

#include "screens/surface_analytics/SurfaceTypes.h"

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>

#include <string>
#include <vector>

namespace fincept {

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
    QString type; // e.g. "local_vol", "liquidity", "implied_dividend", etc.
    std::vector<float> x_axis;              // strikes or row labels (numeric)
    std::vector<int> y_axis;                // expirations or col labels
    std::vector<std::string> x_labels;      // string labels (if non-numeric)
    std::vector<std::string> y_labels;      // string labels (if non-numeric)
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

    static constexpr const char* SECURE_KEY = "databento.api_key";
    static constexpr const char* SCRIPT = "databento_provider.py";
};

} // namespace fincept
