#pragma once
// SurfaceDatabentoPanel — Databento data source controls for Surface Analytics
// Follows P3 (show/hideEvent timers), P6 (connects to DatabentoService signals only)

#include "SurfaceTypes.h"
#include "services/databento/DatabentoService.h"
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QVBoxLayout>

namespace fincept::surface {

class SurfaceDatabentoPanel : public QWidget {
    Q_OBJECT
public:
    explicit SurfaceDatabentoPanel(QWidget* parent = nullptr);

    // Called by SurfaceAnalyticsScreen to wire up which surface is active
    void set_active_chart(ChartType type, const QString& symbol, float spot);

signals:
    // Emitted when Databento data arrives — screen connects to update surfaces
    void vol_surface_received(const fincept::DatabentoVolSurfaceResult& result);
    void ohlcv_received(const fincept::DatabentoOhlcvResult& result);
    void futures_received(const fincept::DatabentoFuturesResult& result);

protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

private slots:
    void on_fetch_clicked();
    void on_save_key_clicked();
    void on_connection_tested(bool ok, const QString& msg);
    void on_fetch_started(const QString& desc);
    void on_fetch_failed(const QString& err);
    void on_ohlcv_ready(const fincept::DatabentoOhlcvResult& r);
    void on_vol_ready(const fincept::DatabentoVolSurfaceResult& r);
    void on_futures_ready(const fincept::DatabentoFuturesResult& r);

private:
    void setup_ui();
    void update_key_status();
    void set_status(const QString& msg, bool is_error = false);
    bool needs_ohlcv() const;
    bool needs_vol_surface() const;
    bool needs_futures() const;

    // Widgets
    QLineEdit*   api_key_input_  = nullptr;
    QPushButton* save_key_btn_   = nullptr;
    QPushButton* test_btn_       = nullptr;
    QPushButton* fetch_btn_      = nullptr;
    QLabel*      key_status_lbl_ = nullptr;
    QLabel*      status_lbl_     = nullptr;
    QLabel*      last_fetch_lbl_ = nullptr;

    // State
    ChartType active_chart_ = ChartType::Volatility;
    QString   active_symbol_ = "SPY";
    float     active_spot_   = 450.0f;
    bool      connected_     = false;
};

} // namespace fincept::surface
