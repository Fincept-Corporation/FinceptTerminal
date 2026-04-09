// src/screens/equity_research/EquityAnalysisTab.cpp
#include "screens/equity_research/EquityAnalysisTab.h"

#include "services/equity/EquityResearchService.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens {

// ── Panel card helpers (file-local) ───────────────────────────────────────────
namespace {

// Creates a titled panel with a colored left-bar accent and bottom border on header
QFrame* make_panel(const QString& title, const QString& accent_color) {
    auto* f = new QFrame;
    f->setStyleSheet(QString("QFrame { background:%1; border:1px solid %2; border-radius:4px; }")
                         .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));
    auto* vl = new QVBoxLayout(f);
    vl->setContentsMargins(16, 14, 16, 16);
    vl->setSpacing(14);

    // Header row: accent bar + title
    auto* hdr = new QWidget(nullptr);
    hdr->setStyleSheet(QString("background:transparent; border:0; border-bottom:2px solid %1;").arg(accent_color));
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(0, 0, 0, 8);
    hl->setSpacing(8);

    auto* bar = new QFrame;
    bar->setFixedSize(4, 16);
    bar->setStyleSheet(QString("background:%1; border:0; border-radius:0;").arg(accent_color));
    hl->addWidget(bar);

    auto* lbl = new QLabel(title);
    lbl->setStyleSheet(QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px; "
                               "background:transparent; border:0;")
                           .arg(accent_color));
    hl->addWidget(lbl);
    hl->addStretch();

    vl->addWidget(hdr);
    return f;
}

// Creates a 2-column metric card: small gray label on top, big colored value below
QWidget* make_card(const QString& label, QLabel*& val_out, const QString& val_color) {
    auto* w = new QWidget(nullptr);
    w->setStyleSheet("background:transparent;");
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(3);

    auto* lbl = new QLabel(label);
    lbl->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; letter-spacing:1px; "
                               "background:transparent; border:0;")
                           .arg(ui::colors::TEXT_TERTIARY));
    vl->addWidget(lbl);

    val_out = new QLabel("—");
    val_out->setStyleSheet(QString("color:%1; font-size:16px; font-weight:700; font-family:monospace; "
                                   "background:transparent; border:0;")
                               .arg(val_color));
    vl->addWidget(val_out);
    return w;
}

} // namespace

// ── EquityAnalysisTab ─────────────────────────────────────────────────────────

EquityAnalysisTab::EquityAnalysisTab(QWidget* parent) : QWidget(parent) {
    build_ui();
    auto& svc = services::equity::EquityResearchService::instance();
    connect(&svc, &services::equity::EquityResearchService::info_loaded, this, &EquityAnalysisTab::on_info_loaded);
}

void EquityAnalysisTab::set_symbol(const QString& symbol) {
    if (symbol == current_symbol_)
        return;
    current_symbol_ = symbol;
    loading_overlay_->show_loading("LOADING ANALYSIS…");
}

