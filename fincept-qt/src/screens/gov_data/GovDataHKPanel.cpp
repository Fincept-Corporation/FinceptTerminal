// src/screens/gov_data/GovDataHKPanel.cpp
// Hong Kong data.gov.hk panel
// Categories use publishers command (normalized string array).
// HK category_datasets may return empty — fall back to datasets_list + client-side filter.
#include "screens/gov_data/GovDataHKPanel.h"

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
static constexpr const char* kGovDataHKScript = "data_gov_hk_api.py";
static constexpr const char* kGovDataHKColor  = "#F43F5E";
} // namespace

// ── Stylesheet ────────────────────────────────────────────────────────────────

static const QString kPanelStyle = QStringLiteral(
    "#hkPanelToolbar { background:#111111; border-bottom:1px solid #1a1a1a; }"

    "#hkTabBtn { background:transparent; color:#808080; border:1px solid #1a1a1a;"
    "  font-size:10px; font-weight:700; padding:4px 12px; letter-spacing:0.5px; }"
    "#hkTabBtn:hover { color:#e5e5e5; background:#161616; }"
    "#hkTabBtn:checked { background:rgba(244,63,94,0.12); color:#F43F5E;"
    "  border:1px solid #F43F5E; }"

    "#hkBackBtn { background:transparent; color:#808080; border:1px solid #1a1a1a;"
    "  font-size:10px; font-weight:700; padding:4px 10px; }"
    "#hkBackBtn:hover { color:#e5e5e5; background:#161616; }"

    "#hkFetchBtn { background:#F43F5E; color:#e5e5e5; border:none;"
    "  font-size:10px; font-weight:700; padding:4px 14px; }"
    "#hkFetchBtn:hover { background:#e11d48; }"
    "#hkFetchBtn:disabled { background:#1a1a1a; color:#404040; }"

    "#hkCsvBtn { background:transparent; color:#808080; border:1px solid #1a1a1a;"
    "  font-size:10px; font-weight:700; padding:4px 10px; }"
    "#hkCsvBtn:hover { color:#e5e5e5; background:#161616; }"

    "#hkSearch { background:#080808; color:#e5e5e5; border:none;"
    "  border-bottom:1px solid #1a1a1a; padding:4px 10px; font-size:11px; }"
    "#hkSearch:focus { border-bottom:1px solid #F43F5E; }"

    "QTableWidget { background:#080808; color:#e5e5e5; border:none;"
    "  gridline-color:#1a1a1a; font-size:11px; alternate-background-color:#0a0a0a; }"
    "QTableWidget::item { padding:5px 8px; border-bottom:1px solid #1a1a1a; }"
    "QTableWidget::item:selected { background:rgba(244,63,94,0.10); color:#F43F5E; }"
    "QHeaderView::section { background:#111111; color:#808080; border:none;"
    "  border-bottom:2px solid #1a1a1a; border-right:1px solid #1a1a1a;"
    "  padding:5px 8px; font-size:10px; font-weight:700; letter-spacing:0.5px; }"

    "#hkStatusPage { background:#080808; }"
    "#hkStatusMsg  { color:#808080; font-size:13px; background:transparent; }"
    "#hkStatusErr  { color:#dc2626; font-size:12px; background:transparent; }"

    "#hkBreadcrumb { background:#0a0a0a; border-bottom:1px solid #1a1a1a; }"
    "#hkBreadText  { color:#808080; font-size:9px; background:transparent; }"
    "#hkBreadCount { color:#808080; font-size:9px; background:transparent; }"

    "QScrollBar:vertical { background:#080808; width:5px; }"
    "QScrollBar::handle:vertical { background:#1a1a1a; min-height:20px; }"
    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"
);

// ── Constructor ───────────────────────────────────────────────────────────────

GovDataHKPanel::GovDataHKPanel(QWidget* parent)
    : QWidget(parent) {
    setStyleSheet(kPanelStyle);
    build_ui();
    connect(&services::GovDataService::instance(),
            &services::GovDataService::result_ready,
            this, &GovDataHKPanel::on_result);
}

