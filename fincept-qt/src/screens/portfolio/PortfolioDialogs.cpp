// src/screens/portfolio/PortfolioDialogs.cpp
#include "screens/portfolio/PortfolioDialogs.h"

#include "services/file_manager/FileManagerService.h"
#include "services/markets/MarketSearchService.h"
#include "ui/theme/Theme.h"

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
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens {

// ── CreatePortfolioDialog ────────────────────────────────────────────────────

CreatePortfolioDialog::CreatePortfolioDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Create Portfolio"));
    setFixedSize(380, 260);
    setStyleSheet(QString("QDialog { background:%1; color:%2; }"
                          "QLabel { color:%3; font-size:11px; }"
                          "QLineEdit { background:%4; color:%2; border:1px solid %5;"
                          "  padding:6px 10px; font-size:12px; }"
                          "QLineEdit:focus { border-color:%6; }"
                          "QComboBox { background:%4; color:%2; border:1px solid %5;"
                          "  padding:6px 10px; font-size:12px; }"
                          "QComboBox::drop-down { border:none; }"
                          "QComboBox QAbstractItemView { background:%4; color:%2;"
                          "  selection-background-color:%7; }")
                      .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_SECONDARY(),
                           ui::colors::BG_BASE(), ui::colors::BORDER_MED(), ui::colors::AMBER(), ui::colors::AMBER_DIM()));

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->setContentsMargins(20, 16, 20, 16);

    // Header
    title_label_ = new QLabel(tr("CREATE NEW PORTFOLIO"));
    title_label_->setStyleSheet(
        QString("color:%1; font-size:13px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
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
    QStringList currencies = {
        "USD", "EUR", "GBP", "JPY", "CHF", "CAD", "AUD", "NZD", "HKD", "SGD", "SEK", "NOK",
        "DKK", "CNY", "INR", "BRL", "MXN", "KRW", "TWD", "THB", "ZAR", "TRY", "RUB", "PLN",
        "CZK", "HUF", "ILS", "AED", "SAR", "IDR", "MYR", "PHP", "VND", "NGN", "EGP", "BDT",
    };
    currency_cb_->addItems(currencies);
    currency_row_label_ = new QLabel(tr("Currency:"));
    form->addRow(currency_row_label_, currency_cb_);

    layout->addLayout(form);
    layout->addStretch();

    // Buttons
    auto* btn_layout = new QHBoxLayout;
    btn_layout->addStretch();

    cancel_btn_ = new QPushButton(tr("CANCEL"));
    cancel_btn_->setFixedSize(90, 30);
    cancel_btn_->setCursor(Qt::PointingHandCursor);
    cancel_btn_->setStyleSheet(
        QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                "  font-size:10px; font-weight:700; }"
                "QPushButton:hover { background:%3; color:%4; }")
            .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED(), ui::colors::BG_HOVER(), ui::colors::TEXT_PRIMARY()));
    connect(cancel_btn_, &QPushButton::clicked, this, &QDialog::reject);
    btn_layout->addWidget(cancel_btn_);

    create_btn_ = new QPushButton(tr("CREATE"));
    create_btn_->setFixedSize(90, 30);
    create_btn_->setCursor(Qt::PointingHandCursor);
    create_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%3; border:none;"
                                      "  font-size:10px; font-weight:700; }"
                                      "QPushButton:hover { background:%2; }")
                                  .arg(ui::colors::AMBER(), ui::colors::WARNING(), ui::colors::BG_BASE()));
    connect(create_btn_, &QPushButton::clicked, this, [this]() {
        if (!name_edit_->text().trimmed().isEmpty())
            accept();
    });
    btn_layout->addWidget(create_btn_);

    layout->addLayout(btn_layout);

    name_edit_->setFocus();
}

void CreatePortfolioDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void CreatePortfolioDialog::retranslateUi() {
    setWindowTitle(tr("Create Portfolio"));
    if (title_label_)         title_label_->setText(tr("CREATE NEW PORTFOLIO"));
    if (name_row_label_)      name_row_label_->setText(tr("Name:"));
    if (owner_row_label_)     owner_row_label_->setText(tr("Owner:"));
    if (currency_row_label_)  currency_row_label_->setText(tr("Currency:"));
    if (name_edit_)           name_edit_->setPlaceholderText(tr("My Portfolio"));
    if (owner_edit_)          owner_edit_->setPlaceholderText(tr("Your name"));
    if (cancel_btn_)          cancel_btn_->setText(tr("CANCEL"));
    if (create_btn_)          create_btn_->setText(tr("CREATE"));
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
    setFixedSize(340, 160);
    setStyleSheet(
        QString("QDialog { background:%1; color:%2; }").arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY()));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(12);

    icon_label_ = new QLabel(tr("\u26A0  DELETE PORTFOLIO"));
    icon_label_->setStyleSheet(QString("color:%1; font-size:13px; font-weight:700;").arg(ui::colors::NEGATIVE()));
    layout->addWidget(icon_label_);

    msg_label_ = new QLabel(tr("Are you sure you want to delete \"%1\"?\n"
                               "This will remove all holdings and transactions.")
                                .arg(portfolio_name_));
    msg_label_->setWordWrap(true);
    msg_label_->setStyleSheet(QString("color:%1; font-size:11px;").arg(ui::colors::TEXT_SECONDARY()));
    layout->addWidget(msg_label_);

    layout->addStretch();

    auto* btn_layout = new QHBoxLayout;
    btn_layout->addStretch();

    cancel_btn_ = new QPushButton(tr("CANCEL"));
    cancel_btn_->setFixedSize(90, 30);
    cancel_btn_->setCursor(Qt::PointingHandCursor);
    cancel_btn_->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                                      "  font-size:10px; font-weight:700; }"
                                      "QPushButton:hover { background:%3; }")
                                  .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED(), ui::colors::BG_HOVER()));
    connect(cancel_btn_, &QPushButton::clicked, this, &QDialog::reject);
    btn_layout->addWidget(cancel_btn_);

    delete_btn_ = new QPushButton(tr("DELETE"));
    delete_btn_->setFixedSize(90, 30);
    delete_btn_->setCursor(Qt::PointingHandCursor);
    delete_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:none;"
                                      "  font-size:10px; font-weight:700; }"
                                      "QPushButton:hover { background:%1; }")
                                  .arg(ui::colors::NEGATIVE(), ui::colors::TEXT_PRIMARY()));
    connect(delete_btn_, &QPushButton::clicked, this, &QDialog::accept);
    btn_layout->addWidget(delete_btn_);

    layout->addLayout(btn_layout);
}

void ConfirmDeleteDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void ConfirmDeleteDialog::retranslateUi() {
    setWindowTitle(tr("Delete Portfolio"));
    if (icon_label_) icon_label_->setText(tr("\u26A0  DELETE PORTFOLIO"));
    if (msg_label_)
        msg_label_->setText(tr("Are you sure you want to delete \"%1\"?\n"
                               "This will remove all holdings and transactions.")
                                .arg(portfolio_name_));
    if (cancel_btn_) cancel_btn_->setText(tr("CANCEL"));
    if (delete_btn_) delete_btn_->setText(tr("DELETE"));
}

// ── AddAssetDialog ───────────────────────────────────────────────────────────

static constexpr int kAssetSearchDebounceMs = 300;
static constexpr int kAssetSearchLimit = 10;

AddAssetDialog::AddAssetDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Add Asset"));
    setFixedSize(400, 260);
    setStyleSheet(QString("QDialog { background:%1; color:%2; }"
                          "QLabel { color:%3; font-size:11px; }"
                          "QLineEdit { background:%4; color:%2; border:1px solid %5;"
                          "  padding:6px 10px; font-size:12px; }"
                          "QLineEdit:focus { border-color:%6; }")
                      .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_SECONDARY(),
                           ui::colors::BG_BASE(), ui::colors::BORDER_MED(), ui::colors::AMBER()));

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->setContentsMargins(20, 16, 20, 16);

    title_label_ = new QLabel(tr("BUY ASSET"));
    title_label_->setStyleSheet(
        QString("color:%1; font-size:13px; font-weight:700; letter-spacing:1px;").arg(ui::colors::POSITIVE()));
    layout->addWidget(title_label_);

    // Hint label under title
    hint_label_ = new QLabel(tr("Type a ticker or company name to search"));
    hint_label_->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::TEXT_TERTIARY()));
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

    layout->addLayout(form);
    layout->addStretch();

    auto* btn_layout = new QHBoxLayout;
    btn_layout->addStretch();

    cancel_btn_ = new QPushButton(tr("CANCEL"));
    cancel_btn_->setFixedSize(80, 28);
    cancel_btn_->setCursor(Qt::PointingHandCursor);
    cancel_btn_->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                                      "  font-size:10px; font-weight:700; }"
                                      "QPushButton:hover { background:%3; }")
                                  .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED(), ui::colors::BG_HOVER()));
    connect(cancel_btn_, &QPushButton::clicked, this, &QDialog::reject);
    btn_layout->addWidget(cancel_btn_);

    add_btn_ = new QPushButton(tr("ADD"));
    add_btn_->setFixedSize(80, 28);
    add_btn_->setCursor(Qt::PointingHandCursor);
    add_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:none;"
                                   "  font-size:10px; font-weight:700; }"
                                   "QPushButton:hover { background:%1; }")
                               .arg(ui::colors::POSITIVE(), ui::colors::BG_BASE()));
    connect(add_btn_, &QPushButton::clicked, this, [this]() {
        if (!symbol_edit_->text().trimmed().isEmpty() && quantity() > 0 && price() > 0)
            accept();
    });
    btn_layout->addWidget(add_btn_);

    layout->addLayout(btn_layout);

    // ── Search dropdown (floats over the dialog, parented to this) ────────────
    search_frame_ = new QFrame(this);
    search_frame_->setObjectName("assetSearchFrame");
    search_frame_->setStyleSheet(
        QString("QFrame#assetSearchFrame { background:%1; border:1px solid %2; border-top:none; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_MED()));
    search_frame_->hide();

    auto* frame_layout = new QVBoxLayout(search_frame_);
    frame_layout->setContentsMargins(0, 0, 0, 0);
    frame_layout->setSpacing(0);

    search_list_ = new QListWidget(search_frame_);
    search_list_->setStyleSheet(QString("QListWidget { background:transparent; border:none; outline:none; }"
                                        "QListWidget::item { padding:0; border:none; background:transparent; }"
                                        "QListWidget::item:selected { background:%1; border-left:3px solid %2; }")
                                    .arg(ui::colors::BORDER_DIM(), ui::colors::AMBER()));
    search_list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
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

    connect(symbol_edit_, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (selecting_)
            return;
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
                    if (request_id != my_rid) return;
                    if (pending_query_ != q) return;
                    show_results(items);
                });
        search_connected_ = true;
    }
    svc.search(query, /*type=*/QStringLiteral("stock"), kAssetSearchLimit, rid);
}

