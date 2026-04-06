#include "screens/dashboard/MarketPulsePanel.h"

#include "services/markets/MarketDataService.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QDateTime>
#include <QFrame>
#include <QShowEvent>
#include <algorithm>

namespace fincept::screens {

// ── Symbols used by this panel ───────────────────────────────────────────────

// Broad basket: used for fear/greed score + NYSE/NASDAQ/S&P breadth proxy
static const QStringList kBreadthSymbols = {
    "^VIX",
    // S&P large caps (proxy for S&P 500 breadth)
    "AAPL", "MSFT", "GOOGL", "AMZN", "NVDA", "META", "TSLA", "BRK-B", "JPM", "UNH",
    "V",    "XOM",  "LLY",   "JNJ",  "WMT",  "MA",   "PG",   "HD",    "CVX", "MRK",
    // NASDAQ-heavy tech
    "NFLX", "AMD",  "INTC",  "QCOM", "ADBE", "CSCO", "ORCL", "CRM",   "AVGO","TXN",
    // NYSE diversified (financials, energy, consumer, industrials)
    "GS",   "BAC",  "WFC",   "C",    "MS",   "BLK",  "AXP",  "CAT",   "BA",  "GE",
    "DIS",  "NKE",  "KO",    "PEP",  "MCD",  "PFE",  "ABT",  "TMO",   "UPS", "FDX",
};

// Movers: used for gainers/losers rows
static const QStringList kMoverSymbols = services::MarketDataService::mover_symbols();

// Global snapshot symbols
static const QStringList kSnapshotSymbols = services::MarketDataService::global_snapshot_symbols();

// ── Constructor ──────────────────────────────────────────────────────────────

MarketPulsePanel::MarketPulsePanel(QWidget* parent) : QWidget(parent) {
    setMinimumWidth(260);

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_header());

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(
        QString("QScrollArea{border:none;background:transparent;}"
                "QScrollBar:vertical{width:6px;background:transparent;}"
                "QScrollBar::handle:vertical{background:%1;border-radius:3px;min-height:20px;}"
                "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
        .arg(ui::colors::BORDER_MED()));

    auto* content = new QWidget;
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

    scroll->setWidget(content);
    vl->addWidget(scroll, 1);

    // ── Timers ──
    refresh_timer_ = new QTimer(this);
    refresh_timer_->setInterval(600000); // 10 min
    connect(refresh_timer_, &QTimer::timeout, this, &MarketPulsePanel::refresh_data);

    hours_timer_ = new QTimer(this);
    hours_timer_->setInterval(60000); // 1 min — market open/close status
    connect(hours_timer_, &QTimer::timeout, this, &MarketPulsePanel::refresh_market_hours);

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed,
            this, [this](const ui::ThemeTokens&) {
                setStyleSheet(QString("background:%1;").arg(ui::colors::PANEL()));
            });
    setStyleSheet(QString("background:%1;").arg(ui::colors::PANEL()));
}

void MarketPulsePanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
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
    auto* bar = new QWidget;
    bar->setFixedHeight(30);
    bar->setStyleSheet(
        QString("background: %1; border-bottom: 1px solid rgba(217,119,6,0.25);").arg(ui::colors::BG_RAISED()));

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 12, 0);

    auto* icon = new QLabel(QChar(0x25C8));
    icon->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(ui::colors::AMBER()));
    hl->addWidget(icon);

    auto* title = new QLabel("MARKET PULSE");
    title->setStyleSheet(
        QString("color: %1; font-size: 10px; font-weight: bold; letter-spacing: 1px; background: transparent;")
            .arg(ui::colors::AMBER()));
    hl->addWidget(title);
    hl->addStretch();

    auto* live_dot = new QLabel;
    live_dot->setFixedSize(6, 6);
    live_dot->setStyleSheet(QString("background: %1; border-radius: 3px;").arg(ui::colors::POSITIVE()));
    hl->addWidget(live_dot);

    return bar;
}

