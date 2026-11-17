// src/screens/portfolio/views/AnalyticsSectorsView.cpp
#include "screens/portfolio/views/AnalyticsSectorsView.h"

#include "screens/portfolio/PortfolioCorrelation.h"
#include "screens/portfolio/PortfolioFormat.h"
#include "services/portfolio/PortfolioService.h"
#include "ui/charts/ChartExport.h"
#include "ui/charts/DonutChart.h"
#include "ui/charts/MatrixHeatmap.h"
#include "ui/charts/Treemap.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/tables/BarCellDelegate.h"
#include "ui/tables/NumericTableWidgetItem.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/StatTile.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QComboBox>
#include <QDate>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <cmath>

namespace fincept::screens {

namespace {

constexpr int kAsvValueRole = Qt::UserRole + 11;
constexpr int kAsvKeyRole = Qt::UserRole + 12;

QTableWidgetItem* asv_num(const QString& text, double v, const QColor& fg = {}) {
    auto* it = new ui::NumericTableWidgetItem(text, std::isfinite(v) ? v : -1e300);
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    it->setData(kAsvValueRole, v);
    if (fg.isValid())
        it->setForeground(fg);
    return it;
}

} // namespace

AnalyticsSectorsView::AnalyticsSectorsView(QWidget* parent) : QWidget(parent) {
    build_ui();
    auto& svc = services::PortfolioService::instance();
    connect(&svc, &services::PortfolioService::allocation_updated, this, [this](const QString& pid) {
        if (has_data_ && pid == summary_.portfolio.id)
            recompute();
    });
    connect(&svc, &services::PortfolioService::market_analytics_loaded, this,
            [this](const QString& pid, bool ok, const QString& error) {
                if (!has_data_ || pid != summary_.portfolio.id)
                    return;
                // The run is over either way: the matrix leaves "loading" — it arrives
                // through set_correlation(), or the failure is shown with RETRY.
                corr_error_ = ok ? QString() : (error.isEmpty() ? tr("the download failed") : error.left(300));
                corr_shown_sig_.clear();
                render_correlation();
                if (!ok && correlation_.isEmpty())
                    recompute(); // the notice bar names the failure and offers RETRY
            });
    connect(notice_->retry_button(), &QPushButton::clicked, this, [this]() {
        if (!has_data_ || summary_.holdings.isEmpty())
            return;
        corr_error_.clear();
        services::PortfolioService::instance().fetch_market_analytics(summary_, true);
        corr_shown_sig_.clear();
        render_correlation();
        recompute();
    });
    // Item colours of the table / charts are data: re-render them once an appearance change
    // has settled (never inside theme_changed). The stylesheet itself is kept in step by the
    // theme manager's re-style pass.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        if (has_data_)
            render_allocation();
    });
    // Currency / number format changed: re-format the report already held (no new
    // allocation / market-info request).
    ui::formatting::on_format_changed(this, [this]() {
        if (!has_data_)
            return;
        render_allocation();
        render_concentration();
        corr_shown_sig_.clear(); // the summary line's numbers are formatted text
        render_correlation();
    });
}

