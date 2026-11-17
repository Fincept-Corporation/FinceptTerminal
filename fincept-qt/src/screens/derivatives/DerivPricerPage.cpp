// DerivPricerPage — construction, inputs, state and "load from market".
// Calculation and result rendering live in DerivPricerCompute.cpp.

#include "screens/derivatives/DerivPricerPage.h"

#include "core/instrument/Instrument.h"
#include "core/logging/Logger.h"
#include "services/company_snapshot/CompanySnapshotService.h"
#include "services/markets/MarketDataService.h"
#include "services/options/CurveDataService.h"
#include "services/options/FreeOptionChainService.h"
#include "services/options/VolSurface.h"
#include "ui/formatting/AmountInput.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/AutoLoadPolicy.h"
#include "ui/widgets/FormGridMetrics.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/PanelHeader.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QBoxLayout>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequence>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStyle>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens::deriv {

namespace md = services::options::models;
using services::options::CurveDataService;
using services::options::FreeOptionChainService;

namespace {

// Result-area views of the pane (the long-standing cache keys).
const char* const kPricerPrimaryView[] = {"bond", "equity", "fx", "swap", "cds"};
const char* const kPricerSecondaryView[] = {"bond_ytm", "equity_iv"};
/// Currencies offered for the money results of the FX, swap and CDS calculators.
const QStringList kPricerCurrencies = {"USD", "EUR", "GBP", "JPY", "CHF", "CAD", "AUD", "NZD", "INR", "CNY", "HKD", "SGD"};

} // namespace

DerivPricerPage::DerivPricerPage(DerivContext* ctx, QWidget* parent)
    : QWidget(parent), ctx_(ctx), book_("fincept::screens::deriv::DerivPricerPage") {
    build();
}

DerivPricerPage::~DerivPricerPage() {
    if (chain_request_)
        FreeOptionChainService::instance().cancel(chain_request_);
    if (snapshot_request_)
        services::CompanySnapshotService::instance().cancel(snapshot_request_);
}

QString DerivPricerPage::instrument_name(int index) const {
    static const char* const kNames[] = {QT_TR_NOOP("BONDS"), QT_TR_NOOP("EQUITY OPTIONS"), QT_TR_NOOP("FX OPTIONS"),
                                         QT_TR_NOOP("IR SWAPS"), QT_TR_NOOP("CREDIT")};
    return (index >= 0 && index < 5) ? tr(kNames[index]) : QString();
}

// ── Construction ─────────────────────────────────────────────────────────────

void DerivPricerPage::build() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(build_instrument_bar());

    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("derivScroll"));
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    root->addWidget(scroll, 1);
    auto* content = new QWidget;
    scroll->setWidget(content); // still empty: moving it is free
    // The input column beside the results on a wide page, above them on a narrow one
    // (apply_responsive flips the direction; the results always take the spare space).
    split_ = new QBoxLayout(QBoxLayout::LeftToRight, content);
    split_->setContentsMargins(12, 12, 12, 12);
    split_->setSpacing(12);

    inputs_col_ = new QWidget(content);
    auto* il = new QVBoxLayout(inputs_col_);
    il->setContentsMargins(0, 0, 0, 0);
    il->setSpacing(0);
    stack_ = new QStackedWidget(inputs_col_);
    il->addWidget(stack_);
    il->addStretch(1);
    stack_->addWidget(build_bonds(stack_));
    stack_->addWidget(build_equity(stack_));
    stack_->addWidget(build_fx(stack_));
    stack_->addWidget(build_swaps(stack_));
    stack_->addWidget(build_credit(stack_));
    split_->addWidget(inputs_col_, 0);
    results_frame_ = build_results(content);
    split_->addWidget(results_frame_, 1);
    update_equity_rows();
    fit_stack();
    apply_responsive();
    // A new font size: field widths, then the breakpoint again (after every widget has its style).
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this] {
        form_.refit();
        apply_responsive();
    });
}

QWidget* DerivPricerPage::card_page(QWidget* page, const QList<QWidget*>& cards) {
    // `page` is the cards' parent already (built in place, nothing is re-parented).
    auto* bl = new QBoxLayout(QBoxLayout::TopToBottom, page);
    bl->setContentsMargins(0, 0, 0, 0);
    bl->setSpacing(12);
    for (QWidget* c : cards) {
        // A card is as wide as its form, never stretched across the page.
        c->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
        bl->addWidget(c);
    }
    bl->addStretch(1);
    card_rows_.append({bl, cards});
    return page;
}

void DerivPricerPage::fit_stack() {
    if (!stack_)
        return;
    for (int i = 0; i < stack_->count(); ++i) {
        QWidget* w = stack_->widget(i);
        const QSizePolicy::Policy p = i == stack_->currentIndex() ? QSizePolicy::Preferred : QSizePolicy::Ignored;
        if (w->sizePolicy().verticalPolicy() != p)
            w->setSizePolicy(p, p);
    }
}

void DerivPricerPage::apply_responsive() {
    if (!split_ || !inputs_col_)
        return;
    namespace fg = ui::form_grid;
    const int gap = split_->spacing();
    const QMargins m = split_->contentsMargins();
    // The page width less the content margins and a vertical scroll bar (reserved either way,
    // so the decision never depends on whether the content scrolls).
    const int avail = width() - m.left() - m.right() - style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    int lead = 0;
    for (const CardRow& r : card_rows_)
        for (QWidget* c : r.cards)
            lead = std::max(lead, c->sizeHint().width());
    // The results need room for about four KPI tiles side by side.
    const int trail_min = QFontMetrics(ui::fonts::font(-2)).horizontalAdvance(QLatin1Char('0')) * 64;
    const bool wide = fg::side_by_side(avail, lead, trail_min, gap);
    if (wide_ != static_cast<int>(wide)) {
        wide_ = static_cast<int>(wide);
        split_->setDirection(wide ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
    }
    // Several cards of one calculator: one under the other in the input column; side by side
    // above the results when the page is narrow and they fit, else one under the other.
    for (const CardRow& r : card_rows_) {
        int need = 0;
        for (QWidget* c : r.cards)
            need += c->sizeHint().width();
        need += gap * std::max<int>(0, static_cast<int>(r.cards.size()) - 1);
        const QBoxLayout::Direction dir =
            !wide && r.cards.size() > 1 && need <= avail ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom;
        if (r.layout->direction() != dir)
            r.layout->setDirection(dir);
    }
}

void DerivPricerPage::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    apply_responsive();
}

QWidget* DerivPricerPage::build_instrument_bar() {
    auto* bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("derivInstrBar"));
    bar->setAttribute(Qt::WA_StyledBackground, true);
    bar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 3, 12, 3);
    hl->setSpacing(2);
    for (int i = 0; i < 5; ++i) {
        auto* b = new QPushButton(instrument_name(i), bar);
        b->setObjectName(QStringLiteral("derivInstrBtn"));
        b->setCursor(Qt::PointingHandCursor);
        b->setProperty("active", i == 0);
        connect(b, &QPushButton::clicked, this, [this, i] { on_instrument_changed(i); });
        hl->addWidget(b);
        instr_btns_.append(b);
    }
    hl->addStretch(1);
    return bar;
}

QGridLayout* DerivPricerPage::grid_in(QVBoxLayout* body, QWidget** host) {
    auto* w = new QWidget(body->parentWidget());
    auto* g = new QGridLayout(w);
    g->setContentsMargins(0, 0, 0, 0);
    g->setHorizontalSpacing(FormFit::kGap);
    g->setVerticalSpacing(4);
    g->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    body->addWidget(w);
    if (host)
        *host = w;
    return g;
}

QLabel* DerivPricerPage::field(QGridLayout* grid, int row, int col, const char* src, QWidget* input, int span) {
    QLabel* l = add_field(grid, row, col, tr(src), input, span);
    book_.label(l, src);
    form_.add(input, l, span); // fixed cell width from the input font: the card's grids line up
    return l;
}

QPushButton* DerivPricerPage::calc_button(QWidget* parent, const char* src) {
    auto* b = make_button(parent, tr(src), true);
    book_.button(b, src);
    return b;
}