QWidget* MarketPulsePanel::build_section_header(const QString& title, const QString& icon_char,
                                                  const QString& color) {
    auto* w = new QWidget;
    w->setFixedHeight(26);
    w->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2; border-top: 1px solid %2;")
                         .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));

    auto* hl = new QHBoxLayout(w);
    hl->setContentsMargins(12, 0, 12, 0);

    auto* icn = new QLabel(icon_char);
    icn->setStyleSheet(QString("color: %1; font-size: 10px; background: transparent;").arg(color));
    hl->addWidget(icn);

    auto* lbl = new QLabel(title);
    lbl->setStyleSheet(
        QString("color: %1; font-size: 9px; font-weight: bold; letter-spacing: 0.5px; background: transparent;")
            .arg(ui::colors::TEXT_SECONDARY()));
    hl->addWidget(lbl);
    hl->addStretch();

    return w;
}

// ── Fear & Greed ─────────────────────────────────────────────────────────────

QWidget* MarketPulsePanel::build_fear_greed_section() {
    auto* w = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(12, 8, 12, 8);
    vl->setSpacing(6);

    // Header row
    auto* header_row = new QWidget;
    auto* hrl = new QHBoxLayout(header_row);
    hrl->setContentsMargins(0, 0, 0, 0);

    auto* label = new QLabel("FEAR & GREED INDEX");
    label->setStyleSheet(
        QString("color: %1; font-size: 9px; font-weight: bold; letter-spacing: 0.5px; background: transparent;")
            .arg(ui::colors::TEXT_SECONDARY()));
    hrl->addWidget(label);
    hrl->addStretch();

    auto* gauge_icon = new QLabel(QChar(0x25CE));
    gauge_icon->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(ui::colors::AMBER()));
    hrl->addWidget(gauge_icon);

    vl->addWidget(header_row);

    // Gradient bar (static decoration)
    fg_gradient_bar_ = new QFrame;
    fg_gradient_bar_->setFixedHeight(6);
    fg_gradient_bar_->setStyleSheet(
        "QFrame { border-radius: 3px; "
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "stop:0 #dc2626, stop:0.25 #FF6644, stop:0.5 #FFD700, stop:0.75 #88CC44, stop:1 #16a34a); }");
    vl->addWidget(fg_gradient_bar_);

    // Score row
    auto* score_row = new QWidget;
    auto* srl = new QHBoxLayout(score_row);
    srl->setContentsMargins(0, 0, 0, 0);

    fg_score_val_ = new QLabel("--");
    fg_score_val_->setStyleSheet(
        QString("color: %1; font-size: 18px; font-weight: bold; background: transparent;")
            .arg(ui::colors::TEXT_TERTIARY()));
    srl->addWidget(fg_score_val_);

    fg_score_max_ = new QLabel("/100");
    fg_score_max_->setStyleSheet(
        QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::TEXT_TERTIARY()));
    srl->addWidget(fg_score_max_);

    srl->addStretch();

    fg_sentiment_ = new QLabel("LOADING...");
    fg_sentiment_->setStyleSheet(
        QString("color: %1; font-size: 9px; font-weight: bold; letter-spacing: 0.5px; background: transparent;")
            .arg(ui::colors::TEXT_TERTIARY()));
    srl->addWidget(fg_sentiment_);

    vl->addWidget(score_row);
    return w;
}

// ── Market Breadth ────────────────────────────────────────────────────────────

