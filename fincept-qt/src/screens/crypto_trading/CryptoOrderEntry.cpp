// CryptoOrderEntry.cpp — production order ticket.
//
// Layout (top → bottom):
//   1. Header strip       — title + paper/live badge
//   2. Account card       — Balance | Mark | Avail-after-order
//   3. BUY / SELL tabs    — full-width, color-coded
//   4. Type buttons       — Market / Limit / Stop / Stop-Limit
//   5. Quantity input     — large monospace, with unit suffix and 25/50/75/100 quick fills
//   6. Price + Stop rows  — auto-shown only for the order types that need them
//   7. Order breakdown    — Cost / Fee / Total / Receive / % of balance
//   8. Advanced (collapsible) — SL / TP
//   9. Futures (visible only when set_futures_mode(true)) — leverage spin + margin segmented control
//  10. Submit             — color-coded big button with computed subtitle
//  11. Status              — inline validation message
//
// Sizing principles:
//  • No fixed pixel heights on inputs/buttons — everything uses padding via QSS so
//    rows scale with the user's terminal font size and HiDPI multipliers.
//  • Right panel is ~290 px wide; layout is single-column with paired label/value
//    sub-rows for compact cost breakdown.
//  • Live cost preview runs on every keystroke (qty / price / type changes) and
//    feeds the submit-button subtitle so the user never has to compute anything.

#include "screens/crypto_trading/CryptoOrderEntry.h"

#include "core/logging/Logger.h"
#include "screens/crypto_trading/CryptoTypes.h"
#include "services/markets/FxRateService.h"
#include "trading/OrderSourceCapture.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"

#include <QComboBox>
#include <QDoubleValidator>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLocale>
#include <QSpinBox>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens::crypto {

namespace {

/// A PAPER order refused by the ticket's own checks never reaches the paper engine
/// (which audits everything it decides), so the refusal is audited here. Plain
/// English reason (audit lines are not translated); no ids or secrets on the line.
void coe_audit_paper_refusal(const QString& symbol, const QString& side, const QString& type, double qty,
                             const char* reason) {
    LOG_AUDIT("Orders", "Order rejected",
              {{"venue", "paper"},
               {"mode", "paper"},
               {"symbol", symbol},
               {"side", side},
               {"type", type},
               {"qty", qty},
               {"reason", reason},
               {"source", fincept::trading::capture_order_source(QStringLiteral("ticket"))}});
}

// Taker fee assumption used for the on-screen estimate AND for the % quick-fill
// sizing. Real fees come back from the exchange post-fill. Shared so the two
// can't drift: sizing 100 % against the bare balance produced an order whose
// total (notional + fee) always exceeded the balance, so the ticket
// immediately flagged "Insufficient balance for this order" — the 100 %
// button could never produce a submittable order.
constexpr double kAssumedFeeRate = 0.0007; // 7 bps / 0.07 %

QFrame* make_card(const QString& object_name = "cryptoOeCard") {
    auto* card = new QFrame;
    card->setObjectName(object_name);
    card->setFrameShape(QFrame::NoFrame);
    return card;
}

// One label-value row for the account/breakdown cards. Label is upper-cased
// dim text; value is bright primary. Returns the value label so callers can
// update it later.
QLabel* add_kv_row(QGridLayout* grid, int row, const QString& label_text,
                   const QString& value_obj_name = "cryptoOeKvValue") {
    auto* lbl = new QLabel(label_text);
    lbl->setObjectName("cryptoOeKvLabel");
    auto* val = new QLabel(QStringLiteral("--"));
    val->setObjectName(value_obj_name);
    val->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    grid->addWidget(lbl, row, 0, Qt::AlignLeft | Qt::AlignVCenter);
    grid->addWidget(val, row, 1, Qt::AlignRight | Qt::AlignVCenter);
    return val;
}

// Base symbol of a "BASE/QUOTE" pair, defaulting to the whole string if no slash.
QString base_of(const QString& pair) {
    const int slash = pair.indexOf('/');
    return slash > 0 ? pair.left(slash) : pair;
}

// Quote asset of a pair (USDT, USD, EUR, BTC, …); a contract's ":SETTLE" suffix
// is not part of it ("BTC/USDT:USDT" -> "USDT"). Empty when the text names none —
// never a guessed currency.
QString quote_of(const QString& pair) {
    return pair_quote_ccy(pair);
}

// Numeric-only validator that allows decimals; reused by every numeric input.
QDoubleValidator* make_decimal_validator(QObject* parent) {
    auto* v = new QDoubleValidator(0.0, 1e15, 8, parent);
    v->setNotation(QDoubleValidator::StandardNotation);
    v->setLocale(QLocale::c());
    return v;
}

void repolish(QWidget* w) {
    if (!w || !w->style())
        return;
    w->style()->unpolish(w);
    w->style()->polish(w);
}

} // namespace

