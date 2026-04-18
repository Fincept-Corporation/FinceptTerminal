#include "screens/derivatives/DerivativesScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "services/python_cli/PythonCliService.h"
#include "storage/cache/CacheManager.h"
#include "ui/theme/StyleSheets.h"
#include "ui/theme/Theme.h"

#include <QDate>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui;

// ── Stylesheet constants (applied once at screen level) ─────────────────────

static const QString kScreenStyle =
    QStringLiteral("#derivScreen { background: %1; }"
                   "#derivHeader { background: %2; border-bottom: 2px solid %3; }"
                   "#derivHeaderTitle { color: %4; font-size: 12px; font-weight: 700; background: transparent; }"
                   "#derivHeaderSub { color: %5; font-size: 9px; letter-spacing: 0.5px; background: transparent; }"
                   "#derivHeaderBadge { color: %6; font-size: 8px; font-weight: 700; background: rgba(22,163,74,0.2); "
                   "  padding: 2px 6px; }"
                   "#derivInstrBar { background: %1; }"
                   "#derivInstrBtn { background: transparent; color: %5; border: none; font-size: 9px; "
                   "  font-weight: 700; letter-spacing: 0.5px; padding: 6px 12px; }"
                   "#derivInstrBtn:hover { color: %4; background: %7; }"
                   "#derivInstrBtn[active=\"true\"] { background: %3; color: %1; }"
                   "#derivPanel { background: %8; border: 1px solid %9; }"
                   "#derivPanelHeader { background: %2; border-bottom: 1px solid %9; }"
                   "#derivPanelTitle { color: %4; font-size: 11px; font-weight: 700; background: transparent; }"
                   "#derivPanelIcon { color: %3; font-size: 14px; background: transparent; }"
                   "#derivLabel { color: %5; font-size: 9px; font-weight: 700; letter-spacing: 0.5px; "
                   "  background: transparent; }"
                   "#derivInput { background: %1; color: %4; border: 1px solid %9; padding: 4px 8px; "
                   "  font-size: 12px; }"
                   "#derivInput:focus { border-color: %10; }"
                   "QDoubleSpinBox { background: %1; color: %4; border: 1px solid %9; padding: 4px 8px; "
                   "  font-size: 12px; }"
                   "QDoubleSpinBox:focus { border-color: %10; }"
                   "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { width: 0; }"
                   "QDateEdit { background: %1; color: %4; border: 1px solid %9; padding: 4px 8px; "
                   "  font-size: 12px; }"
                   "QDateEdit:focus { border-color: %10; }"
                   "QDateEdit::drop-down { border: none; width: 20px; }"
                   "QComboBox { background: %1; color: %4; border: 1px solid %9; padding: 4px 8px; "
                   "  font-size: 12px; }"
                   "QComboBox:focus { border-color: %10; }"
                   "QComboBox::drop-down { border: none; width: 20px; }"
                   "QComboBox QAbstractItemView { background: %2; color: %4; border: 1px solid %9; "
                   "  selection-background-color: %3; }"
                   "#derivCalcBtn { background: %3; color: %1; border: none; padding: 6px 16px; "
                   "  font-size: 9px; font-weight: 700; }"
                   "#derivCalcBtn:hover { background: #FF8800; }"
                   "#derivCalcBtn:disabled { background: %11; color: %12; }"
                   "#derivCalcBtnAlt { background: %13; color: %1; border: none; padding: 6px 16px; "
                   "  font-size: 9px; font-weight: 700; }"
                   "#derivCalcBtnAlt:hover { background: #00F5FF; }"
                   "#derivCalcBtnAlt:disabled { background: %11; color: %12; }"
                   "#derivResultsPanel { background: %8; border: 1px solid %9; }"
                   "#derivResultsHeader { background: %2; border-bottom: 1px solid %9; }"
                   "#derivResultsTitle { color: %4; font-size: 11px; font-weight: 700; background: transparent; }"
                   "#derivResultLabel { color: %5; font-size: 9px; font-weight: 700; letter-spacing: 0.5px; "
                   "  background: transparent; }"
                   "#derivResultValue { color: %13; font-size: 13px; font-weight: 700; background: transparent; }"
                   "#derivResultBigValue { color: %3; font-size: 16px; font-weight: 700; background: transparent; }"
                   "#derivResultCard { background: %2; padding: 8px; }"
                   "#derivErrorPanel { background: rgba(220,38,38,0.1); border: 1px solid %14; }"
                   "#derivErrorText { color: %14; font-size: 10px; background: transparent; }"
                   "#derivErrorLabel { color: %14; font-size: 9px; font-weight: 700; background: transparent; }"
                   "#derivStatusBar { background: %2; border-top: 1px solid %9; }"
                   "#derivStatusText { color: %5; font-size: 9px; background: transparent; }"
                   "#derivStatusEngine { color: %13; font-size: 9px; background: transparent; }"
                   "QScrollArea { background: %1; border: none; }"
                   "QScrollBar:vertical { background: %1; width: 6px; }"
                   "QScrollBar::handle:vertical { background: %9; min-height: 20px; }"
                   "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
        .arg(colors::BG_BASE())        // %1
        .arg(colors::BG_RAISED())      // %2
        .arg(colors::AMBER())          // %3
        .arg(colors::TEXT_PRIMARY())   // %4
        .arg(colors::TEXT_SECONDARY()) // %5
        .arg(colors::POSITIVE())       // %6
        .arg(colors::BG_HOVER())       // %7
        .arg(colors::BG_SURFACE())     // %8
        .arg(colors::BORDER_DIM())     // %9
        .arg(colors::BORDER_BRIGHT())  // %10
        .arg(colors::AMBER_DIM())      // %11
        .arg(colors::TEXT_DIM())       // %12
        .arg(colors::CYAN())           // %13
        .arg(colors::NEGATIVE())       // %14
    ;

// ── Constructor ─────────────────────────────────────────────────────────────

DerivativesScreen::DerivativesScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("derivScreen");
    setStyleSheet(kScreenStyle);
    setup_ui();
    LOG_INFO("Derivatives", "Screen constructed");
}