void EquityAnalysisTab::build_ui() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
    loading_overlay_ = new ui::LoadingOverlay(this);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("background:transparent; border:0;");

    auto* content = new QWidget(this);
    auto* grid = new QGridLayout(content);
    grid->setContentsMargins(12, 12, 12, 12);
    grid->setSpacing(10);

    // ── Panel 1: FINANCIAL HEALTH (top-left, orange) ──────────────────────────
    auto* health_panel = make_panel("FINANCIAL HEALTH", ui::colors::AMBER);
    auto* hl_vl = static_cast<QVBoxLayout*>(health_panel->layout());
    auto* health_grid = new QGridLayout;
    health_grid->setSpacing(16);
    health_grid->addWidget(make_card("TOTAL CASH", cash_val_, ui::colors::POSITIVE), 0, 0);
    health_grid->addWidget(make_card("TOTAL DEBT", debt_val_, ui::colors::NEGATIVE), 0, 1);
    health_grid->addWidget(make_card("FREE CASHFLOW", fcf_val_, "#22d3ee"), 1, 0);
    health_grid->addWidget(make_card("OPERATING CF", ocf_val_, "#60a5fa"), 1, 1);
    hl_vl->addLayout(health_grid);
    hl_vl->addStretch();

    // ── Panel 2: ENTERPRISE VALUE (top-right, cyan) ───────────────────────────
    auto* ev_panel = make_panel("ENTERPRISE VALUE", "#22d3ee");
    auto* ev_vl = static_cast<QVBoxLayout*>(ev_panel->layout());
    auto* ev_grid = new QGridLayout;
    ev_grid->setSpacing(16);
    ev_grid->addWidget(make_card("ENTERPRISE VALUE", ev_val_, "#eab308"), 0, 0);
    ev_grid->addWidget(make_card("EV/REVENUE", ev_rev_val_, "#22d3ee"), 0, 1);
    ev_grid->addWidget(make_card("EV/EBITDA", ev_ebitda_val_, "#22d3ee"), 1, 0);
    ev_grid->addWidget(make_card("BOOK VALUE", book_val_, ui::colors::TEXT_PRIMARY), 1, 1);
    ev_vl->addLayout(ev_grid);
    ev_vl->addStretch();

    // ── Panel 3: REVENUE & PROFITS (bottom-left, green) ───────────────────────
    auto* rev_panel = make_panel("REVENUE & PROFITS", ui::colors::POSITIVE);
    auto* rev_vl = static_cast<QVBoxLayout*>(rev_panel->layout());
    auto* rev_grid = new QGridLayout;
    rev_grid->setSpacing(16);
    rev_grid->addWidget(make_card("TOTAL REVENUE", rev_val_, ui::colors::POSITIVE), 0, 0);
    rev_grid->addWidget(make_card("REVENUE/SHARE", rev_share_val_, "#22d3ee"), 0, 1);
    rev_grid->addWidget(make_card("GROSS PROFITS", gp_val_, ui::colors::POSITIVE), 1, 0);
    rev_grid->addWidget(make_card("EBITDA MARGINS", ebitda_m_val_, "#eab308"), 1, 1);
    rev_vl->addLayout(rev_grid);
    rev_vl->addStretch();

    // ── Panel 4: KEY RATIOS (bottom-right, purple) ────────────────────────────
    auto* ratios_panel = make_panel("KEY RATIOS", "#a855f7");
    auto* rat_vl = static_cast<QVBoxLayout*>(ratios_panel->layout());
    auto* rat_grid = new QGridLayout;
    rat_grid->setSpacing(16);
    rat_grid->addWidget(make_card("P/E RATIO", pe_val_, ui::colors::TEXT_PRIMARY), 0, 0);
    rat_grid->addWidget(make_card("PEG RATIO", peg_val_, ui::colors::TEXT_PRIMARY), 0, 1);
    rat_grid->addWidget(make_card("ROE", roe_val_, ui::colors::POSITIVE), 1, 0);
    rat_grid->addWidget(make_card("ROA", roa_val_, ui::colors::POSITIVE), 1, 1);
    rat_grid->addWidget(make_card("BETA", beta_val_, "#22d3ee"), 2, 0);
    rat_grid->addWidget(make_card("SHORT RATIO", short_rat_val_, ui::colors::NEGATIVE), 2, 1);
    rat_vl->addLayout(rat_grid);
    rat_vl->addStretch();

    grid->addWidget(health_panel, 0, 0);
    grid->addWidget(ev_panel, 0, 1);
    grid->addWidget(rev_panel, 1, 0);
    grid->addWidget(ratios_panel, 1, 1);
    grid->setRowStretch(0, 1);
    grid->setRowStretch(1, 1);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);

    scroll->setWidget(content);
    auto* ol = new QVBoxLayout(this);
    ol->setContentsMargins(0, 0, 0, 0);
    ol->addWidget(scroll);
}

void EquityAnalysisTab::on_info_loaded(services::equity::StockInfo info) {
    if (info.symbol != current_symbol_)
        return;
    loading_overlay_->hide_loading();

    // Financial Health
    cash_val_->setText(fmt_large(info.total_cash));
    debt_val_->setText(fmt_large(info.total_debt));
    fcf_val_->setText(fmt_large(info.free_cashflow));
    ocf_val_->setText(fmt_large(info.operating_cashflow));

    // Enterprise Value
    ev_val_->setText(fmt_large(info.enterprise_value));
    ev_rev_val_->setText(fmt(info.ev_to_revenue));
    ev_ebitda_val_->setText(fmt(info.ev_to_ebitda, 1));
    book_val_->setText(info.book_value > 0 ? QString("$%1").arg(info.book_value, 0, 'f', 2) : "—");

    // Revenue & Profits
    rev_val_->setText(fmt_large(info.total_revenue));
    rev_share_val_->setText(info.revenue_per_share > 0 ? QString("$%1").arg(info.revenue_per_share, 0, 'f', 2) : "—");
    gp_val_->setText(fmt_large(info.gross_profits));
    ebitda_m_val_->setText(fmt_pct(info.ebitda_margins));

    // Key Ratios
    pe_val_->setText(info.pe_ratio > 0 ? fmt(info.pe_ratio, 1) : "—");
    peg_val_->setText(info.peg_ratio > 0 ? fmt(info.peg_ratio) : "—");
    roe_val_->setText(fmt_pct(info.roe));
    roa_val_->setText(fmt_pct(info.roa));
    beta_val_->setText(fmt(info.beta));
    short_rat_val_->setText(info.short_ratio > 0 ? fmt(info.short_ratio) : "—");
}

QString EquityAnalysisTab::fmt(double v, int decimals) {
    return v != 0.0 ? QString::number(v, 'f', decimals) : "—";
}

QString EquityAnalysisTab::fmt_large(double v) {
    if (v == 0.0)
        return "—";
    bool neg = v < 0;
    double a = std::abs(v);
    QString s;
    if (a >= 1e12)
        s = QString("%1T").arg(a / 1e12, 0, 'f', 2);
    else if (a >= 1e9)
        s = QString("%1B").arg(a / 1e9, 0, 'f', 2);
    else if (a >= 1e6)
        s = QString("%1M").arg(a / 1e6, 0, 'f', 1);
    else
        s = QString::number(a, 'f', 0);
    return neg ? "-" + s : s;
}

QString EquityAnalysisTab::fmt_pct(double v) {
    return v != 0.0 ? QString("%1%").arg(v * 100.0, 0, 'f', 2) : "—";
}

} // namespace fincept::screens