// ── Build UI ──────────────────────────────────────────────────────────────────

void GovDataHKPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_toolbar());

    // Breadcrumb bar
    breadcrumb_ = new QWidget;
    breadcrumb_->setObjectName("hkBreadcrumb");
    breadcrumb_->setFixedHeight(24);
    auto* bcl = new QHBoxLayout(breadcrumb_);
    bcl->setContentsMargins(14, 0, 14, 0);
    bcl->setSpacing(4);
    breadcrumb_label_ = new QLabel("Categories");
    breadcrumb_label_->setObjectName("hkBreadText");
    bcl->addWidget(breadcrumb_label_);
    bcl->addStretch(1);
    row_count_label_ = new QLabel;
    row_count_label_->setObjectName("hkBreadCount");
    bcl->addWidget(row_count_label_);
    root->addWidget(breadcrumb_);

    // Content stack
    content_stack_ = new QStackedWidget;

    // Page 0 — Categories table (1 col, no dataset count — always -1 from HK API)
    categories_table_ = new QTableWidget;
    categories_table_->setColumnCount(1);
    categories_table_->setHorizontalHeaderLabels({"CATEGORY"});
    categories_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    categories_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    categories_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    categories_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    categories_table_->verticalHeader()->setVisible(false);
    categories_table_->setAlternatingRowColors(true);
    categories_table_->setShowGrid(true);
    connect(categories_table_, &QTableWidget::cellDoubleClicked,
            this, &GovDataHKPanel::on_category_clicked);
    content_stack_->addWidget(categories_table_);  // index 0

    // Page 1 — Datasets table
    datasets_table_ = new QTableWidget;
    datasets_table_->setColumnCount(3);
    datasets_table_->setHorizontalHeaderLabels({"TITLE", "RESOURCES", "MODIFIED"});
    datasets_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    datasets_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    datasets_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    datasets_table_->setColumnWidth(1, 90);
    datasets_table_->setColumnWidth(2, 110);
    datasets_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    datasets_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    datasets_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    datasets_table_->verticalHeader()->setVisible(false);
    datasets_table_->setAlternatingRowColors(true);
    datasets_table_->setShowGrid(true);
    connect(datasets_table_, &QTableWidget::cellDoubleClicked,
            this, &GovDataHKPanel::on_dataset_clicked);
    content_stack_->addWidget(datasets_table_);  // index 1

    // Page 2 — Resources table
    resources_table_ = new QTableWidget;
    resources_table_->setColumnCount(3);
    resources_table_->setHorizontalHeaderLabels({"NAME", "FORMAT", "MODIFIED"});
    resources_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    resources_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    resources_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    resources_table_->setColumnWidth(1, 80);
    resources_table_->setColumnWidth(2, 110);
    resources_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    resources_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    resources_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    resources_table_->verticalHeader()->setVisible(false);
    resources_table_->setAlternatingRowColors(true);
    resources_table_->setShowGrid(true);

    // OPEN button column
    connect(resources_table_, &QTableWidget::cellClicked, this,
        [this](int row, int col) {
            if (col != 2) return;
            auto* item = resources_table_->item(row, 2);
            if (!item) return;
            QString url = item->data(Qt::UserRole).toString();
            if (!url.isEmpty()) QDesktopServices::openUrl(QUrl(url));
        }, Qt::UniqueConnection);

    content_stack_->addWidget(resources_table_);  // index 2

    // Page 3 — Status
    auto* status_page = new QWidget;
    status_page->setObjectName("hkStatusPage");
    auto* svl = new QVBoxLayout(status_page);
    svl->setAlignment(Qt::AlignCenter);
    status_label_ = new QLabel;
    status_label_->setObjectName("hkStatusMsg");
    status_label_->setAlignment(Qt::AlignCenter);
    status_label_->setWordWrap(true);
    svl->addWidget(status_label_);
    content_stack_->addWidget(status_page);  // index 3

    root->addWidget(content_stack_, 1);
}

