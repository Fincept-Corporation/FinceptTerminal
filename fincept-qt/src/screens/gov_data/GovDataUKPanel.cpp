// src/screens/gov_data/GovDataUKPanel.cpp
#include "screens/gov_data/GovDataUKPanel.h"
#include "screens/gov_data/GovDataProviderPanel.h"

#include "core/logging/Logger.h"
#include "services/gov_data/GovDataService.h"
#include "ui/theme/Theme.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>

namespace fincept::screens {
namespace {
static constexpr const char* kGovDataUKScript = "datagovuk_api.py";
static constexpr const char* kGovDataUKColor = "#10B981";
} // namespace

using namespace fincept::ui;

// ── Constructor ──────────────────────────────────────────────────────────────

GovDataUKPanel::GovDataUKPanel(QWidget* parent) : QWidget(parent) {
    // UK has a unique #govPopularBtn — pass its style as extra_qss
    auto uk_extra = [&]() -> QString {
        const auto& t = ThemeManager::instance().tokens();
        const auto cc = QColor(kGovDataUKColor);
        const QString c = kGovDataUKColor;
        const QString cr = QString::number(cc.red());
        const QString cg = QString::number(cc.green());
        const QString cb = QString::number(cc.blue());
        QString s;
        s += QString("#govPopularBtn { background:transparent; color:%1; border:1px solid %1;"
                     "  font-size:10px; font-weight:700; padding:4px 10px; }")
                 .arg(c);
        s += QString("#govPopularBtn:hover { background:rgba(%1,%2,%3,0.12); }").arg(cr, cg, cb);
        return s;
    };
    setStyleSheet(make_gov_panel_style(kGovDataUKColor, uk_extra()));
    build_ui();
    connect(&services::GovDataService::instance(), &services::GovDataService::result_ready, this,
            &GovDataUKPanel::on_result);
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this,
            [this, uk_extra]() { setStyleSheet(make_gov_panel_style(kGovDataUKColor, uk_extra())); });
}

// ── UI Construction ──────────────────────────────────────────────────────────

