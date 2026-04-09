// src/screens/equity_research/EquityTechnicalsTab.cpp
#include "screens/equity_research/EquityTechnicalsTab.h"

#include "services/equity/EquityResearchService.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QProgressBar>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens {

// ── Accent colors without a theme token ──────────────────────────────────────
static constexpr const char* LTGREEN  = "#4ade80";
static constexpr const char* LTRED    = "#f87171";
static constexpr const char* CYAN     = "#22d3ee";
static constexpr const char* YELLOW   = "#eab308";
static constexpr const char* BLUE     = "#60a5fa";
static constexpr const char* PURPLE   = "#a855f7";
static constexpr const char* GRAY     = "#6b7280";

// ── Signal helpers ───────────────────────────────────────────────────────────

QString EquityTechnicalsTab::signal_text(services::equity::TechSignal s) {
    using S = services::equity::TechSignal;
    switch (s) {
        case S::StrongBuy:  return "STRONG BUY";
        case S::Buy:        return "BUY";
        case S::Sell:       return "SELL";
        case S::StrongSell: return "STRONG SELL";
        default:            return "NEUTRAL";
    }
}

const char* EquityTechnicalsTab::signal_color(services::equity::TechSignal s) {
    using S = services::equity::TechSignal;
    switch (s) {
        case S::StrongBuy:  return ui::colors::POSITIVE;
        case S::Buy:        return LTGREEN;
        case S::Sell:       return LTRED;
        case S::StrongSell: return ui::colors::NEGATIVE;
        default:            return GRAY;
    }
}

/// Context-aware interpretation for each indicator
QString EquityTechnicalsTab::interpretation(const QString& col_key, double value) {
    if (col_key == "rsi") {
        if (value <= 25)      return "Deeply oversold — potential reversal zone";
        if (value <= 30)      return "Oversold — watch for bullish divergence";
        if (value <= 40)      return "Below midpoint — bearish bias weakening";
        if (value <= 60)      return "Neutral zone — no strong momentum";
        if (value <= 70)      return "Above midpoint — bullish bias building";
        if (value <= 80)      return "Overbought — watch for bearish divergence";
        return "Deeply overbought — potential reversal zone";
    }
    if (col_key == "macd") {
        if (value > 2)        return "Strong bullish momentum — histogram expanding";
        if (value > 0)        return "Bullish — MACD above signal line";
        if (value > -2)       return "Bearish — MACD below signal line";
        return "Strong bearish momentum — histogram expanding";
    }
    if (col_key == "stoch_k" || col_key == "stoch_d") {
        if (value <= 20)      return "Oversold zone — potential bounce";
        if (value <= 40)      return "Below midpoint — watch for crossover";
        if (value <= 60)      return "Neutral — consolidation phase";
        if (value <= 80)      return "Above midpoint — bullish momentum";
        return "Overbought zone — potential pullback";
    }
    if (col_key == "williams_r") {
        if (value <= -80)     return "Oversold — potential reversal up";
        if (value <= -50)     return "Bearish territory — selling pressure";
        if (value >= -20)     return "Overbought — potential reversal down";
        return "Neutral zone";
    }
    if (col_key == "cci") {
        if (value <= -200)    return "Extreme oversold — deep value territory";
        if (value <= -100)    return "Oversold — watch for trend reversal";
        if (value >= 200)     return "Extreme overbought — euphoria zone";
        if (value >= 100)     return "Overbought — watch for profit taking";
        return "Neutral — no extreme conditions";
    }
    if (col_key == "mfi") {
        if (value <= 20)      return "Oversold — money flowing out aggressively";
        if (value <= 40)      return "Weak inflow — cautious sentiment";
        if (value >= 80)      return "Overbought — heavy money inflow";
        if (value >= 60)      return "Strong inflow — buying pressure";
        return "Balanced money flow";
    }
    if (col_key == "adx") {
        if (value >= 50)      return "Very strong trend — trade with the trend";
        if (value >= 25)      return "Trending — directional move in play";
        if (value >= 20)      return "Weak trend — possible consolidation";
        return "No trend — range-bound market";
    }
    if (col_key == "bb_pband") {
        if (value < 0)        return "Below lower band — extreme oversold";
        if (value < 0.2)      return "Near lower band — oversold zone";
        if (value > 1.0)      return "Above upper band — extreme overbought";
        if (value > 0.8)      return "Near upper band — overbought zone";
        return "Within bands — normal range";
    }
    if (col_key == "bb_wband") {
        if (value < 0.05)     return "Tight squeeze — breakout imminent";
        if (value < 0.1)      return "Narrowing bands — volatility contracting";
        if (value > 0.3)      return "Wide bands — high volatility";
        return "Normal bandwidth";
    }
    if (col_key == "atr")     return "Average true range — use for stop-loss sizing";
    if (col_key == "roc") {
        if (value > 5)        return "Strong upward momentum";
        if (value < -5)       return "Strong downward momentum";
        return "Flat momentum — sideways movement";
    }
    if (col_key == "cmf") {
        if (value > 0.1)      return "Buying pressure — accumulation";
        if (value < -0.1)     return "Selling pressure — distribution";
        return "Balanced — no clear accumulation or distribution";
    }
    if (col_key == "aroon_up") {
        if (value >= 70)      return "Strong uptrend — recent new highs";
        if (value <= 30)      return "Weak upside — no recent highs";
        return "Moderate — watching for trend development";
    }
    if (col_key == "aroon_down") {
        if (value >= 70)      return "Strong downtrend — recent new lows";
        if (value <= 30)      return "Weak downside — no recent lows";
        return "Moderate — watching for trend development";
    }
    if (col_key == "obv")     return "On-balance volume — confirms price trend with volume";
    if (col_key == "vwap")    return "Volume-weighted avg price — institutional reference";
    if (col_key == "adi")     return "Accumulation/distribution — confirms money flow";
    if (col_key.startsWith("sma_") || col_key.startsWith("ema_") ||
        col_key.startsWith("wma_") || col_key == "kama")
        return "Moving average — price above = bullish, below = bearish";
    if (col_key == "macd_signal") return "Signal line — crossover with MACD triggers trade";
    if (col_key == "bb_mavg") return "Bollinger midline (20-SMA) — dynamic support/resistance";
    if (col_key == "bb_hband") return "Upper band — resistance level, overbought above";
    if (col_key == "bb_lband") return "Lower band — support level, oversold below";
    if (col_key == "ao")      return value > 0 ? "Bullish — momentum above zero line" : "Bearish — momentum below zero line";
    return "";
}

