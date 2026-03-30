// src/screens/gov_data/GovDataFrancePanel.cpp
// France data.gouv.fr panel — Data Services | Datasets (search) | Geo Search
#include "screens/gov_data/GovDataFrancePanel.h"

#include "core/logging/Logger.h"
#include "services/gov_data/GovDataService.h"

#include <QDesktopServices>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QScrollArea>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>

namespace fincept::screens {
namespace {
static constexpr const char* kGovDataFranceScript = "french_gov_api.py";
static constexpr const char* kGovDataFranceColor  = "#2563EB";
} // namespace

// ── Stylesheet ────────────────────────────────────────────────────────────────

static const QString kPanelStyle = QStringLiteral(
    "#frPanelToolbar { background:#111111; border-bottom:1px solid #1a1a1a; }"

    "#frTabBtn { background:transparent; color:#808080; border:1px solid #1a1a1a;"
    "  font-size:10px; font-weight:700; padding:4px 12px; letter-spacing:0.5px; }"
    "#frTabBtn:hover { color:#e5e5e5; background:#161616; }"
    "#frTabBtn:checked { background:rgba(37,99,235,0.12); color:#2563EB;"
    "  border:1px solid #2563EB; }"

    "#frBackBtn { background:transparent; color:#808080; border:1px solid #1a1a1a;"
    "  font-size:10px; font-weight:700; padding:4px 10px; }"
    "#frBackBtn:hover { color:#e5e5e5; background:#161616; }"

    "#frFetchBtn { background:#2563EB; color:#e5e5e5; border:none;"
    "  font-size:10px; font-weight:700; padding:4px 14px; }"
    "#frFetchBtn:hover { background:#1d4ed8; }"
    "#frFetchBtn:disabled { background:#1a1a1a; color:#404040; }"

    "#frCsvBtn { background:transparent; color:#808080; border:1px solid #1a1a1a;"
    "  font-size:10px; font-weight:700; padding:4px 10px; }"
    "#frCsvBtn:hover { color:#e5e5e5; background:#161616; }"

    "#frSearch { background:#080808; color:#e5e5e5; border:none;"
    "  border-bottom:1px solid #1a1a1a; padding:4px 10px; font-size:11px; }"
    "#frSearch:focus { border-bottom:1px solid #2563EB; }"

    "QTableWidget { background:#080808; color:#e5e5e5; border:none;"
    "  gridline-color:#1a1a1a; font-size:11px; alternate-background-color:#0a0a0a; }"
    "QTableWidget::item { padding:5px 8px; border-bottom:1px solid #1a1a1a; }"
    "QTableWidget::item:selected { background:rgba(37,99,235,0.10); color:#2563EB; }"
    "QHeaderView::section { background:#111111; color:#808080; border:none;"
    "  border-bottom:2px solid #1a1a1a; border-right:1px solid #1a1a1a;"
    "  padding:5px 8px; font-size:10px; font-weight:700; letter-spacing:0.5px; }"

    "#frStatusPage { background:#080808; }"
    "#frStatusMsg  { color:#808080; font-size:13px; background:transparent; }"
    "#frStatusErr  { color:#dc2626; font-size:12px; background:transparent; }"

    "#frBreadcrumb { background:#0a0a0a; border-bottom:1px solid #1a1a1a; }"
    "#frBreadText  { color:#808080; font-size:9px; background:transparent; }"
    "#frBreadCount { color:#808080; font-size:9px; background:transparent; }"

    "QScrollBar:vertical { background:#080808; width:5px; }"
    "QScrollBar::handle:vertical { background:#1a1a1a; min-height:20px; }"
    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"
);

// ── Constructor ───────────────────────────────────────────────────────────────

GovDataFrancePanel::GovDataFrancePanel(QWidget* parent)
    : QWidget(parent) {
    setStyleSheet(kPanelStyle);
    build_ui();
    connect(&services::GovDataService::instance(),
            &services::GovDataService::result_ready,
            this, &GovDataFrancePanel::on_result);
}

// ── Build UI ──────────────────────────────────────────────────────────────────

void GovDataFrancePanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_toolbar());

    // Breadcrumb bar
    breadcrumb_ = new QWidget;
    breadcrumb_->setObjectName("frBreadcrumb");
    breadcrumb_->setFixedHeight(24);
    auto* bcl = new QHBoxLayout(breadcrumb_);
    bcl->setContentsMargins(14, 0, 14, 0);
    bcl->setSpacing(4);
    breadcrumb_label_ = new QLabel("Data Services");
    breadcrumb_label_->setObjectName("frBreadText");
    bcl->addWidget(breadcrumb_label_);
    bcl->addStretch(1);
    row_count_label_ = new QLabel;
    row_count_label_->setObjectName("frBreadCount");
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
    services_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    services_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    services_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    services_table_->verticalHeader()->setVisible(false);
    services_table_->setAlternatingRowColors(true);
    services_table_->setShowGrid(true);
    content_stack_->addWidget(services_table_);  // index 0

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
    datasets_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    datasets_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    datasets_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    datasets_table_->verticalHeader()->setVisible(false);
    datasets_table_->setAlternatingRowColors(true);
    datasets_table_->setShowGrid(true);
    connect(datasets_table_, &QTableWidget::cellDoubleClicked,
            this, &GovDataFrancePanel::on_dataset_row_double_clicked);
    content_stack_->addWidget(datasets_table_);  // index 1

    // Page 2 — Geo / Municipalities table
    geo_table_ = new QTableWidget;
    geo_table_->setColumnCount(6);
    geo_table_->setHorizontalHeaderLabels(
        {"NAME", "CODE", "POSTAL", "DEPT", "REGION", "POPULATION"});
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
    geo_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    geo_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    geo_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    geo_table_->verticalHeader()->setVisible(false);
    geo_table_->setAlternatingRowColors(true);
    geo_table_->setShowGrid(true);
    content_stack_->addWidget(geo_table_);  // index 2

    // Page 3 — Resources / Column schema table
    resources_table_ = new QTableWidget;
    resources_table_->setColumnCount(2);
    resources_table_->setHorizontalHeaderLabels({"NAME", "TYPE"});
    resources_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    resources_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    resources_table_->setColumnWidth(1, 120);
    resources_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    resources_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    resources_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    resources_table_->verticalHeader()->setVisible(false);
    resources_table_->setAlternatingRowColors(true);
    resources_table_->setShowGrid(true);
    content_stack_->addWidget(resources_table_);  // index 3

    // Page 4 — Status / loading / error
    auto* status_page = new QWidget;
    status_page->setObjectName("frStatusPage");
    auto* svl = new QVBoxLayout(status_page);
    svl->setAlignment(Qt::AlignCenter);
    status_label_ = new QLabel;
    status_label_->setObjectName("frStatusMsg");
    status_label_->setAlignment(Qt::AlignCenter);
    status_label_->setWordWrap(true);
    svl->addWidget(status_label_);
    content_stack_->addWidget(status_page);  // index 4

    root->addWidget(content_stack_, 1);
}

