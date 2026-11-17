// src/screens/portfolio/views/ReportsView.cpp
#include "screens/portfolio/views/ReportsView.h"

#include "core/market/InstrumentCurrency.h"
#include "screens/portfolio/PortfolioFormat.h"
#include "services/portfolio/PortfolioPerformance.h"
#include "services/portfolio/PortfolioService.h"
#include "ui/charts/BarChart.h"
#include "ui/charts/ChartExport.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/tables/BarCellDelegate.h"
#include "ui/tables/NumericTableWidgetItem.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/InputMemory.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/StatTile.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QButtonGroup>
#include <QDate>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QLabel>
#include <QMessageBox>
#include <QPageSize>
#include <QPdfWriter>
#include <QPointer>
#include <QPushButton>
#include <QShowEvent>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextDocument>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

namespace {

using PW = portfolio::PerfWindow;
const PW kRvWindows[] = {PW::MTD, PW::QTD, PW::YTD, PW::Y1, PW::Y3, PW::Y5, PW::All};
constexpr int kRvValueRole = Qt::UserRole + 41;

QTableWidgetItem* rv_num(const QString& text, double v, const QColor& fg = {}) {
    auto* it = new ui::NumericTableWidgetItem(text, std::isfinite(v) ? v : -1e300);
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    it->setData(kRvValueRole, v);
    if (fg.isValid())
        it->setForeground(fg);
    return it;
}

QTableWidget* rv_table(const QStringList& headers, QWidget* parent) {
    auto* t = new QTableWidget(0, headers.size(), parent);
    t->setHorizontalHeaderLabels(headers);
    t->verticalHeader()->setVisible(false);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setShowGrid(false);
    t->setSortingEnabled(true);
    t->setWordWrap(false);
    t->horizontalHeader()->setStretchLastSection(true);
    ui::metrics::follow_row_height(t, 24);
    return t;
}

/// Amount of a ledger row in the base currency (NaN when its FX is unknown).
double rv_to_base(double amount, const portfolio::Transaction& t, const QString& base) {
    const QString ccy = !t.currency.isEmpty() ? t.currency
                        : portfolio::txn::is_cash_only(t.transaction_type) ? base
                                                                            : core::market::currency_for_symbol(t.symbol);
    const double major = core::market::to_major_units(amount, ccy);
    if (t.fx_rate > 0)
        return major * t.fx_rate;
    const QString maj = core::market::major_currency(ccy.isEmpty() ? base : ccy);
    if (maj == base)
        return major;
    // The trade-date rate from the daily history the ledger replay converts with: rows
    // without a recorded FX (most imports, the DIV dialog's default) were left out of
    // INCOME / FEES here while the ribbon's "Income" counted them.
    if (const auto fx = services::PortfolioService::instance().fx_on(maj, base, portfolio::ledger_row_date(t)))
        return major * *fx;
    return portfolio::kAbsent;
}

QString rv_csv(double v, int dp = 2) {
    return ui::formatting::export_number(v, dp); // NaN -> ""
}

} // namespace

ReportsView::ReportsView(QWidget* parent) : QWidget(parent) {
    build_ui();
    auto& svc = services::PortfolioService::instance();
    connect(&svc, &services::PortfolioService::metrics_computed, this, [this](const portfolio::ComputedMetrics& m) {
        if (!has_data_ || (!m.portfolio_id.isEmpty() && m.portfolio_id != summary_.portfolio.id))
            return;
        metrics_.insert(static_cast<int>(portfolio::perf_window_from_key(m.window)), m);
        if (portfolio::perf_window_from_key(m.window) == window_)
            recompute(); // the period's exact start date comes with the metrics
    });
    connect(&svc, &services::PortfolioService::ledger_changed, this, [this](const QString& pid) {
        if (has_data_ && pid == summary_.portfolio.id) {
            ledger_.clear();
            recompute();
        }
    });
    // Table / chart colours are data: re-render them once an appearance change has settled
    // (never inside theme_changed). The stylesheet itself is kept in step by the theme
    // manager's re-style pass.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        if (has_data_)
            render();
    });
    // Currency / number format changed: re-format the figures already computed (no
    // replay, no price-move download).
    ui::formatting::on_format_changed(this, [this]() {
        if (!has_data_)
            return;
        fill_txn_table();
        render();
    });
}

