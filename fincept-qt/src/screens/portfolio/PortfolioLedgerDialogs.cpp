// src/screens/portfolio/PortfolioLedgerDialogs.cpp
#include "screens/portfolio/PortfolioLedgerDialogs.h"

#include "core/market/InstrumentCurrency.h"
#include "screens/portfolio/PortfolioFormat.h"
#include "services/portfolio/PortfolioLots.h"
#include "services/portfolio/PortfolioService.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

QString ledger_dialog_qss(const QString& accent) {
    return QString("QDialog { background:%1; color:%2; }"
                   "QLabel { color:%3; font-size:%8px; }"
                   "QLineEdit, QDateEdit, QComboBox, QSpinBox, QDoubleSpinBox {"
                   "  background:%4; color:%2; border:1px solid %5; padding:4px 8px; font-size:%9px; }"
                   "QLineEdit:focus, QDateEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus {"
                   "  border-color:%6; }"
                   "QLineEdit:disabled, QDoubleSpinBox:disabled { color:%10; }"
                   "QComboBox QAbstractItemView { background:%4; color:%2; selection-background-color:%6;"
                   "  selection-color:%7; }"
                   "QCheckBox, QRadioButton { color:%2; font-size:%8px; }"
                   "QGroupBox { color:%3; border:1px solid %5; margin-top:10px; padding-top:6px; font-size:%8px; }"
                   "QGroupBox::title { subcontrol-origin:margin; left:8px; padding:0 4px; }"
                   "QTableWidget { background:%4; color:%2; border:1px solid %5; font-size:%9px; }"
                   "QToolButton { background:transparent; color:%3; border:none; font-size:%8px; font-weight:700; }"
                   "QToolButton:hover { color:%6; }")
        .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_SECONDARY(), ui::colors::BG_BASE(),
             ui::colors::BORDER_MED(), accent, ui::colors::TEXT_ON_ACCENT(), ui::fonts::px(-3), ui::fonts::px(-2))
        .arg(ui::colors::TEXT_TERTIARY());
}

namespace {

bool ldg_try_parse(const QString& text, double* out) {
    const QString t = text.trimmed().remove(QLatin1Char(' '));
    if (t.isEmpty())
        return false;
    bool ok = false;
    double v = t.toDouble(&ok);
    if (!ok)
        v = QLocale().toDouble(t, &ok);
    if (!ok)
        v = QString(t).remove(QLatin1Char(',')).toDouble(&ok);
    if (!ok || !std::isfinite(v))
        return false;
    *out = v;
    return true;
}

} // namespace

double ledger_parse_number(const QString& text) {
    double v = 0.0;
    return ldg_try_parse(text, &v) ? v : 0.0;
}

namespace {

QLabel* ldg_title(const QString& text, const QString& color) {
    auto* l = new QLabel(text);
    l->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; letter-spacing:1px;")
                         .arg(color, ui::fonts::px(-1)));
    return l;
}

QLabel* ldg_hint(const QString& text) {
    auto* l = new QLabel(text);
    l->setWordWrap(true);
    l->setStyleSheet(QString("color:%1; font-size:%2px;").arg(ui::colors::TEXT_TERTIARY(), ui::fonts::px(-4)));
    return l;
}

QPushButton* ldg_button(const QString& text, bool primary, const QString& accent) {
    auto* b = new QPushButton(text);
    b->setMinimumHeight(ui::metrics::height(30));
    b->setMinimumWidth(ui::metrics::width(90));
    b->setCursor(Qt::PointingHandCursor);
    if (primary)
        b->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:none; font-size:%3px; font-weight:700;"
                                 "  padding:0 14px; }"
                                 "QPushButton:disabled { background:%4; color:%5; }")
                             .arg(accent, ui::colors::contrast_text_on(QColor(accent)).name(), ui::fonts::px(-4),
                                  ui::colors::BG_RAISED(), ui::colors::TEXT_TERTIARY()));
    else
        b->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                                 "  font-size:%3px; font-weight:700; padding:0 14px; }"
                                 "QPushButton:hover { background:%4; }")
                             .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED(), ui::fonts::px(-4),
                                  ui::colors::BG_HOVER()));
    return b;
}

QDateEdit* ldg_date_edit(const QDate& d) {
    auto* e = new QDateEdit(d.isValid() ? d : QDate::currentDate());
    e->setCalendarPopup(true);
    e->setDisplayFormat(ui::formatting::date_pattern(ui::formatting::DateStyle::Short));
    e->setMaximumDate(QDate::currentDate());
    return e;
}

