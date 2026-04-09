// src/screens/equity_research/EquityOverviewTab.cpp
#include "screens/equity_research/EquityOverviewTab.h"

#include "services/equity/EquityResearchService.h"
#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QPainter>
#include <QScrollArea>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

// ── Accent colors without a theme token ──────────────────────────────────────
static constexpr const char* CYAN     = "#22d3ee";
static constexpr const char* YELLOW   = "#eab308";
static constexpr const char* PURPLE   = "#a855f7";
static constexpr const char* BLUE     = "#3b82f6";
static constexpr const char* MAGENTA  = "#e879f9";

static constexpr int FONT_KEY   = 12;  // key labels
static constexpr int FONT_VAL   = 13;  // value labels
static constexpr int FONT_TITLE = 12;  // panel titles
static constexpr int FONT_DESC  = 12;  // description text

// ── Panel helpers ─────────────────────────────────────────────────────────────

namespace {

QFrame* make_panel(const QString& title, const char* title_color) {
    auto* f = new QFrame;
    f->setStyleSheet(QString("QFrame{background:%1;border:1px solid %2;border-radius:2px;}").arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));
    auto* vl = new QVBoxLayout(f);
    vl->setContentsMargins(10, 7, 10, 7);
    vl->setSpacing(4);

    if (!title.isEmpty()) {
        auto* lbl = new QLabel(title);
        lbl->setStyleSheet(QString("color:%1;font-size:%2px;font-weight:700;"
                                   "letter-spacing:1px;background:transparent;border:0;")
                               .arg(title_color).arg(FONT_TITLE));
        vl->addWidget(lbl);
        auto* sep = new QFrame;
        sep->setFrameShape(QFrame::HLine);
        sep->setFixedHeight(1);
        sep->setStyleSheet(QString("border:0;background:%1;").arg(ui::colors::BORDER_DIM));
        vl->addWidget(sep);
    }
    return f;
}

QLabel* add_row(QFrame* panel, const QString& key, const char* val_color) {
    auto* hl = new QHBoxLayout;
    hl->setSpacing(4);
    hl->setContentsMargins(0, 0, 0, 0);

    auto* k = new QLabel(key);
    k->setStyleSheet(QString("color:%1;font-size:%2px;background:transparent;border:0;").arg(ui::colors::TEXT_SECONDARY).arg(FONT_KEY));

    auto* v = new QLabel("\xe2\x80\x94");
    v->setStyleSheet(QString("color:%1;font-size:%2px;font-weight:600;background:transparent;border:0;")
                         .arg(val_color).arg(FONT_VAL));
    v->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    hl->addWidget(k);
    hl->addWidget(v, 1);
    static_cast<QVBoxLayout*>(panel->layout())->addLayout(hl);
    return v;
}

} // anonymous namespace

// ══════════════════════════════════════════════════════════════════════════════
//  ResearchCandleCanvas — QPainter candlestick chart
// ══════════════════════════════════════════════════════════════════════════════

ResearchCandleCanvas::ResearchCandleCanvas(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(250);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ResearchCandleCanvas::set_candles(const QVector<services::equity::Candle>& candles, const QString& cs) {
    candles_ = candles;
    currency_sym_ = cs;
    dirty_ = true;
    update();
}

void ResearchCandleCanvas::clear() {
    candles_.clear();
    dirty_ = true;
    update();
}

void ResearchCandleCanvas::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    dirty_ = true;
}

void ResearchCandleCanvas::paintEvent(QPaintEvent*) {
    if (dirty_) {
        rebuild_cache();
        dirty_ = false;
    }
    QPainter p(this);
    p.drawPixmap(0, 0, cache_);
}

