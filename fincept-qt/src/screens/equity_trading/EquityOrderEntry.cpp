// EquityOrderEntry.cpp — BUY/SELL form configurable per broker profile
#include "screens/equity_trading/EquityOrderEntry.h"

#include "core/logging/Logger.h"
#include "screens/equity_trading/BasketOrdersDialog.h"
#include "screens/equity_trading/EquityTypes.h"
#include "services/markets/FxRateService.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "trading/OptionsStrategyBuilder.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/PanelHeader.h"

#include <QCheckBox>
#include <QSignalBlocker>
#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QMenu>
#include <QMetaObject>
#include <QPointer>
#include <QScrollArea>
#include <QShortcut>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <QtConcurrent/QtConcurrent>

#include <cmath>

namespace fincept::screens::equity {

using namespace fincept::ui;
namespace fmt = fincept::ui::formatting;
namespace fx = fincept::services::fx;

namespace {
// A quote older than this is flagged STALE in the ticket. Chosen so a normally
// ticking symbol never flickers, while a dead feed is obvious within seconds.
constexpr qint64 kStaleQuoteMs = 12000;
// Failsafe release for the send lock: if a placement result never comes back
// (broker hang, dropped callback) the ticket must not stay disabled forever.
constexpr int kSendUnlockFailsafeMs = 20000;
} // namespace

EquityOrderEntry::EquityOrderEntry(QWidget* parent) : QWidget(parent) {
    setObjectName("eqOrderEntry");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── Header ─────────────────────────────────────────────────────────────────
    // The shared panel header + the PAPER / LIVE chip, and a 2 px rule under it in
    // the state tone (SUCCESS paper, DANGER live): paper vs live is unmistakable
    // in words, shape (ring / dot) and tone.
    auto* header = new fincept::ui::PanelHeader(tr("Order ticket"), this);
    title_label_ = header->title_label();
    mode_label_ = new QLabel(this);
    mode_label_->setObjectName("eqOeMode");
    header->add_control(mode_label_);
    layout->addWidget(header);
    mode_rule_ = new QFrame(this);
    mode_rule_->setObjectName("eqOeModeRule");
    mode_rule_->setFixedHeight(2);
    layout->addWidget(mode_rule_);
    apply_mode_tone();

    // ── Scrollable content — a tall ticket never clips in the narrow column ──────
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName("eqOeScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(scroll, 1);

    auto* content = new QWidget;
    content->setObjectName("eqOeContent");
    scroll->setWidget(content);
    auto* form = new QVBoxLayout(content);
    form->setContentsMargins(10, 10, 10, 10);
    form->setSpacing(8);

    // Thin section separator — groups the ticket into scannable blocks.
    auto make_sep = [content]() {
        auto* line = new QFrame(content);
        line->setObjectName("eqOeSep");
        line->setFrameShape(QFrame::HLine);
        line->setFixedHeight(1);
        return line;
    };

    // ── Broker banner (hidden until set_broker_notice) ──────────────────────────
    {
        auto* box = new QFrame(content);
        box->setObjectName("eqOeNotice");
        auto* nl = new QVBoxLayout(box);
        nl->setContentsMargins(10, 8, 10, 8);
        nl->setSpacing(5);
        notice_title_ = new QLabel(box);
        notice_title_->setWordWrap(true);
        nl->addWidget(notice_title_);
        notice_detail_ = new QLabel(box);
        notice_detail_->setWordWrap(true);
        notice_detail_->setStyleSheet(
            QString("background:transparent;border:none;color:%1;font-size:" + fincept::ui::fonts::px(-3) + "px;").arg(colors::TEXT_SECONDARY()));
        nl->addWidget(notice_detail_);
        auto* open_btn = new QPushButton(tr("OPEN ACCOUNTS"), box);
        open_btn->setObjectName("eqApiBtn"); // same chrome as the command-bar ACCOUNTS button
        open_btn->setMinimumHeight(24);
        open_btn->setCursor(Qt::PointingHandCursor);
        open_btn->setAccessibleName(tr("Open broker accounts"));
        connect(open_btn, &QPushButton::clicked, this, &EquityOrderEntry::accounts_requested);
        nl->addWidget(open_btn, 0, Qt::AlignLeft);
        notice_box_ = box;
        notice_box_->hide();
        form->addWidget(notice_box_);
    }

    // BUY / SELL tabs
    auto* side_row = new QHBoxLayout;
    side_row->setSpacing(0);

    buy_tab_ = new QPushButton(tr("BUY"));
    buy_tab_->setObjectName("eqBuyTab");
    buy_tab_->setProperty("active", true);
    buy_tab_->setCursor(Qt::PointingHandCursor);
    buy_tab_->setMinimumHeight(36);
    connect(buy_tab_, &QPushButton::clicked, this, [this]() { set_buy_side(true); });
    side_row->addWidget(buy_tab_, 1);

    sell_tab_ = new QPushButton(tr("SELL"));
    sell_tab_->setObjectName("eqSellTab");
    sell_tab_->setProperty("active", false);
    sell_tab_->setCursor(Qt::PointingHandCursor);
    sell_tab_->setMinimumHeight(36);
    connect(sell_tab_, &QPushButton::clicked, this, [this]() { set_buy_side(false); });
    side_row->addWidget(sell_tab_, 1);
    form->addLayout(side_row);

    // Order type toggle buttons
    auto* type_row = new QHBoxLayout;
    type_row->setSpacing(4);
    const QString type_labels[] = {tr("MKT"), tr("LMT"), tr("SL"), tr("SL-L")};
    for (int i = 0; i < 4; ++i) {
        type_btns_[i] = new QPushButton(type_labels[i]);
        type_btns_[i]->setObjectName("eqOeTypeBtn");
        type_btns_[i]->setMinimumHeight(26);
        type_btns_[i]->setCursor(Qt::PointingHandCursor);
        if (i == 0)
            type_btns_[i]->setProperty("active", true);
        connect(type_btns_[i], &QPushButton::clicked, this, [this, i]() { set_order_type(i); });
        type_row->addWidget(type_btns_[i]);
    }
    form->addLayout(type_row);

    // ── Symbol context ──────────────────────────────────────────────────────────
    symbol_label_ = new QLabel(QStringLiteral("%1 · %2").arg(current_symbol_, current_exchange_));
    symbol_label_->setObjectName("eqOeSymbol");
    symbol_label_->setAlignment(Qt::AlignCenter);
    form->addWidget(symbol_label_);

    // Venue session line (set_session_line) — e.g. "NSE CLOSED · MARKET orders queue".
    session_label_ = new QLabel(content);
    session_label_->setObjectName("eqOeSession");
    session_label_->setAlignment(Qt::AlignCenter);
    session_label_->setWordWrap(true);
    session_label_->hide();
    form->addWidget(session_label_);

    // ── Live info: LTP (left) · Balance (right) ─────────────────────────────────
    auto* info_box = new QWidget(content);
    info_box->setObjectName("eqOeInfo");
    auto* info_row = new QHBoxLayout(info_box);
    info_row->setContentsMargins(8, 5, 8, 5);
    info_row->setSpacing(6);

    market_price_label_ = new QLabel(tr("LTP --"));
    market_price_label_->setObjectName("eqOeMarketPrice");
    info_row->addWidget(market_price_label_);
    info_row->addStretch();

    balance_label_ = new QLabel(tr("Bal --"));
    balance_label_->setObjectName("eqOeBalance");
    balance_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    info_row->addWidget(balance_label_);
    form->addWidget(info_box);

    form->addWidget(make_sep());

    // ── Order parameters — aligned label + field rows ───────────────────────────
    auto* params = new QGridLayout;
    params->setHorizontalSpacing(8);
    params->setVerticalSpacing(6);
    params->setColumnStretch(0, 0);
    params->setColumnStretch(1, 1);
    int prow = 0;

    product_title_ = new QLabel(tr("PRODUCT"));
    product_title_->setObjectName("eqOeLabel");
    product_combo_ = new QComboBox;
    product_combo_->setObjectName("eqOeCombo");
    product_combo_->addItems({tr("Intraday (MIS)"), tr("Delivery (CNC)"), tr("Margin (NRML)")});
    product_combo_->setMinimumHeight(28);
    // MIS / CNC / NRML change the margin the broker quotes — and, on brokers that carry
    // SL / TP only on bracket products, whether the SL / TP boxes may be used at all.
    connect(product_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        if (margin_timer_)
            margin_timer_->start();
        apply_sl_tp_availability();
    });
    params->addWidget(product_title_, prow, 0);
    params->addWidget(product_combo_, prow, 1);
    ++prow;

    exchange_title_ = new QLabel(tr("EXCHANGE"));
    exchange_title_->setObjectName("eqOeLabel");
    exchange_combo_ = new QComboBox;
    exchange_combo_->setObjectName("eqOeCombo");
    // Exchange codes (NSE/BSE/…) are market identifiers — data, not translated.
    exchange_combo_->addItems({"NSE", "BSE", "NYSE", "NASDAQ"});
    exchange_combo_->setMinimumHeight(28);
    connect(exchange_combo_, &QComboBox::currentTextChanged, this, [this](const QString& ex) {
        current_exchange_ = ex;
        if (symbol_label_)
            symbol_label_->setText(QStringLiteral("%1 · %2").arg(current_symbol_, ex));
        sync_price_currency(); // NSE INR, NYSE USD, LSE GBp ...
        if (margin_timer_)
            margin_timer_->start(); // margin is per exchange
    });
    params->addWidget(exchange_title_, prow, 0);
    params->addWidget(exchange_combo_, prow, 1);
    ++prow;

    qty_title_ = new QLabel(tr("QTY"));
    qty_title_->setObjectName("eqOeLabel");
    qty_edit_ = new QLineEdit;
    qty_edit_->setObjectName("eqOeInput");
    qty_edit_->setPlaceholderText("0");
    qty_edit_->setMinimumHeight(28);
    qty_edit_->setAlignment(Qt::AlignRight);
    connect(qty_edit_, &QLineEdit::textChanged, this, [this]() { update_cost_preview(); });
    // Enter submits like the button (same validation, send lock and the screen's
    // confirmation rules); a disabled button (blocked / locked) ignores it.
    qty_edit_->setProperty("fincept_agent_risk_keys", "Return=order;Enter=order"); // agents: Enter sends the order
    connect(qty_edit_, &QLineEdit::returnPressed, this, [this]() {
        if (submit_btn_ && submit_btn_->isEnabled() && submit_btn_->isVisible())
            submit_btn_->click();
    });
    params->addWidget(qty_title_, prow, 0);
    params->addWidget(qty_edit_, prow, 1);
    ++prow;

    price_title_ = new QLabel(tr("PRICE"));
    price_title_->setObjectName("eqOeLabel");
    price_edit_ = new QLineEdit;
    price_edit_->setObjectName("eqOeInput");
    price_edit_->setPlaceholderText(tr("Limit price"));
    price_edit_->setEnabled(false);
    price_edit_->setMinimumHeight(28);
    price_edit_->setAlignment(Qt::AlignRight);
    connect(price_edit_, &QLineEdit::textChanged, this, [this]() { update_cost_preview(); });
    price_edit_->setProperty("fincept_agent_risk_keys", "Return=order;Enter=order"); // agents: Enter sends the order
    connect(price_edit_, &QLineEdit::returnPressed, this, [this]() {
        if (submit_btn_ && submit_btn_->isEnabled() && submit_btn_->isVisible())
            submit_btn_->click();
    });
    params->addWidget(price_title_, prow, 0);
    params->addWidget(price_edit_, prow, 1);
    ++prow;

    trigger_title_ = new QLabel(tr("TRIGGER"));
    trigger_title_->setObjectName("eqOeLabel");
    stop_price_edit_ = new QLineEdit;
    stop_price_edit_->setObjectName("eqOeInput");
    stop_price_edit_->setPlaceholderText(tr("Trigger price"));
    stop_price_edit_->setEnabled(false);
    stop_price_edit_->setMinimumHeight(28);
    stop_price_edit_->setAlignment(Qt::AlignRight);
    params->addWidget(trigger_title_, prow, 0);
    params->addWidget(stop_price_edit_, prow, 1);
    ++prow;

    form->addLayout(params);

    // Brokerage / fee note
    brokerage_label_ = new QLabel("");
    brokerage_label_->setObjectName("eqOeBrokerage");
    brokerage_label_->setWordWrap(true);
    form->addWidget(brokerage_label_);

    // Advanced toggle
    form->addWidget(make_sep());
    advanced_toggle_ = new QPushButton(tr("+ ADVANCED"));
    advanced_toggle_->setObjectName("eqAdvToggle");
    advanced_toggle_->setCursor(Qt::PointingHandCursor);
    advanced_toggle_->setMinimumHeight(22);
    form->addWidget(advanced_toggle_);

    // SL/TP section
    advanced_section_ = new QWidget(this);
    advanced_section_->setVisible(false);
    auto* adv_layout = new QVBoxLayout(advanced_section_);
    adv_layout->setContentsMargins(0, 0, 0, 0);
    adv_layout->setSpacing(4);

    sl_title_ = new QLabel(tr("SL"));
    sl_title_->setObjectName("eqOeLabel");
    sl_edit_ = new QLineEdit;
    sl_edit_->setObjectName("eqOeInput");
    sl_edit_->setPlaceholderText(tr("Stop Loss"));
    sl_edit_->setMinimumHeight(26);

    tp_title_ = new QLabel(tr("TP"));
    tp_title_->setObjectName("eqOeLabel");
    tp_edit_ = new QLineEdit;
    tp_edit_->setObjectName("eqOeInput");
    tp_edit_->setPlaceholderText(tr("Take Profit"));
    tp_edit_->setMinimumHeight(26);

    adv_layout->addWidget(sl_title_);
    adv_layout->addWidget(sl_edit_);
    adv_layout->addWidget(tp_title_);
    adv_layout->addWidget(tp_edit_);
    form->addWidget(advanced_section_);

    connect(advanced_toggle_, &QPushButton::clicked, this, [this]() {
        const bool show = !advanced_section_->isVisible();
        advanced_section_->setVisible(show);
        advanced_toggle_->setText(show ? tr("- ADVANCED") : tr("+ ADVANCED"));
    });

    // ---- Options Strategy section (collapsible, mirrors advanced_section_) ----
    strategy_toggle_ = new QPushButton(tr("+ OPTIONS STRATEGY"));
    strategy_toggle_->setObjectName("eqAdvToggle");
    strategy_toggle_->setCursor(Qt::PointingHandCursor);
    strategy_toggle_->setMinimumHeight(22);
    form->addWidget(strategy_toggle_);

    strategy_section_ = new QWidget(this);
    strategy_section_->setVisible(false);
    auto* strat_layout = new QVBoxLayout(strategy_section_);
    strat_layout->setContentsMargins(0, 0, 0, 0);
    strat_layout->setSpacing(4);

    strat_title_ = new QLabel(tr("STRATEGY"));
    strat_title_->setObjectName("eqOeLabel");
    strat_layout->addWidget(strat_title_);

    strategy_combo_ = new QComboBox;
    strategy_combo_->setObjectName("eqOeCombo");
    strategy_combo_->addItems({tr("None"), tr("Long Straddle"), tr("Short Straddle"), tr("Long Strangle"),
                               tr("Iron Condor"), tr("Bull Call Spread"), tr("Bear Put Spread")});
    strategy_combo_->setMinimumHeight(26);
    strat_layout->addWidget(strategy_combo_);

    strat_strike_title_ = new QLabel(tr("ATM STRIKE"));
    strat_strike_title_->setObjectName("eqOeLabel");
    strat_layout->addWidget(strat_strike_title_);
    strat_strike_edit_ = new QLineEdit;
    strat_strike_edit_->setObjectName("eqOeInput");
    strat_strike_edit_->setPlaceholderText(tr("e.g. 22500"));
    strat_strike_edit_->setMinimumHeight(26);
    strat_layout->addWidget(strat_strike_edit_);

    strat_expiry_title_ = new QLabel(tr("EXPIRY (YYYY-MM-DD)"));
    strat_expiry_title_->setObjectName("eqOeLabel");
    strat_layout->addWidget(strat_expiry_title_);
    strat_expiry_edit_ = new QLineEdit;
    strat_expiry_edit_->setObjectName("eqOeInput");
    strat_expiry_edit_->setPlaceholderText(tr("YYYY-MM-DD"));
    strat_expiry_edit_->setMinimumHeight(26);
    strat_layout->addWidget(strat_expiry_edit_);

    // Number of LOTS per leg. The order quantity is lots × the contract's own lot size,
    // resolved from the instrument master (OptionsStrategyBuilder::to_basket_order).
    strat_lot_title_ = new QLabel(tr("LOTS"));
    strat_lot_title_->setObjectName("eqOeLabel");
    strat_layout->addWidget(strat_lot_title_);
    strat_lot_edit_ = new QLineEdit;
    strat_lot_edit_->setObjectName("eqOeInput");
    strat_lot_edit_->setText("1");
    strat_lot_edit_->setPlaceholderText("1");
    strat_lot_edit_->setToolTip(tr("Whole number of lots per leg — the quantity sent is lots × the contract lot size"));
    strat_lot_edit_->setMinimumHeight(26);
    strat_layout->addWidget(strat_lot_edit_);

    strat_width_title_ = new QLabel(tr("WIDTH (strangle / condor / spreads)"));
    strat_width_title_->setObjectName("eqOeLabel");
    strat_layout->addWidget(strat_width_title_);
    strat_width_edit_ = new QLineEdit;
    strat_width_edit_->setObjectName("eqOeInput");
    strat_width_edit_->setPlaceholderText(tr("e.g. 200"));
    strat_width_edit_->setMinimumHeight(26);
    strat_layout->addWidget(strat_width_edit_);

    strat_legs_title_ = new QLabel(tr("LEGS"));
    strat_legs_title_->setObjectName("eqOeLabel");
    strat_layout->addWidget(strat_legs_title_);

    strategy_preview_ = new QLabel(tr("Select a strategy"));
    strategy_preview_->setObjectName("eqOeBrokerage");
    strategy_preview_->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-3) + "px;").arg(colors::TEXT_SECONDARY()));
    strategy_preview_->setWordWrap(true);
    strategy_preview_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    strat_layout->addWidget(strategy_preview_);

    place_strategy_btn_ = new QPushButton(tr("PLACE STRATEGY"));
    place_strategy_btn_->setObjectName("eqBuySubmit");
    place_strategy_btn_->setProperty("fincept_agent_risk", "order");
    place_strategy_btn_->setMinimumHeight(30);
    place_strategy_btn_->setCursor(Qt::PointingHandCursor);
    connect(place_strategy_btn_, &QPushButton::clicked, this, &EquityOrderEntry::on_place_strategy);
    strat_layout->addWidget(place_strategy_btn_);

    form->addWidget(strategy_section_);

    connect(strategy_toggle_, &QPushButton::clicked, this, [this]() {
        const bool show = !strategy_section_->isVisible();
        strategy_section_->setVisible(show);
        strategy_toggle_->setText(show ? tr("- OPTIONS STRATEGY") : tr("+ OPTIONS STRATEGY"));
    });

    // Recompute preview whenever the strategy or any input changes
    connect(strategy_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this]() { refresh_strategy_preview(); });
    connect(strat_strike_edit_, &QLineEdit::textChanged, this, [this]() { refresh_strategy_preview(); });
    connect(strat_expiry_edit_, &QLineEdit::textChanged, this, [this]() { refresh_strategy_preview(); });
    connect(strat_lot_edit_, &QLineEdit::textChanged, this, [this]() { refresh_strategy_preview(); });
    connect(strat_width_edit_, &QLineEdit::textChanged, this, [this]() { refresh_strategy_preview(); });

    // ── Cost / margin summary ───────────────────────────────────────────────────
    form->addWidget(make_sep());

    auto* summary = new QWidget(content);
    summary->setObjectName("eqOeSummary");
    auto* sum_layout = new QVBoxLayout(summary);
    sum_layout->setContentsMargins(8, 6, 8, 6);
    sum_layout->setSpacing(3);

    cost_label_ = new QLabel(tr("Est: --"));
    cost_label_->setObjectName("eqOeCost");
    cost_label_->setWordWrap(true); // a second line carries the value in the account currency
    sum_layout->addWidget(cost_label_);

    // Required margin (live broker only)
    margin_label_ = new QLabel("");
    margin_label_->setObjectName("eqOeMargin");
    margin_label_->hide();
    sum_layout->addWidget(margin_label_);

    form->addWidget(summary);

    // Multi-broker selector: BUY/SELL fires the same order to every checked
    // account (empty selection = normal focused-account flow). QWidgetAction-
    // wrapped checkboxes keep the menu open across toggles.
    brokers_btn_ = new QToolButton;
    brokers_btn_->setObjectName("eqBrokersBtn");
    brokers_btn_->setPopupMode(QToolButton::InstantPopup);
    brokers_btn_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    brokers_btn_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    brokers_btn_->setMinimumHeight(24);
    brokers_btn_->setCursor(Qt::PointingHandCursor);
    brokers_menu_ = new QMenu(brokers_btn_);
    brokers_btn_->setMenu(brokers_menu_);
    connect(brokers_menu_, &QMenu::aboutToShow, this, &EquityOrderEntry::rebuild_brokers_menu);
    update_brokers_btn();

    // BASKETS — saved multi-leg order groups, executable on any account(s).
    auto* baskets_btn = new QToolButton;
    baskets_btn->setObjectName("eqBasketsBtn");
    baskets_btn->setText(tr("BASKETS"));
    baskets_btn->setMinimumHeight(24);
    baskets_btn->setCursor(Qt::PointingHandCursor);
    baskets_btn->setStyleSheet(
        QString("QToolButton{background:%1;border:1px solid %2;color:%3;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;"
                "padding:2px 10px;}QToolButton:hover{border-color:%4;color:%4;}")
            .arg(colors::BG_RAISED(), colors::BORDER_MED(), colors::TEXT_SECONDARY(), colors::AMBER()));
    connect(baskets_btn, &QToolButton::clicked, this, [this]() {
        BasketOrdersDialog dlg(this);
        dlg.exec();
        // Baskets may have been executed on any account — let the screen repaint its
        // blotter instead of leaving it stale until the next 5-minute poll.
        emit baskets_closed();
    });

    brokers_row_ = new QWidget(content);
    auto* brokers_row = new QHBoxLayout(brokers_row_);
    brokers_row->setContentsMargins(0, 0, 0, 0);
    brokers_row->setSpacing(4);
    brokers_row->addWidget(brokers_btn_, 1);
    brokers_row->addWidget(baskets_btn);
    form->addWidget(brokers_row_);

    // Built-in paper account: what a fill means, right above the button that makes one.
    sim_note_ = new QLabel(content);
    sim_note_->setObjectName("eqOeSimNote");
    sim_note_->setWordWrap(true);
    sim_note_->hide();
    form->addWidget(sim_note_);

    // Submit row: main submit + broadcast
    auto* submit_row = new QHBoxLayout;
    submit_row->setSpacing(4);

    submit_btn_ = new QPushButton(tr("BUY %1").arg(current_symbol_));
    submit_btn_->setObjectName("eqBuySubmit");
    submit_btn_->setProperty("fincept_agent_risk", "order");
    submit_btn_->setMinimumHeight(40);
    submit_btn_->setCursor(Qt::PointingHandCursor);
    connect(submit_btn_, &QPushButton::clicked, this, &EquityOrderEntry::on_submit);
    submit_row->addWidget(submit_btn_, 3);

    broadcast_btn_ = new QPushButton(tr("ALL"));
    broadcast_btn_->setObjectName("eqBroadcastBtn");
    broadcast_btn_->setMinimumHeight(40);
    broadcast_btn_->setCursor(Qt::PointingHandCursor);
    broadcast_btn_->setToolTip(tr("Broadcast this order to multiple accounts"));
    broadcast_btn_->setStyleSheet(QString("QPushButton { background: %1; color: %2; font-weight: 700; font-size: " + fincept::ui::fonts::px(-2) + "px; "
                                          "  border: 1px solid %3; border-radius: 4px; }"
                                          "QPushButton:hover { background: %4; color: %2; }")
                                      .arg(colors::BG_RAISED(), colors::AMBER(), colors::BORDER_MED(), colors::BG_HOVER()));
    connect(broadcast_btn_, &QPushButton::clicked, this, [this]() {
        // The broadcast path fires the SAME order at N accounts, so it must run
        // the SAME validation as the focused submit. It previously checked only
        // the quantity, so a blank limit price sent price=0 as a LIMIT order to
        // every selected broker (and the dialog even labelled it "MKT").
        if (broker_blocked_)
            return; // button is disabled; guards a stale click / accelerator
        double qty = 0.0;
        if (!validate_entry(qty))
            return;
        Q_UNUSED(qty);
        emit broadcast_requested(build_order());
    });
    submit_row->addWidget(broadcast_btn_, 1);
    form->addLayout(submit_row);

    // Status
    status_label_ = new QLabel("");
    status_label_->setObjectName("eqOeStatus");
    status_label_->setWordWrap(true);
    form->addWidget(status_label_);

    form->addStretch();

    // Debounce timer: fetch margin 600ms after last qty/price change (live broker only)
    margin_timer_ = new QTimer(this);
    margin_timer_->setSingleShot(true);
    margin_timer_->setInterval(600);
    connect(margin_timer_, &QTimer::timeout, this, &EquityOrderEntry::fetch_margin_async);
    connect(qty_edit_, &QLineEdit::textChanged, this, [this]() { margin_timer_->start(); });
    connect(price_edit_, &QLineEdit::textChanged, this, [this]() { margin_timer_->start(); });

    // Send-lock failsafe (see set_send_locked). Interval only — never started
    // from the constructor (P3); armed on submit, disarmed on the result.
    submit_failsafe_ = new QTimer(this);
    submit_failsafe_->setSingleShot(true);
    submit_failsafe_->setInterval(kSendUnlockFailsafeMs);
    connect(submit_failsafe_, &QTimer::timeout, this, [this]() {
        if (submit_locked_) {
            set_send_locked(false);
            status_label_->setText(tr("No confirmation from the broker — check the order book before retrying"));
            status_label_->setStyleSheet(QString("color: %1;").arg(colors::WARNING()));
        }
    });

    // Stale-quote watchdog. Interval only here; started in showEvent / stopped in
    // hideEvent so a hidden ticket costs nothing (P3).
    stale_timer_ = new QTimer(this);
    stale_timer_->setInterval(2000);
    connect(stale_timer_, &QTimer::timeout, this, &EquityOrderEntry::update_stale_state);

    // ── Accessibility + keyboard (order entry is where this matters most) ──────
    setAccessibleName(tr("Order entry ticket"));
    buy_tab_->setAccessibleName(tr("Buy side"));
    sell_tab_->setAccessibleName(tr("Sell side"));
    const QString type_names[] = {tr("Market order"), tr("Limit order"), tr("Stop-loss market order"),
                                  tr("Stop-loss limit order")};
    for (int i = 0; i < 4; ++i)
        type_btns_[i]->setAccessibleName(type_names[i]);
    product_combo_->setAccessibleName(tr("Product type"));
    exchange_combo_->setAccessibleName(tr("Exchange"));
    qty_edit_->setAccessibleName(tr("Quantity"));
    price_edit_->setAccessibleName(tr("Limit price"));
    stop_price_edit_->setAccessibleName(tr("Trigger price"));
    sl_edit_->setAccessibleName(tr("Stop loss price"));
    tp_edit_->setAccessibleName(tr("Take profit price"));
    submit_btn_->setAccessibleName(tr("Submit order"));
    broadcast_btn_->setAccessibleName(tr("Broadcast order to multiple accounts"));
    brokers_btn_->setAccessibleName(tr("Target broker accounts"));
    baskets_btn->setAccessibleName(tr("Saved order baskets"));
    market_price_label_->setAccessibleName(tr("Last traded price"));
    balance_label_->setAccessibleName(tr("Available balance"));
    cost_label_->setAccessibleName(tr("Estimated order value"));
    status_label_->setAccessibleName(tr("Order status"));
    place_strategy_btn_->setAccessibleName(tr("Place options strategy"));

    // Explicit ticket tab order: side → type → qty → price → trigger → product →
    // exchange → submit. Qt's creation order would otherwise walk the collapsed
    // advanced/strategy sections in between.
    setTabOrder(buy_tab_, sell_tab_);
    setTabOrder(sell_tab_, type_btns_[0]);
    setTabOrder(type_btns_[0], type_btns_[1]);
    setTabOrder(type_btns_[1], type_btns_[2]);
    setTabOrder(type_btns_[2], type_btns_[3]);
    setTabOrder(type_btns_[3], qty_edit_);
    setTabOrder(qty_edit_, price_edit_);
    setTabOrder(price_edit_, stop_price_edit_);
    setTabOrder(stop_price_edit_, product_combo_);
    setTabOrder(product_combo_, exchange_combo_);
    setTabOrder(exchange_combo_, submit_btn_);
    setTabOrder(submit_btn_, broadcast_btn_);

    // Trader shortcuts (window-scoped so they work from anywhere on the tab).
    auto add_shortcut = [this](const QKeySequence& keys, auto&& fn) {
        auto* sc = new QShortcut(keys, this);
        sc->setContext(Qt::WindowShortcut);
        connect(sc, &QShortcut::activated, this, fn);
        return sc;
    };
    // Ctrl+Shift+… avoids the platform-reserved Ctrl+Q (quit) / Ctrl+Esc (Start menu).
    add_shortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Q), [this]() { focus_quantity(); });
    add_shortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B), [this]() {
        set_buy_side(true);
        focus_quantity();
    });
    add_shortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S), [this]() {
        set_buy_side(false);
        focus_quantity();
    });
    buy_tab_->setToolTip(tr("Buy side (Ctrl+Shift+B)"));
    sell_tab_->setToolTip(tr("Sell side (Ctrl+Shift+S)"));
    qty_edit_->setToolTip(tr("Quantity (Ctrl+Shift+Q to focus, Esc to clear the ticket)"));

    // Escape clears the ticket — the trader's "abort what I typed" reflex. Scoped
    // to this widget so it can't steal Escape from the rest of the window.
    {
        auto* clear_sc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
        clear_sc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(clear_sc, &QShortcut::activated, this, [this]() {
            qty_edit_->clear();
            price_edit_->clear();
            stop_price_edit_->clear();
            if (sl_edit_)
                sl_edit_->clear();
            if (tp_edit_)
                tp_edit_->clear();
            status_label_->setText(tr("Ticket cleared"));
            status_label_->setStyleSheet(QString("color: %1;").arg(colors::TEXT_SECONDARY()));
            update_cost_preview();
        });
    }

    // Currency / number-format change: re-format from the values already held (no fetch).
    fmt::on_format_changed(this, [this]() {
        render_price_label();
        render_balance_label();
        render_margin_label();
        update_cost_preview();
        refresh_strategy_preview();
    });
    // The Est line's account-currency figure waits for / follows the FX rate.
    connect(&fincept::services::FxRateService::instance(), &fincept::services::FxRateService::rates_changed, this,
            [this]() {
        if (est_converts_)
            update_cost_preview();
    });

    render_price_label();
}

