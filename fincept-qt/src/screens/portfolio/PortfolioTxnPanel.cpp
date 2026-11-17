// src/screens/portfolio/PortfolioTxnPanel.cpp
#include "screens/portfolio/PortfolioTxnPanel.h"

#include "core/market/InstrumentCurrency.h"
#include "screens/portfolio/PortfolioFormat.h"
#include "ui/charts/ChartExport.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/tables/NumericTableWidgetItem.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/PanelHeader.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QComboBox>
#include <QDate>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QScrollBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

namespace {

enum TxnCol { kTxDate, kTxType, kTxSymbol, kTxQty, kTxPrice, kTxCcy, kTxGross, kTxFees, kTxTaxes, kTxFx, kTxNet, kTxNotes, kTxColCount };

constexpr int kTxIdRole = Qt::UserRole + 1;
constexpr int kTxExportRole = Qt::UserRole + 2;

QStringList txn_headers() {
    return {PortfolioTxnPanel::tr("DATE"),  PortfolioTxnPanel::tr("TYPE"),  PortfolioTxnPanel::tr("SYMBOL"),
            PortfolioTxnPanel::tr("QTY"),   PortfolioTxnPanel::tr("PRICE"), PortfolioTxnPanel::tr("CCY"),
            PortfolioTxnPanel::tr("GROSS"), PortfolioTxnPanel::tr("FEES"),  PortfolioTxnPanel::tr("TAXES"),
            PortfolioTxnPanel::tr("FX"),    PortfolioTxnPanel::tr("NET CASH"), PortfolioTxnPanel::tr("NOTES")};
}

QString txn_type_label(const QString& t) {
    namespace tx = portfolio::txn;
    if (t == tx::kBuy)
        return PortfolioTxnPanel::tr("BUY");
    if (t == tx::kSell)
        return PortfolioTxnPanel::tr("SELL");
    if (t == tx::kDividend)
        return PortfolioTxnPanel::tr("DIVIDEND");
    if (t == tx::kSplit)
        return PortfolioTxnPanel::tr("SPLIT");
    if (t == tx::kDeposit)
        return PortfolioTxnPanel::tr("DEPOSIT");
    if (t == tx::kWithdrawal)
        return PortfolioTxnPanel::tr("WITHDRAWAL");
    if (t == tx::kFee)
        return PortfolioTxnPanel::tr("FEE");
    if (t == tx::kTax)
        return PortfolioTxnPanel::tr("TAX");
    if (t == tx::kInterest)
        return PortfolioTxnPanel::tr("INTEREST");
    return t;
}

/// Cash effect of a row in its own currency (+ in, − out). NaN for a split.
double txn_net_cash(const portfolio::Transaction& t) {
    namespace tx = portfolio::txn;
    const QString type = t.transaction_type.toUpper();
    const double gross = t.quantity * t.price;
    if (type == tx::kBuy)
        return -(gross + t.fees + t.taxes);
    if (type == tx::kSell)
        return gross - t.fees - t.taxes;
    if (type == tx::kDividend)
        return gross - t.fees - t.taxes;
    if (type == tx::kDeposit)
        return t.quantity;
    if (type == tx::kWithdrawal || type == tx::kFee || type == tx::kTax)
        return -t.quantity;
    if (type == tx::kInterest)
        return t.quantity - t.taxes;
    return portfolio::kAbsent;
}

QString txn_ccy(const portfolio::Transaction& t, const QString& base) {
    if (!t.currency.isEmpty())
        return t.currency;
    if (portfolio::txn::is_cash_only(t.transaction_type))
        return base;
    return core::market::currency_for_symbol(t.symbol);
}

QColor txn_type_color(const QString& type) {
    namespace tx = portfolio::txn;
    if (type == tx::kBuy)
        return QColor(ui::colors::POSITIVE());
    if (type == tx::kSell)
        return QColor(ui::colors::NEGATIVE());
    if (type == tx::kDividend || type == tx::kInterest)
        return QColor(ui::colors::CYAN());
    if (type == tx::kDeposit || type == tx::kWithdrawal)
        return QColor(ui::colors::INFO());
    if (type == tx::kFee || type == tx::kTax)
        return QColor(ui::colors::WARNING());
    return QColor(ui::colors::TEXT_SECONDARY());
}

QTableWidgetItem* txn_num_item(const QString& text, double v, const QString& exported) {
    auto* it = new ui::NumericTableWidgetItem(text, std::isfinite(v) ? v : -1e300);
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    it->setData(kTxExportRole, exported);
    return it;
}

QString txn_csv(double v, int dp) {
    return ui::formatting::export_number(v, dp); // NaN -> ""
}

int txn_collapse_side() {
    return std::max(22, ui::fonts::font_px(-3) + 11);
}

} // namespace