// ── UI Setup ────────────────────────────────────────────────────────────────

void DerivativesScreen::setup_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Header
    root->addWidget(create_header_bar());

    // Instrument selector bar
    root->addWidget(create_instrument_bar());

    // Scrollable content area
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* content = new QWidget(this);
    auto* content_layout = new QVBoxLayout(content);
    content_layout->setContentsMargins(16, 16, 16, 16);
    content_layout->setSpacing(16);

    // Panel stack (one panel per instrument type)
    panel_stack_ = new QStackedWidget;
    panel_stack_->addWidget(create_bonds_panel());
    panel_stack_->addWidget(create_equity_options_panel());
    panel_stack_->addWidget(create_fx_options_panel());
    panel_stack_->addWidget(create_swaps_panel());
    panel_stack_->addWidget(create_credit_panel());
    content_layout->addWidget(panel_stack_);

    // Results area
    results_container_ = create_results_panel();
    results_container_->hide();
    content_layout->addWidget(results_container_);

    content_layout->addStretch(1);
    scroll->setWidget(content);
    root->addWidget(scroll, 1);

    // Status bar
    root->addWidget(create_status_bar());
}

QWidget* DerivativesScreen::create_header_bar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("derivHeader");
    bar->setFixedHeight(42);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);

    auto* icon = new QLabel(QStringLiteral("\xF0\x9F\x93\x90")); // calculator emoji
    icon->setStyleSheet(QString("color:%1;font-size:18px;background:transparent;").arg(colors::AMBER()));

    auto* title_col = new QVBoxLayout;
    title_col->setSpacing(0);
    auto* title = new QLabel("DERIVATIVES PRICING");
    title->setObjectName("derivHeaderTitle");
    auto* sub = new QLabel("PROFESSIONAL VALUATION ENGINE");
    sub->setObjectName("derivHeaderSub");
    title_col->addWidget(title);
    title_col->addWidget(sub);

    hl->addWidget(icon);
    hl->addSpacing(8);
    hl->addLayout(title_col);
    hl->addStretch(1);

    auto* badge = new QLabel("PYTHON ACTIVE");
    badge->setObjectName("derivHeaderBadge");
    hl->addWidget(badge);

    return bar;
}

QWidget* DerivativesScreen::create_instrument_bar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("derivInstrBar");
    bar->setFixedHeight(34);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);
    hl->setSpacing(4);

    const QStringList names = {"BONDS", "EQUITY OPTIONS", "FX OPTIONS", "IR SWAPS", "CREDIT"};
    for (int i = 0; i < names.size(); ++i) {
        auto* btn = new QPushButton(names[i]);
        btn->setObjectName("derivInstrBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("active", i == 0);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_instrument_changed(i); });
        hl->addWidget(btn);
        instrument_btns_.append(btn);
    }

    hl->addStretch(1);
    return bar;
}

// ── Panel builders ──────────────────────────────────────────────────────────

