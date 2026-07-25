// src/screens/dbnomics/DBnomicsDataTable.cpp
#include "screens/dbnomics/DBnomicsDataTable.h"

#include "ui/theme/Theme.h"

#include <QHeaderView>
#include <QLabel>
#include <QSet>
#include <QStackedWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {
namespace {
/// Most-recent periods rendered in the observation table. The chart still
/// plots the whole series; only this preview grid is capped.
constexpr int kMaxRows = 50;
} // namespace

DBnomicsDataTable::DBnomicsDataTable(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void DBnomicsDataTable::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Section header (always visible) ──────────────────────────────────────
    header_label_ = new QLabel(tr("OBSERVATION DATA"), this);
    header_label_->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: 700; "
                                         "font-family: 'Consolas','Courier New',monospace; "
                                         "padding: 6px 12px; background: %2; "
                                         "border-bottom: 1px solid %3;")
                                     .arg(ui::colors::AMBER())
                                     .arg(ui::colors::BG_RAISED())
                                     .arg(ui::colors::BORDER_DIM()));
    root->addWidget(header_label_);

    stack_ = new QStackedWidget(this);

    // ── Page 0: loading spinner ───────────────────────────────────────────────
    auto* loading_page = new QWidget(stack_);
    loading_page->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    auto* loading_vl = new QVBoxLayout(loading_page);
    spin_label_ = new QLabel(loading_page);
    spin_label_->setAlignment(Qt::AlignCenter);
    spin_label_->setStyleSheet(QString("color:%1; font-family:%2; font-size:13px; background:transparent;")
                                   .arg(ui::colors::TEXT_TERTIARY())
                                   .arg(ui::fonts::DATA_FAMILY));
    loading_vl->addWidget(spin_label_);
    stack_->addWidget(loading_page); // index 0

    // ── Page 1: table ─────────────────────────────────────────────────────────
    table_ = new QTableWidget(stack_);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setAlternatingRowColors(true);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setDefaultAlignment(Qt::AlignRight | Qt::AlignVCenter);
    table_->horizontalHeader()->setMinimumSectionSize(70);
    table_->verticalHeader()->setVisible(false);
    table_->verticalHeader()->setDefaultSectionSize(28);
    table_->setShowGrid(true);
    table_->setWordWrap(false);
    table_->setStyleSheet(QString("QTableWidget { background: %1; alternate-background-color: %7;"
                                  "  color: %2; font-family: 'Consolas','Courier New',monospace; font-size: 12px;"
                                  "  border: none; gridline-color: %3; selection-background-color: %4; }"
                                  "QTableWidget::item { padding: 4px 10px; border: none; }"
                                  "QTableWidget::item:selected { background: %4; color: %2; }"
                                  "QHeaderView::section { background: %5; color: %6;"
                                  "  font-family: 'Consolas','Courier New',monospace;"
                                  "  font-size: 11px; font-weight: 700; padding: 6px 10px;"
                                  "  border: none; border-bottom: 2px solid %3; border-right: 1px solid %3; }"
                                  "QScrollBar:vertical { background: %1; width: 8px; border: none; }"
                                  "QScrollBar::handle:vertical { background: %3; border-radius: 4px; }"
                                  "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
                                  "QScrollBar:horizontal { background: %1; height: 8px; border: none; }"
                                  "QScrollBar::handle:horizontal { background: %3; border-radius: 4px; }"
                                  "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }")
                              .arg(ui::colors::BG_BASE())        // %1
                              .arg(ui::colors::TEXT_PRIMARY())   // %2
                              .arg(ui::colors::BORDER_DIM())     // %3
                              .arg(ui::colors::BG_HOVER())       // %4
                              .arg(ui::colors::BG_RAISED())      // %5
                              .arg(ui::colors::TEXT_SECONDARY()) // %6
                              .arg(ui::colors::BG_SURFACE()));   // %7 alternate row
    stack_->addWidget(table_);                                   // index 1

    stack_->setCurrentIndex(1);
    root->addWidget(stack_);

    // ── Animation timer ───────────────────────────────────────────────────────
    spin_timer_ = new QTimer(this);
    spin_timer_->setInterval(120);
    connect(spin_timer_, &QTimer::timeout, this, [this]() {
        static const QString frames[] = {"⣾", "⣽", "⣻", "⢿", "⡿", "⣟", "⣯", "⣷"};
        spin_label_->setText(tr("%1  LOADING OBSERVATIONS...").arg(frames[frame_ % 8]));
        ++frame_;
    });
}