void ReportsView::build_ui() {
    apply_styles(); // one selector stylesheet, before any child exists
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* bar = new QWidget(this);
    bar->setObjectName("rvBar");
    auto* h = new QHBoxLayout(bar);
    h->setContentsMargins(10, 6, 10, 6);
    h->setSpacing(4);
    window_group_ = new QButtonGroup(this);
    for (const PW w : kRvWindows) {
        auto* b = new QPushButton(portfolio::perf_window_key(w), bar);
        b->setCheckable(true);
        b->setObjectName("rvChip");
        window_group_->addButton(b, static_cast<int>(w));
        h->addWidget(b);
        b->setChecked(w == window_);
    }
    connect(window_group_, &QButtonGroup::idClicked, this, [this](int id) {
        window_ = static_cast<PW>(id);
        request_metrics();
        recompute();
    });
    // The period chip the user picked comes back on the next visit.
    inputs_ = new ui::InputMemory(QStringLiteral("portfolio"), QStringLiteral("reports"), this);
    for (auto* b : window_group_->buttons())
        inputs_->track(QStringLiteral("window_") + b->text(), b);
    inputs_->restore();
    if (window_group_->checkedId() >= 0)
        window_ = static_cast<PW>(window_group_->checkedId());
    h->addSpacing(12);
    moves_btn_ = new QPushButton(tr("REFRESH PRICE MOVES"), bar);
    moves_btn_->setToolTip(tr("Download each holding's price move over the period again (base currency) — they load "
                              "by themselves while this view is open"));
    h->addWidget(moves_btn_);
    moves_timer_ = new QTimer(this);
    moves_timer_->setSingleShot(true);
    moves_timer_->setInterval(300);
    connect(moves_timer_, &QTimer::timeout, this, &ReportsView::load_period_moves);
    h->addStretch(1);
    csv_btn_ = new QPushButton(tr("CSV"), bar);
    csv_btn_->setToolTip(tr("The tab on display as CSV"));
    html_btn_ = new QPushButton(tr("HTML"), bar);
    pdf_btn_ = new QPushButton(tr("PDF"), bar);
    html_btn_->setToolTip(tr("The whole report as a web page"));
    pdf_btn_->setToolTip(tr("The whole report as a PDF"));
    for (auto* b : {csv_btn_, html_btn_, pdf_btn_})
        h->addWidget(b);
    root->addWidget(bar);
    connect(moves_btn_, &QPushButton::clicked, this, &ReportsView::load_period_moves);
    connect(csv_btn_, &QPushButton::clicked, this, &ReportsView::export_csv);
    connect(html_btn_, &QPushButton::clicked, this, [this]() { export_document(false); });
    connect(pdf_btn_, &QPushButton::clicked, this, [this]() { export_document(true); });

    notice_ = new pfmt::PortfolioNoticeBar(this);
    connect(notice_->retry_button(), &QPushButton::clicked, this, &ReportsView::load_period_moves);
    root->addWidget(notice_);

    kpis_ = new ui::KpiStrip(this);
    kpis_->set_min_tile_width(ui::metrics::width(140));
    for (const auto& [k, l] :
         {std::pair{"start", tr("START NAV")}, std::pair{"end", tr("END NAV")}, std::pair{"flows", tr("NET FLOWS")},
          std::pair{"gain", tr("INVESTMENT GAIN")}, std::pair{"twr", tr("TWR")}, std::pair{"mwr", tr("MWR")},
          std::pair{"realised", tr("REALISED")}, std::pair{"income", tr("INCOME")}, std::pair{"costs", tr("FEES & TAXES")}}) {
        auto* t = kpis_->add_tile(QString::fromLatin1(k), l);
        t->set_compact(true);
    }
    auto* kw = new QWidget(this);
    auto* kv = new QVBoxLayout(kw);
    kv->setContentsMargins(10, 6, 10, 2);
    kv->addWidget(kpis_);
    summary_note_ = new QLabel(kw);
    summary_note_->setObjectName("rvNote");
    summary_note_->setWordWrap(true);
    kv->addWidget(summary_note_);
    root->addWidget(kw);

    tabs_ = new QTabWidget(this);
    auto* cpage = new QSplitter(Qt::Vertical, tabs_);
    contrib_chart_ = new ui::BarChart(cpage);
    contrib_chart_->set_mode(ui::BarChart::Mode::Stacked);
    contrib_chart_->set_show_legend(true);
    contrib_chart_->setMinimumHeight(ui::metrics::height(170));
    cpage->addWidget(contrib_chart_);
    contrib_table_ = rv_table({tr("SYMBOL"), tr("REALISED"), tr("INCOME"), tr("COSTS"), tr("BOOKED"),
                               tr("PERIOD MOVE"), tr("UNREALISED (PERIOD, EST.)"), tr("UNREALISED (SINCE COST)"),
                               tr("TODAY'S CONTRIB.")},
                              cpage);
    for (int c : {1, 2, 4, 6, 8}) {
        auto* d = new ui::BarCellDelegate(ui::BarCellDelegate::Mode::SignedBar, contrib_table_);
        d->set_value_role(kRvValueRole);
        contrib_table_->setItemDelegateForColumn(c, d);
    }
    cpage->addWidget(contrib_table_);
    cpage->setStretchFactor(1, 2);
    tabs_->addTab(cpage, tr("CONTRIBUTION"));
    txn_table_ = rv_table({tr("DATE"), tr("TYPE"), tr("SYMBOL"), tr("QTY"), tr("PRICE"), tr("CCY"), tr("AMOUNT"),
                           tr("FEES"), tr("TAXES"), tr("AMOUNT (BASE)"), tr("NOTES")},
                          tabs_);
    tabs_->addTab(txn_table_, tr("TRANSACTIONS"));
    root->addWidget(tabs_, 1);
}