CryptoOrderEntry::CryptoOrderEntry(QWidget* parent) : QWidget(parent) {
    setObjectName("cryptoOrderEntry");
    setMinimumWidth(240);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header ──────────────────────────────────────────────────────────────
    auto* header = new QWidget(this);
    header->setObjectName("cryptoOeHeader");
    auto* h_layout = new QHBoxLayout(header);
    h_layout->setContentsMargins(12, 6, 12, 6);
    h_layout->setSpacing(6);

    title_label_ = new QLabel(tr("ORDER ENTRY"));
    title_label_->setObjectName("cryptoOeTitle");
    h_layout->addWidget(title_label_);
    h_layout->addStretch();

    mode_label_ = new QLabel(tr("PAPER"));
    mode_label_->setObjectName("cryptoOeMode");
    mode_label_->setProperty("mode", "paper");
    h_layout->addWidget(mode_label_);

    root->addWidget(header);

    // ── Scrollable content ──────────────────────────────────────────────────
    auto* content = new QWidget(this);
    auto* form = new QVBoxLayout(content);
    form->setContentsMargins(10, 10, 10, 10);
    form->setSpacing(10);

    // ── 0. Market banner — the market an order goes to ─────────────────────
    // The pair as typed is not necessarily a market on this exchange ("UNI/USDT"
    // on Kraken is shown from UNI/USD). The banner always names the market, its
    // type and settlement; an order is only possible on an exact market, so a
    // substitute comes with a button to choose it explicitly.
    {
        market_label_ = new QLabel(tr("Market: checking…"));
        market_label_->setObjectName("cryptoOeStatus");
        market_label_->setProperty("severity", "info");
        market_label_->setWordWrap(true);
        market_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        market_label_->setAccessibleName(tr("Market the order goes to"));
        form->addWidget(market_label_);

        use_market_btn_ = new QPushButton;
        use_market_btn_->setObjectName("cryptoOePctBtn");
        use_market_btn_->setCursor(Qt::PointingHandCursor);
        use_market_btn_->setFocusPolicy(Qt::NoFocus);
        use_market_btn_->setVisible(false);
        connect(use_market_btn_, &QPushButton::clicked, this, [this]() {
            const QString sym = use_market_btn_->property("market").toString();
            if (!sym.isEmpty())
                emit market_choice_requested(sym);
        });
        form->addWidget(use_market_btn_);
    }

    // ── 1. Account card (Balance / Mark / Avail) ────────────────────────────
    {
        auto* card = make_card();
        auto* grid = new QGridLayout(card);
        grid->setContentsMargins(12, 10, 12, 10);
        grid->setHorizontalSpacing(12);
        grid->setVerticalSpacing(4);
        grid->setColumnStretch(0, 0);
        grid->setColumnStretch(1, 1);

        balance_label_ = add_kv_row(grid, 0, tr("BALANCE"), "cryptoOeKvValueAccent");
        market_price_label_ = add_kv_row(grid, 1, tr("MARK"));
        avail_label_ = add_kv_row(grid, 2, tr("AVAIL"), "cryptoOeKvValueDim");
        balance_title_ = qobject_cast<QLabel*>(grid->itemAtPosition(0, 0)->widget());
        mark_title_ = qobject_cast<QLabel*>(grid->itemAtPosition(1, 0)->widget());
        avail_title_ = qobject_cast<QLabel*>(grid->itemAtPosition(2, 0)->widget());

        balance_label_->setText(fincept::ui::formatting::placeholder()); // until the account's balance arrives
        market_price_label_->setText(QStringLiteral("--"));
        avail_label_->setText(QStringLiteral("--"));

        form->addWidget(card);
    }

    // ── 2. BUY / SELL tabs ──────────────────────────────────────────────────
    {
        auto* side_row = new QHBoxLayout;
        side_row->setSpacing(0);
        side_row->setContentsMargins(0, 0, 0, 0);

        buy_tab_ = new QPushButton(tr("BUY"));
        buy_tab_->setObjectName("cryptoBuyTab");
        buy_tab_->setProperty("active", true);
        buy_tab_->setCursor(Qt::PointingHandCursor);
        buy_tab_->setFocusPolicy(Qt::NoFocus);
        connect(buy_tab_, &QPushButton::clicked, this, [this]() { set_buy_side(true); });
        side_row->addWidget(buy_tab_, 1);

        sell_tab_ = new QPushButton(tr("SELL"));
        sell_tab_->setObjectName("cryptoSellTab");
        sell_tab_->setProperty("active", false);
        sell_tab_->setCursor(Qt::PointingHandCursor);
        sell_tab_->setFocusPolicy(Qt::NoFocus);
        connect(sell_tab_, &QPushButton::clicked, this, [this]() { set_buy_side(false); });
        side_row->addWidget(sell_tab_, 1);

        form->addLayout(side_row);
    }

    // ── 3. Order type segmented control ─────────────────────────────────────
    {
        auto* type_row = new QHBoxLayout;
        type_row->setSpacing(0);
        type_row->setContentsMargins(0, 0, 0, 0);
        // Slightly more readable than the previous 3-letter abbreviations.
        const QString type_labels[] = {tr("MARKET"), tr("LIMIT"), tr("STOP"), tr("STOP-LMT")};
        for (int i = 0; i < 4; ++i) {
            type_btns_[i] = new QPushButton(type_labels[i]);
            type_btns_[i]->setObjectName("cryptoOeTypeBtn");
            type_btns_[i]->setCursor(Qt::PointingHandCursor);
            type_btns_[i]->setFocusPolicy(Qt::NoFocus);
            if (i == 0)
                type_btns_[i]->setProperty("active", true);
            connect(type_btns_[i], &QPushButton::clicked, this, [this, i]() { set_order_type(i); });
            type_row->addWidget(type_btns_[i], 1);
        }
        form->addLayout(type_row);
    }

    // ── 4. Quantity input + 25/50/75/100% quick fills ───────────────────────
    {
        qty_title_ = new QLabel(tr("QUANTITY"));
        qty_title_->setObjectName("cryptoOeLabel");
        form->addWidget(qty_title_);

        // Quantity field with the base-asset suffix shown in a side label so
        // users always know whether they're sizing in BTC or USDT.
        auto* qty_wrap = new QWidget;
        qty_wrap->setObjectName("cryptoOeInputWrap");
        auto* qty_h = new QHBoxLayout(qty_wrap);
        qty_h->setContentsMargins(0, 0, 0, 0);
        qty_h->setSpacing(0);

        qty_edit_ = new QLineEdit;
        qty_edit_->setObjectName("cryptoOeInput");
        qty_edit_->setPlaceholderText(QStringLiteral("0.00"));
        qty_edit_->setValidator(make_decimal_validator(this));
        qty_edit_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        connect(qty_edit_, &QLineEdit::textChanged, this, [this]() {
            submit_error_pinned_ = false; // the ticket changed: the last press's message no longer applies
            update_cost_preview();
        });
        qty_h->addWidget(qty_edit_, 1);

        auto* unit = new QLabel(base_of(current_symbol_));
        unit->setObjectName("cryptoOeUnit");
        unit->setMinimumWidth(40);
        unit->setAlignment(Qt::AlignCenter);
        qty_h->addWidget(unit);

        form->addWidget(qty_wrap);

        // Quick % fills — bigger touch targets than the old 18 px buttons.
        auto* pct_row = new QHBoxLayout;
        pct_row->setSpacing(4);
        for (int pct : {25, 50, 75, 100}) {
            auto* btn = new QPushButton(QString::number(pct) + QStringLiteral("%"));
            btn->setObjectName("cryptoOePctBtn");
            btn->setCursor(Qt::PointingHandCursor);
            btn->setFocusPolicy(Qt::NoFocus);
            connect(btn, &QPushButton::clicked, this, [this, pct]() { on_pct_clicked(pct); });
            pct_row->addWidget(btn, 1);
        }
        form->addLayout(pct_row);

        // Tag the suffix label so set_symbol() can find and update it.
        // (A no-op `connect(this, &CryptoOrderEntry::destroyed, unit, []{})`
        // used to sit here; it did nothing and has been removed.)
        unit->setProperty("ftRoleUnit", true);
    }

    // ── 5. Price (Limit / Stop-Limit) ───────────────────────────────────────
    {
        price_row_ = new QWidget;
        auto* v = new QVBoxLayout(price_row_);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(4);

        price_title_ = new QLabel(tr("LIMIT PRICE"));
        price_title_->setObjectName("cryptoOeLabel");
        v->addWidget(price_title_);

        auto* wrap = new QWidget;
        auto* h = new QHBoxLayout(wrap);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(0);

        price_edit_ = new QLineEdit;
        price_edit_->setObjectName("cryptoOeInput");
        price_edit_->setPlaceholderText(QStringLiteral("0.00"));
        price_edit_->setValidator(make_decimal_validator(this));
        price_edit_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        connect(price_edit_, &QLineEdit::textChanged, this, [this]() {
            submit_error_pinned_ = false;
            update_cost_preview();
        });
        h->addWidget(price_edit_, 1);

        auto* unit = new QLabel(quote_of(current_symbol_));
        unit->setObjectName("cryptoOeUnit");
        unit->setMinimumWidth(40);
        unit->setAlignment(Qt::AlignCenter);
        unit->setProperty("ftRoleUnit", "quote");
        h->addWidget(unit);

        v->addWidget(wrap);
        form->addWidget(price_row_);
        price_row_->setVisible(false); // shown only when type is Limit / Stop-Limit
    }

    // ── 6. Stop trigger price (Stop / Stop-Limit) ───────────────────────────
    {
        stop_row_ = new QWidget;
        auto* v = new QVBoxLayout(stop_row_);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(4);

        stop_title_ = new QLabel(tr("STOP PRICE"));
        stop_title_->setObjectName("cryptoOeLabel");
        v->addWidget(stop_title_);

        auto* wrap = new QWidget;
        auto* h = new QHBoxLayout(wrap);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(0);

        stop_price_edit_ = new QLineEdit;
        stop_price_edit_->setObjectName("cryptoOeInput");
        stop_price_edit_->setPlaceholderText(QStringLiteral("0.00"));
        stop_price_edit_->setValidator(make_decimal_validator(this));
        stop_price_edit_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        h->addWidget(stop_price_edit_, 1);

        auto* unit = new QLabel(quote_of(current_symbol_));
        unit->setObjectName("cryptoOeUnit");
        unit->setMinimumWidth(40);
        unit->setAlignment(Qt::AlignCenter);
        unit->setProperty("ftRoleUnit", "quote");
        h->addWidget(unit);

        v->addWidget(wrap);
        form->addWidget(stop_row_);
        stop_row_->setVisible(false);
    }

    // ── 7. Order breakdown card ────────────────────────────────────────────
    {
        auto* card = make_card("cryptoOeBreakdown");
        auto* grid = new QGridLayout(card);
        grid->setContentsMargins(12, 8, 12, 8);
        grid->setHorizontalSpacing(12);
        grid->setVerticalSpacing(3);
        grid->setColumnStretch(0, 0);
        grid->setColumnStretch(1, 1);

        cost_label_ = add_kv_row(grid, 0, tr("ORDER VALUE"));
        fee_label_ = add_kv_row(grid, 1, tr("FEE EST"), "cryptoOeKvValueDim");
        total_label_ = add_kv_row(grid, 2, tr("TOTAL"), "cryptoOeKvValueAccent");
        recv_label_ = add_kv_row(grid, 3, tr("YOU RECEIVE"), "cryptoOeKvValueDim");
        pct_used_label_ = add_kv_row(grid, 4, tr("% OF BALANCE"), "cryptoOeKvValueDim");
        cost_title_ = qobject_cast<QLabel*>(grid->itemAtPosition(0, 0)->widget());
        fee_title_ = qobject_cast<QLabel*>(grid->itemAtPosition(1, 0)->widget());
        total_title_ = qobject_cast<QLabel*>(grid->itemAtPosition(2, 0)->widget());
        recv_title_ = qobject_cast<QLabel*>(grid->itemAtPosition(3, 0)->widget());
        pct_used_title_ = qobject_cast<QLabel*>(grid->itemAtPosition(4, 0)->widget());

        form->addWidget(card);
    }

    // ── 8. Advanced toggle (SL / TP) ────────────────────────────────────────
    advanced_toggle_ = new QPushButton(tr("▾  Advanced  (SL / TP)"));
    advanced_toggle_->setObjectName("cryptoAdvToggle");
    advanced_toggle_->setCursor(Qt::PointingHandCursor);
    advanced_toggle_->setFocusPolicy(Qt::NoFocus);
    form->addWidget(advanced_toggle_);

    advanced_section_ = new QWidget(this);
    advanced_section_->setVisible(false);
    {
        auto* adv = new QVBoxLayout(advanced_section_);
        adv->setContentsMargins(0, 0, 0, 0);
        adv->setSpacing(6);

        auto add_pair = [&](const QString& label, QLineEdit*& edit, QLabel*& title_out, const QString& placeholder) {
            auto* lbl = new QLabel(label);
            lbl->setObjectName("cryptoOeLabel");
            adv->addWidget(lbl);
            title_out = lbl;
            edit = new QLineEdit;
            edit->setObjectName("cryptoOeInput");
            edit->setPlaceholderText(placeholder);
            edit->setValidator(make_decimal_validator(this));
            edit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            adv->addWidget(edit);
        };
        add_pair(tr("STOP LOSS"), sl_edit_, sl_title_, tr("Trigger price"));
        add_pair(tr("TAKE PROFIT"), tp_edit_, tp_title_, tr("Trigger price"));
        // LIVE: why SL / TP cannot be attached on this exchange / market.
        sltp_note_ = new QLabel;
        sltp_note_->setObjectName("cryptoOeStatus");
        sltp_note_->setProperty("severity", "warning");
        sltp_note_->setWordWrap(true);
        sltp_note_->setVisible(false);
        adv->addWidget(sltp_note_);
        for (QLineEdit* e : {sl_edit_, tp_edit_}) {
            connect(e, &QLineEdit::textChanged, this, [this]() {
                submit_error_pinned_ = false;
                update_cost_preview();
            });
        }
    }
    form->addWidget(advanced_section_);

    connect(advanced_toggle_, &QPushButton::clicked, this, [this]() {
        const bool show = !advanced_section_->isVisible();
        advanced_section_->setVisible(show);
        advanced_toggle_->setText(show ? tr("▴  Advanced  (SL / TP)") : tr("▾  Advanced  (SL / TP)"));
    });

    // ── 9. Futures controls (leverage + margin mode) ────────────────────────
    futures_section_ = new QWidget(this);
    futures_section_->setVisible(false);
    {
        auto* layout = new QVBoxLayout(futures_section_);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(6);

        leverage_title_ = new QLabel(tr("LEVERAGE"));
        leverage_title_->setObjectName("cryptoOeLabel");
        layout->addWidget(leverage_title_);

        leverage_spin_ = new QSpinBox;
        leverage_spin_->setObjectName("cryptoOeSpinBox");
        leverage_spin_->setRange(1, 125);
        leverage_spin_->setValue(1);
        leverage_spin_->setSuffix(QStringLiteral("x"));
        leverage_spin_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        layout->addWidget(leverage_spin_);

        margin_title_ = new QLabel(tr("MARGIN MODE"));
        margin_title_->setObjectName("cryptoOeLabel");
        layout->addWidget(margin_title_);

        margin_mode_combo_ = new QComboBox;
        margin_mode_combo_->setObjectName("cryptoOeCombo");
        margin_mode_combo_->addItem(tr("Cross"), QStringLiteral("cross"));
        margin_mode_combo_->addItem(tr("Isolated"), QStringLiteral("isolated"));
        layout->addWidget(margin_mode_combo_);

        reduce_only_check_ = new QCheckBox(tr("Reduce only"));
        reduce_only_check_->setObjectName("cryptoOeCheck");
        layout->addWidget(reduce_only_check_);

        // The exchange's own setting, and an explicit APPLY: turning the spin box
        // or picking a mode used to change the real account at once (a mouse
        // wheel over the box re-levered it). Now nothing reaches the exchange
        // until APPLY, and the screen confirms that first.
        lev_current_label_ = new QLabel(tr("Account: leverage / margin mode unknown"));
        lev_current_label_->setObjectName("cryptoOeSubmitSubtitle");
        lev_current_label_->setWordWrap(true);
        layout->addWidget(lev_current_label_);

        apply_lev_btn_ = new QPushButton(tr("APPLY LEVERAGE / MARGIN TO ACCOUNT"));
        apply_lev_btn_->setObjectName("cryptoOePctBtn");
        apply_lev_btn_->setProperty("fincept_agent_risk", "money"); // account leverage / margin mode
        apply_lev_btn_->setCursor(Qt::PointingHandCursor);
        apply_lev_btn_->setFocusPolicy(Qt::NoFocus);
        apply_lev_btn_->setAccessibleName(tr("Apply leverage and margin mode to the exchange account"));
        layout->addWidget(apply_lev_btn_);
    }
    form->addWidget(futures_section_);

    // Leverage / margin edits only drive the preview (and, where the exchange
    // takes them per order, the next order). The account changes on APPLY only.
    connect(leverage_spin_, &QSpinBox::valueChanged, this, [this](int) { update_cost_preview(); });
    connect(margin_mode_combo_, &QComboBox::currentIndexChanged, this, [this](int) { update_cost_preview(); });
    connect(apply_lev_btn_, &QPushButton::clicked, this,
            [this]() { emit apply_leverage_requested(leverage(), margin_mode()); });

    // ── 10. Submit button + computed subtitle ───────────────────────────────
    submit_btn_ = new QPushButton(tr("BUY  %1").arg(current_symbol_));
    submit_btn_->setObjectName("cryptoBuySubmit");
    submit_btn_->setProperty("fincept_agent_risk", "order");
    submit_btn_->setCursor(Qt::PointingHandCursor);
    submit_btn_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return));
    submit_btn_->setToolTip(tr("Submit order  (⌘/Ctrl+Enter)"));
    connect(submit_btn_, &QPushButton::clicked, this, &CryptoOrderEntry::on_submit);
    form->addWidget(submit_btn_);

    submit_subtitle_ = new QLabel(tr("Enter a quantity to preview"));
    submit_subtitle_->setObjectName("cryptoOeSubmitSubtitle");
    submit_subtitle_->setAlignment(Qt::AlignCenter);
    submit_subtitle_->setWordWrap(true);
    form->addWidget(submit_subtitle_);

    // ── 11. Inline status / validation message ──────────────────────────────
    status_label_ = new QLabel;
    status_label_->setObjectName("cryptoOeStatus");
    status_label_->setWordWrap(true);
    status_label_->setVisible(false);
    form->addWidget(status_label_);

    form->addStretch();
    root->addWidget(content, 1);

    // ── Accessibility ───────────────────────────────────────────────────────
    // Every field that sizes or prices a real order gets a spoken name, and
    // the ticket gets an explicit tab order: side → type → quantity → price →
    // stop → SL → TP → submit. Without setTabOrder the focus chain follows
    // construction order, which interleaves the collapsed advanced/futures
    // sections and drops the user into hidden widgets.
    buy_tab_->setAccessibleName(tr("Buy side"));
    sell_tab_->setAccessibleName(tr("Sell side"));
    for (int i = 0; i < 4; ++i) {
        if (type_btns_[i])
            type_btns_[i]->setAccessibleName(tr("Order type: %1").arg(type_btns_[i]->text()));
    }
    qty_edit_->setAccessibleName(tr("Order quantity"));
    qty_edit_->setAccessibleDescription(tr("Quantity in the base asset of the selected pair"));
    price_edit_->setAccessibleName(tr("Limit price"));
    stop_price_edit_->setAccessibleName(tr("Stop trigger price"));
    sl_edit_->setAccessibleName(tr("Stop loss trigger price"));
    tp_edit_->setAccessibleName(tr("Take profit trigger price"));
    submit_btn_->setAccessibleName(tr("Submit order"));
    balance_label_->setAccessibleName(tr("Account balance"));
    market_price_label_->setAccessibleName(tr("Mark price"));
    avail_label_->setAccessibleName(tr("Balance available after this order"));
    mode_label_->setAccessibleName(tr("Trading mode, paper or live"));
    status_label_->setAccessibleName(tr("Order validation message"));
    submit_subtitle_->setAccessibleName(tr("Order preview"));
    if (leverage_spin_)
        leverage_spin_->setAccessibleName(tr("Leverage multiplier"));
    if (margin_mode_combo_)
        margin_mode_combo_->setAccessibleName(tr("Margin mode"));
    if (reduce_only_check_)
        reduce_only_check_->setAccessibleName(tr("Reduce only — never increase position size"));

    // Side/type buttons and the advanced toggle are Qt::NoFocus by design
    // (they're click targets, not stops in the keyboard chain), so only the
    // focusable fields are linked here.
    setTabOrder(qty_edit_, price_edit_);
    setTabOrder(price_edit_, stop_price_edit_);
    setTabOrder(stop_price_edit_, sl_edit_);
    setTabOrder(sl_edit_, tp_edit_);
    setTabOrder(tp_edit_, submit_btn_);

    // A currency / number-format change re-renders the held balance, mark and
    // breakdown (no fetch).
    fincept::ui::formatting::on_format_changed(this, [this]() {
        render_account_card();
        update_cost_preview();
    });

    render_account_card();
    update_cost_preview();
}

