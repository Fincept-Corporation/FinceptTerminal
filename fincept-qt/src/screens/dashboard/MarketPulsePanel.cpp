#include "screens/dashboard/MarketPulsePanel.h"

#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QFrame>
#include <QProgressBar>

namespace fincept::screens {

MarketPulsePanel::MarketPulsePanel(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("background: %1;").arg(ui::colors::PANEL));
    setMinimumWidth(260);

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_header());

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }"
                          "QScrollBar:vertical { width: 6px; background: transparent; }"
                          "QScrollBar::handle:vertical { background: #222222; border-radius: 3px; min-height: 20px; }"
                          "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");

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
}

QWidget* MarketPulsePanel::build_header() {
    auto* bar = new QWidget;
    bar->setFixedHeight(30);
    bar->setStyleSheet(
        QString("background: %1; border-bottom: 1px solid rgba(217,119,6,0.25);").arg(ui::colors::BG_RAISED));

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 12, 0);

    auto* icon = new QLabel(QChar(0x25C8)); // ◈
    icon->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(ui::colors::AMBER));
    hl->addWidget(icon);

    auto* title = new QLabel("MARKET PULSE");
    title->setStyleSheet(
        QString("color: %1; font-size: 10px; font-weight: bold; letter-spacing: 1px; background: transparent;")
            .arg(ui::colors::AMBER));
    hl->addWidget(title);
    hl->addStretch();

    auto* live_dot = new QLabel;
    live_dot->setFixedSize(6, 6);
    live_dot->setStyleSheet(QString("background: %1; border-radius: 3px;").arg(ui::colors::POSITIVE));
    hl->addWidget(live_dot);

    return bar;
}

QWidget* MarketPulsePanel::build_section_header(const QString& title, const QString& icon_char, const QString& color) {
    auto* w = new QWidget;
    w->setFixedHeight(26);
    w->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2; border-top: 1px solid %2;")
                         .arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));

    auto* hl = new QHBoxLayout(w);
    hl->setContentsMargins(12, 0, 12, 0);

    auto* icon = new QLabel(icon_char);
    icon->setStyleSheet(QString("color: %1; font-size: 10px; background: transparent;").arg(color));
    hl->addWidget(icon);

    auto* lbl = new QLabel(title);
    lbl->setStyleSheet(
        QString("color: %1; font-size: 9px; font-weight: bold; letter-spacing: 0.5px; background: transparent;")
            .arg(ui::colors::TEXT_SECONDARY));
    hl->addWidget(lbl);
    hl->addStretch();

    return w;
}

// ── Fear & Greed Gauge ──
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
            .arg(ui::colors::TEXT_SECONDARY));
    hrl->addWidget(label);
    hrl->addStretch();

    auto* gauge_icon = new QLabel(QChar(0x25CE)); // ◎
    gauge_icon->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(ui::colors::AMBER));
    hrl->addWidget(gauge_icon);

    vl->addWidget(header_row);

    // Gradient bar
    auto* gradient_bar = new QFrame;
    gradient_bar->setFixedHeight(6);
    gradient_bar->setStyleSheet(
        "QFrame { border-radius: 3px; "
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "stop:0 #dc2626, stop:0.25 #FF6644, stop:0.5 #FFD700, stop:0.75 #88CC44, stop:1 #16a34a); }");
    vl->addWidget(gradient_bar);

    // Score row
    int score = 62; // Demo value — would come from VIX calculation
    QString sentiment_text = "GREED";
    QString sentiment_color = ui::colors::POSITIVE;
    if (score <= 20) {
        sentiment_text = "EXTREME FEAR";
        sentiment_color = ui::colors::NEGATIVE;
    } else if (score <= 40) {
        sentiment_text = "FEAR";
        sentiment_color = "#FF6644";
    } else if (score <= 60) {
        sentiment_text = "NEUTRAL";
        sentiment_color = ui::colors::WARNING;
    } else if (score <= 80) {
        sentiment_text = "GREED";
        sentiment_color = ui::colors::POSITIVE;
    } else {
        sentiment_text = "EXTREME GREED";
        sentiment_color = ui::colors::POSITIVE;
    }

    auto* score_row = new QWidget;
    auto* srl = new QHBoxLayout(score_row);
    srl->setContentsMargins(0, 0, 0, 0);

    auto* score_val = new QLabel(QString::number(score));
    score_val->setStyleSheet(
        QString("color: %1; font-size: 18px; font-weight: bold; background: transparent;").arg(sentiment_color));
    srl->addWidget(score_val);

    auto* score_max = new QLabel("/100");
    score_max->setStyleSheet(
        QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::TEXT_TERTIARY));
    srl->addWidget(score_max);

    srl->addStretch();

    auto* sentiment = new QLabel(sentiment_text);
    sentiment->setStyleSheet(
        QString("color: %1; font-size: 9px; font-weight: bold; letter-spacing: 0.5px; background: transparent;")
            .arg(sentiment_color));
    srl->addWidget(sentiment);

    vl->addWidget(score_row);

    return w;
}