QWidget* GovDataHKPanel::build_toolbar() {
    auto* bar = new QWidget;
    bar->setObjectName("hkPanelToolbar");
    bar->setFixedHeight(40);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(10, 0, 10, 0);
    hl->setSpacing(5);

    back_btn_ = new QPushButton("← BACK");
    back_btn_->setObjectName("hkBackBtn");
    back_btn_->setVisible(false);
    back_btn_->setCursor(Qt::PointingHandCursor);
    connect(back_btn_, &QPushButton::clicked, this, &GovDataHKPanel::on_back);
    hl->addWidget(back_btn_);

    hl->addSpacing(4);

    categories_btn_ = new QPushButton("CATEGORIES");
    categories_btn_->setObjectName("hkTabBtn");
    categories_btn_->setCheckable(true);
    categories_btn_->setChecked(true);
    categories_btn_->setCursor(Qt::PointingHandCursor);
    connect(categories_btn_, &QPushButton::clicked,
            this, [this]() { on_tab_changed(Categories); });
    hl->addWidget(categories_btn_);

    datasets_btn_ = new QPushButton("DATASETS");
    datasets_btn_->setObjectName("hkTabBtn");
    datasets_btn_->setCheckable(true);
    datasets_btn_->setCursor(Qt::PointingHandCursor);
    connect(datasets_btn_, &QPushButton::clicked,
            this, [this]() { on_tab_changed(Datasets); });
    hl->addWidget(datasets_btn_);

    hl->addSpacing(10);

    search_input_ = new QLineEdit;
    search_input_->setObjectName("hkSearch");
    search_input_->setPlaceholderText("Filter datasets…");
    search_input_->setFixedWidth(210);
    search_input_->setFixedHeight(26);
    connect(search_input_, &QLineEdit::returnPressed,
            this, &GovDataHKPanel::on_fetch);
    hl->addWidget(search_input_);

    fetch_btn_ = new QPushButton("FETCH");
    fetch_btn_->setObjectName("hkFetchBtn");
    fetch_btn_->setCursor(Qt::PointingHandCursor);
    connect(fetch_btn_, &QPushButton::clicked, this, &GovDataHKPanel::on_fetch);
    hl->addWidget(fetch_btn_);

    hl->addStretch(1);

    export_btn_ = new QPushButton("CSV");
    export_btn_->setObjectName("hkCsvBtn");
    export_btn_->setCursor(Qt::PointingHandCursor);
    connect(export_btn_, &QPushButton::clicked, this, &GovDataHKPanel::on_export_csv);
    hl->addWidget(export_btn_);

    return bar;
}

// ── Public slot ───────────────────────────────────────────────────────────────

void GovDataHKPanel::load_initial_data() {
    show_loading("Loading categories from data.gov.hk…");
    services::GovDataService::instance().execute(
        kGovDataHKScript, "publishers", {}, "hk_categories");
}

// ── Toolbar actions ───────────────────────────────────────────────────────────

void GovDataHKPanel::on_tab_changed(int tab_index) {
    auto view = static_cast<View>(tab_index);
    categories_btn_->setChecked(view == Categories);
    datasets_btn_->setChecked(view == Datasets);

    current_view_ = view;

    if (view == Categories) {
        if (current_categories_.isEmpty())
            load_initial_data();
        else {
            content_stack_->setCurrentIndex(Categories);
            row_count_label_->setText(
                QString::number(current_categories_.size()) + " categories");
        }
        update_breadcrumb("Categories");
    } else if (view == Datasets) {
        if (current_datasets_.isEmpty())
            show_status("Double-click a category to load datasets, "
                        "or type a filter term above and click FETCH.");
        else {
            content_stack_->setCurrentIndex(Datasets);
            row_count_label_->setText(
                QString::number(current_datasets_.size()) + " datasets");
        }
        update_breadcrumb("Datasets");
    }

    update_toolbar_state();
}

