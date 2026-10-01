// src/screens/surface_analytics/SurfaceAnalyticsScreen_Providers.cpp
//
// Databento provider result handlers — vol surface, OHLCV, futures, generic
// surface, plus fetch-status / connection-test / raw-response logging.
//
// Part of the partial-class split of SurfaceAnalyticsScreen.cpp.

#include "Surface3DWidget.h"
#include "SurfaceAnalyticsScreen.h"
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
#include <QVBoxLayout>
#include <QVariant>

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QMap>
#include <QTimeZone>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace fincept::surface {

using namespace fincept::ui;

namespace {

// ── Risk-surface maths (pure, no Qt) ────────────────────────────────────────
// Everything below works on aligned daily series: closes[asset][t] ascending by date and
// returns[asset][t-1] = close[t] / close[t-1] - 1.

// Pearson correlation of two return series (common length).
double risk_corr(const std::vector<double>& x, const std::vector<double>& y) {
    const size_t n = std::min(x.size(), y.size());
    if (n < 2)
        return 0.0;
    double mx = 0.0, my = 0.0;
    for (size_t i = 0; i < n; ++i) {
        mx += x[i];
        my += y[i];
    }
    mx /= double(n);
    my /= double(n);
    double sxx = 0.0, syy = 0.0, sxy = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double dx = x[i] - mx, dy = y[i] - my;
        sxx += dx * dx;
        syy += dy * dy;
        sxy += dx * dy;
    }
    const double d = std::sqrt(sxx * syy);
    return d > 0.0 ? sxy / d : 0.0;
}

// OLS beta of `y` on `x` over the LAST `k` observations (cov(y,x) / var(x)). 0 when the
// window is too short or x is constant.
double risk_beta_last(const std::vector<double>& y, const std::vector<double>& x, size_t k) {
    const size_t n = std::min(x.size(), y.size());
    k = std::min(k, n);
    if (k < 3)
        return 0.0;
    const size_t off = n - k;
    double mx = 0.0, my = 0.0;
    for (size_t i = off; i < n; ++i) {
        mx += x[i];
        my += y[i];
    }
    mx /= double(k);
    my /= double(k);
    double vx = 0.0, cxy = 0.0;
    for (size_t i = off; i < n; ++i) {
        const double dx = x[i] - mx;
        vx += dx * dx;
        cxy += dx * (y[i] - my);
    }
    return vx > 0.0 ? cxy / vx : 0.0;
}

// Maximum peak-to-trough decline (percent, <= 0) of `close` within its last `w` trading
// days (w + 1 closes). A window longer than the data uses all of it.
double risk_max_drawdown_pct_last(const std::vector<double>& close, size_t w) {
    const size_t n = close.size();
    const size_t k = std::min(n, w + 1);
    if (k < 2)
        return 0.0;
    double peak = close[n - k];
    double mdd = 0.0;
    for (size_t i = n - k; i < n; ++i) {
        peak = std::max(peak, close[i]);
        if (peak > 0.0)
            mdd = std::min(mdd, close[i] / peak - 1.0);
    }
    return mdd * 100.0;
}

// q-quantile (0..1) of `v` by linear interpolation between order statistics.
double risk_quantile(std::vector<double> v, double q) {
    if (v.empty())
        return 0.0;
    std::sort(v.begin(), v.end());
    const double pos = std::min(1.0, std::max(0.0, q)) * double(v.size() - 1);
    const size_t lo = size_t(std::floor(pos));
    const size_t hi = std::min(v.size() - 1, lo + 1);
    return v[lo] + (v[hi] - v[lo]) * (pos - double(lo));
}

