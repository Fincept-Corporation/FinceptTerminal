// src/screens/dbnomics/DBnomicsScreen.cpp
#include "screens/dbnomics/DBnomicsScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/dbnomics/DBnomicsChartWidget.h"
#include "screens/dbnomics/DBnomicsDataTable.h"
#include "screens/dbnomics/DBnomicsSelectionPanel.h"
#include "services/dbnomics/DBnomicsService.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSplitter>
#include <QStackedWidget>
#include <QTextStream>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens {

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
    connect(&svc, &services::DBnomicsService::observations_loaded, this, &DBnomicsScreen::on_observations_loaded);
    connect(&svc, &services::DBnomicsService::search_results_loaded, this, &DBnomicsScreen::on_search_results_loaded);
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
        services::DBnomicsService::instance().fetch_series(selection_panel_->selected_provider(),
                                                           selection_panel_->selected_dataset(), {}, offset);
    });

    connect(selection_panel_, &DBnomicsSelectionPanel::add_to_single_view_clicked, this,
            &DBnomicsScreen::on_add_to_single_view);
    connect(selection_panel_, &DBnomicsSelectionPanel::clear_all_clicked, this, &DBnomicsScreen::on_clear_all);
    connect(selection_panel_, &DBnomicsSelectionPanel::add_slot_clicked, this, &DBnomicsScreen::on_add_slot);
    connect(selection_panel_, &DBnomicsSelectionPanel::add_to_slot_clicked, this, &DBnomicsScreen::on_add_to_slot);
    connect(selection_panel_, &DBnomicsSelectionPanel::remove_from_slot_clicked, this,
            &DBnomicsScreen::on_remove_from_slot);
    connect(selection_panel_, &DBnomicsSelectionPanel::remove_slot_clicked, this, &DBnomicsScreen::on_remove_slot);
}

// ─────────────────────────────────────────────────────────────────────────────
// show / hide
// ─────────────────────────────────────────────────────────────────────────────
void DBnomicsScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    LOG_INFO("DBnomicsScreen", "Screen shown");
    if (provider_count_ == 0) {
        LOG_INFO("DBnomicsScreen", "No providers cached — fetching providers");
        selection_panel_->set_providers_loading(true);
        services::DBnomicsService::instance().fetch_providers();
    }
}

void DBnomicsScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    LOG_INFO("DBnomicsScreen", "Screen hidden");
}

