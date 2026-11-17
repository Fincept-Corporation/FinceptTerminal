// src/screens/equity_research/EquityOwnershipTab.cpp
#include "screens/equity_research/EquityOwnershipTab.h"

#include "screens/equity_research/EquityResearchUi.h"
#include "ui/charts/BarChart.h"
#include "ui/charts/DonutChart.h"
#include "ui/tables/BarCellDelegate.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QHBoxLayout>
#include <QLocale>
#include <QMap>

#include "screens/equity_research/EquityResearchFormat.h"
#include "services/fincept_data/FinceptDataService.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens {

namespace fd = services::fincept_data;

namespace equity_ownership_tab_detail {

/// Insider transactions asked for (newest first; the server keeps up to 200).
constexpr int kTransactionsLimit = 50;

enum OwnershipStat {
    kInsiders, kInstitutions, kInstitutionsFloat, kInstitutionsCount,
    kBuys, kSells, kNetShares, kNetPct,
    kOwnershipStatCount
};

} // namespace equity_ownership_tab_detail

using namespace equity_ownership_tab_detail;

EquityOwnershipTab::EquityOwnershipTab(QWidget* parent) : EquityDataTab(QStringLiteral("equityOwnershipTab"), parent) {
    build_ui();
    retranslate();
}

EquityOwnershipTab::Stat EquityOwnershipTab::make_stat(QWidget* parent, QGridLayout* grid, int row, int column) {
    Stat s;
    s.caption = new QLabel(parent);
    s.caption->setObjectName(QStringLiteral("fdStatCaption"));
    s.value = new QLabel(equity_fmt::dash(), parent);
    s.value->setObjectName(QStringLiteral("fdStatValue"));
    grid->addWidget(s.caption, row * 2, column);
    grid->addWidget(s.value, row * 2 + 1, column);
    return s;
}

void EquityOwnershipTab::build_ui() {
    auto* scroll = new QScrollArea(body_widget());
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("fdScrollContent"));
    content->setStyleSheet(QStringLiteral("#fdScrollContent { background:transparent; }"));
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(0, 0, 4, 0);
    vl->setSpacing(6);

    sec_breakdown_ = make_section(QString(), content);
    vl->addWidget(sec_breakdown_);
    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(28);
    grid->setVerticalSpacing(2);
    for (int i = 0; i < kOwnershipStatCount; ++i)
        stats_.append(make_stat(content, grid, i / 4, i % 4));
    grid->setColumnStretch(4, 1);
    // Who owns the company (donut) next to the stats, and insider net buying by month.
    auto* top = new QHBoxLayout;
    top->setSpacing(10);
    top->addLayout(grid, 1);
    holder_donut_ = new ui::DonutChart(content);
    holder_donut_->setMinimumSize(ui::metrics::width(300), ui::metrics::height(170));
    holder_donut_->set_legend_position(ui::DonutChart::LegendPosition::Right);
    top->addWidget(holder_donut_);
    vl->addLayout(top);
    insider_chart_ = new ui::BarChart(content);
    insider_chart_->set_signed_colors(true);
    insider_chart_->set_show_legend(true);
    insider_chart_->setMinimumHeight(ui::metrics::height(170));
    vl->addWidget(insider_chart_);

    auto add_table = [&](QLabel*& section, QTableWidget*& table) {
        section = make_section(QString(), content);
        vl->addWidget(section);
        table = make_table(content);
        vl->addWidget(table);
    };
    add_table(sec_institutions_, institutions_table_);
    add_table(sec_funds_, funds_table_);
    holders_note_ = make_note(content);
    vl->addWidget(holders_note_);
    add_table(sec_transactions_, transactions_table_);
    transactions_note_ = make_note(content);
    vl->addWidget(transactions_note_);
    add_table(sec_roster_, roster_table_);
    vl->addStretch(1);

    scroll->setWidget(content);
    body_layout()->addWidget(scroll, 1);
}