// ── Constructor ──────────────────────────────────────────────────────────────

EquityTechnicalsTab::EquityTechnicalsTab(QWidget* parent) : QWidget(parent) {
    build_ui();
    auto& svc = services::equity::EquityResearchService::instance();
    connect(&svc, &services::equity::EquityResearchService::technicals_loaded, this,
            &EquityTechnicalsTab::on_technicals_loaded);
}

void EquityTechnicalsTab::set_symbol(const QString& symbol) {
    if (symbol == current_symbol_)
        return;
    current_symbol_ = symbol;
    loading_overlay_->show_loading("COMPUTING INDICATORS\xe2\x80\xa6");
    services::equity::EquityResearchService::instance().fetch_technicals(symbol, "1y");
}

// ── build_ui ─────────────────────────────────────────────────────────────────

void EquityTechnicalsTab::build_ui() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
    loading_overlay_ = new ui::LoadingOverlay(this);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("background:transparent;border:0;");

    auto* content = new QWidget;
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(8);

    // ── Top row: RATING + KEY INDICATORS ─────────────────────────────────────
    auto* top_row = new QHBoxLayout;
    top_row->setSpacing(8);

    // === RATING PANEL ===
    auto* rating_panel = new QFrame;
    rating_panel->setFixedWidth(260);
    rating_panel->setStyleSheet(QString("QFrame{background:%1;border:1px solid %2;border-radius:2px;}").arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));
    auto* rp_vl = new QVBoxLayout(rating_panel);
    rp_vl->setContentsMargins(14, 10, 14, 10);
    rp_vl->setSpacing(10);

    auto* rp_title = new QLabel("TECHNICAL RATING");
    rp_title->setStyleSheet(QString("color:%1;font-size:12px;font-weight:700;letter-spacing:1px;background:transparent;border:0;").arg(ui::colors::AMBER));
    rp_vl->addWidget(rp_title);

    auto* sep = new QFrame; sep->setFrameShape(QFrame::HLine); sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background:%1;border:0;").arg(ui::colors::BORDER_DIM));
    rp_vl->addWidget(sep);

    rating_label_ = new QLabel("\xe2\x80\x94");
    rating_label_->setAlignment(Qt::AlignCenter);
    rating_label_->setStyleSheet(QString("color:%1;font-size:22px;font-weight:700;letter-spacing:2px;background:transparent;border:0;").arg(GRAY));
    rp_vl->addWidget(rating_label_);

    // Gauge bar
    gauge_bar_ = new QProgressBar;
    gauge_bar_->setRange(0, 100);
    gauge_bar_->setValue(50);
    gauge_bar_->setFixedHeight(6);
    gauge_bar_->setTextVisible(false);
    gauge_bar_->setStyleSheet(QString(
        "QProgressBar{background:%1;border:1px solid %2;border-radius:0;}"
        "QProgressBar::chunk{background:%3;}").arg(ui::colors::NEGATIVE, ui::colors::BORDER_DIM, ui::colors::POSITIVE));
    rp_vl->addWidget(gauge_bar_);

    // Signal counts — 5 columns
    auto* counts = new QHBoxLayout;
    counts->setSpacing(4);

    auto make_count = [&](const char* color, const QString& label, QLabel*& out) {
        auto* w = new QWidget;
        w->setStyleSheet(QString("background:%1;border:0;").arg(ui::colors::BG_RAISED));
        auto* cvl = new QVBoxLayout(w);
        cvl->setContentsMargins(4, 4, 4, 4);
        cvl->setSpacing(1);
        cvl->setAlignment(Qt::AlignCenter);
        out = new QLabel("0");
        out->setAlignment(Qt::AlignCenter);
        out->setStyleSheet(QString("color:%1;font-size:16px;font-weight:700;background:transparent;border:0;").arg(color));
        auto* lbl = new QLabel(label);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet(QString("color:%1;font-size:8px;font-weight:600;letter-spacing:0.5px;background:transparent;border:0;").arg(ui::colors::TEXT_TERTIARY));
        cvl->addWidget(out);
        cvl->addWidget(lbl);
        counts->addWidget(w, 1);
    };

    make_count(ui::colors::POSITIVE, "STR.BUY", strong_buy_count_);
    make_count(LTGREEN, "BUY", buy_count_);
    make_count(GRAY, "NEUTRAL", neutral_count_);
    make_count(LTRED, "SELL", sell_count_);
    make_count(ui::colors::NEGATIVE, "STR.SELL", strong_sell_count_);
    rp_vl->addLayout(counts);

    total_label_ = new QLabel("0 INDICATORS");
    total_label_->setAlignment(Qt::AlignCenter);
    total_label_->setStyleSheet(QString("color:%1;font-size:10px;letter-spacing:1px;background:transparent;border:0;").arg(ui::colors::TEXT_TERTIARY));
    rp_vl->addWidget(total_label_);
    rp_vl->addStretch();

    top_row->addWidget(rating_panel);

    // === KEY INDICATORS PANEL ===
    auto* key_panel = new QFrame;
    key_panel->setStyleSheet(QString("QFrame{background:%1;border:1px solid %2;border-radius:2px;}").arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));
    auto* kp_vl = new QVBoxLayout(key_panel);
    kp_vl->setContentsMargins(10, 10, 10, 10);
    kp_vl->setSpacing(6);

    auto* kp_title = new QLabel("KEY INDICATORS");
    kp_title->setStyleSheet(QString("color:%1;font-size:12px;font-weight:700;letter-spacing:1px;background:transparent;border:0;").arg(CYAN));
    kp_vl->addWidget(kp_title);

    auto* ksep = new QFrame; ksep->setFrameShape(QFrame::HLine); ksep->setFixedHeight(1);
    ksep->setStyleSheet(QString("background:%1;border:0;").arg(ui::colors::BORDER_DIM));
    kp_vl->addWidget(ksep);

    key_container_ = new QWidget;
    key_container_->setStyleSheet("background:transparent;border:0;");
    auto* kc_layout = new QGridLayout(key_container_);
    kc_layout->setContentsMargins(0, 0, 0, 0);
    kc_layout->setSpacing(6);
    kp_vl->addWidget(key_container_, 1);

    top_row->addWidget(key_panel, 1);
    vl->addLayout(top_row);

    // ── Category sections ────────────────────────────────────────────────────
    sections_container_ = new QWidget;
    sections_container_->setStyleSheet("background:transparent;border:0;");
    auto* sc_vl = new QVBoxLayout(sections_container_);
    sc_vl->setContentsMargins(0, 0, 0, 0);
    sc_vl->setSpacing(6);
    vl->addWidget(sections_container_);
    vl->addStretch();

    scroll->setWidget(content);
    outer->addWidget(scroll);
}