void AnalyticsSectorsView::build_ui() {
    apply_styles(); // one selector stylesheet, before any child exists
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* bar = new QWidget(this);
    bar->setObjectName("asvBar");
    auto* h = new QHBoxLayout(bar);
    h->setContentsMargins(10, 6, 10, 6);
    h->addWidget(new QLabel(tr("Group by:"), bar));
    dim_cb_ = new QComboBox(bar);
    for (const auto d : portfolio::all_allocation_dimensions())
        dim_cb_->addItem(pfmt::dimension_name(d), static_cast<int>(d));
    connect(dim_cb_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { render_allocation(); });
    h->addWidget(dim_cb_);
    h->addStretch(1);
    export_btn_ = new QPushButton(tr("EXPORT CSV"), bar);
    export_btn_->setToolTip(tr("Every dimension's slices with weights and values"));
    connect(export_btn_, &QPushButton::clicked, this, &AnalyticsSectorsView::export_csv);
    h->addWidget(export_btn_);
    root->addWidget(bar);

    notice_ = new pfmt::PortfolioNoticeBar(this);
    root->addWidget(notice_);

    kpis_ = new ui::KpiStrip(this);
    kpis_->set_min_tile_width(ui::metrics::width(130));
    for (const auto& [k, l] : {std::pair{"pos", tr("POSITIONS")}, std::pair{"effn", tr("EFFECTIVE N")},
                               std::pair{"hhi", tr("HHI")}, std::pair{"top1", tr("LARGEST")},
                               std::pair{"top3", tr("TOP 3")}, std::pair{"top5", tr("TOP 5")},
                               std::pair{"top10", tr("TOP 10")}, std::pair{"unknown", tr("UNCLASSIFIED")}}) {
        auto* t = kpis_->add_tile(QString::fromLatin1(k), l);
        t->set_compact(true);
    }
    auto* kw = new QWidget(this);
    auto* kv = new QVBoxLayout(kw);
    kv->setContentsMargins(10, 6, 10, 6);
    kv->addWidget(kpis_);
    root->addWidget(kw);

    tabs_ = new QTabWidget(this);
    auto* alloc_page = new QSplitter(Qt::Vertical, tabs_);
    auto* top = new QSplitter(Qt::Horizontal, alloc_page);
    donut_ = new ui::DonutChart(top);
    donut_->set_max_slices(10);
    donut_->setMinimumWidth(ui::metrics::width(260));
    top->addWidget(donut_);
    table_ = new QTableWidget(0, 7, top);
    table_->setHorizontalHeaderLabels({tr("GROUP"), tr("WEIGHT"), tr("VALUE"), tr("POSITIONS"), tr("UNREALISED"),
                                       tr("TODAY"), tr("HOLDINGS")});
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setShowGrid(false);
    table_->setSortingEnabled(true);
    table_->setWordWrap(false);
    table_->horizontalHeader()->setStretchLastSection(true);
    ui::metrics::follow_row_height(table_, 26);
    auto* bar_delegate = new ui::BarCellDelegate(ui::BarCellDelegate::Mode::Bar, table_);
    bar_delegate->set_value_role(kAsvValueRole);
    table_->setItemDelegateForColumn(1, bar_delegate);
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (dimension() != portfolio::AllocationDimension::Sector)
            return;
        const auto* it = table_->item(row, 0);
        if (it)
            emit sector_selected(it->data(kAsvKeyRole).toString().isEmpty() ? tr("Unclassified")
                                                                             : it->data(kAsvKeyRole).toString());
    });
    top->addWidget(table_);
    top->setStretchFactor(1, 2);
    alloc_page->addWidget(top);
    treemap_ = new ui::Treemap(alloc_page);
    treemap_->set_grouped(true);
    treemap_->set_value_formatter([](double v) { return pfmt::pct(v, 2); });
    alloc_page->addWidget(treemap_);
    tabs_->addTab(alloc_page, tr("ALLOCATION"));

    auto* corr_page = new QWidget(tabs_);
    auto* cv = new QVBoxLayout(corr_page);
    cv->setContentsMargins(0, 0, 0, 0);
    corr_summary_ = new QLabel(corr_page);
    corr_summary_->setObjectName("asvCorrSummary");
    corr_summary_->setWordWrap(true);
    corr_summary_->setContentsMargins(10, 6, 10, 6);
    cv->addWidget(corr_summary_);
    auto* scroll = new QScrollArea(corr_page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    corr_ = new ui::MatrixHeatmap(scroll);
    corr_->set_min_cell_size(18);
    scroll->setWidget(corr_);
    cv->addWidget(scroll, 1);
    tabs_->addTab(corr_page, tr("CORRELATION"));
    root->addWidget(tabs_, 1);
}

void AnalyticsSectorsView::apply_styles() {
    setStyleSheet(QString("#asvBar { background:%1; border-bottom:1px solid %2; }"
                          "#asvBar QLabel { color:%3; font-size:%4px; }"
                          "#asvCorrSummary { color:%3; font-size:%4px; }"
                          "QComboBox { background:%5; color:%6; border:1px solid %2; padding:2px 6px; }"
                          "QPushButton { background:transparent; color:%7; border:1px solid %7; padding:3px 10px;"
                          "  font-size:%4px; font-weight:700; }"
                          "QTableWidget { background:%8; color:%6; border:none; font-size:%9px; }")
                      .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::TEXT_SECONDARY(),
                           ui::fonts::px(-3), ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::AMBER(),
                           ui::colors::BG_BASE(), ui::fonts::px(-2)));
}

