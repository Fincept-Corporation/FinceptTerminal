// src/screens/gov_data/GovDataAustraliaPanel.cpp
#include "screens/gov_data/GovDataAustraliaPanel.h"

#include "core/logging/Logger.h"
#include "services/gov_data/GovDataService.h"
#include "ui/theme/Theme.h"

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
static constexpr const char* kGovDataAustraliaScript = "datagov_au_api.py";
static constexpr const char* kGovDataAustraliaColor  = "#0EA5E9";
} // namespace

using namespace fincept::ui;

// ── Dynamic stylesheet ──────────────────────────────────────────────────────

static QString build_australia_style() {
    const auto& t  = ui::ThemeManager::instance().tokens();
    const auto  cc = QColor(kGovDataAustraliaColor);
    const QString cr = QString::number(cc.red());
    const QString cg = QString::number(cc.green());
    const QString cb = QString::number(cc.blue());
    const QString c  = kGovDataAustraliaColor;

    QString s;
    s += QString("#govPanelToolbar { background:%1; border-bottom:1px solid %2; }").arg(t.bg_raised, t.border_dim);

    s += QString("#govTabBtn { background:transparent; color:%1; border:1px solid %2;"
                 "  font-size:10px; font-weight:700; padding:4px 12px; letter-spacing:0.5px; }").arg(t.text_secondary, t.border_dim);
    s += QString("#govTabBtn:hover { color:%1; background:%2; }").arg(t.text_primary, t.bg_hover);
    s += QString("#govTabBtn:checked { background:rgba(%1,%2,%3,0.12); color:%4;"
                 "  border:1px solid %4; }").arg(cr, cg, cb, c);

    s += QString("#govBackBtn { background:transparent; color:%1; border:1px solid %2;"
                 "  font-size:10px; font-weight:700; padding:4px 10px; }").arg(t.text_secondary, t.border_dim);
    s += QString("#govBackBtn:hover { color:%1; background:%2; }").arg(t.text_primary, t.bg_hover);

    s += QString("#govFetchBtn { background:%1; color:%2; border:none;"
                 "  font-size:10px; font-weight:700; padding:4px 14px; }").arg(c, t.bg_base);
    s += QString("#govFetchBtn:hover { background:%1; }").arg(cc.lighter(120).name());
    s += QString("#govFetchBtn:disabled { background:%1; color:%2; }").arg(t.border_dim, t.text_dim);

    s += QString("#govCsvBtn { background:transparent; color:%1; border:1px solid %2;"
                 "  font-size:10px; font-weight:700; padding:4px 10px; }").arg(t.text_secondary, t.border_dim);
    s += QString("#govCsvBtn:hover { color:%1; background:%2; }").arg(t.text_primary, t.bg_hover);

    s += QString("#govSearch { background:%1; color:%2; border:none;"
                 "  border-bottom:1px solid %3; padding:4px 10px; font-size:11px; }").arg(t.bg_base, t.text_primary, t.border_dim);
    s += QString("#govSearch:focus { border-bottom:1px solid %1; }").arg(c);

    s += QString("QTableWidget { background:%1; color:%2; border:none;"
                 "  gridline-color:%3; font-size:11px; alternate-background-color:%4; }").arg(t.bg_base, t.text_primary, t.border_dim, t.bg_surface);
    s += QString("QTableWidget::item { padding:5px 8px; border-bottom:1px solid %1; }").arg(t.border_dim);
    s += QString("QTableWidget::item:selected { background:rgba(%1,%2,%3,0.10); color:%4; }").arg(cr, cg, cb, c);
    s += QString("QHeaderView::section { background:%1; color:%2; border:none;"
                 "  border-bottom:2px solid %3; border-right:1px solid %3;"
                 "  padding:5px 8px; font-size:10px; font-weight:700; letter-spacing:0.5px; }").arg(t.bg_raised, t.text_secondary, t.border_dim);

    s += QString("#govStatusPage { background:%1; }").arg(t.bg_base);
    s += QString("#govStatusMsg  { color:%1; font-size:13px; background:transparent; }").arg(t.text_secondary);
    s += QString("#govStatusErr  { color:%1; font-size:12px; background:transparent; }").arg(t.negative);

    s += QString("#govBreadcrumb       { background:%1; border-bottom:1px solid %2; }").arg(t.bg_surface, t.border_dim);
    s += QString("#govBreadcrumbText   { color:%1; font-size:9px; background:transparent; }").arg(t.text_secondary);

    s += QString("QScrollBar:vertical { background:%1; width:5px; }").arg(t.bg_base);
    s += QString("QScrollBar::handle:vertical { background:%1; min-height:20px; }").arg(t.border_dim);
    s += "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }";
    return s;
}