// Enter / Return anywhere inside `card` runs that card's calculator (two per panel).
void DerivPricerPage::bind_enter(QWidget* card, QPushButton* button) {
    for (const auto key : {Qt::Key_Return, Qt::Key_Enter}) {
        auto* sc = new QShortcut(QKeySequence(key), card);
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sc, &QShortcut::activated, button, [button] {
            // The shortcut takes the key before the focused number box sees it, and a box that
            // does not track the keyboard reads its typed text only on Enter or focus-out: read
            // it now, so the calculation uses what is on screen.
            for (QWidget* w = QApplication::focusWidget(); w; w = w->parentWidget())
                if (auto* box = qobject_cast<QAbstractSpinBox*>(w)) {
                    box->interpretText();
                    break;
                }
            button->click();
        });
    }
    button->setToolTip(tr("Calculate (Enter)"));
}

void DerivPricerPage::track(const QString& key, QWidget* input) {
    persisted_.append({key, input});
    if (auto* s = qobject_cast<QDoubleSpinBox*>(input))
        connect(s, &QDoubleSpinBox::valueChanged, this, [this] { emit state_changed(); });
    else if (auto* i = qobject_cast<QSpinBox*>(input))
        connect(i, &QSpinBox::valueChanged, this, [this] { emit state_changed(); });
    else if (auto* c = qobject_cast<QComboBox*>(input))
        connect(c, &QComboBox::currentIndexChanged, this, [this] { emit state_changed(); });
    else if (auto* d = qobject_cast<QDateEdit*>(input))
        connect(d, &QDateEdit::dateChanged, this, [this] { emit state_changed(); });
}

QWidget* DerivPricerPage::build_bonds(QWidget* parent) {
    // Two compact cards of three short fields per row; the calculator's button under its form.
    // Widgets are created in reading order, which is the Tab order.
    auto* page = new QWidget(parent);
    Card price = make_card(page, QStringLiteral("BOND"), tr("BOND PRICE CALCULATOR"));
    book_.label(price.title, QT_TR_NOOP("BOND PRICE CALCULATOR"));
    QWidget* gh = nullptr;
    QGridLayout* g = grid_in(price.body, &gh);
    bond_issue_ = make_date(gh, QDate(2023, 1, 1));
    bond_settle_ = make_date(gh, QDate::currentDate());
    bond_maturity_ = make_date(gh, QDate(2029, 1, 1));
    bond_coupon_ = make_spin(gh, 5.0, 0, 100, 2, QStringLiteral("%"));
    bond_ytm_ = make_spin(gh, 4.5, -10, 100, 2, QStringLiteral("%"));
    bond_freq_ = make_combo(gh, {tr("Annual"), tr("Semi-Annual"), tr("Quarterly")});
    book_.combo(bond_freq_, {QT_TR_NOOP("Annual"), QT_TR_NOOP("Semi-Annual"), QT_TR_NOOP("Quarterly")});
    bond_freq_->setCurrentIndex(1);
    bond_ytm_->setToolTip(tr("Yield to maturity, compounded at the coupon frequency (street convention)."));
    field(g, 0, 0, QT_TR_NOOP("ISSUE DATE"), bond_issue_);
    field(g, 0, 1, QT_TR_NOOP("SETTLEMENT DATE"), bond_settle_);
    field(g, 0, 2, QT_TR_NOOP("MATURITY DATE"), bond_maturity_);
    field(g, 1, 0, QT_TR_NOOP("COUPON RATE (% P.A.)"), bond_coupon_);
    field(g, 1, 1, QT_TR_NOOP("YTM (%, COUPON-FREQ COMPOUNDING)"), bond_ytm_);
    field(g, 1, 2, QT_TR_NOOP("PAYMENT FREQUENCY"), bond_freq_);
    auto* calc = calc_button(price.frame, QT_TR_NOOP("CALCULATE BOND PRICE"));
    price.body->addWidget(calc);
    connect(calc, &QPushButton::clicked, this, &DerivPricerPage::on_calculate);
    bind_enter(price.frame, calc);
    // Settlement dates default to today every session and are not restored.
    track(QStringLiteral("bond.issue"), bond_issue_);
    track(QStringLiteral("bond.maturity"), bond_maturity_);
    track(QStringLiteral("bond.coupon"), bond_coupon_);
    track(QStringLiteral("bond.ytm"), bond_ytm_);
    track(QStringLiteral("bond.freq"), bond_freq_);

    Card ytm = make_card(page, QStringLiteral("YTM"), tr("YIELD TO MATURITY"));
    book_.label(ytm.title, QT_TR_NOOP("YIELD TO MATURITY"));
    QGridLayout* y = grid_in(ytm.body, &gh);
    ytm_issue_ = make_date(gh, QDate(2023, 1, 1));
    ytm_settle_ = make_date(gh, QDate::currentDate());
    ytm_maturity_ = make_date(gh, QDate(2029, 1, 1));
    ytm_coupon_ = make_spin(gh, 5.0, 0, 100, 2, QStringLiteral("%"));
    bond_clean_price_ = make_spin(gh, 102.0, 0, 10000, 2);
    ytm_freq_ = make_combo(gh, {tr("Annual"), tr("Semi-Annual"), tr("Quarterly")});
    book_.combo(ytm_freq_, {QT_TR_NOOP("Annual"), QT_TR_NOOP("Semi-Annual"), QT_TR_NOOP("Quarterly")});
    ytm_freq_->setCurrentIndex(1);
    field(y, 0, 0, QT_TR_NOOP("ISSUE DATE"), ytm_issue_);
    field(y, 0, 1, QT_TR_NOOP("SETTLEMENT DATE"), ytm_settle_);
    field(y, 0, 2, QT_TR_NOOP("MATURITY DATE"), ytm_maturity_);
    field(y, 1, 0, QT_TR_NOOP("COUPON RATE (% P.A.)"), ytm_coupon_);
    field(y, 1, 1, QT_TR_NOOP("CLEAN PRICE (PER 100 FACE)"), bond_clean_price_);
    field(y, 1, 2, QT_TR_NOOP("PAYMENT FREQUENCY"), ytm_freq_);
    auto* ycalc = calc_button(ytm.frame, QT_TR_NOOP("CALCULATE YTM"));
    ytm.body->addWidget(ycalc);
    connect(ycalc, &QPushButton::clicked, this, &DerivPricerPage::on_calculate_secondary);
    bind_enter(ytm.frame, ycalc);
    track(QStringLiteral("ytm.issue"), ytm_issue_);
    track(QStringLiteral("ytm.maturity"), ytm_maturity_);
    track(QStringLiteral("ytm.coupon"), ytm_coupon_);
    track(QStringLiteral("ytm.clean_price"), bond_clean_price_);
    track(QStringLiteral("ytm.freq"), ytm_freq_);
    return card_page(page, {price.frame, ytm.frame});
}

