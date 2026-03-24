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

// ── Layout constants ──────────────────────────────────────────────────────────
static constexpr struct {
    const char* TREND = "#22d3ee";      // cyan
    const char* MOMENTUM = "#22c55e";   // green
    const char* VOLATILITY = "#eab308"; // yellow
    const char* VOLUME = "#60a5fa";     // blue
    const char* OTHERS = "#a855f7";     // purple
} CAT_COLORS;

// Must match display names set in parse_technicals kMomentum/kTrend/kVolatility lists
static const QStringList KEY_INDICATORS = {"RSI", "MACD", "Stoch %K", "ADX", "CCI", "MFI", "BB %B", "Williams %R"};

// ── Signal helpers ────────────────────────────────────────────────────────────
QString EquityTechnicalsTab::signal_label(services::equity::TechSignal s) {
    switch (s) {
        case services::equity::TechSignal::StrongBuy:
            return "STRONG BUY";
        case services::equity::TechSignal::Buy:
            return "BUY";
        case services::equity::TechSignal::Sell:
            return "SELL";
        case services::equity::TechSignal::StrongSell:
            return "STRONG SELL";
        default:
            return "NEUTRAL";
    }
}

QString EquityTechnicalsTab::signal_color(services::equity::TechSignal s) {
    switch (s) {
        case services::equity::TechSignal::StrongBuy:
            return "#22c55e";
        case services::equity::TechSignal::Buy:
            return "#4ade80";
        case services::equity::TechSignal::Sell:
            return "#f87171";
        case services::equity::TechSignal::StrongSell:
            return "#ef4444";
        default:
            return "#6b7280";
    }
}

QString EquityTechnicalsTab::signal_bg(services::equity::TechSignal s) {
    switch (s) {
        case services::equity::TechSignal::StrongBuy:
            return "#052e16";
        case services::equity::TechSignal::Buy:
            return "#14532d";
        case services::equity::TechSignal::Sell:
            return "#450a0a";
        case services::equity::TechSignal::StrongSell:
            return "#7f1d1d";
        default:
            return ui::colors::BG_RAISED;
    }
}

// Threshold-based signal — ti.name is the human display name set in parse_technicals
QString EquityTechnicalsTab::compute_signal(const services::equity::TechIndicator& ti) {
    // Just use the pre-computed signal from the service (score_indicator already ran)
    switch (ti.signal) {
        case services::equity::TechSignal::StrongBuy:
        case services::equity::TechSignal::Buy:
            return "BUY";
        case services::equity::TechSignal::Sell:
        case services::equity::TechSignal::StrongSell:
            return "SELL";
        default:
            return "NEUTRAL";
    }
}

// ── Constructor ───────────────────────────────────────────────────────────────
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
    loading_overlay_->show_loading("COMPUTING INDICATORS…");
    rating_label_->setText("COMPUTING INDICATORS…");
    rating_label_->setStyleSheet(QString("color:%1; font-size:20px; font-weight:700; letter-spacing:2px; "
                                         "background:transparent; border:0;")
                                     .arg(ui::colors::AMBER));
    services::equity::EquityResearchService::instance().fetch_technicals(symbol, "1y");
}

