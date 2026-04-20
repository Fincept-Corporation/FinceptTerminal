// src/screens/gov_data/GovDataFrancePanel.cpp
// France data.gouv.fr panel — Data Services | Datasets (search) | Geo Search
#include "screens/gov_data/GovDataFrancePanel.h"
#include "screens/gov_data/GovDataProviderPanel.h"

#include "core/logging/Logger.h"
#include "services/gov_data/GovDataService.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens {
namespace {
static constexpr const char* kGovDataFranceScript = "french_gov_api.py";
static constexpr const char* kGovDataFranceColor = "#2563EB";
} // namespace

using namespace fincept::ui;

// ── Constructor ───────────────────────────────────────────────────────────────

GovDataFrancePanel::GovDataFrancePanel(QWidget* parent) : QWidget(parent) {
    setStyleSheet(make_gov_panel_style(kGovDataFranceColor));
    build_ui();
    connect(&services::GovDataService::instance(), &services::GovDataService::result_ready, this,
            &GovDataFrancePanel::on_result);
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this]() { setStyleSheet(make_gov_panel_style(kGovDataFranceColor)); });
}

// ── Build UI ──────────────────────────────────────────────────────────────────

void GovDataFrancePanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_toolbar());

    // Breadcrumb bar
    breadcrumb_ = new QWidget(this);
    breadcrumb_->setObjectName("govBreadcrumb");
    breadcrumb_->setFixedHeight(24);
    auto* bcl = new QHBoxLayout(breadcrumb_);
    bcl->setContentsMargins(14, 0, 14, 0);
    bcl->setSpacing(4);
    breadcrumb_label_ = new QLabel("Data Services");
    breadcrumb_label_->setObjectName("govBreadcrumbText");
    bcl->addWidget(breadcrumb_label_);
    bcl->addStretch(1);
    row_count_label_ = new QLabel;
    row_count_label_->setObjectName("govBreadcrumbActive");
    bcl->addWidget(row_count_label_);
    root->addWidget(breadcrumb_);

    // Content stack
    content_stack_ = new QStackedWidget;

    // Page 0 — Data Services table
    services_table_ = new QTableWidget;
    services_table_->setColumnCount(4);
    services_table_->setHorizontalHeaderLabels({"NAME", "VIEWS", "FOLLOWERS", "CREATED"});
    services_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    services_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    services_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    services_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    services_table_->setColumnWidth(1, 80);
    services_table_->setColumnWidth(2, 90);
    services_table_->setColumnWidth(3, 100);
    configure_table(services_table_);
    content_stack_->addWidget(services_table_); // index 0

    // Page 1 — Datasets table (search results)
    datasets_table_ = new QTableWidget;
    datasets_table_->setColumnCount(5);
    datasets_table_->setHorizontalHeaderLabels({"TITLE", "ORG", "LICENSE", "FILES", "MODIFIED"});
    datasets_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    datasets_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    datasets_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    datasets_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    datasets_table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    datasets_table_->setColumnWidth(1, 150);
    datasets_table_->setColumnWidth(2, 130);
    datasets_table_->setColumnWidth(3, 55);
    datasets_table_->setColumnWidth(4, 100);
    configure_table(datasets_table_);
    connect(datasets_table_, &QTableWidget::cellDoubleClicked, this,
            &GovDataFrancePanel::on_dataset_row_double_clicked);
    content_stack_->addWidget(datasets_table_); // index 1

    // Page 2 — Geo / Municipalities table
    geo_table_ = new QTableWidget;
    geo_table_->setColumnCount(6);
    geo_table_->setHorizontalHeaderLabels({"NAME", "CODE", "POSTAL", "DEPT", "REGION", "POPULATION"});
    geo_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    geo_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    geo_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    geo_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    geo_table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    geo_table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    geo_table_->setColumnWidth(1, 70);
    geo_table_->setColumnWidth(2, 110);
    geo_table_->setColumnWidth(3, 55);
    geo_table_->setColumnWidth(4, 65);
    geo_table_->setColumnWidth(5, 95);
    configure_table(geo_table_);
    content_stack_->addWidget(geo_table_); // index 2

    // Page 3 — Resources / Column schema table
    resources_table_ = new QTableWidget;
    resources_table_->setColumnCount(2);
    resources_table_->setHorizontalHeaderLabels({"NAME", "TYPE"});
    resources_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    resources_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    resources_table_->setColumnWidth(1, 120);
    configure_table(resources_table_);
    content_stack_->addWidget(resources_table_); // index 3

    // Page 4 — Status / loading / error
    auto* status_page = new QWidget(this);
    status_page->setObjectName("govStatusPage");
    auto* svl = new QVBoxLayout(status_page);
    svl->setAlignment(Qt::AlignCenter);
    status_label_ = new QLabel;
    status_label_->setObjectName("govStatusMsg");
    status_label_->setAlignment(Qt::AlignCenter);
    status_label_->setWordWrap(true);
    svl->addWidget(status_label_);
    content_stack_->addWidget(status_page); // index 4

    root->addWidget(content_stack_, 1);
}

