// src/screens/portfolio/PortfolioBlotter.cpp
#include "screens/portfolio/PortfolioBlotter.h"

#include "screens/portfolio/PortfolioSparkline.h"
#include "ui/theme/Theme.h"

#include <QHeaderView>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

static const QStringList kColumns = {"SYMBOL", "QTY",  "LAST", "AVG COST", "MKT VAL", "COST BASIS",
                                     "P&L",    "P&L%", "CHG%", "TREND",    "WT%"};

PortfolioBlotter::PortfolioBlotter(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void PortfolioBlotter::build_ui() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    table_ = new QTableWidget(this);
    table_->setColumnCount(kColumns.size());
    table_->setHorizontalHeaderLabels(kColumns);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setShowGrid(false);
    table_->setAlternatingRowColors(false);
    table_->verticalHeader()->setVisible(false);
    table_->setFocusPolicy(Qt::NoFocus);

    // Column widths
    auto* hdr = table_->horizontalHeader();
    hdr->setMinimumSectionSize(50);
    hdr->setSectionResizeMode(QHeaderView::Interactive);
    hdr->setStretchLastSection(true);
    hdr->setSortIndicatorShown(false);

    // Default widths
    int col_widths[] = {72, 55, 68, 72, 80, 80, 78, 60, 58, 68, 52};
    for (int i = 0; i < kColumns.size() && i < 11; ++i)
        table_->setColumnWidth(i, col_widths[i]);

    table_->setStyleSheet(QString("QTableWidget { background:%1; color:%2; border:none;"
                                  "  font-size:11px; font-family:%3; gridline-color:transparent; }"
                                  "QTableWidget::item { padding:3px 6px; border-bottom:1px solid %4; }"
                                  "QTableWidget::item:selected { background:%5; color:%6; }"
                                  "QTableWidget::item:hover { background:%7; }"
                                  "QHeaderView::section { background:%8; color:%9; border:none;"
                                  "  border-bottom:2px solid %10; padding:4px 6px;"
                                  "  font-size:9px; font-weight:700; letter-spacing:0.5px; }")
                              .arg(ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY, ui::fonts::DATA_FAMILY,
                                   ui::colors::BORDER_DIM, ui::colors::AMBER_DIM, ui::colors::AMBER,
                                   ui::colors::BG_HOVER, ui::colors::BG_SURFACE, ui::colors::TEXT_SECONDARY)
                              .arg(ui::colors::AMBER));

    connect(hdr, &QHeaderView::sectionClicked, this, &PortfolioBlotter::on_header_clicked);
    connect(table_, &QTableWidget::cellClicked, this, &PortfolioBlotter::on_row_clicked);

    layout->addWidget(table_);
}

void PortfolioBlotter::set_holdings(const QVector<portfolio::HoldingWithQuote>& holdings) {
    holdings_ = holdings;
    populate_table();
}

void PortfolioBlotter::set_selected_symbol(const QString& symbol) {
    selected_symbol_ = symbol;
    for (int r = 0; r < table_->rowCount(); ++r) {
        auto* item = table_->item(r, 0);
        if (item && item->text() == symbol) {
            table_->selectRow(r);
            return;
        }
    }
}

void PortfolioBlotter::on_header_clicked(int section) {
    // Map column index to SortColumn
    portfolio::SortColumn col;
    switch (section) {
        case 0:
            col = portfolio::SortColumn::Symbol;
            break;
        case 2:
            col = portfolio::SortColumn::Price;
            break;
        case 4:
            col = portfolio::SortColumn::MarketValue;
            break;
        case 6:
            col = portfolio::SortColumn::Pnl;
            break;
        case 7:
            col = portfolio::SortColumn::PnlPct;
            break;
        case 8:
            col = portfolio::SortColumn::Change;
            break;
        case 10:
            col = portfolio::SortColumn::Weight;
            break;
        default:
            return;
    }

    if (col == sort_col_) {
        sort_dir_ = (sort_dir_ == portfolio::SortDirection::Asc) ? portfolio::SortDirection::Desc
                                                                 : portfolio::SortDirection::Asc;
    } else {
        sort_col_ = col;
        sort_dir_ = portfolio::SortDirection::Desc;
    }

    emit sort_changed(sort_col_, sort_dir_);
    populate_table();
}

void PortfolioBlotter::on_row_clicked(int row, int) {
    if (row >= 0 && row < sorted_.size()) {
        selected_symbol_ = sorted_[row].symbol;
        emit symbol_selected(selected_symbol_);
    }
}

