// src/screens/dbnomics/DBnomicsScreen.cpp
#include "screens/dbnomics/DBnomicsScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "datahub/DataHub.h"
#include "screens/common/CommandRows.h"
#include "screens/dbnomics/DBnomicsChartWidget.h"
#include "screens/dbnomics/DBnomicsDataTable.h"
#include "screens/dbnomics/DBnomicsSelectionPanel.h"
#include "services/dbnomics/DBnomicsService.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/LoadingSkeleton.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFuture>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSplitter>
#include <QStackedWidget>
#include <QTextStream>
#include <QThreadPool>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <utility>

namespace fincept::screens {

namespace {
// The example a first-ever open shows (and LOAD EXAMPLE SERIES loads): US real GDP growth from the
// IMF World Economic Outlook — keyless, one ~10 KB request (~0.7 s, measured 2026-11-17; WEO:latest
// redirects to the newest vintage DBnomics holds).
QString dbn_example_provider() {
    return QStringLiteral("IMF");
}
QString dbn_example_dataset() {
    return QStringLiteral("WEO:latest");
}
QString dbn_example_series() {
    return QStringLiteral("USA.NGDP_RPCH.pcent_change");
}

/// The CSV export's worker (one thread: exports are written in the order they were asked for). A
/// child of the application object, so an export still being written is waited for at exit.
QThreadPool* dbn_export_pool() {
    static QThreadPool* const pool = [] {
        auto* p = new QThreadPool(QCoreApplication::instance());
        p->setObjectName(QStringLiteral("DBnomicsExport"));
        p->setMaxThreadCount(1);
        return p;
    }();
    return pool;
}

/// Writes `series` as CSV to `path` (a provenance banner, then one row per period, sorted, with one
/// column per series; an NA observation stays an empty cell). False when the file cannot be opened.
/// No widget, no translation: runs on dbn_export_pool().
bool dbn_write_csv(const QString& path, const QVector<fincept::services::DbnDataPoint>& export_series,
                   const QString& exported_at) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    // Every distinct period across the series, sorted.
    QVector<QString> periods;
    QSet<QString> seen_periods;
    for (const auto& series : export_series) {
        for (const auto& obs : series.observations) {
            if (!seen_periods.contains(obs.period)) {
                seen_periods.insert(obs.period);
                periods.append(obs.period);
            }
        }
    }
    std::sort(periods.begin(), periods.end());

    const auto csv_quote = [](QString v) {
        if (v.contains(',') || v.contains('"') || v.contains('\n'))
            v = "\"" + v.replace("\"", "\"\"") + "\"";
        return v;
    };

    // Provenance banner — a macro export without its source and series ids is not reproducible.
    out << "# source=DBnomics (https://db.nomics.world)  exported=" << exported_at << "\n";
    out << "# series=";
    for (int i = 0; i < export_series.size(); ++i)
        out << (i ? "|" : "") << export_series[i].series_id;
    out << "\n";

    // Header row
    out << "Period";
    for (const auto& series : export_series)
        out << "," << csv_quote(series.series_name);
    out << "\n";

    // Per series: period → its first observation (one lookup per cell — scanning every observation
    // for every period made a long daily series quadratic). Filled from the end: insert() overwrites,
    // so the first observation of a period is the one kept, as before.
    QVector<QHash<QString, qsizetype>> first_obs(export_series.size());
    for (int s = 0; s < export_series.size(); ++s) {
        const auto& obs = export_series[s].observations;
        first_obs[s].reserve(obs.size());
        for (qsizetype i = obs.size() - 1; i >= 0; --i)
            first_obs[s].insert(obs[i].period, i);
    }

    // Data rows
    for (const auto& period : std::as_const(periods)) {
        out << csv_quote(period);
        for (int s = 0; s < export_series.size(); ++s) {
            out << ",";
            const auto at = first_obs[s].constFind(period);
            if (at == first_obs[s].cend())
                continue;
            const auto& obs = export_series[s].observations[at.value()];
            // QTextStream's default real precision is 6 significant digits,
            // which silently rewrote e.g. 21433226000000 as 2.14332e+13.
            if (obs.valid)
                out << QString::number(obs.value, 'g', 15);
            // else: empty cell for NA — a gap must stay a gap
        }
        out << "\n";
    }
    out.flush();
    file.close();
    return true;
}

/// "PROV/DS/CODE" -> its three parts (split on the first two '/' only: codes may contain '/').
bool dbn_split_series_id(const QString& id, QString* prov, QString* ds, QString* code) {
    const int first = id.indexOf(QLatin1Char('/'));
    const int second = first < 0 ? -1 : id.indexOf(QLatin1Char('/'), first + 1);
    if (first <= 0 || second <= first + 1 || second + 1 >= id.size())
        return false;
    *prov = id.left(first);
    *ds = id.mid(first + 1, second - first - 1);
    *code = id.mid(second + 1);
    return true;
}
} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────
DBnomicsScreen::DBnomicsScreen(QWidget* parent) : QWidget(parent) {
    build_ui();

    // ── Connect service signals ───────────────────────────────────────────────
    auto& svc = services::DBnomicsService::instance();
    connect(&svc, &services::DBnomicsService::providers_loaded, this, &DBnomicsScreen::on_providers_loaded);
    connect(&svc, &services::DBnomicsService::datasets_loaded, this, &DBnomicsScreen::on_datasets_loaded);
    connect(&svc, &services::DBnomicsService::series_loaded, this, &DBnomicsScreen::on_series_loaded);
    // Observations arrive through the DataHub (watch_series()), not the service's broadcast signal:
    // that signal also fires for MCP-tool fetches and hub refreshes of other series.
    connect(&svc, &services::DBnomicsService::search_results_loaded, this, &DBnomicsScreen::on_search_results_loaded);
    connect(&svc, &services::DBnomicsService::observations_failed, this, &DBnomicsScreen::on_observations_failed);
    connect(&svc, &services::DBnomicsService::error_occurred, this, &DBnomicsScreen::on_service_error);

    // ── Connect selection_panel_ signals ──────────────────────────────────────
    connect(selection_panel_, &DBnomicsSelectionPanel::provider_selected, this, &DBnomicsScreen::on_provider_selected);
    connect(selection_panel_, &DBnomicsSelectionPanel::dataset_selected, this, &DBnomicsScreen::on_dataset_selected);
    connect(selection_panel_, &DBnomicsSelectionPanel::series_selected, this, &DBnomicsScreen::on_series_selected);

    connect(selection_panel_, &DBnomicsSelectionPanel::global_search_changed, this, [](const QString& q) {
        if (!q.isEmpty())
            services::DBnomicsService::instance().schedule_global_search(q);
    });

    connect(selection_panel_, &DBnomicsSelectionPanel::series_search_changed, this,
            [](const QString& prov, const QString& ds, const QString& q) {
                services::DBnomicsService::instance().schedule_series_search(prov, ds, q);
            });

    connect(selection_panel_, &DBnomicsSelectionPanel::load_more_datasets_requested, this, [this](int offset) {
        services::DBnomicsService::instance().fetch_datasets(selection_panel_->selected_provider(), offset);
    });

    connect(selection_panel_, &DBnomicsSelectionPanel::load_more_series_requested, this, [this](int offset) {
        // The next page of what is listed: with a series search active that is the filtered list —
        // an empty query would be dropped by on_series_loaded() (and would mix in unfiltered rows).
        services::DBnomicsService::instance().fetch_series(selection_panel_->selected_provider(),
                                                           selection_panel_->selected_dataset(),
                                                           selection_panel_->series_search_text().trimmed(), offset);
    });

    // The global-search "LOAD MORE" button emitted this signal but nothing was
    // listening — paging past the first 50 search hits was impossible.
    connect(selection_panel_, &DBnomicsSelectionPanel::load_more_search_requested, this, [this](int offset) {
        const QString q = selection_panel_->global_search_text().trimmed();
        if (q.isEmpty())
            return;
        selection_panel_->set_search_loading(true);
        services::DBnomicsService::instance().global_search(q, offset);
    });

    connect(selection_panel_, &DBnomicsSelectionPanel::add_to_single_view_clicked, this,
            &DBnomicsScreen::on_add_to_single_view);
    connect(selection_panel_, &DBnomicsSelectionPanel::clear_all_clicked, this, &DBnomicsScreen::on_clear_all);
    connect(selection_panel_, &DBnomicsSelectionPanel::add_slot_clicked, this, &DBnomicsScreen::on_add_slot);
    connect(selection_panel_, &DBnomicsSelectionPanel::add_to_slot_clicked, this, &DBnomicsScreen::on_add_to_slot);
    connect(selection_panel_, &DBnomicsSelectionPanel::remove_from_slot_clicked, this,
            &DBnomicsScreen::on_remove_from_slot);
    connect(selection_panel_, &DBnomicsSelectionPanel::remove_slot_clicked, this, &DBnomicsScreen::on_remove_slot);

    // Series colours are the theme's categorical chart colours, stored in each data point: re-assign
    // them for the new theme and re-render the charts, tables and slot legends that show them — once
    // the new appearance has settled (re-rendering builds widgets and stylesheets).
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this,
            &DBnomicsScreen::apply_appearance);
}