// ── Constructor ──────────────────────────────────────────────────────────────

GovDataAustraliaPanel::GovDataAustraliaPanel(QWidget* parent)
    : QWidget(parent) {
    setStyleSheet(build_australia_style());
    build_ui();
    connect(&services::GovDataService::instance(),
            &services::GovDataService::result_ready,
            this,
            &GovDataAustraliaPanel::on_result);
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this]() {
        setStyleSheet(build_australia_style());
    });
}

// ── UI Construction ──────────────────────────────────────────────────────────

void GovDataAustraliaPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_toolbar());

    // Breadcrumb bar
    breadcrumb_ = new QWidget;
    breadcrumb_->setObjectName("govBreadcrumb");
    breadcrumb_->setFixedHeight(24);
    auto* bcl = new QHBoxLayout(breadcrumb_);
    bcl->setContentsMargins(14, 0, 14, 0);
    bcl->setSpacing(4);
    breadcrumb_label_ = new QLabel("All Agencies");
    breadcrumb_label_->setObjectName("govBreadcrumbText");
    bcl->addWidget(breadcrumb_label_);
    bcl->addStretch(1);
    row_count_label_ = new QLabel;
    row_count_label_->setObjectName("govBreadcrumbText");
    bcl->addWidget(row_count_label_);
    root->addWidget(breadcrumb_);

    // Content stack
    content_stack_ = new QStackedWidget;

    // Page 0 — Agencies table: NAME | DESCRIPTION | CREATED
    agencies_table_ = new QTableWidget;
    agencies_table_->setColumnCount(3);
    agencies_table_->setHorizontalHeaderLabels({"NAME", "DESCRIPTION", "CREATED"});
    agencies_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    agencies_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    agencies_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    agencies_table_->setColumnWidth(0, 220);
    agencies_table_->setColumnWidth(2, 100);
    agencies_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    agencies_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    agencies_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    agencies_table_->verticalHeader()->setVisible(false);
    agencies_table_->setAlternatingRowColors(true);
    agencies_table_->setShowGrid(true);
    connect(agencies_table_, &QTableWidget::cellDoubleClicked,
            this, &GovDataAustraliaPanel::on_agency_doubleclicked);
    content_stack_->addWidget(agencies_table_); // index 0

    // Page 1 — Datasets table: TITLE | LICENSE | AUTHOR | RESOURCES | MODIFIED
    datasets_table_ = new QTableWidget;
    datasets_table_->setColumnCount(5);
    datasets_table_->setHorizontalHeaderLabels({"TITLE", "LICENSE", "AUTHOR", "RESOURCES", "MODIFIED"});
    datasets_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    datasets_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    datasets_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    datasets_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    datasets_table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    datasets_table_->setColumnWidth(1, 130);
    datasets_table_->setColumnWidth(2, 140);
    datasets_table_->setColumnWidth(3, 75);
    datasets_table_->setColumnWidth(4, 100);
    datasets_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    datasets_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    datasets_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    datasets_table_->verticalHeader()->setVisible(false);
    datasets_table_->setAlternatingRowColors(true);
    datasets_table_->setShowGrid(true);
    connect(datasets_table_, &QTableWidget::cellDoubleClicked,
            this, &GovDataAustraliaPanel::on_dataset_doubleclicked);
    content_stack_->addWidget(datasets_table_); // index 1

    // Page 2 — Resources table: NAME | FORMAT | SIZE | OPEN
    resources_table_ = new QTableWidget;
    resources_table_->setColumnCount(4);
    resources_table_->setHorizontalHeaderLabels({"NAME", "FORMAT", "SIZE", "OPEN"});
    resources_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    resources_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    resources_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    resources_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    resources_table_->setColumnWidth(1, 80);
    resources_table_->setColumnWidth(2, 90);
    resources_table_->setColumnWidth(3, 60);
    resources_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    resources_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    resources_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    resources_table_->verticalHeader()->setVisible(false);
    resources_table_->setAlternatingRowColors(true);
    resources_table_->setShowGrid(true);
    connect(resources_table_, &QTableWidget::cellClicked, this,
        [this](int row, int col) {
            if (col != 3) return;
            auto* item = resources_table_->item(row, 3);
            if (!item) return;
            const QString url = item->data(Qt::UserRole).toString();
            if (!url.isEmpty()) QDesktopServices::openUrl(QUrl(url));
        }, Qt::UniqueConnection);
    content_stack_->addWidget(resources_table_); // index 2

    // Page 3 — Status / loading / error
    auto* status_page = new QWidget;
    status_page->setObjectName("govStatusPage");
    auto* svl = new QVBoxLayout(status_page);
    svl->setAlignment(Qt::AlignCenter);
    status_label_ = new QLabel;
    status_label_->setObjectName("govStatusMsg");
    status_label_->setAlignment(Qt::AlignCenter);
    status_label_->setWordWrap(true);
    svl->addWidget(status_label_);
    content_stack_->addWidget(status_page); // index 3

    root->addWidget(content_stack_, 1);
}