void GovDataUKPanel::build_ui() {
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

    // Page 0 — Publishers table (single stretch column: NAME)
    publishers_table_ = new QTableWidget;
    publishers_table_->setColumnCount(1);
    publishers_table_->setHorizontalHeaderLabels({"NAME"});
    publishers_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    configure_table(publishers_table_);
    connect(publishers_table_, &QTableWidget::cellDoubleClicked, this, &GovDataUKPanel::on_publisher_doubleclicked);
    content_stack_->addWidget(publishers_table_); // index 0

    // Page 1 — Datasets table: TITLE | FILES | MODIFIED | TAGS
    datasets_table_ = new QTableWidget;
    datasets_table_->setColumnCount(4);
    datasets_table_->setHorizontalHeaderLabels({"TITLE", "FILES", "MODIFIED", "TAGS"});
    datasets_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    datasets_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    datasets_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    datasets_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    datasets_table_->setColumnWidth(1, 55);
    datasets_table_->setColumnWidth(2, 100);
    datasets_table_->setColumnWidth(3, 200);
    configure_table(datasets_table_);
    connect(datasets_table_, &QTableWidget::cellDoubleClicked, this, &GovDataUKPanel::on_dataset_doubleclicked);
    content_stack_->addWidget(datasets_table_); // index 1

    // Page 2 — Resources table: NAME | FORMAT | SIZE | MODIFIED | OPEN
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
    configure_table(resources_table_);
    connect(
        resources_table_, &QTableWidget::cellClicked, this,
        [this](int row, int col) {
            if (col != 4)
                return;
            auto* item = resources_table_->item(row, 4);
            if (!item)
                return;
            const QString url = item->data(Qt::UserRole).toString();
            if (!url.isEmpty())
                QDesktopServices::openUrl(QUrl(url));
        },
        Qt::UniqueConnection);
    content_stack_->addWidget(resources_table_); // index 2

    // Page 3 — Status / loading / error
    auto* status_page = new QWidget(this);
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

QWidget* GovDataUKPanel::build_toolbar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("govPanelToolbar");
    bar->setFixedHeight(36);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(10, 0, 10, 0);
    hl->setSpacing(5);

    // Back button — hidden until we drill in
    back_btn_ = new QPushButton("← BACK");
    back_btn_->setObjectName("govBackBtn");
    back_btn_->setVisible(false);
    back_btn_->setCursor(Qt::PointingHandCursor);
    connect(back_btn_, &QPushButton::clicked, this, &GovDataUKPanel::on_back);
    hl->addWidget(back_btn_);

    hl->addSpacing(4);

    // Tab: Publishers
    publishers_btn_ = new QPushButton("PUBLISHERS");
    publishers_btn_->setObjectName("govTabBtn");
    publishers_btn_->setCheckable(true);
    publishers_btn_->setChecked(true);
    publishers_btn_->setCursor(Qt::PointingHandCursor);
    connect(publishers_btn_, &QPushButton::clicked, this, [this]() {
        if (current_publishers_.isEmpty()) {
            load_initial_data();
        } else {
            current_view_ = Publishers;
            content_stack_->setCurrentIndex(Publishers);
            update_toolbar_state();
            update_breadcrumb();
        }
    });
    hl->addWidget(publishers_btn_);

    // Tab: Datasets
    datasets_btn_ = new QPushButton("DATASETS");
    datasets_btn_->setObjectName("govTabBtn");
    datasets_btn_->setCheckable(true);
    datasets_btn_->setChecked(false);
    datasets_btn_->setCursor(Qt::PointingHandCursor);
    connect(datasets_btn_, &QPushButton::clicked, this, [this]() {
        if (current_datasets_.isEmpty())
            return;
        current_view_ = Datasets;
        content_stack_->setCurrentIndex(Datasets);
        update_toolbar_state();
        update_breadcrumb();
    });
    hl->addWidget(datasets_btn_);

    hl->addSpacing(6);

    // Popular shortcut
    popular_btn_ = new QPushButton("POPULAR");
    popular_btn_->setObjectName("govPopularBtn");
    popular_btn_->setCursor(Qt::PointingHandCursor);
    connect(popular_btn_, &QPushButton::clicked, this, &GovDataUKPanel::on_popular);
    hl->addWidget(popular_btn_);

    hl->addSpacing(10);

    // Search input
    search_input_ = new QLineEdit;
    search_input_->setObjectName("govSearch");
    search_input_->setPlaceholderText("Search datasets…");
    search_input_->setFixedWidth(200);
    search_input_->setFixedHeight(26);
    connect(search_input_, &QLineEdit::returnPressed, this, &GovDataUKPanel::on_search);
    hl->addWidget(search_input_);

    fetch_btn_ = new QPushButton("FETCH");
    fetch_btn_->setObjectName("govFetchBtn");
    fetch_btn_->setCursor(Qt::PointingHandCursor);
    connect(fetch_btn_, &QPushButton::clicked, this, &GovDataUKPanel::on_fetch);
    hl->addWidget(fetch_btn_);

    hl->addStretch(1);

    export_btn_ = new QPushButton("CSV");
    export_btn_->setObjectName("govCsvBtn");
    export_btn_->setCursor(Qt::PointingHandCursor);
    connect(export_btn_, &QPushButton::clicked, this, &GovDataUKPanel::on_export_csv);
    hl->addWidget(export_btn_);

    return bar;
}

// ── Initial load ─────────────────────────────────────────────────────────────

void GovDataUKPanel::load_initial_data() {
    show_loading("Loading UK Government publishers…");
    services::GovDataService::instance().execute(kGovDataUKScript, "publishers", {}, "ukgov_publishers");
}

// ── Result handler ────────────────────────────────────────────────────────────

void GovDataUKPanel::on_result(const QString& request_id, const services::GovDataResult& result) {
    // Only handle request IDs that belong to this panel
    if (!request_id.startsWith("ukgov_"))
        return;

    if (!result.success) {
        show_error(result.error);
        LOG_ERROR("GovDataUKPanel", "Request " + request_id + " failed: " + result.error);
        return;
    }

    // Unwrap top-level "data" field — always an array for UK panel
    QJsonArray data;
    const QJsonValue raw = result.data["data"];
    if (raw.isArray()) {
        data = raw.toArray();
    } else if (raw.isObject()) {
        // Defensive: in case a future command wraps in {datasets:[]}
        const QJsonObject obj = raw.toObject();
        if (obj.contains("datasets"))
            data = obj["datasets"].toArray();
        else if (obj.contains("result"))
            data = obj["result"].toArray();
    }

    if (request_id == "ukgov_publishers" || request_id == "ukgov_popular") {
        current_publishers_ = data;
        populate_publishers(data);
        current_view_ = Publishers;
        content_stack_->setCurrentIndex(Publishers);
        LOG_INFO("GovDataUKPanel", "Loaded " + QString::number(data.size()) + " publishers");

    } else if (request_id.startsWith("ukgov_datasets_")) {
        int total = result.data["metadata"].toObject()["total_count"].toInt(0);
        if (total == 0)
            total = data.size();
        current_datasets_ = data;
        populate_datasets(data, total);
        current_view_ = Datasets;
        content_stack_->setCurrentIndex(Datasets);
        LOG_INFO("GovDataUKPanel", "Loaded " + QString::number(data.size()) + " datasets");

    } else if (request_id.startsWith("ukgov_search_")) {
        int total = result.data["metadata"].toObject()["total_count"].toInt(0);
        if (total == 0)
            total = data.size();
        current_datasets_ = data;
        populate_datasets(data, total);
        current_view_ = Datasets;
        content_stack_->setCurrentIndex(Datasets);
        LOG_INFO("GovDataUKPanel", "Search returned " + QString::number(data.size()) + " datasets");

    } else if (request_id.startsWith("ukgov_resources_")) {
        current_resources_ = data;
        populate_resources(data);
        current_view_ = Resources;
        content_stack_->setCurrentIndex(Resources);
        LOG_INFO("GovDataUKPanel", "Loaded " + QString::number(data.size()) + " resources");
    }

    update_toolbar_state();
    update_breadcrumb();
}

