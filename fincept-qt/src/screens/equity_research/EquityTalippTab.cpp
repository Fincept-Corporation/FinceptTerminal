// src/screens/equity_research/EquityTalippTab.cpp
#include "screens/equity_research/EquityTalippTab.h"

#include "services/equity/EquityResearchService.h"
#include "ui/theme/Theme.h"

#include <QChart>
#include <QDateTimeAxis>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLineSeries>
#include <QPen>
#include <QVBoxLayout>
#include <QValueAxis>

namespace fincept::screens {

// ── Static category/indicator data (matches reference INDICATORS object) ──────
const QList<EquityTalippTab::CategoryDef>& EquityTalippTab::categories() {
    static const QList<CategoryDef> cats = {
        {"trend",
         "TREND",
         "#22d3ee",
         {
             {"sma", "SMA", "prices"},
             {"ema", "EMA", "prices"},
             {"wma", "WMA", "prices"},
             {"dema", "DEMA", "prices"},
             {"tema", "TEMA", "prices"},
             {"hma", "HMA", "prices"},
             {"kama", "KAMA", "prices"},
             {"alma", "ALMA", "prices"},
             {"t3", "T3", "prices"},
             {"zlema", "ZLEMA", "prices"},
         }},
        {"trend_advanced",
         "TREND+",
         "#60a5fa",
         {
             {"adx", "ADX", "ohlcv"},
             {"aroon", "Aroon", "ohlcv"},
             {"ichimoku", "Ichimoku", "ohlcv"},
             {"parabolic_sar", "Parabolic SAR", "ohlcv"},
             {"supertrend", "SuperTrend", "ohlcv"},
         }},
        {"momentum",
         "MOMENTUM",
         ui::colors::AMBER,
         {
             {"rsi", "RSI", "prices"},
             {"macd", "MACD", "prices"},
             {"stoch", "Stochastic", "ohlcv"},
             {"stoch_rsi", "StochRSI", "prices"},
             {"cci", "CCI", "ohlcv"},
             {"roc", "ROC", "prices"},
             {"tsi", "TSI", "prices"},
             {"williams", "Williams %R", "ohlcv"},
         }},
        {"volatility",
         "VOLATILITY",
         "#eab308",
         {
             {"atr", "ATR", "ohlcv"},
             {"bb", "Bollinger Bands", "prices"},
             {"keltner", "Keltner Channels", "ohlcv"},
             {"donchian", "Donchian Channels", "ohlcv"},
             {"chandelier_stop", "Chandelier Stop", "ohlcv"},
             {"natr", "NATR", "ohlcv"},
         }},
        {"volume",
         "VOLUME",
         ui::colors::POSITIVE,
         {
             {"obv", "OBV", "ohlcv"},
             {"vwap", "VWAP", "ohlcv"},
             {"vwma", "VWMA", "ohlcv"},
             {"mfi", "MFI", "ohlcv"},
             {"chaikin_osc", "Chaikin Osc", "ohlcv"},
             {"force_index", "Force Index", "ohlcv"},
         }},
        {"specialized",
         "SPECIAL",
         "#a855f7",
         {
             {"ao", "Awesome Osc", "ohlcv"},
             {"accu_dist", "Accum/Dist", "ohlcv"},
             {"bop", "Balance of Pwr", "ohlcv"},
             {"chop", "CHOP", "ohlcv"},
             {"coppock_curve", "Coppock Curve", "prices"},
             {"dpo", "DPO", "prices"},
             {"emv", "EMV", "ohlcv"},
             {"ibs", "IBS", "ohlcv"},
             {"kst", "KST", "prices"},
             {"kvo", "KVO", "ohlcv"},
             {"mass_index", "Mass Index", "ohlcv"},
             {"mcginley", "McGinley", "prices"},
             {"mean_dev", "Mean Dev", "prices"},
             {"smma", "SMMA", "prices"},
             {"sobv", "Smoothed OBV", "ohlcv"},
             {"stc", "STC", "prices"},
             {"std_dev", "Std Dev", "prices"},
             {"trix", "TRIX", "prices"},
             {"ttm", "TTM Squeeze", "ohlcv"},
             {"uo", "Ultimate Osc", "ohlcv"},
             {"vtx", "Vortex", "ohlcv"},
             {"zigzag", "ZigZag", "ohlcv"},
         }},
    };
    return cats;
}

const QMap<QString, QList<QPair<QString, double>>>& EquityTalippTab::param_defs() {
    static const QMap<QString, QList<QPair<QString, double>>> m = {
        {"sma", {{"period", 20}}},
        {"ema", {{"period", 20}}},
        {"wma", {{"period", 20}}},
        {"dema", {{"period", 20}}},
        {"tema", {{"period", 20}}},
        {"hma", {{"period", 20}}},
        {"kama", {{"period", 10}}},
        {"alma", {{"period", 20}, {"offset", 0.85}, {"sigma", 6.0}}},
        {"t3", {{"period", 5}, {"vfactor", 0.7}}},
        {"zlema", {{"period", 20}}},
        {"adx", {{"period", 14}}},
        {"aroon", {{"period", 14}}},
        {"ichimoku", {{"tenkan", 9}, {"kijun", 26}, {"senkou_b", 52}}},
        {"parabolic_sar", {{"acceleration", 0.02}, {"maximum", 0.2}}},
        {"supertrend", {{"period", 7}, {"multiplier", 3.0}}},
        {"rsi", {{"period", 14}}},
        {"macd", {{"fast", 12}, {"slow", 26}, {"signal", 9}}},
        {"stoch", {{"k_period", 14}, {"d_period", 3}}},
        {"stoch_rsi", {{"period", 14}}},
        {"cci", {{"period", 20}}},
        {"roc", {{"period", 10}}},
        {"tsi", {{"long_period", 25}, {"short_period", 13}}},
        {"williams", {{"period", 14}}},
        {"atr", {{"period", 14}}},
        {"bb", {{"period", 20}, {"stddev", 2.0}}},
        {"keltner", {{"period", 20}, {"multiplier", 2.0}}},
        {"donchian", {{"period", 20}}},
        {"chandelier_stop", {{"period", 22}, {"multiplier", 3.0}}},
        {"natr", {{"period", 14}}},
        {"obv", {}},
        {"vwap", {}},
        {"vwma", {{"period", 20}}},
        {"mfi", {{"period", 14}}},
        {"chaikin_osc", {{"fast", 3}, {"slow", 10}}},
        {"force_index", {{"period", 13}}},
        {"ao", {}},
        {"accu_dist", {}},
        {"bop", {}},
        {"chop", {{"period", 14}}},
        {"coppock_curve", {{"roc1", 14}, {"roc2", 11}, {"wma", 10}}},
        {"dpo", {{"period", 20}}},
        {"emv", {{"period", 14}}},
        {"ibs", {}},
        {"kst", {}},
        {"kvo", {{"fast", 34}, {"slow", 55}, {"signal", 13}}},
        {"mass_index", {{"fast", 9}, {"slow", 25}}},
        {"mcginley", {{"period", 14}}},
        {"mean_dev", {{"period", 14}}},
        {"smma", {{"period", 20}}},
        {"sobv", {{"period", 10}}},
        {"stc", {{"period", 12}, {"fast", 26}, {"slow", 50}}},
        {"std_dev", {{"period", 20}}},
        {"trix", {{"period", 15}}},
        {"ttm", {{"period", 20}}},
        {"uo", {{"fast", 7}, {"medium", 14}, {"slow", 28}}},
        {"vtx", {{"period", 14}}},
        {"zigzag", {{"deviation", 5.0}}},
    };
    return m;
}

QString EquityTalippTab::cat_color(const QString& cat_id) {
    for (const auto& c : categories())
        if (c.id == cat_id)
            return c.color;
    return ui::colors::AMBER;
}

// ── Constructor ───────────────────────────────────────────────────────────────
EquityTalippTab::EquityTalippTab(QWidget* parent) : QWidget(parent) {
    build_ui();
    auto& svc = services::equity::EquityResearchService::instance();
    connect(&svc, &services::equity::EquityResearchService::talipp_result, this, &EquityTalippTab::on_talipp_result);
}

void EquityTalippTab::set_symbol(const QString& symbol) {
    if (symbol == current_symbol_)
        return;
    current_symbol_ = symbol;
    data_points_lbl_->setText("—  data points  |  TALIpp Engine");
    status_label_->setText("Select an indicator and click CALCULATE.");
    empty_widget_->show();
    results_widget_->hide();
    loading_overlay_->hide_loading(); // reset overlay on symbol change
}

// ── build_ui ──────────────────────────────────────────────────────────────────
void EquityTalippTab::build_ui() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    loading_overlay_ = new ui::LoadingOverlay(this);
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(10, 10, 10, 10);
    vl->setSpacing(8);

