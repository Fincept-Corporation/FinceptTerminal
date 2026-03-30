// src/screens/gov_data/GovDataCanadaPanel.cpp
#include "screens/gov_data/GovDataCanadaPanel.h"

#include "core/logging/Logger.h"
#include "services/gov_data/GovDataService.h"
#include "ui/theme/Theme.h"

#include <QDesktopServices>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QScrollArea>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>

namespace fincept::screens {
namespace {
static constexpr const char* kGovDataCanadaScript = "canada_gov_api.py";
static constexpr const char* kGovDataCanadaColor  = "#EF4444";
} // namespace

using namespace fincept::ui;

// ── Panel stylesheet ─────────────────────────────────────────────────────────

static const QString kCanadaStyle = QString(
    "#govPanelToolbar { background:#111111; border-bottom:1px solid #1a1a1a; }"

    "#govTabBtn { background:transparent; color:#808080; border:1px solid #1a1a1a;"
    "  font-size:10px; font-weight:700; padding:4px 12px; letter-spacing:0.5px; }"
    "#govTabBtn:hover { color:#e5e5e5; background:#161616; }"
    "#govTabBtn:checked { background:rgba(239,68,68,0.12); color:#EF4444;"
    "  border:1px solid #EF4444; }"

    "#govBackBtn { background:transparent; color:#808080; border:1px solid #1a1a1a;"
    "  font-size:10px; font-weight:700; padding:4px 10px; }"
    "#govBackBtn:hover { color:#e5e5e5; background:#161616; }"

    "#govFetchBtn { background:#EF4444; color:#080808; border:none;"
    "  font-size:10px; font-weight:700; padding:4px 14px; }"
    "#govFetchBtn:hover { background:#f87171; }"
    "#govFetchBtn:disabled { background:#1a1a1a; color:#404040; }"

    "#govCsvBtn { background:transparent; color:#808080; border:1px solid #1a1a1a;"
    "  font-size:10px; font-weight:700; padding:4px 10px; }"
    "#govCsvBtn:hover { color:#e5e5e5; background:#161616; }"

    "#govSearch { background:#080808; color:#e5e5e5; border:none;"
    "  border-bottom:1px solid #1a1a1a; padding:4px 10px; font-size:11px; }"
    "#govSearch:focus { border-bottom:1px solid #EF4444; }"

    "QTableWidget { background:#080808; color:#e5e5e5; border:none;"
    "  gridline-color:#1a1a1a; font-size:11px; alternate-background-color:#0a0a0a; }"
    "QTableWidget::item { padding:5px 8px; border-bottom:1px solid #1a1a1a; }"
    "QTableWidget::item:selected { background:rgba(239,68,68,0.10); color:#EF4444; }"
    "QHeaderView::section { background:#111111; color:#808080; border:none;"
    "  border-bottom:2px solid #1a1a1a; border-right:1px solid #1a1a1a;"
    "  padding:5px 8px; font-size:10px; font-weight:700; letter-spacing:0.5px; }"

    "#govStatusPage { background:#080808; }"
    "#govStatusMsg  { color:#808080; font-size:13px; background:transparent; }"
    "#govStatusErr  { color:#dc2626; font-size:12px; background:transparent; }"

    "#govBreadcrumb       { background:#0a0a0a; border-bottom:1px solid #1a1a1a; }"
    "#govBreadcrumbText   { color:#808080; font-size:9px; background:transparent; }"
    "#govBreadcrumbActive { color:#EF4444; font-size:9px; font-weight:700; background:transparent; }"

    "QScrollBar:vertical { background:#080808; width:5px; }"
    "QScrollBar::handle:vertical { background:#1a1a1a; min-height:20px; }"
    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"
);

// ── Constructor ──────────────────────────────────────────────────────────────

GovDataCanadaPanel::GovDataCanadaPanel(QWidget* parent)
    : QWidget(parent) {
    setStyleSheet(kCanadaStyle);
    build_ui();
    connect(&services::GovDataService::instance(),
            &services::GovDataService::result_ready,
            this,
            &GovDataCanadaPanel::on_result);
}

// ── Build UI ─────────────────────────────────────────────────────────────────

void GovDataCanadaPanel::build_ui() {
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

    breadcrumb_label_ = new QLabel("All Publishers");
    breadcrumb_label_->setObjectName("govBreadcrumbText");
    bcl->addWidget(breadcrumb_label_);
    bcl->addStretch(1);

    row_count_label_ = new QLabel;
    row_count_label_->setObjectName("govBreadcrumbText");
    bcl->addWidget(row_count_label_);

    root->addWidget(breadcrumb_);

    // Content stack
    content_stack_ = new QStackedWidget;

    // Page 0 — Publishers table (NAME | DATASETS)
    publishers_table_ = new QTableWidget;
    publishers_table_->setColumnCount(2);
    publishers_table_->setHorizontalHeaderLabels({"PUBLISHER", "DATASETS"});
    publishers_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    publishers_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    publishers_table_->setColumnWidth(1, 80);
    publishers_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    publishers_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    publishers_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    publishers_table_->verticalHeader()->setVisible(false);
    publishers_table_->setAlternatingRowColors(true);
    publishers_table_->setShowGrid(true);
    connect(publishers_table_, &QTableWidget::cellDoubleClicked,
            this, &GovDataCanadaPanel::on_publisher_clicked);
    content_stack_->addWidget(publishers_table_);

    // Page 1 — Datasets table (TITLE | FILES | MODIFIED | PUBLISHER)
    datasets_table_ = new QTableWidget;
    datasets_table_->setColumnCount(4);
    datasets_table_->setHorizontalHeaderLabels({"TITLE", "FILES", "MODIFIED", "PUBLISHER"});
    datasets_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    datasets_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    datasets_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    datasets_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    datasets_table_->setColumnWidth(1, 55);
    datasets_table_->setColumnWidth(2, 100);
    datasets_table_->setColumnWidth(3, 180);
    datasets_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    datasets_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    datasets_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    datasets_table_->verticalHeader()->setVisible(false);
    datasets_table_->setAlternatingRowColors(true);
    datasets_table_->setShowGrid(true);
    connect(datasets_table_, &QTableWidget::cellDoubleClicked,
            this, &GovDataCanadaPanel::on_dataset_clicked);
    content_stack_->addWidget(datasets_table_);

    // Page 2 — Resources table (NAME | FORMAT | SIZE | MODIFIED | OPEN)
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
    content_stack_->addWidget(resources_table_);

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
    content_stack_->addWidget(status_page);

    root->addWidget(content_stack_, 1);
}

QWidget* GovDataCanadaPanel::build_toolbar() {
    auto* bar = new QWidget;
    bar->setObjectName("govPanelToolbar");
    bar->setFixedHeight(40);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(10, 0, 10, 0);
    hl->setSpacing(5);

    back_btn_ = new QPushButton("← BACK");
    back_btn_->setObjectName("govBackBtn");
    back_btn_->setVisible(false);
    back_btn_->setCursor(Qt::PointingHandCursor);
    connect(back_btn_, &QPushButton::clicked, this, &GovDataCanadaPanel::on_back);
    hl->addWidget(back_btn_);

    hl->addSpacing(4);

    publishers_btn_ = new QPushButton("PUBLISHERS");
    publishers_btn_->setObjectName("govTabBtn");
    publishers_btn_->setCheckable(true);
    publishers_btn_->setChecked(true);
    publishers_btn_->setCursor(Qt::PointingHandCursor);
    connect(publishers_btn_, &QPushButton::clicked,
            this, [this]() { on_view_changed(Publishers); });
    hl->addWidget(publishers_btn_);

    datasets_btn_ = new QPushButton("DATASETS");
    datasets_btn_->setObjectName("govTabBtn");
    datasets_btn_->setCheckable(true);
    datasets_btn_->setCursor(Qt::PointingHandCursor);
    connect(datasets_btn_, &QPushButton::clicked,
            this, [this]() { on_view_changed(Datasets); });
    hl->addWidget(datasets_btn_);

    search_btn_ = new QPushButton("SEARCH");
    search_btn_->setObjectName("govTabBtn");
    search_btn_->setCheckable(true);
    search_btn_->setCursor(Qt::PointingHandCursor);
    connect(search_btn_, &QPushButton::clicked,
            this, [this]() { on_view_changed(Search); });
    hl->addWidget(search_btn_);

    hl->addSpacing(10);

    search_input_ = new QLineEdit;
    search_input_->setObjectName("govSearch");
    search_input_->setPlaceholderText("Search datasets…");
    search_input_->setFixedWidth(210);
    search_input_->setFixedHeight(26);
    connect(search_input_, &QLineEdit::returnPressed,
            this, &GovDataCanadaPanel::on_search);
    hl->addWidget(search_input_);

    fetch_btn_ = new QPushButton("FETCH");
    fetch_btn_->setObjectName("govFetchBtn");
    fetch_btn_->setCursor(Qt::PointingHandCursor);
    connect(fetch_btn_, &QPushButton::clicked, this, &GovDataCanadaPanel::on_search);
    hl->addWidget(fetch_btn_);

    hl->addStretch(1);

    export_btn_ = new QPushButton("CSV");
    export_btn_->setObjectName("govCsvBtn");
    export_btn_->setCursor(Qt::PointingHandCursor);
    connect(export_btn_, &QPushButton::clicked, this, &GovDataCanadaPanel::on_export_csv);
    hl->addWidget(export_btn_);

    return bar;
}

// ── Data loading ─────────────────────────────────────────────────────────────

void GovDataCanadaPanel::load_initial_data() {
    show_loading("Loading publishers…");
    services::GovDataService::instance().execute(
        kGovDataCanadaScript, "publishers", {}, "gov_orgs_canada_gov_api.py");
}

void GovDataCanadaPanel::on_result(const QString&                  request_id,
                                   const services::GovDataResult& result) {
    // Only handle requests belonging to this script
    if (!request_id.contains("canada_gov_api.py")) return;

    if (!result.success) {
        show_error(result.error);
        return;
    }

    // Unwrap data array — CKAN responses always use {"data":[...], "metadata":{...}}
    QJsonArray data;
    const QJsonValue raw = result.data["data"];
    if (raw.isArray()) {
        data = raw.toArray();
    } else if (raw.isObject()) {
        const QJsonObject obj = raw.toObject();
        if (obj.contains("datasets"))
            data = obj["datasets"].toArray();
        else if (obj.contains("result"))
            data = obj["result"].toArray();
    }

    if (request_id.startsWith("gov_orgs_")) {
        current_publishers_ = data;
        populate_publishers(data);
        content_stack_->setCurrentIndex(Publishers);
        current_view_ = Publishers;
        update_breadcrumb();
        LOG_INFO("GovDataCanada", QString("Loaded %1 publishers").arg(data.size()));
    } else if (request_id.startsWith("gov_datasets_") ||
               request_id.startsWith("gov_search_")) {
        int total = result.data["metadata"].toObject()["total_count"].toInt(0);
        if (total == 0) total = data.size();
        current_datasets_ = data;
        populate_datasets(data, total);
        content_stack_->setCurrentIndex(Datasets);
        current_view_ = Datasets;
        update_breadcrumb();
        LOG_INFO("GovDataCanada", QString("Loaded %1 datasets (total %2)").arg(data.size()).arg(total));
    } else if (request_id.startsWith("gov_resources_")) {
        current_resources_ = data;
        populate_resources(data);
        content_stack_->setCurrentIndex(Resources);
        current_view_ = Resources;
        update_breadcrumb();
        LOG_INFO("GovDataCanada", QString("Loaded %1 resources").arg(data.size()));
    }

    update_toolbar_state();
}

// ── Populate tables ──────────────────────────────────────────────────────────

void GovDataCanadaPanel::populate_publishers(const QJsonArray& data) {
    publishers_table_->setRowCount(0);
    publishers_table_->setRowCount(data.size());

    for (int i = 0; i < data.size(); ++i) {
        const auto obj = data[i].toObject();

        // Resolve display name — prefer display_name, fall back to name, then id
        QString name = obj["display_name"].toString();
        if (name.isEmpty()) name = obj["name"].toString();
        if (name.isEmpty()) name = obj["id"].toString();

        // Store id for dataset queries
        const QString id = obj["id"].toString().isEmpty()
                               ? obj["name"].toString()
                               : obj["id"].toString();

        auto* name_item = new QTableWidgetItem(name);
        name_item->setData(Qt::UserRole, id);
        name_item->setForeground(QColor(kGovDataCanadaColor));
        publishers_table_->setItem(i, 0, name_item);

        // dataset_count may not be present in Canada API — show "—" when absent
        int count = obj["dataset_count"].toInt(-1);
        auto* cnt = new QTableWidgetItem(count >= 0 ? QString::number(count) : "—");
        cnt->setTextAlignment(Qt::AlignCenter);
        publishers_table_->setItem(i, 1, cnt);
    }

    row_count_label_->setText(QString::number(data.size()) + " publishers");
}

void GovDataCanadaPanel::populate_datasets(const QJsonArray& data, int total_count) {
    datasets_table_->setRowCount(0);
    datasets_table_->setRowCount(data.size());

    for (int i = 0; i < data.size(); ++i) {
        const auto obj = data[i].toObject();

        // Title — fall back to name
        QString title = obj["title"].toString();
        if (title.isEmpty()) title = obj["name"].toString();

        const QString id = obj["id"].toString().isEmpty()
                               ? obj["name"].toString()
                               : obj["id"].toString();

        // Long notes → tooltip truncated to 100 chars
        QString notes = obj["notes"].toString().trimmed();
        QString tooltip = notes.isEmpty() ? title : (notes.length() > 100
                              ? notes.left(100) + "…"
                              : notes);

        auto* title_item = new QTableWidgetItem(title);
        title_item->setData(Qt::UserRole, id);
        title_item->setToolTip(tooltip);
        datasets_table_->setItem(i, 0, title_item);

        int num_res = obj["num_resources"].toInt(0);
        auto* res_item = new QTableWidgetItem(QString::number(num_res));
        res_item->setTextAlignment(Qt::AlignCenter);
        res_item->setForeground(QColor(kGovDataCanadaColor));
        datasets_table_->setItem(i, 1, res_item);

        QString modified = obj["metadata_modified"].toString();
        if (modified.length() > 10) modified = modified.left(10);
        datasets_table_->setItem(i, 2, new QTableWidgetItem(modified));

        // Publisher name from publisher_id or nested publisher object
        QString publisher = obj["publisher_id"].toString();
        if (publisher.isEmpty()) {
            const auto pub_obj = obj["publisher"].toObject();
            publisher = pub_obj["display_name"].toString();
            if (publisher.isEmpty()) publisher = pub_obj["name"].toString();
        }
        auto* pub_item = new QTableWidgetItem(publisher);
        pub_item->setForeground(QColor("#808080"));
        datasets_table_->setItem(i, 3, pub_item);
    }

    row_count_label_->setText(
        QString("Showing %1 of %2").arg(data.size()).arg(total_count));
}

void GovDataCanadaPanel::populate_resources(const QJsonArray& data) {
    resources_table_->setRowCount(0);
    resources_table_->setRowCount(data.size());

    for (int i = 0; i < data.size(); ++i) {
        const auto obj = data[i].toObject();

        QString name = obj["name"].toString();
        if (name.isEmpty()) name = obj["id"].toString();

        auto* name_item = new QTableWidgetItem(name);
        // description as tooltip
        const QString desc = obj["description"].toString().trimmed();
        if (!desc.isEmpty()) name_item->setToolTip(desc);
        resources_table_->setItem(i, 0, name_item);

        QString format = obj["format"].toString().toUpper();
        auto* fmt_item = new QTableWidgetItem(format.isEmpty() ? "—" : format);
        fmt_item->setForeground(QColor(kGovDataCanadaColor));
        fmt_item->setTextAlignment(Qt::AlignCenter);
        resources_table_->setItem(i, 1, fmt_item);

        // size is often null — show "—" when null/zero
        QString size_str = "—";
        if (!obj["size"].isNull() && obj["size"].toDouble(0) > 0) {
            const double size_bytes = obj["size"].toDouble(0);
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
        if (modified.length() > 10) modified = modified.left(10);
        if (modified.isEmpty()) modified = obj["created"].toString().left(10);
        resources_table_->setItem(i, 3, new QTableWidgetItem(modified));

        const QString url = obj["url"].toString();
        auto* url_item = new QTableWidgetItem(url.isEmpty() ? "—" : "↗ OPEN");
        url_item->setData(Qt::UserRole, url);
        if (!url.isEmpty()) url_item->setForeground(QColor(kGovDataCanadaColor));
        url_item->setTextAlignment(Qt::AlignCenter);
        resources_table_->setItem(i, 4, url_item);
    }

    // Connect open-URL click — UniqueConnection prevents double-connecting on reload
    connect(resources_table_, &QTableWidget::cellClicked, this,
        [this](int row, int col) {
            if (col != 4) return;
            auto* item = resources_table_->item(row, 4);
            if (!item) return;
            const QString url = item->data(Qt::UserRole).toString();
            if (!url.isEmpty()) QDesktopServices::openUrl(QUrl(url));
        },
        Qt::UniqueConnection);

    row_count_label_->setText(QString::number(data.size()) + " files");
}

// ── View navigation ──────────────────────────────────────────────────────────

void GovDataCanadaPanel::on_view_changed(int index) {
    const auto view = static_cast<View>(index);
    publishers_btn_->setChecked(view == Publishers);
    datasets_btn_->setChecked(view == Datasets);
    search_btn_->setChecked(view == Search);

    if (view == Publishers) {
        if (current_publishers_.isEmpty())
            load_initial_data();
        else
            content_stack_->setCurrentIndex(Publishers);
        current_view_ = Publishers;
    } else if (view == Datasets) {
        if (!current_datasets_.isEmpty()) {
            content_stack_->setCurrentIndex(Datasets);
            current_view_ = Datasets;
        }
        // If no datasets loaded yet, stay on current view — user needs to pick a publisher
    } else if (view == Search) {
        current_view_ = Search;
        search_input_->setFocus();
    }

    update_toolbar_state();
    update_breadcrumb();
}

void GovDataCanadaPanel::on_publisher_clicked(int row) {
    auto* item = publishers_table_->item(row, 0);
    if (!item) return;
    selected_publisher_      = item->data(Qt::UserRole).toString();
    selected_publisher_name_ = item->text();
    show_loading("Loading datasets for "" + selected_publisher_name_ + ""…");
    services::GovDataService::instance().execute(
        kGovDataCanadaScript, "datasets", {selected_publisher_, "100"},
        "gov_datasets_canada_gov_api.py");
}

void GovDataCanadaPanel::on_dataset_clicked(int row) {
    auto* item = datasets_table_->item(row, 0);
    if (!item) return;
    selected_dataset_ = item->data(Qt::UserRole).toString();
    show_loading("Loading resources…");
    services::GovDataService::instance().execute(
        kGovDataCanadaScript, "resources", {selected_dataset_},
        "gov_resources_canada_gov_api.py");
}

void GovDataCanadaPanel::on_search() {
    const QString query = search_input_->text().trimmed();
    if (query.isEmpty()) return;
    show_loading("Searching for "" + query + ""…");
    services::GovDataService::instance().execute(
        kGovDataCanadaScript, "search", {query, "50"},
        "gov_search_canada_gov_api.py");
}

void GovDataCanadaPanel::on_back() {
    if (current_view_ == Resources) {
        content_stack_->setCurrentIndex(Datasets);
        current_view_ = Datasets;
    } else if (current_view_ == Datasets) {
        content_stack_->setCurrentIndex(Publishers);
        current_view_ = Publishers;
        selected_publisher_.clear();
        selected_publisher_name_.clear();
    }
    update_toolbar_state();
    update_breadcrumb();
}

void GovDataCanadaPanel::update_toolbar_state() {
    back_btn_->setVisible(current_view_ == Datasets || current_view_ == Resources);
}

void GovDataCanadaPanel::update_breadcrumb() {
    QString text;
    switch (current_view_) {
        case Publishers:
            text = "All Publishers";
            break;
        case Datasets:
            text = "All Publishers  ›  " + selected_publisher_name_ + "  ›  Datasets";
            break;
        case Resources:
            text = "All Publishers  ›  Datasets  ›  Resources";
            break;
        case Search:
            text = "Search Results";
            break;
    }
    breadcrumb_label_->setText(text);
}

// ── Status helpers ───────────────────────────────────────────────────────────

void GovDataCanadaPanel::show_loading(const QString& message) {
    status_label_->setStyleSheet(
        "color:#EF4444; font-size:13px; background:transparent;");
    status_label_->setText(message);
    content_stack_->setCurrentIndex(3);
}

void GovDataCanadaPanel::show_error(const QString& message) {
    status_label_->setStyleSheet(
        "color:#dc2626; font-size:12px; background:transparent;");
    status_label_->setText("Error: " + message);
    content_stack_->setCurrentIndex(3);
    LOG_ERROR("GovDataCanada", "Error: " + message);
}

void GovDataCanadaPanel::show_empty(const QString& message) {
    status_label_->setStyleSheet(
        "color:#808080; font-size:12px; background:transparent;");
    status_label_->setText(message);
    content_stack_->setCurrentIndex(3);
}

// ── CSV export ───────────────────────────────────────────────────────────────

void GovDataCanadaPanel::on_export_csv() {
    QTableWidget* table    = nullptr;
    QString       def_name;

    if (current_view_ == Publishers && publishers_table_->rowCount() > 0) {
        table    = publishers_table_;
        def_name = "canada_publishers.csv";
    } else if ((current_view_ == Datasets || current_view_ == Search)
               && datasets_table_->rowCount() > 0) {
        table    = datasets_table_;
        def_name = "canada_datasets.csv";
    } else if (current_view_ == Resources && resources_table_->rowCount() > 0) {
        table    = resources_table_;
        def_name = "canada_resources.csv";
    }
    if (!table) return;

    const QString path = QFileDialog::getSaveFileName(
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
}

} // namespace fincept::screens
