#include "screens/derivatives/DerivativesScreen.h"

#include "core/currency/Currency.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "services/python_cli/PythonCliService.h"
#include "storage/cache/CacheManager.h"
#include "ui/theme/StyleSheets.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QDate>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeySequence>
#include <QLocale>
#include <QPointer>
#include <QScrollArea>
#include <QShortcut>
#include <QVBoxLayout>

#include <initializer_list>

namespace fincept::screens {

using namespace fincept::ui;

// ── Stylesheet (applied once at screen level, re-applied on theme change) ────
//
// Built per call: it used to be a static QString evaluated during static
// initialisation, which froze whatever theme tokens existed before main() and
// never followed a theme switch.

static QString deriv_screen_style() {
    return QStringLiteral(
               "#derivScreen { background: %1; }"
                   "#derivHeader { background: %2; border-bottom: 2px solid %3; }"
                   "#derivHeaderIcon { color: %3; font-size: 18px; background: transparent; }"
                   "#derivHeaderTitle { color: %4; font-size: 12px; font-weight: 700; background: transparent; }"
                   "#derivHeaderSub { color: %5; font-size: 9px; letter-spacing: 0.5px; background: transparent; }"
                   "#derivHeaderBadge { color: %5; font-size: 8px; font-weight: 700; border: 1px solid %9; "
                   "  padding: 2px 6px; background: transparent; }"
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
                   "#derivCalcBtn:hover { background: %11; }"
                   "#derivCalcBtn:disabled { background: %11; color: %12; }"
                   "#derivResultsPanel { background: %8; border: 1px solid %9; }"
                   "#derivResultNote { color: %12; font-size: 9px; background: transparent; }"
                   "#derivResultBigValue[sign=\"pos\"] { color: %6; }"
                   "#derivResultBigValue[sign=\"neg\"] { color: %14; }"
                   "#derivResultSection { color: %3; font-size: 9px; font-weight: 700; letter-spacing: 0.5px; "
                   "  background: transparent; }"
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
}

// Spin-box value -> CLI argument. QString::number(double) defaults to 'g' with 6
// significant digits, which silently rounds any input wider than that (an index
// strike of 24350.55 went out as "24350.5", a 12,345,678 spot as "1.23457e+07").
static QString deriv_num(double v) {
    return QString::number(v, 'g', 12);
}

// ── Constructor ─────────────────────────────────────────────────────────────

DerivativesScreen::DerivativesScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("derivScreen");
    setStyleSheet(deriv_screen_style());
    setup_ui();
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this,
            [this](const ThemeTokens&) { setStyleSheet(deriv_screen_style()); });
    LOG_INFO("Derivatives", "Screen constructed");
}

// Enter / Return anywhere inside `card` (spin boxes, dates, combos) runs that
// card's calculator — the panels hold two calculators each, so the shortcut is
// scoped to the card rather than the screen.
static void deriv_bind_enter(QWidget* card, QPushButton* button) {
    for (const auto key : {Qt::Key_Return, Qt::Key_Enter}) {
        auto* sc = new QShortcut(QKeySequence(key), card);
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        QObject::connect(sc, &QShortcut::activated, button, &QPushButton::click);
    }
    button->setToolTip(DerivativesScreen::tr("Calculate (Enter)"));
}

void DerivativesScreen::track_input(const QString& key, QWidget* input) {
    persisted_inputs_.append({key, input});
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

    auto* icon = new QLabel(QString::fromUtf8("\xF0\x9F\x93\x90")); // calculator emoji
    icon->setObjectName("derivHeaderIcon");

    auto* title_col = new QVBoxLayout;
    title_col->setSpacing(0);
    header_title_ = new QLabel(tr("DERIVATIVES PRICING"));
    header_title_->setObjectName("derivHeaderTitle");
    header_sub_ = new QLabel(tr("PROFESSIONAL VALUATION ENGINE"));
    header_sub_->setObjectName("derivHeaderSub");
    title_col->addWidget(header_title_);
    title_col->addWidget(header_sub_);

    hl->addWidget(icon);
    hl->addSpacing(8);
    hl->addLayout(title_col);
    hl->addStretch(1);

    // Descriptive, not a status claim: nothing checks the interpreter before a run.
    header_badge_ = new QLabel(tr("LOCAL PYTHON ENGINE"));
    header_badge_->setObjectName("derivHeaderBadge");
    hl->addWidget(header_badge_);

    return bar;
}

