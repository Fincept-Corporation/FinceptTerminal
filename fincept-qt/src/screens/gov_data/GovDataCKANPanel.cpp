// src/screens/gov_data/GovDataCKANPanel.cpp
// Universal CKAN portal browser using datagovuk_api.py.
// Portal combobox is display-only — the script always queries data.gov.uk.
// Navigation: Publishers → Datasets → Resources  |  Search (independent).
#include "screens/gov_data/GovDataCKANPanel.h"

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
static constexpr const char* kGovDataCKANScript = "datagovuk_api.py";
static constexpr const char* kGovDataCKANColor = "#10B981";
} // namespace

// ── Dynamic stylesheet ───────────────────────────────────────────────────────

static QString build_ckan_style() {
    const auto& t = ui::ThemeManager::instance().tokens();
    const auto cc = QColor(kGovDataCKANColor);
    const QString cr = QString::number(cc.red());
    const QString cg = QString::number(cc.green());
    const QString cb = QString::number(cc.blue());
    const QString c = kGovDataCKANColor;

    QString s;
    // Portal bar
    s += QString("#ckanPortalBar { background:%1; border-bottom:1px solid %2; }").arg(t.bg_surface, t.border_dim);
    s += QString("#ckanPortalLabel { color:%1; font-size:9px; font-weight:700;"
                 "  letter-spacing:0.5px; background:transparent; }")
             .arg(t.text_secondary);
    s += QString("QComboBox#ckanPortalCombo { background:%1; color:%2; border:1px solid %3;"
                 "  font-size:10px; font-weight:700; padding:2px 8px; min-width:160px; }")
             .arg(t.bg_raised, c, t.border_dim);
    s += "QComboBox#ckanPortalCombo::drop-down { border:none; width:18px; }";
    s += QString("QComboBox#ckanPortalCombo QAbstractItemView { background:%1; color:%2;"
                 "  selection-background-color:rgba(%3,%4,%5,0.12); border:1px solid %6; }")
             .arg(t.bg_raised, t.text_primary, cr, cg, cb, t.border_dim);

    // Toolbar
    s += QString("#ckanPanelToolbar { background:%1; border-bottom:1px solid %2; }").arg(t.bg_raised, t.border_dim);

    // Tab buttons
    s += QString("#ckanTabBtn { background:transparent; color:%1; border:1px solid %2;"
                 "  font-size:10px; font-weight:700; padding:4px 12px; letter-spacing:0.5px; }")
             .arg(t.text_secondary, t.border_dim);
    s += QString("#ckanTabBtn:hover { color:%1; background:%2; }").arg(t.text_primary, t.bg_hover);
    s += QString("#ckanTabBtn:checked { background:rgba(%1,%2,%3,0.12); color:%4;"
                 "  border:1px solid %4; }")
             .arg(cr, cg, cb, c);

    // Back button
    s += QString("#ckanBackBtn { background:transparent; color:%1; border:1px solid %2;"
                 "  font-size:10px; font-weight:700; padding:4px 10px; }")
             .arg(t.text_secondary, t.border_dim);
    s += QString("#ckanBackBtn:hover { color:%1; background:%2; }").arg(t.text_primary, t.bg_hover);

    // Fetch / action buttons
    s += QString("#ckanFetchBtn { background:%1; color:%2; border:none;"
                 "  font-size:10px; font-weight:700; padding:4px 14px; }")
             .arg(c, t.bg_base);
    s += QString("#ckanFetchBtn:hover { background:%1; }").arg(cc.lighter(120).name());
    s += QString("#ckanFetchBtn:disabled { background:%1; color:%2; }").arg(t.border_dim, t.text_dim);
    s += QString("#ckanCsvBtn { background:transparent; color:%1; border:1px solid %2;"
                 "  font-size:10px; font-weight:700; padding:4px 10px; }")
             .arg(t.text_secondary, t.border_dim);
    s += QString("#ckanCsvBtn:hover { color:%1; background:%2; }").arg(t.text_primary, t.bg_hover);

    // Search
    s += QString("#ckanSearch { background:%1; color:%2; border:none;"
                 "  border-bottom:1px solid %3; padding:4px 10px; font-size:11px; }")
             .arg(t.bg_base, t.text_primary, t.border_dim);
    s += QString("#ckanSearch:focus { border-bottom:1px solid %1; }").arg(c);

    // Tables
    s += QString("QTableWidget { background:%1; color:%2; border:none;"
                 "  gridline-color:%3; font-size:11px; alternate-background-color:%4; }")
             .arg(t.bg_base, t.text_primary, t.border_dim, t.bg_surface);
    s += QString("QTableWidget::item { padding:5px 8px; border-bottom:1px solid %1; }").arg(t.border_dim);
    s += QString("QTableWidget::item:selected { background:rgba(%1,%2,%3,0.10); color:%4; }").arg(cr, cg, cb, c);
    s += QString("QHeaderView::section { background:%1; color:%2; border:none;"
                 "  border-bottom:2px solid %3; border-right:1px solid %3;"
                 "  padding:5px 8px; font-size:10px; font-weight:700; letter-spacing:0.5px; }")
             .arg(t.bg_raised, t.text_secondary, t.border_dim);

    // Status
    s += QString("#ckanStatusPage { background:%1; }").arg(t.bg_base);
    s += QString("#ckanStatusMsg  { color:%1; font-size:13px; background:transparent; }").arg(t.text_secondary);
    s += QString("#ckanStatusErr  { color:%1; font-size:12px; background:transparent; }").arg(t.negative);

    // Breadcrumb
    s += QString("#ckanBreadcrumb { background:%1; border-bottom:1px solid %2; }").arg(t.bg_surface, t.border_dim);
    s += QString("#ckanBreadText  { color:%1; font-size:9px; background:transparent; }").arg(t.text_secondary);
    s += QString("#ckanBreadCount { color:%1; font-size:9px; background:transparent; }").arg(t.text_secondary);

    // Scrollbar
    s += QString("QScrollBar:vertical { background:%1; width:5px; }").arg(t.bg_base);
    s += QString("QScrollBar::handle:vertical { background:%1; min-height:20px; }").arg(t.border_dim);
    s += "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }";
    return s;
}

