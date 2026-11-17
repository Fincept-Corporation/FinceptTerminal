// src/screens/portfolio/PortfolioDialogs.cpp
#include "screens/portfolio/PortfolioDialogs.h"

#include "core/logging/Logger.h"
#include "core/market/InstrumentCurrency.h"
#include "screens/portfolio/PortfolioLedgerDialogs.h"
#include "services/file_manager/FileManagerService.h"
#include "services/portfolio/PortfolioLots.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/NumberFormat.h"
#include "services/markets/MarketSearchService.h"
#include "ui/theme/Theme.h"

#include <QApplication>
#include <QDate>
#include <QDateEdit>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidgetItem>
#include <QLocale>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QSet>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

// QString::toDouble() happily parses "nan" and "inf". Both slip straight past the
// "<= 0" validation in the dialogs below (NaN compares false with everything) and
// would be written into a position as its quantity/price, poisoning every total.
// Treat them as unparsable (0), which the existing checks already reject.
static double dlg_finite_or_zero(double v) {
    return std::isfinite(v) ? v : 0.0;
}

// Parse a user-typed number. "1234.5" (C locale) first, then the system locale
// ("1.234,5" in de_DE), then with thousands separators stripped ("1,234.50").
// Plain QString::toDouble() rejected "1,234.50" as 0 and the dialog then
// claimed the price was not greater than 0.
static double dlg_parse_number(const QString& text) {
    const QString t = text.trimmed().remove(QLatin1Char(' '));
    bool ok = false;
    double v = t.toDouble(&ok);
    if (!ok)
        v = QLocale().toDouble(t, &ok);
    if (!ok)
        v = QString(t).remove(QLatin1Char(',')).toDouble(&ok);
    return ok ? dlg_finite_or_zero(v) : 0.0;
}

// Trade-date picker shared by the BUY / SELL dialogs: defaults to today and
// cannot be set in the future.
static QDateEdit* dlg_make_trade_date_edit() {
    auto* d = new QDateEdit(QDate::currentDate());
    d->setCalendarPopup(true);
    d->setDisplayFormat("yyyy-MM-dd");
    d->setMaximumDate(QDate::currentDate());
    return d;
}

// Pre-fill text for an editable number: at least 2 decimals (reads like money) but
// up to 8, trailing zeros trimmed. The edit dialog used a fixed 'f', 2, so opening a
// transaction for 0.00123456 BTC showed "0.00" - and saving it (even to fix only the
// notes) wrote the rounded value back, or was blocked by the "> 0" check.
static QString dlg_edit_number(double v) {
    QString s = QString::number(v, 'f', 8);
    const qsizetype dot = s.indexOf(QLatin1Char('.'));
    if (dot >= 0) {
        qsizetype keep = s.size();
        while (keep > dot + 3 && s[keep - 1] == QLatin1Char('0'))
            --keep;
        s.truncate(keep);
    }
    return s;
}

// ── CreatePortfolioDialog ────────────────────────────────────────────────────

CreatePortfolioDialog::CreatePortfolioDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Create Portfolio"));
    setMinimumSize(380, 260);
    setStyleSheet(QString("QDialog { background:%1; color:%2; }"
                          "QLabel { color:%3; font-size:" + fincept::ui::fonts::px(-3) + "px; }"
                          "QLineEdit { background:%4; color:%2; border:1px solid %5;"
                          "  padding:6px 10px; font-size:" + fincept::ui::fonts::px(-2) + "px; }"
                          "QLineEdit:focus { border-color:%6; }"
                          "QComboBox { background:%4; color:%2; border:1px solid %5;"
                          "  padding:6px 10px; font-size:" + fincept::ui::fonts::px(-2) + "px; }"
                          "QComboBox::drop-down { border:none; }"
                          "QComboBox QAbstractItemView { background:%4; color:%2;"
                          "  selection-background-color:%7; selection-color:%8; }")
                      .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_SECONDARY(),
                           ui::colors::BG_BASE(), ui::colors::BORDER_MED(), ui::colors::AMBER(),
                           ui::colors::AMBER_DIM(), ui::colors::TEXT_ON_ACCENT()));

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->setContentsMargins(20, 16, 20, 16);

    // Header
    title_label_ = new QLabel(tr("CREATE NEW PORTFOLIO"));
    title_label_->setStyleSheet(
        QString("color:%1; font-size:" + fincept::ui::fonts::px(-1) + "px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    layout->addWidget(title_label_);

    // Form
    auto* form = new QFormLayout;
    form->setSpacing(8);
    form->setLabelAlignment(Qt::AlignRight);

    name_edit_ = new QLineEdit;
    name_edit_->setPlaceholderText(tr("My Portfolio"));
    name_row_label_ = new QLabel(tr("Name:"));
    form->addRow(name_row_label_, name_edit_);

    owner_edit_ = new QLineEdit;
    owner_edit_->setPlaceholderText(tr("Your name"));
    owner_row_label_ = new QLabel(tr("Owner:"));
    form->addRow(owner_row_label_, owner_edit_);

    currency_cb_ = new QComboBox;
    // Base (reporting) currency: every fiat currency of the shared currency table —
    // minor units (GBp), metals and crypto are not reporting currencies — with the
    // user's display currency preselected.
    QStringList currencies;
    for (const QString& code : ui::formatting::known_currency_codes()) {
        ui::formatting::CurrencySpec spec;
        if (ui::formatting::find_currency(code, &spec) && !spec.crypto && !spec.metal && spec.minor_divisor == 1 &&
            spec.major == spec.code)
            currencies << code;
    }
    currency_cb_->addItems(currencies);
    for (int i = 0; i < currency_cb_->count(); ++i)
        currency_cb_->setItemData(i, ui::formatting::currency_name(currency_cb_->itemText(i)), Qt::ToolTipRole);
    if (const int pre = currency_cb_->findText(ui::formatting::display_currency()); pre >= 0)
        currency_cb_->setCurrentIndex(pre);
    currency_row_label_ = new QLabel(tr("Currency:"));
    form->addRow(currency_row_label_, currency_cb_);

    layout->addLayout(form);
    layout->addStretch();

    // Buttons
    auto* btn_layout = new QHBoxLayout;
    btn_layout->addStretch();

    cancel_btn_ = new QPushButton(tr("CANCEL"));
    cancel_btn_->setMinimumSize(90, 30);
    cancel_btn_->setCursor(Qt::PointingHandCursor);
    cancel_btn_->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                                       "  font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; }"
                                       "QPushButton:hover { background:%3; color:%4; }")
                                   .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED(), ui::colors::BG_HOVER(),
                                        ui::colors::TEXT_PRIMARY()));
    connect(cancel_btn_, &QPushButton::clicked, this, &QDialog::reject);
    btn_layout->addWidget(cancel_btn_);

    create_btn_ = new QPushButton(tr("CREATE"));
    create_btn_->setMinimumSize(90, 30);
    create_btn_->setCursor(Qt::PointingHandCursor);
    create_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%3; border:none;"
                                       "  font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; }"
                                       "QPushButton:hover { background:%2; }")
                                   .arg(ui::colors::AMBER(), ui::colors::WARNING(), ui::colors::TEXT_ON_ACCENT()));
    connect(create_btn_, &QPushButton::clicked, this, [this]() {
        // Was a silent no-op on an empty name — the button simply did nothing
        // and the user had no idea why.
        if (name_edit_->text().trimmed().isEmpty()) {
            name_edit_->setPlaceholderText(tr("A portfolio name is required"));
            name_edit_->setStyleSheet(QString("border:1px solid %1;").arg(ui::colors::NEGATIVE()));
            name_edit_->setFocus();
            return;
        }
        accept();
    });
    btn_layout->addWidget(create_btn_);

    layout->addLayout(btn_layout);

    // Explicit tab order — the default order follows construction, which puts
    // the buttons before the combo here.
    setTabOrder(name_edit_, owner_edit_);
    setTabOrder(owner_edit_, currency_cb_);
    setTabOrder(currency_cb_, create_btn_);
    setTabOrder(create_btn_, cancel_btn_);

    name_edit_->setAccessibleName(tr("Portfolio name"));
    owner_edit_->setAccessibleName(tr("Portfolio owner"));
    currency_cb_->setAccessibleName(tr("Reporting currency"));

    create_btn_->setDefault(true); // Enter creates
    name_edit_->setFocus();
}

void CreatePortfolioDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void CreatePortfolioDialog::retranslateUi() {
    setWindowTitle(tr("Create Portfolio"));
    if (title_label_)
        title_label_->setText(tr("CREATE NEW PORTFOLIO"));
    if (name_row_label_)
        name_row_label_->setText(tr("Name:"));
    if (owner_row_label_)
        owner_row_label_->setText(tr("Owner:"));
    if (currency_row_label_)
        currency_row_label_->setText(tr("Currency:"));
    if (name_edit_)
        name_edit_->setPlaceholderText(tr("My Portfolio"));
    if (owner_edit_)
        owner_edit_->setPlaceholderText(tr("Your name"));
    if (cancel_btn_)
        cancel_btn_->setText(tr("CANCEL"));
    if (create_btn_)
        create_btn_->setText(tr("CREATE"));
}