void ReportsView::apply_styles() {
    setStyleSheet(QString("#rvBar { background:%1; border-bottom:1px solid %2; }"
                          "#rvNote { color:%3; font-size:%4px; }"
                          "QPushButton { background:transparent; color:%3; border:1px solid %2; padding:2px 8px;"
                          "  font-size:%4px; font-weight:700; }"
                          "QPushButton#rvChip:checked { color:%5; border-color:%5; background:%6; }"
                          "QPushButton:hover { color:%5; }"
                          "QTableWidget { background:%7; color:%8; border:none; font-size:%9px; }")
                      .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::TEXT_SECONDARY(),
                           ui::fonts::px(-3), ui::colors::AMBER(), ui::colors::ACCENT_BG(), ui::colors::BG_BASE(),
                           ui::colors::TEXT_PRIMARY(), ui::fonts::px(-2)));
}

QDate ReportsView::period_start() const {
    const auto it = metrics_.constFind(static_cast<int>(window_));
    if (it != metrics_.constEnd() && !it->perf_start.isEmpty())
        return pfmt::iso_date(it->perf_start);
    const QDate b = portfolio::perf_window_base_date(window_, QDate::currentDate());
    return b.isValid() ? b : QDate(1900, 1, 1);
}

void ReportsView::set_data(const portfolio::PortfolioSummary& summary, const QString& currency) {
    const bool new_book = summary.portfolio.id != summary_.portfolio.id;
    summary_ = summary;
    currency_ = currency.isEmpty() ? pfmt::base_ccy(summary) : currency;
    has_data_ = true;
    if (new_book) {
        cancel_period_moves(); // the previous book's download must not land here
        metrics_.clear();
        ledger_.clear();
        moves_.clear();
        moves_for_.clear();
        moves_failed_for_.clear();
        moves_error_.clear();
        request_metrics();
    }
    recompute();
}

QString ReportsView::moves_key() const {
    return portfolio::perf_window_key(window_) + QLatin1Char('|') + period_start().toString(Qt::ISODate) +
           QLatin1Char('|') + summary_.portfolio.id;
}

void ReportsView::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!event->spontaneous())
        moves_failed_for_.clear(); // a new visit may try once more by itself
    schedule_period_moves();
}

void ReportsView::hideEvent(QHideEvent* event) {
    // Leaving the view stops the download (it loads again on the next visit).
    if (!event->spontaneous())
        cancel_period_moves();
    QWidget::hideEvent(event);
}

void ReportsView::cancel_period_moves() {
    moves_timer_->stop();
    if (moves_seq_ == 0)
        return;
    moves_req_.cancel();
    moves_req_ = {};
    moves_seq_ = 0;
    moves_loading_for_.clear();
    moves_btn_->setEnabled(true);
    moves_btn_->setText(tr("REFRESH PRICE MOVES"));
}

void ReportsView::schedule_period_moves() {
    if (!isVisible() || !has_data_ || summary_.holdings.isEmpty())
        return;
    // The period's exact start comes with its metrics (local, fast): wait for them, so
    // the download is done once for the right window.
    if (!metrics_.contains(static_cast<int>(window_)))
        return;
    const QString key = moves_key();
    if (moves_for_ == key || moves_failed_for_ == key || (moves_seq_ != 0 && moves_loading_for_ == key))
        return;
    moves_timer_->start();
}

void ReportsView::request_metrics() {
    if (has_data_ && !metrics_.contains(static_cast<int>(window_)))
        services::PortfolioService::instance().compute_metrics(summary_, window_);
}

