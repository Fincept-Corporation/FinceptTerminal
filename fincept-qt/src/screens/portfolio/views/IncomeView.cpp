// src/screens/portfolio/views/IncomeView.cpp
#include "screens/portfolio/views/IncomeView.h"

#include "core/market/InstrumentCurrency.h"
#include "screens/portfolio/PortfolioFormat.h"
#include "services/portfolio/PortfolioService.h"
#include "ui/charts/BarChart.h"
#include "ui/charts/ChartExport.h"
#include "ui/formatting/FormatSettings.h"
#include "ui/formatting/FormatWidgets.h"
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
#include <QMap>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

namespace {

QTableWidgetItem* inc_num(const QString& text, double v, const QColor& fg = {}) {
    auto* it = new ui::NumericTableWidgetItem(text, std::isfinite(v) ? v : -1e300);
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    if (fg.isValid())
        it->setForeground(fg);
    return it;
}

QTableWidgetItem* inc_text(const QString& text, const QColor& fg = {}) {
    auto* it = new QTableWidgetItem(text);
    if (fg.isValid())
        it->setForeground(fg);
    return it;
}

QTableWidgetItem* inc_date(const QDate& d) {
    return new ui::NumericTableWidgetItem(pfmt::date(d, ui::formatting::DateStyle::Short),
                                          d.isValid() ? double(d.toJulianDay()) : -1e300);
}

QTableWidget* inc_table(const QStringList& headers, QWidget* parent) {
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

void inc_apply_qss(QWidget* w) {
    // Set only when the text changed (each set re-polishes the whole view).
    const QString css = QString("#incBar { background:%1; border-bottom:1px solid %2; }"
                             "#incBar QLabel { color:%3; font-size:%4px; }"
                             "QComboBox { background:%5; color:%6; border:1px solid %2; padding:2px 6px; }"
                             "QPushButton { background:transparent; color:%7; border:1px solid %7; padding:3px 10px;"
                             "  font-size:%4px; font-weight:700; }"
                             "QTableWidget { background:%8; color:%6; border:none; font-size:%9px; }")
                         .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::TEXT_SECONDARY(),
                              ui::fonts::px(-3), ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(),
                              ui::colors::AMBER(), ui::colors::BG_BASE(), ui::fonts::px(-2));
    if (w->styleSheet() != css)
        w->setStyleSheet(css);
}

} // namespace

IncomeView::IncomeView(QWidget* parent) : QWidget(parent) {
    build_ui();
    // Restyle and re-render once an appearance change has settled (never inside theme_changed).
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        inc_apply_qss(this);
        if (has_data_)
            render();
    });
    auto& svc = services::PortfolioService::instance();
    connect(&svc, &services::PortfolioService::ledger_changed, this, [this](const QString& pid) {
        if (has_data_ && pid == summary_.portfolio.id)
            reload();
    });
    connect(&svc, &services::PortfolioService::allocation_updated, this, [this](const QString& pid) {
        if (has_data_ && pid == summary_.portfolio.id)
            render();
    });
    // Currency / number format changed: re-format the payments and summary already held.
    ui::formatting::on_format_changed(this, [this]() {
        if (has_data_)
            render();
    });
}