QWidget* GovDataFrancePanel::build_toolbar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("govPanelToolbar");
    bar->setFixedHeight(36);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(10, 0, 10, 0);
    hl->setSpacing(5);

    back_btn_ = new QPushButton("← BACK");
    back_btn_->setObjectName("govBackBtn");
    back_btn_->setVisible(false);
    back_btn_->setCursor(Qt::PointingHandCursor);
    connect(back_btn_, &QPushButton::clicked, this, &GovDataFrancePanel::on_back);
    hl->addWidget(back_btn_);

    hl->addSpacing(4);

    services_btn_ = new QPushButton("DATA SERVICES");
    services_btn_->setObjectName("govTabBtn");
    services_btn_->setCheckable(true);
    services_btn_->setChecked(true);
    services_btn_->setCursor(Qt::PointingHandCursor);
    connect(services_btn_, &QPushButton::clicked, this, [this]() { on_tab_changed(Services); });
    hl->addWidget(services_btn_);

    datasets_btn_ = new QPushButton("DATASETS");
    datasets_btn_->setObjectName("govTabBtn");
    datasets_btn_->setCheckable(true);
    datasets_btn_->setCursor(Qt::PointingHandCursor);
    connect(datasets_btn_, &QPushButton::clicked, this, [this]() { on_tab_changed(Datasets); });
    hl->addWidget(datasets_btn_);

    geo_btn_ = new QPushButton("GEO SEARCH");
    geo_btn_->setObjectName("govTabBtn");
    geo_btn_->setCheckable(true);
    geo_btn_->setCursor(Qt::PointingHandCursor);
    connect(geo_btn_, &QPushButton::clicked, this, [this]() { on_tab_changed(Geo); });
    hl->addWidget(geo_btn_);

    hl->addSpacing(10);

    search_input_ = new QLineEdit;
    search_input_->setObjectName("govSearch");
    search_input_->setPlaceholderText("Search…");
    search_input_->setFixedWidth(210);
    search_input_->setFixedHeight(26);
    connect(search_input_, &QLineEdit::returnPressed, this, &GovDataFrancePanel::on_fetch);
    hl->addWidget(search_input_);

    fetch_btn_ = new QPushButton("FETCH");
    fetch_btn_->setObjectName("govFetchBtn");
    fetch_btn_->setCursor(Qt::PointingHandCursor);
    connect(fetch_btn_, &QPushButton::clicked, this, &GovDataFrancePanel::on_fetch);
    hl->addWidget(fetch_btn_);

    hl->addStretch(1);

    export_btn_ = new QPushButton("CSV");
    export_btn_->setObjectName("govCsvBtn");
    export_btn_->setCursor(Qt::PointingHandCursor);
    connect(export_btn_, &QPushButton::clicked, this, &GovDataFrancePanel::on_export_csv);
    hl->addWidget(export_btn_);

    return bar;
}

// ── Public slot ───────────────────────────────────────────────────────────────

void GovDataFrancePanel::load_initial_data() {
    show_loading("Loading data services from data.gouv.fr…");
    services::GovDataService::instance().execute(kGovDataFranceScript, "publishers", {}, "fr_services");
}

// ── Toolbar actions ───────────────────────────────────────────────────────────