void GovDataHKPanel::on_fetch() {
    QString query = search_input_->text().trimmed();

    // If user typed something, run datasets_list and filter client-side
    // (HK has no text search endpoint)
    if (!query.isEmpty()) {
        if (all_dataset_names_.isEmpty()) {
            show_loading("Loading full dataset list for filtering…");
            // Store query so we can filter after datasets_list returns
            services::GovDataService::instance().execute(
                kGovDataHKScript, "datasets_list", {}, "hk_datasets_list");
        } else {
            filter_datasets_list(query);
        }
        return;
    }

    // No query — go back to categories
    if (current_categories_.isEmpty())
        load_initial_data();
    else {
        current_view_ = Categories;
        categories_btn_->setChecked(true);
        datasets_btn_->setChecked(false);
        content_stack_->setCurrentIndex(Categories);
        update_breadcrumb("Categories");
        row_count_label_->setText(
            QString::number(current_categories_.size()) + " categories");
        update_toolbar_state();
    }
}

void GovDataHKPanel::filter_datasets_list(const QString& query) {
    QJsonArray filtered;
    QString lower = query.toLower();
    for (const auto& v : all_dataset_names_) {
        QString name = v.isString() ? v.toString()
                                    : v.toObject()["name"].toString();
        if (name.toLower().contains(lower)) {
            // Build a minimal dataset object for populate_datasets
            QJsonObject obj;
            obj["title"] = name;
            obj["id"]    = name;
            obj["num_resources"] = 0;
            obj["metadata_modified"] = "";
            filtered.append(obj);
        }
    }

    if (filtered.isEmpty()) {
        show_status("No datasets matched "" + query + "" in the HK catalogue.\n"
                    "HK DATA — Categories may have limited datasets");
        return;
    }

    current_datasets_ = filtered;
    populate_datasets(filtered);
    current_view_ = Datasets;
    categories_btn_->setChecked(false);
    datasets_btn_->setChecked(true);
    content_stack_->setCurrentIndex(Datasets);
    update_breadcrumb("Datasets  ›  Filter: "" + query + """);
    row_count_label_->setText(
        QString::number(filtered.size()) + " matched");
    update_toolbar_state();
}

void GovDataHKPanel::on_category_clicked(int row) {
    auto* item = categories_table_->item(row, 0);
    if (!item) return;
    selected_category_id_   = item->data(Qt::UserRole).toString();
    selected_category_name_ = item->text();

    show_loading("Loading datasets for "" + selected_category_name_ + ""…");
    services::GovDataService::instance().execute(
        kGovDataHKScript, "datasets", {selected_category_id_, "100"}, "hk_datasets");
}

void GovDataHKPanel::on_dataset_clicked(int row) {
    auto* item = datasets_table_->item(row, 0);
    if (!item) return;
    selected_dataset_id_ = item->data(Qt::UserRole).toString();
    if (selected_dataset_id_.isEmpty()) return;

    show_loading("Loading resources…");
    services::GovDataService::instance().execute(
        kGovDataHKScript, "resources", {selected_dataset_id_}, "hk_resources");
}

void GovDataHKPanel::on_back() {
    if (current_view_ == Resources) {
        content_stack_->setCurrentIndex(Datasets);
        current_view_ = Datasets;
        row_count_label_->setText(
            QString::number(current_datasets_.size()) + " datasets");
        update_breadcrumb("Datasets  ›  " + selected_category_name_);
    } else if (current_view_ == Datasets) {
        content_stack_->setCurrentIndex(Categories);
        current_view_ = Categories;
        categories_btn_->setChecked(true);
        datasets_btn_->setChecked(false);
        selected_category_id_.clear();
        selected_category_name_.clear();
        row_count_label_->setText(
            QString::number(current_categories_.size()) + " categories");
        update_breadcrumb("Categories");
    }
    update_toolbar_state();
}

void GovDataHKPanel::update_toolbar_state() {
    back_btn_->setVisible(current_view_ == Datasets || current_view_ == Resources);
}