// Eigen-decomposition of a symmetric matrix by cyclic Jacobi rotations (n is a basket,
// a handful of assets). Eigenvalues come back in DESCENDING order; evecs[row][k] is the
// row-th component of the k-th eigenvector.
void risk_jacobi_eigen(std::vector<std::vector<double>> a, std::vector<double>& evals,
                       std::vector<std::vector<double>>& evecs) {
    const size_t n = a.size();
    std::vector<std::vector<double>> v(n, std::vector<double>(n, 0.0));
    for (size_t i = 0; i < n; ++i)
        v[i][i] = 1.0;
    for (int sweep = 0; sweep < 100; ++sweep) {
        double off = 0.0;
        for (size_t p = 0; p + 1 < n; ++p)
            for (size_t q = p + 1; q < n; ++q)
                off += a[p][q] * a[p][q];
        if (off < 1e-22)
            break;
        for (size_t p = 0; p + 1 < n; ++p) {
            for (size_t q = p + 1; q < n; ++q) {
                if (std::fabs(a[p][q]) < 1e-300)
                    continue;
                const double theta = (a[q][q] - a[p][p]) / (2.0 * a[p][q]);
                const double t = (theta >= 0.0 ? 1.0 : -1.0) / (std::fabs(theta) + std::sqrt(theta * theta + 1.0));
                const double c = 1.0 / std::sqrt(t * t + 1.0);
                const double s = t * c;
                for (size_t k = 0; k < n; ++k) {
                    const double akp = a[k][p], akq = a[k][q];
                    a[k][p] = c * akp - s * akq;
                    a[k][q] = s * akp + c * akq;
                }
                for (size_t k = 0; k < n; ++k) {
                    const double apk = a[p][k], aqk = a[q][k];
                    a[p][k] = c * apk - s * aqk;
                    a[q][k] = s * apk + c * aqk;
                }
                for (size_t k = 0; k < n; ++k) {
                    const double vkp = v[k][p], vkq = v[k][q];
                    v[k][p] = c * vkp - s * vkq;
                    v[k][q] = s * vkp + c * vkq;
                }
            }
        }
    }
    std::vector<size_t> order(n);
    for (size_t i = 0; i < n; ++i)
        order[i] = i;
    std::sort(order.begin(), order.end(), [&](size_t l, size_t r) { return a[l][l] > a[r][r]; });
    evals.assign(n, 0.0);
    evecs.assign(n, std::vector<double>(n, 0.0));
    for (size_t k = 0; k < n; ++k) {
        evals[k] = a[order[k]][order[k]];
        for (size_t i = 0; i < n; ++i)
            evecs[i][k] = v[i][order[k]];
    }
}

// Aligned daily closes for the symbols of an OHLCV response: the dates COMMON to every
// symbol, ascending, with a positive close. rets[a][t-1] = close[t] / close[t-1] - 1.
struct SurfaceRiskInputs {
    std::vector<std::string> assets;
    std::vector<std::vector<double>> closes;
    std::vector<std::vector<double>> rets;
};

// "YYYY-MM-DD" of a bar: the provider stringifies the pandas index ("2026-09-29 00:00:00+00:00"),
// or - without pandas - a nanosecond `ts_event`.
QString surface_bar_date(const QJsonObject& bar) {
    const QString d = bar.value(QStringLiteral("date")).toString().left(10);
    if (QDate::fromString(d, Qt::ISODate).isValid())
        return d;
    const qint64 ns = bar.value(QStringLiteral("ts_event")).toVariant().toLongLong();
    if (ns > 0)
        return QDateTime::fromMSecsSinceEpoch(ns / 1000000, QTimeZone::UTC).date().toString(Qt::ISODate);
    return {};
}

bool surface_build_risk_inputs(const QHash<QString, QVector<QJsonObject>>& data, const QStringList& preferred,
                               SurfaceRiskInputs& out, QString& err) {
    QStringList order;
    for (const QString& s : preferred)
        if (data.contains(s) && !data.value(s).isEmpty() && !order.contains(s))
            order << s;
    QStringList rest = data.keys();
    rest.sort();
    for (const QString& s : rest)
        if (!order.contains(s) && !data.value(s).isEmpty())
            order << s;
    if (order.isEmpty()) {
        err = QCoreApplication::translate("SurfaceAnalyticsScreen", "The response carried no price bars.");
        return false;
    }

    QVector<QMap<QString, double>> by_date;
    for (const QString& sym : order) {
        QMap<QString, double> m;
        for (const QJsonObject& bar : data.value(sym)) {
            const double close = bar.value(QStringLiteral("close")).toDouble();
            const QString d = surface_bar_date(bar);
            if (close > 0.0 && std::isfinite(close) && !d.isEmpty())
                m.insert(d, close);
        }
        by_date.append(m);
    }
    QStringList common = by_date.first().keys(); // QMap keys are sorted ascending
    for (int i = 1; i < by_date.size(); ++i) {
        QStringList kept;
        for (const QString& d : common)
            if (by_date[i].contains(d))
                kept << d;
        common = kept;
    }
    constexpr int kMinCommonDays = 6;
    if (common.size() < kMinCommonDays) {
        err = QCoreApplication::translate("SurfaceAnalyticsScreen",
                                          "Only %1 trading day(s) are common to %2 - at least %3 are needed. A "
                                          "symbol with a short history (a recent listing) limits the whole basket.")
                  .arg(common.size())
                  .arg(order.join(QStringLiteral(", ")))
                  .arg(kMinCommonDays);
        return false;
    }
    for (int a = 0; a < order.size(); ++a) {
        out.assets.push_back(order[a].toStdString());
        std::vector<double> closes;
        closes.reserve(size_t(common.size()));
        for (const QString& d : common)
            closes.push_back(by_date[a].value(d));
        std::vector<double> rets;
        rets.reserve(closes.size() - 1);
        for (size_t t = 1; t < closes.size(); ++t)
            rets.push_back(closes[t] / closes[t - 1] - 1.0);
        out.closes.push_back(std::move(closes));
        out.rets.push_back(std::move(rets));
    }
    return true;
}

} // namespace

