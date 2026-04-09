// src/screens/portfolio/PortfolioDialogs.cpp
#include "screens/portfolio/PortfolioDialogs.h"

#include "network/http/HttpClient.h"
#include "services/file_manager/FileManagerService.h"
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
    setWindowTitle("Create Portfolio");
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
                      .arg(ui::colors::BG_SURFACE, ui::colors::TEXT_PRIMARY, ui::colors::TEXT_SECONDARY,
                           ui::colors::BG_BASE, ui::colors::BORDER_MED, ui::colors::AMBER, ui::colors::AMBER_DIM));

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->setContentsMargins(20, 16, 20, 16);

    // Header
    auto* title = new QLabel("CREATE NEW PORTFOLIO");
    title->setStyleSheet(
        QString("color:%1; font-size:13px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    layout->addWidget(title);

    // Form
    auto* form = new QFormLayout;
    form->setSpacing(8);
    form->setLabelAlignment(Qt::AlignRight);

    name_edit_ = new QLineEdit;
    name_edit_->setPlaceholderText("My Portfolio");
    form->addRow("Name:", name_edit_);

    owner_edit_ = new QLineEdit;
    owner_edit_->setPlaceholderText("Your name");
    form->addRow("Owner:", owner_edit_);

    currency_cb_ = new QComboBox;
    QStringList currencies = {
        "USD", "EUR", "GBP", "JPY", "CHF", "CAD", "AUD", "NZD", "HKD", "SGD", "SEK", "NOK",
        "DKK", "CNY", "INR", "BRL", "MXN", "KRW", "TWD", "THB", "ZAR", "TRY", "RUB", "PLN",
        "CZK", "HUF", "ILS", "AED", "SAR", "IDR", "MYR", "PHP", "VND", "NGN", "EGP", "BDT",
    };
    currency_cb_->addItems(currencies);
    form->addRow("Currency:", currency_cb_);

    layout->addLayout(form);
    layout->addStretch();

    // Buttons
    auto* btn_layout = new QHBoxLayout;
    btn_layout->addStretch();

    auto* cancel_btn = new QPushButton("CANCEL");
    cancel_btn->setFixedSize(90, 30);
    cancel_btn->setCursor(Qt::PointingHandCursor);
    cancel_btn->setStyleSheet(
        QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                "  font-size:10px; font-weight:700; }"
                "QPushButton:hover { background:%3; color:%4; }")
            .arg(ui::colors::TEXT_SECONDARY, ui::colors::BORDER_MED, ui::colors::BG_HOVER, ui::colors::TEXT_PRIMARY));
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
    btn_layout->addWidget(cancel_btn);

    auto* create_btn = new QPushButton("CREATE");
    create_btn->setFixedSize(90, 30);
    create_btn->setCursor(Qt::PointingHandCursor);
    create_btn->setStyleSheet(QString("QPushButton { background:%1; color:%3; border:none;"
                                      "  font-size:10px; font-weight:700; }"
                                      "QPushButton:hover { background:%2; }")
                                  .arg(ui::colors::AMBER, ui::colors::WARNING, ui::colors::BG_BASE));
    connect(create_btn, &QPushButton::clicked, this, [this]() {
        if (!name_edit_->text().trimmed().isEmpty())
            accept();
    });
    btn_layout->addWidget(create_btn);

    layout->addLayout(btn_layout);

    name_edit_->setFocus();
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

ConfirmDeleteDialog::ConfirmDeleteDialog(const QString& portfolio_name, QWidget* parent) : QDialog(parent) {
    setWindowTitle("Delete Portfolio");
    setFixedSize(340, 160);
    setStyleSheet(
        QString("QDialog { background:%1; color:%2; }").arg(ui::colors::BG_SURFACE, ui::colors::TEXT_PRIMARY));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(12);

    auto* icon_label = new QLabel(QString("\u26A0  DELETE PORTFOLIO"));
    icon_label->setStyleSheet(QString("color:%1; font-size:13px; font-weight:700;").arg(ui::colors::NEGATIVE));
    layout->addWidget(icon_label);

    auto* msg = new QLabel(QString("Are you sure you want to delete \"%1\"?\n"
                                   "This will remove all holdings and transactions.")
                               .arg(portfolio_name));
    msg->setWordWrap(true);
    msg->setStyleSheet(QString("color:%1; font-size:11px;").arg(ui::colors::TEXT_SECONDARY));
    layout->addWidget(msg);

    layout->addStretch();

    auto* btn_layout = new QHBoxLayout;
    btn_layout->addStretch();

    auto* cancel_btn = new QPushButton("CANCEL");
    cancel_btn->setFixedSize(90, 30);
    cancel_btn->setCursor(Qt::PointingHandCursor);
    cancel_btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                                      "  font-size:10px; font-weight:700; }"
                                      "QPushButton:hover { background:%3; }")
                                  .arg(ui::colors::TEXT_SECONDARY, ui::colors::BORDER_MED, ui::colors::BG_HOVER));
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
    btn_layout->addWidget(cancel_btn);

    auto* delete_btn = new QPushButton("DELETE");
    delete_btn->setFixedSize(90, 30);
    delete_btn->setCursor(Qt::PointingHandCursor);
    delete_btn->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:none;"
                                      "  font-size:10px; font-weight:700; }"
                                      "QPushButton:hover { background:%1; }")
                                  .arg(ui::colors::NEGATIVE, ui::colors::TEXT_PRIMARY));
    connect(delete_btn, &QPushButton::clicked, this, &QDialog::accept);
    btn_layout->addWidget(delete_btn);

    layout->addLayout(btn_layout);
}

