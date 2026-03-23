// src/screens/equity_research/EquityOverviewTab.cpp
#include "screens/equity_research/EquityOverviewTab.h"

#include "services/equity/EquityResearchService.h"
#include "ui/theme/Theme.h"

#include <QCandlestickSeries>
#include <QCandlestickSet>
#include <QChart>
#include <QDateTimeAxis>
#include <QFrame>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QSizePolicy>
#include <QValueAxis>
#include <QVBoxLayout>

namespace fincept::screens {

// ── Helpers ───────────────────────────────────────────────────────────────────

namespace {

// Dark panel with a colored header label
QFrame* make_panel(const QString& title, const QString& title_color) {
    auto* f = new QFrame;
    f->setStyleSheet(QString("QFrame { background:%1; border:1px solid %2; border-radius:4px; }")
                     .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));
    auto* vl = new QVBoxLayout(f);
    vl->setContentsMargins(10, 8, 10, 8);
    vl->setSpacing(5);

    if (!title.isEmpty()) {
        auto* lbl = new QLabel(title);
        lbl->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; "
                                   "letter-spacing:1px; background:transparent; border:0;")
                           .arg(title_color));
        vl->addWidget(lbl);
        auto* sep = new QFrame;
        sep->setFrameShape(QFrame::HLine);
        sep->setStyleSheet(QString("border:0; border-top:1px solid %1; background:transparent;")
                           .arg(ui::colors::BORDER_DIM));
        vl->addWidget(sep);
    }
    return f;
}

// Key-value row; returns pointer to value label for later updates
QLabel* add_row(QFrame* panel, const QString& key, const QString& val_color,
                const QString& initial = "—") {
    auto* hl = new QHBoxLayout;
    hl->setSpacing(4);
    hl->setContentsMargins(0, 0, 0, 0);

    auto* k = new QLabel(key);
    k->setStyleSheet(QString("color:%1; font-size:10px; background:transparent; border:0;")
                     .arg(ui::colors::TEXT_SECONDARY));
    k->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto* v = new QLabel(initial);
    v->setStyleSheet(QString("color:%1; font-size:10px; font-weight:600; "
                              "background:transparent; border:0;")
                     .arg(val_color));
    v->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    v->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    hl->addWidget(k);
    hl->addWidget(v, 1);
    static_cast<QVBoxLayout*>(panel->layout())->addLayout(hl);
    return v;
}

} // anonymous namespace

// ── Constructor ───────────────────────────────────────────────────────────────

EquityOverviewTab::EquityOverviewTab(QWidget* parent) : QWidget(parent) {
    build_ui();

    auto& svc = services::equity::EquityResearchService::instance();
    connect(&svc, &services::equity::EquityResearchService::quote_loaded,
            this, &EquityOverviewTab::on_quote_loaded);
    connect(&svc, &services::equity::EquityResearchService::info_loaded,
            this, &EquityOverviewTab::on_info_loaded);
    connect(&svc, &services::equity::EquityResearchService::historical_loaded,
            this, &EquityOverviewTab::on_historical_loaded);
}

void EquityOverviewTab::set_symbol(const QString& symbol) {
    if (symbol == current_symbol_) return;
    current_symbol_ = symbol;
    info_loaded_ = quote_loaded_ = historical_loaded_ = false;
    loading_overlay_->show_loading("LOADING OVERVIEW…");
}

// ── Build UI ──────────────────────────────────────────────────────────────────

void EquityOverviewTab::build_ui() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));

    loading_overlay_ = new ui::LoadingOverlay(this);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("background:transparent; border:0;");
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto* content = new QWidget;
    content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(10, 10, 10, 10);
    vl->setSpacing(8);

    // ── Top 4-column grid ─────────────────────────────────────────────────────
    // Col1: Trading + Valuation + Share Stats
    // Col2+3: Chart (spans 2)
    // Col4: Analyst + 52W + Profitability + Growth
    auto* top = new QHBoxLayout;
    top->setSpacing(8);

    auto* col1 = build_col1();
    col1->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    auto* chart = build_chart_panel();
    chart->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* col4 = build_col4();
    col4->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    top->addWidget(col1, 1);
    top->addWidget(chart, 2);
    top->addWidget(col4, 1);

    vl->addLayout(top, 1);

    // ── Bottom row: Description + Company Info + Financial Health ─────────────
    vl->addWidget(build_bottom_row(), 0);

    scroll->setWidget(content);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);
}

