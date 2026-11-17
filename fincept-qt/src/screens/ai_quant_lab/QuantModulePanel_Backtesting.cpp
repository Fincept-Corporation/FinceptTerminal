// src/screens/ai_quant_lab/QuantModulePanel_Backtesting.cpp
//
// Backtesting panel — Qlib-style strategy backtester with rich result display
// (KPI cards, equity curve chart, execution cost strip, JSON export). Extracted
// from QuantModulePanel.cpp to keep that file maintainable.
#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "screens/ai_quant_lab/QuantModulePanel.h"
#include "screens/ai_quant_lab/QuantModulePanel_Common.h"
#include "screens/ai_quant_lab/QuantModulePanel_GsHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"
#include "services/ai_quant_lab/AIQuantLabService.h"
#include "services/backtesting/BacktestingService.h"
#include "services/file_manager/FileManagerService.h"
#include "ui/formatting/NumberFormat.h"
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
#include <QVBoxLayout>
#include <QValueAxis>
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

    // One view: a complete default (8 large caps, last 3 years, top-4 momentum) runs on first show.
    begin_view({.key = QStringLiteral("backtest"),
                .cost = ViewCost::Fetch,
                .loading = tr("Downloading 3 years of prices and running the backtest…"),
                .shape = ui::LoadingSkeleton::Shape::Result});

    // Labels say what qlib_advanced_backtest.py computes; the item data is the id it takes.
    // All three rank on the trailing 20-day return (no Qlib model is involved).
    auto* strat = new QComboBox(w);
    strat->addItem(tr("Top-K dropout (20-day momentum)"), QStringLiteral("topk_dropout"));
    strat->addItem(tr("Momentum-weighted top-K"), QStringLiteral("weight_based"));
    strat->addItem(tr("Equal weight (all names)"), QStringLiteral("enhanced_indexing"));
    strat->setStyleSheet(combo_ss());
    combo_inputs_["bt_strategy"] = strat;
    vl->addWidget(build_input_row(tr("Strategy"), strat, w));

    auto* instruments = new QLineEdit(w);
    instruments->setPlaceholderText(tr("Yahoo symbols, comma-separated (e.g. AAPL,MSFT,GOOG,AMZN)"));
    // A complete default universe (the field used to be empty, so even a click failed).
    instruments->setText(QStringLiteral("AAPL,MSFT,GOOG,AMZN,META,NVDA,JPM,XOM"));
    instruments->setStyleSheet(input_ss());
    text_inputs_["bt_instruments"] = instruments;
    vl->addWidget(build_input_row(tr("Instruments"), instruments, w));

    // Default window: the last three years (it was a fixed 2020-2024, i.e. already stale).
    const QDate today = QDate::currentDate();
    auto* start_date = new QDateEdit(today.addYears(-3), w);
    start_date->setDisplayFormat("yyyy-MM-dd");
    start_date->setCalendarPopup(true);
    start_date->setStyleSheet(input_ss());
    date_inputs_["bt_start"] = start_date;
    vl->addWidget(build_input_row(tr("Start Date"), start_date, w));

    auto* end_date = new QDateEdit(today, w);
    end_date->setDisplayFormat("yyyy-MM-dd");
    end_date->setCalendarPopup(true);
    end_date->setStyleSheet(input_ss());
    date_inputs_["bt_end"] = end_date;
    vl->addWidget(build_input_row(tr("End Date"), end_date, w));

    auto* capital = make_double_spin(1000, 1e12, 1e6, 0, "", w);
    double_inputs_["bt_capital"] = capital;
    vl->addWidget(build_input_row(tr("Initial Capital ($)"), capital, w));

    auto* topk = new QSpinBox(w);
    topk->setRange(1, 100);
    topk->setValue(4); // half of the default 8-name universe (10 held everything: no selection at all)
    topk->setStyleSheet(spinbox_ss());
    int_inputs_["bt_topk"] = topk;
    vl->addWidget(build_input_row(tr("Top K Positions"), topk, w));

    // Names replaced per rebalance (Top-K dropout only).
    auto* ndrop = new QSpinBox(w);
    ndrop->setRange(1, 100);
    ndrop->setValue(1);
    ndrop->setStyleSheet(spinbox_ss());
    int_inputs_["bt_ndrop"] = ndrop;
    vl->addWidget(build_input_row(tr("N-Drop per Rebalance"), ndrop, w));
    auto sync_ndrop = [ndrop, strat]() {
        ndrop->setEnabled(strat->currentData().toString() == QLatin1String("topk_dropout"));
    };
    connect(strat, &QComboBox::currentIndexChanged, ndrop, sync_ndrop);
    sync_ndrop();

    auto* rebalance = new QSpinBox(w);
    rebalance->setRange(1, 252);
    rebalance->setValue(5);
    rebalance->setSuffix(tr(" days"));
    rebalance->setStyleSheet(spinbox_ss());
    int_inputs_["bt_rebalance"] = rebalance;
    vl->addWidget(build_input_row(tr("Rebalance Every"), rebalance, w));

    // Charged per unit of one-way turnover at every rebalance; every metric is net of it.
    auto* cost_bps = make_double_spin(0, 500, 10.0, 1, tr(" bps"), w);
    cost_bps->setToolTip(tr("Commission + slippage charged on each rebalance's turnover, in basis points."));
    double_inputs_["bt_cost_bps"] = cost_bps;
    vl->addWidget(build_input_row(tr("Trading Cost"), cost_bps, w));

    auto* rf = make_double_spin(0, 20, 2.0, 2, "%", w);
    rf->setToolTip(tr("Annual risk-free rate subtracted from returns in the Sharpe ratio."));
    double_inputs_["bt_rf"] = rf;
    vl->addWidget(build_input_row(tr("Risk-Free Rate"), rf, w));

    auto* benchmark = new QLineEdit(w);
    // The backtest prices come from Yahoo Finance, so the benchmark must be a Yahoo symbol. The
    // old placeholder ("SH000300", a Qlib/CSI 300 id) resolved to nothing and produced a flat line.
    benchmark->setPlaceholderText(tr("SPY (Yahoo Finance symbol; blank = SPY)"));
    benchmark->setStyleSheet(input_ss());
    text_inputs_["bt_benchmark"] = benchmark;
    vl->addWidget(build_input_row(tr("Benchmark"), benchmark, w));

    auto* run = make_run_button(tr("RUN BACKTEST"), w);
    set_action(run, [this]() {
        const QJsonArray tickers = parse_csv_strings(text_inputs_["bt_instruments"]->text());
        if (tickers.isEmpty()) {
            display_error(tr("Enter at least one instrument, e.g. AAPL,MSFT,GOOG."));
            return;
        }
        const QDate start = date_inputs_["bt_start"]->date();
        const QDate end = date_inputs_["bt_end"]->date();
        if (start >= end) {
            display_error(tr("Start date (%1) must be before end date (%2).")
                              .arg(start.toString("yyyy-MM-dd"), end.toString("yyyy-MM-dd")));
            return;
        }
        // NOTE: top_k >= universe size degenerates to "hold everything" (no selection); not blocked
        // (a small test universe is legitimate), the default is 4 of 8 names.
        const int topk = int_inputs_["bt_topk"]->value();
        const QString strategy_id = combo_inputs_["bt_strategy"]->currentData().toString();
        const int ndrop = int_inputs_["bt_ndrop"]->value();
        if (strategy_id == QLatin1String("topk_dropout") && ndrop > topk) {
            display_error(tr("N-Drop (%1) cannot exceed Top K (%2).").arg(ndrop).arg(topk));
            return;
        }
        show_loading(tr("Backtesting %1 over %2 instrument(s), %3 → %4...")
                         .arg(combo_inputs_["bt_strategy"]->currentText())
                         .arg(tickers.size())
                         .arg(start.toString("yyyy-MM-dd"), end.toString("yyyy-MM-dd")));
        QJsonObject params;
        QJsonObject strategy_config;
        strategy_config["type"] = strategy_id;
        strategy_config["topk"] = topk;
        strategy_config["n_drop"] = ndrop;
        strategy_config["rebalance_days"] = int_inputs_["bt_rebalance"]->value();
        params["strategy_config"] = strategy_config;
        QJsonObject dataset;
        dataset["instruments"] = text_inputs_["bt_instruments"]->text();
        dataset["start_date"] = start.toString("yyyy-MM-dd");
        dataset["end_date"] = end.toString("yyyy-MM-dd");
        params["dataset_config"] = dataset;
        QJsonObject portfolio;
        portfolio["initial_capital"] = double_inputs_["bt_capital"]->value();
        portfolio["benchmark"] = text_inputs_["bt_benchmark"]->text().trimmed();
        // One cost figure in the panel; the script splits it into commission + slippage.
        portfolio["commission_bps"] = double_inputs_["bt_cost_bps"]->value() / 2.0;
        portfolio["slippage_bps"] = double_inputs_["bt_cost_bps"]->value() / 2.0;
        portfolio["risk_free_rate"] = double_inputs_["bt_rf"]->value() / 100.0;
        params["portfolio_config"] = portfolio;
        AIQuantLabService::instance().run_backtest(params);
    });
    vl->addWidget(run);

    auto* open_terminal = make_run_button(tr("OPEN IN BACKTESTING TERMINAL"), w);
    open_terminal->setProperty(kAccentShapeProp, QVariant()); // outlined, not module-filled
    open_terminal->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                                         "padding:8px 16px; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700; }"
                                         "QPushButton:hover { background:%1; color:%3; }")
                                     .arg(ui::colors::AMBER(), ui::colors::AMBER_DIM(), ui::colors::TEXT_ON_ACCENT()));
    connect(open_terminal, &QPushButton::clicked, this, [this]() {
        auto* instruments_edit = text_inputs_.value("bt_instruments");
        // Used to return silently — the button simply did nothing and the user
        // had no idea why.
        if (!instruments_edit || instruments_edit->text().trimmed().isEmpty()) {
            display_error(tr("Enter at least one instrument before opening the Backtesting terminal."));
            return;
        }
        const QJsonArray symbols = parse_csv_strings(instruments_edit->text());
        QJsonObject config;
        config["symbols"] = symbols;
        auto* capital_spin = double_inputs_.value("bt_capital");
        if (capital_spin)
            config["initialCapital"] = capital_spin->value();
        fincept::services::backtest::BacktestingService::instance().set_pending_portfolio_config(config);
        fincept::EventBus::instance().publish("nav.switch_screen", {{"screen_id", QString("backtesting")}});
    });
    vl->addWidget(open_terminal);

    end_view();
    add_result_pane(vl);
    return w;
}