// ── AddAssetDialog ───────────────────────────────────────────────────────────

static constexpr int kAssetSearchDebounceMs = 300;
static constexpr int kAssetSearchLimit = 10;

AddAssetDialog::AddAssetDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Add Asset");
    setFixedSize(400, 260);
    setStyleSheet(QString("QDialog { background:%1; color:%2; }"
                          "QLabel { color:%3; font-size:11px; }"
                          "QLineEdit { background:%4; color:%2; border:1px solid %5;"
                          "  padding:6px 10px; font-size:12px; }"
                          "QLineEdit:focus { border-color:%6; }")
                      .arg(ui::colors::BG_SURFACE, ui::colors::TEXT_PRIMARY, ui::colors::TEXT_SECONDARY,
                           ui::colors::BG_BASE, ui::colors::BORDER_MED, ui::colors::AMBER));

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->setContentsMargins(20, 16, 20, 16);

    auto* title = new QLabel("BUY ASSET");
    title->setStyleSheet(
        QString("color:%1; font-size:13px; font-weight:700; letter-spacing:1px;").arg(ui::colors::POSITIVE));
    layout->addWidget(title);

    // Hint label under title
    auto* hint = new QLabel("Type a ticker or company name to search");
    hint->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::TEXT_TERTIARY));
    layout->addWidget(hint);

    auto* form = new QFormLayout;
    form->setSpacing(8);

    symbol_edit_ = new QLineEdit;
    symbol_edit_->setPlaceholderText("e.g. AAPL, Apple, Reliance…");
    symbol_edit_->installEventFilter(this);
    form->addRow("Symbol:", symbol_edit_);

    quantity_edit_ = new QLineEdit;
    quantity_edit_->setPlaceholderText("e.g. 10");
    form->addRow("Quantity:", quantity_edit_);

    price_edit_ = new QLineEdit;
    price_edit_->setPlaceholderText("e.g. 150.00");
    form->addRow("Price:", price_edit_);

    layout->addLayout(form);
    layout->addStretch();

    auto* btn_layout = new QHBoxLayout;
    btn_layout->addStretch();

    auto* cancel_btn = new QPushButton("CANCEL");
    cancel_btn->setFixedSize(80, 28);
    cancel_btn->setCursor(Qt::PointingHandCursor);
    cancel_btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                                      "  font-size:10px; font-weight:700; }"
                                      "QPushButton:hover { background:%3; }")
                                  .arg(ui::colors::TEXT_SECONDARY, ui::colors::BORDER_MED, ui::colors::BG_HOVER));
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
    btn_layout->addWidget(cancel_btn);

    auto* add_btn = new QPushButton("ADD");
    add_btn->setFixedSize(80, 28);
    add_btn->setCursor(Qt::PointingHandCursor);
    add_btn->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:none;"
                                   "  font-size:10px; font-weight:700; }"
                                   "QPushButton:hover { background:%1; }")
                               .arg(ui::colors::POSITIVE, ui::colors::BG_BASE));
    connect(add_btn, &QPushButton::clicked, this, [this]() {
        if (!symbol_edit_->text().trimmed().isEmpty() && quantity() > 0 && price() > 0)
            accept();
    });
    btn_layout->addWidget(add_btn);

    layout->addLayout(btn_layout);

    // ── Search dropdown (floats over the dialog, parented to this) ────────────
    search_frame_ = new QFrame(this);
    search_frame_->setObjectName("assetSearchFrame");
    search_frame_->setStyleSheet(
        QString("QFrame#assetSearchFrame { background:%1; border:1px solid %2; border-top:none; }")
            .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_MED));
    search_frame_->hide();

    auto* frame_layout = new QVBoxLayout(search_frame_);
    frame_layout->setContentsMargins(0, 0, 0, 0);
    frame_layout->setSpacing(0);

    search_list_ = new QListWidget(search_frame_);
    search_list_->setStyleSheet(QString("QListWidget { background:transparent; border:none; outline:none; }"
                                        "QListWidget::item { padding:0; border:none; background:transparent; }"
                                        "QListWidget::item:selected { background:%1; border-left:3px solid %2; }")
                                    .arg(ui::colors::BORDER_DIM, ui::colors::AMBER));
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
    const QString url = QString("/market/search?q=%1&type=stock&limit=%2").arg(query).arg(kAssetSearchLimit);

    QPointer<AddAssetDialog> self = this;
    fincept::HttpClient::instance().get(url, [self, query](fincept::Result<QJsonDocument> result) {
        if (!self || self->pending_query_ != query)
            return;
        if (!result.is_ok())
            return;

        const auto doc = result.value();
        QJsonArray arr;
        if (doc.isArray()) {
            arr = doc.array();
        } else if (doc.isObject()) {
            const auto obj = doc.object();
            if (obj.contains("results"))
                arr = obj["results"].toArray();
            else if (obj.contains("data"))
                arr = obj["data"].toArray();
        }
        QMetaObject::invokeMethod(
            self,
            [self, arr]() {
                if (self)
                    self->show_results(arr);
            },
            Qt::QueuedConnection);
    });
}