// ── Column 1: Trading + Valuation + Share Stats ───────────────────────────────

QWidget* EquityOverviewTab::build_col1() {
    auto* w = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(6);
    vl->addWidget(build_trading_panel());
    vl->addWidget(build_valuation_panel());
    vl->addWidget(build_share_stats_panel());
    return w;
}

QWidget* EquityOverviewTab::build_trading_panel() {
    auto* p = make_panel("TODAY'S TRADING", ui::colors::AMBER);
    open_val_       = add_row(p, "OPEN",       "#22d3ee");
    high_val_       = add_row(p, "HIGH",       ui::colors::POSITIVE);
    low_val_        = add_row(p, "LOW",        ui::colors::NEGATIVE);
    prev_close_val_ = add_row(p, "PREV CLOSE", ui::colors::TEXT_PRIMARY);
    vol_val_        = add_row(p, "VOLUME",     "#eab308");
    static_cast<QVBoxLayout*>(p->layout())->addStretch();
    return p;
}

QWidget* EquityOverviewTab::build_valuation_panel() {
    auto* p = make_panel("VALUATION", "#22d3ee");
    mktcap_val_ = add_row(p, "MARKET CAP", "#22d3ee");
    pe_val_     = add_row(p, "P/E RATIO",  "#eab308");
    fwd_pe_val_ = add_row(p, "FWD P/E",    "#eab308");
    peg_val_    = add_row(p, "PEG RATIO",  "#eab308");
    pb_val_     = add_row(p, "P/B RATIO",  "#22d3ee");
    div_val_    = add_row(p, "DIV YIELD",  ui::colors::POSITIVE);
    beta_val_   = add_row(p, "BETA",       ui::colors::TEXT_PRIMARY);
    static_cast<QVBoxLayout*>(p->layout())->addStretch();
    return p;
}

QWidget* EquityOverviewTab::build_share_stats_panel() {
    auto* p = make_panel("SHARE STATS", "#a855f7");
    shares_out_val_   = add_row(p, "SHARES OUT",   "#22d3ee");
    float_val_        = add_row(p, "FLOAT",         "#22d3ee");
    insiders_val_     = add_row(p, "INSIDERS",      "#eab308");
    institutions_val_ = add_row(p, "INSTITUTIONS",  "#eab308");
    short_pct_val_    = add_row(p, "SHORT %",       ui::colors::NEGATIVE);
    static_cast<QVBoxLayout*>(p->layout())->addStretch();
    return p;
}

// ── Chart Panel ───────────────────────────────────────────────────────────────

