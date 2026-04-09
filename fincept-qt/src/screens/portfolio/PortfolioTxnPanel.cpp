// src/screens/portfolio/PortfolioTxnPanel.cpp
#include "screens/portfolio/PortfolioTxnPanel.h"

#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace fincept::screens {

PortfolioTxnPanel::PortfolioTxnPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void PortfolioTxnPanel::build_ui() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header bar
    auto* header = new QWidget(this);
    header->setFixedHeight(28);
    header->setStyleSheet(QString("background:%1; border-top:1px solid %2; border-bottom:1px solid %2;")
                              .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(10, 0, 10, 0);
    hl->setSpacing(8);

    auto* title = new QLabel("TRANSACTION HISTORY");
    title->setStyleSheet(
        QString("color:%1; font-size:10px; font-weight:700; letter-spacing:1px;").arg(ui::colors::TEXT_SECONDARY));
    hl->addWidget(title);

    hl->addStretch();

    count_label_ = new QLabel;
    count_label_->setStyleSheet(QString("color:%1; font-size:9px; font-weight:600;").arg(ui::colors::TEXT_TERTIARY));
    hl->addWidget(count_label_);

    layout->addWidget(header);

    // Table
    static const QStringList kHeaders = {"Date", "Symbol", "Type", "Qty", "Price", "Total", "Notes"};
    table_ = new QTableWidget(0, kHeaders.size(), this);
    table_->setHorizontalHeaderLabels(kHeaders);
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setShowGrid(false);
    table_->setAlternatingRowColors(false);
    table_->setSortingEnabled(true);

    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch); // Notes stretches

    table_->setStyleSheet(QString("QTableWidget {"
                                  "  background:%1; color:%2; border:none;"
                                  "  font-size:11px; font-family:%3; gridline-color:%4;"
                                  "}"
                                  "QTableWidget::item { padding:4px 8px; border-bottom:1px solid %4; }"
                                  "QTableWidget::item:selected { background:%5; color:%2; }"
                                  "QHeaderView::section {"
                                  "  background:%6; color:%7; border:none;"
                                  "  border-bottom:2px solid %4; border-right:1px solid %4;"
                                  "  font-size:10px; font-weight:700;"
                                  "  letter-spacing:0.5px; padding:4px 8px; }"
                                  "QScrollBar:vertical { background:%1; width:5px; }"
                                  "QScrollBar::handle:vertical { background:%4; min-height:20px; }"
                                  "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0px; }")
                              .arg(ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY, ui::fonts::DATA_FAMILY,
                                   ui::colors::BORDER_DIM, ui::colors::BG_HOVER, ui::colors::BG_SURFACE,
                                   ui::colors::TEXT_TERTIARY));

    table_->verticalHeader()->setDefaultSectionSize(24);

    layout->addWidget(table_, 1);
}

void PortfolioTxnPanel::set_transactions(const QVector<portfolio::Transaction>& txns) {
    txns_ = txns;
    populate();
}

void PortfolioTxnPanel::clear() {
    txns_.clear();
    table_->setRowCount(0);
    count_label_->clear();
}

void PortfolioTxnPanel::populate() {
    table_->setSortingEnabled(false);
    table_->setRowCount(0);

    for (const auto& t : txns_) {
        const int row = table_->rowCount();
        table_->insertRow(row);

        // Date (ISO → display)
        auto* date_item = new QTableWidgetItem(t.transaction_date.left(10));
        date_item->setForeground(QColor(ui::colors::TEXT_SECONDARY()));
        table_->setItem(row, 0, date_item);

        // Symbol
        auto* sym_item = new QTableWidgetItem(t.symbol);
        sym_item->setForeground(QColor(ui::colors::TEXT_PRIMARY()));
        sym_item->setFont(QFont(ui::fonts::DATA_FAMILY, 10, QFont::Bold));
        table_->setItem(row, 1, sym_item);

        // Type — colour-coded
        auto* type_item = new QTableWidgetItem(t.transaction_type);
        QColor type_color;
        if (t.transaction_type == "BUY")
            type_color = QColor(ui::colors::POSITIVE());
        else if (t.transaction_type == "SELL")
            type_color = QColor(ui::colors::NEGATIVE());
        else if (t.transaction_type == "DIVIDEND")
            type_color = QColor(ui::colors::CYAN());
        else
            type_color = QColor(ui::colors::TEXT_SECONDARY());
        type_item->setForeground(type_color);
        type_item->setFont(QFont(ui::fonts::DATA_FAMILY, 9, QFont::Bold));
        table_->setItem(row, 2, type_item);

        // Qty
        auto* qty_item = new QTableWidgetItem(QString::number(t.quantity, 'f', 4));
        qty_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        qty_item->setForeground(QColor(ui::colors::TEXT_PRIMARY()));
        table_->setItem(row, 3, qty_item);

        // Price
        auto* price_item = new QTableWidgetItem(QString::number(t.price, 'f', 2));
        price_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        price_item->setForeground(QColor(ui::colors::TEXT_PRIMARY()));
        table_->setItem(row, 4, price_item);

        // Total
        auto* total_item = new QTableWidgetItem(QString::number(t.total_value, 'f', 2));
        total_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        const char* total_color = (t.transaction_type == "BUY")    ? ui::colors::NEGATIVE
                                  : (t.transaction_type == "SELL") ? ui::colors::POSITIVE
                                                                   : ui::colors::CYAN;
        total_item->setForeground(QColor(total_color));
        table_->setItem(row, 5, total_item);

        // Notes
        auto* notes_item = new QTableWidgetItem(t.notes);
        notes_item->setForeground(QColor(ui::colors::TEXT_TERTIARY()));
        table_->setItem(row, 6, notes_item);
    }

    table_->setSortingEnabled(true);
    count_label_->setText(QString("%1 transactions").arg(txns_.size()));
}

void PortfolioTxnPanel::refresh_theme() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));

    const QString bsz = QString::number(ui::fonts::font_px(0));
    const QString hsz = QString::number(ui::fonts::font_px(-2));
    table_->setStyleSheet(QString("QTableWidget {"
                                  "  background:%1; color:%2; border:none;"
                                  "  font-size:" +
                                  bsz +
                                  "px; font-family:%3; gridline-color:%4;"
                                  "}"
                                  "QTableWidget::item { padding:4px 8px; border-bottom:1px solid %4; }"
                                  "QTableWidget::item:selected { background:%5; color:%2; }"
                                  "QHeaderView::section {"
                                  "  background:%6; color:%7; border:none;"
                                  "  border-bottom:2px solid %4; border-right:1px solid %4;"
                                  "  font-size:" +
                                  hsz +
                                  "px; font-weight:700;"
                                  "  letter-spacing:0.5px; padding:4px 8px; }"
                                  "QScrollBar:vertical { background:%1; width:5px; }"
                                  "QScrollBar::handle:vertical { background:%4; min-height:20px; }"
                                  "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0px; }")
                              .arg(ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY, ui::fonts::DATA_FAMILY,
                                   ui::colors::BORDER_DIM, ui::colors::BG_HOVER, ui::colors::BG_SURFACE,
                                   ui::colors::TEXT_TERTIARY));

    populate();
}

} // namespace fincept::screens