/// Trade-currency choices: the instrument's and the portfolio's first, then a short
/// list of common quote currencies (minor units included: LSE trades in GBp). The
/// combo is editable, so any other code can be typed.
QStringList ldg_common_currencies(const QString& first, const QString& second) {
    QStringList out;
    for (const QString& c : {first, second})
        if (!c.isEmpty() && !out.contains(c))
            out << c;
    for (const char* c : {"USD", "EUR", "GBP", "GBp", "JPY", "CHF", "CAD", "AUD", "HKD", "SGD", "INR", "CNY", "SEK",
                          "NOK", "DKK", "NZD", "ZAR", "BRL", "MXN", "KRW"}) {
        const QString s = QString::fromLatin1(c);
        if (!out.contains(s))
            out << s;
    }
    return out;
}

bool ldg_valid_ccy(const QString& c) {
    if (c.isEmpty())
        return true;
    return core::market::major_currency(c).size() == 3 &&
           std::all_of(c.begin(), c.end(), [](QChar ch) { return ch.isLetter(); });
}

[[maybe_unused]] QString ldg_fx_text(double v) {
    ui::formatting::InstrumentFmt fx;
    fx.fx = true;
    return ui::formatting::price(v, fx);
}

} // namespace

// ── TradeCostsBox ────────────────────────────────────────────────────────────

TradeCostsBox::TradeCostsBox(QWidget* parent) : QWidget(parent) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(4);

    toggle_ = new QToolButton(this);
    toggle_->setCheckable(true);
    toggle_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    toggle_->setCursor(Qt::PointingHandCursor);
    v->addWidget(toggle_);

    body_ = new QWidget(this);
    auto* form = new QFormLayout(body_);
    form->setContentsMargins(12, 0, 0, 0);
    form->setSpacing(6);
    fees_edit_ = new QLineEdit(body_);
    fees_edit_->setPlaceholderText(QStringLiteral("0"));
    fees_label_ = new QLabel(body_);
    form->addRow(fees_label_, fees_edit_);
    taxes_edit_ = new QLineEdit(body_);
    taxes_edit_->setPlaceholderText(QStringLiteral("0"));
    taxes_label_ = new QLabel(body_);
    form->addRow(taxes_label_, taxes_edit_);
    ccy_cb_ = new QComboBox(body_);
    ccy_cb_->setEditable(true);
    ccy_cb_->setInsertPolicy(QComboBox::NoInsert);
    ccy_label_ = new QLabel(body_);
    form->addRow(ccy_label_, ccy_cb_);
    fx_edit_ = new QLineEdit(body_);
    fx_label_ = new QLabel(body_);
    form->addRow(fx_label_, fx_edit_);
    hint_ = ldg_hint(QString());
    form->addRow(hint_);
    v->addWidget(body_);
    body_->setVisible(false);

    connect(toggle_, &QToolButton::toggled, this, [this](bool on) {
        body_->setVisible(on);
        retranslate();
        if (auto* w = window())
            w->adjustSize();
    });
    for (QLineEdit* e : {fees_edit_, taxes_edit_, fx_edit_})
        connect(e, &QLineEdit::textChanged, this, [this]() { emit changed(); });
    connect(ccy_cb_, &QComboBox::currentTextChanged, this, [this]() {
        update_fx_state();
        emit changed();
    });
    // Only a currency the user chose is written to the ledger row.
    connect(ccy_cb_, &QComboBox::textActivated, this, [this](const QString&) { ccy_explicit_ = true; });
    if (ccy_cb_->lineEdit())
        connect(ccy_cb_->lineEdit(), &QLineEdit::textEdited, this, [this](const QString&) { ccy_explicit_ = true; });
    retranslate();
}

QString TradeCostsBox::shown_currency() const {
    return ccy_cb_->currentText().trimmed();
}

void TradeCostsBox::retranslate() {
    // "&" escaped: a button reads it as a mnemonic ("TAXES _CURRENCY").
    toggle_->setText((toggle_->isChecked() ? QStringLiteral("▾ ") : QStringLiteral("▸ ")) +
                     tr("FEES, TAXES & CURRENCY (optional)").replace(QLatin1Char('&'), QLatin1String("&&")));
    fees_label_->setText(tr("Fees / commission:"));
    taxes_label_->setText(taxes_text_.isEmpty() ? tr("Taxes / stamp duty:") : taxes_text_);
    ccy_label_->setText(tr("Currency:"));
    update_fx_state();
}

void TradeCostsBox::set_context(const QString& base_ccy, const QString& instrument_ccy) {
    // Empty until the caller knows the portfolio: the display currency stands in.
    base_ccy_ = base_ccy.isEmpty() ? ui::formatting::display_currency() : base_ccy.toUpper();
    set_instrument_currency(instrument_ccy);
}

void TradeCostsBox::set_instrument_currency(const QString& instrument_ccy) {
    instrument_ccy_ = instrument_ccy;
    const QString keep = ccy_cb_->currentText();
    QSignalBlocker block(ccy_cb_);
    ccy_cb_->clear();
    ccy_cb_->addItems(ldg_common_currencies(instrument_ccy_, base_ccy_));
    ccy_cb_->setCurrentText(instrument_ccy_.isEmpty() ? (keep.isEmpty() ? base_ccy_ : keep) : instrument_ccy_);
    update_fx_state();
    // A foreign instrument: show the block so the FX rate is not missed.
    if (!instrument_ccy_.isEmpty() && core::market::major_currency(instrument_ccy_) != base_ccy_)
        set_expanded(true);
    emit changed();
}