void GovDataHKPanel::update_breadcrumb(const QString& text) {
    breadcrumb_label_->setText(text);
}

// ── Result handler ────────────────────────────────────────────────────────────

void GovDataHKPanel::on_result(const QString& request_id,
                               const services::GovDataResult& result) {
    if (!request_id.startsWith("hk_")) return;

    if (!result.success) {
        show_error(result.error);
        return;
    }

    QJsonArray data = result.data["data"].toArray();

    if (request_id == "hk_categories") {
        current_categories_ = data;
        populate_categories(data);
        current_view_ = Categories;
        categories_btn_->setChecked(true);
        datasets_btn_->setChecked(false);
        content_stack_->setCurrentIndex(Categories);
        update_breadcrumb("Categories");
        row_count_label_->setText(
            QString::number(data.size()) + " categories");

    } else if (request_id == "hk_datasets") {
        if (data.isEmpty()) {
            // HK category_datasets frequently returns empty — show note
            show_status("HK DATA — Categories may have limited datasets\n\n"
                        "No datasets found for "" + selected_category_name_ + "".\n"
                        "Try searching by name using the search box above.");
            LOG_WARN("GovHK",
                "Category "" + selected_category_id_ + "" returned 0 datasets");
            return;
        }
        current_datasets_ = data;
        populate_datasets(data);
        current_view_ = Datasets;
        categories_btn_->setChecked(false);
        datasets_btn_->setChecked(true);
        content_stack_->setCurrentIndex(Datasets);

        int total = result.data["metadata"].toObject()["total_count"].toInt(0);
        if (total == 0) total = data.size();
        update_breadcrumb("Datasets  ›  " + selected_category_name_);
        row_count_label_->setText(
            QString("Showing %1 of %2").arg(data.size()).arg(total));

    } else if (request_id == "hk_datasets_list") {
        // Full dataset name list for client-side search
        all_dataset_names_ = data;
        QString query = search_input_->text().trimmed();
        if (!query.isEmpty())
            filter_datasets_list(query);
        else
            show_status("Dataset list loaded. Type a search term and click FETCH.");

    } else if (request_id == "hk_resources") {
        current_resources_ = data;
        populate_resources(data);
        current_view_ = Resources;
        content_stack_->setCurrentIndex(Resources);
        update_breadcrumb("Datasets  ›  Resources");
        row_count_label_->setText(
            QString::number(data.size()) + " files");
    }

    update_toolbar_state();
}

// ── Populate tables ───────────────────────────────────────────────────────────

void GovDataHKPanel::populate_categories(const QJsonArray& data) {
    categories_table_->setRowCount(0);
    categories_table_->setRowCount(data.size());

    for (int i = 0; i < data.size(); ++i) {
        const auto obj = data[i].toObject();

        QString name = obj["display_name"].toString();
        if (name.isEmpty()) name = obj["name"].toString();
        if (name.isEmpty()) name = obj["id"].toString();

        auto* name_item = new QTableWidgetItem(name);
        name_item->setForeground(QColor(kGovDataHKColor));
        name_item->setData(Qt::UserRole,
            obj["id"].toString().isEmpty() ? obj["name"].toString() : obj["id"].toString());
        categories_table_->setItem(i, 0, name_item);
    }

    LOG_INFO("GovHK", QString("Populated %1 categories").arg(data.size()));
}