QString CryptoOrderEntry::price_pair() const {
    return market_.valid() ? market_.symbol : current_symbol_;
}

QString CryptoOrderEntry::quote_ccy() const {
    if (market_.valid() && !market_.quote.trimmed().isEmpty())
        return market_.quote.trimmed().toUpper();
    return quote_of(current_symbol_);
}

QString CryptoOrderEntry::balance_ccy() const {
    return balance_ccy_.isEmpty() ? quote_ccy() : balance_ccy_;
}

void CryptoOrderEntry::render_account_card() {
    namespace fmt = fincept::ui::formatting;
    if (balance_label_)
        balance_label_->setText(has_balance_ ? fmt::money(balance_, balance_ccy()) : fmt::placeholder());
    if (market_price_label_)
        market_price_label_->setText(current_price_ > 0 ? pair_price_with_unit(current_price_, price_pair())
                                                        : QStringLiteral("--"));
    // The limit / stop price boxes are in the market's quote asset: their unit
    // labels follow it (and the user's symbol style). Rebound only when the
    // quote asset changes — a second binding replaces the first.
    const QString quote = quote_ccy();
    if (quote.isEmpty())
        return;
    for (auto* lbl : findChildren<QLabel*>()) {
        if (lbl->property("ftRoleUnit").toString() != QLatin1String("quote"))
            continue;
        if (lbl->property("ftBoundCcy").toString() == quote)
            continue;
        lbl->setProperty("ftBoundCcy", quote);
        fmt::bind_currency_label(lbl, QStringLiteral("%1"), quote);
    }
}