void EquityOrderEntry::configure_for_broker(const trading::BrokerProfile& profile) {
    set_builtin_mode(false); // a broker ticket: exchange picker, broker rows, live price rules
    price_digits_ = -1;
    lot_size_ = 1.0;
    lot_known_ = false;
    symbol_display_.clear();
    // Cache product types for order construction
    product_types_ = profile.product_types;
    // The account (balance, margin) is in the broker's currency; prices in the
    // selected exchange's (sync_price_currency(), once the combo is rebuilt).
    balance_currency_ = profile.currency;
    current_currency_ = profile.currency;
    instrument_ccy_known_ = false;

    // Rebuild product combo
    product_combo_->clear();
    for (const auto& pt : profile.product_types)
        product_combo_->addItem(pt.label);

    // Rebuild exchange combo
    exchange_combo_->clear();
    for (const auto& ex : profile.exchanges)
        exchange_combo_->addItem(ex);

    // Set default exchange
    if (!profile.default_exchange.isEmpty()) {
        int idx = exchange_combo_->findText(profile.default_exchange);
        if (idx >= 0)
            exchange_combo_->setCurrentIndex(idx);
        current_exchange_ = profile.default_exchange;
    }
    sync_price_currency();

    // Update brokerage label
    brokerage_label_->setText(profile.brokerage_info.isEmpty() ? "" : tr("Fee: %1").arg(profile.brokerage_info));

    // Reset form state
    qty_edit_->clear();
    price_edit_->clear();
    stop_price_edit_->clear();
    if (sl_edit_)
        sl_edit_->clear();
    if (tp_edit_)
        tp_edit_->clear();
    status_label_->clear();
    current_price_ = 0;
    last_price_ms_ = 0;
    price_is_stale_ = false;
    balance_ = 0;
    balance_set_ = false;
    render_balance_label(); // the previous account's balance, in its currency, must not stay up
    set_send_locked(false); // a broker switch must never leave the ticket wedged
    sl_tp_profile_ = profile;
    apply_sl_tp_availability();
    render_price_label();
    update_cost_preview();
}