QWidget* GovDataAustraliaPanel::build_toolbar() {
    auto* bar = new QWidget;
    bar->setObjectName("govPanelToolbar");
    bar->setFixedHeight(40);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(10, 0, 10, 0);
    hl->setSpacing(5);

    // Back button — hidden until drilling in
    back_btn_ = new QPushButton("← BACK");
    back_btn_->setObjectName("govBackBtn");
    back_btn_->setVisible(false);
    back_btn_->setCursor(Qt::PointingHandCursor);
    connect(back_btn_, &QPushButton::clicked, this, &GovDataAustraliaPanel::on_back);
    hl->addWidget(back_btn_);

    hl->addSpacing(4);

    // Tab: Agencies
    agencies_btn_ = new QPushButton("AGENCIES");
    agencies_btn_->setObjectName("govTabBtn");
    agencies_btn_->setCheckable(true);
    agencies_btn_->setChecked(true);
    agencies_btn_->setCursor(Qt::PointingHandCursor);
    connect(agencies_btn_, &QPushButton::clicked, this, [this]() {
        if (current_agencies_.isEmpty()) {
            load_initial_data();
        } else {
            current_view_ = Agencies;
            content_stack_->setCurrentIndex(Agencies);
            update_toolbar_state();
            update_breadcrumb();
        }
    });
    hl->addWidget(agencies_btn_);

    // Tab: Datasets (jump back to cached datasets view)
    datasets_btn_ = new QPushButton("DATASETS");
    datasets_btn_->setObjectName("govTabBtn");
    datasets_btn_->setCheckable(true);
    datasets_btn_->setChecked(false);
    datasets_btn_->setCursor(Qt::PointingHandCursor);
    connect(datasets_btn_, &QPushButton::clicked, this, [this]() {
        if (current_datasets_.isEmpty()) return;
        current_view_ = Datasets;
        content_stack_->setCurrentIndex(Datasets);
        update_toolbar_state();
        update_breadcrumb();
    });
    hl->addWidget(datasets_btn_);

    // Tab: Recent
    recent_btn_ = new QPushButton("RECENT");
    recent_btn_->setObjectName("govTabBtn");
    recent_btn_->setCheckable(true);
    recent_btn_->setChecked(false);
    recent_btn_->setCursor(Qt::PointingHandCursor);
    connect(recent_btn_, &QPushButton::clicked, this, &GovDataAustraliaPanel::on_recent);
    hl->addWidget(recent_btn_);

    hl->addSpacing(10);

    // Search input
    search_input_ = new QLineEdit;
    search_input_->setObjectName("govSearch");
    search_input_->setPlaceholderText("Search datasets…");
    search_input_->setFixedWidth(200);
    search_input_->setFixedHeight(26);
    connect(search_input_, &QLineEdit::returnPressed, this, &GovDataAustraliaPanel::on_search);
    hl->addWidget(search_input_);

    fetch_btn_ = new QPushButton("FETCH");
    fetch_btn_->setObjectName("govFetchBtn");
    fetch_btn_->setCursor(Qt::PointingHandCursor);
    connect(fetch_btn_, &QPushButton::clicked, this, &GovDataAustraliaPanel::on_fetch);
    hl->addWidget(fetch_btn_);

    hl->addStretch(1);

    export_btn_ = new QPushButton("CSV");
    export_btn_->setObjectName("govCsvBtn");
    export_btn_->setCursor(Qt::PointingHandCursor);
    connect(export_btn_, &QPushButton::clicked, this, &GovDataAustraliaPanel::on_export_csv);
    hl->addWidget(export_btn_);

    return bar;
}