QWidget* EquityOverviewTab::build_chart_panel() {
    auto* p = make_panel("", ui::colors::AMBER);
    p->setStyleSheet(QString("QFrame { background:%1; border:1px solid %2; border-radius:4px; }")
                     .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));

    // Period buttons row
    auto* btn_row = new QHBoxLayout;
    btn_row->setSpacing(4);
    btn_row->setContentsMargins(0, 0, 0, 0);

    auto* period_lbl = new QLabel("PERIOD");
    period_lbl->setStyleSheet(QString("color:%1; font-size:10px; background:transparent; border:0;")
                               .arg(ui::colors::TEXT_SECONDARY));
    btn_row->addWidget(period_lbl);

    QString btn_style = QString(R"(
        QPushButton {
            background:transparent; color:%1; border:1px solid %2;
            border-radius:3px; padding:2px 8px; font-size:10px; font-weight:700;
        }
        QPushButton:checked { background:%3; color:%4; border-color:%3; }
        QPushButton:hover:!checked { border-color:%3; background:#2a2a2a; }
    )").arg(ui::colors::TEXT_SECONDARY, ui::colors::BORDER_DIM,
            ui::colors::AMBER, ui::colors::BG_BASE);

    auto make_btn = [&](const QString& label, QPushButton*& out) {
        out = new QPushButton(label);
        out->setStyleSheet(btn_style);
        out->setCheckable(true);
        btn_row->addWidget(out);
    };

    make_btn("1M", btn_1m_);
    make_btn("3M", btn_3m_);
    make_btn("6M", btn_6m_);
    make_btn("1Y", btn_1y_);
    make_btn("5Y", btn_5y_);
    btn_1y_->setChecked(true);
    btn_row->addStretch();

    // Exclusive button group behavior
    auto decheck_others = [this](QPushButton* active) {
        for (auto* b : {btn_1m_, btn_3m_, btn_6m_, btn_1y_, btn_5y_})
            b->setChecked(b == active);
    };

    auto reload = [this, decheck_others](QPushButton* btn, const QString& period) {
        decheck_others(btn);
        current_period_ = period;
        services::equity::EquityResearchService::instance().load_symbol(current_symbol_, period);
    };

    connect(btn_1m_, &QPushButton::clicked, this, [=]() { reload(btn_1m_, "1mo"); });
    connect(btn_3m_, &QPushButton::clicked, this, [=]() { reload(btn_3m_, "3mo"); });
    connect(btn_6m_, &QPushButton::clicked, this, [=]() { reload(btn_6m_, "6mo"); });
    connect(btn_1y_, &QPushButton::clicked, this, [=]() { reload(btn_1y_, "1y");  });
    connect(btn_5y_, &QPushButton::clicked, this, [=]() { reload(btn_5y_, "5y");  });

    // Chart view
    chart_view_ = new QChartView;
    chart_view_->setRenderHint(QPainter::Antialiasing, false);
    chart_view_->setStyleSheet("background:transparent; border:0;");
    chart_view_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Loading placeholder
    auto* placeholder = new QLabel("Select a symbol to load chart");
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet(QString("color:%1; font-size:12px; background:transparent; border:0;")
                                .arg(ui::colors::TEXT_TERTIARY));

    auto* vl = static_cast<QVBoxLayout*>(p->layout());
    vl->addLayout(btn_row);
    vl->addWidget(chart_view_, 1);
    vl->addWidget(placeholder);
    placeholder->hide();

    return p;
}

// ── Column 4: Analyst + 52W + Profitability + Growth ─────────────────────────

QWidget* EquityOverviewTab::build_col4() {
    auto* w = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(6);
    vl->addWidget(build_analyst_panel());
    vl->addWidget(build_52w_panel());
    vl->addWidget(build_profitability_panel());
    vl->addWidget(build_growth_panel());
    return w;
}

QWidget* EquityOverviewTab::build_analyst_panel() {
    auto* p = make_panel("ANALYST TARGETS", "#FF00FF");
    target_high_val_   = add_row(p, "HIGH",     ui::colors::POSITIVE);
    target_mean_val_   = add_row(p, "MEAN",     "#eab308");
    target_low_val_    = add_row(p, "LOW",      ui::colors::NEGATIVE);
    analyst_count_val_ = add_row(p, "ANALYSTS", "#22d3ee");

    // Recommendation badge
    rec_key_label_ = new QLabel("—");
    rec_key_label_->setAlignment(Qt::AlignCenter);
    rec_key_label_->setStyleSheet(
        QString("background:%1; color:%2; border-radius:3px; padding:3px 6px; "
                "font-size:10px; font-weight:700;")
        .arg(ui::colors::BG_RAISED, ui::colors::TEXT_SECONDARY));
    static_cast<QVBoxLayout*>(p->layout())->addWidget(rec_key_label_);
    static_cast<QVBoxLayout*>(p->layout())->addStretch();
    return p;
}