// ─────────────────────────────────────────────────────────────────────────────
// build_ui
// ─────────────────────────────────────────────────────────────────────────────
void DBnomicsScreen::build_ui() {
    setStyleSheet(
        QString("QWidget { background:%1; color:%2; }").arg(ui::colors::BG_BASE).arg(ui::colors::TEXT_PRIMARY));

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

    // View toggle bar (36px)
    auto* toggle_bar = new QWidget(right_widget);
    toggle_bar->setFixedHeight(36);
    toggle_bar->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED).arg(ui::colors::BORDER_MED));

    auto* toggle_hl = new QHBoxLayout(toggle_bar);
    toggle_hl->setContentsMargins(8, 0, 8, 0);
    toggle_hl->setSpacing(4);

    auto btn_style_checked = QString("QPushButton { background:rgba(217,119,6,0.15); color:%1; border:1px solid %2;"
                                     " font-family:%3; font-size:12px; font-weight:700; padding:0 12px; height:26px; }"
                                     "QPushButton:hover { background:rgba(217,119,6,0.25); }")
                                 .arg(ui::colors::AMBER)
                                 .arg(ui::colors::AMBER_DIM)
                                 .arg(ui::fonts::DATA_FAMILY);

    auto btn_style_unchecked =
        QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                " font-family:%3; font-size:12px; font-weight:700; padding:0 12px; height:26px; }"
                "QPushButton:hover { background:%4; }")
            .arg(ui::colors::TEXT_SECONDARY)
            .arg(ui::colors::BORDER_DIM)
            .arg(ui::fonts::DATA_FAMILY)
            .arg(ui::colors::BG_HOVER);

    single_btn_ = new QPushButton("SINGLE", toggle_bar);
    single_btn_->setCheckable(true);
    single_btn_->setChecked(true);
    single_btn_->setStyleSheet(btn_style_checked);

    compare_btn_ = new QPushButton("COMPARE", toggle_bar);
    compare_btn_->setCheckable(true);
    compare_btn_->setChecked(false);
    compare_btn_->setStyleSheet(btn_style_unchecked);

    connect(single_btn_, &QPushButton::clicked, this, [this, btn_style_checked, btn_style_unchecked]() {
        single_btn_->setChecked(true);
        compare_btn_->setChecked(false);
        single_btn_->setStyleSheet(btn_style_checked);
        compare_btn_->setStyleSheet(btn_style_unchecked);
        view_mode_ = services::DbnViewMode::Single;
        view_stack_->setCurrentIndex(0);
    });

    connect(compare_btn_, &QPushButton::clicked, this, [this, btn_style_checked, btn_style_unchecked]() {
        compare_btn_->setChecked(true);
        single_btn_->setChecked(false);
        compare_btn_->setStyleSheet(btn_style_checked);
        single_btn_->setStyleSheet(btn_style_unchecked);
        view_mode_ = services::DbnViewMode::Comparison;
        view_stack_->setCurrentIndex(1);
    });

    toggle_hl->addWidget(single_btn_);
    toggle_hl->addWidget(compare_btn_);
    toggle_hl->addStretch();

    auto* chart_type_label = new QLabel("CHART:", toggle_bar);
    chart_type_label->setStyleSheet(QString("color:%1; font-family:%2; font-size:11px; font-weight:600;")
                                        .arg(ui::colors::TEXT_TERTIARY)
                                        .arg(ui::fonts::DATA_FAMILY));

    chart_type_combo_ = new QComboBox(toggle_bar);
    chart_type_combo_->addItems({"LINE", "AREA", "BAR", "SCATTER"});
    chart_type_combo_->setFixedHeight(26);
    chart_type_combo_->setStyleSheet(
        QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                " font-family:%4; font-size:11px; padding:0 6px; }"
                "QComboBox::drop-down { border:none; }"
                "QComboBox QAbstractItemView { background:%1; color:%2; border:1px solid %3; }")
            .arg(ui::colors::BG_HOVER)
            .arg(ui::colors::TEXT_PRIMARY)
            .arg(ui::colors::BORDER_MED)
            .arg(ui::fonts::DATA_FAMILY));

    connect(chart_type_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &DBnomicsScreen::on_chart_type_changed);

    toggle_hl->addWidget(chart_type_label);
    toggle_hl->addWidget(chart_type_combo_);

    right_vl->addWidget(toggle_bar);

    // view_stack_
    view_stack_ = new QStackedWidget(right_widget);

    // Page 0: single view (chart + table with draggable splitter)
    auto* single_page = new QWidget(view_stack_);
    auto* single_vl = new QVBoxLayout(single_page);
    single_vl->setContentsMargins(0, 0, 0, 0);
    single_vl->setSpacing(0);

    auto* single_splitter = new QSplitter(Qt::Vertical, single_page);
    single_splitter->setHandleWidth(5);
    single_splitter->setChildrenCollapsible(false);
    single_splitter->setStyleSheet(
        QString("QSplitter::handle:vertical {"
                "  background: %1; height: 5px;"
                "  border-top: 1px solid %2; border-bottom: 1px solid %2; }"
                "QSplitter::handle:vertical:hover { background: %3; }")
            .arg(ui::colors::BORDER_DIM, ui::colors::BORDER_MED, ui::colors::AMBER));

    chart_widget_ = new DBnomicsChartWidget(single_splitter);
    data_table_ = new DBnomicsDataTable(single_splitter);
    chart_widget_->setMinimumHeight(120);
    data_table_->setMinimumHeight(150);

    single_splitter->addWidget(chart_widget_);
    single_splitter->addWidget(data_table_);
    single_splitter->setStretchFactor(0, 3); // chart ~65 %
    single_splitter->setStretchFactor(1, 2); // table ~35 %

    single_vl->addWidget(single_splitter);

    view_stack_->addWidget(single_page); // index 0

    // Page 1: comparison view (scrollable slot cards)
    auto* compare_page = new QWidget(view_stack_);
    compare_page->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
    auto* compare_vl = new QVBoxLayout(compare_page);
    compare_vl->setContentsMargins(0, 0, 0, 0);
    compare_vl->setSpacing(0);

    auto* compare_scroll = new QScrollArea(compare_page);
    compare_scroll->setWidgetResizable(true);
    compare_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    compare_scroll->setStyleSheet(QString("QScrollArea { border: none; background: %1; }"
                                          "QScrollBar:vertical { background: %1; width: 6px; }"
                                          "QScrollBar::handle:vertical { background: %2; }"
                                          "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
                                      .arg(ui::colors::BG_BASE)
                                      .arg(ui::colors::BORDER_DIM));

    comparison_content_ = new QWidget(compare_scroll);
    comparison_content_->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
    comparison_layout_ = new QVBoxLayout(comparison_content_);
    comparison_layout_->setContentsMargins(8, 8, 8, 8);
    comparison_layout_->setSpacing(8);

    // Initial placeholder
    auto* init_placeholder =
        new QLabel("NO COMPARISON SLOTS\nClick  + ADD SLOT  in the left panel to begin", comparison_content_);
    init_placeholder->setAlignment(Qt::AlignCenter);
    init_placeholder->setWordWrap(true);
    init_placeholder->setStyleSheet(QString("color:%1; font-family:%2; font-size:13px;")
                                        .arg(ui::colors::TEXT_TERTIARY)
                                        .arg(ui::fonts::DATA_FAMILY));
    comparison_layout_->addWidget(init_placeholder);
    comparison_layout_->addStretch();

    compare_scroll->setWidget(comparison_content_);
    compare_vl->addWidget(compare_scroll);

    view_stack_->addWidget(compare_page); // index 1
    view_stack_->setCurrentIndex(0);

    right_vl->addWidget(view_stack_, 1);
    content_hl->addWidget(right_widget, 1);
    root->addWidget(content_widget, 1);

    // ── Status bar (28px) ─────────────────────────────────────────────────────
    auto* status_bar = new QWidget(this);
    status_bar->setFixedHeight(28);
    status_bar->setStyleSheet(
        QString("background:%1; border-top:1px solid %2;").arg(ui::colors::BG_RAISED).arg(ui::colors::BORDER_DIM));

    auto* status_hl = new QHBoxLayout(status_bar);
    status_hl->setContentsMargins(12, 0, 12, 0);
    status_hl->setSpacing(0);

    status_label_ = new QLabel("Ready", status_bar);
    status_label_->setStyleSheet(QString("color:%1; font-family:%2; font-size:11px;")
                                     .arg(ui::colors::TEXT_SECONDARY)
                                     .arg(ui::fonts::DATA_FAMILY));

    stats_label_ = new QLabel("", status_bar);
    stats_label_->setStyleSheet(QString("color:%1; font-family:%2; font-size:11px;")
                                    .arg(ui::colors::TEXT_TERTIARY)
                                    .arg(ui::fonts::DATA_FAMILY));
    stats_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    status_hl->addWidget(status_label_);
    status_hl->addStretch();
    status_hl->addWidget(stats_label_);

    root->addWidget(status_bar);
}