QWidget* GovDataFrancePanel::build_toolbar() {
    auto* bar = new QWidget;
    bar->setObjectName("frPanelToolbar");
    bar->setFixedHeight(40);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(10, 0, 10, 0);
    hl->setSpacing(5);

    back_btn_ = new QPushButton("← BACK");
    back_btn_->setObjectName("frBackBtn");
    back_btn_->setVisible(false);
    back_btn_->setCursor(Qt::PointingHandCursor);
    connect(back_btn_, &QPushButton::clicked, this, &GovDataFrancePanel::on_back);
    hl->addWidget(back_btn_);

    hl->addSpacing(4);

    services_btn_ = new QPushButton("DATA SERVICES");
    services_btn_->setObjectName("frTabBtn");
    services_btn_->setCheckable(true);
    services_btn_->setChecked(true);
    services_btn_->setCursor(Qt::PointingHandCursor);
    connect(services_btn_, &QPushButton::clicked,
            this, [this]() { on_tab_changed(Services); });
    hl->addWidget(services_btn_);

    datasets_btn_ = new QPushButton("DATASETS");
    datasets_btn_->setObjectName("frTabBtn");
    datasets_btn_->setCheckable(true);
    datasets_btn_->setCursor(Qt::PointingHandCursor);
    connect(datasets_btn_, &QPushButton::clicked,
            this, [this]() { on_tab_changed(Datasets); });
    hl->addWidget(datasets_btn_);

    geo_btn_ = new QPushButton("GEO SEARCH");
    geo_btn_->setObjectName("frTabBtn");
    geo_btn_->setCheckable(true);
    geo_btn_->setCursor(Qt::PointingHandCursor);
    connect(geo_btn_, &QPushButton::clicked,
            this, [this]() { on_tab_changed(Geo); });
    hl->addWidget(geo_btn_);

    hl->addSpacing(10);

    search_input_ = new QLineEdit;
    search_input_->setObjectName("frSearch");
    search_input_->setPlaceholderText("Search…");
    search_input_->setFixedWidth(210);
    search_input_->setFixedHeight(26);
    connect(search_input_, &QLineEdit::returnPressed,
            this, &GovDataFrancePanel::on_fetch);
    hl->addWidget(search_input_);

    fetch_btn_ = new QPushButton("FETCH");
    fetch_btn_->setObjectName("frFetchBtn");
    fetch_btn_->setCursor(Qt::PointingHandCursor);
    connect(fetch_btn_, &QPushButton::clicked,
            this, &GovDataFrancePanel::on_fetch);
    hl->addWidget(fetch_btn_);

    hl->addStretch(1);

    export_btn_ = new QPushButton("CSV");
    export_btn_->setObjectName("frCsvBtn");
    export_btn_->setCursor(Qt::PointingHandCursor);
    connect(export_btn_, &QPushButton::clicked,
            this, &GovDataFrancePanel::on_export_csv);
    hl->addWidget(export_btn_);

    return bar;
}

