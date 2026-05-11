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

#include <QComboBox>
#include <QDoubleValidator>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens::crypto {

namespace {

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

// Quote symbol (typically USDT, USD, USDC, …) of a pair.
QString quote_of(const QString& pair) {
    const int slash = pair.indexOf('/');
    return slash > 0 ? pair.mid(slash + 1) : QStringLiteral("USD");
}

// Numeric-only validator that allows decimals; reused by every numeric input.
QDoubleValidator* make_decimal_validator(QObject* parent) {
    auto* v = new QDoubleValidator(0.0, 1e15, 8, parent);
    v->setNotation(QDoubleValidator::StandardNotation);
    v->setLocale(QLocale::c());
    return v;
}

void repolish(QWidget* w) {
    if (!w || !w->style()) return;
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

    auto* title = new QLabel(QStringLiteral("ORDER ENTRY"));
    title->setObjectName("cryptoOeTitle");
    h_layout->addWidget(title);
    h_layout->addStretch();

    mode_label_ = new QLabel(QStringLiteral("PAPER"));
    mode_label_->setObjectName("cryptoOeMode");
    mode_label_->setProperty("mode", "paper");
    h_layout->addWidget(mode_label_);

    root->addWidget(header);

    // ── Scrollable content ──────────────────────────────────────────────────
    auto* content = new QWidget(this);
    auto* form = new QVBoxLayout(content);
    form->setContentsMargins(10, 10, 10, 10);
    form->setSpacing(10);

    // ── 1. Account card (Balance / Mark / Avail) ────────────────────────────
    {
        auto* card = make_card();
        auto* grid = new QGridLayout(card);
        grid->setContentsMargins(12, 10, 12, 10);
        grid->setHorizontalSpacing(12);
        grid->setVerticalSpacing(4);
        grid->setColumnStretch(0, 0);
        grid->setColumnStretch(1, 1);

        balance_label_      = add_kv_row(grid, 0, QStringLiteral("BALANCE"),  "cryptoOeKvValueAccent");
        market_price_label_ = add_kv_row(grid, 1, QStringLiteral("MARK"));
        avail_label_        = add_kv_row(grid, 2, QStringLiteral("AVAIL"),    "cryptoOeKvValueDim");

        balance_label_->setText(QStringLiteral("$0.00"));
        market_price_label_->setText(QStringLiteral("--"));
        avail_label_->setText(QStringLiteral("--"));

        form->addWidget(card);
    }

    // ── 2. BUY / SELL tabs ──────────────────────────────────────────────────
    {
        auto* side_row = new QHBoxLayout;
        side_row->setSpacing(0);
        side_row->setContentsMargins(0, 0, 0, 0);

        buy_tab_ = new QPushButton(QStringLiteral("BUY"));
        buy_tab_->setObjectName("cryptoBuyTab");
        buy_tab_->setProperty("active", true);
        buy_tab_->setCursor(Qt::PointingHandCursor);
        buy_tab_->setFocusPolicy(Qt::NoFocus);
        connect(buy_tab_, &QPushButton::clicked, this, [this]() { set_buy_side(true); });
        side_row->addWidget(buy_tab_, 1);

        sell_tab_ = new QPushButton(QStringLiteral("SELL"));
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
        const char* type_labels[] = {"MARKET", "LIMIT", "STOP", "STOP-LMT"};
        for (int i = 0; i < 4; ++i) {
            type_btns_[i] = new QPushButton(QString::fromLatin1(type_labels[i]));
            type_btns_[i]->setObjectName("cryptoOeTypeBtn");
            type_btns_[i]->setCursor(Qt::PointingHandCursor);
            type_btns_[i]->setFocusPolicy(Qt::NoFocus);
            if (i == 0) type_btns_[i]->setProperty("active", true);
            connect(type_btns_[i], &QPushButton::clicked, this, [this, i]() { set_order_type(i); });
            type_row->addWidget(type_btns_[i], 1);
        }
        form->addLayout(type_row);
    }

    // ── 4. Quantity input + 25/50/75/100% quick fills ───────────────────────
    {
        auto* qty_label = new QLabel(QStringLiteral("QUANTITY"));
        qty_label->setObjectName("cryptoOeLabel");
        form->addWidget(qty_label);

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
        connect(qty_edit_, &QLineEdit::textChanged, this, [this]() { update_cost_preview(); });
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

        // Symbol-change signal will update the unit label.
        connect(this, &CryptoOrderEntry::destroyed, unit, [](){});
        // Repurpose set_symbol's path: we keep a reference to update later.
        unit->setProperty("ftRoleUnit", true);
    }

    // ── 5. Price (Limit / Stop-Limit) ───────────────────────────────────────
    {
        price_row_ = new QWidget;
        auto* v = new QVBoxLayout(price_row_);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("LIMIT PRICE"));
        lbl->setObjectName("cryptoOeLabel");
        v->addWidget(lbl);

        auto* wrap = new QWidget;
        auto* h = new QHBoxLayout(wrap);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(0);

        price_edit_ = new QLineEdit;
        price_edit_->setObjectName("cryptoOeInput");
        price_edit_->setPlaceholderText(QStringLiteral("0.00"));
        price_edit_->setValidator(make_decimal_validator(this));
        price_edit_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        connect(price_edit_, &QLineEdit::textChanged, this, [this]() { update_cost_preview(); });
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

        auto* lbl = new QLabel(QStringLiteral("STOP PRICE"));
        lbl->setObjectName("cryptoOeLabel");
        v->addWidget(lbl);

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

        cost_label_     = add_kv_row(grid, 0, QStringLiteral("ORDER VALUE"));
        fee_label_      = add_kv_row(grid, 1, QStringLiteral("FEE EST"),     "cryptoOeKvValueDim");
        total_label_    = add_kv_row(grid, 2, QStringLiteral("TOTAL"),       "cryptoOeKvValueAccent");
        recv_label_     = add_kv_row(grid, 3, QStringLiteral("YOU RECEIVE"), "cryptoOeKvValueDim");
        pct_used_label_ = add_kv_row(grid, 4, QStringLiteral("% OF BALANCE"),"cryptoOeKvValueDim");

        form->addWidget(card);
    }

