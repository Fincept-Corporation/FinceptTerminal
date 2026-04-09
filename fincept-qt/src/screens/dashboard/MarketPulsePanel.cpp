#include "screens/dashboard/MarketPulsePanel.h"

#include "services/markets/MarketDataService.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QDateTime>
#include <QFrame>
#include <QPalette>
#include <QShowEvent>

#include <algorithm>

namespace fincept::screens {

// ── Symbols used by this panel ───────────────────────────────────────────────

// Broad basket: used for fear/greed score + NYSE/NASDAQ/S&P breadth proxy
static const QStringList kBreadthSymbols = {
    "^VIX",
    // S&P large caps (proxy for S&P 500 breadth)
    "AAPL",
    "MSFT",
    "GOOGL",
    "AMZN",
    "NVDA",
    "META",
    "TSLA",
    "BRK-B",
    "JPM",
    "UNH",
    "V",
    "XOM",
    "LLY",
    "JNJ",
    "WMT",
    "MA",
    "PG",
    "HD",
    "CVX",
    "MRK",
    // NASDAQ-heavy tech
    "NFLX",
    "AMD",
    "INTC",
    "QCOM",
    "ADBE",
    "CSCO",
    "ORCL",
    "CRM",
    "AVGO",
    "TXN",
    // NYSE diversified (financials, energy, consumer, industrials)
    "GS",
    "BAC",
    "WFC",
    "C",
    "MS",
    "BLK",
    "AXP",
    "CAT",
    "BA",
    "GE",
    "DIS",
    "NKE",
    "KO",
    "PEP",
    "MCD",
    "PFE",
    "ABT",
    "TMO",
    "UPS",
    "FDX",
};

// Movers: used for gainers/losers rows
static const QStringList kMoverSymbols = services::MarketDataService::mover_symbols();

// Global snapshot symbols
static const QStringList kSnapshotSymbols = services::MarketDataService::global_snapshot_symbols();

// ── Constructor ──────────────────────────────────────────────────────────────

MarketPulsePanel::MarketPulsePanel(QWidget* parent) : QWidget(parent) {
    setMinimumWidth(180);
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(true);
    // Constrain preferred width so the QSplitter doesn't give this panel
    // the majority of space before the user sees the dashboard.
    // The user can still drag the splitter handle wider if needed.
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_header());

    scroll_area_ = new QScrollArea;
    scroll_area_->setWidgetResizable(true);
    // Styling applied by refresh_theme() called at end of constructor

    auto* content = new QWidget(this);
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(0);

    cl->addWidget(build_fear_greed_section());
    cl->addWidget(build_breadth_section());
    cl->addWidget(build_gainers_section());
    cl->addWidget(build_losers_section());
    cl->addWidget(build_global_snapshot_section());
    cl->addWidget(build_market_hours_section());
    cl->addStretch();

    scroll_area_->setWidget(content);
    vl->addWidget(scroll_area_, 1);

    // ── Timers ──
    refresh_timer_ = new QTimer(this);
    refresh_timer_->setInterval(600000); // 10 min
    connect(refresh_timer_, &QTimer::timeout, this, &MarketPulsePanel::refresh_data);

    hours_timer_ = new QTimer(this);
    hours_timer_->setInterval(60000); // 1 min — market open/close status
    connect(hours_timer_, &QTimer::timeout, this, &MarketPulsePanel::refresh_market_hours);

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { refresh_theme(); });
    refresh_theme();
}

// ── Theme refresh ────────────────────────────────────────────────────────────