void TradeCostsBox::set_values(double fees, double taxes, const QString& currency, double fx_rate) {
    fees_edit_->setText(fees > 0 ? QString::number(fees, 'g', 12) : QString());
    taxes_edit_->setText(taxes > 0 ? QString::number(taxes, 'g', 12) : QString());
    ccy_explicit_ = !currency.isEmpty(); // keep an edited row's currency (or its absence) as it was
    if (!currency.isEmpty())
        ccy_cb_->setCurrentText(currency);
    fx_edit_->setText(fx_rate > 0 ? QString::number(fx_rate, 'g', 12) : QString());
    update_fx_state();
    if (fees > 0 || taxes > 0 || fx_rate > 0 || !currency.isEmpty())
        set_expanded(true);
}

void TradeCostsBox::set_taxes_label(const QString& text) {
    taxes_text_ = text;
    retranslate();
}

void TradeCostsBox::set_expanded(bool expanded) {
    toggle_->setChecked(expanded);
}

void TradeCostsBox::update_fx_state() {
    const QString ccy = shown_currency().isEmpty() ? instrument_ccy_ : shown_currency();
    const QString major = core::market::major_currency(ccy);
    const bool same = major.isEmpty() || major == base_ccy_;
    fx_label_->setText(tr("FX to %1:").arg(base_ccy_));
    fx_edit_->setEnabled(!same);
    if (same) {
        fx_edit_->setPlaceholderText(tr("1 — same currency as the portfolio"));
        hint_->setText(tr("Price, fees and taxes are in %1.").arg(ccy.isEmpty() ? base_ccy_ : ccy));
    } else {
        fx_edit_->setPlaceholderText(tr("%1 per 1 %2 — blank = daily rate of the trade date").arg(base_ccy_, major));
        hint_->setText(tr("Price, fees and taxes are in %1%2. Recording the FX rate of the trade lets the P&L be "
                          "split into price and currency effects.")
                           .arg(ccy, core::market::is_minor_unit(ccy) ? tr(" (minor units)") : QString()));
    }
}

double TradeCostsBox::fees() const {
    return ledger_parse_number(fees_edit_->text());
}
double TradeCostsBox::taxes() const {
    return ledger_parse_number(taxes_edit_->text());
}
QString TradeCostsBox::currency() const {
    return ccy_explicit_ ? shown_currency() : QString();
}
double TradeCostsBox::fx_rate() const {
    if (!fx_edit_->isEnabled())
        return 0.0;
    return ledger_parse_number(fx_edit_->text());
}

bool TradeCostsBox::validate(QString* why) const {
    auto fail = [why](const QString& m) {
        if (why)
            *why = m;
        return false;
    };
    for (QLineEdit* e : {fees_edit_, taxes_edit_}) {
        if (e->text().trimmed().isEmpty())
            continue;
        double v = 0;
        if (!ldg_try_parse(e->text(), &v))
            return fail(tr("'%1' is not a number.").arg(e->text().trimmed()));
        if (v < 0)
            return fail(e == fees_edit_ ? tr("Fees must be 0 or more.") : tr("Taxes must be 0 or more."));
    }
    if (!ldg_valid_ccy(currency()))
        return fail(tr("'%1' is not a currency code.").arg(currency()));
    // An FX rate without a currency means "per 1 unit of the instrument's quoted currency";
    // only meaningful when that currency is known (the shown one is a guess from the ticker).
    if (!ccy_explicit_ && instrument_ccy_.isEmpty() && fx_rate() > 0)
        return fail(tr("Choose the trade currency before entering an FX rate."));
    if (fx_edit_->isEnabled() && !fx_edit_->text().trimmed().isEmpty() && !(fx_rate() > 0))
        return fail(tr("The FX rate must be greater than 0 (or blank)."));
    return true;
}

// ── CashMovementDialog ───────────────────────────────────────────────────────