// ── Constructor ───────────────────────────────────────────────────────────────

GovDataCKANPanel::GovDataCKANPanel(QWidget* parent) : QWidget(parent) {
    setStyleSheet(build_ckan_style());
    build_ui();
    connect(&services::GovDataService::instance(), &services::GovDataService::result_ready, this,
            &GovDataCKANPanel::on_result);
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this]() { setStyleSheet(build_ckan_style()); });
}

// ── Build UI ──────────────────────────────────────────────────────────────────

void GovDataCKANPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_portal_bar());
    root->addWidget(build_toolbar());

    // Breadcrumb bar
    breadcrumb_ = new QWidget(this);
    breadcrumb_->setObjectName("ckanBreadcrumb");
    breadcrumb_->setFixedHeight(24);
    auto* bcl = new QHBoxLayout(breadcrumb_);
    bcl->setContentsMargins(14, 0, 14, 0);
    bcl->setSpacing(4);
    breadcrumb_label_ = new QLabel("Publishers");
    breadcrumb_label_->setObjectName("ckanBreadText");
    bcl->addWidget(breadcrumb_label_);
    bcl->addStretch(1);
    row_count_label_ = new QLabel;
    row_count_label_->setObjectName("ckanBreadCount");
    bcl->addWidget(row_count_label_);
    root->addWidget(breadcrumb_);

    // Content stack
    content_stack_ = new QStackedWidget;

    // Page 0 — Publishers table (1 col, full-stretch, colored)
    publishers_table_ = new QTableWidget;
    publishers_table_->setColumnCount(1);
    publishers_table_->setHorizontalHeaderLabels({"NAME"});
    publishers_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    publishers_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    publishers_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    publishers_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    publishers_table_->verticalHeader()->setVisible(false);
    publishers_table_->setAlternatingRowColors(true);
    publishers_table_->setShowGrid(true);
    connect(publishers_table_, &QTableWidget::cellDoubleClicked, this, &GovDataCKANPanel::on_publisher_clicked);
    content_stack_->addWidget(publishers_table_); // index 0

    // Page 1 — Datasets table
    datasets_table_ = new QTableWidget;
    datasets_table_->setColumnCount(4);
    datasets_table_->setHorizontalHeaderLabels({"TITLE", "FILES", "MODIFIED", "TAGS"});
    datasets_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    datasets_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    datasets_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    datasets_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    datasets_table_->setColumnWidth(1, 60);
    datasets_table_->setColumnWidth(2, 100);
    datasets_table_->setColumnWidth(3, 200);
    datasets_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    datasets_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    datasets_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    datasets_table_->verticalHeader()->setVisible(false);
    datasets_table_->setAlternatingRowColors(true);
    datasets_table_->setShowGrid(true);
    connect(datasets_table_, &QTableWidget::cellDoubleClicked, this, &GovDataCKANPanel::on_dataset_clicked);
    content_stack_->addWidget(datasets_table_); // index 1

    // Page 2 — Resources table
    resources_table_ = new QTableWidget;
    resources_table_->setColumnCount(5);
    resources_table_->setHorizontalHeaderLabels({"NAME", "FORMAT", "SIZE", "MODIFIED", "OPEN"});
    resources_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    resources_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    resources_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    resources_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    resources_table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    resources_table_->setColumnWidth(1, 70);
    resources_table_->setColumnWidth(2, 80);
    resources_table_->setColumnWidth(3, 100);
    resources_table_->setColumnWidth(4, 60);
    resources_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    resources_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    resources_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    resources_table_->verticalHeader()->setVisible(false);
    resources_table_->setAlternatingRowColors(true);
    resources_table_->setShowGrid(true);

    connect(
        resources_table_, &QTableWidget::cellClicked, this,
        [this](int row, int col) {
            if (col != 4)
                return;
            auto* item = resources_table_->item(row, 4);
            if (!item)
                return;
            QString url = item->data(Qt::UserRole).toString();
            if (!url.isEmpty())
                QDesktopServices::openUrl(QUrl(url));
        },
        Qt::UniqueConnection);

    content_stack_->addWidget(resources_table_); // index 2

    // Page 3 — Status / loading / error
    auto* status_page = new QWidget(this);
    status_page->setObjectName("ckanStatusPage");
    auto* svl = new QVBoxLayout(status_page);
    svl->setAlignment(Qt::AlignCenter);
    status_label_ = new QLabel;
    status_label_->setObjectName("ckanStatusMsg");
    status_label_->setAlignment(Qt::AlignCenter);
    status_label_->setWordWrap(true);
    svl->addWidget(status_label_);
    content_stack_->addWidget(status_page); // index 3

    root->addWidget(content_stack_, 1);
}