// ── Public slot ───────────────────────────────────────────────────────────────

void GovDataFrancePanel::load_initial_data() {
    show_loading("Loading data services from data.gouv.fr…");
    services::GovDataService::instance().execute(
        kGovDataFranceScript, "publishers", {}, "fr_services");
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
            row_count_label_->setText(
                QString::number(current_services_.size()) + " services");
        }
        update_breadcrumb("Data Services");
    } else if (view == Datasets) {
        if (current_datasets_.isEmpty()) {
            show_status("Enter a search term above and click FETCH to find datasets.");
        } else {
            content_stack_->setCurrentIndex(Datasets);
            row_count_label_->setText(
                QString::number(current_datasets_.size()) + " datasets");
        }
        update_breadcrumb("Datasets");
        search_input_->setPlaceholderText("Search datasets…");
        search_input_->setFocus();
    } else if (view == Geo) {
        if (current_geo_.isEmpty()) {
            show_status("Enter a municipality name above and click FETCH to search.");
        } else {
            content_stack_->setCurrentIndex(Geo);
            row_count_label_->setText(
                QString::number(current_geo_.size()) + " municipalities");
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
        services::GovDataService::instance().execute(
            kGovDataFranceScript, "publishers", {}, "fr_services");
        return;
    }

    if (query.isEmpty()) return;
    search_query_ = query;

    if (current_view_ == Datasets) {
        show_loading("Searching datasets for "" + query + ""…");
        services::GovDataService::instance().execute(
            kGovDataFranceScript, "datasets", {query, "50"}, "fr_datasets");
    } else if (current_view_ == Geo) {
        show_loading("Searching municipalities for "" + query + ""…");
        services::GovDataService::instance().execute(
            kGovDataFranceScript, "municipalities", {query}, "fr_geo");
    }
}

void GovDataFrancePanel::on_dataset_row_double_clicked(int row) {
    auto* item = datasets_table_->item(row, 0);
    if (!item) return;
    selected_dataset_id_ = item->data(Qt::UserRole).toString();
    if (selected_dataset_id_.isEmpty()) return;

    show_loading("Loading column schema for "" + item->text() + ""…");
    services::GovDataService::instance().execute(
        kGovDataFranceScript, "resources", {selected_dataset_id_}, "fr_resources");
}