void CryptoOrderEntry::set_buy_side(bool is_buy) {
    if (is_buy == is_buy_side_)
        return;
    is_buy_side_ = is_buy;
    submit_error_pinned_ = false;
    buy_tab_->setProperty("active", is_buy);
    sell_tab_->setProperty("active", !is_buy);

    if (!submit_busy_)
        submit_btn_->setText(is_buy ? tr("BUY  %1").arg(current_symbol_) : tr("SELL  %1").arg(current_symbol_));
    submit_btn_->setObjectName(is_buy ? "cryptoBuySubmit" : "cryptoSellSubmit");

    repolish(buy_tab_);
    repolish(sell_tab_);
    repolish(submit_btn_);
    update_cost_preview();
}

void CryptoOrderEntry::set_order_type(int idx) {
    const int prev = active_type_;
    if (idx == active_type_) {
        // Defensive: still recompute visibility / preview in case state drifted.
        if (price_row_)
            price_row_->setVisible(idx == 1 || idx == 3);
        if (stop_row_)
            stop_row_->setVisible(idx == 2 || idx == 3);
        if (price_edit_)
            price_edit_->setEnabled(idx == 1 || idx == 3);
        if (stop_price_edit_)
            stop_price_edit_->setEnabled(idx == 2 || idx == 3);
        update_cost_preview();
        return;
    }
    active_type_ = idx;
    submit_error_pinned_ = false;
    for (int i = 0; i < 4; ++i)
        type_btns_[i]->setProperty("active", i == idx);

    if (prev >= 0 && prev < 4)
        repolish(type_btns_[prev]);
    repolish(type_btns_[idx]);

    if (price_row_)
        price_row_->setVisible(idx == 1 || idx == 3);
    if (stop_row_)
        stop_row_->setVisible(idx == 2 || idx == 3);
    if (price_edit_)
        price_edit_->setEnabled(idx == 1 || idx == 3);
    if (stop_price_edit_)
        stop_price_edit_->setEnabled(idx == 2 || idx == 3);

    update_cost_preview();
}

void CryptoOrderEntry::set_balance(double balance, const QString& ccy) {
    balance_ = balance;
    balance_ccy_ = ccy.trimmed();
    has_balance_ = true;
    render_account_card();
    update_cost_preview();
}

void CryptoOrderEntry::set_current_price(double price) {
    current_price_ = price;
    // In the market's quote asset, decimals scaled to the magnitude: a fixed
    // 2 dp rendered the MARK of every sub-cent pair as "0.00", which is both
    // useless and dangerous next to a live BUY button.
    market_price_label_->setText(price > 0 ? pair_price_with_unit(price, price_pair()) : QStringLiteral("--"));
    update_cost_preview();
}

void CryptoOrderEntry::set_limit_price(double price) {
    if (price <= 0.0 || !price_edit_)
        return;
    if (active_type_ == 0)
        set_order_type(1); // MARKET -> LIMIT
    else if (active_type_ == 2)
        set_order_type(3); // STOP -> STOP-LMT
    price_edit_->setText(pair_price_edit_text(price, price_pair()));
}

void CryptoOrderEntry::set_mode(bool is_paper) {
    is_paper_ = is_paper;
    mode_label_->setText(is_paper ? tr("PAPER") : tr("LIVE"));
    mode_label_->setProperty("mode", is_paper ? "paper" : "live");
    repolish(mode_label_);
    apply_capabilities();
    update_cost_preview();
}

void CryptoOrderEntry::set_venue(const QString& exchange_id) {
    venue_ = exchange_id;
    update_cost_preview();
}

QString CryptoOrderEntry::margin_mode() const {
    return margin_mode_combo_ ? margin_mode_combo_->currentData().toString() : QStringLiteral("cross");
}

int CryptoOrderEntry::leverage() const {
    return leverage_spin_ ? leverage_spin_->value() : 1;
}