void AddAssetDialog::show_results(const QList<fincept::services::MarketSearchService::Item>& results) {
    search_list_->clear();

    if (results.isEmpty()) {
        auto* item = new QListWidgetItem(search_list_);
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        auto* row = new QWidget(this);
        row->setStyleSheet("background:transparent;");
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(10, 6, 10, 6);
        auto* lbl = new QLabel(tr("No results found"));
        lbl->setStyleSheet(
            QString("color:%1; font-size:11px; font-family:'Consolas',monospace; background:transparent;")
                .arg(ui::colors::TEXT_TERTIARY()));
        rl->addWidget(lbl);
        item->setSizeHint(QSize(0, 28));
        search_list_->setItemWidget(item, row);
        position_dropdown();
        search_frame_->show();
        return;
    }

    for (const auto& entry : results) {
        const QString& sym  = entry.symbol;
        const QString& name = entry.name;
        const QString& exch = entry.exchange;
        const QString  type = entry.type.isEmpty() ? QStringLiteral("stock") : entry.type;
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

        auto* row = new QWidget(this);
        row->setStyleSheet("background:transparent;");
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(10, 4, 10, 4);
        hl->setSpacing(8);

        auto* sym_lbl = new QLabel(yf_sym);
        sym_lbl->setStyleSheet(QString("color:%1; font-size:12px; font-weight:700;"
                                       "font-family:'Consolas',monospace; background:transparent;")
                                   .arg(ui::colors::TEXT_PRIMARY()));
        sym_lbl->setFixedWidth(100);

        auto* name_lbl = new QLabel(name);
        name_lbl->setStyleSheet(QString("color:%1; font-size:11px; background:transparent;"
                                        "font-family:'Consolas',monospace;")
                                    .arg(ui::colors::TEXT_SECONDARY()));
        name_lbl->setMaximumWidth(180);

        hl->addWidget(sym_lbl);
        hl->addWidget(name_lbl, 1);

        if (!exch.isEmpty()) {
            auto* exch_lbl = new QLabel(exch);
            exch_lbl->setStyleSheet(QString("color:%1; font-size:10px; font-family:'Consolas',monospace;"
                                            "background:transparent;")
                                        .arg(ui::colors::TEXT_TERTIARY()));
            hl->addWidget(exch_lbl);
        }

        auto* type_lbl = new QLabel(type.toUpper());
        type_lbl->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700;"
                                        "font-family:'Consolas',monospace; background:%2;"
                                        "padding:1px 4px; border-radius:2px;")
                                    .arg(ui::colors::AMBER(), ui::colors::BORDER_DIM()));
        hl->addWidget(type_lbl);

        item->setSizeHint(QSize(0, 30));
        search_list_->setItemWidget(item, row);
    }

    if (search_list_->count() > 0)
        search_list_->setCurrentRow(0);

    position_dropdown();
    search_frame_->show();
    search_frame_->raise();
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
    const QPoint origin = symbol_edit_->mapTo(this, QPoint(0, symbol_edit_->height()));
    const int w = symbol_edit_->width() + 60; // a bit wider to show exchange column
    const int rows = std::min(search_list_->count(), kAssetSearchLimit);
    const int h = rows * 30 + 2;
    search_frame_->setGeometry(origin.x(), origin.y(), w, h);
}