void MarketPulsePanel::refresh_theme() {
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(ui::colors::PANEL()));
    pal.setColor(QPalette::Base, QColor(ui::colors::PANEL()));
    setPalette(pal);

    // ── Root panel ──
    setStyleSheet(QString("background:%1;").arg(ui::colors::PANEL()));

    // ── Header bar ──
    if (header_bar_)
        header_bar_->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;")
                                       .arg(ui::colors::BG_RAISED(), ui::colors::AMBER_DIM()));
    if (header_icon_)
        header_icon_->setStyleSheet(
            QString("color: %1; font-size: 12px; background: transparent;").arg(ui::colors::AMBER()));
    if (header_title_)
        header_title_->setStyleSheet(
            QString("color: %1; font-size: 10px; font-weight: bold; letter-spacing: 1px; background: transparent;")
                .arg(ui::colors::AMBER()));
    if (header_live_dot_)
        header_live_dot_->setStyleSheet(QString("background: %1; border-radius: 3px;").arg(ui::colors::POSITIVE()));

    // ── Scroll area ──
    if (scroll_area_)
        scroll_area_->setStyleSheet(
            QString("QScrollArea{border:none;background:transparent;}"
                    "QScrollBar:vertical{width:6px;background:transparent;}"
                    "QScrollBar::handle:vertical{background:%1;border-radius:3px;min-height:20px;}"
                    "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
                .arg(ui::colors::BORDER_MED()));

    // ── Section headers ──
    auto style_section = [](SectionHeader& sh, const QString& icon_color) {
        if (sh.container)
            sh.container->setStyleSheet(
                QString("background: %1; border-bottom: 1px solid %2; border-top: 1px solid %2;")
                    .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
        if (sh.icon)
            sh.icon->setStyleSheet(QString("color: %1; font-size: 10px; background: transparent;").arg(icon_color));
        if (sh.title)
            sh.title->setStyleSheet(
                QString("color: %1; font-size: 9px; font-weight: bold; letter-spacing: 0.5px; background: transparent;")
                    .arg(ui::colors::TEXT_SECONDARY()));
    };

    style_section(sh_breadth_, ui::colors::CYAN());
    style_section(sh_gainers_, ui::colors::POSITIVE());
    style_section(sh_losers_, ui::colors::NEGATIVE());
    style_section(sh_snapshot_, ui::colors::INFO());
    style_section(sh_hours_, ui::colors::WARNING());

    // ── Fear & Greed ──
    if (fg_header_label_)
        fg_header_label_->setStyleSheet(
            QString("color: %1; font-size: 9px; font-weight: bold; letter-spacing: 0.5px; background: transparent;")
                .arg(ui::colors::TEXT_SECONDARY()));
    if (fg_gauge_icon_)
        fg_gauge_icon_->setStyleSheet(
            QString("color: %1; font-size: 12px; background: transparent;").arg(ui::colors::AMBER()));
    if (fg_gradient_bar_)
        fg_gradient_bar_->setStyleSheet(
            QString("QFrame { border-radius: 3px; "
                    "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
                    "stop:0 %1, stop:0.25 %2, stop:0.5 %3, stop:0.75 %4, stop:1 %5); }")
                .arg(ui::colors::NEGATIVE(), ui::colors::AMBER(), ui::colors::WARNING(),
                     ui::colors::POSITIVE_DIM(), ui::colors::POSITIVE()));
    if (fg_score_val_)
        fg_score_val_->setStyleSheet(QString("color: %1; font-size: 18px; font-weight: bold; background: transparent;")
                                         .arg(ui::colors::TEXT_TERTIARY()));
    if (fg_score_max_)
        fg_score_max_->setStyleSheet(
            QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::TEXT_TERTIARY()));
    if (fg_sentiment_)
        fg_sentiment_->setStyleSheet(
            QString("color: %1; font-size: 9px; font-weight: bold; letter-spacing: 0.5px; background: transparent;")
                .arg(ui::colors::TEXT_TERTIARY()));

    // ── Market Breadth rows ──
    auto style_breadth = [](BreadthRow& row) {
        if (row.name)
            row.name->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;")
                                        .arg(ui::colors::TEXT_SECONDARY()));
        if (row.adv)
            row.adv->setStyleSheet(
                QString("color: %1; font-size: 8px; background: transparent;").arg(ui::colors::POSITIVE()));
        if (row.slash)
            row.slash->setStyleSheet(
                QString("color: %1; font-size: 8px; background: transparent;").arg(ui::colors::TEXT_TERTIARY()));
        if (row.dec)
            row.dec->setStyleSheet(
                QString("color: %1; font-size: 8px; background: transparent;").arg(ui::colors::NEGATIVE()));
        if (row.green)
            row.green->setStyleSheet(QString("background: %1; border-radius: 0;").arg(ui::colors::POSITIVE()));
        if (row.red)
            row.red->setStyleSheet(QString("background: %1; border-radius: 0;").arg(ui::colors::NEGATIVE()));
    };

    style_breadth(nyse_row_);
    style_breadth(nasdaq_row_);
    style_breadth(sp500_row_);

    // ── Global Snapshot rows ──
    // Each stat row has a fixed val_color that maps to a specific token.
    // Re-resolve them here so theme changes take effect.
    struct StatDef {
        StatRow& row;
        QString val_color;
    };
    StatDef stat_defs[] = {
        {vix_row_, ui::colors::WARNING()},  {us10y_row_, ui::colors::CYAN()}, {dxy_row_, ui::colors::CYAN()},
        {gold_row_, ui::colors::WARNING()}, {oil_row_, ui::colors::CYAN()},   {btc_row_, ui::colors::AMBER()},
    };

    for (auto& sd : stat_defs) {
        if (sd.row.container)
            sd.row.container->setStyleSheet(QString("border-bottom: 1px solid %1;").arg(ui::colors::BORDER_DIM()));
        if (sd.row.name_lbl)
            sd.row.name_lbl->setStyleSheet(
                QString("color: %1; font-size: 9px; font-weight: bold; letter-spacing: 0.3px; background: transparent;")
                    .arg(ui::colors::TEXT_SECONDARY()));
        if (sd.row.val)
            sd.row.val->setStyleSheet(
                QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;").arg(sd.val_color));
        if (sd.row.chg)
            sd.row.chg->setStyleSheet(QString("color: %1; font-size: 8px; font-weight: bold; background: transparent;")
                                          .arg(ui::colors::TEXT_TERTIARY()));
    }

    // ── Market Hours rows ──
    for (auto& hr : hours_rows_) {
        if (hr.container)
            hr.container->setStyleSheet(QString("border-bottom: 1px solid %1;").arg(ui::colors::BORDER_DIM()));
        if (hr.name_lbl)
            hr.name_lbl->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;")
                                           .arg(ui::colors::TEXT_SECONDARY()));
        // dot and status colors are data-driven (open/closed/pre) —
        // refresh_market_hours() handles those, call it to re-resolve tokens.
    }

    // Re-resolve data-driven status dot/label colors.
    refresh_market_hours();

    // Mover rows (gainers/losers) are fully rebuilt by refresh_data().
    // Trigger a refresh so they pick up new theme colors.
    if (isVisible())
        refresh_data();
}

void MarketPulsePanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    refresh_theme();
    refresh_data();
    refresh_market_hours();
    refresh_timer_->start();
    hours_timer_->start();
}

void MarketPulsePanel::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    refresh_timer_->stop();
    hours_timer_->stop();
}

// ── Header ───────────────────────────────────────────────────────────────────

QWidget* MarketPulsePanel::build_header() {
    header_bar_ = new QWidget(this);
    header_bar_->setFixedHeight(30);
    // Styling applied by refresh_theme()

    auto* hl = new QHBoxLayout(header_bar_);
    hl->setContentsMargins(12, 0, 12, 0);

    header_icon_ = new QLabel(QChar(0x25C8));
    hl->addWidget(header_icon_);

    header_title_ = new QLabel("MARKET PULSE");
    hl->addWidget(header_title_);
    hl->addStretch();

    header_live_dot_ = new QLabel;
    header_live_dot_->setFixedSize(6, 6);
    hl->addWidget(header_live_dot_);

    return header_bar_;
}

QWidget* MarketPulsePanel::build_section_header(const QString& title, const QString& icon_char, const QString& color) {
    Q_UNUSED(color)
    auto* w = new QWidget(this);
    w->setFixedHeight(26);
    // Styling applied by refresh_theme() via style_section lambda

    auto* hl = new QHBoxLayout(w);
    hl->setContentsMargins(12, 0, 12, 0);

    auto* icn = new QLabel(icon_char);
    hl->addWidget(icn);

    auto* lbl = new QLabel(title);
    hl->addWidget(lbl);
    hl->addStretch();

    // Store pointers into the corresponding SectionHeader member
    // so refresh_theme() can re-apply styles later.
    SectionHeader* sh = nullptr;
    if (title == "MARKET BREADTH")
        sh = &sh_breadth_;
    else if (title == "TOP GAINERS")
        sh = &sh_gainers_;
    else if (title == "TOP LOSERS")
        sh = &sh_losers_;
    else if (title == "GLOBAL SNAPSHOT")
        sh = &sh_snapshot_;
    else if (title == "MARKET HOURS")
        sh = &sh_hours_;

    if (sh) {
        sh->container = w;
        sh->icon = icn;
        sh->title = lbl;
    }

    return w;
}