QWidget* GovDataCKANPanel::build_portal_bar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("ckanPortalBar");
    bar->setFixedHeight(32);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(14, 0, 14, 0);
    hl->setSpacing(8);

    auto* lbl = new QLabel("CKAN PORTAL:");
    lbl->setObjectName("ckanPortalLabel");
    hl->addWidget(lbl);

    portal_combo_ = new QComboBox;
    portal_combo_->setObjectName("ckanPortalCombo");
    portal_combo_->addItems({"data.gov.uk", "open.canada.ca", "data.gov.au", "data.gov.hk", "opendata.swiss",
                             "data.gouv.fr", "data.gov", "openafrica.net"});
    // Display-only — the underlying script always queries data.gov.uk
    portal_combo_->setToolTip("This panel uses datagovuk_api.py which queries data.gov.uk.\n"
                              "The selector shows all CKAN portals covered by the universal provider.");
    portal_combo_->setEnabled(true); // allow selection for visual context
    hl->addWidget(portal_combo_);

    hl->addStretch(1);

    return bar;
}

QWidget* GovDataCKANPanel::build_toolbar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("ckanPanelToolbar");
    bar->setFixedHeight(40);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(10, 0, 10, 0);
    hl->setSpacing(5);

    back_btn_ = new QPushButton("← BACK");
    back_btn_->setObjectName("ckanBackBtn");
    back_btn_->setVisible(false);
    back_btn_->setCursor(Qt::PointingHandCursor);
    connect(back_btn_, &QPushButton::clicked, this, &GovDataCKANPanel::on_back);
    hl->addWidget(back_btn_);

    hl->addSpacing(4);

    publishers_btn_ = new QPushButton("PUBLISHERS");
    publishers_btn_->setObjectName("ckanTabBtn");
    publishers_btn_->setCheckable(true);
    publishers_btn_->setChecked(true);
    publishers_btn_->setCursor(Qt::PointingHandCursor);
    connect(publishers_btn_, &QPushButton::clicked, this, [this]() { on_tab_changed(Publishers); });
    hl->addWidget(publishers_btn_);

    datasets_btn_ = new QPushButton("DATASETS");
    datasets_btn_->setObjectName("ckanTabBtn");
    datasets_btn_->setCheckable(true);
    datasets_btn_->setCursor(Qt::PointingHandCursor);
    connect(datasets_btn_, &QPushButton::clicked, this, [this]() { on_tab_changed(Datasets); });
    hl->addWidget(datasets_btn_);

    search_btn_ = new QPushButton("SEARCH");
    search_btn_->setObjectName("ckanTabBtn");
    search_btn_->setCheckable(true);
    search_btn_->setCursor(Qt::PointingHandCursor);
    connect(search_btn_, &QPushButton::clicked, this, [this]() { on_tab_changed(Search); });
    hl->addWidget(search_btn_);

    hl->addSpacing(10);

    search_input_ = new QLineEdit;
    search_input_->setObjectName("ckanSearch");
    search_input_->setPlaceholderText("Search datasets…");
    search_input_->setFixedWidth(210);
    search_input_->setFixedHeight(26);
    connect(search_input_, &QLineEdit::returnPressed, this, &GovDataCKANPanel::on_search);
    hl->addWidget(search_input_);

    fetch_btn_ = new QPushButton("FETCH");
    fetch_btn_->setObjectName("ckanFetchBtn");
    fetch_btn_->setCursor(Qt::PointingHandCursor);
    connect(fetch_btn_, &QPushButton::clicked, this, &GovDataCKANPanel::on_search);
    hl->addWidget(fetch_btn_);

    hl->addStretch(1);

    export_btn_ = new QPushButton("CSV");
    export_btn_->setObjectName("ckanCsvBtn");
    export_btn_->setCursor(Qt::PointingHandCursor);
    connect(export_btn_, &QPushButton::clicked, this, &GovDataCKANPanel::on_export_csv);
    hl->addWidget(export_btn_);

    return bar;
}

