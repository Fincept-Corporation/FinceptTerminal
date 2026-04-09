// src/screens/gov_data/GovDataProviderPanel.cpp
#include "screens/gov_data/GovDataProviderPanel.h"

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

using namespace fincept::ui;

// ── Shared panel stylesheet ──────────────────────────────────────────────────

static QString make_panel_style(const QString& color) {
    const auto& t = ThemeManager::instance().tokens();
    const auto cc = QColor(color);
    const QString cr = QString::number(cc.red());
    const QString cg = QString::number(cc.green());
    const QString cb = QString::number(cc.blue());

    QString s;
    // Toolbar
    s += QString("#govPanelToolbar { background:%1; border-bottom:1px solid %2; }").arg(t.bg_raised, t.border_dim);

    // Tab buttons
    s += QString("#govTabBtn { background:transparent; color:%1; border:1px solid %2;"
                 "  font-size:10px; font-weight:700; padding:4px 12px; letter-spacing:0.5px; }").arg(t.text_secondary, t.border_dim);
    s += QString("#govTabBtn:hover { color:%1; background:%2; }").arg(t.text_primary, t.bg_hover);
    s += QString("#govTabBtn:checked { background:rgba(%1,%2,%3,0.12); color:%4;"
                 "  border:1px solid %4; }").arg(cr, cg, cb, color);

    // Back button
    s += QString("#govBackBtn { background:transparent; color:%1; border:1px solid %2;"
                 "  font-size:10px; font-weight:700; padding:4px 10px; }").arg(t.text_secondary, t.border_dim);
    s += QString("#govBackBtn:hover { color:%1; background:%2; }").arg(t.text_primary, t.bg_hover);

    // Fetch / action buttons
    s += QString("#govFetchBtn { background:%1; color:%2; border:none;"
                 "  font-size:10px; font-weight:700; padding:4px 14px; }").arg(color, t.bg_base);
    s += QString("#govFetchBtn:hover { background:%1; }").arg(cc.lighter(120).name());
    s += QString("#govFetchBtn:disabled { background:%1; color:%2; }").arg(t.border_dim, t.text_dim);
    s += QString("#govCsvBtn { background:transparent; color:%1; border:1px solid %2;"
                 "  font-size:10px; font-weight:700; padding:4px 10px; }").arg(t.text_secondary, t.border_dim);
    s += QString("#govCsvBtn:hover { color:%1; background:%2; }").arg(t.text_primary, t.bg_hover);

    // Search
    s += QString("#govSearch { background:%1; color:%2; border:none;"
                 "  border-bottom:1px solid %3; padding:4px 10px; font-size:11px; }").arg(t.bg_base, t.text_primary, t.border_dim);
    s += QString("#govSearch:focus { border-bottom:1px solid %1; }").arg(color);

    // Tables
    s += QString("QTableWidget { background:%1; color:%2; border:none;"
                 "  gridline-color:%3; font-size:11px; alternate-background-color:%4; }").arg(t.bg_base, t.text_primary, t.border_dim, t.bg_surface);
    s += QString("QTableWidget::item { padding:5px 8px; border-bottom:1px solid %1; }").arg(t.border_dim);
    s += QString("QTableWidget::item:selected { background:rgba(%1,%2,%3,0.1); color:%4; }").arg(cr, cg, cb, color);
    s += QString("QHeaderView::section { background:%1; color:%2; border:none;"
                 "  border-bottom:2px solid %3; border-right:1px solid %3;"
                 "  padding:5px 8px; font-size:10px; font-weight:700; letter-spacing:0.5px; }").arg(t.bg_raised, t.text_secondary, t.border_dim);

    // Status / empty state
    s += QString("#govStatusPage { background:%1; }").arg(t.bg_base);
    s += QString("#govStatusMsg { color:%1; font-size:13px; background:transparent; }").arg(t.text_secondary);
    s += QString("#govStatusErr { color:%1; font-size:12px; background:transparent; }").arg(t.negative);

    // Breadcrumb
    s += QString("#govBreadcrumb { background:%1; border-bottom:1px solid %2; }").arg(t.bg_surface, t.border_dim);
    s += QString("#govBreadcrumbText { color:%1; font-size:9px; background:transparent; }").arg(t.text_secondary);
    s += QString("#govBreadcrumbActive { color:%1; font-size:9px; font-weight:700; background:transparent; }").arg(color);

    // Scrollbar
    s += QString("QScrollBar:vertical { background:%1; width:5px; }").arg(t.bg_base);
    s += QString("QScrollBar::handle:vertical { background:%1; min-height:20px; }").arg(t.border_dim);
    s += "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }";
    return s;
}