void DBnomicsScreen::apply_appearance() {
    assign_series_colors();
    for (int i = 0; i < slots_.size(); ++i) {
        for (int k = 0; k < slots_[i].series.size(); ++k)
            slots_[i].series[k].color = ui::colors::chart_color(k);
        selection_panel_->update_slot_series(i, slots_[i].series);
    }
    if (!single_view_series().isEmpty())
        render_single_view();
    if (!slots_.isEmpty())
        rebuild_comparison_view();
}

// ─────────────────────────────────────────────────────────────────────────────
// show / hide
// ─────────────────────────────────────────────────────────────────────────────
void DBnomicsScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    LOG_DEBUG("DBnomicsScreen", "Screen shown");
    if (provider_count_ == 0) {
        LOG_DEBUG("DBnomicsScreen", "No providers cached — fetching providers");
        selection_panel_->set_providers_loading(true);
        services::DBnomicsService::instance().fetch_providers();
    }
    // Visibility-driven hub lifecycle (P3 / D3): resume the series this screen shows. A cached,
    // still-fresh observation set is delivered immediately on subscribe.
    const QSet<QString> watched = watched_series_;
    for (const QString& series_id : watched)
        watch_series(series_id);
    if (!first_show_done_) {
        // One event-loop turn later: the router shows a new screen before it restores its state.
        QTimer::singleShot(0, this, [this]() {
            if (isVisible())
                first_show();
        });
    }
}

void DBnomicsScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    datahub::DataHub::instance().unsubscribe(this);
    LOG_DEBUG("DBnomicsScreen", "Screen hidden");
}

void DBnomicsScreen::first_show() {
    if (first_show_done_)
        return;
    first_show_done_ = true;
    if (!pending_state_.isEmpty())
        apply_restored_state(std::exchange(pending_state_, QVariantMap()));

    bool slot_series = false;
    for (const auto& ids : std::as_const(restore_slots_))
        slot_series = slot_series || !ids.isEmpty();
    const bool nothing_remembered = selection_panel_->selected_provider().isEmpty() && single_series_.isEmpty() &&
                                    restore_single_.isEmpty() && pending_series_id_.isEmpty() && !slot_series;
    if (nothing_remembered)
        load_example_series(); // never an empty first open
    else
        update_single_page();
}

void DBnomicsScreen::load_example_series() {
    const QString prov = dbn_example_provider();
    const QString ds = dbn_example_dataset();
    const QString code = dbn_example_series();
    // Through the same provider → dataset → series path a click takes, so the lists on the left
    // show where the example lives.
    selection_panel_->restore_selection(prov, ds, code);
    on_provider_selected(prov);
    on_dataset_selected(ds);
    on_series_selected(prov, ds, code);
    set_status(tr("Loading the example: US real GDP growth (IMF World Economic Outlook)…"));
}

// ─────────────────────────────────────────────────────────────────────────────
// DataHub subscriptions for observations
// ─────────────────────────────────────────────────────────────────────────────
QString DBnomicsScreen::series_topic(const QString& series_id) {
    QString prov, ds, code;
    if (!dbn_split_series_id(series_id, &prov, &ds, &code))
        return {};
    // hub_topic() escapes ':' inside the codes (IMF "WEO:2024-10"), so the producer can split it back.
    return services::DBnomicsService::hub_topic(prov, ds, code);
}

void DBnomicsScreen::watch_series(const QString& series_id) {
    watched_series_.insert(series_id);
    const QString topic = series_topic(series_id);
    if (topic.isEmpty() || !isVisible())
        return; // hidden: showEvent() subscribes everything in watched_series_
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this, topic); // idempotent re-subscribe (no duplicate delivery)
    hub.subscribe<services::DbnDataPoint>(
        this, topic, [this](const services::DbnDataPoint& point) { on_observations_loaded(point); });
}