PortfolioTxnPanel::PortfolioTxnPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
    // Re-format the held rows (selection and scroll position are kept by populate()).
    ui::formatting::on_format_changed(this, [this]() { populate(); });
}

void PortfolioTxnPanel::build_ui() {
    // Panel stylesheet BEFORE the children exist (table_ is still null: apply_styles() skips
    // it); the call at the end then only styles the table. Set last, it re-styled every
    // child of the panel a second time.
    apply_styles();
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto header = ui::make_panel_header(tr("TRANSACTION HISTORY"), this);
    title_label_ = header.title_label;
    auto* slot = header.controls_slot->layout();

    count_label_ = new QLabel(this);
    count_label_->setObjectName("pfTxnCount");
    slot->addWidget(count_label_);

    type_cb_ = new QComboBox(this);
    type_cb_->addItem(tr("All types"), QString());
    for (const QString& t : portfolio::txn::all_types())
        type_cb_->addItem(txn_type_label(t), t);
    type_cb_->setAccessibleName(tr("Filter by transaction type"));
    connect(type_cb_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { populate(); });
    slot->addWidget(type_cb_);

    search_edit_ = new QLineEdit(this);
    search_edit_->setPlaceholderText(tr("Filter symbol / notes…"));
    search_edit_->setClearButtonEnabled(true);
    ui::metrics::follow_fixed_width(search_edit_, 170);
    connect(search_edit_, &QLineEdit::textChanged, this, [this](const QString&) { populate(); });
    slot->addWidget(search_edit_);

    export_btn_ = new QPushButton(tr("EXPORT"), this);
    export_btn_->setCursor(Qt::PointingHandCursor);
    export_btn_->setToolTip(tr("Save the rows shown as CSV (machine-readable numbers)"));
    connect(export_btn_, &QPushButton::clicked, this, &PortfolioTxnPanel::export_visible);
    slot->addWidget(export_btn_);

    collapse_btn_ = new QPushButton("▾", this);
    collapse_btn_->setFixedSize(txn_collapse_side(), txn_collapse_side());
    collapse_btn_->setCursor(Qt::PointingHandCursor);
    collapse_btn_->setToolTip(tr("Collapse / expand transaction history"));
    connect(collapse_btn_, &QPushButton::clicked, this, [this]() {
        collapsed_ = !collapsed_;
        apply_collapsed_state();
        emit collapse_toggled(collapsed_);
    });
    slot->addWidget(collapse_btn_);
    layout->addWidget(header.header);

    table_ = new QTableWidget(0, kTxColCount, this);
    table_->setHorizontalHeaderLabels(txn_headers());
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setShowGrid(false);
    table_->setWordWrap(false);
    table_->setSortingEnabled(true);
    table_->sortByColumn(kTxDate, Qt::DescendingOrder); // newest first until the user sorts otherwise
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table_->horizontalHeader()->setStretchLastSection(true);
    ui::metrics::follow_row_height(table_, 24);

    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        const auto* sym_item = table_->item(row, kTxSymbol);
        if (sym_item && !sym_item->text().isEmpty() && sym_item->text() != pfmt::dash())
            emit symbol_activated(sym_item->text());
    });

    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table_, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        const int row = table_->rowAt(pos.y());
        const auto* date_item = row >= 0 ? table_->item(row, kTxDate) : nullptr;
        const auto* sym_item = row >= 0 ? table_->item(row, kTxSymbol) : nullptr;
        const QString id = date_item ? date_item->data(kTxIdRole).toString() : QString();
        QString symbol = sym_item ? sym_item->text() : QString();
        if (symbol == pfmt::dash())
            symbol.clear();

        QMenu menu(this);
        if (!symbol.isEmpty()) {
            QMenu* open_in = menu.addMenu(tr("Open %1 in").arg(symbol));
            const struct {
                const char* screen;
                const char* label;
            } kTargets[] = {
                {"equity_research", QT_TR_NOOP("Equity Research")}, {"equity_trading", QT_TR_NOOP("Equity Trading")},
                {"news", QT_TR_NOOP("News")},                       {"watchlist", QT_TR_NOOP("Watchlist")},
                {"backtesting", QT_TR_NOOP("Backtesting")},
            };
            for (const auto& t : kTargets) {
                const QString screen = QString::fromLatin1(t.screen);
                connect(open_in->addAction(tr(t.label)), &QAction::triggered, this,
                        [this, screen, symbol]() { emit open_symbol_requested(screen, symbol); });
            }
        }
        if (!id.isEmpty()) {
            menu.addSeparator();
            connect(menu.addAction(tr("Edit Transaction…")), &QAction::triggered, this,
                    [this, id]() { emit edit_transaction_requested(id); });
            connect(menu.addAction(tr("Delete Transaction…")), &QAction::triggered, this,
                    [this, id]() { emit delete_transaction_requested(id); });
            menu.actions().constLast()->setProperty("fincept_agent_risk", "delete");
        }
        menu.addSeparator();
        connect(menu.addAction(tr("Export Rows Shown (CSV)…")), &QAction::triggered, this,
                &PortfolioTxnPanel::export_visible);
        connect(menu.addAction(tr("Export Full Ledger (CSV)…")), &QAction::triggered, this,
                &PortfolioTxnPanel::export_requested);
        menu.exec(table_->viewport()->mapToGlobal(pos));
    });

    layout->addWidget(table_, 1);
    apply_styles();
}