QWidget* DerivativesScreen::create_instrument_bar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("derivInstrBar");
    bar->setFixedHeight(34);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);
    hl->setSpacing(4);

    const QStringList names = {tr("BONDS"), tr("EQUITY OPTIONS"), tr("FX OPTIONS"), tr("IR SWAPS"), tr("CREDIT")};
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
    auto* phi = new QLabel;
    cur::bindLabel(phi, "%1"); // panel icon reflects the preferred currency, live
    phi->setObjectName("derivPanelIcon");
    auto* pht = new QLabel(tr("BOND PRICE CALCULATOR"));
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
    bond_freq_ = create_combo({tr("Annual"), tr("Semi-Annual"), tr("Quarterly")});
    bond_freq_->setCurrentIndex(1);

    bond_ytm_->setToolTip(tr("Yield to maturity, compounded at the coupon frequency (street convention)."));

    bl->addWidget(create_two_col(create_input_row(tr("ISSUE DATE"), bond_issue_date_),
                                 create_input_row(tr("SETTLEMENT DATE"), bond_settle_date_)));
    bl->addWidget(create_input_row(tr("MATURITY DATE"), bond_maturity_date_));
    bl->addWidget(create_two_col(create_input_row(tr("COUPON RATE (% P.A.)"), bond_coupon_),
                                 create_input_row(tr("YTM (%, COUPON-FREQ COMPOUNDING)"), bond_ytm_)));
    bl->addWidget(create_input_row(tr("PAYMENT FREQUENCY"), bond_freq_));

    auto* calc_btn = create_calc_button(tr("CALCULATE BOND PRICE"));
    connect(calc_btn, &QPushButton::clicked, this, &DerivativesScreen::on_calculate);
    bl->addWidget(calc_btn);
    deriv_bind_enter(price_panel, calc_btn);
    // Settlement dates default to today every session and are not restored.
    track_input("bond.issue", bond_issue_date_);
    track_input("bond.maturity", bond_maturity_date_);
    track_input("bond.coupon", bond_coupon_);
    track_input("bond.ytm", bond_ytm_);
    track_input("bond.freq", bond_freq_);

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
    auto* yht = new QLabel(tr("YIELD TO MATURITY"));
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

    // Held as members - on_calculate_secondary() reads these, not the price card's fields.
    ytm_issue_date_ = create_date(QDate(2023, 1, 1));
    ytm_settle_date_ = create_date(QDate::currentDate());
    ytm_maturity_date_ = create_date(QDate(2029, 1, 1));
    ytm_coupon_ = create_spin(5.0, 0, 100, 2, "%");
    ytm_freq_ = create_combo({tr("Annual"), tr("Semi-Annual"), tr("Quarterly")});
    ytm_freq_->setCurrentIndex(1);

    ybl->addWidget(create_two_col(create_input_row(tr("ISSUE DATE"), ytm_issue_date_),
                                  create_input_row(tr("SETTLEMENT DATE"), ytm_settle_date_)));
    ybl->addWidget(create_input_row(tr("MATURITY DATE"), ytm_maturity_date_));
    ybl->addWidget(create_two_col(create_input_row(tr("COUPON RATE (% P.A.)"), ytm_coupon_),
                                  create_input_row(tr("CLEAN PRICE (PER 100 FACE)"), bond_clean_price_)));
    ybl->addWidget(create_input_row(tr("PAYMENT FREQUENCY"), ytm_freq_));

    auto* ytm_btn = create_calc_button(tr("CALCULATE YTM"));
    connect(ytm_btn, &QPushButton::clicked, this, &DerivativesScreen::on_calculate_secondary);
    ybl->addWidget(ytm_btn);
    deriv_bind_enter(ytm_panel, ytm_btn);
    track_input("ytm.issue", ytm_issue_date_);
    track_input("ytm.maturity", ytm_maturity_date_);
    track_input("ytm.coupon", ytm_coupon_);
    track_input("ytm.clean_price", bond_clean_price_);
    track_input("ytm.freq", ytm_freq_);

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
    auto* bhi = new QLabel(QString::fromUtf8("\xE2\x86\x97")); // arrow
    bhi->setObjectName("derivPanelIcon");
    auto* bht = new QLabel(tr("BLACK-SCHOLES PRICING"));
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
    opt_type_ = create_combo({tr("Call"), tr("Put")});

    bbl->addWidget(create_two_col(create_input_row(tr("SPOT PRICE"), opt_spot_),
                                  create_input_row(tr("STRIKE PRICE"), opt_strike_)));
    bbl->addWidget(create_two_col(create_input_row(tr("TIME TO EXPIRY (YEARS)"), opt_time_),
                                  create_input_row(tr("VOLATILITY (% ANNUAL)"), opt_vol_)));
    bbl->addWidget(create_two_col(create_input_row(tr("RISK-FREE RATE (% CONT.)"), opt_rate_),
                                  create_input_row(tr("DIVIDEND YIELD (% CONT.)"), opt_div_)));
    bbl->addWidget(create_input_row(tr("OPTION TYPE (EUROPEAN)"), opt_type_));

    auto* bs_btn = create_calc_button(tr("CALCULATE PRICE & GREEKS"));
    connect(bs_btn, &QPushButton::clicked, this, &DerivativesScreen::on_calculate);
    bbl->addWidget(bs_btn);
    deriv_bind_enter(bs_panel, bs_btn);
    track_input("opt.spot", opt_spot_);
    track_input("opt.strike", opt_strike_);
    track_input("opt.time", opt_time_);
    track_input("opt.vol", opt_vol_);
    track_input("opt.rate", opt_rate_);
    track_input("opt.div", opt_div_);
    track_input("opt.type", opt_type_);

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
    auto* ivhi = new QLabel(QString::fromUtf8("\xE2\x9A\xA1")); // lightning
    ivhi->setObjectName("derivPanelIcon");
    auto* ivht = new QLabel(tr("IMPLIED VOLATILITY"));
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

    // Held as members — on_calculate_secondary() reads these, not the
    // Black-Scholes panel's fields.
    iv_spot_ = create_spin(105.0, 0, 1e8, 2);
    iv_strike_ = create_spin(100.0, 0, 1e8, 2);
    iv_time_ = create_spin(1.0, 0.001, 30, 4);
    iv_rate_ = create_spin(5.0, -20, 100, 2, "%");
    iv_div_ = create_spin(2.0, 0, 100, 2, "%");
    iv_type_ = create_combo({tr("Call"), tr("Put")});

    ivbl->addWidget(
        create_two_col(create_input_row(tr("SPOT PRICE"), iv_spot_), create_input_row(tr("STRIKE PRICE"), iv_strike_)));
    ivbl->addWidget(create_input_row(tr("MARKET OPTION PRICE"), opt_market_price_));
    ivbl->addWidget(create_two_col(create_input_row(tr("TIME TO EXPIRY (YEARS)"), iv_time_),
                                   create_input_row(tr("RISK-FREE RATE (% CONT.)"), iv_rate_)));
    ivbl->addWidget(create_two_col(create_input_row(tr("DIVIDEND YIELD (% CONT.)"), iv_div_),
                                   create_input_row(tr("OPTION TYPE (EUROPEAN)"), iv_type_)));

    auto* iv_btn = create_calc_button(tr("CALCULATE IMPLIED VOL"));
    connect(iv_btn, &QPushButton::clicked, this, &DerivativesScreen::on_calculate_secondary);
    ivbl->addWidget(iv_btn);
    deriv_bind_enter(iv_panel, iv_btn);
    track_input("iv.spot", iv_spot_);
    track_input("iv.strike", iv_strike_);
    track_input("iv.market_price", opt_market_price_);
    track_input("iv.time", iv_time_);
    track_input("iv.rate", iv_rate_);
    track_input("iv.div", iv_div_);
    track_input("iv.type", iv_type_);

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
    auto* icon = new QLabel(QString::fromUtf8("\xF0\x9F\x92\xB1")); // currency emoji
    icon->setObjectName("derivPanelIcon");
    auto* title = new QLabel(tr("FX VANILLA OPTION PRICING"));
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
    fx_type_ = create_combo({tr("Call"), tr("Put")});
    fx_notional_ = create_spin(1000000, 0, 1e12, 0);

    fx_spot_->setToolTip(tr("Domestic currency per 1 unit of foreign currency (e.g. EURUSD: USD per EUR)."));
    fx_notional_->setToolTip(tr("Foreign-currency notional (units of the base currency)."));

    bl->addWidget(create_two_col(create_input_row(tr("SPOT (DOM PER 1 FOR)"), fx_spot_),
                                 create_input_row(tr("STRIKE (DOM PER 1 FOR)"), fx_strike_)));
    bl->addWidget(create_three_col(create_input_row(tr("VOLATILITY (% ANNUAL)"), fx_vol_),
                                   create_input_row(tr("DOMESTIC RATE (% CONT.)"), fx_dom_rate_),
                                   create_input_row(tr("FOREIGN RATE (% CONT.)"), fx_for_rate_)));
    bl->addWidget(create_two_col(create_input_row(tr("TIME TO EXPIRY (YEARS)"), fx_time_),
                                 create_input_row(tr("NOTIONAL (FOREIGN CCY)"), fx_notional_)));
    bl->addWidget(create_input_row(tr("OPTION TYPE (CALL = BUY FOREIGN)"), fx_type_));

    auto* btn = create_calc_button(tr("CALCULATE FX OPTION PRICE"));
    connect(btn, &QPushButton::clicked, this, &DerivativesScreen::on_calculate);
    bl->addWidget(btn);
    deriv_bind_enter(panel, btn);
    track_input("fx.spot", fx_spot_);
    track_input("fx.strike", fx_strike_);
    track_input("fx.time", fx_time_);
    track_input("fx.vol", fx_vol_);
    track_input("fx.dom_rate", fx_dom_rate_);
    track_input("fx.for_rate", fx_for_rate_);
    track_input("fx.notional", fx_notional_);
    track_input("fx.type", fx_type_);

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
    auto* icon = new QLabel(QString::fromUtf8("\xF0\x9F\x94\x84")); // arrows emoji
    icon->setObjectName("derivPanelIcon");
    auto* title = new QLabel(tr("INTEREST RATE SWAP PRICING"));
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
    swap_freq_ = create_combo({tr("Annual"), tr("Semi-Annual"), tr("Quarterly")});
    swap_freq_->setCurrentIndex(1);
    swap_discount_ = create_spin(3.5, -20, 100, 2, "%");
    swap_notional_ = create_spin(1000000, 0, 1e12, 0);

    swap_eff_date_->setToolTip(tr("Start of the swap. The swap is valued AS OF this date (spot-starting): "
                                  "the floating leg is worth par minus the discounted notional."));
    swap_discount_->setToolTip(tr("Flat, continuously-compounded discount rate used for every cash flow."));

    bl->addWidget(create_two_col(create_input_row(tr("EFFECTIVE (VALUATION) DATE"), swap_eff_date_),
                                 create_input_row(tr("MATURITY DATE"), swap_mat_date_)));
    bl->addWidget(create_three_col(create_input_row(tr("FIXED RATE (% P.A.)"), swap_fixed_rate_),
                                   create_input_row(tr("FREQUENCY"), swap_freq_),
                                   create_input_row(tr("DISCOUNT RATE (% CONT., FLAT)"), swap_discount_)));
    bl->addWidget(create_input_row(tr("NOTIONAL AMOUNT"), swap_notional_));

    auto* btn = create_calc_button(tr("CALCULATE SWAP VALUE"));
    connect(btn, &QPushButton::clicked, this, &DerivativesScreen::on_calculate);
    bl->addWidget(btn);
    deriv_bind_enter(panel, btn);
    // The effective (valuation) date defaults to today each session and is not restored.
    track_input("swap.maturity", swap_mat_date_);
    track_input("swap.fixed_rate", swap_fixed_rate_);
    track_input("swap.freq", swap_freq_);
    track_input("swap.discount", swap_discount_);
    track_input("swap.notional", swap_notional_);

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
    auto* icon = new QLabel(QString::fromUtf8("\xF0\x9F\x8E\xAF")); // target emoji
    icon->setObjectName("derivPanelIcon");
    auto* title = new QLabel(tr("CREDIT DEFAULT SWAP PRICING"));
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
    cds_coupon_ = create_spin(100.0, 0, 10000, 1);
    cds_rate_ = create_spin(5.0, -20, 100, 2, "%");
    cds_spread_->setToolTip(tr("Market (par) CDS spread. The flat hazard rate is calibrated so a CDS paying "
                               "this spread is worth zero."));
    cds_coupon_->setToolTip(tr("Premium actually paid under the contract. The value is the protection leg minus "
                               "the premium leg at THIS coupon; set it equal to the spread and the value is zero."));
    cds_rate_->setToolTip(tr("Flat continuously-compounded risk-free rate used to discount both legs."));

    bl->addWidget(create_two_col(create_input_row(tr("VALUATION DATE"), cds_val_date_),
                                 create_input_row(tr("MATURITY DATE"), cds_mat_date_)));
    bl->addWidget(create_three_col(create_input_row(tr("RECOVERY RATE (%)"), cds_recovery_),
                                   create_input_row(tr("MARKET SPREAD (BPS)"), cds_spread_),
                                   create_input_row(tr("NOTIONAL"), cds_notional_)));
    bl->addWidget(create_two_col(create_input_row(tr("CONTRACT COUPON (BPS)"), cds_coupon_),
                                 create_input_row(tr("RISK-FREE RATE (% CONT.)"), cds_rate_)));

    auto* btn = create_calc_button(tr("CALCULATE CDS VALUE"));
    connect(btn, &QPushButton::clicked, this, &DerivativesScreen::on_calculate);
    bl->addWidget(btn);
    deriv_bind_enter(panel, btn);
    // The valuation date defaults to today each session and is not restored.
    track_input("cds.maturity", cds_mat_date_);
    track_input("cds.recovery", cds_recovery_);
    track_input("cds.spread", cds_spread_);
    track_input("cds.notional", cds_notional_);
    track_input("cds.coupon", cds_coupon_);
    track_input("cds.rate", cds_rate_);

    vl->addWidget(body);
    return panel;
}