QWidget* DerivativesScreen::create_bonds_panel() {
    auto* outer = new QWidget(this);
    auto* grid = new QGridLayout(outer);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(16);

    // ── Left: Bond Price Calculator ──
    auto* price_panel = new QWidget(this);
    price_panel->setObjectName("derivPanel");
    auto* pv = new QVBoxLayout(price_panel);
    pv->setContentsMargins(0, 0, 0, 0);
    pv->setSpacing(0);

    // Header
    auto* ph = new QWidget(this);
    ph->setObjectName("derivPanelHeader");
    ph->setFixedHeight(34);
    auto* phl = new QHBoxLayout(ph);
    phl->setContentsMargins(12, 0, 12, 0);
    auto* phi = new QLabel("$");
    phi->setObjectName("derivPanelIcon");
    auto* pht = new QLabel("BOND PRICE CALCULATOR");
    pht->setObjectName("derivPanelTitle");
    phl->addWidget(phi);
    phl->addSpacing(8);
    phl->addWidget(pht);
    phl->addStretch(1);
    pv->addWidget(ph);

    // Inputs
    auto* body = new QWidget(this);
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(12, 12, 12, 12);
    bl->setSpacing(10);

    bond_issue_date_ = create_date(QDate(2023, 1, 1));
    bond_settle_date_ = create_date(QDate::currentDate());
    bond_maturity_date_ = create_date(QDate(2029, 1, 1));
    bond_coupon_ = create_spin(5.0, 0, 100, 2, "%");
    bond_ytm_ = create_spin(4.5, -10, 100, 2, "%");
    bond_freq_ = create_combo({"Annual", "Semi-Annual", "Quarterly"});
    bond_freq_->setCurrentIndex(1);

    bl->addWidget(create_two_col(create_input_row("ISSUE DATE", bond_issue_date_),
                                 create_input_row("SETTLEMENT DATE", bond_settle_date_)));
    bl->addWidget(create_input_row("MATURITY DATE", bond_maturity_date_));
    bl->addWidget(
        create_two_col(create_input_row("COUPON RATE (%)", bond_coupon_), create_input_row("YTM (%)", bond_ytm_)));
    bl->addWidget(create_input_row("PAYMENT FREQUENCY", bond_freq_));

    auto* calc_btn = create_calc_button("CALCULATE BOND PRICE");
    connect(calc_btn, &QPushButton::clicked, this, &DerivativesScreen::on_calculate);
    bl->addWidget(calc_btn);

    pv->addWidget(body);
    grid->addWidget(price_panel, 0, 0);

    // ── Right: YTM Calculator ──
    auto* ytm_panel = new QWidget(this);
    ytm_panel->setObjectName("derivPanel");
    auto* yv = new QVBoxLayout(ytm_panel);
    yv->setContentsMargins(0, 0, 0, 0);
    yv->setSpacing(0);

    auto* yh = new QWidget(this);
    yh->setObjectName("derivPanelHeader");
    yh->setFixedHeight(34);
    auto* yhl = new QHBoxLayout(yh);
    yhl->setContentsMargins(12, 0, 12, 0);
    auto* yhi = new QLabel("%");
    yhi->setObjectName("derivPanelIcon");
    auto* yht = new QLabel("YIELD TO MATURITY");
    yht->setObjectName("derivPanelTitle");
    yhl->addWidget(yhi);
    yhl->addSpacing(8);
    yhl->addWidget(yht);
    yhl->addStretch(1);
    yv->addWidget(yh);

    auto* ybody = new QWidget(this);
    auto* ybl = new QVBoxLayout(ybody);
    ybl->setContentsMargins(12, 12, 12, 12);
    ybl->setSpacing(10);

    bond_clean_price_ = create_spin(102.0, 0, 10000, 2);

    // Reuse same date/coupon/freq inputs — display-only labels for dates
    ybl->addWidget(create_two_col(create_input_row("ISSUE DATE", create_date(QDate(2023, 1, 1))),
                                  create_input_row("SETTLEMENT DATE", create_date(QDate::currentDate()))));
    ybl->addWidget(create_input_row("MATURITY DATE", create_date(QDate(2029, 1, 1))));
    ybl->addWidget(create_two_col(create_input_row("COUPON RATE (%)", create_spin(5.0, 0, 100, 2, "%")),
                                  create_input_row("CLEAN PRICE", bond_clean_price_)));

    auto* ytm_freq = create_combo({"Annual", "Semi-Annual", "Quarterly"});
    ytm_freq->setCurrentIndex(1);
    ybl->addWidget(create_input_row("PAYMENT FREQUENCY", ytm_freq));

    auto* ytm_btn = create_calc_button("CALCULATE YTM");
    connect(ytm_btn, &QPushButton::clicked, this, &DerivativesScreen::on_calculate_secondary);
    ybl->addWidget(ytm_btn);

    yv->addWidget(ybody);
    grid->addWidget(ytm_panel, 0, 1);

    return outer;
}

QWidget* DerivativesScreen::create_equity_options_panel() {
    auto* outer = new QWidget(this);
    auto* grid = new QGridLayout(outer);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(16);

    // ── Left: Black-Scholes Pricing ──
    auto* bs_panel = new QWidget(this);
    bs_panel->setObjectName("derivPanel");
    auto* bv = new QVBoxLayout(bs_panel);
    bv->setContentsMargins(0, 0, 0, 0);
    bv->setSpacing(0);

    auto* bh = new QWidget(this);
    bh->setObjectName("derivPanelHeader");
    bh->setFixedHeight(34);
    auto* bhl = new QHBoxLayout(bh);
    bhl->setContentsMargins(12, 0, 12, 0);
    auto* bhi = new QLabel(QStringLiteral("\xE2\x86\x97")); // arrow
    bhi->setObjectName("derivPanelIcon");
    auto* bht = new QLabel("BLACK-SCHOLES PRICING");
    bht->setObjectName("derivPanelTitle");
    bhl->addWidget(bhi);
    bhl->addSpacing(8);
    bhl->addWidget(bht);
    bhl->addStretch(1);
    bv->addWidget(bh);

    auto* bbody = new QWidget(this);
    auto* bbl = new QVBoxLayout(bbody);
    bbl->setContentsMargins(12, 12, 12, 12);
    bbl->setSpacing(10);

    opt_spot_ = create_spin(105.0, 0, 1e8, 2);
    opt_strike_ = create_spin(100.0, 0, 1e8, 2);
    opt_time_ = create_spin(1.0, 0.001, 30, 4);
    opt_vol_ = create_spin(25.0, 0, 500, 2, "%");
    opt_rate_ = create_spin(5.0, -20, 100, 2, "%");
    opt_div_ = create_spin(2.0, 0, 100, 2, "%");
    opt_type_ = create_combo({"Call", "Put"});

    bbl->addWidget(
        create_two_col(create_input_row("SPOT PRICE", opt_spot_), create_input_row("STRIKE PRICE", opt_strike_)));
    bbl->addWidget(create_two_col(create_input_row("TIME TO EXPIRY (years)", opt_time_),
                                  create_input_row("VOLATILITY (%)", opt_vol_)));
    bbl->addWidget(create_two_col(create_input_row("RISK-FREE RATE (%)", opt_rate_),
                                  create_input_row("DIVIDEND YIELD (%)", opt_div_)));
    bbl->addWidget(create_input_row("OPTION TYPE", opt_type_));

    auto* bs_btn = create_calc_button("CALCULATE PRICE & GREEKS");
    connect(bs_btn, &QPushButton::clicked, this, &DerivativesScreen::on_calculate);
    bbl->addWidget(bs_btn);

    bv->addWidget(bbody);
    grid->addWidget(bs_panel, 0, 0);

    // ── Right: Implied Volatility ──
    auto* iv_panel = new QWidget(this);
    iv_panel->setObjectName("derivPanel");
    auto* iv = new QVBoxLayout(iv_panel);
    iv->setContentsMargins(0, 0, 0, 0);
    iv->setSpacing(0);

    auto* ivh = new QWidget(this);
    ivh->setObjectName("derivPanelHeader");
    ivh->setFixedHeight(34);
    auto* ivhl = new QHBoxLayout(ivh);
    ivhl->setContentsMargins(12, 0, 12, 0);
    auto* ivhi = new QLabel(QStringLiteral("\xE2\x9A\xA1")); // lightning
    ivhi->setObjectName("derivPanelIcon");
    auto* ivht = new QLabel("IMPLIED VOLATILITY");
    ivht->setObjectName("derivPanelTitle");
    ivhl->addWidget(ivhi);
    ivhl->addSpacing(8);
    ivhl->addWidget(ivht);
    ivhl->addStretch(1);
    iv->addWidget(ivh);

    auto* ivbody = new QWidget(this);
    auto* ivbl = new QVBoxLayout(ivbody);
    ivbl->setContentsMargins(12, 12, 12, 12);
    ivbl->setSpacing(10);

    opt_market_price_ = create_spin(15.0, 0, 1e8, 4);

    auto* iv_spot = create_spin(105.0, 0, 1e8, 2);
    auto* iv_strike = create_spin(100.0, 0, 1e8, 2);
    auto* iv_time = create_spin(1.0, 0.001, 30, 4);
    auto* iv_rate = create_spin(5.0, -20, 100, 2, "%");
    auto* iv_div = create_spin(2.0, 0, 100, 2, "%");
    auto* iv_type = create_combo({"Call", "Put"});

    ivbl->addWidget(
        create_two_col(create_input_row("SPOT PRICE", iv_spot), create_input_row("STRIKE PRICE", iv_strike)));
    ivbl->addWidget(create_input_row("MARKET OPTION PRICE", opt_market_price_));
    ivbl->addWidget(create_two_col(create_input_row("TIME TO EXPIRY (years)", iv_time),
                                   create_input_row("RISK-FREE RATE (%)", iv_rate)));
    ivbl->addWidget(
        create_two_col(create_input_row("DIVIDEND YIELD (%)", iv_div), create_input_row("OPTION TYPE", iv_type)));

    auto* iv_btn = create_calc_button("CALCULATE IMPLIED VOL");
    connect(iv_btn, &QPushButton::clicked, this, &DerivativesScreen::on_calculate_secondary);
    ivbl->addWidget(iv_btn);

    iv->addWidget(ivbody);
    grid->addWidget(iv_panel, 0, 1);

    return outer;
}