CashMovementDialog::CashMovementDialog(const QString& base_ccy, bool track_cash, const QStringList& symbols,
                                       QWidget* parent)
    : QDialog(parent), base_ccy_(base_ccy.isEmpty() ? ui::formatting::display_currency() : base_ccy.toUpper()),
      track_cash_(track_cash) {
    setWindowTitle(tr("Cash Movement"));
    setStyleSheet(ledger_dialog_qss(ui::colors::INFO()));
    setMinimumWidth(ui::metrics::width(420));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(10);
    layout->addWidget(ldg_title(tr("RECORD CASH MOVEMENT"), ui::colors::INFO()));

    auto* form = new QFormLayout;
    form->setSpacing(8);
    type_cb_ = new QComboBox;
    type_cb_->addItem(tr("Deposit (money in)"), portfolio::txn::kDeposit);
    type_cb_->addItem(tr("Withdrawal (money out)"), portfolio::txn::kWithdrawal);
    type_cb_->addItem(tr("Fee (custody, platform, account)"), portfolio::txn::kFee);
    type_cb_->addItem(tr("Tax payment"), portfolio::txn::kTax);
    type_cb_->addItem(tr("Interest received"), portfolio::txn::kInterest);
    form->addRow(tr("Type:"), type_cb_);

    amount_edit_ = new QLineEdit;
    amount_edit_->setPlaceholderText(tr("e.g. 5000"));
    form->addRow(tr("Amount:"), amount_edit_);

    ccy_cb_ = new QComboBox;
    ccy_cb_->setEditable(true);
    ccy_cb_->setInsertPolicy(QComboBox::NoInsert);
    ccy_cb_->addItems(ldg_common_currencies(base_ccy_, QString()));
    ccy_cb_->setCurrentText(base_ccy_);
    form->addRow(tr("Currency:"), ccy_cb_);

    fx_edit_ = new QLineEdit;
    fx_label_ = new QLabel(tr("FX to %1:").arg(base_ccy_));
    form->addRow(fx_label_, fx_edit_);

    withheld_edit_ = new QLineEdit;
    withheld_edit_->setPlaceholderText(QStringLiteral("0"));
    withheld_label_ = new QLabel(tr("Tax withheld:"));
    form->addRow(withheld_label_, withheld_edit_);

    symbol_cb_ = new QComboBox;
    symbol_cb_->addItem(tr("(none)"), QString());
    for (const QString& s : symbols)
        symbol_cb_->addItem(s, s);
    symbol_label_ = new QLabel(tr("Related holding:"));
    form->addRow(symbol_label_, symbol_cb_);

    date_edit_ = ldg_date_edit(QDate::currentDate());
    form->addRow(tr("Date:"), date_edit_);

    notes_edit_ = new QLineEdit;
    notes_edit_->setPlaceholderText(tr("Optional note"));
    form->addRow(tr("Notes:"), notes_edit_);
    layout->addLayout(form);

    effect_ = ldg_hint(QString());
    layout->addWidget(effect_);
    layout->addStretch();

    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    auto* cancel = ldg_button(tr("CANCEL"), false, ui::colors::INFO());
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(cancel);
    ok_btn_ = ldg_button(tr("RECORD"), true, ui::colors::INFO());
    ok_btn_->setDefault(true);
    connect(ok_btn_, &QPushButton::clicked, this, [this]() {
        const QString err = QString("border:1px solid %1;").arg(ui::colors::DANGER());
        if (!(ledger_parse_number(amount_edit_->text()) > 0)) {
            amount_edit_->setStyleSheet(err);
            amount_edit_->setFocus();
            return;
        }
        if (!ldg_valid_ccy(ccy_cb_->currentText().trimmed())) {
            ccy_cb_->setStyleSheet(err);
            ccy_cb_->setFocus();
            return;
        }
        if (fx_edit_->isEnabled() && !fx_edit_->text().trimmed().isEmpty() &&
            !(ledger_parse_number(fx_edit_->text()) > 0)) {
            fx_edit_->setStyleSheet(err);
            fx_edit_->setFocus();
            return;
        }
        accept();
    });
    buttons->addWidget(ok_btn_);
    layout->addLayout(buttons);

    connect(type_cb_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { update_state(); });
    connect(ccy_cb_, &QComboBox::currentTextChanged, this, [this]() { update_state(); });
    connect(amount_edit_, &QLineEdit::textEdited, this, [this]() { amount_edit_->setStyleSheet(QString()); });
    update_state();
    amount_edit_->setFocus();
}

void CashMovementDialog::update_state() {
    namespace tx = portfolio::txn;
    const QString type = type_cb_->currentData().toString();
    const QString major = core::market::major_currency(ccy_cb_->currentText().trimmed());
    const bool foreign = !major.isEmpty() && major != base_ccy_;
    fx_edit_->setEnabled(foreign);
    fx_edit_->setPlaceholderText(foreign ? tr("%1 per 1 %2 — blank = daily rate of the date").arg(base_ccy_, major)
                                         : tr("1 — same currency as the portfolio"));
    const bool interest = type == tx::kInterest;
    withheld_edit_->setVisible(interest);
    withheld_label_->setVisible(interest);
    const bool related = type == tx::kFee || type == tx::kTax;
    symbol_cb_->setVisible(related);
    symbol_label_->setVisible(related);

    QString effect;
    if (type == tx::kDeposit || type == tx::kWithdrawal) {
        effect = tr("An external flow: time- and money-weighted returns treat it as money moved in or out, not as "
                    "performance.");
        if (!track_cash_)
            effect += QLatin1Char(' ') + tr("Cash tracking is OFF for this portfolio — the movement is recorded, but "
                                            "cash is not counted in NAV until you turn tracking on in Settings.");
    } else if (type == tx::kInterest) {
        effect = tr("Income: counted in total return and in the INCOME view (net of the tax withheld).");
    } else {
        effect = tr("A cost: counted in total fees / taxes and reduces the return.");
    }
    effect_->setText(effect);
}