QWidget* DerivPricerPage::build_equity(QWidget* parent) {
    auto* page = new QWidget(parent);

    // ── Pricing card ── (three short fields per row; the two long choices span the form)
    Card card = make_card(page, QStringLiteral("OPT"), tr("OPTION PRICING"));
    book_.label(card.title, QT_TR_NOOP("OPTION PRICING"));
    QWidget* gh = nullptr;
    QGridLayout* m = grid_in(card.body, &gh);
    sym_box_ = new ui::SymbolSearchBox(gh);
    sym_box_->set_compact(true);
    sym_box_->set_clear_on_pick(false);
    sym_box_->set_placeholder(tr("Load from market: ticker or name (optional)"));
    services::ResolveOptions ro;
    ro.ctx.allowed_classes = {core::instrument::AssetClass::Equity, core::instrument::AssetClass::Etf,
                              core::instrument::AssetClass::Index};
    sym_box_->set_options(ro);
    sym_box_->set_recent_key(QStringLiteral("derivatives.underlying"));
    field(m, 0, 0, QT_TR_NOOP("LOAD FROM MARKET (SPOT, DIVIDEND, RATE, IV)"), sym_box_, 3);
    market_status_ = new QLabel(gh);
    market_status_->setObjectName(QStringLiteral("derivStatusLine"));
    market_status_->setWordWrap(true);
    market_status_->setText(tr("Optional: pick a symbol to fill spot, dividend yield, the Treasury rate at this tenor "
                               "and the chain's implied volatility. Typed values are never cleared."));
    m->addWidget(market_status_, 2, 0, 1, 3);
    form_.add_wide(market_status_, 3);
    connect(sym_box_, &ui::SymbolSearchBox::instrument_picked, this, &DerivPricerPage::on_symbol_picked);
    connect(sym_box_, &ui::SymbolSearchBox::unresolved, this,
            [this](const QStringList&, const QString& msg) { set_market_status(msg, true); });

    QGridLayout* g = grid_in(card.body, &gh);
    opt_type_ = make_combo(gh, {tr("Call"), tr("Put")});
    book_.combo(opt_type_, {QT_TR_NOOP("Call"), QT_TR_NOOP("Put")});
    opt_exercise_ = make_combo(gh, {tr("European"), tr("American")});
    book_.combo(opt_exercise_, {QT_TR_NOOP("European"), QT_TR_NOOP("American")});
    opt_div_mode_ = make_combo(gh, {tr("Continuous yield"), tr("Discrete cash dividends")});
    book_.combo(opt_div_mode_, {QT_TR_NOOP("Continuous yield"), QT_TR_NOOP("Discrete cash dividends")});
    opt_model_ = make_combo(gh, {tr("Black-Scholes-Merton"), tr("Binomial (Cox-Ross-Rubinstein)"),
                                 tr("Barone-Adesi-Whaley"), tr("Black-76 (option on a forward)"),
                                 tr("Bachelier (normal model)")});
    book_.combo(opt_model_, {QT_TR_NOOP("Black-Scholes-Merton"), QT_TR_NOOP("Binomial (Cox-Ross-Rubinstein)"),
                             QT_TR_NOOP("Barone-Adesi-Whaley"), QT_TR_NOOP("Black-76 (option on a forward)"),
                             QT_TR_NOOP("Bachelier (normal model)")});
    opt_payoff_ = make_combo(gh, {tr("Vanilla"), tr("Digital: cash-or-nothing"), tr("Digital: asset-or-nothing"),
                                  tr("Barrier (continuous monitoring)"), tr("Asian: arithmetic average"),
                                  tr("Asian: geometric average"), tr("Spread on two forwards (Kirk)")});
    book_.combo(opt_payoff_, {QT_TR_NOOP("Vanilla"), QT_TR_NOOP("Digital: cash-or-nothing"),
                              QT_TR_NOOP("Digital: asset-or-nothing"), QT_TR_NOOP("Barrier (continuous monitoring)"),
                              QT_TR_NOOP("Asian: arithmetic average"), QT_TR_NOOP("Asian: geometric average"),
                              QT_TR_NOOP("Spread on two forwards (Kirk)")});
    opt_spot_ = make_spin(gh, 105.0, 0, 1e8, 2);
    opt_strike_ = make_spin(gh, 100.0, -1e8, 1e8, 2);
    opt_time_ = make_spin(gh, 1.0, 0.001, 30, 4);
    opt_vol_ = make_spin(gh, 25.0, 0, 500, 2, QStringLiteral("%"));
    opt_rate_ = make_spin(gh, 5.0, -20, 100, 2, QStringLiteral("%"));
    field(g, 0, 0, QT_TR_NOOP("OPTION TYPE"), opt_type_);
    field(g, 0, 1, QT_TR_NOOP("EXERCISE"), opt_exercise_);
    field(g, 0, 2, QT_TR_NOOP("DIVIDENDS"), opt_div_mode_);
    field(g, 1, 0, QT_TR_NOOP("MODEL"), opt_model_, 3);
    field(g, 2, 0, QT_TR_NOOP("PAYOFF"), opt_payoff_, 3);
    opt_spot_label_ = field(g, 3, 0, QT_TR_NOOP("SPOT PRICE"), opt_spot_);
    opt_strike_label_ = field(g, 3, 1, QT_TR_NOOP("STRIKE PRICE"), opt_strike_);
    field(g, 3, 2, QT_TR_NOOP("TIME TO EXPIRY (YEARS)"), opt_time_);
    field(g, 4, 0, QT_TR_NOOP("VOLATILITY (% ANNUAL)"), opt_vol_);
    field(g, 4, 1, QT_TR_NOOP("RISK-FREE RATE (% CONT.)"), opt_rate_);

    // Conditional groups: each its own little grid, shown for the model / payoff that needs it.
    // The dividend yield takes the free third cell of the rate row (its caption and box line up
    // with the row's, the group spanning the caption and input rows).
    grp_yield_ = new QWidget(gh);
    auto* yg = new QGridLayout(grp_yield_);
    yg->setContentsMargins(0, 0, 0, 0);
    yg->setHorizontalSpacing(FormFit::kGap);
    yg->setVerticalSpacing(g->verticalSpacing());
    opt_div_ = make_spin(grp_yield_, 2.0, -50, 100, 2, QStringLiteral("%"));
    field(yg, 0, 0, QT_TR_NOOP("DIVIDEND YIELD (% CONT.)"), opt_div_);
    g->addWidget(grp_yield_, 8, 2, 2, 1);

    QGridLayout* dg = grid_in(card.body, &grp_divs_);
    opt_divs_ = new QTableWidget(4, 2, grp_divs_);
    dense_table(opt_divs_);
    opt_divs_->setHorizontalHeaderLabels({tr("Days from today"), tr("Cash amount")});
    opt_divs_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    opt_divs_->setMaximumHeight(opt_divs_->horizontalHeader()->sizeHint().height() +
                                4 * opt_divs_->verticalHeader()->defaultSectionSize() + 4);
    opt_divs_->setToolTip(tr("Escrowed-dividend model: the tree (or Black-Scholes) runs on spot minus the present "
                             "value of these dividends. Leave rows empty to ignore them."));
    field(dg, 0, 0, QT_TR_NOOP("DISCRETE DIVIDENDS (UP TO 4)"), opt_divs_, 3);
    connect(opt_divs_, &QTableWidget::itemChanged, this, [this] { emit state_changed(); });

    QGridLayout* sg = grid_in(card.body, &grp_steps_);
    // Up to 2,000 steps: the price, its Greeks and the sweeps stay within a few tens of
    // milliseconds on the GUI thread (O(steps²) per tree).
    opt_steps_ = make_int_spin(grp_steps_, 500, 10, 2000);
    field(sg, 0, 0, QT_TR_NOOP("TREE STEPS"), opt_steps_);

    QGridLayout* ng = grid_in(card.body, &grp_normal_);
    opt_normal_vol_ = make_spin(grp_normal_, 25.0, 0, 1e7, 4);
    opt_normal_vol_->setToolTip(tr("Absolute volatility of the forward in price units per square-root year "
                                   "(e.g. 25 for a 100 forward is about 25 % lognormal at the money)."));
    field(ng, 0, 0, QT_TR_NOOP("NORMAL VOL (PRICE UNITS PER YEAR^0.5)"), opt_normal_vol_);

    QGridLayout* cg = grid_in(card.body, &grp_cash_);
    opt_cash_ = make_spin(grp_cash_, 10.0, 0, 1e9, 2);
    field(cg, 0, 0, QT_TR_NOOP("CASH PAYOUT"), opt_cash_);

    QGridLayout* bg = grid_in(card.body, &grp_barrier_);
    opt_barrier_kind_ = make_combo(grp_barrier_, {tr("Down-and-out"), tr("Down-and-in"), tr("Up-and-out"), tr("Up-and-in")});
    book_.combo(opt_barrier_kind_, {QT_TR_NOOP("Down-and-out"), QT_TR_NOOP("Down-and-in"), QT_TR_NOOP("Up-and-out"),
                                    QT_TR_NOOP("Up-and-in")});
    opt_barrier_ = make_spin(grp_barrier_, 95.0, 0, 1e8, 2);
    opt_rebate_ = make_spin(grp_barrier_, 0.0, 0, 1e8, 2);
    field(bg, 0, 0, QT_TR_NOOP("BARRIER TYPE"), opt_barrier_kind_);
    field(bg, 0, 1, QT_TR_NOOP("BARRIER LEVEL"), opt_barrier_);
    field(bg, 0, 2, QT_TR_NOOP("REBATE"), opt_rebate_);

    QGridLayout* ag = grid_in(card.body, &grp_asian_);
    opt_avg_so_far_ = make_spin(grp_asian_, 0.0, 0, 1e9, 4);
    opt_avg_so_far_->setToolTip(tr("The average of the underlying observed so far, when the averaging period has "
                                   "already started. 0 = averaging starts today."));
    opt_avg_elapsed_ = make_spin(grp_asian_, 0.0, 0, 30, 4);
    opt_avg_elapsed_->setToolTip(tr("Years of the averaging period already run (the time to expiry is what is left)."));
    field(ag, 0, 0, QT_TR_NOOP("AVERAGE SO FAR (0 = NOT STARTED)"), opt_avg_so_far_);
    field(ag, 0, 1, QT_TR_NOOP("YEARS ALREADY AVERAGED"), opt_avg_elapsed_);

    QGridLayout* pg = grid_in(card.body, &grp_spread_);
    opt_f2_ = make_spin(grp_spread_, 95.0, 0, 1e8, 2);
    opt_vol2_ = make_spin(grp_spread_, 30.0, 0, 500, 2, QStringLiteral("%"));
    opt_rho_ = make_spin(grp_spread_, 0.5, -1, 1, 2);
    field(pg, 0, 0, QT_TR_NOOP("SECOND FORWARD (F2)"), opt_f2_);
    field(pg, 0, 1, QT_TR_NOOP("VOLATILITY OF F2"), opt_vol2_);
    field(pg, 0, 2, QT_TR_NOOP("CORRELATION"), opt_rho_);

    QGridLayout* qg = grid_in(card.body, &gh);
    opt_qty_ = make_spin(gh, 1, -1e6, 1e6, 0);
    opt_mult_ = make_spin(gh, 100, 1, 1e6, 0);
    opt_qty_->setToolTip(tr("Signed number of contracts (negative = short) for the position's dollar Greeks."));
    field(qg, 0, 0, QT_TR_NOOP("CONTRACTS (POSITION)"), opt_qty_);
    field(qg, 0, 1, QT_TR_NOOP("MULTIPLIER"), opt_mult_);

    model_note_ = make_note(card.frame, QString());
    form_.add_wide(model_note_, 3); // wraps at the form's width
    card.body->addWidget(model_note_);
    auto* calc = calc_button(card.frame, QT_TR_NOOP("CALCULATE PRICE & GREEKS"));
    card.body->addWidget(calc);
    connect(calc, &QPushButton::clicked, this, &DerivPricerPage::on_calculate);
    bind_enter(card.frame, calc);
    for (QComboBox* c : {opt_exercise_, opt_model_, opt_payoff_, opt_div_mode_})
        connect(c, &QComboBox::currentIndexChanged, this, [this] { update_equity_rows(); });

    track(QStringLiteral("opt.spot"), opt_spot_);
    track(QStringLiteral("opt.strike"), opt_strike_);
    track(QStringLiteral("opt.time"), opt_time_);
    track(QStringLiteral("opt.vol"), opt_vol_);
    track(QStringLiteral("opt.rate"), opt_rate_);
    track(QStringLiteral("opt.div"), opt_div_);
    track(QStringLiteral("opt.type"), opt_type_);
    track(QStringLiteral("opt.exercise"), opt_exercise_);
    track(QStringLiteral("opt.model"), opt_model_);
    track(QStringLiteral("opt.payoff"), opt_payoff_);
    track(QStringLiteral("opt.div_mode"), opt_div_mode_);
    track(QStringLiteral("opt.steps"), opt_steps_);
    track(QStringLiteral("opt.normal_vol"), opt_normal_vol_);
    track(QStringLiteral("opt.cash"), opt_cash_);
    track(QStringLiteral("opt.barrier_kind"), opt_barrier_kind_);
    track(QStringLiteral("opt.barrier"), opt_barrier_);
    track(QStringLiteral("opt.rebate"), opt_rebate_);
    track(QStringLiteral("opt.f2"), opt_f2_);
    track(QStringLiteral("opt.vol2"), opt_vol2_);
    track(QStringLiteral("opt.rho"), opt_rho_);
    track(QStringLiteral("opt.qty"), opt_qty_);
    track(QStringLiteral("opt.mult"), opt_mult_);
    track(QStringLiteral("opt.avg_so_far"), opt_avg_so_far_);
    track(QStringLiteral("opt.avg_elapsed"), opt_avg_elapsed_);

    // ── Implied volatility ──
    Card ivc = make_card(page, QStringLiteral("IV"), tr("IMPLIED VOLATILITY"));
    book_.label(ivc.title, QT_TR_NOOP("IMPLIED VOLATILITY"));
    QGridLayout* v = grid_in(ivc.body, &gh);
    iv_spot_ = make_spin(gh, 105.0, -1e8, 1e8, 2);
    iv_strike_ = make_spin(gh, 100.0, -1e8, 1e8, 2);
    iv_price_ = make_spin(gh, 15.0, 0, 1e8, 4);
    iv_time_ = make_spin(gh, 1.0, 0.001, 30, 4);
    iv_rate_ = make_spin(gh, 5.0, -20, 100, 2, QStringLiteral("%"));
    iv_div_ = make_spin(gh, 2.0, -50, 100, 2, QStringLiteral("%"));
    iv_model_ = make_combo(gh, {tr("Black-Scholes-Merton (European)"), tr("American: binomial"),
                                tr("American: Barone-Adesi-Whaley"), tr("Black-76 (on a forward)"),
                                tr("Bachelier: normal vol")});
    book_.combo(iv_model_, {QT_TR_NOOP("Black-Scholes-Merton (European)"), QT_TR_NOOP("American: binomial"),
                            QT_TR_NOOP("American: Barone-Adesi-Whaley"), QT_TR_NOOP("Black-76 (on a forward)"),
                            QT_TR_NOOP("Bachelier: normal vol")});
    iv_type_ = make_combo(gh, {tr("Call"), tr("Put")});
    book_.combo(iv_type_, {QT_TR_NOOP("Call"), QT_TR_NOOP("Put")});
    field(v, 0, 0, QT_TR_NOOP("SPOT / FORWARD"), iv_spot_);
    field(v, 0, 1, QT_TR_NOOP("STRIKE PRICE"), iv_strike_);
    field(v, 0, 2, QT_TR_NOOP("MARKET OPTION PRICE"), iv_price_);
    field(v, 1, 0, QT_TR_NOOP("TIME TO EXPIRY (YEARS)"), iv_time_);
    field(v, 1, 1, QT_TR_NOOP("RISK-FREE RATE (% CONT.)"), iv_rate_);
    field(v, 1, 2, QT_TR_NOOP("DIVIDEND YIELD (% CONT.)"), iv_div_);
    field(v, 2, 0, QT_TR_NOOP("MODEL"), iv_model_, 2);
    field(v, 2, 2, QT_TR_NOOP("OPTION TYPE"), iv_type_);
    auto* ivcalc = calc_button(ivc.frame, QT_TR_NOOP("CALCULATE IMPLIED VOL"));
    ivc.body->addWidget(ivcalc);
    connect(ivcalc, &QPushButton::clicked, this, &DerivPricerPage::on_calculate_secondary);
    bind_enter(ivc.frame, ivcalc);
    track(QStringLiteral("iv.spot"), iv_spot_);
    track(QStringLiteral("iv.strike"), iv_strike_);
    track(QStringLiteral("iv.market_price"), iv_price_);
    track(QStringLiteral("iv.time"), iv_time_);
    track(QStringLiteral("iv.rate"), iv_rate_);
    track(QStringLiteral("iv.div"), iv_div_);
    track(QStringLiteral("iv.type"), iv_type_);
    track(QStringLiteral("iv.model"), iv_model_);
    return card_page(page, {card.frame, ivc.frame});
}