// ── Constructor ──────────────────────────────────────────────────────────────

GovDataProviderPanel::GovDataProviderPanel(const QString& script,
                                           const QString& provider_color,
                                           const QString& org_label,
                                           QWidget*       parent)
    : QWidget(parent), script_(script), color_(provider_color), org_label_(org_label) {
    setStyleSheet(make_panel_style(color_));
    build_ui();
    connect(&services::GovDataService::instance(), &services::GovDataService::result_ready,
            this, &GovDataProviderPanel::on_result);
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this, [this]() {
        setStyleSheet(make_panel_style(color_));
    });
}

// ── Build UI ─────────────────────────────────────────────────────────────────

void GovDataProviderPanel::build_ui() {
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
    breadcrumb_label_ = new QLabel("All " + org_label_);
    breadcrumb_label_->setObjectName("govBreadcrumbText");
    bcl->addWidget(breadcrumb_label_);
    bcl->addStretch(1);
    row_count_label_ = new QLabel;
    row_count_label_->setObjectName("govBreadcrumbText");
    bcl->addWidget(row_count_label_);
    root->addWidget(breadcrumb_);

    // Content stack
    content_stack_ = new QStackedWidget;

    // Page 0 — Orgs table
    orgs_table_ = new QTableWidget;
    orgs_table_->setColumnCount(2);
    orgs_table_->setHorizontalHeaderLabels({org_label_.toUpper(), "DATASETS"});
    orgs_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    orgs_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    orgs_table_->setColumnWidth(1, 80);
    orgs_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    orgs_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    orgs_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    orgs_table_->verticalHeader()->setVisible(false);
    orgs_table_->setAlternatingRowColors(true);
    orgs_table_->setShowGrid(true);
    connect(orgs_table_, &QTableWidget::cellDoubleClicked,
            this, &GovDataProviderPanel::on_org_clicked);
    content_stack_->addWidget(orgs_table_);

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
    connect(datasets_table_, &QTableWidget::cellDoubleClicked,
            this, &GovDataProviderPanel::on_dataset_clicked);
    content_stack_->addWidget(datasets_table_);

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
    content_stack_->addWidget(resources_table_);

    // Page 3 — Status / empty / loading
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

QWidget* GovDataProviderPanel::build_toolbar() {
    auto* bar = new QWidget;
    bar->setObjectName("govPanelToolbar");
    bar->setFixedHeight(40);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(10, 0, 10, 0);
    hl->setSpacing(5);

    // Back button
    back_btn_ = new QPushButton("← BACK");
    back_btn_->setObjectName("govBackBtn");
    back_btn_->setVisible(false);
    back_btn_->setCursor(Qt::PointingHandCursor);
    connect(back_btn_, &QPushButton::clicked, this, &GovDataProviderPanel::on_back);
    hl->addWidget(back_btn_);

    hl->addSpacing(4);

    // View tabs
    orgs_btn_ = new QPushButton(org_label_.toUpper());
    orgs_btn_->setObjectName("govTabBtn");
    orgs_btn_->setCheckable(true);
    orgs_btn_->setChecked(true);
    orgs_btn_->setCursor(Qt::PointingHandCursor);
    connect(orgs_btn_, &QPushButton::clicked, this, [this]() { on_view_changed(Orgs); });
    hl->addWidget(orgs_btn_);

    datasets_btn_ = new QPushButton("DATASETS");
    datasets_btn_->setObjectName("govTabBtn");
    datasets_btn_->setCheckable(true);
    datasets_btn_->setCursor(Qt::PointingHandCursor);
    connect(datasets_btn_, &QPushButton::clicked, this, [this]() { on_view_changed(Datasets); });
    hl->addWidget(datasets_btn_);

    search_btn_ = new QPushButton("SEARCH");
    search_btn_->setObjectName("govTabBtn");
    search_btn_->setCheckable(true);
    search_btn_->setCursor(Qt::PointingHandCursor);
    connect(search_btn_, &QPushButton::clicked, this, [this]() { on_view_changed(Search); });
    hl->addWidget(search_btn_);

    hl->addSpacing(10);

    // Search input
    search_input_ = new QLineEdit;
    search_input_->setObjectName("govSearch");
    search_input_->setPlaceholderText("Search datasets…");
    search_input_->setFixedWidth(210);
    search_input_->setFixedHeight(26);
    connect(search_input_, &QLineEdit::returnPressed, this, &GovDataProviderPanel::on_search);
    hl->addWidget(search_input_);

    fetch_btn_ = new QPushButton("FETCH");
    fetch_btn_->setObjectName("govFetchBtn");
    fetch_btn_->setCursor(Qt::PointingHandCursor);
    connect(fetch_btn_, &QPushButton::clicked, this, &GovDataProviderPanel::on_search);
    hl->addWidget(fetch_btn_);

    hl->addStretch(1);

    export_btn_ = new QPushButton("CSV");
    export_btn_->setObjectName("govCsvBtn");
    export_btn_->setCursor(Qt::PointingHandCursor);
    connect(export_btn_, &QPushButton::clicked, this, &GovDataProviderPanel::on_export_csv);
    hl->addWidget(export_btn_);

    return bar;
}

// ── Data loading ─────────────────────────────────────────────────────────────

void GovDataProviderPanel::load_initial_data() {
    show_loading("Loading " + org_label_.toLower() + "…");
    services::GovDataService::instance().execute(
        script_, "publishers", {}, "gov_orgs_" + script_);
}

void GovDataProviderPanel::on_result(const QString& request_id,
                                     const services::GovDataResult& result) {
    if (!request_id.contains(script_) &&
        !request_id.startsWith("gov_search_"   + script_) &&
        !request_id.startsWith("gov_datasets_" + script_) &&
        !request_id.startsWith("gov_resources_"+ script_))
        return;

    if (!result.success) { show_error(result.error); return; }

    // Some scripts (Australia search/org-datasets) return data as {"datasets":[...], "count":...}
    // instead of a flat array. Unwrap if needed.
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

    if (request_id.startsWith("gov_orgs_")) {
        current_orgs_ = data;
        populate_orgs(data);
        content_stack_->setCurrentIndex(Orgs);
        current_view_ = Orgs;
        update_breadcrumb();
    } else if (request_id.startsWith("gov_datasets_") ||
               request_id.startsWith("gov_search_")) {
        int total = result.data["metadata"].toObject()["total_count"].toInt(0);
        // Australia search wraps count inside data object
        if (total == 0 && result.data["data"].isObject())
            total = result.data["data"].toObject()["count"].toInt(0);
        if (total == 0) total = data.size();
        current_datasets_ = data;
        populate_datasets(data, total);
        content_stack_->setCurrentIndex(Datasets);
        current_view_ = Datasets;
        update_breadcrumb();
    } else if (request_id.startsWith("gov_resources_")) {
        current_resources_ = data;
        populate_resources(data);
        content_stack_->setCurrentIndex(Resources);
        current_view_ = Resources;
        update_breadcrumb();
    }

    update_toolbar_state();
}

// ── Populate tables ──────────────────────────────────────────────────────────

void GovDataProviderPanel::populate_orgs(const QJsonArray& data) {
    orgs_table_->setRowCount(0);
    orgs_table_->setRowCount(data.size());

    for (int i = 0; i < data.size(); ++i) {
        const auto obj = data[i].toObject();
        QString name = obj["display_name"].toString();
        if (name.isEmpty()) name = obj["title"].toString();
        if (name.isEmpty()) name = obj["name"].toString();
        if (name.isEmpty()) name = obj["id"].toString();

        auto* name_item = new QTableWidgetItem(name);
        name_item->setData(Qt::UserRole,
            obj["id"].toString().isEmpty() ? obj["name"].toString() : obj["id"].toString());
        name_item->setForeground(QColor(color_));
        orgs_table_->setItem(i, 0, name_item);

        int count = obj["dataset_count"].toInt(-1);
        auto* cnt = new QTableWidgetItem(count >= 0 ? QString::number(count) : "—");
        cnt->setTextAlignment(Qt::AlignCenter);
        orgs_table_->setItem(i, 1, cnt);
    }

    row_count_label_->setText(QString::number(data.size()) + " " + org_label_.toLower() + "s");
}

void GovDataProviderPanel::populate_datasets(const QJsonArray& data, int total_count) {
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
        auto* res_item = new QTableWidgetItem(QString::number(num_res));
        res_item->setTextAlignment(Qt::AlignCenter);
        res_item->setForeground(QColor(color_));
        datasets_table_->setItem(i, 1, res_item);

        QString modified = obj["metadata_modified"].toString();
        if (modified.length() > 10) modified = modified.left(10);
        datasets_table_->setItem(i, 2, new QTableWidgetItem(modified));

        QStringList tags;
        for (const auto& t : obj["tags"].toArray()) {
            if (t.isString()) tags << t.toString();
            else if (t.isObject()) tags << t.toObject()["name"].toString();
        }
        QString tags_str = tags.mid(0, 3).join(", ");
        if (tags.size() > 3) tags_str += QString(" +%1").arg(tags.size() - 3);
        auto* tag_item = new QTableWidgetItem(tags_str);
        tag_item->setForeground(QColor(colors::TEXT_SECONDARY()));
        datasets_table_->setItem(i, 3, tag_item);
    }

    row_count_label_->setText(
        QString("Showing %1 of %2").arg(data.size()).arg(total_count));
}