void IncomeView::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    notice_ = new pfmt::PortfolioNoticeBar(this);
    root->addWidget(notice_);

    auto* bar = new QWidget(this);
    bar->setObjectName("incBar");
    auto* h = new QHBoxLayout(bar);
    h->setContentsMargins(10, 6, 10, 6);
    h->addWidget(new QLabel(tr("Monthly income:"), bar));
    span_cb_ = new QComboBox(bar);
    span_cb_->addItem(tr("Last 12 months"), 12);
    span_cb_->addItem(tr("Last 24 months"), 24);
    span_cb_->addItem(tr("Last 36 months"), 36);
    span_cb_->addItem(tr("All history"), 0);
    span_cb_->setCurrentIndex(1);
    h->addWidget(span_cb_);
    h->addStretch(1);
    export_btn_ = new QPushButton(tr("EXPORT CSV"), bar);
    export_btn_->setToolTip(tr("Every dividend / interest payment, local and base currency"));
    h->addWidget(export_btn_);
    root->addWidget(bar);

    kpis_ = new ui::KpiStrip(this);
    kpis_->set_min_tile_width(ui::metrics::width(140));
    for (const auto& [key, label] :
         {std::pair{"ttm", tr("TRAILING 12M")}, std::pair{"ytd", tr("YEAR TO DATE")}, std::pair{"all", tr("ALL TIME")},
          std::pair{"fwd", tr("FORWARD 12M")}, std::pair{"yield", tr("FORWARD YIELD")},
          std::pair{"yoc", tr("YIELD ON COST")}, std::pair{"wht", tr("TAX WITHHELD")}}) {
        auto* t = kpis_->add_tile(QString::fromLatin1(key), label);
        t->set_compact(true);
    }
    auto* kpi_wrap = new QWidget(this);
    auto* kv = new QVBoxLayout(kpi_wrap);
    kv->setContentsMargins(10, 6, 10, 6);
    kv->addWidget(kpis_);
    root->addWidget(kpi_wrap);

    auto* split = new QSplitter(Qt::Vertical, this);
    monthly_ = new ui::BarChart(split);
    monthly_->set_mode(ui::BarChart::Mode::Stacked);
    monthly_->setMinimumHeight(ui::metrics::height(170));
    split->addWidget(monthly_);

    auto* tabs = new QTabWidget(split);
    holdings_table_ = inc_table({tr("SYMBOL"), tr("RECEIVED 12M"), tr("RECEIVED ALL"), tr("ANNUAL RATE"), tr("CCY"),
                                 tr("FORWARD 12M"), tr("YIELD"), tr("YIELD ON COST"), tr("EX-DIVIDEND"),
                                 tr("EARNINGS")},
                                tabs);
    tabs->addTab(holdings_table_, tr("BY HOLDING"));
    upcoming_table_ = inc_table({tr("DATE"), tr("SYMBOL"), tr("EVENT"), tr("DETAIL")}, tabs);
    tabs->addTab(upcoming_table_, tr("UPCOMING"));
    payments_table_ = inc_table({tr("DATE"), tr("SYMBOL"), tr("TYPE"), tr("NET (LOCAL)"), tr("CCY"), tr("NET (BASE)"),
                                 tr("WITHHELD")},
                                tabs);
    tabs->addTab(payments_table_, tr("PAYMENTS"));
    split->addWidget(tabs);
    split->setStretchFactor(1, 2);
    root->addWidget(split, 1);

    connect(span_cb_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { render(); });
    connect(export_btn_, &QPushButton::clicked, this, &IncomeView::export_csv);
    inc_apply_qss(this);
}

void IncomeView::set_data(const portfolio::PortfolioSummary& summary, const QString&) {
    const bool new_book = summary.portfolio.id != summary_.portfolio.id;
    summary_ = summary;
    has_data_ = true;
    services::PortfolioService::instance().allocation(summary_); // requests market info not cached yet
    if (new_book || payments_.isEmpty())
        reload();
    else
        render();
}

void IncomeView::reload() {
    payments_.clear();
    unconverted_ = 0;
    const QString base = pfmt::base_ccy(summary_);
    auto& svc = services::PortfolioService::instance();
    const auto ledger = svc.ledger(summary_.portfolio.id);
    for (const auto& t : ledger) {
        const QString type = t.transaction_type.toUpper();
        if (type != portfolio::txn::kDividend && type != portfolio::txn::kInterest)
            continue;
        Payment p;
        p.date = portfolio::ledger_row_date(t);
        p.symbol = t.symbol;
        p.type = type;
        const QString ccy = !t.currency.isEmpty() ? t.currency
                            : type == portfolio::txn::kInterest ? base
                                                                : core::market::currency_for_symbol(t.symbol);
        const double gross = type == portfolio::txn::kDividend ? t.quantity * t.price : t.quantity;
        p.net_local = core::market::to_major_units(gross - t.taxes - t.fees, ccy);
        p.withheld_local = core::market::to_major_units(t.taxes, ccy);
        p.currency = core::market::major_currency(ccy.isEmpty() ? base : ccy);
        double fx = portfolio::kAbsent;
        if (t.fx_rate > 0)
            fx = t.fx_rate;
        else if (p.currency == base)
            fx = 1.0;
        else if (const auto h = svc.fx_on(p.currency, base, p.date))
            fx = *h; // the payment-date rate the ledger (ribbon "Income") converts with
        p.net_base = std::isfinite(fx) ? p.net_local * fx : portfolio::kAbsent;
        if (!std::isfinite(p.net_base))
            ++unconverted_;
        payments_.append(p);
    }
    std::sort(payments_.begin(), payments_.end(), [](const Payment& a, const Payment& b) { return a.date > b.date; });
    render();
}