void ResearchCandleCanvas::rebuild_cache() {
    const int W = width();
    const int H = height();
    if (W <= 0 || H <= 0) return;

    cache_ = QPixmap(W, H);
    cache_.fill(QColor(ui::colors::BG_BASE()));

    QPainter p(&cache_);
    p.setRenderHint(QPainter::Antialiasing, false);

    const int total = candles_.size();
    if (total == 0) {
        p.setPen(QColor(ui::colors::TEXT_SECONDARY()));
        p.setFont(QFont("Consolas", 11));
        p.drawText(cache_.rect(), Qt::AlignCenter, "Waiting for data...");
        return;
    }

    const int start = qMax(0, total - MAX_VISIBLE);
    const int count = total - start;

    // Price range
    double lo = 1e18, hi = 0.0;
    for (int i = start; i < total; ++i) {
        lo = std::min(lo, candles_[i].low);
        hi = std::max(hi, candles_[i].high);
    }
    if (lo >= hi) return;

    const double margin = (hi - lo) * 0.06;
    lo -= margin;
    hi += margin;

    const int plot_w = W - PRICE_AXIS_W;
    const int plot_h = H - TIME_AXIS_H;
    if (plot_w <= 0 || plot_h <= 0) return;

    auto py = [&](double price) -> int {
        return static_cast<int>(plot_h - (price - lo) / (hi - lo) * plot_h);
    };

    // Grid lines
    p.setPen(QPen(QColor(ui::colors::BORDER_DIM()), 1, Qt::DotLine));
    for (int g = 1; g < 6; ++g) {
        int gy = plot_h * g / 6;
        p.drawLine(0, gy, plot_w, gy);
    }

    // Candles
    const double slot_w = static_cast<double>(plot_w) / count;
    const int body_w = qMax(1, static_cast<int>(slot_w * 0.65));
    const int half = body_w / 2;

    const QColor bull_color(ui::colors::POSITIVE.get());
    const QColor bear_color(ui::colors::NEGATIVE.get());
    const QColor wick_bull("#2a9d5c");
    const QColor wick_bear("#b83a3a");

    for (int i = 0; i < count; ++i) {
        const auto& c = candles_[start + i];
        const int cx = static_cast<int>((i + 0.5) * slot_w);
        const bool bull = c.close >= c.open;
        const QColor& col = bull ? bull_color : bear_color;
        const QColor& wcol = bull ? wick_bull : wick_bear;

        const int open_y  = py(c.open);
        const int close_y = py(c.close);
        const int high_y  = py(c.high);
        const int low_y   = py(c.low);

        const int body_top = std::min(open_y, close_y);
        const int body_bot = std::max(open_y, close_y);
        const int body_h   = qMax(1, body_bot - body_top);

        // Wick
        p.setPen(QPen(wcol, 1));
        p.drawLine(cx, high_y, cx, body_top);
        p.drawLine(cx, body_bot, cx, low_y);

        // Body
        p.fillRect(cx - half, body_top, body_w, body_h, col);
    }

    // Price axis (right)
    p.setPen(QPen(QColor(ui::colors::BORDER_DIM.get()), 1));
    p.drawLine(plot_w, 0, plot_w, plot_h);

    QFont lbl_font("Consolas", 9);
    p.setFont(lbl_font);
    QFontMetrics fm(lbl_font);
    p.setPen(QColor(ui::colors::TEXT_SECONDARY.get()));

    const bool is_large = hi > 1000;
    for (int g = 0; g <= 6; ++g) {
        double price = lo + (hi - lo) * g / 6.0;
        int gy = py(price);
        QString txt = currency_sym_ + (is_large ? QString::number(price, 'f', 0) : QString::number(price, 'f', 2));
        p.drawText(plot_w + 6, gy + fm.ascent() / 2, txt);
    }

    // Time axis (bottom)
    p.setPen(QPen(QColor(ui::colors::BORDER_DIM.get()), 1));
    p.drawLine(0, plot_h, plot_w, plot_h);

    p.setPen(QColor(ui::colors::TEXT_SECONDARY.get()));
    const qint64 span_sec = candles_.last().timestamp - candles_.first().timestamp;
    const int label_step = qMax(1, count / 8);

    for (int i = 0; i < count; i += label_step) {
        const auto& c = candles_[start + i];
        QDateTime dt = QDateTime::fromSecsSinceEpoch(c.timestamp);
        QString label;
        if (span_sec > 365LL * 86400)
            label = dt.toString("MMM yy");
        else if (span_sec > 60LL * 86400)
            label = dt.toString("dd MMM");
        else
            label = dt.toString("dd MMM");

        int lx = static_cast<int>((i + 0.5) * slot_w);
        int tw = fm.horizontalAdvance(label);
        if (lx - tw / 2 > 0 && lx + tw / 2 < plot_w)
            p.drawText(lx - tw / 2, plot_h + TIME_AXIS_H - 4, label);
    }

    // Last candle close price line (highlight)
    if (!candles_.isEmpty()) {
        double last_close = candles_.last().close;
        int ly = py(last_close);
        p.setPen(QPen(QColor(ui::colors::AMBER.get()), 1, Qt::DashLine));
        p.drawLine(0, ly, plot_w, ly);
    }
}