void PortfolioTxnPanel::apply_styles() {
    // Runs at the end of build_ui() and again from refresh_theme() right after construction
    // (PortfolioScreen): re-applying unchanged text re-styled the whole panel for nothing.
    const auto set_css = [](QWidget* w, const QString& css) {
        if (w && w->styleSheet() != css)
            w->setStyleSheet(css);
    };
    set_css(this, QString("QWidget { background:%1; }"
                          "#pfTxnCount { color:%2; font-size:%3px; font-weight:600; background:transparent; }"
                          "QComboBox, QLineEdit { background:%4; color:%5; border:1px solid %6; padding:1px 6px;"
                          "  font-size:%7px; }"
                          "QPushButton { background:transparent; color:%8; border:1px solid %6; padding:1px 8px;"
                          "  font-size:%7px; font-weight:700; }"
                          "QPushButton:hover { color:%9; border-color:%9; }")
                      .arg(ui::colors::BG_BASE(), ui::colors::TEXT_TERTIARY(), ui::fonts::px(-4),
                           ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(),
                           ui::fonts::px(-3), ui::colors::TEXT_SECONDARY(), ui::colors::AMBER()));
    set_css(table_, QString("QTableWidget { background:%1; color:%2; border:none; font-size:%7px;"
                                  "  font-family:%3; gridline-color:%4; }"
                                  "QTableWidget::item { padding:2px 8px; border-bottom:1px solid %4; }"
                                  "QTableWidget::item:selected { background:%5; color:%2; }"
                                  "QHeaderView::section { background:%6; color:%8; border:none;"
                                  "  border-bottom:1px solid %4; border-right:1px solid %4;"
                                  "  font-size:%9px; font-weight:700; letter-spacing:0.5px; padding:3px 8px; }")
                              .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::fonts::DATA_FAMILY,
                                   ui::colors::BORDER_DIM(), ui::colors::BG_HOVER(), ui::colors::BG_SURFACE(),
                                   ui::fonts::px(-2), ui::colors::TEXT_SECONDARY(), ui::fonts::px(-4)));
    if (collapse_btn_)
        collapse_btn_->setFixedSize(txn_collapse_side(), txn_collapse_side());
}

void PortfolioTxnPanel::set_transactions(const QVector<portfolio::Transaction>& txns) {
    txns_ = txns;
    populate();
}

void PortfolioTxnPanel::set_base_currency(const QString& ccy) {
    if (ccy.isEmpty() || ccy == base_ccy_)
        return;
    base_ccy_ = ccy.toUpper();
    populate();
}

void PortfolioTxnPanel::clear() {
    txns_.clear();
    table_->clearSpans();
    table_->setRowCount(0);
    count_label_->clear();
}

void PortfolioTxnPanel::apply_collapsed_state() {
    table_->setVisible(!collapsed_);
    type_cb_->setVisible(!collapsed_);
    search_edit_->setVisible(!collapsed_);
    export_btn_->setVisible(!collapsed_);
    collapse_btn_->setText(collapsed_ ? "▸" : "▾");
}