portfolio::AllocationDimension AnalyticsSectorsView::dimension() const {
    return static_cast<portfolio::AllocationDimension>(dim_cb_->currentData().toInt());
}

void AnalyticsSectorsView::set_data(const portfolio::PortfolioSummary& summary, const QString& currency) {
    if (summary.portfolio.id != summary_.portfolio.id) {
        corr_error_.clear();
        corr_shown_sig_.clear();
    }
    summary_ = summary;
    currency_ = currency.isEmpty() ? pfmt::base_ccy(summary) : currency;
    has_data_ = true;
    recompute();
    // Loading only while the analytics run is in flight; otherwise the matrix, the
    // failure (with RETRY) or "needs two holdings".
    render_correlation();
}

void AnalyticsSectorsView::set_correlation(const QHash<QString, double>& matrix) {
    correlation_ = matrix;
    render_correlation();
}

void AnalyticsSectorsView::recompute() {
    report_ = services::PortfolioService::instance().allocation(summary_);
    QStringList notes;
    auto kind = pfmt::PortfolioNoticeBar::Kind::Hidden;
    if (!summary_.complete) {
        kind = pfmt::PortfolioNoticeBar::Kind::Warning;
        notes << pfmt::incomplete_line(summary_);
    }
    if (!summary_.fx_stale.isEmpty()) {
        if (kind == pfmt::PortfolioNoticeBar::Kind::Hidden)
            kind = pfmt::PortfolioNoticeBar::Kind::Warning;
        notes << pfmt::fx_stale_line(summary_);
    }
    if (!report_.info_missing.isEmpty()) {
        if (kind == pfmt::PortfolioNoticeBar::Kind::Hidden)
            kind = pfmt::PortfolioNoticeBar::Kind::Loading;
        notes << tr("Loading sector / country data for %1 — they show as Unknown until it arrives.")
                     .arg(report_.info_missing.join(QStringLiteral(", ")));
    }
    if (!report_.class_guessed.isEmpty())
        notes << tr("Asset class guessed from the ticker for: %1.").arg(report_.class_guessed.join(QStringLiteral(", ")));
    bool retry = false;
    if (!corr_error_.isEmpty() && correlation_.isEmpty() && !summary_.holdings.isEmpty()) {
        kind = pfmt::PortfolioNoticeBar::Kind::Error;
        retry = true;
        notes.prepend(tr("Correlation unavailable — price history could not be downloaded: %1").arg(corr_error_));
    }
    notice_->set_notice(kind, notes.join(QLatin1Char('\n')), retry);
    render_allocation();
    render_concentration();
}