portfolio::Transaction CashMovementDialog::transaction() const {
    portfolio::Transaction t;
    t.transaction_type = type_cb_->currentData().toString();
    t.quantity = ledger_parse_number(amount_edit_->text());
    t.price = 1.0;
    t.total_value = t.quantity;
    t.currency = ccy_cb_->currentText().trimmed();
    if (core::market::major_currency(t.currency) == base_ccy_ && !core::market::is_minor_unit(t.currency))
        t.fx_rate = 1.0;
    else
        t.fx_rate = fx_edit_->isEnabled() ? std::max(0.0, ledger_parse_number(fx_edit_->text())) : 0.0;
    if (t.transaction_type == portfolio::txn::kInterest)
        t.taxes = std::max(0.0, ledger_parse_number(withheld_edit_->text()));
    if (symbol_cb_->isVisible())
        t.symbol = symbol_cb_->currentData().toString();
    t.transaction_date = date_edit_->date().toString(Qt::ISODate);
    t.notes = notes_edit_->text().trimmed();
    return t;
}

// ── SplitDialog ──────────────────────────────────────────────────────────────

SplitDialog::SplitDialog(const QVector<portfolio::HoldingWithQuote>& holdings, const QString& preset_symbol,
                         QWidget* parent)
    : QDialog(parent), holdings_(holdings) {
    setWindowTitle(tr("Record Split"));
    setStyleSheet(ledger_dialog_qss(ui::colors::WARNING()));
    setMinimumWidth(ui::metrics::width(420));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(10);
    layout->addWidget(ldg_title(tr("RECORD STOCK SPLIT"), ui::colors::WARNING()));
    layout->addWidget(ldg_hint(tr("A split changes the number of units, not the cost: the position's cost basis is "
                                  "kept and the average cost per unit is divided by the factor.")));

    auto* form = new QFormLayout;
    form->setSpacing(8);
    symbol_cb_ = new QComboBox;
    for (const auto& h : holdings_)
        symbol_cb_->addItem(h.name.isEmpty() ? h.symbol : QStringLiteral("%1 — %2").arg(h.symbol, h.name), h.symbol);
    const int idx = symbol_cb_->findData(preset_symbol);
    if (idx >= 0)
        symbol_cb_->setCurrentIndex(idx);
    form->addRow(tr("Holding:"), symbol_cb_);

    auto* ratio_row = new QHBoxLayout;
    new_spin_ = new QDoubleSpinBox;
    new_spin_->setRange(0.0001, 100000);
    new_spin_->setDecimals(4);
    new_spin_->setValue(2);
    old_spin_ = new QDoubleSpinBox;
    old_spin_->setRange(0.0001, 100000);
    old_spin_->setDecimals(4);
    old_spin_->setValue(1);
    ratio_row->addWidget(new_spin_);
    ratio_row->addWidget(new QLabel(tr("new for")));
    ratio_row->addWidget(old_spin_);
    ratio_row->addWidget(new QLabel(tr("old")));
    form->addRow(tr("Ratio:"), ratio_row);

    date_edit_ = ldg_date_edit(QDate::currentDate());
    form->addRow(tr("Effective (ex-)date:"), date_edit_);
    notes_edit_ = new QLineEdit;
    notes_edit_->setPlaceholderText(tr("Optional note"));
    form->addRow(tr("Notes:"), notes_edit_);
    layout->addLayout(form);

    preview_ = ldg_hint(QString());
    layout->addWidget(preview_);
    layout->addStretch();

    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    auto* cancel = ldg_button(tr("CANCEL"), false, ui::colors::WARNING());
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(cancel);
    ok_btn_ = ldg_button(tr("RECORD SPLIT"), true, ui::colors::WARNING());
    ok_btn_->setDefault(true);
    connect(ok_btn_, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(ok_btn_);
    layout->addLayout(buttons);

    connect(symbol_cb_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { update_preview(); });
    connect(new_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) { update_preview(); });
    connect(old_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) { update_preview(); });
    update_preview();
}