void EquityOrderEntry::apply_sl_tp_availability() {
    // Local paper honours SL / TP (OrderMatcher's bracket registry); a broker-routed order
    // only where the adapter transmits them for the selected product. Anywhere else they
    // used to be dropped silently after the confirm dialog had listed them — disable AND
    // clear the box so a value can never ride along. (UnifiedTrading also refuses such an
    // order at the broker boundary.)
    const trading::ProductType product = selected_product_type();
    const bool sl_ok = is_paper_ || trading::entry_sl_tp_honoured(sl_tp_profile_, product, true, false);
    const bool tp_ok = is_paper_ || trading::entry_sl_tp_honoured(sl_tp_profile_, product, false, true);
    const QString tip = tr("This broker does not accept SL/TP on the entry order for this product — place a "
                           "separate SL order");
    auto apply = [&tip](QLineEdit* edit, bool enabled) {
        if (!edit)
            return;
        if (!enabled)
            edit->clear();
        edit->setEnabled(enabled);
        edit->setToolTip(enabled ? QString() : tip);
    };
    apply(sl_edit_, sl_ok);
    apply(tp_edit_, tp_ok);
}

void EquityOrderEntry::set_buy_side(bool is_buy) {
    is_buy_side_ = is_buy;
    buy_tab_->setProperty("active", is_buy);
    sell_tab_->setProperty("active", !is_buy);
    buy_tab_->style()->unpolish(buy_tab_);
    buy_tab_->style()->polish(buy_tab_);
    sell_tab_->style()->unpolish(sell_tab_);
    sell_tab_->style()->polish(sell_tab_);

    const QString shown =
        symbol_display_.isEmpty() ? current_symbol_ : symbol_display_.section(QStringLiteral(" · "), 0, 0);
    submit_btn_->setText(is_buy ? tr("BUY %1").arg(shown) : tr("SELL %1").arg(shown));
    submit_btn_->setAccessibleName(is_buy ? tr("Buy %1").arg(shown) : tr("Sell %1").arg(shown));
    submit_btn_->setObjectName(is_buy ? "eqBuySubmit" : "eqSellSubmit");
    submit_btn_->style()->unpolish(submit_btn_);
    submit_btn_->style()->polish(submit_btn_);
    // Required margin differs by side (and by order type / product / exchange, below).
    if (margin_timer_)
        margin_timer_->start();
    update_cost_preview(); // "buying power after" applies to a BUY only
}