void DBnomicsScreen::unwatch_unused_series() {
    QSet<QString> in_use;
    if (!pending_series_id_.isEmpty())
        in_use.insert(pending_series_id_);
    if (has_preview_)
        in_use.insert(preview_.series_id);
    for (const auto& id : std::as_const(restore_single_))
        in_use.insert(id);
    for (const auto& ids : std::as_const(restore_slots_))
        for (const auto& id : ids)
            in_use.insert(id);
    for (const auto& s : std::as_const(single_series_))
        in_use.insert(s.series_id);
    for (const auto& slot : std::as_const(slots_))
        for (const auto& s : slot.series)
            in_use.insert(s.series_id);

    auto& hub = datahub::DataHub::instance();
    for (auto it = watched_series_.begin(); it != watched_series_.end();) {
        if (in_use.contains(*it)) {
            ++it;
            continue;
        }
        const QString topic = series_topic(*it);
        if (!topic.isEmpty())
            hub.unsubscribe(this, topic);
        it = watched_series_.erase(it);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// build_ui
// ─────────────────────────────────────────────────────────────────────────────
void DBnomicsScreen::build_ui() {
    setStyleSheet(
        QString("QWidget { background:%1; color:%2; }").arg(ui::colors::BG_BASE()).arg(ui::colors::TEXT_PRIMARY()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Toolbar ───────────────────────────────────────────────────────────────
    root->addWidget(build_toolbar());

    // ── Content area ─────────────────────────────────────────────────────────
    auto* content_widget = new QWidget(this);
    auto* content_hl = new QHBoxLayout(content_widget);
    content_hl->setContentsMargins(0, 0, 0, 0);
    content_hl->setSpacing(0);

    // Left sidebar
    selection_panel_ = new DBnomicsSelectionPanel(content_widget);
    selection_panel_->setMinimumWidth(220);
    selection_panel_->setMaximumWidth(340);
    content_hl->addWidget(selection_panel_);

    // Right panel
    auto* right_widget = new QWidget(content_widget);
    auto* right_vl = new QVBoxLayout(right_widget);
    right_vl->setContentsMargins(0, 0, 0, 0);
    right_vl->setSpacing(0);

    // View toggle bar. One selector stylesheet, set before its children exist: the SINGLE / COMPARE
    // buttons are checkable and styled by :checked (they used to be restyled on every click).
    auto* toggle_bar = new QWidget(right_widget);
    toggle_bar->setObjectName("dbnToggleBar");
    toggle_bar->setMinimumHeight(36);
    toggle_bar->setStyleSheet(
        QString("#dbnToggleBar { background:%1; border-bottom:1px solid %2; }"
                "QPushButton#dbnModeBtn { background:transparent; color:%3; border:1px solid %2;"
                "  font-family:%4; font-size:%5px; font-weight:700; padding:0 12px; min-height:26px; }"
                "QPushButton#dbnModeBtn:hover { background:%6; }"
                "QPushButton#dbnModeBtn:checked { background:%7; color:%8; border-color:%9; }")
            .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_MED(), ui::colors::TEXT_SECONDARY(),
                 QString(ui::fonts::DATA_FAMILY), ui::fonts::px(-2), ui::colors::BG_HOVER(), ui::colors::ACCENT_BG(),
                 ui::colors::AMBER(), ui::colors::AMBER_DIM()) +
        QString("QPushButton#dbnModeBtn:checked:hover { background:%1; }"
                "QLabel#dbnChartTypeLabel { color:%2; font-family:%3; font-size:%4px; font-weight:600;"
                "  background:transparent; }"
                "QComboBox#dbnChartType { background:%5; color:%6; border:1px solid %7; font-family:%3;"
                "  font-size:%4px; padding:0 6px; min-height:26px; }"
                "QComboBox#dbnChartType::drop-down { border:none; }"
                "QComboBox#dbnChartType QAbstractItemView { background:%5; color:%6; border:1px solid %7; }")
            .arg(ui::colors::tint(ui::colors::AMBER, 0.25), ui::colors::TEXT_SECONDARY(),
                 QString(ui::fonts::DATA_FAMILY), ui::fonts::px(-3), ui::colors::BG_HOVER(),
                 ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED()));

    auto* toggle_hl = new QHBoxLayout(toggle_bar);
    toggle_hl->setContentsMargins(8, 0, 8, 0);
    toggle_hl->setSpacing(4);

    single_btn_ = new QPushButton(tr("SINGLE"), toggle_bar);
    single_btn_->setObjectName("dbnModeBtn");
    single_btn_->setCheckable(true);
    single_btn_->setChecked(true);

    compare_btn_ = new QPushButton(tr("COMPARE"), toggle_bar);
    compare_btn_->setObjectName("dbnModeBtn");
    compare_btn_->setCheckable(true);

    auto* mode_group = new QButtonGroup(toggle_bar);
    mode_group->setExclusive(true);
    mode_group->addButton(single_btn_);
    mode_group->addButton(compare_btn_);
    connect(single_btn_, &QPushButton::clicked, this, [this]() { set_view_mode(services::DbnViewMode::Single); });
    connect(compare_btn_, &QPushButton::clicked, this,
            [this]() { set_view_mode(services::DbnViewMode::Comparison); });

    toggle_hl->addWidget(single_btn_);
    toggle_hl->addWidget(compare_btn_);
    toggle_hl->addStretch();

    chart_type_label_ = new QLabel(tr("CHART:"), toggle_bar);
    chart_type_label_->setObjectName("dbnChartTypeLabel");

    chart_type_combo_ = new QComboBox(toggle_bar);
    chart_type_combo_->setObjectName("dbnChartType");
    chart_type_combo_->addItems({tr("LINE"), tr("AREA"), tr("BAR"), tr("SCATTER")});

    connect(chart_type_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &DBnomicsScreen::on_chart_type_changed);

    toggle_hl->addWidget(chart_type_label_);
    toggle_hl->addWidget(chart_type_combo_);

    right_vl->addWidget(toggle_bar);

    // view_stack_
    view_stack_ = new QStackedWidget(right_widget);

    // Page 0: single view — chart + table (draggable splitter) | skeleton | designed state
    single_stack_ = new QStackedWidget(view_stack_);

    single_content_ = new QWidget(single_stack_);
    auto* single_vl = new QVBoxLayout(single_content_);
    single_vl->setContentsMargins(0, 0, 0, 0);
    single_vl->setSpacing(0);

    auto* single_splitter = new QSplitter(Qt::Vertical, single_content_);
    single_splitter->setHandleWidth(5);
    single_splitter->setChildrenCollapsible(false);
    single_splitter->setStyleSheet(QString("QSplitter::handle:vertical {"
                                           "  background: %1; height: 5px;"
                                           "  border-top: 1px solid %2; border-bottom: 1px solid %2; }"
                                           "QSplitter::handle:vertical:hover { background: %3; }")
                                       .arg(ui::colors::BORDER_DIM(), ui::colors::BORDER_MED(), ui::colors::AMBER()));

    chart_widget_ = new DBnomicsChartWidget(single_splitter);
    data_table_ = new DBnomicsDataTable(single_splitter);
    chart_widget_->setMinimumHeight(120);
    data_table_->setMinimumHeight(150);

    single_splitter->addWidget(chart_widget_);
    single_splitter->addWidget(data_table_);
    single_splitter->setStretchFactor(0, 3); // chart ~65 %
    single_splitter->setStretchFactor(1, 2); // table ~35 %

    single_vl->addWidget(single_splitter);
    single_stack_->addWidget(single_content_);

    single_skeleton_ = new ui::LoadingSkeleton(ui::LoadingSkeleton::Shape::Chart, single_stack_);
    single_skeleton_->set_message(tr("Loading…"));
    single_stack_->addWidget(single_skeleton_);

    single_state_ = new ui::EmptyState(single_stack_);
    connect(single_state_, &ui::EmptyState::primary_clicked, this, [this]() {
        if (!single_failed_) {
            load_example_series();
            return;
        }
        // RETRY the series that failed.
        const QString id = failed_series_id_;
        single_failed_ = false;
        if (failed_was_restore_) {
            restore_single_ << id;
            watch_series(id);
            const QString topic = series_topic(id);
            if (!topic.isEmpty())
                datahub::DataHub::instance().request(topic, /*force=*/true);
            update_single_page();
        } else {
            QString prov, ds, code;
            if (dbn_split_series_id(id, &prov, &ds, &code))
                on_series_selected(prov, ds, code);
            else
                update_single_page();
        }
    });
    connect(single_state_, &ui::EmptyState::secondary_clicked, this, &DBnomicsScreen::load_example_series);
    single_stack_->addWidget(single_state_);
    // Until the first show decides (remembered state or the example), the chart area is loading.
    single_stack_->setCurrentWidget(single_skeleton_);

    view_stack_->addWidget(single_stack_); // index 0

    // Page 1: comparison view — scrollable slot cards | designed state
    compare_stack_ = new QStackedWidget(view_stack_);

    compare_grid_page_ = new QWidget(compare_stack_);
    compare_grid_page_->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    auto* compare_vl = new QVBoxLayout(compare_grid_page_);
    compare_vl->setContentsMargins(0, 0, 0, 0);
    compare_vl->setSpacing(0);

    auto* compare_scroll = new QScrollArea(compare_grid_page_);
    compare_scroll->setWidgetResizable(true);
    compare_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    compare_scroll->setStyleSheet(QString("QScrollArea { border: none; background: %1; }"
                                          "QScrollBar:vertical { background: %1; width: 6px; }"
                                          "QScrollBar::handle:vertical { background: %2; }"
                                          "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
                                      .arg(ui::colors::BG_BASE())
                                      .arg(ui::colors::BORDER_DIM()));

    comparison_content_ = new QWidget(compare_scroll);
    comparison_content_->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    comparison_layout_ = new QVBoxLayout(comparison_content_);
    comparison_layout_->setContentsMargins(8, 8, 8, 8);
    comparison_layout_->setSpacing(8);

    compare_scroll->setWidget(comparison_content_);
    compare_vl->addWidget(compare_scroll);
    compare_stack_->addWidget(compare_grid_page_);

    compare_empty_ = new ui::EmptyState(compare_stack_);
    connect(compare_empty_, &ui::EmptyState::primary_clicked, this, &DBnomicsScreen::on_add_slot);
    connect(compare_empty_, &ui::EmptyState::secondary_clicked, this, &DBnomicsScreen::load_example_series);
    compare_stack_->addWidget(compare_empty_);

    view_stack_->addWidget(compare_stack_); // index 1
    view_stack_->setCurrentIndex(0);
    rebuild_comparison_view(); // the designed "nothing to compare" page until a slot exists

    right_vl->addWidget(view_stack_, 1);
    content_hl->addWidget(right_widget, 1);
    root->addWidget(content_widget, 1);

    // ── Status bar (28px) ─────────────────────────────────────────────────────
    auto* status_bar = new QWidget(this);
    status_bar->setMinimumHeight(28);
    status_bar->setStyleSheet(
        QString("background:%1; border-top:1px solid %2;").arg(ui::colors::BG_RAISED()).arg(ui::colors::BORDER_DIM()));

    auto* status_hl = new QHBoxLayout(status_bar);
    status_hl->setContentsMargins(12, 0, 12, 0);
    status_hl->setSpacing(0);

    status_label_ = new QLabel(tr("Ready"), status_bar);
    status_label_->setStyleSheet(QString("color:%1; font-family:%2; font-size:" + fincept::ui::fonts::px(-3) + "px;")
                                     .arg(ui::colors::TEXT_SECONDARY())
                                     .arg(ui::fonts::DATA_FAMILY));
    // Long statuses (series names, errors) are clipped, never widening the screen; full text in the tooltip.
    status_label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    stats_label_ = new QLabel("", status_bar);
    stats_label_->setStyleSheet(QString("color:%1; font-family:%2; font-size:" + fincept::ui::fonts::px(-3) + "px;")
                                    .arg(ui::colors::TEXT_SECONDARY()) // on the raised status bar
                                    .arg(ui::fonts::DATA_FAMILY));
    stats_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // Source attribution — an economics screen must say where its numbers come
    // from and under what terms.
    attribution_label_ = new QLabel(tr("Source: DBnomics (db.nomics.world) — aggregated public data, "
                                       "terms per originating provider"),
                                    status_bar);
    attribution_label_->setStyleSheet(QString("color:%1; font-family:%2; font-size:" + fincept::ui::fonts::px(-4) + "px;")
                                          .arg(ui::colors::TEXT_SECONDARY()) // on the raised status bar
                                          .arg(ui::fonts::DATA_FAMILY));
    attribution_label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    attribution_label_->setToolTip(attribution_label_->text());

    status_hl->addWidget(status_label_, 3);
    status_hl->addSpacing(14);
    status_hl->addWidget(attribution_label_, 2);
    status_hl->addSpacing(14);
    status_hl->addWidget(stats_label_);

    root->addWidget(status_bar);
}

// ─────────────────────────────────────────────────────────────────────────────
// build_toolbar
// ─────────────────────────────────────────────────────────────────────────────
QWidget* DBnomicsScreen::build_toolbar() {
    auto* toolbar = new QWidget(this);
    toolbar->setMinimumHeight(44);
    toolbar->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;")
                               .arg(ui::colors::BG_RAISED())
                               .arg(ui::colors::BORDER_MED()));

    auto* hl = new QHBoxLayout(toolbar);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(8);

    toolbar_title_ = new QLabel(tr("DBNOMICS TERMINAL"), toolbar);
    toolbar_title_->setStyleSheet(QString("color:%1; font-family:%2; font-size:" + fincept::ui::fonts::px(0) + "px; font-weight:700;")
                                      .arg(ui::colors::AMBER())
                                      .arg(ui::fonts::DATA_FAMILY));

    hl->addWidget(toolbar_title_);
    hl->addStretch();

    const QString btn_style = QString("QPushButton { background:%4; color:%1;"
                                      " border:1px solid %2; font-family:%3; font-size:" + fincept::ui::fonts::px(-2) + "px; font-weight:600;"
                                      " padding:0 10px; min-height:28px; }"
                                      "QPushButton:hover { background:%5; }"
                                      "QPushButton:pressed { background:%6; }")
                                  .arg(ui::colors::AMBER())
                                  .arg(ui::colors::AMBER_DIM())
                                  .arg(ui::fonts::DATA_FAMILY)
                                  .arg(ui::colors::ACCENT_BG())
                                  .arg(ui::colors::tint(ui::colors::AMBER, 0.2))
                                  .arg(ui::colors::tint(ui::colors::AMBER, 0.3));

    fetch_btn_ = new QPushButton(tr("FETCH"), toolbar);
    fetch_btn_->setStyleSheet(btn_style);
    connect(fetch_btn_, &QPushButton::clicked, this, &DBnomicsScreen::on_fetch_clicked);

    refresh_btn_ = new QPushButton(tr("REFRESH"), toolbar);
    refresh_btn_->setStyleSheet(btn_style);
    connect(refresh_btn_, &QPushButton::clicked, this, &DBnomicsScreen::on_refresh_clicked);

    export_btn_ = new QPushButton(tr("EXPORT CSV"), toolbar);
    export_btn_->setStyleSheet(btn_style);
    connect(export_btn_, &QPushButton::clicked, this, &DBnomicsScreen::on_export_csv);

    hl->addWidget(fetch_btn_);
    hl->addWidget(refresh_btn_);
    hl->addWidget(export_btn_);

    return toolbar;
}

// ─────────────────────────────────────────────────────────────────────────────
// set_status
// ─────────────────────────────────────────────────────────────────────────────
void DBnomicsScreen::set_status(const QString& message) {
    if (status_label_) {
        status_label_->setText(message);
        status_label_->setToolTip(message);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Live language switch
// ─────────────────────────────────────────────────────────────────────────────
void DBnomicsScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void DBnomicsScreen::retranslateUi() {
    // Toolbar
    if (toolbar_title_)
        toolbar_title_->setText(tr("DBNOMICS TERMINAL"));
    if (fetch_btn_)
        fetch_btn_->setText(tr("FETCH"));
    if (refresh_btn_)
        refresh_btn_->setText(tr("REFRESH"));
    if (export_btn_)
        export_btn_->setText(tr("EXPORT CSV"));

    // View toggle
    if (single_btn_)
        single_btn_->setText(tr("SINGLE"));
    if (compare_btn_)
        compare_btn_->setText(tr("COMPARE"));
    if (chart_type_label_)
        chart_type_label_->setText(tr("CHART:"));
    if (attribution_label_) {
        attribution_label_->setText(tr("Source: DBnomics (db.nomics.world) — aggregated public data, "
                                       "terms per originating provider"));
        attribution_label_->setToolTip(attribution_label_->text());
    }

    // Chart-type combo — setItemText preserves the current selection/index.
    if (chart_type_combo_ && chart_type_combo_->count() >= 4) {
        chart_type_combo_->setItemText(0, tr("LINE"));
        chart_type_combo_->setItemText(1, tr("AREA"));
        chart_type_combo_->setItemText(2, tr("BAR"));
        chart_type_combo_->setItemText(3, tr("SCATTER"));
    }

    // Designed states carry translated text: rebuild them.
    if (first_show_done_)
        update_single_page();
    if (slots_.isEmpty())
        rebuild_comparison_view();

    // Comparison slot cards: re-apply per-card chrome. SLOT number + series
    // count follow the slots_ order; combo items map by index.
    for (int i = 0; i < slot_cards_.size(); ++i) {
        QComboBox* combo = slot_cards_[i].chart_combo;
        if (combo && combo->count() >= 4) {
            combo->setItemText(0, tr("LINE"));
            combo->setItemText(1, tr("AREA"));
            combo->setItemText(2, tr("BAR"));
            combo->setItemText(3, tr("SCATTER"));
        }
        QWidget* header = combo ? combo->parentWidget() : nullptr;
        if (!header)
            continue;
        if (auto* lbl = header->findChild<QLabel*>(QStringLiteral("dbnCardSlotLabel")))
            lbl->setText(tr("SLOT %1").arg(i + 1));
        if (auto* cnt = header->findChild<QLabel*>(QStringLiteral("dbnCardCountLabel"))) {
            const int n = (i < slots_.size()) ? slots_[i].series.size() : 0;
            cnt->setText(tr("(%1 series)").arg(n));
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SINGLE view
// ─────────────────────────────────────────────────────────────────────────────
QVector<services::DbnDataPoint> DBnomicsScreen::single_view_series() const {
    QVector<services::DbnDataPoint> out = single_series_;
    if (has_preview_) {
        const bool kept = std::any_of(out.cbegin(), out.cend(), [this](const services::DbnDataPoint& s) {
            return s.series_id == preview_.series_id;
        });
        if (!kept)
            out.append(preview_);
    }
    return out;
}

void DBnomicsScreen::render_single_view() {
    const QVector<services::DbnDataPoint> shown = single_view_series();
    if (shown.isEmpty()) {
        chart_widget_->clear();
        data_table_->clear();
    } else {
        chart_widget_->set_data(shown, single_chart_type_);
        data_table_->set_data(shown);
    }
    update_single_page();
}

void DBnomicsScreen::update_single_page() {
    if (!single_stack_)
        return;
    if (!single_view_series().isEmpty()) {
        single_stack_->setCurrentWidget(single_content_);
        return;
    }
    if (selection_loading_ || (!restore_single_.isEmpty() && !single_failed_)) {
        single_skeleton_->set_message(selection_loading_
                                          ? tr("Loading %1 from DBnomics…").arg(pending_series_id_)
                                          : tr("Restoring %n series…", nullptr, int(restore_single_.size())));
        single_stack_->setCurrentWidget(single_skeleton_);
        return;
    }
    ui::EmptyState::Spec spec;
    if (single_failed_) {
        spec = ui::EmptyState::error_spec(single_error_, tr("Could not load %1").arg(failed_series_id_), tr("RETRY"));
        spec.secondary = tr("LOAD EXAMPLE SERIES");
    } else {
        spec = ui::EmptyState::no_data_spec(tr("No series on the chart yet"),
                                            tr("Pick a provider, a dataset and a series on the left — the series "
                                               "you click is charted here at once. Or start from an example."),
                                            tr("LOAD EXAMPLE SERIES"));
        spec.details = {{tr("Provider"), dbn_example_provider()},
                        {tr("Dataset"), tr("World Economic Outlook (latest)")},
                        {tr("Series"), tr("United States · real GDP growth")}};
        spec.details_heading = tr("THE EXAMPLE");
        spec.illustration = true;
        spec.illustration_shape = ui::LoadingSkeleton::Shape::Chart;
    }
    single_state_->apply(spec);
    single_stack_->setCurrentWidget(single_state_);
}

// ─────────────────────────────────────────────────────────────────────────────
// assign_series_colors
// ─────────────────────────────────────────────────────────────────────────────
void DBnomicsScreen::assign_series_colors() {
    for (int i = 0; i < single_series_.size(); ++i)
        single_series_[i].color = ui::colors::chart_color(i);
    if (has_preview_)
        preview_.color = ui::colors::chart_color(static_cast<int>(single_series_.size()));
}

// ─────────────────────────────────────────────────────────────────────────────
// Service signal slots
// ─────────────────────────────────────────────────────────────────────────────
void DBnomicsScreen::on_providers_loaded(const QVector<services::DbnProvider>& providers) {
    provider_count_ = providers.size();
    selection_panel_->set_providers_loading(false);
    selection_panel_->populate_providers(providers);
    stats_label_->setText(tr("%1 providers").arg(provider_count_));
    set_status(tr("Loaded %1 providers").arg(provider_count_));
    LOG_DEBUG("DBnomicsScreen", QString("Providers loaded: %1").arg(provider_count_));
}

void DBnomicsScreen::on_datasets_loaded(const QString& provider_code, const QVector<services::DbnDataset>& datasets,
                                        const services::DbnPagination& page, bool append) {
    // Broadcast signal: a slow answer for a provider the user already left (or an MCP tool's request)
    // must not replace the list shown under the current provider.
    if (provider_code != selection_panel_->selected_provider())
        return;
    if (!append)
        selection_panel_->set_datasets_loading(false);
    selection_panel_->populate_datasets(datasets, page, append);
}

void DBnomicsScreen::on_series_loaded(const QString& provider_code, const QString& dataset_code, const QString& query,
                                      const QVector<services::DbnSeriesInfo>& series,
                                      const services::DbnPagination& page, bool append) {
    if (provider_code != selection_panel_->selected_provider() ||
        dataset_code != selection_panel_->selected_dataset() ||
        query != selection_panel_->series_search_text().trimmed())
        return; // superseded selection / search text, or another caller's request
    // A list next to an empty chart: its first series is shown at once (nothing waits for a click).
    const bool pick_first = !append && query.isEmpty() && !series.isEmpty() && isVisible() &&
                            selection_panel_->selected_series().isEmpty() && pending_series_id_.isEmpty() &&
                            restore_single_.isEmpty() && single_view_series().isEmpty();
    if (pick_first)
        selection_panel_->restore_selection(provider_code, dataset_code, series.first().code);
    if (!append)
        selection_panel_->set_series_loading(false);
    selection_panel_->populate_series(series, page, append);
    set_status(tr("Loaded %1 series").arg(series.size()));
    if (pick_first)
        on_series_selected(provider_code, dataset_code, series.first().code);
}

void DBnomicsScreen::on_observations_loaded(const services::DbnDataPoint& point) {
    // Hub deliveries also cover refreshes of series that are already on screen. Only the series the
    // user just picked becomes the "current" one for ADD TO SINGLE VIEW / ADD TO SLOT, and only its
    // arrival ends the loading state.
    const bool is_selection = point.series_id == pending_series_id_;
    if (is_selection) {
        last_loaded_data_ = point;
        has_pending_data_ = true;
        selection_loading_ = false;
        single_failed_ = false;
    }

    // Update every place this series is already shown — single view AND any
    // comparison slot (slots used to be skipped, so REFRESH silently left
    // stale numbers on screen in COMPARE mode).
    // Refresh replaces the whole data point (observations AND metadata) but keeps the colour.
    auto refresh_into = [&point](services::DbnDataPoint& s) {
        const QColor color = s.color;
        s = point;
        s.color = color;
    };
    bool in_view = false;
    for (auto& s : single_series_) {
        if (s.series_id == point.series_id) {
            refresh_into(s);
            in_view = true;
        }
    }
    if (has_preview_ && preview_.series_id == point.series_id) {
        refresh_into(preview_);
        in_view = true;
    }
    bool in_slot = false;
    for (auto& slot : slots_) {
        for (auto& s : slot.series) {
            if (s.series_id == point.series_id) {
                refresh_into(s);
                in_slot = true;
            }
        }
    }
    // Series remembered from the last session come back into their view as their data arrives.
    if (restore_single_.removeAll(point.series_id) > 0) {
        if (!std::any_of(single_series_.cbegin(), single_series_.cend(),
                         [&point](const services::DbnDataPoint& s) { return s.series_id == point.series_id; }))
            single_series_.append(point);
        in_view = true;
    }
    for (int i = 0; i < restore_slots_.size() && i < slots_.size(); ++i) {
        if (restore_slots_[i].removeAll(point.series_id) > 0) {
            slots_[i].series.append(point);
            for (int k = 0; k < slots_[i].series.size(); ++k)
                slots_[i].series[k].color = ui::colors::chart_color(k);
            selection_panel_->update_slot_series(i, slots_[i].series);
            in_slot = true;
        }
    }
    // The series the user clicked is charted at once, as the preview (ADD TO SINGLE VIEW keeps it;
    // the next click replaces it).
    if (is_selection && !std::any_of(single_series_.cbegin(), single_series_.cend(),
                                     [&point](const services::DbnDataPoint& s) {
                                         return s.series_id == point.series_id;
                                     })) {
        if (!has_preview_ || preview_.series_id != point.series_id) {
            preview_ = point;
            has_preview_ = true;
        }
        in_view = true;
    }
    if (in_view) {
        assign_series_colors();
        render_single_view();
    }
    if (in_slot)
        rebuild_comparison_view();
    if (!is_selection)
        return;

    // Say what arrived (vintage included) and how to keep it.
    QString latest;
    for (auto it = point.observations.crbegin(); it != point.observations.crend(); ++it) {
        if (it->valid) {
            latest = it->period;
            break;
        }
    }
    const bool kept = std::any_of(single_series_.cbegin(), single_series_.cend(),
                                  [&point](const services::DbnDataPoint& s) { return s.series_id == point.series_id; });
    const QString facts = tr("%1  •  %2 obs, latest %3  •  DBnomics indexed %4")
                              .arg(point.series_name)
                              .arg(point.observations.size())
                              .arg(latest.isEmpty() ? QStringLiteral("—") : latest,
                                   point.indexed_at.isEmpty() ? QStringLiteral("—") : point.indexed_at.left(10));
    set_status(kept ? tr("Loaded: %1").arg(facts)
                    : tr("Showing: %1  •  ADD TO SINGLE VIEW keeps it on the chart").arg(facts));
    LOG_DEBUG("DBnomicsScreen", QString("Observations loaded for: %1").arg(point.series_id));
}

void DBnomicsScreen::on_search_results_loaded(const QString& query,
                                              const QVector<services::DbnSearchResult>& results,
                                              const services::DbnPagination& page, bool append) {
    if (query.trimmed() != selection_panel_->global_search_text().trimmed())
        return; // results for text the user has since changed
    selection_panel_->set_search_loading(false);
    selection_panel_->populate_search_results(results, page, append);
}

void DBnomicsScreen::on_observations_failed(const QString& series_id, const QString& message) {
    // Only the series the user is waiting for (or a remembered one being restored) ends the loading
    // state; a failed background refresh of a series already on screen keeps its last good data.
    const bool restoring = restore_single_.contains(series_id);
    if (series_id == pending_series_id_ || restoring) {
        if (series_id == pending_series_id_)
            selection_loading_ = false;
        if (restoring)
            restore_single_.removeAll(series_id);
        if (single_view_series().isEmpty()) {
            single_failed_ = true;
            single_error_ = message;
            failed_series_id_ = series_id;
            failed_was_restore_ = restoring;
        }
        set_status(tr("ERROR loading %1: %2").arg(series_id, message));
        update_single_page();
    } else {
        set_status(tr("Refresh of %1 failed (%2) — showing the last loaded data").arg(series_id, message));
    }
    LOG_WARN("DBnomicsScreen", QString("Observations failed for %1: %2").arg(series_id, message));
}

void DBnomicsScreen::on_service_error(const QString& context, const QString& message) {
    if (context == QLatin1String("observations"))
        return; // handled per series by on_observations_failed()
    // The service has logged the failure already (and the hub, for refreshes).
    LOG_DEBUG("DBnomicsScreen", QString("[%1] %2").arg(context, message));
    set_status(tr("ERROR [%1]: %2").arg(context, message));
    // Stop any active loading state so it doesn't hang on error
    selection_panel_->set_providers_loading(false);
    selection_panel_->set_datasets_loading(false);
    selection_panel_->set_series_loading(false);
    selection_panel_->set_search_loading(false);
    // The list that failed says so, instead of staying empty.
    if (context == QLatin1String("providers") && provider_count_ == 0)
        selection_panel_->show_list_hint(DBnomicsSelectionPanel::ListKind::Providers,
                                         tr("Providers did not load — press FETCH to retry"));
    else if (context == QLatin1String("datasets"))
        selection_panel_->show_list_hint(DBnomicsSelectionPanel::ListKind::Datasets,
                                         tr("Datasets did not load — pick the provider again to retry"));
    else if (context == QLatin1String("series"))
        selection_panel_->show_list_hint(DBnomicsSelectionPanel::ListKind::Series,
                                         tr("Series did not load — pick the dataset again to retry"));
}

// ─────────────────────────────────────────────────────────────────────────────
// Toolbar action slots
// ─────────────────────────────────────────────────────────────────────────────
void DBnomicsScreen::on_fetch_clicked() {
    set_status(tr("Fetching providers..."));
    selection_panel_->set_providers_loading(true);
    services::DBnomicsService::instance().fetch_providers();
}

void DBnomicsScreen::on_refresh_clicked() {
    // Refresh every loaded series, in BOTH views — comparison slots used to be
    // ignored entirely, so REFRESH looked like a no-op in COMPARE mode.
    QSet<QString> seen;
    QStringList topics;
    auto refresh_one = [&](const services::DbnDataPoint& s) {
        if (seen.contains(s.series_id))
            return;
        seen.insert(s.series_id);
        const QString topic = series_topic(s.series_id);
        if (topic.isEmpty())
            return;
        watch_series(s.series_id);
        topics << topic;
    };

    for (const auto& s : single_view_series())
        refresh_one(s);
    for (const auto& slot : std::as_const(slots_))
        for (const auto& s : slot.series)
            refresh_one(s);

    if (topics.isEmpty()) {
        set_status(tr("Nothing to refresh — add a series first"));
        return;
    }
    // force=true: a user-driven refresh bypasses the topic's min_interval (per-producer rate
    // limiting still applies).
    datahub::DataHub::instance().request(topics, /*force=*/true);
    set_status(tr("Refreshing %1 series…").arg(topics.size()));
}

void DBnomicsScreen::on_export_csv() {
    // Export whatever the user is actually looking at. Previously COMPARE mode
    // always reported "No data to export" even with every slot populated,
    // because only single_series_ was ever considered.
    QVector<services::DbnDataPoint> export_series;
    if (view_mode_ == services::DbnViewMode::Comparison) {
        for (int i = 0; i < slots_.size(); ++i) {
            for (const auto& s : std::as_const(slots_[i].series)) {
                services::DbnDataPoint copy = s;
                copy.series_name = tr("Slot %1: %2").arg(i + 1).arg(s.series_name);
                export_series.append(copy);
            }
        }
    } else {
        export_series = single_view_series();
    }

    if (export_series.isEmpty()) {
        set_status(tr("No data to export"));
        return;
    }

    const QString default_name =
        QString("dbnomics_%1.csv").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));

    const QString path = QFileDialog::getSaveFileName(this, tr("Export CSV"), default_name, tr("CSV Files (*.csv)"));

    if (path.isEmpty())
        return;

    // Written on a worker (a long daily series is tens of thousands of lines); the status line says
    // how it went.
    set_status(tr("Exporting CSV…"));
    QPointer<DBnomicsScreen> self = this;
    const QString exported_at = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    (void)QtConcurrent::run(dbn_export_pool(),
                            [export_series, path, exported_at]() {
                                return dbn_write_csv(path, export_series, exported_at);
                            })
        .then(this, [self, path](bool written) {
            if (!self)
                return;
            if (!written) {
                self->set_status(tr("Failed to open file: %1").arg(path));
                LOG_ERROR("DBnomicsScreen", QString("Cannot open file for CSV export: %1").arg(path));
                return;
            }
            self->set_status(tr("Exported CSV: %1").arg(path));
            LOG_INFO("DBnomicsScreen", QString("CSV exported to: %1").arg(path));
        });
}

// ─────────────────────────────────────────────────────────────────────────────
// Selection panel navigation slots
// ─────────────────────────────────────────────────────────────────────────────
void DBnomicsScreen::on_provider_selected(const QString& code) {
    set_status(tr("Loading datasets for %1...").arg(code));
    selection_panel_->set_datasets_loading(true);
    services::DBnomicsService::instance().fetch_datasets(code, 0);
    ScreenStateManager::instance().notify_changed(this);
}

void DBnomicsScreen::on_dataset_selected(const QString& code) {
    set_status(tr("Loading series for %1...").arg(code));
    selection_panel_->set_series_loading(true);
    // With the series search text the panel shows (empty after a dataset click): on_series_loaded()
    // only accepts the list for the text in the box.
    services::DBnomicsService::instance().fetch_series(selection_panel_->selected_provider(), code,
                                                       selection_panel_->series_search_text().trimmed(), 0);
}

void DBnomicsScreen::on_series_selected(const QString& prov, const QString& ds, const QString& code) {
    const QString series_id = prov + "/" + ds + "/" + code;
    const QString topic = series_topic(series_id);
    if (topic.isEmpty()) {
        set_status(tr("ERROR [observations]: unsupported series id %1").arg(series_id));
        LOG_WARN("DBnomicsScreen", "Series id cannot be addressed through the hub: " + series_id);
        return;
    }
    set_status(tr("Fetching observations for %1…").arg(series_id));
    pending_series_id_ = series_id;
    selection_loading_ = true;
    single_failed_ = false;
    unwatch_unused_series(); // a previously browsed series that never made it into a view stops refreshing
    update_single_page();    // chart skeleton while nothing else is on the chart
    // Subscribe first (a still-fresh cached set is delivered at once), then ask for a refresh.
    watch_series(series_id);
    datahub::DataHub::instance().request(topic, /*force=*/true);
    ScreenStateManager::instance().notify_changed(this);
}

// ─────────────────────────────────────────────────────────────────────────────
// Chart type
// ─────────────────────────────────────────────────────────────────────────────
void DBnomicsScreen::on_chart_type_changed(int index) {
    switch (index) {
        case 0:
            single_chart_type_ = services::DbnChartType::Line;
            break;
        case 1:
            single_chart_type_ = services::DbnChartType::Area;
            break;
        case 2:
            single_chart_type_ = services::DbnChartType::Bar;
            break;
        case 3:
            single_chart_type_ = services::DbnChartType::Scatter;
            break;
        default:
            single_chart_type_ = services::DbnChartType::Line;
            break;
    }
    render_single_view();
}

// ─────────────────────────────────────────────────────────────────────────────
// View management slots
// ─────────────────────────────────────────────────────────────────────────────
void DBnomicsScreen::set_view_mode(services::DbnViewMode mode) {
    view_mode_ = mode;
    single_btn_->setChecked(mode == services::DbnViewMode::Single);
    compare_btn_->setChecked(mode == services::DbnViewMode::Comparison);
    if (mode == services::DbnViewMode::Comparison && slots_.isEmpty()) {
        // COMPARE with no slot starts slot 1 from what the SINGLE view shows (nothing to fetch).
        const QVector<services::DbnDataPoint> shown = single_view_series();
        if (!shown.isEmpty()) {
            slots_.append(services::DbnSlot{});
            selection_panel_->add_comparison_slot();
            slots_[0].series = shown;
            for (int k = 0; k < slots_[0].series.size(); ++k)
                slots_[0].series[k].color = ui::colors::chart_color(k);
            selection_panel_->update_slot_series(0, slots_[0].series);
            rebuild_comparison_view();
            set_status(tr("Slot 1 starts with the SINGLE view's series — + ADD SLOT for another chart"));
        }
    }
    view_stack_->setCurrentIndex(mode == services::DbnViewMode::Comparison ? 1 : 0);
    ScreenStateManager::instance().notify_changed(this);
}

void DBnomicsScreen::on_add_to_single_view() {
    if (!has_pending_data_) {
        set_status(tr("No data loaded — click a series first"));
        return;
    }

    // Check duplicate
    for (const auto& s : std::as_const(single_series_)) {
        if (s.series_id == last_loaded_data_.series_id) {
            set_status(tr("Series already kept on the chart"));
            return;
        }
    }

    single_series_.append(last_loaded_data_);
    if (has_preview_ && preview_.series_id == last_loaded_data_.series_id)
        has_preview_ = false; // the preview is now kept
    assign_series_colors();
    render_single_view();

    if (view_mode_ != services::DbnViewMode::Single)
        set_view_mode(services::DbnViewMode::Single);

    set_status(tr("Kept: %1 — click another series to see it beside this one").arg(last_loaded_data_.series_name));
    LOG_DEBUG("DBnomicsScreen", QString("Series added to single view: %1").arg(last_loaded_data_.series_id));
}

void DBnomicsScreen::on_clear_all() {
    single_series_.clear();
    has_preview_ = false;
    slots_.clear();
    restore_single_.clear();
    restore_slots_.clear();
    has_pending_data_ = false;
    selection_loading_ = false;
    single_failed_ = false;
    pending_series_id_.clear();
    unwatch_unused_series();
    selection_panel_->clear_slots();
    render_single_view();
    rebuild_comparison_view();
    set_status(tr("Cleared"));
    LOG_DEBUG("DBnomicsScreen", "All series cleared");
}

void DBnomicsScreen::on_add_slot() {
    slots_.append(services::DbnSlot{});
    selection_panel_->add_comparison_slot();
    rebuild_comparison_view();
    if (view_mode_ != services::DbnViewMode::Comparison)
        set_view_mode(services::DbnViewMode::Comparison);
    LOG_DEBUG("DBnomicsScreen", QString("Comparison slot added. Total slots: %1").arg(slots_.size()));
}

void DBnomicsScreen::on_add_to_slot(int slot_index) {
    if (!has_pending_data_) {
        set_status(tr("No data loaded — click a series on the left first"));
        return;
    }

    if (slot_index >= slots_.size())
        slots_.resize(slot_index + 1);

    // Check duplicate in slot
    for (const auto& s : std::as_const(slots_[slot_index].series)) {
        if (s.series_id == last_loaded_data_.series_id)
            return;
    }

    slots_[slot_index].series.append(last_loaded_data_);

    // Assign colors for this slot's series
    for (int i = 0; i < slots_[slot_index].series.size(); ++i)
        slots_[slot_index].series[i].color = ui::colors::chart_color(i);

    selection_panel_->update_slot_series(slot_index, slots_[slot_index].series);
    rebuild_comparison_view();

    set_status(tr("Added %1 to slot %2").arg(last_loaded_data_.series_name).arg(slot_index + 1));
    LOG_DEBUG("DBnomicsScreen",
              QString("Series added to slot %1: %2").arg(slot_index).arg(last_loaded_data_.series_id));
}

// ─────────────────────────────────────────────────────────────────────────────
// Comparison view slot management
// ─────────────────────────────────────────────────────────────────────────────
void DBnomicsScreen::on_remove_from_slot(int slot_index, const QString& series_id) {
    if (slot_index < 0 || slot_index >= slots_.size())
        return;

    auto& slot = slots_[slot_index];
    slot.series.erase(
        std::remove_if(slot.series.begin(), slot.series.end(),
                       [&series_id](const services::DbnDataPoint& dp) { return dp.series_id == series_id; }),
        slot.series.end());

    // Re-assign colors after removal
    for (int i = 0; i < slot.series.size(); ++i)
        slot.series[i].color = ui::colors::chart_color(i);

    selection_panel_->update_slot_series(slot_index, slot.series);
    rebuild_comparison_view();
    unwatch_unused_series();

    set_status(tr("Removed series from slot %1").arg(slot_index + 1));
    LOG_DEBUG("DBnomicsScreen", QString("Removed %1 from slot %2").arg(series_id).arg(slot_index));
}

void DBnomicsScreen::on_remove_slot(int slot_index) {
    if (slot_index < 0 || slot_index >= slots_.size())
        return;
    slots_.removeAt(slot_index);
    if (slot_index < restore_slots_.size())
        restore_slots_.removeAt(slot_index);
    selection_panel_->remove_comparison_slot(slot_index);
    rebuild_comparison_view();
    unwatch_unused_series();
    set_status(tr("Slot removed. %1 slot(s) remaining").arg(slots_.size()));
    LOG_DEBUG("DBnomicsScreen", QString("Slot %1 removed. Remaining: %2").arg(slot_index + 1).arg(slots_.size()));
}

// ─────────────────────────────────────────────────────────────────────────────
// rebuild_comparison_view — 2-column grid, 4 charts visible at once
// ─────────────────────────────────────────────────────────────────────────────
void DBnomicsScreen::rebuild_comparison_view() {
    if (!comparison_layout_)
        return;

    // Clear every item
    while (comparison_layout_->count() > 0) {
        QLayoutItem* item = comparison_layout_->takeAt(0);
        if (item) {
            if (item->widget())
                item->widget()->deleteLater();
            delete item;
        }
    }
    slot_cards_.clear();

    if (slots_.isEmpty()) {
        // Nothing to compare yet: say what a slot is and offer the first one (and the example).
        ui::EmptyState::Spec spec = ui::EmptyState::no_data_spec(
            tr("Nothing to compare yet"),
            tr("A comparison slot puts one or more series on its own chart, beside the other slots. Load a "
               "series, then add it to a slot — or start from the example."),
            tr("+ ADD SLOT"));
        spec.secondary = tr("LOAD EXAMPLE SERIES");
        spec.illustration = true;
        spec.illustration_shape = ui::LoadingSkeleton::Shape::Cards;
        compare_empty_->apply(spec);
        compare_stack_->setCurrentWidget(compare_empty_);
        return;
    }
    compare_stack_->setCurrentWidget(compare_grid_page_);

    // ── 2-column grid: up to 2 rows × 2 cols fit on screen without scrolling ─
    auto* grid_widget = new QWidget(comparison_content_);
    grid_widget->setObjectName("dbnSlotGrid");
    // One stylesheet for every card (objectName selectors) — it used to be six setStyleSheet() calls
    // per card inside the loop, a full CSS parse each, on every rebuild.
    grid_widget->setStyleSheet(
        QString("QWidget#dbnSlotGrid { background:%1; }"
                "QWidget#slotCard { background:%2; border:1px solid %3; }"
                "QWidget#cardHeader { background:%4; border-bottom:1px solid %3; }"
                "QLabel#dbnCardSlotLabel { color:%5; font-family:%6; font-size:" + fincept::ui::fonts::px(-2) + "px; font-weight:700;"
                "  background:transparent; border:none; }"
                "QLabel#dbnCardCountLabel { color:%7; font-family:%6; font-size:" + fincept::ui::fonts::px(-4) + "px; background:transparent;"
                "  border:none; }"
                "QComboBox#dbnCardCombo { background:%8; color:%9; border:1px solid %10; font-family:%6;"
                "  font-size:" + fincept::ui::fonts::px(-4) + "px; padding:0 4px; }"
                "QComboBox#dbnCardCombo::drop-down { border:none; }"
                "QComboBox#dbnCardCombo QAbstractItemView { background:%8; color:%9; border:1px solid %10; }"
                "QSplitter#dbnCardSplitter::handle:vertical { background:%3; height:4px; }"
                "QSplitter#dbnCardSplitter::handle:vertical:hover { background:%5; }")
            .arg(ui::colors::BG_BASE(), ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::BG_RAISED(),
                 ui::colors::AMBER(), QString(ui::fonts::DATA_FAMILY), ui::colors::TEXT_SECONDARY(),
                 ui::colors::BG_HOVER(), ui::colors::TEXT_PRIMARY())
            .arg(ui::colors::BORDER_MED()));
    auto* grid = new QGridLayout(grid_widget);
    grid->setContentsMargins(6, 6, 6, 6);
    grid->setSpacing(6);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);

    const int n = slots_.size();
    for (int i = 0; i < n; ++i) {
        const auto& slot = slots_[i];
        const int row = i / 2;
        const int col = i % 2;
        // Odd last card spans both columns for visual balance
        const int col_span = (i == n - 1 && n % 2 == 1) ? 2 : 1;

        // ── Card ─────────────────────────────────────────────────────────────
        auto* card = new QWidget(grid_widget);
        card->setMinimumHeight(260);
        card->setMinimumWidth(280); // room for the Y-axis labels; two columns still fit at 1280 px
        card->setObjectName("slotCard");

        auto* card_vl = new QVBoxLayout(card);
        card_vl->setContentsMargins(0, 0, 0, 0);
        card_vl->setSpacing(0);

        // ── Card header (32px) ────────────────────────────────────────────────
        auto* header = new QWidget(card);
        header->setMinimumHeight(32);
        header->setObjectName("cardHeader");

        auto* header_hl = new QHBoxLayout(header);
        header_hl->setContentsMargins(10, 0, 8, 0);
        header_hl->setSpacing(8);

        auto* slot_label = new QLabel(tr("SLOT %1").arg(i + 1), header);
        slot_label->setObjectName("dbnCardSlotLabel");
        header_hl->addWidget(slot_label);

        auto* count_label = new QLabel(tr("(%1 series)").arg(slot.series.size()), header);
        count_label->setObjectName("dbnCardCountLabel");
        header_hl->addWidget(count_label);
        header_hl->addStretch();

        auto* chart_combo = new QComboBox(header);
        chart_combo->addItems({tr("LINE"), tr("AREA"), tr("BAR"), tr("SCATTER")});
        chart_combo->setCurrentIndex(static_cast<int>(slot.chart_type));
        chart_combo->setMinimumHeight(22);
        chart_combo->setObjectName("dbnCardCombo");
        header_hl->addWidget(chart_combo);
        card_vl->addWidget(header);

        const int slot_i = i;
        if (slot.series.isEmpty()) {
            // An empty slot is a designed state with the action that fills it, not an empty chart.
            chart_combo->setEnabled(false);
            auto* empty = new ui::EmptyState(card);
            empty->set_compact(true);
            empty->apply(ui::EmptyState::no_data_spec(
                tr("Empty slot"),
                has_pending_data_ ? tr("Add the series you are looking at, or pick another on the left.")
                                  : tr("Pick a series on the left, then add it here."),
                has_pending_data_ ? tr("ADD CURRENT SERIES") : QString()));
            connect(empty, &ui::EmptyState::primary_clicked, this, [this, slot_i]() { on_add_to_slot(slot_i); });
            card_vl->addWidget(empty, 1);
            slot_cards_.append({nullptr, nullptr, chart_combo});
            grid->addWidget(card, row, col, 1, col_span);
            grid->setRowStretch(row, 1);
            continue;
        }

        // ── Chart + Table (draggable splitter) ────────────────────────────────
        auto* card_splitter = new QSplitter(Qt::Vertical, card);
        card_splitter->setHandleWidth(4);
        card_splitter->setChildrenCollapsible(false);
        card_splitter->setObjectName("dbnCardSplitter");

        auto* chart = new DBnomicsChartWidget(card_splitter);
        chart->set_compact(true); // tighter margins + no legend for grid slots
        auto* table = new DBnomicsDataTable(card_splitter);
        chart->setMinimumHeight(80);
        table->setMinimumHeight(80);
        chart->set_data(slot.series, slot.chart_type);
        table->set_data(slot.series);

        card_splitter->addWidget(chart);
        card_splitter->addWidget(table);
        card_splitter->setStretchFactor(0, 3);
        card_splitter->setStretchFactor(1, 2);

        card_vl->addWidget(card_splitter, 1);

        // Connect chart type combo
        connect(chart_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, slot_i, chart](int idx) {
            if (slot_i >= slots_.size())
                return;
            services::DbnChartType ct;
            switch (idx) {
                case 1:
                    ct = services::DbnChartType::Area;
                    break;
                case 2:
                    ct = services::DbnChartType::Bar;
                    break;
                case 3:
                    ct = services::DbnChartType::Scatter;
                    break;
                default:
                    ct = services::DbnChartType::Line;
                    break;
            }
            slots_[slot_i].chart_type = ct;
            if (!slots_[slot_i].series.isEmpty())
                chart->set_data(slots_[slot_i].series, ct);
        });

        slot_cards_.append({chart, table, chart_combo});
        grid->addWidget(card, row, col, 1, col_span);
        grid->setRowStretch(row, 1);
    }

    // grid_widget expands to fill all available space in comparison_content_
    comparison_layout_->addWidget(grid_widget, 1);
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap DBnomicsScreen::save_state() const {
    // Restored but not shown yet: the state is still the one that came in.
    if (!first_show_done_ && !pending_state_.isEmpty())
        return pending_state_;
    QVariantMap state{
        {"provider", selection_panel_->selected_provider()},
        {"dataset", selection_panel_->selected_dataset()},
        {"series", selection_panel_->selected_series()},
        {"view_mode", static_cast<int>(view_mode_)},
    };
    if (selection_panel_) {
        state["global_search"] = selection_panel_->global_search_text();
        state["provider_filter"] = selection_panel_->provider_filter_text();
        state["series_search"] = selection_panel_->series_search_text();
    }
    // The views themselves (series ids + chart types) — not just the last selection — so a restart
    // brings the chart set back. Ids still waiting to be restored are kept. The preview comes back
    // through the "series" selection.
    QStringList single_ids = restore_single_;
    for (const auto& dp : single_series_)
        if (!single_ids.contains(dp.series_id))
            single_ids << dp.series_id;
    state["single_series"] = single_ids;
    state["single_chart_type"] = static_cast<int>(single_chart_type_);
    QVariantList slot_list;
    for (int i = 0; i < slots_.size(); ++i) {
        QStringList ids = i < restore_slots_.size() ? restore_slots_[i] : QStringList{};
        for (const auto& dp : slots_[i].series)
            if (!ids.contains(dp.series_id))
                ids << dp.series_id;
        slot_list << QVariantMap{{"series", ids}, {"chart_type", static_cast<int>(slots_[i].chart_type)}};
    }
    state["slots"] = slot_list;
    return state;
}

void DBnomicsScreen::restore_state(const QVariantMap& state) {
    // Applied on the first show: nothing is fetched while the screen is hidden.
    if (!first_show_done_) {
        pending_state_ = state;
        return;
    }
    apply_restored_state(state);
}

void DBnomicsScreen::apply_restored_state(const QVariantMap& state) {
    const QString prov = state.value("provider").toString();
    const QString ds = state.value("dataset").toString();
    const QString ser = state.value("series").toString();
    const int mode = state.value("view_mode", 0).toInt();

    if (selection_panel_) {
        // Search texts first: the dataset's series list is requested for the text in the box.
        if (state.contains("global_search"))
            selection_panel_->set_global_search_text(state.value("global_search").toString());
        if (state.contains("provider_filter"))
            selection_panel_->set_provider_filter_text(state.value("provider_filter").toString());
    }

    // The handlers below read the panel's remembered provider/dataset (on_dataset_selected() asks it
    // for the provider), which a fresh panel doesn't have — seed it first, otherwise the dataset
    // and series requests were issued with an empty provider.
    selection_panel_->restore_selection(prov, ds, ser);
    if (selection_panel_ && state.contains("series_search") && !ds.isEmpty())
        selection_panel_->set_series_search_text(state.value("series_search").toString());
    if (!prov.isEmpty())
        on_provider_selected(prov);
    if (!ds.isEmpty())
        on_dataset_selected(ds);

    // Views: recreate the slots, then re-add each remembered series as its observations arrive
    // (on_observations_loaded); subscribing delivers a still-fresh cached copy immediately.
    if (state.contains("single_chart_type") && chart_type_combo_)
        chart_type_combo_->setCurrentIndex(std::clamp(state.value("single_chart_type").toInt(), 0, 3));
    restore_single_ = state.value("single_series").toStringList();
    restore_slots_.clear();
    const QVariantList slot_list = state.value("slots").toList();
    for (const auto& sv : slot_list) {
        const QVariantMap m = sv.toMap();
        slots_.append(services::DbnSlot{});
        selection_panel_->add_comparison_slot();
        slots_.last().chart_type =
            static_cast<services::DbnChartType>(std::clamp(m.value("chart_type").toInt(), 0, 3));
        restore_slots_.append(m.value("series").toStringList());
    }
    rebuild_comparison_view();
    if (mode == 1)
        set_view_mode(services::DbnViewMode::Comparison);

    QStringList topics;
    auto want = [this, &topics](const QString& id) {
        const QString topic = series_topic(id);
        if (topic.isEmpty())
            return;
        watch_series(id);
        if (!topics.contains(topic))
            topics << topic;
    };
    for (const auto& id : std::as_const(restore_single_))
        want(id);
    for (const auto& ids : std::as_const(restore_slots_))
        for (const auto& id : ids)
            want(id);
    if (!topics.isEmpty())
        datahub::DataHub::instance().request(topics, /*force=*/false);

    // The selected series comes back as the chart's preview.
    if (!ser.isEmpty())
        on_series_selected(prov, ds, ser);
    update_single_page();
}

bool DBnomicsScreen::open_section(const QString& section, const QString& focus) {
    Q_UNUSED(focus); // nothing here takes a focus without a fetch (the global search queries DBnomics)
    QString key = section.trimmed().toLower();
    if (key.isEmpty())
        return true;
    QString rest;
    if (command_rows::split_kind(key, "view", &rest))
        key = rest;
    services::DbnViewMode mode = services::DbnViewMode::Single;
    if (key == QLatin1String("compare") || key == QLatin1String("comparison"))
        mode = services::DbnViewMode::Comparison;
    else if (key != QLatin1String("single"))
        return false;
    if (!first_show_done_ && !pending_state_.isEmpty()) {
        // The first show applies the remembered state (its view mode included): make it this one.
        pending_state_.insert(QStringLiteral("view_mode"), static_cast<int>(mode));
        return true;
    }
    set_view_mode(mode); // the SINGLE / COMPARE buttons' path
    return true;
}

// ── Search box rows (core/command/CommandRegistry.h; screens/common/CommandRows.h) ──
namespace {

QList<command::CommandItem> dbn_command_rows() {
    namespace rows = command_rows;
    const QString screen = QStringLiteral("dbnomics");
    const QString where = DBnomicsScreen::tr("DBnomics");
    return {
        rows::section(screen, QStringLiteral("single"), DBnomicsScreen::tr("DBnomics series chart"),
                      {QStringLiteral("single view"), QStringLiteral("chart"), QStringLiteral("economic series"),
                       QStringLiteral("time series"), QStringLiteral("observations"), QStringLiteral("dbnomics")},
                      where),
        rows::section(screen, QStringLiteral("compare"), DBnomicsScreen::tr("Compare DBnomics series"),
                      {QStringLiteral("compare"), QStringLiteral("comparison"), QStringLiteral("side by side"),
                       QStringLiteral("slots"), QStringLiteral("economic series"), QStringLiteral("dbnomics")},
                      where),
    };
}

void dbn_install_commands() {
    command_rows::install(QStringLiteral("dbnomics"), &dbn_command_rows);
}

} // namespace

Q_COREAPP_STARTUP_FUNCTION(dbn_install_commands)

} // namespace fincept::screens