void GovDataFrancePanel::on_tab_changed(int tab_index) {
    auto view = static_cast<View>(tab_index);
    services_btn_->setChecked(view == Services);
    datasets_btn_->setChecked(view == Datasets);
    geo_btn_->setChecked(view == Geo);

    current_view_ = view;

    if (view == Services) {
        if (current_services_.isEmpty())
            load_initial_data();
        else {
            content_stack_->setCurrentIndex(Services);
            row_count_label_->setText(QString::number(current_services_.size()) + " services");
        }
        update_breadcrumb("Data Services");
    } else if (view == Datasets) {
        if (current_datasets_.isEmpty()) {
            show_status("Enter a search term above and click FETCH to find datasets.");
        } else {
            content_stack_->setCurrentIndex(Datasets);
            row_count_label_->setText(QString::number(current_datasets_.size()) + " datasets");
        }
        update_breadcrumb("Datasets");
        search_input_->setPlaceholderText("Search datasets…");
        search_input_->setFocus();
    } else if (view == Geo) {
        if (current_geo_.isEmpty()) {
            show_status("Enter a municipality name above and click FETCH to search.");
        } else {
            content_stack_->setCurrentIndex(Geo);
            row_count_label_->setText(QString::number(current_geo_.size()) + " municipalities");
        }
        update_breadcrumb("Geo Search — Municipalities");
        search_input_->setPlaceholderText("Municipality name…");
        search_input_->setFocus();
    }

    update_toolbar_state();
}

void GovDataFrancePanel::on_fetch() {
    QString query = search_input_->text().trimmed();

    if (current_view_ == Services || current_view_ == Resources) {
        // Re-load services
        show_loading("Loading data services…");
        services::GovDataService::instance().execute(kGovDataFranceScript, "publishers", {}, "fr_services");
        return;
    }

    if (query.isEmpty())
        return;
    search_query_ = query;

    if (current_view_ == Datasets) {
        show_loading("Searching datasets for "
                     " + query + "
                     "…");
        services::GovDataService::instance().execute(kGovDataFranceScript, "datasets", {query, "50"}, "fr_datasets");
    } else if (current_view_ == Geo) {
        show_loading("Searching municipalities for "
                     " + query + "
                     "…");
        services::GovDataService::instance().execute(kGovDataFranceScript, "municipalities", {query}, "fr_geo");
    }
}

void GovDataFrancePanel::on_dataset_row_double_clicked(int row) {
    auto* item = datasets_table_->item(row, 0);
    if (!item)
        return;
    selected_dataset_id_ = item->data(Qt::UserRole).toString();
    if (selected_dataset_id_.isEmpty())
        return;

    show_loading("Loading column schema for "
                 " + item->text() + "
                 "…");
    services::GovDataService::instance().execute(kGovDataFranceScript, "resources", {selected_dataset_id_},
                                                 "fr_resources");
}

void GovDataFrancePanel::on_back() {
    if (current_view_ == Resources) {
        content_stack_->setCurrentIndex(Datasets);
        current_view_ = Datasets;
        services_btn_->setChecked(false);
        datasets_btn_->setChecked(true);
        geo_btn_->setChecked(false);
        row_count_label_->setText(QString::number(current_datasets_.size()) + " datasets");
        update_breadcrumb("Datasets");
    }
    update_toolbar_state();
}

void GovDataFrancePanel::update_toolbar_state() {
    back_btn_->setVisible(current_view_ == Resources);
}

void GovDataFrancePanel::update_breadcrumb(const QString& text) {
    breadcrumb_label_->setText(text);
}

// ── Result handler ────────────────────────────────────────────────────────────

void GovDataFrancePanel::on_result(const QString& request_id, const services::GovDataResult& result) {
    if (!request_id.startsWith("fr_"))
        return;

    if (!result.success) {
        show_error(result.error);
        return;
    }

    QJsonArray payload = result.data["data"].toArray();

    if (request_id == "fr_services") {
        current_services_ = payload;
        populate_services(payload);
        current_view_ = Services;
        services_btn_->setChecked(true);
        datasets_btn_->setChecked(false);
        geo_btn_->setChecked(false);
        content_stack_->setCurrentIndex(Services);
        update_breadcrumb("Data Services");
        row_count_label_->setText(QString::number(payload.size()) + " services");
    } else if (request_id == "fr_datasets") {
        current_datasets_ = payload;
        populate_datasets(payload);
        current_view_ = Datasets;
        datasets_btn_->setChecked(true);
        services_btn_->setChecked(false);
        geo_btn_->setChecked(false);
        content_stack_->setCurrentIndex(Datasets);
        update_breadcrumb("Datasets  ›  "
                          " + search_query_ + "
                          "");
        row_count_label_->setText(QString::number(payload.size()) + " datasets");
    } else if (request_id == "fr_geo") {
        current_geo_ = payload;
        populate_geo(payload);
        current_view_ = Geo;
        geo_btn_->setChecked(true);
        services_btn_->setChecked(false);
        datasets_btn_->setChecked(false);
        content_stack_->setCurrentIndex(Geo);
        update_breadcrumb("Municipalities  ›  "
                          " + search_query_ + "
                          "");
        row_count_label_->setText(QString::number(payload.size()) + " results");
    } else if (request_id == "fr_resources") {
        current_resources_ = payload;
        populate_resources(payload);
        current_view_ = Resources;
        content_stack_->setCurrentIndex(Resources);
        update_breadcrumb("Datasets  ›  Column Schema");
        row_count_label_->setText(QString::number(payload.size()) + " columns");
    }

    update_toolbar_state();
}