// ─────────────────────────────────────────────────────────────────────────────
// build_toolbar
// ─────────────────────────────────────────────────────────────────────────────
QWidget* DBnomicsScreen::build_toolbar() {
    auto* toolbar = new QWidget(this);
    toolbar->setFixedHeight(44);
    toolbar->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED).arg(ui::colors::BORDER_MED));

    auto* hl = new QHBoxLayout(toolbar);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(8);

    auto* title = new QLabel("DBNOMICS TERMINAL", toolbar);
    title->setStyleSheet(QString("color:%1; font-family:%2; font-size:14px; font-weight:700;")
                             .arg(ui::colors::AMBER)
                             .arg(ui::fonts::DATA_FAMILY));

    hl->addWidget(title);
    hl->addStretch();

    const QString btn_style = QString("QPushButton { background:rgba(217,119,6,0.1); color:%1;"
                                      " border:1px solid %2; font-family:%3; font-size:12px; font-weight:600;"
                                      " padding:0 10px; height:28px; }"
                                      "QPushButton:hover { background:rgba(217,119,6,0.2); }"
                                      "QPushButton:pressed { background:rgba(217,119,6,0.3); }")
                                  .arg(ui::colors::AMBER)
                                  .arg(ui::colors::AMBER_DIM)
                                  .arg(ui::fonts::DATA_FAMILY);

    auto* fetch_btn = new QPushButton("FETCH", toolbar);
    fetch_btn->setStyleSheet(btn_style);
    connect(fetch_btn, &QPushButton::clicked, this, &DBnomicsScreen::on_fetch_clicked);

    auto* refresh_btn = new QPushButton("REFRESH", toolbar);
    refresh_btn->setStyleSheet(btn_style);
    connect(refresh_btn, &QPushButton::clicked, this, &DBnomicsScreen::on_refresh_clicked);

    auto* export_btn = new QPushButton("EXPORT CSV", toolbar);
    export_btn->setStyleSheet(btn_style);
    connect(export_btn, &QPushButton::clicked, this, &DBnomicsScreen::on_export_csv);

    hl->addWidget(fetch_btn);
    hl->addWidget(refresh_btn);
    hl->addWidget(export_btn);

    return toolbar;
}