void EquityOrderEntry::set_order_type(int idx) {
    active_type_ = idx;
    for (int i = 0; i < 4; ++i) {
        type_btns_[i]->setProperty("active", i == idx);
        type_btns_[i]->style()->unpolish(type_btns_[i]);
        type_btns_[i]->style()->polish(type_btns_[i]);
    }
    price_edit_->setEnabled(idx == 1 || idx == 3);      // Limit, SL-Limit
    stop_price_edit_->setEnabled(idx == 2 || idx == 3); // SL, SL-Limit
    update_cost_preview();
    if (margin_timer_)
        margin_timer_->start();
}

void EquityOrderEntry::set_balance(double balance, const QString& currency) {
    balance_ = balance;
    balance_set_ = true;
    if (!currency.isEmpty())
        balance_currency_ = currency;
    render_balance_label();
    update_cost_preview();
}

void EquityOrderEntry::render_balance_label() {
    if (!balance_label_)
        return;
    if (!balance_set_) {
        balance_label_->setText(tr("Bal --"));
        return;
    }
    // Empty currency (unknown) prints the amount without a unit rather than a guessed one.
    const QString amount = fmt::money(balance_, balance_currency_);
    balance_label_->setText(builtin_mode_ ? tr("Cash %1").arg(amount) : tr("Bal %1").arg(amount));
}

void EquityOrderEntry::render_margin_label() {
    if (!margin_label_ || margin_label_->isHidden())
        return;
    // The broker quotes margin in the account's currency (unknown: no unit, never a guess).
    margin_label_->setText(tr("Margin: %1").arg(fmt::money(margin_total_, balance_currency_)));
}

namespace {
fmt::InstrumentFmt ticket_price_fmt(const QString& symbol, const QString& ccy, int digits) {
    fmt::InstrumentFmt inst = fmt::instrument_for_symbol(symbol, ccy);
    if (digits >= 0)
        inst.decimals = digits;
    return inst;
}
} // namespace

QString EquityOrderEntry::fmt_px(double v) const {
    return fmt::price(v, ticket_price_fmt(current_symbol_, current_currency_, price_digits_));
}

QString EquityOrderEntry::fmt_px_sym(double v) const {
    return fmt::price_with_symbol(v, ticket_price_fmt(current_symbol_, current_currency_, price_digits_));
}

QString EquityOrderEntry::fmt_value(double v) const {
    return fmt::money(fmt::to_major_units(v, current_currency_), fmt::major_currency(current_currency_));
}

void EquityOrderEntry::sync_price_currency() {
    if (instrument_ccy_known_)
        return; // the instrument named its own currency (an LSE line may quote in USD)
    // Broker: an unlisted segment (MCX, CDS) is quoted in the account currency. Built-in
    // paper: no guess — set_instrument_meta() follows with the quote's currency.
    const QString ccy = exchange_currency(current_exchange_, builtin_mode_ ? QString() : balance_currency_);
    if (ccy.isEmpty() || ccy == current_currency_)
        return;
    current_currency_ = ccy;
    render_price_label();
    update_cost_preview();
}

void EquityOrderEntry::set_builtin_mode(bool on, const QString& account_currency) {
    builtin_mode_ = on;
    if (!account_currency.isEmpty())
        balance_currency_ = account_currency;
    if (on) {
        // Products of the simulator: intraday squares off at the venue's close,
        // delivery carries overnight. No broker exchange list: the instrument's venue.
        product_types_ = {{tr("Intraday — squared off at the close"), trading::ProductType::Intraday},
                          {tr("Delivery — carried overnight"), trading::ProductType::Delivery}};
        {
            QSignalBlocker block(product_combo_);
            product_combo_->clear();
            for (const auto& pt : product_types_)
                product_combo_->addItem(pt.label);
            product_combo_->setCurrentIndex(1); // delivery: the safe default for a cash account
        }
        brokerage_label_->setText(tr("Simulated: 0.05% fee per fill, no slippage model."));
        broker_id_.clear(); // never a broker margin call
        account_id_.clear();
        margin_label_->hide();
        sim_note_->setText(tr("Paper account \xE2\x80\x94 fills are SIMULATED at delayed public prices "
                              "(typically 15 min). No order reaches a broker or an exchange."));
        balance_ = 0;
        balance_set_ = false;
        render_balance_label();
        current_price_ = 0;
        last_price_ms_ = 0;
        price_is_stale_ = false;
        set_send_locked(false);
        sl_tp_profile_ = trading::BrokerProfile{};
        apply_sl_tp_availability();
    }
    exchange_title_->setVisible(!on);
    exchange_combo_->setVisible(!on);
    brokers_row_->setVisible(!on);
    broadcast_btn_->setVisible(!on);
    strategy_toggle_->setVisible(!on);
    if (on)
        strategy_section_->setVisible(false);
    sim_note_->setVisible(on);
    if (!on)
        set_price_source(false);
    render_price_label();
    update_cost_preview();
}

void EquityOrderEntry::set_price_source(bool delayed, int delay_min, qint64 printed_ms, const QString& tz_label,
                                        qint64 max_age_ms) {
    delayed_price_ = delayed;
    delay_min_ = delay_min;
    printed_ms_ = printed_ms;
    printed_tz_ = tz_label;
    delayed_max_age_ms_ = max_age_ms;
    update_stale_state();
    render_price_label();
}