void AddAssetDialog::show_results(const QJsonArray& results) {
    search_list_->clear();

    if (results.isEmpty()) {
        auto* item = new QListWidgetItem(search_list_);
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        auto* row = new QWidget(this);
        row->setStyleSheet("background:transparent;");
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(10, 6, 10, 6);
        auto* lbl = new QLabel("No results found");
        lbl->setStyleSheet(
            QString("color:%1; font-size:11px; font-family:'Consolas',monospace; background:transparent;")
                .arg(ui::colors::TEXT_TERTIARY));
        rl->addWidget(lbl);
        item->setSizeHint(QSize(0, 28));
        search_list_->setItemWidget(item, row);
        position_dropdown();
        search_frame_->show();
        return;
    }

    for (const auto& val : results) {
        const auto obj = val.toObject();
        const QString sym = obj["symbol"].toString();
        const QString name = obj["name"].toString();
        const QString exch = obj["exchange"].toString();
        const QString type = obj["type"].toString("stock");
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
                                   .arg(ui::colors::TEXT_PRIMARY));
        sym_lbl->setFixedWidth(100);

        auto* name_lbl = new QLabel(name);
        name_lbl->setStyleSheet(QString("color:%1; font-size:11px; background:transparent;"
                                        "font-family:'Consolas',monospace;")
                                    .arg(ui::colors::TEXT_SECONDARY));
        name_lbl->setMaximumWidth(180);

        hl->addWidget(sym_lbl);
        hl->addWidget(name_lbl, 1);

        if (!exch.isEmpty()) {
            auto* exch_lbl = new QLabel(exch);
            exch_lbl->setStyleSheet(QString("color:%1; font-size:10px; font-family:'Consolas',monospace;"
                                            "background:transparent;")
                                        .arg(ui::colors::TEXT_TERTIARY));
            hl->addWidget(exch_lbl);
        }

        auto* type_lbl = new QLabel(type.toUpper());
        type_lbl->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700;"
                                        "font-family:'Consolas',monospace; background:%2;"
                                        "padding:1px 4px; border-radius:2px;")
                                    .arg(ui::colors::AMBER, ui::colors::BORDER_DIM));
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