void ReportsView::recompute() {
    if (!has_data_)
        return;
    auto& svc = services::PortfolioService::instance();
    if (ledger_.isEmpty()) {
        ledger_ = svc.ledger(summary_.portfolio.id);
        replay_ = svc.replay_ledger(summary_.portfolio.id);
    }
    const QString base = pfmt::base_ccy(summary_);
    const QDate start = period_start(); // base point: rows AFTER it belong to the period
    const QDate today = QDate::currentDate();

    QHash<QString, Line> by;
    realised_ = income_ = fees_ = taxes_ = 0;
    unknown_fx_rows_ = 0;
    for (const auto& h : summary_.holdings) {
        Line& l = by[h.symbol];
        l.symbol = h.symbol;
        l.unrealised = h.unrealized_pnl;
    }
    for (const auto& r : replay_.realized) {
        if (!(r.close_date > start && r.close_date <= today))
            continue;
        Line& l = by[r.symbol];
        l.symbol = r.symbol;
        if (std::isfinite(r.gain_base)) {
            l.realised += r.gain_base;
            realised_ += r.gain_base;
        } else {
            l.unknown_fx = true;
            ++unknown_fx_rows_;
        }
    }
    namespace tx = portfolio::txn;
    for (const auto& t : ledger_) {
        const QDate d = portfolio::ledger_row_date(t);
        if (!(d > start && d <= today))
            continue;
        const QString type = t.transaction_type.toUpper();
        const double fees_b = t.fees > 0 ? rv_to_base(t.fees, t, base) : 0.0;
        const double taxes_b = t.taxes > 0 ? rv_to_base(t.taxes, t, base) : 0.0;
        if (!std::isfinite(fees_b) || !std::isfinite(taxes_b))
            ++unknown_fx_rows_;
        double income = 0, costs = 0;
        if (type == tx::kDividend) {
            income = rv_to_base(t.quantity * t.price - t.fees - t.taxes, t, base);
            taxes_ += std::isfinite(taxes_b) ? taxes_b : 0;
            fees_ += std::isfinite(fees_b) ? fees_b : 0;
        } else if (type == tx::kInterest) {
            income = rv_to_base(t.quantity - t.taxes, t, base);
            taxes_ += std::isfinite(taxes_b) ? taxes_b : 0;
        } else if (type == tx::kFee) {
            costs = rv_to_base(t.quantity, t, base);
            fees_ += std::isfinite(costs) ? costs : 0;
        } else if (type == tx::kTax) {
            costs = rv_to_base(t.quantity, t, base);
            taxes_ += std::isfinite(costs) ? costs : 0;
        } else if (type == tx::kBuy || type == tx::kSell) {
            costs = (std::isfinite(fees_b) ? fees_b : 0) + (std::isfinite(taxes_b) ? taxes_b : 0);
            fees_ += std::isfinite(fees_b) ? fees_b : 0;
            taxes_ += std::isfinite(taxes_b) ? taxes_b : 0;
        }
        if (!std::isfinite(income)) {
            ++unknown_fx_rows_;
            income = 0;
        }
        if (!std::isfinite(costs)) {
            ++unknown_fx_rows_;
            costs = 0;
        }
        income_ += income;
        const QString key = t.symbol.isEmpty() ? tr("(cash / account)") : t.symbol;
        Line& l = by[key];
        l.symbol = key;
        l.income += income;
        l.costs += costs;
    }

    // Today's contribution against the previous close NAV (the old view divided by today's).
    const double prev_nav = pfmt::holdings_value(summary_) - summary_.total_day_change;
    for (const auto& h : summary_.holdings) {
        Line& l = by[h.symbol];
        l.day_contrib_pct = std::isfinite(h.day_change_value) && prev_nav > 0 ? h.day_change_value / prev_nav * 100.0
                                                                              : portfolio::kAbsent;
        // Period price moves (loaded on demand): lots older than the period move by the
        // period return; lots bought inside it by their P&L since purchase.
        const auto mv = moves_.constFind(h.symbol);
        if (moves_for_ == moves_key() && mv != moves_.constEnd() && h.valued()) {
            l.period_move = *mv;
            const auto pos = replay_.positions.constFind(h.symbol);
            double old_q = h.quantity, new_unreal = 0;
            bool explained = pos != replay_.positions.constEnd() && h.ledger_explained;
            if (explained) {
                old_q = 0;
                const double unit = h.market_value / h.quantity;
                for (const auto& lot : pos->lots) {
                    if (lot.open_date <= start)
                        old_q += lot.quantity;
                    else if (std::isfinite(lot.cost_base))
                        new_unreal += lot.quantity * unit - lot.cost_base;
                }
            }
            const double old_value = h.quantity > 0 ? h.market_value * old_q / h.quantity : 0;
            l.period_unrealised = (*mv > -1.0 ? old_value * *mv / (1.0 + *mv) : portfolio::kAbsent) + new_unreal;
        }
    }
    lines_.clear();
    for (auto it = by.constBegin(); it != by.constEnd(); ++it)
        lines_.append(it.value());
    std::sort(lines_.begin(), lines_.end(), [](const Line& a, const Line& b) {
        return (a.realised + a.income - a.costs) > (b.realised + b.income - b.costs);
    });

    fill_txn_table();
    render();
    schedule_period_moves();
}