QString CreatePortfolioDialog::name() const {
    return name_edit_->text().trimmed();
}
QString CreatePortfolioDialog::owner() const {
    return owner_edit_->text().trimmed();
}
QString CreatePortfolioDialog::currency() const {
    return currency_cb_->currentText();
}

// ── ConfirmDeleteDialog ──────────────────────────────────────────────────────

ConfirmDeleteDialog::ConfirmDeleteDialog(const QString& portfolio_name, QWidget* parent)
    : QDialog(parent), portfolio_name_(portfolio_name) {
    setWindowTitle(tr("Delete Portfolio"));
    setMinimumSize(340, 160);
    setStyleSheet(
        QString("QDialog { background:%1; color:%2; }").arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY()));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(12);

    icon_label_ = new QLabel(tr("\u26A0  DELETE PORTFOLIO"));
    icon_label_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-1) + "px; font-weight:700;").arg(ui::colors::NEGATIVE()));
    layout->addWidget(icon_label_);

    msg_label_ = new QLabel(tr("Are you sure you want to delete \"%1\"?\n"
                               "This will remove all holdings and transactions.")
                                .arg(portfolio_name_));
    msg_label_->setWordWrap(true);
    msg_label_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px;").arg(ui::colors::TEXT_SECONDARY()));
    layout->addWidget(msg_label_);

    layout->addStretch();

    auto* btn_layout = new QHBoxLayout;
    btn_layout->addStretch();

    cancel_btn_ = new QPushButton(tr("CANCEL"));
    cancel_btn_->setMinimumSize(90, 30);
    cancel_btn_->setCursor(Qt::PointingHandCursor);
    cancel_btn_->setStyleSheet(
        QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                "  font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; }"
                "QPushButton:hover { background:%3; }")
            .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED(), ui::colors::BG_HOVER()));
    connect(cancel_btn_, &QPushButton::clicked, this, &QDialog::reject);
    btn_layout->addWidget(cancel_btn_);

    delete_btn_ = new QPushButton(tr("DELETE"));
    delete_btn_->setProperty("fincept_agent_risk", "delete");
    delete_btn_->setMinimumSize(90, 30);
    delete_btn_->setCursor(Qt::PointingHandCursor);
    delete_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:none;"
                                       "  font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; }"
                                       "QPushButton:hover { background:%1; }")
                                   .arg(ui::colors::NEGATIVE(), ui::colors::TEXT_ON_ACCENT()));
    connect(delete_btn_, &QPushButton::clicked, this, &QDialog::accept);
    btn_layout->addWidget(delete_btn_);

    layout->addLayout(btn_layout);

    // CANCEL is the default on a destructive dialog: Enter must not delete.
    setTabOrder(cancel_btn_, delete_btn_);
    cancel_btn_->setDefault(true);
    cancel_btn_->setFocus();
    delete_btn_->setAccessibleName(tr("Confirm deletion"));
}

void ConfirmDeleteDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void ConfirmDeleteDialog::retranslateUi() {
    setWindowTitle(tr("Delete Portfolio"));
    if (icon_label_)
        icon_label_->setText(tr("\u26A0  DELETE PORTFOLIO"));
    if (msg_label_)
        msg_label_->setText(tr("Are you sure you want to delete \"%1\"?\n"
                               "This will remove all holdings and transactions.")
                                .arg(portfolio_name_));
    if (cancel_btn_)
        cancel_btn_->setText(tr("CANCEL"));
    if (delete_btn_)
        delete_btn_->setText(tr("DELETE"));
}

// ── AddAssetDialog ───────────────────────────────────────────────────────────

static constexpr int kAssetSearchDebounceMs = 300;
static constexpr int kAssetSearchLimit = 10;
// Rows fetched per search before keeping the holdable instrument types (see fire_search).
static constexpr int kAssetSearchFetchLimit = 50;

// Height of one search-result row: 30px at the default font size, growing with it.
static int dlg_search_row_height() {
    return std::max(30, ui::fonts::font_px(-2) + 18);
}

AddAssetDialog::AddAssetDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Add Asset"));
    setMinimumSize(400, 296);
    setStyleSheet(QString("QDialog { background:%1; color:%2; }"
                          "QLabel { color:%3; font-size:" + fincept::ui::fonts::px(-3) + "px; }"
                          "QLineEdit { background:%4; color:%2; border:1px solid %5;"
                          "  padding:6px 10px; font-size:" + fincept::ui::fonts::px(-2) + "px; }"
                          "QLineEdit:focus { border-color:%6; }")
                      .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_SECONDARY(),
                           ui::colors::BG_BASE(), ui::colors::BORDER_MED(), ui::colors::AMBER()));

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->setContentsMargins(20, 16, 20, 16);

    title_label_ = new QLabel(tr("BUY ASSET"));
    title_label_->setStyleSheet(
        QString("color:%1; font-size:" + fincept::ui::fonts::px(-1) + "px; font-weight:700; letter-spacing:1px;").arg(ui::colors::POSITIVE()));
    layout->addWidget(title_label_);

    // Hint label under title
    hint_label_ = new QLabel(tr("Type a ticker or company name to search"));
    hint_label_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px;").arg(ui::colors::TEXT_TERTIARY()));
    layout->addWidget(hint_label_);

    auto* form = new QFormLayout;
    form->setSpacing(8);

    symbol_edit_ = new QLineEdit;
    symbol_edit_->setPlaceholderText(tr("e.g. AAPL, Apple, Reliance…"));
    symbol_edit_->installEventFilter(this);
    symbol_row_label_ = new QLabel(tr("Symbol:"));
    form->addRow(symbol_row_label_, symbol_edit_);

    quantity_edit_ = new QLineEdit;
    quantity_edit_->setPlaceholderText(tr("e.g. 10"));
    quantity_row_label_ = new QLabel(tr("Quantity:"));
    form->addRow(quantity_row_label_, quantity_edit_);

    price_edit_ = new QLineEdit;
    price_edit_->setPlaceholderText(tr("e.g. 150.00"));
    price_row_label_ = new QLabel(tr("Price:"));
    form->addRow(price_row_label_, price_edit_);

    // Trade date: every BUY used to be stamped "now", so back-entered trades
    // rebuilt a NAV history that started on the day they were typed in.
    date_edit_ = dlg_make_trade_date_edit();
    date_row_label_ = new QLabel(tr("Trade date:"));
    form->addRow(date_row_label_, date_edit_);

    layout->addLayout(form);

    // Fees, taxes, trade currency and trade-date FX (v055 ledger fields).
    costs_ = new TradeCostsBox(this);
    costs_->set_context(QString(), QString()); // the caller sets the portfolio's base (set_base_currency)
    layout->addWidget(costs_);
    connect(symbol_edit_, &QLineEdit::textChanged, this, [this](const QString& text) {
        const QString sym = text.trimmed().toUpper();
        if (sym.length() >= 1)
            costs_->set_instrument_currency(core::market::currency_for_symbol(sym));
    });
    layout->addStretch();

    auto* btn_layout = new QHBoxLayout;
    btn_layout->addStretch();

    cancel_btn_ = new QPushButton(tr("CANCEL"));
    cancel_btn_->setMinimumSize(80, 28);
    cancel_btn_->setCursor(Qt::PointingHandCursor);
    cancel_btn_->setStyleSheet(
        QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                "  font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; }"
                "QPushButton:hover { background:%3; }")
            .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED(), ui::colors::BG_HOVER()));
    connect(cancel_btn_, &QPushButton::clicked, this, &QDialog::reject);
    btn_layout->addWidget(cancel_btn_);

    add_btn_ = new QPushButton(tr("ADD"));
    add_btn_->setMinimumSize(80, 28);
    add_btn_->setCursor(Qt::PointingHandCursor);
    add_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:none;"
                                    "  font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; }"
                                    "QPushButton:hover { background:%1; }")
                                .arg(ui::colors::POSITIVE(), ui::colors::TEXT_ON_ACCENT()));
    connect(add_btn_, &QPushButton::clicked, this, [this]() {
        // Point at the offending field instead of doing nothing.
        const QString err_style = QString("border:1px solid %1;").arg(ui::colors::NEGATIVE());
        if (symbol_edit_->text().trimmed().isEmpty()) {
            symbol_edit_->setStyleSheet(err_style);
            symbol_edit_->setPlaceholderText(tr("A ticker is required"));
            symbol_edit_->setFocus();
            return;
        }
        if (quantity() <= 0) {
            quantity_edit_->setStyleSheet(err_style);
            quantity_edit_->setPlaceholderText(tr("Quantity must be greater than 0"));
            quantity_edit_->setFocus();
            return;
        }
        if (price() <= 0) {
            price_edit_->setStyleSheet(err_style);
            price_edit_->setPlaceholderText(tr("Price must be greater than 0"));
            price_edit_->setFocus();
            return;
        }
        QString why;
        if (costs_ && !costs_->validate(&why)) {
            costs_->set_expanded(true);
            QMessageBox::warning(this, tr("Add Asset"), why);
            return;
        }
        accept();
    });
    btn_layout->addWidget(add_btn_);

    layout->addLayout(btn_layout);

    search_frame_ = new QFrame(this);
    search_frame_->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    search_frame_->setAttribute(Qt::WA_ShowWithoutActivating);
    search_frame_->setObjectName("assetSearchFrame");
    search_frame_->setStyleSheet(
        QString("QFrame#assetSearchFrame { background:%1; border:1px solid %2; border-top:none; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_MED()));
    search_frame_->hide();

    // Hide dropdown when app loses focus
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget*, QWidget* now) {
        if (search_frame_->isVisible()) {
            if (!now || (now != symbol_edit_ && !search_frame_->isAncestorOf(now) && now != search_list_)) {
                search_frame_->hide();
            }
        }
    });

    auto* frame_layout = new QVBoxLayout(search_frame_);
    frame_layout->setContentsMargins(0, 0, 0, 0);
    frame_layout->setSpacing(0);

    search_list_ = new QListWidget(search_frame_);
    search_list_->setStyleSheet(QString("QListWidget { background:%1; border:none; outline:none; }"
                                        "QListWidget::item { padding:0; border:none; background:transparent; }"
                                        "QListWidget::item:selected { background:%2; border-left:3px solid %3; }"
                                        "QScrollBar:vertical { background:%1; width:6px; margin:0; }"
                                        "QScrollBar::handle:vertical { background:%2; min-height:15px; }"
                                        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }")
                                    .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::AMBER()));
    search_list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    search_list_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    search_list_->setCursor(Qt::PointingHandCursor);
    frame_layout->addWidget(search_list_);

    connect(search_list_, &QListWidget::itemClicked, this,
            [this](QListWidgetItem* item) { select_result(item->data(Qt::UserRole).toString()); });

    // ── Debounce timer ────────────────────────────────────────────────────────
    search_debounce_ = new QTimer(this);
    search_debounce_->setSingleShot(true);
    search_debounce_->setInterval(kAssetSearchDebounceMs);
    connect(search_debounce_, &QTimer::timeout, this, [this]() {
        if (!pending_query_.isEmpty())
            fire_search(pending_query_);
    });

    setTabOrder(symbol_edit_, quantity_edit_);
    setTabOrder(quantity_edit_, price_edit_);
    setTabOrder(price_edit_, date_edit_);
    setTabOrder(date_edit_, add_btn_);
    setTabOrder(add_btn_, cancel_btn_);
    symbol_edit_->setAccessibleName(tr("Ticker symbol"));
    quantity_edit_->setAccessibleName(tr("Quantity to buy"));
    price_edit_->setAccessibleName(tr("Purchase price per share"));
    add_btn_->setDefault(true);

    connect(symbol_edit_, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (selecting_)
            return;
        // Clear any validation highlight as soon as the user edits the field.
        symbol_edit_->setStyleSheet(QString());
        const QString q = text.trimmed();
        if (q.length() < 2) {
            search_debounce_->stop();
            search_frame_->hide();
            return;
        }
        schedule_search(q);
    });

    symbol_edit_->setFocus();
}