void GovDataFrancePanel::on_back() {
    if (current_view_ == Resources) {
        content_stack_->setCurrentIndex(Datasets);
        current_view_ = Datasets;
        services_btn_->setChecked(false);
        datasets_btn_->setChecked(true);
        geo_btn_->setChecked(false);
        row_count_label_->setText(
            QString::number(current_datasets_.size()) + " datasets");
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

void GovDataFrancePanel::on_result(const QString& request_id,
                                   const services::GovDataResult& result) {
    if (!request_id.startsWith("fr_")) return;

    if (!result.success) {
        show_error(result.error);
        return;
    }

    QJsonArray data = result.data["data"].toArray();

    if (request_id == "fr_services") {
        current_services_ = data;
        populate_services(data);
        current_view_ = Services;
        services_btn_->setChecked(true);
        datasets_btn_->setChecked(false);
        geo_btn_->setChecked(false);
        content_stack_->setCurrentIndex(Services);
        update_breadcrumb("Data Services");
        row_count_label_->setText(
            QString::number(data.size()) + " services");
    } else if (request_id == "fr_datasets") {
        current_datasets_ = data;
        populate_datasets(data);
        current_view_ = Datasets;
        datasets_btn_->setChecked(true);
        services_btn_->setChecked(false);
        geo_btn_->setChecked(false);
        content_stack_->setCurrentIndex(Datasets);
        update_breadcrumb("Datasets  ›  "" + search_query_ + """);
        row_count_label_->setText(
            QString::number(data.size()) + " datasets");
    } else if (request_id == "fr_geo") {
        current_geo_ = data;
        populate_geo(data);
        current_view_ = Geo;
        geo_btn_->setChecked(true);
        services_btn_->setChecked(false);
        datasets_btn_->setChecked(false);
        content_stack_->setCurrentIndex(Geo);
        update_breadcrumb("Municipalities  ›  "" + search_query_ + """);
        row_count_label_->setText(
            QString::number(data.size()) + " results");
    } else if (request_id == "fr_resources") {
        current_resources_ = data;
        populate_resources(data);
        current_view_ = Resources;
        content_stack_->setCurrentIndex(Resources);
        update_breadcrumb("Datasets  ›  Column Schema");
        row_count_label_->setText(
            QString::number(data.size()) + " columns");
    }

    update_toolbar_state();
}

// ── Populate tables ───────────────────────────────────────────────────────────

void GovDataFrancePanel::populate_services(const QJsonArray& data) {
    services_table_->setRowCount(0);
    services_table_->setRowCount(data.size());

    for (int i = 0; i < data.size(); ++i) {
        const auto obj = data[i].toObject();

        QString name = obj["display_name"].toString();
        if (name.isEmpty()) name = obj["title"].toString();
        if (name.isEmpty()) name = obj["name"].toString();

        auto* name_item = new QTableWidgetItem(name);
        name_item->setForeground(QColor(kGovDataFranceColor));
        // Full description as tooltip (may be in French)
        QString desc = obj["description"].toString();
        if (!desc.isEmpty()) name_item->setToolTip(desc);
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
        if (created.length() > 10) created = created.left(10);
        auto* dt = new QTableWidgetItem(created);
        dt->setForeground(QColor("#808080"));
        services_table_->setItem(i, 3, dt);
    }

    LOG_INFO("GovFrance", QString("Populated %1 data services").arg(data.size()));
}

void GovDataFrancePanel::populate_datasets(const QJsonArray& data) {
    datasets_table_->setRowCount(0);
    datasets_table_->setRowCount(data.size());

    for (int i = 0; i < data.size(); ++i) {
        const auto obj = data[i].toObject();

        QString title = obj["title"].toString();
        if (title.isEmpty()) title = obj["original_title"].toString();
        if (title.isEmpty()) title = obj["id"].toString();

        auto* title_item = new QTableWidgetItem(title);
        title_item->setData(Qt::UserRole, obj["id"].toString());
        // French description as tooltip
        QString desc = obj["description"].toString();
        if (!desc.isEmpty()) title_item->setToolTip(desc);
        datasets_table_->setItem(i, 0, title_item);

        // organization is a plain string in this API, not an object
        QString org = obj["organization"].toString();
        auto* org_item = new QTableWidgetItem(org);
        org_item->setForeground(QColor("#808080"));
        org_item->setToolTip(org);
        datasets_table_->setItem(i, 1, org_item);

        QString license = obj["license"].toString();
        auto* lic_item = new QTableWidgetItem(license.isEmpty() ? "—" : license);
        lic_item->setForeground(QColor("#808080"));
        datasets_table_->setItem(i, 2, lic_item);

        int num_res = obj["num_resources"].toInt(0);
        auto* res_item = new QTableWidgetItem(QString::number(num_res));
        res_item->setTextAlignment(Qt::AlignCenter);
        res_item->setForeground(QColor(kGovDataFranceColor));
        datasets_table_->setItem(i, 3, res_item);

        QString modified = obj["metadata_modified"].toString();
        if (modified.length() > 10) modified = modified.left(10);
        datasets_table_->setItem(i, 4, new QTableWidgetItem(modified));
    }

    LOG_INFO("GovFrance", QString("Populated %1 datasets").arg(data.size()));
}

void GovDataFrancePanel::populate_geo(const QJsonArray& data) {
    geo_table_->setRowCount(0);
    geo_table_->setRowCount(data.size());

    for (int i = 0; i < data.size(); ++i) {
        const auto obj = data[i].toObject();

        auto* name_item = new QTableWidgetItem(obj["name"].toString());
        name_item->setForeground(QColor(kGovDataFranceColor));
        geo_table_->setItem(i, 0, name_item);

        geo_table_->setItem(i, 1, new QTableWidgetItem(obj["code"].toString()));

        // Postal codes is an array — join first few
        QStringList postal;
        for (const auto& p : obj["postal_codes"].toArray())
            postal << p.toString();
        QString postal_str = postal.mid(0, 3).join(", ");
        if (postal.size() > 3) postal_str += QString(" +%1").arg(postal.size() - 3);
        geo_table_->setItem(i, 2, new QTableWidgetItem(postal_str));

        geo_table_->setItem(i, 3,
            new QTableWidgetItem(obj["department_code"].toString()));
        geo_table_->setItem(i, 4,
            new QTableWidgetItem(obj["region_code"].toString()));

        int pop = obj["population"].toInt(0);
        auto* pop_item = new QTableWidgetItem(
            pop > 0 ? QString::number(pop) : "—");
        pop_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        geo_table_->setItem(i, 5, pop_item);
    }

    LOG_INFO("GovFrance", QString("Populated %1 municipalities").arg(data.size()));
}

void GovDataFrancePanel::populate_resources(const QJsonArray& data) {
    resources_table_->setRowCount(0);
    resources_table_->setRowCount(data.size());

    for (int i = 0; i < data.size(); ++i) {
        const auto obj = data[i].toObject();

        // Schema rows: name = column name, format = type
        auto* col_item = new QTableWidgetItem(obj["name"].toString());
        col_item->setForeground(QColor(kGovDataFranceColor));
        resources_table_->setItem(i, 0, col_item);

        QString type = obj["format"].toString();
        auto* type_item = new QTableWidgetItem(type.isEmpty() ? "—" : type);
        type_item->setForeground(QColor("#808080"));
        type_item->setTextAlignment(Qt::AlignCenter);
        resources_table_->setItem(i, 1, type_item);
    }

    LOG_INFO("GovFrance", QString("Populated %1 schema columns").arg(data.size()));
}

// ── Status helpers ────────────────────────────────────────────────────────────

void GovDataFrancePanel::show_loading(const QString& message) {
    status_label_->setStyleSheet(
        QString("color:%1; font-size:13px; background:transparent;").arg(kGovDataFranceColor));
    status_label_->setText(message);
    content_stack_->setCurrentIndex(Status);
}

void GovDataFrancePanel::show_error(const QString& message) {
    status_label_->setStyleSheet(
        "color:#dc2626; font-size:12px; background:transparent;");
    status_label_->setText("Error: " + message);
    content_stack_->setCurrentIndex(Status);
    LOG_ERROR("GovFrance", message);
}

void GovDataFrancePanel::show_status(const QString& message, bool is_error) {
    status_label_->setStyleSheet(is_error
        ? "color:#dc2626; font-size:12px; background:transparent;"
        : "color:#808080; font-size:12px; background:transparent;");
    status_label_->setText(message);
    content_stack_->setCurrentIndex(Status);
}

// ── CSV export ────────────────────────────────────────────────────────────────

void GovDataFrancePanel::on_export_csv() {
    QTableWidget* table    = nullptr;
    QString       def_name;

    switch (current_view_) {
        case Services:
            if (services_table_->rowCount() > 0) {
                table    = services_table_;
                def_name = "france_data_services.csv";
            }
            break;
        case Datasets:
            if (datasets_table_->rowCount() > 0) {
                table    = datasets_table_;
                def_name = "france_datasets.csv";
            }
            break;
        case Geo:
            if (geo_table_->rowCount() > 0) {
                table    = geo_table_;
                def_name = "france_municipalities.csv";
            }
            break;
        case Resources:
            if (resources_table_->rowCount() > 0) {
                table    = resources_table_;
                def_name = "france_schema.csv";
            }
            break;
        default:
            break;
    }
    if (!table) return;

    QString path = QFileDialog::getSaveFileName(
        this, "Export CSV", def_name, "CSV Files (*.csv)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&file);

    QStringList headers;
    for (int c = 0; c < table->columnCount(); ++c) {
        auto* h = table->horizontalHeaderItem(c);
        headers << (h ? h->text() : QString::number(c));
    }
    out << headers.join(",") << "\n";

    for (int r = 0; r < table->rowCount(); ++r) {
        QStringList row;
        for (int c = 0; c < table->columnCount(); ++c) {
            auto* item = table->item(r, c);
            QString val = item ? item->text() : "";
            if (val.contains(',') || val.contains('"'))
                val = "\"" + val.replace("\"", "\"\"") + "\"";
            row << val;
        }
        out << row.join(",") << "\n";
    }

    LOG_INFO("GovFrance", "Exported CSV: " + path);
}

} // namespace fincept::screens