QWidget* DerivPricerPage::build_fx(QWidget* parent) {
    auto* page = new QWidget(parent);
    Card card = make_card(page, QStringLiteral("FX"), tr("FX VANILLA OPTION PRICING"));
    book_.label(card.title, QT_TR_NOOP("FX VANILLA OPTION PRICING"));
    QWidget* gh = nullptr;
    QGridLayout* g = grid_in(card.body, &gh);
    fx_spot_ = make_spin(gh, 1.08, 0, 1e6, 4);
    fx_strike_ = make_spin(gh, 1.10, 0, 1e6, 4);
    fx_time_ = make_spin(gh, 1.0, 0.001, 30, 4);
    fx_vol_ = make_spin(gh, 12.0, 0, 500, 2, QStringLiteral("%"));
    fx_dom_ = make_spin(gh, 2.0, -20, 100, 2, QStringLiteral("%"));
    fx_for_ = make_spin(gh, 1.5, -20, 100, 2, QStringLiteral("%"));
    fx_notional_ = make_spin(gh, 1000000, 0, 1e12, 0);
    fx_type_ = make_combo(gh, {tr("Call"), tr("Put")});
    book_.combo(fx_type_, {QT_TR_NOOP("Call"), QT_TR_NOOP("Put")});
    fx_ccy_ = make_combo(gh, kPricerCurrencies);
    fx_spot_->setToolTip(tr("Domestic currency per 1 unit of foreign currency (e.g. EURUSD: USD per EUR)."));
    fx_notional_->setToolTip(tr("Foreign-currency notional (units of the base currency)."));
    fx_ccy_->setToolTip(tr("Domestic currency: the premium and the desk Greeks are amounts in it."));
    field(g, 0, 0, QT_TR_NOOP("SPOT (DOM PER 1 FOR)"), fx_spot_);
    field(g, 0, 1, QT_TR_NOOP("STRIKE (DOM PER 1 FOR)"), fx_strike_);
    field(g, 0, 2, QT_TR_NOOP("TIME TO EXPIRY (YEARS)"), fx_time_);
    field(g, 1, 0, QT_TR_NOOP("VOLATILITY (% ANNUAL)"), fx_vol_);
    field(g, 1, 1, QT_TR_NOOP("DOMESTIC RATE (% CONT.)"), fx_dom_);
    field(g, 1, 2, QT_TR_NOOP("FOREIGN RATE (% CONT.)"), fx_for_);
    field(g, 2, 0, QT_TR_NOOP("NOTIONAL (FOREIGN CCY)"), fx_notional_);
    field(g, 2, 1, QT_TR_NOOP("OPTION TYPE (CALL = BUY FOREIGN)"), fx_type_);
    field(g, 2, 2, QT_TR_NOOP("DOMESTIC CURRENCY"), fx_ccy_);
    auto* calc = calc_button(card.frame, QT_TR_NOOP("CALCULATE FX OPTION PRICE"));
    card.body->addWidget(calc);
    connect(calc, &QPushButton::clicked, this, &DerivPricerPage::on_calculate);
    bind_enter(card.frame, calc);
    track(QStringLiteral("fx.spot"), fx_spot_);
    track(QStringLiteral("fx.strike"), fx_strike_);
    track(QStringLiteral("fx.time"), fx_time_);
    track(QStringLiteral("fx.vol"), fx_vol_);
    track(QStringLiteral("fx.dom_rate"), fx_dom_);
    track(QStringLiteral("fx.for_rate"), fx_for_);
    track(QStringLiteral("fx.notional"), fx_notional_);
    track(QStringLiteral("fx.type"), fx_type_);
    track(QStringLiteral("fx.ccy"), fx_ccy_);
    return card_page(page, {card.frame});
}