void GovDataProviderPanel::populate_resources(const QJsonArray& data) {
    resources_table_->setRowCount(0);
    resources_table_->setRowCount(data.size());

    for (int i = 0; i < data.size(); ++i) {
        const auto obj = data[i].toObject();
        QString name = obj["name"].toString();
        if (name.isEmpty()) name = obj["id"].toString();
        resources_table_->setItem(i, 0, new QTableWidgetItem(name));

        QString format = obj["format"].toString().toUpper();
        auto* fmt_item = new QTableWidgetItem(format.isEmpty() ? "—" : format);
        fmt_item->setForeground(QColor(color_));
        fmt_item->setTextAlignment(Qt::AlignCenter);
        resources_table_->setItem(i, 1, fmt_item);

        double size_bytes = obj["size"].toDouble(0);
        QString size_str = "—";
        if (size_bytes > 0) {
            if (size_bytes < 1024)
                size_str = QString::number(size_bytes, 'f', 0) + " B";
            else if (size_bytes < 1024 * 1024)
                size_str = QString::number(size_bytes / 1024, 'f', 1) + " KB";
            else
                size_str = QString::number(size_bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
        }
        auto* sz = new QTableWidgetItem(size_str);
        sz->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        resources_table_->setItem(i, 2, sz);

        QString modified = obj["last_modified"].toString();
        if (modified.length() > 10) modified = modified.left(10);
        resources_table_->setItem(i, 3, new QTableWidgetItem(modified));

        QString url = obj["url"].toString();
        auto* url_item = new QTableWidgetItem(url.isEmpty() ? "—" : "↗ OPEN");
        url_item->setData(Qt::UserRole, url);
        if (!url.isEmpty()) url_item->setForeground(QColor(color_));
        url_item->setTextAlignment(Qt::AlignCenter);
        resources_table_->setItem(i, 4, url_item);
    }

    connect(resources_table_, &QTableWidget::cellClicked, this,
        [this](int row, int col) {
            if (col != 4) return;
            auto* item = resources_table_->item(row, 4);
            if (!item) return;
            QString url = item->data(Qt::UserRole).toString();
            if (!url.isEmpty()) QDesktopServices::openUrl(QUrl(url));
        }, Qt::UniqueConnection);

    row_count_label_->setText(QString::number(data.size()) + " files");
}

// ── View navigation ──────────────────────────────────────────────────────────

void GovDataProviderPanel::on_view_changed(int index) {
    auto view = static_cast<View>(index);
    orgs_btn_->setChecked(view == Orgs);
    datasets_btn_->setChecked(view == Datasets);
    search_btn_->setChecked(view == Search);

    if (view == Orgs) {
        if (current_orgs_.isEmpty()) load_initial_data();
        else content_stack_->setCurrentIndex(Orgs);
        current_view_ = Orgs;
    } else if (view == Search) {
        current_view_ = Search;
        search_input_->setFocus();
    }
    update_toolbar_state();
    update_breadcrumb();
}

void GovDataProviderPanel::on_org_clicked(int row) {
    auto* item = orgs_table_->item(row, 0);
    if (!item) return;
    selected_org_ = item->data(Qt::UserRole).toString();
    show_loading("Loading datasets for "" + item->text() + ""…");
    services::GovDataService::instance().execute(
        script_, "datasets", {selected_org_, "100"}, "gov_datasets_" + script_);
}

void GovDataProviderPanel::on_dataset_clicked(int row) {
    auto* item = datasets_table_->item(row, 0);
    if (!item) return;
    selected_dataset_ = item->data(Qt::UserRole).toString();
    show_loading("Loading resources…");
    services::GovDataService::instance().execute(
        script_, "resources", {selected_dataset_}, "gov_resources_" + script_);
}

void GovDataProviderPanel::on_search() {
    QString query = search_input_->text().trimmed();
    if (query.isEmpty()) return;
    show_loading("Searching for "" + query + ""…");
    services::GovDataService::instance().execute(
        script_, "search", {query, "50"}, "gov_search_" + script_);
}

void GovDataProviderPanel::on_back() {
    if (current_view_ == Resources) {
        content_stack_->setCurrentIndex(Datasets);
        current_view_ = Datasets;
    } else if (current_view_ == Datasets) {
        content_stack_->setCurrentIndex(Orgs);
        current_view_ = Orgs;
        selected_org_.clear();
    }
    update_toolbar_state();
    update_breadcrumb();
}

void GovDataProviderPanel::update_toolbar_state() {
    back_btn_->setVisible(current_view_ == Datasets || current_view_ == Resources);
}

void GovDataProviderPanel::update_breadcrumb() {
    QString text;
    switch (current_view_) {
        case Orgs:
            text = "All " + org_label_; break;
        case Datasets:
            text = "All " + org_label_ + "  ›  " + selected_org_ + "  ›  Datasets"; break;
        case Resources:
            text = "All " + org_label_ + "  ›  Datasets  ›  Resources"; break;
        case Search:
            text = "Search Results"; break;
    }
    breadcrumb_label_->setText(text);
}

// ── Status helpers ───────────────────────────────────────────────────────────

void GovDataProviderPanel::show_loading(const QString& message) {
    status_label_->setObjectName("govStatusMsg");
    status_label_->setStyleSheet(
        QString("color:%1; font-size:13px; background:transparent;").arg(color_));
    status_label_->setText(message);
    content_stack_->setCurrentIndex(3);
}

void GovDataProviderPanel::show_error(const QString& message) {
    status_label_->setObjectName("govStatusErr");
    status_label_->setStyleSheet(
        QString("color:%1; font-size:12px; background:transparent;").arg(colors::NEGATIVE()));
    status_label_->setText("Error: " + message);
    content_stack_->setCurrentIndex(3);
}

void GovDataProviderPanel::show_empty(const QString& message) {
    status_label_->setObjectName("govStatusMsg");
    status_label_->setStyleSheet(
        QString("color:%1; font-size:12px; background:transparent;").arg(colors::TEXT_SECONDARY()));
    status_label_->setText(message);
    content_stack_->setCurrentIndex(3);
}

// ── CSV export ───────────────────────────────────────────────────────────────

void GovDataProviderPanel::on_export_csv() {
    QTableWidget* table     = nullptr;
    QString       def_name;

    if (current_view_ == Orgs && orgs_table_->rowCount() > 0) {
        table    = orgs_table_;
        def_name = "gov_" + org_label_.toLower() + ".csv";
    } else if ((current_view_ == Datasets || current_view_ == Search)
               && datasets_table_->rowCount() > 0) {
        table    = datasets_table_;
        def_name = "gov_datasets.csv";
    } else if (current_view_ == Resources && resources_table_->rowCount() > 0) {
        table    = resources_table_;
        def_name = "gov_resources.csv";
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
}

} // namespace fincept::screens
