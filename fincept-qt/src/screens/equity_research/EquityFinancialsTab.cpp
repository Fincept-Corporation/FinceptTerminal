// src/screens/equity_research/EquityFinancialsTab.cpp
//
// Core: static helpers, ctor, set_symbol, get_val, populate_table,
// fmt_large/fmt_pct/fmt_ratio. Other concerns:
//   - EquityFinancialsTab_Views.cpp     — build_ui + build_*_view
//   - EquityFinancialsTab_Populate.cpp  — on_financials_loaded + populate/rebuild
#include "screens/equity_research/EquityFinancialsTab.h"
#include "screens/equity_research/EquityFinancialsTab_internal.h"

#include "services/equity/EquityResearchService.h"
#include "services/file_manager/FileManagerService.h"
#include "ui/theme/Theme.h"

#include <QBarCategoryAxis>
#include <QBarSeries>
#include <QBarSet>
#include <QChart>
#include <QDateTime>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLineSeries>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QTextStream>
#include <QVBoxLayout>
#include <QValueAxis>

#include <cmath>

namespace fincept::screens {

using namespace financials_internal;

// ── Constructor ───────────────────────────────────────────────────────────────

EquityFinancialsTab::EquityFinancialsTab(QWidget* parent) : QWidget(parent) {
    build_ui();
    auto& svc = services::equity::EquityResearchService::instance();
    connect(&svc, &services::equity::EquityResearchService::financials_loaded, this,
            &EquityFinancialsTab::on_financials_loaded);
    // Dismiss the "LOADING FINANCIALS…" overlay on failure — it's hidden only on success.
    connect(&svc, &services::equity::EquityResearchService::error_occurred, this,
            [this](const QString& ctx, const QString&) {
                if (ctx != "Financials")
                    return;
                if (loading_overlay_)
                    loading_overlay_->hide_loading();
                // A cached copy is on screen and its refresh failed: keep it, say so.
                if (loaded_ && cached_data_.stale && period_note_)
                    period_note_->setText(tr("ANNUAL STATEMENTS · CACHED COPY — REFRESH FAILED"));
                // Hiding the overlay alone left the PREVIOUS symbol's statements on screen
                // under the new symbol's header. Nothing has loaded for this symbol, so
                // return every card/table/chart to its blank state.
                if (!loaded_)
                    rebuild_views();
            });
}

void EquityFinancialsTab::set_symbol(const QString& symbol) {
    if (symbol == current_symbol_)
        return;
    current_symbol_ = symbol;
    loaded_ = false;
    loading_overlay_->show_loading(tr("LOADING FINANCIALS…"));
    services::equity::EquityResearchService::instance().fetch_financials(symbol);
}

// ── Re-translation ───────────────────────────────────────────────────────────
//
// Strategy: rebuild the three stacked view pages on language change. Every
// metric card, ratio row, section title, and chart axis label is created
// inside build_*_view() with localized strings, so a fresh build_*_view()
// call after the language change produces a fully-translated subtree. The
// statement selector buttons + EXPORT CSV button sit above the stack and
// are retranslated explicitly via cached members.
//
// Member pointers (inc_revenue_val_ et al.) get re-pointed by the new
// build_*_view() calls; on_financials_loaded(cached_data_) then re-renders
// every value into them, so no live data is lost.

void EquityFinancialsTab::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void EquityFinancialsTab::retranslateUi() {
    if (btn_income_)
        btn_income_->setText(tr("Income Statement"));
    if (btn_balance_)
        btn_balance_->setText(tr("Balance Sheet"));
    if (btn_cashflow_)
        btn_cashflow_->setText(tr("Cash Flow"));
    if (btn_export_csv_)
        btn_export_csv_->setText(tr("EXPORT CSV"));

    rebuild_views();
}

void EquityFinancialsTab::rebuild_views() {
    if (!stack_)
        return;

    const int saved_idx = stack_->currentIndex();

    // Remove + delete existing pages. Member pointers held by the pages
    // (inc_revenue_val_, bal_table_, cf_chart_, ...) all reference children
    // of those pages, so they're invalidated by the deleteLater chain.
    while (stack_->count() > 0) {
        auto* w = stack_->widget(0);
        stack_->removeWidget(w);
        w->deleteLater();
    }

    // Rebuild — each build_*_view() re-assigns the relevant member pointers.
    stack_->addWidget(build_income_view());
    stack_->addWidget(build_balance_view());
    stack_->addWidget(build_cashflow_view());
    stack_->setCurrentIndex(saved_idx);

    // Replay cached data so the freshly-built value labels and tables show
    // the right numbers in the new locale. on_financials_loaded is the only
    // entry point that knows how to populate all three views from a single
    // FinancialsData payload.
    if (loaded_)
        on_financials_loaded(cached_data_);
}

// ── Build UI ──────────────────────────────────────────────────────────────────

void EquityFinancialsTab::populate_table(QTableWidget* table, const QVector<QPair<QString, QJsonObject>>& stmt) {

    if (!table || stmt.isEmpty())
        return;
    table->clearContents();

    QStringList periods;
    QSet<QString> keys_set;
    for (const auto& p : stmt) {
        periods << p.first.left(10);
        for (auto it = p.second.constBegin(); it != p.second.constEnd(); ++it)
            keys_set.insert(it.key());
    }
    QStringList metrics = keys_set.values();
    metrics.sort();

    table->setColumnCount(periods.size() + 1);
    table->setRowCount(metrics.size());

    QStringList headers;
    headers << tr("Metric");
    headers.append(periods);
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

    for (int r = 0; r < metrics.size(); ++r) {
        auto* mk = new QTableWidgetItem(metrics[r]);
        mk->setForeground(QColor(ui::colors::TEXT_SECONDARY()));
        table->setItem(r, 0, mk);

        for (int c = 0; c < stmt.size(); ++c) {
            double val = stmt[c].second[metrics[r]].toVariant().toDouble();
            QString text = val == 0.0 ? "—" : fmt_large(val);
            auto* cell = new QTableWidgetItem(text);
            cell->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            if (val != 0.0)
                cell->setData(Qt::UserRole, val); // raw figure, used by EXPORT CSV (the text is "391.04B")
            if (val < 0)
                cell->setForeground(QColor(kRed));
            table->setItem(r, c + 1, cell);
        }
    }
    table->resizeColumnsToContents();
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
}

// ── Formatters ────────────────────────────────────────────────────────────────

QString EquityFinancialsTab::fmt_large(double v) {
    bool neg = v < 0;
    double av = qAbs(v);
    QString s;
    if (av >= 1e12)
        s = QString("%1T").arg(av / 1e12, 0, 'f', 2);
    else if (av >= 1e9)
        s = QString("%1B").arg(av / 1e9, 0, 'f', 2);
    else if (av >= 1e6)
        s = QString("%1M").arg(av / 1e6, 0, 'f', 1);
    else if (av >= 1e3)
        s = QString("%1K").arg(av / 1e3, 0, 'f', 1);
    else
        // Per-share and ratio lines (Basic EPS 6.08, Tax Rate For Calcs 0.16) used to be cast
        // to an integer here and printed as "6" / "0".
        s = QString::number(av, 'f', av == std::floor(av) ? 0 : (av < 1.0 ? 4 : 2));
    return neg ? "-" + s : s;
}

QString EquityFinancialsTab::fmt_pct(double v) {
    return QString("%1%").arg(v * 100.0, 0, 'f', 2);
}

QString EquityFinancialsTab::fmt_ratio(double v) {
    return QString::number(v, 'f', 2);
}

} // namespace fincept::screens