// ── Public slot ───────────────────────────────────────────────────────────────

void GovDataCKANPanel::load_initial_data() {
    show_loading("Loading publishers from data.gov.uk…");
    services::GovDataService::instance().execute(kGovDataCKANScript, "publishers", {}, "ckan_publishers");
}

// ── Toolbar actions ───────────────────────────────────────────────────────────

void GovDataCKANPanel::on_tab_changed(int tab_index) {
    auto view = static_cast<View>(tab_index);
    publishers_btn_->setChecked(view == Publishers);
    datasets_btn_->setChecked(view == Datasets);
    search_btn_->setChecked(view == Search);

    current_view_ = view;

    if (view == Publishers) {
        if (current_publishers_.isEmpty())
            load_initial_data();
        else {
            content_stack_->setCurrentIndex(Publishers);
            row_count_label_->setText(QString::number(current_publishers_.size()) + " publishers");
        }
        update_breadcrumb();
    } else if (view == Datasets) {
        if (current_datasets_.isEmpty())
            show_status("Double-click a publisher to load their datasets.");
        else {
            content_stack_->setCurrentIndex(Datasets);
            row_count_label_->setText(QString::number(current_datasets_.size()) + " datasets");
        }
        update_breadcrumb();
    } else if (view == Search) {
        update_breadcrumb();
        search_input_->setFocus();
    }

    update_toolbar_state();
}