// ── Populate tables ───────────────────────────────────────────────────────────

void GovDataFrancePanel::populate_services(const QJsonArray& json) {
    services_table_->setRowCount(0);
    services_table_->setRowCount(json.size());

    for (int i = 0; i < json.size(); ++i) {
        const auto obj = json[i].toObject();

        QString name = obj["display_name"].toString();
        if (name.isEmpty())
            name = obj["title"].toString();
        if (name.isEmpty())
            name = obj["name"].toString();

        auto* name_item = new QTableWidgetItem(name);
        name_item->setForeground(QColor(kGovDataFranceColor));
        // Full description as tooltip (may be in French)
        QString desc = obj["description"].toString();
        if (!desc.isEmpty())
            name_item->setToolTip(desc);
        services_table_->setItem(i, 0, name_item);

        const auto metrics = obj["metrics"].toObject();

        auto make_num = [](int n) {
            auto* it = new QTableWidgetItem(n >= 0 ? QString::number(n) : "—");
            it->setTextAlignment(Qt::AlignCenter);
            return it;
        };
        services_table_->setItem(i, 1, make_num(metrics["views"].toInt(-1)));
        services_table_->setItem(i, 2, make_num(metrics["followers"].toInt(-1)));

        QString created = obj["created_at"].toString();
        if (created.length() > 10)
            created = created.left(10);
        auto* dt = new QTableWidgetItem(created);
        dt->setForeground(QColor(ui::colors::TEXT_SECONDARY()));
        services_table_->setItem(i, 3, dt);
    }

    LOG_INFO("GovFrance", QString("Populated %1 json services").arg(json.size()));
}

void GovDataFrancePanel::populate_datasets(const QJsonArray& json) {
    datasets_table_->setRowCount(0);
    datasets_table_->setRowCount(json.size());

    for (int i = 0; i < json.size(); ++i) {
        const auto obj = json[i].toObject();

        QString title = obj["title"].toString();
        if (title.isEmpty())
            title = obj["original_title"].toString();
        if (title.isEmpty())
            title = obj["id"].toString();

        auto* title_item = new QTableWidgetItem(title);
        title_item->setData(Qt::UserRole, obj["id"].toString());
        // French description as tooltip
        QString desc = obj["description"].toString();
        if (!desc.isEmpty())
            title_item->setToolTip(desc);
        datasets_table_->setItem(i, 0, title_item);

        // organization is a plain string in this API, not an object
        QString org = obj["organization"].toString();
        auto* org_item = new QTableWidgetItem(org);
        org_item->setForeground(QColor(ui::colors::TEXT_SECONDARY()));
        org_item->setToolTip(org);
        datasets_table_->setItem(i, 1, org_item);

        QString license = obj["license"].toString();
        auto* lic_item = new QTableWidgetItem(license.isEmpty() ? "—" : license);
        lic_item->setForeground(QColor(ui::colors::TEXT_SECONDARY()));
        datasets_table_->setItem(i, 2, lic_item);

        int num_res = obj["num_resources"].toInt(0);
        auto* res_item = new QTableWidgetItem(QString::number(num_res));
        res_item->setTextAlignment(Qt::AlignCenter);
        res_item->setForeground(QColor(kGovDataFranceColor));
        datasets_table_->setItem(i, 3, res_item);

        QString modified = obj["metadata_modified"].toString();
        if (modified.length() > 10)
            modified = modified.left(10);
        datasets_table_->setItem(i, 4, new QTableWidgetItem(modified));
    }

    LOG_INFO("GovFrance", QString("Populated %1 datasets").arg(json.size()));
}