void PortfolioTxnPanel::populate() {
    QString prev_id;
    if (const int prev_row = table_->currentRow(); prev_row >= 0)
        if (const auto* it = table_->item(prev_row, kTxDate))
            prev_id = it->data(kTxIdRole).toString();
    const int prev_scroll = table_->verticalScrollBar() ? table_->verticalScrollBar()->value() : 0;

    const QString type_filter = type_cb_ ? type_cb_->currentData().toString() : QString();
    const QString needle = search_edit_ ? search_edit_->text().trimmed().toLower() : QString();

    // Newest first: by trade date, then entry time.
    QVector<portfolio::Transaction> rows;
    rows.reserve(txns_.size());
    for (const auto& t : txns_) {
        const QString type = t.transaction_type.toUpper();
        if (!type_filter.isEmpty() && type != type_filter)
            continue;
        if (!needle.isEmpty() && !t.symbol.toLower().contains(needle) && !t.notes.toLower().contains(needle))
            continue;
        rows.append(t);
    }
    std::stable_sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
        const QString da = a.transaction_date.left(10), db = b.transaction_date.left(10);
        if (da != db)
            return da > db;
        return a.created_at > b.created_at;
    });

    table_->setSortingEnabled(false);
    table_->clearSpans();
    table_->setRowCount(0);
    table_->setRowCount(static_cast<int>(rows.size()));

    for (int r = 0; r < rows.size(); ++r) {
        const auto& t = rows[r];
        const QString type = t.transaction_type.toUpper();
        const QString ccy = txn_ccy(t, base_ccy_);
        const bool cash_only = portfolio::txn::is_cash_only(type);
        const bool split = type == portfolio::txn::kSplit;

        // Display in the locale's format, sort by the calendar date (then entry order).
        const QDate td = pfmt::iso_date(t.transaction_date);
        auto* date_item = new ui::NumericTableWidgetItem(pfmt::date(t.transaction_date, ui::formatting::DateStyle::Short),
                                                         td.isValid() ? static_cast<double>(td.toJulianDay()) - r * 1e-6
                                                                      : -1e300);
        date_item->setData(kTxIdRole, t.id);
        date_item->setData(kTxExportRole, t.transaction_date.left(10));
        date_item->setForeground(QColor(ui::colors::TEXT_SECONDARY()));
        table_->setItem(r, kTxDate, date_item);

        auto* type_item = new QTableWidgetItem(txn_type_label(type));
        type_item->setForeground(txn_type_color(type));
        type_item->setFont(ui::fonts::font(-2, QFont::Bold));
        type_item->setData(kTxExportRole, type);
        table_->setItem(r, kTxType, type_item);

        auto* sym_item = new QTableWidgetItem(t.symbol.isEmpty() ? pfmt::dash() : t.symbol);
        sym_item->setForeground(QColor(t.symbol.isEmpty() ? ui::colors::TEXT_TERTIARY() : ui::colors::TEXT_PRIMARY()));
        sym_item->setData(kTxExportRole, t.symbol);
        table_->setItem(r, kTxSymbol, sym_item);

        // QTY: units for trades / dividends, the factor for a split, blank for cash rows.
        QTableWidgetItem* qty_item = nullptr;
        if (split) {
            const double f = t.quantity;
            const QString ratio = f >= 1 ? tr("%1-for-1").arg(pfmt::qty(f)) : tr("1-for-%1").arg(pfmt::qty(1.0 / f));
            qty_item = txn_num_item(ratio, f, txn_csv(f, 8));
        } else if (cash_only) {
            qty_item = txn_num_item(QString(), portfolio::kAbsent, QString());
        } else {
            qty_item = txn_num_item(pfmt::qty(t.quantity), t.quantity, txn_csv(t.quantity, 8));
        }
        table_->setItem(r, kTxQty, qty_item);

        auto* price_item = (split || cash_only)
                               ? txn_num_item(QString(), portfolio::kAbsent, QString())
                               : txn_num_item(pfmt::price(t.price, ccy, t.symbol), t.price, txn_csv(t.price, 8));
        table_->setItem(r, kTxPrice, price_item);

        auto* ccy_item = new QTableWidgetItem(ccy.isEmpty() ? pfmt::dash() : ccy);
        ccy_item->setForeground(QColor(t.currency.isEmpty() ? ui::colors::TEXT_TERTIARY() : ui::colors::TEXT_SECONDARY()));
        ccy_item->setToolTip(t.currency.isEmpty() ? tr("Not recorded on the row — the instrument's quote currency is assumed")
                                                  : QString());
        ccy_item->setData(kTxExportRole, ccy);
        table_->setItem(r, kTxCcy, ccy_item);

        const double gross = split ? portfolio::kAbsent : (cash_only ? t.quantity : t.quantity * t.price);
        table_->setItem(r, kTxGross, txn_num_item(pfmt::money(gross, ccy), gross, txn_csv(gross, 4)));

        auto money_or_blank = [&](double v) {
            return v > 0 ? txn_num_item(pfmt::money(v, ccy), v, txn_csv(v, 4)) : txn_num_item(QString(), 0, QStringLiteral("0"));
        };
        table_->setItem(r, kTxFees, money_or_blank(t.fees));
        table_->setItem(r, kTxTaxes, money_or_blank(t.taxes));

        QTableWidgetItem* fx_item = nullptr;
        if (t.fx_rate > 0) {
            ui::formatting::InstrumentFmt fx;
            fx.fx = true;
            fx_item = txn_num_item(ui::formatting::price(t.fx_rate, fx), t.fx_rate, txn_csv(t.fx_rate, 8));
        } else {
            fx_item = txn_num_item(QString(), portfolio::kAbsent, QString());
            fx_item->setToolTip(tr("No trade-date FX recorded — the daily FX history is used when available"));
        }
        table_->setItem(r, kTxFx, fx_item);

        const double net = txn_net_cash(t);
        auto* net_item = txn_num_item(std::isfinite(net) ? pfmt::smoney(net, ccy) : QString(), net, txn_csv(net, 4));
        net_item->setForeground(pfmt::dir_color(net));
        table_->setItem(r, kTxNet, net_item);

        auto* notes_item = new QTableWidgetItem(t.notes);
        notes_item->setForeground(QColor(ui::colors::TEXT_SECONDARY()));
        notes_item->setToolTip(t.notes);
        notes_item->setData(kTxExportRole, t.notes);
        table_->setItem(r, kTxNotes, notes_item);
    }
    table_->setSortingEnabled(true);

    const int total = static_cast<int>(txns_.size());
    const int shown = static_cast<int>(rows.size());
    count_label_->setText(shown == total ? tr("%n row(s)", "", total) : tr("%1 of %2 rows").arg(shown).arg(total));

    if (rows.isEmpty()) {
        table_->setSortingEnabled(false);
        table_->setRowCount(1);
        auto* msg = new QTableWidgetItem(total == 0 ? tr("No transactions recorded for this portfolio yet.")
                                                    : tr("No transactions match the filter."));
        msg->setForeground(QColor(ui::colors::TEXT_TERTIARY()));
        msg->setTextAlignment(Qt::AlignCenter);
        msg->setFlags(Qt::ItemIsEnabled);
        table_->setItem(0, 0, msg);
        table_->setSpan(0, 0, 1, table_->columnCount());
        return;
    }

    if (!sized_ || table_->columnWidth(kTxDate) < 20) { // first fill only: keep the user's widths afterwards
        table_->resizeColumnsToContents();
        sized_ = true;
    }

    if (!prev_id.isEmpty()) {
        for (int r = 0; r < table_->rowCount(); ++r) {
            const auto* it = table_->item(r, kTxDate);
            if (it && it->data(kTxIdRole).toString() == prev_id) {
                table_->selectRow(r);
                break;
            }
        }
    }
    if (table_->verticalScrollBar())
        table_->verticalScrollBar()->setValue(prev_scroll);
}