void GovDataCKANPanel::on_search() {
    QString query = search_input_->text().trimmed();
    if (query.isEmpty())
        return;

    show_loading("Searching for "
                 " + query + "
                 "…");
    services::GovDataService::instance().execute(kGovDataCKANScript, "search", {query, "50"}, "ckan_search");
}

void GovDataCKANPanel::on_publisher_clicked(int row) {
    auto* item = publishers_table_->item(row, 0);
    if (!item)
        return;
    selected_publisher_ = item->data(Qt::UserRole).toString();
    selected_publisher_name_ = item->text();

    show_loading("Loading datasets for "
                 " + selected_publisher_name_ + "
                 "…");
    services::GovDataService::instance().execute(kGovDataCKANScript, "datasets", {selected_publisher_, "100"},
                                                 "ckan_datasets");
}

void GovDataCKANPanel::on_dataset_clicked(int row) {
    auto* item = datasets_table_->item(row, 0);
    if (!item)
        return;
    selected_dataset_ = item->data(Qt::UserRole).toString();
    if (selected_dataset_.isEmpty())
        return;

    show_loading("Loading resources…");
    services::GovDataService::instance().execute(kGovDataCKANScript, "resources", {selected_dataset_},
                                                 "ckan_resources");
}

void GovDataCKANPanel::on_back() {
    if (current_view_ == Resources) {
        content_stack_->setCurrentIndex(Datasets);
        current_view_ = Datasets;
        row_count_label_->setText(QString::number(current_datasets_.size()) + " datasets");
    } else if (current_view_ == Datasets) {
        content_stack_->setCurrentIndex(Publishers);
        current_view_ = Publishers;
        publishers_btn_->setChecked(true);
        datasets_btn_->setChecked(false);
        selected_publisher_.clear();
        selected_publisher_name_.clear();
        row_count_label_->setText(QString::number(current_publishers_.size()) + " publishers");
    }
    update_toolbar_state();
    update_breadcrumb();
}

void GovDataCKANPanel::update_toolbar_state() {
    back_btn_->setVisible(current_view_ == Datasets || current_view_ == Resources);
}

void GovDataCKANPanel::update_breadcrumb() {
    QString text;
    switch (current_view_) {
        case Publishers:
            text = "Publishers";
            break;
        case Datasets:
            text = selected_publisher_name_.isEmpty() ? "Datasets"
                                                      : "Publishers  ›  " + selected_publisher_name_ + "  ›  Datasets";
            break;
        case Resources:
            text = "Publishers  ›  Datasets  ›  Resources";
            break;
        case Search:
            text = "Search Results";
            break;
    }
    breadcrumb_label_->setText(text);
}

// ── Result handler ────────────────────────────────────────────────────────────

void GovDataCKANPanel::on_result(const QString& request_id, const services::GovDataResult& result) {
    if (!request_id.startsWith("ckan_"))
        return;

    if (!result.success) {
        show_error(result.error);
        return;
    }

    // Unwrap data — handle flat array or wrapped object
    QJsonArray data;
    QJsonValue raw = result.data["data"];
    if (raw.isArray()) {
        data = raw.toArray();
    } else if (raw.isObject()) {
        QJsonObject obj = raw.toObject();
        if (obj.contains("datasets"))
            data = obj["datasets"].toArray();
        else if (obj.contains("result"))
            data = obj["result"].toArray();
    }

    if (request_id == "ckan_publishers") {
        current_publishers_ = data;
        populate_publishers(data);
        current_view_ = Publishers;
        publishers_btn_->setChecked(true);
        datasets_btn_->setChecked(false);
        search_btn_->setChecked(false);
        content_stack_->setCurrentIndex(Publishers);
        row_count_label_->setText(QString::number(data.size()) + " publishers");
        update_breadcrumb();

    } else if (request_id == "ckan_datasets") {
        int total = result.data["metadata"].toObject()["total_count"].toInt(0);
        if (total == 0)
            total = data.size();
        current_datasets_ = data;
        populate_datasets(data, total);
        current_view_ = Datasets;
        publishers_btn_->setChecked(false);
        datasets_btn_->setChecked(true);
        search_btn_->setChecked(false);
        content_stack_->setCurrentIndex(Datasets);
        row_count_label_->setText(QString("Showing %1 of %2").arg(data.size()).arg(total));
        update_breadcrumb();

    } else if (request_id == "ckan_search") {
        int total = result.data["metadata"].toObject()["total_count"].toInt(0);
        if (total == 0)
            total = data.size();
        current_datasets_ = data;
        populate_datasets(data, total);
        current_view_ = Search;
        publishers_btn_->setChecked(false);
        datasets_btn_->setChecked(false);
        search_btn_->setChecked(true);
        content_stack_->setCurrentIndex(Datasets);
        row_count_label_->setText(QString("Showing %1 of %2").arg(data.size()).arg(total));
        update_breadcrumb();

    } else if (request_id == "ckan_resources") {
        current_resources_ = data;
        populate_resources(data);
        current_view_ = Resources;
        content_stack_->setCurrentIndex(Resources);
        row_count_label_->setText(QString::number(data.size()) + " files");
        update_breadcrumb();
    }

    update_toolbar_state();
}