QWidget* DerivPricerPage::build_swaps(QWidget* parent) {
    auto* page = new QWidget(parent);
    Card card = make_card(page, QStringLiteral("IRS"), tr("INTEREST RATE SWAP PRICING"));
    book_.label(card.title, QT_TR_NOOP("INTEREST RATE SWAP PRICING"));
    QWidget* gh = nullptr;
    QGridLayout* g = grid_in(card.body, &gh);
    swap_eff_ = make_date(gh, QDate::currentDate());
    swap_mat_ = make_date(gh, QDate::currentDate().addYears(5));
    swap_freq_ = make_combo(gh, {tr("Annual"), tr("Semi-Annual"), tr("Quarterly")});
    book_.combo(swap_freq_, {QT_TR_NOOP("Annual"), QT_TR_NOOP("Semi-Annual"), QT_TR_NOOP("Quarterly")});
    swap_freq_->setCurrentIndex(1);
    swap_fixed_ = make_spin(gh, 3.0, -20, 100, 2, QStringLiteral("%"));
    swap_disc_ = make_spin(gh, 3.5, -20, 100, 2, QStringLiteral("%"));
    swap_notional_ = make_spin(gh, 1000000, 0, 1e12, 0);
    swap_ccy_ = make_combo(gh, kPricerCurrencies);
    swap_eff_->setToolTip(tr("Start of the swap. The swap is valued AS OF this date (spot-starting): "
                             "the floating leg is worth par minus the discounted notional."));
    swap_disc_->setToolTip(tr("Flat, continuously-compounded discount rate used for every cash flow."));
    field(g, 0, 0, QT_TR_NOOP("EFFECTIVE (VALUATION) DATE"), swap_eff_);
    field(g, 0, 1, QT_TR_NOOP("MATURITY DATE"), swap_mat_);
    field(g, 0, 2, QT_TR_NOOP("FREQUENCY"), swap_freq_);
    field(g, 1, 0, QT_TR_NOOP("FIXED RATE (% P.A.)"), swap_fixed_);
    field(g, 1, 1, QT_TR_NOOP("DISCOUNT RATE (% CONT., FLAT)"), swap_disc_);
    field(g, 1, 2, QT_TR_NOOP("NOTIONAL AMOUNT"), swap_notional_);
    field(g, 2, 0, QT_TR_NOOP("CURRENCY"), swap_ccy_);
    auto* calc = calc_button(card.frame, QT_TR_NOOP("CALCULATE SWAP VALUE"));
    card.body->addWidget(calc);
    connect(calc, &QPushButton::clicked, this, &DerivPricerPage::on_calculate);
    bind_enter(card.frame, calc);
    // The effective (valuation) date defaults to today each session and is not restored.
    track(QStringLiteral("swap.maturity"), swap_mat_);
    track(QStringLiteral("swap.fixed_rate"), swap_fixed_);
    track(QStringLiteral("swap.freq"), swap_freq_);
    track(QStringLiteral("swap.discount"), swap_disc_);
    track(QStringLiteral("swap.notional"), swap_notional_);
    track(QStringLiteral("swap.ccy"), swap_ccy_);
    return card_page(page, {card.frame});
}

QWidget* DerivPricerPage::build_credit(QWidget* parent) {
    auto* page = new QWidget(parent);
    Card card = make_card(page, QStringLiteral("CDS"), tr("CREDIT DEFAULT SWAP PRICING"));
    book_.label(card.title, QT_TR_NOOP("CREDIT DEFAULT SWAP PRICING"));
    QWidget* gh = nullptr;
    QGridLayout* g = grid_in(card.body, &gh);
    cds_val_ = make_date(gh, QDate::currentDate());
    cds_mat_ = make_date(gh, QDate::currentDate().addYears(5));
    cds_ccy_ = make_combo(gh, kPricerCurrencies);
    cds_recovery_ = make_spin(gh, 40.0, 0, 100, 2, QStringLiteral("%"));
    cds_spread_ = make_spin(gh, 150.0, 0, 10000, 1);
    cds_coupon_ = make_spin(gh, 100.0, 0, 10000, 1);
    cds_notional_ = make_spin(gh, 10000000, 0, 1e12, 0);
    cds_rate_ = make_spin(gh, 5.0, -20, 100, 2, QStringLiteral("%"));
    cds_spread_->setToolTip(tr("Market (par) CDS spread. The flat hazard rate is calibrated so a CDS paying "
                               "this spread is worth zero."));
    cds_coupon_->setToolTip(tr("Premium actually paid under the contract. The value is the protection leg minus "
                               "the premium leg at THIS coupon; set it equal to the spread and the value is zero."));
    cds_rate_->setToolTip(tr("Flat continuously-compounded risk-free rate used to discount both legs."));
    field(g, 0, 0, QT_TR_NOOP("VALUATION DATE"), cds_val_);
    field(g, 0, 1, QT_TR_NOOP("MATURITY DATE"), cds_mat_);
    field(g, 0, 2, QT_TR_NOOP("CURRENCY"), cds_ccy_);
    field(g, 1, 0, QT_TR_NOOP("RECOVERY RATE (%)"), cds_recovery_);
    field(g, 1, 1, QT_TR_NOOP("MARKET SPREAD (BPS)"), cds_spread_);
    field(g, 1, 2, QT_TR_NOOP("CONTRACT COUPON (BPS)"), cds_coupon_);
    field(g, 2, 0, QT_TR_NOOP("NOTIONAL"), cds_notional_);
    field(g, 2, 1, QT_TR_NOOP("RISK-FREE RATE (% CONT.)"), cds_rate_);
    auto* calc = calc_button(card.frame, QT_TR_NOOP("CALCULATE CDS VALUE"));
    card.body->addWidget(calc);
    connect(calc, &QPushButton::clicked, this, &DerivPricerPage::on_calculate);
    bind_enter(card.frame, calc);
    // The valuation date defaults to today each session and is not restored.
    track(QStringLiteral("cds.maturity"), cds_mat_);
    track(QStringLiteral("cds.recovery"), cds_recovery_);
    track(QStringLiteral("cds.spread"), cds_spread_);
    track(QStringLiteral("cds.notional"), cds_notional_);
    track(QStringLiteral("cds.coupon"), cds_coupon_);
    track(QStringLiteral("cds.rate"), cds_rate_);
    track(QStringLiteral("cds.ccy"), cds_ccy_);
    return card_page(page, {card.frame});
}