void EquityOrderEntry::set_instrument_meta(const QString& currency, int digits, double lot_size, bool lot_known) {
    if (!currency.isEmpty()) {
        current_currency_ = currency;
        instrument_ccy_known_ = true;
    }
    price_digits_ = digits;
    lot_size_ = lot_size > 0.0 ? lot_size : 1.0;
    lot_known_ = lot_known;
    qty_edit_->setPlaceholderText(lot_known_ && lot_size_ > 1.0 ? tr("Lot of %1").arg(format_quantity(lot_size_))
                                                                : QStringLiteral("0"));
    render_price_label();
    update_cost_preview();
}

void EquityOrderEntry::set_symbol_display(const QString& text) {
    symbol_display_ = text;
    if (symbol_label_)
        symbol_label_->setText(text.isEmpty() ? QStringLiteral("%1 · %2").arg(current_symbol_, current_exchange_) : text);
    const QString shown = text.isEmpty() ? current_symbol_ : text.section(QStringLiteral(" · "), 0, 0);
    submit_btn_->setText(is_buy_side_ ? tr("BUY %1").arg(shown) : tr("SELL %1").arg(shown));
}

void EquityOrderEntry::set_session_line(const QString& text, bool open) {
    if (!session_label_)
        return;
    session_label_->setText(text);
    session_label_->setVisible(!text.isEmpty());
    const QString sheet = QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:600;")
                              .arg(open ? colors::TEXT_SECONDARY() : colors::WARNING());
    if (session_label_->styleSheet() != sheet)
        session_label_->setStyleSheet(sheet);
}

void EquityOrderEntry::set_trade_block(const QString& reason) {
    if (reason == trade_block_)
        return;
    trade_block_ = reason;
    set_send_locked(submit_locked_); // re-derive the send controls' enabled state
    if (!reason.isEmpty()) {
        status_label_->setText(reason);
        status_label_->setStyleSheet(QString("color: %1;").arg(colors::WARNING()));
    } else {
        status_label_->clear(); // the previous block reason
    }
}

void EquityOrderEntry::set_current_price(double price) {
    current_price_ = price;
    if (price > 0.0)
        last_price_ms_ = QDateTime::currentMSecsSinceEpoch();
    update_stale_state(); // clears the STALE marker on the first fresh tick
    render_price_label();
    update_cost_preview();
}

void EquityOrderEntry::apply_mode_tone() {
    mode_label_->setText(is_paper_ ? tr("\xE2\x97\x8B PAPER \xC2\xB7 SIMULATED") : tr("\xE2\x97\x8F LIVE \xC2\xB7 REAL ORDERS"));
    mode_label_->setToolTip(is_paper_ ? tr("Paper: every fill is simulated, nothing reaches a broker")
                                      : tr("LIVE: orders from this ticket reach the broker"));
    const QString tone = is_paper_ ? QStringLiteral("paper") : QStringLiteral("live");
    if (tone == mode_tone_)
        return;
    mode_tone_ = tone;
    for (QWidget* w : {static_cast<QWidget*>(mode_label_), static_cast<QWidget*>(mode_rule_)}) {
        w->setProperty("mode", tone);
        w->style()->unpolish(w);
        w->style()->polish(w);
    }
}

void EquityOrderEntry::set_mode(bool is_paper) {
    is_paper_ = is_paper;
    apply_mode_tone();
    if (is_paper)
        margin_label_->hide();
    apply_sl_tp_availability(); // SL/TP availability depends on paper vs broker routing
}

void EquityOrderEntry::set_symbol(const QString& symbol) {
    const bool changed = (symbol != current_symbol_);
    current_symbol_ = symbol;
    if (changed)
        symbol_display_.clear(); // the caller sets the new symbol's label next
    submit_btn_->setText(is_buy_side_ ? tr("BUY %1").arg(symbol) : tr("SELL %1").arg(symbol));
    submit_btn_->setAccessibleName(is_buy_side_ ? tr("Buy %1").arg(symbol) : tr("Sell %1").arg(symbol));
    if (symbol_label_)
        symbol_label_->setText(QStringLiteral("%1 · %2").arg(symbol, current_exchange_));
    if (changed) {
        // The cached LTP belongs to the PREVIOUS symbol. Showing it under the new
        // ticker is a wrong price on an order ticket — clear until the new
        // symbol's first quote arrives.
        current_price_ = 0.0;
        last_price_ms_ = 0;
        price_is_stale_ = false;
        // ... and so does its currency: the venue's until the instrument names its own
        // (built-in paper: unknown until set_instrument_meta()).
        instrument_ccy_known_ = false;
        if (builtin_mode_)
            current_currency_.clear();
        sync_price_currency();
        render_price_label();
        update_cost_preview();
        // Same for the broker margin: it was quoted for the previous symbol.
        if (margin_label_)
            margin_label_->hide();
        if (margin_timer_)
            margin_timer_->start();
    }
}

void EquityOrderEntry::set_exchange(const QString& exchange) {
    current_exchange_ = exchange;
    if (symbol_label_)
        symbol_label_->setText(QStringLiteral("%1 · %2").arg(current_symbol_, exchange));
    sync_price_currency();
    for (int i = 0; i < exchange_combo_->count(); ++i) {
        if (exchange_combo_->itemText(i) == exchange) {
            exchange_combo_->setCurrentIndex(i);
            break;
        }
    }
}

trading::OrderType EquityOrderEntry::selected_order_type() const {
    static const trading::OrderType type_map[] = {trading::OrderType::Market, trading::OrderType::Limit,
                                                  trading::OrderType::StopLoss, trading::OrderType::StopLossLimit};
    const int idx = (active_type_ >= 0 && active_type_ < 4) ? active_type_ : 0;
    return type_map[idx];
}

trading::ProductType EquityOrderEntry::selected_product_type() const {
    const int prod_idx = product_combo_->currentIndex();
    if (!product_types_.isEmpty() && prod_idx >= 0 && prod_idx < product_types_.size())
        return product_types_[prod_idx].value;
    return trading::ProductType::Intraday;
}

bool EquityOrderEntry::validate_entry(double& qty_out) {
    auto fail = [this](const QString& msg) {
        status_label_->setText(msg);
        status_label_->setStyleSheet(QString("color: %1;").arg(colors::NEGATIVE()));
        return false;
    };

    // QString::toDouble() is locale-independent and returns 0.0 on ANY parse
    // failure, so "1,000" / "12 " / "abc" all silently became zero. Check the
    // flag explicitly and say what is wrong instead of a generic message.
    bool ok = false;
    const double qty = qty_edit_->text().trimmed().toDouble(&ok);
    if (!ok || !std::isfinite(qty))
        return fail(tr("Quantity is not a number (use plain digits, no separators)"));
    if (qty <= 0.0)
        return fail(tr("Enter a valid quantity"));
    if (lot_known_ && lot_size_ > 1.0) {
        const double lots = qty / lot_size_;
        if (std::fabs(lots - std::round(lots)) > 1e-9)
            return fail(tr("This market trades in lots of %1 — enter a multiple of %1").arg(format_quantity(lot_size_)));
    }
    qty_out = qty;

    // Validate price/trigger for the order types that require them. Without this
    // a blank Limit price sends price=0 to the broker (and a paper SELL-limit @0
    // would fill at market).
    const trading::OrderType otype = selected_order_type();
    if (otype == trading::OrderType::Limit || otype == trading::OrderType::StopLossLimit) {
        bool px_ok = false;
        const double limit_px = price_edit_->text().trimmed().toDouble(&px_ok);
        if (!px_ok || !std::isfinite(limit_px) || limit_px <= 0.0)
            return fail(tr("Enter a valid limit price"));
    }
    if (otype == trading::OrderType::StopLoss || otype == trading::OrderType::StopLossLimit) {
        bool tp_ok = false;
        const double trigger_px = stop_price_edit_->text().trimmed().toDouble(&tp_ok);
        if (!tp_ok || !std::isfinite(trigger_px) || trigger_px <= 0.0)
            return fail(tr("Enter a valid trigger price"));
    }

    // Optional protective legs: blank means "none", but a box that is filled in and does not
    // parse ("2,450", "abc") used to become 0.0 in build_order() — the order went out with
    // the stop-loss / target the trader believed they had set silently dropped.
    auto filled_but_invalid = [](const QLineEdit* edit) {
        if (!edit)
            return false;
        const QString txt = edit->text().trimmed();
        if (txt.isEmpty())
            return false;
        bool num_ok = false;
        const double v = txt.toDouble(&num_ok);
        return !num_ok || !std::isfinite(v) || v <= 0.0;
    };
    const bool sl_bad = filled_but_invalid(sl_edit_);
    if (sl_bad || filled_but_invalid(tp_edit_)) {
        // SL / TP live in the collapsible ADVANCED section — open it so the offending box is visible.
        if (advanced_toggle_ && advanced_section_ && !advanced_section_->isVisible())
            advanced_toggle_->click();
        return fail(sl_bad ? tr("Stop loss is not a valid price") : tr("Take profit is not a valid price"));
    }
    return true;
}

trading::UnifiedOrder EquityOrderEntry::build_order() const {
    const trading::OrderType otype = selected_order_type();
    const bool uses_limit = (otype == trading::OrderType::Limit || otype == trading::OrderType::StopLossLimit);
    const bool uses_trigger = (otype == trading::OrderType::StopLoss || otype == trading::OrderType::StopLossLimit);

    trading::UnifiedOrder order;
    order.symbol = current_symbol_;
    // Built-in paper: the instrument's own venue (no broker exchange picker).
    order.exchange = builtin_mode_ ? current_exchange_ : exchange_combo_->currentText();
    order.side = is_buy_side_ ? trading::OrderSide::Buy : trading::OrderSide::Sell;
    order.order_type = otype;
    order.quantity = qty_edit_->text().trimmed().toDouble();
    // The price/trigger boxes keep their text when disabled, so a value typed for
    // a LIMIT order used to ride along on a MARKET order (brokers differ on
    // whether they ignore it). Send only the fields the order type actually uses.
    order.price = uses_limit ? price_edit_->text().trimmed().toDouble() : 0.0;
    order.stop_price = uses_trigger ? stop_price_edit_->text().trimmed().toDouble() : 0.0;
    order.stop_loss = sl_edit_ ? sl_edit_->text().trimmed().toDouble() : 0.0;
    order.take_profit = tp_edit_ ? tp_edit_->text().trimmed().toDouble() : 0.0;
    order.product_type = selected_product_type();
    order.validity = QStringLiteral("DAY");
    return order;
}

