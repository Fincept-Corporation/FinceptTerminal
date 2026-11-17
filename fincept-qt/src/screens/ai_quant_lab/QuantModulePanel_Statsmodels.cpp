// src/screens/ai_quant_lab/QuantModulePanel_Statsmodels.cpp
//
// Statsmodels panel — 11 econometric analyses (OLS, ARIMA, stationarity,
// ACF/PACF, Granger causality, descriptive, cointegration, rolling beta,
// regime switching, PCA, STL decomposition). Mirrors the GS Quant / Functime
// pattern. Extracted from QuantModulePanel.cpp to keep that file maintainable.
//
// The script returns null for a value it cannot compute (an R² of a constant
// series, a half-life of a spread that does not mean-revert); every figure here
// goes through the *_safe formatters so null reads "—", never 0.
#include "screens/ai_quant_lab/QuantModulePanel.h"
#include "screens/ai_quant_lab/QuantModulePanel_AdvancedHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_Common.h"
#include "screens/ai_quant_lab/QuantModulePanel_GsHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_ResultCharts.h"
#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"
#include "ui/charts/BarChart.h"
#include "ui/tables/BarCellDelegate.h"
#include "services/ai_quant_lab/AIQuantLabService.h"
#include "ui/theme/Theme.h"

#include <QAbstractItemView>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::screens {

using namespace fincept::services::quant;
using namespace fincept::screens::quant_styles;
using namespace fincept::screens::quant_common;
using namespace fincept::screens::quant_gs_helpers;
using namespace fincept::screens::quant_charts;
using namespace fincept::screens::quant_advanced_helpers;

namespace {

// LCG + Box-Muller — the recipe of quant_common's samples, so a seed always
// gives the same synthetic data.
struct SmRng {
    quint32 state;
    explicit SmRng(unsigned seed) : state(seed ? seed : 1u) {}
    double uniform() {
        state = state * 1664525u + 1013904223u;
        return double(state >> 8) / double(1u << 24);
    }
    double normal() {
        const double u1 = std::max(uniform(), 1e-12);
        const double u2 = uniform();
        return std::sqrt(-2.0 * std::log(u1)) * std::cos(6.283185307179586 * u2);
    }
};

QString sm_join(const QVector<double>& v, int decimals) {
    QStringList out;
    out.reserve(v.size());
    for (const double x : v)
        out << QString::number(x, 'f', decimals);
    return out.join(',');
}

// Synthetic cointegrated pair of price levels: X a random walk, Y = 2 + 1.5·X +
// AR(1) noise. Both LOAD SAMPLE buttons call this with the same seed, so the
// two legs always belong together.
QString sm_sample_coint_leg(bool y_leg, unsigned seed = 131, int n = 250) {
    SmRng rng(seed);
    QVector<double> x(n), y(n);
    double level = 50.0;
    double e = 0.0;
    for (int i = 0; i < n; ++i) {
        level += rng.normal();
        e = 0.85 * e + rng.normal();
        x[i] = level;
        y[i] = 2.0 + 1.5 * level + e;
    }
    return sm_join(y_leg ? y : x, 4);
}

// Synthetic asset / benchmark daily returns whose beta steps from 0.6 to 1.4
// half-way through (same seed for both legs).
QString sm_sample_beta_leg(bool asset_leg, unsigned seed = 141, int n = 250) {
    SmRng rng(seed);
    QVector<double> x(n), y(n);
    for (int i = 0; i < n; ++i) {
        x[i] = 0.0004 + 0.01 * rng.normal();
        const double beta = i < n / 2 ? 0.6 : 1.4;
        y[i] = 0.0002 + beta * x[i] + 0.005 * rng.normal();
    }
    return sm_join(asset_leg ? y : x, 6);
}

// Synthetic daily returns that alternate between calm and turbulent spells.
QString sm_sample_regime_returns(unsigned seed = 151, int n = 600) {
    SmRng rng(seed);
    QVector<double> r(n);
    bool turbulent = false;
    for (int i = 0; i < n; ++i) {
        const double u = rng.uniform();
        if (!turbulent && u < 0.02)
            turbulent = true;
        else if (turbulent && u < 0.05)
            turbulent = false;
        r[i] = turbulent ? -0.0015 + 0.022 * rng.normal() : 0.0008 + 0.006 * rng.normal();
    }
    return sm_join(r, 6);
}

// Four synthetic return series driven by one common factor: "SYN1: …; SYN2: …".
QString sm_sample_return_panel(unsigned seed = 161, int n = 250) {
    SmRng rng(seed);
    constexpr double kLoad[4] = {1.0, 0.8, 0.6, 0.2};
    QVector<QVector<double>> cols(4, QVector<double>(n));
    for (int i = 0; i < n; ++i) {
        const double f = 0.01 * rng.normal();
        for (int a = 0; a < 4; ++a)
            cols[a][i] = kLoad[a] * f + 0.005 * rng.normal();
    }
    QStringList parts;
    for (int a = 0; a < 4; ++a)
        parts << QStringLiteral("SYN%1: %2").arg(a + 1).arg(sm_join(cols[a], 6));
    return parts.join(QStringLiteral("; "));
}

// "AAPL: 0.01, -0.02 …; MSFT: …" (series separated by ';' or new lines), or the
// ticker source's {"AAPL": [0.01, …], "MSFT": […]} → {name: [values]}. Returns ""
// or a user-facing reason; `names` keeps the input order, `length` the common length.
QString sm_parse_named_series(const QString& text, QJsonObject& out, QStringList& names, int& length) {
    out = QJsonObject{};
    names.clear();
    length = -1;
    if (text.trimmed().startsWith(QLatin1Char('{'))) {
        QJsonParseError pe{};
        const QJsonDocument doc = QJsonDocument::fromJson(text.trimmed().toUtf8(), &pe);
        if (pe.error != QJsonParseError::NoError || !doc.isObject())
            return QCoreApplication::translate("QuantModulePanel", "The named series are not valid JSON: %1.")
                .arg(pe.errorString());
        const QJsonObject obj = doc.object();
        for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
            const QJsonArray vals = it.value().toArray();
            for (const QJsonValue& v : vals) {
                if (!v.isDouble())
                    return QCoreApplication::translate("QuantModulePanel", "%1: a value is not numeric.").arg(it.key());
            }
            if (length >= 0 && vals.size() != length)
                return QCoreApplication::translate("QuantModulePanel",
                                                   "Every series needs the same length: %1 has %2 values, %3 has %4.")
                    .arg(names.first())
                    .arg(length)
                    .arg(it.key())
                    .arg(vals.size());
            length = static_cast<int>(vals.size());
            names << it.key();
            out[it.key()] = vals;
        }
        return {};
    }
    static const QRegularExpression kSplit(QStringLiteral("[;\\n\\r]"));
    for (const QString& chunk : text.split(kSplit, Qt::SkipEmptyParts)) {
        const QString c = chunk.trimmed();
        if (c.isEmpty())
            continue;
        const qsizetype colon = c.indexOf(QLatin1Char(':'));
        if (colon <= 0)
            return QCoreApplication::translate("QuantModulePanel",
                                               "Each series needs a name — write it as NAME: v1, v2, … (got '%1').")
                .arg(c.left(24));
        const QString name = c.left(colon).trimmed();
        if (names.contains(name, Qt::CaseInsensitive))
            return QCoreApplication::translate("QuantModulePanel", "Series '%1' appears twice.").arg(name);
        QJsonArray vals;
        QString bad;
        if (!parse_doubles(c.mid(colon + 1), vals, &bad))
            return QCoreApplication::translate("QuantModulePanel", "%1: '%2' is not numeric.").arg(name, bad);
        if (length >= 0 && vals.size() != length)
            return QCoreApplication::translate("QuantModulePanel",
                                               "Every series needs the same length: %1 has %2 values, %3 has %4.")
                .arg(names.first())
                .arg(length)
                .arg(name)
                .arg(vals.size());
        length = static_cast<int>(vals.size());
        names << name;
        out[name] = vals;
    }
    return {};
}

// A p-value in 'g' format (tiny p-values stay readable); null → "—".
QString sm_p(const QJsonValue& v) {
    return v.isDouble() ? QString::number(v.toDouble(), 'g', 4) : QString::fromUtf8("—");
}

// A right-aligned, sortable number cell; null → "—" (sorts first).
QTableWidgetItem* sm_num(const QJsonValue& v, int decimals, const QString& color = {}) {
    if (!v.isDouble())
        return num_item(QString::fromUtf8("—"), -std::numeric_limits<double>::infinity());
    return num_item(QString::number(v.toDouble(), 'f', decimals), v.toDouble(), color);
}

// A sortable percent cell for a fraction (0.25 → "25.0%"); null → "—".
QTableWidgetItem* sm_pct(const QJsonValue& v, int decimals, const QString& color = {}) {
    if (!v.isDouble())
        return num_item(QString::fromUtf8("—"), -std::numeric_limits<double>::infinity());
    return num_item(fmt_pct_safe(v, decimals), v.toDouble(), color);
}

// Result-table shell in the panel's style.
QTableWidget* sm_table(int rows, const QStringList& headers, QWidget* parent, int max_height = 320) {
    auto* t = new QTableWidget(rows, static_cast<int>(headers.size()), parent);
    t->setHorizontalHeaderLabels(headers);
    t->verticalHeader()->setVisible(false);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionMode(QAbstractItemView::NoSelection);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    t->setStyleSheet(table_ss());
    t->setMaximumHeight(qMin(rows * row_px(24) + row_px(32), row_px(max_height)));
    return t;
}

// The summary strip above a result (text on the surface, coloured left edge).
QLabel* sm_strip(const QString& text, const QString& edge) {
    auto* l = new QLabel(text);
    l->setWordWrap(true);
    l->setStyleSheet(QString("color:%1; font-size:" + ui::fonts::px(-3) +
                             "px; font-weight:700; padding:8px 10px; background:%2; border-left:3px solid %3;")
                         .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), edge));
    return l;
}

// A muted explanatory line under a tab's inputs.
QLabel* sm_hint(const QString& text, QWidget* parent) {
    auto* l = new QLabel(text, parent);
    l->setWordWrap(true);
    l->setStyleSheet(QString("color:%1; font-size:" + ui::fonts::px(-4) + "px;").arg(ui::colors::TEXT_TERTIARY()));
    return l;
}