QWidget* DerivPricerPage::build_results(QWidget* parent) {
    auto* frame = new QWidget(parent);
    frame->setObjectName(QStringLiteral("derivResultsPanel"));
    auto* vl = new QVBoxLayout(frame);
    vl->setContentsMargins(1, 1, 1, 1); // the panel's hairline border stays visible around the header
    vl->setSpacing(0);
    // The shared one-line panel header: RESULTS, then (right) the as-of line and the exports.
    auto* hdr = new ui::PanelHeader(tr("RESULTS"), frame);
    results_title_ = hdr->title_label();
    book_.label(results_title_, QT_TR_NOOP("RESULTS"));
    results_meta_ = new QLabel(hdr->controls());
    results_meta_->setObjectName(QStringLiteral("derivResultsMeta"));
    results_meta_->setWordWrap(true); // wraps rather than widening the panel past a narrow window
    hdr->add_control(results_meta_, 1);
    result_bar_ = new DerivResultBar(QStringLiteral("derivatives_pricer"), hdr->controls());
    result_bar_->set_table([this] { return result_table(); });
    result_bar_->set_image([this]() -> QPixmap { return last_chart_ ? ui::ChartExport::grab(last_chart_) : QPixmap(); });
    result_bar_->set_payload([this] { return report_payload(); });
    hdr->add_control(result_bar_);
    vl->addWidget(hdr);

    // The shared never-blank mechanism: one view per calculator; the first show of a view
    // shows its remembered result and prices it with the inputs on screen (local C++,
    // delivered synchronously).
    pane_ = new ui::AutoLoadPane(frame);
    results_body_ = new QWidget(pane_->content_parent());
    auto* bl = new QVBoxLayout(results_body_);
    bl->setContentsMargins(0, 0, 0, 0);
    bl->setSpacing(0);
    pane_->set_content(results_body_);
    pane_->set_cache_key(QStringLiteral("derivatives"));
    // Microseconds, and the settlement / valuation dates are "today": every show re-prices.
    pane_->set_max_age(ui::autoload::kAlwaysRefresh);
    pane_->set_loading_text(tr("Pricing…"));
    pane_->set_status_label(results_meta_);
    pane_->skeleton()->set_kpi_count(4);
    pane_->skeleton()->set_row_count(3);
    pane_->set_start([this](quint64 id) {
        QString error;
        const QJsonObject payload = compute_view(pane_->view(), &error);
        emit state_changed();
        if (!error.isEmpty())
            pane_->fail(id, error, tr("Check the inputs")); // every pricing error is an input problem
        else
            pane_->deliver(id, payload);
    });
    pane_->set_render([this](const QJsonValue& payload, const QDateTime&, bool) { render_payload(payload); });
    pane_->set_view(QString::fromLatin1(kPricerPrimaryView[0]));
    vl->addWidget(pane_, 1); // the panel's spare height goes to the result (its chart grows)
    return frame;
}

// ── Model / payoff dependent inputs ──────────────────────────────────────────

void DerivPricerPage::update_equity_rows() {
    if (!opt_model_)
        return;
    const int payoff = opt_payoff_->currentIndex();
    const bool vanilla = payoff == 0;
    auto* mm = qobject_cast<QStandardItemModel*>(opt_model_->model());
    auto set_enabled = [mm](int row, bool on) {
        if (mm && mm->item(row))
            mm->item(row)->setEnabled(on);
    };
    // Exotics are European closed forms (Black-Scholes-Merton world).
    opt_exercise_->setEnabled(vanilla);
    if (!vanilla && opt_exercise_->currentIndex() != 0)
        opt_exercise_->setCurrentIndex(0);
    const bool american = vanilla && opt_exercise_->currentIndex() == 1;
    set_enabled(0, vanilla && !american); // BSM
    set_enabled(1, vanilla);              // binomial
    set_enabled(2, american);             // BAW
    set_enabled(3, vanilla && !american); // Black-76
    set_enabled(4, vanilla && !american); // Bachelier
    opt_model_->setEnabled(vanilla);
    int model = opt_model_->currentIndex();
    if (!vanilla && model != 0) {
        QSignalBlocker block(opt_model_);
        opt_model_->setCurrentIndex(0);
        model = 0;
    } else if (american && model != 1 && model != 2) {
        QSignalBlocker block(opt_model_);
        opt_model_->setCurrentIndex(1);
        model = 1;
    } else if (!american && model == 2) {
        QSignalBlocker block(opt_model_);
        opt_model_->setCurrentIndex(0);
        model = 0;
    }
    const bool on_forward = vanilla && (model == 3 || model == 4);
    const bool spread = payoff == 6;
    // Barone-Adesi-Whaley has no discrete-dividend form: with cash dividends it falls back to
    // the binomial tree (escrowed dividends), and the note says so.
    const bool discrete_ok = vanilla && (model == 0 || model == 1 || model == 2);
    if (!discrete_ok && opt_div_mode_->currentIndex() == 1) {
        QSignalBlocker block(opt_div_mode_);
        opt_div_mode_->setCurrentIndex(0);
    }
    auto* dm = qobject_cast<QStandardItemModel*>(opt_div_mode_->model());
    if (dm && dm->item(1))
        dm->item(1)->setEnabled(discrete_ok);
    const bool discrete = opt_div_mode_->currentIndex() == 1;
    opt_div_mode_->setEnabled(!on_forward && !spread);
    grp_yield_->setVisible(!on_forward && !spread && !discrete);
    grp_divs_->setVisible(!on_forward && !spread && discrete);
    const bool baw_tree = vanilla && model == 2 && discrete;
    grp_steps_->setVisible(vanilla && (model == 1 || baw_tree));
    grp_normal_->setVisible(vanilla && model == 4);
    opt_vol_->setEnabled(!(vanilla && model == 4));
    grp_cash_->setVisible(payoff == 1);
    grp_barrier_->setVisible(payoff == 3);
    grp_asian_->setVisible(payoff == 4);
    grp_spread_->setVisible(spread);
    opt_spot_label_->setText(on_forward || spread ? (spread ? tr("FIRST FORWARD (F1)") : tr("FORWARD / FUTURES PRICE"))
                                                  : tr("SPOT PRICE"));
    opt_strike_label_->setText(spread ? tr("SPREAD STRIKE (F1 - F2)") : tr("STRIKE PRICE"));
    QString note;
    if (payoff == 4)
        note = tr("Arithmetic average, Turnbull-Wakeman approximation (European, Black-Scholes-Merton world). When the "
                  "averaging has already started, give the average so far and the years already averaged: the rest is "
                  "priced on an adjusted strike (Haug §4.20.2); if that strike is below zero a call is certain to be "
                  "exercised and is worth its discounted forward. Greeks by finite differences.");
    else if (!vanilla)
        note = tr("Exotic payoffs are priced in closed form (European, Black-Scholes-Merton world); Greeks by "
                  "finite differences.");
    else if (baw_tree)
        note = tr("Barone-Adesi-Whaley has no form for discrete cash dividends, so this option is priced on the "
                  "Cox-Ross-Rubinstein tree instead (escrowed dividends, the tree steps below, early exercise at "
                  "every step). Switch the dividends to a continuous yield to use Barone-Adesi-Whaley itself.");
    else if (model == 1)
        note = american ? tr("Cox-Ross-Rubinstein tree with early exercise at every step; Greeks from the tree "
                             "(vega and rho by re-pricing).")
                        : tr("Cox-Ross-Rubinstein tree, European exercise (converges to Black-Scholes-Merton).");
    else if (model == 2)
        note = tr("Barone-Adesi & Whaley (1987) quadratic approximation of American exercise with a continuous "
                  "dividend yield; Greeks by finite differences.");
    else if (model == 3)
        note = tr("Black (1976): the input is the forward or futures price, discounted at the risk-free rate.");
    else if (model == 4)
        note = tr("Bachelier: the forward moves in price units (negative prices allowed); the volatility is absolute.");
    else
        note = tr("Black-Scholes-Merton, European exercise, continuous rate and dividend yield.");
    model_note_->setText(note);
}

