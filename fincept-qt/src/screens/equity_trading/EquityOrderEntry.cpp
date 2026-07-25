// EquityOrderEntry.cpp — BUY/SELL form configurable per broker profile
#include "screens/equity_trading/EquityOrderEntry.h"

#include "core/logging/Logger.h"
#include "screens/equity_trading/BasketOrdersDialog.h"
#include "screens/equity_trading/EquityTypes.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "trading/OptionsStrategyBuilder.h"
#include "ui/theme/Theme.h"

#include <QCheckBox>
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
    auto* header = new QWidget(this);
    header->setObjectName("eqOeHeader");
    header->setFixedHeight(32);
    auto* h_layout = new QHBoxLayout(header);
    h_layout->setContentsMargins(10, 0, 10, 0);

    title_label_ = new QLabel(tr("ORDER ENTRY"));
    title_label_->setObjectName("eqOeTitle");
    h_layout->addWidget(title_label_);
    h_layout->addStretch();

    mode_label_ = new QLabel(tr("PAPER"));
    mode_label_->setObjectName("eqOeMode");
    mode_label_->setStyleSheet(QString("color: %1;").arg(colors::POSITIVE()));
    h_layout->addWidget(mode_label_);

    layout->addWidget(header);

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

    // BUY / SELL tabs
    auto* side_row = new QHBoxLayout;
    side_row->setSpacing(0);

    buy_tab_ = new QPushButton(tr("BUY"));
    buy_tab_->setObjectName("eqBuyTab");
    buy_tab_->setProperty("active", true);
    buy_tab_->setCursor(Qt::PointingHandCursor);
    buy_tab_->setFixedHeight(36);
    connect(buy_tab_, &QPushButton::clicked, this, [this]() { set_buy_side(true); });
    side_row->addWidget(buy_tab_, 1);

    sell_tab_ = new QPushButton(tr("SELL"));
    sell_tab_->setObjectName("eqSellTab");
    sell_tab_->setProperty("active", false);
    sell_tab_->setCursor(Qt::PointingHandCursor);
    sell_tab_->setFixedHeight(36);
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
        type_btns_[i]->setFixedHeight(26);
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
    product_combo_->setFixedHeight(28);
    params->addWidget(product_title_, prow, 0);
    params->addWidget(product_combo_, prow, 1);
    ++prow;

    exchange_title_ = new QLabel(tr("EXCHANGE"));
    exchange_title_->setObjectName("eqOeLabel");
    exchange_combo_ = new QComboBox;
    exchange_combo_->setObjectName("eqOeCombo");
    // Exchange codes (NSE/BSE/…) are market identifiers — data, not translated.
    exchange_combo_->addItems({"NSE", "BSE", "NYSE", "NASDAQ"});
    exchange_combo_->setFixedHeight(28);
    connect(exchange_combo_, &QComboBox::currentTextChanged, this, [this](const QString& ex) {
        current_exchange_ = ex;
        if (symbol_label_)
            symbol_label_->setText(QStringLiteral("%1 · %2").arg(current_symbol_, ex));
    });
    params->addWidget(exchange_title_, prow, 0);
    params->addWidget(exchange_combo_, prow, 1);
    ++prow;

    qty_title_ = new QLabel(tr("QTY"));
    qty_title_->setObjectName("eqOeLabel");
    qty_edit_ = new QLineEdit;
    qty_edit_->setObjectName("eqOeInput");
    qty_edit_->setPlaceholderText("0");
    qty_edit_->setFixedHeight(28);
    qty_edit_->setAlignment(Qt::AlignRight);
    connect(qty_edit_, &QLineEdit::textChanged, this, [this]() { update_cost_preview(); });
    params->addWidget(qty_title_, prow, 0);
    params->addWidget(qty_edit_, prow, 1);
    ++prow;

    price_title_ = new QLabel(tr("PRICE"));
    price_title_->setObjectName("eqOeLabel");
    price_edit_ = new QLineEdit;
    price_edit_->setObjectName("eqOeInput");
    price_edit_->setPlaceholderText(tr("Limit price"));
    price_edit_->setEnabled(false);
    price_edit_->setFixedHeight(28);
    price_edit_->setAlignment(Qt::AlignRight);
    connect(price_edit_, &QLineEdit::textChanged, this, [this]() { update_cost_preview(); });
    params->addWidget(price_title_, prow, 0);
    params->addWidget(price_edit_, prow, 1);
    ++prow;

    trigger_title_ = new QLabel(tr("TRIGGER"));
    trigger_title_->setObjectName("eqOeLabel");
    stop_price_edit_ = new QLineEdit;
    stop_price_edit_->setObjectName("eqOeInput");
    stop_price_edit_->setPlaceholderText(tr("Trigger price"));
    stop_price_edit_->setEnabled(false);
    stop_price_edit_->setFixedHeight(28);
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
    advanced_toggle_->setFixedHeight(22);
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
    sl_edit_->setFixedHeight(26);

    tp_title_ = new QLabel(tr("TP"));
    tp_title_->setObjectName("eqOeLabel");
    tp_edit_ = new QLineEdit;
    tp_edit_->setObjectName("eqOeInput");
    tp_edit_->setPlaceholderText(tr("Take Profit"));
    tp_edit_->setFixedHeight(26);

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
    strategy_toggle_->setFixedHeight(22);
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
    strategy_combo_->setFixedHeight(26);
    strat_layout->addWidget(strategy_combo_);

    strat_strike_title_ = new QLabel(tr("ATM STRIKE"));
    strat_strike_title_->setObjectName("eqOeLabel");
    strat_layout->addWidget(strat_strike_title_);
    strat_strike_edit_ = new QLineEdit;
    strat_strike_edit_->setObjectName("eqOeInput");
    strat_strike_edit_->setPlaceholderText(tr("e.g. 22500"));
    strat_strike_edit_->setFixedHeight(26);
    strat_layout->addWidget(strat_strike_edit_);

    strat_expiry_title_ = new QLabel(tr("EXPIRY (YYYY-MM-DD)"));
    strat_expiry_title_->setObjectName("eqOeLabel");
    strat_layout->addWidget(strat_expiry_title_);
    strat_expiry_edit_ = new QLineEdit;
    strat_expiry_edit_->setObjectName("eqOeInput");
    strat_expiry_edit_->setPlaceholderText("2025-03-28");
    strat_expiry_edit_->setFixedHeight(26);
    strat_layout->addWidget(strat_expiry_edit_);

    strat_lot_title_ = new QLabel(tr("LOT SIZE"));
    strat_lot_title_->setObjectName("eqOeLabel");
    strat_layout->addWidget(strat_lot_title_);
    strat_lot_edit_ = new QLineEdit;
    strat_lot_edit_->setObjectName("eqOeInput");
    strat_lot_edit_->setText("1");
    strat_lot_edit_->setPlaceholderText("1");
    strat_lot_edit_->setFixedHeight(26);
    strat_layout->addWidget(strat_lot_edit_);

    strat_width_title_ = new QLabel(tr("WIDTH (strangle / condor / spreads)"));
    strat_width_title_->setObjectName("eqOeLabel");
    strat_layout->addWidget(strat_width_title_);
    strat_width_edit_ = new QLineEdit;
    strat_width_edit_->setObjectName("eqOeInput");
    strat_width_edit_->setPlaceholderText(tr("e.g. 200"));
    strat_width_edit_->setFixedHeight(26);
    strat_layout->addWidget(strat_width_edit_);

    strat_legs_title_ = new QLabel(tr("LEGS"));
    strat_legs_title_->setObjectName("eqOeLabel");
    strat_layout->addWidget(strat_legs_title_);

    strategy_preview_ = new QLabel(tr("Select a strategy"));
    strategy_preview_->setObjectName("eqOeBrokerage");
    strategy_preview_->setStyleSheet(QString("color: %1; font-size: 11px;").arg(colors::TEXT_SECONDARY()));
    strategy_preview_->setWordWrap(true);
    strategy_preview_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    strat_layout->addWidget(strategy_preview_);

    place_strategy_btn_ = new QPushButton(tr("PLACE STRATEGY"));
    place_strategy_btn_->setObjectName("eqBuySubmit");
    place_strategy_btn_->setFixedHeight(30);
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
    brokers_btn_->setFixedHeight(24);
    brokers_btn_->setCursor(Qt::PointingHandCursor);
    brokers_menu_ = new QMenu(brokers_btn_);
    brokers_btn_->setMenu(brokers_menu_);
    connect(brokers_menu_, &QMenu::aboutToShow, this, &EquityOrderEntry::rebuild_brokers_menu);
    update_brokers_btn();

    // BASKETS — saved multi-leg order groups, executable on any account(s).
    auto* baskets_btn = new QToolButton;
    baskets_btn->setObjectName("eqBasketsBtn");
    baskets_btn->setText(tr("BASKETS"));
    baskets_btn->setFixedHeight(24);
    baskets_btn->setCursor(Qt::PointingHandCursor);
    baskets_btn->setStyleSheet(
        QString("QToolButton{background:%1;border:1px solid %2;color:%3;font-size:10px;font-weight:700;"
                "padding:2px 10px;}QToolButton:hover{border-color:%4;color:%4;}")
            .arg(colors::BG_RAISED(), colors::BORDER_MED(), colors::TEXT_TERTIARY(), colors::AMBER()));
    connect(baskets_btn, &QToolButton::clicked, this, [this]() {
        BasketOrdersDialog dlg(this);
        dlg.exec();
    });

    auto* brokers_row = new QHBoxLayout;
    brokers_row->setSpacing(4);
    brokers_row->addWidget(brokers_btn_, 1);
    brokers_row->addWidget(baskets_btn);
    form->addLayout(brokers_row);

    // Submit row: main submit + broadcast
    auto* submit_row = new QHBoxLayout;
    submit_row->setSpacing(4);

    submit_btn_ = new QPushButton(tr("BUY %1").arg(current_symbol_));
    submit_btn_->setObjectName("eqBuySubmit");
    submit_btn_->setFixedHeight(40);
    submit_btn_->setCursor(Qt::PointingHandCursor);
    connect(submit_btn_, &QPushButton::clicked, this, &EquityOrderEntry::on_submit);
    submit_row->addWidget(submit_btn_, 3);

    broadcast_btn_ = new QPushButton(tr("ALL"));
    broadcast_btn_->setObjectName("eqBroadcastBtn");
    broadcast_btn_->setFixedHeight(40);
    broadcast_btn_->setCursor(Qt::PointingHandCursor);
    broadcast_btn_->setToolTip(tr("Broadcast this order to multiple accounts"));
    broadcast_btn_->setStyleSheet(QString("QPushButton { background: %1; color: %2; font-weight: 700; font-size: 12px; "
                                          "  border: 1px solid %3; border-radius: 4px; }"
                                          "QPushButton:hover { background: %3; color: %2; }")
                                      .arg(colors::BG_RAISED(), colors::AMBER(), colors::BORDER_MED()));
    connect(broadcast_btn_, &QPushButton::clicked, this, [this]() {
        // The broadcast path fires the SAME order at N accounts, so it must run
        // the SAME validation as the focused submit. It previously checked only
        // the quantity, so a blank limit price sent price=0 as a LIMIT order to
        // every selected broker (and the dialog even labelled it "MKT").
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

    render_price_label();
}

void EquityOrderEntry::configure_for_broker(const trading::BrokerProfile& profile) {
    // Cache product types for order construction
    product_types_ = profile.product_types;
    current_currency_ = profile.currency;

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
    set_send_locked(false); // a broker switch must never leave the ticket wedged
    render_price_label();
    update_cost_preview();
}

void EquityOrderEntry::set_buy_side(bool is_buy) {
    is_buy_side_ = is_buy;
    buy_tab_->setProperty("active", is_buy);
    sell_tab_->setProperty("active", !is_buy);
    buy_tab_->style()->unpolish(buy_tab_);
    buy_tab_->style()->polish(buy_tab_);
    sell_tab_->style()->unpolish(sell_tab_);
    sell_tab_->style()->polish(sell_tab_);

    submit_btn_->setText(is_buy ? tr("BUY %1").arg(current_symbol_) : tr("SELL %1").arg(current_symbol_));
    submit_btn_->setAccessibleName(is_buy ? tr("Buy %1").arg(current_symbol_) : tr("Sell %1").arg(current_symbol_));
    submit_btn_->setObjectName(is_buy ? "eqBuySubmit" : "eqSellSubmit");
    submit_btn_->style()->unpolish(submit_btn_);
    submit_btn_->style()->polish(submit_btn_);
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
}

void EquityOrderEntry::set_balance(double balance) {
    balance_ = balance;
    balance_label_->setText(tr("Bal %1%2").arg(currency_symbol(current_currency_)).arg(balance, 0, 'f', 2));
    update_cost_preview();
}

void EquityOrderEntry::set_current_price(double price) {
    current_price_ = price;
    if (price > 0.0)
        last_price_ms_ = QDateTime::currentMSecsSinceEpoch();
    update_stale_state(); // clears the STALE marker on the first fresh tick
    render_price_label();
    update_cost_preview();
}

void EquityOrderEntry::set_mode(bool is_paper) {
    is_paper_ = is_paper;
    mode_label_->setText(is_paper ? tr("PAPER") : tr("LIVE"));
    mode_label_->setStyleSheet(QString("color: %1;").arg(is_paper ? colors::POSITIVE() : colors::NEGATIVE()));
    if (is_paper)
        margin_label_->hide();
}

void EquityOrderEntry::set_symbol(const QString& symbol) {
    const bool changed = (symbol != current_symbol_);
    current_symbol_ = symbol;
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
        render_price_label();
        update_cost_preview();
    }
}

