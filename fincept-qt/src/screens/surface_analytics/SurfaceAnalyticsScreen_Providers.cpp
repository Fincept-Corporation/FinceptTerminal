// src/screens/surface_analytics/SurfaceAnalyticsScreen_Providers.cpp
//
// Databento provider result handlers — vol surface, OHLCV, futures, generic
// surface, plus fetch-status / connection-test / raw-response logging.
//
// Part of the partial-class split of SurfaceAnalyticsScreen.cpp.

#include "SurfaceAnalyticsScreen.h"

#include "Surface3DWidget.h"
#include "SurfaceCapabilities.h"
#include "SurfaceControlPanel.h"
#include "SurfaceCsvImporter.h"
#include "SurfaceDataInspector.h"
#include "SurfaceDefaults.h"
#include "SurfaceLineWidget.h"
#include "SurfaceTableWidget.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "services/markets/MarketDataService.h"
#include "ui/theme/Theme.h"

#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QStringList>
#include <QVariant>
#include <QVBoxLayout>

namespace fincept::surface {

using namespace fincept::ui;

void SurfaceAnalyticsScreen::on_vol_surface_received(const fincept::DatabentoVolSurfaceResult& r) {
    if (data_inspector_)
        data_inspector_->set_status(r.success ? "Vol surface loaded" : "Vol fetch failed", r.success);
    if (!r.success) {
        if (data_inspector_)
            data_inspector_->set_error(r.error);
        update_chart();
        return;
    }

    QString sym = current_symbol_or_default();
    std::string sym_std = sym.toStdString();
    float spot = spot_for(sym);

    if (!r.vol.z.empty()) {
        vol_data_ = r.vol;
        vol_data_.underlying = sym_std;
        vol_data_.spot_price = spot;
    }
    if (!r.delta.z.empty()) {
        delta_data_ = r.delta;
        delta_data_.underlying = sym_std;
        delta_data_.spot_price = spot;
    }
    if (!r.gamma.z.empty()) {
        gamma_data_ = r.gamma;
        gamma_data_.underlying = sym_std;
        gamma_data_.spot_price = spot;
    }
    if (!r.vega.z.empty()) {
        vega_data_ = r.vega;
        vega_data_.underlying = sym_std;
        vega_data_.spot_price = spot;
    }
    if (!r.theta.z.empty()) {
        theta_data_ = r.theta;
        theta_data_.underlying = sym_std;
        theta_data_.spot_price = spot;
    }
    if (!r.skew.z.empty()) {
        skew_data_ = r.skew;
        skew_data_.underlying = sym_std;
    }

    if (data_inspector_) {
        QStringList headers = {"strike", "expiration", "iv"};
        QVector<QStringList> rows;
        for (size_t i = 0; i < vol_data_.strikes.size(); ++i) {
            for (size_t j = 0; j < vol_data_.expirations.size(); ++j) {
                if (i < vol_data_.z.size() && j < vol_data_.z[i].size()) {
                    rows.push_back({QString::number(vol_data_.strikes[i], 'f', 2),
                                    QString::number(vol_data_.expirations[j]),
                                    QString::number(vol_data_.z[i][j], 'f', 4)});
                }
            }
        }
        data_inspector_->show_table("vol_surface", headers, rows);
    }

    update_chart();
    update_metrics();
    update_inspector_lineage();
}

void SurfaceAnalyticsScreen::on_ohlcv_received(const fincept::DatabentoOhlcvResult& r) {
    if (data_inspector_) {
        data_inspector_->set_status(r.success ? "OHLCV loaded" : "OHLCV fetch failed", r.success);
        if (!r.success)
            data_inspector_->set_error(r.error);
        else {
            QStringList headers = {"symbol", "date", "open", "high", "low", "close", "volume"};
            QVector<QStringList> rows;
            for (auto it = r.data.constBegin(); it != r.data.constEnd(); ++it) {
                for (const QJsonObject& bar : it.value()) {
                    rows.push_back({it.key(),
                                    bar.value("date").toString(),
                                    QString::number(bar.value("open").toDouble(), 'f', 2),
                                    QString::number(bar.value("high").toDouble(), 'f', 2),
                                    QString::number(bar.value("low").toDouble(), 'f', 2),
                                    QString::number(bar.value("close").toDouble(), 'f', 2),
                                    QString::number(bar.value("volume").toDouble(), 'f', 0)});
                }
            }
            data_inspector_->show_table("ohlcv-1d", headers, rows);
        }
    }
    update_chart();
    update_metrics();
    update_inspector_lineage();
}

void SurfaceAnalyticsScreen::on_futures_received(const fincept::DatabentoFuturesResult& r) {
    if (data_inspector_) {
        data_inspector_->set_status(r.success ? "Futures curve loaded" : "Futures fetch failed", r.success);
        if (!r.success)
            data_inspector_->set_error(r.error);
    }
    if (r.success) {
        if (!r.forward.z.empty())
            cmdty_fwd_data_ = r.forward;
        if (!r.contango.z.empty())
            contango_data_ = r.contango;
    }
    update_chart();
    update_metrics();
    update_inspector_lineage();
}

void SurfaceAnalyticsScreen::on_surface_received(const fincept::DatabentoSurfaceResult& r) {
    if (data_inspector_) {
        data_inspector_->set_status(r.success ? "Surface loaded" : "Surface fetch failed", r.success);
        if (!r.success)
            data_inspector_->set_error(r.error);
    }
    if (!r.success) {
        update_chart();
        return;
    }
    const auto& type = r.type;
    QString sym = current_symbol_or_default();
    std::string sym_std = sym.toStdString();
    float spot = spot_for(sym);

    if (type == "local_vol" && !r.z.empty()) {
        local_vol_data_.strikes.assign(r.x_axis.begin(), r.x_axis.end());
        local_vol_data_.expirations.assign(r.y_axis.begin(), r.y_axis.end());
        local_vol_data_.z = r.z;
        local_vol_data_.underlying = sym_std;
        local_vol_data_.spot_price = spot;
    } else if (type == "implied_dividend" && !r.z.empty()) {
        impl_div_data_.strikes.assign(r.x_axis.begin(), r.x_axis.end());
        impl_div_data_.expirations.assign(r.y_axis.begin(), r.y_axis.end());
        impl_div_data_.z = r.z;
        impl_div_data_.underlying = sym_std;
    } else if (type == "liquidity" && !r.z.empty()) {
        liquidity_data_.strikes.assign(r.x_axis.begin(), r.x_axis.end());
        liquidity_data_.expirations.assign(r.y_axis.begin(), r.y_axis.end());
        liquidity_data_.z = r.z;
        liquidity_data_.underlying = sym_std;
    } else if (type == "commodity_vol" && !r.z.empty()) {
        cmdty_vol_data_.strikes.assign(r.x_axis.begin(), r.x_axis.end());
        cmdty_vol_data_.expirations.assign(r.y_axis.begin(), r.y_axis.end());
        cmdty_vol_data_.z = r.z;
        cmdty_vol_data_.commodity = sym_std;
    } else if (type == "crack_spread" && !r.z.empty()) {
        crack_data_.spread_types = r.x_labels;
        crack_data_.contract_months.assign(r.y_axis.begin(), r.y_axis.end());
        crack_data_.z = r.z;
    } else if (type == "stress_test" && !r.z.empty()) {
        stress_data_.scenarios = r.x_labels;
        stress_data_.portfolios = r.y_labels;
        stress_data_.z = r.z;
    }

    update_chart();
    update_metrics();
    update_inspector_lineage();
}

void SurfaceAnalyticsScreen::on_db_fetch_started(const QString& desc) {
    if (data_inspector_)
        data_inspector_->set_status(desc, true);
}

void SurfaceAnalyticsScreen::on_db_fetch_failed(const QString& err) {
    if (data_inspector_) {
        data_inspector_->set_status("Fetch failed", false);
        data_inspector_->set_error(err);
    }
}

void SurfaceAnalyticsScreen::on_db_connection_tested(bool ok, const QString& msg) {
    if (!control_panel_)
        return;
    control_panel_->set_provider_status("databento",
                                        ok ? "connected" : "error",
                                        ok ? QString() : msg);
}

void SurfaceAnalyticsScreen::on_db_raw_response(const QString& cmd, const QString& raw_stdout) {
    if (!data_inspector_)
        return;
    QString header = QString("=== %1 ===\n").arg(cmd);
    data_inspector_->set_raw_output(header + raw_stdout);
}

} // namespace fincept::surface