QWidget* EquityOverviewTab::build_52w_panel() {
    auto* p = make_panel("52 WEEK RANGE", "#eab308");
    w52h_val_    = add_row(p, "HIGH",    ui::colors::POSITIVE);
    w52l_val_    = add_row(p, "LOW",     ui::colors::NEGATIVE);
    avg_vol_val_ = add_row(p, "AVG VOL", "#22d3ee");
    static_cast<QVBoxLayout*>(p->layout())->addStretch();
    return p;
}

QWidget* EquityOverviewTab::build_profitability_panel() {
    auto* p = make_panel("PROFITABILITY", ui::colors::POSITIVE);
    gross_margin_val_  = add_row(p, "GROSS MARGIN", ui::colors::POSITIVE);
    op_margin_val_     = add_row(p, "OPER. MARGIN", ui::colors::POSITIVE);
    profit_margin_val_ = add_row(p, "PROFIT MARGIN",ui::colors::POSITIVE);
    roa_val_           = add_row(p, "ROA",           "#22d3ee");
    roe_val_           = add_row(p, "ROE",           "#22d3ee");
    static_cast<QVBoxLayout*>(p->layout())->addStretch();
    return p;
}

QWidget* EquityOverviewTab::build_growth_panel() {
    auto* p = make_panel("GROWTH RATES", "#3b82f6");
    rev_growth_val_      = add_row(p, "REVENUE",   ui::colors::POSITIVE);
    earnings_growth_val_ = add_row(p, "EARNINGS",  ui::colors::POSITIVE);
    static_cast<QVBoxLayout*>(p->layout())->addStretch();
    return p;
}

// ── Bottom Row ────────────────────────────────────────────────────────────────

QWidget* EquityOverviewTab::build_bottom_row() {
    auto* w = new QWidget;
    auto* hl = new QHBoxLayout(w);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(8);
    hl->addWidget(build_company_desc_panel(), 2);

    auto* right = new QWidget;
    auto* rvl = new QVBoxLayout(right);
    rvl->setContentsMargins(0, 0, 0, 0);
    rvl->setSpacing(6);
    rvl->addWidget(build_company_info_panel());
    rvl->addWidget(build_financial_health_panel());

    hl->addWidget(right, 1);
    return w;
}

QWidget* EquityOverviewTab::build_company_desc_panel() {
    auto* p = make_panel("COMPANY OVERVIEW", "#22d3ee");
    company_desc_ = new QLabel("—");
    company_desc_->setWordWrap(true);
    company_desc_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    company_desc_->setStyleSheet(
        QString("color:%1; font-size:10px; line-height:1.5; "
                "background:transparent; border:0;").arg(ui::colors::TEXT_PRIMARY));
    company_desc_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    static_cast<QVBoxLayout*>(p->layout())->addWidget(company_desc_);
    return p;
}

QWidget* EquityOverviewTab::build_company_info_panel() {
    auto* p = make_panel("COMPANY INFO", ui::colors::TEXT_PRIMARY);
    company_emp_      = add_row(p, "EMPLOYEES", "#22d3ee");
    company_web_      = add_row(p, "WEBSITE",   "#3b82f6");
    company_currency_ = add_row(p, "CURRENCY",  "#22d3ee");
    static_cast<QVBoxLayout*>(p->layout())->addStretch();
    return p;
}