void EquityOrderEntry::set_exchange(const QString& exchange) {
    current_exchange_ = exchange;
    if (symbol_label_)
        symbol_label_->setText(QStringLiteral("%1 · %2").arg(current_symbol_, exchange));
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
    return true;
}

trading::UnifiedOrder EquityOrderEntry::build_order() const {
    const trading::OrderType otype = selected_order_type();
    const bool uses_limit = (otype == trading::OrderType::Limit || otype == trading::OrderType::StopLossLimit);
    const bool uses_trigger = (otype == trading::OrderType::StopLoss || otype == trading::OrderType::StopLossLimit);

    trading::UnifiedOrder order;
    order.symbol = current_symbol_;
    order.exchange = exchange_combo_->currentText();
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
    if (submit_btn_)
        submit_btn_->setEnabled(!locked);
    if (broadcast_btn_)
        broadcast_btn_->setEnabled(!locked);
    if (place_strategy_btn_)
        place_strategy_btn_->setEnabled(!locked);
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
        cb->setStyleSheet("QCheckBox{padding:4px 10px;font-size:11px;}");
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
        QString("QToolButton{background:%1;border:1px solid %2;color:%3;font-size:10px;font-weight:700;"
                "padding:2px 8px;}QToolButton::menu-indicator{image:none;}")
            .arg(colors::BG_RAISED(), n == 0 ? colors::BORDER_MED() : colors::AMBER(),
                 n == 0 ? colors::TEXT_TERTIARY() : colors::AMBER()));
}