void GovDataFrancePanel::populate_geo(const QJsonArray& json) {
    geo_table_->setRowCount(0);
    geo_table_->setRowCount(json.size());

    for (int i = 0; i < json.size(); ++i) {
        const auto obj = json[i].toObject();

        auto* name_item = new QTableWidgetItem(obj["name"].toString());
        name_item->setForeground(QColor(kGovDataFranceColor));
        geo_table_->setItem(i, 0, name_item);

        geo_table_->setItem(i, 1, new QTableWidgetItem(obj["code"].toString()));

        // Postal codes is an array — join first few
        QStringList postal;
        for (const auto& p : obj["postal_codes"].toArray())
            postal << p.toString();
        QString postal_str = postal.mid(0, 3).join(", ");
        if (postal.size() > 3)
            postal_str += QString(" +%1").arg(postal.size() - 3);
        geo_table_->setItem(i, 2, new QTableWidgetItem(postal_str));

        geo_table_->setItem(i, 3, new QTableWidgetItem(obj["department_code"].toString()));
        geo_table_->setItem(i, 4, new QTableWidgetItem(obj["region_code"].toString()));

        int pop = obj["population"].toInt(0);
        auto* pop_item = new QTableWidgetItem(pop > 0 ? QString::number(pop) : "—");
        pop_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        geo_table_->setItem(i, 5, pop_item);
    }

    LOG_INFO("GovFrance", QString("Populated %1 municipalities").arg(json.size()));
}

void GovDataFrancePanel::populate_resources(const QJsonArray& json) {
    resources_table_->setRowCount(0);
    resources_table_->setRowCount(json.size());

    for (int i = 0; i < json.size(); ++i) {
        const auto obj = json[i].toObject();

        // Schema rows: name = column name, format = type
        auto* col_item = new QTableWidgetItem(obj["name"].toString());
        col_item->setForeground(QColor(kGovDataFranceColor));
        resources_table_->setItem(i, 0, col_item);

        QString type = obj["format"].toString();
        auto* type_item = new QTableWidgetItem(type.isEmpty() ? "—" : type);
        type_item->setForeground(QColor(ui::colors::TEXT_SECONDARY()));
        type_item->setTextAlignment(Qt::AlignCenter);
        resources_table_->setItem(i, 1, type_item);
    }

    LOG_INFO("GovFrance", QString("Populated %1 schema columns").arg(json.size()));
}

// ── Status helpers ────────────────────────────────────────────────────────────

void GovDataFrancePanel::show_loading(const QString& message) {
    status_label_->setStyleSheet(QString("color:%1; font-size:13px; background:transparent;").arg(kGovDataFranceColor));
    status_label_->setText(message);
    content_stack_->setCurrentIndex(Status);
}

void GovDataFrancePanel::show_error(const QString& message) {
    status_label_->setStyleSheet(
        QString("color:%1; font-size:12px; background:transparent;").arg(ui::colors::NEGATIVE()));
    status_label_->setText("Error: " + message);
    content_stack_->setCurrentIndex(Status);
    LOG_ERROR("GovFrance", message);
}

void GovDataFrancePanel::show_status(const QString& message, bool is_error) {
    status_label_->setStyleSheet(
        is_error ? QString("color:%1; font-size:12px; background:transparent;").arg(ui::colors::NEGATIVE())
                 : QString("color:%1; font-size:12px; background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
    status_label_->setText(message);
    content_stack_->setCurrentIndex(Status);
}

// ── CSV export ────────────────────────────────────────────────────────────────

void GovDataFrancePanel::on_export_csv() {
    QTableWidget* table = nullptr;
    QString def_name;

    switch (current_view_) {
        case Services:
            if (services_table_->rowCount() > 0) {
                table = services_table_;
                def_name = "france_data_services.csv";
            }
            break;
        case Datasets:
            if (datasets_table_->rowCount() > 0) {
                table = datasets_table_;
                def_name = "france_datasets.csv";
            }
            break;
        case Geo:
            if (geo_table_->rowCount() > 0) {
                table = geo_table_;
                def_name = "france_municipalities.csv";
            }
            break;
        case Resources:
            if (resources_table_->rowCount() > 0) {
                table = resources_table_;
                def_name = "france_schema.csv";
            }
            break;
        default:
            break;
    }
    if (!table)
        return;

    export_table_to_csv(table, def_name, this);
}

} // namespace fincept::screens