QWidget* DerivativesScreen::create_status_bar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("derivStatusBar");
    bar->setFixedHeight(26);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);

    status_left_ = new QLabel(tr("DERIVATIVES"));
    status_left_->setObjectName("derivStatusText");
    hl->addWidget(status_left_);
    hl->addStretch(1);

    status_instrument_ = new QLabel(tr("INSTRUMENT: %1").arg(tr("BONDS")));
    status_instrument_->setObjectName("derivStatusText");
    hl->addWidget(status_instrument_);

    hl->addSpacing(16);
    status_engine_ = new QLabel(tr("PYTHON ENGINE"));
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
    results_title_ = new QLabel(tr("RESULTS"));
    results_title_->setObjectName("derivResultsTitle");
    hl->addWidget(results_title_);
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

// ── Live language switch ─────────────────────────────────────────────────────

void DerivativesScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void DerivativesScreen::retranslateUi() {
    // Header
    if (header_title_)
        header_title_->setText(tr("DERIVATIVES PRICING"));
    if (header_sub_)
        header_sub_->setText(tr("PROFESSIONAL VALUATION ENGINE"));
    if (header_badge_)
        header_badge_->setText(tr("LOCAL PYTHON ENGINE"));

    // Instrument selector + status line (same source strings)
    const QStringList names = {tr("BONDS"), tr("EQUITY OPTIONS"), tr("FX OPTIONS"), tr("IR SWAPS"), tr("CREDIT")};
    for (int i = 0; i < instrument_btns_.size() && i < names.size(); ++i)
        instrument_btns_[i]->setText(names[i]);
    if (status_instrument_ && active_instrument_ >= 0 && active_instrument_ < names.size())
        status_instrument_->setText(tr("INSTRUMENT: %1").arg(names[active_instrument_]));

    // Status bar + results header
    if (status_left_)
        status_left_->setText(tr("DERIVATIVES"));
    if (status_engine_)
        status_engine_->setText(tr("PYTHON ENGINE"));
    if (results_title_)
        results_title_->setText(tr("RESULTS"));
    // Panel titles / input labels are re-applied when the panel is rebuilt;
    // their tr() source strings are extracted at construction.
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

    const QStringList names = {tr("BONDS"), tr("EQUITY OPTIONS"), tr("FX OPTIONS"), tr("IR SWAPS"), tr("CREDIT")};
    status_instrument_->setText(tr("INSTRUMENT: %1").arg(names[index]));
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
                                          deriv_num(bond_coupon_->value()),
                                          "--ytm",
                                          deriv_num(bond_ytm_->value()),
                                          "--freq",
                                          QString::number(freq_val),
                                      });
            break;
        }
        case 1: { // Equity options — price + greeks
            run_pricing("option_price", {
                                            "--spot",
                                            deriv_num(opt_spot_->value()),
                                            "--strike",
                                            deriv_num(opt_strike_->value()),
                                            "--time",
                                            deriv_num(opt_time_->value()),
                                            "--rate",
                                            deriv_num(opt_rate_->value()),
                                            "--vol",
                                            deriv_num(opt_vol_->value()),
                                            "--div-yield",
                                            deriv_num(opt_div_->value()),
                                            "--type",
                                            opt_type_->currentIndex() == 0 ? "call" : "put",
                                        });
            break;
        }
        case 2: { // FX options
            run_pricing("fx_option_price", {
                                               "--spot",
                                               deriv_num(fx_spot_->value()),
                                               "--strike",
                                               deriv_num(fx_strike_->value()),
                                               "--time",
                                               deriv_num(fx_time_->value()),
                                               "--domestic-rate",
                                               deriv_num(fx_dom_rate_->value()),
                                               "--foreign-rate",
                                               deriv_num(fx_for_rate_->value()),
                                               "--vol",
                                               deriv_num(fx_vol_->value()),
                                               "--type",
                                               fx_type_->currentIndex() == 0 ? "call" : "put",
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
                                          deriv_num(swap_fixed_rate_->value()),
                                          "--freq",
                                          QString::number(freq_val),
                                          "--notional",
                                          QString::number(swap_notional_->value(), 'f', 0),
                                          "--discount-rate",
                                          deriv_num(swap_discount_->value()),
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
                                         deriv_num(cds_recovery_->value()),
                                         "--notional",
                                         QString::number(cds_notional_->value(), 'f', 0),
                                         "--spread-bps",
                                         deriv_num(cds_spread_->value()),
                                         "--coupon-bps",
                                         deriv_num(cds_coupon_->value()),
                                         "--risk-free-rate",
                                         deriv_num(cds_rate_->value()),
                                     });
            break;
        }
    }
}

