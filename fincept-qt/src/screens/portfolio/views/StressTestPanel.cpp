// src/screens/portfolio/views/StressTestPanel.cpp
#include "screens/portfolio/views/StressTestPanel.h"

#include "screens/portfolio/PortfolioFormat.h"
#include "screens/portfolio/PortfolioLedgerDialogs.h"
#include "screens/portfolio/PortfolioSharedRun.h"
#include "services/portfolio/PortfolioService.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/charts/BarChart.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/BarCellDelegate.h"
#include "ui/tables/NumericTableWidgetItem.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/StatTile.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QDateEdit>
#include <QDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

namespace ps = portfolio_stress;

namespace {

constexpr const char* kStressUserKey = "portfolio.stress.user_scenarios";
/// + portfolio id: the scenario last looked at in that book (replayed on the next open).
constexpr const char* kStressLastKey = "portfolio.stress.last_scenario.";
constexpr int kStpValueRole = Qt::UserRole + 21;
constexpr int kStpIdRole = Qt::UserRole + 22;

const char* const kStpRegions[] = {"north_america", "uk", "europe", "japan", "china", "india", "asia_pacific",
                                   "emerging"};

QTableWidgetItem* stp_num(const QString& text, double v, const QColor& fg = {}) {
    auto* it = new ui::NumericTableWidgetItem(text, std::isfinite(v) ? v : -1e300);
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    it->setData(kStpValueRole, v);
    if (fg.isValid())
        it->setForeground(fg);
    return it;
}

QTableWidget* stp_table(const QStringList& headers, QWidget* parent) {
    auto* t = new QTableWidget(0, headers.size(), parent);
    t->setHorizontalHeaderLabels(headers);
    t->verticalHeader()->setVisible(false);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->setShowGrid(false);
    t->setSortingEnabled(true);
    t->setWordWrap(false);
    t->horizontalHeader()->setStretchLastSection(true);
    ui::metrics::follow_row_height(t, 24);
    return t;
}

QString stp_opt_text(double v) {
    return std::isfinite(v) ? QString::number(v, 'g', 8) : QString();
}

double stp_parse_opt(const QLineEdit* e) {
    const QString t = e->text().trimmed();
    if (t.isEmpty())
        return portfolio::kAbsent;
    return ledger_parse_number(t);
}

} // namespace

StressTestPanel::StressTestPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
    load_user_scenarios();
    // Currency / number format changed: re-format the scenario results and the last
    // replay already held (no replay run, no change to the replay pane's view).
    ui::formatting::on_format_changed(this, [this]() {
        if (!has_data_)
            return;
        reformat_only_ = true;
        render();
        reformat_only_ = false;
        if (replay_pane_ && replay_pane_->has_result())
            render_replay(replay_pane_->payload().toObject());
    });
}

StressTestPanel::~StressTestPanel() {
    // The pane is deleted after this object's members: detach its callbacks first
    // (its destructor would otherwise call the CancelFn on a destroyed request).
    if (replay_pane_) {
        replay_pane_->set_cancel({});
        replay_pane_->set_start({});
        replay_pane_->set_render({});
    }
    replay_req_.cancel();
}