SellAssetDialog::SellAssetDialog(const QString& symbol, double held_qty, QWidget* parent) : QDialog(parent) {
    setWindowTitle("Sell Asset");
    setFixedSize(340, 220);
    setStyleSheet(QString("QDialog { background:%1; color:%2; }"
                          "QLabel { color:%3; font-size:11px; }"
                          "QLineEdit { background:%4; color:%2; border:1px solid %5;"
                          "  padding:6px 10px; font-size:12px; }"
                          "QLineEdit:focus { border-color:%6; }")
                      .arg(ui::colors::BG_SURFACE, ui::colors::TEXT_PRIMARY, ui::colors::TEXT_SECONDARY,
                           ui::colors::BG_BASE, ui::colors::BORDER_MED, ui::colors::AMBER));

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->setContentsMargins(20, 16, 20, 16);

    auto* title = new QLabel(QString("SELL %1").arg(symbol));
    title->setStyleSheet(
        QString("color:%1; font-size:13px; font-weight:700; letter-spacing:1px;").arg(ui::colors::NEGATIVE));
    layout->addWidget(title);

    auto* held_label = new QLabel(QString("Currently holding: %1 shares").arg(held_qty));
    held_label->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::TEXT_TERTIARY));
    layout->addWidget(held_label);

    auto* form = new QFormLayout;
    form->setSpacing(8);

    quantity_edit_ = new QLineEdit;
    quantity_edit_->setPlaceholderText(QString("Max %1").arg(held_qty));
    form->addRow("Quantity:", quantity_edit_);

    price_edit_ = new QLineEdit;
    price_edit_->setPlaceholderText("Sell price");
    form->addRow("Price:", price_edit_);

    layout->addLayout(form);
    layout->addStretch();

    auto* btn_layout = new QHBoxLayout;
    btn_layout->addStretch();

    auto* cancel_btn = new QPushButton("CANCEL");
    cancel_btn->setFixedSize(80, 28);
    cancel_btn->setCursor(Qt::PointingHandCursor);
    cancel_btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                                      "  font-size:10px; font-weight:700; }"
                                      "QPushButton:hover { background:%3; }")
                                  .arg(ui::colors::TEXT_SECONDARY, ui::colors::BORDER_MED, ui::colors::BG_HOVER));
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
    btn_layout->addWidget(cancel_btn);

    auto* sell_btn = new QPushButton("SELL");
    sell_btn->setFixedSize(80, 28);
    sell_btn->setCursor(Qt::PointingHandCursor);
    sell_btn->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:none;"
                                    "  font-size:10px; font-weight:700; }"
                                    "QPushButton:hover { background:%1; }")
                                .arg(ui::colors::NEGATIVE, ui::colors::TEXT_PRIMARY));
    connect(sell_btn, &QPushButton::clicked, this, [this, held_qty]() {
        if (quantity() > 0 && quantity() <= held_qty && price() > 0)
            accept();
    });
    btn_layout->addWidget(sell_btn);

    layout->addLayout(btn_layout);
    quantity_edit_->setFocus();
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
    setWindowTitle("Import Portfolio");
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
                      .arg(ui::colors::BG_SURFACE, ui::colors::TEXT_PRIMARY, ui::colors::TEXT_SECONDARY,
                           ui::colors::BG_BASE, ui::colors::BORDER_MED, ui::colors::AMBER));

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->setContentsMargins(20, 16, 20, 16);

    auto* title = new QLabel("IMPORT PORTFOLIO");
    title->setStyleSheet(
        QString("color:%1; font-size:13px; font-weight:700; letter-spacing:1px;").arg(ui::colors::CYAN));
    layout->addWidget(title);

    // File picker
    auto* file_row = new QHBoxLayout;
    file_edit_ = new QLineEdit;
    file_edit_->setPlaceholderText("Select JSON file...");
    file_edit_->setReadOnly(true);
    file_row->addWidget(file_edit_, 1);

    auto* browse_btn = new QPushButton("BROWSE");
    browse_btn->setFixedSize(80, 30);
    browse_btn->setCursor(Qt::PointingHandCursor);
    browse_btn->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:none;"
                                      "  font-size:10px; font-weight:700; }"
                                      "QPushButton:hover { background:%1; }")
                                  .arg(ui::colors::CYAN, ui::colors::BG_BASE));
    connect(browse_btn, &QPushButton::clicked, this, &ImportPortfolioDialog::browse_file);
    file_row->addWidget(browse_btn);

    layout->addLayout(file_row);

    // Demo JSON download hint
    auto* demo_row = new QHBoxLayout;
    auto* demo_hint = new QLabel("Need a template? Download the demo portfolio JSON:");
    demo_hint->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::TEXT_TERTIARY));
    demo_row->addWidget(demo_hint, 1);

    auto* demo_dl_btn = new QPushButton("DOWNLOAD DEMO");
    demo_dl_btn->setFixedSize(120, 24);
    demo_dl_btn->setCursor(Qt::PointingHandCursor);
    demo_dl_btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %1;"
                                       "  font-size:9px; font-weight:700; letter-spacing:0.3px; }"
                                       "QPushButton:hover { background:%1; color:%2; }")
                                   .arg(ui::colors::CYAN, ui::colors::BG_BASE));
    connect(demo_dl_btn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getSaveFileName(this, "Save Demo Portfolio JSON", "demo_portfolio.json",
                                                    "JSON Files (*.json)");
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
            QMessageBox::information(this, "Demo JSON Saved",
                                     "Demo portfolio JSON saved.\nYou can now import it using the BROWSE button.");
        } else {
            QMessageBox::warning(this, "Save Failed", "Could not write to: " + path);
        }
    });
    demo_row->addWidget(demo_dl_btn);
    layout->addLayout(demo_row);

    // Import mode
    auto* mode_label = new QLabel("IMPORT MODE");
    mode_label->setStyleSheet(
        QString("color:%1; font-size:9px; font-weight:700; letter-spacing:0.5px;").arg(ui::colors::TEXT_TERTIARY));
    layout->addWidget(mode_label);

    new_radio_ = new QRadioButton("Create new portfolio from file");
    new_radio_->setChecked(true);
    layout->addWidget(new_radio_);

    merge_radio_ = new QRadioButton("Merge transactions into existing portfolio:");
    layout->addWidget(merge_radio_);

    target_cb_ = new QComboBox;
    target_cb_->setEnabled(false);
    for (const auto& p : portfolios)
        target_cb_->addItem(QString("%1 (%2)").arg(p.name, p.currency), p.id);
    layout->addWidget(target_cb_);

    connect(merge_radio_, &QRadioButton::toggled, target_cb_, &QComboBox::setEnabled);

    status_label_ = new QLabel;
    status_label_->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::TEXT_TERTIARY));
    layout->addWidget(status_label_);

    layout->addStretch();

    // Buttons
    auto* btn_layout = new QHBoxLayout;
    btn_layout->addStretch();

    auto* cancel_btn = new QPushButton("CANCEL");
    cancel_btn->setFixedSize(80, 28);
    cancel_btn->setCursor(Qt::PointingHandCursor);
    cancel_btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                                      "  font-size:10px; font-weight:700; }"
                                      "QPushButton:hover { background:%3; }")
                                  .arg(ui::colors::TEXT_SECONDARY, ui::colors::BORDER_MED, ui::colors::BG_HOVER));
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
    btn_layout->addWidget(cancel_btn);

    auto* import_btn = new QPushButton("IMPORT");
    import_btn->setFixedSize(80, 28);
    import_btn->setCursor(Qt::PointingHandCursor);
    import_btn->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:none;"
                                      "  font-size:10px; font-weight:700; }"
                                      "QPushButton:hover { background:%1; }")
                                  .arg(ui::colors::CYAN, ui::colors::BG_BASE));
    connect(import_btn, &QPushButton::clicked, this, [this]() {
        if (!file_edit_->text().isEmpty())
            accept();
    });
    btn_layout->addWidget(import_btn);

    layout->addLayout(btn_layout);
}