void AnalyticsSectorsView::render_allocation() {
    const auto dim = dimension();
    const QString ccy = currency_;
    const portfolio::AllocationBreakdown* bd = nullptr;
    for (const auto& b : report_.breakdowns)
        if (b.dimension == dim)
            bd = &b;
    QHash<QString, const portfolio::HoldingWithQuote*> by_sym;
    for (const auto& h : summary_.holdings)
        by_sym.insert(h.symbol, &h);

    QVector<ui::DonutSlice> slices;
    table_->setSortingEnabled(false);
    table_->setRowCount(0);
    QHash<QString, QString> group_of;
    if (bd) {
        for (const auto& s : bd->slices) {
            const QString label = pfmt::slice_label(dim, s.key);
            slices.append({s.key.isEmpty() ? QStringLiteral("__unknown__") : s.key, label, s.value, {}});
            double upnl = 0, day = 0;
            bool upnl_ok = true, day_ok = true;
            for (const auto& sym : s.symbols) {
                group_of.insert(sym, label);
                const auto* h = by_sym.value(sym, nullptr);
                if (!h)
                    continue;
                if (std::isfinite(h->unrealized_pnl))
                    upnl += h->unrealized_pnl;
                else
                    upnl_ok = false;
                if (std::isfinite(h->day_change_value))
                    day += h->day_change_value;
                else
                    day_ok = false;
            }
            const int r = table_->rowCount();
            table_->insertRow(r);
            auto* name = new QTableWidgetItem(label);
            name->setData(kAsvKeyRole, s.key);
            name->setForeground(QColor(s.key.isEmpty() ? ui::colors::TEXT_TERTIARY() : ui::colors::TEXT_PRIMARY()));
            table_->setItem(r, 0, name);
            table_->setItem(r, 1, asv_num(pfmt::upct(s.weight, 1), s.weight));
            table_->setItem(r, 2, asv_num(pfmt::money(s.value, ccy), s.value));
            table_->setItem(r, 3, asv_num(QString::number(s.count), s.count));
            const double u = upnl_ok ? upnl : portfolio::kAbsent;
            table_->setItem(r, 4, asv_num(pfmt::smoney(u, ccy), u, pfmt::dir_color(u)));
            const double d = day_ok ? day : portfolio::kAbsent;
            table_->setItem(r, 5, asv_num(pfmt::smoney(d, ccy), d, pfmt::dir_color(d)));
            auto* syms = new QTableWidgetItem(s.symbols.join(QStringLiteral(", ")));
            syms->setForeground(QColor(ui::colors::TEXT_SECONDARY()));
            syms->setToolTip(s.symbols.join(QStringLiteral(", ")));
            table_->setItem(r, 6, syms);
        }
    }
    table_->setSortingEnabled(true);
    table_->sortByColumn(1, Qt::DescendingOrder);
    table_->resizeColumnsToContents();
    table_->setToolTip(dim == portfolio::AllocationDimension::Sector
                           ? tr("Double-click a sector to filter the positions table")
                           : QString());

    donut_->set_value_formatter([ccy](double v) { return pfmt::money_c(v, ccy); });
    donut_->set_slices(slices);
    donut_->set_center_text(pfmt::dimension_name(dim).toUpper());
    donut_->set_state(slices.isEmpty() ? ui::PaintedChart::Empty : ui::PaintedChart::Ready,
                      tr("No valued positions."));

    QVector<ui::TreemapItem> items;
    for (const auto& h : summary_.holdings) {
        if (!h.valued() || !(h.market_value > 0))
            continue;
        ui::TreemapItem it;
        it.id = h.symbol;
        it.label = h.symbol;
        it.sublabel = h.name;
        it.group = group_of.value(h.symbol, pfmt::slice_label(dim, {}));
        it.weight = h.market_value;
        it.value = h.day_change_percent;
        items.append(it);
    }
    treemap_->set_weight_formatter([ccy](double v) { return pfmt::money_c(v, ccy); });
    treemap_->set_items(items);
    treemap_->set_state(items.isEmpty() ? ui::PaintedChart::Empty : ui::PaintedChart::Ready);
    kpis_->tile("unknown")->set_value(bd ? pfmt::upct(bd->unknown_weight, 1) : pfmt::dash());
    kpis_->tile("unknown")->set_sub_label(pfmt::dimension_name(dim));
}

void AnalyticsSectorsView::render_concentration() {
    const auto& c = report_.concentration;
    auto set = [this](const char* k, const QString& v, const QString& sub, const QString& tip) {
        auto* t = kpis_->tile(QString::fromLatin1(k));
        t->set_value(v);
        t->set_sub_label(sub);
        t->set_tooltip_text(tip);
    };
    set("pos", QString::number(c.positions),
        summary_.complete ? tr("valued") : tr("of %1 held").arg(summary_.total_positions),
        tr("Positions included in the weights (valued in %1).").arg(currency_));
    set("effn", pfmt::num(c.effective_n, 1), tr("equal-weight equivalent"),
        tr("1 / HHI: the number of equally weighted positions with the same concentration."));
    set("hhi", pfmt::num(c.hhi, 3), c.hhi > 0.25 ? tr("concentrated") : c.hhi > 0.15 ? tr("moderate") : tr("diversified"),
        tr("Herfindahl index Σw² of the weights (1/N for equal weights, 1 for a single position)."));
    if (c.hhi > 0.25)
        kpis_->tile("hhi")->set_tone(ui::StatTile::Tone::Warning);
    else
        kpis_->tile("hhi")->set_tone(ui::StatTile::Tone::Neutral);
    set("top1", pfmt::upct(c.top1, 1), c.largest_symbol, tr("Weight of the largest position."));
    set("top3", pfmt::upct(c.top3, 1), QString(), tr("Combined weight of the three largest positions."));
    set("top5", pfmt::upct(c.top5, 1), QString(), tr("Combined weight of the five largest positions."));
    set("top10", pfmt::upct(c.top10, 1), QString(), tr("Combined weight of the ten largest positions."));
}