void StressTestPanel::build_ui() {
    // One selector stylesheet before any child exists; the theme manager's re-style
    // pass keeps it in step with theme / font changes.
    setStyleSheet(QString("#stpBar { background:%1; border-bottom:1px solid %2; }"
                          "#stpBar QLabel, #stpTitle { color:%3; font-size:%4px; }"
                          "#stpReplayTitle { color:%3; font-size:%4px; font-weight:700; letter-spacing:0.5px; }"
                          "#stpReplayStatus { color:%6; font-size:%4px; }"
                          // Scoped to the tool bar: the replay pane's empty / error pages style their own buttons.
                          "#stpBar QPushButton { background:transparent; color:%5; border:1px solid %2; padding:3px 8px;"
                          "  font-size:%4px; font-weight:700; }"
                          "#stpBar QPushButton:hover { border-color:%5; }"
                          "#stpBar QPushButton:disabled { color:%6; }"
                          "QDateEdit { background:%7; color:%8; border:1px solid %2; padding:2px 4px; }"
                          "QTableWidget { background:%9; color:%8; border:none; font-size:%10px; }")
                      .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::TEXT_SECONDARY(),
                           ui::fonts::px(-3), ui::colors::AMBER(), ui::colors::TEXT_TERTIARY(),
                           ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BG_BASE())
                      .arg(ui::fonts::px(-2)));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* bar = new QWidget(this);
    bar->setObjectName("stpBar");
    auto* h = new QHBoxLayout(bar);
    h->setContentsMargins(10, 6, 10, 6);
    h->setSpacing(6);
    new_btn_ = new QPushButton(tr("+ NEW SCENARIO"), bar);
    dup_btn_ = new QPushButton(tr("DUPLICATE"), bar);
    edit_btn_ = new QPushButton(tr("EDIT"), bar);
    delete_btn_ = new QPushButton(tr("DELETE"), bar);
    delete_btn_->setProperty("fincept_agent_risk", "delete");
    replay_btn_ =new QPushButton(tr("REPLAY ON REAL PRICES"), bar);
    replay_btn_->setToolTip(tr("Run the current book through the scenario's historical window on actual prices"));
    for (auto* b : {new_btn_, dup_btn_, edit_btn_, delete_btn_, replay_btn_})
        h->addWidget(b);
    h->addStretch(1);
    h->addWidget(new QLabel(tr("Any window:"), bar));
    from_edit_ = new QDateEdit(QDate::currentDate().addYears(-1), bar);
    to_edit_ = new QDateEdit(QDate::currentDate(), bar);
    for (auto* d : {from_edit_, to_edit_}) {
        d->setCalendarPopup(true);
        d->setDisplayFormat(ui::formatting::date_pattern(ui::formatting::DateStyle::Short));
        d->setMaximumDate(QDate::currentDate());
        h->addWidget(d);
    }
    run_window_btn_ = new QPushButton(tr("RUN"), bar);
    h->addWidget(run_window_btn_);
    root->addWidget(bar);

    auto* split = new QSplitter(Qt::Vertical, this);
    auto* top = new QSplitter(Qt::Horizontal, split);
    table_ = stp_table({tr("SCENARIO"), tr("TYPE"), tr("WINDOW"), tr("IMPACT"), tr("P&L"), tr("OF WHICH FX"),
                        tr("BETA-SCALED"), tr("NOT VALUED")},
                       top);
    auto* impact_bar = new ui::BarCellDelegate(ui::BarCellDelegate::Mode::SignedBar, table_);
    impact_bar->set_value_role(kStpValueRole);
    table_->setItemDelegateForColumn(3, impact_bar);
    top->addWidget(table_);
    impact_chart_ = new ui::BarChart(top);
    impact_chart_->set_orientation(Qt::Horizontal);
    impact_chart_->set_signed_colors(true);
    impact_chart_->set_value_formatter([](double v) { return pfmt::pct(v, 1); });
    top->addWidget(impact_chart_);
    top->setStretchFactor(0, 3);
    top->setStretchFactor(1, 2);
    split->addWidget(top);

    auto* detail = new QWidget(split);
    auto* dv = new QVBoxLayout(detail);
    dv->setContentsMargins(0, 0, 0, 0);
    detail_title_ = new QLabel(detail);
    detail_title_->setObjectName("stpTitle");
    detail_title_->setWordWrap(true);
    detail_title_->setContentsMargins(10, 6, 10, 4);
    dv->addWidget(detail_title_);
    // Per-holding detail above, the replay on real prices below (resizable).
    auto* dvsplit = new QSplitter(Qt::Vertical, detail);
    dvsplit->setChildrenCollapsible(false);
    auto* dsplit = new QSplitter(Qt::Horizontal, dvsplit);
    detail_table_ = stp_table({tr("SYMBOL"), tr("CLASS"), tr("REGION"), tr("WEIGHT"), tr("LOCAL"), tr("FX"),
                               tr("TOTAL"), tr("P&L"), tr("MODEL")},
                              dsplit);
    auto* pnl_bar = new ui::BarCellDelegate(ui::BarCellDelegate::Mode::SignedBar, detail_table_);
    pnl_bar->set_value_role(kStpValueRole);
    detail_table_->setItemDelegateForColumn(7, pnl_bar);
    dsplit->addWidget(detail_table_);
    holding_chart_ = new ui::BarChart(dsplit);
    holding_chart_->set_orientation(Qt::Horizontal);
    holding_chart_->set_signed_colors(true);
    dsplit->addWidget(holding_chart_);
    dsplit->setStretchFactor(0, 3);
    dsplit->setStretchFactor(1, 2);
    dvsplit->addWidget(dsplit);

    // ── Historical replay (AutoLoadPane: skeleton → KPI tiles + path, or why not) ──
    auto* replay_box = new QWidget(dvsplit);
    auto* rbv = new QVBoxLayout(replay_box);
    rbv->setContentsMargins(0, 0, 0, 0);
    rbv->setSpacing(0);
    auto* rhead = new QHBoxLayout;
    rhead->setContentsMargins(10, 4, 10, 2);
    rhead->setSpacing(8);
    replay_title_ = new QLabel(replay_box);
    replay_title_->setObjectName("stpReplayTitle");
    replay_title_->setWordWrap(true);
    rhead->addWidget(replay_title_, 1);
    replay_status_ = new QLabel(replay_box);
    replay_status_->setObjectName("stpReplayStatus");
    rhead->addWidget(replay_status_, 0, Qt::AlignRight | Qt::AlignVCenter);
    rbv->addLayout(rhead);

    replay_pane_ = new ui::AutoLoadPane(ui::AutoLoadPane::Policy::Manual, replay_box);
    auto* content = new QWidget(replay_pane_->content_parent());
    auto* cv = new QVBoxLayout(content);
    cv->setContentsMargins(10, 2, 10, 4);
    cv->setSpacing(4);
    replay_kpis_ = new ui::KpiStrip(content);
    replay_kpis_->set_min_tile_width(ui::metrics::width(120));
    for (const auto& [k, l] : {std::pair{"ret", tr("BOOK RETURN")}, std::pair{"pnl", tr("P&L")},
                               std::pair{"dd", tr("MAX DRAWDOWN")}, std::pair{"bench", tr("BENCHMARK")},
                               std::pair{"cov", tr("COVERAGE")}}) {
        auto* t = replay_kpis_->add_tile(QString::fromLatin1(k), l);
        t->set_compact(true);
    }
    cv->addWidget(replay_kpis_);
    hist_chart_ = new ui::TimeSeriesChart(content);
    hist_chart_->set_utc(true);
    hist_chart_->set_show_mode_buttons(false);
    hist_chart_->set_time_formatter([](double x) { return ui::formatting::bar_date(static_cast<qint64>(x / 1000.0)); });
    hist_chart_->set_export_name(QStringLiteral("stress_replay"));
    hist_chart_->setMinimumHeight(ui::metrics::height(120));
    cv->addWidget(hist_chart_, 1);
    hist_summary_ = new QLabel(content);
    hist_summary_->setObjectName("stpTitle");
    hist_summary_->setWordWrap(true);
    cv->addWidget(hist_summary_);
    replay_pane_->set_content(content);
    replay_pane_->skeleton()->set_shape(ui::LoadingSkeleton::Shape::Result);
    replay_pane_->skeleton()->set_kpi_count(5);
    replay_pane_->skeleton()->set_row_count(1);
    replay_pane_->set_cache_key(QStringLiteral("portfolio/stress_replay"), kPortfolioResultTtlDays);
    replay_pane_->set_status_label(replay_status_);
    replay_pane_->set_auto_delay(300); // users flick through the scenario rows
    replay_pane_->set_timeout(150);    // the service's own budget for portfolio_metrics.py
    replay_pane_->set_start([this](quint64 id) { start_replay(id); });
    replay_pane_->set_cancel([this](quint64 id) {
        if (id == replay_req_for_) {
            replay_req_.cancel();
            replay_req_ = {};
            replay_req_for_ = 0;
        }
    });
    replay_pane_->set_render([this](const QJsonValue& p, const QDateTime&, bool) { render_replay(p.toObject()); });
    replay_pane_->preview()->apply(ui::EmptyState::no_data_spec(
        tr("Historical replay"), tr("Select a scenario with a historical window to replay it on real prices.")));
    replay_pane_->preview()->set_compact(true);
    rbv->addWidget(replay_pane_, 1);
    replay_box->setMinimumHeight(ui::metrics::height(170));
    dvsplit->addWidget(replay_box);
    dvsplit->setStretchFactor(0, 1);
    dvsplit->setStretchFactor(1, 1);
    dv->addWidget(dvsplit, 1);
    split->addWidget(detail);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 1);
    root->addWidget(split, 1);

    connect(table_, &QTableWidget::itemSelectionChanged, this, [this]() {
        const int r = table_->currentRow();
        if (r >= 0 && table_->item(r, 0))
            selected_id_ = table_->item(r, 0)->data(kStpIdRole).toString();
        custom_window_ = false; // a scenario row replaces the custom window on display
        remember_scenario();
        render_detail();
    });
    connect(new_btn_, &QPushButton::clicked, this, [this]() { edit_scenario(-1, false); });
    connect(dup_btn_, &QPushButton::clicked, this, [this]() { edit_scenario(selected_index(), true); });
    connect(edit_btn_, &QPushButton::clicked, this, [this]() { edit_scenario(selected_index(), false); });
    connect(delete_btn_, &QPushButton::clicked, this, [this]() { delete_scenario(selected_index()); });
    connect(replay_btn_, &QPushButton::clicked, this, [this]() {
        const int i = selected_index();
        if (i < 0 || scenarios_[i].window_start.isEmpty())
            return;
        custom_window_ = false;
        update_replay_view();
        replay_pane_->run(); // the user asked: run again even when a replay is remembered
    });
    connect(run_window_btn_, &QPushButton::clicked, this, [this]() {
        if (from_edit_->date() >= to_edit_->date()) {
            QMessageBox::warning(this, tr("Historical Window"), tr("The start must be before the end."));
            return;
        }
        if (!has_data_ || summary_.holdings.isEmpty())
            return;
        custom_window_ = true;
        custom_start_ = from_edit_->date().toString(Qt::ISODate);
        custom_end_ = to_edit_->date().toString(Qt::ISODate);
        update_replay_view();
        replay_pane_->run();
    });

    // Item colours and charts are data: re-render them once an appearance change has
    // settled (never inside theme_changed). The stylesheet itself is kept in step by the
    // theme manager's re-style pass.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        if (has_data_)
            render();
    });
}