void ImportPortfolioDialog::browse_file() {
    QString path = QFileDialog::getOpenFileName(this, "Select Portfolio JSON", QString(), "JSON Files (*.json)");
    if (!path.isEmpty()) {
        file_edit_->setText(path);
        status_label_->setText("File selected: " + QFileInfo(path).fileName());
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

EditTransactionDialog::EditTransactionDialog(const portfolio::Transaction& txn, QWidget* parent) : QDialog(parent) {
    setWindowTitle("Edit Transaction");
    setFixedSize(380, 280);
    setStyleSheet(QString("QDialog { background:%1; color:%2; }"
                          "QLabel { color:%3; font-size:11px; }"
                          "QLineEdit { background:%4; color:%2; border:1px solid %5;"
                          "  padding:6px 10px; font-size:12px; }"
                          "QLineEdit:focus { border-color:%6; }")
                      .arg(ui::colors::BG_SURFACE, ui::colors::TEXT_PRIMARY, ui::colors::TEXT_SECONDARY,
                           ui::colors::BG_BASE, ui::colors::BORDER_MED, ui::colors::AMBER));

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->setContentsMargins(20, 16, 20, 16);

    auto* title = new QLabel(QString("EDIT %1 — %2").arg(txn.transaction_type, txn.symbol));
    title->setStyleSheet(
        QString("color:%1; font-size:13px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    layout->addWidget(title);

    auto* form = new QFormLayout;
    form->setSpacing(8);

    quantity_edit_ = new QLineEdit(QString::number(txn.quantity, 'f', 2));
    form->addRow("Quantity:", quantity_edit_);

    price_edit_ = new QLineEdit(QString::number(txn.price, 'f', 2));
    form->addRow("Price:", price_edit_);

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
            .arg(ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED, ui::colors::AMBER));
    form->addRow("Date:", date_edit_);

    notes_edit_ = new QLineEdit(txn.notes);
    notes_edit_->setPlaceholderText("Optional notes");
    form->addRow("Notes:", notes_edit_);

    layout->addLayout(form);
    layout->addStretch();

    auto* btn_layout = new QHBoxLayout;
    btn_layout->addStretch();

    auto* cancel_btn = new QPushButton("CANCEL");
    cancel_btn->setFixedSize(80, 28);
    cancel_btn->setCursor(Qt::PointingHandCursor);
    cancel_btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                                      "  font-size:10px; font-weight:700; }"
                                      "QPushButton:hover { background:%3; }")
                                  .arg(ui::colors::TEXT_SECONDARY, ui::colors::BORDER_MED, ui::colors::BG_HOVER));
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
    btn_layout->addWidget(cancel_btn);

    auto* save_btn = new QPushButton("SAVE");
    save_btn->setFixedSize(80, 28);
    save_btn->setCursor(Qt::PointingHandCursor);
    save_btn->setStyleSheet(QString("QPushButton { background:%1; color:%3; border:none;"
                                    "  font-size:10px; font-weight:700; }"
                                    "QPushButton:hover { background:%2; }")
                                .arg(ui::colors::AMBER, ui::colors::WARNING, ui::colors::BG_BASE));
    connect(save_btn, &QPushButton::clicked, this, [this]() {
        if (quantity() > 0 && price() > 0)
            accept();
    });
    btn_layout->addWidget(save_btn);

    layout->addLayout(btn_layout);
    quantity_edit_->setFocus();
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
    setWindowTitle("Sector Mapping");
    setFixedSize(440, std::min(500, 120 + static_cast<int>(holdings.size()) * 34));
    setStyleSheet(QString("QDialog { background:%1; color:%2; }"
                          "QLabel { color:%3; font-size:11px; }"
                          "QComboBox { background:%4; color:%2; border:1px solid %5;"
                          "  padding:4px 8px; font-size:11px; }"
                          "QComboBox::drop-down { border:none; }"
                          "QComboBox QAbstractItemView { background:%4; color:%2; }")
                      .arg(ui::colors::BG_SURFACE, ui::colors::TEXT_PRIMARY, ui::colors::TEXT_SECONDARY,
                           ui::colors::BG_BASE, ui::colors::BORDER_MED));

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);
    layout->setContentsMargins(20, 16, 20, 16);

    auto* title = new QLabel("MAP HOLDINGS TO SECTORS");
    title->setStyleSheet(
        QString("color:%1; font-size:13px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    layout->addWidget(title);

    auto* desc = new QLabel("Assign each holding to a sector for allocation analysis.");
    desc->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::TEXT_TERTIARY));
    layout->addWidget(desc);

    // Scrollable grid
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QString("QScrollArea { border:none; }"
                                  "QScrollBar:vertical { width:4px; background:transparent; }"
                                  "QScrollBar::handle:vertical { background:%1; }")
                              .arg(ui::colors::BORDER_BRIGHT));

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
        sym_label->setStyleSheet(QString("color:%1; font-weight:600;").arg(ui::colors::CYAN));
        grid->addRow(sym_label, cb);
    }

    scroll->setWidget(grid_w);
    layout->addWidget(scroll, 1);

    auto* btn_layout = new QHBoxLayout;
    btn_layout->addStretch();

    auto* cancel_btn = new QPushButton("CANCEL");
    cancel_btn->setFixedSize(80, 28);
    cancel_btn->setCursor(Qt::PointingHandCursor);
    cancel_btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                                      "  font-size:10px; font-weight:700; }"
                                      "QPushButton:hover { background:%3; }")
                                  .arg(ui::colors::TEXT_SECONDARY, ui::colors::BORDER_MED, ui::colors::BG_HOVER));
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
    btn_layout->addWidget(cancel_btn);

    auto* save_btn = new QPushButton("SAVE");
    save_btn->setFixedSize(80, 28);
    save_btn->setCursor(Qt::PointingHandCursor);
    save_btn->setStyleSheet(QString("QPushButton { background:%1; color:%3; border:none;"
                                    "  font-size:10px; font-weight:700; }"
                                    "QPushButton:hover { background:%2; }")
                                .arg(ui::colors::AMBER, ui::colors::WARNING, ui::colors::BG_BASE));
    connect(save_btn, &QPushButton::clicked, this, &QDialog::accept);
    btn_layout->addWidget(save_btn);

    layout->addLayout(btn_layout);
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
        .arg(ui::colors::BG_SURFACE, ui::colors::TEXT_PRIMARY, ui::colors::TEXT_SECONDARY, ui::colors::BG_BASE,
             ui::colors::BORDER_MED, accent);
}