void CryptoOrderEntry::set_account_leverage(int lev, const QString& mode) {
    account_leverage_ = lev;
    account_margin_mode_ = mode.toLower();
    if (leverage_spin_ && lev > 0) {
        const QSignalBlocker block(leverage_spin_);
        leverage_spin_->setValue(std::clamp(lev, leverage_spin_->minimum(), leverage_spin_->maximum()));
    }
    if (margin_mode_combo_ && !account_margin_mode_.isEmpty()) {
        const int idx = margin_mode_combo_->findData(account_margin_mode_);
        if (idx >= 0) {
            const QSignalBlocker block(margin_mode_combo_);
            margin_mode_combo_->setCurrentIndex(idx);
        }
    }
    if (lev_current_label_) {
        const QString lev_txt = lev > 0 ? QStringLiteral("%1x").arg(lev) : tr("unknown");
        const QString mode_txt = account_margin_mode_.isEmpty() ? tr("unknown") : account_margin_mode_;
        lev_current_label_->setText(tr("Account now: leverage %1, margin %2").arg(lev_txt, mode_txt));
    }
    update_cost_preview();
}

void CryptoOrderEntry::show_order_refusal(const QString& message) {
    show_submit_error(message, nullptr);
}

void CryptoOrderEntry::show_notice(const QString& message, const QString& severity) {
    if (!status_label_ || message.isEmpty())
        return;
    status_label_->setText(message);
    status_label_->setProperty("severity", severity);
    status_label_->setVisible(true);
    submit_error_pinned_ = true; // survives price ticks until the ticket is edited
    repolish(status_label_);
}

void CryptoOrderEntry::set_market(const ResolvedMarket& market) {
    market_ = market;
    update_market_banner();
    apply_capabilities();
    render_account_card(); // a substitute market can be quoted in another asset
    update_cost_preview();
}

void CryptoOrderEntry::update_market_banner() {
    if (!market_label_)
        return;
    QString text;
    QString severity = QStringLiteral("info");
    QString choice;
    if (!market_.loaded) {
        text = tr("Market: checking %1…").arg(current_symbol_);
    } else if (!market_.valid()) {
        severity = QStringLiteral("warning");
        text = tr("No market for %1: %2").arg(current_symbol_, market_.reason);
        if (!market_.alternatives.isEmpty()) {
            text += QLatin1Char('\n') + tr("Markets for %1 there: %2")
                                            .arg(current_symbol_.section(QLatin1Char('/'), 0, 0),
                                                 market_.alternatives.mid(0, 6).join(QStringLiteral(", ")));
            choice = market_.alternatives.first();
        }
    } else if (market_.match == QLatin1String("exact")) {
        text = tr("Market: %1").arg(market_.describe());
        if (!market_.tradable) {
            severity = QStringLiteral("warning");
            text += QLatin1Char('\n') + market_.reason;
        } else if (!market_.orders && !is_paper_) {
            severity = QStringLiteral("warning");
            text += QLatin1Char('\n') + market_.orders_reason;
        }
    } else {
        // Shown from a related market; an order needs the user's explicit choice.
        severity = QStringLiteral("warning");
        text = tr("Showing %1 — %2. Orders are only sent to an exact market.").arg(market_.describe(), market_.reason);
        choice = market_.symbol;
    }
    market_label_->setText(text);
    market_label_->setProperty("severity", severity);
    repolish(market_label_);
    if (use_market_btn_) {
        use_market_btn_->setVisible(!choice.isEmpty());
        use_market_btn_->setText(tr("TRADE %1 INSTEAD").arg(choice));
        use_market_btn_->setProperty("market", choice);
    }
}

void CryptoOrderEntry::apply_capabilities() {
    // PAPER: the simulator handles every type locally. LIVE: only what the
    // exchange takes in a form that was checked; the rest is disabled with why.
    const bool gate = !is_paper_ && market_.loaded && market_.valid();
    auto gate_btn = [&](int idx, bool ok, const QString& why) {
        if (!type_btns_[idx])
            return;
        type_btns_[idx]->setEnabled(!gate || ok);
        type_btns_[idx]->setToolTip(gate && !ok ? why : QString());
    };
    gate_btn(2, market_.stop, market_.stop_reason);
    gate_btn(3, market_.stop_limit, market_.stop_limit_reason);
    const bool sltp_ok = !gate || market_.sl_tp;
    for (QLineEdit* e : {sl_edit_, tp_edit_}) {
        if (e)
            e->setEnabled(sltp_ok);
    }
    if (sltp_note_) {
        QString note;
        if (gate && !market_.sl_tp)
            note = market_.sl_tp_reason;
        else if (gate && market_.sl_tp && market_.sl_tp_types.size() == 1)
            note = tr("On this market SL / TP can be attached to %1 orders only.")
                       .arg(market_.sl_tp_types.first().toUpper());
        sltp_note_->setText(note);
        sltp_note_->setVisible(!note.isEmpty());
    }
    if (reduce_only_check_) {
        reduce_only_check_->setEnabled(!gate || market_.reduce_only);
        if (gate && !market_.reduce_only)
            reduce_only_check_->setChecked(false);
    }
    if (apply_lev_btn_)
        apply_lev_btn_->setVisible(!is_paper_ && (market_.leverage || market_.margin_mode));
    if (lev_current_label_)
        lev_current_label_->setVisible(!is_paper_);
    update_market_banner();
}

QString CryptoOrderEntry::live_refusal(int order_type, bool has_sl_tp) const {
    if (!market_.loaded)
        return tr("⚠ Still checking which market %1 is on this exchange — try again in a moment").arg(current_symbol_);
    if (!market_.valid()) {
        QString msg = tr("⚠ %1 is not a market here: %2").arg(current_symbol_, market_.reason);
        if (!market_.alternatives.isEmpty())
            msg += QLatin1Char('\n') + tr("Choose one of: %1").arg(market_.alternatives.mid(0, 6).join(", "));
        return msg;
    }
    if (market_.match != QLatin1String("exact"))
        return tr("⚠ %1 is shown from %2. Orders only go to an exact market — press \"TRADE %2 INSTEAD\" to "
                  "trade it.")
            .arg(current_symbol_, market_.symbol);
    if (!market_.tradable)
        return tr("⚠ %1").arg(market_.reason);
    if (market_.stale)
        return tr("⚠ The market list for this exchange is out of date and could not be refreshed; no order is "
                  "sent until it can be.");
    if (!market_.orders)
        return tr("⚠ %1").arg(market_.orders_reason);
    if (order_type == 2 && !market_.stop)
        return tr("⚠ %1").arg(market_.stop_reason);
    if (order_type == 3 && !market_.stop_limit)
        return tr("⚠ %1").arg(market_.stop_limit_reason);
    if (has_sl_tp) {
        if (!market_.sl_tp)
            return tr("⚠ %1").arg(market_.sl_tp_reason);
        static const char* kTypes[] = {"market", "limit", "stop", "stop_limit"};
        if (!market_.sl_tp_types.contains(QString::fromLatin1(kTypes[order_type])))
            return tr("⚠ On %1 stop-loss / take-profit can only be attached to %2 orders")
                .arg(market_.symbol, market_.sl_tp_types.join(QStringLiteral(" / ")).toUpper());
    }
    return {};
}

void CryptoOrderEntry::set_symbol(const QString& symbol) {
    // Quantity, prices and SL/TP are denominated in the previous pair — a BTC
    // limit price or stop left in the box is meaningless (and dangerous) on ETH.
    const bool symbol_changed = symbol != current_symbol_;
    current_symbol_ = symbol;
    if (symbol_changed) {
        clear_ticket_fields();
        set_market_limits(0.0, 0.0); // the old pair's minimums; the screen fetches the new ones
        // The old pair's market and capabilities no longer apply: "checking"
        // until the screen delivers the new resolution (LIVE refuses meanwhile).
        market_ = ResolvedMarket{};
        market_.requested = symbol;
        account_leverage_ = 0;
        account_margin_mode_.clear();
        if (lev_current_label_)
            lev_current_label_->setText(tr("Account: leverage / margin mode unknown"));
        apply_capabilities();
    }
    if (!submit_busy_)
        submit_btn_->setText(is_buy_side_ ? tr("BUY  %1").arg(symbol) : tr("SELL  %1").arg(symbol));

    // Update unit suffix labels by looking up the children tagged with the
    // ftRoleUnit dynamic property: the base asset on the quantity box here, the
    // quote asset on the price / stop boxes in render_account_card() (bound to
    // the user's currency style).
    const QString base = base_of(symbol);
    for (auto* lbl : findChildren<QLabel*>()) {
        const auto role = lbl->property("ftRoleUnit");
        if (!role.isValid() || role.toString() == QLatin1String("quote"))
            continue;
        lbl->setText(base);
    }
    render_account_card();
    update_cost_preview();
}