void PortfolioBlotter::populate_table() {
    // Clean up old sparkline cell widgets before repopulating (prevents memory leak)
    for (int r = 0; r < table_->rowCount(); ++r) {
        auto* w = table_->cellWidget(r, 9);
        if (w) {
            table_->removeCellWidget(r, 9);
            w->deleteLater();
        }
    }

    sorted_ = holdings_;

    // Sort
    bool asc = (sort_dir_ == portfolio::SortDirection::Asc);
    auto cmp = [&](const portfolio::HoldingWithQuote& a, const portfolio::HoldingWithQuote& b) {
        double va = 0, vb = 0;
        switch (sort_col_) {
            case portfolio::SortColumn::Symbol:
                return asc ? a.symbol < b.symbol : a.symbol > b.symbol;
            case portfolio::SortColumn::Price:
                va = a.current_price;
                vb = b.current_price;
                break;
            case portfolio::SortColumn::Change:
                va = a.day_change_percent;
                vb = b.day_change_percent;
                break;
            case portfolio::SortColumn::Pnl:
                va = a.unrealized_pnl;
                vb = b.unrealized_pnl;
                break;
            case portfolio::SortColumn::PnlPct:
                va = a.unrealized_pnl_percent;
                vb = b.unrealized_pnl_percent;
                break;
            case portfolio::SortColumn::Weight:
                va = a.weight;
                vb = b.weight;
                break;
            case portfolio::SortColumn::MarketValue:
                va = a.market_value;
                vb = b.market_value;
                break;
        }
        return asc ? va < vb : va > vb;
    };
    std::stable_sort(sorted_.begin(), sorted_.end(), cmp);

    table_->setRowCount(sorted_.size());

    for (int r = 0; r < sorted_.size(); ++r) {
        const auto& h = sorted_[r];
        table_->setRowHeight(r, 28);

        auto set_cell = [&](int col, const QString& text, const char* color = nullptr,
                            Qt::Alignment align = Qt::AlignRight | Qt::AlignVCenter) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(align);
            if (color)
                item->setForeground(QColor(color));
            table_->setItem(r, col, item);
        };

        // SYMBOL
        set_cell(0, h.symbol, ui::colors::CYAN, Qt::AlignLeft | Qt::AlignVCenter);

        // QTY
        set_cell(1, format_value(h.quantity, h.quantity == std::floor(h.quantity) ? 0 : 2));

        // LAST (price)
        set_cell(2, format_value(h.current_price));

        // AVG COST
        set_cell(3, format_value(h.avg_buy_price));

        // MKT VAL
        set_cell(4, format_value(h.market_value), ui::colors::WARNING);

        // COST BASIS
        set_cell(5, format_value(h.cost_basis));

        // P&L
        const char* pnl_color = h.unrealized_pnl >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE;
        set_cell(6, QString("%1%2").arg(h.unrealized_pnl >= 0 ? "+" : "").arg(format_value(h.unrealized_pnl)),
                 pnl_color);

        // P&L%
        set_cell(
            7,
            QString("%1%2%").arg(h.unrealized_pnl_percent >= 0 ? "+" : "").arg(format_value(h.unrealized_pnl_percent)),
            pnl_color);

        // CHG%
        const char* chg_color = h.day_change_percent >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE;
        set_cell(8, QString("%1%2%").arg(h.day_change_percent >= 0 ? "+" : "").arg(format_value(h.day_change_percent)),
                 chg_color);

        // TREND (sparkline widget)
        auto* sparkline = new PortfolioSparkline(65, 20);
        // Use current price as single point — real historical data comes later
        QVector<double> trend_data;
        double base = h.current_price - h.day_change;
        for (int i = 0; i < 10; ++i)
            trend_data.append(base + (h.day_change * i / 9.0));
        sparkline->set_data(trend_data);
        sparkline->set_color(QColor(h.day_change_percent >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE));
        table_->setCellWidget(r, 9, sparkline);

        // WT%
        set_cell(10, QString("%1%").arg(format_value(h.weight, 1)));

        // Highlight selected row
        if (h.symbol == selected_symbol_) {
            table_->selectRow(r);
        }
    }
}

QString PortfolioBlotter::format_value(double v, int dp) const {
    return QString::number(v, 'f', dp);
}

} // namespace fincept::screens