// ── build_ui ──────────────────────────────────────────────────────────────────
void EquityTechnicalsTab::build_ui() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
    loading_overlay_ = new ui::LoadingOverlay(this);
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // ── Scroll wrapper for everything ─────────────────────────────────────────
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("background:transparent; border:0;");

    auto* content = new QWidget;
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(10, 10, 10, 10);
    vl->setSpacing(8);

    // ── Top row: TECHNICAL RATING panel + KEY INDICATORS panel ───────────────
    auto* top_row = new QHBoxLayout;
    top_row->setSpacing(8);

    // ── TECHNICAL RATING panel (fixed 280px, orange header) ──────────────────
    auto* rating_panel = new QFrame;
    rating_panel->setFixedWidth(280);
    rating_panel->setStyleSheet(
        QString("QFrame { background:%1; border:1px solid %2; }").arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));
    auto* rp_vl = new QVBoxLayout(rating_panel);
    rp_vl->setContentsMargins(0, 0, 0, 0);
    rp_vl->setSpacing(0);

    // Header
    auto* rp_hdr = new QWidget;
    rp_hdr->setStyleSheet(
        QString("background:%1; border-bottom:2px solid %2;").arg(ui::colors::BG_RAISED, ui::colors::AMBER));
    auto* rp_hdr_l = new QHBoxLayout(rp_hdr);
    rp_hdr_l->setContentsMargins(12, 8, 12, 8);
    auto* rp_title = new QLabel("TECHNICAL RATING");
    rp_title->setStyleSheet(QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px; "
                                    "background:transparent; border:0;")
                                .arg(ui::colors::AMBER));
    rp_hdr_l->addWidget(rp_title);
    rp_vl->addWidget(rp_hdr);

    // Rating content
    auto* rp_body = new QWidget;
    rp_body->setStyleSheet("background:transparent;");
    auto* rp_body_vl = new QVBoxLayout(rp_body);
    rp_body_vl->setContentsMargins(14, 16, 14, 16);
    rp_body_vl->setSpacing(12);
    rp_body_vl->setAlignment(Qt::AlignHCenter);

    rating_label_ = new QLabel("—");
    rating_label_->setAlignment(Qt::AlignCenter);
    rating_label_->setStyleSheet(QString("color:#6b7280; font-size:20px; font-weight:700; letter-spacing:2px; "
                                         "background:transparent; border:0;"));
    rp_body_vl->addWidget(rating_label_);

    // Gauge bar: SELL | NEUTRAL | BUY
    auto* gauge_row = new QHBoxLayout;
    gauge_row->setSpacing(4);
    auto* sell_icon = new QLabel("▼");
    sell_icon->setStyleSheet("color:#ef4444; font-size:10px; background:transparent; border:0;");
    gauge_bar_ = new QProgressBar;
    gauge_bar_->setRange(0, 100);
    gauge_bar_->setValue(50);
    gauge_bar_->setFixedHeight(6);
    gauge_bar_->setTextVisible(false);
    gauge_bar_->setStyleSheet(QString("QProgressBar { background:%1; border:1px solid %2; border-radius:0; }"
                                      "QProgressBar::chunk { background:#22c55e; }")
                                  .arg(ui::colors::BG_BASE, ui::colors::BORDER_DIM));
    auto* buy_icon = new QLabel("▲");
    buy_icon->setStyleSheet("color:#22c55e; font-size:10px; background:transparent; border:0;");
    gauge_row->addWidget(sell_icon);
    gauge_row->addWidget(gauge_bar_, 1);
    gauge_row->addWidget(buy_icon);
    rp_body_vl->addLayout(gauge_row);

    // BUY | HOLD | SELL counts
    auto* counts_row = new QHBoxLayout;
    counts_row->setSpacing(6);

    auto make_count_cell = [&](const QString& color, const QString& label_text, QLabel*& count_out) -> QWidget* {
        auto* w = new QWidget;
        w->setStyleSheet(QString("background:%1; border-left:2px solid %2;").arg(color + "10", color));
        auto* wl = new QVBoxLayout(w);
        wl->setContentsMargins(8, 6, 8, 6);
        wl->setSpacing(2);
        wl->setAlignment(Qt::AlignHCenter);
        count_out = new QLabel("0");
        count_out->setAlignment(Qt::AlignCenter);
        count_out->setStyleSheet(
            QString("color:%1; font-size:16px; font-weight:700; background:transparent; border:0;").arg(color));
        auto* lbl = new QLabel(label_text);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet(QString("color:#6b7280; font-size:9px; background:transparent; border:0;"));
        wl->addWidget(count_out);
        wl->addWidget(lbl);
        return w;
    };

    counts_row->addWidget(make_count_cell("#22c55e", "BUY", buy_count_));
    counts_row->addWidget(make_count_cell("#6b7280", "HOLD", neutral_count_));
    counts_row->addWidget(make_count_cell("#ef4444", "SELL", sell_count_));
    rp_body_vl->addLayout(counts_row);

    total_label_ = new QLabel("0 INDICATORS ANALYZED");
    total_label_->setAlignment(Qt::AlignCenter);
    total_label_->setStyleSheet("color:#4b5563; font-size:9px; letter-spacing:1px; background:transparent; border:0;");
    rp_body_vl->addWidget(total_label_);
    rp_body_vl->addStretch();

    rp_vl->addWidget(rp_body, 1);
    top_row->addWidget(rating_panel);

    // ── KEY INDICATORS panel (flex, cyan header) ──────────────────────────────
    auto* key_panel = new QFrame;
    key_panel->setStyleSheet(
        QString("QFrame { background:%1; border:1px solid %2; }").arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));
    auto* kp_vl = new QVBoxLayout(key_panel);
    kp_vl->setContentsMargins(0, 0, 0, 0);
    kp_vl->setSpacing(0);

    auto* kp_hdr = new QWidget;
    kp_hdr->setStyleSheet(QString("background:%1; border-bottom:2px solid #22d3ee;").arg(ui::colors::BG_RAISED));
    auto* kp_hdr_l = new QHBoxLayout(kp_hdr);
    kp_hdr_l->setContentsMargins(12, 8, 12, 8);
    auto* kp_title = new QLabel("KEY INDICATORS");
    kp_title->setStyleSheet("color:#22d3ee; font-size:11px; font-weight:700; letter-spacing:1px; "
                            "background:transparent; border:0;");
    kp_hdr_l->addWidget(kp_title);
    kp_vl->addWidget(kp_hdr);

    key_grid_ = new QWidget;
    key_grid_->setStyleSheet("background:transparent;");
    auto* kg_layout = new QGridLayout(key_grid_);
    kg_layout->setContentsMargins(10, 10, 10, 10);
    kg_layout->setSpacing(6);

    // Pre-create 8 placeholder cards — will be filled in populate()
    for (int i = 0; i < 8; ++i) {
        auto* card = new QFrame;
        card->setObjectName(QString("key_card_%1").arg(i));
        card->setStyleSheet(QString("QFrame { background:%1; border:1px solid %2; border-left:2px solid #6b7280; }")
                                .arg(ui::colors::BG_BASE, ui::colors::BORDER_DIM));
        auto* cl = new QVBoxLayout(card);
        cl->setContentsMargins(8, 6, 8, 6);
        cl->setSpacing(3);

        auto* name_lbl = new QLabel("—");
        name_lbl->setObjectName("name");
        name_lbl->setStyleSheet(
            "color:#6b7280; font-size:9px; letter-spacing:0.5px; background:transparent; border:0;");
        auto* val_lbl = new QLabel("—");
        val_lbl->setObjectName("value");
        val_lbl->setStyleSheet("color:#f9fafb; font-size:14px; font-weight:700; font-family:monospace; "
                               "background:transparent; border:0;");
        auto* sig_lbl = new QLabel("—");
        sig_lbl->setObjectName("signal");
        sig_lbl->setStyleSheet("color:#6b7280; font-size:9px; font-weight:700; background:transparent; border:0;");

        cl->addWidget(name_lbl);
        cl->addWidget(val_lbl);
        cl->addWidget(sig_lbl);
        kg_layout->addWidget(card, i / 4, i % 4);
    }

    kp_vl->addWidget(key_grid_, 1);
    top_row->addWidget(key_panel, 1);
    vl->addLayout(top_row);

    // ── Scrollable category sections ──────────────────────────────────────────
    sections_container_ = new QWidget;
    sections_container_->setStyleSheet("background:transparent;");
    auto* sc_vl = new QVBoxLayout(sections_container_);
    sc_vl->setContentsMargins(0, 0, 0, 0);
    sc_vl->setSpacing(4);
    vl->addWidget(sections_container_);

    // ── Trading Notes panel ───────────────────────────────────────────────────
    auto* notes_panel = new QFrame;
    notes_panel->setStyleSheet(
        QString("QFrame { background:%1; border:1px solid %2; }").arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));
    auto* np_vl = new QVBoxLayout(notes_panel);
    np_vl->setContentsMargins(0, 0, 0, 0);
    np_vl->setSpacing(0);

    auto* np_hdr = new QWidget;
    np_hdr->setStyleSheet(
        QString("background:%1; border-bottom:2px solid %2;").arg(ui::colors::BG_RAISED, ui::colors::AMBER));
    auto* np_hdr_l = new QHBoxLayout(np_hdr);
    np_hdr_l->setContentsMargins(12, 8, 12, 8);
    auto* np_title = new QLabel("⚠  TRADING NOTES");
    np_title->setStyleSheet(QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px; "
                                    "background:transparent; border:0;")
                                .arg(ui::colors::AMBER));
    np_hdr_l->addWidget(np_title);
    np_vl->addWidget(np_hdr);

    auto* np_body = new QWidget;
    np_body->setStyleSheet("background:transparent;");
    auto* np_body_vl = new QVBoxLayout(np_body);
    np_body_vl->setContentsMargins(14, 12, 14, 12);
    np_body_vl->setSpacing(6);

    static const QStringList notes = {
        "Technical indicators should be used in conjunction with fundamental analysis and risk management.",
        "Signal strength increases when multiple indicators align (confluence).",
        "Overbought/oversold conditions can persist in strong trends — use with trend confirmation.",
        "Past performance does not guarantee future results. Always use stop-losses.",
    };
    for (const auto& note : notes) {
        auto* row = new QHBoxLayout;
        row->setSpacing(8);
        auto* dot = new QLabel("•");
        dot->setStyleSheet("color:#4b5563; font-size:12px; background:transparent; border:0;");
        dot->setFixedWidth(10);
        auto* txt = new QLabel(note);
        txt->setWordWrap(true);
        txt->setStyleSheet("color:#6b7280; font-size:11px; line-height:1.6; background:transparent; border:0;");
        row->addWidget(dot, 0, Qt::AlignTop);
        row->addWidget(txt, 1);
        np_body_vl->addLayout(row);
    }
    np_vl->addWidget(np_body);
    vl->addWidget(notes_panel);
    vl->addStretch();

    scroll->setWidget(content);
    outer->addWidget(scroll);
}