// ── clear_sections ───────────────────────────────────────────────────────────

void EquityTechnicalsTab::clear_sections() {
    // Clear key indicators grid
    if (auto* gl = qobject_cast<QGridLayout*>(key_container_->layout())) {
        while (gl->count() > 0) {
            auto* item = gl->takeAt(0);
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
    }
    // Clear category sections
    if (auto* vl = qobject_cast<QVBoxLayout*>(sections_container_->layout())) {
        while (vl->count() > 0) {
            auto* item = vl->takeAt(0);
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
    }
}

// ── populate ─────────────────────────────────────────────────────────────────

void EquityTechnicalsTab::populate(const services::equity::TechnicalsData& data) {
    clear_sections();

    // Flatten all
    QVector<services::equity::TechIndicator> all;
    all << data.trend << data.momentum << data.volatility << data.volume;

    // ── Rating ───────────────────────────────────────────────────────────────
    int sb = data.strong_buy, b = data.buy, n = data.neutral, s = data.sell, ss = data.strong_sell;
    int total = sb + b + n + s + ss;

    const char* rec_color = GRAY;
    QString rec_text = signal_text(data.overall_signal);
    rec_color = signal_color(data.overall_signal);

    rating_label_->setText(rec_text);
    rating_label_->setStyleSheet(QString("color:%1;font-size:22px;font-weight:700;letter-spacing:2px;background:transparent;border:0;").arg(rec_color));

    int bulls = sb + b;
    gauge_bar_->setValue(total > 0 ? static_cast<int>(100.0 * bulls / total) : 50);

    strong_buy_count_->setText(QString::number(sb));
    buy_count_->setText(QString::number(b));
    neutral_count_->setText(QString::number(n));
    sell_count_->setText(QString::number(s));
    strong_sell_count_->setText(QString::number(ss));
    total_label_->setText(QString("%1 INDICATORS ANALYZED").arg(total));

    // ── Key indicators — pick the most decision-relevant ones ────────────────
    // These are column keys we look for in the flattened indicators
    static const QStringList key_names = {"RSI", "MACD", "Stoch %K", "ADX", "CCI", "MFI", "BB %B", "Williams %R", "ATR", "CMF"};

    QMap<QString, services::equity::TechIndicator> name_map;
    for (const auto& ti : all)
        name_map[ti.name] = ti;

    auto* kg = qobject_cast<QGridLayout*>(key_container_->layout());
    int ki_row = 0, ki_col = 0;

    for (const auto& key : key_names) {
        auto it = name_map.find(key);
        if (it == name_map.end()) continue;
        const auto& ti = it.value();

        auto* card = new QFrame;
        const char* sc = signal_color(ti.signal);
        card->setStyleSheet(QString("QFrame{background:%1;border:1px solid %2;border-left:3px solid %3;border-radius:2px;}")
                                .arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM, sc));
        auto* cl = new QVBoxLayout(card);
        cl->setContentsMargins(10, 6, 10, 6);
        cl->setSpacing(2);

        // Name
        auto* nm = new QLabel(ti.name.toUpper());
        nm->setStyleSheet(QString("color:%1;font-size:10px;font-weight:600;letter-spacing:0.5px;background:transparent;border:0;").arg(ui::colors::TEXT_SECONDARY));
        cl->addWidget(nm);

        // Value + signal row
        auto* vr = new QHBoxLayout;
        vr->setSpacing(8);
        auto* val = new QLabel(QString::number(ti.value, 'f', 2));
        val->setStyleSheet(QString("color:%1;font-size:16px;font-weight:700;font-family:'Consolas',monospace;background:transparent;border:0;").arg(ui::colors::TEXT_PRIMARY));
        auto* sig = new QLabel(signal_text(ti.signal));
        sig->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;background:transparent;border:0;").arg(sc));
        sig->setAlignment(Qt::AlignRight | Qt::AlignBottom);
        vr->addWidget(val);
        vr->addStretch();
        vr->addWidget(sig);
        cl->addLayout(vr);

        // Interpretation
        QString interp = interpretation(ti.category == "trend" && ti.name == "MACD" ? "macd" :
                                        ti.category == "momentum" && ti.name == "RSI" ? "rsi" :
                                        ti.name.toLower().replace(' ', '_').replace(QString("%"), QString()), ti.value);
        if (!interp.isEmpty()) {
            auto* desc = new QLabel(interp);
            desc->setWordWrap(true);
            desc->setStyleSheet(QString("color:%1;font-size:10px;background:transparent;border:0;").arg(ui::colors::TEXT_TERTIARY));
            cl->addWidget(desc);
        }

        kg->addWidget(card, ki_row, ki_col);
        ki_col++;
        if (ki_col == 5) { ki_col = 0; ki_row++; }
    }

    // ── Category sections ────────────────────────────────────────────────────
    auto* sc_vl = qobject_cast<QVBoxLayout*>(sections_container_->layout());

    struct CatDef {
        QString title;
        const QVector<services::equity::TechIndicator>* inds;
        const char* color;
    };
    QList<CatDef> cats = {
        {"TREND INDICATORS",       &data.trend,       CYAN},
        {"MOMENTUM INDICATORS",    &data.momentum,    ui::colors::POSITIVE},
        {"VOLATILITY INDICATORS",  &data.volatility,  YELLOW},
        {"VOLUME INDICATORS",      &data.volume,      BLUE},
    };

    for (const auto& cat : cats) {
        if (!cat.inds || cat.inds->isEmpty()) continue;

        // Count per category
        int cb = 0, cs = 0, cn = 0;
        for (const auto& ti : *cat.inds) {
            using S = services::equity::TechSignal;
            if (ti.signal == S::StrongBuy || ti.signal == S::Buy) cb++;
            else if (ti.signal == S::StrongSell || ti.signal == S::Sell) cs++;
            else cn++;
        }

        auto* section = new QFrame;
        section->setStyleSheet(QString("QFrame{background:%1;border:1px solid %2;border-radius:2px;}").arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));
        auto* svl = new QVBoxLayout(section);
        svl->setContentsMargins(0, 0, 0, 0);
        svl->setSpacing(0);

        // Header
        auto* hdr = new QWidget;
        hdr->setStyleSheet(QString("background:%1;border:0;border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));
        auto* hl = new QHBoxLayout(hdr);
        hl->setContentsMargins(12, 8, 12, 8);
        hl->setSpacing(10);

        auto* htitle = new QLabel(cat.title);
        htitle->setStyleSheet(QString("color:%1;font-size:12px;font-weight:700;letter-spacing:1px;background:transparent;border:0;").arg(cat.color));
        hl->addWidget(htitle);

        auto* cnt = new QLabel(QString("%1 indicators").arg(cat.inds->size()));
        cnt->setStyleSheet(QString("color:%1;font-size:10px;background:transparent;border:0;").arg(ui::colors::TEXT_TERTIARY));
        hl->addWidget(cnt);
        hl->addStretch();

        auto add_badge = [&](int count, const QString& label, const char* color) {
            if (count == 0) return;
            auto* badge = new QLabel(QString("%1 %2").arg(count).arg(label));
            badge->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;padding:2px 6px;background:transparent;border:0;").arg(color));
            hl->addWidget(badge);
        };
        add_badge(cb, "BUY", ui::colors::POSITIVE);
        add_badge(cn, "HOLD", GRAY);
        add_badge(cs, "SELL", ui::colors::NEGATIVE);

        svl->addWidget(hdr);

        // Table header
        auto* tbl_hdr = new QWidget;
        tbl_hdr->setStyleSheet(QString("background:%1;border:0;border-bottom:1px solid %2;").arg(ui::colors::BG_BASE, ui::colors::BORDER_DIM));
        auto* thl = new QHBoxLayout(tbl_hdr);
        thl->setContentsMargins(12, 4, 12, 4);
        thl->setSpacing(0);

        auto hdr_lbl = [&](const QString& text, int stretch, Qt::Alignment align = Qt::AlignLeft) {
            auto* l = new QLabel(text);
            l->setAlignment(align | Qt::AlignVCenter);
            l->setStyleSheet(QString("color:%1;font-size:10px;font-weight:600;letter-spacing:0.5px;background:transparent;border:0;").arg(ui::colors::TEXT_TERTIARY));
            thl->addWidget(l, stretch);
        };
        hdr_lbl("INDICATOR", 2);
        hdr_lbl("VALUE", 1, Qt::AlignRight);
        hdr_lbl("SIGNAL", 1, Qt::AlignCenter);
        hdr_lbl("INTERPRETATION", 3);
        svl->addWidget(tbl_hdr);

        // Rows
        bool alt = false;
        for (const auto& ti : *cat.inds) {
            const char* sc = signal_color(ti.signal);

            auto* row = new QWidget;
            row->setStyleSheet(QString("background:%1;border:0;border-bottom:1px solid %2;")
                                   .arg(alt ? ui::colors::BG_RAISED : ui::colors::BG_BASE, ui::colors::BORDER_DIM));
            auto* rl = new QHBoxLayout(row);
            rl->setContentsMargins(12, 5, 12, 5);
            rl->setSpacing(0);

            // Name
            auto* name_lbl = new QLabel(ti.name);
            name_lbl->setStyleSheet(QString("color:%1;font-size:12px;font-weight:600;background:transparent;border:0;").arg(ui::colors::TEXT_PRIMARY));
            rl->addWidget(name_lbl, 2);

            // Value
            auto* val_lbl = new QLabel(QString::number(ti.value, 'f', 4));
            val_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            val_lbl->setStyleSheet(QString("color:%1;font-size:12px;font-family:'Consolas',monospace;background:transparent;border:0;").arg(ui::colors::TEXT_SECONDARY));
            rl->addWidget(val_lbl, 1);

            // Signal badge
            auto* sig_w = new QWidget;
            sig_w->setStyleSheet("background:transparent;border:0;");
            auto* sig_hl = new QHBoxLayout(sig_w);
            sig_hl->setContentsMargins(0, 0, 0, 0);
            sig_hl->setAlignment(Qt::AlignCenter);
            auto* sig_lbl = new QLabel(signal_text(ti.signal));
            sig_lbl->setAlignment(Qt::AlignCenter);
            sig_lbl->setFixedWidth(90);
            sig_lbl->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;background:%2;"
                                           "border-radius:2px;padding:2px 6px;border:0;").arg(sc, ui::colors::BG_RAISED));
            sig_hl->addWidget(sig_lbl);
            rl->addWidget(sig_w, 1);

            // Interpretation
            // Build the column key from the indicator for interpretation lookup
            QString col_key = ti.name.toLower().replace(' ', '_').replace(QString("%"), QString());
            // Special mappings
            if (ti.name == "Stoch %K") col_key = "stoch_k";
            else if (ti.name == "Stoch %D") col_key = "stoch_d";
            else if (ti.name == "Williams %R") col_key = "williams_r";
            else if (ti.name == "BB %B") col_key = "bb_pband";
            else if (ti.name == "BB Width") col_key = "bb_wband";
            else if (ti.name == "BB Mid") col_key = "bb_mavg";
            else if (ti.name == "BB Upper") col_key = "bb_hband";
            else if (ti.name == "BB Lower") col_key = "bb_lband";
            else if (ti.name == "Awesome Osc") col_key = "ao";
            else if (ti.name == "MACD Signal") col_key = "macd_signal";

            QString interp = interpretation(col_key, ti.value);
            auto* interp_lbl = new QLabel(interp.isEmpty() ? "\xe2\x80\x94" : interp);
            interp_lbl->setWordWrap(true);
            interp_lbl->setStyleSheet(QString("color:%1;font-size:11px;background:transparent;border:0;").arg(ui::colors::TEXT_TERTIARY));
            rl->addWidget(interp_lbl, 3);

            svl->addWidget(row);
            alt = !alt;
        }

        sc_vl->addWidget(section);
    }
}

// ── on_technicals_loaded ─────────────────────────────────────────────────────

void EquityTechnicalsTab::on_technicals_loaded(services::equity::TechnicalsData data) {
    if (data.symbol != current_symbol_)
        return;
    loading_overlay_->hide_loading();
    populate(data);
}

} // namespace fincept::screens