// ══════════════════════════════════════════════════════════════════════════════
//  EquityOverviewTab
// ══════════════════════════════════════════════════════════════════════════════

EquityOverviewTab::EquityOverviewTab(QWidget* parent) : QWidget(parent) {
    build_ui();

    auto& svc = services::equity::EquityResearchService::instance();
    connect(&svc, &services::equity::EquityResearchService::quote_loaded, this, &EquityOverviewTab::on_quote_loaded);
    connect(&svc, &services::equity::EquityResearchService::info_loaded, this, &EquityOverviewTab::on_info_loaded);
    connect(&svc, &services::equity::EquityResearchService::historical_loaded, this,
            &EquityOverviewTab::on_historical_loaded);
}

void EquityOverviewTab::set_symbol(const QString& symbol) {
    if (symbol == current_symbol_)
        return;
    current_symbol_ = symbol;
    info_loaded_ = quote_loaded_ = historical_loaded_ = false;
    loading_overlay_->show_loading("LOADING OVERVIEW\xe2\x80\xa6");
}

// ── Build UI ──────────────────────────────────────────────────────────────────

void EquityOverviewTab::build_ui() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));

    loading_overlay_ = new ui::LoadingOverlay(this);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("background:transparent;border:0;");
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto* content = new QWidget;
    content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    auto* top = new QHBoxLayout;
    top->setSpacing(6);

    auto* col1 = build_col1();
    col1->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    col1->setFixedWidth(220);

    auto* chart = build_chart_panel();
    chart->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* col4 = build_col4();
    col4->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    col4->setFixedWidth(220);

    top->addWidget(col1);
    top->addWidget(chart, 1);
    top->addWidget(col4);

    vl->addLayout(top, 1);
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
    open_val_ = add_row(p, "OPEN", CYAN);
    high_val_ = add_row(p, "HIGH", ui::colors::POSITIVE);
    low_val_ = add_row(p, "LOW", ui::colors::NEGATIVE);
    prev_close_val_ = add_row(p, "PREV CLOSE", ui::colors::TEXT_PRIMARY);
    vol_val_ = add_row(p, "VOLUME", YELLOW);
    static_cast<QVBoxLayout*>(p->layout())->addStretch();
    return p;
}

QWidget* EquityOverviewTab::build_valuation_panel() {
    auto* p = make_panel("VALUATION", CYAN);
    mktcap_val_ = add_row(p, "MARKET CAP", CYAN);
    pe_val_ = add_row(p, "P/E RATIO", YELLOW);
    fwd_pe_val_ = add_row(p, "FWD P/E", YELLOW);
    peg_val_ = add_row(p, "PEG RATIO", YELLOW);
    pb_val_ = add_row(p, "P/B RATIO", CYAN);
    div_val_ = add_row(p, "DIV YIELD", ui::colors::POSITIVE);
    beta_val_ = add_row(p, "BETA", ui::colors::TEXT_PRIMARY);
    static_cast<QVBoxLayout*>(p->layout())->addStretch();
    return p;
}

QWidget* EquityOverviewTab::build_share_stats_panel() {
    auto* p = make_panel("SHARE STATS", PURPLE);
    shares_out_val_ = add_row(p, "SHARES OUT", CYAN);
    float_val_ = add_row(p, "FLOAT", CYAN);
    insiders_val_ = add_row(p, "INSIDERS", YELLOW);
    institutions_val_ = add_row(p, "INSTITUTIONS", YELLOW);
    short_pct_val_ = add_row(p, "SHORT %", ui::colors::NEGATIVE);
    static_cast<QVBoxLayout*>(p->layout())->addStretch();
    return p;
}

// ── Chart Panel ───────────────────────────────────────────────────────────────