// ── Populate tables ───────────────────────────────────────────────────────────

void GovDataCKANPanel::populate_publishers(const QJsonArray& data) {
    publishers_table_->setRowCount(0);
    publishers_table_->setRowCount(data.size());

    for (int i = 0; i < data.size(); ++i) {
        const auto obj = data[i].toObject();

        QString name = obj["display_name"].toString();
        if (name.isEmpty())
            name = obj["title"].toString();
        if (name.isEmpty())
            name = obj["name"].toString();
        if (name.isEmpty())
            name = obj["id"].toString();

        auto* name_item = new QTableWidgetItem(name);
        name_item->setForeground(QColor(kGovDataCKANColor));
        name_item->setData(Qt::UserRole,
                           obj["id"].toString().isEmpty() ? obj["name"].toString() : obj["id"].toString());
        publishers_table_->setItem(i, 0, name_item);
    }

    LOG_INFO("GovCKAN", QString("Populated %1 publishers").arg(data.size()));
}

void GovDataCKANPanel::populate_datasets(const QJsonArray& data, int total_count) {
    datasets_table_->setRowCount(0);
    datasets_table_->setRowCount(data.size());

    for (int i = 0; i < data.size(); ++i) {
        const auto obj = data[i].toObject();

        QString title = obj["title"].toString();
        if (title.isEmpty())
            title = obj["name"].toString();

        auto* title_item = new QTableWidgetItem(title);
        title_item->setData(Qt::UserRole,
                            obj["id"].toString().isEmpty() ? obj["name"].toString() : obj["id"].toString());
        datasets_table_->setItem(i, 0, title_item);

        int num_res = obj["num_resources"].toInt(0);
        auto* res_item = new QTableWidgetItem(QString::number(num_res));
        res_item->setTextAlignment(Qt::AlignCenter);
        res_item->setForeground(QColor(kGovDataCKANColor));
        datasets_table_->setItem(i, 1, res_item);

        QString modified = obj["metadata_modified"].toString();
        if (modified.length() > 10)
            modified = modified.left(10);
        datasets_table_->setItem(i, 2, new QTableWidgetItem(modified));

        QStringList tags;
        for (const auto& t : obj["tags"].toArray()) {
            if (t.isString())
                tags << t.toString();
            else if (t.isObject())
                tags << t.toObject()["name"].toString();
        }
        QString tags_str = tags.mid(0, 3).join(", ");
        if (tags.size() > 3)
            tags_str += QString(" +%1").arg(tags.size() - 3);
        auto* tag_item = new QTableWidgetItem(tags_str);
        tag_item->setForeground(QColor(ui::colors::TEXT_SECONDARY()));
        datasets_table_->setItem(i, 3, tag_item);
    }

    LOG_INFO("GovCKAN", QString("Populated %1 datasets (total: %2)").arg(data.size()).arg(total_count));
}

