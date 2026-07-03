// src/screens/equity_research/EquityFinancialsTab.cpp
//
// Core: static helpers, ctor, set_symbol, get_val, populate_table,
// fmt_large/fmt_pct/fmt_ratio. Other concerns:
//   - EquityFinancialsTab_Views.cpp     — build_ui + build_*_view
//   - EquityFinancialsTab_Populate.cpp  — on_financials_loaded + populate/rebuild
#include "screens/equity_research/EquityFinancialsTab.h"

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

namespace fincept::screens {


// ── Static helpers (anon ns + statics from original file) ──
namespace {

static const QString kAmber = "#f59e0b";
static const QString kCyan = "#22d3ee";
static const QString kGreen = ui::colors::POSITIVE;
static const QString kRed = ui::colors::NEGATIVE;
static const QString kBlue = "#3b82f6";
static const QString kPurple = "#a855f7";
static const QString kOrange = "#f97316";
static const QString kYellow = "#eab308";

[[maybe_unused]] QFrame* section_frame(const QString& title, const QString& color) {
    auto* f = new QFrame;
    f->setStyleSheet(QString("QFrame { background:%1; border:1px solid %2; border-radius:4px; }")
                         .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* vl = new QVBoxLayout(f);
    vl->setContentsMargins(10, 8, 10, 10);
    vl->setSpacing(8);

    auto* hdr = new QHBoxLayout;
    hdr->setSpacing(6);
    auto* title_lbl = new QLabel(title);
    title_lbl->setStyleSheet(QString("color:%1; font-size:11px; font-weight:700; "
                                     "letter-spacing:1px; background:transparent; border:0;")
                                 .arg(color));
    hdr->addWidget(title_lbl);
    hdr->addStretch();
    vl->addLayout(hdr);

    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(
        QString("border:0; border-top:1px solid %1; background:transparent;").arg(ui::colors::BORDER_DIM()));
    vl->addWidget(sep);
    return f;
}

// Large metric card: label on top, big value, optional subtitle
[[maybe_unused]] QWidget* metric_card(const QString& label, QLabel*& val_out, QLabel*& sub_out, const QString& val_color,
                     const QString& initial_val = "—", const QString& initial_sub = {}) {
    auto* f = new QFrame;
    f->setStyleSheet(QString("QFrame { background:%1; border:1px solid %2; border-radius:4px; }")
                         .arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM()));
    auto* vl = new QVBoxLayout(f);
    vl->setContentsMargins(8, 6, 8, 6);
    vl->setSpacing(2);

    auto* lbl = new QLabel(label);
    lbl->setStyleSheet(QString("color:%1; font-size:9px; font-weight:600; letter-spacing:1px; "
                               "background:transparent; border:0;")
                           .arg(ui::colors::TEXT_SECONDARY()));
    vl->addWidget(lbl);

    val_out = new QLabel(initial_val);
    val_out->setStyleSheet(QString("color:%1; font-size:14px; font-weight:700; "
                                   "background:transparent; border:0;")
                               .arg(val_color));
    vl->addWidget(val_out);

    if (!initial_sub.isNull()) {
        sub_out = new QLabel(initial_sub.isEmpty() ? "" : initial_sub);
        sub_out->setStyleSheet(
            QString("color:%1; font-size:9px; background:transparent; border:0;").arg(ui::colors::TEXT_TERTIARY()));
        vl->addWidget(sub_out);
    } else {
        sub_out = nullptr;
    }
    return f;
}

// Small ratio row
[[maybe_unused]] QLabel* ratio_row(QWidget* parent_vl_owner, const QString& label, const QString& color) {
    auto* hl = new QHBoxLayout;
    hl->setSpacing(4);
    hl->setContentsMargins(0, 0, 0, 0);
    auto* k = new QLabel(label);
    k->setStyleSheet(
        QString("color:%1; font-size:10px; background:transparent; border:0;").arg(ui::colors::TEXT_SECONDARY()));
    auto* v = new QLabel("—");
    v->setStyleSheet(
        QString("color:%1; font-size:10px; font-weight:600; background:transparent; border:0;").arg(color));
    v->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    hl->addWidget(k);
    hl->addStretch();
    hl->addWidget(v);
    static_cast<QVBoxLayout*>(parent_vl_owner->layout())->addLayout(hl);
    return v;
}

[[maybe_unused]] QTableWidget* make_table() {
    auto* t = new QTableWidget;
    t->setAlternatingRowColors(true);
    t->setStyleSheet(QString(R"(
        QTableWidget {
            background:%1; alternate-background-color:%2;
            gridline-color:%3; color:%4; border:0; font-size:10px;
        }
        QHeaderView::section {
            background:%5; color:%6; font-size:9px; font-weight:700;
            padding:4px; border:0; border-bottom:1px solid %3;
        }
        QTableWidget::item { padding:2px 6px; }
    )")
                         .arg(ui::colors::BG_SURFACE(), ui::colors::BG_BASE(), ui::colors::BORDER_DIM(),
                              ui::colors::TEXT_PRIMARY(), ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY()));
    t->horizontalHeader()->setStretchLastSection(false);
    t->verticalHeader()->setDefaultSectionSize(24);
    t->verticalHeader()->hide();
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    return t;
}

[[maybe_unused]] QChartView* make_chart_view(int fixed_height = 0) {
    auto* cv = new QChartView;
    cv->setRenderHint(QPainter::Antialiasing, false);
    cv->setStyleSheet("background:transparent; border:0;");
    if (fixed_height > 0)
        cv->setFixedHeight(fixed_height);
    return cv;
}

} // anonymous namespace

// ── Constructor ───────────────────────────────────────────────────────────────


EquityFinancialsTab::EquityFinancialsTab(QWidget* parent) : QWidget(parent) {
    build_ui();
    auto& svc = services::equity::EquityResearchService::instance();
    connect(&svc, &services::equity::EquityResearchService::financials_loaded, this,
            &EquityFinancialsTab::on_financials_loaded);
    // Dismiss the "LOADING FINANCIALS…" overlay on failure — it's hidden only on success.
    connect(&svc, &services::equity::EquityResearchService::error_occurred, this,
            [this](const QString& ctx, const QString&) {
                if (ctx == "Financials" && loading_overlay_)
                    loading_overlay_->hide_loading();
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
    if (btn_income_)     btn_income_->setText(tr("Income Statement"));
    if (btn_balance_)    btn_balance_->setText(tr("Balance Sheet"));
    if (btn_cashflow_)   btn_cashflow_->setText(tr("Cash Flow"));
    if (btn_export_csv_) btn_export_csv_->setText(tr("EXPORT CSV"));

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
        s = QString::number(static_cast<qint64>(av));
    return neg ? "-" + s : s;
}

QString EquityFinancialsTab::fmt_pct(double v) {
    return QString("%1%").arg(v * 100.0, 0, 'f', 2);
}

QString EquityFinancialsTab::fmt_ratio(double v) {
    return QString::number(v, 'f', 2);
}

} // namespace fincept::screens