void PortfolioTxnPanel::export_visible() {
    QStringList headers = {"date", "type", "symbol", "quantity", "price", "currency", "gross", "fees", "taxes",
                           "fx_rate_to_" + base_ccy_, "net_cash", "notes", "transaction_id"};
    QVector<QStringList> rows;
    for (int r = 0; r < table_->rowCount(); ++r) {
        const auto* first = table_->item(r, kTxDate);
        if (!first || first->data(kTxIdRole).toString().isEmpty())
            continue;
        QStringList row;
        for (int c = 0; c < kTxColCount; ++c) {
            const auto* it = table_->item(r, c);
            row << (it ? it->data(kTxExportRole).toString() : QString());
        }
        row << first->data(kTxIdRole).toString();
        rows.append(row);
    }
    if (rows.isEmpty())
        return;
    ui::ChartExport::save_csv(headers, rows,
                              QStringLiteral("transactions_%1").arg(QDate::currentDate().toString(Qt::ISODate)), this);
}

void PortfolioTxnPanel::refresh_theme() {
    apply_styles();
    populate();
}

void PortfolioTxnPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void PortfolioTxnPanel::retranslateUi() {
    if (title_label_)
        title_label_->setText(tr("TRANSACTION HISTORY"));
    if (collapse_btn_)
        collapse_btn_->setToolTip(tr("Collapse / expand transaction history"));
    if (export_btn_)
        export_btn_->setText(tr("EXPORT"));
    if (search_edit_)
        search_edit_->setPlaceholderText(tr("Filter symbol / notes…"));
    if (type_cb_) {
        type_cb_->setItemText(0, tr("All types"));
        for (int i = 1; i < type_cb_->count(); ++i)
            type_cb_->setItemText(i, txn_type_label(type_cb_->itemData(i).toString()));
    }
    table_->setHorizontalHeaderLabels(txn_headers());
    populate();
}

} // namespace fincept::screens