void CryptoOrderEntry::set_submit_busy(bool busy, const QString& label) {
    submit_busy_ = busy;
    if (!submit_btn_)
        return;
    submit_btn_->setEnabled(!busy);
    if (busy) {
        submit_btn_->setText(label.isEmpty() ? tr("SENDING…") : label);
    } else {
        submit_btn_->setText(is_buy_side_ ? tr("BUY  %1").arg(current_symbol_) : tr("SELL  %1").arg(current_symbol_));
    }
}

void CryptoOrderEntry::on_submit() {
    // In-flight guard: a live order POST is dispatched to a worker thread and
    // the receiver returns immediately, so without this a double-click or a
    // held Ctrl+Enter would fire a second real exchange order. set_submit_busy
    // disables the button, but the keyboard-shortcut / scripted path can still
    // re-enter the slot, so gate here too.
    if (submit_busy_)
        return;

    const QString side = is_buy_side_ ? "buy" : "sell";
    static const char* type_map[] = {"market", "limit", "stop", "stop_limit"};
    const QString order_type = type_map[active_type_];

    const double qty = qty_edit_->text().toDouble();
    const auto audit_refusal = [&](const char* reason) {
        if (is_paper_)
            coe_audit_paper_refusal(current_symbol_, side, order_type, qty, reason);
    };
    if (qty <= 0) {
        audit_refusal("invalid quantity");
        status_label_->setText(tr("⚠ Enter a valid quantity"));
        status_label_->setProperty("severity", "error");
        status_label_->setVisible(true);
        repolish(status_label_);
        qty_edit_->setProperty("invalid", true);
        repolish(qty_edit_);
        qty_edit_->setFocus();
        return;
    }
    qty_edit_->setProperty("invalid", false);
    repolish(qty_edit_);

    if ((active_type_ == 1 || active_type_ == 3) && price_edit_->text().toDouble() <= 0) {
        audit_refusal("limit price required");
        status_label_->setText(tr("⚠ Limit price required"));
        status_label_->setProperty("severity", "error");
        status_label_->setVisible(true);
        submit_error_pinned_ = true;
        repolish(status_label_);
        price_edit_->setFocus();
        return;
    }
    if ((active_type_ == 2 || active_type_ == 3) && stop_price_edit_->text().toDouble() <= 0) {
        audit_refusal("stop trigger price required");
        status_label_->setText(tr("⚠ Stop trigger price required"));
        status_label_->setProperty("severity", "error");
        status_label_->setVisible(true);
        submit_error_pinned_ = true;
        repolish(status_label_);
        stop_price_edit_->setFocus();
        return;
    }

    // LIVE: only an exact, supported market, order type and SL / TP leave the
    // ticket; otherwise say why here (the message stays until the ticket is
    // edited — update_cost_preview() runs on every tick and must not hide it).
    if (!is_paper_) {
        const bool adv_open = advanced_section_ && !advanced_section_->isHidden();
        const bool has_sl_tp =
            adv_open && (sl_edit_->text().toDouble() > 0.0 || tp_edit_->text().toDouble() > 0.0);
        const QString why = live_refusal(active_type_, has_sl_tp);
        if (!why.isEmpty()) {
            show_submit_error(why, nullptr);
            return;
        }
    }

    // The exchange's own minimums (fetch_markets limits) — an order below them is
    // rejected by the exchange, so say why here instead of sending it. A small
    // relative tolerance keeps a quantity typed exactly at the minimum valid.
    const QString market = limits_market_.isEmpty() ? current_symbol_ : limits_market_;
    if (min_amount_ > 0.0 && qty < min_amount_ * (1.0 - 1e-9)) {
        audit_refusal("quantity below the exchange minimum");
        show_submit_error(tr("⚠ Quantity %1 is below the exchange minimum of %2 for %3")
                              .arg(format_size(qty), format_size(min_amount_), market),
                          qty_edit_);
        return;
    }
    if (min_cost_ > 0.0) {
        // Notional at the price the order would execute at: the limit price, the
        // stop trigger for a plain STOP, otherwise the current price.
        double ref_px = current_price_;
        if (active_type_ == 1 || active_type_ == 3)
            ref_px = price_edit_->text().toDouble();
        else if (active_type_ == 2)
            ref_px = stop_price_edit_->text().toDouble();
        const QString ccy = limits_cost_ccy_.isEmpty() ? quote_ccy() : limits_cost_ccy_;
        if (ref_px > 0.0 && qty * ref_px < min_cost_ * (1.0 - 1e-9)) {
            audit_refusal("order value below the exchange minimum");
            namespace fmt = fincept::ui::formatting;
            show_submit_error(tr("⚠ Order value %1 is below the exchange minimum of %2 for %3")
                                  .arg(fmt::money_precise(qty * ref_px, ccy), fmt::money_precise(min_cost_, ccy), market),
                              qty_edit_);
            return;
        }
    }

    submit_error_pinned_ = false;
    status_label_->setVisible(false);

    // Only read the price / trigger boxes for the order types that show them.
    // Both fields keep their text when hidden, so a limit price typed earlier
    // used to ride along on a MARKET or STOP order (and a stale trigger on a
    // LIMIT) straight into the exchange request — place_exchange_order sends
    // any price > 0 as the order price.
    const double price = (active_type_ == 1 || active_type_ == 3) ? price_edit_->text().toDouble() : 0.0;
    const double stop_price = (active_type_ == 2 || active_type_ == 3) ? stop_price_edit_->text().toDouble() : 0.0;
    // Same rule for the bracket legs: SL / TP typed earlier and then collapsed out of
    // sight used to ride along on every later order (other side, other symbol) — in
    // PAPER mode with no confirmation at all. Only send them while the section is open.
    const bool advanced_open = advanced_section_ && !advanced_section_->isHidden();
    const double sl = advanced_open ? sl_edit_->text().toDouble() : 0.0;
    const double tp = advanced_open ? tp_edit_->text().toDouble() : 0.0;

    emit order_submitted(side, order_type, qty, price, stop_price, sl, tp);
}

void CryptoOrderEntry::clear_ticket_fields() {
    for (QLineEdit* e : {qty_edit_, price_edit_, stop_price_edit_, sl_edit_, tp_edit_}) {
        if (e)
            e->clear();
    }
    if (qty_edit_) {
        qty_edit_->setProperty("invalid", false);
        repolish(qty_edit_);
    }
    // A validation / balance message belongs to the ticket that was just cleared.
    submit_error_pinned_ = false;
    if (status_label_) {
        status_label_->setProperty("severity", QString());
        status_label_->setVisible(false);
        repolish(status_label_);
    }
    update_cost_preview();
}

void CryptoOrderEntry::reset_after_fill(const QString& message, const QString& severity) {
    clear_ticket_fields();
    if (!status_label_ || message.isEmpty())
        return;
    status_label_->setText(message);
    status_label_->setProperty("severity", severity);
    status_label_->setVisible(true);
    repolish(status_label_);
}

void CryptoOrderEntry::set_market_limits(double min_amount, double min_cost, const QString& market,
                                         const QString& cost_ccy) {
    min_amount_ = std::isfinite(min_amount) && min_amount > 0.0 ? min_amount : 0.0;
    min_cost_ = std::isfinite(min_cost) && min_cost > 0.0 ? min_cost : 0.0;
    limits_market_ = market;
    limits_cost_ccy_ = cost_ccy;
}

void CryptoOrderEntry::show_submit_error(const QString& message, QWidget* focus) {
    status_label_->setText(message);
    status_label_->setProperty("severity", "error");
    status_label_->setVisible(true);
    submit_error_pinned_ = true;
    repolish(status_label_);
    if (focus)
        focus->setFocus();
}