QWidget* MarketPulsePanel::build_breadth_section() {
    auto* w = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_section_header("MARKET BREADTH", QChar(0x2593), ui::colors::CYAN()));

    auto* bars = new QWidget;
    auto* bl = new QVBoxLayout(bars);
    bl->setContentsMargins(0, 6, 0, 6);
    bl->setSpacing(0);

    auto make_row = [&](const QString& name, BreadthRow& row) {
        auto* rw = new QWidget;
        auto* rl = new QVBoxLayout(rw);
        rl->setContentsMargins(12, 4, 12, 4);
        rl->setSpacing(3);

        auto* top = new QWidget;
        auto* tl = new QHBoxLayout(top);
        tl->setContentsMargins(0, 0, 0, 0);

        auto* nm = new QLabel(name);
        nm->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;")
                              .arg(ui::colors::TEXT_SECONDARY()));
        tl->addWidget(nm);
        tl->addStretch();

        row.adv = new QLabel("--");
        row.adv->setStyleSheet(
            QString("color: %1; font-size: 8px; background: transparent;").arg(ui::colors::POSITIVE()));
        tl->addWidget(row.adv);

        auto* slash = new QLabel("/");
        slash->setStyleSheet(
            QString("color: %1; font-size: 8px; background: transparent;").arg(ui::colors::TEXT_TERTIARY()));
        tl->addWidget(slash);

        row.dec = new QLabel("--");
        row.dec->setStyleSheet(
            QString("color: %1; font-size: 8px; background: transparent;").arg(ui::colors::NEGATIVE()));
        tl->addWidget(row.dec);

        rl->addWidget(top);

        auto* bar_container = new QWidget;
        bar_container->setFixedHeight(4);
        auto* bar_layout = new QHBoxLayout(bar_container);
        bar_layout->setContentsMargins(0, 0, 0, 0);
        bar_layout->setSpacing(0);

        auto* green = new QFrame;
        green->setStyleSheet(QString("background: %1; border-radius: 0;").arg(ui::colors::POSITIVE()));
        bar_layout->addWidget(green, 1);
        row.green = green;

        auto* red = new QFrame;
        red->setStyleSheet(QString("background: %1; border-radius: 0;").arg(ui::colors::NEGATIVE()));
        bar_layout->addWidget(red, 1);
        row.red = red;

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
    auto* w = new QWidget;
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
    auto* w = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_section_header("TOP GAINERS", QChar(0x2191), ui::colors::POSITIVE()));

    auto* rows_w = new QWidget;
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
    auto* w = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_section_header("TOP LOSERS", QChar(0x2193), ui::colors::NEGATIVE()));

    auto* rows_w = new QWidget;
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
    auto* w = new QWidget;
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
    val->setStyleSheet(
        QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;").arg(color));
    hl->addWidget(val);

    if (!change.isEmpty()) {
        bool positive = change.startsWith('+');
        auto* chg = new QLabel(change);
        chg->setStyleSheet(
            QString("color: %1; font-size: 8px; font-weight: bold; background: transparent;")
                .arg(positive                 ? ui::colors::POSITIVE()
                     : change.startsWith('-') ? ui::colors::NEGATIVE()
                                              : ui::colors::TEXT_TERTIARY()));
        hl->addWidget(chg);
    }

    return w;
}

QWidget* MarketPulsePanel::build_global_snapshot_section() {
    auto* w = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_section_header("GLOBAL SNAPSHOT", QChar(0x25CB), ui::colors::INFO()));

    // Helper to add a live stat row and store the label pointers
    struct RowDef {
        const char* label;
        StatRow&    row;
        QString     color;
    };
    RowDef defs[] = {
        {"VIX",     vix_row_,  ui::colors::WARNING()},
        {"US 10Y",  us10y_row_,ui::colors::CYAN()},
        {"DXY",     dxy_row_,  ui::colors::CYAN()},
        {"GOLD",    gold_row_,  ui::colors::WARNING()},
        {"OIL WTI", oil_row_,  ui::colors::CYAN()},
        {"BTC",     btc_row_,  ui::colors::AMBER()},
    };

    for (auto& d : defs) {
        auto* rw = new QWidget;
        rw->setStyleSheet(QString("border-bottom: 1px solid %1;").arg(ui::colors::BORDER_DIM()));
        auto* hl = new QHBoxLayout(rw);
        hl->setContentsMargins(12, 4, 12, 4);

        auto* lbl = new QLabel(d.label);
        lbl->setStyleSheet(
            QString("color: %1; font-size: 9px; font-weight: bold; letter-spacing: 0.3px; background: transparent;")
                .arg(ui::colors::TEXT_SECONDARY()));
        hl->addWidget(lbl);
        hl->addStretch();

        d.row.val = new QLabel("--");
        d.row.val->setStyleSheet(
            QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;").arg(d.color));
        hl->addWidget(d.row.val);

        d.row.chg = new QLabel("");
        d.row.chg->setStyleSheet(
            QString("color: %1; font-size: 8px; font-weight: bold; background: transparent;")
                .arg(ui::colors::TEXT_TERTIARY()));
        hl->addWidget(d.row.chg);

        vl->addWidget(rw);
    }

    return w;
}