void EquityOwnershipTab::retranslate() {
    sec_breakdown_->setText(tr("OWNERSHIP BREAKDOWN"));
    sec_institutions_->setText(tr("TOP INSTITUTIONAL HOLDERS"));
    sec_funds_->setText(tr("TOP FUND HOLDERS"));
    sec_transactions_->setText(tr("INSIDER TRANSACTIONS"));
    sec_roster_->setText(tr("INSIDER ROSTER"));
    holders_note_->setText(tr("Positions as of each holder's latest 13F / fund filing (REPORTED)."));
    populate();
}

QString EquityOwnershipTab::loading_text() const {
    return tr("LOADING OWNERSHIP…");
}

QString EquityOwnershipTab::not_found_text(const fd::DataError& e) const {
    return e.message.isEmpty() ? tr("No ownership data found for %1.").arg(symbol()) : e.message;
}

void EquityOwnershipTab::clear_view() {
    data_ = {};
    loaded_ = false;
    populate();
}

void EquityOwnershipTab::request(bool refresh) {
    const quint64 token = start_request();
    fd::FinceptDataService::instance().fetch_ownership(
        symbol(), kTransactionsLimit, this,
        [this, token](const fd::DataResult& r) {
            if (!is_current(token))
                return; // the user moved to another symbol meanwhile
            finish(r);
            if (!r.ok())
                return;
            data_ = fd::parse_ownership(r.data);
            loaded_ = true;
            populate();
        },
        refresh);
}

bool EquityOwnershipTab::apply_remembered(const fd::DataResult& r) {
    data_ = fd::parse_ownership(r.data);
    loaded_ = true;
    populate();
    return true;
}

void EquityOwnershipTab::populate() {
    populate_stats();
    populate_holders(institutions_table_, data_.institutional);
    populate_holders(funds_table_, data_.funds);
    populate_transactions();
    populate_roster();
}

void EquityOwnershipTab::populate_stats() {
    const auto& a = data_.activity;
    const QString period = a.present && !a.period.isEmpty() ? a.period.toUpper() : QString();
    auto with_period = [&](const QString& caption) {
        return period.isEmpty() ? caption : QStringLiteral("%1 (%2)").arg(caption, period);
    };
    stats_[kInsiders].caption->setText(tr("INSIDERS"));
    stats_[kInstitutions].caption->setText(tr("INSTITUTIONS"));
    stats_[kInstitutionsFloat].caption->setText(tr("INSTITUTIONS, OF FLOAT"));
    stats_[kInstitutionsCount].caption->setText(tr("INSTITUTIONAL HOLDERS"));
    stats_[kBuys].caption->setText(with_period(tr("INSIDER BUYS")));
    stats_[kSells].caption->setText(with_period(tr("INSIDER SELLS")));
    stats_[kNetShares].caption->setText(with_period(tr("NET INSIDER SHARES")));
    stats_[kNetPct].caption->setText(with_period(tr("NET, OF INSIDER SHARES")));

    stats_[kInsiders].value->setText(percent(data_.insiders_pct, 2));
    stats_[kInstitutions].value->setText(percent(data_.institutions_pct, 2));
    stats_[kInstitutionsFloat].value->setText(percent(data_.institutions_float_pct, 2));
    stats_[kInstitutionsCount].value->setText(count(data_.institutions_count));
    auto trades = [](double n, double shares) {
        if (!fd::has(n))
            return equity_fmt::dash();
        return fd::has(shares) ? tr("%1 · %2 sh").arg(EquityDataTab::count(n), EquityDataTab::big(shares))
                               : EquityDataTab::count(n);
    };
    stats_[kBuys].value->setText(trades(a.buy_count, a.buy_shares));
    stats_[kSells].value->setText(trades(a.sell_count, a.sell_shares));
    const QString net = fd::has(a.net_shares) ? (a.net_shares > 0 ? QStringLiteral("+") : QString()) + big(a.net_shares)
                                              : equity_fmt::dash();
    stats_[kNetShares].value->setText(net);
    stats_[kNetShares].value->setStyleSheet(
        fd::has(a.net_shares) ? QStringLiteral("color:%1;").arg(sign_color(a.net_shares).name()) : QString());
    stats_[kNetPct].value->setText(percent(a.net_pct_insider_shares, 2, true));
    stats_[kNetShares].caption->setToolTip(tr("Shares bought minus shares sold by insiders over the period"));
    stats_[kInstitutionsFloat].caption->setToolTip(tr("Institutional holdings as a share of the free float"));

    // Holder mix (percent of shares outstanding).
    QVector<ui::DonutSlice> slices;
    const double ins = fd::has(data_.insiders_pct) ? data_.insiders_pct : 0.0;
    const double inst = fd::has(data_.institutions_pct) ? data_.institutions_pct : 0.0;
    if (ins > 0 || inst > 0) {
        slices.append({QStringLiteral("insiders"), tr("Insiders"), ins, {}});
        slices.append({QStringLiteral("institutions"), tr("Institutions"), inst, {}});
        slices.append({QStringLiteral("other"), tr("Public & other"), std::max(0.0, 100.0 - ins - inst), {}});
    }
    holder_donut_->set_slices(slices);
    holder_donut_->set_value_formatter([](double v) { return EquityDataTab::percent(v, 1); });
    holder_donut_->set_center_text(tr("HOLDERS"), fd::has(data_.institutions_count) ? count(data_.institutions_count)
                                                                                     : QString());
    holder_donut_->set_state(slices.isEmpty() ? ui::PaintedChart::Empty : ui::PaintedChart::Ready,
                             loaded_ ? tr("No ownership split reported") : QString());
}