void AnalyticsSectorsView::render_correlation() {
    QStringList syms;
    int valued = 0;
    for (const auto& h : summary_.holdings) {
        syms << h.symbol;
        if (h.valued())
            ++valued;
    }
    const bool pending =
        has_data_ && services::PortfolioService::instance().market_analytics_pending(summary_.portfolio.id);
    // set_data() / set_correlation() run on every price refresh: redraw only when the
    // matrix, the symbols or the load state changed.
    QString sig = syms.join(QLatin1Char(',')) + QLatin1Char('|') + QString::number(correlation_.size());
    for (auto it = correlation_.constBegin(); it != correlation_.constEnd(); ++it)
        sig += QString::number(it.value(), 'f', 3);
    sig += (pending ? QStringLiteral("|p|") : QStringLiteral("|-|")) + corr_error_ + currency_;
    if (sig == corr_shown_sig_)
        return;
    corr_shown_sig_ = sig;

    const auto m = pfcorr::build(correlation_, syms);
    if (m.labels.size() < 2) {
        corr_->clear();
        corr_summary_->clear();
        if (valued >= 2 && !corr_error_.isEmpty())
            corr_->set_state(ui::PaintedChart::Error, tr("Price history could not be downloaded: %1").arg(corr_error_));
        else if (valued >= 2 && pending)
            corr_->set_state(ui::PaintedChart::Loading, tr("Downloading price history…"));
        else
            corr_->set_state(ui::PaintedChart::Empty, tr("Needs two or more holdings with price history."));
        return;
    }
    corr_->set_state(ui::PaintedChart::Ready);
    pfcorr::show(corr_, m);
    const QStringList missing = [&]() {
        QStringList out;
        for (const auto& s : syms)
            if (!m.labels.contains(s))
                out << s;
        return out;
    }();
    QString text = tr("Pearson correlation of daily returns in %1 over the last %2 trading days, %3 holdings, ordered by "
                      "clustering so co-moving groups sit together. Red = move together (concentrated risk), green = "
                      "move opposite (diversifying).\nAverage pair %4 · most correlated %5 (%6) · least %7 (%8).")
                       .arg(currency_)
                       .arg(services::PortfolioService::kCorrelationWindowDays)
                       .arg(m.labels.size())
                       .arg(pfmt::num(m.avg_pair, 2), m.max_pair, pfmt::num(m.max_r, 2), m.min_pair,
                            pfmt::num(m.min_r, 2));
    if (!missing.isEmpty())
        text += QLatin1Char('\n') + tr("No price history: %1").arg(missing.join(QStringLiteral(", ")));
    corr_summary_->setText(text);
}

void AnalyticsSectorsView::export_csv() {
    QVector<QStringList> rows;
    for (const auto& b : report_.breakdowns)
        for (const auto& s : b.slices)
            rows.append({b.key, s.key, ui::formatting::export_number(s.weight, 4),
                         ui::formatting::export_number(s.value, 2), QString::number(s.count),
                         s.symbols.join(QLatin1Char(' '))});
    if (rows.isEmpty())
        return;
    ui::ChartExport::save_csv({"dimension", "group", "weight_pct", "value_" + currency_, "positions", "symbols"}, rows,
                              QStringLiteral("allocation_%1").arg(QDate::currentDate().toString(Qt::ISODate)), this);
}

void AnalyticsSectorsView::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange && has_data_) {
        for (int i = 0; i < dim_cb_->count(); ++i)
            dim_cb_->setItemText(i, pfmt::dimension_name(static_cast<portfolio::AllocationDimension>(dim_cb_->itemData(i).toInt())));
        recompute();
        corr_shown_sig_.clear();
        render_correlation();
    }
    QWidget::changeEvent(event);
}

} // namespace fincept::screens