// ── Populate tables ───────────────────────────────────────────────────────────

void GovDataUKPanel::populate_publishers(const QJsonArray& data) {
    publishers_table_->setRowCount(0);
    publishers_table_->setRowCount(data.size());

    for (int i = 0; i < data.size(); ++i) {
        const auto obj = data[i].toObject();

        // Prefer display_name → title → name → id
        QString name = obj["display_name"].toString();
        if (name.isEmpty())
            name = obj["title"].toString();
        if (name.isEmpty())
            name = obj["name"].toString();
        if (name.isEmpty())
            name = obj["id"].toString();

        // Use id as the navigation key (name field as fallback)
        const QString id = obj["id"].toString().isEmpty() ? obj["name"].toString() : obj["id"].toString();

        auto* name_item = new QTableWidgetItem(name);
        name_item->setData(Qt::UserRole, id);
        name_item->setData(Qt::UserRole + 1, name); // store display name for breadcrumb
        name_item->setForeground(QColor(kGovDataUKColor));
        publishers_table_->setItem(i, 0, name_item);
    }

    row_count_label_->setText(QString::number(data.size()) + " publishers");
}

void GovDataUKPanel::populate_datasets(const QJsonArray& data, int total_count) {
    datasets_table_->setRowCount(0);
    datasets_table_->setRowCount(data.size());

    for (int i = 0; i < data.size(); ++i) {
        const auto obj = data[i].toObject();

        QString title = obj["title"].toString();
        if (title.isEmpty())
            title = obj["name"].toString();

        const QString id = obj["id"].toString().isEmpty() ? obj["name"].toString() : obj["id"].toString();

        auto* title_item = new QTableWidgetItem(title);
        title_item->setData(Qt::UserRole, id);
        datasets_table_->setItem(i, 0, title_item);

        const int num_res = obj["num_resources"].toInt(0);
        auto* res_item = new QTableWidgetItem(QString::number(num_res));
        res_item->setTextAlignment(Qt::AlignCenter);
        res_item->setForeground(QColor(kGovDataUKColor));
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
        tag_item->setForeground(QColor(colors::TEXT_SECONDARY()));
        datasets_table_->setItem(i, 3, tag_item);
    }

    row_count_label_->setText(QString("Showing %1 of %2").arg(data.size()).arg(total_count));
}

void GovDataUKPanel::populate_resources(const QJsonArray& data) {
    resources_table_->setRowCount(0);
    resources_table_->setRowCount(data.size());

    for (int i = 0; i < data.size(); ++i) {
        const auto obj = data[i].toObject();

        QString name = obj["name"].toString();
        if (name.isEmpty())
            name = obj["description"].toString().left(60);
        if (name.isEmpty())
            name = obj["id"].toString();
        resources_table_->setItem(i, 0, new QTableWidgetItem(name));

        const QString format = obj["format"].toString().toUpper();
        auto* fmt_item = new QTableWidgetItem(format.isEmpty() ? "—" : format);
        fmt_item->setForeground(QColor(kGovDataUKColor));
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

        QString modified = obj["last_modified"].toString();
        if (modified.length() > 10)
            modified = modified.left(10);
        resources_table_->setItem(i, 3, new QTableWidgetItem(modified));

        const QString url = obj["url"].toString();
        auto* url_item = new QTableWidgetItem(url.isEmpty() ? "—" : "↗ OPEN");
        url_item->setData(Qt::UserRole, url);
        if (!url.isEmpty())
            url_item->setForeground(QColor(kGovDataUKColor));
        url_item->setTextAlignment(Qt::AlignCenter);
        resources_table_->setItem(i, 4, url_item);
    }

    row_count_label_->setText(QString::number(data.size()) + " files");
}

// ── Navigation slots ──────────────────────────────────────────────────────────