void SplitDialog::update_preview() {
    const double f = factor();
    const QString sym = symbol();
    const portfolio::HoldingWithQuote* h = nullptr;
    for (const auto& x : holdings_)
        if (x.symbol == sym)
            h = &x;
    const bool valid = !sym.isEmpty() && std::isfinite(f) && f > 0 && std::abs(f - 1.0) > 1e-12;
    ok_btn_->setEnabled(valid);
    if (!valid) {
        preview_->setText(sym.isEmpty() ? tr("No holding selected.") : tr("A factor of 1 changes nothing."));
        return;
    }
    QString text = f >= 1 ? tr("%1-for-1 split (factor %2).").arg(pfmt::qty(f), pfmt::qty(f))
                          : tr("1-for-%1 reverse split (factor %2).").arg(pfmt::qty(1.0 / f), pfmt::qty(f));
    if (h) {
        const QString qccy = h->price_currency.isEmpty() ? h->currency : h->price_currency;
        text += QLatin1Char(' ') + tr("If the position is unchanged until then: %1 → %2 units; average cost %3 → %4.")
                                       .arg(pfmt::qty(h->quantity), pfmt::qty(h->quantity * f),
                                            pfmt::price_sym(h->avg_buy_price, qccy, h->symbol),
                                            pfmt::price_sym(h->avg_buy_price / f, qccy, h->symbol));
    }
    preview_->setText(text);
}

QString SplitDialog::symbol() const {
    return symbol_cb_->currentData().toString();
}
double SplitDialog::factor() const {
    return old_spin_->value() > 0 ? new_spin_->value() / old_spin_->value() : portfolio::kAbsent;
}
QString SplitDialog::date() const {
    return date_edit_->date().toString(Qt::ISODate);
}
QString SplitDialog::notes() const {
    const QString n = notes_edit_->text().trimmed();
    return n.isEmpty() ? tr("%1-for-%2 split").arg(pfmt::qty(new_spin_->value()), pfmt::qty(old_spin_->value())) : n;
}

// ── PortfolioSettingsDialog ──────────────────────────────────────────────────