void EquityOrderEntry::set_send_locked(bool locked) {
    submit_locked_ = locked;
    // A blocking broker banner (or an instrument the account cannot trade) keeps the
    // controls off even when a result arrives.
    const bool enabled = !locked && !broker_blocked_ && trade_block_.isEmpty();
    if (submit_btn_)
        submit_btn_->setEnabled(enabled);
    if (broadcast_btn_)
        broadcast_btn_->setEnabled(enabled);
    if (place_strategy_btn_)
        place_strategy_btn_->setEnabled(enabled);
    if (!submit_failsafe_)
        return;
    if (locked)
        submit_failsafe_->start();
    else
        submit_failsafe_->stop();
}

void EquityOrderEntry::on_submit() {
    // Double-submit guard. The previous version re-enabled after a fixed 1s,
    // but a live placement round-trip regularly exceeds that — a second click
    // then sent a SECOND real order. Stay locked until the placement result
    // arrives via show_order_status(), with a failsafe timer so the ticket can
    // never wedge.
    if (submit_locked_)
        return;
    if (broker_blocked_) {
        status_label_->setText(notice_title_ ? notice_title_->text() : tr("Orders need a broker account"));
        status_label_->setStyleSheet(QString("color: %1;").arg(colors::NEGATIVE()));
        return;
    }
    if (!trade_block_.isEmpty()) {
        status_label_->setText(trade_block_);
        status_label_->setStyleSheet(QString("color: %1;").arg(colors::WARNING()));
        return;
    }

    double qty = 0.0;
    if (!validate_entry(qty))
        return;

    const trading::UnifiedOrder order = build_order();

    // Drop any account the user ticked in the BROKERS menu that has since been
    // removed or deactivated — the menu only prunes when it is re-opened, so a
    // stale id could otherwise be broadcast to.
    QStringList targets;
    const bool multi_armed = !broadcast_ids_.isEmpty();
    if (multi_armed) {
        QSet<QString> alive;
        for (const auto& account : trading::AccountManager::instance().active_accounts())
            alive.insert(account.account_id);
        for (const auto& id : broadcast_ids_)
            if (alive.contains(id))
                targets << id;
        broadcast_ids_ = QSet<QString>(targets.begin(), targets.end());
        update_brokers_btn();
        if (targets.isEmpty()) {
            // Every ticked account is gone. Silently falling through would route
            // the order to the FOCUSED account instead — never guess a destination.
            status_label_->setText(tr("Selected broker accounts no longer exist — pick targets again"));
            status_label_->setStyleSheet(QString("color: %1;").arg(colors::NEGATIVE()));
            return;
        }
    }

    set_send_locked(true);

    if (!targets.isEmpty()) {
        // Multi-broker route: the same order to every checked account; the
        // screen confirms, gates per-account (Semi-Auto), and broadcasts.
        emit multi_broker_submit(order, targets);
        return;
    }

    emit order_submitted(order);
}

// ── Inline multi-broker selector ─────────────────────────────────────────────

void EquityOrderEntry::rebuild_brokers_menu() {
    brokers_menu_->clear();

    const auto accounts = trading::AccountManager::instance().active_accounts();
    QSet<QString> alive;
    for (const auto& account : accounts) {
        alive.insert(account.account_id);
        auto* broker = trading::BrokerRegistry::instance().get(account.broker_id);
        const QString broker_name = broker ? broker->profile().display_name : account.broker_id;
        const QString mode_tag = account.trading_mode == "live" ? tr("[LIVE]") : tr("[PAPER]");
        const QString text = QString("%1  [%2]  %3").arg(account.display_name, broker_name, mode_tag);

        auto* cb = new QCheckBox(text, brokers_menu_);
        cb->setChecked(broadcast_ids_.contains(account.account_id));
        cb->setStyleSheet("QCheckBox{padding:4px 10px;font-size:" + fincept::ui::fonts::px(-3) + "px;}");
        const QString id = account.account_id;
        connect(cb, &QCheckBox::toggled, this, [this, id](bool on) {
            if (on)
                broadcast_ids_.insert(id);
            else
                broadcast_ids_.remove(id);
            update_brokers_btn();
        });
        auto* wa = new QWidgetAction(brokers_menu_); // checkbox click keeps the menu open
        wa->setDefaultWidget(cb);
        brokers_menu_->addAction(wa);
    }

    if (accounts.isEmpty()) {
        auto* none = brokers_menu_->addAction(tr("No active accounts"));
        none->setEnabled(false);
    }

    // Drop selections for accounts that were removed/deactivated since checked.
    for (auto it = broadcast_ids_.begin(); it != broadcast_ids_.end();)
        it = alive.contains(*it) ? std::next(it) : broadcast_ids_.erase(it);
    update_brokers_btn();
}

void EquityOrderEntry::update_brokers_btn() {
    const int n = int(broadcast_ids_.size());
    brokers_btn_->setText(n == 0 ? tr("BROKERS: FOCUSED ▾") : tr("BROKERS: %1 SELECTED ▾").arg(n));
    brokers_btn_->setToolTip(n == 0 ? tr("Orders go to the focused account. Click to pick multiple brokers.")
                                    : tr("BUY/SELL will fire this order on %1 account(s)").arg(n));
    // Amber accent when armed so a multi-broker submit is never a surprise.
    brokers_btn_->setStyleSheet(
        QString("QToolButton{background:%1;border:1px solid %2;color:%3;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;"
                "padding:2px 8px;}QToolButton::menu-indicator{image:none;}")
            .arg(colors::BG_RAISED(), n == 0 ? colors::BORDER_MED() : colors::AMBER(),
                 n == 0 ? colors::TEXT_SECONDARY() : colors::AMBER()));
}

void EquityOrderEntry::show_order_status(const QString& msg, bool success) {
    // A placement outcome (or a cancellation) is the signal that the in-flight
    // order is resolved — release the send lock here rather than on a timer.
    set_send_locked(false);
    show_status_message(msg, success);
}

void EquityOrderEntry::show_status_message(const QString& msg, bool success) {
    status_label_->setText(msg);
    status_label_->setStyleSheet(QString("color: %1;").arg(success ? colors::POSITIVE() : colors::NEGATIVE()));
    // Clear only if THIS message is still showing: an older result's timer must not wipe a
    // newer message (validation error / next order's outcome) a few seconds after it appeared.
    QTimer::singleShot(5000, status_label_, [this, msg]() {
        if (status_label_->text() == msg)
            status_label_->clear();
    });
}

QString EquityOrderEntry::current_exchange() const {
    if (builtin_mode_)
        return current_exchange_;
    return exchange_combo_ ? exchange_combo_->currentText() : current_exchange_;
}

void EquityOrderEntry::set_account_id(const QString& account_id) {
    if (account_id == account_id_)
        return;
    account_id_ = account_id;
    // A margin figure quoted for the previous account must not stay on screen.
    if (margin_label_)
        margin_label_->hide();
    if (margin_timer_)
        margin_timer_->start();
}

void EquityOrderEntry::set_limit_price(double price) {
    if (!price_edit_ || price <= 0.0)
        return;
    // A depth-ladder click is only meaningful as a LIMIT: on MKT the price box is
    // disabled and the value would be both invisible and unused.
    if (active_type_ != 1 && active_type_ != 3)
        set_order_type(1); // LMT
    // Plain C-locale digits (the box parses back with toDouble), in the instrument's
    // price decimals: a 0.0345 or 117.705p level is no longer cut to 2 decimals.
    price_edit_->setText(
        QString::number(price, 'f', fmt::price_decimals(price, ticket_price_fmt(current_symbol_, current_currency_,
                                                                                 price_digits_))));
    update_cost_preview();
    if (status_label_) {
        status_label_->setText(tr("Limit price %1 taken from market depth").arg(fmt_px_sym(price)));
        status_label_->setStyleSheet(QString("color: %1;").arg(colors::TEXT_SECONDARY()));
    }
}

void EquityOrderEntry::focus_quantity() {
    if (!qty_edit_)
        return;
    qty_edit_->setFocus(Qt::ShortcutFocusReason);
    qty_edit_->selectAll();
}

void EquityOrderEntry::focus_side(bool is_buy) {
    if (buy_tab_ && sell_tab_ && is_buy != is_buy_side_)
        set_buy_side(is_buy);
    focus_quantity();
}

void EquityOrderEntry::set_broker_notice(const QString& title, const QString& detail, bool blocking) {
    if (!notice_box_)
        return;
    const bool show = !title.isEmpty();
    const bool block = show && blocking;
    if (show && (notice_box_->isHidden() || block != notice_blocking_style_)) {
        // Blocking = nothing can be sent (red accent); informational = amber.
        const QString accent = block ? QString(colors::NEGATIVE()) : QString(colors::WARNING());
        notice_box_->setStyleSheet(
            QString("QFrame#eqOeNotice{background:%1;border:1px solid %2;border-left:3px solid %2;"
                    "border-radius:3px;}")
                .arg(colors::BG_BASE(), accent));
        notice_title_->setStyleSheet(QString("background:transparent;border:none;color:%1;font-size:" + fincept::ui::fonts::px(-3) + "px;"
                                             "font-weight:700;letter-spacing:0.5px;")
                                         .arg(accent));
        notice_blocking_style_ = block;
    }
    notice_title_->setText(title);
    notice_detail_->setText(detail);
    notice_detail_->setVisible(!detail.isEmpty());
    notice_box_->setVisible(show);

    if (block != broker_blocked_) {
        broker_blocked_ = block;
        set_send_locked(submit_locked_); // re-derive the send controls' enabled state
    }
}

void EquityOrderEntry::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (stale_timer_)
        stale_timer_->start(); // P3: cadence only while the ticket is on screen
    update_stale_state();
}

void EquityOrderEntry::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (stale_timer_)
        stale_timer_->stop();
}