// ─────────────────────────────────────────────────────────────────────────────
// set_status
// ─────────────────────────────────────────────────────────────────────────────
void DBnomicsScreen::set_status(const QString& message) {
    if (status_label_)
        status_label_->setText(message);
}

// ─────────────────────────────────────────────────────────────────────────────
// render_single_view
// ─────────────────────────────────────────────────────────────────────────────
void DBnomicsScreen::render_single_view() {
    if (single_series_.isEmpty()) {
        chart_widget_->clear();
        data_table_->clear();
        return;
    }
    chart_widget_->set_data(single_series_, single_chart_type_);
    data_table_->set_data(single_series_);
}

// ─────────────────────────────────────────────────────────────────────────────
// assign_series_colors
// ─────────────────────────────────────────────────────────────────────────────
void DBnomicsScreen::assign_series_colors() {
    for (int i = 0; i < single_series_.size(); ++i)
        single_series_[i].color = services::DBnomicsService::chart_color(i);
}

// ─────────────────────────────────────────────────────────────────────────────
// Service signal slots
// ─────────────────────────────────────────────────────────────────────────────
void DBnomicsScreen::on_providers_loaded(const QVector<services::DbnProvider>& providers) {
    provider_count_ = providers.size();
    selection_panel_->set_providers_loading(false);
    selection_panel_->populate_providers(providers);
    stats_label_->setText(QString("%1 providers").arg(provider_count_));
    set_status(QString("Loaded %1 providers").arg(provider_count_));
    LOG_INFO("DBnomicsScreen", QString("Providers loaded: %1").arg(provider_count_));
}

void DBnomicsScreen::on_datasets_loaded(const QVector<services::DbnDataset>& datasets,
                                        const services::DbnPagination& page, bool append) {
    if (!append)
        selection_panel_->set_datasets_loading(false);
    selection_panel_->populate_datasets(datasets, page, append);
}

void DBnomicsScreen::on_series_loaded(const QVector<services::DbnSeriesInfo>& series,
                                      const services::DbnPagination& page, bool append) {
    if (!append)
        selection_panel_->set_series_loading(false);
    selection_panel_->populate_series(series, page, append);
    set_status(QString("Loaded %1 series").arg(series.size()));
}

void DBnomicsScreen::on_observations_loaded(const services::DbnDataPoint& data) {
    last_loaded_data_ = data;
    has_pending_data_ = true;

    // Always stop loading spinners — data has arrived
    chart_widget_->set_loading(false);
    data_table_->set_loading(false);

    // Check if this series is already in single_series_
    for (auto& s : single_series_) {
        if (s.series_id == data.series_id) {
            s.observations = data.observations;
            s.series_name = data.series_name;
            render_single_view();
            return;
        }
    }

    // Not yet in view — prompt user
    set_status(QString("Loaded: %1  •  Click ADD TO SINGLE VIEW").arg(data.series_name));
    LOG_INFO("DBnomicsScreen", QString("Observations loaded for: %1").arg(data.series_id));
}

void DBnomicsScreen::on_search_results_loaded(const QVector<services::DbnSearchResult>& results,
                                              const services::DbnPagination& page, bool append) {
    selection_panel_->set_search_loading(false);
    selection_panel_->populate_search_results(results, page, append);
}