QWidget* DerivativesScreen::create_fx_options_panel() {
    auto* panel = new QWidget(this);
    panel->setObjectName("derivPanel");
    panel->setMaximumWidth(700);

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Header
    auto* hdr = new QWidget(this);
    hdr->setObjectName("derivPanelHeader");
    hdr->setFixedHeight(34);
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(12, 0, 12, 0);
    auto* icon = new QLabel(QStringLiteral("\xF0\x9F\x92\xB1")); // currency emoji
    icon->setObjectName("derivPanelIcon");
    auto* title = new QLabel("FX VANILLA OPTION PRICING");
    title->setObjectName("derivPanelTitle");
    hl->addWidget(icon);
    hl->addSpacing(8);
    hl->addWidget(title);
    hl->addStretch(1);
    vl->addWidget(hdr);

    auto* body = new QWidget(this);
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(12, 12, 12, 12);
    bl->setSpacing(10);

    fx_spot_ = create_spin(1.08, 0, 1e6, 4);
    fx_strike_ = create_spin(1.10, 0, 1e6, 4);
    fx_time_ = create_spin(1.0, 0.001, 30, 4);
    fx_vol_ = create_spin(12.0, 0, 500, 2, "%");
    fx_dom_rate_ = create_spin(2.0, -20, 100, 2, "%");
    fx_for_rate_ = create_spin(1.5, -20, 100, 2, "%");
    fx_type_ = create_combo({"Call", "Put"});
    fx_notional_ = create_spin(1000000, 0, 1e12, 0);

    bl->addWidget(
        create_two_col(create_input_row("SPOT FX RATE", fx_spot_), create_input_row("STRIKE FX RATE", fx_strike_)));
    bl->addWidget(create_three_col(create_input_row("VOLATILITY (%)", fx_vol_),
                                   create_input_row("DOMESTIC RATE (%)", fx_dom_rate_),
                                   create_input_row("FOREIGN RATE (%)", fx_for_rate_)));
    bl->addWidget(create_two_col(create_input_row("TIME TO EXPIRY (years)", fx_time_),
                                 create_input_row("NOTIONAL", fx_notional_)));
    bl->addWidget(create_input_row("OPTION TYPE", fx_type_));

    auto* btn = create_calc_button("CALCULATE FX OPTION PRICE");
    connect(btn, &QPushButton::clicked, this, &DerivativesScreen::on_calculate);
    bl->addWidget(btn);

    vl->addWidget(body);
    return panel;
}