    // ── Category tab buttons ──────────────────────────────────────────────────
    auto* cat_row = new QHBoxLayout;
    cat_row->setSpacing(2);
    cat_row->setContentsMargins(0, 0, 0, 0);

    for (const auto& cat : categories()) {
        auto* btn = new QPushButton(cat.label);
        btn->setStyleSheet(QString("QPushButton { background:transparent; color:%3; "
                                   "border:1px solid %1; padding:5px 14px; font-size:10px; font-weight:700; }"
                                   "QPushButton:hover { border-color:%2; color:%2; }")
                               .arg(ui::colors::BORDER_DIM(), cat.color, ui::colors::TEXT_TERTIARY()));
        btn->setProperty("cat_id", cat.id);
        connect(btn, &QPushButton::clicked, this, [this, id = cat.id]() { on_category_clicked(id); });
        cat_buttons_[cat.id] = btn;
        cat_row->addWidget(btn);
    }
    cat_row->addStretch();
    vl->addLayout(cat_row);

    // ── Indicator selector + params + run ─────────────────────────────────────
    auto* ctrl_frame = new QFrame;
    ctrl_frame->setStyleSheet(
        QString("QFrame { background:%1; border:1px solid %2; }").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* cl = new QHBoxLayout(ctrl_frame);
    cl->setContentsMargins(12, 10, 12, 10);
    cl->setSpacing(12);

    indicator_combo_ = new QComboBox;
    indicator_combo_->setMinimumWidth(200);
    indicator_combo_->setStyleSheet(
        QString(R"(
        QComboBox {
            background:%1; color:%2; border:1px solid %3;
            padding:5px 10px; font-size:12px;
        }
        QComboBox::drop-down { border:0; }
        QComboBox QAbstractItemView {
            background:%1; color:%2; border:1px solid %3;
            selection-background-color:%4;
        }
    )")
            .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), ui::colors::AMBER()));
    cl->addWidget(indicator_combo_);

    param_widget_ = new QWidget(this);
    param_widget_->setStyleSheet("background:transparent;");
    param_form_ = new QFormLayout(param_widget_);
    param_form_->setContentsMargins(0, 0, 0, 0);
    param_form_->setSpacing(6);
    param_form_->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    cl->addWidget(param_widget_);
    cl->addStretch();

    data_points_lbl_ = new QLabel("—  data points  |  TALIpp Engine");
    data_points_lbl_->setStyleSheet(
        QString("color:%1; font-size:10px; background:transparent; border:0;").arg(ui::colors::TEXT_TERTIARY()));
    cl->addWidget(data_points_lbl_);

    compute_btn_ = new QPushButton("▶  CALCULATE");
    compute_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:0; padding:6px 18px; "
                                        "font-size:10px; font-weight:700; }"
                                        "QPushButton:hover { background:%3; }"
                                        "QPushButton:disabled { background:%4; color:%5; }")
                                    .arg(ui::colors::AMBER(), ui::colors::BG_BASE(), ui::colors::AMBER_DIM(),
                                         ui::colors::TEXT_DIM(), ui::colors::TEXT_TERTIARY()));
    connect(compute_btn_, &QPushButton::clicked, this, &EquityTalippTab::on_compute_clicked);
    cl->addWidget(compute_btn_);
    vl->addWidget(ctrl_frame);

    // ── Empty state ───────────────────────────────────────────────────────────
    empty_widget_ = new QFrame;
    empty_widget_->setStyleSheet(
        QString("QFrame { background:%1; border:1px solid %2; }").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* ew_vl = new QVBoxLayout(empty_widget_);
    ew_vl->setAlignment(Qt::AlignCenter);
    ew_vl->setSpacing(8);

    auto* ew_icon = new QLabel("◈");
    ew_icon->setAlignment(Qt::AlignCenter);
    ew_icon->setStyleSheet(
        QString("color:%1; font-size:32px; background:transparent; border:0;").arg(ui::colors::TEXT_DIM()));
    status_label_ = new QLabel("Select an indicator and click CALCULATE");
    status_label_->setAlignment(Qt::AlignCenter);
    status_label_->setStyleSheet(
        QString("color:%1; font-size:13px; background:transparent; border:0;").arg(ui::colors::TEXT_TERTIARY()));
    auto* ew_sub = new QLabel("50+ indicators across 6 categories — powered by TALIpp incremental engine");
    ew_sub->setAlignment(Qt::AlignCenter);
    ew_sub->setStyleSheet(
        QString("color:%1; font-size:10px; background:transparent; border:0;").arg(ui::colors::TEXT_DIM()));
    ew_vl->addWidget(ew_icon);
    ew_vl->addWidget(status_label_);
    ew_vl->addWidget(ew_sub);
    vl->addWidget(empty_widget_, 1);

    // ── Results area (hidden until first compute) ─────────────────────────────
    results_widget_ = new QWidget(this);
    results_widget_->setStyleSheet("background:transparent;");
    auto* rw_vl = new QVBoxLayout(results_widget_);
    rw_vl->setContentsMargins(0, 0, 0, 0);
    rw_vl->setSpacing(8);

    last_values_area_ = new QWidget(this);
    last_values_area_->setStyleSheet("background:transparent;");
    new QHBoxLayout(last_values_area_);
    static_cast<QHBoxLayout*>(last_values_area_->layout())->setContentsMargins(0, 0, 0, 0);
    static_cast<QHBoxLayout*>(last_values_area_->layout())->setSpacing(8);
    rw_vl->addWidget(last_values_area_);

    chart_view_ = new QChartView;
    chart_view_->setRenderHint(QPainter::Antialiasing, false);
    chart_view_->setMinimumHeight(300);
    chart_view_->setStyleSheet(
        QString("background:%1; border:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    rw_vl->addWidget(chart_view_, 1);

    results_widget_->hide();
    vl->addWidget(results_widget_, 1);

    connect(indicator_combo_, &QComboBox::currentTextChanged, this, &EquityTalippTab::on_indicator_changed);

    on_category_clicked("trend");
}

// ── on_category_clicked ───────────────────────────────────────────────────────
void EquityTalippTab::on_category_clicked(const QString& cat_id) {
    active_category_ = cat_id;

    for (auto it = cat_buttons_.begin(); it != cat_buttons_.end(); ++it) {
        bool active = (it.key() == cat_id);
        QString color = cat_color(it.key());
        it.value()->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:1px solid %3; "
                                          "padding:5px 14px; font-size:10px; font-weight:700; }"
                                          "QPushButton:hover { border-color:%3; }")
                                      .arg(active ? color : "transparent",
                                           active ? ui::colors::BG_BASE() : ui::colors::TEXT_TERTIARY(),
                                           active ? color : ui::colors::BORDER_DIM()));
    }

    rebuild_indicator_combo(cat_id);
}