// ── Live language switch ─────────────────────────────────────────────────────

void DBnomicsDataTable::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void DBnomicsDataTable::retranslateUi() {
    if (header_label_)
        header_label_->setText(tr("OBSERVATION DATA"));
    // The table headers / cells are rebuilt by set_data(); only the empty-state
    // placeholder needs an explicit re-apply here.
    if (showing_placeholder_)
        clear();
}

void DBnomicsDataTable::set_loading(bool on) {
    if (on) {
        frame_ = 0;
        spin_label_->setText(tr("%1  LOADING OBSERVATIONS...").arg(QStringLiteral("⣾")));
        stack_->setCurrentIndex(0);
        spin_timer_->start();
    } else {
        spin_timer_->stop();
        stack_->setCurrentIndex(1);
    }
}

void DBnomicsDataTable::clear() {
    if (header_label_)
        header_label_->setText(tr("OBSERVATION DATA"));
    table_->clear();
    table_->setRowCount(1);
    table_->setColumnCount(1);
    table_->setHorizontalHeaderLabels({tr("STATUS")});
    auto* item = new QTableWidgetItem(tr("No data — select a series"));
    item->setForeground(QColor(ui::colors::TEXT_TERTIARY()));
    item->setTextAlignment(Qt::AlignCenter);
    table_->setItem(0, 0, item);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    showing_placeholder_ = true;
}

void DBnomicsDataTable::set_data(const QVector<services::DbnDataPoint>& series) {
    table_->clear();
    if (series.isEmpty()) {
        clear();
        return;
    }
    showing_placeholder_ = false;

    // Collect all unique periods across all series, sorted descending
    QSet<QString> period_set;
    for (const auto& dp : series) {
        for (const auto& obs : dp.observations) {
            period_set.insert(obs.period);
        }
    }
    QVector<QString> periods(period_set.begin(), period_set.end());
    std::sort(periods.begin(), periods.end(), std::greater<QString>());
    const int total_periods = static_cast<int>(periods.size());
    if (total_periods > kMaxRows)
        periods.resize(kMaxRows);
    const int shown_periods = static_cast<int>(periods.size());
    // Be explicit that the table is truncated — it silently showed only the
    // 50 most recent periods with no indication the series went further back.
    if (header_label_) {
        header_label_->setText(total_periods > shown_periods
                                   ? tr("OBSERVATION DATA — LATEST %1 OF %2 PERIODS")
                                         .arg(shown_periods)
                                         .arg(total_periods)
                                   : tr("OBSERVATION DATA — %1 PERIODS").arg(total_periods));
    }

    // Columns: Period + one per series
    const int col_count = 1 + series.size();
    table_->setColumnCount(col_count);
    QStringList headers;
    headers << tr("PERIOD");
    for (const auto& dp : series) {
        QString short_name = dp.series_name;
        if (short_name.length() > 20)
            short_name = short_name.left(17) + "...";
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
        period_item->setForeground(QColor(ui::colors::TEXT_SECONDARY()));
        period_item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        table_->setItem(row, 0, period_item);

        for (int s = 0; s < series.size(); ++s) {
            QTableWidgetItem* cell;
            if (lookups[s].contains(period) && lookups[s][period].valid) {
                const double v = lookups[s][period].value;
                // Fixed 4-decimals mangled both ends of the range: a levels
                // series (GDP) got 4 pointless zeros, and anything below 1e-4
                // (rates in decimal form, small indices) collapsed to "0.0000".
                QString txt;
                if (v == std::floor(v) && std::abs(v) < 1e15)
                    txt = QString::number(static_cast<qint64>(v));
                else if (std::abs(v) >= 1e-4)
                    txt = QString::number(v, 'f', 4);
                else
                    txt = QString::number(v, 'g', 6);
                cell = new QTableWidgetItem(txt);
                // Theme token, not a hard-coded hex — the old #00E5FF was
                // invisible against a light theme background.
                cell->setForeground(QColor(ui::colors::CYAN()));
            } else {
                cell = new QTableWidgetItem("—");
                cell->setForeground(QColor(ui::colors::TEXT_TERTIARY()));
            }
            cell->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            table_->setItem(row, s + 1, cell);
        }
    }
    table_->resizeColumnsToContents();
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
}

} // namespace fincept::screens