void StressTestPanel::load_user_scenarios() {
    scenarios_ = ps::preset_scenarios();
    n_presets_ = static_cast<int>(scenarios_.size());
    auto r = SettingsRepository::instance().get(kStressUserKey);
    if (r.is_ok() && !r.value().isEmpty()) {
        const auto arr = QJsonDocument::fromJson(r.value().toUtf8()).array();
        for (const auto& v : arr)
            if (v.isObject())
                scenarios_.append(ps::scenario_from_json(v.toObject()));
    }
}

void StressTestPanel::save_user_scenarios() const {
    QJsonArray arr;
    for (int i = n_presets_; i < scenarios_.size(); ++i)
        arr.append(ps::scenario_to_json(scenarios_[i]));
    SettingsRepository::instance().set(kStressUserKey, QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)),
                                       "portfolio");
}

void StressTestPanel::set_data(const portfolio::PortfolioSummary& summary) {
    summary_ = summary;
    has_data_ = true;
    if (loaded_for_ != summary.portfolio.id) {
        // Back to the scenario this book was last looked at (its replay runs on open).
        loaded_for_ = summary.portfolio.id;
        selected_id_ = remembered_scenario(summary.portfolio.id);
        custom_window_ = false;
    }
    render();
}

QString StressTestPanel::remembered_scenario(const QString& portfolio_id) const {
    if (!portfolio_id.isEmpty()) {
        const auto r = SettingsRepository::instance().get(QString::fromLatin1(kStressLastKey) + portfolio_id);
        if (r.is_ok() && !r.value().isEmpty()) {
            for (const auto& sc : scenarios_)
                if (sc.id == r.value())
                    return sc.id;
        }
    }
    // Never looked at: the first preset with a real window to replay.
    for (int i = 0; i < n_presets_ && i < scenarios_.size(); ++i)
        if (!scenarios_[i].window_start.isEmpty())
            return scenarios_[i].id;
    return scenarios_.isEmpty() ? QString() : scenarios_.first().id;
}