PortfolioSettingsDialog::PortfolioSettingsDialog(const portfolio::Portfolio& p, QWidget* parent)
    : QDialog(parent), p_(p) {
    setWindowTitle(tr("Portfolio Settings"));
    setStyleSheet(ledger_dialog_qss(ui::colors::AMBER()));
    setMinimumWidth(ui::metrics::width(520));
    auto& svc = services::PortfolioService::instance();
    const QString base = p.currency.toUpper();

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(10);
    layout->addWidget(ldg_title(tr("PORTFOLIO SETTINGS — %1").arg(p.name.toUpper()), ui::colors::AMBER()));
    layout->addWidget(ldg_hint(tr("Base (reporting) currency: %1 — %2. Every value, return and risk figure is "
                                  "converted into it. It is fixed when the portfolio is created.")
                                   .arg(base, ui::formatting::currency_name(base))));

    // ── Benchmark ────────────────────────────────────────────────────────────
    auto* bench_box = new QGroupBox(tr("Benchmark"));
    auto* bv = new QVBoxLayout(bench_box);
    const QString def = services::PortfolioService::default_benchmark_for_currency(base);
    bench_default_ = new QRadioButton(
        tr("Default for %1: %2%3")
            .arg(base, def,
                 services::PortfolioService::default_benchmark_is_total_return(base) ? tr(" (total return)")
                                                                                     : tr(" (price index)")));
    bench_custom_ = new QRadioButton(tr("Custom — one symbol, or a fixed-weight blend rebalanced daily"));
    bv->addWidget(bench_default_);
    bv->addWidget(bench_custom_);
    blend_table_ = new QTableWidget(0, 2);
    blend_table_->setHorizontalHeaderLabels({tr("Symbol (Yahoo)"), tr("Weight %")});
    blend_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    blend_table_->verticalHeader()->setVisible(false);
    blend_table_->setMinimumHeight(ui::metrics::height(110));
    bv->addWidget(blend_table_);
    auto* row_btns = new QHBoxLayout;
    add_row_btn_ = ldg_button(tr("+ ADD"), false, ui::colors::AMBER());
    remove_row_btn_ = ldg_button(tr("REMOVE"), false, ui::colors::AMBER());
    row_btns->addWidget(add_row_btn_);
    row_btns->addWidget(remove_row_btn_);
    row_btns->addStretch();
    bv->addLayout(row_btns);
    bench_status_ = ldg_hint(QString());
    bv->addWidget(bench_status_);
    layout->addWidget(bench_box);

    const auto parts = services::PortfolioService::parse_benchmark(p.benchmark);
    if (p.benchmark.trimmed().isEmpty() || parts.isEmpty()) {
        bench_default_->setChecked(true);
        add_blend_row(def, 100);
    } else {
        bench_custom_->setChecked(true);
        for (const auto& pr : parts)
            add_blend_row(pr.first, pr.second * 100.0);
    }
    connect(add_row_btn_, &QPushButton::clicked, this, [this]() {
        add_blend_row(QString(), 0);
        bench_custom_->setChecked(true);
        update_state();
    });
    connect(remove_row_btn_, &QPushButton::clicked, this, [this]() {
        const int r = blend_table_->currentRow();
        if (r >= 0 && blend_table_->rowCount() > 1)
            blend_table_->removeRow(r);
        update_state();
    });
    connect(blend_table_, &QTableWidget::cellChanged, this, [this](int, int) {
        bench_custom_->setChecked(true);
        update_state();
    });
    connect(bench_default_, &QRadioButton::toggled, this, [this]() { update_state(); });

    // ── Risk-free rate ───────────────────────────────────────────────────────
    auto* rf_box = new QGroupBox(tr("Risk-free rate (Sharpe, Sortino, alpha)"));
    auto* rv = new QVBoxLayout(rf_box);
    portfolio::Portfolio auto_p = p;
    auto_p.risk_free_override.reset();
    const auto auto_rf = svc.risk_free_for(auto_p);
    rf_auto_ = new QCheckBox(tr("Automatic for %1").arg(base));
    rf_auto_label_ = ldg_hint(auto_rf.known ? tr("Automatic rate now: %1 — %2.")
                                                  .arg(pfmt::fpct(auto_rf.rate, 2, false), auto_rf.source)
                                            : tr("No automatic rate is available for %1: 0 % is used and the figures "
                                                 "are flagged. Set an override to use your own.")
                                                  .arg(base));
    auto* rf_row = new QHBoxLayout;
    rf_row->addWidget(new QLabel(tr("Override:")));
    rf_spin_ = new QDoubleSpinBox;
    rf_spin_->setRange(-4.99, 99.99);
    rf_spin_->setDecimals(3);
    rf_spin_->setSuffix(QStringLiteral(" %"));
    rf_spin_->setValue(p.risk_free_override ? *p.risk_free_override * 100.0 : (auto_rf.known ? auto_rf.rate * 100.0 : 0));
    rf_row->addWidget(rf_spin_);
    rf_row->addStretch();
    rv->addWidget(rf_auto_);
    rv->addWidget(rf_auto_label_);
    rv->addLayout(rf_row);
    rf_auto_->setChecked(!p.risk_free_override.has_value());
    connect(rf_auto_, &QCheckBox::toggled, this, [this]() { update_state(); });
    layout->addWidget(rf_box);

    // ── Cash, lots ───────────────────────────────────────────────────────────
    auto* book_box = new QGroupBox(tr("Book keeping"));
    auto* form = new QFormLayout(book_box);
    form->setSpacing(8);
    cash_cb_ = new QCheckBox(tr("Track cash (deposits / withdrawals are the external flows; cash counts in NAV)"));
    cash_cb_->setChecked(p.track_cash);
    form->addRow(cash_cb_);
    lot_cb_ = new QComboBox;
    lot_cb_->addItem(tr("FIFO — first in, first out"), QStringLiteral("FIFO"));
    lot_cb_->addItem(tr("LIFO — last in, first out"), QStringLiteral("LIFO"));
    lot_cb_->addItem(tr("HIFO — highest cost first"), QStringLiteral("HIFO"));
    lot_cb_->addItem(tr("Average cost (pooled)"), QStringLiteral("AVERAGE"));
    lot_cb_->addItem(tr("Specific lot (chosen per sale)"), QStringLiteral("SPECIFIC"));
    const int li = lot_cb_->findData(portfolio::lot_method_key(portfolio::lot_method_from_key(p.lot_method)));
    lot_cb_->setCurrentIndex(li >= 0 ? li : 0);
    form->addRow(tr("Tax-lot relief:"), lot_cb_);
    lot_hint_ = ldg_hint(QString());
    form->addRow(lot_hint_);
    lt_spin_ = new QSpinBox;
    lt_spin_->setRange(1, 36500);
    lt_spin_->setSuffix(tr(" days"));
    lt_spin_->setValue(p.long_term_days > 0 ? p.long_term_days : 365);
    form->addRow(tr("Long-term after:"), lt_spin_);
    form->addRow(ldg_hint(tr("Lots held longer than this count as long-term gains (365 days is the US rule; "
                             "use your jurisdiction's).")));
    layout->addWidget(book_box);

    recompute_note_ = ldg_hint(QString());
    recompute_note_->setStyleSheet(QString("color:%1; font-size:%2px;").arg(ui::colors::WARNING(), ui::fonts::px(-4)));
    layout->addWidget(recompute_note_);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    auto* cancel = ldg_button(tr("CANCEL"), false, ui::colors::AMBER());
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(cancel);
    ok_btn_ = ldg_button(tr("SAVE"), true, ui::colors::AMBER());
    ok_btn_->setDefault(true);
    connect(ok_btn_, &QPushButton::clicked, this, [this]() {
        QString err;
        if (bench_custom_->isChecked() && blend_spec(&err).isEmpty()) {
            bench_status_->setText(err);
            return;
        }
        accept();
    });
    buttons->addWidget(ok_btn_);
    layout->addLayout(buttons);

    connect(cash_cb_, &QCheckBox::toggled, this, [this]() { update_state(); });
    connect(lot_cb_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { update_state(); });
    connect(lt_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { update_state(); });
    update_state();
}