// Crosshair / tooltip title on an observation-number axis.
std::function<QString(double)> sm_obs_label() {
    return [](double v) { return QStringLiteral("obs %1").arg(static_cast<qlonglong>(std::llround(v))); };
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════════════
// STATSMODELS PANEL — 11 econometric analyses. Mirrors the GS Quant / Functime
// pattern: one view per operation, one service call. Each view opens on real
// data (D1: the daily prices of a symbol, a pair or a list, 1 year) and runs on
// first show; the paste box is the "Custom series" source.
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* QuantModulePanel::build_statsmodels_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* tabs = make_view_tabs(w);

    // Shared helper: synthetic-sample button styled by the panel sheet; runs
    // fed with its data are flagged SAMPLE INPUT on the results.
    auto add_sample_btn = [this](QLineEdit* edit, QWidget* parent, unsigned seed, const QString& tip,
                                 bool walk = false) -> QPushButton* {
        return make_sample_button(edit, parent, tip, [seed, walk]() {
            return walk ? sample_random_walk(seed) : sample_series(seed);
        });
    };

    // ── OLS Regression ───────────────────────────────────────────────────────
    add_view_tab(tabs, {.key = QStringLiteral("ols"), .cost = ViewCost::Fetch, .loading = tr("Fitting the OLS regression…")}, tr("OLS Regression"),
                 [this, add_sample_btn](QWidget* ols, QVBoxLayout* oll) {
        // Real data by default (D1): y = the symbol's daily returns, x = the regressor's.
        add_series_source(ols, oll, SeriesShape::Pair, tr("Regressor"));

        auto* ols_y = new QLineEdit(ols);
        ols_y->setPlaceholderText(tr("Dependent variable y (>= 10 values)"));
        ols_y->setStyleSheet(input_ss());
        text_inputs_["sm_ols_y"] = ols_y;
        mark_series_input(ols_y, SeriesKind::Returns, 0);
        oll->addWidget(build_input_row(tr("y (Dependent)"), ols_y, ols));
        oll->addWidget(add_sample_btn(ols_y, ols, 71, tr("100-pt synthetic series (level ~50)")));

        auto* ols_x = new QLineEdit(ols);
        ols_x->setPlaceholderText(
            tr("Regressor x (single column, same length as y). For multi-feature use the JSON 2D form."));
        ols_x->setStyleSheet(input_ss());
        text_inputs_["sm_ols_x"] = ols_x;
        mark_series_input(ols_x, SeriesKind::Returns, 1);
        oll->addWidget(build_input_row(tr("x (Regressor)"), ols_x, ols));
        oll->addWidget(add_sample_btn(ols_x, ols, 72, tr("100-pt synthetic regressor")));

        auto* ols_const = new QComboBox(ols);
        ols_const->addItems({"true", "false"});
        ols_const->setStyleSheet(combo_ss());
        combo_inputs_["sm_ols_const"] = ols_const;
        oll->addWidget(build_input_row(tr("Add Constant"), ols_const, ols));

        auto* ols_run = make_run_button(tr("RUN OLS REGRESSION"), ols);
        set_action(ols_run, [this]() {
            QJsonArray y, x;
            QString bad;
            if (!parse_doubles(series_text("sm_ols_y"), y, &bad)) {
                display_error(tr("y: '%1' is not numeric.").arg(bad));
                return;
            }
            if (!parse_doubles(series_text("sm_ols_x"), x, &bad)) {
                display_error(tr("x: '%1' is not numeric.").arg(bad));
                return;
            }
            if (y.size() < 10) {
                display_error(tr("OLS needs at least 10 observations; you provided %1.").arg(y.size()));
                return;
            }
            if (y.size() != x.size()) {
                display_error(tr("y (%1) and x (%2) must have the same length.").arg(y.size()).arg(x.size()));
                return;
            }
            show_loading(tr("Fitting OLS on %1 obs...").arg(y.size()));
            QJsonObject params;
            params["y"] = y;
            params["x"] = x;
            params["add_constant"] = combo_inputs_["sm_ols_const"]->currentText() == "true";
            AIQuantLabService::instance().sm_ols(params);
        });
        oll->addWidget(ols_run);
        oll->addStretch();
    });

    // ── ARIMA ────────────────────────────────────────────────────────────────
    add_view_tab(tabs, {.key = QStringLiteral("arima"), .cost = ViewCost::Fetch, .loading = tr("Fitting the ARIMA model and forecasting…")}, tr("ARIMA"),
                 [this, add_sample_btn](QWidget* ar, QVBoxLayout* arl) {
        add_series_source(ar, arl, SeriesShape::Single); // daily closes of the symbol (D1)

        auto* ar_vals = new QLineEdit(ar);
        ar_vals->setPlaceholderText(tr("Time series values (>= 30)"));
        ar_vals->setStyleSheet(input_ss());
        text_inputs_["sm_ar_values"] = ar_vals;
        mark_series_input(ar_vals, SeriesKind::Closes);
        arl->addWidget(build_input_row(tr("Series Values"), ar_vals, ar));
        arl->addWidget(add_sample_btn(ar_vals, ar, 81, tr("200-pt synthetic series with seasonality")));

        auto* ar_p = new QSpinBox(ar);
        ar_p->setRange(0, 10);
        ar_p->setValue(1);
        ar_p->setStyleSheet(spinbox_ss());
        int_inputs_["sm_ar_p"] = ar_p;
        arl->addWidget(build_input_row(tr("AR Order p"), ar_p, ar));

        auto* ar_d = new QSpinBox(ar);
        ar_d->setRange(0, 3);
        ar_d->setValue(1);
        ar_d->setStyleSheet(spinbox_ss());
        int_inputs_["sm_ar_d"] = ar_d;
        arl->addWidget(build_input_row(tr("Differencing d"), ar_d, ar));

        auto* ar_q = new QSpinBox(ar);
        ar_q->setRange(0, 10);
        ar_q->setValue(1);
        ar_q->setStyleSheet(spinbox_ss());
        int_inputs_["sm_ar_q"] = ar_q;
        arl->addWidget(build_input_row(tr("MA Order q"), ar_q, ar));

        auto* ar_h = new QSpinBox(ar);
        ar_h->setRange(1, 365);
        ar_h->setValue(14);
        ar_h->setStyleSheet(spinbox_ss());
        int_inputs_["sm_ar_horizon"] = ar_h;
        arl->addWidget(build_input_row(tr("Forecast Horizon"), ar_h, ar));

        auto* ar_run = make_run_button(tr("FIT ARIMA + FORECAST"), ar);
        set_action(ar_run, [this]() {
            QJsonArray vals;
            QString bad;
            if (!parse_doubles(series_text("sm_ar_values"), vals, &bad)) {
                display_error(tr("Series Values: '%1' is not numeric.").arg(bad));
                return;
            }
            if (vals.size() < 30) {
                display_error(tr("ARIMA needs at least 30 observations; you provided %1.").arg(vals.size()));
                return;
            }
            const int p = int_inputs_["sm_ar_p"]->value();
            const int d = int_inputs_["sm_ar_d"]->value();
            const int q = int_inputs_["sm_ar_q"]->value();
            show_loading(tr("Fitting ARIMA(%1,%2,%3) on %4 obs and projecting %5 steps...")
                             .arg(p)
                             .arg(d)
                             .arg(q)
                             .arg(vals.size())
                             .arg(int_inputs_["sm_ar_horizon"]->value()));
            QJsonObject params;
            params["values"] = vals;
            params["p"] = p;
            params["d"] = d;
            params["q"] = q;
            params["horizon"] = int_inputs_["sm_ar_horizon"]->value();
            AIQuantLabService::instance().sm_arima(params);
        });
        arl->addWidget(ar_run);
        arl->addStretch();
    });

    // ── Stationarity Tests (single d, full ADF + KPSS detail) ────────────────
    add_view_tab(tabs, {.key = QStringLiteral("stationarity"), .cost = ViewCost::Fetch, .loading = tr("Running the ADF and KPSS tests…"), .shape = ui::LoadingSkeleton::Shape::Cards}, tr("Stationarity"),
                 [this, add_sample_btn](QWidget* st, QVBoxLayout* stl) {
        // Price levels (D1): at d = 0 a price is the textbook non-stationary series; d = 1 tests its returns.
        add_series_source(st, stl, SeriesShape::Single);

        auto* st_vals = new QLineEdit(st);
        st_vals->setPlaceholderText(tr("Time series values (>= 30)"));
        st_vals->setStyleSheet(input_ss());
        text_inputs_["sm_st_values"] = st_vals;
        mark_series_input(st_vals, SeriesKind::Closes);
        stl->addWidget(build_input_row(tr("Series Values"), st_vals, st));
        stl->addWidget(add_sample_btn(st_vals, st, 91, tr("150-pt random walk with drift"), true));

        auto* st_d = new QSpinBox(st);
        st_d->setRange(0, 3);
        st_d->setValue(0);
        st_d->setStyleSheet(spinbox_ss());
        int_inputs_["sm_st_d"] = st_d;
        stl->addWidget(build_input_row(tr("Differencing Order d"), st_d, st));

        auto* st_adf = new QComboBox(st);
        st_adf->addItems({"c", "ct", "ctt", "n"});
        st_adf->setToolTip(
            tr("c = constant, ct = constant + trend, ctt = constant + trend + quadratic trend, n = no constant"));
        st_adf->setStyleSheet(combo_ss());
        combo_inputs_["sm_st_adf_reg"] = st_adf;
        stl->addWidget(build_input_row(tr("ADF Regression"), st_adf, st));

        auto* st_kpss = new QComboBox(st);
        st_kpss->addItems({"c", "ct"});
        st_kpss->setToolTip(tr("c = constant only, ct = constant + trend"));
        st_kpss->setStyleSheet(combo_ss());
        combo_inputs_["sm_st_kpss_reg"] = st_kpss;
        stl->addWidget(build_input_row(tr("KPSS Regression"), st_kpss, st));

        auto* st_run = make_run_button(tr("RUN ADF + KPSS"), st);
        set_action(st_run, [this]() {
            QJsonArray vals;
            QString bad;
            if (!parse_doubles(series_text("sm_st_values"), vals, &bad)) {
                display_error(tr("Series Values: '%1' is not numeric.").arg(bad));
                return;
            }
            if (vals.size() < 30) {
                display_error(tr("Stationarity tests need at least 30 observations; you provided %1.").arg(vals.size()));
                return;
            }
            show_loading(tr("Running ADF + KPSS at d=%1...").arg(int_inputs_["sm_st_d"]->value()));
            QJsonObject params;
            params["values"] = vals;
            params["d"] = int_inputs_["sm_st_d"]->value();
            params["adf_regression"] = combo_inputs_["sm_st_adf_reg"]->currentText();
            params["kpss_regression"] = combo_inputs_["sm_st_kpss_reg"]->currentText();
            AIQuantLabService::instance().sm_stationarity_tests(params);
        });
        stl->addWidget(st_run);
        stl->addStretch();
    });

    // ── ACF / PACF ───────────────────────────────────────────────────────────
    add_view_tab(tabs, {.key = QStringLiteral("acf_pacf"), .cost = ViewCost::Fetch, .loading = tr("Computing the ACF and PACF…")}, tr("ACF / PACF"),
                 [this, add_sample_btn](QWidget* ap, QVBoxLayout* apl) {
        add_series_source(ap, apl, SeriesShape::Single); // daily returns of the symbol (D1)

        auto* ap_vals = new QLineEdit(ap);
        ap_vals->setPlaceholderText(tr("Time series values (>= 20). Used for ARIMA(p,q) order selection."));
        ap_vals->setStyleSheet(input_ss());
        text_inputs_["sm_ap_values"] = ap_vals;
        mark_series_input(ap_vals, SeriesKind::Returns);
        apl->addWidget(build_input_row(tr("Series Values"), ap_vals, ap));
        apl->addWidget(add_sample_btn(ap_vals, ap, 101, tr("200-pt synthetic series with seasonality")));

        auto* ap_lags = new QSpinBox(ap);
        ap_lags->setRange(2, 100);
        ap_lags->setValue(20);
        ap_lags->setStyleSheet(spinbox_ss());
        int_inputs_["sm_ap_nlags"] = ap_lags;
        apl->addWidget(build_input_row(tr("Number of Lags"), ap_lags, ap));

        auto* ap_run = make_run_button(tr("COMPUTE ACF + PACF"), ap);
        set_action(ap_run, [this]() {
            QJsonArray vals;
            QString bad;
            if (!parse_doubles(series_text("sm_ap_values"), vals, &bad)) {
                display_error(tr("Series Values: '%1' is not numeric.").arg(bad));
                return;
            }
            if (vals.size() < 20) {
                display_error(tr("ACF/PACF needs at least 20 observations; you provided %1.").arg(vals.size()));
                return;
            }
            show_loading(tr("Computing ACF + PACF up to lag %1...").arg(int_inputs_["sm_ap_nlags"]->value()));
            QJsonObject params;
            params["values"] = vals;
            params["nlags"] = int_inputs_["sm_ap_nlags"]->value();
            AIQuantLabService::instance().sm_acf_pacf(params);
        });
        apl->addWidget(ap_run);
        apl->addStretch();
    });

    // ── Granger Causality ────────────────────────────────────────────────────
    add_view_tab(tabs, {.key = QStringLiteral("granger"), .cost = ViewCost::Fetch, .loading = tr("Testing Granger causality…"), .shape = ui::LoadingSkeleton::Shape::Table}, tr("Granger Causality"),
                 [this, add_sample_btn](QWidget* gc, QVBoxLayout* gcl) {
        // Daily returns of both symbols (D1): does the second one's past help predict the first?
        add_series_source(gc, gcl, SeriesShape::Pair, tr("Cause (x)"));

        auto* gc_y = new QLineEdit(gc);
        gc_y->setPlaceholderText(tr("Effect series y (the one we ask: 'is this caused by x?')"));
        gc_y->setStyleSheet(input_ss());
        text_inputs_["sm_gc_y"] = gc_y;
        mark_series_input(gc_y, SeriesKind::Returns, 0);
        gcl->addWidget(build_input_row(tr("y (Effect)"), gc_y, gc));
        gcl->addWidget(add_sample_btn(gc_y, gc, 111, tr("200-pt synthetic effect series")));

        auto* gc_x = new QLineEdit(gc);
        gc_x->setPlaceholderText(tr("Potential cause series x (same length as y)"));
        gc_x->setStyleSheet(input_ss());
        text_inputs_["sm_gc_x"] = gc_x;
        mark_series_input(gc_x, SeriesKind::Returns, 1);
        gcl->addWidget(build_input_row(tr("x (Potential Cause)"), gc_x, gc));
        gcl->addWidget(add_sample_btn(gc_x, gc, 112, tr("200-pt synthetic candidate cause")));

        auto* gc_lag = new QSpinBox(gc);
        gc_lag->setRange(1, 20);
        gc_lag->setValue(4);
        gc_lag->setStyleSheet(spinbox_ss());
        int_inputs_["sm_gc_max_lag"] = gc_lag;
        gcl->addWidget(build_input_row(tr("Max Lag to Test"), gc_lag, gc));

        auto* gc_hint = new QLabel(
            tr("H₀: x does NOT Granger-cause y. p < 0.05 ⇒ x carries predictive information about y at that lag."), gc);
        gc_hint->setWordWrap(true);
        gc_hint->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px;").arg(ui::colors::TEXT_TERTIARY()));
        gcl->addWidget(gc_hint);

        auto* gc_run = make_run_button(tr("TEST GRANGER CAUSALITY"), gc);
        set_action(gc_run, [this]() {
            QJsonArray y, x;
            QString bad;
            if (!parse_doubles(series_text("sm_gc_y"), y, &bad)) {
                display_error(tr("y: '%1' is not numeric.").arg(bad));
                return;
            }
            if (!parse_doubles(series_text("sm_gc_x"), x, &bad)) {
                display_error(tr("x: '%1' is not numeric.").arg(bad));
                return;
            }
            if (y.size() < 30 || x.size() < 30) {
                display_error(tr("Granger test needs at least 30 observations in both series."));
                return;
            }
            if (y.size() != x.size()) {
                display_error(tr("y (%1) and x (%2) must have the same length.").arg(y.size()).arg(x.size()));
                return;
            }
            show_loading(tr("Testing Granger causality up to lag %1...").arg(int_inputs_["sm_gc_max_lag"]->value()));
            QJsonObject params;
            params["y"] = y;
            params["x"] = x;
            params["max_lag"] = int_inputs_["sm_gc_max_lag"]->value();
            AIQuantLabService::instance().sm_granger_causality(params);
        });
        gcl->addWidget(gc_run);
        gcl->addStretch();
    });

    // ── Descriptive + Normality ──────────────────────────────────────────────
    add_view_tab(tabs, {.key = QStringLiteral("descriptive"), .cost = ViewCost::Fetch, .loading = tr("Computing statistics and normality tests…"), .shape = ui::LoadingSkeleton::Shape::Cards}, tr("Descriptive"),
                 [this, add_sample_btn](QWidget* de, QVBoxLayout* del) {
        add_series_source(de, del, SeriesShape::Single); // daily returns of the symbol (D1)

        auto* de_vals = new QLineEdit(de);
        de_vals->setPlaceholderText(tr("Numeric values (>= 8). Includes Jarque-Bera + Shapiro-Wilk normality tests."));
        de_vals->setStyleSheet(input_ss());
        text_inputs_["sm_de_values"] = de_vals;
        mark_series_input(de_vals, SeriesKind::Returns);
        del->addWidget(build_input_row(tr("Values"), de_vals, de));
        del->addWidget(add_sample_btn(de_vals, de, 121, tr("200-pt synthetic series")));

        auto* de_run = make_run_button(tr("DESCRIBE + TEST NORMALITY"), de);
        set_action(de_run, [this]() {
            QJsonArray vals;
            QString bad;
            if (!parse_doubles(series_text("sm_de_values"), vals, &bad)) {
                display_error(tr("Values: '%1' is not numeric.").arg(bad));
                return;
            }
            if (vals.size() < 8) {
                display_error(tr("Need at least 8 values; you provided %1.").arg(vals.size()));
                return;
            }
            show_loading(tr("Computing descriptive statistics + normality on %1 values...").arg(vals.size()));
            QJsonObject params;
            params["values"] = vals;
            AIQuantLabService::instance().sm_descriptive(params);
        });
        del->addWidget(de_run);
        del->addStretch();
    });

    // ── Cointegration (Engle-Granger) ────────────────────────────────────────
    // Two level series, like Granger's y / x pair: the daily closes of both symbols (D1), or pasted.
    add_view_tab(tabs, {.key = QStringLiteral("cointegration"), .cost = ViewCost::Fetch, .loading = tr("Testing cointegration (Engle-Granger)…")}, tr("Cointegration"),
                 [this](QWidget* co, QVBoxLayout* col) {
        add_series_source(co, col, SeriesShape::Pair, tr("Leg X"));

        auto* co_y = new QLineEdit(co);
        co_y->setPlaceholderText(tr("Leg Y price levels (>= 30), e.g. the first ticker's closes"));
        co_y->setStyleSheet(input_ss());
        text_inputs_["sm_co_y"] = co_y;
        mark_series_input(co_y, SeriesKind::Closes, 0);
        col->addWidget(build_input_row(tr("y (Leg Y levels)"), co_y, co));
        col->addWidget(make_sample_button(co_y, co, tr("250-pt synthetic leg Y = 2 + 1.5·X + mean-reverting noise"),
                                          []() { return sm_sample_coint_leg(true); }));

        auto* co_x = new QLineEdit(co);
        co_x->setPlaceholderText(tr("Leg X price levels (same length and dates as y)"));
        co_x->setStyleSheet(input_ss());
        text_inputs_["sm_co_x"] = co_x;
        mark_series_input(co_x, SeriesKind::Closes, 1);
        col->addWidget(build_input_row(tr("x (Leg X levels)"), co_x, co));
        col->addWidget(make_sample_button(co_x, co, tr("250-pt synthetic random walk (leg X of the same pair)"),
                                          []() { return sm_sample_coint_leg(false); }));

        auto* co_trend = new QComboBox(co);
        co_trend->addItems({"c", "ct", "ctt", "n"});
        co_trend->setToolTip(tr("Deterministic terms of the cointegrating regression: c = constant, ct = constant + "
                                "trend, ctt = constant + trend + quadratic trend, n = none"));
        co_trend->setStyleSheet(combo_ss());
        combo_inputs_["sm_co_trend"] = co_trend;
        col->addWidget(build_input_row(tr("Trend Terms"), co_trend, co));

        col->addWidget(sm_hint(tr("Engle-Granger: H₀ = no cointegration. p < 0.05 ⇒ the spread y − α − β·x mean-reverts. "
                                  "A custom series takes price levels of both legs on the same dates, not returns."),
                               co));

        auto* co_run = make_run_button(tr("TEST COINTEGRATION"), co);
        set_action(co_run, [this]() {
            QJsonArray y, x;
            QString bad;
            if (!parse_doubles(series_text("sm_co_y"), y, &bad)) {
                display_error(tr("y: '%1' is not numeric.").arg(bad));
                return;
            }
            if (!parse_doubles(series_text("sm_co_x"), x, &bad)) {
                display_error(tr("x: '%1' is not numeric.").arg(bad));
                return;
            }
            if (y.size() < 30 || x.size() < 30) {
                display_error(tr("Cointegration needs at least 30 observations in both legs."));
                return;
            }
            if (y.size() != x.size()) {
                display_error(tr("y (%1) and x (%2) must have the same length.").arg(y.size()).arg(x.size()));
                return;
            }
            show_loading(tr("Testing cointegration on %1 paired observations (Engle-Granger)...").arg(y.size()));
            QJsonObject params;
            params["y"] = y;
            params["x"] = x;
            params["trend"] = combo_inputs_["sm_co_trend"]->currentText();
            AIQuantLabService::instance().sm_cointegration(params);
        });
        col->addWidget(co_run);
        col->addStretch();
    });

    // ── Rolling OLS (rolling beta) ───────────────────────────────────────────
    add_view_tab(tabs, {.key = QStringLiteral("rolling_beta"), .cost = ViewCost::Fetch, .loading = tr("Rolling the beta regression…"), .shape = ui::LoadingSkeleton::Shape::Chart}, tr("Rolling Beta"),
                 [this](QWidget* ro, QVBoxLayout* rol) {
        // The symbol's daily returns regressed on the benchmark's (D1).
        add_series_source(ro, rol, SeriesShape::Pair, tr("Benchmark"));

        auto* ro_y = new QLineEdit(ro);
        ro_y->setPlaceholderText(tr("Dependent y, e.g. the asset's daily returns (>= 20)"));
        ro_y->setStyleSheet(input_ss());
        text_inputs_["sm_ro_y"] = ro_y;
        mark_series_input(ro_y, SeriesKind::Returns, 0);
        rol->addWidget(build_input_row(tr("y (Asset)"), ro_y, ro));
        rol->addWidget(make_sample_button(ro_y, ro, tr("250 synthetic asset returns whose beta steps from 0.6 to 1.4"),
                                          []() { return sm_sample_beta_leg(true); }));

        auto* ro_x = new QLineEdit(ro);
        ro_x->setPlaceholderText(tr("Regressor x, e.g. the benchmark's daily returns (same length as y)"));
        ro_x->setStyleSheet(input_ss());
        text_inputs_["sm_ro_x"] = ro_x;
        mark_series_input(ro_x, SeriesKind::Returns, 1);
        rol->addWidget(build_input_row(tr("x (Benchmark)"), ro_x, ro));
        rol->addWidget(make_sample_button(ro_x, ro, tr("250 synthetic benchmark returns (pairs with the asset sample)"),
                                          []() { return sm_sample_beta_leg(false); }));

        auto* ro_window = new QSpinBox(ro);
        ro_window->setRange(10, 2000);
        ro_window->setValue(60);
        ro_window->setSuffix(tr(" obs"));
        ro_window->setStyleSheet(spinbox_ss());
        int_inputs_["sm_ro_window"] = ro_window;
        rol->addWidget(build_input_row(tr("Window"), ro_window, ro));

        rol->addWidget(sm_hint(tr("Each point is an OLS of y on x with an intercept over the trailing window; the "
                                  "first window − 1 points have no estimate."),
                               ro));

        auto* ro_run = make_run_button(tr("RUN ROLLING REGRESSION"), ro);
        set_action(ro_run, [this]() {
            QJsonArray y, x;
            QString bad;
            if (!parse_doubles(series_text("sm_ro_y"), y, &bad)) {
                display_error(tr("y: '%1' is not numeric.").arg(bad));
                return;
            }
            if (!parse_doubles(series_text("sm_ro_x"), x, &bad)) {
                display_error(tr("x: '%1' is not numeric.").arg(bad));
                return;
            }
            if (y.size() < 20 || x.size() < 20) {
                display_error(tr("Rolling OLS needs at least 20 observations in both series."));
                return;
            }
            if (y.size() != x.size()) {
                display_error(tr("y (%1) and x (%2) must have the same length.").arg(y.size()).arg(x.size()));
                return;
            }
            const int window = int_inputs_["sm_ro_window"]->value();
            if (window > y.size()) {
                display_error(tr("The window (%1) is longer than the series (%2 observations).").arg(window).arg(y.size()));
                return;
            }
            show_loading(tr("Rolling a %1-observation OLS window over %2 points...").arg(window).arg(y.size()));
            QJsonObject params;
            params["y"] = y;
            params["x"] = x;
            params["window"] = window;
            AIQuantLabService::instance().sm_rolling_ols(params);
        });
        rol->addWidget(ro_run);
        rol->addStretch();
    });

    // ── Regime switching (Markov) ────────────────────────────────────────────
    add_view_tab(tabs, {.key = QStringLiteral("regimes"), .cost = ViewCost::Fetch, .loading = tr("Fitting the Markov regime model…")}, tr("Regimes"),
                 [this](QWidget* rs, QVBoxLayout* rsl) {
        add_series_source(rs, rsl, SeriesShape::Single); // daily returns of the symbol (D1)

        auto* rs_vals = new QLineEdit(rs);
        rs_vals->setPlaceholderText(tr("Return series (>= 60, and >= 30 per regime) — returns, not prices"));
        rs_vals->setStyleSheet(input_ss());
        text_inputs_["sm_rs_values"] = rs_vals;
        mark_series_input(rs_vals, SeriesKind::Returns);
        rsl->addWidget(build_input_row(tr("Returns"), rs_vals, rs));
        rsl->addWidget(make_sample_button(rs_vals, rs, tr("600 synthetic daily returns alternating calm and turbulent spells"),
                                          []() { return sm_sample_regime_returns(); }));

        auto* rs_k = new QSpinBox(rs);
        rs_k->setRange(2, 4);
        rs_k->setValue(2);
        rs_k->setStyleSheet(spinbox_ss());
        int_inputs_["sm_rs_k"] = rs_k;
        rsl->addWidget(build_input_row(tr("Regimes (k)"), rs_k, rs));

        auto* rs_sv = new QComboBox(rs);
        rs_sv->addItems({"true", "false"});
        rs_sv->setToolTip(tr("true: each regime has its own variance (volatility regimes). false: the regimes differ "
                             "in mean only."));
        rs_sv->setStyleSheet(combo_ss());
        combo_inputs_["sm_rs_switch_var"] = rs_sv;
        rsl->addWidget(build_input_row(tr("Switching Variance"), rs_sv, rs));

        rsl->addWidget(sm_hint(tr("Markov-switching (Hamilton) mean / variance model. Regimes are numbered by volatility: "
                                  "regime 0 is the calmest."),
                               rs));

        auto* rs_run = make_run_button(tr("FIT REGIME MODEL"), rs);
        set_action(rs_run, [this]() {
            QJsonArray vals;
            QString bad;
            if (!parse_doubles(series_text("sm_rs_values"), vals, &bad)) {
                display_error(tr("Returns: '%1' is not numeric.").arg(bad));
                return;
            }
            const int k = int_inputs_["sm_rs_k"]->value();
            const int min_n = std::max(60, 30 * k);
            if (vals.size() < min_n) {
                display_error(tr("A %1-regime model needs at least %2 returns; you provided %3.")
                                  .arg(k)
                                  .arg(min_n)
                                  .arg(vals.size()));
                return;
            }
            show_loading(tr("Fitting a %1-regime Markov-switching model on %2 returns...").arg(k).arg(vals.size()));
            QJsonObject params;
            params["values"] = vals;
            params["k_regimes"] = k;
            params["switching_variance"] = combo_inputs_["sm_rs_switch_var"]->currentText() == "true";
            AIQuantLabService::instance().sm_regime_switching(params);
        });
        rsl->addWidget(rs_run);
        rsl->addStretch();
    });

    // ── PCA (several named series) ───────────────────────────────────────────
    add_view_tab(tabs, {.key = QStringLiteral("pca"), .cost = ViewCost::Fetch, .loading = tr("Running the principal component analysis…")}, tr("PCA"),
                 [this](QWidget* pc, QVBoxLayout* pcl) {
        // The daily returns of several symbols (D1; default SPY, QQQ, IWM, TLT, GLD), or pasted series.
        add_series_source(pc, pcl, SeriesShape::List);

        auto* pc_series = new QLineEdit(pc);
        pc_series->setPlaceholderText(tr("One series per ticker: AAPL: 0.012, -0.004, …; MSFT: …  (>= 2 series, same "
                                         "length, >= 20 values)"));
        pc_series->setStyleSheet(input_ss());
        text_inputs_["sm_pca_series"] = pc_series;
        mark_series_input(pc_series, SeriesKind::SeriesMatrixJson);
        pcl->addWidget(build_input_row(tr("Named Series"), pc_series, pc));
        pcl->addWidget(make_sample_button(pc_series, pc, tr("4 synthetic return series (SYN1–SYN4) driven by one common factor"),
                                          []() { return sm_sample_return_panel(); }));

        auto* pc_input = new QComboBox(pc);
        pc_input->addItems({"returns", "prices"});
        pc_input->setToolTip(tr("Of a custom series: prices are converted to simple returns before the "
                                "decomposition. A ticker source always provides daily returns."));
        pc_input->setStyleSheet(combo_ss());
        combo_inputs_["sm_pca_input"] = pc_input;
        pcl->addWidget(build_input_row(tr("Input Type"), pc_input, pc));
        mark_series_widget(pc_input->parentWidget(), /*ticker_only=*/false); // a ticker run sends returns

        auto* pc_ncomp = new QSpinBox(pc);
        pc_ncomp->setRange(1, 20);
        pc_ncomp->setValue(3);
        pc_ncomp->setStyleSheet(spinbox_ss());
        int_inputs_["sm_pca_ncomp"] = pc_ncomp;
        pcl->addWidget(build_input_row(tr("Components"), pc_ncomp, pc));

        auto* pc_std = new QComboBox(pc);
        pc_std->addItems({"true", "false"});
        pc_std->setToolTip(tr("true: correlation-matrix PCA (each series scaled to unit variance). false: covariance "
                              "matrix (high-volatility series dominate)."));
        pc_std->setStyleSheet(combo_ss());
        combo_inputs_["sm_pca_standardize"] = pc_std;
        pcl->addWidget(build_input_row(tr("Standardize"), pc_std, pc));

        auto* pc_run = make_run_button(tr("RUN PCA"), pc);
        set_action(pc_run, [this]() {
            QJsonObject series;
            QStringList names;
            int length = 0;
            const QString err = sm_parse_named_series(series_text("sm_pca_series"), series, names, length);
            if (!err.isEmpty()) {
                display_error(err);
                return;
            }
            if (names.size() < 2) {
                display_error(tr("PCA needs at least 2 named series (NAME: values; NAME: values)."));
                return;
            }
            if (length < 20) {
                display_error(tr("PCA needs at least 20 values per series; you provided %1.").arg(length));
                return;
            }
            const int ncomp = int_inputs_["sm_pca_ncomp"]->value();
            if (ncomp > names.size()) {
                display_error(
                    tr("Components (%1) cannot exceed the number of series (%2).").arg(ncomp).arg(names.size()));
                return;
            }
            show_loading(tr("Running PCA on %1 series × %2 observations...").arg(names.size()).arg(length));
            QJsonObject params;
            params["series"] = series;
            params["n_components"] = ncomp;
            params["standardize"] = combo_inputs_["sm_pca_standardize"]->currentText() == "true";
            // The ticker source hands over returns whatever the Input Type says.
            params["input_type"] = series_ticker_mode(QStringLiteral("pca"))
                                       ? QStringLiteral("returns")
                                       : combo_inputs_["sm_pca_input"]->currentText();
            AIQuantLabService::instance().sm_pca(params);
        });
        pcl->addWidget(pc_run);
        pcl->addStretch();
    });

    // ── STL decomposition ────────────────────────────────────────────────────
    add_view_tab(tabs, {.key = QStringLiteral("stl"), .cost = ViewCost::Fetch, .loading = tr("Decomposing the series (STL)…"), .shape = ui::LoadingSkeleton::Shape::Chart}, tr("STL Decompose"),
                 [this, add_sample_btn](QWidget* dc, QVBoxLayout* dcl) {
        add_series_source(dc, dcl, SeriesShape::Single); // daily closes of the symbol (D1)

        auto* dc_vals = new QLineEdit(dc);
        dc_vals->setPlaceholderText(tr("Time series values (>= 24 and at least two full seasons)"));
        dc_vals->setStyleSheet(input_ss());
        text_inputs_["sm_dc_values"] = dc_vals;
        mark_series_input(dc_vals, SeriesKind::Closes);
        dcl->addWidget(build_input_row(tr("Series Values"), dc_vals, dc));
        auto* dc_sample = add_sample_btn(dc_vals, dc, 171, tr("200-pt series with embedded period-30 seasonality"));
        dcl->addWidget(dc_sample);

        // Daily prices have no cycle the autocorrelation can find (auto fails on them), so the
        // default is one trading month; the synthetic sample carries its own period-30 cycle.
        auto* dc_period = new QSpinBox(dc);
        dc_period->setRange(0, 365);
        dc_period->setValue(21);
        dc_period->setSpecialValueText(tr("auto"));
        dc_period->setToolTip(tr("Length of one seasonal cycle in observations: 21 = a trading month of daily "
                                 "prices, 5 = a trading week. 0 detects it from the autocorrelation."));
        dc_period->setStyleSheet(spinbox_ss());
        int_inputs_["sm_dc_period"] = dc_period;
        dcl->addWidget(build_input_row(tr("Period (0 = auto)"), dc_period, dc));
        connect(dc_sample, &QPushButton::clicked, dc_period, [dc_period]() { dc_period->setValue(0); });

        auto* dc_robust = new QComboBox(dc);
        dc_robust->addItems({"true", "false"});
        dc_robust->setToolTip(tr("Robust STL down-weights outliers so a few spikes do not bend the trend."));
        dc_robust->setStyleSheet(combo_ss());
        combo_inputs_["sm_dc_robust"] = dc_robust;
        dcl->addWidget(build_input_row(tr("Robust STL"), dc_robust, dc));

        auto* dc_run = make_run_button(tr("DECOMPOSE (STL)"), dc);
        set_action(dc_run, [this]() {
            QJsonArray vals;
            QString bad;
            if (!parse_doubles(series_text("sm_dc_values"), vals, &bad)) {
                display_error(tr("Series Values: '%1' is not numeric.").arg(bad));
                return;
            }
            if (vals.size() < 24) {
                display_error(tr("STL needs at least 24 observations; you provided %1.").arg(vals.size()));
                return;
            }
            const int period = int_inputs_["sm_dc_period"]->value();
            if (period >= 2 && vals.size() < 2 * period + 1) {
                display_error(tr("A period of %1 needs at least %2 observations (two full seasons); you provided %3.")
                                  .arg(period)
                                  .arg(2 * period + 1)
                                  .arg(vals.size()));
                return;
            }
            show_loading(tr("Running STL decomposition on %1 obs...").arg(vals.size()));
            QJsonObject params;
            params["values"] = vals;
            params["period"] = period;
            params["robust"] = combo_inputs_["sm_dc_robust"]->currentText() == "true";
            AIQuantLabService::instance().sm_decompose(params);
        });
        dcl->addWidget(dc_run);
        dcl->addStretch();
    });

    vl->addWidget(tabs);

    add_result_pane(vl);

    return w;
}

