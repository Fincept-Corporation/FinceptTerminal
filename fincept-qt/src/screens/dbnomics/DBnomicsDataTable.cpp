// src/screens/dbnomics/DBnomicsDataTable.cpp
#include "screens/dbnomics/DBnomicsDataTable.h"
#include "ui/theme/Theme.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QLabel>
#include <QSet>
#include <algorithm>

namespace fincept::screens {

DBnomicsDataTable::DBnomicsDataTable(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void DBnomicsDataTable::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* label = new QLabel("OBSERVATION DATA");
    label->setObjectName("tableTitle");
    label->setStyleSheet(QString(
        "color: %1; font-size: 11px; font-weight: 700; "
        "font-family: 'Consolas','Courier New',monospace; "
        "padding: 6px 12px; background: %2; "
        "border-bottom: 1px solid %3;")
        .arg(ui::colors::AMBER)
        .arg(ui::colors::BG_RAISED)
        .arg(ui::colors::BORDER_DIM));
    root->addWidget(label);

    table_ = new QTableWidget(this);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setAlternatingRowColors(false);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setDefaultAlignment(Qt::AlignRight | Qt::AlignVCenter);
    table_->verticalHeader()->setVisible(false);
    table_->setShowGrid(false);
    table_->setStyleSheet(QString(
        "QTableWidget { background: %1; color: %2; "
        "  font-family: 'Consolas','Courier New',monospace; font-size: 12px; "
        "  border: none; gridline-color: %3; }"
        "QTableWidget::item { padding: 3px 8px; border-bottom: 1px solid %3; }"
        "QTableWidget::item:selected { background: %4; color: %2; }"
        "QHeaderView::section { background: %5; color: %6; "
        "  font-size: 11px; font-weight: 700; padding: 4px 8px; "
        "  border: none; border-bottom: 1px solid %3; }")
        .arg(ui::colors::BG_BASE)
        .arg(ui::colors::TEXT_PRIMARY)
        .arg(ui::colors::BORDER_DIM)
        .arg(ui::colors::BG_HOVER)
        .arg(ui::colors::BG_RAISED)
        .arg(ui::colors::TEXT_SECONDARY));
    root->addWidget(table_);
}

void DBnomicsDataTable::clear() {
    table_->clear();
    table_->setRowCount(1);
    table_->setColumnCount(1);
    table_->setHorizontalHeaderLabels({"STATUS"});
    auto* item = new QTableWidgetItem("No data — select a series");
    item->setForeground(QColor(ui::colors::TEXT_TERTIARY));
    item->setTextAlignment(Qt::AlignCenter);
    table_->setItem(0, 0, item);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
}

void DBnomicsDataTable::set_data(const QVector<services::DbnDataPoint>& series) {
    table_->clear();
    if (series.isEmpty()) {
        clear();
        return;
    }

    // Collect all unique periods across all series, sorted descending
    QSet<QString> period_set;
    for (const auto& dp : series) {
        for (const auto& obs : dp.observations) {
            period_set.insert(obs.period);
        }
    }
    QVector<QString> periods(period_set.begin(), period_set.end());
    std::sort(periods.begin(), periods.end(), std::greater<QString>());
    if (periods.size() > 50) periods.resize(50);

    // Columns: Period + one per series
    const int col_count = 1 + series.size();
    table_->setColumnCount(col_count);
    QStringList headers;
    headers << "PERIOD";
    for (const auto& dp : series) {
        QString short_name = dp.series_name;
        if (short_name.length() > 20) short_name = short_name.left(17) + "...";
        headers << short_name;
    }
    table_->setHorizontalHeaderLabels(headers);
    table_->setRowCount(periods.size());

    // Color-code series header cells
    for (int s = 0; s < series.size(); ++s) {
        if (auto* h = table_->horizontalHeaderItem(s + 1)) {
            h->setForeground(series[s].color);
        }
    }

    // Build fast lookup: series index → period → observation
    QVector<QMap<QString, services::DbnObservation>> lookups(series.size());
    for (int s = 0; s < series.size(); ++s) {
        for (const auto& obs : series[s].observations) {
            lookups[s][obs.period] = obs;
        }
    }

    // Fill rows
    for (int row = 0; row < periods.size(); ++row) {
        const QString& period = periods[row];

        auto* period_item = new QTableWidgetItem(period);
        period_item->setForeground(QColor(ui::colors::TEXT_SECONDARY));
        period_item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        table_->setItem(row, 0, period_item);

        for (int s = 0; s < series.size(); ++s) {
            QTableWidgetItem* cell;
            if (lookups[s].contains(period) && lookups[s][period].valid) {
                cell = new QTableWidgetItem(
                    QString::number(lookups[s][period].value, 'f', 4));
                cell->setForeground(QColor("#00E5FF")); // cyan for valid data
            } else {
                cell = new QTableWidgetItem("—");
                cell->setForeground(QColor(ui::colors::TEXT_TERTIARY));
            }
            cell->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            table_->setItem(row, s + 1, cell);
        }
    }
    table_->resizeColumnsToContents();
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
}

} // namespace fincept::screens