// ── Fear & Greed ─────────────────────────────────────────────────────────────

QWidget* MarketPulsePanel::build_fear_greed_section() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(12, 8, 12, 8);
    vl->setSpacing(6);

    // Header row
    auto* header_row = new QWidget(this);
    auto* hrl = new QHBoxLayout(header_row);
    hrl->setContentsMargins(0, 0, 0, 0);

    fg_header_label_ = new QLabel("FEAR & GREED INDEX");
    hrl->addWidget(fg_header_label_);
    hrl->addStretch();

    fg_gauge_icon_ = new QLabel(QChar(0x25CE));
    hrl->addWidget(fg_gauge_icon_);

    vl->addWidget(header_row);

    // Gradient bar (red→green, themed)
    fg_gradient_bar_ = new QFrame;
    fg_gradient_bar_->setFixedHeight(6);
    vl->addWidget(fg_gradient_bar_);

    // Score row
    auto* score_row = new QWidget(this);
    auto* srl = new QHBoxLayout(score_row);
    srl->setContentsMargins(0, 0, 0, 0);

    fg_score_val_ = new QLabel("--");
    srl->addWidget(fg_score_val_);

    fg_score_max_ = new QLabel("/100");
    srl->addWidget(fg_score_max_);

    srl->addStretch();

    fg_sentiment_ = new QLabel("LOADING...");
    srl->addWidget(fg_sentiment_);
    // All styling applied by refresh_theme()

    vl->addWidget(score_row);
    return w;
}

// ── Market Breadth ────────────────────────────────────────────────────────────

QWidget* MarketPulsePanel::build_breadth_section() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_section_header("MARKET BREADTH", QChar(0x2593), ui::colors::CYAN()));

    auto* bars = new QWidget(this);
    auto* bl = new QVBoxLayout(bars);
    bl->setContentsMargins(0, 6, 0, 6);
    bl->setSpacing(0);

    auto make_row = [&](const QString& name, BreadthRow& row) {
        auto* rw = new QWidget(this);
        auto* rl = new QVBoxLayout(rw);
        rl->setContentsMargins(12, 4, 12, 4);
        rl->setSpacing(3);

        auto* top = new QWidget(this);
        auto* tl = new QHBoxLayout(top);
        tl->setContentsMargins(0, 0, 0, 0);

        row.name = new QLabel(name);
        tl->addWidget(row.name);
        tl->addStretch();

        row.adv = new QLabel("--");
        tl->addWidget(row.adv);

        row.slash = new QLabel("/");
        tl->addWidget(row.slash);

        row.dec = new QLabel("--");
        tl->addWidget(row.dec);
        // All label styling applied by refresh_theme() via style_breadth lambda

        rl->addWidget(top);

        auto* bar_container = new QWidget(this);
        bar_container->setFixedHeight(4);
        auto* bar_layout = new QHBoxLayout(bar_container);
        bar_layout->setContentsMargins(0, 0, 0, 0);
        bar_layout->setSpacing(0);

        auto* green = new QFrame;
        bar_layout->addWidget(green, 1);
        row.green = green;

        auto* red = new QFrame;
        bar_layout->addWidget(red, 1);
        row.red = red;
        // Bar styling applied by refresh_theme() via style_breadth lambda

        rl->addWidget(bar_container);
        bl->addWidget(rw);
    };

    make_row("NYSE", nyse_row_);
    make_row("NASDAQ", nasdaq_row_);
    make_row("S&P 500", sp500_row_);

    vl->addWidget(bars);
    return w;
}

// ── Top Movers ────────────────────────────────────────────────────────────────