// ── Market Hours ─────────────────────────────────────────────────────────────

QWidget* MarketPulsePanel::build_market_hours_section() {
    auto* w = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_section_header("MARKET HOURS", QChar(0x26A1), ui::colors::WARNING()));

    auto* content = new QWidget;
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(12, 6, 12, 6);
    cl->setSpacing(0);

    struct ExDef { const char* name; const char* region; };
    ExDef exchanges[] = {
        {"NYSE/NASDAQ", "US"},
        {"LSE",         "UK"},
        {"TSE (TOKYO)", "JP"},
        {"SSE (SHANGHAI)", "CN"},
        {"NSE (INDIA)", "IN"},
    };

    for (auto& ex : exchanges) {
        auto* row = new QWidget;
        row->setStyleSheet(QString("border-bottom: 1px solid %1;").arg(ui::colors::BORDER_DIM()));
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 3, 0, 3);

        auto* name = new QLabel(ex.name);
        name->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;")
                                .arg(ui::colors::TEXT_SECONDARY()));
        rl->addWidget(name);
        rl->addStretch();

        HoursRow hr;
        hr.region = ex.region;

        hr.dot = new QLabel;
        hr.dot->setFixedSize(5, 5);
        rl->addWidget(hr.dot);

        hr.status = new QLabel;
        hr.status->setStyleSheet(
            QString("color: %1; font-size: 8px; font-weight: bold; background: transparent;")
                .arg(ui::colors::TEXT_TERTIARY()));
        rl->addWidget(hr.status);

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
    int day  = now.date().dayOfWeek(); // 1=Mon, 7=Sun

    if (day >= 6)
        return "CLOSED";

    if (region == "US") {
        if (hour >= 13 && hour < 14) return "PRE";
        if (hour >= 14 && hour < 21) return "OPEN";
    } else if (region == "UK") {
        if (hour >= 7 && hour < 8)   return "PRE";
        if (hour >= 8 && hour < 17)  return "OPEN";
    } else if (region == "JP") {
        if (hour >= 0 && hour < 6)   return "OPEN";
    } else if (region == "CN") {
        if (hour >= 1 && hour < 7)   return "OPEN";
    } else if (region == "IN") {
        if (hour >= 3 && hour < 10)  return "OPEN";
    }
    return "CLOSED";
}

// ── Refresh ───────────────────────────────────────────────────────────────────