// The six EQUITIES-tier surfaces (Correlation, PCA, VaR, Drawdown, Beta, Factor Exposure)
// are declared as Databento OHLCV-driven in the capability matrix, and the fetch ran —
// but the response only ever filled the inspector's raw table, so the surfaces stayed
// sample data. Compute the five that daily closes can support. Factor Exposure needs
// factor-return series (size / value / momentum ...) that OHLCV does not carry, so it stays
// sample data and says so.
void SurfaceAnalyticsScreen::apply_risk_surfaces(const fincept::DatabentoOhlcvResult& r) {
    SurfaceRiskInputs in;
    QString err;
    const QStringList basket = control_panel_ ? control_panel_->state().basket : QStringList{};
    if (!surface_build_risk_inputs(r.data, basket, in, err)) {
        if (data_inspector_)
            data_inspector_->set_error(err);
        return;
    }
    const size_t n = in.assets.size();
    const size_t nret = in.rets[0].size();
    const std::vector<int> windows = {5, 10, 21, 63, 126, 252};
    QStringList computed;

    // ── Drawdown: max peak-to-trough % inside each trailing window ───────────
    drawdown_data_ = {};
    drawdown_data_.assets = in.assets;
    drawdown_data_.windows = windows;
    for (size_t a = 0; a < n; ++a) {
        std::vector<float> row;
        for (int w : windows)
            row.push_back(float(risk_max_drawdown_pct_last(in.closes[a], size_t(w))));
        drawdown_data_.z.push_back(std::move(row));
    }
    real_data_charts_.insert(static_cast<int>(ChartType::Drawdown));
    computed << QStringLiteral("Drawdown");

    // ── Beta vs SPY over each trailing horizon ───────────────────────────────
    size_t spy = n;
    for (size_t a = 0; a < n; ++a)
        if (in.assets[a] == "SPY")
            spy = a;
    if (spy < n) {
        beta_data_ = {};
        beta_data_.assets = in.assets;
        beta_data_.horizons = windows;
        for (size_t a = 0; a < n; ++a) {
            std::vector<float> row;
            for (int h : windows)
                row.push_back(float(risk_beta_last(in.rets[a], in.rets[spy], size_t(h))));
            beta_data_.z.push_back(std::move(row));
        }
        real_data_charts_.insert(static_cast<int>(ChartType::BetaSurface));
        computed << QStringLiteral("Beta");
    }

    // ── VaR of the equal-weight basket (historical simulation, % loss) ───────
    {
        std::vector<double> port(nret, 0.0);
        for (size_t t = 0; t < nret; ++t) {
            for (size_t a = 0; a < n; ++a)
                port[t] += in.rets[a][t];
            port[t] /= double(n);
        }
        var_data_ = {};
        var_data_.confidence_levels = {90.0f, 95.0f, 99.0f, 99.5f, 99.9f};
        var_data_.horizons = {1, 5, 10, 20, 60};
        std::vector<std::vector<double>> by_horizon; // overlapping compounded h-day returns
        for (int h : var_data_.horizons) {
            std::vector<double> rh;
            const size_t hh = size_t(h);
            if (nret >= hh + 19) { // >= 20 windows, else scale the 1-day figure below
                for (size_t s = 0; s + hh <= nret; ++s) {
                    double g = 1.0;
                    for (size_t k = s; k < s + hh; ++k)
                        g *= 1.0 + port[k];
                    rh.push_back(g - 1.0);
                }
            }
            by_horizon.push_back(std::move(rh));
        }
        for (float conf : var_data_.confidence_levels) {
            const double alpha = 1.0 - double(conf) / 100.0;
            const double q1 = risk_quantile(port, alpha);
            std::vector<float> row;
            for (size_t hi = 0; hi < var_data_.horizons.size(); ++hi) {
                const double qh = by_horizon[hi].empty()
                                      ? q1 * std::sqrt(double(var_data_.horizons[hi]))
                                      : risk_quantile(by_horizon[hi], alpha);
                row.push_back(float(-qh * 100.0));
            }
            var_data_.z.push_back(std::move(row));
        }
        real_data_charts_.insert(static_cast<int>(ChartType::VaR));
        computed << QStringLiteral("VaR");
    }

    // ── Correlation + PCA (need at least two assets) ─────────────────────────
    if (n >= 2) {
        std::vector<std::vector<double>> corr(n, std::vector<double>(n, 1.0));
        for (size_t i = 0; i < n; ++i)
            for (size_t j = 0; j < n; ++j)
                if (i != j)
                    corr[i][j] = risk_corr(in.rets[i], in.rets[j]);

        corr_data_ = {};
        corr_data_.assets = in.assets;
        corr_data_.window = int(nret);
        std::vector<float> flat;
        flat.reserve(n * n);
        for (size_t i = 0; i < n; ++i)
            for (size_t j = 0; j < n; ++j)
                flat.push_back(float(corr[i][j]));
        corr_data_.z.push_back(std::move(flat)); // one slice: the whole fetched window
        real_data_charts_.insert(static_cast<int>(ChartType::Correlation));
        computed << QStringLiteral("Correlation");

        std::vector<double> evals;
        std::vector<std::vector<double>> evecs;
        risk_jacobi_eigen(corr, evals, evecs);
        const size_t kpc = std::min<size_t>(n, 5);
        pca_data_ = {};
        pca_data_.assets = in.assets;
        for (size_t k = 0; k < kpc; ++k) {
            pca_data_.factors.push_back("PC" + std::to_string(k + 1));
            pca_data_.variance_explained.push_back(float(std::max(0.0, evals[k]) / double(n)));
        }
        // Loadings = eigenvector x sqrt(eigenvalue): the correlation of each asset with the
        // component. An eigenvector's sign is arbitrary, so orient every component so its
        // loadings sum positive (PC1 then reads as "the market").
        std::vector<double> sign(kpc, 1.0);
        for (size_t k = 0; k < kpc; ++k) {
            double s = 0.0;
            for (size_t a = 0; a < n; ++a)
                s += evecs[a][k];
            sign[k] = s < 0.0 ? -1.0 : 1.0;
        }
        for (size_t a = 0; a < n; ++a) {
            std::vector<float> row;
            for (size_t k = 0; k < kpc; ++k)
                row.push_back(float(sign[k] * evecs[a][k] * std::sqrt(std::max(0.0, evals[k]))));
            pca_data_.z.push_back(std::move(row));
        }
        real_data_charts_.insert(static_cast<int>(ChartType::PCA));
        computed << QStringLiteral("PCA");
    }

    sync_synthetic_badge();
    if (data_inspector_) {
        QString msg = tr("Computed %1 from %2 symbol(s) x %3 trading days of daily closes.")
                          .arg(computed.join(QStringLiteral(", ")))
                          .arg(n)
                          .arg(nret + 1);
        if (nret < 252)
            msg += tr(" Windows / horizons longer than the sample use all of it.");
        if (nret < 30)
            msg += tr(" The sample is short - widen the date range for stable estimates.");
        data_inspector_->set_status(msg, true);
        if (active_chart_ == ChartType::FactorExposure)
            data_inspector_->set_error(
                tr("Factor exposure needs factor-return series (size, value, momentum ...) that daily closes "
                   "do not carry - this surface is still sample data."));
        else if (active_chart_ == ChartType::BetaSurface && spy >= n)
            data_inspector_->set_error(
                tr("Beta is measured against SPY - add SPY to the basket and fetch again. "
                   "This surface is still sample data."));
        else if (active_chart_ == ChartType::Correlation && n < 2)
            data_inspector_->set_error(tr("Correlation needs at least two symbols in the basket."));
        else if (active_chart_ == ChartType::PCA && n < 2)
            data_inspector_->set_error(tr("PCA needs at least two symbols in the basket."));
    }
}