// ── Backtest Result Display ───────────────────────────────────────────────────

void QuantModulePanel::display_backtest_result(const QJsonObject& payload) {
    clear_results();

    if (!payload["success"].toBool()) {
        display_error(payload["error"].toString(tr("Unknown error")));
        return;
    }

    const QJsonObject metrics = payload["metrics"].toObject();
    const QJsonArray curve = payload["equity_curve"].toArray();
    // Costs the run actually charged (turnover x rate at each rebalance); every metric is net of them.
    const QJsonObject costs = payload["costs"].toObject();
    const QStringList tickers = [&]() {
        QStringList t;
        for (const auto& v : payload["tickers"].toArray())
            t << v.toString();
        return t;
    }();

    const QString strategy = payload["strategy"].toString();
    const QString start_date = payload["start_date"].toString();
    const QString end_date = payload["end_date"].toString();

    const QColor accent = ui::colors::legible_color(module_.color);
    const QString accent_hex = accent.name();
    const QString green_hex = ui::colors::POSITIVE();
    const QString red_hex = ui::colors::NEGATIVE();
    const QString text_p = ui::colors::TEXT_PRIMARY();
    const QString text_s = ui::colors::TEXT_SECONDARY();
    const QString text_t = ui::colors::TEXT_TERTIARY();
    const QString bg_surface = ui::colors::BG_SURFACE();
    const QString bg_raised = ui::colors::BG_RAISED();
    const QString border_dim = ui::colors::BORDER_DIM();
    const QString border_med = ui::colors::BORDER_MED();
    const QString font_data = ui::fonts::DATA_FAMILY;
    const int fs_sm = ui::fonts::SMALL;
    const int fs_lg = ui::fonts::HEADER;

    // ── 1. Header bar ──────────────────────────────────────────────────────
    auto* header_w = new QWidget;
    auto* header_h = new QHBoxLayout(header_w);
    header_h->setContentsMargins(0, 0, 0, 8);
    header_h->setSpacing(8);

    auto* title_lbl = new QLabel(tr("BACKTEST RESULTS"));
    title_lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; font-weight:800; letter-spacing:2px;")
                                 .arg(accent_hex)
                                 .arg(fs_lg)
                                 .arg(font_data));
    header_h->addWidget(title_lbl);

    header_h->addSpacing(16);

    auto make_chip = [&](const QString& text, const QString& fg, const QString& bg_hex) -> QLabel* {
        auto* c = new QLabel(text);
        c->setStyleSheet(QString("color:%1; background:%2; border-radius:3px; padding:2px 8px;"
                                 "font-size:%3px; font-family:%4; font-weight:600;")
                             .arg(fg, bg_hex)
                             .arg(fs_sm)
                             .arg(font_data));
        return c;
    };

    QString strat_display = payload["strategy_label"].toString(strategy);
    strat_display.replace('_', ' ');
    strat_display = strat_display.toUpper();
    header_h->addWidget(make_chip(strat_display, accent_hex, ui::colors::tint(module_.color, 0.15)));

    header_h->addWidget(make_chip(start_date + "  →  " + end_date, text_s, bg_raised));

    for (const auto& t : tickers)
        header_h->addWidget(make_chip(t, text_p, bg_raised));

    header_h->addStretch();
    results_layout_->addWidget(header_w);

    // ── 2. KPI cards row ──────────────────────────────────────────────────
    struct KpiCard {
        QString label;
        QString value;
        QString sub;
        bool positive = true;
        bool neutral = false;
    };

    double total_ret = metrics["total_return_pct"].toDouble();
    const QJsonValue ann_ret_v = metrics["annualised_return"];
    const QJsonValue ann_vol_v = metrics["annualised_vol"];
    const QJsonValue sharpe_v = metrics["sharpe_ratio"];
    const QJsonValue calmar_v = metrics["calmar_ratio"];
    const QJsonValue bm_ret_v = metrics["benchmark_total_return_pct"];
    const QJsonValue gross_ret_v = metrics["gross_total_return_pct"];
    double ann_ret = ann_ret_v.toDouble();
    double sharpe = sharpe_v.toDouble();
    double max_dd = metrics["max_drawdown_pct"].toDouble();
    double win_rate = metrics["win_rate_pct"].toDouble();
    double final_val = metrics["final_value"].toDouble();
    double init_cap = metrics["initial_capital"].toDouble();
    int t_days = metrics["trading_days"].toInt();

    const QString dash = QString::fromUtf8("—");
    auto fmt_pct = [](double v) { return QString("%1%2%").arg(v >= 0 ? "+" : "").arg(v, 0, 'f', 2); };
    auto fmt_pct_v = [&](const QJsonValue& v) { return v.isDouble() ? fmt_pct(v.toDouble()) : dash; };
    auto fmt_num_v = [&](const QJsonValue& v, int dec) {
        return v.isDouble() ? QString::number(v.toDouble(), 'f', dec) : dash;
    };
    // The script compounds returns on the capital the user typed (labelled in the display
    // currency), so every amount of the result is in that currency.
    auto fmt_capital = [](double v) -> QString { return gs_fmt_money(v); };

    QList<KpiCard> kpis = {
        {tr("TOTAL RETURN (NET)"), fmt_pct(total_ret),
         tr("Gross %1 · %2 final").arg(fmt_pct_v(gross_ret_v), fmt_capital(final_val)), total_ret >= 0, false},
        {tr("ANN. RETURN"), fmt_pct_v(ann_ret_v),
         ann_vol_v.isDouble() ? tr("Vol: %1%").arg(ann_vol_v.toDouble(), 0, 'f', 1) : tr("Vol: —"), ann_ret >= 0,
         !ann_ret_v.isDouble()},
        {tr("SHARPE RATIO"), fmt_num_v(sharpe_v, 3),
         !sharpe_v.isDouble() ? tr("Undefined (flat returns)")
         : sharpe >= 1        ? tr("Excellent · excess of risk-free")
         : sharpe >= 0.5      ? tr("Good · excess of risk-free")
                              : tr("Weak · excess of risk-free"),
         sharpe >= 0.5, !sharpe_v.isDouble()},
        {tr("MAX DRAWDOWN"), fmt_pct(max_dd), tr("Calmar: %1").arg(fmt_num_v(calmar_v, 3)), false, false},
        {tr("WIN RATE"), QString("%1%").arg(win_rate, 0, 'f', 1), tr("%1 days").arg(t_days), win_rate >= 50, false},
        {tr("BENCHMARK"), fmt_pct_v(bm_ret_v),
         bm_ret_v.isDouble() ? tr("%1 total return · capital %2").arg(payload["benchmark"].toString(), fmt_capital(init_cap))
                             : tr("No benchmark data · capital %1").arg(fmt_capital(init_cap)),
         bm_ret_v.toDouble() >= 0, !bm_ret_v.isDouble()},
    };

    auto* cards_w = new QWidget;
    auto* cards_gl = new QGridLayout(cards_w);
    cards_gl->setContentsMargins(0, 0, 0, 0);
    cards_gl->setSpacing(8);

    for (int i = 0; i < kpis.size(); ++i) {
        const auto& k = kpis[i];
        auto* card = new QWidget;
        card->setObjectName("btKpiCard");
        card->setStyleSheet(QString("#btKpiCard { background:%1; border:1px solid %2; border-radius:4px; }")
                                .arg(bg_surface, border_dim));

        auto* cl = new QVBoxLayout(card);
        cl->setContentsMargins(12, 10, 12, 10);
        cl->setSpacing(2);

        auto* lbl = new QLabel(k.label);
        lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; font-weight:600; letter-spacing:1px;")
                               .arg(text_t)
                               .arg(fs_sm - 1)
                               .arg(font_data));

        QString val_color = k.neutral ? text_s : (k.positive ? green_hex : red_hex);
        auto* val = new QLabel(k.value);
        val->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; font-weight:800;")
                               .arg(val_color)
                               .arg(fs_lg + 2)
                               .arg(font_data));

        auto* sub = new QLabel(k.sub);
        sub->setStyleSheet(
            QString("color:%1; font-size:%2px; font-family:%3;").arg(text_t).arg(fs_sm - 1).arg(font_data));

        cl->addWidget(lbl);
        cl->addWidget(val);
        cl->addWidget(sub);
        cards_gl->addWidget(card, i / 3, i % 3);
    }

    results_layout_->addWidget(cards_w);

    // ── 3. Equity curve chart ─────────────────────────────────────────────
    if (!curve.isEmpty()) {
        auto* port_series = new QLineSeries;
        auto* bm_series = new QLineSeries;
        auto* port_upper = new QLineSeries; // for area fill
        auto* port_base = new QLineSeries;  // baseline for area

        port_series->setName(tr("Portfolio"));
        bm_series->setName(tr("Benchmark"));

        // Style lines
        QPen port_pen(accent);
        port_pen.setWidth(2);
        port_series->setPen(port_pen);
        port_series->setProperty(kSeriesColorProp, module_.color); // re-adapted on theme change

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
            port_series->append(ms, pv);
            port_upper->append(ms, pv);
            port_base->append(ms, init_cap);
            min_val = std::min(min_val, pv);
            max_val = std::max(max_val, pv);
            // No benchmark prices → null points: leave the line out rather than draw it at 0.
            if (pt["benchmark"].isDouble()) {
                const double bv = pt["benchmark"].toDouble();
                bm_series->append(ms, bv);
                min_val = std::min(min_val, bv);
                max_val = std::max(max_val, bv);
            }
        }

        // Area series for portfolio fill
        auto* area = new QAreaSeries(port_upper, port_base);
        QColor fill_color = accent;
        fill_color.setAlpha(30);
        area->setBrush(fill_color);
        area->setProperty(kSeriesColorProp, module_.color);
        QPen area_pen(Qt::transparent);
        area->setPen(area_pen);

        auto* chart = new QChart;
        chart->addSeries(area);
        chart->addSeries(port_series);
        if (bm_series->count() > 0)
            chart->addSeries(bm_series);
        else
            delete bm_series;
        chart->setBackgroundBrush(QBrush(QColor(QString(bg_surface))));
        chart->setBackgroundRoundness(0);
        chart->setMargins(QMargins(4, 4, 4, 4));
        chart->legend()->setLabelColor(QColor(QString(text_s)));
        chart->legend()->setFont(ui::fonts::font(-3));
        chart->legend()->setAlignment(Qt::AlignTop);
        chart->setAnimationOptions(QChart::NoAnimation);
        chart->setTitle("");

        auto* x_axis = new QDateTimeAxis;
        x_axis->setFormat("MMM yy");
        x_axis->setLabelsColor(QColor(QString(text_t)));
        x_axis->setLabelsFont(ui::fonts::font(-3));
        x_axis->setGridLineColor(QColor(QString(border_dim)));
        x_axis->setLinePen(QPen(QColor(QString(border_med))));
        chart->addAxis(x_axis, Qt::AlignBottom);

        auto* y_axis = new QValueAxis;
        double padding = (max_val - min_val) * 0.05;
        y_axis->setRange(min_val - padding, max_val + padding);
        y_axis->setLabelFormat(
            ui::formatting::axis_printf_format(ui::formatting::display_currency(), 0)); // capital currency
        y_axis->setLabelsColor(QColor(QString(text_t)));
        y_axis->setLabelsFont(ui::fonts::font(-3));
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
            QString("background:%1; border:1px solid %2; border-radius:4px;").arg(bg_surface, border_dim));

        // Chart title label above
        auto* chart_title = new QLabel(tr("EQUITY CURVE  ·  NET OF TRADING COSTS"));
        chart_title->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; font-weight:700;"
                                           "letter-spacing:1px; padding:8px 0 2px 0;")
                                       .arg(text_s)
                                       .arg(fs_sm)
                                       .arg(font_data));
        results_layout_->addWidget(chart_title);
        results_layout_->addWidget(chart_view);
    }

    // ── 4. Execution cost strip ────────────────────────────────────────────
    auto* cost_w = new QWidget;
    cost_w->setStyleSheet(
        QString("background:%1; border:1px solid %2; border-radius:4px;").arg(bg_surface, border_dim));
    auto* cost_h = new QHBoxLayout(cost_w);
    cost_h->setContentsMargins(16, 8, 16, 8);
    cost_h->setSpacing(24);

    auto add_cost_item = [&](const QString& lbl, const QString& val) {
        auto* w = new QWidget;
        auto* l = new QHBoxLayout(w);
        l->setContentsMargins(0, 0, 0, 0);
        l->setSpacing(6);
        auto* k = new QLabel(lbl);
        k->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;").arg(text_t).arg(fs_sm).arg(font_data));
        auto* v = new QLabel(val);
        v->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; font-weight:700;")
                             .arg(text_s)
                             .arg(fs_sm)
                             .arg(font_data));
        l->addWidget(k);
        l->addWidget(v);
        cost_h->addWidget(w);
    };

    auto* cost_hdr = new QLabel(tr("EXECUTION COSTS"));
    cost_hdr->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; font-weight:700; letter-spacing:1px;")
                                .arg(text_t)
                                .arg(fs_sm - 1)
                                .arg(font_data));
    cost_h->addWidget(cost_hdr);
    cost_h->addWidget([&]() {
        auto* sep = new QFrame;
        sep->setFrameShape(QFrame::VLine);
        sep->setStyleSheet(QString("color:%1;").arg(border_dim));
        return sep;
    }());

    auto cost_pct = [&](const char* key, int dec) {
        const QJsonValue v = costs.value(QLatin1String(key));
        return v.isDouble() ? QStringLiteral("%1%").arg(v.toDouble(), 0, 'f', dec) : dash;
    };
    add_cost_item(tr("Rate"), tr("%1 bps / turnover")
                                  .arg(costs["commission_bps"].toDouble() + costs["slippage_bps"].toDouble(), 0, 'f', 1));
    add_cost_item(tr("Rebalances"), QString::number(costs["rebalances"].toInt()));
    add_cost_item(tr("Avg Turnover"), cost_pct("avg_turnover_per_rebalance_pct", 1));
    add_cost_item(tr("Annual Turnover"), cost_pct("annual_turnover_pct", 0));
    add_cost_item(tr("Total Cost Drag"), cost_pct("total_cost_drag_pct", 2));
    cost_h->addStretch();
    results_layout_->addWidget(cost_w);

    // ── 5. Export button ───────────────────────────────────────────────────
    auto* export_btn = new QPushButton(tr("EXPORT JSON"));
    export_btn->setCursor(Qt::PointingHandCursor);
    export_btn->setMinimumHeight(28);
    export_btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                                      "font-size:%3px; font-family:%4; padding:0 14px; border-radius:3px; }"
                                      "QPushButton:hover { background:%5; }")
                                  .arg(accent_hex, border_dim)
                                  .arg(fs_sm)
                                  .arg(font_data)
                                  .arg(ui::colors::overlay(0.05)));

    QString json_str = QJsonDocument(payload).toJson(QJsonDocument::Indented);
    connect(export_btn, &QPushButton::clicked, this, [this, json_str]() {
        QString safe = module_.label;
        safe.replace(QRegularExpression("[^a-zA-Z0-9_\\-]"), "_");
        QString fname = safe + "_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".json";
        QString dest = services::FileManagerService::instance().storage_dir() + "/" + fname;
        QFile f(dest);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            f.write(json_str.toUtf8());
            f.close();
            services::FileManagerService::instance().register_file(
                fname, module_.label + "_backtest.json", QFileInfo(dest).size(), "application/json", "ai_quant_lab");
            LOG_INFO("AIQuantLab", "Backtest exported: " + fname);
        } else {
            LOG_WARN("AIQuantLab", fincept::logf("Result export could not be written",
                                                 {{"path", fincept::log_path(dest)}, {"error", f.errorString()}}));
            if (status_label_)
                status_label_->setText(tr("Export failed: %1").arg(f.errorString()));
        }
    });

    auto* export_row = new QWidget;
    auto* export_hl = new QHBoxLayout(export_row);
    export_hl->setContentsMargins(0, 4, 0, 0);
    export_hl->addStretch();
    export_hl->addWidget(export_btn);
    results_layout_->addWidget(export_row);

    status_label_->setText(tr("Done — %1% return  |  Sharpe %2").arg(total_ret, 0, 'f', 2).arg(sharpe, 0, 'f', 3));
}

} // namespace fincept::screens