bool AddAssetDialog::eventFilter(QObject* obj, QEvent* event) {
    if (obj == symbol_edit_ && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (search_frame_->isVisible()) {
            if (ke->key() == Qt::Key_Down) {
                const int next = (search_list_->currentRow() + 1) % search_list_->count();
                search_list_->setCurrentRow(next);
                return true;
            }
            if (ke->key() == Qt::Key_Up) {
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

void AddAssetDialog::retranslateUi() {
    setWindowTitle(tr("Add Asset"));
    if (title_label_)        title_label_->setText(tr("BUY ASSET"));
    if (hint_label_)         hint_label_->setText(tr("Type a ticker or company name to search"));
    if (symbol_row_label_)   symbol_row_label_->setText(tr("Symbol:"));
    if (quantity_row_label_) quantity_row_label_->setText(tr("Quantity:"));
    if (price_row_label_)    price_row_label_->setText(tr("Price:"));
    if (symbol_edit_)        symbol_edit_->setPlaceholderText(tr("e.g. AAPL, Apple, Reliance…"));
    if (quantity_edit_)      quantity_edit_->setPlaceholderText(tr("e.g. 10"));
    if (price_edit_)         price_edit_->setPlaceholderText(tr("e.g. 150.00"));
    if (cancel_btn_)         cancel_btn_->setText(tr("CANCEL"));
    if (add_btn_)            add_btn_->setText(tr("ADD"));
}

QString AddAssetDialog::symbol() const {
    return symbol_edit_->text().trimmed().toUpper();
}
double AddAssetDialog::quantity() const {
    return quantity_edit_->text().toDouble();
}
double AddAssetDialog::price() const {
    return price_edit_->text().toDouble();
}

// ── SellAssetDialog ──────────────────────────────────────────────────────────

SellAssetDialog::SellAssetDialog(const QString& symbol, double held_qty, QWidget* parent)
    : QDialog(parent), symbol_(symbol), held_qty_(held_qty) {
    setWindowTitle(tr("Sell Asset"));
    setFixedSize(340, 220);
    setStyleSheet(QString("QDialog { background:%1; color:%2; }"
                          "QLabel { color:%3; font-size:11px; }"
                          "QLineEdit { background:%4; color:%2; border:1px solid %5;"
                          "  padding:6px 10px; font-size:12px; }"
                          "QLineEdit:focus { border-color:%6; }")
                      .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_SECONDARY(),
                           ui::colors::BG_BASE(), ui::colors::BORDER_MED(), ui::colors::AMBER()));

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->setContentsMargins(20, 16, 20, 16);

    title_label_ = new QLabel(tr("SELL %1").arg(symbol_));
    title_label_->setStyleSheet(
        QString("color:%1; font-size:13px; font-weight:700; letter-spacing:1px;").arg(ui::colors::NEGATIVE()));
    layout->addWidget(title_label_);

    held_label_ = new QLabel(tr("Currently holding: %1 shares").arg(held_qty_));
    held_label_->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::TEXT_TERTIARY()));
    layout->addWidget(held_label_);

    auto* form = new QFormLayout;
    form->setSpacing(8);

    quantity_edit_ = new QLineEdit;
    quantity_edit_->setPlaceholderText(tr("Max %1").arg(held_qty_));
    quantity_row_label_ = new QLabel(tr("Quantity:"));
    form->addRow(quantity_row_label_, quantity_edit_);

    price_edit_ = new QLineEdit;
    price_edit_->setPlaceholderText(tr("Sell price"));
    price_row_label_ = new QLabel(tr("Price:"));
    form->addRow(price_row_label_, price_edit_);

    layout->addLayout(form);
    layout->addStretch();

    auto* btn_layout = new QHBoxLayout;
    btn_layout->addStretch();

    cancel_btn_ = new QPushButton(tr("CANCEL"));
    cancel_btn_->setFixedSize(80, 28);
    cancel_btn_->setCursor(Qt::PointingHandCursor);
    cancel_btn_->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                                      "  font-size:10px; font-weight:700; }"
                                      "QPushButton:hover { background:%3; }")
                                  .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED(), ui::colors::BG_HOVER()));
    connect(cancel_btn_, &QPushButton::clicked, this, &QDialog::reject);
    btn_layout->addWidget(cancel_btn_);

    sell_btn_ = new QPushButton(tr("SELL"));
    sell_btn_->setFixedSize(80, 28);
    sell_btn_->setCursor(Qt::PointingHandCursor);
    sell_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:none;"
                                    "  font-size:10px; font-weight:700; }"
                                    "QPushButton:hover { background:%1; }")
                                .arg(ui::colors::NEGATIVE(), ui::colors::TEXT_PRIMARY()));
    connect(sell_btn_, &QPushButton::clicked, this, [this]() {
        if (quantity() > 0 && quantity() <= held_qty_ && price() > 0)
            accept();
    });
    btn_layout->addWidget(sell_btn_);

    layout->addLayout(btn_layout);
    quantity_edit_->setFocus();
}

void SellAssetDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void SellAssetDialog::retranslateUi() {
    setWindowTitle(tr("Sell Asset"));
    if (title_label_)        title_label_->setText(tr("SELL %1").arg(symbol_));
    if (held_label_)         held_label_->setText(tr("Currently holding: %1 shares").arg(held_qty_));
    if (quantity_row_label_) quantity_row_label_->setText(tr("Quantity:"));
    if (price_row_label_)    price_row_label_->setText(tr("Price:"));
    if (quantity_edit_)      quantity_edit_->setPlaceholderText(tr("Max %1").arg(held_qty_));
    if (price_edit_)         price_edit_->setPlaceholderText(tr("Sell price"));
    if (cancel_btn_)         cancel_btn_->setText(tr("CANCEL"));
    if (sell_btn_)           sell_btn_->setText(tr("SELL"));
}

double SellAssetDialog::quantity() const {
    return quantity_edit_->text().toDouble();
}
double SellAssetDialog::price() const {
    return price_edit_->text().toDouble();
}

// ── ImportPortfolioDialog ────────────────────────────────────────────────────