void ReportsView::fill_txn_table() {
    namespace tx = portfolio::txn;
    const QString base = pfmt::base_ccy(summary_);
    const QDate start = period_start();
    const QDate today = QDate::currentDate();
    // Period transactions, newest first.
    txn_table_->setSortingEnabled(false);
    txn_table_->setRowCount(0);
    for (const auto& t : ledger_) {
        const QDate d = portfolio::ledger_row_date(t);
        if (!(d > start && d <= today))
            continue;
        const int r = txn_table_->rowCount();
        txn_table_->insertRow(r);
        const QString type = t.transaction_type.toUpper();
        const bool cash = tx::is_cash_only(type);
        const QString ccy = !t.currency.isEmpty() ? t.currency : cash ? base : core::market::currency_for_symbol(t.symbol);
        const double amount = cash ? t.quantity : t.quantity * t.price;
        txn_table_->setItem(r, 0, new ui::NumericTableWidgetItem(pfmt::date(d, ui::formatting::DateStyle::Short), d.toJulianDay()));
        txn_table_->setItem(r, 1, new QTableWidgetItem(type));
        txn_table_->setItem(r, 2, new QTableWidgetItem(t.symbol));
        txn_table_->setItem(r, 3, rv_num(cash ? QString() : pfmt::qty(t.quantity), cash ? portfolio::kAbsent : t.quantity));
        txn_table_->setItem(r, 4, rv_num(cash || type == tx::kSplit ? QString() : pfmt::price(t.price, ccy, t.symbol),
                                         t.price));
        txn_table_->setItem(r, 5, new QTableWidgetItem(ccy));
        txn_table_->setItem(r, 6, rv_num(type == tx::kSplit ? QString() : pfmt::money(amount, ccy), amount));
        txn_table_->setItem(r, 7, rv_num(t.fees > 0 ? pfmt::money(t.fees, ccy) : QString(), t.fees));
        txn_table_->setItem(r, 8, rv_num(t.taxes > 0 ? pfmt::money(t.taxes, ccy) : QString(), t.taxes));
        const double ab = type == tx::kSplit ? portfolio::kAbsent : rv_to_base(amount, t, base);
        txn_table_->setItem(r, 9, rv_num(pfmt::money(ab, base), ab));
        txn_table_->setItem(r, 10, new QTableWidgetItem(t.notes));
    }
    txn_table_->setSortingEnabled(true);
    txn_table_->sortByColumn(0, Qt::DescendingOrder);
    txn_table_->resizeColumnsToContents();
}