void SurfaceAnalyticsScreen::on_vol_surface_received(const fincept::DatabentoVolSurfaceResult& r) {
    if (data_inspector_)
        data_inspector_->set_status(r.success ? tr("Vol surface loaded") : tr("Vol fetch failed"), r.success);
    if (!r.success) {
        if (data_inspector_)
            data_inspector_->set_error(r.error);
        update_chart();
        return;
    }

    QString sym = current_symbol_or_default();
    std::string sym_std = sym.toStdString();
    float spot = spot_for(sym);

    // SAFETY: the DEMO badge may only be cleared for grids that were genuinely
    // replaced. A "successful" response can still carry an empty grid for some
    // (or every) surface — clearing the badge unconditionally would relabel
    // untouched rand() sample data as live market data.
    bool replaced_active = false;
    auto note_replaced = [&](ChartType t) {
        real_data_charts_.insert(static_cast<int>(t));
        if (active_chart_ == t)
            replaced_active = true;
    };

    if (!r.vol.z.empty()) {
        vol_data_ = r.vol;
        vol_data_.underlying = sym_std;
        vol_data_.spot_price = spot;
        note_replaced(ChartType::Volatility);
    }
    if (!r.delta.z.empty()) {
        delta_data_ = r.delta;
        delta_data_.underlying = sym_std;
        delta_data_.spot_price = spot;
        note_replaced(ChartType::DeltaSurface);
    }
    if (!r.gamma.z.empty()) {
        gamma_data_ = r.gamma;
        gamma_data_.underlying = sym_std;
        gamma_data_.spot_price = spot;
        note_replaced(ChartType::GammaSurface);
    }
    if (!r.vega.z.empty()) {
        vega_data_ = r.vega;
        vega_data_.underlying = sym_std;
        vega_data_.spot_price = spot;
        note_replaced(ChartType::VegaSurface);
    }
    if (!r.theta.z.empty()) {
        theta_data_ = r.theta;
        theta_data_.underlying = sym_std;
        theta_data_.spot_price = spot;
        note_replaced(ChartType::ThetaSurface);
    }
    if (!r.skew.z.empty()) {
        skew_data_ = r.skew;
        skew_data_.underlying = sym_std;
        note_replaced(ChartType::SkewSurface);
    }
    sync_synthetic_badge();
    if (!replaced_active && data_inspector_)
        data_inspector_->set_error(
            tr("The response carried no grid for the surface on screen — it is still showing sample data."));

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
    // The raw bars go to the inspector's table. The DEMO badge is only ever lowered by
    // apply_risk_surfaces() below, and only for the surfaces it actually computed from
    // those bars — everything it does not touch is still rand() sample data.
    if (data_inspector_) {
        data_inspector_->set_status(r.success ? tr("OHLCV loaded") : tr("OHLCV fetch failed"), r.success);
        if (!r.success)
            data_inspector_->set_error(r.error);
        else {
            QStringList headers = {"symbol", "date", "open", "high", "low", "close", "volume"};
            QVector<QStringList> rows;
            for (auto it = r.data.constBegin(); it != r.data.constEnd(); ++it) {
                for (const QJsonObject& bar : it.value()) {
                    rows.push_back({it.key(), bar.value("date").toString(),
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
    if (r.success)
        apply_risk_surfaces(r);
    update_chart();
    update_metrics();
    update_inspector_lineage();
}

void SurfaceAnalyticsScreen::on_futures_received(const fincept::DatabentoFuturesResult& r) {
    if (data_inspector_) {
        data_inspector_->set_status(r.success ? tr("Futures curve loaded") : tr("Futures fetch failed"), r.success);
        if (!r.success)
            data_inspector_->set_error(r.error);
    }
    if (r.success) {
        // Only the two grids below come from this response — anything else on
        // screen is still sample data and keeps its DEMO badge.
        if (!r.forward.z.empty())
            real_data_charts_.insert(static_cast<int>(ChartType::CommodityForward));
        if (!r.contango.z.empty())
            real_data_charts_.insert(static_cast<int>(ChartType::ContangoBackwardation));
        if (!r.forward.z.empty())
            cmdty_fwd_data_ = r.forward;
        if (!r.contango.z.empty())
            contango_data_ = r.contango;
        sync_synthetic_badge();
    }
    update_chart();
    update_metrics();
    update_inspector_lineage();
}

void SurfaceAnalyticsScreen::on_surface_received(const fincept::DatabentoSurfaceResult& r) {
    if (data_inspector_) {
        data_inspector_->set_status(r.success ? tr("Surface loaded") : tr("Surface fetch failed"), r.success);
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

    // Track which surface (if any) this payload actually replaced. `type` can be
    // a value we don't handle, or arrive with an empty grid — in either case
    // nothing on screen changed and the DEMO badge must stay on.
    ChartType replaced = active_chart_;
    bool did_replace = false;
    if (type == "local_vol" && !r.z.empty()) {
        local_vol_data_.strikes.assign(r.x_axis.begin(), r.x_axis.end());
        local_vol_data_.expirations.assign(r.y_axis.begin(), r.y_axis.end());
        local_vol_data_.z = r.z;
        local_vol_data_.underlying = sym_std;
        local_vol_data_.spot_price = spot;
        replaced = ChartType::LocalVolSurface;
        did_replace = true;
    } else if (type == "implied_dividend" && !r.z.empty()) {
        impl_div_data_.strikes.assign(r.x_axis.begin(), r.x_axis.end());
        impl_div_data_.expirations.assign(r.y_axis.begin(), r.y_axis.end());
        impl_div_data_.z = r.z;
        impl_div_data_.underlying = sym_std;
        replaced = ChartType::ImpliedDividend;
        did_replace = true;
    } else if (type == "liquidity" && !r.z.empty()) {
        liquidity_data_.strikes.assign(r.x_axis.begin(), r.x_axis.end());
        liquidity_data_.expirations.assign(r.y_axis.begin(), r.y_axis.end());
        liquidity_data_.z = r.z;
        liquidity_data_.underlying = sym_std;
        replaced = ChartType::LiquidityHeatmap;
        did_replace = true;
    } else if (type == "commodity_vol" && !r.z.empty()) {
        cmdty_vol_data_.strikes.assign(r.x_axis.begin(), r.x_axis.end());
        cmdty_vol_data_.expirations.assign(r.y_axis.begin(), r.y_axis.end());
        cmdty_vol_data_.z = r.z;
        cmdty_vol_data_.commodity = sym_std;
        replaced = ChartType::CommodityVol;
        did_replace = true;
    } else if (type == "crack_spread" && !r.z.empty()) {
        crack_data_.spread_types = r.x_labels;
        crack_data_.contract_months.assign(r.y_axis.begin(), r.y_axis.end());
        crack_data_.z = r.z;
        replaced = ChartType::CrackSpread;
        did_replace = true;
    } else if (type == "stress_test" && !r.z.empty()) {
        stress_data_.scenarios = r.x_labels;
        stress_data_.portfolios = r.y_labels;
        stress_data_.z = r.z;
        replaced = ChartType::StressTestPnL;
        did_replace = true;
    }

    if (did_replace)
        real_data_charts_.insert(static_cast<int>(replaced));
    const bool active_is_real = did_replace && replaced == active_chart_;
    sync_synthetic_badge();
    if (!active_is_real && data_inspector_)
        data_inspector_->set_error(
            tr("The response replaced no grid for the surface on screen — it is still showing sample data."));

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
        data_inspector_->set_status(tr("Fetch failed"), false);
        data_inspector_->set_error(err);
    }
}

void SurfaceAnalyticsScreen::on_db_connection_tested(bool ok, const QString& msg) {
    if (!control_panel_)
        return;
    control_panel_->set_provider_status("databento", ok ? "connected" : "error", ok ? QString() : msg);
}

void SurfaceAnalyticsScreen::on_db_raw_response(const QString& cmd, const QString& raw_stdout) {
    if (!data_inspector_)
        return;
    QString header = QString("=== %1 ===\n").arg(cmd);
    data_inspector_->set_raw_output(header + raw_stdout);
}

} // namespace fincept::surface