// ── clear_sections ────────────────────────────────────────────────────────────
void EquityTechnicalsTab::clear_sections() {
    auto* vl = qobject_cast<QVBoxLayout*>(sections_container_->layout());
    if (!vl)
        return;
    while (vl->count() > 0) {
        auto* item = vl->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
}

// ── populate ──────────────────────────────────────────────────────────────────
void EquityTechnicalsTab::populate(const services::equity::TechnicalsData& data) {
    // Flatten all indicators
    QVector<services::equity::TechIndicator> all;
    all << data.trend << data.momentum << data.volatility << data.volume;

    // Count buy/neutral/sell using threshold logic
    int buy_n = 0, sell_n = 0, neutral_n = 0;
    for (const auto& ti : all) {
        QString sig = compute_signal(ti);
        if (sig == "BUY")
            buy_n++;
        else if (sig == "SELL")
            sell_n++;
        else
            neutral_n++;
    }
    int total = buy_n + sell_n + neutral_n;

    // Overall recommendation
    QString recommendation = "NEUTRAL";
    QString rec_color = "#eab308";
    if (total > 0) {
        double buy_pct = 100.0 * buy_n / total;
        double sell_pct = 100.0 * sell_n / total;
        if (buy_pct >= 70) {
            recommendation = "STRONG BUY";
            rec_color = "#22c55e";
        } else if (buy_pct >= 50) {
            recommendation = "BUY";
            rec_color = "#4ade80";
        } else if (sell_pct >= 70) {
            recommendation = "STRONG SELL";
            rec_color = "#ef4444";
        } else if (sell_pct >= 50) {
            recommendation = "SELL";
            rec_color = "#f87171";
        }
    }

    rating_label_->setText(recommendation);
    rating_label_->setStyleSheet(QString("color:%1; font-size:20px; font-weight:700; letter-spacing:2px; "
                                         "background:transparent; border:0;")
                                     .arg(rec_color));

    // Gauge: show buy% on the progress bar
    gauge_bar_->setValue(total > 0 ? static_cast<int>(100.0 * buy_n / total) : 50);

    buy_count_->setText(QString::number(buy_n));
    neutral_count_->setText(QString::number(neutral_n));
    sell_count_->setText(QString::number(sell_n));
    total_label_->setText(QString("%1 INDICATORS ANALYZED").arg(total));

    // KEY INDICATORS — update the 8 pre-built cards
    auto cards = key_grid_->findChildren<QFrame*>();
    // Build lookup: indicator name (upper) → TechIndicator
    QMap<QString, services::equity::TechIndicator> lookup;
    for (const auto& ti : all)
        lookup[ti.name.toUpper()] = ti;

    for (int i = 0; i < 8 && i < cards.size(); ++i) {
        auto* card = cards[i];
        auto* name_lbl = card->findChild<QLabel*>("name");
        auto* val_lbl = card->findChild<QLabel*>("value");
        auto* sig_lbl = card->findChild<QLabel*>("signal");
        if (!name_lbl || !val_lbl || !sig_lbl)
            continue;

        // Find the key indicator by exact name match (names are human display names)
        const QString& key = KEY_INDICATORS[i];
        services::equity::TechIndicator found;
        bool found_flag = false;
        auto it = lookup.find(key.toUpper());
        if (it != lookup.end()) {
            found = it.value();
            found_flag = true;
        }

        if (!found_flag) {
            name_lbl->setText(key);
            val_lbl->setText("N/A");
            sig_lbl->setText("—");
            card->setStyleSheet(QString("QFrame { background:%1; border:1px solid %2; border-left:2px solid #6b7280; }")
                                    .arg(ui::colors::BG_BASE, ui::colors::BORDER_DIM));
            continue;
        }

        QString sig = compute_signal(found);
        QString sig_color = sig == "BUY" ? "#22c55e" : sig == "SELL" ? "#ef4444" : "#6b7280";
        QString arrow = sig == "BUY" ? "▲" : sig == "SELL" ? "▼" : "—";

        // Truncate name for display
        QString display_name = found.name.replace('_', ' ');
        if (display_name.length() > 10)
            display_name = display_name.left(10);

        name_lbl->setText(display_name.toUpper());
        name_lbl->setStyleSheet(
            "color:#6b7280; font-size:9px; letter-spacing:0.5px; background:transparent; border:0;");
        val_lbl->setText(QString::number(found.value, 'f', 2));
        val_lbl->setStyleSheet("color:#f9fafb; font-size:14px; font-weight:700; font-family:monospace; "
                               "background:transparent; border:0;");
        sig_lbl->setText(arrow + " " + sig);
        sig_lbl->setStyleSheet(
            QString("color:%1; font-size:9px; font-weight:700; background:transparent; border:0;").arg(sig_color));
        card->setStyleSheet(QString("QFrame { background:%1; border:1px solid %2; border-left:2px solid %3; }")
                                .arg(ui::colors::BG_BASE, ui::colors::BORDER_DIM, sig_color));
    }

    // CATEGORY SECTIONS
    clear_sections();
    auto* sc_vl = qobject_cast<QVBoxLayout*>(sections_container_->layout());

    struct CatDef {
        QString title;
        const QVector<services::equity::TechIndicator>* inds;
        QString color;
    };
    QList<CatDef> cats = {
        {"TREND INDICATORS", &data.trend, CAT_COLORS.TREND},
        {"MOMENTUM INDICATORS", &data.momentum, CAT_COLORS.MOMENTUM},
        {"VOLATILITY INDICATORS", &data.volatility, CAT_COLORS.VOLATILITY},
        {"VOLUME INDICATORS", &data.volume, CAT_COLORS.VOLUME},
    };

    for (const auto& cat : cats) {
        if (!cat.inds || cat.inds->isEmpty())
            continue;

        // Count per-category signals
        int c_buy = 0, c_sell = 0, c_neutral = 0;
        for (const auto& ti : *cat.inds) {
            QString s = compute_signal(ti);
            if (s == "BUY")
                c_buy++;
            else if (s == "SELL")
                c_sell++;
            else
                c_neutral++;
        }

        // Category header (collapsible look — static expanded)
        auto* section = new QFrame;
        section->setStyleSheet("QFrame { background:transparent; border:0; }");
        auto* sec_vl = new QVBoxLayout(section);
        sec_vl->setContentsMargins(0, 0, 0, 0);
        sec_vl->setSpacing(0);

        // Header row
        auto* hdr = new QWidget;
        hdr->setStyleSheet(QString("background:%1; border-left:2px solid %2; border-bottom:1px solid %3;")
                               .arg(ui::colors::BG_RAISED, cat.color, ui::colors::BORDER_DIM));
        auto* hdr_l = new QHBoxLayout(hdr);
        hdr_l->setContentsMargins(12, 8, 12, 8);
        hdr_l->setSpacing(10);

        auto* hdr_title = new QLabel("▾  " + cat.title);
        hdr_title->setStyleSheet(QString("color:#f9fafb; font-size:11px; font-weight:700; letter-spacing:1px; "
                                         "background:transparent; border:0;"));
        hdr_l->addWidget(hdr_title);

        auto* count_badge = new QLabel(QString("%1 INDICATORS").arg(cat.inds->size()));
        count_badge->setStyleSheet(QString("color:#6b7280; font-size:9px; background:%1; border:1px solid %2; "
                                           "padding:2px 6px;")
                                       .arg(ui::colors::BG_BASE, ui::colors::BORDER_DIM));
        hdr_l->addWidget(count_badge);
        hdr_l->addStretch();

        // Mini signal badges
        auto make_mini = [&](int count, const QString& label, const QString& color) {
            if (count == 0)
                return;
            auto* badge = new QLabel(QString("%1 %2").arg(count).arg(label));
            badge->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; "
                                         "background:%2; border-left:2px solid %1; padding:2px 6px;")
                                     .arg(color, color + "15"));
            hdr_l->addWidget(badge);
        };
        make_mini(c_buy, "BUY", "#22c55e");
        make_mini(c_sell, "SELL", "#ef4444");
        make_mini(c_neutral, "HOLD", "#6b7280");

        sec_vl->addWidget(hdr);

        // Indicator grid (auto-fill, minmax 340px — 2-col on wide, 1-col on narrow)
        auto* grid_w = new QWidget;
        grid_w->setStyleSheet(QString("background:%1; border-left:2px solid %2; border-bottom:1px solid %3;")
                                  .arg(ui::colors::BG_BASE, cat.color, ui::colors::BORDER_DIM));
        auto* grid = new QGridLayout(grid_w);
        grid->setContentsMargins(8, 8, 8, 8);
        grid->setSpacing(6);
        grid->setColumnStretch(0, 1);
        grid->setColumnStretch(1, 1);

        int row = 0, col = 0;
        for (const auto& ti : *cat.inds) {
            QString sig = compute_signal(ti);
            QString sig_color = sig == "BUY" ? "#22c55e" : sig == "SELL" ? "#ef4444" : "#6b7280";
            QString arrow = sig == "BUY" ? "▲" : sig == "SELL" ? "▼" : "—";

            auto* ind_card = new QFrame;
            ind_card->setStyleSheet(QString("QFrame { background:%1; border:1px solid %2; border-left:2px solid %3; }")
                                        .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM, sig_color));
            auto* ic_l = new QHBoxLayout(ind_card);
            ic_l->setContentsMargins(10, 6, 10, 6);
            ic_l->setSpacing(8);

            auto* ic_name = new QLabel(ti.name);
            ic_name->setStyleSheet(
                QString("color:%1; font-size:11px; background:transparent; border:0;").arg(ui::colors::TEXT_PRIMARY));
            ic_name->setMinimumWidth(120);

            auto* ic_val = new QLabel(QString::number(ti.value, 'f', 4));
            ic_val->setStyleSheet("color:#9ca3af; font-size:11px; font-family:monospace; "
                                  "background:transparent; border:0;");
            ic_val->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

            auto* ic_sig = new QLabel(arrow + " " + sig);
            ic_sig->setAlignment(Qt::AlignCenter);
            ic_sig->setFixedWidth(80);
            ic_sig->setStyleSheet(QString("background:%1; color:%2; border-radius:2px; padding:2px 4px; "
                                          "font-size:9px; font-weight:700; border:0;")
                                      .arg(sig_color + "20", sig_color));

            ic_l->addWidget(ic_name);
            ic_l->addStretch();
            ic_l->addWidget(ic_val);
            ic_l->addWidget(ic_sig);

            grid->addWidget(ind_card, row, col);
            col++;
            if (col == 2) {
                col = 0;
                row++;
            }
        }

        sec_vl->addWidget(grid_w);
        sc_vl->addWidget(section);
    }
}

// ── on_technicals_loaded ──────────────────────────────────────────────────────
void EquityTechnicalsTab::on_technicals_loaded(services::equity::TechnicalsData data) {
    if (data.symbol != current_symbol_)
        return;
    loading_overlay_->hide_loading();
    populate(data);
}

} // namespace fincept::screens