void GovDataUKPanel::on_publisher_doubleclicked(int row, int /*col*/) {
    auto* item = publishers_table_->item(row, 0);
    if (!item)
        return;

    selected_publisher_id_ = item->data(Qt::UserRole).toString();
    selected_publisher_ = item->data(Qt::UserRole + 1).toString();
    if (selected_publisher_.isEmpty())
        selected_publisher_ = item->text();

    show_loading("Loading datasets for "
                 " + selected_publisher_ + "
                 "…");
    services::GovDataService::instance().execute(kGovDataUKScript, "datasets", {selected_publisher_id_, "100"},
                                                 "ukgov_datasets_" + selected_publisher_id_);
}

void GovDataUKPanel::on_dataset_doubleclicked(int row, int /*col*/) {
    auto* item = datasets_table_->item(row, 0);
    if (!item)
        return;

    selected_dataset_id_ = item->data(Qt::UserRole).toString();
    show_loading("Loading resources…");
    services::GovDataService::instance().execute(kGovDataUKScript, "resources", {selected_dataset_id_},
                                                 "ukgov_resources_" + selected_dataset_id_);
}

void GovDataUKPanel::on_fetch() {
    // If search field has text, run a search; otherwise reload publishers
    const QString query = search_input_->text().trimmed();
    if (!query.isEmpty()) {
        on_search();
    } else {
        load_initial_data();
    }
}

void GovDataUKPanel::on_search() {
    const QString query = search_input_->text().trimmed();
    if (query.isEmpty())
        return;

    show_loading("Searching for "
                 " + query + "
                 "…");
    services::GovDataService::instance().execute(kGovDataUKScript, "search", {query, "50"}, "ukgov_search_" + query);
}

void GovDataUKPanel::on_popular() {
    show_loading("Loading popular publishers…");
    services::GovDataService::instance().execute(kGovDataUKScript, "popular-publishers", {}, "ukgov_popular");
}

void GovDataUKPanel::on_back() {
    if (current_view_ == Resources) {
        current_view_ = Datasets;
        content_stack_->setCurrentIndex(Datasets);
    } else if (current_view_ == Datasets) {
        current_view_ = Publishers;
        content_stack_->setCurrentIndex(Publishers);
        selected_publisher_.clear();
        selected_publisher_id_.clear();
    }
    update_toolbar_state();
    update_breadcrumb();
}

// ── Toolbar / breadcrumb state ────────────────────────────────────────────────

void GovDataUKPanel::update_toolbar_state() {
    const bool drilling = (current_view_ == Datasets || current_view_ == Resources);
    back_btn_->setVisible(drilling);

    publishers_btn_->setChecked(current_view_ == Publishers);
    datasets_btn_->setChecked(current_view_ == Datasets || current_view_ == Resources);
}

void GovDataUKPanel::update_breadcrumb() {
    QString text;
    switch (current_view_) {
        case Publishers:
            text = "All Publishers";
            break;
        case Datasets:
            text = selected_publisher_.isEmpty() ? "Search Results"
                                                 : "All Publishers  ›  " + selected_publisher_ + "  ›  Datasets";
            break;
        case Resources:
            text = "All Publishers  ›  " + selected_publisher_ + "  ›  Datasets  ›  Resources";
            break;
        case Status:
            text = "Loading…";
            break;
    }
    breadcrumb_label_->setText(text);
}

// ── Status helpers ────────────────────────────────────────────────────────────

void GovDataUKPanel::show_loading(const QString& message) {
    status_label_->setStyleSheet(QString("color:%1; font-size:13px; background:transparent;").arg(kGovDataUKColor));
    status_label_->setText(message);
    content_stack_->setCurrentIndex(Status);
}

void GovDataUKPanel::show_error(const QString& message) {
    status_label_->setStyleSheet(QString("color:%1; font-size:12px; background:transparent;").arg(colors::NEGATIVE()));
    status_label_->setText("Error: " + message);
    content_stack_->setCurrentIndex(Status);
}

// ── CSV export ────────────────────────────────────────────────────────────────

void GovDataUKPanel::on_export_csv() {
    QTableWidget* table = nullptr;
    QString def_name;

    switch (current_view_) {
        case Publishers:
            if (publishers_table_->rowCount() > 0) {
                table = publishers_table_;
                def_name = "uk_publishers.csv";
            }
            break;
        case Datasets:
            if (datasets_table_->rowCount() > 0) {
                table = datasets_table_;
                def_name = "uk_datasets.csv";
            }
            break;
        case Resources:
            if (resources_table_->rowCount() > 0) {
                table = resources_table_;
                def_name = "uk_resources.csv";
            }
            break;
        case Status:
            break;
    }
    if (!table)
        return;
    export_table_to_csv(table, def_name, this);
}

} // namespace fincept::screens
