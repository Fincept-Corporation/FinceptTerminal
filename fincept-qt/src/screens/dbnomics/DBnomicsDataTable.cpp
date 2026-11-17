// src/screens/dbnomics/DBnomicsDataTable.cpp
#include "screens/dbnomics/DBnomicsDataTable.h"

#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/LoadingSkeleton.h"

#include <QDate>
#include <QFontMetrics>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QSet>
#include <QStackedWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {
namespace {
/// Most-recent periods rendered in the observation table. The chart still
/// plots the whole series; only this preview grid is capped.
constexpr int kMaxRows = 50;

/// 28px rows at the default font size, growing with the user's font size.
int dbn_row_height() {
    return qMax(28, QFontMetrics(ui::fonts::font(-2)).height() + 12);
}
} // namespace

DBnomicsDataTable::DBnomicsDataTable(QWidget* parent) : QWidget(parent) {
    build_ui();
    // The row height follows the font; item brushes (series / direction colours) are redrawn by the
    // owning screen, which re-renders with the new theme's colours once the appearance has settled.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this,
            &DBnomicsDataTable::apply_appearance);
}

void DBnomicsDataTable::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Section header (always visible) ──────────────────────────────────────
    header_label_ = new QLabel(tr("OBSERVATION DATA"), this);
    header_label_->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; "
                                         "padding: 6px 12px; background: %2; "
                                         "border-bottom: 1px solid %3;")
                                     .arg(ui::colors::AMBER())
                                     .arg(ui::colors::BG_RAISED())
                                     .arg(ui::colors::BORDER_DIM()));
    // Unit / frequency facts can be long: clipped (tooltip), never widening the view.
    header_label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    root->addWidget(header_label_);

    stack_ = new QStackedWidget(this);

    // ── Page 0: loading — a table-shaped skeleton ─────────────────────────────
    skeleton_ = new ui::LoadingSkeleton(ui::LoadingSkeleton::Shape::Table, stack_);
    skeleton_->set_row_count(5);
    stack_->addWidget(skeleton_); // index 0

    // ── Page 1: table ─────────────────────────────────────────────────────────
    table_ = new QTableWidget(stack_);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setAlternatingRowColors(true);
    // Interactive columns sized once after each fill (never resize-to-contents while filling).
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setDefaultAlignment(Qt::AlignRight | Qt::AlignVCenter);
    table_->horizontalHeader()->setMinimumSectionSize(70);
    table_->verticalHeader()->setVisible(false);
    table_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table_->verticalHeader()->setDefaultSectionSize(dbn_row_height());
    table_->setShowGrid(true);
    table_->setWordWrap(false);
    table_->setStyleSheet(QString("QTableWidget { background: %1; alternate-background-color: %7;"
                                  "  color: %2; font-size: " + fincept::ui::fonts::px(-2) + "px;"
                                  "  border: none; gridline-color: %3; selection-background-color: %4; }"
                                  "QTableWidget::item { padding: 4px 10px; border: none; }"
                                  "QTableWidget::item:selected { background: %4; color: %2; }"
                                  "QHeaderView::section { background: %5; color: %6;"
                                  "  font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; padding: 6px 10px;"
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

    // ── Page 2: nothing to show ───────────────────────────────────────────────
    empty_ = new ui::EmptyState(stack_);
    empty_->set_compact(true);
    empty_->apply(ui::EmptyState::no_data_spec(tr("No observations yet"),
                                               tr("Pick a series on the left to see its latest values here.")));
    stack_->addWidget(empty_); // index 2

    stack_->setCurrentWidget(empty_);
    root->addWidget(stack_);
}

void DBnomicsDataTable::apply_appearance() {
    table_->verticalHeader()->setDefaultSectionSize(dbn_row_height());
}

// ── Live language switch ─────────────────────────────────────────────────────

void DBnomicsDataTable::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void DBnomicsDataTable::retranslateUi() {
    if (header_label_ && series_.isEmpty())
        header_label_->setText(tr("OBSERVATION DATA"));
    if (empty_)
        empty_->apply(ui::EmptyState::no_data_spec(tr("No observations yet"),
                                                   tr("Pick a series on the left to see its latest values here.")));
    // The table headers / cells are rebuilt by set_data().
    if (!series_.isEmpty())
        set_data(series_);
}

void DBnomicsDataTable::set_loading(bool on) {
    if (on) {
        skeleton_->set_message(tr("Loading observations…"));
        stack_->setCurrentWidget(skeleton_);
    } else {
        stack_->setCurrentWidget(series_.isEmpty() ? static_cast<QWidget*>(empty_) : table_);
    }
}

void DBnomicsDataTable::clear() {
    series_.clear();
    if (header_label_) {
        header_label_->setText(tr("OBSERVATION DATA"));
        header_label_->setToolTip(QString());
    }
    table_->clear();
    table_->setRowCount(0);
    table_->setColumnCount(0);
    stack_->setCurrentWidget(empty_);
}

void DBnomicsDataTable::set_data(const QVector<services::DbnDataPoint>& series) {
    if (series.isEmpty()) {
        clear();
        return;
    }
    series_ = series;

    // The newest kMaxRows periods across all series, newest first BY PERIOD START: period strings of
    // mixed frequency ("2024-12" vs "2024-Q4") do not sort lexically. Observations are ascending by
    // period start (the service sorts them), so those periods are among each series' last kMaxRows
    // observations: only that tail is collected, sorted and looked up — a long daily series no
    // longer goes through maps and a sort of every observation for a 50-row table.
    QHash<QString, QDate> period_start;
    for (const auto& dp : series) {
        const auto& obs = dp.observations;
        for (qsizetype i = std::max<qsizetype>(0, obs.size() - kMaxRows); i < obs.size(); ++i)
            if (!period_start.contains(obs[i].period))
                period_start.insert(obs[i].period, obs[i].start);
    }
    QVector<QString> periods = period_start.keys();
    std::sort(periods.begin(), periods.end(), [&period_start](const QString& a, const QString& b) {
        const QDate da = period_start.value(a);
        const QDate db = period_start.value(b);
        if (da.isValid() && db.isValid() && da != db)
            return da > db;
        return a > b;
    });
    if (periods.size() > kMaxRows)
        periods.resize(kMaxRows);
    const int shown_periods = static_cast<int>(periods.size());
    // Every distinct period of the series (the header's "of N"): one series lists each period once;
    // several are counted together.
    int total_periods = 0;
    if (series.size() == 1) {
        total_periods = static_cast<int>(series.first().observations.size());
    } else {
        qsizetype all = 0;
        for (const auto& dp : series)
            all += dp.observations.size();
        QSet<QString> distinct;
        distinct.reserve(all);
        for (const auto& dp : series)
            for (const auto& o : dp.observations)
                distinct.insert(o.period);
        total_periods = static_cast<int>(distinct.size());
    }
    total_periods = std::max(total_periods, shown_periods);
    // Be explicit that the table is truncated, and say what the numbers are (unit / frequency /
    // seasonal adjustment as DBnomics reports them for the series).
    if (header_label_) {
        QString text = total_periods > shown_periods
                           ? tr("OBSERVATION DATA — LATEST %1 OF %2 PERIODS").arg(shown_periods).arg(total_periods)
                           : tr("OBSERVATION DATA — %1 PERIODS").arg(total_periods);
        if (series.size() == 1) {
            const auto& dp = series.first();
            QStringList facts;
            for (const QString& f : {dp.unit, dp.frequency, dp.seasonal})
                if (!f.isEmpty())
                    facts << f;
            if (!facts.isEmpty())
                text += QStringLiteral("  ·  ") + facts.join(QStringLiteral(" · ")).toUpper();
        }
        header_label_->setText(text);
        header_label_->setToolTip(text);
    }

    // One pass with repaints off; columns sized once afterwards.
    table_->setUpdatesEnabled(false);
    table_->clear();

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

    // Color-code series header cells (the full name is the tooltip)
    for (int s = 0; s < series.size(); ++s) {
        if (auto* h = table_->horizontalHeaderItem(s + 1)) {
            h->setForeground(series[s].color);
            h->setToolTip(series[s].series_name);
        }
    }

    // Fast lookup over each series' tail (where every shown period of it lies): period → index of its
    // latest observation. The previous valid value (the period-over-period direction) is found by
    // stepping back from that index (observations are ascending by period start).
    QVector<QHash<QString, qsizetype>> lookups(series.size());
    for (int s = 0; s < series.size(); ++s) {
        const auto& obs = series[s].observations;
        for (qsizetype i = std::max<qsizetype>(0, obs.size() - kMaxRows); i < obs.size(); ++i)
            lookups[s].insert(obs[i].period, i);
    }
    const QColor up_color = QColor(ui::colors::POSITIVE());
    const QColor down_color = QColor(ui::colors::NEGATIVE());
    const QColor flat_color = QColor(ui::colors::TEXT_PRIMARY());

    // Fill rows
    for (int row = 0; row < periods.size(); ++row) {
        const QString& period = periods[row];

        auto* period_item = new QTableWidgetItem(period);
        period_item->setForeground(QColor(ui::colors::TEXT_SECONDARY()));
        period_item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        table_->setItem(row, 0, period_item);

        for (int s = 0; s < series.size(); ++s) {
            QTableWidgetItem* cell;
            const auto& series_obs = series[s].observations;
            const auto obs_it = lookups[s].constFind(period);
            if (obs_it != lookups[s].cend() && series_obs[obs_it.value()].valid) {
                const qsizetype at = obs_it.value();
                const double v = series_obs[at].value;
                // Fixed 4-decimals mangled both ends of the range: a levels
                // series (GDP) got 4 pointless zeros, and anything below 1e-4
                // (rates in decimal form, small indices) collapsed to "0.0000".
                // Whole numbers in full and grouped; fractions to ~9 significant
                // digits, trailing zeros trimmed — in the user's number locale.
                QString txt;
                const double a = std::abs(v);
                if (v == std::floor(v) && a < 1e15) {
                    txt = ui::formatting::number(v, 0);
                } else if (a > 0.0 && a < 1e-8) {
                    txt = QString::number(v, 'g', 6);
                } else {
                    const int int_digits = a >= 1.0 ? static_cast<int>(std::floor(std::log10(a))) + 1 : 0;
                    txt = ui::formatting::quantity(v, std::clamp(9 - int_digits, 0, 8));
                }
                // Direction vs the series' previous valid observation: ▲ / ▼ (never colour alone) in
                // the theme's gain / loss colours.
                qsizetype prev_at = at - 1;
                while (prev_at >= 0 && !series_obs[prev_at].valid)
                    --prev_at;
                const double prev = prev_at >= 0 ? series_obs[prev_at].value : v;
                const bool moved = prev_at >= 0 && v != prev;
                const bool up = moved && v > prev;
                if (moved)
                    txt += up ? QStringLiteral(" ▲") : QStringLiteral(" ▼");
                cell = new QTableWidgetItem(txt);
                if (moved) {
                    cell->setForeground(up ? up_color : down_color);
                    cell->setToolTip(tr("Change vs previous observation: %1")
                                         .arg(ui::formatting::signed_number(v - prev, a >= 100.0 ? 2 : 4)));
                } else {
                    cell->setForeground(flat_color);
                }
            } else {
                cell = new QTableWidgetItem(ui::formatting::placeholder());
                cell->setForeground(QColor(ui::colors::TEXT_TERTIARY()));
            }
            cell->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            table_->setItem(row, s + 1, cell);
        }
    }
    // Sized once, after the fill; the columns stay Interactive (no resize-to-contents mode left on
    // for the next fill).
    table_->resizeColumnsToContents();
    table_->setUpdatesEnabled(true);
    stack_->setCurrentWidget(table_);
}

} // namespace fincept::screens