void PortfolioSettingsDialog::add_blend_row(const QString& symbol, double weight_pct) {
    QSignalBlocker block(blend_table_);
    const int r = blend_table_->rowCount();
    blend_table_->insertRow(r);
    blend_table_->setItem(r, 0, new QTableWidgetItem(symbol));
    auto* w = new QTableWidgetItem(weight_pct > 0 ? QString::number(weight_pct, 'g', 6) : QString());
    w->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    blend_table_->setItem(r, 1, w);
}

QString PortfolioSettingsDialog::blend_spec(QString* error) const {
    QStringList parts;
    QStringList syms;
    double total = 0;
    for (int r = 0; r < blend_table_->rowCount(); ++r) {
        const QString sym = blend_table_->item(r, 0) ? blend_table_->item(r, 0)->text().trimmed().toUpper() : QString();
        const QString wt = blend_table_->item(r, 1) ? blend_table_->item(r, 1)->text().trimmed() : QString();
        if (sym.isEmpty())
            continue;
        const double w = wt.isEmpty() ? 0 : ledger_parse_number(wt);
        syms << sym;
        parts << QStringLiteral("%1:%2").arg(sym, QString::number(w, 'g', 8));
        total += w;
    }
    if (syms.isEmpty()) {
        if (error)
            *error = tr("Enter at least one benchmark symbol.");
        return {};
    }
    const QString spec = syms.size() == 1 ? syms.first() : parts.join(QLatin1Char(','));
    if (syms.size() > 1 && !(total > 0)) {
        if (error)
            *error = tr("Give the blend's symbols positive weights.");
        return {};
    }
    if (services::PortfolioService::parse_benchmark(spec).isEmpty()) {
        if (error)
            *error = tr("Not a valid benchmark: %1").arg(spec);
        return {};
    }
    return spec;
}

void PortfolioSettingsDialog::update_state() {
    const bool custom = bench_custom_->isChecked();
    blend_table_->setEnabled(custom);
    add_row_btn_->setEnabled(custom);
    remove_row_btn_->setEnabled(custom);
    if (custom) {
        QString err;
        const QString spec = blend_spec(&err);
        if (spec.isEmpty()) {
            bench_status_->setText(err);
        } else {
            const auto parts = services::PortfolioService::parse_benchmark(spec);
            QStringList shown;
            for (const auto& pr : parts)
                shown << QStringLiteral("%1 %2").arg(pr.first, pfmt::fpct(pr.second, 1, false));
            bench_status_->setText(parts.size() > 1 ? tr("Blend (weights normalised): %1. Returns are converted into "
                                                         "the base currency.")
                                                          .arg(shown.join(QStringLiteral(" + ")))
                                                    : tr("Returns are converted into the base currency."));
        }
    } else {
        bench_status_->setText(tr("Benchmark returns are converted into the base currency."));
    }
    rf_spin_->setEnabled(!rf_auto_->isChecked());

    const QString method = lot_cb_->currentData().toString();
    if (method == QLatin1String("SPECIFIC"))
        lot_hint_->setText(tr("Sales consume the lot named on the SELL row (edit the transaction to choose it); any "
                              "remainder is taken first in, first out."));
    else if (method == QLatin1String("AVERAGE"))
        lot_hint_->setText(tr("Every unit carries the pooled average cost; holding periods are counted first in, first "
                              "out."));
    else
        lot_hint_->setText(tr("Decides which purchase a sale is matched with — and so the realised gain and its "
                              "short / long-term split. No transaction is changed."));

    QStringList recompute;
    if (cash_cb_->isChecked() != p_.track_cash)
        recompute << tr("cash tracking changes what NAV includes and which rows are flows — the NAV history is "
                        "replayed");
    if (method != portfolio::lot_method_key(portfolio::lot_method_from_key(p_.lot_method)) ||
        lt_spin_->value() != p_.long_term_days)
        recompute << tr("realised gains and tax lots are recomputed");
    recompute_note_->setText(recompute.isEmpty() ? QString()
                                                 : tr("On save: %1. Your transactions are not modified.")
                                                       .arg(recompute.join(QStringLiteral("; "))));
}

QString PortfolioSettingsDialog::benchmark() const {
    if (!bench_custom_->isChecked())
        return {};
    return blend_spec(nullptr);
}
std::optional<double> PortfolioSettingsDialog::risk_free_override() const {
    if (rf_auto_->isChecked())
        return std::nullopt;
    return rf_spin_->value() / 100.0;
}
bool PortfolioSettingsDialog::track_cash() const {
    return cash_cb_->isChecked();
}
QString PortfolioSettingsDialog::lot_method() const {
    return lot_cb_->currentData().toString();
}
int PortfolioSettingsDialog::long_term_days() const {
    return lt_spin_->value();
}

} // namespace fincept::screens