void IncomeView::render() {
    if (!has_data_)
        return;
    const QString base = pfmt::base_ccy(summary_);
    const QDate today = QDate::currentDate();
    const QDate ttm_from = today.addYears(-1);
    auto& svc = services::PortfolioService::instance();

    // ── Received ─────────────────────────────────────────────────────────────
    double ttm = 0, ytd = 0, withheld = 0;
    QHash<QString, double> ttm_by_sym;
    for (const auto& p : payments_) {
        if (!std::isfinite(p.net_base))
            continue;
        if (p.date > ttm_from) {
            ttm += p.net_base;
            ttm_by_sym[p.symbol] += p.net_base;
        }
        if (p.date.year() == today.year())
            ytd += p.net_base;
        if (std::isfinite(p.net_base) && p.net_local != 0)
            withheld += p.withheld_local * (p.net_base / p.net_local);
    }

    // ── Forward (market info: annual dividend rate per share, major units) ──
    double forward = 0;
    int forward_known = 0;
    struct Row {
        const portfolio::HoldingWithQuote* h;
        std::optional<services::InfoData> info;
        double fwd = portfolio::kAbsent;
    };
    QVector<Row> rows;
    for (const auto& h : summary_.holdings) {
        Row r{&h, svc.cached_info(h.symbol)};
        if (r.info && std::isfinite(r.info->dividend_rate) && std::isfinite(h.fx_rate)) {
            r.fwd = r.info->dividend_rate * h.quantity * h.fx_rate;
            forward += r.fwd;
            ++forward_known;
        } else if (r.info && std::isfinite(r.info->dividend_yield) && r.info->dividend_yield == 0) {
            r.fwd = 0;
            ++forward_known;
        }
        rows.append(r);
    }
    const double hv = pfmt::holdings_value(summary_);
    auto tile = [this](const char* k) { return kpis_->tile(QString::fromLatin1(k)); };
    tile("ttm")->set_value(pfmt::money(ttm, base));
    tile("ttm")->set_sub_label(tr("net of withholding"));
    tile("ytd")->set_value(pfmt::money(ytd, base));
    tile("all")->set_value(pfmt::money(summary_.total_dividends + summary_.total_interest, base));
    tile("all")->set_sub_label(tr("dividends %1 · interest %2")
                                   .arg(pfmt::money_c(summary_.total_dividends, base),
                                        pfmt::money_c(summary_.total_interest, base)));
    tile("fwd")->set_value(forward_known > 0 ? pfmt::money(forward, base) : pfmt::dash());
    tile("fwd")->set_sub_label(tr("%1 of %2 holdings known").arg(forward_known).arg(summary_.holdings.size()));
    tile("fwd")->set_tooltip_text(tr("Annual dividend rate reported for each holding × units held × today's FX. An "
                                     "estimate: rates change and special dividends are not included."));
    tile("yield")->set_value(forward_known > 0 && hv > 0 ? pfmt::upct(forward / hv * 100.0, 2) : pfmt::dash());
    tile("yoc")->set_value(forward_known > 0 && summary_.total_cost_basis > 0
                               ? pfmt::upct(forward / summary_.total_cost_basis * 100.0, 2)
                               : pfmt::dash());
    tile("wht")->set_value(pfmt::money(withheld, base));
    tile("wht")->set_sub_label(tr("on recorded payments"));

    // ── Monthly chart ───────────────────────────────────────────────────────
    const int months = span_cb_->currentData().toInt();
    QDate first;
    if (months > 0) {
        first = QDate(today.year(), today.month(), 1).addMonths(-(months - 1));
    } else {
        first = QDate(today.year(), today.month(), 1);
        for (const auto& p : payments_)
            if (p.date.isValid() && p.date < first)
                first = QDate(p.date.year(), p.date.month(), 1);
    }
    QStringList cats;
    QVector<double> div_v, int_v;
    const QLocale loc = ui::formatting::FormatSettings::instance().locale();
    QMap<QDate, int> idx;
    for (QDate m = first; m <= today; m = m.addMonths(1)) {
        idx.insert(m, static_cast<int>(cats.size()));
        cats << loc.toString(m, QStringLiteral("MMM yy"));
        div_v << 0;
        int_v << 0;
    }
    for (const auto& p : payments_) {
        if (!std::isfinite(p.net_base) || !p.date.isValid())
            continue;
        const auto it = idx.constFind(QDate(p.date.year(), p.date.month(), 1));
        if (it == idx.constEnd())
            continue;
        (p.type == portfolio::txn::kInterest ? int_v : div_v)[*it] += p.net_base;
    }
    monthly_->set_categories(cats);
    monthly_->set_series({ui::BarSeries{tr("Dividends"), div_v, ui::colors::chart_color(0)},
                          ui::BarSeries{tr("Interest"), int_v, ui::colors::chart_color(1)}});
    monthly_->set_value_formatter([base](double v) { return pfmt::money_c(v, base); });
    monthly_->set_state(payments_.isEmpty() ? ui::PaintedChart::Empty : ui::PaintedChart::Ready,
                        tr("No dividends or interest recorded yet — use DIV or CASH › Interest."));

    // ── By holding ──────────────────────────────────────────────────────────
    holdings_table_->setSortingEnabled(false);
    holdings_table_->setRowCount(static_cast<int>(rows.size()));
    for (int i = 0; i < rows.size(); ++i) {
        const auto& r = rows[i];
        const auto& h = *r.h;
        const double rate = r.info ? r.info->dividend_rate : portfolio::kAbsent;
        const double yld = r.info ? r.info->dividend_yield : portfolio::kAbsent;
        const double yoc = std::isfinite(r.fwd) && h.cost_basis > 0 ? r.fwd / h.cost_basis * 100.0 : portfolio::kAbsent;
        holdings_table_->setItem(i, 0, inc_text(h.symbol, QColor(ui::colors::CYAN())));
        const double rec12 = ttm_by_sym.value(h.symbol, 0.0);
        holdings_table_->setItem(i, 1, inc_num(pfmt::money(rec12, base), rec12));
        holdings_table_->setItem(i, 2, inc_num(pfmt::money(h.dividends, base), h.dividends));
        holdings_table_->setItem(i, 3, inc_num(std::isfinite(rate) ? pfmt::money(rate, h.currency) : pfmt::dash(), rate));
        holdings_table_->setItem(i, 4, inc_text(h.currency, QColor(ui::colors::TEXT_SECONDARY())));
        holdings_table_->setItem(i, 5, inc_num(pfmt::money(r.fwd, base), r.fwd));
        holdings_table_->setItem(i, 6, inc_num(std::isfinite(yld) ? pfmt::upct(yld * 100.0, 2) : pfmt::dash(), yld));
        holdings_table_->setItem(i, 7, inc_num(std::isfinite(yoc) ? pfmt::upct(yoc, 2) : pfmt::dash(), yoc));
        holdings_table_->setItem(i, 8, inc_date(r.info ? r.info->ex_dividend_date : QDate()));
        holdings_table_->setItem(i, 9, inc_date(r.info ? r.info->earnings_date : QDate()));
    }
    holdings_table_->setSortingEnabled(true);
    holdings_table_->resizeColumnsToContents();

    // ── Upcoming ────────────────────────────────────────────────────────────
    struct Ev {
        QDate d;
        QString sym, ev, detail;
    };
    QVector<Ev> evs;
    for (const auto& r : rows) {
        if (!r.info)
            continue;
        if (r.info->ex_dividend_date.isValid() && r.info->ex_dividend_date >= today)
            evs.append({r.info->ex_dividend_date, r.h->symbol, tr("Ex-dividend"),
                        std::isfinite(r.info->dividend_rate)
                            ? tr("annual rate %1 per share; this payment's amount is not published here")
                                  .arg(pfmt::money(r.info->dividend_rate, r.h->currency))
                            : QString()});
        if (r.info->earnings_date.isValid() && r.info->earnings_date >= today)
            evs.append({r.info->earnings_date, r.h->symbol, tr("Earnings"), QString()});
    }
    std::sort(evs.begin(), evs.end(), [](const Ev& a, const Ev& b) { return a.d < b.d; });
    upcoming_table_->setSortingEnabled(false);
    upcoming_table_->setRowCount(static_cast<int>(evs.size()));
    for (int i = 0; i < evs.size(); ++i) {
        upcoming_table_->setItem(i, 0, inc_date(evs[i].d));
        upcoming_table_->setItem(i, 1, inc_text(evs[i].sym, QColor(ui::colors::CYAN())));
        upcoming_table_->setItem(i, 2, inc_text(evs[i].ev));
        upcoming_table_->setItem(i, 3, inc_text(evs[i].detail, QColor(ui::colors::TEXT_SECONDARY())));
    }
    upcoming_table_->setSortingEnabled(true);
    upcoming_table_->resizeColumnsToContents();

    // ── Payments ────────────────────────────────────────────────────────────
    payments_table_->setSortingEnabled(false);
    payments_table_->setRowCount(static_cast<int>(payments_.size()));
    for (int i = 0; i < payments_.size(); ++i) {
        const auto& p = payments_[i];
        payments_table_->setItem(i, 0, inc_date(p.date));
        payments_table_->setItem(i, 1, inc_text(p.symbol.isEmpty() ? pfmt::dash() : p.symbol));
        payments_table_->setItem(i, 2, inc_text(p.type == portfolio::txn::kInterest ? tr("INTEREST") : tr("DIVIDEND"),
                                                QColor(ui::colors::CYAN())));
        payments_table_->setItem(i, 3, inc_num(pfmt::money(p.net_local, p.currency), p.net_local));
        payments_table_->setItem(i, 4, inc_text(p.currency, QColor(ui::colors::TEXT_SECONDARY())));
        auto* nb = inc_num(pfmt::money(p.net_base, base), p.net_base);
        if (!std::isfinite(p.net_base))
            nb->setToolTip(tr("No FX rate recorded on the row — edit it to add the rate of the payment date."));
        payments_table_->setItem(i, 5, nb);
        payments_table_->setItem(i, 6, inc_num(p.withheld_local > 0 ? pfmt::money(p.withheld_local, p.currency) : QString(),
                                               p.withheld_local));
    }
    payments_table_->setSortingEnabled(true);
    payments_table_->resizeColumnsToContents();

    QStringList notes;
    if (unconverted_ > 0)
        notes << tr("%n payment(s) in a foreign currency have no FX rate recorded and are left out of the base-currency "
                    "totals (edit the row to add the rate).",
                    "", unconverted_);
    notes << tr("Ex-dividend and earnings dates come from market data; payment dates are not available.");
    notice_->set_notice(unconverted_ > 0 ? pfmt::PortfolioNoticeBar::Kind::Warning : pfmt::PortfolioNoticeBar::Kind::Info,
                        notes.join(QLatin1Char(' ')));
}

void IncomeView::export_csv() {
    if (payments_.isEmpty()) {
        QMessageBox::information(this, tr("Export"), tr("No dividends or interest recorded yet."));
        return;
    }
    const QString base = pfmt::base_ccy(summary_);
    QVector<QStringList> rows;
    for (const auto& p : payments_)
        rows.append({p.date.toString(Qt::ISODate), p.symbol, p.type, ui::formatting::export_number(p.net_local, 4),
                     p.currency, ui::formatting::export_number(p.net_base, 4), // NaN (no FX) -> ""
                     ui::formatting::export_number(p.withheld_local, 4)});
    ui::ChartExport::save_csv({"date", "symbol", "type", "net_local", "currency", "net_" + base, "withheld_local"}, rows,
                              QStringLiteral("income_%1").arg(QDate::currentDate().toString(Qt::ISODate)), this);
}

void IncomeView::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange && has_data_)
        render();
    QWidget::changeEvent(event);
}

} // namespace fincept::screens