void ReportsView::render() {
    const QString base = pfmt::base_ccy(summary_);
    const auto mit = metrics_.constFind(static_cast<int>(window_));
    const portfolio::ComputedMetrics* m = mit == metrics_.constEnd() ? nullptr : &*mit;
    auto tile = [this](const char* k) { return kpis_->tile(QString::fromLatin1(k)); };
    using Tone = ui::StatTile::Tone;
    auto tone = [](double v) { return !std::isfinite(v) || v == 0 ? Tone::Neutral : v > 0 ? Tone::Positive : Tone::Negative; };

    double start_nav = portfolio::kAbsent, end_nav = portfolio::kAbsent, flows = 0;
    if (m && m->twr_series.size() >= 2) {
        start_nav = m->twr_series.first().nav;
        end_nav = m->twr_series.last().nav;
        for (int i = 1; i < m->twr_series.size(); ++i)
            flows += m->twr_series[i].net_flow;
    }
    const double gain = std::isfinite(start_nav) && std::isfinite(end_nav) ? end_nav - start_nav - flows : portfolio::kAbsent;
    for (auto* t : {tile("start"), tile("end"), tile("flows"), tile("gain"), tile("twr"), tile("mwr")})
        t->set_state(m ? ui::StatTile::State::Normal : ui::StatTile::State::Loading);
    tile("start")->set_value(pfmt::money(start_nav, base));
    tile("start")->set_sub_label(m ? pfmt::date(m->perf_start) : QString());
    tile("end")->set_value(pfmt::money(end_nav, base));
    tile("end")->set_sub_label(m ? pfmt::date(m->perf_end) : QString());
    tile("flows")->set_value(pfmt::smoney(flows, base));
    tile("flows")->set_sub_label(summary_.portfolio.track_cash ? tr("deposits − withdrawals") : tr("buys − sells − income"));
    tile("gain")->set_value(pfmt::smoney(gain, base));
    tile("gain")->set_tone(tone(gain));
    tile("gain")->set_sub_label(tr("end − start − flows"));
    tile("twr")->set_value(m ? pfmt::opt_pct(m->total_return) : pfmt::dash());
    tile("twr")->set_tone(m && m->total_return ? tone(*m->total_return) : Tone::Neutral);
    tile("twr")->set_sub_label(m && m->benchmark_return
                                   ? tr("%1 %2").arg(m->benchmark_symbol, pfmt::pct(*m->benchmark_return))
                                   : QString());
    tile("mwr")->set_value(m ? (m->mwr ? pfmt::opt_pct(m->mwr) : pfmt::opt_pct(m->mwr_period)) : pfmt::dash());
    tile("mwr")->set_sub_label(m && m->mwr ? tr("p.a.") : tr("period"));
    tile("realised")->set_value(pfmt::smoney(realised_, base));
    tile("realised")->set_tone(tone(realised_));
    tile("realised")->set_sub_label(tr("booked vs cost"));
    tile("income")->set_value(pfmt::money(income_, base));
    tile("income")->set_sub_label(tr("dividends + interest, net"));
    tile("costs")->set_value(pfmt::money(fees_ + taxes_, base));
    tile("costs")->set_sub_label(tr("fees %1 · taxes %2").arg(pfmt::money_c(fees_, base), pfmt::money_c(taxes_, base)));

    QStringList notes;
    notes << tr("Realised = gains on lots sold in the period, measured against their cost (%1 relief). Income is net "
                "of withholding. Unrealised (period) is an estimate from each holding's price move over the period "
                "(downloaded while this view is open).")
                 .arg(summary_.portfolio.lot_method);
    if (unknown_fx_rows_ > 0)
        notes << tr("%n ledger row(s) in a foreign currency have no FX rate recorded and are left out of the "
                    "base-currency totals.",
                    "", unknown_fx_rows_);
    summary_note_->setText(notes.join(QLatin1Char(' ')));
    QStringList warn;
    if (!summary_.complete)
        warn << pfmt::incomplete_line(summary_);
    if (!m)
        warn << tr("Computing the period's NAV history…");
    const QString mkey = moves_key();
    const bool moves_failed = moves_failed_for_ == mkey && !moves_error_.isEmpty();
    if (moves_failed)
        warn.prepend(tr("Period price moves could not be loaded: %1").arg(moves_error_));
    notice_->set_notice(warn.isEmpty()       ? pfmt::PortfolioNoticeBar::Kind::Hidden
                        : moves_failed       ? pfmt::PortfolioNoticeBar::Kind::Error
                        : !summary_.complete ? pfmt::PortfolioNoticeBar::Kind::Warning
                                             : pfmt::PortfolioNoticeBar::Kind::Loading,
                        warn.join(QLatin1Char('\n')), moves_failed);
    // "…" in the two period columns while their download runs (never blank meanwhile).
    const bool moves_loading = moves_seq_ != 0 && moves_loading_for_ == mkey;

    contrib_table_->setSortingEnabled(false);
    contrib_table_->setRowCount(static_cast<int>(lines_.size()));
    QStringList cats;
    QVector<double> rv, iv, cv, uv;
    for (int r = 0; r < lines_.size(); ++r) {
        const auto& l = lines_[r];
        const double booked = l.realised + l.income - l.costs;
        contrib_table_->setItem(r, 0, new QTableWidgetItem(l.symbol));
        contrib_table_->setItem(r, 1, rv_num(pfmt::smoney(l.realised, base), l.realised, pfmt::dir_color(l.realised)));
        contrib_table_->setItem(r, 2, rv_num(pfmt::money(l.income, base), l.income));
        contrib_table_->setItem(r, 3, rv_num(l.costs > 0 ? pfmt::money(l.costs, base) : QString(), l.costs));
        contrib_table_->setItem(r, 4, rv_num(pfmt::smoney(booked, base), booked, pfmt::dir_color(booked)));
        if (moves_loading) {
            for (int c : {5, 6}) {
                auto* busy = rv_num(QStringLiteral("…"), portfolio::kAbsent, QColor(ui::colors::TEXT_TERTIARY()));
                busy->setToolTip(tr("Downloading the period's price moves…"));
                contrib_table_->setItem(r, c, busy);
            }
        } else {
            contrib_table_->setItem(r, 5, rv_num(std::isfinite(l.period_move) ? pfmt::fpct(l.period_move, 1) : QString(),
                                                 l.period_move, pfmt::dir_color(l.period_move)));
            contrib_table_->setItem(r, 6, rv_num(pfmt::smoney(l.period_unrealised, base), l.period_unrealised,
                                                 pfmt::dir_color(l.period_unrealised)));
        }
        contrib_table_->setItem(r, 7, rv_num(pfmt::smoney(l.unrealised, base), l.unrealised, pfmt::dir_color(l.unrealised)));
        contrib_table_->setItem(r, 8, rv_num(pfmt::pct(l.day_contrib_pct, 2), l.day_contrib_pct,
                                             pfmt::dir_color(l.day_contrib_pct)));
        if (l.unknown_fx)
            contrib_table_->item(r, 1)->setToolTip(tr("Some sales of %1 have no FX rate: left out.").arg(l.symbol));
        cats << l.symbol;
        rv << l.realised;
        iv << l.income;
        cv << -l.costs;
        uv << l.period_unrealised;
    }
    contrib_table_->setSortingEnabled(true);
    contrib_table_->resizeColumnsToContents();
    QVector<ui::BarSeries> series = {ui::BarSeries{tr("Realised"), rv, ui::colors::chart_color(0)},
                                     ui::BarSeries{tr("Income"), iv, ui::colors::chart_color(1)},
                                     ui::BarSeries{tr("Costs"), cv, ui::colors::chart_color(2)}};
    if (moves_for_ == mkey)
        series.append(ui::BarSeries{tr("Unrealised (period, est.)"), uv, ui::colors::chart_color(3)});
    contrib_chart_->set_categories(cats);
    contrib_chart_->set_series(series);
    contrib_chart_->set_value_formatter([base](double v) { return pfmt::money_c(v, base); });
    contrib_chart_->set_state(cats.isEmpty() ? ui::PaintedChart::Empty : ui::PaintedChart::Ready);
}

