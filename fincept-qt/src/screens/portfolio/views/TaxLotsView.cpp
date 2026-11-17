// src/screens/portfolio/views/TaxLotsView.cpp
#include "screens/portfolio/views/TaxLotsView.h"

#include "screens/portfolio/PortfolioFormat.h"
#include "services/portfolio/PortfolioService.h"
#include "ui/charts/BarChart.h"
#include "ui/charts/ChartExport.h"
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
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <cmath>

namespace fincept::screens {

namespace {

QTableWidgetItem* tl_num(const QString& text, double v, const QColor& fg = {}) {
    auto* it = new ui::NumericTableWidgetItem(text, std::isfinite(v) ? v : -1e300);
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    if (fg.isValid())
        it->setForeground(fg);
    return it;
}

QTableWidgetItem* tl_text(const QString& text, const QColor& fg = {}) {
    auto* it = new QTableWidgetItem(text);
    if (fg.isValid())
        it->setForeground(fg);
    return it;
}

QTableWidgetItem* tl_date(const QDate& d) {
    auto* it = new ui::NumericTableWidgetItem(pfmt::date(d, ui::formatting::DateStyle::Short),
                                              d.isValid() ? double(d.toJulianDay()) : -1e300);
    return it;
}

QTableWidget* tl_table(const QStringList& headers, QWidget* parent) {
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

QString tl_csv(double v, int dp = 4) {
    return ui::formatting::export_number(v, dp); // NaN -> ""
}

void tl_apply_qss(QWidget* w) {
    // Set only when the text changed (each set re-polishes the whole view).
    const QString css = QString("#tlBar { background:%1; border-bottom:1px solid %2; }"
                             "#tlBar QLabel { color:%3; font-size:%4px; }"
                             "QComboBox, QSpinBox { background:%5; color:%6; border:1px solid %2; padding:2px 6px; }"
                             "QPushButton { background:transparent; color:%7; border:1px solid %7; padding:3px 10px;"
                             "  font-size:%4px; font-weight:700; }"
                             "QPushButton:disabled { color:%10; border-color:%2; }"
                             "QTableWidget { background:%8; color:%6; border:none; font-size:%9px; }")
                         .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::TEXT_SECONDARY(),
                              ui::fonts::px(-3), ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(),
                              ui::colors::AMBER(), ui::colors::BG_BASE(), ui::fonts::px(-2))
                         .arg(ui::colors::TEXT_TERTIARY());
    if (w->styleSheet() != css)
        w->setStyleSheet(css);
}

} // namespace

TaxLotsView::TaxLotsView(QWidget* parent) : QWidget(parent) {
    build_ui();
    // Restyle and re-render once an appearance change has settled (never inside theme_changed).
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        tl_apply_qss(this);
        if (has_data_)
            render();
    });
    // A ledger edit changes lots and gains.
    connect(&services::PortfolioService::instance(), &services::PortfolioService::ledger_changed, this,
            [this](const QString& pid) {
                if (has_data_ && pid == summary_.portfolio.id) {
                    ledger_for_.clear();
                    recompute();
                }
            });
    // Currency / number format changed: re-format the replayed lots already held.
    ui::formatting::on_format_changed(this, [this]() {
        if (has_data_)
            render();
    });
}