AddDividendDialog::AddDividendDialog(const QStringList& symbols, QWidget* parent) : QDialog(parent) {
    setWindowTitle("Record Dividend");
    setFixedSize(360, 300);
    setStyleSheet(kDividendStyle(ui::colors::CYAN));

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->setContentsMargins(20, 16, 20, 16);

    auto* title = new QLabel("RECORD DIVIDEND");
    title->setStyleSheet(
        QString("color:%1; font-size:13px; font-weight:700; letter-spacing:1px;").arg(ui::colors::CYAN));
    layout->addWidget(title);

    auto* form = new QFormLayout;
    form->setSpacing(8);
    form->setLabelAlignment(Qt::AlignRight);

    symbol_cb_ = new QComboBox;
    for (const auto& s : symbols)
        symbol_cb_->addItem(s);
    form->addRow("Symbol:", symbol_cb_);

    amount_edit_ = new QLineEdit;
    amount_edit_->setPlaceholderText("e.g. 0.88");
    form->addRow("Amount/share:", amount_edit_);

    date_edit_ = new QDateEdit;
    date_edit_->setCalendarPopup(true);
    date_edit_->setDisplayFormat("yyyy-MM-dd");
    date_edit_->setDate(QDate::currentDate());
    form->addRow("Ex-div date:", date_edit_);

    notes_edit_ = new QLineEdit;
    notes_edit_->setPlaceholderText("Optional note");
    form->addRow("Notes:", notes_edit_);

    layout->addLayout(form);
    layout->addStretch();

    auto* btn_row = new QHBoxLayout;
    btn_row->addStretch();

    auto* cancel = new QPushButton("CANCEL");
    cancel->setFixedHeight(32);
    cancel->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                                  "  font-size:10px; font-weight:700; padding:0 16px; }"
                                  "QPushButton:hover { background:%3; }")
                              .arg(ui::colors::TEXT_SECONDARY, ui::colors::BORDER_MED, ui::colors::BG_HOVER));
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    btn_row->addWidget(cancel);

    auto* ok = new QPushButton("RECORD");
    ok->setFixedHeight(32);
    ok->setStyleSheet(QString("QPushButton { background:%1; color:%3; border:none;"
                              "  font-size:10px; font-weight:700; padding:0 16px; }"
                              "QPushButton:hover { background:%2; }")
                          .arg(ui::colors::CYAN, ui::colors::TEXT_PRIMARY, ui::colors::BG_BASE));
    connect(ok, &QPushButton::clicked, this, [this]() {
        if (amount_edit_->text().trimmed().isEmpty()) {
            amount_edit_->setPlaceholderText("Required!");
            return;
        }
        accept();
    });
    btn_row->addWidget(ok);

    layout->addLayout(btn_row);
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