void EquityOrderEntry::show_order_status(const QString& msg, bool success) {
    // A placement outcome (or a cancellation) is the signal that the in-flight
    // order is resolved — release the send lock here rather than on a timer.
    set_send_locked(false);
    status_label_->setText(msg);
    status_label_->setStyleSheet(QString("color: %1;").arg(success ? colors::POSITIVE() : colors::NEGATIVE()));
    QTimer::singleShot(5000, status_label_, [this]() { status_label_->clear(); });
}

void EquityOrderEntry::set_limit_price(double price) {
    if (!price_edit_ || price <= 0.0)
        return;
    // A depth-ladder click is only meaningful as a LIMIT: on MKT the price box is
    // disabled and the value would be both invisible and unused.
    if (active_type_ != 1 && active_type_ != 3)
        set_order_type(1); // LMT
    price_edit_->setText(QString::number(price, 'f', 2));
    update_cost_preview();
    if (status_label_) {
        status_label_->setText(tr("Limit price %1 taken from market depth").arg(price, 0, 'f', 2));
        status_label_->setStyleSheet(QString("color: %1;").arg(colors::TEXT_SECONDARY()));
    }
}

void EquityOrderEntry::focus_quantity() {
    if (!qty_edit_)
        return;
    qty_edit_->setFocus(Qt::ShortcutFocusReason);
    qty_edit_->selectAll();
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
        market_price_label_->setText(tr("LTP --"));
        return;
    }
    const QString sym = currency_symbol(current_currency_);
    market_price_label_->setText(price_is_stale_
                                     ? tr("LTP %1%2  \xe2\x9a\xa0 STALE").arg(sym).arg(current_price_, 0, 'f', 2)
                                     : tr("LTP %1%2").arg(sym).arg(current_price_, 0, 'f', 2));
}