void TaxLotsView::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    notice_ = new pfmt::PortfolioNoticeBar(this);
    root->addWidget(notice_);

    auto* bar = new QWidget(this);
    bar->setObjectName("tlBar");
    auto* h = new QHBoxLayout(bar);
    h->setContentsMargins(10, 6, 10, 6);
    h->setSpacing(8);
    h->addWidget(new QLabel(tr("Relief method:"), bar));
    method_cb_ = new QComboBox(bar);
    method_cb_->addItem(tr("FIFO"), static_cast<int>(portfolio::LotMethod::Fifo));
    method_cb_->addItem(tr("LIFO"), static_cast<int>(portfolio::LotMethod::Lifo));
    method_cb_->addItem(tr("HIFO"), static_cast<int>(portfolio::LotMethod::Hifo));
    method_cb_->addItem(tr("Average cost"), static_cast<int>(portfolio::LotMethod::AverageCost));
    method_cb_->addItem(tr("Specific lot"), static_cast<int>(portfolio::LotMethod::SpecificLot));
    method_cb_->setToolTip(tr("Replays the ledger with this method — a comparison, nothing is saved"));
    h->addWidget(method_cb_);
    h->addWidget(new QLabel(tr("Long-term after:"), bar));
    lt_spin_ = new QSpinBox(bar);
    lt_spin_->setRange(1, 36500);
    lt_spin_->setSuffix(tr(" days"));
    h->addWidget(lt_spin_);
    h->addWidget(new QLabel(tr("Tax year:"), bar));
    year_cb_ = new QComboBox(bar);
    h->addWidget(year_cb_);
    h->addStretch(1);
    apply_btn_ = new QPushButton(tr("USE AS PORTFOLIO METHOD"), bar);
    apply_btn_->setToolTip(tr("Save this method and threshold in the portfolio's settings"));
    h->addWidget(apply_btn_);
    export_btn_ = new QPushButton(tr("EXPORT CSV"), bar);
    h->addWidget(export_btn_);
    root->addWidget(bar);

    kpis_ = new ui::KpiStrip(this);
    kpis_->set_min_tile_width(ui::metrics::width(150));
    for (const auto& [key, label] : {std::pair{"realised", tr("REALISED")}, std::pair{"st", tr("SHORT-TERM")},
                                     std::pair{"lt", tr("LONG-TERM")}, std::pair{"unreal_st", tr("UNREALISED ST")},
                                     std::pair{"unreal_lt", tr("UNREALISED LT")}, std::pair{"lots", tr("OPEN LOTS")}}) {
        auto* t = kpis_->add_tile(QString::fromLatin1(key), label);
        t->set_compact(true);
    }
    auto* kpi_wrap = new QWidget(this);
    auto* kv = new QVBoxLayout(kpi_wrap);
    kv->setContentsMargins(10, 6, 10, 6);
    kv->addWidget(kpis_);
    root->addWidget(kpi_wrap);

    tabs_ = new QTabWidget(this);
    open_table_ = tl_table({tr("SYMBOL"), tr("OPENED"), tr("QTY"), tr("UNIT COST"), tr("CCY"), tr("COST"), tr("VALUE"),
                            tr("UNREALISED"), tr("DAYS"), tr("TERM"), tr("FX AT PURCHASE")},
                           this);
    tabs_->addTab(open_table_, tr("OPEN LOTS"));

    auto* realised_page = new QSplitter(Qt::Vertical, this);
    year_chart_ = new ui::BarChart(realised_page);
    year_chart_->set_mode(ui::BarChart::Mode::Stacked);
    year_chart_->set_show_legend(true);
    year_chart_->setMinimumHeight(ui::metrics::height(160));
    realised_page->addWidget(year_chart_);
    realised_table_ = tl_table({tr("SOLD"), tr("SYMBOL"), tr("OPENED"), tr("QTY"), tr("PROCEEDS"), tr("COST"),
                                tr("GAIN"), tr("GAIN (LOCAL)"), tr("CCY"), tr("DAYS"), tr("TERM")},
                               realised_page);
    realised_page->addWidget(realised_table_);
    realised_page->setStretchFactor(1, 2);
    tabs_->addTab(realised_page, tr("REALISED GAINS"));
    root->addWidget(tabs_, 1);

    connect(method_cb_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { recompute(); });
    connect(lt_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { recompute(); });
    connect(year_cb_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { render_realised(); });
    connect(export_btn_, &QPushButton::clicked, this, &TaxLotsView::export_csv);
    connect(apply_btn_, &QPushButton::clicked, this, [this]() {
        const auto& p = summary_.portfolio;
        const QString method = portfolio::lot_method_key(selected_method());
        const auto answer = QMessageBox::question(
            this, tr("Tax-lot Method"),
            tr("Use %1 with a %2-day long-term threshold for %3?\n\nRealised gains and lots are recomputed from the "
               "ledger; no transaction is changed.")
                .arg(method)
                .arg(lt_spin_->value())
                .arg(p.name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
        services::PortfolioService::instance().update_portfolio_settings(p.id, p.benchmark, p.risk_free_override,
                                                                         p.track_cash, method, lt_spin_->value());
    });

    tl_apply_qss(this);
}

portfolio::LotMethod TaxLotsView::selected_method() const {
    return static_cast<portfolio::LotMethod>(method_cb_->currentData().toInt());
}

void TaxLotsView::set_data(const portfolio::PortfolioSummary& summary, const QString&) {
    const bool new_book = summary.portfolio.id != summary_.portfolio.id;
    summary_ = summary;
    has_data_ = true;
    if (new_book) {
        QSignalBlocker b1(method_cb_);
        QSignalBlocker b2(lt_spin_);
        const int idx = method_cb_->findData(static_cast<int>(portfolio::lot_method_from_key(summary.portfolio.lot_method)));
        method_cb_->setCurrentIndex(idx >= 0 ? idx : 0);
        lt_spin_->setValue(summary.portfolio.long_term_days > 0 ? summary.portfolio.long_term_days : 365);
        ledger_for_.clear();
    }
    recompute();
}

void TaxLotsView::recompute() {
    if (!has_data_ || summary_.portfolio.id.isEmpty())
        return;
    const QString key = QStringLiteral("%1|%2|%3")
                            .arg(summary_.portfolio.id)
                            .arg(static_cast<int>(selected_method()))
                            .arg(lt_spin_->value());
    if (key != ledger_for_) {
        ledger_ = services::PortfolioService::instance().replay_ledger(summary_.portfolio.id, selected_method(),
                                                                       lt_spin_->value());
        ledger_for_ = key;
        // Tax years present in the realised lots.
        QSignalBlocker block(year_cb_);
        const QString keep = year_cb_->currentData().toString();
        year_cb_->clear();
        year_cb_->addItem(tr("All years"), QString());
        QMap<int, bool> years;
        for (const auto& r : ledger_.realized)
            if (r.close_date.isValid())
                years.insert(r.close_date.year(), true);
        for (auto it = years.constEnd(); it != years.constBegin();) {
            --it;
            year_cb_->addItem(QString::number(it.key()), QString::number(it.key()));
        }
        const int k = year_cb_->findData(keep);
        year_cb_->setCurrentIndex(k >= 0 ? k : 0);
    }
    const bool is_setting = portfolio::lot_method_key(selected_method()) ==
                                portfolio::lot_method_key(portfolio::lot_method_from_key(summary_.portfolio.lot_method)) &&
                            lt_spin_->value() == summary_.portfolio.long_term_days;
    apply_btn_->setEnabled(!is_setting);
    render();
}

void TaxLotsView::render() {
    const QString base = pfmt::base_ccy(summary_);
    QStringList notes;
    if (!ledger_.realized_base_complete)
        notes << tr("Some sales have no FX rate for the purchase or sale date: their base-currency gain is unknown "
                    "and left out of the totals.");
    if (!ledger_.warnings.isEmpty())
        notes << ledger_.warnings.mid(0, 4);
    if (ledger_.warnings.size() > 4)
        notes << tr("… and %1 more ledger notes").arg(ledger_.warnings.size() - 4);
    notice_->set_notice(notes.isEmpty() ? pfmt::PortfolioNoticeBar::Kind::Hidden : pfmt::PortfolioNoticeBar::Kind::Warning,
                        notes.join(QLatin1Char('\n')));
    render_open_lots();
    render_realised();
}

void TaxLotsView::render_open_lots() {
    const QString base = pfmt::base_ccy(summary_);
    QHash<QString, const portfolio::HoldingWithQuote*> by_sym;
    for (const auto& h : summary_.holdings)
        by_sym.insert(h.symbol, &h);

    open_table_->setSortingEnabled(false);
    open_table_->setRowCount(0);
    double unreal_st = 0, unreal_lt = 0;
    bool unreal_partial = false;
    int lots = 0;
    for (auto it = ledger_.positions.constBegin(); it != ledger_.positions.constEnd(); ++it) {
        const auto& pos = it.value();
        const auto* h = by_sym.value(pos.symbol, nullptr);
        const double unit_value = h && h->valued() && h->quantity > 0 ? h->market_value / h->quantity : portfolio::kAbsent;
        for (const auto& lot : pos.lots) {
            if (!(lot.quantity > 0))
                continue;
            ++lots;
            const int r = open_table_->rowCount();
            open_table_->insertRow(r);
            const double value = std::isfinite(unit_value) ? lot.quantity * unit_value : portfolio::kAbsent;
            const double unreal = std::isfinite(value) && std::isfinite(lot.cost_base) ? value - lot.cost_base
                                                                                       : portfolio::kAbsent;
            if (std::isfinite(unreal))
                (lot.long_term ? unreal_lt : unreal_st) += unreal;
            else
                unreal_partial = true;
            open_table_->setItem(r, 0, tl_text(lot.symbol, QColor(ui::colors::CYAN())));
            open_table_->setItem(r, 1, tl_date(lot.open_date));
            open_table_->setItem(r, 2, tl_num(pfmt::qty(lot.quantity), lot.quantity));
            // Cost per unit is a price: in the holding's quote unit (pence stay pence), at price precision.
            const QString qccy = h && !h->price_currency.isEmpty() &&
                                         ui::formatting::major_currency(h->price_currency) == lot.currency
                                     ? h->price_currency
                                     : lot.currency;
            const double unit_cost = lot.unit_cost_local() * ui::formatting::minor_unit_divisor(qccy);
            open_table_->setItem(r, 3, tl_num(pfmt::price_sym(unit_cost, qccy, lot.symbol), lot.unit_cost_local()));
            open_table_->setItem(r, 4, tl_text(lot.currency, QColor(ui::colors::TEXT_SECONDARY())));
            open_table_->setItem(r, 5, tl_num(pfmt::money(lot.cost_base, base), lot.cost_base));
            open_table_->setItem(r, 6, tl_num(pfmt::money(value, base), value));
            open_table_->setItem(r, 7, tl_num(pfmt::smoney(unreal, base), unreal, pfmt::dir_color(unreal)));
            open_table_->setItem(r, 8, tl_num(QString::number(lot.holding_days), lot.holding_days));
            open_table_->setItem(r, 9, tl_text(lot.long_term ? tr("LONG") : tr("SHORT"),
                                               QColor(lot.long_term ? ui::colors::INFO() : ui::colors::TEXT_SECONDARY())));
            QString fx_text = pfmt::dash();
            if (std::isfinite(lot.fx_rate)) {
                ui::formatting::InstrumentFmt f;
                f.fx = true;
                fx_text = ui::formatting::price(lot.fx_rate, f);
            }
            open_table_->setItem(r, 10, tl_num(fx_text, lot.fx_rate));
        }
    }
    open_table_->setSortingEnabled(true);
    open_table_->resizeColumnsToContents();

    auto* t_lots = kpis_->tile("lots");
    t_lots->set_value(QString::number(lots));
    t_lots->set_sub_label(tr("%1 relief").arg(portfolio::lot_method_key(selected_method())));
    auto set_money = [&](const char* key, double v, bool partial) {
        auto* t = kpis_->tile(QString::fromLatin1(key));
        t->set_value(pfmt::smoney(v, base));
        t->set_tone(v > 0 ? ui::StatTile::Tone::Positive : v < 0 ? ui::StatTile::Tone::Negative : ui::StatTile::Tone::Neutral);
        t->set_sub_label(partial ? tr("partial — some values unknown") : QString());
    };
    set_money("unreal_st", unreal_st, unreal_partial);
    set_money("unreal_lt", unreal_lt, unreal_partial);
}

void TaxLotsView::render_realised() {
    const QString base = pfmt::base_ccy(summary_);
    const QString year = year_cb_->currentData().toString();
    realised_table_->setSortingEnabled(false);
    realised_table_->setRowCount(0);
    double total = 0, st = 0, lt = 0;
    QMap<int, QPair<double, double>> by_year; // year -> (short, long)
    for (const auto& r : ledger_.realized) {
        if (std::isfinite(r.gain_base) && r.close_date.isValid()) {
            auto& y = by_year[r.close_date.year()];
            (r.long_term ? y.second : y.first) += r.gain_base;
        }
        if (!year.isEmpty() && QString::number(r.close_date.year()) != year)
            continue;
        if (std::isfinite(r.gain_base)) {
            total += r.gain_base;
            (r.long_term ? lt : st) += r.gain_base;
        }
        const int row = realised_table_->rowCount();
        realised_table_->insertRow(row);
        realised_table_->setItem(row, 0, tl_date(r.close_date));
        realised_table_->setItem(row, 1, tl_text(r.symbol, QColor(ui::colors::CYAN())));
        realised_table_->setItem(row, 2, tl_date(r.open_date));
        realised_table_->setItem(row, 3, tl_num(pfmt::qty(r.quantity), r.quantity));
        realised_table_->setItem(row, 4, tl_num(pfmt::money(r.proceeds_base, base), r.proceeds_base));
        realised_table_->setItem(row, 5, tl_num(pfmt::money(r.cost_base, base), r.cost_base));
        realised_table_->setItem(row, 6, tl_num(pfmt::smoney(r.gain_base, base), r.gain_base, pfmt::dir_color(r.gain_base)));
        realised_table_->setItem(row, 7, tl_num(pfmt::smoney(r.gain_local, r.currency), r.gain_local,
                                                pfmt::dir_color(r.gain_local)));
        realised_table_->setItem(row, 8, tl_text(r.currency, QColor(ui::colors::TEXT_SECONDARY())));
        realised_table_->setItem(row, 9, tl_num(QString::number(r.holding_days), r.holding_days));
        realised_table_->setItem(row, 10, tl_text(r.long_term ? tr("LONG") : tr("SHORT"),
                                                  QColor(r.long_term ? ui::colors::INFO() : ui::colors::TEXT_SECONDARY())));
    }
    realised_table_->setSortingEnabled(true);
    realised_table_->sortByColumn(0, Qt::DescendingOrder);
    realised_table_->resizeColumnsToContents();

    // Realised by tax year, short vs long term (all years, whatever the filter).
    QStringList cats;
    QVector<double> sv, lv;
    for (auto it = by_year.constBegin(); it != by_year.constEnd(); ++it) {
        cats << QString::number(it.key());
        sv << it.value().first;
        lv << it.value().second;
    }
    year_chart_->set_categories(cats);
    year_chart_->set_series({ui::BarSeries{tr("Short-term"), sv, ui::colors::chart_color(0)},
                             ui::BarSeries{tr("Long-term"), lv, ui::colors::chart_color(1)}});
    year_chart_->set_value_formatter([base](double v) { return pfmt::money_c(v, base); });
    year_chart_->set_state(cats.isEmpty() ? ui::PaintedChart::Empty : ui::PaintedChart::Ready,
                           tr("No sales recorded yet."));

    const QString span = year.isEmpty() ? tr("all years") : year;
    auto set_money = [&](const char* key, double v) {
        auto* t = kpis_->tile(QString::fromLatin1(key));
        t->set_value(pfmt::smoney(v, base));
        t->set_tone(v > 0 ? ui::StatTile::Tone::Positive : v < 0 ? ui::StatTile::Tone::Negative : ui::StatTile::Tone::Neutral);
        t->set_sub_label(span);
    };
    set_money("realised", total);
    set_money("st", st);
    set_money("lt", lt);
    kpis_->tile("lt")->set_tooltip_text(tr("Lots held longer than %1 days.").arg(lt_spin_->value()));
}

void TaxLotsView::export_csv() {
    const QString base = pfmt::base_ccy(summary_);
    QStringList headers = {"record", "symbol", "open_date", "close_date", "quantity", "currency", "cost_local",
                           "proceeds_local", "gain_local", "cost_" + base, "proceeds_" + base, "gain_" + base,
                           "holding_days", "term", "method"};
    const QString method = portfolio::lot_method_key(selected_method());
    QVector<QStringList> rows;
    for (auto it = ledger_.positions.constBegin(); it != ledger_.positions.constEnd(); ++it)
        for (const auto& lot : it->lots)
            if (lot.quantity > 0)
                rows.append({"open_lot", lot.symbol, lot.open_date.toString(Qt::ISODate), QString(),
                             tl_csv(lot.quantity, 8), lot.currency, tl_csv(lot.cost_local), QString(), QString(),
                             tl_csv(lot.cost_base), QString(), QString(), QString::number(lot.holding_days),
                             lot.long_term ? "long" : "short", method});
    for (const auto& r : ledger_.realized)
        rows.append({"realised", r.symbol, r.open_date.toString(Qt::ISODate), r.close_date.toString(Qt::ISODate),
                     tl_csv(r.quantity, 8), r.currency, tl_csv(r.cost_local), tl_csv(r.proceeds_local),
                     tl_csv(r.gain_local), tl_csv(r.cost_base), tl_csv(r.proceeds_base), tl_csv(r.gain_base),
                     QString::number(r.holding_days), r.long_term ? "long" : "short", method});
    if (rows.isEmpty()) {
        QMessageBox::information(this, tr("Export"), tr("No lots or realised gains to export."));
        return;
    }
    ui::ChartExport::save_csv(headers, rows,
                              QStringLiteral("tax_lots_%1_%2").arg(method, QDate::currentDate().toString(Qt::ISODate)),
                              this);
}

void TaxLotsView::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange && has_data_)
        render();
    QWidget::changeEvent(event);
}

} // namespace fincept::screens