void DBnomicsScreen::on_service_error(const QString& context, const QString& message) {
    LOG_ERROR("DBnomicsScreen", QString("[%1] %2").arg(context, message));
    set_status(QString("ERROR [%1]: %2").arg(context, message));
    // Stop any active loading spinners so they don't hang on error
    selection_panel_->set_providers_loading(false);
    selection_panel_->set_datasets_loading(false);
    selection_panel_->set_series_loading(false);
    selection_panel_->set_search_loading(false);
    chart_widget_->set_loading(false);
    data_table_->set_loading(false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Toolbar action slots
// ─────────────────────────────────────────────────────────────────────────────
void DBnomicsScreen::on_fetch_clicked() {
    set_status("Fetching providers...");
    selection_panel_->set_providers_loading(true);
    services::DBnomicsService::instance().fetch_providers();
}

void DBnomicsScreen::on_refresh_clicked() {
    if (single_series_.isEmpty())
        return;

    for (const auto& s : std::as_const(single_series_)) {
        // series_id format: "PROV/DS/CODE"
        const QStringList parts = s.series_id.split('/');
        if (parts.size() == 3)
            services::DBnomicsService::instance().fetch_observations(parts[0], parts[1], parts[2]);
    }
    set_status("Refreshing series data...");
}

void DBnomicsScreen::on_export_csv() {
    if (single_series_.isEmpty()) {
        set_status("No data to export");
        return;
    }

    const QString default_name =
        QString("dbnomics_%1.csv").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));

    const QString path = QFileDialog::getSaveFileName(this, "Export CSV", default_name, "CSV Files (*.csv)");

    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        set_status(QString("Failed to open file: %1").arg(path));
        LOG_ERROR("DBnomicsScreen", QString("Cannot open file for CSV export: %1").arg(path));
        return;
    }

    QTextStream out(&file);

    // Collect all unique periods in order (use the first series as the period reference,
    // then union with others for completeness)
    QVector<QString> periods;
    QSet<QString> seen_periods;
    for (const auto& series : std::as_const(single_series_)) {
        for (const auto& obs : series.observations) {
            if (!seen_periods.contains(obs.period)) {
                seen_periods.insert(obs.period);
                periods.append(obs.period);
            }
        }
    }
    std::sort(periods.begin(), periods.end());

    // Header row
    out << "Period";
    for (const auto& series : std::as_const(single_series_))
        out << "," << series.series_name;
    out << "\n";

    // Data rows
    for (const auto& period : std::as_const(periods)) {
        out << period;
        for (const auto& series : std::as_const(single_series_)) {
            out << ",";
            bool found = false;
            for (const auto& obs : series.observations) {
                if (obs.period == period) {
                    if (obs.valid)
                        out << obs.value;
                    // else: empty cell for NA
                    found = true;
                    break;
                }
            }
            Q_UNUSED(found)
        }
        out << "\n";
    }

    file.close();
    set_status(QString("Exported CSV: %1").arg(path));
    LOG_INFO("DBnomicsScreen", QString("CSV exported to: %1").arg(path));
}

// ─────────────────────────────────────────────────────────────────────────────
// Selection panel navigation slots
// ─────────────────────────────────────────────────────────────────────────────
void DBnomicsScreen::on_provider_selected(const QString& code) {
    set_status(QString("Loading datasets for %1...").arg(code));
    selection_panel_->set_datasets_loading(true);
    services::DBnomicsService::instance().fetch_datasets(code, 0);
    ScreenStateManager::instance().notify_changed(this);
}

void DBnomicsScreen::on_dataset_selected(const QString& code) {
    set_status(QString("Loading series for %1...").arg(code));
    selection_panel_->set_series_loading(true);
    services::DBnomicsService::instance().fetch_series(selection_panel_->selected_provider(), code, {}, 0);
}