void DerivativesScreen::on_calculate_secondary() {
    if (loading_)
        return;

    switch (active_instrument_) {
        case 0: { // Bond YTM from price - solved from the YTM card's OWN inputs.
            int freq_val = (ytm_freq_->currentIndex() == 0) ? 1 : (ytm_freq_->currentIndex() == 1) ? 2 : 4;
            run_pricing("bond_ytm", {
                                        "--issue-date",
                                        ytm_issue_date_->date().toString("yyyy-MM-dd"),
                                        "--settlement-date",
                                        ytm_settle_date_->date().toString("yyyy-MM-dd"),
                                        "--maturity-date",
                                        ytm_maturity_date_->date().toString("yyyy-MM-dd"),
                                        "--coupon-rate",
                                        deriv_num(ytm_coupon_->value()),
                                        "--clean-price",
                                        deriv_num(bond_clean_price_->value()),
                                        "--freq",
                                        QString::number(freq_val),
                                    });
            break;
        }
        case 1: { // Implied volatility — solved from the IV card's OWN inputs.
            if (opt_market_price_->value() <= 0) {
                display_error(tr("Enter the option's market price — implied volatility is solved from it."));
                return;
            }
            run_pricing("implied_vol", {
                                           "--spot",
                                           deriv_num(iv_spot_->value()),
                                           "--strike",
                                           deriv_num(iv_strike_->value()),
                                           "--time",
                                           deriv_num(iv_time_->value()),
                                           "--rate",
                                           deriv_num(iv_rate_->value()),
                                           "--market-price",
                                           deriv_num(opt_market_price_->value()),
                                           "--div-yield",
                                           deriv_num(iv_div_->value()),
                                           "--type",
                                           iv_type_->currentIndex() == 0 ? "call" : "put",
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
    // Persist the inputs that produced this run (debounced by the manager).
    fincept::ScreenStateManager::instance().notify_changed(this);

    // Loading state — the results panel was simply emptied, so a slow Python
    // start looked identical to "the button did nothing".
    if (status_engine_)
        status_engine_->setText(tr("CALCULATING…"));
    {
        auto* busy = new QLabel(tr("Calculating %1 …").arg(command), this);
        busy->setObjectName("derivResultLabel");
        busy->setContentsMargins(12, 10, 12, 10);
        results_container_->layout()->addWidget(busy);
        results_container_->show();
    }

    QPointer<DerivativesScreen> self = this;
    const int requested_instrument = active_instrument_;

    services::python_cli::PythonCliService::instance().run(
        QStringLiteral("derivatives_pricing.py"), full_args,
        [self, cache_key, requested_instrument](const services::python_cli::CliResult& r) {
            if (!self)
                return;

            self->loading_ = false;
            if (self->status_engine_)
                self->status_engine_->setText(DerivativesScreen::tr("PYTHON ENGINE"));

            // The user switched instrument while this ran: its cards no longer apply to
            // the panel on screen (a bond result under the FX panel), so don't draw them.
            // The (deterministic) result is still cached so switching back is instant.
            if (self->active_instrument_ != requested_instrument) {
                if (r.success)
                    fincept::CacheManager::instance().put(
                        cache_key, QVariant(QString::fromUtf8(QJsonDocument(r.data).toJson(QJsonDocument::Compact))),
                        10 * 60, "derivatives");
                return;
            }

            if (!r.success) {
                self->display_error(r.error.isEmpty() ? DerivativesScreen::tr("Pricing failed") : r.error);
                return;
            }

            const QJsonObject obj = r.data;

            fincept::CacheManager::instance().put(
                cache_key, QVariant(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact))), 10 * 60,
                "derivatives");

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

    auto* err_label = new QLabel(tr("ERROR"));
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
    // The pricing script reports solver failures in-band, e.g. implied_vol
    // returns {"error": "Could not converge on implied volatility"} with a zero
    // exit code. Without this the message fell through to the generic key dump
    // and rendered as a result card titled "ERROR" — indistinguishable from a
    // successful calculation at a glance.
    if (result.contains("error") && result.value("error").isString()) {
        const QString msg = result.value("error").toString();
        if (!msg.isEmpty()) {
            display_error(msg);
            return;
        }
    }

    clear_results();

    auto* body = new QWidget(this);
    auto* vl = new QVBoxLayout(body);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(8);

    // A card: LABEL over value. `sign` colours a signed headline value (P&L-like).
    auto make_card = [](const QString& label, const QString& value, bool big = false, double sign = 0.0) {
        auto* card = new QWidget(nullptr);
        card->setObjectName("derivResultCard");
        auto* cv = new QVBoxLayout(card);
        cv->setContentsMargins(8, 6, 8, 6);
        cv->setSpacing(2);

        auto* lbl = new QLabel(label);
        lbl->setObjectName("derivResultLabel");
        auto* val = new QLabel(value);
        val->setObjectName(big ? "derivResultBigValue" : "derivResultValue");
        if (big && sign != 0.0)
            val->setProperty("sign", sign > 0 ? "pos" : "neg");
        val->setTextInteractionFlags(Qt::TextSelectableByMouse);

        cv->addWidget(lbl);
        cv->addWidget(val);
        return card;
    };
    auto add_row = [this, vl](std::initializer_list<QWidget*> cards) {
        auto* row = new QWidget(this);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(8);
        for (QWidget* c : cards)
            rl->addWidget(c);
        vl->addWidget(row);
    };
    auto add_note = [this, vl](const QString& text) {
        auto* note = new QLabel(text, this);
        note->setObjectName("derivResultNote");
        note->setWordWrap(true);
        vl->addWidget(note);
    };
    auto num = [&result](const char* key, int decimals) {
        return QLocale().toString(result.value(QLatin1String(key)).toDouble(), 'f', decimals);
    };
    auto gnum = [](const QJsonObject& o, const char* key, int decimals) {
        return QLocale().toString(o.value(QLatin1String(key)).toDouble(), 'f', decimals);
    };

    // Bond results
    if (result.contains("clean_price")) {
        add_row({make_card(tr("CLEAN PRICE (PER 100)"), num("clean_price", 4), true),
                 make_card(tr("DIRTY PRICE (PER 100)"), num("dirty_price", 4)),
                 make_card(tr("ACCRUED (PER 100)"), num("accrued_interest", 4))});
        add_row({make_card(tr("MACAULAY DURATION (YRS)"), num("duration", 4)),
                 make_card(tr("MODIFIED DURATION (YRS)"), num("modified_duration", 4)),
                 make_card(tr("CONVEXITY (YRS²)"), num("convexity", 4))});
        if (result.contains("next_coupon_date"))
            add_row({make_card(tr("NEXT COUPON"), result.value("next_coupon_date").toString()),
                     make_card(tr("COUPONS REMAINING"), QString::number(result.value("remaining_coupons").toInt()))});
        add_note(tr("Price from a yield compounded at the coupon frequency; accrued interest actual/actual within "
                    "the coupon period. A 1bp yield rise changes the dirty price by about −modified duration × "
                    "dirty price × 0.0001."));
    }
    // YTM result
    else if (result.contains("ytm") && !result.contains("clean_price")) {
        vl->addWidget(make_card(tr("YIELD TO MATURITY"), num("ytm", 6) + "%", true));
        add_note(tr("Compounded at the coupon frequency, solved from the clean price."));
    }
    // FX option: premium for the notional, greeks scaled to the notional
    else if (result.contains("price") && result.contains("notional")) {
        add_row({make_card(tr("PREMIUM (DOMESTIC CCY)"), num("price", 2), true),
                 make_card(tr("PER 1 FOREIGN UNIT (DOM)"), num("price_per_unit", 6)),
                 make_card(tr("% OF FOREIGN NOTIONAL"), num("premium_pct_foreign", 4) + "%")});
        const QJsonObject gn = result.value("greeks_notional").toObject();
        if (!gn.isEmpty()) {
            add_row({make_card(tr("DELTA (FOREIGN CCY)"), gnum(gn, "delta_foreign", 0)),
                     make_card(tr("GAMMA (FOR CCY / 1% SPOT)"), gnum(gn, "gamma_1pct_foreign", 0)),
                     make_card(tr("VEGA (DOM / 1 VOL PT)"), gnum(gn, "vega_domestic", 2)),
                     make_card(tr("THETA (DOM / DAY)"), gnum(gn, "theta_domestic", 2)),
                     make_card(tr("RHO DOM (DOM / 1%)"), gnum(gn, "rho_domestic", 2))});
        }
        const QJsonObject g = result.value("greeks").toObject();
        add_note(tr("Garman-Kohlhagen, continuous rates. Per 1 unit of foreign notional: delta %1, gamma %2 (per 1.0 "
                    "spot move), vega %3, theta %4, rho %5.")
                     .arg(gnum(g, "delta", 6), gnum(g, "gamma", 6), gnum(g, "vega", 6), gnum(g, "theta", 6),
                          gnum(g, "rho", 6)));
    }
    // Equity option results (price + greeks)
    else if (result.contains("price") && result.contains("greeks")) {
        vl->addWidget(make_card(tr("OPTION PRICE"), num("price", 6), true));
        const QJsonObject greeks = result.value("greeks").toObject();
        add_row({make_card(tr("DELTA"), gnum(greeks, "delta", 6)),
                 make_card(tr("GAMMA (PER 1.0 SPOT)"), gnum(greeks, "gamma", 6)),
                 make_card(tr("VEGA (PER 1 VOL PT)"), gnum(greeks, "vega", 6)),
                 make_card(tr("THETA (PER DAY)"), gnum(greeks, "theta", 6)),
                 make_card(tr("RHO (PER 1% RATE)"), gnum(greeks, "rho", 6))});
        add_note(tr("Black-Scholes-Merton, European exercise, continuous rate and dividend yield. Theta is per "
                    "calendar day (annual theta / 365)."));
    }
    // Implied volatility
    else if (result.contains("implied_volatility")) {
        vl->addWidget(make_card(tr("IMPLIED VOLATILITY (ANNUAL)"), num("implied_volatility", 4) + "%", true));
    }
    // Swap results
    else if (result.contains("swap_value")) {
        const double value = result.value("swap_value").toDouble();
        add_row({make_card(tr("SWAP VALUE (PAY FIXED)"), num("swap_value", 2), true, value),
                 make_card(tr("PAR SWAP RATE"), num("par_swap_rate", 4) + "%")});
        add_row({make_card(tr("FIXED LEG PV"), num("fixed_leg_pv", 2)),
                 make_card(tr("FLOATING LEG PV"), num("floating_leg_pv", 2)),
                 make_card(tr("TENOR (YRS)"), num("tenor_years", 2)),
                 make_card(tr("PAYMENTS"), QString::number(result.value("payments").toInt()))});
        // The sign of SWAP VALUE is meaningless without saying whose side it is.
        add_note(tr("Valued at the effective date to the PAYER of fixed / receiver of floating, on a flat "
                    "continuously-compounded curve. Positive = the fixed rate is below the par rate."));
    }
    // CDS results
    else if (result.contains("upfront_value")) {
        const double value = result.value("upfront_value").toDouble();
        add_row({make_card(tr("VALUE TO PROTECTION BUYER"), num("upfront_value", 2), true, value),
                 make_card(tr("UPFRONT (POINTS)"), num("upfront_points", 4)),
                 make_card(tr("RISKY PV01 (PER 1BP)"), num("risky_pv01", 2))});
        add_row({make_card(tr("PREMIUM LEG PV"), num("premium_leg_pv", 2)),
                 make_card(tr("PROTECTION LEG PV"), num("protection_leg_pv", 2)),
                 make_card(tr("PAR SPREAD"), num("breakeven_spread_bps", 2) + " bps")});
        add_row({make_card(tr("HAZARD RATE (% / YR)"), num("hazard_rate", 4) + "%"),
                 make_card(tr("SURVIVAL TO MATURITY"), num("survival_probability", 2) + "%"),
                 make_card(tr("PREMIUM PAYMENTS"), QString::number(result.value("premium_payments").toInt()))});
        add_note(tr("Flat hazard rate calibrated to the market spread; quarterly premiums with accrual on default; "
                    "flat continuously-compounded discounting. Positive value = the protection buyer pays "
                    "upfront."));
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
    // The last inputs of every calculator, so a desk set-up survives a restart.
    // As-of dates (settlement / effective / valuation) are deliberately absent:
    // they default to today each session rather than silently re-pricing a stale date.
    QVariantMap inputs;
    for (const auto& [key, widget] : persisted_inputs_) {
        if (auto* spin = qobject_cast<QDoubleSpinBox*>(widget))
            inputs.insert(key, spin->value());
        else if (auto* date = qobject_cast<QDateEdit*>(widget))
            inputs.insert(key, date->date().toString(Qt::ISODate));
        else if (auto* combo = qobject_cast<QComboBox*>(widget))
            inputs.insert(key, combo->currentIndex());
    }
    return {{"instrument", active_instrument_}, {"inputs", inputs}};
}

void DerivativesScreen::restore_state(const QVariantMap& state) {
    const QVariantMap inputs = state.value("inputs").toMap();
    for (const auto& [key, widget] : persisted_inputs_) {
        const auto it = inputs.constFind(key);
        if (it == inputs.constEnd())
            continue;
        if (auto* spin = qobject_cast<QDoubleSpinBox*>(widget)) {
            spin->setValue(it.value().toDouble());
        } else if (auto* date = qobject_cast<QDateEdit*>(widget)) {
            const QDate d = QDate::fromString(it.value().toString(), Qt::ISODate);
            if (d.isValid())
                date->setDate(d);
        } else if (auto* combo = qobject_cast<QComboBox*>(widget)) {
            const int i = it.value().toInt();
            if (i >= 0 && i < combo->count())
                combo->setCurrentIndex(i);
        }
    }

    const int idx = state.value("instrument", -1).toInt();
    if (idx < 0 || idx >= instrument_btns_.size())
        return;
    on_instrument_changed(idx);
}

} // namespace fincept::screens