// ── Market Breadth ──
QWidget* MarketPulsePanel::build_breadth_section() {
    auto* w = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_section_header("MARKET BREADTH", QChar(0x2593), ui::colors::CYAN)); // ▓

    auto* bars = new QWidget;
    auto* bl = new QVBoxLayout(bars);
    bl->setContentsMargins(0, 6, 0, 6);
    bl->setSpacing(0);

    bl->addWidget(build_breadth_bar("NYSE", 1847, 1253));
    bl->addWidget(build_breadth_bar("NASDAQ", 2156, 1644));
    bl->addWidget(build_breadth_bar("S&P 500", 312, 188));

    vl->addWidget(bars);
    return w;
}

QWidget* MarketPulsePanel::build_breadth_bar(const QString& label, int advancing, int declining) {
    auto* w = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(12, 4, 12, 4);
    vl->setSpacing(3);

    auto* top = new QWidget;
    auto* tl = new QHBoxLayout(top);
    tl->setContentsMargins(0, 0, 0, 0);

    auto* name = new QLabel(label);
    name->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;")
                            .arg(ui::colors::TEXT_SECONDARY));
    tl->addWidget(name);
    tl->addStretch();

    auto* adv = new QLabel(QString::number(advancing));
    adv->setStyleSheet(QString("color: %1; font-size: 8px; background: transparent;").arg(ui::colors::POSITIVE));
    tl->addWidget(adv);

    auto* slash = new QLabel("/");
    slash->setStyleSheet(QString("color: %1; font-size: 8px; background: transparent;").arg(ui::colors::TEXT_TERTIARY));
    tl->addWidget(slash);

    auto* dec = new QLabel(QString::number(declining));
    dec->setStyleSheet(QString("color: %1; font-size: 8px; background: transparent;").arg(ui::colors::NEGATIVE));
    tl->addWidget(dec);

    vl->addWidget(top);

    // Dual-color bar
    int total = advancing + declining;
    double adv_pct = total > 0 ? (double(advancing) / total) * 100.0 : 50.0;

    auto* bar_container = new QWidget;
    bar_container->setFixedHeight(4);
    auto* bar_layout = new QHBoxLayout(bar_container);
    bar_layout->setContentsMargins(0, 0, 0, 0);
    bar_layout->setSpacing(0);

    auto* green_bar = new QFrame;
    green_bar->setStyleSheet(QString("background: %1; border-radius: 0;").arg(ui::colors::POSITIVE));
    bar_layout->addWidget(green_bar, static_cast<int>(adv_pct));

    auto* red_bar = new QFrame;
    red_bar->setStyleSheet(QString("background: %1; border-radius: 0;").arg(ui::colors::NEGATIVE));
    bar_layout->addWidget(red_bar, static_cast<int>(100 - adv_pct));

    vl->addWidget(bar_container);
    return w;
}

// ── Top Gainers / Losers ──
QWidget* MarketPulsePanel::build_mover_row(const QString& symbol, double change, const QString& volume) {
    auto* w = new QWidget;
    w->setStyleSheet(QString("border-bottom: 1px solid %1;").arg(ui::colors::BORDER_DIM));

    auto* hl = new QHBoxLayout(w);
    hl->setContentsMargins(12, 5, 12, 5);
    hl->setSpacing(4);

    auto* sym = new QLabel(symbol);
    sym->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;")
                           .arg(ui::colors::TEXT_PRIMARY));
    hl->addWidget(sym);
    hl->addStretch();

    bool positive = change >= 0;
    auto* arrow = new QLabel(positive ? QChar(0x25B2) : QChar(0x25BC)); // ▲ ▼
    arrow->setStyleSheet(QString("color: %1; font-size: 8px; background: transparent;")
                             .arg(positive ? ui::colors::POSITIVE : ui::colors::NEGATIVE));
    hl->addWidget(arrow);

    auto* chg = new QLabel(QString("%1%2%").arg(positive ? "+" : "").arg(change, 0, 'f', 2));
    chg->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;")
                           .arg(positive ? ui::colors::POSITIVE : ui::colors::NEGATIVE));
    hl->addWidget(chg);

    if (!volume.isEmpty()) {
        auto* vol = new QLabel(QString("VOL: %1").arg(volume));
        vol->setStyleSheet(
            QString("color: %1; font-size: 8px; background: transparent;").arg(ui::colors::TEXT_TERTIARY));
        hl->addWidget(vol);
    }

    return w;
}

QWidget* MarketPulsePanel::build_gainers_section() {
    auto* w = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_section_header("TOP GAINERS", QChar(0x2191), ui::colors::POSITIVE)); // ↑

    struct Mover {
        const char* sym;
        double chg;
        const char* vol;
    };
    Mover data[] = {
        {"SMCI", 4.56, "2.1M"},
        {"PLTR", 3.89, "45.3M"},
        {"NVDA", 2.34, "38.7M"},
    };
    for (auto& m : data) {
        vl->addWidget(build_mover_row(m.sym, m.chg, m.vol));
    }

    return w;
}