// ── STATSMODELS: rich card-based display per command ────────────────────────

void QuantModulePanel::display_statsmodels_result(const QString& command, const QJsonObject& payload) {
    // STL decomposition answers in the shape of Functime's seasonality op (plus
    // robust / method): one renderer draws both.
    if (command == "decompose") {
        display_functime_result(command, payload);
        return;
    }

    clear_results();

    if (!payload.value("success").toBool(false)) {
        const QString err = payload.value("error").toString(tr("Unknown error"));
        const QString kind = payload.value("error_kind").toString();
        const QString prefix = kind == "validation" ? tr("Input error: ")
                               : kind == "runtime"  ? tr("Computation failed: ")
                                                    : QString();
        display_error(prefix + err);
        return;
    }

    const QJsonObject d = payload.value("data").toObject();
    const QString accent = ui::colors::legible(module_.color);

    // Header strip — same shape as gs_quant / functime
    QString header_text = command.toUpper();
    header_text.replace('_', ' ');
    if (d.contains("n_observations"))
        header_text += tr("  |  %1 OBS").arg(d.value("n_observations").toInt());
    results_layout_->addWidget(gs_section_header(header_text, accent));

    // ── 2. OLS ───────────────────────────────────────────────────────────────
    if (command == "ols") {
        const QJsonValue r2 = d.value("r_squared");
        const QJsonValue f_p = d.value("f_p_value");
        const QJsonValue dw = d.value("durbin_watson");
        const QJsonValue homo = d.value("homoscedastic_5pct");

        auto* hdr = new QLabel(tr("OLS  |  %1 OBS  |  %2 FEATURES%3")
                                   .arg(d.value("n_observations").toInt())
                                   .arg(d.value("n_features").toInt())
                                   .arg(d.value("has_constant").toBool() ? tr("  +  CONSTANT") : QString()));
        hdr->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700;"
                                   "padding:8px 10px; background:%2; border:1px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
        results_layout_->addWidget(hdr);

        const QString r2_col = !r2.isDouble()           ? QString(ui::colors::TEXT_SECONDARY())
                               : r2.toDouble() >= 0.7 ? QString(ui::colors::POSITIVE())
                               : r2.toDouble() >= 0.3 ? QString(ui::colors::WARNING())
                                                      : QString(ui::colors::NEGATIVE());
        QList<QWidget*> fit = {
            gs_make_card(tr("R²"), fmt_num_safe(r2, 4), this, r2_col),
            gs_make_card(tr("ADJ R²"), fmt_num_safe(d.value("adj_r_squared"), 4), this),
            gs_make_card(tr("F STATISTIC"), fmt_num_safe(d.value("f_statistic"), 2), this),
            gs_make_card(tr("F p-VALUE"), fmt_num_safe(f_p, 4), this,
                         !f_p.isDouble()          ? ui::colors::TEXT_SECONDARY()
                         : f_p.toDouble() < 0.05 ? ui::colors::POSITIVE()
                                                 : ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(fit, this));

        QList<QWidget*> ic = {
            gs_make_card(tr("AIC"), fmt_num_safe(d.value("aic"), 2), this),
            gs_make_card(tr("BIC"), fmt_num_safe(d.value("bic"), 2), this),
            gs_make_card(tr("LOG-LIK"), fmt_num_safe(d.value("log_likelihood"), 2), this),
            gs_make_card(tr("RESID SE"), fmt_num_safe(d.value("residual_std_error"), 4), this),
        };
        results_layout_->addWidget(gs_card_row(ic, this));

        const QString dw_col = !dw.isDouble() ? QString(ui::colors::TEXT_SECONDARY())
                               : (dw.toDouble() >= 1.5 && dw.toDouble() <= 2.5) ? QString(ui::colors::POSITIVE())
                                                                                : QString(ui::colors::WARNING());
        QList<QWidget*> diag = {
            gs_make_card(tr("DURBIN-WATSON"), fmt_num_safe(dw, 3), this, dw_col),
            gs_make_card(tr("AUTOCORR."), d.value("dw_interpretation").toString(QStringLiteral("—")).toUpper(), this,
                         dw_col),
            gs_make_card(tr("BREUSCH-PAGAN p"), fmt_num_safe(d.value("breusch_pagan_p"), 4), this),
            gs_make_card(tr("HOMOSCEDASTIC"),
                         !homo.isBool()   ? QStringLiteral("—")
                         : homo.toBool() ? tr("YES")
                                         : tr("NO"),
                         this,
                         !homo.isBool()   ? ui::colors::TEXT_SECONDARY()
                         : homo.toBool() ? ui::colors::POSITIVE()
                                         : ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(diag, this));

        // Coefficient table
        const QJsonArray coefs = d.value("coefficients").toArray();
        if (!coefs.isEmpty()) {
            auto* table = new QTableWidget(coefs.size(), 7, this);
            table->setHorizontalHeaderLabels({tr("Term"), tr("Estimate"), tr("Std. Error"), tr("t"), tr("p"),
                                              tr("95% CI Lower"), tr("95% CI Upper")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionMode(QAbstractItemView::SingleSelection);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(coefs.size() * row_px(26) + row_px(40), 320));
            for (int i = 0; i < coefs.size(); ++i) {
                const auto c = coefs[i].toObject();
                table->setItem(i, 0, new QTableWidgetItem(c.value("name").toString()));
                table->setItem(i, 1, sm_num(c.value("estimate"), 4, gs_pos_neg_color(c.value("estimate").toDouble())));
                table->setItem(i, 2, sm_num(c.value("std_error"), 4));
                table->setItem(i, 3, sm_num(c.value("t_stat"), 3));
                table->setItem(i, 4,
                               sm_num(c.value("p_value"), 4,
                                      c.value("significant_5pct").toBool() ? ui::colors::POSITIVE()
                                                                           : ui::colors::TEXT_TERTIARY()));
                table->setItem(i, 5, sm_num(c.value("ci_lower"), 4));
                table->setItem(i, 6, sm_num(c.value("ci_upper"), 4));
                table->setRowHeight(i, row_px(24));
            }
            finish_table(table);
            results_layout_->addWidget(table);
        }

        status_label_->setText(tr("R²=%1  Adj=%2  F=%3 (p=%4)")
                                   .arg(fmt_num_safe(r2, 4), fmt_num_safe(d.value("adj_r_squared"), 4),
                                        fmt_num_safe(d.value("f_statistic"), 2), sm_p(f_p)));
        return;
    }

    // ── 3. ARIMA ─────────────────────────────────────────────────────────────
    if (command == "arima") {
        const QJsonArray order = d.value("order").toArray();
        const int p = order.size() > 0 ? order[0].toInt() : 0;
        const int d_ord = order.size() > 1 ? order[1].toInt() : 0;
        const int q = order.size() > 2 ? order[2].toInt() : 0;
        const int horizon = d.value("horizon").toInt();
        // in_sample_r2 is null for a series whose differenced target is constant; the
        // Ljung-Box figures are null when the test could not run.
        const QJsonValue r2 = d.value("in_sample_r2");
        const QJsonValue lb_p = d.value("ljung_box_lag10_p");
        const QJsonValue whitenoise = d.value("ljung_box_residuals_white_noise");
        const QJsonValue last_actual = d.value("last_actual");
        const QJsonValue first_fc = d.value("first_forecast");
        const QJsonValue last_fc = d.value("last_forecast");
        const QString wn_col = !whitenoise.isBool()   ? QString(ui::colors::TEXT_SECONDARY())
                               : whitenoise.toBool() ? QString(ui::colors::POSITIVE())
                                                     : QString(ui::colors::WARNING());
        // Δ vs the last actual, NaN (→ "—", neutral colour) when either side is null.
        const auto delta = [&last_actual](const QJsonValue& v) {
            return v.isDouble() && last_actual.isDouble() ? v.toDouble() - last_actual.toDouble() : kQcNaN;
        };
        const auto fmt_delta = [](double v) {
            return std::isfinite(v) ? gs_fmt_num(v, 4) : QString::fromUtf8("—");
        };
        const auto delta_col = [](double v) {
            return std::isfinite(v) ? gs_pos_neg_color(v) : QString(ui::colors::TEXT_PRIMARY());
        };

        auto* hdr = new QLabel(tr("ARIMA(%1,%2,%3)  |  HORIZON %4 STEPS  |  %5 OBS")
                                   .arg(p)
                                   .arg(d_ord)
                                   .arg(q)
                                   .arg(horizon)
                                   .arg(d.value("n_observations").toInt()));
        hdr->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700;"
                                   "padding:8px 10px; background:%2; border:1px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
        results_layout_->addWidget(hdr);

        QList<QWidget*> fit = {
            gs_make_card(tr("AIC"), fmt_num_safe(d.value("aic"), 2), this),
            gs_make_card(tr("BIC"), fmt_num_safe(d.value("bic"), 2), this),
            gs_make_card(tr("HQIC"), fmt_num_safe(d.value("hqic"), 2), this),
            gs_make_card(tr("IN-SAMPLE R²"), fmt_num_safe(r2, 4), this,
                         !r2.isDouble()          ? ui::colors::TEXT_SECONDARY()
                         : r2.toDouble() >= 0.7 ? ui::colors::POSITIVE()
                         : r2.toDouble() >= 0.3 ? ui::colors::WARNING()
                                                : ui::colors::NEGATIVE()),
        };
        results_layout_->addWidget(gs_card_row(fit, this));

        QList<QWidget*> resid = {
            gs_make_card(tr("LB(10) p-VALUE"), fmt_num_safe(lb_p, 4), this, wn_col),
            gs_make_card(tr("RESID WHITE NOISE"),
                         !whitenoise.isBool()   ? QStringLiteral("—")
                         : whitenoise.toBool() ? tr("YES")
                                               : tr("NO"),
                         this, wn_col),
            gs_make_card(tr("RESID MEAN"), fmt_num_safe(d.value("residual_mean"), 4), this),
            gs_make_card(tr("RESID σ"), fmt_num_safe(d.value("residual_std"), 4), this),
        };
        results_layout_->addWidget(gs_card_row(resid, this));

        const double first_drift = delta(first_fc);
        const double horizon_drift = delta(last_fc);
        QList<QWidget*> proj = {
            gs_make_card(tr("LAST ACTUAL"), fmt_num_safe(last_actual, 4), this),
            gs_make_card(tr("FIRST FORECAST"), fmt_num_safe(first_fc, 4), this, delta_col(first_drift)),
            gs_make_card(tr("LAST FORECAST"), fmt_num_safe(last_fc, 4), this, delta_col(horizon_drift)),
            gs_make_card(tr("HORIZON Δ"), fmt_delta(horizon_drift), this, delta_col(horizon_drift)),
        };
        results_layout_->addWidget(gs_card_row(proj, this));

        // Forecast fan (95% interval opening from the last actual value; the script sends no
        // history for ARIMA) above the table with every step.
        const QJsonArray forecast = d.value("forecast").toArray();
        if (!forecast.isEmpty()) {
            QcForecast f;
            f.title = tr("ARIMA FORECAST  ·  95% INTERVAL  ·  FROM THE LAST ACTUAL");
            f.fc_x = qc_column(forecast, QStringLiteral("step"));
            f.point = qc_column(forecast, QStringLiteral("point"));
            QcBand band;
            band.lower_pct = 2.5;
            band.upper_pct = 97.5;
            band.lower = qc_column(forecast, QStringLiteral("lower_95"));
            band.upper = qc_column(forecast, QStringLiteral("upper_95"));
            f.bands.append(band);
            f.anchor_x = 0.0;
            f.anchor_y = last_actual.toDouble(kQcNaN);
            results_layout_->addWidget(qc_forecast_chart(f, this));

            auto* table = new QTableWidget(forecast.size(), 4, this);
            table->setHorizontalHeaderLabels({tr("Step"), tr("Lower 95%"), tr("Point"), tr("Upper 95%")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionMode(QAbstractItemView::NoSelection);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(forecast.size() * row_px(22) + row_px(32), row_px(220)));
            for (int i = 0; i < forecast.size(); ++i) {
                const auto o = forecast[i].toObject();
                table->setItem(i, 0, new QTableWidgetItem(QString::number(o.value("step").toInt())));
                table->setItem(i, 1, sm_num(o.value("lower_95"), 4, ui::colors::NEGATIVE()));
                table->setItem(i, 2, sm_num(o.value("point"), 4));
                table->setItem(i, 3, sm_num(o.value("upper_95"), 4, ui::colors::POSITIVE()));
                table->setRowHeight(i, row_px(22));
            }
            results_layout_->addWidget(table);
        }

        status_label_->setText(tr("ARIMA(%1,%2,%3) — AIC=%4  R²=%5  LB(10) p=%6")
                                   .arg(p)
                                   .arg(d_ord)
                                   .arg(q)
                                   .arg(fmt_num_safe(d.value("aic"), 1), fmt_num_safe(r2, 3), fmt_num_safe(lb_p, 3)));
        return;
    }

    // ── 4. STATIONARITY TESTS ────────────────────────────────────────────────
    if (command == "stationarity_tests") {
        const QString verdict = d.value("verdict").toString();
        const int d_ord = d.value("differencing_order").toInt();

        auto* hdr = new QLabel(tr("VERDICT:  %1   |   DIFFERENCING d = %2").arg(verdict).arg(d_ord));
        const bool stationary = verdict.contains("STATIONARY") && !verdict.contains("NON-");
        hdr->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700;"
                                   "padding:8px 10px; background:%2; border-left:3px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(),
                                    stationary ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
        results_layout_->addWidget(hdr);

        const QJsonObject adf = d.value("adf").toObject();
        const QJsonObject kp = d.value("kpss").toObject();

        // ADF row
        auto* adf_lbl = new QLabel(tr("ADF (H₀: unit root → non-stationary)"));
        adf_lbl->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; padding:4px 0 0 2px;")
                                   .arg(ui::colors::TEXT_TERTIARY()));
        results_layout_->addWidget(adf_lbl);
        QList<QWidget*> adf_row = {
            gs_make_card(tr("STATISTIC"), fmt_num_safe(adf.value("statistic"), 4), this),
            gs_make_card(tr("p-VALUE"), fmt_num_safe(adf.value("p_value"), 4), this,
                         adf.value("stationary_5pct").toBool() ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card(tr("VERDICT @ 5%"),
                         adf.value("stationary_5pct").toBool() ? tr("STATIONARY") : tr("NON-STATIONARY"), this,
                         adf.value("stationary_5pct").toBool() ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
            gs_make_card(tr("LAGS USED"), QString::number(adf.value("lags_used").toInt()), this),
        };
        results_layout_->addWidget(gs_card_row(adf_row, this));
        QList<QWidget*> adf_crit = {
            gs_make_card(tr("ADF CRIT 1%"), fmt_num_safe(adf.value("critical_1pct"), 4), this),
            gs_make_card(tr("ADF CRIT 5%"), fmt_num_safe(adf.value("critical_5pct"), 4), this),
            gs_make_card(tr("ADF CRIT 10%"), fmt_num_safe(adf.value("critical_10pct"), 4), this),
            gs_make_card(tr("REGRESSION"), adf.value("regression").toString().toUpper(), this),
        };
        results_layout_->addWidget(gs_card_row(adf_crit, this));

        // KPSS row
        auto* kpss_lbl = new QLabel(tr("KPSS (H₀: stationary)"));
        kpss_lbl->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; padding:4px 0 0 2px;")
                                    .arg(ui::colors::TEXT_TERTIARY()));
        results_layout_->addWidget(kpss_lbl);
        QList<QWidget*> kp_row = {
            gs_make_card(tr("STATISTIC"), fmt_num_safe(kp.value("statistic"), 4), this),
            gs_make_card(tr("p-VALUE"), fmt_num_safe(kp.value("p_value"), 4), this,
                         kp.value("stationary_5pct").toBool() ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card(tr("VERDICT @ 5%"),
                         kp.value("stationary_5pct").toBool() ? tr("STATIONARY") : tr("NON-STATIONARY"), this,
                         kp.value("stationary_5pct").toBool() ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
            gs_make_card(tr("LAGS USED"), QString::number(kp.value("lags_used").toInt()), this),
        };
        results_layout_->addWidget(gs_card_row(kp_row, this));
        QList<QWidget*> kp_crit = {
            gs_make_card(tr("KPSS CRIT 1%"), fmt_num_safe(kp.value("critical_1pct"), 4), this),
            gs_make_card(tr("KPSS CRIT 5%"), fmt_num_safe(kp.value("critical_5pct"), 4), this),
            gs_make_card(tr("KPSS CRIT 10%"), fmt_num_safe(kp.value("critical_10pct"), 4), this),
            gs_make_card(tr("REGRESSION"), kp.value("regression").toString().toUpper(), this),
        };
        results_layout_->addWidget(gs_card_row(kp_crit, this));

        status_label_->setText(verdict);
        return;
    }

    // ── 5. ACF / PACF ───────────────────────────────────────────────────────
    if (command == "acf_pacf") {
        const int p_sug = d.value("suggested_arima_p").toInt();
        const int q_sug = d.value("suggested_arima_q").toInt();
        const QJsonValue band = d.value("significance_band_95pct");

        auto* hdr = new QLabel(tr("LAGS %1  |  SIGNIFICANCE BAND ±%2  |  SUGGESTED ARIMA(p=%3, q=%4)")
                                   .arg(d.value("nlags").toInt())
                                   .arg(fmt_num_safe(band, 4))
                                   .arg(p_sug)
                                   .arg(q_sug));
        hdr->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700;"
                                   "padding:8px 10px; background:%2; border-left:3px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), accent));
        results_layout_->addWidget(hdr);

        QList<QWidget*> sum = {
            gs_make_card(tr("SIG. ACF LAGS"), QString::number(d.value("n_significant_acf_lags").toInt()), this),
            gs_make_card(tr("SIG. PACF LAGS"), QString::number(d.value("n_significant_pacf_lags").toInt()), this),
            gs_make_card(tr("SUGGESTED p"), QString::number(p_sug), this, ui::colors::INFO()),
            gs_make_card(tr("SUGGESTED q"), QString::number(q_sug), this, ui::colors::INFO()),
        };
        results_layout_->addWidget(gs_card_row(sum, this));

        // Correlograms: ACF and PACF by lag (lag 0 is 1 by definition and left out so it
        // does not flatten the scale), signed bars with the ±95% band in the title; the
        // lag table below ticks the significant lags.
        const QJsonArray lags = d.value("lags").toArray();
        if (lags.size() > 1) {
            QStringList cats;
            QVector<double> acf_v;
            QVector<double> pacf_v;
            for (qsizetype i = 0; i < lags.size(); ++i) {
                const auto o = lags[i].toObject();
                if (o.value("lag").toInt() == 0)
                    continue;
                cats << QString::number(o.value("lag").toInt());
                acf_v << o.value("acf").toDouble(kQcNaN); // null → no bar
                pacf_v << o.value("pacf").toDouble(kQcNaN);
            }
            auto make_corr = [&](const QString& title, const QVector<double>& vals) {
                auto* box = new QWidget(this);
                auto* bl = new QVBoxLayout(box);
                bl->setContentsMargins(0, 0, 0, 0);
                bl->setSpacing(2);
                bl->addWidget(qc_caption(title));
                auto* chart = new ui::BarChart(box);
                chart->set_data(cats, vals, title);
                chart->set_signed_colors(true);
                chart->set_value_formatter(qc_fmt_num(3));
                chart->setFixedHeight(row_px(200));
                bl->addWidget(chart);
                return box;
            };
            auto* pair = new QWidget(this);
            auto* pl = new QHBoxLayout(pair);
            pl->setContentsMargins(0, 0, 0, 0);
            pl->setSpacing(8);
            pl->addWidget(make_corr(tr("ACF BY LAG  ·  SIGNIFICANT IF |ACF| > %1").arg(fmt_num_safe(band, 3)), acf_v), 1);
            pl->addWidget(make_corr(tr("PACF BY LAG  ·  SIGNIFICANT IF |PACF| > %1").arg(fmt_num_safe(band, 3)), pacf_v),
                          1);
            results_layout_->addWidget(pair);
        }
        if (!lags.isEmpty()) {
            auto* table = new QTableWidget(lags.size(), 5, this);
            table->setHorizontalHeaderLabels({tr("Lag"), tr("ACF"), tr("ACF Sig."), tr("PACF"), tr("PACF Sig.")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionMode(QAbstractItemView::NoSelection);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(lags.size() * row_px(22) + row_px(32), 360));
            for (int i = 0; i < lags.size(); ++i) {
                const auto o = lags[i].toObject();
                table->setItem(i, 0, new QTableWidgetItem(QString::number(o.value("lag").toInt())));
                table->setItem(i, 1,
                               sm_num(o.value("acf"), 4,
                                      o.value("acf_significant").toBool() ? ui::colors::POSITIVE()
                                                                          : ui::colors::TEXT_PRIMARY()));
                table->setItem(i, 2, new QTableWidgetItem(o.value("acf_significant").toBool() ? "✓" : "—"));
                table->setItem(i, 3,
                               sm_num(o.value("pacf"), 4,
                                      o.value("pacf_significant").toBool() ? ui::colors::POSITIVE()
                                                                           : ui::colors::TEXT_PRIMARY()));
                table->setItem(i, 4, new QTableWidgetItem(o.value("pacf_significant").toBool() ? "✓" : "—"));
                table->setRowHeight(i, row_px(22));
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(tr("Suggested ARIMA(p=%1, q=%2)").arg(p_sug).arg(q_sug));
        return;
    }

    // ── 6. GRANGER CAUSALITY ────────────────────────────────────────────────
    if (command == "granger_causality") {
        const bool any_causal = d.value("x_causes_y_at_any_lag").toBool();
        const QJsonArray causal_lags = d.value("causal_lags").toArray();

        auto* hdr = new QLabel(tr("MAX LAG TESTED: %1   |   %2")
                                   .arg(d.value("max_lag").toInt())
                                   .arg(d.value("interpretation").toString()));
        hdr->setWordWrap(true);
        hdr->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700;"
                                   "padding:8px 10px; background:%2; border-left:3px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(),
                                    any_causal ? ui::colors::POSITIVE() : ui::colors::TEXT_TERTIARY()));
        results_layout_->addWidget(hdr);

        QStringList causal_strs;
        for (const auto& v : causal_lags)
            causal_strs << QString::number(v.toInt());

        QList<QWidget*> sum = {
            gs_make_card(tr("ANY CAUSAL LAG"), any_causal ? tr("YES") : tr("NO"), this,
                         any_causal ? ui::colors::POSITIVE() : ui::colors::TEXT_PRIMARY()),
            gs_make_card(tr("CAUSAL LAGS"), causal_strs.isEmpty() ? QStringLiteral("—") : causal_strs.join(", "), this,
                         any_causal ? ui::colors::POSITIVE() : ui::colors::TEXT_SECONDARY()),
            gs_make_card(tr("LAGS TESTED"), QString::number(d.value("max_lag").toInt()), this),
            gs_make_card(tr("OBS"), QString::number(d.value("n_observations").toInt()), this),
        };
        results_layout_->addWidget(gs_card_row(sum, this));

        const QJsonArray tests = d.value("tests").toArray();
        if (!tests.isEmpty()) {
            auto* table = new QTableWidget(tests.size(), 6, this);
            table->setHorizontalHeaderLabels(
                {tr("Lag"), tr("F-stat"), tr("F p-value"), tr("χ² stat"), tr("χ² p-value"), tr("x ⇒ y @ 5%")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionMode(QAbstractItemView::NoSelection);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(tests.size() * row_px(26) + row_px(32), 280));
            for (int i = 0; i < tests.size(); ++i) {
                const auto t = tests[i].toObject();
                const bool causal = t.value("x_causes_y_5pct").toBool();
                table->setItem(i, 0, new QTableWidgetItem(QString::number(t.value("lag").toInt())));
                table->setItem(i, 1, sm_num(t.value("f_statistic"), 3));
                auto* fp = new QTableWidgetItem(sm_p(t.value("f_p_value")));
                fp->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                fp->setForeground(QColor(causal ? ui::colors::POSITIVE() : ui::colors::TEXT_TERTIARY()));
                table->setItem(i, 2, fp);
                table->setItem(i, 3, sm_num(t.value("chi2_statistic"), 3));
                auto* cp = new QTableWidgetItem(sm_p(t.value("chi2_p_value")));
                cp->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                cp->setForeground(QColor(causal ? ui::colors::POSITIVE() : ui::colors::TEXT_TERTIARY()));
                table->setItem(i, 4, cp);
                auto* yn = new QTableWidgetItem(causal ? tr("YES") : tr("NO"));
                yn->setForeground(QColor(causal ? ui::colors::POSITIVE() : ui::colors::TEXT_TERTIARY()));
                yn->setTextAlignment(Qt::AlignCenter);
                table->setItem(i, 5, yn);
                table->setRowHeight(i, row_px(24));
            }
            results_layout_->addWidget(table);
        }

        status_label_->setText(d.value("interpretation").toString());
        return;
    }

    // ── 7. DESCRIPTIVE + NORMALITY ───────────────────────────────────────────
    if (command == "descriptive") {
        QList<QWidget*> central = {
            gs_make_card(tr("MEAN"), fmt_num_safe(d.value("mean"), 4), this),
            gs_make_card(tr("MEDIAN"), fmt_num_safe(d.value("median"), 4), this),
            gs_make_card(tr("STD DEV"), fmt_num_safe(d.value("std"), 4), this),
            gs_make_card(tr("VARIANCE"), fmt_num_safe(d.value("variance"), 4), this),
        };
        results_layout_->addWidget(gs_card_row(central, this));

        QList<QWidget*> spread = {
            gs_make_card(tr("MIN"), fmt_num_safe(d.value("min"), 4), this, ui::colors::NEGATIVE()),
            gs_make_card(tr("MAX"), fmt_num_safe(d.value("max"), 4), this, ui::colors::POSITIVE()),
            gs_make_card(tr("RANGE"), fmt_num_safe(d.value("range"), 4), this),
            gs_make_card(tr("IQR"), fmt_num_safe(d.value("iqr"), 4), this),
        };
        results_layout_->addWidget(gs_card_row(spread, this));

        QList<QWidget*> qrt = {
            gs_make_card(tr("p25"), fmt_num_safe(d.value("p25"), 4), this),
            gs_make_card(tr("p50"), fmt_num_safe(d.value("p50"), 4), this),
            gs_make_card(tr("p75"), fmt_num_safe(d.value("p75"), 4), this),
            gs_make_card(tr("CV"), fmt_num_safe(d.value("coefficient_of_variation"), 4), this),
        };
        results_layout_->addWidget(gs_card_row(qrt, this));

        // Skewness / kurtosis are null for a constant series.
        QList<QWidget*> shape = {
            gs_make_card(tr("SKEWNESS"), fmt_num_safe(d.value("skewness"), 3), this,
                         gs_pos_neg_color(d.value("skewness").toDouble())),
            gs_make_card(tr("KURTOSIS (excess)"), fmt_num_safe(d.value("kurtosis_excess"), 3), this),
            gs_make_card(tr("SUM"), fmt_num_safe(d.value("sum"), 2), this),
            gs_make_card(tr("OBS"), QString::number(d.value("n_observations").toInt()), this),
        };
        results_layout_->addWidget(gs_card_row(shape, this));

        // Normality
        auto* nh = new QLabel(tr("NORMALITY  (H₀: data is normally distributed; p > 0.05 ⇒ cannot reject normal)"));
        nh->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; padding:4px 0 0 2px;")
                              .arg(ui::colors::TEXT_TERTIARY()));
        results_layout_->addWidget(nh);

        const QJsonValue jb_normal = d.value("jarque_bera_normal_5pct");
        const QJsonValue sw_normal = d.value("shapiro_wilk_normal_5pct");
        // Both tests are null for a constant series (and Shapiro-Wilk above 5000 points):
        // then there is no joint verdict, not a "NO".
        const bool both_known = jb_normal.isBool() && sw_normal.isBool();
        const bool both_normal = both_known && jb_normal.toBool() && sw_normal.toBool();
        QList<QWidget*> norm = {
            gs_make_card(tr("JARQUE-BERA STAT"), fmt_num_safe(d.value("jarque_bera_stat"), 3), this),
            gs_make_card(tr("JARQUE-BERA p"), fmt_num_safe(d.value("jarque_bera_p"), 4), this,
                         jb_normal.isBool() ? (jb_normal.toBool() ? ui::colors::POSITIVE() : ui::colors::WARNING())
                                            : ui::colors::TEXT_PRIMARY()),
            gs_make_card(tr("SHAPIRO-WILK p"), fmt_num_safe(d.value("shapiro_wilk_p"), 4), this,
                         sw_normal.isBool() ? (sw_normal.toBool() ? ui::colors::POSITIVE() : ui::colors::WARNING())
                                            : ui::colors::TEXT_PRIMARY()),
            gs_make_card(tr("NORMAL (BOTH)"),
                         !both_known   ? QStringLiteral("—")
                         : both_normal ? tr("YES")
                                       : tr("NO"),
                         this,
                         !both_known   ? ui::colors::TEXT_SECONDARY()
                         : both_normal ? ui::colors::POSITIVE()
                                       : ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(norm, this));

        status_label_->setText(tr("μ=%1  σ=%2  n=%3  JB p=%4")
                                   .arg(fmt_num_safe(d.value("mean"), 3), fmt_num_safe(d.value("std"), 3))
                                   .arg(d.value("n_observations").toInt())
                                   .arg(fmt_num_safe(d.value("jarque_bera_p"), 3)));
        return;
    }

    // ── 8. COINTEGRATION (Engle-Granger) ─────────────────────────────────────
    if (command == "cointegration") {
        const QJsonValue p = d.value("p_value");
        const QJsonValue z = d.value("spread_zscore_last");
        const QJsonValue hl = d.value("half_life_periods");
        const bool coint = d.value("cointegrated_5pct").toBool();
        // A test outcome (state), not a price direction.
        const QString verdict = coint ? tr("COINTEGRATED") : tr("NOT COINTEGRATED");
        const QString verdict_col = coint ? ui::colors::SUCCESS() : ui::colors::WARNING();

        results_layout_->addWidget(sm_strip(tr("%1 AT 5%  |  EG t = %2  vs 5% CRITICAL %3  |  TREND TERMS '%4'")
                                                .arg(verdict)
                                                .arg(fmt_num_safe(d.value("t_statistic"), 3),
                                                     fmt_num_safe(d.value("critical_5pct"), 3),
                                                     d.value("trend").toString()),
                                            verdict_col));

        QList<QWidget*> top = {
            gs_make_card(tr("VERDICT @ 5%"), verdict, this, verdict_col),
            gs_make_card(tr("EG p-VALUE"), fmt_num_safe(p, 4), this,
                         p.isDouble() ? verdict_col : QString(ui::colors::TEXT_SECONDARY())),
            gs_make_card(tr("HEDGE RATIO β"), fmt_num_safe(d.value("hedge_ratio"), 4), this, ui::colors::INFO()),
            gs_make_card(tr("SPREAD Z (LAST)"), fmt_num_safe(z, 2), this,
                         z.isDouble() && std::abs(z.toDouble()) >= 2.0 ? ui::colors::WARNING()
                                                                         : ui::colors::TEXT_PRIMARY()),
            // null when the spread does not mean-revert (no finite half-life).
            gs_make_card(tr("HALF-LIFE"),
                         hl.isDouble() ? tr("%1 periods").arg(hl.toDouble(), 0, 'f', 1) : QString::fromUtf8("—"), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> test = {
            gs_make_card(tr("EG t-STATISTIC"), fmt_num_safe(d.value("t_statistic"), 3), this),
            gs_make_card(tr("CRITICAL 1%"), fmt_num_safe(d.value("critical_1pct"), 3), this),
            gs_make_card(tr("CRITICAL 5%"), fmt_num_safe(d.value("critical_5pct"), 3), this),
            gs_make_card(tr("CRITICAL 10%"), fmt_num_safe(d.value("critical_10pct"), 3), this),
        };
        results_layout_->addWidget(gs_card_row(test, this));

        auto* adf_ref = gs_make_card(tr("SPREAD ADF p (REF.)"), fmt_num_safe(d.value("spread_adf_p"), 4), this,
                                     ui::colors::TEXT_SECONDARY());
        adf_ref->setToolTip(tr("Plain ADF p-value of the fitted spread, for reference only: it ignores that β was "
                               "estimated and is too optimistic. The verdict uses the Engle-Granger p-value."));
        QList<QWidget*> fitrow = {
            gs_make_card(tr("INTERCEPT α"), fmt_num_safe(d.value("intercept"), 4), this),
            gs_make_card(tr("REGRESSION R²"), fmt_num_safe(d.value("regression_r_squared"), 4), this),
            gs_make_card(tr("SPREAD σ"), fmt_num_safe(d.value("spread_std"), 4), this),
            adf_ref,
        };
        results_layout_->addWidget(gs_card_row(fitrow, this));

        // The spread with its mean and ±2σ as reference lines (drawn as series so the
        // y-range always takes them in).
        const QJsonArray spread = d.value("spread").toArray();
        if (spread.size() >= 2) {
            const int start = d.value("spread_start_index").toInt();
            QcChartSpec spec;
            spec.title = tr("SPREAD  ·  y − α − β·x  ·  MEAN AND ±2σ");
            spec.xs.reserve(spread.size());
            for (qsizetype i = 0; i < spread.size(); ++i)
                spec.xs.append(static_cast<double>(start + i));
            spec.fmt = qc_fmt_auto();
            spec.x_label = sm_obs_label();
            QcLine sl;
            sl.name = tr("Spread");
            sl.ys = qc_values(spread);
            sl.color = ui::ChartColorRole::Accent;
            sl.width = 1.5;
            spec.lines.append(sl);
            const QJsonValue mean = d.value("spread_mean");
            const QJsonValue sd = d.value("spread_std");
            const auto flat = [&spread](double v) { return QVector<double>(spread.size(), v); };
            if (mean.isDouble()) {
                QcLine ml;
                ml.name = tr("Mean");
                ml.ys = flat(mean.toDouble());
                ml.color = ui::ChartColorRole::Muted;
                ml.dashed = true;
                ml.width = 1.0;
                spec.lines.append(ml);
                if (sd.isDouble()) {
                    QcLine up = ml;
                    up.name = tr("+2σ");
                    up.ys = flat(mean.toDouble() + 2.0 * sd.toDouble());
                    up.color = ui::ChartColorRole::Warning;
                    QcLine dn = up;
                    dn.name = tr("−2σ");
                    dn.ys = flat(mean.toDouble() - 2.0 * sd.toDouble());
                    spec.lines.append(up);
                    spec.lines.append(dn);
                }
            }
            results_layout_->addWidget(qc_series_chart(spec, this));
            results_layout_->addWidget(
                qc_caption(tr("SPREAD = %1  ·  LAST %2  ·  Z = (LAST − MEAN) / σ")
                               .arg(d.value("spread_definition").toString(), fmt_num_safe(d.value("spread_last"), 4))));
        }

        status_label_->setText(tr("%1 (p=%2)  ·  β=%3  ·  z=%4  ·  half-life %5")
                                   .arg(verdict.toLower(), sm_p(p), fmt_num_safe(d.value("hedge_ratio"), 3),
                                        fmt_num_safe(z, 2), fmt_num_safe(hl, 1)));
        return;
    }

    // ── 9. ROLLING OLS (rolling beta) ────────────────────────────────────────
    if (command == "rolling_ols") {
        const int window = d.value("window").toInt();
        QList<QWidget*> last = {
            gs_make_card(tr("LAST β"), fmt_num_safe(d.value("last_beta"), 4), this, ui::colors::INFO()),
            gs_make_card(tr("LAST α (PER PERIOD)"), fmt_num_safe(d.value("last_alpha"), 6), this),
            gs_make_card(tr("LAST R²"), fmt_num_safe(d.value("last_r_squared"), 3), this),
            gs_make_card(tr("WINDOWS FITTED"),
                         tr("%1 of %2").arg(d.value("n_estimates").toInt()).arg(d.value("n_observations").toInt()),
                         this),
        };
        results_layout_->addWidget(gs_card_row(last, this));

        QList<QWidget*> range = {
            gs_make_card(tr("β MIN"), fmt_num_safe(d.value("beta_min"), 4), this),
            gs_make_card(tr("β MAX"), fmt_num_safe(d.value("beta_max"), 4), this),
            gs_make_card(tr("β MEAN"), fmt_num_safe(d.value("beta_mean"), 4), this),
            gs_make_card(tr("β σ"), fmt_num_safe(d.value("beta_std"), 4), this),
        };
        results_layout_->addWidget(gs_card_row(range, this));

        // β over time (warm-up rows are null → a gap at the start) with its mean, R² below.
        const QJsonArray points = d.value("points").toArray();
        if (points.size() >= 2) {
            const XAxis x = qc_x_axis(points, {QStringLiteral("date"), QStringLiteral("index")});
            QcChartSpec spec;
            spec.title = tr("ROLLING β  ·  %1-OBSERVATION WINDOW").arg(window);
            spec.x_time = x.time;
            spec.xs = x.xs;
            spec.fmt = qc_fmt_num(3);
            if (!x.time)
                spec.x_label = sm_obs_label();
            QcLine bl;
            bl.name = tr("β");
            bl.ys = qc_column(points, QStringLiteral("beta"));
            bl.color = ui::ChartColorRole::Accent;
            bl.width = 2.0;
            spec.lines.append(bl);
            const QJsonValue mean = d.value("beta_mean");
            if (mean.isDouble()) {
                QcLine ml;
                ml.name = tr("Mean β");
                ml.ys = QVector<double>(points.size(), mean.toDouble());
                ml.color = ui::ChartColorRole::Muted;
                ml.dashed = true;
                ml.width = 1.0;
                spec.lines.append(ml);
            }
            QcLine rl;
            rl.name = tr("R²");
            rl.ys = qc_column(points, QStringLiteral("r_squared"));
            rl.color = ui::ChartColorRole::Info;
            rl.style = ui::TimeSeriesChart::Style::Area;
            rl.has_baseline = true;
            rl.baseline = 0.0;
            rl.pane = 1;
            spec.lines.append(rl);
            spec.pane_titles = {tr("R² OF EACH WINDOW")};
            spec.pane_fmts = {qc_fmt_num(2)};
            auto* chart = qc_series_chart(spec, this);
            chart->set_y_range(1, ui::TimeSeriesChart::Axis::Left, 0.0, 1.0);
            results_layout_->addWidget(chart);

            const QVector<int> latest = qc_latest_rows(points.size());
            results_layout_->addWidget(qc_caption(qc_latest_caption(latest.size(), points.size())));
            auto* table = sm_table(static_cast<int>(latest.size()), {tr("Obs / Date"), tr("α"), tr("β"), tr("R²")},
                                   this);
            for (int r = 0; r < latest.size(); ++r) {
                const auto o = points[latest.at(r)].toObject();
                table->setItem(r, 0,
                               new QTableWidgetItem(o.contains("date") ? o.value("date").toString()
                                                                       : QString::number(o.value("index").toInt())));
                table->setItem(r, 1, sm_num(o.value("alpha"), 6));
                table->setItem(r, 2, sm_num(o.value("beta"), 4));
                table->setItem(r, 3, sm_num(o.value("r_squared"), 3));
                table->setRowHeight(r, row_px(22));
            }
            results_layout_->addWidget(table);
        }

        status_label_->setText(tr("Rolling β (window %1): last %2  ·  range %3 – %4")
                                   .arg(window)
                                   .arg(fmt_num_safe(d.value("last_beta"), 3), fmt_num_safe(d.value("beta_min"), 3),
                                        fmt_num_safe(d.value("beta_max"), 3)));
        return;
    }

    // ── 10. REGIME SWITCHING (Markov) ────────────────────────────────────────
    if (command == "regime_switching") {
        const int k = d.value("k_regimes").toInt();
        const int cur = d.value("current_regime").toInt();
        const QJsonValue cur_p = d.value("current_regime_probability");
        const bool converged = d.value("converged").toBool(true);
        const bool by_variance = d.value("switching_variance").toBool(true);
        const QJsonArray regimes = d.value("regimes").toArray();
        const auto regime_name = [&](int i) {
            if (i == 0)
                return by_variance ? tr("Regime 0 (calmest)") : tr("Regime 0 (lowest mean)");
            return tr("Regime %1").arg(i);
        };

        results_layout_->addWidget(sm_strip(tr("CURRENT REGIME %1 OF %2  ·  P = %3  ·  REGIMES NUMBERED BY %4")
                                                .arg(cur)
                                                .arg(k)
                                                .arg(fmt_pct_safe(cur_p, 1), d.value("regime_order").toString().toUpper()),
                                            ui::colors::legible(module_.color)));

        QList<QWidget*> top = {
            gs_make_card(tr("CURRENT REGIME"), regime_name(cur), this, ui::colors::INFO()),
            gs_make_card(tr("P(CURRENT)"), fmt_pct_safe(cur_p, 1), this),
            gs_make_card(tr("CONVERGED"), converged ? tr("YES") : tr("NO"), this,
                         converged ? ui::colors::SUCCESS() : ui::colors::WARNING()),
            gs_make_card(tr("SWITCHING VARIANCE"), by_variance ? tr("YES") : tr("NO"), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> ic = {
            gs_make_card(tr("LOG-LIKELIHOOD"), fmt_num_safe(d.value("log_likelihood"), 1), this),
            gs_make_card(tr("AIC"), fmt_num_safe(d.value("aic"), 1), this),
            gs_make_card(tr("BIC"), fmt_num_safe(d.value("bic"), 1), this),
            gs_make_card(tr("OBSERVATIONS"), QString::number(d.value("n_observations").toInt()), this),
        };
        results_layout_->addWidget(gs_card_row(ic, this));

        // Per-regime moments (per period of the input returns).
        if (!regimes.isEmpty()) {
            results_layout_->addWidget(qc_caption(tr("REGIMES  ·  MEAN AND σ PER PERIOD  ·  DURATION IN PERIODS")));
            auto* table = sm_table(static_cast<int>(regimes.size()),
                                   {tr("Regime"), tr("Mean"), tr("σ"), tr("Exp. Duration"), tr("Share of Time")}, this);
            for (int r = 0; r < regimes.size(); ++r) {
                const auto o = regimes[r].toObject();
                const int id = o.value("regime").toInt();
                table->setItem(r, 0,
                               new QTableWidgetItem(id == cur ? tr("%1  ◀ current").arg(regime_name(id)) : regime_name(id)));
                table->setItem(r, 1, sm_pct(o.value("mean"), 3, gs_pos_neg_color(o.value("mean").toDouble())));
                table->setItem(r, 2, sm_pct(o.value("std"), 3));
                table->setItem(r, 3, sm_num(o.value("expected_duration"), 1));
                table->setItem(r, 4, sm_pct(o.value("share_of_time"), 1));
                table->setRowHeight(r, row_px(24));
            }
            table->setItemDelegateForColumn(4, new ui::BarCellDelegate(ui::BarCellDelegate::Mode::Bar, table));
            finish_table(table);
            results_layout_->addWidget(table);
        }

        // Transition matrix: row = today's regime, column = tomorrow's; rows sum to 100%.
        const QJsonArray tm = d.value("transition_matrix").toArray();
        if (!tm.isEmpty()) {
            results_layout_->addWidget(qc_caption(tr("TRANSITION MATRIX  ·  P(TOMORROW = COLUMN | TODAY = ROW)")));
            QStringList headers = {tr("From \\ To")};
            for (int j = 0; j < tm.size(); ++j)
                headers << tr("→ %1").arg(j);
            auto* table = sm_table(static_cast<int>(tm.size()), headers, this);
            auto* heat = new ui::BarCellDelegate(ui::BarCellDelegate::Mode::Heat, table);
            heat->set_heat_sequential(true);
            heat->set_heat_clamp(100.0); // the cell text is a percentage
            for (int i = 0; i < tm.size(); ++i) {
                const QJsonArray row = tm[i].toArray();
                table->setItem(i, 0, new QTableWidgetItem(regime_name(i)));
                for (int j = 0; j < tm.size(); ++j)
                    table->setItem(i, j + 1, sm_pct(row.at(j), 1));
                table->setRowHeight(i, row_px(24));
            }
            for (int j = 0; j < tm.size(); ++j)
                table->setItemDelegateForColumn(j + 1, heat);
            results_layout_->addWidget(table);
        }

        // Smoothed probabilities: one pane per regime on a fixed 0..1 scale.
        const QJsonArray probs = d.value("smoothed_probabilities").toArray();
        if (probs.size() >= 2 && k >= 2) {
            QcChartSpec spec;
            spec.title = tr("SMOOTHED REGIME PROBABILITIES  ·  P(REGIME AT t | ALL DATA)");
            spec.xs = qc_x_positions(probs.size()).xs;
            spec.fmt = qc_fmt_num(2);
            spec.x_label = sm_obs_label();
            for (int i = 0; i < k; ++i) {
                QcLine l;
                l.name = regime_name(i);
                l.ys.reserve(probs.size());
                for (const QJsonValue& row : probs)
                    l.ys.append(row.toArray().at(i).toDouble(kQcNaN));
                l.style = ui::TimeSeriesChart::Style::Area;
                l.has_baseline = true;
                l.baseline = 0.0;
                l.color = ui::ChartColorRole::Palette;
                l.palette = i;
                l.pane = i;
                spec.lines.append(l);
                if (i > 0) {
                    spec.pane_titles << tr("P(REGIME %1)").arg(i);
                    spec.pane_fmts << qc_fmt_num(2);
                }
            }
            auto* chart = qc_series_chart(spec, this);
            chart->set_pane_title(0, tr("P(REGIME 0)"));
            for (int i = 0; i < k; ++i) {
                if (i > 0)
                    chart->set_pane_ratio(i, 1.0); // equal panes: one per regime
                chart->set_y_range(i, ui::TimeSeriesChart::Axis::Left, 0.0, 1.0);
            }
            chart->setFixedHeight(row_px(70 + 110 * k));
            results_layout_->addWidget(chart);
        }

        status_label_->setText(tr("Current regime %1 (p=%2)  ·  %3 regimes  ·  AIC %4")
                                   .arg(cur)
                                   .arg(fmt_pct_safe(cur_p, 1))
                                   .arg(k)
                                   .arg(fmt_num_safe(d.value("aic"), 1)));
        return;
    }

    // ── 11. PCA ──────────────────────────────────────────────────────────────
    if (command == "pca") {
        const QJsonArray evr = d.value("explained_variance_ratio").toArray();
        const QJsonArray cum = d.value("cumulative_explained_variance").toArray();
        const QJsonArray eig = d.value("eigenvalues").toArray();
        const QJsonArray all_eig = d.value("all_eigenvalues").toArray();
        const QJsonArray loadings = d.value("loadings").toArray();
        const int nc = d.value("n_components").toInt();
        const QString matrix = d.value("matrix").toString();

        results_layout_->addWidget(sm_strip(tr("%1 MATRIX  ·  %2 SERIES × %3 OBS  ·  INPUT %4  ·  %5 COMPONENTS")
                                                .arg(matrix.toUpper())
                                                .arg(d.value("n_assets").toInt())
                                                .arg(d.value("n_observations").toInt())
                                                .arg(d.value("input_type").toString().toUpper())
                                                .arg(nc),
                                            ui::colors::legible(module_.color)));

        QList<QWidget*> top = {
            gs_make_card(tr("PC1 EXPLAINS"), fmt_pct_safe(evr.at(0), 1), this, ui::colors::INFO()),
            gs_make_card(tr("FIRST %1 EXPLAIN").arg(nc), fmt_pct_safe(cum.isEmpty() ? QJsonValue() : cum.last(), 1),
                         this),
            gs_make_card(tr("PC1 EIGENVALUE"), fmt_num_safe(eig.at(0), 3), this),
            gs_make_card(tr("MATRIX"), matrix.toUpper(), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Every component's share of the variance (from all eigenvalues), with the running total.
        double total = 0.0;
        for (const QJsonValue& v : all_eig)
            total += v.toDouble();
        if (all_eig.size() >= 2 && total > 0.0) {
            QStringList cats;
            QVector<double> share;
            QVector<double> running;
            double acc = 0.0;
            for (int i = 0; i < all_eig.size(); ++i) {
                const double s = all_eig.at(i).toDouble() / total * 100.0;
                acc += s;
                cats << QStringLiteral("PC%1").arg(i + 1);
                share << s;
                running << acc;
            }
            results_layout_->addWidget(qc_caption(tr("EXPLAINED VARIANCE BY COMPONENT  ·  %")));
            auto* bars = new ui::BarChart(this);
            bars->set_categories(cats);
            bars->set_series({ui::BarSeries{tr("Explained"), share, QColor()},
                              ui::BarSeries{tr("Cumulative"), running, QColor()}});
            bars->set_show_values(true);
            bars->set_value_formatter(qc_fmt_pct(1));
            bars->setFixedHeight(row_px(220));
            results_layout_->addWidget(bars);
        }

        // Loadings: each column a unit eigenvector (|loading| <= 1), on the diverging scale.
        if (!loadings.isEmpty() && nc > 0) {
            results_layout_->addWidget(qc_caption(tr("LOADINGS  ·  SIGN-FIXED SO EACH COMPONENT SUMS ≥ 0")));
            QStringList headers = {tr("Series")};
            for (int j = 0; j < nc; ++j)
                headers << QStringLiteral("PC%1").arg(j + 1);
            auto* table = sm_table(static_cast<int>(loadings.size()), headers, this, 360);
            auto* heat = new ui::BarCellDelegate(ui::BarCellDelegate::Mode::Heat, table);
            heat->set_heat_clamp(1.0);
            for (int r = 0; r < loadings.size(); ++r) {
                const auto o = loadings[r].toObject();
                table->setItem(r, 0, new QTableWidgetItem(o.value("asset").toString()));
                for (int j = 0; j < nc; ++j)
                    table->setItem(r, j + 1, sm_num(o.value(QStringLiteral("pc%1").arg(j + 1)), 3));
                table->setRowHeight(r, row_px(24));
            }
            for (int j = 0; j < nc; ++j)
                table->setItemDelegateForColumn(j + 1, heat);
            finish_table(table);
            results_layout_->addWidget(table);
        }

        const QJsonArray scores = d.value("last_scores").toArray();
        if (!scores.isEmpty()) {
            QStringList parts;
            for (int j = 0; j < scores.size(); ++j)
                parts << QStringLiteral("PC%1 %2").arg(j + 1).arg(fmt_num_safe(scores.at(j), 3));
            results_layout_->addWidget(
                qc_caption(tr("SCORES OF THE LAST OBSERVATION  ·  %1").arg(parts.join(QStringLiteral("  ·  ")))));
        }

        status_label_->setText(tr("PC1 explains %1  ·  first %2 explain %3")
                                   .arg(fmt_pct_safe(evr.at(0), 1))
                                   .arg(nc)
                                   .arg(fmt_pct_safe(cum.isEmpty() ? QJsonValue() : cum.last(), 1)));
        return;
    }

    // ── Fallback ─────────────────────────────────────────────────────────────
    display_result(d);
}

} // namespace fincept::screens