void CryptoOrderEntry::on_pct_clicked(int pct) {
    if (balance_ <= 0 || current_price_ <= 0)
        return;
    // For limit orders use the limit price as the sizing basis if present, so
    // 100% means "use my whole balance at the limit I'd actually fill at" —
    // not at the current mark.
    double basis = current_price_;
    if ((active_type_ == 1 || active_type_ == 3)) {
        const double limit_p = price_edit_->text().toDouble();
        if (limit_p > 0)
            basis = limit_p;
    }
    const int leverage = (is_futures_ && leverage_spin_) ? leverage_spin_->value() : 1;
    // Leave headroom for the taker fee. Sizing against the bare balance made
    // "100 %" cost balance × (1 + fee), which the ticket then rejected as
    // insufficient — so the button never yielded a usable order.
    const double cost_per_unit = basis * (1.0 + kAssumedFeeRate);
    if (cost_per_unit <= 0.0)
        return;
    // The balance is held in its own currency (a USD paper account, a USDC
    // balance on a USDT pair); the price is in the quote asset. Size with a real
    // rate — never as if the two were the same unit.
    const QString quote = quote_ccy();
    const QString bal_ccy = balance_ccy();
    double balance_in_quote = balance_;
    QString approx_note; // shown after the quantity is filled in (editing it clears the status line)
    if (!quote.isEmpty() && bal_ccy != quote) {
        namespace fmt = fincept::ui::formatting;
        const fmt::ConvertedMoney c = fincept::services::fx::convert_for_display(balance_, bal_ccy, quote);
        if (!c.ok()) {
            show_submit_error(tr("⚠ The %1 → %2 exchange rate has not arrived yet (it is being fetched): the balance "
                                 "is in %1 and this pair is priced in %2, so %3 % of it cannot be sized. Try again "
                                 "in a few seconds.")
                                  .arg(bal_ccy, quote)
                                  .arg(pct),
                              nullptr);
            return;
        }
        balance_in_quote = c.amount;
        if (c.approximate || c.stale)
            approx_note = tr("Sized at %1 (%2)").arg(fmt::converted_money(c), fmt::fx_note(c));
    }
    const double max_qty = (balance_in_quote * leverage) / cost_per_unit;
    const double qty = max_qty * pct / 100.0;
    // Truncate rather than round — rounding the last digit up can push the
    // notional back over the balance.
    const double truncated = std::floor(qty * 1e8) / 1e8;
    qty_edit_->setText(QString::number(truncated, 'f', 8));
    if (!approx_note.isEmpty())
        show_notice(approx_note, QStringLiteral("warning"));
}

void CryptoOrderEntry::set_futures_mode(bool is_futures) {
    is_futures_ = is_futures;
    futures_section_->setVisible(is_futures);
    if (!is_futures && reduce_only_check_)
        reduce_only_check_->setChecked(false); // spot has no reduce-only concept
}

bool CryptoOrderEntry::reduce_only() const {
    return is_futures_ && reduce_only_check_ && reduce_only_check_->isChecked();
}

void CryptoOrderEntry::update_cost_preview() {
    const double qty = qty_edit_ ? qty_edit_->text().toDouble() : 0.0;
    double price = current_price_;
    if (active_type_ == 1 || active_type_ == 3) {
        const double limit_p = price_edit_ ? price_edit_->text().toDouble() : 0.0;
        if (limit_p > 0)
            price = limit_p;
    }

    namespace fmt = fincept::ui::formatting;
    // An order's value is in the market's quote asset; the balance is in its own
    // currency (bal_ccy). Amounts are never compared across the two without a rate.
    const QString quote = quote_ccy();
    const QString base = base_of(price_pair().section(QLatin1Char(':'), 0, 0));
    const QString bal_ccy = balance_ccy();
    auto fmt_money = [&](double v) { return fmt::money(v, quote); };
    auto fmt_base = [&](double v) { return tr("%1 %2").arg(format_size(v), base); };
    if (avail_label_)
        avail_label_->setToolTip(QString());

    if (qty > 0 && price > 0) {
        const double notional = qty * price;
        const int leverage = (is_futures_ && leverage_spin_) ? leverage_spin_->value() : 1;
        const double margin = notional / std::max(1, leverage);

        const double fee = notional * kAssumedFeeRate;
        const double total = margin + fee;

        // margin / total in the balance's currency: same currency as is, else at
        // the FX service's rate (NaN while it has none: "—", never 1:1).
        const bool same_ccy = quote.isEmpty() || bal_ccy == quote;
        fmt::ConvertedMoney margin_bal;
        fmt::ConvertedMoney total_bal;
        if (!same_ccy) {
            margin_bal = fincept::services::fx::convert_for_display(margin, quote, bal_ccy);
            total_bal = fincept::services::fx::convert_for_display(total, quote, bal_ccy);
        }
        const bool have_rate = same_ccy || (margin_bal.ok() && total_bal.ok());
        const double margin_in_bal = same_ccy ? margin : margin_bal.amount;
        const double total_in_bal = same_ccy ? total : total_bal.amount;

        cost_label_->setText(fmt_money(notional));
        fee_label_->setText(QStringLiteral("~%1").arg(fmt::money_precise(fee, quote)));
        total_label_->setText(fmt_money(total));
        recv_label_->setText(is_buy_side_ ? fmt_base(qty) : fmt_money(qty * price));
        pct_used_label_->setText(have_rate && balance_ > 0 ? fmt::pct(margin_in_bal / balance_ * 100.0, 2, false)
                                                           : fmt::placeholder());
        if (avail_label_) {
            if (!has_balance_) {
                avail_label_->setText(fmt::placeholder());
            } else if (have_rate) {
                avail_label_->setText(fmt::money(std::max(0.0, balance_ - total_in_bal), bal_ccy));
                if (total_bal.converted)
                    avail_label_->setToolTip(fmt::fx_note(total_bal)); // the rate and its time
            } else {
                avail_label_->setText(fmt::placeholder());
                avail_label_->setToolTip(tr("No %1 → %2 exchange rate yet (it is being fetched): the order is "
                                            "priced in %1, the balance is held in %2.")
                                             .arg(quote, bal_ccy));
            }
        }

        // Submit subtitle reads "0.31 BTC ≈ 25,000.00 USDT" or, for futures,
        // appends the leverage — and, on a contract market, the number of
        // contracts the exchange will receive (the exact rounded figure is in
        // the confirmation).
        QString subtitle =
            is_futures_ ? QString("%1 @ %2x  ≈  %3").arg(fmt_base(qty)).arg(leverage).arg(fmt_money(margin))
                        : QString("%1  ≈  %2").arg(fmt_base(qty), fmt_money(notional));
        if (market_.valid() && market_.is_contract() && market_.contract_size > 0.0) {
            const double contracts =
                market_.inverse ? notional / market_.contract_size : qty / market_.contract_size;
            subtitle += tr("  =  ≈%1 contracts").arg(fmt::quantity(contracts, 6));
        }
        submit_subtitle_->setText(subtitle);

        // Compared in the balance's currency; without a rate there is nothing to
        // compare (the engine checks the order itself at the real rate).
        const bool insufficient = !is_futures_ && have_rate && total_in_bal > balance_ && balance_ > 0;
        if (submit_error_pinned_) {
            // A refusal / notice from the last press stays readable until the
            // ticket is edited — this runs on every price tick.
        } else if (insufficient) {
            status_label_->setText(tr("⚠ Insufficient balance for this order"));
            status_label_->setProperty("severity", "warning");
            status_label_->setVisible(true);
            repolish(status_label_);
        } else if (!submit_error_pinned_) {
            // A message from the last BUY / SELL press (below the exchange minimum, limit price
            // required) stays until the ticket is edited: this runs on every price tick, and
            // clearing here wiped the message before it could be read — the button looked dead.
            // Also clear a sticky "error" severity here. Previously an error
            // set by on_submit() (e.g. "Enter a valid quantity") latched the
            // severity property forever, so the message stayed on screen even
            // after the user typed a perfectly valid ticket.
            status_label_->setProperty("severity", QString());
            status_label_->setVisible(false);
            repolish(status_label_);
        }
    } else {
        cost_label_->setText(QStringLiteral("--"));
        fee_label_->setText(QStringLiteral("--"));
        total_label_->setText(QStringLiteral("--"));
        recv_label_->setText(QStringLiteral("--"));
        pct_used_label_->setText(QStringLiteral("--"));
        if (avail_label_ && has_balance_ && balance_ > 0)
            avail_label_->setText(fmt::money(balance_, bal_ccy));
        else if (avail_label_)
            avail_label_->setText(QStringLiteral("--"));
        submit_subtitle_->setText(tr("Enter a quantity to preview"));
    }

    // What pressing the button would do, in words — read by assistive tools
    // and shown in an AI agent's approval request for this button.
    if (submit_btn_) {
        static const char* kTypes[] = {"MARKET", "LIMIT", "STOP", "STOP-LIMIT"};
        QString desc = tr("%1 %2 %3 %4 %5")
                           .arg(is_paper_ ? tr("PAPER") : tr("LIVE"), is_buy_side_ ? tr("BUY") : tr("SELL"),
                                QString::fromLatin1(kTypes[std::clamp(active_type_, 0, 3)]),
                                qty > 0 ? format_size(qty) : tr("(no quantity)"), base);
        desc += tr(" on %1 market %2").arg(venue_.toUpper(), market_.valid() ? market_.symbol : current_symbol_);
        if ((active_type_ == 1 || active_type_ == 3) && price_edit_ && price_edit_->text().toDouble() > 0)
            desc += tr(" limit %1").arg(price_edit_->text());
        if ((active_type_ == 2 || active_type_ == 3) && stop_price_edit_ && stop_price_edit_->text().toDouble() > 0)
            desc += tr(" stop %1").arg(stop_price_edit_->text());
        submit_btn_->setAccessibleDescription(desc);
    }
}