void StressTestPanel::remember_scenario() const {
    if (summary_.portfolio.id.isEmpty() || selected_id_.isEmpty())
        return;
    const QString key = QString::fromLatin1(kStressLastKey) + summary_.portfolio.id;
    const auto r = SettingsRepository::instance().get(key);
    if (r.is_ok() && r.value() == selected_id_)
        return; // same row clicked again: no write
    SettingsRepository::instance().set(key, selected_id_, QStringLiteral("portfolio"));
}

void StressTestPanel::set_analytics(const portfolio::MarketAnalytics& analytics) {
    analytics_ = analytics;
    if (has_data_)
        render();
}

int StressTestPanel::selected_index() const {
    for (int i = 0; i < scenarios_.size(); ++i)
        if (scenarios_[i].id == selected_id_)
            return i;
    return -1;
}

void StressTestPanel::render() {
    const QString base = pfmt::base_ccy(summary_);
    const bool betas = analytics_.portfolio_id == summary_.portfolio.id && !analytics_.holdings.isEmpty();
    const QHash<QString, portfolio::HoldingRisk>* risk = betas ? &analytics_.holdings : nullptr;
    results_.clear();
    for (const auto& s : scenarios_)
        results_.append(ps::apply(s, summary_, risk));

    table_->setSortingEnabled(false);
    table_->setRowCount(static_cast<int>(scenarios_.size()));
    QVector<QPair<QString, double>> bars;
    for (int i = 0; i < scenarios_.size(); ++i) {
        const auto& s = scenarios_[i];
        const auto& r = results_[i];
        auto* name = new QTableWidgetItem(ps::scenario_name(s));
        name->setData(kStpIdRole, s.id);
        name->setToolTip(ps::scenario_description(s));
        table_->setItem(i, 0, name);
        auto* type = new QTableWidgetItem(i < n_presets_ ? tr("Preset") : tr("Custom"));
        type->setForeground(QColor(i < n_presets_ ? ui::colors::TEXT_TERTIARY() : ui::colors::INFO()));
        table_->setItem(i, 1, type);
        table_->setItem(i, 2, new QTableWidgetItem(s.window_start.isEmpty()
                                                       ? tr("hypothetical")
                                                       : tr("%1 → %2").arg(pfmt::date(s.window_start, ui::formatting::DateStyle::Short),
                                                                           pfmt::date(s.window_end, ui::formatting::DateStyle::Short))));
        table_->setItem(i, 3, stp_num(pfmt::pct(r.impact_pct, 1), r.impact_pct, pfmt::dir_color(r.impact_pct)));
        table_->setItem(i, 4, stp_num(pfmt::smoney(r.pnl, base), r.pnl, pfmt::dir_color(r.pnl)));
        table_->setItem(i, 5, stp_num(pfmt::smoney(r.fx_pnl, base), r.fx_pnl, pfmt::dir_color(r.fx_pnl)));
        table_->setItem(i, 6, stp_num(QStringLiteral("%1 / %2").arg(r.beta_lines).arg(r.equity_lines), r.beta_lines));
        table_->setItem(i, 7, stp_num(QString::number(r.unvalued), r.unvalued,
                                      r.unvalued > 0 ? QColor(ui::colors::WARNING()) : QColor()));
        bars.append({ps::scenario_name(s), r.impact_pct});
    }
    table_->setSortingEnabled(true);
    table_->sortByColumn(3, Qt::AscendingOrder);
    table_->resizeColumnsToContents();

    std::sort(bars.begin(), bars.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
    QStringList cats;
    QVector<double> vals;
    for (const auto& b : bars) {
        cats << b.first;
        vals << b.second;
    }
    impact_chart_->set_data(cats, vals, tr("Impact"));

    // Re-select.
    int row = -1;
    for (int r = 0; r < table_->rowCount(); ++r)
        if (table_->item(r, 0) && table_->item(r, 0)->data(kStpIdRole).toString() == selected_id_)
            row = r;
    if (row < 0 && table_->rowCount() > 0) {
        row = 0;
        selected_id_ = table_->item(0, 0)->data(kStpIdRole).toString();
    }
    if (row >= 0) {
        QSignalBlocker block(table_);
        table_->selectRow(row);
    }
    render_detail();
}

void StressTestPanel::render_detail() {
    const int i = selected_index();
    const bool user = i >= n_presets_;
    edit_btn_->setEnabled(i >= 0 && user);
    delete_btn_->setEnabled(i >= 0 && user);
    dup_btn_->setEnabled(i >= 0);
    replay_btn_->setEnabled(i >= 0 && !scenarios_[i].window_start.isEmpty() && !summary_.holdings.isEmpty());
    if (!reformat_only_)
        update_replay_view();
    if (i < 0 || i >= results_.size()) {
        detail_title_->clear();
        detail_table_->setRowCount(0);
        holding_chart_->clear();
        return;
    }
    const auto& s = scenarios_[i];
    const auto& r = results_[i];
    const QString base = pfmt::base_ccy(summary_);
    QStringList shocks;
    shocks << tr("equities %1").arg(pfmt::pct(s.equity_shock, 1));
    for (auto it = s.equity_by_region.constBegin(); it != s.equity_by_region.constEnd(); ++it)
        shocks << QStringLiteral("%1 %2").arg(pfmt::slice_label(portfolio::AllocationDimension::Region, it.key()),
                                              pfmt::pct(it.value(), 1));
    shocks << (std::isfinite(s.rates_bp) ? tr("rates %1").arg(ui::formatting::bp(s.rates_bp))
                                         : tr("bonds %1").arg(pfmt::pct(s.bond_shock, 1)));
    shocks << tr("commodities %1").arg(pfmt::pct(s.commodity_shock, 1));
    if (!s.fx_vs_usd.isEmpty())
        shocks << tr("%n currency move(s) vs USD", "", static_cast<int>(s.fx_vs_usd.size()));
    detail_title_->setText(tr("<b>%1</b> — %2: portfolio %3 (%4, of which FX %5). Shocks: %6. Approximate moves of "
                              "the episode, for orientation; REPLAY uses real prices.")
                               .arg(ps::scenario_name(s).toHtmlEscaped(), ps::scenario_description(s).toHtmlEscaped(),
                                    pfmt::pct(r.impact_pct, 1), pfmt::smoney(r.pnl, base), pfmt::smoney(r.fx_pnl, base),
                                    shocks.join(QStringLiteral(", ")).toHtmlEscaped()));

    detail_table_->setSortingEnabled(false);
    detail_table_->setRowCount(static_cast<int>(r.lines.size()));
    QVector<QPair<QString, double>> bars;
    for (int k = 0; k < r.lines.size(); ++k) {
        const auto& li = r.lines[k];
        detail_table_->setItem(k, 0, new QTableWidgetItem(li.symbol));
        detail_table_->setItem(k, 1, new QTableWidgetItem(
                                         pfmt::slice_label(portfolio::AllocationDimension::AssetClass,
                                                           portfolio::asset_class_key(li.asset_class)) +
                                         (li.class_guessed ? QStringLiteral(" *") : QString())));
        detail_table_->setItem(k, 2, new QTableWidgetItem(pfmt::slice_label(portfolio::AllocationDimension::Region, li.region)));
        detail_table_->setItem(k, 3, stp_num(pfmt::fpct(li.weight, 1, false), li.weight));
        detail_table_->setItem(k, 4, stp_num(pfmt::pct(li.local_pct, 1), li.local_pct, pfmt::dir_color(li.local_pct)));
        detail_table_->setItem(k, 5, stp_num(pfmt::pct(li.fx_pct, 1), li.fx_pct, pfmt::dir_color(li.fx_pct)));
        detail_table_->setItem(k, 6, stp_num(pfmt::pct(li.total_pct, 1), li.total_pct, pfmt::dir_color(li.total_pct)));
        detail_table_->setItem(k, 7, stp_num(pfmt::smoney(li.pnl, base), li.pnl, pfmt::dir_color(li.pnl)));
        detail_table_->setItem(k, 8, new QTableWidgetItem(li.beta_used ? tr("own beta") : tr("scenario move")));
        bars.append({li.symbol, li.pnl});
    }
    detail_table_->setSortingEnabled(true);
    detail_table_->sortByColumn(7, Qt::AscendingOrder);
    detail_table_->resizeColumnsToContents();
    std::sort(bars.begin(), bars.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
    QStringList cats;
    QVector<double> vals;
    for (const auto& b : bars) {
        cats << b.first;
        vals << b.second;
    }
    holding_chart_->set_value_formatter([base](double v) { return pfmt::smoney_c(v, base); });
    holding_chart_->set_data(cats, vals, tr("P&L"));
}

void StressTestPanel::update_replay_view() {
    using Policy = ui::AutoLoadPane::Policy;
    const int i = selected_index();
    QString key;
    Policy policy = Policy::Manual;
    int max_age = ui::autoload::kNeverRefresh;
    ui::EmptyState::Spec preview;
    QString title;
    QString start, end, label;
    if (!has_data_ || summary_.holdings.isEmpty()) {
        key = QStringLiteral("none");
        preview = ui::EmptyState::no_data_spec(tr("Nothing to replay yet"),
                                               tr("Add a position: its moves through past crises appear here."));
        title = tr("HISTORICAL REPLAY");
    } else if (custom_window_) {
        start = custom_start_;
        end = custom_end_;
        label = tr("%1 → %2").arg(pfmt::date(start), pfmt::date(end));
        key = QStringLiteral("window:%1:%2|").arg(start, end) + portfolio_run_key(summary_);
        policy = Policy::AutoRun;
        title = tr("HISTORICAL REPLAY — %1").arg(label);
    } else if (i < 0) {
        key = QStringLiteral("none");
        preview = ui::EmptyState::no_data_spec(
            tr("Historical replay"), tr("Select a scenario with a historical window to replay it on real prices."));
        title = tr("HISTORICAL REPLAY");
    } else if (scenarios_[i].window_start.isEmpty()) {
        key = QStringLiteral("hypothetical:") + scenarios_[i].id;
        preview = ui::EmptyState::no_data_spec(
            tr("Hypothetical scenario — no historical window to replay"),
            tr("The table above applies its shocks to the book. Pick a crisis preset (or set any window above) "
               "to see the book on real prices."));
        title = tr("HISTORICAL REPLAY — %1").arg(ps::scenario_name(scenarios_[i]));
    } else {
        const auto& sc = scenarios_[i];
        start = sc.window_start;
        end = sc.window_end;
        label = ps::scenario_name(sc);
        key = sc.id + QLatin1Char('|') + portfolio_run_key(summary_);
        policy = Policy::AutoRun;
        title = tr("HISTORICAL REPLAY — %1 · %2 → %3")
                    .arg(label, pfmt::date(start, ui::formatting::DateStyle::Short),
                         pfmt::date(end, ui::formatting::DateStyle::Short));
    }
    if (policy == Policy::AutoRun) {
        // A closed past window is deterministic: remembered for good. A window
        // reaching today moves with the market.
        const QDate e = QDate::fromString(end, Qt::ISODate);
        max_age = e.isValid() && e < QDate::currentDate() ? ui::autoload::kNeverRefresh : kPortfolioScriptMaxAgeS;
    }
    if (replay_title_->text() != title)
        replay_title_->setText(title);
    if (key == replay_pane_->view() && policy == replay_pane_->policy())
        return; // same view (price refresh): nothing to do
    replay_start_ = start;
    replay_end_ = end;
    replay_label_ = label;
    replay_pane_->set_loading_text(label.isEmpty() ? QString() : tr("Replaying %1 on real prices…").arg(label));
    if (policy == Policy::Manual) {
        preview.glyph = QStringLiteral("\u25CB"); // ○
        replay_pane_->preview()->apply(preview);
        replay_pane_->preview()->set_compact(true);
    }
    replay_pane_->set_max_age(max_age);
    replay_pane_->set_view(key, policy);
}

void StressTestPanel::start_replay(quint64 request_id) {
    if (!has_data_ || summary_.holdings.isEmpty() || replay_start_.isEmpty()) {
        replay_pane_->fail(request_id, tr("Nothing to replay for this selection."));
        return;
    }
    replay_req_.cancel(); // superseded
    replay_req_for_ = request_id;
    QPointer<ui::AutoLoadPane> pane(replay_pane_);
    QPointer<StressTestPanel> self(this);
    replay_req_ = services::PortfolioService::instance().historical_stress(
        summary_, replay_start_, replay_end_, [self, pane, request_id](const services::AnalyticsResult& res) {
            if (!self || !pane || res.cancelled)
                return;
            if (self->replay_req_for_ == request_id) {
                self->replay_req_ = {};
                self->replay_req_for_ = 0;
            }
            // deliver() / fail() drop a reply whose request id is no longer current.
            if (!res.success)
                pane->fail(request_id, res.error.isEmpty() ? tr("The replay failed.") : res.error);
            else
                pane->deliver(request_id, res.data);
        });
}

void StressTestPanel::render_replay(const QJsonObject& d) {
    using State = ui::StatTile::State;
    using Tone = ui::StatTile::Tone;
    const QString base = pfmt::base_ccy(summary_);
    const QJsonValue ret = d.value("return");
    auto tile = [this](const char* k) { return replay_kpis_->tile(QString::fromLatin1(k)); };
    auto tone = [](const QJsonValue& v) {
        return !v.isDouble() || v.toDouble() == 0 ? Tone::Neutral : v.toDouble() > 0 ? Tone::Positive : Tone::Negative;
    };
    auto frac = [](const QJsonValue& v, int dp, bool sign) {
        return v.isDouble() ? pfmt::fpct(v.toDouble(), dp, sign) : pfmt::dash();
    };
    const bool priced = ret.isDouble();
    for (const char* k : {"ret", "pnl", "dd", "bench", "cov"})
        tile(k)->set_state(priced ? State::Normal : State::Error, priced ? QString() : tr("No prices for that window"));
    tile("ret")->set_value(frac(ret, 1, true));
    tile("ret")->set_tone(tone(ret));
    tile("ret")->set_sub_label(tr("current weights, buy and hold"));
    const QJsonValue pnl = d.value("pnl");
    tile("pnl")->set_value(pnl.isDouble() ? ui::formatting::signed_money(pnl.toDouble(), base,
                                                                         ui::formatting::Compact::FromSettings)
                                          : pfmt::dash());
    tile("pnl")->set_tone(tone(pnl));
    tile("pnl")->set_sub_label(tr("on today's book, %1").arg(base));
    tile("dd")->set_value(frac(d.value("max_drawdown"), 1, true));
    tile("dd")->set_sub_label(tr("worst peak-to-trough"));
    tile("bench")->set_value(frac(d.value("benchmark_return"), 1, true));
    tile("bench")->set_tone(tone(d.value("benchmark_return")));
    tile("bench")->set_sub_label(services::PortfolioService::benchmark_for(summary_.portfolio));
    tile("cov")->set_value(frac(d.value("coverage"), 0, false));
    tile("cov")->set_sub_label(tr("of the book priced then"));

    QStringList notes;
    QStringList missing;
    for (const auto& v : d.value("missing").toArray())
        missing << v.toString();
    if (!missing.isEmpty())
        notes << tr("Not priced then: %1.").arg(missing.join(QStringLiteral(", ")));
    for (const auto& v : d.value("warnings").toArray())
        notes << v.toString();
    hist_summary_->setText(notes.join(QLatin1Char(' ')));
    hist_summary_->setVisible(!notes.isEmpty());

    const QJsonObject path = d.value("path").toObject();
    const QJsonArray dates = path.value("dates").toArray();
    const QJsonArray level = path.value("level").toArray();
    QVector<QPointF> pts;
    pts.reserve(dates.size());
    for (int i = 0; i < dates.size() && i < level.size(); ++i)
        pts.append({pfmt::utc_ms(dates[i].toString()), (level[i].toDouble() - 1.0) * 100.0});
    ui::TimeSeriesChart::SeriesSpec spec;
    spec.name = tr("Book, current weights");
    spec.points = pts;
    spec.style = ui::TimeSeriesChart::Style::Baseline;
    spec.baseline = 0.0;
    spec.rebase = false;
    spec.formatter = [](double v) { return pfmt::pct(v, 2); };
    hist_chart_->clear();
    hist_chart_->add_series(spec);
    hist_chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left, [](double v) { return pfmt::pct(v, 0); });
    hist_chart_->set_state(pts.isEmpty() ? ui::TimeSeriesChart::State::Empty : ui::TimeSeriesChart::State::Ready,
                           priced ? QString() : tr("No holding has prices for that window."));
    hist_chart_->reset_x_range();
}

void StressTestPanel::delete_scenario(int index) {
    if (index < n_presets_ || index >= scenarios_.size())
        return;
    const auto answer = QMessageBox::question(this, tr("Delete Scenario"),
                                              tr("Delete the scenario \"%1\"?").arg(ps::scenario_name(scenarios_[index])),
                                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;
    scenarios_.removeAt(index);
    selected_id_.clear();
    save_user_scenarios();
    render();
}

void StressTestPanel::edit_scenario(int index, bool duplicate) {
    ps::Scenario s;
    const bool editing = index >= 0 && !duplicate;
    if (index >= 0)
        s = scenarios_[index];
    if (index < 0 || duplicate) {
        s.id = QStringLiteral("user:") + QUuid::createUuid().toString(QUuid::WithoutBraces);
        if (duplicate)
            s.custom_name = tr("%1 (copy)").arg(ps::scenario_name(scenarios_[index]));
        s.custom_description = index >= 0 ? ps::scenario_description(scenarios_[index]) : QString();
    }

    QDialog dlg(this);
    dlg.setWindowTitle(editing ? tr("Edit Scenario") : tr("New Scenario"));
    dlg.setStyleSheet(ledger_dialog_qss(ui::colors::WARNING()));
    auto* v = new QVBoxLayout(&dlg);
    auto* form = new QFormLayout;
    auto* name = new QLineEdit(ps::scenario_name(s));
    auto* desc = new QLineEdit(ps::scenario_description(s));
    auto* eq = new QLineEdit(QString::number(s.equity_shock, 'g', 8));
    auto* bond = new QLineEdit(QString::number(s.bond_shock, 'g', 8));
    auto* rates = new QLineEdit(stp_opt_text(s.rates_bp));
    rates->setPlaceholderText(tr("blank = use the bond move"));
    auto* cmdty = new QLineEdit(QString::number(s.commodity_shock, 'g', 8));
    auto* gold = new QLineEdit(stp_opt_text(s.gold_shock));
    gold->setPlaceholderText(tr("blank = commodity move"));
    auto* crypto = new QLineEdit(stp_opt_text(s.crypto_shock));
    crypto->setPlaceholderText(tr("blank = 1.5 × equity move"));
    auto* re = new QLineEdit(stp_opt_text(s.real_estate_shock));
    re->setPlaceholderText(tr("blank = the region's equity move"));
    form->addRow(tr("Name:"), name);
    form->addRow(tr("Description:"), desc);
    form->addRow(tr("Equities (default) %:"), eq);
    form->addRow(tr("Bonds %:"), bond);
    form->addRow(tr("Rates shift (bp):"), rates);
    form->addRow(tr("Commodities %:"), cmdty);
    form->addRow(tr("Gold %:"), gold);
    form->addRow(tr("Crypto %:"), crypto);
    form->addRow(tr("Real estate %:"), re);
    v->addLayout(form);

    auto* grid = new QGridLayout;
    grid->addWidget(new QLabel(tr("Equity move by region % (blank = default):")), 0, 0, 1, 4);
    QHash<QString, QLineEdit*> region_edits;
    int k = 0;
    for (const char* key : kStpRegions) {
        const QString rk = QString::fromLatin1(key);
        auto* e = new QLineEdit(s.equity_by_region.contains(rk) ? QString::number(s.equity_by_region.value(rk), 'g', 8)
                                                                : QString());
        region_edits.insert(rk, e);
        grid->addWidget(new QLabel(pfmt::slice_label(portfolio::AllocationDimension::Region, rk)), 1 + k / 2, (k % 2) * 2);
        grid->addWidget(e, 1 + k / 2, (k % 2) * 2 + 1);
        ++k;
    }
    v->addLayout(grid);

    auto* fx_label = new QLabel(tr("Currency moves vs USD % (e.g. EUR -10 = the euro loses 10 % against the dollar):"));
    fx_label->setWordWrap(true);
    v->addWidget(fx_label);
    auto* fx = new QTableWidget(0, 2);
    fx->setHorizontalHeaderLabels({tr("Currency"), tr("Move %")});
    fx->horizontalHeader()->setStretchLastSection(true);
    fx->verticalHeader()->setVisible(false);
    fx->setMinimumHeight(ui::metrics::height(120));
    for (auto it = s.fx_vs_usd.constBegin(); it != s.fx_vs_usd.constEnd(); ++it) {
        const int r = fx->rowCount();
        fx->insertRow(r);
        fx->setItem(r, 0, new QTableWidgetItem(it.key()));
        fx->setItem(r, 1, new QTableWidgetItem(QString::number(it.value(), 'g', 8)));
    }
    v->addWidget(fx);
    auto* fx_add = new QPushButton(tr("+ CURRENCY"));
    connect(fx_add, &QPushButton::clicked, fx, [fx]() { fx->insertRow(fx->rowCount()); });
    v->addWidget(fx_add, 0, Qt::AlignLeft);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    auto* cancel = new QPushButton(tr("CANCEL"));
    auto* ok = new QPushButton(tr("SAVE"));
    buttons->addWidget(cancel);
    buttons->addWidget(ok);
    v->addLayout(buttons);
    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(ok, &QPushButton::clicked, &dlg, [&dlg, name]() {
        if (name->text().trimmed().isEmpty()) {
            name->setFocus();
            return;
        }
        dlg.accept();
    });
    if (dlg.exec() != QDialog::Accepted)
        return;

    s.name = "";
    s.description = "";
    s.custom_name = name->text().trimmed();
    s.custom_description = desc->text().trimmed();
    s.equity_shock = ledger_parse_number(eq->text());
    s.bond_shock = ledger_parse_number(bond->text());
    s.rates_bp = stp_parse_opt(rates);
    s.commodity_shock = ledger_parse_number(cmdty->text());
    s.gold_shock = stp_parse_opt(gold);
    s.crypto_shock = stp_parse_opt(crypto);
    s.real_estate_shock = stp_parse_opt(re);
    s.equity_by_region.clear();
    for (auto it = region_edits.constBegin(); it != region_edits.constEnd(); ++it) {
        const double x = stp_parse_opt(it.value());
        if (std::isfinite(x))
            s.equity_by_region.insert(it.key(), x);
    }
    s.fx_vs_usd.clear();
    for (int r = 0; r < fx->rowCount(); ++r) {
        const QString c = fx->item(r, 0) ? fx->item(r, 0)->text().trimmed().toUpper() : QString();
        const QString m = fx->item(r, 1) ? fx->item(r, 1)->text().trimmed() : QString();
        if (c.size() == 3 && !m.isEmpty())
            s.fx_vs_usd.insert(c, ledger_parse_number(m));
    }
    if (editing)
        scenarios_[index] = s;
    else
        scenarios_.append(s);
    selected_id_ = s.id;
    save_user_scenarios();
    render();
}

} // namespace fincept::screens
