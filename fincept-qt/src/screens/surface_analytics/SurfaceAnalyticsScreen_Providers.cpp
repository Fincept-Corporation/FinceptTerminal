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
#include "services/options/DerivMarketData.h"
#include "services/options/FreeOptionChainService.h"
#include "services/options/OptionPricing.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadGate.h"
#include "ui/widgets/EmptyState.h"

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
#include <QPointer>
#include <QStyle>
#include <QTimeZone>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>
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

// ── Option-chain window ─────────────────────────────────────────────────────
// The provider returns the whole listed chain; the panel's Strike window (± % of
// spot) and DTE min / max are applied here so those controls change what is
// shown. Grids are z[expiry][strike] with expirations in days. Returns false
// (grid untouched) when the window would leave fewer than two strikes or no
// expiry — showing nothing would be worse than showing the full chain.
template <typename Grid>
bool apply_chain_window(Grid& g, float spot, int window_pct, int dte_min, int dte_max) {
    if (g.z.empty() || g.z.size() != g.expirations.size())
        return false;
    std::vector<size_t> keep_s, keep_e;
    for (size_t i = 0; i < g.strikes.size(); ++i)
        if (spot <= 0.0f || window_pct <= 0 || std::fabs(g.strikes[i] / spot - 1.0f) * 100.0f <= float(window_pct) + 1e-4f)
            keep_s.push_back(i);
    for (size_t j = 0; j < g.expirations.size(); ++j)
        if (g.expirations[j] >= dte_min && g.expirations[j] <= dte_max)
            keep_e.push_back(j);
    if (keep_s.size() < 2 || keep_e.empty())
        return false;
    if (keep_s.size() == g.strikes.size() && keep_e.size() == g.expirations.size())
        return true;
    decltype(g.strikes) strikes;
    decltype(g.expirations) expirations;
    decltype(g.z) z;
    for (size_t i : keep_s)
        strikes.push_back(g.strikes[i]);
    for (size_t j : keep_e) {
        expirations.push_back(g.expirations[j]);
        std::vector<float> row;
        for (size_t i : keep_s)
            row.push_back(i < g.z[j].size() ? g.z[j][i] : std::nanf(""));
        z.push_back(std::move(row));
    }
    g.strikes = std::move(strikes);
    g.expirations = std::move(expirations);
    g.z = std::move(z);
    return true;
}