QWidget* DerivativesScreen::create_swaps_panel() {
    auto* panel = new QWidget(this);
    panel->setObjectName("derivPanel");
    panel->setMaximumWidth(700);

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* hdr = new QWidget(this);
    hdr->setObjectName("derivPanelHeader");
    hdr->setFixedHeight(34);
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(12, 0, 12, 0);
    auto* icon = new QLabel(QStringLiteral("\xF0\x9F\x94\x84")); // arrows emoji
    icon->setObjectName("derivPanelIcon");
    auto* title = new QLabel("INTEREST RATE SWAP PRICING");
    title->setObjectName("derivPanelTitle");
    hl->addWidget(icon);
    hl->addSpacing(8);
    hl->addWidget(title);
    hl->addStretch(1);
    vl->addWidget(hdr);

    auto* body = new QWidget(this);
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(12, 12, 12, 12);
    bl->setSpacing(10);

    swap_eff_date_ = create_date(QDate::currentDate());
    swap_mat_date_ = create_date(QDate::currentDate().addYears(5));
    swap_fixed_rate_ = create_spin(3.0, -20, 100, 2, "%");
    swap_freq_ = create_combo({"Annual", "Semi-Annual", "Quarterly"});
    swap_freq_->setCurrentIndex(1);
    swap_discount_ = create_spin(3.5, -20, 100, 2, "%");
    swap_notional_ = create_spin(1000000, 0, 1e12, 0);

    bl->addWidget(create_two_col(create_input_row("EFFECTIVE DATE", swap_eff_date_),
                                 create_input_row("MATURITY DATE", swap_mat_date_)));
    bl->addWidget(create_three_col(create_input_row("FIXED RATE (%)", swap_fixed_rate_),
                                   create_input_row("FREQUENCY", swap_freq_),
                                   create_input_row("DISCOUNT RATE (%)", swap_discount_)));
    bl->addWidget(create_input_row("NOTIONAL AMOUNT", swap_notional_));

    auto* btn = create_calc_button("CALCULATE SWAP VALUE");
    connect(btn, &QPushButton::clicked, this, &DerivativesScreen::on_calculate);
    bl->addWidget(btn);

    vl->addWidget(body);
    return panel;
}

QWidget* DerivativesScreen::create_credit_panel() {
    auto* panel = new QWidget(this);
    panel->setObjectName("derivPanel");
    panel->setMaximumWidth(700);

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* hdr = new QWidget(this);
    hdr->setObjectName("derivPanelHeader");
    hdr->setFixedHeight(34);
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(12, 0, 12, 0);
    auto* icon = new QLabel(QStringLiteral("\xF0\x9F\x8E\xAF")); // target emoji
    icon->setObjectName("derivPanelIcon");
    auto* title = new QLabel("CREDIT DEFAULT SWAP PRICING");
    title->setObjectName("derivPanelTitle");
    hl->addWidget(icon);
    hl->addSpacing(8);
    hl->addWidget(title);
    hl->addStretch(1);
    vl->addWidget(hdr);

    auto* body = new QWidget(this);
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(12, 12, 12, 12);
    bl->setSpacing(10);

    cds_val_date_ = create_date(QDate::currentDate());
    cds_mat_date_ = create_date(QDate::currentDate().addYears(5));
    cds_recovery_ = create_spin(40.0, 0, 100, 2, "%");
    cds_spread_ = create_spin(150.0, 0, 10000, 1);
    cds_notional_ = create_spin(10000000, 0, 1e12, 0);

    bl->addWidget(create_two_col(create_input_row("VALUATION DATE", cds_val_date_),
                                 create_input_row("MATURITY DATE", cds_mat_date_)));
    bl->addWidget(create_three_col(create_input_row("RECOVERY RATE (%)", cds_recovery_),
                                   create_input_row("SPREAD (BPS)", cds_spread_),
                                   create_input_row("NOTIONAL", cds_notional_)));

    auto* btn = create_calc_button("CALCULATE CDS VALUE");
    connect(btn, &QPushButton::clicked, this, &DerivativesScreen::on_calculate);
    bl->addWidget(btn);

    vl->addWidget(body);
    return panel;
}

QWidget* DerivativesScreen::create_status_bar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("derivStatusBar");
    bar->setFixedHeight(26);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);

    auto* left = new QLabel("DERIVATIVES");
    left->setObjectName("derivStatusText");
    hl->addWidget(left);
    hl->addStretch(1);

    status_instrument_ = new QLabel("INSTRUMENT: BONDS");
    status_instrument_->setObjectName("derivStatusText");
    hl->addWidget(status_instrument_);

    hl->addSpacing(16);
    status_engine_ = new QLabel("PYTHON ENGINE");
    status_engine_->setObjectName("derivStatusEngine");
    hl->addWidget(status_engine_);

    return bar;
}

QWidget* DerivativesScreen::create_results_panel() {
    auto* frame = new QWidget(this);
    frame->setObjectName("derivResultsPanel");

    auto* vl = new QVBoxLayout(frame);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Header
    auto* hdr = new QWidget(this);
    hdr->setObjectName("derivResultsHeader");
    hdr->setFixedHeight(34);
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(12, 0, 12, 0);
    auto* title = new QLabel("RESULTS");
    title->setObjectName("derivResultsTitle");
    hl->addWidget(title);
    hl->addStretch(1);
    vl->addWidget(hdr);

    return frame;
}

// ── Widget factories ────────────────────────────────────────────────────────

QWidget* DerivativesScreen::create_input_row(const QString& label_text, QWidget* input) {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(4);

    auto* label = new QLabel(label_text);
    label->setObjectName("derivLabel");
    vl->addWidget(label);
    vl->addWidget(input);

    return w;
}

QWidget* DerivativesScreen::create_two_col(QWidget* left, QWidget* right) {
    auto* w = new QWidget(this);
    auto* hl = new QHBoxLayout(w);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(8);
    hl->addWidget(left, 1);
    hl->addWidget(right, 1);
    return w;
}

QWidget* DerivativesScreen::create_three_col(QWidget* a, QWidget* b, QWidget* c) {
    auto* w = new QWidget(this);
    auto* hl = new QHBoxLayout(w);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(8);
    hl->addWidget(a, 1);
    hl->addWidget(b, 1);
    hl->addWidget(c, 1);
    return w;
}

QDoubleSpinBox* DerivativesScreen::create_spin(double val, double min_val, double max_val, int decimals,
                                               const QString& suffix) {
    auto* spin = new QDoubleSpinBox;
    spin->setRange(min_val, max_val);
    spin->setDecimals(decimals);
    spin->setValue(val);
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    if (!suffix.isEmpty())
        spin->setSuffix(" " + suffix);
    return spin;
}