void ReportsView::load_period_moves() {
    if (!has_data_ || summary_.holdings.isEmpty())
        return;
    moves_timer_->stop();
    const QDate start = period_start();
    const QString key = moves_key();
    moves_req_.cancel(); // a newer load supersedes the one in flight
    moves_req_ = {};
    moves_failed_for_.clear();
    moves_error_.clear();
    moves_loading_for_ = key;
    moves_btn_->setEnabled(false);
    moves_btn_->setText(tr("LOADING…"));
    const quint64 seq = ++moves_next_seq_;
    moves_seq_ = seq;
    QPointer<ReportsView> self(this);
    const services::AnalyticsRequest req = services::PortfolioService::instance().historical_stress(
        summary_, start.toString(Qt::ISODate), QDate::currentDate().toString(Qt::ISODate),
        [self, key, seq](const services::AnalyticsResult& res) {
            // Superseded, cancelled or the view was left: a late reply never draws (the
            // reply may also come synchronously — Python missing — hence the own id).
            if (!self || res.cancelled || seq != self->moves_seq_)
                return;
            self->moves_seq_ = 0;
            self->moves_req_ = {};
            self->moves_loading_for_.clear();
            self->moves_btn_->setEnabled(true);
            self->moves_btn_->setText(tr("REFRESH PRICE MOVES"));
            if (!res.success) {
                self->moves_failed_for_ = key; // no automatic retry in this visit: RETRY is the user's
                self->moves_error_ = res.error.isEmpty() ? tr("the download failed") : res.error;
                self->render();
                return;
            }
            self->moves_.clear();
            const QJsonObject hold = res.data.value("holdings").toObject();
            for (auto it = hold.begin(); it != hold.end(); ++it)
                if (it.value().toObject().value("return").isDouble())
                    self->moves_.insert(it.key(), it.value().toObject().value("return").toDouble());
            self->moves_for_ = key;
            self->recompute();
        });
    if (moves_seq_ == seq) {
        moves_req_ = req; // still in flight: keep its cancel token
        render();         // "…" in the period columns while it runs
    }
}

void ReportsView::export_csv() {
    auto* table = tabs_->currentIndex() == 0 ? contrib_table_ : txn_table_;
    QStringList headers;
    for (int c = 0; c < table->columnCount(); ++c)
        headers << table->horizontalHeaderItem(c)->text();
    // Base-currency amounts name their currency (the transactions' own amounts are in
    // their CCY column): one ISO code column, appended.
    headers << QStringLiteral("base_currency");
    const QString base = pfmt::base_ccy(summary_);
    QVector<QStringList> rows;
    for (int r = 0; r < table->rowCount(); ++r) {
        QStringList row;
        for (int c = 0; c < table->columnCount(); ++c) {
            const auto* it = table->item(r, c);
            if (!it) {
                row << QString();
                continue;
            }
            const QVariant v = it->data(kRvValueRole);
            row << (v.isValid() ? rv_csv(v.toDouble(), 4) : it->text());
        }
        row << base;
        rows.append(row);
    }
    ui::ChartExport::save_csv(headers, rows,
                              QStringLiteral("report_%1_%2_%3")
                                  .arg(tabs_->currentIndex() == 0 ? QStringLiteral("contribution") : QStringLiteral("transactions"),
                                       portfolio::perf_window_key(window_), QDate::currentDate().toString(Qt::ISODate)),
                              this);
}