void EquityOwnershipTab::populate_holders(QTableWidget* table, const QVector<fd::Holder>& holders) {
    reset_table(table, {tr("HOLDER"), tr("REPORTED"), tr("% HELD"), tr("SHARES"), tr("VALUE"), tr("% CHANGE")},
                static_cast<int>(holders.size()));
    for (int r = 0; r < holders.size(); ++r) {
        const auto& h = holders[r];
        table->setItem(r, 0, text_item(h.holder));
        table->setItem(r, 1, date_item(h.report_date));
        auto* held = number_item(h.pct_held, percent(h.pct_held, 2));
        held->setData(er_ui::kRawRole, fd::has(h.pct_held) ? QVariant(h.pct_held) : QVariant(QString()));
        table->setItem(r, 2, held);
        table->setItem(r, 3, number_item(h.shares, count(h.shares)));
        table->setItem(r, 4, number_item(h.value, big(h.value)));
        auto* chg = number_item(h.pct_change, percent(h.pct_change, 2, true), sign_color(h.pct_change));
        chg->setData(er_ui::kRawRole, fd::has(h.pct_change) ? QVariant(h.pct_change) : QVariant(QString()));
        table->setItem(r, 5, chg);
    }
    // In-cell bars: concentration (% held) and position changes (signed).
    if (!table->property("er_bars").toBool()) {
        auto* held_bar = new ui::BarCellDelegate(ui::BarCellDelegate::Mode::Bar, table);
        held_bar->set_value_role(er_ui::kRawRole);
        table->setItemDelegateForColumn(2, held_bar);
        auto* chg_bar = new ui::BarCellDelegate(ui::BarCellDelegate::Mode::SignedBar, table);
        chg_bar->set_value_role(er_ui::kRawRole);
        table->setItemDelegateForColumn(5, chg_bar);
        table->setProperty("er_bars", true);
    }
    table->resizeColumnsToContents();
    table->setSortingEnabled(true);
    table->sortByColumn(2, Qt::DescendingOrder); // largest holders first
    fit_table_height(table);
}