void GovDataHKPanel::populate_datasets(const QJsonArray& data) {
    datasets_table_->setRowCount(0);
    datasets_table_->setRowCount(data.size());

    for (int i = 0; i < data.size(); ++i) {
        const auto obj = data[i].toObject();

        QString title = obj["title"].toString();
        if (title.isEmpty()) title = obj["name"].toString();

        auto* title_item = new QTableWidgetItem(title);
        title_item->setData(Qt::UserRole,
            obj["id"].toString().isEmpty() ? obj["name"].toString() : obj["id"].toString());
        datasets_table_->setItem(i, 0, title_item);

        int num_res = obj["num_resources"].toInt(0);
        // Also count from inline resources array if num_resources missing
        if (num_res == 0 && obj.contains("resources"))
            num_res = obj["resources"].toArray().size();
        auto* res_item = new QTableWidgetItem(num_res > 0 ? QString::number(num_res) : "—");
        res_item->setTextAlignment(Qt::AlignCenter);
        res_item->setForeground(QColor(kGovDataHKColor));
        datasets_table_->setItem(i, 1, res_item);

        QString modified = obj["metadata_modified"].toString();
        if (modified.length() > 10) modified = modified.left(10);
        datasets_table_->setItem(i, 2, new QTableWidgetItem(modified));
    }

    LOG_INFO("GovHK", QString("Populated %1 datasets").arg(data.size()));
}

void GovDataHKPanel::populate_resources(const QJsonArray& data) {
    resources_table_->setRowCount(0);
    resources_table_->setRowCount(data.size());

    for (int i = 0; i < data.size(); ++i) {
        const auto obj = data[i].toObject();

        QString name = obj["name"].toString();
        if (name.isEmpty()) name = obj["id"].toString();
        resources_table_->setItem(i, 0, new QTableWidgetItem(name));

        QString format = obj["format"].toString().toUpper();
        auto* fmt_item = new QTableWidgetItem(format.isEmpty() ? "—" : format);
        fmt_item->setForeground(QColor(kGovDataHKColor));
        fmt_item->setTextAlignment(Qt::AlignCenter);
        resources_table_->setItem(i, 1, fmt_item);

        QString modified = obj["last_modified"].toString();
        if (modified.length() > 10) modified = modified.left(10);
        QString url = obj["url"].toString();
        auto* mod_item = new QTableWidgetItem(modified.isEmpty() ? "—" : modified);
        mod_item->setData(Qt::UserRole, url);
        if (!url.isEmpty()) {
            mod_item->setText("↗ OPEN");
            mod_item->setForeground(QColor(kGovDataHKColor));
            mod_item->setTextAlignment(Qt::AlignCenter);
        }
        resources_table_->setItem(i, 2, mod_item);
    }

    LOG_INFO("GovHK", QString("Populated %1 resources").arg(data.size()));
}

// ── Status helpers ────────────────────────────────────────────────────────────

void GovDataHKPanel::show_loading(const QString& message) {
    status_label_->setStyleSheet(
        QString("color:%1; font-size:13px; background:transparent;").arg(kGovDataHKColor));
    status_label_->setText(message);
    content_stack_->setCurrentIndex(Status);
}

void GovDataHKPanel::show_error(const QString& message) {
    status_label_->setStyleSheet(
        "color:#dc2626; font-size:12px; background:transparent;");
    status_label_->setText("Error: " + message);
    content_stack_->setCurrentIndex(Status);
    LOG_ERROR("GovHK", message);
}

void GovDataHKPanel::show_status(const QString& message, bool is_error) {
    status_label_->setStyleSheet(is_error
        ? "color:#dc2626; font-size:12px; background:transparent;"
        : "color:#808080; font-size:12px; background:transparent;");
    status_label_->setText(message);
    content_stack_->setCurrentIndex(Status);
}

// ── CSV export ────────────────────────────────────────────────────────────────

void GovDataHKPanel::on_export_csv() {
    QTableWidget* table    = nullptr;
    QString       def_name;

    switch (current_view_) {
        case Categories:
            if (categories_table_->rowCount() > 0) {
                table    = categories_table_;
                def_name = "hk_categories.csv";
            }
            break;
        case Datasets:
            if (datasets_table_->rowCount() > 0) {
                table    = datasets_table_;
                def_name = "hk_datasets.csv";
            }
            break;
        case Resources:
            if (resources_table_->rowCount() > 0) {
                table    = resources_table_;
                def_name = "hk_resources.csv";
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

    LOG_INFO("GovHK", "Exported CSV: " + path);
}

} // namespace fincept::screens