// Text only — this runs on every tick, so it must never touch the stylesheet
// (each setStyleSheet is a full CSS reparse; see CLAUDE.md P7).
void EquityOrderEntry::render_price_label() {
    if (!market_price_label_)
        return;
    if (current_price_ <= 0.0) {
        market_price_label_->setText(delayed_price_ ? tr("LTP -- (waiting for a delayed quote)") : tr("LTP --"));
        return;
    }
    if (delayed_price_) {
        // A delayed public price: say so on the number itself, with when it printed.
        QString when;
        if (printed_ms_ > 0)
            when = QDateTime::fromMSecsSinceEpoch(printed_ms_).toString(QStringLiteral("HH:mm"));
        const QString delay = delay_min_ > 0 ? tr("DELAYED %1m").arg(delay_min_) : tr("DELAYED");
        QString text = tr("LTP %1 \xC2\xB7 %2").arg(fmt_px_sym(current_price_), delay);
        if (!when.isEmpty())
            text += QStringLiteral(" \xC2\xB7 ") + when;
        if (price_is_stale_)
            text += tr("  \xe2\x9a\xa0 NOT UPDATING");
        market_price_label_->setText(text);
        return;
    }
    // In the instrument's own currency and decimals ("$351.71", "117.70p", "¥2,845").
    const QString px = fmt_px_sym(current_price_);
    market_price_label_->setText(price_is_stale_ ? tr("LTP %1  \xe2\x9a\xa0 STALE").arg(px) : tr("LTP %1").arg(px));
}

// Owns the stale flag AND its styling; the stylesheet is only reparsed on the
// rare transition, never per tick.
void EquityOrderEntry::update_stale_state() {
    if (!market_price_label_)
        return;
    // A delayed public quote refreshes about every 30 s; judge it on that cadence.
    const qint64 limit = delayed_price_ && delayed_max_age_ms_ > 0 ? delayed_max_age_ms_ : kStaleQuoteMs;
    const bool stale = current_price_ > 0.0 && last_price_ms_ > 0 &&
                       (QDateTime::currentMSecsSinceEpoch() - last_price_ms_) > limit;
    if (stale == price_is_stale_)
        return; // no churn on the common path
    price_is_stale_ = stale;
    if (stale) {
        market_price_label_->setStyleSheet(QString("color: %1; font-weight:700;").arg(colors::WARNING()));
        market_price_label_->setToolTip(
            delayed_price_
                ? tr("No delayed quote has arrived for over %1 min — the public price feed is not updating. MARKET "
                     "orders are refused until it does.")
                      .arg(limit / 60000)
                : tr("This price has not updated for over %1s — the feed may be down or the market closed. "
                     "Do not treat it as the live price.")
                      .arg(kStaleQuoteMs / 1000));
    } else {
        market_price_label_->setStyleSheet(QString()); // back to the global sheet
        market_price_label_->setToolTip(delayed_price_ ? tr("Last traded price — delayed public data, not live")
                                                       : tr("Last traded price"));
    }
    render_price_label();
}

void EquityOrderEntry::update_cost_preview() {
    const double qty = qty_edit_->text().toDouble();
    double price = current_price_;
    if (active_type_ == 1 || active_type_ == 3) {
        const double limit_p = price_edit_->text().toDouble();
        if (limit_p > 0)
            price = limit_p;
    }
    est_converts_ = false;
    if (!(qty > 0 && price > 0)) {
        cost_label_->setText(tr("Est: --"));
        cost_label_->setToolTip(QString());
        return;
    }
    // Order value in the price currency, major unit (100 x 117.70p -> £117.70).
    const double value = qty * price;
    QString text = tr("Est: %1").arg(fmt_value(value));
    QString tip;
    double value_in_account = NAN; // the order value in the account currency, when known
    // The account (balance, margin, paper cash) is in another currency: show the value
    // there too, converted at the current rate with the rate and its time. No rate ->
    // "—", never 1:1.
    if (!balance_currency_.isEmpty() && !current_currency_.isEmpty() &&
        fmt::major_currency(current_currency_) != fmt::major_currency(balance_currency_)) {
        est_converts_ = true;
        const fmt::ConvertedMoney c = fx::convert_for_display(value, current_currency_, balance_currency_);
        if (c.ok()) {
            value_in_account = c.amount;
            const QString note = fmt::fx_note(c);
            text += QStringLiteral("\n") + tr("In %1: \xe2\x89\x88 %2").arg(fmt::major_currency(balance_currency_),
                                                                            fmt::money(c.amount, c.currency));
            if (!note.isEmpty())
                text += QStringLiteral(" (%1)").arg(note);
        } else {
            text += QStringLiteral("\n") + tr("In %1: %2 (no %3\xe2\x86\x92%1 rate yet)")
                                               .arg(fmt::major_currency(balance_currency_), fmt::placeholder(),
                                                    fmt::major_currency(current_currency_));
        }
        tip = builtin_mode_ ? tr("Cash is booked in %1 at the FX rate of the fill.").arg(fmt::major_currency(balance_currency_))
                            : tr("At the current FX rate; the broker converts at its own rate.");
    } else {
        // Same currency. Either currency unknown: no "after" line (never assume 1:1).
        if (!balance_currency_.isEmpty() && !current_currency_.isEmpty())
            value_in_account = fmt::to_major_units(value, current_currency_);
    }
    // Buying power after a BUY, in the account currency. A shortfall is said inline
    // (the broker / paper engine still decides; nothing is blocked here).
    if (is_buy_side_ && balance_set_ && std::isfinite(value_in_account) && !balance_currency_.isEmpty()) {
        const double after = balance_ - value_in_account;
        text += QStringLiteral("\n") +
                (after >= 0.0 ? tr("Buying power after: %1").arg(fmt::money(after, balance_currency_))
                              : tr("\xE2\x9A\xA0 Exceeds buying power by %1").arg(fmt::money(-after, balance_currency_)));
    }
    cost_label_->setText(text);
    cost_label_->setToolTip(tip);
}

void EquityOrderEntry::set_broker_id(const QString& broker_id) {
    broker_id_ = broker_id;
}

void EquityOrderEntry::fetch_margin_async() {
    // Only fetch for live mode with a real broker
    if (is_paper_ || broker_id_.isEmpty() || account_id_.isEmpty())
        return;

    const double qty = qty_edit_->text().toDouble();
    if (qty <= 0) {
        margin_label_->hide(); // no quantity -> no margin; don't leave the last figure up
        return;
    }

    // Guard against concurrent fetches. A skipped request is NOT dropped: the in-flight
    // result belongs to older inputs, so the completion below re-arms this timer.
    bool expected = false;
    if (!margin_fetching_.compare_exchange_strong(expected, true))
        return;

    trading::UnifiedOrder order;
    order.symbol = current_symbol_;
    order.exchange = exchange_combo_->currentText();
    order.side = is_buy_side_ ? trading::OrderSide::Buy : trading::OrderSide::Sell;
    order.quantity = qty;
    order.price = price_edit_->text().toDouble();
    order.stop_price = stop_price_edit_->text().toDouble();
    order.order_type = selected_order_type();
    order.product_type = selected_product_type();

    const QString bid = broker_id_;
    const QString acct_id = account_id_; // by value: the focused account may change mid-flight
    QPointer<EquityOrderEntry> self = this;

    (void)QtConcurrent::run([self, bid, acct_id, order]() {
        auto* broker = trading::BrokerRegistry::instance().get(bid);
        if (!broker) {
            if (self)
                self->margin_fetching_ = false;
            return;
        }
        // The FOCUSED account's credentials ("account.<id>.*", as OrderConfirmDialog
        // uses). IBroker::load_credentials() reads the legacy per-broker "broker.<id>.*"
        // keys, which the multi-account app never writes — so the margin line never
        // appeared for an ACCOUNTS-created account, or showed another login's margin.
        auto creds = trading::AccountManager::instance().load_credentials(acct_id);
        if (creds.api_key.isEmpty() || creds.broker_id != bid) {
            if (self)
                self->margin_fetching_ = false;
            return;
        }
        auto result = broker->get_order_margins(creds, order);
        // QMetaObject::invokeMethod asserts on a null receiver — the widget can be
        // torn down while the (blocking) broker call is in flight (P8).
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, result, order, acct_id]() {
                if (!self)
                    return;
                self->margin_fetching_ = false;
                // The ticket may have changed while the broker call was in flight (account,
                // symbol, side, qty, price...). That margin belongs to the OLD ticket — showing
                // it would quote the wrong requirement, so drop it and fetch for the current one.
                const bool inputs_changed =
                    self->account_id_ != acct_id || self->current_symbol_ != order.symbol ||
                    self->exchange_combo_->currentText() != order.exchange ||
                    (self->is_buy_side_ ? trading::OrderSide::Buy : trading::OrderSide::Sell) != order.side ||
                    self->selected_order_type() != order.order_type ||
                    self->selected_product_type() != order.product_type ||
                    self->qty_edit_->text().toDouble() != order.quantity ||
                    self->price_edit_->text().toDouble() != order.price ||
                    self->stop_price_edit_->text().toDouble() != order.stop_price;
                if (inputs_changed) {
                    self->margin_label_->hide();
                    self->margin_timer_->start();
                    return;
                }
                if (result.success && result.data) {
                    const auto& m = *result.data;
                    self->margin_total_ = m.total; // account currency (BrokerMargin)
                    self->margin_label_->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-4) + "px;").arg(colors::AMBER()));
                    self->margin_label_->show();
                    self->render_margin_label();
                } else {
                    self->margin_label_->hide();
                }
            },
            Qt::QueuedConnection);
    });
}