void AddAssetDialog::schedule_search(const QString& query) {
    pending_query_ = query;
    search_debounce_->start(); // restarts the 300ms window
}

void AddAssetDialog::fire_search(const QString& query) {
    auto& svc = fincept::services::MarketSearchService::instance();
    const QString rid = QString::number(reinterpret_cast<quintptr>(this), 16);
    if (!search_connected_) {
        connect(&svc, &fincept::services::MarketSearchService::results_ready, this,
                [this](const QString& request_id, const QString& q,
                       const QList<fincept::services::MarketSearchService::Item>& items) {
                    const QString my_rid = QString::number(reinterpret_cast<quintptr>(this), 16);
                    if (request_id != my_rid)
                        return;
                    if (pending_query_ != q)
                        return;
                    // The search used to send type=stock, which hides every ETF and fund
                    // (stored as type "fund": SPY, QQQ, GLD, NIFTYBEES) and ADR ("dr").
                    // No type filter is sent now; keep the instrument types a holding is
                    // priced as, in the backend's ranking order.
                    static const QSet<QString> kHoldableTypes{QStringLiteral("stock"), QStringLiteral("fund"),
                                                              QStringLiteral("dr")};
                    QList<fincept::services::MarketSearchService::Item> holdable;
                    for (const auto& it : items) {
                        if (it.type.isEmpty() || kHoldableTypes.contains(it.type.toLower()))
                            holdable.append(it);
                        if (holdable.size() >= kAssetSearchLimit)
                            break;
                    }
                    // A previous failure may have left the warning hint up.
                    hint_label_->setText(tr("Type a ticker or company name to search"));
                    hint_label_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px;")
                                                   .arg(ui::colors::TEXT_TERTIARY()));
                    show_results(holdable);
                });
        // A failed lookup (offline, 5xx, timeout) used to look exactly like "still typing".
        connect(&svc, &fincept::services::MarketSearchService::search_failed, this,
                [this](const QString& request_id, const QString& q, const QString& /*reason*/) {
                    const QString my_rid = QString::number(reinterpret_cast<quintptr>(this), 16);
                    if (request_id != my_rid || pending_query_ != q)
                        return;
                    hint_label_->setText(
                        tr("Search unavailable - type the exact ticker (e.g. AAPL, RELIANCE.NS) and press ADD"));
                    hint_label_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px;")
                                                   .arg(ui::colors::WARNING()));
                    search_frame_->hide();
                });
        search_connected_ = true;
    }
    svc.search(query, /*type=*/QString(), kAssetSearchFetchLimit, rid);
}

void AddAssetDialog::show_results(const QList<fincept::services::MarketSearchService::Item>& results) {
    search_list_->clear();

    if (results.isEmpty()) {
        auto* item = new QListWidgetItem(search_list_);
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        auto* row = new QWidget(search_list_);
        row->setStyleSheet("background:transparent;");
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(10, 6, 10, 6);
        auto* lbl = new QLabel(tr("No results found"));
        lbl->setStyleSheet(
            QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; background:transparent;")
                .arg(ui::colors::TEXT_TERTIARY()));
        rl->addWidget(lbl);
        item->setSizeHint(QSize(0, dlg_search_row_height() - 2));
        search_list_->setItemWidget(item, row);
        position_dropdown();
        search_frame_->show();
        return;
    }

    for (const auto& entry : results) {
        const QString& sym = entry.symbol;
        const QString& name = entry.name;
        const QString& exch = entry.exchange;
        const QString type = entry.type.isEmpty() ? QStringLiteral("stock") : entry.type;
        if (sym.isEmpty())
            continue;

        // Resolve yfinance-compatible ticker (same logic as CommandBar)
        QString yf_sym = sym;
        static const QHash<QString, QString> suffix_map = {
            {"NSE", ".NS"}, {"BSE", ".BO"},        {"HKEX", ".HK"}, {"TSE", ".T"},  {"KRX", ".KS"}, {"SGX", ".SI"},
            {"ASX", ".AX"}, {"IDX", ".JK"},        {"MYX", ".KL"},  {"SET", ".BK"}, {"PSE", ".PS"}, {"XETR", ".DE"},
            {"FWB", ".F"},  {"LSE", ".L"},         {"BME", ".MC"},  {"MIL", ".MI"}, {"SIX", ".SW"}, {"TSX", ".TO"},
            {"TSXV", ".V"}, {"BMFBOVESPA", ".SA"}, {"BIST", ".IS"}, {"EGX", ".CA"},
        };
        const auto it = suffix_map.find(exch.toUpper());
        if (it != suffix_map.end())
            yf_sym = sym + it.value();

        auto* item = new QListWidgetItem(search_list_);
        item->setData(Qt::UserRole, yf_sym);

        auto* row = new QWidget(search_list_);
        row->setStyleSheet("background:transparent;");
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(10, 4, 10, 4);
        hl->setSpacing(8);

        auto* sym_lbl = new QLabel(yf_sym);
        sym_lbl->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-2) + "px; font-weight:700;"
                                       "background:transparent;")
                                   .arg(ui::colors::TEXT_PRIMARY()));
        // 100px at the default font size: the symbol column lines up across rows.
        sym_lbl->setFixedWidth(std::max(100, ui::fonts::font_px(-2) * 100 / 12));

        auto* name_lbl = new QLabel(name);
        name_lbl->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; background:transparent;")
                                    .arg(ui::colors::TEXT_SECONDARY()));
        name_lbl->setMaximumWidth(180);

        hl->addWidget(sym_lbl);
        hl->addWidget(name_lbl, 1);

        if (!exch.isEmpty()) {
            auto* exch_lbl = new QLabel(exch);
            exch_lbl->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px;"
                                            "background:transparent;")
                                        .arg(ui::colors::TEXT_TERTIARY()));
            hl->addWidget(exch_lbl);
        }

        auto* type_lbl = new QLabel(type.toUpper());
        type_lbl->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700;"
                                        "background:%2;"
                                        "padding:1px 4px;")
                                    .arg(ui::colors::AMBER(), ui::colors::ACCENT_BG()));
        hl->addWidget(type_lbl);

        item->setSizeHint(QSize(0, dlg_search_row_height()));
        search_list_->setItemWidget(item, row);
    }

    if (search_list_->count() > 0)
        search_list_->setCurrentRow(0);

    position_dropdown();
    search_frame_->show();
    search_frame_->raise();
}