ImportPortfolioDialog::ImportPortfolioDialog(const QVector<portfolio::Portfolio>& portfolios, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("Import Portfolio"));
    setFixedSize(420, 330);
    setStyleSheet(QString("QDialog { background:%1; color:%2; }"
                          "QLabel { color:%3; font-size:11px; }"
                          "QLineEdit { background:%4; color:%2; border:1px solid %5;"
                          "  padding:6px 10px; font-size:12px; }"
                          "QLineEdit:focus { border-color:%6; }"
                          "QRadioButton { color:%2; font-size:11px; spacing:6px; }"
                          "QRadioButton::indicator { width:14px; height:14px; }"
                          "QComboBox { background:%4; color:%2; border:1px solid %5;"
                          "  padding:6px 10px; font-size:12px; }"
                          "QComboBox::drop-down { border:none; }"
                          "QComboBox QAbstractItemView { background:%4; color:%2; }")
                      .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_SECONDARY(),
                           ui::colors::BG_BASE(), ui::colors::BORDER_MED(), ui::colors::AMBER()));

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->setContentsMargins(20, 16, 20, 16);

    title_label_ = new QLabel(tr("IMPORT PORTFOLIO"));
    title_label_->setStyleSheet(
        QString("color:%1; font-size:13px; font-weight:700; letter-spacing:1px;").arg(ui::colors::CYAN()));
    layout->addWidget(title_label_);

    // File picker
    auto* file_row = new QHBoxLayout;
    file_edit_ = new QLineEdit;
    file_edit_->setPlaceholderText(tr("Select JSON file..."));
    file_edit_->setReadOnly(true);
    file_row->addWidget(file_edit_, 1);

    browse_btn_ = new QPushButton(tr("BROWSE"));
    browse_btn_->setFixedSize(80, 30);
    browse_btn_->setCursor(Qt::PointingHandCursor);
    browse_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:none;"
                                      "  font-size:10px; font-weight:700; }"
                                      "QPushButton:hover { background:%1; }")
                                  .arg(ui::colors::CYAN(), ui::colors::BG_BASE()));
    connect(browse_btn_, &QPushButton::clicked, this, &ImportPortfolioDialog::browse_file);
    file_row->addWidget(browse_btn_);

    layout->addLayout(file_row);

    // Demo JSON download hint
    auto* demo_row = new QHBoxLayout;
    demo_hint_label_ = new QLabel(tr("Need a template? Download the demo portfolio JSON:"));
    demo_hint_label_->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::TEXT_TERTIARY()));
    demo_row->addWidget(demo_hint_label_, 1);

    demo_dl_btn_ = new QPushButton(tr("DOWNLOAD DEMO"));
    demo_dl_btn_->setFixedSize(120, 24);
    demo_dl_btn_->setCursor(Qt::PointingHandCursor);
    demo_dl_btn_->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %1;"
                                       "  font-size:9px; font-weight:700; letter-spacing:0.3px; }"
                                       "QPushButton:hover { background:%1; color:%2; }")
                                   .arg(ui::colors::CYAN(), ui::colors::BG_BASE()));
    connect(demo_dl_btn_, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getSaveFileName(this, tr("Save Demo Portfolio JSON"), "demo_portfolio.json",
                                                    tr("JSON Files (*.json)"));
        if (path.isEmpty())
            return;

        QJsonArray holdings;
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
        for (const auto& h : demo) {
            QJsonObject o;
            o["symbol"] = h.symbol;
            o["quantity"] = h.qty;
            o["avg_buy_price"] = h.price;
            o["sector"] = h.sector;
            holdings.append(o);
        }

        QJsonObject root;
        root["name"] = "Demo Portfolio";
        root["owner"] = "Fincept User";
        root["currency"] = "USD";
        root["description"] = "Sample portfolio for demonstration";
        root["holdings"] = holdings;

        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
            f.close();
            services::FileManagerService::instance().import_file(path, "portfolio");
            QMessageBox::information(this, tr("Demo JSON Saved"),
                                     tr("Demo portfolio JSON saved.\nYou can now import it using the BROWSE button."));
        } else {
            QMessageBox::warning(this, tr("Save Failed"), tr("Could not write to: %1").arg(path));
        }
    });
    demo_row->addWidget(demo_dl_btn_);
    layout->addLayout(demo_row);

    // Import mode
    mode_label_ = new QLabel(tr("IMPORT MODE"));
    mode_label_->setStyleSheet(
        QString("color:%1; font-size:9px; font-weight:700; letter-spacing:0.5px;").arg(ui::colors::TEXT_TERTIARY()));
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
    status_label_->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::TEXT_TERTIARY()));
    layout->addWidget(status_label_);

    layout->addStretch();

    // Buttons
    auto* btn_layout = new QHBoxLayout;
    btn_layout->addStretch();

    cancel_btn_ = new QPushButton(tr("CANCEL"));
    cancel_btn_->setFixedSize(80, 28);
    cancel_btn_->setCursor(Qt::PointingHandCursor);
    cancel_btn_->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                                      "  font-size:10px; font-weight:700; }"
                                      "QPushButton:hover { background:%3; }")
                                  .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED(), ui::colors::BG_HOVER()));
    connect(cancel_btn_, &QPushButton::clicked, this, &QDialog::reject);
    btn_layout->addWidget(cancel_btn_);

    import_btn_ = new QPushButton(tr("IMPORT"));
    import_btn_->setFixedSize(80, 28);
    import_btn_->setCursor(Qt::PointingHandCursor);
    import_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:none;"
                                      "  font-size:10px; font-weight:700; }"
                                      "QPushButton:hover { background:%1; }")
                                  .arg(ui::colors::CYAN(), ui::colors::BG_BASE()));
    connect(import_btn_, &QPushButton::clicked, this, [this]() {
        if (!file_edit_->text().isEmpty())
            accept();
    });
    btn_layout->addWidget(import_btn_);

    layout->addLayout(btn_layout);
}

void ImportPortfolioDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void ImportPortfolioDialog::retranslateUi() {
    setWindowTitle(tr("Import Portfolio"));
    if (title_label_)      title_label_->setText(tr("IMPORT PORTFOLIO"));
    if (file_edit_)        file_edit_->setPlaceholderText(tr("Select JSON file..."));
    if (browse_btn_)       browse_btn_->setText(tr("BROWSE"));
    if (demo_hint_label_)  demo_hint_label_->setText(tr("Need a template? Download the demo portfolio JSON:"));
    if (demo_dl_btn_)      demo_dl_btn_->setText(tr("DOWNLOAD DEMO"));
    if (mode_label_)       mode_label_->setText(tr("IMPORT MODE"));
    if (new_radio_)        new_radio_->setText(tr("Create new portfolio from file"));
    if (merge_radio_)      merge_radio_->setText(tr("Merge transactions into existing portfolio:"));
    if (cancel_btn_)       cancel_btn_->setText(tr("CANCEL"));
    if (import_btn_)       import_btn_->setText(tr("IMPORT"));
}

void ImportPortfolioDialog::browse_file() {
    QString path = QFileDialog::getOpenFileName(this, tr("Select Portfolio JSON"), QString(), tr("JSON Files (*.json)"));
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
    : QDialog(parent), txn_type_(txn.transaction_type), txn_symbol_(txn.symbol) {
    setWindowTitle(tr("Edit Transaction"));
    setFixedSize(380, 280);
    setStyleSheet(QString("QDialog { background:%1; color:%2; }"
                          "QLabel { color:%3; font-size:11px; }"
                          "QLineEdit { background:%4; color:%2; border:1px solid %5;"
                          "  padding:6px 10px; font-size:12px; }"
                          "QLineEdit:focus { border-color:%6; }")
                      .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_SECONDARY(),
                           ui::colors::BG_BASE(), ui::colors::BORDER_MED(), ui::colors::AMBER()));

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->setContentsMargins(20, 16, 20, 16);

    title_label_ = new QLabel(tr("EDIT %1 — %2").arg(txn_type_, txn_symbol_));
    title_label_->setStyleSheet(
        QString("color:%1; font-size:13px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    layout->addWidget(title_label_);

    auto* form = new QFormLayout;
    form->setSpacing(8);

    quantity_edit_ = new QLineEdit(QString::number(txn.quantity, 'f', 2));
    quantity_row_label_ = new QLabel(tr("Quantity:"));
    form->addRow(quantity_row_label_, quantity_edit_);

    price_edit_ = new QLineEdit(QString::number(txn.price, 'f', 2));
    price_row_label_ = new QLabel(tr("Price:"));
    form->addRow(price_row_label_, price_edit_);

    date_edit_ = new QDateEdit;
    date_edit_->setCalendarPopup(true);
    date_edit_->setDisplayFormat("yyyy-MM-dd");
    {
        QDate d = QDate::fromString(txn.transaction_date, "yyyy-MM-dd");
        date_edit_->setDate(d.isValid() ? d : QDate::currentDate());
    }
    date_edit_->setStyleSheet(
        QString("QDateEdit { background:%1; color:%2; border:1px solid %3;"
                "  padding:6px 10px; font-size:12px; }"
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

    layout->addLayout(form);
    layout->addStretch();

    auto* btn_layout = new QHBoxLayout;
    btn_layout->addStretch();

    cancel_btn_ = new QPushButton(tr("CANCEL"));
    cancel_btn_->setFixedSize(80, 28);
    cancel_btn_->setCursor(Qt::PointingHandCursor);
    cancel_btn_->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                                      "  font-size:10px; font-weight:700; }"
                                      "QPushButton:hover { background:%3; }")
                                  .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED(), ui::colors::BG_HOVER()));
    connect(cancel_btn_, &QPushButton::clicked, this, &QDialog::reject);
    btn_layout->addWidget(cancel_btn_);

    save_btn_ = new QPushButton(tr("SAVE"));
    save_btn_->setFixedSize(80, 28);
    save_btn_->setCursor(Qt::PointingHandCursor);
    save_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%3; border:none;"
                                    "  font-size:10px; font-weight:700; }"
                                    "QPushButton:hover { background:%2; }")
                                .arg(ui::colors::AMBER(), ui::colors::WARNING(), ui::colors::BG_BASE()));
    connect(save_btn_, &QPushButton::clicked, this, [this]() {
        if (quantity() > 0 && price() > 0)
            accept();
    });
    btn_layout->addWidget(save_btn_);

    layout->addLayout(btn_layout);
    quantity_edit_->setFocus();
}

void EditTransactionDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void EditTransactionDialog::retranslateUi() {
    setWindowTitle(tr("Edit Transaction"));
    if (title_label_)        title_label_->setText(tr("EDIT %1 — %2").arg(txn_type_, txn_symbol_));
    if (quantity_row_label_) quantity_row_label_->setText(tr("Quantity:"));
    if (price_row_label_)    price_row_label_->setText(tr("Price:"));
    if (date_row_label_)     date_row_label_->setText(tr("Date:"));
    if (notes_row_label_)    notes_row_label_->setText(tr("Notes:"));
    if (notes_edit_)         notes_edit_->setPlaceholderText(tr("Optional notes"));
    if (cancel_btn_)         cancel_btn_->setText(tr("CANCEL"));
    if (save_btn_)           save_btn_->setText(tr("SAVE"));
}

double EditTransactionDialog::quantity() const {
    return quantity_edit_->text().toDouble();
}
double EditTransactionDialog::price() const {
    return price_edit_->text().toDouble();
}
QString EditTransactionDialog::date() const {
    return date_edit_->date().toString("yyyy-MM-dd");
}
QString EditTransactionDialog::notes() const {
    return notes_edit_->text().trimmed();
}

// ── SectorMappingDialog ──────────────────────────────────────────────────────

static const QStringList kSectors = {
    "Technology",  "Healthcare",    "Financial Services", "Energy",    "Consumer Cyclical", "Consumer Defensive",
    "Industrials", "Communication", "Real Estate",        "Utilities", "Basic Materials",   "Cryptocurrency",
    "US Equity",   "Bonds",         "Commodities",        "Other",
};

SectorMappingDialog::SectorMappingDialog(const QVector<portfolio::HoldingWithQuote>& holdings, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("Sector Mapping"));
    setFixedSize(440, std::min(500, 120 + static_cast<int>(holdings.size()) * 34));
    setStyleSheet(QString("QDialog { background:%1; color:%2; }"
                          "QLabel { color:%3; font-size:11px; }"
                          "QComboBox { background:%4; color:%2; border:1px solid %5;"
                          "  padding:4px 8px; font-size:11px; }"
                          "QComboBox::drop-down { border:none; }"
                          "QComboBox QAbstractItemView { background:%4; color:%2; }")
                      .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_SECONDARY(),
                           ui::colors::BG_BASE(), ui::colors::BORDER_MED()));

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);
    layout->setContentsMargins(20, 16, 20, 16);

    title_label_ = new QLabel(tr("MAP HOLDINGS TO SECTORS"));
    title_label_->setStyleSheet(
        QString("color:%1; font-size:13px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    layout->addWidget(title_label_);

    desc_label_ = new QLabel(tr("Assign each holding to a sector for allocation analysis."));
    desc_label_->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::TEXT_TERTIARY()));
    layout->addWidget(desc_label_);

    // Scrollable grid
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QString("QScrollArea { border:none; }"
                                  "QScrollBar:vertical { width:4px; background:transparent; }"
                                  "QScrollBar::handle:vertical { background:%1; }")
                              .arg(ui::colors::BORDER_BRIGHT()));

    auto* grid_w = new QWidget(this);
    auto* grid = new QFormLayout(grid_w);
    grid->setSpacing(6);

    for (const auto& h : holdings) {
        auto* cb = new QComboBox;
        cb->addItems(kSectors);
        // Try to pre-select based on simple heuristic
        cb->setCurrentText("Other");
        combos_[h.symbol] = cb;

        auto* sym_label = new QLabel(h.symbol);
        sym_label->setStyleSheet(QString("color:%1; font-weight:600;").arg(ui::colors::CYAN()));
        grid->addRow(sym_label, cb);
    }

    scroll->setWidget(grid_w);
    layout->addWidget(scroll, 1);

    auto* btn_layout = new QHBoxLayout;
    btn_layout->addStretch();

    cancel_btn_ = new QPushButton(tr("CANCEL"));
    cancel_btn_->setFixedSize(80, 28);
    cancel_btn_->setCursor(Qt::PointingHandCursor);
    cancel_btn_->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                                      "  font-size:10px; font-weight:700; }"
                                      "QPushButton:hover { background:%3; }")
                                  .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED(), ui::colors::BG_HOVER()));
    connect(cancel_btn_, &QPushButton::clicked, this, &QDialog::reject);
    btn_layout->addWidget(cancel_btn_);

    save_btn_ = new QPushButton(tr("SAVE"));
    save_btn_->setFixedSize(80, 28);
    save_btn_->setCursor(Qt::PointingHandCursor);
    save_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%3; border:none;"
                                    "  font-size:10px; font-weight:700; }"
                                    "QPushButton:hover { background:%2; }")
                                .arg(ui::colors::AMBER(), ui::colors::WARNING(), ui::colors::BG_BASE()));
    connect(save_btn_, &QPushButton::clicked, this, &QDialog::accept);
    btn_layout->addWidget(save_btn_);

    layout->addLayout(btn_layout);
}

void SectorMappingDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void SectorMappingDialog::retranslateUi() {
    setWindowTitle(tr("Sector Mapping"));
    if (title_label_) title_label_->setText(tr("MAP HOLDINGS TO SECTORS"));
    if (desc_label_)  desc_label_->setText(tr("Assign each holding to a sector for allocation analysis."));
    if (cancel_btn_)  cancel_btn_->setText(tr("CANCEL"));
    if (save_btn_)    save_btn_->setText(tr("SAVE"));
}

QHash<QString, QString> SectorMappingDialog::sector_map() const {
    QHash<QString, QString> result;
    for (auto it = combos_.begin(); it != combos_.end(); ++it)
        result[it.key()] = it.value()->currentText();
    return result;
}

// ── AddDividendDialog ─────────────────────────────────────────────────────────

static QString kDividendStyle(const QString& accent) {
    return QString("QDialog { background:%1; color:%2; }"
                   "QLabel { color:%3; font-size:11px; }"
                   "QLineEdit, QDateEdit, QComboBox {"
                   "  background:%4; color:%2; border:1px solid %5;"
                   "  padding:5px 8px; font-size:12px; }"
                   "QLineEdit:focus, QDateEdit:focus { border-color:%6; }"
                   "QComboBox::drop-down { border:none; }"
                   "QComboBox QAbstractItemView { background:%4; color:%2;"
                   "  selection-background-color:%6; }")
        .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_SECONDARY(), ui::colors::BG_BASE(),
             ui::colors::BORDER_MED(), accent);
}

AddDividendDialog::AddDividendDialog(const QStringList& symbols, QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Record Dividend"));
    setFixedSize(360, 300);
    setStyleSheet(kDividendStyle(ui::colors::CYAN));

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->setContentsMargins(20, 16, 20, 16);

    title_label_ = new QLabel(tr("RECORD DIVIDEND"));
    title_label_->setStyleSheet(
        QString("color:%1; font-size:13px; font-weight:700; letter-spacing:1px;").arg(ui::colors::CYAN()));
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

    notes_edit_ = new QLineEdit;
    notes_edit_->setPlaceholderText(tr("Optional note"));
    notes_row_label_ = new QLabel(tr("Notes:"));
    form->addRow(notes_row_label_, notes_edit_);

    layout->addLayout(form);
    layout->addStretch();

    auto* btn_row = new QHBoxLayout;
    btn_row->addStretch();

    cancel_btn_ = new QPushButton(tr("CANCEL"));
    cancel_btn_->setFixedHeight(32);
    cancel_btn_->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                                  "  font-size:10px; font-weight:700; padding:0 16px; }"
                                  "QPushButton:hover { background:%3; }")
                              .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED(), ui::colors::BG_HOVER()));
    connect(cancel_btn_, &QPushButton::clicked, this, &QDialog::reject);
    btn_row->addWidget(cancel_btn_);

    record_btn_ = new QPushButton(tr("RECORD"));
    record_btn_->setFixedHeight(32);
    record_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%3; border:none;"
                              "  font-size:10px; font-weight:700; padding:0 16px; }"
                              "QPushButton:hover { background:%2; }")
                          .arg(ui::colors::CYAN(), ui::colors::TEXT_PRIMARY(), ui::colors::BG_BASE()));
    connect(record_btn_, &QPushButton::clicked, this, [this]() {
        if (amount_edit_->text().trimmed().isEmpty()) {
            amount_edit_->setPlaceholderText(tr("Required!"));
            return;
        }
        accept();
    });
    btn_row->addWidget(record_btn_);

    layout->addLayout(btn_row);
}

void AddDividendDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void AddDividendDialog::retranslateUi() {
    setWindowTitle(tr("Record Dividend"));
    if (title_label_)      title_label_->setText(tr("RECORD DIVIDEND"));
    if (symbol_row_label_) symbol_row_label_->setText(tr("Symbol:"));
    if (amount_row_label_) amount_row_label_->setText(tr("Amount/share:"));
    if (date_row_label_)   date_row_label_->setText(tr("Ex-div date:"));
    if (notes_row_label_)  notes_row_label_->setText(tr("Notes:"));
    if (amount_edit_)      amount_edit_->setPlaceholderText(tr("e.g. 0.88"));
    if (notes_edit_)       notes_edit_->setPlaceholderText(tr("Optional note"));
    if (cancel_btn_)       cancel_btn_->setText(tr("CANCEL"));
    if (record_btn_)       record_btn_->setText(tr("RECORD"));
}

QString AddDividendDialog::symbol() const {
    return symbol_cb_->currentText();
}

double AddDividendDialog::amount_per_share() const {
    return amount_edit_->text().trimmed().toDouble();
}

QString AddDividendDialog::date() const {
    return date_edit_->date().toString("yyyy-MM-dd");
}

QString AddDividendDialog::notes() const {
    return notes_edit_->text().trimmed();
}

} // namespace fincept::screens