void MarketPulsePanel::refresh_market_hours() {
    for (auto& hr : hours_rows_) {
        QString status = market_status(hr.region);
        QString color  = (status == "OPEN")  ? ui::colors::POSITIVE()
                       : (status == "PRE")   ? ui::colors::WARNING()
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
    services::MarketDataService::instance().fetch_quotes(
        kBreadthSymbols, [self](bool ok, QVector<services::QuoteData> quotes) {
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
                const QStringList sp500_set = {
                    "AAPL","MSFT","GOOGL","AMZN","NVDA","META","TSLA","BRK-B","JPM","UNH",
                    "V","XOM","LLY","JNJ","WMT","MA","PG","HD","CVX","MRK"
                };
                const QStringList nasdaq_set = {
                    "NFLX","AMD","INTC","QCOM","ADBE","CSCO","ORCL","CRM","AVGO","TXN"
                };

                bool in_sp = sp500_set.contains(q.symbol);
                bool in_nq = nasdaq_set.contains(q.symbol);
                // rest is NYSE proxy

                if (q.change_pct > 0.3) {
                    if (in_sp)      ++sp500_adv;
                    else if (in_nq) ++nasdaq_adv;
                    else            ++nyse_adv;
                    ++bullish;
                } else if (q.change_pct < -0.3) {
                    if (in_sp)      ++sp500_dec;
                    else if (in_nq) ++nasdaq_dec;
                    else            ++nyse_dec;
                    ++bearish;
                } else {
                    ++neutral_count;
                }
            }

            // ── Update breadth bars ──
            auto update_row = [](MarketPulsePanel::BreadthRow& row, int adv, int dec) {
                if (!row.adv) return;
                row.adv->setText(QString::number(adv));
                row.dec->setText(QString::number(dec));
                int total = adv + dec;
                if (total == 0) total = 1;
                int adv_pct = static_cast<int>((double(adv) / total) * 100);
                auto* layout = qobject_cast<QHBoxLayout*>(row.green->parentWidget()->layout());
                if (layout) {
                    layout->setStretch(0, adv_pct);
                    layout->setStretch(1, 100 - adv_pct);
                }
            };

            update_row(self->nyse_row_,   nyse_adv,   nyse_dec);
            update_row(self->nasdaq_row_, nasdaq_adv, nasdaq_dec);
            update_row(self->sp500_row_,  sp500_adv,  sp500_dec);

            // ── Fear & Greed score (0-100 scale) ──
            int total_stocks = bullish + bearish + neutral_count;
            if (total_stocks == 0) total_stocks = 1;
            // Start at 50 (neutral), scale by breadth
            int score = 50 + static_cast<int>(((bullish - bearish) / static_cast<double>(total_stocks)) * 50);

            if (vix > 0) {
                if (vix > 30)      score -= 20;
                else if (vix > 25) score -= 10;
                else if (vix < 15) score += 10;
            }
            score = qBound(0, score, 100);

            QString sentiment_text, sentiment_color;
            if (score <= 20) {
                sentiment_text  = "EXTREME FEAR";
                sentiment_color = ui::colors::NEGATIVE();
            } else if (score <= 40) {
                sentiment_text  = "FEAR";
                sentiment_color = "#FF6644";
            } else if (score <= 60) {
                sentiment_text  = "NEUTRAL";
                sentiment_color = ui::colors::WARNING();
            } else if (score <= 80) {
                sentiment_text  = "GREED";
                sentiment_color = ui::colors::POSITIVE();
            } else {
                sentiment_text  = "EXTREME GREED";
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
    services::MarketDataService::instance().fetch_quotes(
        kMoverSymbols, [self](bool ok, QVector<services::QuoteData> quotes) {
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
                if (q.change_pct <= 0 || gainers_added >= 3) break;
                self->gainers_layout_->addWidget(
                    self->build_mover_row(q.symbol, q.change_pct, format_volume(q.volume)));
                ++gainers_added;
            }

            // Top 3 losers (worst first)
            int losers_added = 0;
            for (int i = quotes.size() - 1; i >= 0 && losers_added < 3; --i) {
                if (quotes[i].change_pct >= 0) continue;
                self->losers_layout_->addWidget(
                    self->build_mover_row(quotes[i].symbol, quotes[i].change_pct,
                                          format_volume(quotes[i].volume)));
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
                return QString("%1%2%")
                    .arg(q.change_pct >= 0 ? "+" : "")
                    .arg(q.change_pct, 0, 'f', 2);
            };

            auto update_stat = [&](const QString& sym, StatRow& row) {
                if (!qmap.contains(sym) || !row.val) return;
                const auto& q = qmap[sym];
                row.val->setText(fmt_price(q));
                QString chg_text = fmt_chg(q);
                row.chg->setText(chg_text);
                QString chg_color = q.change_pct >= 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE();
                row.chg->setStyleSheet(
                    QString("color: %1; font-size: 8px; font-weight: bold; background: transparent;")
                        .arg(chg_color));
            };

            // global_snapshot_symbols() returns: ^VIX, ^TNX, DX-Y.NYB, GC=F, CL=F, BTC-USD
            update_stat("^VIX",     self->vix_row_);
            update_stat("^TNX",     self->us10y_row_);
            update_stat("DX-Y.NYB", self->dxy_row_);
            update_stat("GC=F",     self->gold_row_);
            update_stat("CL=F",     self->oil_row_);
            update_stat("BTC-USD",  self->btc_row_);
        });
}

} // namespace fincept::screens