// Owns the stale flag AND its styling; the stylesheet is only reparsed on the
// rare transition, never per tick.
void EquityOrderEntry::update_stale_state() {
    if (!market_price_label_)
        return;
    const bool stale = current_price_ > 0.0 && last_price_ms_ > 0 &&
                       (QDateTime::currentMSecsSinceEpoch() - last_price_ms_) > kStaleQuoteMs;
    if (stale == price_is_stale_)
        return; // no churn on the common path
    price_is_stale_ = stale;
    if (stale) {
        market_price_label_->setStyleSheet(QString("color: %1; font-weight:700;").arg(colors::WARNING()));
        market_price_label_->setToolTip(
            tr("This price has not updated for over %1s — the feed may be down or the market closed. "
               "Do not treat it as the live price.")
                .arg(kStaleQuoteMs / 1000));
    } else {
        market_price_label_->setStyleSheet(QString()); // back to the global sheet
        market_price_label_->setToolTip(tr("Last traded price"));
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
    const QString sym = currency_symbol(current_currency_);
    if (qty > 0 && price > 0)
        cost_label_->setText(tr("Est: %1%2").arg(sym).arg(qty * price, 0, 'f', 2));
    else
        cost_label_->setText(tr("Est: --"));
}

void EquityOrderEntry::set_broker_id(const QString& broker_id) {
    broker_id_ = broker_id;
}

void EquityOrderEntry::fetch_margin_async() {
    // Only fetch for live mode with a real broker
    if (is_paper_ || broker_id_.isEmpty())
        return;

    const double qty = qty_edit_->text().toDouble();
    if (qty <= 0)
        return;

    // Guard against concurrent fetches
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
    QPointer<EquityOrderEntry> self = this;

    (void)QtConcurrent::run([self, bid, order]() {
        auto* broker = trading::BrokerRegistry::instance().get(bid);
        if (!broker) {
            if (self)
                self->margin_fetching_ = false;
            return;
        }
        auto creds = broker->load_credentials();
        if (creds.api_key.isEmpty()) {
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
            [self, result]() {
                if (!self)
                    return;
                self->margin_fetching_ = false;
                if (result.success && result.data) {
                    const auto& m = *result.data;
                    const QString sym = currency_symbol(self->current_currency_);
                    self->margin_label_->setText(self->tr("Margin: %1%2").arg(sym).arg(m.total, 0, 'f', 2));
                    self->margin_label_->setStyleSheet(QString("color: %1; font-size: 10px;").arg(colors::AMBER()));
                    self->margin_label_->show();
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
    double lot = strat_lot_edit_->text().toDouble();
    if (lot <= 0)
        lot = 1;
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
        strategy_preview_->setText(tr("Enter expiry, ATM strike (and width where required)."));
        return;
    }

    QStringList lines;
    if (!strat.name.isEmpty())
        lines << strat.name;
    for (const auto& leg : strat.legs) {
        // SIDE qty SYMBOL @strike type
        lines << QString("%1 %2 %3 @%4 %5")
                     .arg(QString(trading::order_side_str(leg.side)).toUpper())
                     .arg(leg.quantity, 0, 'f', 0)
                     .arg(leg.symbol)
                     .arg(leg.strike, 0, 'f', 0)
                     .arg(leg.option_type);
    }

    QStringList metrics;
    if (strat.max_profit != 0.0)
        metrics << QString("Max P: %1").arg(strat.max_profit, 0, 'f', 2);
    if (strat.max_loss != 0.0)
        metrics << QString("Max L: %1").arg(strat.max_loss, 0, 'f', 2);
    if (strat.breakeven_lower != 0.0)
        metrics << QString("BE-: %1").arg(strat.breakeven_lower, 0, 'f', 2);
    if (strat.breakeven_upper != 0.0)
        metrics << QString("BE+: %1").arg(strat.breakeven_upper, 0, 'f', 2);
    if (!metrics.isEmpty())
        lines << metrics.join("  ");

    strategy_preview_->setText(lines.join("\n"));
}

void EquityOrderEntry::on_place_strategy() {
    if (submit_locked_)
        return; // a placement is already in flight
    trading::OptionsStrategy strat;
    if (!build_strategy(strat)) {
        status_label_->setText(tr("Select a strategy and enter valid expiry / strike / width"));
        status_label_->setStyleSheet(QString("color: %1;").arg(colors::NEGATIVE()));
        return;
    }
    set_send_locked(true); // released by show_order_status() on the result

    // Product type from profile-driven combo (same mapping as on_submit)
    trading::ProductType product = trading::ProductType::Intraday;
    const int prod_idx = product_combo_->currentIndex();
    if (!product_types_.isEmpty() && prod_idx >= 0 && prod_idx < product_types_.size())
        product = product_types_[prod_idx].value;

    trading::BasketOrderRequest basket = trading::OptionsStrategyBuilder::to_basket_order(strat, product, broker_id_);
    emit strategy_order_submitted(basket);
}

void EquityOrderEntry::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void EquityOrderEntry::retranslateUi() {
    if (title_label_)
        title_label_->setText(tr("ORDER ENTRY"));
    if (mode_label_)
        mode_label_->setText(is_paper_ ? tr("PAPER") : tr("LIVE"));

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
        strat_lot_title_->setText(tr("LOT SIZE"));
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