QDateEdit* DerivativesScreen::create_date(const QDate& date) {
    auto* de = new QDateEdit(date);
    de->setCalendarPopup(true);
    de->setDisplayFormat("yyyy-MM-dd");
    return de;
}

QComboBox* DerivativesScreen::create_combo(const QStringList& items) {
    auto* cb = new QComboBox;
    cb->addItems(items);
    return cb;
}

QPushButton* DerivativesScreen::create_calc_button(const QString& text) {
    auto* btn = new QPushButton(text);
    btn->setObjectName("derivCalcBtn");
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedHeight(32);
    return btn;
}

// ── Slots ───────────────────────────────────────────────────────────────────

void DerivativesScreen::on_instrument_changed(int index) {
    active_instrument_ = index;
    panel_stack_->setCurrentIndex(index);
    clear_results();

    // Update button states
    for (int i = 0; i < instrument_btns_.size(); ++i) {
        instrument_btns_[i]->setProperty("active", i == index);
        instrument_btns_[i]->style()->unpolish(instrument_btns_[i]);
        instrument_btns_[i]->style()->polish(instrument_btns_[i]);
    }

    const QStringList names = {"BONDS", "EQUITY OPTIONS", "FX OPTIONS", "IR SWAPS", "CREDIT"};
    status_instrument_->setText("INSTRUMENT: " + names[index]);
    LOG_INFO("Derivatives", "Switched to: " + names[index]);

    fincept::ScreenStateManager::instance().notify_changed(this);
}

void DerivativesScreen::on_calculate() {
    if (loading_)
        return;

    switch (active_instrument_) {
        case 0: { // Bonds — price from YTM
            int freq_val = (bond_freq_->currentIndex() == 0) ? 1 : (bond_freq_->currentIndex() == 1) ? 2 : 4;
            run_pricing("bond_price", {
                                          "--issue-date",
                                          bond_issue_date_->date().toString("yyyy-MM-dd"),
                                          "--settlement-date",
                                          bond_settle_date_->date().toString("yyyy-MM-dd"),
                                          "--maturity-date",
                                          bond_maturity_date_->date().toString("yyyy-MM-dd"),
                                          "--coupon-rate",
                                          QString::number(bond_coupon_->value()),
                                          "--ytm",
                                          QString::number(bond_ytm_->value()),
                                          "--freq",
                                          QString::number(freq_val),
                                      });
            break;
        }
        case 1: { // Equity options — price + greeks
            run_pricing("option_price", {
                                            "--spot",
                                            QString::number(opt_spot_->value()),
                                            "--strike",
                                            QString::number(opt_strike_->value()),
                                            "--time",
                                            QString::number(opt_time_->value()),
                                            "--rate",
                                            QString::number(opt_rate_->value()),
                                            "--vol",
                                            QString::number(opt_vol_->value()),
                                            "--div-yield",
                                            QString::number(opt_div_->value()),
                                            "--type",
                                            opt_type_->currentText().toLower(),
                                        });
            break;
        }
        case 2: { // FX options
            run_pricing("fx_option_price", {
                                               "--spot",
                                               QString::number(fx_spot_->value()),
                                               "--strike",
                                               QString::number(fx_strike_->value()),
                                               "--time",
                                               QString::number(fx_time_->value()),
                                               "--domestic-rate",
                                               QString::number(fx_dom_rate_->value()),
                                               "--foreign-rate",
                                               QString::number(fx_for_rate_->value()),
                                               "--vol",
                                               QString::number(fx_vol_->value()),
                                               "--type",
                                               fx_type_->currentText().toLower(),
                                               "--notional",
                                               QString::number(fx_notional_->value(), 'f', 0),
                                           });
            break;
        }
        case 3: { // Swaps
            int freq_val = (swap_freq_->currentIndex() == 0) ? 1 : (swap_freq_->currentIndex() == 1) ? 2 : 4;
            run_pricing("swap_value", {
                                          "--effective-date",
                                          swap_eff_date_->date().toString("yyyy-MM-dd"),
                                          "--maturity-date",
                                          swap_mat_date_->date().toString("yyyy-MM-dd"),
                                          "--fixed-rate",
                                          QString::number(swap_fixed_rate_->value()),
                                          "--freq",
                                          QString::number(freq_val),
                                          "--notional",
                                          QString::number(swap_notional_->value(), 'f', 0),
                                          "--discount-rate",
                                          QString::number(swap_discount_->value()),
                                      });
            break;
        }
        case 4: { // Credit (CDS)
            run_pricing("cds_value", {
                                         "--valuation-date",
                                         cds_val_date_->date().toString("yyyy-MM-dd"),
                                         "--maturity-date",
                                         cds_mat_date_->date().toString("yyyy-MM-dd"),
                                         "--recovery-rate",
                                         QString::number(cds_recovery_->value()),
                                         "--notional",
                                         QString::number(cds_notional_->value(), 'f', 0),
                                         "--spread-bps",
                                         QString::number(cds_spread_->value()),
                                     });
            break;
        }
    }
}