std::vector<md::CashDividend> DerivPricerPage::dividends(QString* error) const {
    namespace fmt = fincept::ui::formatting;
    std::vector<md::CashDividend> out;
    if (!opt_divs_)
        return out;
    auto report = [error](const QString& why) {
        if (error && error->isEmpty())
            *error = why;
    };
    for (int r = 0; r < opt_divs_->rowCount(); ++r) {
        const QTableWidgetItem* d = opt_divs_->item(r, 0);
        const QTableWidgetItem* a = opt_divs_->item(r, 1);
        const QString days_text = d ? d->text().trimmed() : QString();
        const QString amount_text = a ? a->text().trimmed() : QString();
        if (days_text.isEmpty() && amount_text.isEmpty())
            continue; // an unused row
        const fmt::AmountInput days = fmt::read_number_input(days_text);
        if (!days.ok() || !(*days.value > 0)) {
            report(tr("Dividend row %1: enter the days from today as a positive number.").arg(r + 1));
            continue;
        }
        // The shared amount reader: the user's separators, symbols, pence for a pence-quoted share; a
        // cash amount in another currency than the underlying's is refused, never converted.
        const fmt::AmountInput amount = fmt::read_amount_input(amount_text, market_ccy_);
        if (!amount.ok()) {
            report(tr("Dividend row %1: %2").arg(r + 1).arg(amount.error));
            continue;
        }
        if (!(*amount.value > 0)) {
            report(tr("Dividend row %1: the cash amount must be positive.").arg(r + 1));
            continue;
        }
        out.push_back({*days.value / 365.0, *amount.value});
    }
    return out;
}

QString DerivPricerPage::dividends_text() const {
    QStringList parts;
    for (const md::CashDividend& d : dividends())
        parts << QStringLiteral("%1:%2").arg(QString::number(d.time * 365.0, 'f', 0), QString::number(d.amount, 'g', 10));
    return parts.join(QLatin1Char(';'));
}

void DerivPricerPage::set_dividends_text(const QString& text) {
    if (!opt_divs_)
        return;
    QSignalBlocker block(opt_divs_);
    opt_divs_->clearContents();
    const QStringList parts = text.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    // Stored machine-readable ("30:0.82"); shown in the user's number format, which the readers above
    // read back exactly (C-locale text such as "0.825" would read as 825 under a 1.234,5 locale).
    auto shown = [](const QString& stored) {
        bool ok = false;
        const double v = QLocale::c().toDouble(stored.trimmed(), &ok);
        return ok ? fincept::ui::formatting::amount_input_text(v, 6) : stored;
    };
    for (int r = 0; r < parts.size() && r < opt_divs_->rowCount(); ++r) {
        const QStringList kv = parts[r].split(QLatin1Char(':'));
        if (kv.size() != 2)
            continue;
        opt_divs_->setItem(r, 0, new QTableWidgetItem(shown(kv[0])));
        opt_divs_->setItem(r, 1, new QTableWidgetItem(shown(kv[1])));
    }
}

// ── Instrument switching / calculate ─────────────────────────────────────────

void DerivPricerPage::on_instrument_changed(int index) {
    if (index < 0 || index >= 5)
        return;
    active_ = index;
    stack_->setCurrentIndex(index);
    fit_stack();
    for (int i = 0; i < instr_btns_.size(); ++i) {
        instr_btns_[i]->setProperty("active", i == index);
        instr_btns_[i]->style()->unpolish(instr_btns_[i]);
        instr_btns_[i]->style()->polish(instr_btns_[i]);
    }
    // Never an empty result area: this instrument's last result at once, then priced.
    if (pane_)
        pane_->set_view(QString::fromLatin1(kPricerPrimaryView[index]));
    emit instrument_changed(instrument_name(index));
    emit state_changed();
}

void DerivPricerPage::on_calculate() {
    if (!pane_)
        return;
    pane_->set_view(QString::fromLatin1(kPricerPrimaryView[active_]));
    pane_->run();
}

void DerivPricerPage::on_calculate_secondary() {
    if (!pane_ || active_ > 1) // only bonds (yield from price) and equity options (implied vol)
        return;
    pane_->set_view(QString::fromLatin1(kPricerSecondaryView[active_]));
    pane_->run();
}

// ── Language ─────────────────────────────────────────────────────────────────

void DerivPricerPage::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(e);
}

void DerivPricerPage::retranslate() {
    book_.apply();
    for (int i = 0; i < instr_btns_.size(); ++i)
        instr_btns_[i]->setText(instrument_name(i));
    update_equity_rows(); // the model-dependent captions and note
    if (sym_box_)
        sym_box_->set_placeholder(tr("Load from market: ticker or name (optional)"));
    if (pane_ && pane_->has_result())
        render_payload(pane_->payload()); // tiles and notes in the new language
}

// ── State ────────────────────────────────────────────────────────────────────

QVariantMap DerivPricerPage::save_state() const {
    // The last inputs of every calculator. As-of dates (settlement / effective / valuation)
    // are deliberately absent: they default to today rather than re-pricing a stale date.
    QVariantMap inputs;
    for (const auto& [key, widget] : persisted_) {
        if (auto* spin = qobject_cast<QDoubleSpinBox*>(widget))
            inputs.insert(key, spin->value());
        else if (auto* ispin = qobject_cast<QSpinBox*>(widget))
            inputs.insert(key, ispin->value());
        else if (auto* date = qobject_cast<QDateEdit*>(widget))
            inputs.insert(key, date->date().toString(Qt::ISODate));
        else if (auto* combo = qobject_cast<QComboBox*>(widget))
            inputs.insert(key, combo->currentIndex());
    }
    inputs.insert(QStringLiteral("opt.divs"), dividends_text());
    if (!market_symbol_.isEmpty())
        inputs.insert(QStringLiteral("opt.symbol"), market_symbol_);
    return {{QStringLiteral("instrument"), active_}, {QStringLiteral("inputs"), inputs},
            {QStringLiteral("chart_view"), chart_view_}};
}

void DerivPricerPage::restore_state(const QVariantMap& state) {
    const QVariantMap inputs = state.value(QStringLiteral("inputs")).toMap();
    // Combos first: their handlers adjust which other inputs apply.
    for (int pass = 0; pass < 2; ++pass) {
        for (const auto& [key, widget] : persisted_) {
            const auto it = inputs.constFind(key);
            if (it == inputs.constEnd())
                continue;
            auto* combo = qobject_cast<QComboBox*>(widget);
            if ((pass == 0) != (combo != nullptr))
                continue;
            if (combo) {
                const int i = it.value().toInt();
                if (i >= 0 && i < combo->count())
                    combo->setCurrentIndex(i);
            } else if (auto* spin = qobject_cast<QDoubleSpinBox*>(widget)) {
                spin->setValue(it.value().toDouble());
            } else if (auto* ispin = qobject_cast<QSpinBox*>(widget)) {
                ispin->setValue(it.value().toInt());
            } else if (auto* date = qobject_cast<QDateEdit*>(widget)) {
                const QDate d = QDate::fromString(it.value().toString(), Qt::ISODate);
                if (d.isValid())
                    date->setDate(d);
            }
        }
    }
    set_dividends_text(inputs.value(QStringLiteral("opt.divs")).toString());
    market_symbol_ = inputs.value(QStringLiteral("opt.symbol")).toString();
    if (!market_symbol_.isEmpty() && sym_box_)
        sym_box_->set_text(market_symbol_);
    chart_view_ = std::clamp(state.value(QStringLiteral("chart_view"), 0).toInt(), 0, 8);
    update_equity_rows();
    const int idx = state.value(QStringLiteral("instrument"), -1).toInt();
    if (idx >= 0 && idx < instr_btns_.size())
        on_instrument_changed(idx);
}