QWidget* MarketPulsePanel::build_losers_section() {
    auto* w = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_section_header("TOP LOSERS", QChar(0x2193), ui::colors::NEGATIVE)); // ↓

    struct Mover {
        const char* sym;
        double chg;
        const char* vol;
    };
    Mover data[] = {
        {"RIVN", -5.67, "12.4M"},
        {"COIN", -3.45, "8.9M"},
        {"MARA", -4.12, "6.2M"},
    };
    for (auto& m : data) {
        vl->addWidget(build_mover_row(m.sym, m.chg, m.vol));
    }

    return w;
}

// ── Global Snapshot ──
QWidget* MarketPulsePanel::build_stat_row(const QString& label, const QString& value, const QString& change,
                                          const QString& color) {
    auto* w = new QWidget;
    w->setStyleSheet(QString("border-bottom: 1px solid %1;").arg(ui::colors::BORDER_DIM));

    auto* hl = new QHBoxLayout(w);
    hl->setContentsMargins(12, 4, 12, 4);

    auto* lbl = new QLabel(label);
    lbl->setStyleSheet(
        QString("color: %1; font-size: 9px; font-weight: bold; letter-spacing: 0.3px; background: transparent;")
            .arg(ui::colors::TEXT_SECONDARY));
    hl->addWidget(lbl);
    hl->addStretch();

    auto* val = new QLabel(value);
    val->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;").arg(color));
    hl->addWidget(val);

    if (!change.isEmpty()) {
        bool positive = change.startsWith('+');
        auto* chg = new QLabel(change);
        chg->setStyleSheet(QString("color: %1; font-size: 8px; font-weight: bold; background: transparent;")
                               .arg(positive                 ? ui::colors::POSITIVE
                                    : change.startsWith('-') ? ui::colors::NEGATIVE
                                                             : ui::colors::TEXT_TERTIARY));
        hl->addWidget(chg);
    }

    return w;
}

QWidget* MarketPulsePanel::build_global_snapshot_section() {
    auto* w = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_section_header("GLOBAL SNAPSHOT", QChar(0x25CB), ui::colors::INFO)); // ○

    vl->addWidget(build_stat_row("VIX", "18.42", "+1.23%", ui::colors::WARNING));
    vl->addWidget(build_stat_row("US 10Y", "4.28%", "-0.15%", ui::colors::CYAN));
    vl->addWidget(build_stat_row("DXY", "104.32", "+0.08%", ui::colors::CYAN));
    vl->addWidget(build_stat_row("GOLD", "$2,345", "+0.45%", ui::colors::WARNING));
    vl->addWidget(build_stat_row("OIL WTI", "$78.34", "-0.67%", ui::colors::CYAN));
    vl->addWidget(build_stat_row("BTC", "$67.2K", "+2.12%", ui::colors::AMBER));

    return w;
}

// ── Market Hours ──
QWidget* MarketPulsePanel::build_market_hours_section() {
    auto* w = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_section_header("MARKET HOURS", QChar(0x26A1), ui::colors::WARNING)); // ⚡

    auto* content = new QWidget;
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(12, 6, 12, 6);
    cl->setSpacing(0);

    struct Exchange {
        const char* name;
        const char* region;
        const char* color;
    };
    Exchange exchanges[] = {
        {"NYSE/NASDAQ", "US", ui::colors::POSITIVE},
        {"LSE", "UK", ui::colors::INFO},
        {"TSE (TOKYO)", "JP", "#9D4EDD"},
        {"SSE (SHANGHAI)", "CN", ui::colors::NEGATIVE},
        {"NSE (INDIA)", "IN", ui::colors::AMBER},
    };

    for (auto& ex : exchanges) {
        auto* row = new QWidget;
        row->setStyleSheet(QString("border-bottom: 1px solid %1;").arg(ui::colors::BORDER_DIM));
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 3, 0, 3);

        auto* name = new QLabel(ex.name);
        name->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;")
                                .arg(ui::colors::TEXT_SECONDARY));
        rl->addWidget(name);
        rl->addStretch();

        QString status = market_status(ex.region);
        QString status_color = (status == "OPEN")  ? ui::colors::POSITIVE
                               : (status == "PRE") ? ui::colors::WARNING
                                                   : ui::colors::NEGATIVE;

        auto* dot = new QLabel;
        dot->setFixedSize(5, 5);
        dot->setStyleSheet(QString("background: %1; border-radius: 2px;").arg(status_color));
        rl->addWidget(dot);

        auto* st = new QLabel(status);
        st->setStyleSheet(
            QString("color: %1; font-size: 8px; font-weight: bold; background: transparent;").arg(status_color));
        rl->addWidget(st);

        cl->addWidget(row);
    }

    vl->addWidget(content);
    return w;
}

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

} // namespace fincept::screens