QWidget* MarketPulsePanel::build_mover_row(const QString& symbol, double change, const QString& volume) {
    auto* w = new QWidget(this);
    w->setStyleSheet(QString("border-bottom: 1px solid %1;").arg(ui::colors::BORDER_DIM()));

    auto* hl = new QHBoxLayout(w);
    hl->setContentsMargins(12, 5, 12, 5);
    hl->setSpacing(4);

    auto* sym = new QLabel(symbol);
    sym->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;")
                           .arg(ui::colors::TEXT_PRIMARY()));
    hl->addWidget(sym);
    hl->addStretch();

    bool positive = change >= 0;
    auto* arrow = new QLabel(positive ? QChar(0x25B2) : QChar(0x25BC));
    arrow->setStyleSheet(QString("color: %1; font-size: 8px; background: transparent;")
                             .arg(positive ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
    hl->addWidget(arrow);

    auto* chg = new QLabel(QString("%1%2%").arg(positive ? "+" : "").arg(change, 0, 'f', 2));
    chg->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;")
                           .arg(positive ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
    hl->addWidget(chg);

    if (!volume.isEmpty()) {
        auto* vol = new QLabel(QString("VOL: %1").arg(volume));
        vol->setStyleSheet(
            QString("color: %1; font-size: 8px; background: transparent;").arg(ui::colors::TEXT_TERTIARY()));
        hl->addWidget(vol);
    }

    return w;
}

static QString format_volume(double vol) {
    if (vol >= 1e9)
        return QString("%1B").arg(vol / 1e9, 0, 'f', 1);
    if (vol >= 1e6)
        return QString("%1M").arg(vol / 1e6, 0, 'f', 1);
    if (vol >= 1e3)
        return QString("%1K").arg(vol / 1e3, 0, 'f', 1);
    return QString::number(static_cast<int>(vol));
}

QWidget* MarketPulsePanel::build_gainers_section() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_section_header("TOP GAINERS", QChar(0x2191), ui::colors::POSITIVE()));

    auto* rows_w = new QWidget(this);
    gainers_layout_ = new QVBoxLayout(rows_w);
    gainers_layout_->setContentsMargins(0, 0, 0, 0);
    gainers_layout_->setSpacing(0);

    // Placeholder rows while loading
    for (int i = 0; i < 3; ++i)
        gainers_layout_->addWidget(build_mover_row("...", 0.0, ""));

    vl->addWidget(rows_w);
    return w;
}

QWidget* MarketPulsePanel::build_losers_section() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_section_header("TOP LOSERS", QChar(0x2193), ui::colors::NEGATIVE()));

    auto* rows_w = new QWidget(this);
    losers_layout_ = new QVBoxLayout(rows_w);
    losers_layout_->setContentsMargins(0, 0, 0, 0);
    losers_layout_->setSpacing(0);

    for (int i = 0; i < 3; ++i)
        losers_layout_->addWidget(build_mover_row("...", 0.0, ""));

    vl->addWidget(rows_w);
    return w;
}

// ── Global Snapshot ───────────────────────────────────────────────────────────

QWidget* MarketPulsePanel::build_stat_row(const QString& label, const QString& value, const QString& change,
                                          const QString& color) {
    auto* w = new QWidget(this);
    w->setStyleSheet(QString("border-bottom: 1px solid %1;").arg(ui::colors::BORDER_DIM()));

    auto* hl = new QHBoxLayout(w);
    hl->setContentsMargins(12, 4, 12, 4);

    auto* lbl = new QLabel(label);
    lbl->setStyleSheet(
        QString("color: %1; font-size: 9px; font-weight: bold; letter-spacing: 0.3px; background: transparent;")
            .arg(ui::colors::TEXT_SECONDARY()));
    hl->addWidget(lbl);
    hl->addStretch();

    auto* val = new QLabel(value);
    val->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;").arg(color));
    hl->addWidget(val);

    if (!change.isEmpty()) {
        bool positive = change.startsWith('+');
        auto* chg = new QLabel(change);
        chg->setStyleSheet(QString("color: %1; font-size: 8px; font-weight: bold; background: transparent;")
                               .arg(positive                 ? ui::colors::POSITIVE()
                                    : change.startsWith('-') ? ui::colors::NEGATIVE()
                                                             : ui::colors::TEXT_TERTIARY()));
        hl->addWidget(chg);
    }

    return w;
}