void DerivativesScreen::on_calculate_secondary() {
    if (loading_)
        return;

    switch (active_instrument_) {
        case 0: { // Bond YTM from price
            // Read from the YTM panel's own inputs (children of panel_stack_ page 0)
            // The bond_clean_price_ spin is the dedicated input for this
            int freq_val = (bond_freq_->currentIndex() == 0) ? 1 : (bond_freq_->currentIndex() == 1) ? 2 : 4;
            run_pricing("bond_ytm", {
                                        "--issue-date",
                                        bond_issue_date_->date().toString("yyyy-MM-dd"),
                                        "--settlement-date",
                                        bond_settle_date_->date().toString("yyyy-MM-dd"),
                                        "--maturity-date",
                                        bond_maturity_date_->date().toString("yyyy-MM-dd"),
                                        "--coupon-rate",
                                        QString::number(bond_coupon_->value()),
                                        "--clean-price",
                                        QString::number(bond_clean_price_->value()),
                                        "--freq",
                                        QString::number(freq_val),
                                    });
            break;
        }
        case 1: { // Implied volatility
            run_pricing("implied_vol", {
                                           "--spot",
                                           QString::number(opt_spot_->value()),
                                           "--strike",
                                           QString::number(opt_strike_->value()),
                                           "--time",
                                           QString::number(opt_time_->value()),
                                           "--rate",
                                           QString::number(opt_rate_->value()),
                                           "--market-price",
                                           QString::number(opt_market_price_->value()),
                                           "--div-yield",
                                           QString::number(opt_div_->value()),
                                           "--type",
                                           opt_type_->currentText().toLower(),
                                       });
            break;
        }
    }
}

// ── Python integration ──────────────────────────────────────────────────────

void DerivativesScreen::run_pricing(const QString& command, const QStringList& args) {
    QStringList full_args;
    full_args << command << args;

    // Pricing is deterministic — cache 10 min
    const QString cache_key = "deriv:" + full_args.join(":");
    const QVariant cached = fincept::CacheManager::instance().get(cache_key);
    if (!cached.isNull()) {
        auto doc = QJsonDocument::fromJson(cached.toString().toUtf8());
        if (!doc.isNull() && doc.isObject()) {
            display_results(doc.object());
            return;
        }
    }

    loading_ = true;
    clear_results();

    QPointer<DerivativesScreen> self = this;

    services::python_cli::PythonCliService::instance().run(
        QStringLiteral("derivatives_pricing.py"), full_args,
        [self, cache_key](const services::python_cli::CliResult& r) {
            if (!self)
                return;

            self->loading_ = false;

            if (!r.success) {
                self->display_error(r.error.isEmpty() ? "Pricing failed" : r.error);
                return;
            }

            const QJsonObject obj = r.data;

            fincept::CacheManager::instance().put(
                cache_key,
                QVariant(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact))),
                10 * 60, "derivatives");

            self->display_results(obj);
        });

    LOG_INFO("Derivatives", "Running: " + command);
}

// ── Results display ─────────────────────────────────────────────────────────

void DerivativesScreen::clear_results() {
    results_container_->hide();

    // Remove old result content (keep the header)
    auto* layout = results_container_->layout();
    if (layout && layout->count() > 1) {
        while (layout->count() > 1) {
            auto* item = layout->takeAt(1);
            if (item->widget()) {
                item->widget()->deleteLater();
            }
            delete item;
        }
    }
}

void DerivativesScreen::display_error(const QString& error) {
    clear_results();

    auto* panel = new QWidget(this);
    panel->setObjectName("derivErrorPanel");
    auto* hl = new QHBoxLayout(panel);
    hl->setContentsMargins(12, 8, 12, 8);
    hl->setSpacing(8);

    auto* err_label = new QLabel("ERROR");
    err_label->setObjectName("derivErrorLabel");
    auto* err_text = new QLabel(error);
    err_text->setObjectName("derivErrorText");
    err_text->setWordWrap(true);

    hl->addWidget(err_label);
    hl->addWidget(err_text, 1);

    results_container_->layout()->addWidget(panel);
    results_container_->show();

    LOG_ERROR("Derivatives", "Calculation error: " + error);
}