// ── Initial load ─────────────────────────────────────────────────────────────

void GovDataAustraliaPanel::load_initial_data() {
    show_loading("Loading Australian Government agencies…");
    services::GovDataService::instance().execute(
        kGovDataAustraliaScript, "organizations", {}, "ausgov_agencies");
}

// ── Result unwrapping helper ──────────────────────────────────────────────────
// org-datasets and search both return: {data:{datasets:[...], count:N, ...}}
// recent returns:                      {data:[...]}

QJsonArray GovDataAustraliaPanel::unwrap_datasets(const services::GovDataResult& result,
                                                  int& out_total) {
    out_total = 0;
    const QJsonValue raw = result.data["data"];

    if (raw.isArray()) {
        // recent command — flat array
        const QJsonArray arr = raw.toArray();
        out_total = arr.size();
        return arr;
    }

    if (raw.isObject()) {
        const QJsonObject obj = raw.toObject();
        if (obj.contains("datasets")) {
            out_total = obj["count"].toInt(0);
            const QJsonArray arr = obj["datasets"].toArray();
            if (out_total == 0) out_total = arr.size();
            return arr;
        }
        // Unexpected nested shape — try result key
        if (obj.contains("result")) {
            const QJsonArray arr = obj["result"].toArray();
            out_total = arr.size();
            return arr;
        }
    }

    return {};
}

// ── Result handler ────────────────────────────────────────────────────────────

void GovDataAustraliaPanel::on_result(const QString& request_id,
                                      const services::GovDataResult& result) {
    // Only handle request IDs that belong to this panel
    if (!request_id.startsWith("ausgov_")) return;

    if (!result.success) {
        show_error(result.error);
        LOG_ERROR("GovDataAustraliaPanel", "Request " + request_id + " failed: " + result.error);
        return;
    }

    if (request_id == "ausgov_agencies") {
        // Agencies: {data:[{id, name, title, description, created, dataset_count, ...}]}
        QJsonArray data;
        const QJsonValue raw = result.data["data"];
        if (raw.isArray())
            data = raw.toArray();

        current_agencies_ = data;
        populate_agencies(data);
        current_view_ = Agencies;
        content_stack_->setCurrentIndex(Agencies);
        LOG_INFO("GovDataAustraliaPanel",
                 "Loaded " + QString::number(data.size()) + " agencies");

    } else if (request_id.startsWith("ausgov_datasets_") ||
               request_id.startsWith("ausgov_search_")   ||
               request_id == "ausgov_recent") {

        int total = 0;
        const QJsonArray data = unwrap_datasets(result, total);

        current_datasets_ = data;
        showing_recent_   = (request_id == "ausgov_recent");

        populate_datasets(data, total);
        current_view_ = Datasets;
        content_stack_->setCurrentIndex(Datasets);

        LOG_INFO("GovDataAustraliaPanel",
                 "Loaded " + QString::number(data.size()) + " datasets"
                 + (showing_recent_ ? " (recent)" : ""));
    }

    update_toolbar_state();
    update_breadcrumb();
}

// ── Populate tables ───────────────────────────────────────────────────────────