    // ── 8. Advanced toggle (SL / TP) ────────────────────────────────────────
    advanced_toggle_ = new QPushButton(QStringLiteral("▾  Advanced  (SL / TP)"));
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

        auto add_pair = [&](const QString& label, QLineEdit*& edit, const QString& placeholder) {
            auto* lbl = new QLabel(label);
            lbl->setObjectName("cryptoOeLabel");
            adv->addWidget(lbl);
            edit = new QLineEdit;
            edit->setObjectName("cryptoOeInput");
            edit->setPlaceholderText(placeholder);
            edit->setValidator(make_decimal_validator(this));
            edit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            adv->addWidget(edit);
        };
        add_pair(QStringLiteral("STOP LOSS"),   sl_edit_, QStringLiteral("Trigger price"));
        add_pair(QStringLiteral("TAKE PROFIT"), tp_edit_, QStringLiteral("Trigger price"));
    }
    form->addWidget(advanced_section_);

    connect(advanced_toggle_, &QPushButton::clicked, this, [this]() {
        const bool show = !advanced_section_->isVisible();
        advanced_section_->setVisible(show);
        advanced_toggle_->setText(show ? QStringLiteral("▴  Advanced  (SL / TP)")
                                       : QStringLiteral("▾  Advanced  (SL / TP)"));
    });

    // ── 9. Futures controls (leverage + margin mode) ────────────────────────
    futures_section_ = new QWidget(this);
    futures_section_->setVisible(false);
    {
        auto* layout = new QVBoxLayout(futures_section_);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(6);

        auto* lev_lbl = new QLabel(QStringLiteral("LEVERAGE"));
        lev_lbl->setObjectName("cryptoOeLabel");
        layout->addWidget(lev_lbl);

        leverage_spin_ = new QSpinBox;
        leverage_spin_->setObjectName("cryptoOeSpinBox");
        leverage_spin_->setRange(1, 125);
        leverage_spin_->setValue(1);
        leverage_spin_->setSuffix(QStringLiteral("x"));
        leverage_spin_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        layout->addWidget(leverage_spin_);

        auto* margin_lbl = new QLabel(QStringLiteral("MARGIN MODE"));
        margin_lbl->setObjectName("cryptoOeLabel");
        layout->addWidget(margin_lbl);

        margin_mode_combo_ = new QComboBox;
        margin_mode_combo_->setObjectName("cryptoOeCombo");
        margin_mode_combo_->addItem(QStringLiteral("Cross"),    QStringLiteral("cross"));
        margin_mode_combo_->addItem(QStringLiteral("Isolated"), QStringLiteral("isolated"));
        layout->addWidget(margin_mode_combo_);
    }
    form->addWidget(futures_section_);

    connect(leverage_spin_, &QSpinBox::valueChanged, this, [this](int val) {
        emit leverage_changed(val);
        update_cost_preview();
    });
    connect(margin_mode_combo_, &QComboBox::currentIndexChanged, this,
            [this](int idx) { emit margin_mode_changed(margin_mode_combo_->itemData(idx).toString()); });

    // ── 10. Submit button + computed subtitle ───────────────────────────────
    submit_btn_ = new QPushButton(QStringLiteral("BUY  ") + current_symbol_);
    submit_btn_->setObjectName("cryptoBuySubmit");
    submit_btn_->setCursor(Qt::PointingHandCursor);
    submit_btn_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return));
    submit_btn_->setToolTip(QStringLiteral("Submit order  (⌘/Ctrl+Enter)"));
    connect(submit_btn_, &QPushButton::clicked, this, &CryptoOrderEntry::on_submit);
    form->addWidget(submit_btn_);

    submit_subtitle_ = new QLabel(QStringLiteral("Enter a quantity to preview"));
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

    update_cost_preview();
}