void GovDataCKANPanel::populate_resources(const QJsonArray& data) {
    resources_table_->setRowCount(0);
    resources_table_->setRowCount(data.size());

    for (int i = 0; i < data.size(); ++i) {
        const auto obj = data[i].toObject();

        QString name = obj["name"].toString();
        if (name.isEmpty())
            name = obj["id"].toString();
        resources_table_->setItem(i, 0, new QTableWidgetItem(name));

        QString format = obj["format"].toString().toUpper();
        auto* fmt_item = new QTableWidgetItem(format.isEmpty() ? "—" : format);
        fmt_item->setForeground(QColor(kGovDataCKANColor));
        fmt_item->setTextAlignment(Qt::AlignCenter);
        resources_table_->setItem(i, 1, fmt_item);

        double size_bytes = obj["size"].toDouble(0);
        QString size_str = "—";
        if (size_bytes > 0) {
            if (size_bytes < 1024)
                size_str = QString::number(size_bytes, 'f', 0) + " B";
            else if (size_bytes < 1024 * 1024)
                size_str = QString::number(size_bytes / 1024.0, 'f', 1) + " KB";
            else
                size_str = QString::number(size_bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
        }
        auto* sz = new QTableWidgetItem(size_str);
        sz->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        resources_table_->setItem(i, 2, sz);

        QString modified = obj["last_modified"].toString();
        if (modified.length() > 10)
            modified = modified.left(10);
        resources_table_->setItem(i, 3, new QTableWidgetItem(modified));

        QString url = obj["url"].toString();
        auto* url_item = new QTableWidgetItem(url.isEmpty() ? "—" : "↗ OPEN");
        url_item->setData(Qt::UserRole, url);
        if (!url.isEmpty())
            url_item->setForeground(QColor(kGovDataCKANColor));
        url_item->setTextAlignment(Qt::AlignCenter);
        resources_table_->setItem(i, 4, url_item);
    }

    LOG_INFO("GovCKAN", QString("Populated %1 resources").arg(data.size()));
}

// ── Status helpers ────────────────────────────────────────────────────────────

void GovDataCKANPanel::show_loading(const QString& message) {
    status_label_->setStyleSheet(QString("color:%1; font-size:13px; background:transparent;").arg(kGovDataCKANColor));
    status_label_->setText(message);
    content_stack_->setCurrentIndex(3);
}

void GovDataCKANPanel::show_error(const QString& message) {
    status_label_->setStyleSheet(
        QString("color:%1; font-size:12px; background:transparent;").arg(ui::colors::NEGATIVE()));
    status_label_->setText("Error: " + message);
    content_stack_->setCurrentIndex(3);
    LOG_ERROR("GovCKAN", message);
}

void GovDataCKANPanel::show_status(const QString& message, bool is_error) {
    status_label_->setStyleSheet(QString("color:%1; font-size:12px; background:transparent;")
                                     .arg(is_error ? ui::colors::NEGATIVE() : ui::colors::TEXT_SECONDARY()));
    status_label_->setText(message);
    content_stack_->setCurrentIndex(3);
}

// ── CSV export ────────────────────────────────────────────────────────────────

void GovDataCKANPanel::on_export_csv() {
    QTableWidget* table = nullptr;
    QString def_name;

    if (current_view_ == Publishers && publishers_table_->rowCount() > 0) {
        table = publishers_table_;
        def_name = "ckan_publishers.csv";
    } else if ((current_view_ == Datasets || current_view_ == Search) && datasets_table_->rowCount() > 0) {
        table = datasets_table_;
        def_name = "ckan_datasets.csv";
    } else if (current_view_ == Resources && resources_table_->rowCount() > 0) {
        table = resources_table_;
        def_name = "ckan_resources.csv";
    }
    if (!table)
        return;

    QString path = QFileDialog::getSaveFileName(this, "Export CSV", def_name, "CSV Files (*.csv)");
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;
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

    LOG_INFO("GovCKAN", "Exported CSV: " + path);
}

} // namespace fincept::screens