void GovDataAustraliaPanel::populate_agencies(const QJsonArray& data) {
    agencies_table_->setRowCount(0);
    agencies_table_->setRowCount(data.size());

    for (int i = 0; i < data.size(); ++i) {
        const auto obj = data[i].toObject();

        // Prefer display_name → title → name
        QString name = obj["display_name"].toString();
        if (name.isEmpty()) name = obj["title"].toString();
        if (name.isEmpty()) name = obj["name"].toString();

        const QString id = obj["id"].toString().isEmpty()
                               ? obj["name"].toString()
                               : obj["id"].toString();

        auto* name_item = new QTableWidgetItem(name);
        name_item->setData(Qt::UserRole,     id);
        name_item->setData(Qt::UserRole + 1, name); // for breadcrumb
        name_item->setForeground(QColor(kGovDataAustraliaColor));
        agencies_table_->setItem(i, 0, name_item);

        // Description — truncate at 80 chars
        QString desc = obj["description"].toString();
        if (desc.length() > 80)
            desc = desc.left(77) + "…";
        auto* desc_item = new QTableWidgetItem(desc);
        desc_item->setForeground(QColor(ui::colors::TEXT_SECONDARY()));
        agencies_table_->setItem(i, 1, desc_item);

        // Created date
        QString created = obj["created"].toString();
        if (created.length() > 10) created = created.left(10);
        auto* created_item = new QTableWidgetItem(created);
        created_item->setTextAlignment(Qt::AlignCenter);
        agencies_table_->setItem(i, 2, created_item);
    }

    row_count_label_->setText(QString::number(data.size()) + " agencies");
}

void GovDataAustraliaPanel::populate_datasets(const QJsonArray& data, int total_count) {
    datasets_table_->setRowCount(0);
    datasets_table_->setRowCount(data.size());

    for (int i = 0; i < data.size(); ++i) {
        const auto obj = data[i].toObject();

        QString title = obj["title"].toString();
        if (title.isEmpty()) title = obj["name"].toString();

        // Store row index as UserRole so we can extract resources without re-fetch
        auto* title_item = new QTableWidgetItem(title);
        title_item->setData(Qt::UserRole, i); // row index into current_datasets_
        datasets_table_->setItem(i, 0, title_item);

        // License
        QString license = obj["license"].toString();
        if (license.isEmpty()) license = obj["license_title"].toString();
        if (license.isEmpty()) license = "—";
        auto* lic_item = new QTableWidgetItem(license);
        lic_item->setForeground(QColor(ui::colors::TEXT_SECONDARY()));
        lic_item->setToolTip(license);
        datasets_table_->setItem(i, 1, lic_item);

        // Author
        QString author = obj["author"].toString();
        if (author.isEmpty()) {
            const QJsonObject org = obj["organization"].toObject();
            author = org["title"].toString();
            if (author.isEmpty()) author = org["name"].toString();
        }
        if (author.isEmpty()) author = "—";
        datasets_table_->setItem(i, 2, new QTableWidgetItem(author));

        // Resource count — prefer resource_count field, fallback to resources array size
        int res_count = obj["resource_count"].toInt(-1);
        if (res_count < 0)
            res_count = obj["resources"].toArray().size();
        auto* res_item = new QTableWidgetItem(QString::number(res_count));
        res_item->setTextAlignment(Qt::AlignCenter);
        res_item->setForeground(QColor(kGovDataAustraliaColor));
        datasets_table_->setItem(i, 3, res_item);

        // Modified date — try "modified" then "metadata_modified"
        QString modified = obj["modified"].toString();
        if (modified.isEmpty()) modified = obj["metadata_modified"].toString();
        if (modified.length() > 10) modified = modified.left(10);
        auto* mod_item = new QTableWidgetItem(modified);
        mod_item->setTextAlignment(Qt::AlignCenter);
        datasets_table_->setItem(i, 4, mod_item);
    }

    row_count_label_->setText(
        QString("Showing %1 of %2").arg(data.size()).arg(total_count));
}