void EquityOwnershipTab::populate_transactions() {
    reset_table(transactions_table_,
                {tr("DATE"), tr("INSIDER"), tr("RELATION"), tr("TRANSACTION"), tr("SHARES"), tr("VALUE"),
                 tr("OWNERSHIP")},
                static_cast<int>(data_.transactions.size()));
    for (int r = 0; r < data_.transactions.size(); ++r) {
        const auto& t = data_.transactions[r];
        // Yahoo's text says what happened ("Sale at price …", "Stock Gift"): colour sales / purchases.
        QColor fg;
        if (t.transaction.startsWith(QLatin1String("Sale"), Qt::CaseInsensitive))
            fg = QColor(ui::colors::NEGATIVE());
        else if (t.transaction.startsWith(QLatin1String("Purchase"), Qt::CaseInsensitive) ||
                 t.transaction.startsWith(QLatin1String("Buy"), Qt::CaseInsensitive))
            fg = QColor(ui::colors::POSITIVE());
        transactions_table_->setItem(r, 0, date_item(t.date));
        transactions_table_->setItem(r, 1, text_item(t.insider));
        transactions_table_->setItem(r, 2, text_item(t.relation));
        transactions_table_->setItem(r, 3, text_item(t.transaction, fg));
        transactions_table_->setItem(r, 4, number_item(t.shares, count(t.shares)));
        transactions_table_->setItem(r, 5, number_item(t.value, big(t.value)));
        transactions_table_->setItem(r, 6, text_item(t.ownership));
    }
    transactions_table_->resizeColumnsToContents();
    transactions_table_->setSortingEnabled(true);
    transactions_table_->sortByColumn(0, Qt::DescendingOrder);
    fit_table_height(transactions_table_);
    // Net insider shares by month (purchases − sales), oldest on the left.
    QMap<QString, double> by_month;
    for (const auto& t : data_.transactions) {
        if (!t.date.isValid() || !fd::has(t.shares))
            continue;
        double sign = 0.0;
        if (t.transaction.startsWith(QLatin1String("Sale"), Qt::CaseInsensitive))
            sign = -1.0;
        else if (t.transaction.startsWith(QLatin1String("Purchase"), Qt::CaseInsensitive) ||
                 t.transaction.startsWith(QLatin1String("Buy"), Qt::CaseInsensitive))
            sign = 1.0;
        if (sign == 0.0)
            continue; // gifts, awards and option exercises are not market trades
        by_month[t.date.toString(QStringLiteral("yyyy-MM"))] += sign * t.shares;
    }
    QStringList months;
    ui::BarSeries net;
    // Not the NET INSIDER SHARES figure above (Yahoo's summary also counts awards and
    // option exercises as buys): only open-market purchases and sales — say so in the legend.
    net.name = tr("Open-market insider trades by month — net shares (purchases − sales)");
    for (auto it = by_month.cbegin(); it != by_month.cend(); ++it) {
        months << QLocale().toString(QDate::fromString(it.key() + QStringLiteral("-01"), Qt::ISODate), QStringLiteral("MMM yy"));
        net.values << it.value();
    }
    insider_chart_->set_categories(months);
    insider_chart_->set_series({net});
    insider_chart_->set_value_formatter([](double v) { return EquityDataTab::big(v); });
    insider_chart_->set_state(months.isEmpty() ? ui::PaintedChart::Empty : ui::PaintedChart::Ready,
                              loaded_ ? tr("No insider purchases or sales on record") : QString());

    if (!loaded_)
        transactions_note_->clear();
    else if (data_.transactions.isEmpty())
        transactions_note_->setText(tr("No insider transactions reported."));
    else
        transactions_note_->setText(tr("Newest %1 of %2 insider transactions on record.")
                                        .arg(data_.transactions.size())
                                        .arg(data_.transactions_total));
}

void EquityOwnershipTab::populate_roster() {
    reset_table(roster_table_,
                {tr("NAME"), tr("RELATION"), tr("LATEST TRANSACTION"), tr("DATE"), tr("SHARES (DIRECT)"),
                 tr("SHARES (INDIRECT)")},
                static_cast<int>(data_.roster.size()));
    for (int r = 0; r < data_.roster.size(); ++r) {
        const auto& e = data_.roster[r];
        roster_table_->setItem(r, 0, text_item(e.name));
        roster_table_->setItem(r, 1, text_item(e.relation));
        roster_table_->setItem(r, 2, text_item(e.latest_transaction));
        roster_table_->setItem(r, 3, date_item(e.latest_transaction_date));
        roster_table_->setItem(r, 4, number_item(e.shares_direct, count(e.shares_direct)));
        roster_table_->setItem(r, 5, number_item(e.shares_indirect, count(e.shares_indirect)));
    }
    roster_table_->resizeColumnsToContents();
    roster_table_->setSortingEnabled(true);
    roster_table_->sortByColumn(0, Qt::AscendingOrder);
    fit_table_height(roster_table_);
}

} // namespace fincept::screens