void AddAssetDialog::preset(const QString& symbol, double price) {
    if (symbol.isEmpty())
        return;
    // selecting_ suppresses the textChanged handler so setting the field does
    // not immediately fire a search for a ticker we already resolved.
    selecting_ = true;
    symbol_edit_->setText(symbol);
    selecting_ = false;
    search_debounce_->stop();
    search_frame_->hide();
    if (price > 0.0 && std::isfinite(price))
        price_edit_->setText(dlg_edit_number(price));
    quantity_edit_->setFocus();
    quantity_edit_->selectAll();
}

void AddAssetDialog::select_result(const QString& sym) {
    selecting_ = true;
    symbol_edit_->setText(sym);
    selecting_ = false;
    search_frame_->hide();
    search_debounce_->stop();
    quantity_edit_->setFocus();
    quantity_edit_->selectAll();
}

void AddAssetDialog::position_dropdown() {
    // Place the dropdown directly below the symbol_edit_ row
    const QPoint origin = symbol_edit_->mapToGlobal(QPoint(0, symbol_edit_->height()));
    const int w = symbol_edit_->width() + 60; // a bit wider to show exchange column
    const int rows = std::min(search_list_->count(), 6);
    const int h = rows * dlg_search_row_height() + 2;
    search_frame_->setGeometry(origin.x(), origin.y(), w, h);
}

bool AddAssetDialog::eventFilter(QObject* obj, QEvent* event) {
    if (obj == symbol_edit_ && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (search_frame_->isVisible()) {
            // count() > 0: the wrap-around below takes `% count()`, and a result set
            // whose rows were all skipped (empty symbols) leaves the list empty.
            if (ke->key() == Qt::Key_Down && search_list_->count() > 0) {
                const int next = (search_list_->currentRow() + 1) % search_list_->count();
                search_list_->setCurrentRow(next);
                return true;
            }
            if (ke->key() == Qt::Key_Up && search_list_->count() > 0) {
                const int prev = (search_list_->currentRow() - 1 + search_list_->count()) % search_list_->count();
                search_list_->setCurrentRow(prev);
                return true;
            }
            if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
                auto* cur = search_list_->currentItem();
                if (cur) {
                    select_result(cur->data(Qt::UserRole).toString());
                    return true;
                }
            }
            if (ke->key() == Qt::Key_Escape) {
                search_frame_->hide();
                return true;
            }
        }
    }
    return QDialog::eventFilter(obj, event);
}

void AddAssetDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void AddAssetDialog::moveEvent(QMoveEvent* event) {
    QDialog::moveEvent(event);
    if (search_frame_ && search_frame_->isVisible()) {
        position_dropdown();
    }
}

void AddAssetDialog::retranslateUi() {
    setWindowTitle(tr("Add Asset"));
    if (title_label_)
        title_label_->setText(tr("BUY ASSET"));
    if (hint_label_)
        hint_label_->setText(tr("Type a ticker or company name to search"));
    if (symbol_row_label_)
        symbol_row_label_->setText(tr("Symbol:"));
    if (quantity_row_label_)
        quantity_row_label_->setText(tr("Quantity:"));
    if (price_row_label_)
        price_row_label_->setText(tr("Price:"));
    if (date_row_label_)
        date_row_label_->setText(tr("Trade date:"));
    if (symbol_edit_)
        symbol_edit_->setPlaceholderText(tr("e.g. AAPL, Apple, Reliance…"));
    if (quantity_edit_)
        quantity_edit_->setPlaceholderText(tr("e.g. 10"));
    if (price_edit_)
        price_edit_->setPlaceholderText(tr("e.g. 150.00"));
    if (cancel_btn_)
        cancel_btn_->setText(tr("CANCEL"));
    if (add_btn_)
        add_btn_->setText(tr("ADD"));
}

QString AddAssetDialog::symbol() const {
    return symbol_edit_->text().trimmed().toUpper();
}
double AddAssetDialog::quantity() const {
    return dlg_parse_number(quantity_edit_->text());
}
double AddAssetDialog::price() const {
    return dlg_parse_number(price_edit_->text());
}
QString AddAssetDialog::date() const {
    return date_edit_->date().toString(Qt::ISODate);
}
void AddAssetDialog::set_base_currency(const QString& base_ccy) {
    base_ccy_ = base_ccy;
    if (costs_)
        costs_->set_context(base_ccy, symbol().isEmpty() ? QString() : core::market::currency_for_symbol(symbol()));
}

// ── SellAssetDialog ──────────────────────────────────────────────────────────

SellAssetDialog::SellAssetDialog(const QString& symbol, double held_qty, QWidget* parent)
    : QDialog(parent), symbol_(symbol), held_qty_(held_qty) {
    setWindowTitle(tr("Sell Asset"));
    setMinimumSize(340, 256);
    setStyleSheet(QString("QDialog { background:%1; color:%2; }"
                          "QLabel { color:%3; font-size:" + fincept::ui::fonts::px(-3) + "px; }"
                          "QLineEdit { background:%4; color:%2; border:1px solid %5;"
                          "  padding:6px 10px; font-size:" + fincept::ui::fonts::px(-2) + "px; }"
                          "QLineEdit:focus { border-color:%6; }")
                      .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_SECONDARY(),
                           ui::colors::BG_BASE(), ui::colors::BORDER_MED(), ui::colors::AMBER()));

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->setContentsMargins(20, 16, 20, 16);

    title_label_ = new QLabel(tr("SELL %1").arg(symbol_));
    title_label_->setStyleSheet(
        QString("color:%1; font-size:" + fincept::ui::fonts::px(-1) + "px; font-weight:700; letter-spacing:1px;").arg(ui::colors::NEGATIVE()));
    layout->addWidget(title_label_);

    held_label_ = new QLabel(tr("Currently holding: %1 shares").arg(ui::formatting::quantity(held_qty_, 8)));
    held_label_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px;").arg(ui::colors::TEXT_TERTIARY()));
    layout->addWidget(held_label_);

    auto* form = new QFormLayout;
    form->setSpacing(8);

    quantity_edit_ = new QLineEdit;
    quantity_edit_->setPlaceholderText(tr("Max %1").arg(ui::formatting::quantity(held_qty_, 8)));
    quantity_row_label_ = new QLabel(tr("Quantity:"));
    form->addRow(quantity_row_label_, quantity_edit_);

    price_edit_ = new QLineEdit;
    price_edit_->setPlaceholderText(tr("Sell price"));
    price_row_label_ = new QLabel(tr("Price:"));
    form->addRow(price_row_label_, price_edit_);

    date_edit_ = dlg_make_trade_date_edit();
    date_row_label_ = new QLabel(tr("Trade date:"));
    form->addRow(date_row_label_, date_edit_);

    layout->addLayout(form);

    // Fees, taxes, trade currency and trade-date FX (v055 ledger fields).
    costs_ = new TradeCostsBox(this);
    // The caller sets the portfolio's base right after (set_currency_context).
    costs_->set_context(QString(), core::market::currency_for_symbol(symbol_));
    layout->addWidget(costs_);
    layout->addStretch();

    auto* btn_layout = new QHBoxLayout;
    btn_layout->addStretch();

    cancel_btn_ = new QPushButton(tr("CANCEL"));
    cancel_btn_->setMinimumSize(80, 28);
    cancel_btn_->setCursor(Qt::PointingHandCursor);
    cancel_btn_->setStyleSheet(
        QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                "  font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; }"
                "QPushButton:hover { background:%3; }")
            .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED(), ui::colors::BG_HOVER()));
    connect(cancel_btn_, &QPushButton::clicked, this, &QDialog::reject);
    btn_layout->addWidget(cancel_btn_);

    sell_btn_ = new QPushButton(tr("SELL"));
    sell_btn_->setMinimumSize(80, 28);
    sell_btn_->setCursor(Qt::PointingHandCursor);
    sell_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:none;"
                                     "  font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; }"
                                     "QPushButton:hover { background:%1; }")
                                 .arg(ui::colors::NEGATIVE(), ui::colors::TEXT_ON_ACCENT()));
    connect(sell_btn_, &QPushButton::clicked, this, [this]() {
        // Silent no-op previously — in particular, over-selling looked like a
        // dead button rather than a rejected input.
        const QString err_style = QString("border:1px solid %1;").arg(ui::colors::NEGATIVE());
        if (quantity() <= 0) {
            quantity_edit_->setStyleSheet(err_style);
            quantity_edit_->setPlaceholderText(tr("Quantity must be greater than 0"));
            quantity_edit_->setFocus();
            return;
        }
        if (quantity() > held_qty_) {
            quantity_edit_->setStyleSheet(err_style);
            quantity_edit_->clear();
            quantity_edit_->setPlaceholderText(tr("You only hold %1").arg(ui::formatting::quantity(held_qty_, 8)));
            quantity_edit_->setFocus();
            return;
        }
        if (price() <= 0) {
            price_edit_->setStyleSheet(err_style);
            price_edit_->setPlaceholderText(tr("Price must be greater than 0"));
            price_edit_->setFocus();
            return;
        }
        QString why;
        if (costs_ && !costs_->validate(&why)) {
            costs_->set_expanded(true);
            QMessageBox::warning(this, tr("Sell Asset"), why);
            return;
        }
        accept();
    });
    btn_layout->addWidget(sell_btn_);

    layout->addLayout(btn_layout);

    setTabOrder(quantity_edit_, price_edit_);
    setTabOrder(price_edit_, date_edit_);
    setTabOrder(date_edit_, sell_btn_);
    setTabOrder(sell_btn_, cancel_btn_);
    quantity_edit_->setAccessibleName(tr("Quantity to sell"));
    price_edit_->setAccessibleName(tr("Sale price per share"));
    sell_btn_->setDefault(true);

    connect(quantity_edit_, &QLineEdit::textEdited, this,
            [this](const QString&) { quantity_edit_->setStyleSheet(QString()); });
    connect(price_edit_, &QLineEdit::textEdited, this,
            [this](const QString&) { price_edit_->setStyleSheet(QString()); });

    quantity_edit_->setFocus();
}

void SellAssetDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void SellAssetDialog::retranslateUi() {
    setWindowTitle(tr("Sell Asset"));
    if (title_label_)
        title_label_->setText(tr("SELL %1").arg(symbol_));
    if (held_label_)
        held_label_->setText(tr("Currently holding: %1 shares").arg(ui::formatting::quantity(held_qty_, 8)));
    if (quantity_row_label_)
        quantity_row_label_->setText(tr("Quantity:"));
    if (price_row_label_)
        price_row_label_->setText(tr("Price:"));
    if (quantity_edit_)
        quantity_edit_->setPlaceholderText(tr("Max %1").arg(ui::formatting::quantity(held_qty_, 8)));
    if (price_edit_)
        price_edit_->setPlaceholderText(tr("Sell price"));
    if (date_row_label_)
        date_row_label_->setText(tr("Trade date:"));
    if (cancel_btn_)
        cancel_btn_->setText(tr("CANCEL"));
    if (sell_btn_)
        sell_btn_->setText(tr("SELL"));
}

double SellAssetDialog::quantity() const {
    return dlg_parse_number(quantity_edit_->text());
}
double SellAssetDialog::price() const {
    return dlg_parse_number(price_edit_->text());
}
QString SellAssetDialog::date() const {
    return date_edit_->date().toString(Qt::ISODate);
}
void SellAssetDialog::preset_price(double price) {
    // Full precision of the quote (a 0.00001234 price used to be prefilled as "0.00").
    if (price > 0.0 && std::isfinite(price))
        price_edit_->setText(dlg_edit_number(price));
}
void SellAssetDialog::set_currency_context(const QString& base_ccy, const QString& instrument_ccy) {
    if (costs_)
        costs_->set_context(base_ccy, instrument_ccy.isEmpty() ? core::market::currency_for_symbol(symbol_)
                                                               : instrument_ccy);
}

// ── ImportPortfolioDialog ────────────────────────────────────────────────────

ImportPortfolioDialog::ImportPortfolioDialog(const QVector<portfolio::Portfolio>& portfolios, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("Import Portfolio"));
    setMinimumSize(420, 330);
    setStyleSheet(QString("QDialog { background:%1; color:%2; }"
                          "QLabel { color:%3; font-size:" + fincept::ui::fonts::px(-3) + "px; }"
                          "QLineEdit { background:%4; color:%2; border:1px solid %5;"
                          "  padding:6px 10px; font-size:" + fincept::ui::fonts::px(-2) + "px; }"
                          "QLineEdit:focus { border-color:%6; }"
                          "QRadioButton { color:%2; font-size:" + fincept::ui::fonts::px(-3) + "px; spacing:6px; }"
                          "QRadioButton::indicator { width:14px; height:14px; }"
                          "QComboBox { background:%4; color:%2; border:1px solid %5;"
                          "  padding:6px 10px; font-size:" + fincept::ui::fonts::px(-2) + "px; }"
                          "QComboBox::drop-down { border:none; }"
                          "QComboBox QAbstractItemView { background:%4; color:%2; }")
                      .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_SECONDARY(),
                           ui::colors::BG_BASE(), ui::colors::BORDER_MED(), ui::colors::AMBER()));

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->setContentsMargins(20, 16, 20, 16);

    title_label_ = new QLabel(tr("IMPORT PORTFOLIO"));
    title_label_->setStyleSheet(
        QString("color:%1; font-size:" + fincept::ui::fonts::px(-1) + "px; font-weight:700; letter-spacing:1px;").arg(ui::colors::CYAN()));
    layout->addWidget(title_label_);

    // File picker
    auto* file_row = new QHBoxLayout;
    file_edit_ = new QLineEdit;
    file_edit_->setPlaceholderText(tr("Select JSON file..."));
    file_edit_->setReadOnly(true);
    file_row->addWidget(file_edit_, 1);

    browse_btn_ = new QPushButton(tr("BROWSE"));
    browse_btn_->setMinimumSize(80, 30);
    browse_btn_->setCursor(Qt::PointingHandCursor);
    browse_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:none;"
                                       "  font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; }"
                                       "QPushButton:hover { background:%1; }")
                                   .arg(ui::colors::CYAN(), ui::colors::TEXT_ON_ACCENT()));
    connect(browse_btn_, &QPushButton::clicked, this, &ImportPortfolioDialog::browse_file);
    file_row->addWidget(browse_btn_);

    layout->addLayout(file_row);

    // Demo JSON download hint
    auto* demo_row = new QHBoxLayout;
    demo_hint_label_ = new QLabel(tr("Need a template? Download the demo portfolio JSON:"));
    demo_hint_label_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px;").arg(ui::colors::TEXT_TERTIARY()));
    demo_row->addWidget(demo_hint_label_, 1);

    demo_dl_btn_ = new QPushButton(tr("DOWNLOAD DEMO"));
    demo_dl_btn_->setMinimumSize(120, 24);
    demo_dl_btn_->setCursor(Qt::PointingHandCursor);
    demo_dl_btn_->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %1;"
                                        "  font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700; letter-spacing:0.3px; }"
                                        "QPushButton:hover { background:%1; color:%2; }")
                                    .arg(ui::colors::CYAN(), ui::colors::TEXT_ON_ACCENT()));
    connect(demo_dl_btn_, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getSaveFileName(this, tr("Save Demo Portfolio JSON"), "demo_portfolio.json",
                                                    tr("JSON Files (*.json)"));
        if (path.isEmpty())
            return;

        QJsonArray txns;
        struct H {
            const char* symbol;
            double qty;
            double price;
            const char* sector;
        };
        static const H demo[] = {
            {"AAPL", 15, 178.50, "Technology"},
            {"MSFT", 12, 375.20, "Technology"},
            {"GOOGL", 8, 141.80, "Technology"},
            {"NVDA", 10, 480.00, "Technology"},
            {"AMZN", 6, 178.25, "Consumer Discretionary"},
            {"TSLA", 5, 245.00, "Consumer Discretionary"},
            {"JPM", 20, 195.50, "Financials"},
            {"JNJ", 15, 155.75, "Healthcare"},
            {"XOM", 25, 105.30, "Energy"},
            {"V", 10, 280.00, "Financials"},
            {"UNH", 4, 525.60, "Healthcare"},
            {"PG", 12, 158.90, "Consumer Staples"},
        };
        // The template MUST be the terminal's own export format - 'portfolio_name' plus
        // a 'transactions' array - because that is all PortfolioService::import_json()
        // accepts. This used to write a holdings-only file ('name' + 'holdings'), which
        // the importer rejects outright ("Holdings-only snapshots are not supported"),
        // so the template offered right here could never be imported. Each holding
        // becomes a BUY a year ago (which also gives the charts a year of history to
        // backfill); the per-transaction 'sector' is honoured by the importer.
        const QString buy_date = QDate::currentDate().addYears(-1).toString("yyyy-MM-dd");
        for (const auto& h : demo) {
            QJsonObject o;
            o["date"] = buy_date;
            o["symbol"] = h.symbol;
            o["type"] = "BUY";
            o["quantity"] = h.qty;
            o["price"] = h.price;
            o["currency"] = "USD"; // the sample prices are US-dollar prices (US listings)
            o["sector"] = h.sector;
            txns.append(o);
        }

        QJsonObject root;
        root["format_version"] = "1.0";
        root["portfolio_name"] = "Demo Portfolio";
        root["owner"] = "Fincept User";
        // Reported in the user's own currency; the USD holdings convert at the daily FX.
        root["currency"] = ui::formatting::display_currency();
        root["transactions"] = txns;

        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
            f.close();
            services::FileManagerService::instance().import_file(path, "portfolio");
            QMessageBox::information(this, tr("Demo JSON Saved"),
                                     tr("Demo portfolio JSON saved.\nYou can now import it using the BROWSE button."));
        } else {
            LOG_WARN("PortfolioDialogs", logf("Demo portfolio file could not be written",
                                              {{"path", log_path(path)}, {"error", f.errorString()}}));
            QMessageBox::warning(this, tr("Save Failed"), tr("Could not write to: %1").arg(path));
        }
    });
    demo_row->addWidget(demo_dl_btn_);
    layout->addLayout(demo_row);

    // Import mode
    mode_label_ = new QLabel(tr("IMPORT MODE"));
    mode_label_->setStyleSheet(
        QString("color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700; letter-spacing:0.5px;").arg(ui::colors::TEXT_TERTIARY()));
    layout->addWidget(mode_label_);

    new_radio_ = new QRadioButton(tr("Create new portfolio from file"));
    new_radio_->setChecked(true);
    layout->addWidget(new_radio_);

    merge_radio_ = new QRadioButton(tr("Merge transactions into existing portfolio:"));
    layout->addWidget(merge_radio_);

    target_cb_ = new QComboBox;
    target_cb_->setEnabled(false);
    for (const auto& p : portfolios)
        target_cb_->addItem(QString("%1 (%2)").arg(p.name, p.currency), p.id);
    layout->addWidget(target_cb_);

    connect(merge_radio_, &QRadioButton::toggled, target_cb_, &QComboBox::setEnabled);

    status_label_ = new QLabel;
    status_label_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px;").arg(ui::colors::TEXT_TERTIARY()));
    layout->addWidget(status_label_);

    layout->addStretch();

    // Buttons
    auto* btn_layout = new QHBoxLayout;
    btn_layout->addStretch();

    cancel_btn_ = new QPushButton(tr("CANCEL"));
    cancel_btn_->setMinimumSize(80, 28);
    cancel_btn_->setCursor(Qt::PointingHandCursor);
    cancel_btn_->setStyleSheet(
        QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                "  font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; }"
                "QPushButton:hover { background:%3; }")
            .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED(), ui::colors::BG_HOVER()));
    connect(cancel_btn_, &QPushButton::clicked, this, &QDialog::reject);
    btn_layout->addWidget(cancel_btn_);

    import_btn_ = new QPushButton(tr("IMPORT"));
    import_btn_->setMinimumSize(80, 28);
    import_btn_->setCursor(Qt::PointingHandCursor);
    import_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:none;"
                                       "  font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; }"
                                       "QPushButton:hover { background:%1; }")
                                   .arg(ui::colors::CYAN(), ui::colors::TEXT_ON_ACCENT()));
    connect(import_btn_, &QPushButton::clicked, this, [this]() {
        if (file_edit_->text().isEmpty()) {
            status_label_->setText(tr("Choose a JSON file first — press BROWSE."));
            status_label_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px;").arg(ui::colors::NEGATIVE()));
            return;
        }
        if (merge_radio_->isChecked() && target_cb_->count() == 0) {
            status_label_->setText(tr("No existing portfolio to merge into. Choose 'Create new' instead."));
            status_label_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px;").arg(ui::colors::NEGATIVE()));
            return;
        }
        accept();
    });
    btn_layout->addWidget(import_btn_);

    layout->addLayout(btn_layout);

    setTabOrder(browse_btn_, demo_dl_btn_);
    setTabOrder(demo_dl_btn_, new_radio_);
    setTabOrder(new_radio_, merge_radio_);
    setTabOrder(merge_radio_, target_cb_);
    setTabOrder(target_cb_, import_btn_);
    setTabOrder(import_btn_, cancel_btn_);
    browse_btn_->setAccessibleName(tr("Choose a portfolio JSON file"));
    target_cb_->setAccessibleName(tr("Portfolio to merge into"));
    import_btn_->setDefault(true);
}

void ImportPortfolioDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void ImportPortfolioDialog::retranslateUi() {
    setWindowTitle(tr("Import Portfolio"));
    if (title_label_)
        title_label_->setText(tr("IMPORT PORTFOLIO"));
    if (file_edit_)
        file_edit_->setPlaceholderText(tr("Select JSON file..."));
    if (browse_btn_)
        browse_btn_->setText(tr("BROWSE"));
    if (demo_hint_label_)
        demo_hint_label_->setText(tr("Need a template? Download the demo portfolio JSON:"));
    if (demo_dl_btn_)
        demo_dl_btn_->setText(tr("DOWNLOAD DEMO"));
    if (mode_label_)
        mode_label_->setText(tr("IMPORT MODE"));
    if (new_radio_)
        new_radio_->setText(tr("Create new portfolio from file"));
    if (merge_radio_)
        merge_radio_->setText(tr("Merge transactions into existing portfolio:"));
    if (cancel_btn_)
        cancel_btn_->setText(tr("CANCEL"));
    if (import_btn_)
        import_btn_->setText(tr("IMPORT"));
}

void ImportPortfolioDialog::browse_file() {
    QString path =
        QFileDialog::getOpenFileName(this, tr("Select Portfolio JSON"), QString(), tr("JSON Files (*.json)"));
    if (!path.isEmpty()) {
        file_edit_->setText(path);
        status_label_->setText(tr("File selected: %1").arg(QFileInfo(path).fileName()));
    }
}

QString ImportPortfolioDialog::file_path() const {
    return file_edit_->text();
}

portfolio::ImportMode ImportPortfolioDialog::mode() const {
    return new_radio_->isChecked() ? portfolio::ImportMode::New : portfolio::ImportMode::Merge;
}

QString ImportPortfolioDialog::merge_target_id() const {
    return target_cb_->currentData().toString();
}

// ── EditTransactionDialog ────────────────────────────────────────────────────

