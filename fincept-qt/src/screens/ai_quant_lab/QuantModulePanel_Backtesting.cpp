// src/screens/ai_quant_lab/QuantModulePanel_Backtesting.cpp
//
// Backtesting panel — Qlib-style strategy backtester with rich result display
// (KPI cards, equity curve chart, execution cost strip, JSON export). Extracted
// from QuantModulePanel.cpp to keep that file maintainable.
#include "screens/ai_quant_lab/QuantModulePanel.h"
#include "screens/ai_quant_lab/QuantModulePanel_Common.h"
#include "screens/ai_quant_lab/QuantModulePanel_GsHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"

#include "core/logging/Logger.h"
#include "services/ai_quant_lab/AIQuantLabService.h"
#include "services/file_manager/FileManagerService.h"
#include "ui/theme/Theme.h"

#include <QAreaSeries>
#include <QBrush>
#include <QChart>
#include <QChartView>
#include <QColor>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDateTime>
#include <QDateTimeAxis>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLineSeries>
#include <QList>
#include <QMargins>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QString>
#include <QStringList>
#include <QValueAxis>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

using namespace fincept::services::quant;
using namespace fincept::screens::quant_styles;
using namespace fincept::screens::quant_common;
using namespace fincept::screens::quant_gs_helpers;

// ═══════════════════════════════════════════════════════════════════════════════
// BACKTESTING PANEL
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* QuantModulePanel::build_backtesting_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* strat = new QComboBox(w);
    strat->addItems({"topk_dropout", "weight_based", "enhanced_indexing"});
    strat->setStyleSheet(combo_ss());
    combo_inputs_["bt_strategy"] = strat;
    vl->addWidget(build_input_row("Strategy", strat, w));

    auto* instruments = new QLineEdit(w);
    instruments->setPlaceholderText("AAPL,MSFT,GOOG,AMZN");
    instruments->setStyleSheet(input_ss());
    text_inputs_["bt_instruments"] = instruments;
    vl->addWidget(build_input_row("Instruments", instruments, w));

    auto* start_date = new QDateEdit(QDate(2020, 1, 1), w);
    start_date->setDisplayFormat("yyyy-MM-dd");
    start_date->setCalendarPopup(true);
    start_date->setStyleSheet(input_ss());
    date_inputs_["bt_start"] = start_date;
    vl->addWidget(build_input_row("Start Date", start_date, w));

    auto* end_date = new QDateEdit(QDate(2024, 1, 1), w);
    end_date->setDisplayFormat("yyyy-MM-dd");
    end_date->setCalendarPopup(true);
    end_date->setStyleSheet(input_ss());
    date_inputs_["bt_end"] = end_date;
    vl->addWidget(build_input_row("End Date", end_date, w));

    auto* capital = make_double_spin(1000, 1e12, 1e6, 0, "", w);
    double_inputs_["bt_capital"] = capital;
    vl->addWidget(build_input_row("Initial Capital ($)", capital, w));

    auto* topk = new QSpinBox(w);
    topk->setRange(1, 100);
    topk->setValue(10);
    topk->setStyleSheet(spinbox_ss());
    int_inputs_["bt_topk"] = topk;
    vl->addWidget(build_input_row("Top K Positions", topk, w));

    auto* benchmark = new QLineEdit(w);
    benchmark->setPlaceholderText("SH000300 (CSI300)");
    benchmark->setStyleSheet(input_ss());
    text_inputs_["bt_benchmark"] = benchmark;
    vl->addWidget(build_input_row("Benchmark", benchmark, w));

    auto* run = make_run_button("RUN BACKTEST", w);
    connect(run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Backtesting...");
        QJsonObject params;
        QJsonObject strategy_config;
        strategy_config["type"] = combo_inputs_["bt_strategy"]->currentText();
        strategy_config["topk"] = int_inputs_["bt_topk"]->value();
        params["strategy_config"] = strategy_config;
        QJsonObject dataset;
        dataset["instruments"] = text_inputs_["bt_instruments"]->text();
        dataset["start_date"] = date_inputs_["bt_start"]->date().toString("yyyy-MM-dd");
        dataset["end_date"] = date_inputs_["bt_end"]->date().toString("yyyy-MM-dd");
        params["dataset_config"] = dataset;
        QJsonObject portfolio;
        portfolio["initial_capital"] = double_inputs_["bt_capital"]->value();
        portfolio["benchmark"] = text_inputs_["bt_benchmark"]->text();
        params["portfolio_config"] = portfolio;
        AIQuantLabService::instance().run_backtest(params);
    });
    vl->addWidget(run);

    auto* rc = new QWidget(w);
    results_layout_ = new QVBoxLayout(rc);
    results_layout_->setContentsMargins(0, 8, 0, 0);
    results_layout_->setSpacing(8);
    vl->addWidget(rc);
    vl->addStretch();

    return w;
}