void CryptoOrderEntry::set_buy_side(bool is_buy) {
    if (is_buy == is_buy_side_)
        return;
    is_buy_side_ = is_buy;
    buy_tab_->setProperty("active", is_buy);
    sell_tab_->setProperty("active", !is_buy);

    submit_btn_->setText(QString::fromLatin1(is_buy ? "BUY  " : "SELL  ") + current_symbol_);
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
        if (price_row_) price_row_->setVisible(idx == 1 || idx == 3);
        if (stop_row_)  stop_row_->setVisible(idx == 2 || idx == 3);
        if (price_edit_) price_edit_->setEnabled(idx == 1 || idx == 3);
        if (stop_price_edit_) stop_price_edit_->setEnabled(idx == 2 || idx == 3);
        update_cost_preview();
        return;
    }
    active_type_ = idx;
    for (int i = 0; i < 4; ++i)
        type_btns_[i]->setProperty("active", i == idx);

    if (prev >= 0 && prev < 4) repolish(type_btns_[prev]);
    repolish(type_btns_[idx]);

    if (price_row_) price_row_->setVisible(idx == 1 || idx == 3);
    if (stop_row_)  stop_row_->setVisible(idx == 2 || idx == 3);
    if (price_edit_) price_edit_->setEnabled(idx == 1 || idx == 3);
    if (stop_price_edit_) stop_price_edit_->setEnabled(idx == 2 || idx == 3);

    update_cost_preview();
}

void CryptoOrderEntry::set_balance(double balance) {
    balance_ = balance;
    balance_label_->setText(QString("$%1").arg(balance, 0, 'f', 2));
    update_cost_preview();
}

void CryptoOrderEntry::set_current_price(double price) {
    current_price_ = price;
    market_price_label_->setText(QString("$%1").arg(price, 0, 'f', 2));
    update_cost_preview();
}

void CryptoOrderEntry::set_mode(bool is_paper) {
    is_paper_ = is_paper;
    mode_label_->setText(is_paper ? QStringLiteral("PAPER") : QStringLiteral("LIVE"));
    mode_label_->setProperty("mode", is_paper ? "paper" : "live");
    repolish(mode_label_);
}

void CryptoOrderEntry::set_symbol(const QString& symbol) {
    current_symbol_ = symbol;
    submit_btn_->setText(QString::fromLatin1(is_buy_side_ ? "BUY  " : "SELL  ") + symbol);

    // Update unit suffix labels (base on qty, quote on price/stop) by looking
    // up the children we tagged with the ftRoleUnit dynamic property.
    const QString base = base_of(symbol);
    const QString quote = quote_of(symbol);
    for (auto* lbl : findChildren<QLabel*>()) {
        const auto role = lbl->property("ftRoleUnit");
        if (!role.isValid()) continue;
        if (role.toString() == QLatin1String("quote"))
            lbl->setText(quote);
        else
            lbl->setText(base);
    }
    update_cost_preview();
}