void DerivativesScreen::display_results(const QJsonObject& result) {
    clear_results();

    auto* body = new QWidget(this);
    auto* vl = new QVBoxLayout(body);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(8);

    auto make_card = [](const QString& label, const QString& value, bool big = false) {
        auto* card = new QWidget(nullptr);
        card->setObjectName("derivResultCard");
        auto* cv = new QVBoxLayout(card);
        cv->setContentsMargins(8, 6, 8, 6);
        cv->setSpacing(2);

        auto* lbl = new QLabel(label);
        lbl->setObjectName("derivResultLabel");
        auto* val = new QLabel(value);
        val->setObjectName(big ? "derivResultBigValue" : "derivResultValue");

        cv->addWidget(lbl);
        cv->addWidget(val);
        return card;
    };

    // Bond results
    if (result.contains("clean_price")) {
        auto* row1 = new QWidget(this);
        auto* r1l = new QHBoxLayout(row1);
        r1l->setContentsMargins(0, 0, 0, 0);
        r1l->setSpacing(8);
        r1l->addWidget(make_card("CLEAN PRICE", QString::number(result["clean_price"].toDouble(), 'f', 4)));
        r1l->addWidget(make_card("DIRTY PRICE", QString::number(result["dirty_price"].toDouble(), 'f', 4)));
        vl->addWidget(row1);

        auto* row2 = new QWidget(this);
        auto* r2l = new QHBoxLayout(row2);
        r2l->setContentsMargins(0, 0, 0, 0);
        r2l->setSpacing(8);
        r2l->addWidget(make_card("DURATION", QString::number(result["duration"].toDouble(), 'f', 4)));
        r2l->addWidget(make_card("CONVEXITY", QString::number(result["convexity"].toDouble(), 'f', 4)));
        r2l->addWidget(make_card("ACCRUED INT", QString::number(result["accrued_interest"].toDouble(), 'f', 4)));
        vl->addWidget(row2);
    }
    // YTM result
    else if (result.contains("ytm") && !result.contains("clean_price")) {
        vl->addWidget(make_card("YIELD TO MATURITY", QString::number(result["ytm"].toDouble(), 'f', 6) + "%", true));
    }
    // Option results (price + greeks)
    else if (result.contains("price") && result.contains("greeks")) {
        vl->addWidget(make_card("OPTION PRICE", QString::number(result["price"].toDouble(), 'f', 6), true));

        auto greeks = result["greeks"].toObject();
        auto* grow = new QWidget(this);
        auto* gl = new QHBoxLayout(grow);
        gl->setContentsMargins(0, 0, 0, 0);
        gl->setSpacing(8);
        gl->addWidget(make_card("DELTA", QString::number(greeks["delta"].toDouble(), 'f', 6)));
        gl->addWidget(make_card("GAMMA", QString::number(greeks["gamma"].toDouble(), 'f', 6)));
        gl->addWidget(make_card("VEGA", QString::number(greeks["vega"].toDouble(), 'f', 6)));
        gl->addWidget(make_card("THETA", QString::number(greeks["theta"].toDouble(), 'f', 6)));
        gl->addWidget(make_card("RHO", QString::number(greeks["rho"].toDouble(), 'f', 6)));
        vl->addWidget(grow);
    }
    // Implied volatility
    else if (result.contains("implied_volatility")) {
        vl->addWidget(make_card("IMPLIED VOLATILITY",
                                QString::number(result["implied_volatility"].toDouble(), 'f', 4) + "%", true));
    }
    // FX option (price + greeks)
    else if (result.contains("price") && result.contains("notional")) {
        vl->addWidget(make_card("OPTION PRICE", QString::number(result["price"].toDouble(), 'f', 4), true));
        if (result.contains("price_per_unit")) {
            vl->addWidget(make_card("PRICE PER UNIT", QString::number(result["price_per_unit"].toDouble(), 'f', 6)));
        }
        if (result.contains("greeks")) {
            auto greeks = result["greeks"].toObject();
            auto* grow = new QWidget(this);
            auto* gl = new QHBoxLayout(grow);
            gl->setContentsMargins(0, 0, 0, 0);
            gl->setSpacing(8);
            gl->addWidget(make_card("DELTA", QString::number(greeks["delta"].toDouble(), 'f', 6)));
            gl->addWidget(make_card("GAMMA", QString::number(greeks["gamma"].toDouble(), 'f', 6)));
            gl->addWidget(make_card("VEGA", QString::number(greeks["vega"].toDouble(), 'f', 6)));
            gl->addWidget(make_card("THETA", QString::number(greeks["theta"].toDouble(), 'f', 6)));
            gl->addWidget(make_card("RHO", QString::number(greeks["rho"].toDouble(), 'f', 6)));
            vl->addWidget(grow);
        }
    }
    // Swap results
    else if (result.contains("swap_value")) {
        vl->addWidget(make_card("SWAP VALUE", QString::number(result["swap_value"].toDouble(), 'f', 2), true));

        auto* row = new QWidget(this);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(8);
        rl->addWidget(make_card("FIXED LEG PV", QString::number(result["fixed_leg_pv"].toDouble(), 'f', 2)));
        rl->addWidget(make_card("FLOATING LEG PV", QString::number(result["floating_leg_pv"].toDouble(), 'f', 2)));
        rl->addWidget(make_card("PAR SWAP RATE", QString::number(result["par_swap_rate"].toDouble(), 'f', 4) + "%"));
        vl->addWidget(row);
    }
    // CDS results
    else if (result.contains("upfront_value")) {
        vl->addWidget(make_card("UPFRONT VALUE", QString::number(result["upfront_value"].toDouble(), 'f', 2), true));

        auto* row1 = new QWidget(this);
        auto* r1l = new QHBoxLayout(row1);
        r1l->setContentsMargins(0, 0, 0, 0);
        r1l->setSpacing(8);
        r1l->addWidget(make_card("PREMIUM LEG PV", QString::number(result["premium_leg_pv"].toDouble(), 'f', 2)));
        r1l->addWidget(make_card("PROTECTION LEG PV", QString::number(result["protection_leg_pv"].toDouble(), 'f', 2)));
        vl->addWidget(row1);

        auto* row2 = new QWidget(this);
        auto* r2l = new QHBoxLayout(row2);
        r2l->setContentsMargins(0, 0, 0, 0);
        r2l->setSpacing(8);
        r2l->addWidget(
            make_card("BREAKEVEN SPREAD", QString::number(result["breakeven_spread_bps"].toDouble(), 'f', 2) + " bps"));
        r2l->addWidget(make_card("HAZARD RATE", QString::number(result["hazard_rate"].toDouble(), 'f', 4) + "%"));
        r2l->addWidget(
            make_card("SURVIVAL PROB", QString::number(result["survival_probability"].toDouble(), 'f', 2) + "%"));
        vl->addWidget(row2);
    }
    // Generic fallback — display all keys
    else {
        for (auto it = result.begin(); it != result.end(); ++it) {
            QString val_str;
            if (it.value().isDouble()) {
                val_str = QString::number(it.value().toDouble(), 'f', 4);
            } else {
                val_str = it.value().toVariant().toString();
            }
            vl->addWidget(make_card(it.key().toUpper(), val_str));
        }
    }

    results_container_->layout()->addWidget(body);
    results_container_->show();

    LOG_INFO("Derivatives", "Results displayed");
}

// ── IStatefulScreen ──────────────────────────────────────────────────────────

QVariantMap DerivativesScreen::save_state() const {
    return {{"instrument", active_instrument_}};
}

void DerivativesScreen::restore_state(const QVariantMap& state) {
    const int idx = state.value("instrument", -1).toInt();
    if (idx < 0 || idx >= instrument_btns_.size())
        return;
    on_instrument_changed(idx);
}

} // namespace fincept::screens