QWidget* EquityOverviewTab::build_chart_panel() {
    auto* p = new QFrame;
    p->setStyleSheet(QString("QFrame{background:%1;border:1px solid %2;border-radius:2px;}").arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));

    auto* vl = new QVBoxLayout(p);
    vl->setContentsMargins(8, 6, 8, 6);
    vl->setSpacing(4);

    // Period buttons row
    auto* btn_row = new QHBoxLayout;
    btn_row->setSpacing(4);
    btn_row->setContentsMargins(0, 0, 0, 0);

    auto* period_lbl = new QLabel("PERIOD");
    period_lbl->setStyleSheet(QString("color:%1;font-size:12px;font-weight:600;background:transparent;border:0;").arg(ui::colors::TEXT_SECONDARY));
    btn_row->addWidget(period_lbl);

    auto btn_style_inactive = QString(
        "QPushButton{background:transparent;color:%1;border:1px solid %2;"
        "border-radius:2px;padding:3px 10px;font-size:12px;font-weight:700;font-family:'Consolas',monospace;}"
        "QPushButton:hover{border-color:%3;background:%4;}")
        .arg(ui::colors::TEXT_SECONDARY, ui::colors::BORDER_DIM, ui::colors::AMBER, ui::colors::BG_RAISED);

    auto btn_style_active = QString(
        "QPushButton{background:%1;color:%2;border:1px solid %1;"
        "border-radius:2px;padding:3px 10px;font-size:12px;font-weight:700;font-family:'Consolas',monospace;}")
        .arg(ui::colors::AMBER, ui::colors::BG_BASE);

    auto make_btn = [&](const QString& label, QPushButton*& out, const QString& period) {
        out = new QPushButton(label);
        out->setCursor(Qt::PointingHandCursor);
        out->setStyleSheet(period == current_period_ ? btn_style_active : btn_style_inactive);
        connect(out, &QPushButton::clicked, this, [this, out, period]() { switch_period(out, period); });
        btn_row->addWidget(out);
    };

    make_btn("1M", btn_1m_, "1mo");
    make_btn("3M", btn_3m_, "3mo");
    make_btn("6M", btn_6m_, "6mo");
    make_btn("1Y", btn_1y_, "1y");
    make_btn("5Y", btn_5y_, "5y");
    active_period_btn_ = btn_1y_;
    btn_row->addStretch();

    vl->addLayout(btn_row);

    // Canvas
    candle_canvas_ = new ResearchCandleCanvas;
    vl->addWidget(candle_canvas_, 1);

    return p;
}

void EquityOverviewTab::switch_period(QPushButton* btn, const QString& period) {
    if (period == current_period_ && historical_loaded_)
        return;
    current_period_ = period;

    // Update button styles
    auto inactive = QString(
        "QPushButton{background:transparent;color:%1;border:1px solid %2;"
        "border-radius:2px;padding:3px 10px;font-size:12px;font-weight:700;font-family:'Consolas',monospace;}"
        "QPushButton:hover{border-color:%3;background:%4;}")
        .arg(ui::colors::TEXT_SECONDARY, ui::colors::BORDER_DIM, ui::colors::AMBER, ui::colors::BG_RAISED);

    auto active = QString(
        "QPushButton{background:%1;color:%2;border:1px solid %1;"
        "border-radius:2px;padding:3px 10px;font-size:12px;font-weight:700;font-family:'Consolas',monospace;}")
        .arg(ui::colors::AMBER, ui::colors::BG_BASE);

    for (auto* b : {btn_1m_, btn_3m_, btn_6m_, btn_1y_, btn_5y_})
        b->setStyleSheet(b == btn ? active : inactive);
    active_period_btn_ = btn;

    // Reload data with new period
    if (!current_symbol_.isEmpty())
        services::equity::EquityResearchService::instance().load_symbol(current_symbol_, period);
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
    auto* p = make_panel("ANALYST TARGETS", MAGENTA);
    target_high_val_ = add_row(p, "HIGH", ui::colors::POSITIVE);
    target_mean_val_ = add_row(p, "MEAN", YELLOW);
    target_low_val_ = add_row(p, "LOW", ui::colors::NEGATIVE);
    analyst_count_val_ = add_row(p, "ANALYSTS", CYAN);

    rec_key_label_ = new QLabel("\xe2\x80\x94");
    rec_key_label_->setAlignment(Qt::AlignCenter);
    rec_key_label_->setStyleSheet(QString("background:%1;color:%2;border-radius:2px;padding:3px 8px;"
                                          "font-size:12px;font-weight:700;").arg(ui::colors::BG_RAISED, ui::colors::TEXT_SECONDARY));
    static_cast<QVBoxLayout*>(p->layout())->addWidget(rec_key_label_);
    static_cast<QVBoxLayout*>(p->layout())->addStretch();
    return p;
}