QString ReportsView::report_html() const {
    const QString base = pfmt::base_ccy(summary_);
    auto esc = [](const QString& s) { return s.toHtmlEscaped(); };
    QString h;
    QTextStream out(&h);
    out << "<html><head><meta charset='utf-8'><title>" << esc(summary_.portfolio.name) << "</title>"
        << "<style>body{font-family:sans-serif;} table{border-collapse:collapse;margin:8px 0;} "
           "td,th{border:1px solid;padding:3px 8px;} td.n{text-align:right;}</style></head><body>";
    out << "<h2>" << esc(tr("Portfolio report — %1").arg(summary_.portfolio.name)) << "</h2>";
    out << "<p>" << esc(tr("Period: %1 (%2). Base currency: %3. Generated %4.")
                            .arg(portfolio::perf_window_key(window_),
                                 pfmt::date(period_start()) + QStringLiteral(" → ") + pfmt::date(QDate::currentDate()),
                                 base, ui::formatting::date_time(QDateTime::currentDateTime())))
        << "</p>";
    if (!summary_.complete)
        out << "<p><b>" << esc(pfmt::incomplete_line(summary_)) << "</b></p>";
    out << "<table>";
    for (auto* t : kpis_->findChildren<ui::StatTile*>())
        out << "<tr><th align='left'>" << esc(t->label()) << "</th><td class='n'>" << esc(t->value_text()) << "</td></tr>";
    out << "</table>";
    auto dump = [&](const QTableWidget* table, const QString& title) {
        out << "<h3>" << esc(title) << "</h3><table><tr>";
        for (int c = 0; c < table->columnCount(); ++c)
            out << "<th>" << esc(table->horizontalHeaderItem(c)->text()) << "</th>";
        out << "</tr>";
        for (int r = 0; r < table->rowCount(); ++r) {
            out << "<tr>";
            for (int c = 0; c < table->columnCount(); ++c) {
                const auto* it = table->item(r, c);
                out << (c == 0 ? "<td>" : "<td class='n'>") << esc(it ? it->text() : QString()) << "</td>";
            }
            out << "</tr>";
        }
        out << "</table>";
    };
    dump(contrib_table_, tr("Contribution by holding"));
    dump(txn_table_, tr("Transactions in the period"));
    out << "<p>" << esc(summary_note_->text()) << "</p></body></html>";
    return h;
}

void ReportsView::export_document(bool pdf) {
    const QString name = QStringLiteral("portfolio_report_%1_%2.%3")
                             .arg(portfolio::perf_window_key(window_), QDate::currentDate().toString(Qt::ISODate),
                                  pdf ? QStringLiteral("pdf") : QStringLiteral("html"));
    const QString path = QFileDialog::getSaveFileName(this, pdf ? tr("Save PDF") : tr("Save HTML"), name,
                                                      pdf ? tr("PDF (*.pdf)") : tr("Web page (*.html)"));
    if (path.isEmpty())
        return;
    const QString html = report_html();
    if (pdf) {
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QPageSize::A4));
        writer.setPageMargins(QMarginsF(12, 12, 12, 12), QPageLayout::Millimeter);
        // QPdfWriter defaults to 1200 dpi while the document lays out in screen pixels
        // (the app font is pixel-sized): the whole report came out as a thumbnail in the
        // page corner. Lay out at 96 dpi, the page's printable width in those pixels.
        writer.setResolution(96);
        writer.setTitle(summary_.portfolio.name);
        QTextDocument doc;
        QFont doc_font = doc.defaultFont();
        doc_font.setPointSizeF(8.5);
        doc.setDefaultFont(doc_font);
        doc.setHtml(html);
        doc.setPageSize(QSizeF(writer.pageLayout().paintRectPixels(writer.resolution()).size()));
        doc.print(&writer);
        // QPdfWriter reports nothing when the file cannot be created.
        if (!QFile::exists(path))
            QMessageBox::warning(this, tr("Export"), tr("Could not write %1.").arg(path));
        return;
    }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export"), tr("Could not write %1: %2").arg(path, f.errorString()));
        return;
    }
    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    ts << html;
}

void ReportsView::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange && has_data_)
        render();
    QWidget::changeEvent(event);
}

} // namespace fincept::screens