QWidget* MarketPulsePanel::build_global_snapshot_section() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_section_header("GLOBAL SNAPSHOT", QChar(0x25CB), ui::colors::INFO()));

    // Build stat rows — colors are applied by refresh_theme()
    struct RowDef {
        const char* label;
        StatRow& row;
    };
    RowDef defs[] = {
        {"VIX", vix_row_},     {"US 10Y", us10y_row_}, {"DXY", dxy_row_},
        {"GOLD", gold_row_},   {"OIL WTI", oil_row_},  {"BTC", btc_row_},
    };

    for (auto& d : defs) {
        auto* rw = new QWidget(this);
        auto* hl = new QHBoxLayout(rw);
        hl->setContentsMargins(12, 4, 12, 4);

        auto* lbl = new QLabel(d.label);
        hl->addWidget(lbl);
        hl->addStretch();

        d.row.container = rw;
        d.row.name_lbl = lbl;

        d.row.val = new QLabel("--");
        hl->addWidget(d.row.val);

        d.row.chg = new QLabel("");
        hl->addWidget(d.row.chg);
        // All styling applied by refresh_theme()

        vl->addWidget(rw);
    }

    return w;
}

// ── Market Hours ─────────────────────────────────────────────────────────────

QWidget* MarketPulsePanel::build_market_hours_section() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_section_header("MARKET HOURS", QChar(0x26A1), ui::colors::WARNING()));

    auto* content = new QWidget(this);
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(12, 6, 12, 6);
    cl->setSpacing(0);

    struct ExDef {
        const char* name;
        const char* region;
    };
    ExDef exchanges[] = {
        {"NYSE/NASDAQ", "US"}, {"LSE", "UK"}, {"TSE (TOKYO)", "JP"}, {"SSE (SHANGHAI)", "CN"}, {"NSE (INDIA)", "IN"},
    };

    for (auto& ex : exchanges) {
        auto* row = new QWidget(this);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 3, 0, 3);

        auto* name = new QLabel(ex.name);
        rl->addWidget(name);
        rl->addStretch();

        HoursRow hr;
        hr.container = row;
        hr.name_lbl = name;
        hr.region = ex.region;

        hr.dot = new QLabel;
        hr.dot->setFixedSize(5, 5);
        rl->addWidget(hr.dot);

        hr.status = new QLabel;
        rl->addWidget(hr.status);
        // All styling applied by refresh_theme() + refresh_market_hours()

        hours_rows_.append(hr);
        cl->addWidget(row);
    }

    vl->addWidget(content);
    return w;
}

// ── Market status helper ─────────────────────────────────────────────────────

QString MarketPulsePanel::market_status(const QString& region) {
    auto now = QDateTime::currentDateTimeUtc();
    int hour = now.time().hour();
    int day = now.date().dayOfWeek(); // 1=Mon, 7=Sun

    if (day >= 6)
        return "CLOSED";

    if (region == "US") {
        if (hour >= 13 && hour < 14)
            return "PRE";
        if (hour >= 14 && hour < 21)
            return "OPEN";
    } else if (region == "UK") {
        if (hour >= 7 && hour < 8)
            return "PRE";
        if (hour >= 8 && hour < 17)
            return "OPEN";
    } else if (region == "JP") {
        if (hour >= 0 && hour < 6)
            return "OPEN";
    } else if (region == "CN") {
        if (hour >= 1 && hour < 7)
            return "OPEN";
    } else if (region == "IN") {
        if (hour >= 3 && hour < 10)
            return "OPEN";
    }
    return "CLOSED";
}

// ── Refresh ───────────────────────────────────────────────────────────────────

void MarketPulsePanel::refresh_market_hours() {
    for (auto& hr : hours_rows_) {
        QString status = market_status(hr.region);
        QString color = (status == "OPEN")  ? ui::colors::POSITIVE()
                        : (status == "PRE") ? ui::colors::WARNING()
                                            : ui::colors::NEGATIVE();
        hr.dot->setStyleSheet(QString("background: %1; border-radius: 2px;").arg(color));
        hr.status->setText(status);
        hr.status->setStyleSheet(
            QString("color: %1; font-size: 8px; font-weight: bold; background: transparent;").arg(color));
    }
}