void DBnomicsScreen::on_series_selected(const QString& prov, const QString& ds, const QString& code) {
    set_status("Fetching observations...");
    chart_widget_->set_loading(true);
    data_table_->set_loading(true);
    services::DBnomicsService::instance().fetch_observations(prov, ds, code);
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
void DBnomicsScreen::on_add_to_single_view() {
    if (!has_pending_data_) {
        set_status("No data loaded — click a series first");
        return;
    }

    // Check duplicate
    for (const auto& s : std::as_const(single_series_)) {
        if (s.series_id == last_loaded_data_.series_id) {
            set_status("Series already in view");
            return;
        }
    }

    single_series_.append(last_loaded_data_);
    assign_series_colors();
    render_single_view();

    // Switch to single view
    single_btn_->setChecked(true);
    compare_btn_->setChecked(false);
    view_mode_ = services::DbnViewMode::Single;
    view_stack_->setCurrentIndex(0);

    set_status(QString("Added: %1").arg(last_loaded_data_.series_name));
    LOG_INFO("DBnomicsScreen", QString("Series added to single view: %1").arg(last_loaded_data_.series_id));
}

void DBnomicsScreen::on_clear_all() {
    single_series_.clear();
    slots_.clear();
    has_pending_data_ = false;
    chart_widget_->clear();
    data_table_->clear();
    selection_panel_->clear_slots();
    rebuild_comparison_view();
    set_status("Cleared");
    LOG_INFO("DBnomicsScreen", "All series cleared");
}

void DBnomicsScreen::on_add_slot() {
    slots_.append(services::DbnSlot{});
    selection_panel_->add_comparison_slot();
    rebuild_comparison_view();
    LOG_INFO("DBnomicsScreen", QString("Comparison slot added. Total slots: %1").arg(slots_.size()));
}

void DBnomicsScreen::on_add_to_slot(int slot_index) {
    if (!has_pending_data_) {
        set_status("No data loaded");
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
        slots_[slot_index].series[i].color = services::DBnomicsService::chart_color(i);

    selection_panel_->update_slot_series(slot_index, slots_[slot_index].series);
    rebuild_comparison_view();

    set_status(QString("Added %1 to slot %2").arg(last_loaded_data_.series_name).arg(slot_index + 1));
    LOG_INFO("DBnomicsScreen", QString("Series added to slot %1: %2").arg(slot_index).arg(last_loaded_data_.series_id));
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
        slot.series[i].color = services::DBnomicsService::chart_color(i);

    selection_panel_->update_slot_series(slot_index, slot.series);
    rebuild_comparison_view();

    set_status(QString("Removed series from slot %1").arg(slot_index + 1));
    LOG_INFO("DBnomicsScreen", QString("Removed %1 from slot %2").arg(series_id).arg(slot_index));
}

void DBnomicsScreen::on_remove_slot(int slot_index) {
    if (slot_index < 0 || slot_index >= slots_.size())
        return;
    slots_.removeAt(slot_index);
    selection_panel_->remove_comparison_slot(slot_index);
    rebuild_comparison_view();
    set_status(QString("Slot removed. %1 slot(s) remaining").arg(slots_.size()));
    LOG_INFO("DBnomicsScreen", QString("Slot %1 removed. Remaining: %2").arg(slot_index + 1).arg(slots_.size()));
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
        auto* placeholder =
            new QLabel("NO COMPARISON SLOTS\nClick  + ADD SLOT  in the left panel to begin", comparison_content_);
        placeholder->setAlignment(Qt::AlignCenter);
        placeholder->setWordWrap(true);
        placeholder->setStyleSheet(QString("color:%1; font-family:%2; font-size:13px;")
                                       .arg(ui::colors::TEXT_TERTIARY)
                                       .arg(ui::fonts::DATA_FAMILY));
        comparison_layout_->addWidget(placeholder);
        comparison_layout_->addStretch();
        return;
    }

    // ── 2-column grid: up to 2 rows × 2 cols fit on screen without scrolling ─
    auto* grid_widget = new QWidget(comparison_content_);
    grid_widget->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
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
        card->setMinimumWidth(320); // ensure Y-axis labels always have room
        card->setObjectName("slotCard");
        card->setStyleSheet(QString("QWidget#slotCard { background:%1; border:1px solid %2; }")
                                .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));

        auto* card_vl = new QVBoxLayout(card);
        card_vl->setContentsMargins(0, 0, 0, 0);
        card_vl->setSpacing(0);

        // ── Card header (32px) ────────────────────────────────────────────────
        auto* header = new QWidget(card);
        header->setFixedHeight(32);
        header->setObjectName("cardHeader");
        header->setStyleSheet(QString("QWidget#cardHeader { background:%1; border-bottom:1px solid %2; }")
                                  .arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));

        auto* header_hl = new QHBoxLayout(header);
        header_hl->setContentsMargins(10, 0, 8, 0);
        header_hl->setSpacing(8);

        auto* slot_label = new QLabel(QString("SLOT %1").arg(i + 1), header);
        slot_label->setStyleSheet(QString("color:%1; font-family:%2; font-size:12px; font-weight:700;"
                                          " background:transparent; border:none;")
                                      .arg(ui::colors::AMBER)
                                      .arg(ui::fonts::DATA_FAMILY));
        header_hl->addWidget(slot_label);

        auto* count_label = new QLabel(QString("(%1 series)").arg(slot.series.size()), header);
        count_label->setStyleSheet(QString("color:%1; font-family:%2; font-size:10px;"
                                           " background:transparent; border:none;")
                                       .arg(ui::colors::TEXT_TERTIARY)
                                       .arg(ui::fonts::DATA_FAMILY));
        header_hl->addWidget(count_label);
        header_hl->addStretch();

        auto* chart_combo = new QComboBox(header);
        chart_combo->addItems({"LINE", "AREA", "BAR", "SCATTER"});
        chart_combo->setCurrentIndex(static_cast<int>(slot.chart_type));
        chart_combo->setFixedHeight(22);
        chart_combo->setStyleSheet(
            QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                    " font-family:%4; font-size:10px; padding:0 4px; }"
                    "QComboBox::drop-down { border:none; }"
                    "QComboBox QAbstractItemView { background:%1; color:%2; border:1px solid %3; }")
                .arg(ui::colors::BG_HOVER)
                .arg(ui::colors::TEXT_PRIMARY)
                .arg(ui::colors::BORDER_MED)
                .arg(ui::fonts::DATA_FAMILY));
        header_hl->addWidget(chart_combo);
        card_vl->addWidget(header);

        // ── Chart + Table (draggable splitter) ────────────────────────────────
        auto* card_splitter = new QSplitter(Qt::Vertical, card);
        card_splitter->setHandleWidth(4);
        card_splitter->setChildrenCollapsible(false);
        card_splitter->setStyleSheet(
            QString("QSplitter::handle:vertical { background:%1; height:4px; }"
                    "QSplitter::handle:vertical:hover { background:%2; }")
                .arg(ui::colors::BORDER_DIM, ui::colors::AMBER));

        auto* chart = new DBnomicsChartWidget(card_splitter);
        chart->set_compact(true); // tighter margins + no legend for grid slots
        auto* table = new DBnomicsDataTable(card_splitter);
        chart->setMinimumHeight(80);
        table->setMinimumHeight(80);

        if (!slot.series.isEmpty()) {
            chart->set_data(slot.series, slot.chart_type);
            table->set_data(slot.series);
        } else {
            chart->clear();
            table->clear();
        }

        card_splitter->addWidget(chart);
        card_splitter->addWidget(table);
        card_splitter->setStretchFactor(0, 3);
        card_splitter->setStretchFactor(1, 2);

        card_vl->addWidget(card_splitter, 1);

        // Connect chart type combo
        const int slot_i = i;
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
    return {
        {"provider", selection_panel_->selected_provider()},
        {"dataset",  selection_panel_->selected_dataset()},
        {"series",   selection_panel_->selected_series()},
        {"view_mode", static_cast<int>(view_mode_)},
    };
}

void DBnomicsScreen::restore_state(const QVariantMap& state) {
    const QString prov = state.value("provider").toString();
    const QString ds   = state.value("dataset").toString();
    const QString ser  = state.value("series").toString();
    const int mode     = state.value("view_mode", 0).toInt();

    if (mode == 1) {
        view_mode_ = services::DbnViewMode::Comparison;
        view_stack_->setCurrentIndex(1);
    }

    if (!prov.isEmpty())
        on_provider_selected(prov);
    if (!ds.isEmpty())
        on_dataset_selected(ds);
    if (!ser.isEmpty())
        on_series_selected(prov, ds, ser);
}

} // namespace fincept::screens