void GovDataAustraliaPanel::populate_resources(const QJsonArray& resources) {
    resources_table_->setRowCount(0);
    resources_table_->setRowCount(resources.size());

    for (int i = 0; i < resources.size(); ++i) {
        const auto obj = resources[i].toObject();

        QString name = obj["name"].toString();
        if (name.isEmpty()) name = obj["description"].toString().left(60);
        if (name.isEmpty()) name = obj["id"].toString();
        resources_table_->setItem(i, 0, new QTableWidgetItem(name));

        const QString format = obj["format"].toString().toUpper();
        auto* fmt_item = new QTableWidgetItem(format.isEmpty() ? "—" : format);
        fmt_item->setForeground(QColor(kGovDataAustraliaColor));
        fmt_item->setTextAlignment(Qt::AlignCenter);
        resources_table_->setItem(i, 1, fmt_item);

        const double size_bytes = obj["size"].toDouble(0.0);
        QString size_str = "—";
        if (size_bytes > 0.0) {
            if (size_bytes < 1024.0)
                size_str = QString::number(size_bytes, 'f', 0) + " B";
            else if (size_bytes < 1024.0 * 1024.0)
                size_str = QString::number(size_bytes / 1024.0, 'f', 1) + " KB";
            else
                size_str = QString::number(size_bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
        }
        auto* sz_item = new QTableWidgetItem(size_str);
        sz_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        resources_table_->setItem(i, 2, sz_item);

        const QString url = obj["url"].toString();
        auto* url_item = new QTableWidgetItem(url.isEmpty() ? "—" : "↗ OPEN");
        url_item->setData(Qt::UserRole, url);
        if (!url.isEmpty()) url_item->setForeground(QColor(kGovDataAustraliaColor));
        url_item->setTextAlignment(Qt::AlignCenter);
        resources_table_->setItem(i, 3, url_item);
    }

    row_count_label_->setText(QString::number(resources.size()) + " files");
}

// ── Navigation slots ──────────────────────────────────────────────────────────

void GovDataAustraliaPanel::on_agency_doubleclicked(int row, int /*col*/) {
    auto* item = agencies_table_->item(row, 0);
    if (!item) return;

    selected_agency_id_ = item->data(Qt::UserRole).toString();
    selected_agency_    = item->data(Qt::UserRole + 1).toString();
    if (selected_agency_.isEmpty())
        selected_agency_ = item->text();

    showing_recent_ = false;
    show_loading("Loading datasets for "" + selected_agency_ + ""…");

    services::GovDataService::instance().execute(
        kGovDataAustraliaScript, "org-datasets",
        {selected_agency_id_, "100"},
        "ausgov_datasets_" + selected_agency_id_);
}

void GovDataAustraliaPanel::on_dataset_doubleclicked(int row, int /*col*/) {
    // Resources are embedded — extract directly from current_datasets_, no fetch needed
    auto* item = datasets_table_->item(row, 0);
    if (!item) return;

    const int dataset_idx = item->data(Qt::UserRole).toInt();
    if (dataset_idx < 0 || dataset_idx >= current_datasets_.size()) return;

    const QJsonObject dataset = current_datasets_[dataset_idx].toObject();
    selected_dataset_title_ = dataset["title"].toString();
    if (selected_dataset_title_.isEmpty())
        selected_dataset_title_ = dataset["name"].toString();

    const QJsonArray resources = dataset["resources"].toArray();
    populate_resources(resources);
    current_view_ = Resources;
    content_stack_->setCurrentIndex(Resources);
    update_toolbar_state();
    update_breadcrumb();
}

void GovDataAustraliaPanel::on_fetch() {
    const QString query = search_input_->text().trimmed();
    if (!query.isEmpty()) {
        on_search();
    } else {
        load_initial_data();
    }
}

void GovDataAustraliaPanel::on_search() {
    const QString query = search_input_->text().trimmed();
    if (query.isEmpty()) return;

    showing_recent_     = false;
    selected_agency_.clear();
    selected_agency_id_.clear();

    show_loading("Searching for "" + query + ""…");
    services::GovDataService::instance().execute(
        kGovDataAustraliaScript, "search",
        {query, "50"},
        "ausgov_search_" + query);
}

void GovDataAustraliaPanel::on_recent() {
    selected_agency_.clear();
    selected_agency_id_.clear();

    show_loading("Loading recent datasets…");
    services::GovDataService::instance().execute(
        kGovDataAustraliaScript, "recent", {"20"}, "ausgov_recent");
}

void GovDataAustraliaPanel::on_back() {
    if (current_view_ == Resources) {
        current_view_ = Datasets;
        content_stack_->setCurrentIndex(Datasets);
    } else if (current_view_ == Datasets) {
        current_view_ = Agencies;
        content_stack_->setCurrentIndex(Agencies);
        selected_agency_.clear();
        selected_agency_id_.clear();
        showing_recent_ = false;
    }
    update_toolbar_state();
    update_breadcrumb();
}

// ── Toolbar / breadcrumb state ────────────────────────────────────────────────

void GovDataAustraliaPanel::update_toolbar_state() {
    const bool drilling = (current_view_ == Datasets || current_view_ == Resources);
    back_btn_->setVisible(drilling);

    agencies_btn_->setChecked(current_view_ == Agencies);
    datasets_btn_->setChecked(current_view_ == Datasets && !showing_recent_);
    recent_btn_->setChecked(current_view_ == Datasets && showing_recent_);
}

void GovDataAustraliaPanel::update_breadcrumb() {
    QString text;
    switch (current_view_) {
        case Agencies:
            text = "All Agencies";
            break;
        case Datasets:
            if (showing_recent_) {
                text = "Recent Datasets";
            } else if (!selected_agency_.isEmpty()) {
                text = "All Agencies  ›  " + selected_agency_ + "  ›  Datasets";
            } else {
                text = "Search Results";
            }
            break;
        case Resources:
            if (!selected_agency_.isEmpty()) {
                text = "All Agencies  ›  " + selected_agency_
                       + "  ›  Datasets  ›  " + selected_dataset_title_;
            } else {
                text = "Datasets  ›  " + selected_dataset_title_ + "  ›  Resources";
            }
            break;
        case Status:
            text = "Loading…";
            break;
    }
    breadcrumb_label_->setText(text);
}

// ── Status helpers ────────────────────────────────────────────────────────────

void GovDataAustraliaPanel::show_loading(const QString& message) {
    status_label_->setStyleSheet(
        QString("color:%1; font-size:13px; background:transparent;").arg(kGovDataAustraliaColor));
    status_label_->setText(message);
    content_stack_->setCurrentIndex(Status);
}

void GovDataAustraliaPanel::show_error(const QString& message) {
    status_label_->setStyleSheet(
        QString("color:%1; font-size:12px; background:transparent;").arg(ui::colors::NEGATIVE()));
    status_label_->setText("Error: " + message);
    content_stack_->setCurrentIndex(Status);
}

// ── CSV export ────────────────────────────────────────────────────────────────

void GovDataAustraliaPanel::on_export_csv() {
    QTableWidget* table    = nullptr;
    QString       def_name;

    switch (current_view_) {
        case Agencies:
            if (agencies_table_->rowCount() > 0) {
                table    = agencies_table_;
                def_name = "au_agencies.csv";
            }
            break;
        case Datasets:
            if (datasets_table_->rowCount() > 0) {
                table    = datasets_table_;
                def_name = showing_recent_ ? "au_recent_datasets.csv" : "au_datasets.csv";
            }
            break;
        case Resources:
            if (resources_table_->rowCount() > 0) {
                table    = resources_table_;
                def_name = "au_resources.csv";
            }
            break;
        case Status:
            break;
    }
    if (!table) return;
    export_table_csv(table, def_name);
}

void GovDataAustraliaPanel::export_table_csv(QTableWidget* table, const QString& default_name) {
    const QString path = QFileDialog::getSaveFileName(
        this, "Export CSV", default_name, "CSV Files (*.csv)");
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
            auto*   item = table->item(r, c);
            QString val  = item ? item->text() : "";
            if (val.contains(',') || val.contains('"'))
                val = "\"" + val.replace("\"", "\"\"") + "\"";
            row << val;
        }
        out << row.join(",") << "\n";
    }
}

} // namespace fincept::screens