// ── Backtest Result Display ───────────────────────────────────────────────────

void QuantModulePanel::display_backtest_result(const QJsonObject& payload) {
    clear_results();

    if (!payload["success"].toBool()) {
        display_error(payload["error"].toString("Unknown error"));
        return;
    }

    const QJsonObject metrics = payload["metrics"].toObject();
    const QJsonArray  curve   = payload["equity_curve"].toArray();
    const QJsonObject costs   = payload["execution_cost_estimate"].toObject();
    const QStringList tickers = [&]() {
        QStringList t;
        for (const auto& v : payload["tickers"].toArray())
            t << v.toString();
        return t;
    }();

    const QString strategy   = payload["strategy"].toString();
    const QString start_date = payload["start_date"].toString();
    const QString end_date   = payload["end_date"].toString();

    const QColor  accent      = module_.color;
    const QString accent_hex  = accent.name();
    const QString green_hex   = ui::colors::POSITIVE();
    const QString red_hex     = ui::colors::NEGATIVE();
    const QString text_p      = ui::colors::TEXT_PRIMARY();
    const QString text_s      = ui::colors::TEXT_SECONDARY();
    const QString text_t      = ui::colors::TEXT_TERTIARY();
    const QString bg_surface  = ui::colors::BG_SURFACE();
    const QString bg_raised   = ui::colors::BG_RAISED();
    const QString border_dim  = ui::colors::BORDER_DIM();
    const QString border_med  = ui::colors::BORDER_MED();
    const QString font_data   = ui::fonts::DATA_FAMILY;
    const int     fs_sm       = ui::fonts::SMALL;
    const int     fs_lg       = ui::fonts::HEADER;

    // ── 1. Header bar ──────────────────────────────────────────────────────
    auto* header_w = new QWidget;
    auto* header_h = new QHBoxLayout(header_w);
    header_h->setContentsMargins(0, 0, 0, 8);
    header_h->setSpacing(8);

    auto* title_lbl = new QLabel("BACKTEST RESULTS");
    title_lbl->setStyleSheet(
        QString("color:%1; font-size:%2px; font-family:%3; font-weight:800; letter-spacing:2px;")
            .arg(accent_hex).arg(fs_lg).arg(font_data));
    header_h->addWidget(title_lbl);

    header_h->addSpacing(16);

    auto make_chip = [&](const QString& text, const QString& fg, const QString& bg_hex) -> QLabel* {
        auto* c = new QLabel(text);
        c->setStyleSheet(
            QString("color:%1; background:%2; border-radius:3px; padding:2px 8px;"
                    "font-size:%3px; font-family:%4; font-weight:600;")
                .arg(fg, bg_hex).arg(fs_sm).arg(font_data));
        return c;
    };

    QString strat_display = strategy;
    strat_display.replace('_', ' ');
    strat_display = strat_display.toUpper();
    header_h->addWidget(make_chip(strat_display, accent_hex,
                                  QColor(accent).darker(300).name() + "55"));

    header_h->addWidget(make_chip(start_date + "  →  " + end_date,
                                  text_s, bg_raised));

    for (const auto& t : tickers)
        header_h->addWidget(make_chip(t, text_p, bg_raised));

    header_h->addStretch();
    results_layout_->addWidget(header_w);

    // ── 2. KPI cards row ──────────────────────────────────────────────────
    struct KpiCard {
        QString label;
        QString value;
        QString sub;
        bool    positive = true;
        bool    neutral  = false;
    };

    double total_ret  = metrics["total_return_pct"].toDouble();
    double ann_ret    = metrics["annualised_return"].toDouble();
    double ann_vol    = metrics["annualised_vol"].toDouble();
    double sharpe     = metrics["sharpe_ratio"].toDouble();
    double max_dd     = metrics["max_drawdown_pct"].toDouble();
    double calmar     = metrics["calmar_ratio"].toDouble();
    double win_rate   = metrics["win_rate_pct"].toDouble();
    double final_val  = metrics["final_value"].toDouble();
    double init_cap   = metrics["initial_capital"].toDouble();
    int    t_days     = metrics["trading_days"].toInt();

    auto fmt_pct = [](double v) { return QString("%1%2%").arg(v >= 0 ? "+" : "").arg(v, 0, 'f', 2); };
    auto fmt_usd = [](double v) -> QString {
        if (v >= 1e6) return QString("$%1M").arg(v / 1e6, 0, 'f', 2);
        return QString("$%1K").arg(v / 1e3, 0, 'f', 0);
    };

    QList<KpiCard> kpis = {
        {"TOTAL RETURN",   fmt_pct(total_ret),  fmt_usd(final_val) + " final",  total_ret >= 0, false},
        {"ANN. RETURN",    fmt_pct(ann_ret),     QString("Vol: %1%").arg(ann_vol, 0, 'f', 1), ann_ret >= 0, false},
        {"SHARPE RATIO",   QString::number(sharpe, 'f', 3),
                           sharpe >= 1 ? "Excellent" : sharpe >= 0.5 ? "Good" : "Weak", sharpe >= 0.5, false},
        {"MAX DRAWDOWN",   fmt_pct(max_dd),      QString("Calmar: %1").arg(calmar, 0, 'f', 3), false, false},
        {"WIN RATE",       QString("%1%").arg(win_rate, 0, 'f', 1), QString("%1 days").arg(t_days), win_rate >= 50, false},
        {"CAPITAL",        fmt_usd(init_cap),    "Initial capital",  true, true},
    };

    auto* cards_w  = new QWidget;
    auto* cards_gl = new QGridLayout(cards_w);
    cards_gl->setContentsMargins(0, 0, 0, 0);
    cards_gl->setSpacing(8);

    for (int i = 0; i < kpis.size(); ++i) {
        const auto& k = kpis[i];
        auto* card = new QWidget;
        card->setObjectName("btKpiCard");
        card->setStyleSheet(
            QString("#btKpiCard { background:%1; border:1px solid %2; border-radius:4px; }")
                .arg(bg_surface, border_dim));

        auto* cl = new QVBoxLayout(card);
        cl->setContentsMargins(12, 10, 12, 10);
        cl->setSpacing(2);

        auto* lbl = new QLabel(k.label);
        lbl->setStyleSheet(
            QString("color:%1; font-size:%2px; font-family:%3; font-weight:600; letter-spacing:1px;")
                .arg(text_t).arg(fs_sm - 1).arg(font_data));

        QString val_color = k.neutral ? text_s : (k.positive ? green_hex : red_hex);
        auto* val = new QLabel(k.value);
        val->setStyleSheet(
            QString("color:%1; font-size:%2px; font-family:%3; font-weight:800;")
                .arg(val_color).arg(fs_lg + 2).arg(font_data));

        auto* sub = new QLabel(k.sub);
        sub->setStyleSheet(
            QString("color:%1; font-size:%2px; font-family:%3;")
                .arg(text_t).arg(fs_sm - 1).arg(font_data));

        cl->addWidget(lbl);
        cl->addWidget(val);
        cl->addWidget(sub);
        cards_gl->addWidget(card, i / 3, i % 3);
    }

    results_layout_->addWidget(cards_w);

    // ── 3. Equity curve chart ─────────────────────────────────────────────
    if (!curve.isEmpty()) {
        auto* port_series  = new QLineSeries;
        auto* bm_series    = new QLineSeries;
        auto* port_upper   = new QLineSeries;  // for area fill
        auto* port_base    = new QLineSeries;  // baseline for area

        port_series->setName("Portfolio");
        bm_series->setName("Benchmark");

        // Style lines
        QPen port_pen(accent);
        port_pen.setWidth(2);
        port_series->setPen(port_pen);

        {
            QString bm_col_str = QString(text_t);
            QPen bm_pen;
            bm_pen.setColor(::QColor(bm_col_str));
            bm_pen.setWidth(1);
            bm_pen.setStyle(Qt::DashLine);
            bm_series->setPen(bm_pen);
        }

        double min_val = 1e18, max_val = -1e18;

        for (const auto& pt_val : curve) {
            auto pt = pt_val.toObject();
            QDateTime dt = QDateTime::fromString(pt["date"].toString(), "yyyy-MM-dd");
            qint64 ms = dt.toMSecsSinceEpoch();
            double pv = pt["portfolio"].toDouble();
            double bv = pt["benchmark"].toDouble();
            port_series->append(ms, pv);
            bm_series->append(ms, bv);
            port_upper->append(ms, pv);
            port_base->append(ms, init_cap);
            min_val = std::min({min_val, pv, bv});
            max_val = std::max({max_val, pv, bv});
        }

        // Area series for portfolio fill
        auto* area = new QAreaSeries(port_upper, port_base);
        QColor fill_color = accent;
        fill_color.setAlpha(30);
        area->setBrush(fill_color);
        QPen area_pen(Qt::transparent);
        area->setPen(area_pen);

        auto* chart = new QChart;
        chart->addSeries(area);
        chart->addSeries(port_series);
        chart->addSeries(bm_series);
        chart->setBackgroundBrush(QBrush(QColor(QString(bg_surface))));
        chart->setBackgroundRoundness(0);
        chart->setMargins(QMargins(4, 4, 4, 4));
        chart->legend()->setLabelColor(QColor(QString(text_s)));
        chart->legend()->setAlignment(Qt::AlignTop);
        chart->setAnimationOptions(QChart::NoAnimation);
        chart->setTitle("");

        auto* x_axis = new QDateTimeAxis;
        x_axis->setFormat("MMM yy");
        x_axis->setLabelsColor(QColor(QString(text_t)));
        x_axis->setGridLineColor(QColor(QString(border_dim)));
        x_axis->setLinePen(QPen(QColor(QString(border_med))));
        chart->addAxis(x_axis, Qt::AlignBottom);

        auto* y_axis = new QValueAxis;
        double padding = (max_val - min_val) * 0.05;
        y_axis->setRange(min_val - padding, max_val + padding);
        y_axis->setLabelFormat("$%.0f");
        y_axis->setLabelsColor(QColor(QString(text_t)));
        y_axis->setGridLineColor(QColor(QString(border_dim)));
        y_axis->setLinePen(QPen(QColor(QString(border_med))));
        chart->addAxis(y_axis, Qt::AlignLeft);

        for (auto* s : chart->series()) {
            s->attachAxis(x_axis);
            s->attachAxis(y_axis);
        }

        auto* chart_view = new QChartView(chart);
        chart_view->setRenderHint(QPainter::Antialiasing);
        chart_view->setFixedHeight(280);
        chart_view->setStyleSheet(
            QString("background:%1; border:1px solid %2; border-radius:4px;")
                .arg(bg_surface, border_dim));

        // Chart title label above
        auto* chart_title = new QLabel("EQUITY CURVE");
        chart_title->setStyleSheet(
            QString("color:%1; font-size:%2px; font-family:%3; font-weight:700;"
                    "letter-spacing:1px; padding:8px 0 2px 0;")
                .arg(text_s).arg(fs_sm).arg(font_data));
        results_layout_->addWidget(chart_title);
        results_layout_->addWidget(chart_view);
    }

    // ── 4. Execution cost strip ────────────────────────────────────────────
    auto* cost_w = new QWidget;
    cost_w->setStyleSheet(
        QString("background:%1; border:1px solid %2; border-radius:4px;")
            .arg(bg_surface, border_dim));
    auto* cost_h = new QHBoxLayout(cost_w);
    cost_h->setContentsMargins(16, 8, 16, 8);
    cost_h->setSpacing(24);

    auto add_cost_item = [&](const QString& lbl, const QString& val) {
        auto* w = new QWidget;
        auto* l = new QHBoxLayout(w);
        l->setContentsMargins(0, 0, 0, 0);
        l->setSpacing(6);
        auto* k = new QLabel(lbl);
        k->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                             .arg(text_t).arg(fs_sm).arg(font_data));
        auto* v = new QLabel(val);
        v->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; font-weight:700;")
                             .arg(text_s).arg(fs_sm).arg(font_data));
        l->addWidget(k);
        l->addWidget(v);
        cost_h->addWidget(w);
    };

    auto* cost_hdr = new QLabel("EXECUTION COSTS");
    cost_hdr->setStyleSheet(
        QString("color:%1; font-size:%2px; font-family:%3; font-weight:700; letter-spacing:1px;")
            .arg(text_t).arg(fs_sm - 1).arg(font_data));
    cost_h->addWidget(cost_hdr);
    cost_h->addWidget([&]() {
        auto* sep = new QFrame;
        sep->setFrameShape(QFrame::VLine);
        sep->setStyleSheet(QString("color:%1;").arg(border_dim));
        return sep;
    }());

    add_cost_item("Commission",
                  QString("%1 bps").arg(costs["commission_bps"].toDouble(), 0, 'f', 2));
    add_cost_item("Expected Slippage",
                  QString("%1 bps").arg(costs["expected_slippage_bps"].toDouble(), 0, 'f', 2));
    cost_h->addStretch();
    results_layout_->addWidget(cost_w);

    // ── 5. Export button ───────────────────────────────────────────────────
    auto* export_btn = new QPushButton("EXPORT JSON");
    export_btn->setCursor(Qt::PointingHandCursor);
    export_btn->setFixedHeight(28);
    export_btn->setStyleSheet(
        QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                "font-size:%3px; font-family:%4; padding:0 14px; border-radius:3px; }"
                "QPushButton:hover { background:rgba(255,255,255,0.05); }")
            .arg(accent_hex, border_dim).arg(fs_sm).arg(font_data));

    QString json_str = QJsonDocument(payload).toJson(QJsonDocument::Indented);
    connect(export_btn, &QPushButton::clicked, this, [this, json_str]() {
        QString safe = module_.label;
        safe.replace(QRegularExpression("[^a-zA-Z0-9_\\-]"), "_");
        QString fname = safe + "_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".json";
        QString dest  = services::FileManagerService::instance().storage_dir() + "/" + fname;
        QFile f(dest);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            f.write(json_str.toUtf8());
            f.close();
            services::FileManagerService::instance().register_file(
                fname, module_.label + "_backtest.json", QFileInfo(dest).size(),
                "application/json", "ai_quant_lab");
            LOG_INFO("AIQuantLab", "Backtest exported: " + fname);
        }
    });

    auto* export_row = new QWidget;
    auto* export_hl  = new QHBoxLayout(export_row);
    export_hl->setContentsMargins(0, 4, 0, 0);
    export_hl->addStretch();
    export_hl->addWidget(export_btn);
    results_layout_->addWidget(export_row);

    status_label_->setText(QString("Done — %1% return  |  Sharpe %2")
                               .arg(total_ret, 0, 'f', 2)
                               .arg(sharpe, 0, 'f', 3));
}

} // namespace fincept::screens