// DTE only (skew: z[expiry][delta bucket]).
template <typename Grid>
bool apply_dte_window(Grid& g, int dte_min, int dte_max) {
    if (g.z.empty() || g.z.size() != g.expirations.size())
        return false;
    decltype(g.expirations) expirations;
    decltype(g.z) z;
    for (size_t j = 0; j < g.expirations.size(); ++j)
        if (g.expirations[j] >= dte_min && g.expirations[j] <= dte_max) {
            expirations.push_back(g.expirations[j]);
            z.push_back(g.z[j]);
        }
    if (expirations.empty())
        return false;
    g.expirations = std::move(expirations);
    g.z = std::move(z);
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
    // The panel's Strike window / DTE range, applied to the fetched chain.
    const auto& cs = control_panel_->state();
    bool window_skipped = false;
    auto window = [&](auto& grid) {
        if (!apply_chain_window(grid, spot, cs.strike_window_pct, cs.dte_min, cs.dte_max))
            window_skipped = true;
    };

    if (!r.vol.z.empty()) {
        vol_data_ = r.vol;
        vol_data_.underlying = sym_std;
        vol_data_.spot_price = spot;
        window(vol_data_);
        note_replaced(ChartType::Volatility);
    }
    if (!r.delta.z.empty()) {
        delta_data_ = r.delta;
        delta_data_.underlying = sym_std;
        delta_data_.spot_price = spot;
        window(delta_data_);
        note_replaced(ChartType::DeltaSurface);
    }
    if (!r.gamma.z.empty()) {
        gamma_data_ = r.gamma;
        gamma_data_.underlying = sym_std;
        gamma_data_.spot_price = spot;
        window(gamma_data_);
        note_replaced(ChartType::GammaSurface);
    }
    if (!r.vega.z.empty()) {
        vega_data_ = r.vega;
        vega_data_.underlying = sym_std;
        vega_data_.spot_price = spot;
        window(vega_data_);
        note_replaced(ChartType::VegaSurface);
    }
    if (!r.theta.z.empty()) {
        theta_data_ = r.theta;
        theta_data_.underlying = sym_std;
        theta_data_.spot_price = spot;
        window(theta_data_);
        note_replaced(ChartType::ThetaSurface);
    }
    if (!r.skew.z.empty()) {
        skew_data_ = r.skew;
        skew_data_.underlying = sym_std;
        if (!apply_dte_window(skew_data_, cs.dte_min, cs.dte_max))
            window_skipped = true;
        note_replaced(ChartType::SkewSurface);
    }
    sync_synthetic_badge();
    if (window_skipped && data_inspector_)
        data_inspector_->set_status(
            tr("Strike window / DTE range matched no listed contract on some surfaces — showing their full chain."),
            true);
    if (!replaced_active && data_inspector_)
        data_inspector_->set_error(
            tr("The response carried no grid for the surface on screen — it is still showing sample data."));

    if (data_inspector_) {
        QStringList headers = {"strike", "expiration", "iv"};
        QVector<QStringList> rows;
        // The grid is z[expiry][strike]: indexing it z[strike][expiry] paired each
        // strike with another strike's vol.
        for (size_t i = 0; i < vol_data_.strikes.size(); ++i) {
            for (size_t j = 0; j < vol_data_.expirations.size(); ++j) {
                if (j < vol_data_.z.size() && i < vol_data_.z[j].size()) {
                    rows.push_back({QString::number(vol_data_.strikes[i], 'f', 2),
                                    QString::number(vol_data_.expirations[j]),
                                    QString::number(vol_data_.z[j][i], 'f', 4)});
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
    // The panel's Strike window / DTE range, applied to option-chain grids.
    const auto& cs = control_panel_->state();
    auto window = [&](auto& grid) {
        if (!apply_chain_window(grid, spot, cs.strike_window_pct, cs.dte_min, cs.dte_max) && data_inspector_)
            data_inspector_->set_status(
                tr("Strike window / DTE range matched no listed contract — showing the full chain."), true);
    };
    if (type == "local_vol" && !r.z.empty()) {
        local_vol_data_.strikes.assign(r.x_axis.begin(), r.x_axis.end());
        local_vol_data_.expirations.assign(r.y_axis.begin(), r.y_axis.end());
        local_vol_data_.z = r.z;
        local_vol_data_.underlying = sym_std;
        local_vol_data_.spot_price = spot;
        window(local_vol_data_);
        replaced = ChartType::LocalVolSurface;
        did_replace = true;
    } else if (type == "implied_dividend" && !r.z.empty()) {
        impl_div_data_.strikes.assign(r.x_axis.begin(), r.x_axis.end());
        impl_div_data_.expirations.assign(r.y_axis.begin(), r.y_axis.end());
        impl_div_data_.z = r.z;
        impl_div_data_.underlying = sym_std;
        window(impl_div_data_);
        replaced = ChartType::ImpliedDividend;
        did_replace = true;
    } else if (type == "liquidity" && !r.z.empty()) {
        liquidity_data_.strikes.assign(r.x_axis.begin(), r.x_axis.end());
        liquidity_data_.expirations.assign(r.y_axis.begin(), r.y_axis.end());
        liquidity_data_.z = r.z;
        liquidity_data_.underlying = sym_std;
        window(liquidity_data_);
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
    } else if (type == "yield_curve" && !r.z.empty()) {
        // Rows = settlement dates (newest first), columns = maturities in months.
        yield_data_ = {};
        for (float m : r.x_axis)
            yield_data_.maturities.push_back(static_cast<int>(std::lround(m)));
        yield_data_.time_points.assign(r.y_axis.begin(), r.y_axis.end());
        yield_data_.time_labels = r.y_labels;
        yield_data_.z = r.z;
        replaced = ChartType::YieldCurve;
        did_replace = true;
    } else if (type == "forward_rate" && !r.z.empty()) {
        fwd_rate_data_ = {};
        for (float st : r.x_axis)
            fwd_rate_data_.start_tenors.push_back(static_cast<int>(std::lround(st)));
        fwd_rate_data_.forward_periods.assign(r.y_axis.begin(), r.y_axis.end());
        fwd_rate_data_.z = r.z;
        replaced = ChartType::ForwardRate;
        did_replace = true;
    } else if (type == "rate_path" && !r.z.empty()) {
        monetary_data_ = {};
        monetary_data_.central_banks = r.x_labels;
        monetary_data_.meetings_ahead.assign(r.y_axis.begin(), r.y_axis.end());
        monetary_data_.step_labels = r.y_labels;
        monetary_data_.z = r.z;
        replaced = ChartType::MonetaryPolicyPath;
        did_replace = true;
    } else if (type == "fx_forward_points" && !r.z.empty()) {
        fx_fwd_data_ = {};
        fx_fwd_data_.pairs = r.x_labels;
        fx_fwd_data_.tenors.assign(r.y_axis.begin(), r.y_axis.end());
        fx_fwd_data_.tenor_labels = r.y_labels;
        fx_fwd_data_.z = r.z;
        replaced = ChartType::FXForwardPoints;
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

void SurfaceAnalyticsScreen::on_db_raw_response(const QString& cmd, const QString& raw_stdout) {
    if (!data_inspector_)
        return;
    QString header = QString("=== %1 ===\n").arg(cmd);
    data_inspector_->set_raw_output(header + raw_stdout);
}

// ── Free delayed option chain → the six option surfaces ──────────────────────

namespace {

namespace sa_mkt = fincept::services::options::market;

bool sa_free_is_option_chart(ChartType t) {
    return t == ChartType::Volatility || t == ChartType::DeltaSurface || t == ChartType::GammaSurface ||
           t == ChartType::VegaSurface || t == ChartType::ThetaSurface || t == ChartType::SkewSurface;
}

/// The chain's implied-vol grid (moneyness K/F rows × expiries, SVI where it fits,
/// else the market smile — computed by analyse_chain), restricted to 2..730-day
/// expiries with at least four points, thinned to ten spread across the term, and
/// gap-filled per expiry (linear inside, flat at the wings) so every surface view
/// gets a complete grid. iv[col][row] is decimal.
struct SaFreeGrid {
    std::vector<double> moneyness;
    std::vector<int> cols; ///< expiry indices into chain.expiries
    std::vector<std::vector<double>> iv;
};

SaFreeGrid sa_free_build_grid(const sa_mkt::FreeChain& c) {
    SaFreeGrid g;
    const int rows = c.surface_moneyness.size();
    if (rows < 3 || c.surface_iv.size() != rows)
        return g;
    std::vector<int> cols;
    for (int e = 0; e < c.expiries.size(); ++e) {
        const auto& x = c.expiries[e];
        if (x.days < 2 || x.days > 730)
            continue;
        int valid = 0;
        for (int r = 0; r < rows; ++r)
            if (e < c.surface_iv[r].size() && std::isfinite(c.surface_iv[r][e]))
                ++valid;
        if (valid >= 4)
            cols.push_back(e);
    }
    constexpr int kMaxCols = 10;
    if ((int)cols.size() > kMaxCols) {
        std::vector<int> thin;
        for (int i = 0; i < kMaxCols; ++i) {
            const int pick = cols[(size_t)std::lround(double(i) * double(cols.size() - 1) / double(kMaxCols - 1))];
            if (thin.empty() || thin.back() != pick)
                thin.push_back(pick);
        }
        cols = thin;
    }
    for (double m : c.surface_moneyness)
        g.moneyness.push_back(m);
    for (int e : cols) {
        std::vector<double> col(rows, std::numeric_limits<double>::quiet_NaN());
        for (int r = 0; r < rows; ++r)
            if (e < c.surface_iv[r].size())
                col[r] = c.surface_iv[r][e];
        // Gaps: linear between known points, flat beyond the outermost ones.
        int prev = -1;
        for (int r = 0; r < rows; ++r) {
            if (!std::isfinite(col[r]))
                continue;
            if (prev < 0) {
                for (int k = 0; k < r; ++k)
                    col[k] = col[r];
            } else if (r - prev > 1) {
                for (int k = prev + 1; k < r; ++k)
                    col[k] = col[prev] + (col[r] - col[prev]) * double(k - prev) / double(r - prev);
            }
            prev = r;
        }
        if (prev < 0)
            continue;
        for (int k = prev + 1; k < rows; ++k)
            col[k] = col[prev];
        g.cols.push_back(e);
        g.iv.push_back(std::move(col));
    }
    return g;
}

} // namespace

void SurfaceAnalyticsScreen::schedule_free_chain() {
    if (free_scheduled_)
        return;
    free_scheduled_ = true;
    QTimer::singleShot(0, this, [this]() {
        free_scheduled_ = false;
        maybe_load_free_chain();
    });
}

void SurfaceAnalyticsScreen::maybe_load_free_chain() {
    // Only the visible screen, only an option surface still on the sample, once per
    // symbol (a failure is not retried automatically; REFRESH retries).
    if (!isVisible() || !sa_free_is_option_chart(active_chart_) ||
        real_data_charts_.contains(static_cast<int>(active_chart_)))
        return;
    const QString sym = surface_us_chain_symbol(current_symbol_or_default());
    if (sym.isEmpty() || sym == free_loading_symbol_ || free_failure_applies())
        return;
    start_free_chain(false);
}

void SurfaceAnalyticsScreen::release_free_gate() {
    free_queued_ = false;
    if (free_ticket_ != 0)
        fincept::ui::AutoLoadGate::instance().leave(std::exchange(free_ticket_, 0));
}

void SurfaceAnalyticsScreen::start_free_chain(bool user_run) {
    namespace opt = fincept::services::options;
    // The plain US ticker the chain lists ("^GSPC" -> "SPX"), as Derivatives and F&O
    // pass it: one cache entry per symbol across the three.
    const QString sym = surface_us_chain_symbol(current_symbol_or_default());
    if (sym.isEmpty())
        return;
    if (free_req_ != 0) {
        opt::FreeOptionChainService::instance().cancel(free_req_);
        free_req_ = 0;
    }
    release_free_gate();
    free_loading_symbol_ = sym;
    free_failed_symbol_.clear();
    free_failed_message_.clear();
    free_failed_status_.clear();
    // Leaving the "no listed options" page (RETRY): the sample surface + loading banner.
    if (chain_state_ && view_stack_ && view_stack_->currentWidget() == chain_state_)
        update_chart();
    update_source_banner();
    if (data_inspector_)
        data_inspector_->set_status(tr("Loading the delayed %1 option chain…").arg(sym), true);

    QPointer<SurfaceAnalyticsScreen> self(this);
    auto begin = [self, sym, user_run]() {
        if (!self)
            return;
        self->free_queued_ = false;
        self->update_source_banner(); // "Queued" -> "Loading"
        if (self->free_loading_symbol_ != sym || (!user_run && !self->isVisible())) {
            self->release_free_gate();
            return;
        }
        self->free_req_ = opt::FreeOptionChainService::instance().fetch(
            sym, QStringLiteral("auto"), user_run, self.data(), [self, sym](const opt::FreeOptionChainService::Reply& r) {
                if (!self || self->free_loading_symbol_ != sym)
                    return;
                using Stage = opt::FreeOptionChainService::Stage;
                const bool final_reply = r.stage != Stage::Saved;
                if (r.chain && r.chain->ok()) {
                    self->apply_free_chain(r.chain);
                } else if (final_reply && !self->free_charts_.contains(static_cast<int>(self->active_chart_))) {
                    const QString why = r.chain && !r.chain->message.isEmpty() ? r.chain->message
                                        : !r.message.isEmpty()                 ? r.message
                                                                               : tr("the delayed quote feed answered nothing");
                    self->free_failed_symbol_ = sym;
                    self->free_failed_message_ = why;
                    self->free_failed_status_ = r.chain && !r.chain->status.isEmpty() ? r.chain->status : r.status;
                    LOG_WARN("SurfaceAnalytics",
                             QString("Delayed option chain unavailable symbol=%1 status=%2 error=\"%3\"")
                                 .arg(sym, r.status, why));
                    if (self->data_inspector_) {
                        self->data_inspector_->set_status(tr("Delayed option chain unavailable"), false);
                        self->data_inspector_->set_error(why);
                    }
                    self->update_chart(); // the "No listed options for X" page
                } else if (final_reply && r.stage == Stage::Failed && self->data_inspector_) {
                    // A saved copy is on screen: it stays, the refresh failed.
                    self->data_inspector_->set_status(tr("Refresh failed — showing the saved chain"), false);
                }
                if (final_reply) {
                    self->free_req_ = 0;
                    self->free_loading_symbol_.clear();
                    self->release_free_gate();
                    self->update_source_banner();
                }
            });
    };
    if (user_run) {
        begin(); // REFRESH: the user's run never waits for the gate
        return;
    }
    quint64 ticket = 0;
    const bool now = fincept::ui::AutoLoadGate::instance().enter(this, begin, 0, &ticket);
    free_ticket_ = ticket;
    if (now)
        begin();
    else
        free_queued_ = true;
}

void SurfaceAnalyticsScreen::apply_free_chain(const std::shared_ptr<const sa_mkt::FreeChain>& chain) {
    namespace pricing = fincept::services::options::pricing;
    if (!chain || !chain->ok())
        return;
    const SaFreeGrid g = sa_free_build_grid(*chain);
    const double spot = std::isfinite(chain->spot) && chain->spot > 0 ? chain->spot : 0.0;
    if (g.cols.empty() || spot <= 0.0) {
        free_failed_symbol_ = chain->symbol.toUpper();
        free_failed_message_ = tr("the chain has no expiry with enough quoted strikes to draw a surface");
        free_failed_status_ = QStringLiteral("thin");
        if (data_inspector_) {
            data_inspector_->set_status(tr("Delayed option chain too thin"), false);
            data_inspector_->set_error(free_failed_message_);
        }
        update_chart(); // the designed page instead of the sample surface
        return;
    }
    const std::string sym = chain->symbol.toStdString();
    const float spot_f = float(spot);

    std::vector<float> strikes;
    for (double m : g.moneyness)
        strikes.push_back(float(std::round(m * spot * 100.0) / 100.0));
    std::vector<int> dtes;
    for (int e : g.cols)
        dtes.push_back(std::max(1, chain->expiries[e].days));

    VolatilitySurfaceData vol;
    GreeksSurfaceData dl, gm, vg, th;
    SkewSurfaceData sk;
    vol.strikes = dl.strikes = gm.strikes = vg.strikes = th.strikes = strikes;
    vol.expirations = dl.expirations = gm.expirations = vg.expirations = th.expirations = sk.expirations = dtes;
    vol.underlying = dl.underlying = gm.underlying = vg.underlying = th.underlying = sk.underlying = sym;
    vol.spot_price = dl.spot_price = gm.spot_price = vg.spot_price = th.spot_price = spot_f;
    dl.greek_name = "Delta";
    gm.greek_name = "Gamma";
    vg.greek_name = "Vega";
    th.greek_name = "Theta";
    sk.deltas = {10, 25, 50, 75, 90};

    for (size_t c = 0; c < g.cols.size(); ++c) {
        const auto& x = chain->expiries[g.cols[c]];
        const double t = x.years > 0 ? x.years : std::max(1, x.days) / 365.0;
        const double r = std::isfinite(x.rate) ? x.rate : 0.0;
        const double q = std::isfinite(x.carry_q) ? x.carry_q : 0.0;
        std::vector<float> v_row, d_row, g_row, vg_row, th_row;
        std::vector<double> call_delta;
        for (size_t i = 0; i < strikes.size(); ++i) {
            const double sigma = std::max(1e-4, g.iv[c][i]);
            const auto gk = pricing::bsm_greeks(true, spot, double(strikes[i]), t, r, sigma, q);
            v_row.push_back(float(sigma * 100.0)); // percent, as every surface view expects
            d_row.push_back(float(gk.delta));
            g_row.push_back(float(gk.gamma));
            vg_row.push_back(float(gk.vega / 100.0)); // per vol point
            th_row.push_back(float(gk.theta));        // per calendar day
            call_delta.push_back(gk.delta);
        }
        // Skew: implied vol at the strike whose call delta is nearest each bucket.
        std::vector<float> s_row;
        for (float bucket : sk.deltas) {
            size_t best = 0;
            double best_gap = std::numeric_limits<double>::max();
            for (size_t i = 0; i < call_delta.size(); ++i) {
                const double gap = std::fabs(call_delta[i] - double(bucket) / 100.0);
                if (gap < best_gap) {
                    best_gap = gap;
                    best = i;
                }
            }
            s_row.push_back(float(g.iv[c][best] * 100.0));
        }
        vol.z.push_back(std::move(v_row));
        dl.z.push_back(std::move(d_row));
        gm.z.push_back(std::move(g_row));
        vg.z.push_back(std::move(vg_row));
        th.z.push_back(std::move(th_row));
        sk.z.push_back(std::move(s_row));
    }

    // The panel's DTE range applies as for a Databento chain (the strike axis is
    // already ±20 % of the forward).
    if (control_panel_) {
        const auto& cs = control_panel_->state();
        apply_chain_window(vol, spot_f, cs.strike_window_pct, cs.dte_min, cs.dte_max);
        apply_chain_window(dl, spot_f, cs.strike_window_pct, cs.dte_min, cs.dte_max);
        apply_chain_window(gm, spot_f, cs.strike_window_pct, cs.dte_min, cs.dte_max);
        apply_chain_window(vg, spot_f, cs.strike_window_pct, cs.dte_min, cs.dte_max);
        apply_chain_window(th, spot_f, cs.strike_window_pct, cs.dte_min, cs.dte_max);
        apply_dte_window(sk, cs.dte_min, cs.dte_max);
    }
    vol_data_ = std::move(vol);
    delta_data_ = std::move(dl);
    gamma_data_ = std::move(gm);
    vega_data_ = std::move(vg);
    theta_data_ = std::move(th);
    skew_data_ = std::move(sk);

    free_chain_ = chain;
    for (ChartType t : {ChartType::Volatility, ChartType::DeltaSurface, ChartType::GammaSurface,
                        ChartType::VegaSurface, ChartType::ThetaSurface, ChartType::SkewSurface}) {
        real_data_charts_.insert(static_cast<int>(t));
        free_charts_.insert(static_cast<int>(t));
    }
    if (control_panel_)
        control_panel_->set_spot(spot);
    // The underlying the "no listed options" page offers to go BACK TO.
    free_good_symbol_ = surface_us_chain_symbol(current_symbol_or_default());
    sync_synthetic_badge();
    update_chart();
    update_metrics();
    update_inspector_lineage();
    if (data_inspector_)
        data_inspector_->set_status(tr("Delayed %1 chain: %2 expiries × %3 strikes from %4")
                                        .arg(chain->symbol)
                                        .arg(g.cols.size())
                                        .arg(strikes.size())
                                        .arg(chain->source_label),
                                    true);
}

void SurfaceAnalyticsScreen::update_source_banner() {
    if (!demo_banner_)
        return;
    const int active = static_cast<int>(active_chart_);
    QString tone;
    QString text;
    if (free_chain_ && free_charts_.contains(active)) {
        const QDateTime at = free_chain_->quote_time.isValid() ? free_chain_->quote_time : free_chain_->fetched_at;
        tone = QStringLiteral("delayed");
        text = tr("DELAYED — %1 option chain from %2, quotes as of %3. Free public data, about 15 minutes behind the "
                  "market; FETCH loads Databento.")
                   .arg(free_chain_->symbol,
                        free_chain_->source_label.isEmpty() ? tr("public delayed quotes") : free_chain_->source_label,
                        at.toLocalTime().toString(QStringLiteral("d MMM HH:mm")));
    } else if (real_data_charts_.contains(active)) {
        demo_banner_->setVisible(false); // fetched (Databento) or imported data: no caveat
        return;
    } else if (sa_free_is_option_chart(active_chart_) && !free_loading_symbol_.isEmpty()) {
        tone = QStringLiteral("loading");
        text = free_queued_ ? tr("Queued — the delayed %1 option chain loads after the other loads running now; the "
                                 "sample surface stays until it arrives.")
                                  .arg(free_loading_symbol_)
                            : tr("Loading the delayed %1 option chain (free public quotes)… the sample surface stays "
                                 "until it arrives.")
                                  .arg(free_loading_symbol_);
    } else if (free_failure_applies()) {
        demo_banner_->setVisible(false); // the "no listed options" page says it, in place of the chart
        return;
    } else {
        tone = QStringLiteral("sample");
        const auto& cap = capability_for(active_chart_);
        text = tr("SAMPLE DATA — synthetic values, not a live market surface. Do not trade or quote from this.");
        if (sa_free_is_option_chart(active_chart_) && !free_failed_symbol_.isEmpty())
            text += QLatin1Char(' ') + tr("No free delayed chain for %1 (%2) — REFRESH retries.")
                                           .arg(free_failed_symbol_, free_failed_message_);
        if (cap.tier != SurfaceTier::DEMO && !DatabentoService::instance().has_api_key())
            text += QLatin1Char(' ') + tr("Real data: add a Databento API key in Settings > Credentials.");
    }
    demo_banner_->setText(text);
    if (demo_banner_->property("tone").toString() != tone) {
        demo_banner_->setProperty("tone", tone);
        demo_banner_->style()->unpolish(demo_banner_);
        demo_banner_->style()->polish(demo_banner_);
    }
    demo_banner_->setVisible(true);
}

bool SurfaceAnalyticsScreen::free_failure_applies() const {
    namespace opt = fincept::services::options;
    if (free_failed_symbol_.isEmpty() || !sa_free_is_option_chart(active_chart_) ||
        real_data_charts_.contains(static_cast<int>(active_chart_)))
        return false;
    return opt::FreeOptionChainService::key_of(free_failed_symbol_) ==
           opt::FreeOptionChainService::key_of(surface_us_chain_symbol(current_symbol_or_default()));
}

bool SurfaceAnalyticsScreen::show_free_failure_page() {
    namespace opt = fincept::services::options;
    if (!chain_state_ || !view_stack_ || !free_failure_applies())
        return false;
    const QString sym = surface_us_chain_symbol(current_symbol_or_default());
    const QString key = opt::FreeOptionChainService::key_of(sym);
    const bool unlisted = free_failed_status_ == QLatin1String("no_options") ||
                          free_failed_status_ == QLatin1String("bad_symbol");
    const bool thin = free_failed_status_ == QLatin1String("thin");
    // BACK TO the last underlying whose chain loaded (the service still holds it, so it
    // is back at once); without one, SPY.
    const bool has_back = !free_good_symbol_.isEmpty() && opt::FreeOptionChainService::key_of(free_good_symbol_) != key;
    chain_state_back_ = has_back ? free_good_symbol_ : (key != QLatin1String("SPY") ? QStringLiteral("SPY") : QString());

    fincept::ui::EmptyState::Spec s;
    s.glyph = QStringLiteral("◇");
    s.glyph_tone = unlisted || thin ? fincept::ui::EmptyState::Tone::Warning : fincept::ui::EmptyState::Tone::Danger;
    if (unlisted) {
        s.title = tr("No listed options for %1").arg(sym);
        s.message = tr("The free delayed feed (Cboe, Yahoo Finance as the fallback) has no option chain for %1, so "
                       "there is no surface to draw. Options trade on US-listed stocks, ETFs and indices.")
                        .arg(sym);
        s.technical = free_failed_message_;
    } else if (thin) {
        s.title = tr("Too few quotes for a %1 surface").arg(sym);
        s.message = tr("The delayed %1 chain has no expiry with enough quoted strikes to draw a surface.").arg(sym);
    } else {
        s.title = tr("No option chain for %1").arg(sym);
        s.message = tr("The free delayed source (Cboe, with Yahoo Finance as the fallback) returned no chain for %1 — "
                       "it may have no listed options, or the feed did not answer.")
                        .arg(sym);
        s.technical = free_failed_message_;
    }
    s.note = tr("Pick another underlying in the ASSET box on the left — any US stock, ETF or index.");
    s.note_tone = fincept::ui::EmptyState::Tone::Info;
    if (!chain_state_back_.isEmpty()) {
        s.primary = has_back ? tr("BACK TO %1").arg(chain_state_back_) : tr("OPEN %1").arg(chain_state_back_);
        s.primary_tooltip = tr("Show the %1 surfaces").arg(chain_state_back_);
        if (!unlisted)
            s.secondary = tr("RETRY");
    } else {
        s.primary = tr("RETRY"); // primary_clicked retries when there is nothing to go back to
    }
    s.illustration = true;
    s.illustration_shape = fincept::ui::LoadingSkeleton::Shape::Chart;
    chain_state_->apply(s);
    if (view_stack_->currentWidget() != chain_state_)
        view_stack_->setCurrentWidget(chain_state_);
    return true;
}

} // namespace fincept::surface