bool EquityOrderEntry::build_strategy(trading::OptionsStrategy& out) const {
    const int idx = strategy_combo_ ? strategy_combo_->currentIndex() : 0;
    if (idx <= 0) // "None"
        return false;

    const QString underlying = current_symbol_;
    const QString expiry = strat_expiry_edit_->text().trimmed();
    const double atm = strat_strike_edit_->text().toDouble();
    // LOTS per leg: a whole number >= 1. A blank / garbage / fractional entry is refused
    // rather than silently becoming 1 lot (to_basket_order multiplies it by the
    // contract's lot size to get the exchange quantity).
    bool lot_ok = false;
    const double lot = strat_lot_edit_->text().trimmed().toDouble(&lot_ok);
    if (!lot_ok || !std::isfinite(lot) || lot < 1.0 || std::floor(lot) != lot)
        return false;
    const double width = strat_width_edit_->text().toDouble();

    if (expiry.isEmpty() || atm <= 0)
        return false;

    using B = trading::OptionsStrategyBuilder;
    using Side = trading::OrderSide;

    switch (idx) {
        case 1: // Long Straddle
            out = B::straddle(underlying, expiry, atm, lot, Side::Buy);
            return true;
        case 2: // Short Straddle
            out = B::straddle(underlying, expiry, atm, lot, Side::Sell);
            return true;
        case 3: // Long Strangle
            if (width <= 0)
                return false;
            out = B::strangle(underlying, expiry, atm, width, lot, Side::Buy);
            return true;
        case 4: // Iron Condor — near_width = width, far_width = 2 * width
            if (width <= 0)
                return false;
            out = B::iron_condor(underlying, expiry, atm, width, width * 2.0, lot);
            return true;
        case 5: // Bull Call Spread — buy ATM CE, sell (ATM + width) CE
            if (width <= 0)
                return false;
            out = B::bull_call_spread(underlying, expiry, atm, atm + width, lot);
            return true;
        case 6: // Bear Put Spread — buy ATM PE, sell (ATM - width) PE
            if (width <= 0)
                return false;
            out = B::bear_put_spread(underlying, expiry, atm, atm - width, lot);
            return true;
        default:
            return false;
    }
}

void EquityOrderEntry::refresh_strategy_preview() {
    if (!strategy_preview_)
        return;

    const int idx = strategy_combo_ ? strategy_combo_->currentIndex() : 0;
    if (idx <= 0) {
        strategy_preview_->setText(tr("Select a strategy"));
        return;
    }

    trading::OptionsStrategy strat;
    if (!build_strategy(strat)) {
        strategy_preview_->setText(
            tr("Enter expiry, ATM strike, a whole number of lots (and width where required)."));
        return;
    }

    // Resolve the legs exactly as PLACE STRATEGY will, so the preview shows the real
    // contracts and the quantity that will be sent (lots × the contract's lot size).
    trading::BasketOrderRequest basket;
    QString build_error;
    const bool resolved = trading::OptionsStrategyBuilder::to_basket_order(strat, selected_product_type(), broker_id_,
                                                                          basket, build_error);
    const double lots = strat.legs.isEmpty() ? 0.0 : strat.legs.first().quantity;
    double contract_lot = 0.0; // contract multiplier, known once the legs resolve
    if (resolved && !basket.orders.isEmpty() && lots > 0.0)
        contract_lot = basket.orders.first().quantity / lots;

    QStringList lines;
    if (!strat.name.isEmpty())
        lines << strat.name;
    if (resolved) {
        // SIDE qty CONTRACT · EXCHANGE (in placement order: SELL legs first)
        for (const auto& o : basket.orders)
            lines << QString("%1 %2 %3 · %4")
                         .arg(QString(trading::order_side_str(o.side)).toUpper(), format_quantity(o.quantity),
                              o.symbol, o.exchange);
        if (contract_lot > 0.0)
            lines << tr("%1 lot(s) × %2 per leg").arg(format_quantity(lots), format_quantity(contract_lot));
    } else {
        for (const auto& leg : strat.legs) {
            // SIDE lots SYMBOL @strike type
            lines << tr("%1 %2 lot(s) %3 @%4 %5")
                         .arg(QString(trading::order_side_str(leg.side)).toUpper())
                         .arg(leg.quantity, 0, 'f', 0)
                         .arg(leg.symbol)
                         .arg(leg.strike, 0, 'f', 0)
                         .arg(leg.option_type);
        }
        lines << tr("Quantity unknown — %1").arg(build_error);
    }

    QStringList metrics;
    // max_profit from the builder is the gross strike spread × LOTS; only shown once the
    // contract lot size is known, as spread × lots × lot size.
    // Max P / Max L are amounts, breakevens are underlying prices — all in the ticket's price currency.
    if (strat.max_profit != 0.0 && contract_lot > 0.0)
        metrics << tr("Max P: %1").arg(fmt_value(strat.max_profit * contract_lot));
    if (strat.max_loss != 0.0)
        metrics << tr("Max L: %1").arg(fmt_value(strat.max_loss));
    if (strat.breakeven_lower != 0.0)
        metrics << tr("BE-: %1").arg(fmt_px_sym(strat.breakeven_lower));
    if (strat.breakeven_upper != 0.0)
        metrics << tr("BE+: %1").arg(fmt_px_sym(strat.breakeven_upper));
    if (!metrics.isEmpty())
        lines << metrics.join("  ");

    strategy_preview_->setText(lines.join("\n"));
}

void EquityOrderEntry::on_place_strategy() {
    if (submit_locked_ || broker_blocked_)
        return; // a placement is already in flight / no broker to send to
    trading::OptionsStrategy strat;
    if (!build_strategy(strat)) {
        status_label_->setText(tr("Select a strategy and enter valid expiry / strike / lots / width"));
        status_label_->setStyleSheet(QString("color: %1;").arg(colors::NEGATIVE()));
        return;
    }
    // Product type from profile-driven combo (same mapping as on_submit)
    trading::ProductType product = trading::ProductType::Intraday;
    const int prod_idx = product_combo_->currentIndex();
    if (!product_types_.isEmpty() && prod_idx >= 0 && prod_idx < product_types_.size())
        product = product_types_[prod_idx].value;

    // Every leg must resolve to a real contract in the broker's instrument master;
    // otherwise nothing is sent (a placeholder symbol must never reach a broker).
    trading::BasketOrderRequest basket;
    QString build_error;
    if (!trading::OptionsStrategyBuilder::to_basket_order(strat, product, broker_id_, basket, build_error)) {
        show_order_status(build_error, false); // lock not armed yet — releasing it is a no-op
        return;
    }
    set_send_locked(true); // released by show_order_status() on the result
    emit strategy_order_submitted(basket);
}

void EquityOrderEntry::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void EquityOrderEntry::retranslateUi() {
    if (title_label_)
        title_label_->setText(tr("ORDER TICKET")); // the panel header shows titles upper case
    if (mode_label_)
        apply_mode_tone();

    // Side tabs + submit button (reflect current side + symbol)
    if (buy_tab_)
        buy_tab_->setText(tr("BUY"));
    if (sell_tab_)
        sell_tab_->setText(tr("SELL"));
    if (submit_btn_)
        submit_btn_->setText(is_buy_side_ ? tr("BUY %1").arg(current_symbol_) : tr("SELL %1").arg(current_symbol_));
    if (broadcast_btn_) {
        broadcast_btn_->setText(tr("ALL"));
        broadcast_btn_->setToolTip(tr("Broadcast this order to multiple accounts"));
    }

    // Order type segmented control
    const QString type_labels[] = {tr("MKT"), tr("LMT"), tr("SL"), tr("SL-L")};
    for (int i = 0; i < 4; ++i)
        if (type_btns_[i])
            type_btns_[i]->setText(type_labels[i]);

    // Static field / section titles
    if (product_title_)
        product_title_->setText(tr("PRODUCT"));
    if (exchange_title_)
        exchange_title_->setText(tr("EXCHANGE"));
    if (qty_title_)
        qty_title_->setText(tr("QTY"));
    if (price_title_)
        price_title_->setText(tr("PRICE"));
    if (trigger_title_)
        trigger_title_->setText(tr("TRIGGER"));
    if (sl_title_)
        sl_title_->setText(tr("SL"));
    if (tp_title_)
        tp_title_->setText(tr("TP"));
    if (strat_title_)
        strat_title_->setText(tr("STRATEGY"));
    if (strat_strike_title_)
        strat_strike_title_->setText(tr("ATM STRIKE"));
    if (strat_expiry_title_)
        strat_expiry_title_->setText(tr("EXPIRY (YYYY-MM-DD)"));
    if (strat_lot_title_)
        strat_lot_title_->setText(tr("LOTS"));
    if (strat_width_title_)
        strat_width_title_->setText(tr("WIDTH (strangle / condor / spreads)"));
    if (strat_legs_title_)
        strat_legs_title_->setText(tr("LEGS"));

    // Placeholders
    if (price_edit_)
        price_edit_->setPlaceholderText(tr("Limit price"));
    if (stop_price_edit_)
        stop_price_edit_->setPlaceholderText(tr("Trigger price"));
    if (sl_edit_)
        sl_edit_->setPlaceholderText(tr("Stop Loss"));
    if (tp_edit_)
        tp_edit_->setPlaceholderText(tr("Take Profit"));
    if (strat_strike_edit_)
        strat_strike_edit_->setPlaceholderText(tr("e.g. 22500"));
    if (strat_expiry_edit_)
        strat_expiry_edit_->setPlaceholderText(tr("YYYY-MM-DD"));
    if (strat_lot_edit_)
        strat_lot_edit_->setToolTip(
            tr("Whole number of lots per leg — the quantity sent is lots × the contract lot size"));
    if (strat_width_edit_)
        strat_width_edit_->setPlaceholderText(tr("e.g. 200"));

    // Collapsible toggles reflect their expanded state
    if (advanced_toggle_ && advanced_section_)
        advanced_toggle_->setText(advanced_section_->isVisible() ? tr("- ADVANCED") : tr("+ ADVANCED"));
    if (strategy_toggle_ && strategy_section_)
        strategy_toggle_->setText(strategy_section_->isVisible() ? tr("- OPTIONS STRATEGY") : tr("+ OPTIONS STRATEGY"));

    // Strategy combo — re-apply fixed UI labels, preserving the selection.
    if (strategy_combo_) {
        const int sel = strategy_combo_->currentIndex();
        const QStringList items = {tr("None"),        tr("Long Straddle"),    tr("Short Straddle"), tr("Long Strangle"),
                                   tr("Iron Condor"), tr("Bull Call Spread"), tr("Bear Put Spread")};
        for (int i = 0; i < items.size() && i < strategy_combo_->count(); ++i)
            strategy_combo_->setItemText(i, items[i]);
        strategy_combo_->setCurrentIndex(sel);
    }

    // Recompute data-derived previews so they re-render in the new language.
    refresh_strategy_preview();
    update_cost_preview();
    render_price_label();
}

} // namespace fincept::screens::equity