void MarketPulsePanel::refresh_data() {
    // ── 1. Breadth + Fear/Greed basket ────────────────────────────────────────
    QPointer<MarketPulsePanel> self = this;
    services::MarketDataService::instance().fetch_quotes(kBreadthSymbols, [self](bool ok,
                                                                                 QVector<services::QuoteData> quotes) {
        if (!self || !ok || quotes.isEmpty())
            return;

        // Classify stocks
        // We'll partition the basket into 3 groups mirroring real exchange composition:
        // NYSE-heavy = financials/energy/consumer/industrials (last 20 in kBreadthSymbols)
        // NASDAQ-heavy = tech (middle 10)
        // S&P 500 proxy = first 10 large-caps
        int sp500_adv = 0, sp500_dec = 0;
        int nasdaq_adv = 0, nasdaq_dec = 0;
        int nyse_adv = 0, nyse_dec = 0;

        double vix = -1;
        int bullish = 0, bearish = 0, neutral_count = 0;

        for (const auto& q : quotes) {
            if (q.symbol == "^VIX") {
                vix = q.price;
                continue;
            }

            // S&P 500 proxy: AAPL..MRK (indices 1-10 in kBreadthSymbols, 0=^VIX)
            const QStringList sp500_set = {"AAPL",  "MSFT", "GOOGL", "AMZN", "NVDA", "META", "TSLA",
                                           "BRK-B", "JPM",  "UNH",   "V",    "XOM",  "LLY",  "JNJ",
                                           "WMT",   "MA",   "PG",    "HD",   "CVX",  "MRK"};
            const QStringList nasdaq_set = {"NFLX", "AMD",  "INTC", "QCOM", "ADBE",
                                            "CSCO", "ORCL", "CRM",  "AVGO", "TXN"};

            bool in_sp = sp500_set.contains(q.symbol);
            bool in_nq = nasdaq_set.contains(q.symbol);
            // rest is NYSE proxy

            if (q.change_pct > 0.3) {
                if (in_sp)
                    ++sp500_adv;
                else if (in_nq)
                    ++nasdaq_adv;
                else
                    ++nyse_adv;
                ++bullish;
            } else if (q.change_pct < -0.3) {
                if (in_sp)
                    ++sp500_dec;
                else if (in_nq)
                    ++nasdaq_dec;
                else
                    ++nyse_dec;
                ++bearish;
            } else {
                ++neutral_count;
            }
        }

        // ── Update breadth bars ──
        auto update_row = [](MarketPulsePanel::BreadthRow& row, int adv, int dec) {
            if (!row.adv)
                return;
            row.adv->setText(QString::number(adv));
            row.dec->setText(QString::number(dec));
            int total = adv + dec;
            if (total == 0)
                total = 1;
            int adv_pct = static_cast<int>((double(adv) / total) * 100);
            auto* layout = qobject_cast<QHBoxLayout*>(row.green->parentWidget()->layout());
            if (layout) {
                layout->setStretch(0, adv_pct);
                layout->setStretch(1, 100 - adv_pct);
            }
        };

        update_row(self->nyse_row_, nyse_adv, nyse_dec);
        update_row(self->nasdaq_row_, nasdaq_adv, nasdaq_dec);
        update_row(self->sp500_row_, sp500_adv, sp500_dec);

        // ── Fear & Greed score (0-100 scale) ──
        int total_stocks = bullish + bearish + neutral_count;
        if (total_stocks == 0)
            total_stocks = 1;
        // Start at 50 (neutral), scale by breadth
        int score = 50 + static_cast<int>(((bullish - bearish) / static_cast<double>(total_stocks)) * 50);

        if (vix > 0) {
            if (vix > 30)
                score -= 20;
            else if (vix > 25)
                score -= 10;
            else if (vix < 15)
                score += 10;
        }
        score = qBound(0, score, 100);

        QString sentiment_text, sentiment_color;
        if (score <= 20) {
            sentiment_text = "EXTREME FEAR";
            sentiment_color = ui::colors::NEGATIVE();
        } else if (score <= 40) {
            sentiment_text = "FEAR";
            sentiment_color = ui::colors::WARNING();
        } else if (score <= 60) {
            sentiment_text = "NEUTRAL";
            sentiment_color = ui::colors::WARNING();
        } else if (score <= 80) {
            sentiment_text = "GREED";
            sentiment_color = ui::colors::POSITIVE();
        } else {
            sentiment_text = "EXTREME GREED";
            sentiment_color = ui::colors::POSITIVE();
        }

        if (self->fg_score_val_) {
            self->fg_score_val_->setText(QString::number(score));
            self->fg_score_val_->setStyleSheet(
                QString("color: %1; font-size: 18px; font-weight: bold; background: transparent;")
                    .arg(sentiment_color));
        }
        if (self->fg_sentiment_) {
            self->fg_sentiment_->setText(sentiment_text);
            self->fg_sentiment_->setStyleSheet(
                QString("color: %1; font-size: 9px; font-weight: bold; letter-spacing: 0.5px; background: transparent;")
                    .arg(sentiment_color));
        }
    });

    // ── 2. Top Movers ─────────────────────────────────────────────────────────
    services::MarketDataService::instance().fetch_quotes(kMoverSymbols, [self](bool ok,
                                                                               QVector<services::QuoteData> quotes) {
        if (!self || !ok || quotes.isEmpty())
            return;

        // Sort by change_pct descending
        std::sort(quotes.begin(), quotes.end(),
                  [](const auto& a, const auto& b) { return a.change_pct > b.change_pct; });

        // Clear old rows
        auto clear_layout = [](QVBoxLayout* layout) {
            while (layout->count()) {
                auto* item = layout->takeAt(0);
                if (item->widget())
                    item->widget()->deleteLater();
                delete item;
            }
        };
        clear_layout(self->gainers_layout_);
        clear_layout(self->losers_layout_);

        // Top 3 gainers
        int gainers_added = 0;
        for (const auto& q : quotes) {
            if (q.change_pct <= 0 || gainers_added >= 3)
                break;
            self->gainers_layout_->addWidget(self->build_mover_row(q.symbol, q.change_pct, format_volume(q.volume)));
            ++gainers_added;
        }

        // Top 3 losers (worst first)
        int losers_added = 0;
        for (int i = quotes.size() - 1; i >= 0 && losers_added < 3; --i) {
            if (quotes[i].change_pct >= 0)
                continue;
            self->losers_layout_->addWidget(
                self->build_mover_row(quotes[i].symbol, quotes[i].change_pct, format_volume(quotes[i].volume)));
            ++losers_added;
        }
    });

    // ── 3. Global Snapshot ────────────────────────────────────────────────────
    services::MarketDataService::instance().fetch_quotes(
        kSnapshotSymbols, [self](bool ok, QVector<services::QuoteData> quotes) {
            if (!self || !ok || quotes.isEmpty())
                return;

            // Map symbol -> quote for quick lookup
            QHash<QString, services::QuoteData> qmap;
            for (const auto& q : quotes)
                qmap[q.symbol] = q;

            // Helper to format price display
            auto fmt_price = [](const services::QuoteData& q) -> QString {
                if (q.price >= 1000)
                    return QString("$%1K").arg(q.price / 1000.0, 0, 'f', 1);
                if (q.price >= 100)
                    return QString("%1").arg(q.price, 0, 'f', 2);
                return QString("%1").arg(q.price, 0, 'f', 2);
            };

            auto fmt_chg = [](const services::QuoteData& q) -> QString {
                return QString("%1%2%").arg(q.change_pct >= 0 ? "+" : "").arg(q.change_pct, 0, 'f', 2);
            };

            auto update_stat = [&](const QString& sym, StatRow& row) {
                if (!qmap.contains(sym) || !row.val)
                    return;
                const auto& q = qmap[sym];
                row.val->setText(fmt_price(q));
                QString chg_text = fmt_chg(q);
                row.chg->setText(chg_text);
                QString chg_color = q.change_pct >= 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE();
                row.chg->setStyleSheet(
                    QString("color: %1; font-size: 8px; font-weight: bold; background: transparent;").arg(chg_color));
            };

            // global_snapshot_symbols() returns: ^VIX, ^TNX, DX-Y.NYB, GC=F, CL=F, BTC-USD
            update_stat("^VIX", self->vix_row_);
            update_stat("^TNX", self->us10y_row_);
            update_stat("DX-Y.NYB", self->dxy_row_);
            update_stat("GC=F", self->gold_row_);
            update_stat("CL=F", self->oil_row_);
            update_stat("BTC-USD", self->btc_row_);
        });
}

} // namespace fincept::screens