// ── Load from market (D2) ────────────────────────────────────────────────────

void DerivPricerPage::set_market_status(const QString& text, bool warn) {
    if (!market_status_)
        return;
    market_status_->setText(text);
    market_status_->setObjectName(warn ? QStringLiteral("derivChip") : QStringLiteral("derivStatusLine"));
    market_status_->setProperty("tone", warn ? QStringLiteral("warn") : QString());
    market_status_->style()->unpolish(market_status_);
    market_status_->style()->polish(market_status_);
}

void DerivPricerPage::on_symbol_picked(const core::instrument::Instrument& inst) {
    const QString sym = inst.symbol.isEmpty() ? inst.display : inst.symbol;
    if (sym.isEmpty())
        return;
    market_symbol_ = sym;
    market_ccy_ = inst.currency.isEmpty() ? QStringLiteral("USD") : inst.currency;
    set_market_status(tr("Fetching price for %1…").arg(inst.display.isEmpty() ? sym : inst.display));
    QPointer<DerivPricerPage> self(this);
    services::MarketDataService::instance().fetch_quotes(
        {sym}, [self, sym](bool ok, QVector<services::QuoteData> quotes) {
            if (!self || self->market_symbol_ != sym)
                return;
            const services::QuoteData* q = nullptr;
            for (const auto& x : quotes)
                if (x.price > 0)
                    q = &x;
            if (!ok || !q) {
                self->set_market_status(tr("No price found for %1 - type the spot yourself.").arg(sym), true);
                return;
            }
            self->opt_spot_->setValue(q->price);
            if (!q->currency.isEmpty())
                self->market_ccy_ = q->currency;
            QString when;
            if (q->market_time > 0)
                when = QDateTime::fromSecsSinceEpoch(q->market_time).toString(QStringLiteral("d MMM HH:mm"));
            self->set_market_status(tr("%1 spot %2 %3%4 (delayed quote)")
                                        .arg(sym, num(q->price, 2), self->market_ccy_,
                                             when.isEmpty() ? QString() : tr(" as of %1").arg(when)));
            self->apply_rate_for_tenor();
        });
    // Dividend yield, name, currency and the 52-week range from the shared company snapshot
    // (quote side only: one Yahoo call, cached 10 minutes and shared with the other tabs). It
    // also says when a company pays nothing, which the old info call could not tell apart
    // from a missing figure.
    auto& snaps = services::CompanySnapshotService::instance();
    if (snapshot_request_)
        snaps.cancel(snapshot_request_);
    services::CompanySnapshotService::Options so;
    so.financials = false;
    so.sec_fallback = false;
    snapshot_request_ = snaps.fetch(sym, so, this, [self, sym](const services::CompanySnapshot& s) {
        if (!self || self->market_symbol_ != sym)
            return;
        self->snapshot_request_ = 0;
        if (!s.ok)
            return; // the quote above still fills the spot; the typed yield stays
        if (s.has(services::CompanySnapshot::kDividendYield) && s.dividend_yield >= 0 && s.dividend_yield < 0.5) {
            self->opt_div_mode_->setCurrentIndex(0);
            self->opt_div_->setValue(s.dividend_yield * 100.0);
        }
        QStringList extra;
        if (!s.name.isEmpty())
            extra << s.name;
        if (s.has(services::CompanySnapshot::kDividendYield))
            extra << tr("dividend yield %1").arg(vol_pct(s.dividend_yield));
        if (std::isfinite(s.week52_low) && std::isfinite(s.week52_high))
            extra << tr("52-week range %1–%2").arg(num(s.week52_low, 2), num(s.week52_high, 2));
        if (!extra.isEmpty())
            self->opt_div_->setToolTip(extra.join(QStringLiteral(" · ")) + QStringLiteral(" (") + s.caption() +
                                       QLatin1Char(')'));
    });
    // The chain's implied volatility at this strike and tenor (free delayed chain).
    auto& chains = FreeOptionChainService::instance();
    if (auto c = chains.cached(sym); c && c->ok()) {
        apply_chain_iv(c);
    } else {
        if (chain_request_)
            chains.cancel(chain_request_);
        chain_request_ = chains.fetch(sym, QStringLiteral("auto"), false, this,
                                      [self, sym](const FreeOptionChainService::Reply& r) {
                                          if (!self || self->market_symbol_ != sym)
                                              return;
                                          if (r.chain && r.chain->ok())
                                              self->apply_chain_iv(r.chain);
                                      });
    }
}

void DerivPricerPage::apply_rate_for_tenor() {
    auto& curves = CurveDataService::instance();
    if (!curves.treasury_now()) {
        QPointer<DerivPricerPage> self(this);
        curves.treasury_curve(false, this, [self](const CurveDataService::TreasuryReply& r) {
            if (self && r.curve)
                self->apply_rate_for_tenor();
        });
        return;
    }
    QString label;
    const double r = curves.rate_for(opt_time_->value(), opt_rate_->value() / 100.0, &label);
    opt_rate_->setValue(r * 100.0);
    opt_rate_->setToolTip(label);
}

void DerivPricerPage::apply_chain_iv(const DerivContext::ChainPtr& chain) {
    if (!chain || chain->expiries.isEmpty())
        return;
    namespace mk = services::options::market;
    const double T = opt_time_->value();
    int best = 0;
    for (int e = 1; e < chain->expiries.size(); ++e)
        if (std::fabs(chain->expiries[e].years - T) < std::fabs(chain->expiries[best].years - T))
            best = e;
    const mk::FreeExpiry& x = chain->expiries[best];
    if (!(x.forward > 0))
        return;
    const double k = std::log(std::max(opt_strike_->value(), 1e-9) / x.forward);
    double iv = 0;
    if (x.svi.ok && k >= x.k_lo - 0.05 && k <= x.k_hi + 0.05)
        iv = services::options::vol::svi_vol(x.svi.params, k, x.years);
    if (!(iv > 0))
        iv = x.smile.atm_iv;
    if (!(iv > 0))
        return;
    opt_vol_->setValue(iv * 100.0);
    set_market_status(market_status_->text() + QLatin1String(" · ") +
                      tr("IV %1 from the %2 smile (%3)").arg(vol_pct(iv), x.date.toString(Qt::ISODate), chain->source_label));
}

void DerivPricerPage::load_contract(const ContractRef& c) {
    on_instrument_changed(1);
    market_symbol_ = c.symbol;
    if (sym_box_)
        sym_box_->set_text(c.symbol);
    opt_payoff_->setCurrentIndex(0);
    opt_exercise_->setCurrentIndex(1); // US listed equity options are American
    opt_model_->setCurrentIndex(1);
    opt_div_mode_->setCurrentIndex(0);
    opt_type_->setCurrentIndex(c.is_call ? 0 : 1);
    opt_spot_->setValue(c.spot);
    opt_strike_->setValue(c.strike);
    opt_time_->setValue(std::max(c.years, 0.001));
    if (c.iv > 0)
        opt_vol_->setValue(c.iv * 100.0);
    opt_rate_->setValue(c.rate * 100.0);
    opt_div_->setValue(c.q * 100.0);
    iv_spot_->setValue(c.spot);
    iv_strike_->setValue(c.strike);
    iv_time_->setValue(std::max(c.years, 0.001));
    iv_rate_->setValue(c.rate * 100.0);
    iv_div_->setValue(c.q * 100.0);
    iv_type_->setCurrentIndex(c.is_call ? 0 : 1);
    if (c.mid > 0)
        iv_price_->setValue(c.mid);
    set_market_status(tr("%1 %2 %3 %4 from the chain: mid %5, IV %6 (carry %7 implied by put-call parity).")
                          .arg(c.symbol, c.expiry_label, num(c.strike, 2), c.is_call ? tr("call") : tr("put"),
                               num(c.mid, 2), vol_pct(c.iv), pct_points(c.q * 100.0)));
    update_equity_rows();
    on_calculate();
}

} // namespace fincept::screens::deriv
