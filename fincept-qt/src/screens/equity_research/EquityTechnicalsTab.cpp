// src/screens/equity_research/EquityTechnicalsTab.cpp
#include "screens/equity_research/EquityTechnicalsTab.h"

#include "services/equity/EquityResearchService.h"
#include "ui/theme/Theme.h"

#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QProgressBar>
#include <QScrollArea>
#include <QStyle>
#include <QVBoxLayout>

namespace fincept::screens {

// ── Accent colors without a theme token ──────────────────────────────────────
static constexpr const char* LTGREEN = "#4ade80";
static constexpr const char* LTRED = "#f87171";
static constexpr const char* CYAN = "#22d3ee";
static constexpr const char* YELLOW = "#eab308";
static constexpr const char* BLUE = "#60a5fa";
static constexpr const char* GRAY = "#6b7280";

// ── Signal helpers ───────────────────────────────────────────────────────────

namespace technicals_tab_detail {
void tech_repolish(QWidget* w) {
    w->style()->unpolish(w);
    w->style()->polish(w);
}
} // namespace technicals_tab_detail
using technicals_tab_detail::tech_repolish;

/// Stylesheet property value for a signal (QLabel[sig="buy"] …).
const char* EquityTechnicalsTab::signal_key(services::equity::TechSignal s) {
    using S = services::equity::TechSignal;
    switch (s) {
        case S::StrongBuy:
            return "strong_buy";
        case S::Buy:
            return "buy";
        case S::Sell:
            return "sell";
        case S::StrongSell:
            return "strong_sell";
        default:
            return "neutral";
    }
}

QString EquityTechnicalsTab::signal_text(services::equity::TechSignal s) const {
    using S = services::equity::TechSignal;
    switch (s) {
        case S::StrongBuy:
            return tr("STRONG BUY");
        case S::Buy:
            return tr("BUY");
        case S::Sell:
            return tr("SELL");
        case S::StrongSell:
            return tr("STRONG SELL");
        default:
            return tr("NEUTRAL");
    }
}

/// Context-aware interpretation for each indicator
QString EquityTechnicalsTab::interpretation(const QString& col_key, double value) const {
    if (col_key == "rsi") {
        if (value <= 25)
            return tr("Deeply oversold — potential reversal zone");
        if (value <= 30)
            return tr("Oversold — watch for bullish divergence");
        if (value <= 40)
            return tr("Below midpoint — bearish bias weakening");
        if (value <= 60)
            return tr("Neutral zone — no strong momentum");
        if (value <= 70)
            return tr("Above midpoint — bullish bias building");
        if (value <= 80)
            return tr("Overbought — watch for bearish divergence");
        return tr("Deeply overbought — potential reversal zone");
    }
    // MACD values are in price units, so only the sign is comparable across stocks.
    if (col_key == "macd")
        return value >= 0 ? tr("Above zero — 12-EMA over 26-EMA (uptrend bias)")
                          : tr("Below zero — 12-EMA under 26-EMA (downtrend bias)");
    if (col_key == "macd_diff")
        return value >= 0 ? tr("MACD above its signal line — bullish crossover state")
                          : tr("MACD below its signal line — bearish crossover state");
    if (col_key == "stoch_k" || col_key == "stoch_d") {
        if (value <= 20)
            return tr("Oversold zone — potential bounce");
        if (value <= 40)
            return tr("Below midpoint — watch for crossover");
        if (value <= 60)
            return tr("Neutral — consolidation phase");
        if (value <= 80)
            return tr("Above midpoint — bullish momentum");
        return tr("Overbought zone — potential pullback");
    }
    if (col_key == "williams_r") {
        if (value <= -80)
            return tr("Oversold — potential reversal up");
        if (value <= -50)
            return tr("Bearish territory — selling pressure");
        if (value >= -20)
            return tr("Overbought — potential reversal down");
        return tr("Neutral zone");
    }
    if (col_key == "cci") {
        if (value <= -200)
            return tr("Extreme oversold — deep value territory");
        if (value <= -100)
            return tr("Oversold — watch for trend reversal");
        if (value >= 200)
            return tr("Extreme overbought — euphoria zone");
        if (value >= 100)
            return tr("Overbought — watch for profit taking");
        return tr("Neutral — no extreme conditions");
    }
    if (col_key == "mfi") {
        if (value <= 20)
            return tr("Oversold — money flowing out aggressively");
        if (value <= 40)
            return tr("Weak inflow — cautious sentiment");
        if (value >= 80)
            return tr("Overbought — heavy money inflow");
        if (value >= 60)
            return tr("Strong inflow — buying pressure");
        return tr("Balanced money flow");
    }
    if (col_key == "adx") {
        if (value >= 50)
            return tr("Very strong trend — trade with the trend");
        if (value >= 25)
            return tr("Trending — directional move in play");
        if (value >= 20)
            return tr("Weak trend — possible consolidation");
        return tr("No trend — range-bound market");
    }
    if (col_key == "bb_pband") {
        if (value < 0)
            return tr("Below lower band — extreme oversold");
        if (value < 0.2)
            return tr("Near lower band — oversold zone");
        if (value > 1.0)
            return tr("Above upper band — extreme overbought");
        if (value > 0.8)
            return tr("Near upper band — overbought zone");
        return tr("Within bands — normal range");
    }
    // bb_wband is (upper − lower) / middle × 100 — already a percentage.
    if (col_key == "bb_wband") {
        if (value < 5.0)
            return tr("Tight squeeze — volatility compressed, watch for a breakout");
        if (value < 10.0)
            return tr("Narrowing bands — volatility contracting");
        if (value > 30.0)
            return tr("Wide bands — high volatility");
        return tr("Normal bandwidth");
    }
    if (col_key == "atr")
        return tr("Average true range — use for stop-loss sizing");
    if (col_key == "roc") {
        if (value > 5)
            return tr("Strong upward momentum");
        if (value < -5)
            return tr("Strong downward momentum");
        return tr("Flat momentum — sideways movement");
    }
    if (col_key == "cmf") {
        if (value > 0.1)
            return tr("Buying pressure — accumulation");
        if (value < -0.1)
            return tr("Selling pressure — distribution");
        return tr("Balanced — no clear accumulation or distribution");
    }
    if (col_key == "aroon_up") {
        if (value >= 70)
            return tr("Strong uptrend — recent new highs");
        if (value <= 30)
            return tr("Weak upside — no recent highs");
        return tr("Moderate — watching for trend development");
    }
    if (col_key == "aroon_down") {
        if (value >= 70)
            return tr("Strong downtrend — recent new lows");
        if (value <= 30)
            return tr("Weak downside — no recent lows");
        return tr("Moderate — watching for trend development");
    }
    if (col_key == "obv")
        return tr("On-balance volume — confirms price trend with volume");
    if (col_key == "vwap")
        return tr("14-bar rolling volume-weighted price (not the intraday session VWAP)");
    if (col_key == "adi")
        return tr("Accumulation/distribution — confirms money flow");
    if (col_key.startsWith("sma_") || col_key.startsWith("ema_") || col_key.startsWith("wma_") || col_key == "kama")
        return tr("Moving average — price above = bullish, below = bearish");
    if (col_key == "macd_signal")
        return tr("Signal line — crossover with MACD triggers trade");
    if (col_key == "bb_mavg")
        return tr("Bollinger midline (20-SMA) — dynamic support/resistance");
    if (col_key == "bb_hband")
        return tr("Upper band — resistance level, overbought above");
    if (col_key == "bb_lband")
        return tr("Lower band — support level, oversold below");
    if (col_key == "ao")
        return value > 0 ? tr("Bullish — momentum above zero line") : tr("Bearish — momentum below zero line");
    return QString();
}

// ── Constructor ──────────────────────────────────────────────────────────────

EquityTechnicalsTab::EquityTechnicalsTab(QWidget* parent) : QWidget(parent) {
    build_ui();
    auto& svc = services::equity::EquityResearchService::instance();
    connect(&svc, &services::equity::EquityResearchService::technicals_loaded, this,
            &EquityTechnicalsTab::on_technicals_loaded);
    // Without this a failed fetch/compute leaves the overlay spinning "COMPUTING
    // INDICATORS…" forever (the overlay is hidden only on the success path).
    connect(&svc, &services::equity::EquityResearchService::error_occurred, this,
            [this](const QString& ctx, const QString&) {
                if (ctx != "Technicals" && ctx != "Historical")
                    return;
                if (loading_overlay_)
                    loading_overlay_->hide_loading();
                // Nothing computed for this symbol: don't leave the previous symbol's rating and
                // indicator tables on screen under the new symbol's name.
                if (!data_loaded_) {
                    populate(services::equity::TechnicalsData{});
                    rating_label_->setText(QString::fromUtf8("\xe2\x80\x94"));
                }
            });
}

void EquityTechnicalsTab::set_symbol(const QString& symbol) {
    if (symbol == current_symbol_)
        return;
    current_symbol_ = symbol;
    data_loaded_ = false; // the figures on screen belong to the previous symbol until new ones land
    loading_overlay_->show_loading(tr("COMPUTING INDICATORS…"));
    services::equity::EquityResearchService::instance().fetch_technicals(symbol, "1y");
}

// ── build_ui ─────────────────────────────────────────────────────────────────

void EquityTechnicalsTab::build_ui() {
    setObjectName(QStringLiteral("equityTechnicalsTab"));
    // One stylesheet for every dynamic card / row populate() builds: they are styled by
    // objectName and a "sig"/"cat"/"alt" property. populate() used to issue ~10 setStyleSheet
    // calls per indicator — a full CSS re-parse each — on every load.
    setStyleSheet(
        QString("QWidget { background:%1; }"
                "QLabel { background:transparent; border:0; }"
                "QFrame#techCard { background:%2; border:1px solid %3; border-left:3px solid %4; }"
                "QFrame#techCard[sig=\"strong_buy\"] { border-left-color:%5; }"
                "QFrame#techCard[sig=\"buy\"] { border-left-color:%6; }"
                "QFrame#techCard[sig=\"sell\"] { border-left-color:%7; }"
                "QFrame#techCard[sig=\"strong_sell\"] { border-left-color:%8; }"
                "QLabel#techCardName { color:%9; font-size:10px; font-weight:600; letter-spacing:0.5px; }"
                "QLabel#techCardValue { color:%10; font-size:16px; font-weight:700; font-family:'Consolas',monospace; }"
                "QLabel#techCardDesc { color:%11; font-size:10px; }"
                "QLabel#techRowInterp { color:%11; font-size:11px; }"
                "QLabel[sig=\"strong_buy\"] { color:%5; } QLabel[sig=\"buy\"] { color:%6; }"
                "QLabel[sig=\"neutral\"] { color:%4; } QLabel[sig=\"sell\"] { color:%7; }"
                "QLabel[sig=\"strong_sell\"] { color:%8; }"
                "QLabel#techSig { font-size:10px; font-weight:700; }"
                "QLabel#techSigPill { font-size:10px; font-weight:700; background:%2; padding:2px 6px; }"
                "QLabel#techRating { font-size:22px; font-weight:700; letter-spacing:2px; color:%4; }"
                "QFrame#techSection { background:%12; border:1px solid %3; }"
                "QWidget#techSectionHdr { background:%2; border:0; border-bottom:1px solid %3; }"
                "QLabel#techSectionTitle { font-size:12px; font-weight:700; letter-spacing:1px; }"
                "QLabel#techSectionTitle[cat=\"trend\"] { color:%13; }"
                "QLabel#techSectionTitle[cat=\"momentum\"] { color:%5; }"
                "QLabel#techSectionTitle[cat=\"volatility\"] { color:%14; }"
                "QLabel#techSectionTitle[cat=\"volume\"] { color:%15; }"
                "QLabel#techMuted { color:%11; font-size:10px; }"
                "QLabel#techColHdr { color:%11; font-size:10px; font-weight:600; letter-spacing:0.5px; }"
                "QLabel#techBadge { font-size:10px; font-weight:700; padding:2px 6px; }"
                "QWidget#techTblHdr { background:%1; border:0; border-bottom:1px solid %3; }"
                "QWidget#techRow { background:%1; border:0; border-bottom:1px solid %3; }"
                "QWidget#techRow[alt=\"true\"] { background:%2; }"
                "QWidget#techSigCell { background:transparent; border:0; }"
                "QLabel#techRowName { color:%10; font-size:12px; font-weight:600; }"
                "QLabel#techRowValue { color:%9; font-size:12px; font-family:'Consolas',monospace; }")
            .arg(ui::colors::BG_BASE(), ui::colors::BG_RAISED(), ui::colors::BORDER_DIM(), QString(GRAY),
                 ui::colors::POSITIVE(), QString(LTGREEN), QString(LTRED), ui::colors::NEGATIVE(),
                 ui::colors::TEXT_SECONDARY())
            .arg(ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_TERTIARY(), ui::colors::BG_SURFACE(), QString(CYAN),
                 QString(YELLOW), QString(BLUE)));
    loading_overlay_ = new ui::LoadingOverlay(this);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("background:transparent;border:0;");

    auto* content = new QWidget(this);
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(8);

    // ── Top row: RATING + KEY INDICATORS ─────────────────────────────────────
    auto* top_row = new QHBoxLayout;
    top_row->setSpacing(8);

    // === RATING PANEL ===
    auto* rating_panel = new QFrame;
    rating_panel->setFixedWidth(260);
    rating_panel->setStyleSheet(QString("QFrame{background:%1;border:1px solid %2;border-radius:2px;}")
                                    .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* rp_vl = new QVBoxLayout(rating_panel);
    rp_vl->setContentsMargins(14, 10, 14, 10);
    rp_vl->setSpacing(10);

    rating_title_ = new QLabel(tr("TECHNICAL RATING"));
    rating_title_->setStyleSheet(
        QString("color:%1;font-size:12px;font-weight:700;letter-spacing:1px;background:transparent;border:0;")
            .arg(ui::colors::AMBER()));
    rp_vl->addWidget(rating_title_);

    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background:%1;border:0;").arg(ui::colors::BORDER_DIM()));
    rp_vl->addWidget(sep);

    rating_label_ = new QLabel("\xe2\x80\x94");
    rating_label_->setObjectName(QStringLiteral("techRating"));
    rating_label_->setAlignment(Qt::AlignCenter);
    rp_vl->addWidget(rating_label_);

    // Gauge bar
    gauge_bar_ = new QProgressBar;
    gauge_bar_->setRange(0, 100);
    gauge_bar_->setValue(50);
    gauge_bar_->setFixedHeight(6);
    gauge_bar_->setTextVisible(false);
    gauge_bar_->setStyleSheet(QString("QProgressBar{background:%1;border:1px solid %2;border-radius:0;}"
                                      "QProgressBar::chunk{background:%3;}")
                                  .arg(ui::colors::NEGATIVE(), ui::colors::BORDER_DIM(), ui::colors::POSITIVE()));
    rp_vl->addWidget(gauge_bar_);

    // Signal counts — 5 columns
    auto* counts = new QHBoxLayout;
    counts->setSpacing(4);

    auto make_count = [&](const char* color, const QString& label, QLabel*& out, QLabel*& caption_out) {
        auto* w = new QWidget(this);
        w->setStyleSheet(QString("background:%1;border:0;").arg(ui::colors::BG_RAISED()));
        auto* cvl = new QVBoxLayout(w);
        cvl->setContentsMargins(4, 4, 4, 4);
        cvl->setSpacing(1);
        cvl->setAlignment(Qt::AlignCenter);
        out = new QLabel("0");
        out->setAlignment(Qt::AlignCenter);
        out->setStyleSheet(
            QString("color:%1;font-size:16px;font-weight:700;background:transparent;border:0;").arg(color));
        caption_out = new QLabel(label);
        caption_out->setAlignment(Qt::AlignCenter);
        caption_out->setStyleSheet(
            QString("color:%1;font-size:8px;font-weight:600;letter-spacing:0.5px;background:transparent;border:0;")
                .arg(ui::colors::TEXT_TERTIARY()));
        cvl->addWidget(out);
        cvl->addWidget(caption_out);
        counts->addWidget(w, 1);
    };

    make_count(ui::colors::POSITIVE, tr("STR.BUY"), strong_buy_count_, str_buy_caption_);
    make_count(LTGREEN, tr("BUY"), buy_count_, buy_caption_);
    make_count(GRAY, tr("NEUTRAL"), neutral_count_, neutral_caption_);
    make_count(LTRED, tr("SELL"), sell_count_, sell_caption_);
    make_count(ui::colors::NEGATIVE, tr("STR.SELL"), strong_sell_count_, str_sell_caption_);
    rp_vl->addLayout(counts);

    total_label_ = new QLabel(tr("0 INDICATORS"));
    total_label_->setAlignment(Qt::AlignCenter);
    total_label_->setStyleSheet(QString("color:%1;font-size:10px;letter-spacing:1px;background:transparent;border:0;")
                                    .arg(ui::colors::TEXT_TERTIARY()));
    rp_vl->addWidget(total_label_);
    rp_vl->addStretch();

    top_row->addWidget(rating_panel);

    // === KEY INDICATORS PANEL ===
    auto* key_panel = new QFrame;
    key_panel->setStyleSheet(QString("QFrame{background:%1;border:1px solid %2;border-radius:2px;}")
                                 .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* kp_vl = new QVBoxLayout(key_panel);
    kp_vl->setContentsMargins(10, 10, 10, 10);
    kp_vl->setSpacing(6);

    key_title_ = new QLabel(tr("KEY INDICATORS"));
    key_title_->setStyleSheet(
        QString("color:%1;font-size:12px;font-weight:700;letter-spacing:1px;background:transparent;border:0;")
            .arg(CYAN));
    kp_vl->addWidget(key_title_);

    auto* ksep = new QFrame;
    ksep->setFrameShape(QFrame::HLine);
    ksep->setFixedHeight(1);
    ksep->setStyleSheet(QString("background:%1;border:0;").arg(ui::colors::BORDER_DIM()));
    kp_vl->addWidget(ksep);

    key_container_ = new QWidget(this);
    key_container_->setStyleSheet("background:transparent;border:0;");
    auto* kc_layout = new QGridLayout(key_container_);
    kc_layout->setContentsMargins(0, 0, 0, 0);
    kc_layout->setSpacing(6);
    kp_vl->addWidget(key_container_, 1);

    top_row->addWidget(key_panel, 1);
    vl->addLayout(top_row);

    // ── Category sections ────────────────────────────────────────────────────
    sections_container_ = new QWidget(this);
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
            if (item->widget())
                item->widget()->deleteLater();
            delete item;
        }
    }
    // Clear category sections
    if (auto* vl = qobject_cast<QVBoxLayout*>(sections_container_->layout())) {
        while (vl->count() > 0) {
            auto* item = vl->takeAt(0);
            if (item->widget())
                item->widget()->deleteLater();
            delete item;
        }
    }
}

// ── populate ─────────────────────────────────────────────────────────────────

void EquityTechnicalsTab::populate(const services::equity::TechnicalsData& payload) {
    clear_sections();

    QVector<services::equity::TechIndicator> all;
    all << payload.trend << payload.momentum << payload.volatility << payload.volume;

    // ── Rating ───────────────────────────────────────────────────────────────
    const int sb = payload.strong_buy, b = payload.buy, n = payload.neutral, s = payload.sell,
              ss = payload.strong_sell;
    const int total = sb + b + n + s + ss;
    rating_label_->setText(signal_text(payload.overall_signal));
    rating_label_->setProperty("sig", QString::fromLatin1(signal_key(payload.overall_signal)));
    tech_repolish(rating_label_);
    gauge_bar_->setValue(total > 0 ? static_cast<int>(100.0 * (sb + b) / total) : 50);
    strong_buy_count_->setText(QString::number(sb));
    buy_count_->setText(QString::number(b));
    neutral_count_->setText(QString::number(n));
    sell_count_->setText(QString::number(s));
    strong_sell_count_->setText(QString::number(ss));
    total_label_->setText(tr("%1 INDICATORS ANALYZED").arg(total));

    // ── Key indicators — the most decision-relevant ones ─────────────────────
    static const QStringList key_names = {"RSI", "MACD Hist", "Stoch %K",    "ADX", "CCI",
                                          "MFI", "BB %B",     "Williams %R", "ATR", "CMF"};
    QMap<QString, services::equity::TechIndicator> name_map;
    for (const auto& ti : all)
        name_map[ti.name] = ti;

    auto* kg = qobject_cast<QGridLayout*>(key_container_->layout());
    int ki_row = 0, ki_col = 0;
    for (const auto& key : key_names) {
        auto it = name_map.find(key);
        if (it == name_map.end())
            continue;
        const auto& ti = it.value();
        const QString sig = QString::fromLatin1(signal_key(ti.signal));

        auto* card = new QFrame;
        card->setObjectName(QStringLiteral("techCard"));
        card->setProperty("sig", sig);
        auto* cl = new QVBoxLayout(card);
        cl->setContentsMargins(10, 6, 10, 6);
        cl->setSpacing(2);
        auto* nm = new QLabel(ti.name.toUpper());
        nm->setObjectName(QStringLiteral("techCardName"));
        cl->addWidget(nm);

        auto* vr = new QHBoxLayout;
        vr->setSpacing(8);
        auto* val = new QLabel(QString::number(ti.value, 'f', 2));
        val->setObjectName(QStringLiteral("techCardValue"));
        auto* sig_lbl = new QLabel(signal_text(ti.signal));
        sig_lbl->setObjectName(QStringLiteral("techSig"));
        sig_lbl->setProperty("sig", sig);
        sig_lbl->setAlignment(Qt::AlignRight | Qt::AlignBottom);
        vr->addWidget(val);
        vr->addStretch();
        vr->addWidget(sig_lbl);
        cl->addLayout(vr);

        const QString interp = interpretation(ti.key, ti.value);
        if (!interp.isEmpty()) {
            auto* desc = new QLabel(interp);
            desc->setObjectName(QStringLiteral("techCardDesc"));
            desc->setWordWrap(true);
            cl->addWidget(desc);
        }
        kg->addWidget(card, ki_row, ki_col);
        if (++ki_col == 5) {
            ki_col = 0;
            ++ki_row;
        }
    }

    // ── Category sections ────────────────────────────────────────────────────
    auto* sc_vl = qobject_cast<QVBoxLayout*>(sections_container_->layout());
    struct CatDef {
        QString title;
        const QVector<services::equity::TechIndicator>* inds;
        const char* cat;
    };
    const QList<CatDef> cats = {
        {tr("TREND INDICATORS"), &payload.trend, "trend"},
        {tr("MOMENTUM INDICATORS"), &payload.momentum, "momentum"},
        {tr("VOLATILITY INDICATORS"), &payload.volatility, "volatility"},
        {tr("VOLUME INDICATORS"), &payload.volume, "volume"},
    };

    for (const auto& cat : cats) {
        if (!cat.inds || cat.inds->isEmpty())
            continue;
        int cb = 0, cs = 0, cn = 0;
        for (const auto& ti : *cat.inds) {
            using S = services::equity::TechSignal;
            if (ti.signal == S::StrongBuy || ti.signal == S::Buy)
                ++cb;
            else if (ti.signal == S::StrongSell || ti.signal == S::Sell)
                ++cs;
            else
                ++cn;
        }

        auto* section = new QFrame;
        section->setObjectName(QStringLiteral("techSection"));
        auto* svl = new QVBoxLayout(section);
        svl->setContentsMargins(0, 0, 0, 0);
        svl->setSpacing(0);

        auto* hdr = new QWidget(this);
        hdr->setObjectName(QStringLiteral("techSectionHdr"));
        auto* hl = new QHBoxLayout(hdr);
        hl->setContentsMargins(12, 8, 12, 8);
        hl->setSpacing(10);
        auto* htitle = new QLabel(cat.title);
        htitle->setObjectName(QStringLiteral("techSectionTitle"));
        htitle->setProperty("cat", QString::fromLatin1(cat.cat));
        hl->addWidget(htitle);
        auto* cnt = new QLabel(tr("%1 indicators").arg(cat.inds->size()));
        cnt->setObjectName(QStringLiteral("techMuted"));
        hl->addWidget(cnt);
        hl->addStretch();
        auto add_badge = [&](int count, const QString& label, const char* sig) {
            if (count == 0)
                return;
            auto* badge = new QLabel(QString("%1 %2").arg(count).arg(label));
            badge->setObjectName(QStringLiteral("techBadge"));
            badge->setProperty("sig", QString::fromLatin1(sig));
            hl->addWidget(badge);
        };
        add_badge(cb, tr("BUY"), "strong_buy");
        add_badge(cn, tr("HOLD"), "neutral");
        add_badge(cs, tr("SELL"), "strong_sell");
        svl->addWidget(hdr);

        auto* tbl_hdr = new QWidget(this);
        tbl_hdr->setObjectName(QStringLiteral("techTblHdr"));
        auto* thl = new QHBoxLayout(tbl_hdr);
        thl->setContentsMargins(12, 4, 12, 4);
        thl->setSpacing(0);
        auto hdr_lbl = [&](const QString& text, int stretch, Qt::Alignment align = Qt::AlignLeft) {
            auto* l = new QLabel(text);
            l->setObjectName(QStringLiteral("techColHdr"));
            l->setAlignment(align | Qt::AlignVCenter);
            thl->addWidget(l, stretch);
        };
        hdr_lbl(tr("INDICATOR"), 2);
        hdr_lbl(tr("VALUE"), 1, Qt::AlignRight);
        hdr_lbl(tr("SIGNAL"), 1, Qt::AlignCenter);
        hdr_lbl(tr("INTERPRETATION"), 3);
        svl->addWidget(tbl_hdr);

        bool alt = false;
        for (const auto& ti : *cat.inds) {
            auto* row = new QWidget(this);
            row->setObjectName(QStringLiteral("techRow"));
            row->setProperty("alt", alt);
            auto* rl = new QHBoxLayout(row);
            rl->setContentsMargins(12, 5, 12, 5);
            rl->setSpacing(0);

            auto* name_lbl = new QLabel(ti.name);
            name_lbl->setObjectName(QStringLiteral("techRowName"));
            rl->addWidget(name_lbl, 2);

            auto* val_lbl = new QLabel(QString::number(ti.value, 'f', 4));
            val_lbl->setObjectName(QStringLiteral("techRowValue"));
            val_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            rl->addWidget(val_lbl, 1);

            auto* sig_w = new QWidget(this);
            sig_w->setObjectName(QStringLiteral("techSigCell"));
            auto* sig_hl = new QHBoxLayout(sig_w);
            sig_hl->setContentsMargins(0, 0, 0, 0);
            sig_hl->setAlignment(Qt::AlignCenter);
            auto* sig_lbl = new QLabel(signal_text(ti.signal));
            sig_lbl->setObjectName(QStringLiteral("techSigPill"));
            sig_lbl->setProperty("sig", QString::fromLatin1(signal_key(ti.signal)));
            sig_lbl->setAlignment(Qt::AlignCenter);
            sig_lbl->setFixedWidth(90);
            sig_hl->addWidget(sig_lbl);
            rl->addWidget(sig_w, 1);

            const QString interp = interpretation(ti.key, ti.value);
            auto* interp_lbl = new QLabel(interp.isEmpty() ? QString::fromUtf8("\xe2\x80\x94") : interp);
            interp_lbl->setObjectName(QStringLiteral("techRowInterp"));
            interp_lbl->setWordWrap(true);
            rl->addWidget(interp_lbl, 3);

            svl->addWidget(row);
            alt = !alt;
        }
        sc_vl->addWidget(section);
    }
}

// ── on_technicals_loaded ─────────────────────────────────────────────────────

void EquityTechnicalsTab::on_technicals_loaded(services::equity::TechnicalsData payload) {
    if (payload.symbol != current_symbol_)
        return;
    loading_overlay_->hide_loading();
    cached_data_ = payload;
    data_loaded_ = true;
    populate(payload);
}

// ── Re-translation ───────────────────────────────────────────────────────────
// populate() rebuilds every dynamic section (key indicator cards, category
// sections, table rows). Calling it with the cached payload on language
// change re-renders every signal label and interpretation string in the new
// locale. Static chrome (panel titles, signal-count captions) is re-set
// explicitly here.

void EquityTechnicalsTab::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void EquityTechnicalsTab::retranslateUi() {
    if (rating_title_)
        rating_title_->setText(tr("TECHNICAL RATING"));
    if (key_title_)
        key_title_->setText(tr("KEY INDICATORS"));
    if (str_buy_caption_)
        str_buy_caption_->setText(tr("STR.BUY"));
    if (buy_caption_)
        buy_caption_->setText(tr("BUY"));
    if (neutral_caption_)
        neutral_caption_->setText(tr("NEUTRAL"));
    if (sell_caption_)
        sell_caption_->setText(tr("SELL"));
    if (str_sell_caption_)
        str_sell_caption_->setText(tr("STR.SELL"));

    if (data_loaded_) {
        populate(cached_data_);
    } else if (total_label_) {
        total_label_->setText(tr("0 INDICATORS"));
    }
}

} // namespace fincept::screens