QWidget* EquityOverviewTab::build_52w_panel() {
    auto* p = make_panel("52 WEEK RANGE", YELLOW);
    w52h_val_ = add_row(p, "HIGH", ui::colors::POSITIVE);
    w52l_val_ = add_row(p, "LOW", ui::colors::NEGATIVE);
    avg_vol_val_ = add_row(p, "AVG VOL", CYAN);
    static_cast<QVBoxLayout*>(p->layout())->addStretch();
    return p;
}

QWidget* EquityOverviewTab::build_profitability_panel() {
    auto* p = make_panel("PROFITABILITY", ui::colors::POSITIVE);
    gross_margin_val_ = add_row(p, "GROSS MARGIN", ui::colors::POSITIVE);
    op_margin_val_ = add_row(p, "OPER. MARGIN", ui::colors::POSITIVE);
    profit_margin_val_ = add_row(p, "PROFIT MARGIN", ui::colors::POSITIVE);
    roa_val_ = add_row(p, "ROA", CYAN);
    roe_val_ = add_row(p, "ROE", CYAN);
    static_cast<QVBoxLayout*>(p->layout())->addStretch();
    return p;
}

QWidget* EquityOverviewTab::build_growth_panel() {
    auto* p = make_panel("GROWTH RATES", BLUE);
    rev_growth_val_ = add_row(p, "REVENUE", ui::colors::POSITIVE);
    earnings_growth_val_ = add_row(p, "EARNINGS", ui::colors::POSITIVE);
    static_cast<QVBoxLayout*>(p->layout())->addStretch();
    return p;
}

// ── Bottom Row ────────────────────────────────────────────────────────────────

QWidget* EquityOverviewTab::build_bottom_row() {
    auto* w = new QWidget;
    auto* hl = new QHBoxLayout(w);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(6);
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
    auto* p = make_panel("COMPANY OVERVIEW", CYAN);
    company_desc_ = new QLabel("\xe2\x80\x94");
    company_desc_->setWordWrap(true);
    company_desc_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    company_desc_->setStyleSheet(QString("color:%1;font-size:%2px;line-height:1.5;"
                                         "background:transparent;border:0;").arg(ui::colors::TEXT_PRIMARY).arg(FONT_DESC));
    company_desc_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    static_cast<QVBoxLayout*>(p->layout())->addWidget(company_desc_);
    return p;
}

QWidget* EquityOverviewTab::build_company_info_panel() {
    auto* p = make_panel("COMPANY INFO", ui::colors::TEXT_PRIMARY);
    company_emp_ = add_row(p, "EMPLOYEES", CYAN);
    company_web_ = add_row(p, "WEBSITE", BLUE);
    company_currency_ = add_row(p, "CURRENCY", CYAN);
    static_cast<QVBoxLayout*>(p->layout())->addStretch();
    return p;
}