EditTransactionDialog::EditTransactionDialog(const portfolio::Transaction& txn, QWidget* parent)
    : QDialog(parent), txn_type_(txn.transaction_type.toUpper()), txn_symbol_(txn.symbol), original_(txn) {
    setWindowTitle(tr("Edit Transaction"));
    setMinimumSize(380, 280);
    setStyleSheet(QString("QDialog { background:%1; color:%2; }"
                          "QLabel { color:%3; font-size:" + fincept::ui::fonts::px(-3) + "px; }"
                          "QLineEdit { background:%4; color:%2; border:1px solid %5;"
                          "  padding:6px 10px; font-size:" + fincept::ui::fonts::px(-2) + "px; }"
                          "QLineEdit:focus { border-color:%6; }")
                      .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_SECONDARY(),
                           ui::colors::BG_BASE(), ui::colors::BORDER_MED(), ui::colors::AMBER()));

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->setContentsMargins(20, 16, 20, 16);

    title_label_ = new QLabel(tr("EDIT %1 — %2").arg(txn_type_, txn_symbol_));
    title_label_->setStyleSheet(
        QString("color:%1; font-size:" + fincept::ui::fonts::px(-1) + "px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    layout->addWidget(title_label_);

    auto* form = new QFormLayout;
    form->setSpacing(8);

    quantity_edit_ = new QLineEdit(dlg_edit_number(txn.quantity));
    quantity_row_label_ = new QLabel(tr("Quantity:"));
    form->addRow(quantity_row_label_, quantity_edit_);

    price_edit_ = new QLineEdit(dlg_edit_number(txn.price));
    price_row_label_ = new QLabel(tr("Price:"));
    form->addRow(price_row_label_, price_edit_);

    date_edit_ = new QDateEdit;
    date_edit_->setCalendarPopup(true);
    date_edit_->setDisplayFormat("yyyy-MM-dd");
    {
        // Rows written by add_asset() carry a full ISO date-time; parsing the
        // whole string as yyyy-MM-dd failed and silently reset the date to today,
        // so editing only the notes moved the trade to today.
        QDate d = QDate::fromString(txn.transaction_date.left(10), "yyyy-MM-dd");
        date_edit_->setDate(d.isValid() ? d : QDate::currentDate());
        date_edit_->setMaximumDate(QDate::currentDate());
    }
    date_edit_->setStyleSheet(
        QString("QDateEdit { background:%1; color:%2; border:1px solid %3;"
                "  padding:6px 10px; font-size:" + fincept::ui::fonts::px(-2) + "px; }"
                "QDateEdit:focus { border-color:%4; }"
                "QDateEdit::drop-down { border:none; width:18px; }"
                "QCalendarWidget { background:%1; color:%2; }")
            .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED(), ui::colors::AMBER()));
    date_row_label_ = new QLabel(tr("Date:"));
    form->addRow(date_row_label_, date_edit_);

    notes_edit_ = new QLineEdit(txn.notes);
    notes_edit_->setPlaceholderText(tr("Optional notes"));
    notes_row_label_ = new QLabel(tr("Notes:"));
    form->addRow(notes_row_label_, notes_edit_);

    // Type-aware fields: a SPLIT stores its factor in the quantity (price 0) and the
    // cash rows store their amount there (price 1) — the old dialog showed both as a
    // trade and refused to save a split (price 0) at all.
    const bool split = txn_type_ == portfolio::txn::kSplit;
    const bool cash_only = portfolio::txn::is_cash_only(txn_type_);
    if (split || cash_only) {
        price_edit_->setVisible(false);
        price_row_label_->setVisible(false);
    }

    layout->addLayout(form);

    if (!split) {
        costs_ = new TradeCostsBox(this);
        // The caller sets the portfolio's base right after (set_base_currency).
        costs_->set_context(QString(), cash_only ? QString() : core::market::currency_for_symbol(txn.symbol));
        if (txn_type_ == portfolio::txn::kDividend || txn_type_ == portfolio::txn::kInterest)
            costs_->set_taxes_label(tr("Withholding tax:"));
        costs_->set_values(txn.fees, txn.taxes, txn.currency, txn.fx_rate);
        layout->addWidget(costs_);
    }
    layout->addStretch();

    auto* btn_layout = new QHBoxLayout;
    btn_layout->addStretch();

    cancel_btn_ = new QPushButton(tr("CANCEL"));
    cancel_btn_->setMinimumSize(80, 28);
    cancel_btn_->setCursor(Qt::PointingHandCursor);
    cancel_btn_->setStyleSheet(
        QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                "  font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; }"
                "QPushButton:hover { background:%3; }")
            .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED(), ui::colors::BG_HOVER()));
    connect(cancel_btn_, &QPushButton::clicked, this, &QDialog::reject);
    btn_layout->addWidget(cancel_btn_);

    save_btn_ = new QPushButton(tr("SAVE"));
    save_btn_->setProperty("fincept_agent_risk", "destructive"); // overwrites a recorded transaction
    save_btn_->setMinimumSize(80, 28);
    save_btn_->setCursor(Qt::PointingHandCursor);
    save_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%3; border:none;"
                                     "  font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; }"
                                     "QPushButton:hover { background:%2; }")
                                 .arg(ui::colors::AMBER(), ui::colors::WARNING(), ui::colors::TEXT_ON_ACCENT()));
    connect(save_btn_, &QPushButton::clicked, this, [this]() {
        const QString err_style = QString("border:1px solid %1;").arg(ui::colors::NEGATIVE());
        if (quantity() <= 0) {
            quantity_edit_->setStyleSheet(err_style);
            quantity_edit_->setFocus();
            return;
        }
        if (txn_type_ == portfolio::txn::kSplit && std::abs(quantity() - 1.0) < 1e-12) {
            quantity_edit_->setStyleSheet(err_style);
            quantity_edit_->setFocus();
            return;
        }
        const bool needs_price = txn_type_ == portfolio::txn::kBuy || txn_type_ == portfolio::txn::kSell ||
                                 txn_type_ == portfolio::txn::kDividend;
        if (needs_price && price() <= 0) {
            price_edit_->setStyleSheet(err_style);
            price_edit_->setFocus();
            return;
        }
        QString why;
        if (costs_ && !costs_->validate(&why)) {
            costs_->set_expanded(true);
            QMessageBox::warning(this, tr("Edit Transaction"), why);
            return;
        }
        accept();
    });
    connect(quantity_edit_, &QLineEdit::textEdited, this,
            [this](const QString&) { quantity_edit_->setStyleSheet(QString()); });
    connect(price_edit_, &QLineEdit::textEdited, this,
            [this](const QString&) { price_edit_->setStyleSheet(QString()); });
    btn_layout->addWidget(save_btn_);

    layout->addLayout(btn_layout);

    setTabOrder(quantity_edit_, price_edit_);
    setTabOrder(price_edit_, date_edit_);
    setTabOrder(date_edit_, notes_edit_);
    setTabOrder(notes_edit_, save_btn_);
    setTabOrder(save_btn_, cancel_btn_);
    quantity_edit_->setAccessibleName(tr("Transaction quantity"));
    price_edit_->setAccessibleName(tr("Transaction price"));
    date_edit_->setAccessibleName(tr("Transaction date"));
    save_btn_->setDefault(true);

    retranslateUi(); // type-aware row labels
    quantity_edit_->setFocus();
}

void EditTransactionDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void EditTransactionDialog::retranslateUi() {
    setWindowTitle(tr("Edit Transaction"));
    if (title_label_)
        title_label_->setText(tr("EDIT %1 — %2").arg(txn_type_, txn_symbol_));
    if (quantity_row_label_)
        quantity_row_label_->setText(txn_type_ == portfolio::txn::kSplit ? tr("Factor (new per old):")
                                     : portfolio::txn::is_cash_only(txn_type_) ? tr("Amount:")
                                                                                : tr("Quantity:"));
    if (price_row_label_)
        price_row_label_->setText(tr("Price:"));
    if (date_row_label_)
        date_row_label_->setText(tr("Date:"));
    if (notes_row_label_)
        notes_row_label_->setText(tr("Notes:"));
    if (notes_edit_)
        notes_edit_->setPlaceholderText(tr("Optional notes"));
    if (cancel_btn_)
        cancel_btn_->setText(tr("CANCEL"));
    if (save_btn_)
        save_btn_->setText(tr("SAVE"));
}

double EditTransactionDialog::quantity() const {
    return dlg_parse_number(quantity_edit_->text());
}
double EditTransactionDialog::price() const {
    return dlg_parse_number(price_edit_->text());
}
QString EditTransactionDialog::date() const {
    return date_edit_->date().toString("yyyy-MM-dd");
}
QString EditTransactionDialog::notes() const {
    return notes_edit_->text().trimmed();
}
void EditTransactionDialog::set_base_currency(const QString& base_ccy) {
    if (!costs_)
        return;
    const bool cash_only = portfolio::txn::is_cash_only(txn_type_);
    costs_->set_context(base_ccy, cash_only ? base_ccy : core::market::currency_for_symbol(txn_symbol_));
    // set_context resets the currency to the instrument's: restore the row's own values.
    costs_->set_values(original_.fees, original_.taxes, original_.currency, original_.fx_rate);
}
bool EditTransactionDialog::costs_changed() const {
    if (!costs_)
        return false;
    auto differs = [](double a, double b) { return std::abs(a - b) > 1e-12 * std::max(1.0, std::abs(b)); };
    const QString ccy = costs_->currency();
    // An unchanged default currency on a legacy row (empty currency) is not an edit.
    const QString orig_ccy = original_.currency;
    const bool ccy_changed = !orig_ccy.isEmpty() ? ccy != orig_ccy
                                                 : (!ccy.isEmpty() && ccy != core::market::currency_for_symbol(txn_symbol_) &&
                                                    !portfolio::txn::is_cash_only(txn_type_));
    return differs(costs_->fees(), original_.fees) || differs(costs_->taxes(), original_.taxes) || ccy_changed ||
           differs(costs_->fx_rate(), original_.fx_rate);
}

// ── AddDividendDialog ─────────────────────────────────────────────────────────

static QString kDividendStyle(const QString& accent) {
    return QString("QDialog { background:%1; color:%2; }"
                   "QLabel { color:%3; font-size:" + fincept::ui::fonts::px(-3) + "px; }"
                   "QLineEdit, QDateEdit, QComboBox {"
                   "  background:%4; color:%2; border:1px solid %5;"
                   "  padding:5px 8px; font-size:" + fincept::ui::fonts::px(-2) + "px; }"
                   "QLineEdit:focus, QDateEdit:focus { border-color:%6; }"
                   "QComboBox::drop-down { border:none; }"
                   "QComboBox QAbstractItemView { background:%4; color:%2;"
                   "  selection-background-color:%6; selection-color:%7; }")
        .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_SECONDARY(), ui::colors::BG_BASE(),
             ui::colors::BORDER_MED(), accent, ui::colors::TEXT_ON_ACCENT());
}