QWidget* EquityOverviewTab::build_financial_health_panel() {
    auto* p = make_panel("FINANCIAL HEALTH", ui::colors::AMBER);
    cash_val_    = add_row(p, "CASH",    ui::colors::POSITIVE);
    debt_val_    = add_row(p, "DEBT",    ui::colors::NEGATIVE);
    free_cf_val_ = add_row(p, "FREE CF", "#22d3ee");
    static_cast<QVBoxLayout*>(p->layout())->addStretch();
    return p;
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void EquityOverviewTab::on_quote_loaded(services::equity::QuoteData q) {
    if (q.symbol != current_symbol_) return;
    cached_quote_ = q;
    quote_loaded_ = true;
    if (info_loaded_ && quote_loaded_ && historical_loaded_) loading_overlay_->hide_loading();

    open_val_->setText(fmt_price(q.open));
    high_val_->setText(fmt_price(q.high));
    low_val_->setText(fmt_price(q.low));
    prev_close_val_->setText(fmt_price(q.prev_close));
    vol_val_->setText(fmt_large(q.volume));
}

void EquityOverviewTab::on_info_loaded(services::equity::StockInfo info) {
    if (info.symbol != current_symbol_) return;
    cached_info_ = info;
    info_loaded_ = true;
    if (info_loaded_ && quote_loaded_ && historical_loaded_) loading_overlay_->hide_loading();

    // ── Valuation ─────────────────────────────────────────────────────────────
    mktcap_val_->setText(info.market_cap > 0 ? fmt_large(info.market_cap) : "N/A");
    pe_val_->setText(info.pe_ratio > 0 ? QString::number(info.pe_ratio, 'f', 2) : "N/A");
    fwd_pe_val_->setText(info.forward_pe > 0 ? QString::number(info.forward_pe, 'f', 2) : "N/A");
    peg_val_->setText(info.peg_ratio > 0 ? QString::number(info.peg_ratio, 'f', 2) : "N/A");
    pb_val_->setText(info.price_to_book > 0 ? QString::number(info.price_to_book, 'f', 2) : "N/A");
    div_val_->setText(info.dividend_yield > 0 ? fmt_pct(info.dividend_yield) : "N/A");
    beta_val_->setText(info.beta != 0.0 ? QString::number(info.beta, 'f', 2) : "N/A");

    // ── Share Stats ───────────────────────────────────────────────────────────
    shares_out_val_->setText(fmt_large(info.shares_outstanding));
    float_val_->setText(fmt_large(info.float_shares));
    insiders_val_->setText(fmt_pct(info.held_insiders_pct));
    institutions_val_->setText(fmt_pct(info.held_institutions_pct));
    short_pct_val_->setText(fmt_pct(info.short_pct_of_float));

    // ── 52 Week Range ─────────────────────────────────────────────────────────
    w52h_val_->setText(fmt_price(info.week52_high));
    w52l_val_->setText(fmt_price(info.week52_low));
    avg_vol_val_->setText(fmt_large(info.avg_volume));

    // ── Analyst Targets ───────────────────────────────────────────────────────
    target_high_val_->setText(fmt_price(info.target_high));
    target_mean_val_->setText(fmt_price(info.target_mean));
    target_low_val_->setText(fmt_price(info.target_low));
    analyst_count_val_->setText(info.analyst_count > 0
                                ? QString::number(info.analyst_count) : "N/A");

    QString rec = info.recommendation_key.toUpper().replace("_", " ");
    QString rec_color = ui::colors::TEXT_SECONDARY;
    if (rec == "STRONG BUY" || rec == "BUY")   rec_color = ui::colors::POSITIVE;
    else if (rec == "STRONG SELL" || rec == "SELL") rec_color = ui::colors::NEGATIVE;
    else if (rec == "HOLD")                         rec_color = ui::colors::AMBER;
    rec_key_label_->setText(rec.isEmpty() ? "—" : rec);
    rec_key_label_->setStyleSheet(
        QString("background:%1; color:%2; border-radius:3px; padding:3px 6px; "
                "font-size:10px; font-weight:700;")
        .arg(ui::colors::BG_RAISED, rec_color));

    // ── Profitability ─────────────────────────────────────────────────────────
    gross_margin_val_->setText(fmt_pct(info.gross_margins));
    op_margin_val_->setText(fmt_pct(info.operating_margins));
    profit_margin_val_->setText(fmt_pct(info.profit_margins));
    roa_val_->setText(fmt_pct(info.roa));
    roe_val_->setText(fmt_pct(info.roe));

    // ── Growth Rates ──────────────────────────────────────────────────────────
    rev_growth_val_->setText(fmt_pct(info.revenue_growth));
    earnings_growth_val_->setText(fmt_pct(info.earnings_growth));

    // ── Company Info ──────────────────────────────────────────────────────────
    company_desc_->setText(info.description);
    company_emp_->setText(info.employees > 0
                          ? fmt_large(static_cast<double>(info.employees)) : "N/A");
    company_web_->setText(info.website.left(28));
    company_currency_->setText(info.currency.isEmpty() ? "N/A" : info.currency);

    // ── Financial Health ──────────────────────────────────────────────────────
    cash_val_->setText(fmt_large(info.total_cash));
    debt_val_->setText(fmt_large(info.total_debt));
    free_cf_val_->setText(fmt_large(info.free_cashflow));
}

void EquityOverviewTab::on_historical_loaded(QString symbol,
                                              QVector<services::equity::Candle> candles) {
    if (symbol != current_symbol_) return;
    historical_loaded_ = true;
    if (info_loaded_ && quote_loaded_ && historical_loaded_) loading_overlay_->hide_loading();
    rebuild_chart(candles);
}

// ── Chart ─────────────────────────────────────────────────────────────────────

void EquityOverviewTab::rebuild_chart(const QVector<services::equity::Candle>& candles) {
    if (candles.isEmpty()) return;

    auto* series = new QCandlestickSeries;
    series->setIncreasingColor(QColor(ui::colors::POSITIVE));
    series->setDecreasingColor(QColor(ui::colors::NEGATIVE));
    series->setBodyOutlineVisible(false);
    series->setMaximumColumnWidth(8);

    for (const auto& c : candles) {
        auto* s = new QCandlestickSet(c.open, c.high, c.low, c.close,
                                      static_cast<qreal>(c.timestamp) * 1000.0);
        series->append(s);
    }

    auto* chart = new QChart;
    chart->addSeries(series);
    chart->setBackgroundBrush(QBrush(QColor(ui::colors::BG_SURFACE)));
    chart->setPlotAreaBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE)));
    chart->setPlotAreaBackgroundVisible(true);
    chart->legend()->hide();
    chart->setMargins(QMargins(4, 4, 4, 4));

    auto* axisX = new QDateTimeAxis;
    axisX->setFormat("MMM yy");
    axisX->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY));
    axisX->setGridLineColor(QColor(ui::colors::BORDER_DIM));
    axisX->setLabelsFont(QFont("monospace", 8));
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    auto* axisY = new QValueAxis;
    axisY->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY));
    axisY->setGridLineColor(QColor(ui::colors::BORDER_DIM));
    axisY->setLabelFormat("$%.0f");
    axisY->setLabelsFont(QFont("monospace", 8));
    chart->addAxis(axisY, Qt::AlignRight);
    series->attachAxis(axisY);

    chart_view_->setChart(chart);
}

// ── Formatters ────────────────────────────────────────────────────────────────

QString EquityOverviewTab::fmt_large(double v) {
    bool neg = v < 0;
    double av = qAbs(v);
    QString s;
    if (av >= 1e12)      s = QString("%1T").arg(av / 1e12, 0, 'f', 2);
    else if (av >= 1e9)  s = QString("%1B").arg(av / 1e9,  0, 'f', 2);
    else if (av >= 1e6)  s = QString("%1M").arg(av / 1e6,  0, 'f', 2);
    else if (av >= 1e3)  s = QString("%1K").arg(av / 1e3,  0, 'f', 1);
    else                 s = QString::number(static_cast<qint64>(av));
    return neg ? "-" + s : s;
}

QString EquityOverviewTab::fmt_pct(double v) {
    // yfinance returns e.g. 0.45 for 45% — multiply by 100
    return QString("%1%").arg(v * 100.0, 0, 'f', 2);
}

QString EquityOverviewTab::fmt_price(double v) {
    if (v == 0.0) return "—";
    return QString("$%1").arg(v, 0, 'f', 2);
}

} // namespace fincept::screens