void EquityTalippTab::rebuild_indicator_combo(const QString& cat_id) {
    indicator_combo_->blockSignals(true);
    indicator_combo_->clear();
    for (const auto& cat : categories()) {
        if (cat.id != cat_id)
            continue;
        for (const auto& item : cat.items)
            indicator_combo_->addItem(item.label, item.id);
        break;
    }
    indicator_combo_->blockSignals(false);
    if (indicator_combo_->count() > 0)
        rebuild_param_form(indicator_combo_->currentData().toString());
}

// ── on_indicator_changed ──────────────────────────────────────────────────────
void EquityTalippTab::on_indicator_changed(const QString& /*label*/) {
    rebuild_param_form(indicator_combo_->currentData().toString());
    empty_widget_->show();
    results_widget_->hide();
}

void EquityTalippTab::rebuild_param_form(const QString& indicator_id) {
    while (param_form_->rowCount() > 0)
        param_form_->removeRow(0);

    const auto& defs = param_defs();
    if (!defs.contains(indicator_id))
        return;

    for (const auto& [name, default_val] : defs[indicator_id]) {
        auto* spin = new QDoubleSpinBox;
        spin->setObjectName(name);
        spin->setValue(default_val);
        spin->setRange(0.001, 9999);
        spin->setDecimals(name.contains("period") || name == "fast" || name == "slow" || name == "signal" ? 0 : 3);
        spin->setFixedWidth(72);
        spin->setStyleSheet(QString("QDoubleSpinBox { background:%1; color:%2; border:1px solid %3; "
                                    "padding:3px 6px; font-size:11px; }")
                                .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM()));
        auto* lbl = new QLabel(name + ":");
        lbl->setStyleSheet(
            QString("color:%1; font-size:10px; background:transparent; border:0;").arg(ui::colors::TEXT_SECONDARY()));
        param_form_->addRow(lbl, spin);
    }
}