AddDividendDialog::AddDividendDialog(const QStringList& symbols, QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Record Dividend"));
    setMinimumSize(360, 300);
    setStyleSheet(kDividendStyle(ui::colors::CYAN));

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->setContentsMargins(20, 16, 20, 16);

    title_label_ = new QLabel(tr("RECORD DIVIDEND"));
    title_label_->setStyleSheet(
        QString("color:%1; font-size:" + fincept::ui::fonts::px(-1) + "px; font-weight:700; letter-spacing:1px;").arg(ui::colors::CYAN()));
    layout->addWidget(title_label_);

    auto* form = new QFormLayout;
    form->setSpacing(8);
    form->setLabelAlignment(Qt::AlignRight);

    symbol_cb_ = new QComboBox;
    for (const auto& s : symbols)
        symbol_cb_->addItem(s);
    symbol_row_label_ = new QLabel(tr("Symbol:"));
    form->addRow(symbol_row_label_, symbol_cb_);

    amount_edit_ = new QLineEdit;
    amount_edit_->setPlaceholderText(tr("e.g. 0.88"));
    amount_row_label_ = new QLabel(tr("Amount/share:"));
    form->addRow(amount_row_label_, amount_edit_);

    date_edit_ = new QDateEdit;
    date_edit_->setCalendarPopup(true);
    date_edit_->setDisplayFormat("yyyy-MM-dd");
    date_edit_->setDate(QDate::currentDate());
    date_row_label_ = new QLabel(tr("Ex-div date:"));
    form->addRow(date_row_label_, date_edit_);
    date_edit_->setMaximumDate(QDate::currentDate());

    // Entitlement: the units held at the START of the ex-date, from the ledger. The
    // dialog used to record today's holding, which over- or under-states the payment
    // whenever the position changed since the ex-date.
    qty_edit_ = new QLineEdit;
    qty_row_label_ = new QLabel(tr("Units entitled:"));
    form->addRow(qty_row_label_, qty_edit_);
    entitled_label_ = new QLabel;
    entitled_label_->setWordWrap(true);
    entitled_label_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px;")
                                       .arg(ui::colors::TEXT_TERTIARY()));
    form->addRow(entitled_label_);

    notes_edit_ = new QLineEdit;
    notes_edit_->setPlaceholderText(tr("Optional note"));
    notes_row_label_ = new QLabel(tr("Notes:"));
    form->addRow(notes_row_label_, notes_edit_);

    layout->addLayout(form);

    costs_ = new TradeCostsBox(this);
    costs_->set_taxes_label(tr("Withholding tax:"));
    // The caller sets the portfolio's base right after (AddDividendDialog::set_context).
    costs_->set_context(QString(), core::market::currency_for_symbol(symbol_cb_->currentText()));
    layout->addWidget(costs_);
    layout->addStretch();

    connect(symbol_cb_, &QComboBox::currentTextChanged, this, [this](const QString& sym) {
        qty_user_edited_ = false;
        costs_->set_instrument_currency(quote_ccy_.value(sym, core::market::currency_for_symbol(sym)));
        update_entitlement();
    });
    connect(date_edit_, &QDateEdit::dateChanged, this, [this](const QDate&) { update_entitlement(); });
    connect(qty_edit_, &QLineEdit::textEdited, this, [this](const QString&) {
        qty_user_edited_ = true;
        qty_edit_->setStyleSheet(QString());
    });

    auto* btn_row = new QHBoxLayout;
    btn_row->addStretch();

    cancel_btn_ = new QPushButton(tr("CANCEL"));
    cancel_btn_->setMinimumHeight(32);
    cancel_btn_->setStyleSheet(
        QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                "  font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; padding:0 16px; }"
                "QPushButton:hover { background:%3; }")
            .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED(), ui::colors::BG_HOVER()));
    connect(cancel_btn_, &QPushButton::clicked, this, &QDialog::reject);
    btn_row->addWidget(cancel_btn_);

    record_btn_ = new QPushButton(tr("RECORD"));
    record_btn_->setMinimumHeight(32);
    record_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%3; border:none;"
                                       "  font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; padding:0 16px; }"
                                       "QPushButton:hover { background:%2; }")
                                   .arg(ui::colors::CYAN(), ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_ON_ACCENT()));
    connect(record_btn_, &QPushButton::clicked, this, [this]() {
        // Also reject 0 / non-numeric input — "abc" parsed to 0.0 and recorded
        // a zero-value dividend transaction.
        if (amount_per_share() <= 0.0) {
            amount_edit_->clear();
            amount_edit_->setPlaceholderText(tr("Enter an amount greater than 0"));
            amount_edit_->setStyleSheet(QString("border:1px solid %1;").arg(ui::colors::NEGATIVE()));
            amount_edit_->setFocus();
            return;
        }
        if (quantity() <= 0.0) {
            qty_edit_->setStyleSheet(QString("border:1px solid %1;").arg(ui::colors::NEGATIVE()));
            qty_edit_->setFocus();
            return;
        }
        QString why;
        if (costs_ && !costs_->validate(&why)) {
            costs_->set_expanded(true);
            QMessageBox::warning(this, tr("Record Dividend"), why);
            return;
        }
        accept();
    });
    connect(amount_edit_, &QLineEdit::textEdited, this,
            [this](const QString&) { amount_edit_->setStyleSheet(QString()); });
    btn_row->addWidget(record_btn_);

    layout->addLayout(btn_row);

    setTabOrder(symbol_cb_, amount_edit_);
    setTabOrder(amount_edit_, date_edit_);
    setTabOrder(date_edit_, notes_edit_);
    setTabOrder(notes_edit_, record_btn_);
    setTabOrder(record_btn_, cancel_btn_);
    symbol_cb_->setAccessibleName(tr("Holding that paid the dividend"));
    amount_edit_->setAccessibleName(tr("Dividend amount per share"));
    date_edit_->setAccessibleName(tr("Ex-dividend date"));
    record_btn_->setDefault(true);
    amount_edit_->setFocus();
}

void AddDividendDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void AddDividendDialog::retranslateUi() {
    setWindowTitle(tr("Record Dividend"));
    if (title_label_)
        title_label_->setText(tr("RECORD DIVIDEND"));
    if (symbol_row_label_)
        symbol_row_label_->setText(tr("Symbol:"));
    if (amount_row_label_)
        amount_row_label_->setText(tr("Amount/share:"));
    if (date_row_label_)
        date_row_label_->setText(tr("Ex-div date:"));
    if (notes_row_label_)
        notes_row_label_->setText(tr("Notes:"));
    if (qty_row_label_)
        qty_row_label_->setText(tr("Units entitled:"));
    if (entitled_label_)
        update_entitlement();
    if (amount_edit_)
        amount_edit_->setPlaceholderText(tr("e.g. 0.88"));
    if (notes_edit_)
        notes_edit_->setPlaceholderText(tr("Optional note"));
    if (cancel_btn_)
        cancel_btn_->setText(tr("CANCEL"));
    if (record_btn_)
        record_btn_->setText(tr("RECORD"));
}

QString AddDividendDialog::symbol() const {
    return symbol_cb_->currentText();
}

double AddDividendDialog::amount_per_share() const {
    return dlg_parse_number(amount_edit_->text());
}

QString AddDividendDialog::date() const {
    return date_edit_->date().toString("yyyy-MM-dd");
}

QString AddDividendDialog::notes() const {
    return notes_edit_->text().trimmed();
}

double AddDividendDialog::quantity() const {
    return dlg_parse_number(qty_edit_->text());
}

void AddDividendDialog::set_symbol(const QString& symbol) {
    const int i = symbol_cb_->findText(symbol);
    if (i >= 0)
        symbol_cb_->setCurrentIndex(i);
}

void AddDividendDialog::set_context(const QString& base_ccy, const QVector<portfolio::Transaction>& ledger,
                                    const QHash<QString, double>& held_now, const QHash<QString, QString>& quote_ccy) {
    ledger_ = ledger;
    held_now_ = held_now;
    quote_ccy_ = quote_ccy;
    const QString sym = symbol_cb_->currentText();
    costs_->set_context(base_ccy, quote_ccy_.value(sym, core::market::currency_for_symbol(sym)));
    update_entitlement();
}

void AddDividendDialog::update_entitlement() {
    const QString sym = symbol_cb_->currentText();
    const QDate ex = date_edit_->date();
    const double ledger_qty = portfolio::quantity_held_before(ledger_, sym, ex);
    const double now = held_now_.value(sym, 0.0);
    QString note;
    double suggested = 0.0;
    if (ledger_qty > 0) {
        suggested = ledger_qty;
        note = tr("From the ledger: %1 units held at the start of %2 (the ex-date). Today's holding is %3.")
                   .arg(ui::formatting::quantity(ledger_qty, 8), ui::formatting::calendar_date(ex),
                        ui::formatting::quantity(now, 8));
    } else if (now > 0) {
        suggested = now;
        note = tr("The ledger shows no %1 before %2 (the position may pre-date its transactions). Today's holding is "
                  "filled in — correct it to the units you held on the ex-date.")
                   .arg(sym, ui::formatting::calendar_date(ex));
    } else {
        note = tr("No %1 held on the ex-date according to the ledger — enter the units entitled.").arg(sym);
    }
    if (!qty_user_edited_)
        qty_edit_->setText(suggested > 0 ? dlg_edit_number(suggested) : QString());
    entitled_label_->setText(note);
}

} // namespace fincept::screens