void CryptoOrderEntry::on_submit() {
    const QString side = is_buy_side_ ? "buy" : "sell";
    static const char* type_map[] = {"market", "limit", "stop", "stop_limit"};
    const QString order_type = type_map[active_type_];

    const double qty = qty_edit_->text().toDouble();
    if (qty <= 0) {
        status_label_->setText(QStringLiteral("⚠ Enter a valid quantity"));
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
        status_label_->setText(QStringLiteral("⚠ Limit price required"));
        status_label_->setProperty("severity", "error");
        status_label_->setVisible(true);
        repolish(status_label_);
        price_edit_->setFocus();
        return;
    }
    if ((active_type_ == 2 || active_type_ == 3) && stop_price_edit_->text().toDouble() <= 0) {
        status_label_->setText(QStringLiteral("⚠ Stop trigger price required"));
        status_label_->setProperty("severity", "error");
        status_label_->setVisible(true);
        repolish(status_label_);
        stop_price_edit_->setFocus();
        return;
    }

    status_label_->setVisible(false);

    const double price = price_edit_->text().toDouble();
    const double stop_price = stop_price_edit_->text().toDouble();
    const double sl = sl_edit_->text().toDouble();
    const double tp = tp_edit_->text().toDouble();

    emit order_submitted(side, order_type, qty, price, stop_price, sl, tp);
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
        if (limit_p > 0) basis = limit_p;
    }
    const int leverage = (is_futures_ && leverage_spin_) ? leverage_spin_->value() : 1;
    const double max_qty = (balance_ * leverage) / basis;
    const double qty = max_qty * pct / 100.0;
    qty_edit_->setText(QString::number(qty, 'f', 6));
}

void CryptoOrderEntry::set_futures_mode(bool is_futures) {
    is_futures_ = is_futures;
    futures_section_->setVisible(is_futures);
}

void CryptoOrderEntry::update_cost_preview() {
    const double qty = qty_edit_ ? qty_edit_->text().toDouble() : 0.0;
    double price = current_price_;
    if (active_type_ == 1 || active_type_ == 3) {
        const double limit_p = price_edit_ ? price_edit_->text().toDouble() : 0.0;
        if (limit_p > 0) price = limit_p;
    }

    const QString quote = quote_of(current_symbol_);
    const QString base  = base_of(current_symbol_);
    auto fmt_money = [&](double v) { return QString("%1 %2").arg(v, 0, 'f', 2).arg(quote); };
    auto fmt_base  = [&](double v) { return QString("%1 %2").arg(v, 0, 'f', 6).arg(base); };

    if (qty > 0 && price > 0) {
        const double notional = qty * price;
        const int leverage = (is_futures_ && leverage_spin_) ? leverage_spin_->value() : 1;
        const double margin = notional / std::max(1, leverage);

        // Taker fee assumption — used purely for the on-screen estimate. Real
        // fees come back from the exchange post-fill.
        constexpr double kAssumedFeeBps = 0.0007; // 7 bps / 0.07 %
        const double fee = notional * kAssumedFeeBps;
        const double total = margin + fee;
        const double pct = balance_ > 0 ? (margin / balance_) * 100.0 : 0.0;
        const double avail = std::max(0.0, balance_ - total);

        cost_label_->setText(fmt_money(notional));
        fee_label_->setText(QString("~%1").arg(fmt_money(fee)));
        total_label_->setText(fmt_money(total));
        recv_label_->setText(is_buy_side_ ? fmt_base(qty) : fmt_money(qty * price));
        pct_used_label_->setText(QString("%1%").arg(pct, 0, 'f', 2));
        if (avail_label_)
            avail_label_->setText(fmt_money(avail));

        // Submit subtitle reads "0.31 BTC ≈ $25,000" or, for futures,
        // appends the leverage.
        const QString subtitle =
            is_futures_
                ? QString("%1 @ %2x  ≈  %3").arg(fmt_base(qty)).arg(leverage).arg(fmt_money(margin))
                : QString("%1  ≈  %2").arg(fmt_base(qty), fmt_money(notional));
        submit_subtitle_->setText(subtitle);

        const bool insufficient = !is_futures_ && total > balance_ && balance_ > 0;
        if (insufficient) {
            status_label_->setText(QStringLiteral("⚠ Insufficient balance for this order"));
            status_label_->setProperty("severity", "warning");
            status_label_->setVisible(true);
            repolish(status_label_);
        } else if (status_label_->property("severity").toString() != QLatin1String("error")) {
            status_label_->setVisible(false);
        }
    } else {
        cost_label_->setText(QStringLiteral("--"));
        fee_label_->setText(QStringLiteral("--"));
        total_label_->setText(QStringLiteral("--"));
        recv_label_->setText(QStringLiteral("--"));
        pct_used_label_->setText(QStringLiteral("--"));
        if (avail_label_ && balance_ > 0)
            avail_label_->setText(fmt_money(balance_));
        else if (avail_label_)
            avail_label_->setText(QStringLiteral("--"));
        submit_subtitle_->setText(QStringLiteral("Enter a quantity to preview"));
    }
}

} // namespace fincept::screens::crypto