void CryptoOrderEntry::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void CryptoOrderEntry::retranslateUi() {
    if (title_label_)
        title_label_->setText(tr("ORDER ENTRY"));
    if (mode_label_)
        mode_label_->setText(is_paper_ ? tr("PAPER") : tr("LIVE"));

    // Account card titles
    if (balance_title_)
        balance_title_->setText(tr("BALANCE"));
    if (mark_title_)
        mark_title_->setText(tr("MARK"));
    if (avail_title_)
        avail_title_->setText(tr("AVAIL"));

    // Side tabs
    if (buy_tab_)
        buy_tab_->setText(tr("BUY"));
    if (sell_tab_)
        sell_tab_->setText(tr("SELL"));

    // Order type segmented control
    const QString type_labels[] = {tr("MARKET"), tr("LIMIT"), tr("STOP"), tr("STOP-LMT")};
    for (int i = 0; i < 4; ++i)
        if (type_btns_[i])
            type_btns_[i]->setText(type_labels[i]);

    // Field / section titles
    if (qty_title_)
        qty_title_->setText(tr("QUANTITY"));
    if (price_title_)
        price_title_->setText(tr("LIMIT PRICE"));
    if (stop_title_)
        stop_title_->setText(tr("STOP PRICE"));
    if (sl_title_)
        sl_title_->setText(tr("STOP LOSS"));
    if (tp_title_)
        tp_title_->setText(tr("TAKE PROFIT"));
    if (sl_edit_)
        sl_edit_->setPlaceholderText(tr("Trigger price"));
    if (tp_edit_)
        tp_edit_->setPlaceholderText(tr("Trigger price"));
    if (leverage_title_)
        leverage_title_->setText(tr("LEVERAGE"));
    if (margin_title_)
        margin_title_->setText(tr("MARGIN MODE"));
    if (apply_lev_btn_)
        apply_lev_btn_->setText(tr("APPLY LEVERAGE / MARGIN TO ACCOUNT"));
    if (lev_current_label_)
        set_account_leverage(account_leverage_, account_margin_mode_);
    apply_capabilities(); // banner + gating reasons in the new language

    // Margin mode combo items (preserve userData / selection)
    if (margin_mode_combo_) {
        margin_mode_combo_->setItemText(0, tr("Cross"));
        margin_mode_combo_->setItemText(1, tr("Isolated"));
    }

    // Order breakdown titles
    if (cost_title_)
        cost_title_->setText(tr("ORDER VALUE"));
    if (fee_title_)
        fee_title_->setText(tr("FEE EST"));
    if (total_title_)
        total_title_->setText(tr("TOTAL"));
    if (recv_title_)
        recv_title_->setText(tr("YOU RECEIVE"));
    if (pct_used_title_)
        pct_used_title_->setText(tr("% OF BALANCE"));

    // Advanced toggle reflects current expanded state
    if (advanced_toggle_ && advanced_section_)
        advanced_toggle_->setText(advanced_section_->isVisible() ? tr("▴  Advanced  (SL / TP)")
                                                                 : tr("▾  Advanced  (SL / TP)"));

    // Submit button + tooltip + subtitle
    if (submit_btn_) {
        if (!submit_busy_)
            submit_btn_->setText(is_buy_side_ ? tr("BUY  %1").arg(current_symbol_)
                                              : tr("SELL  %1").arg(current_symbol_));
        submit_btn_->setToolTip(tr("Submit order  (⌘/Ctrl+Enter)"));
    }

    // Recompute the live cost preview so the subtitle / breakdown values pick
    // up the new language; covers both the populated and the placeholder state.
    update_cost_preview();
}

void CryptoOrderEntry::focus_quantity() {
    if (!qty_edit_)
        return;
    qty_edit_->setFocus(Qt::OtherFocusReason);
    qty_edit_->selectAll();
}

void CryptoOrderEntry::prefill(bool is_buy, double quantity, const QString& order_type, double limit_price) {
    if (!qty_edit_ || !price_edit_)
        return;
    // A fresh ticket: nothing typed earlier (stop, SL / TP, an old message) rides along.
    clear_ticket_fields();
    set_buy_side(is_buy);
    const bool has_limit = std::isfinite(limit_price) && limit_price > 0.0;
    const QString want = order_type.trimmed().toLower();
    int type = -1;
    if (want.isEmpty())
        type = has_limit ? 1 : 0;
    else if (want == QLatin1String("market"))
        type = 0;
    else if (want == QLatin1String("limit"))
        type = 1;
    else if (want == QLatin1String("stop"))
        type = 2;
    else if (want == QLatin1String("stop_limit") || want == QLatin1String("stop-limit") ||
             want == QLatin1String("stoplimit"))
        type = 3;
    if (type >= 0)
        set_order_type(type);

    // Shortest exact decimal text (never rounded, never exponent notation) — what the
    // boxes' toDouble() reads back as the same number.
    auto plain = [](double v) { return QString::number(v, 'f', QLocale::FloatingPointShortest); };
    auto typed_ok = [](QLineEdit* e) {
        if (!e->validator())
            return true;
        QString text = e->text();
        int pos = 0;
        return e->validator()->validate(text, pos) == QValidator::Acceptable;
    };
    QStringList problems;
    if (type < 0)
        problems << tr("⚠ Order type \"%1\" is not on this ticket — pick MARKET, LIMIT, STOP or STOP-LMT.")
                        .arg(order_type.trimmed());
    if (std::isfinite(quantity) && quantity > 0.0) {
        qty_edit_->setText(plain(quantity)); // textChanged: cost preview, balance check
        if (!typed_ok(qty_edit_)) {
            qty_edit_->setProperty("invalid", true);
            repolish(qty_edit_);
            problems << tr("⚠ Quantity %1 is not a size this ticket takes (at most 8 decimals); it was not "
                           "rounded — correct it before sending.")
                            .arg(qty_edit_->text());
        }
    }
    if (has_limit && (active_type_ == 1 || active_type_ == 3)) {
        price_edit_->setText(plain(limit_price));
        if (!typed_ok(price_edit_))
            problems << tr("⚠ Limit price %1 is not a price this ticket takes (at most 8 decimals); it was not "
                           "rounded — correct it before sending.")
                            .arg(price_edit_->text());
    }
    // Shown like a refusal: it stays until the ticket is edited.
    if (!problems.isEmpty())
        show_notice(problems.join(QLatin1Char(' ')), QStringLiteral("warning"));
}

} // namespace fincept::screens::crypto