QWidget* EquityOverviewTab::build_financial_health_panel() {
    auto* p = make_panel("FINANCIAL HEALTH", ui::colors::AMBER);
    cash_val_ = add_row(p, "CASH", ui::colors::POSITIVE);
    debt_val_ = add_row(p, "DEBT", ui::colors::NEGATIVE);
    free_cf_val_ = add_row(p, "FREE CF", CYAN);
    static_cast<QVBoxLayout*>(p->layout())->addStretch();
    return p;
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void EquityOverviewTab::on_quote_loaded(services::equity::QuoteData q) {
    if (q.symbol != current_symbol_)
        return;
    cached_quote_ = q;
    quote_loaded_ = true;
    if (info_loaded_ && quote_loaded_ && historical_loaded_)
        loading_overlay_->hide_loading();

    open_val_->setText(fmt_price(q.open));
    high_val_->setText(fmt_price(q.high));
    low_val_->setText(fmt_price(q.low));
    prev_close_val_->setText(fmt_price(q.prev_close));
    vol_val_->setText(fmt_large(q.volume));
}

void EquityOverviewTab::on_info_loaded(services::equity::StockInfo info) {
    if (info.symbol != current_symbol_)
        return;
    cached_info_ = info;
    info_loaded_ = true;
    current_currency_ = info.currency;
    if (info_loaded_ && quote_loaded_ && historical_loaded_)
        loading_overlay_->hide_loading();

    // Re-render quote and chart with correct currency
    if (quote_loaded_) {
        open_val_->setText(fmt_price(cached_quote_.open));
        high_val_->setText(fmt_price(cached_quote_.high));
        low_val_->setText(fmt_price(cached_quote_.low));
        prev_close_val_->setText(fmt_price(cached_quote_.prev_close));
    }
    if (historical_loaded_ && !cached_candles_.isEmpty()) {
        candle_canvas_->set_candles(cached_candles_,
                                    currency_symbol(current_currency_.isEmpty() ? "USD" : current_currency_));
    }

    // Valuation
    mktcap_val_->setText(info.market_cap > 0 ? fmt_large(info.market_cap) : "N/A");
    pe_val_->setText(info.pe_ratio > 0 ? QString::number(info.pe_ratio, 'f', 2) : "N/A");
    fwd_pe_val_->setText(info.forward_pe > 0 ? QString::number(info.forward_pe, 'f', 2) : "N/A");
    peg_val_->setText(info.peg_ratio > 0 ? QString::number(info.peg_ratio, 'f', 2) : "N/A");
    pb_val_->setText(info.price_to_book > 0 ? QString::number(info.price_to_book, 'f', 2) : "N/A");
    div_val_->setText(info.dividend_yield > 0 ? fmt_pct(info.dividend_yield) : "N/A");
    beta_val_->setText(info.beta != 0.0 ? QString::number(info.beta, 'f', 2) : "N/A");

    // Share Stats
    shares_out_val_->setText(fmt_large(info.shares_outstanding));
    float_val_->setText(fmt_large(info.float_shares));
    insiders_val_->setText(fmt_pct(info.held_insiders_pct));
    institutions_val_->setText(fmt_pct(info.held_institutions_pct));
    short_pct_val_->setText(fmt_pct(info.short_pct_of_float));

    // 52 Week Range
    w52h_val_->setText(fmt_price(info.week52_high));
    w52l_val_->setText(fmt_price(info.week52_low));
    avg_vol_val_->setText(fmt_large(info.avg_volume));

    // Analyst Targets
    target_high_val_->setText(fmt_price(info.target_high));
    target_mean_val_->setText(fmt_price(info.target_mean));
    target_low_val_->setText(fmt_price(info.target_low));
    analyst_count_val_->setText(info.analyst_count > 0 ? QString::number(info.analyst_count) : "N/A");

    QString rec = info.recommendation_key.toUpper().replace("_", " ");
    const char* rec_color = ui::colors::TEXT_SECONDARY;
    if (rec == "STRONG BUY" || rec == "BUY")
        rec_color = ui::colors::POSITIVE;
    else if (rec == "STRONG SELL" || rec == "SELL")
        rec_color = ui::colors::NEGATIVE;
    else if (rec == "HOLD")
        rec_color = ui::colors::AMBER;
    rec_key_label_->setText(rec.isEmpty() ? "\xe2\x80\x94" : rec);
    rec_key_label_->setStyleSheet(QString("background:%1;color:%2;border-radius:2px;padding:3px 8px;"
                                          "font-size:12px;font-weight:700;").arg(ui::colors::BG_RAISED, rec_color));

    // Profitability
    gross_margin_val_->setText(fmt_pct(info.gross_margins));
    op_margin_val_->setText(fmt_pct(info.operating_margins));
    profit_margin_val_->setText(fmt_pct(info.profit_margins));
    roa_val_->setText(fmt_pct(info.roa));
    roe_val_->setText(fmt_pct(info.roe));

    // Growth
    rev_growth_val_->setText(fmt_pct(info.revenue_growth));
    earnings_growth_val_->setText(fmt_pct(info.earnings_growth));

    // Company Info
    company_desc_->setText(info.description);
    company_emp_->setText(info.employees > 0 ? fmt_large(static_cast<double>(info.employees)) : "N/A");
    company_web_->setText(info.website.left(40));
    company_currency_->setText(info.currency.isEmpty() ? "N/A" : info.currency);

    // Financial Health
    cash_val_->setText(fmt_large(info.total_cash));
    debt_val_->setText(fmt_large(info.total_debt));
    free_cf_val_->setText(fmt_large(info.free_cashflow));
}

void EquityOverviewTab::on_historical_loaded(QString symbol, QVector<services::equity::Candle> candles) {
    if (symbol != current_symbol_)
        return;
    historical_loaded_ = true;
    cached_candles_ = candles;
    if (info_loaded_ && quote_loaded_ && historical_loaded_)
        loading_overlay_->hide_loading();
    rebuild_chart(candles);
}

void EquityOverviewTab::rebuild_chart(const QVector<services::equity::Candle>& candles) {
    const QString cs = currency_symbol(current_currency_.isEmpty() ? "USD" : current_currency_);
    candle_canvas_->set_candles(candles, cs);
}

// ── Formatters ────────────────────────────────────────────────────────────────

QString EquityOverviewTab::fmt_large(double v) {
    bool neg = v < 0;
    double av = qAbs(v);
    QString s;
    if (av >= 1e12)
        s = QString("%1T").arg(av / 1e12, 0, 'f', 2);
    else if (av >= 1e9)
        s = QString("%1B").arg(av / 1e9, 0, 'f', 2);
    else if (av >= 1e6)
        s = QString("%1M").arg(av / 1e6, 0, 'f', 2);
    else if (av >= 1e3)
        s = QString("%1K").arg(av / 1e3, 0, 'f', 1);
    else
        s = QString::number(static_cast<qint64>(av));
    return neg ? "-" + s : s;
}

QString EquityOverviewTab::fmt_pct(double v) {
    return QString("%1%").arg(v * 100.0, 0, 'f', 2);
}

QString EquityOverviewTab::currency_symbol(const QString& currency_code) {
    static const QHash<QString, QString> map = {
        {"USD", "$"},    {"EUR", "\xe2\x82\xac"}, {"GBP", "\xc2\xa3"},    {"JPY", "\xc2\xa5"},
        {"CNY", "\xc2\xa5"}, {"INR", "\xe2\x82\xb9"}, {"KRW", "\xe2\x82\xa9"}, {"RUB", "\xe2\x82\xbd"},
        {"BRL", "R$"},  {"TRY", "\xe2\x82\xba"}, {"CHF", "CHF "},  {"CAD", "C$"},
        {"AUD", "A$"},  {"NZD", "NZ$"}, {"HKD", "HK$"}, {"SGD", "S$"},
        {"TWD", "NT$"}, {"MXN", "MX$"}, {"ZAR", "R"},   {"SEK", "kr"},
        {"NOK", "kr"},  {"DKK", "kr"},  {"PLN", "z\xc5\x82"}, {"THB", "\xe0\xb8\xbf"},
        {"IDR", "Rp"},  {"MYR", "RM"},  {"PHP", "\xe2\x82\xb1"}, {"CZK", "K\xc4\x8d"},
        {"HUF", "Ft"},  {"ILS", "\xe2\x82\xaa"}, {"CLP", "CL$"}, {"COP", "COL$"},
        {"PEN", "S/."},  {"ARS", "AR$"}, {"EGP", "E\xc2\xa3"}, {"NGN", "\xe2\x82\xa6"},
        {"PKR", "Rs"},  {"BDT", "\xe0\xa7\xb3"}, {"VND", "\xe2\x82\xab"}, {"SAR", "SR"},
        {"QAR", "QR"},  {"AED", "AED "}, {"KWD", "KD"},
    };
    const auto it = map.find(currency_code.toUpper());
    return (it != map.end()) ? it.value() : (currency_code + " ");
}

QString EquityOverviewTab::fmt_price(double v) const {
    if (v == 0.0)
        return "\xe2\x80\x94";
    const QString sym = current_currency_.isEmpty() ? "$" : currency_symbol(current_currency_);
    return QString("%1%2").arg(sym).arg(v, 0, 'f', 2);
}

} // namespace fincept::screens