QVariantMap EquityTalippTab::collect_params() const {
    QVariantMap params;
    for (auto* spin : param_widget_->findChildren<QDoubleSpinBox*>())
        params[spin->objectName()] = spin->value();
    return params;
}

// ── on_compute_clicked ────────────────────────────────────────────────────────
void EquityTalippTab::on_compute_clicked() {
    if (current_symbol_.isEmpty()) {
        status_label_->setText("No symbol loaded. Search for a symbol first.");
        return;
    }
    QString id = indicator_combo_->currentData().toString();
    QString label = indicator_combo_->currentText();
    if (id.isEmpty())
        return;

    compute_btn_->setEnabled(false);
    compute_btn_->setText("COMPUTING…");
    status_label_->setText("Computing " + label + "…");
    loading_overlay_->show_loading("COMPUTING " + label.toUpper() + "…");
    empty_widget_->show();
    results_widget_->hide();

    services::equity::EquityResearchService::instance().compute_talipp(current_symbol_, id, collect_params(), "2y");
}

// ── rebuild_last_values ───────────────────────────────────────────────────────
void EquityTalippTab::rebuild_last_values(const QString& indicator_id, const QVector<double>& values) {
    auto* hl = qobject_cast<QHBoxLayout*>(last_values_area_->layout());
    if (!hl)
        return;
    while (hl->count() > 0) {
        auto* item = hl->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    auto make_lv = [&](const QString& lbl, const QString& val, const QString& color) {
        auto* w = new QFrame;
        w->setStyleSheet(QString("QFrame { background:%1; border:1px solid %2; }")
                             .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
        auto* wl = new QVBoxLayout(w);
        wl->setContentsMargins(12, 8, 12, 8);
        wl->setSpacing(2);
        auto* l = new QLabel(lbl);
        l->setStyleSheet(QString("color:%1; font-size:9px; letter-spacing:0.5px; "
                                 "background:transparent; border:0;")
                             .arg(ui::colors::TEXT_TERTIARY()));
        auto* v = new QLabel(val);
        v->setStyleSheet(QString("color:%1; font-size:15px; font-weight:700; font-family:monospace; "
                                 "background:transparent; border:0;")
                             .arg(color));
        wl->addWidget(l);
        wl->addWidget(v);
        hl->addWidget(w);
    };

    if (!values.isEmpty()) {
        double last = values.last();
        make_lv("LAST", !qIsNaN(last) ? QString::number(last, 'f', 4) : "N/A", ui::colors::AMBER);
    }

    for (auto* spin : param_widget_->findChildren<QDoubleSpinBox*>())
        make_lv(spin->objectName().toUpper(), QString::number(spin->value(), 'f', 0), ui::colors::TEXT_TERTIARY());

    hl->addStretch();
}

// ── rebuild_chart ─────────────────────────────────────────────────────────────
void EquityTalippTab::rebuild_chart(const QString& /*indicator_id*/, const QVector<double>& values,
                                    const QVector<qint64>& timestamps) {
    QString color = cat_color(active_category_);
    QString label = indicator_combo_->currentText();

    auto* series = new QLineSeries;
    series->setName(label);
    {
        QPen pen(QColor(color), 2, Qt::SolidLine);
        series->setPen(pen);
    }

    for (int i = 0; i < values.size() && i < timestamps.size(); ++i) {
        if (!qIsNaN(values[i]) && !qIsInf(values[i]))
            series->append(static_cast<qreal>(timestamps[i]) * 1000.0, values[i]);
    }

    auto* chart = new QChart;
    chart->addSeries(series);
    chart->setBackgroundBrush(QBrush(QColor(ui::colors::BG_SURFACE())));
    chart->setPlotAreaBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE())));
    chart->setPlotAreaBackgroundVisible(true);
    chart->legend()->hide();
    chart->setMargins(QMargins(4, 4, 4, 4));
    chart->setTitle(label.toUpper() + "  —  " + QString(active_category_).toUpper());
    QFont title_font("", 10, QFont::Bold);
    chart->setTitleFont(title_font);
    chart->setTitleBrush(QBrush(QColor(color)));

    auto* axisX = new QDateTimeAxis;
    axisX->setFormat("MMM yy");
    axisX->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    axisX->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    axisX->setLabelsFont(QFont("", 8));
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    auto* axisY = new QValueAxis;
    axisY->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    axisY->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    axisY->setLabelsFont(QFont("", 8));
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    chart_view_->setChart(chart);
}

// ── on_talipp_result ──────────────────────────────────────────────────────────
void EquityTalippTab::on_talipp_result(QString indicator, QVector<double> values, QVector<qint64> timestamps) {
    compute_btn_->setEnabled(true);
    compute_btn_->setText("▶  CALCULATE");
    loading_overlay_->hide_loading();

    if (values.isEmpty()) {
        status_label_->setText("No data returned for " + indicator);
        empty_widget_->show();
        results_widget_->hide();
        return;
    }

    rebuild_last_values(indicator, values);
    rebuild_chart(indicator, values, timestamps);

    empty_widget_->hide();
    results_widget_->show();
}

} // namespace fincept::screens
