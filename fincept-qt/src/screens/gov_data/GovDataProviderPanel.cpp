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

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

GovDataProviderPanel::GovDataProviderPanel(const QString& script,
                                             const QString& provider_color,
                                             const QString& org_label,
                                             QWidget* parent)
    : QWidget(parent), script_(script), color_(provider_color), org_label_(org_label) {
    build_ui();

    connect(&services::GovDataService::instance(),
            &services::GovDataService::result_ready,
            this, &GovDataProviderPanel::on_result);
}

// ─────────────────────────────────────────────────────────────────────────────
// Build UI
// ─────────────────────────────────────────────────────────────────────────────

void GovDataProviderPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_toolbar());

    // Content stack
    content_stack_ = new QStackedWidget(this);

    // Page 0: Orgs table
    orgs_table_ = new QTableWidget(this);
    orgs_table_->setColumnCount(2);
    orgs_table_->setHorizontalHeaderLabels({org_label_.toUpper(), "DATASETS"});
    orgs_table_->horizontalHeader()->setStretchLastSection(false);
    orgs_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    orgs_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    orgs_table_->setColumnWidth(1, 80);
    orgs_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    orgs_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    orgs_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    orgs_table_->verticalHeader()->setVisible(false);
    orgs_table_->setAlternatingRowColors(true);
    connect(orgs_table_, &QTableWidget::cellDoubleClicked, this, &GovDataProviderPanel::on_org_clicked);
    content_stack_->addWidget(orgs_table_);

    // Page 1: Datasets table
    datasets_table_ = new QTableWidget(this);
    datasets_table_->setColumnCount(4);
    datasets_table_->setHorizontalHeaderLabels({"TITLE", "RESOURCES", "MODIFIED", "TAGS"});
    datasets_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    datasets_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    datasets_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    datasets_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    datasets_table_->setColumnWidth(1, 80);
    datasets_table_->setColumnWidth(2, 100);
    datasets_table_->setColumnWidth(3, 180);
    datasets_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    datasets_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    datasets_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    datasets_table_->verticalHeader()->setVisible(false);
    datasets_table_->setAlternatingRowColors(true);
    connect(datasets_table_, &QTableWidget::cellDoubleClicked, this, &GovDataProviderPanel::on_dataset_clicked);
    content_stack_->addWidget(datasets_table_);

    // Page 2: Resources table
    resources_table_ = new QTableWidget(this);
    resources_table_->setColumnCount(5);
    resources_table_->setHorizontalHeaderLabels({"NAME", "FORMAT", "SIZE", "MODIFIED", "URL"});
    resources_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    resources_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    resources_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    resources_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    resources_table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    resources_table_->setColumnWidth(1, 80);
    resources_table_->setColumnWidth(2, 80);
    resources_table_->setColumnWidth(3, 100);
    resources_table_->setColumnWidth(4, 60);
    resources_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    resources_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    resources_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    resources_table_->verticalHeader()->setVisible(false);
    resources_table_->setAlternatingRowColors(true);
    content_stack_->addWidget(resources_table_);

    // Page 3: Empty/loading/error label
    status_label_ = new QLabel(this);
    status_label_->setAlignment(Qt::AlignCenter);
    status_label_->setWordWrap(true);
    content_stack_->addWidget(status_label_);

    root->addWidget(content_stack_, 1);

    // Style all tables
    const QString table_style = QString(
        "QTableWidget { background:%1; color:%2; border:none; gridline-color:%3;"
        "  font-family:%4; font-size:12px; }"
        "QTableWidget::item { padding:6px 8px; border-bottom:1px solid %3; }"
        "QTableWidget::item:selected { background:%5; }"
        "QTableWidget::item:alternate { background:%6; }"
        "QHeaderView::section { background:%7; color:%8; border:none; border-bottom:1px solid %3;"
        "  font-family:%4; font-size:10px; font-weight:700; padding:6px 8px; }")
        .arg(colors::BG_BASE, colors::TEXT_PRIMARY, colors::BORDER_DIM,
             fonts::DATA_FAMILY, colors::BG_HOVER, colors::ROW_ALT,
             colors::BG_RAISED, color_);

    orgs_table_->setStyleSheet(table_style);
    datasets_table_->setStyleSheet(table_style);
    resources_table_->setStyleSheet(table_style);

    status_label_->setStyleSheet(
        QString("QLabel { color:%1; font-family:%2; font-size:12px; }")
            .arg(colors::TEXT_SECONDARY, fonts::DATA_FAMILY));
}

QWidget* GovDataProviderPanel::build_toolbar() {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(38);
    bar->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;")
            .arg(colors::BG_RAISED, colors::BORDER_MED));

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(8, 0, 8, 0);
    hl->setSpacing(4);

    const QString btn_style =
        QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                " font-family:%3; font-size:10px; font-weight:700; padding:4px 10px; }"
                "QPushButton:hover { background:%4; }"
                "QPushButton:checked { background:%5; color:%6; border-color:%5; }")
            .arg(colors::TEXT_SECONDARY, colors::BORDER_DIM, fonts::DATA_FAMILY,
                 colors::BG_HOVER, color_, colors::TEXT_PRIMARY);

    // Back button
    back_btn_ = new QPushButton("< BACK", bar);
    back_btn_->setStyleSheet(btn_style);
    back_btn_->setVisible(false);
    connect(back_btn_, &QPushButton::clicked, this, &GovDataProviderPanel::on_back);
    hl->addWidget(back_btn_);

    // View tabs
    orgs_btn_ = new QPushButton(org_label_.toUpper(), bar);
    orgs_btn_->setCheckable(true);
    orgs_btn_->setChecked(true);
    orgs_btn_->setStyleSheet(btn_style);
    connect(orgs_btn_, &QPushButton::clicked, this, [this]() { on_view_changed(Orgs); });
    hl->addWidget(orgs_btn_);

    datasets_btn_ = new QPushButton("DATASETS", bar);
    datasets_btn_->setCheckable(true);
    datasets_btn_->setStyleSheet(btn_style);
    connect(datasets_btn_, &QPushButton::clicked, this, [this]() { on_view_changed(Datasets); });
    hl->addWidget(datasets_btn_);

    search_btn_ = new QPushButton("SEARCH", bar);
    search_btn_->setCheckable(true);
    search_btn_->setStyleSheet(btn_style);
    connect(search_btn_, &QPushButton::clicked, this, [this]() { on_view_changed(Search); });
    hl->addWidget(search_btn_);

    hl->addSpacing(8);

    // Search input
    search_input_ = new QLineEdit(bar);
    search_input_->setPlaceholderText("Search datasets...");
    search_input_->setFixedWidth(200);
    search_input_->setStyleSheet(
        QString("QLineEdit { background:%1; color:%2; border:1px solid %3;"
                " font-family:%4; font-size:11px; padding:4px 8px; }"
                "QLineEdit:focus { border-color:%5; }")
            .arg(colors::BG_BASE, colors::TEXT_PRIMARY, colors::BORDER_DIM,
                 fonts::DATA_FAMILY, color_));
    connect(search_input_, &QLineEdit::returnPressed, this, &GovDataProviderPanel::on_search);
    hl->addWidget(search_input_);

    // Fetch button
    fetch_btn_ = new QPushButton("FETCH", bar);
    fetch_btn_->setStyleSheet(
        QString("QPushButton { background:%1; color:%2; border:none;"
                " font-family:%3; font-size:10px; font-weight:700; padding:4px 12px; }"
                "QPushButton:hover { background:%4; }")
            .arg(color_, colors::TEXT_PRIMARY, fonts::DATA_FAMILY,
                 color_)); // same color, opacity handled in code
    connect(fetch_btn_, &QPushButton::clicked, this, &GovDataProviderPanel::on_search);
    hl->addWidget(fetch_btn_);

    hl->addStretch();

    // Export CSV
    export_btn_ = new QPushButton("CSV", bar);
    export_btn_->setStyleSheet(btn_style);
    connect(export_btn_, &QPushButton::clicked, this, &GovDataProviderPanel::on_export_csv);
    hl->addWidget(export_btn_);

    return bar;
}

// ─────────────────────────────────────────────────────────────────────────────
// Data loading
// ─────────────────────────────────────────────────────────────────────────────

void GovDataProviderPanel::load_initial_data() {
    show_loading("Loading " + org_label_.toLower() + "...");
    services::GovDataService::instance().execute(
        script_, "publishers", {}, "gov_orgs_" + script_);
}

void GovDataProviderPanel::on_result(const QString& request_id,
                                       const services::GovDataResult& result) {
    // Filter results for this panel's script
    if (!request_id.contains(script_) && !request_id.startsWith("gov_search_" + script_)
        && !request_id.startsWith("gov_datasets_" + script_)
        && !request_id.startsWith("gov_resources_" + script_)) {
        return;
    }

    if (!result.success) {
        show_error(result.error);
        return;
    }

    const QJsonArray data = result.data["data"].toArray();

    if (request_id.startsWith("gov_orgs_")) {
        current_orgs_ = data;
        populate_orgs(data);
        content_stack_->setCurrentIndex(Orgs);
        current_view_ = Orgs;
    } else if (request_id.startsWith("gov_datasets_") || request_id.startsWith("gov_search_")) {
        int total = result.data["metadata"].toObject()["total_count"].toInt(data.size());
        current_datasets_ = data;
        populate_datasets(data, total);
        content_stack_->setCurrentIndex(Datasets);
        current_view_ = Datasets;
    } else if (request_id.startsWith("gov_resources_")) {
        current_resources_ = data;
        populate_resources(data);
        content_stack_->setCurrentIndex(Resources);
        current_view_ = Resources;
    }

    update_toolbar_state();
}

// ─────────────────────────────────────────────────────────────────────────────
// Populate tables
// ─────────────────────────────────────────────────────────────────────────────

void GovDataProviderPanel::populate_orgs(const QJsonArray& data) {
    orgs_table_->setRowCount(0);
    orgs_table_->setRowCount(data.size());

    for (int i = 0; i < data.size(); ++i) {
        const auto obj = data[i].toObject();
        QString name = obj["display_name"].toString();
        if (name.isEmpty()) name = obj["name"].toString();
        if (name.isEmpty()) name = obj["id"].toString();

        auto* name_item = new QTableWidgetItem(name);
        name_item->setData(Qt::UserRole, obj["id"].toString().isEmpty()
                                             ? obj["name"].toString()
                                             : obj["id"].toString());
        name_item->setForeground(QColor(color_));
        orgs_table_->setItem(i, 0, name_item);

        int count = obj["dataset_count"].toInt(-1);
        auto* count_item = new QTableWidgetItem(count >= 0 ? QString::number(count) : "-");
        count_item->setTextAlignment(Qt::AlignCenter);
        orgs_table_->setItem(i, 1, count_item);
    }
}

void GovDataProviderPanel::populate_datasets(const QJsonArray& data, int total_count) {
    datasets_table_->setRowCount(0);
    datasets_table_->setRowCount(data.size());

    for (int i = 0; i < data.size(); ++i) {
        const auto obj = data[i].toObject();
        QString title = obj["title"].toString();
        if (title.isEmpty()) title = obj["name"].toString();

        auto* title_item = new QTableWidgetItem(title);
        title_item->setData(Qt::UserRole, obj["id"].toString().isEmpty()
                                              ? obj["name"].toString()
                                              : obj["id"].toString());
        datasets_table_->setItem(i, 0, title_item);

        int num_res = obj["num_resources"].toInt(0);
        auto* res_item = new QTableWidgetItem(QString::number(num_res));
        res_item->setTextAlignment(Qt::AlignCenter);
        res_item->setForeground(QColor(color_));
        datasets_table_->setItem(i, 1, res_item);

        QString modified = obj["metadata_modified"].toString();
        if (modified.length() > 10) modified = modified.left(10);
        datasets_table_->setItem(i, 2, new QTableWidgetItem(modified));

        // Tags
        QStringList tags;
        const auto tags_arr = obj["tags"].toArray();
        for (const auto& t : tags_arr) {
            if (t.isString()) tags << t.toString();
            else if (t.isObject()) tags << t.toObject()["name"].toString();
        }
        QString tags_str = tags.mid(0, 3).join(", ");
        if (tags.size() > 3) tags_str += QString(" +%1").arg(tags.size() - 3);
        datasets_table_->setItem(i, 3, new QTableWidgetItem(tags_str));
    }

    // Update status
    if (status_label_) {
        status_label_->setText(QString("Showing %1 of %2 datasets")
                                   .arg(data.size()).arg(total_count));
    }
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
        auto* fmt_item = new QTableWidgetItem(format);
        fmt_item->setForeground(QColor(color_));
        resources_table_->setItem(i, 1, fmt_item);

        // Size
        double size_bytes = obj["size"].toDouble(0);
        QString size_str = "-";
        if (size_bytes > 0) {
            if (size_bytes < 1024) size_str = QString::number(size_bytes, 'f', 0) + " B";
            else if (size_bytes < 1024 * 1024)
                size_str = QString::number(size_bytes / 1024, 'f', 1) + " KB";
            else size_str = QString::number(size_bytes / (1024 * 1024), 'f', 1) + " MB";
        }
        resources_table_->setItem(i, 2, new QTableWidgetItem(size_str));

        QString modified = obj["last_modified"].toString();
        if (modified.length() > 10) modified = modified.left(10);
        resources_table_->setItem(i, 3, new QTableWidgetItem(modified));

        // URL link
        QString url = obj["url"].toString();
        auto* url_item = new QTableWidgetItem(url.isEmpty() ? "-" : "OPEN");
        url_item->setData(Qt::UserRole, url);
        if (!url.isEmpty()) url_item->setForeground(QColor(color_));
        url_item->setTextAlignment(Qt::AlignCenter);
        resources_table_->setItem(i, 4, url_item);
    }

    // Enable URL click
    connect(resources_table_, &QTableWidget::cellClicked, this,
            [this](int row, int col) {
                if (col == 4) {
                    auto* item = resources_table_->item(row, 4);
                    if (!item) return;
                    QString url = item->data(Qt::UserRole).toString();
                    if (!url.isEmpty()) {
                        QDesktopServices::openUrl(QUrl(url));
                    }
                }
            },
            Qt::UniqueConnection);
}

// ─────────────────────────────────────────────────────────────────────────────
// View navigation
// ─────────────────────────────────────────────────────────────────────────────

void GovDataProviderPanel::on_view_changed(int index) {
    auto view = static_cast<View>(index);

    orgs_btn_->setChecked(view == Orgs);
    datasets_btn_->setChecked(view == Datasets);
    search_btn_->setChecked(view == Search);

    if (view == Orgs) {
        if (current_orgs_.isEmpty()) {
            load_initial_data();
        } else {
            content_stack_->setCurrentIndex(Orgs);
        }
        current_view_ = Orgs;
    } else if (view == Search) {
        current_view_ = Search;
        search_input_->setFocus();
    }

    update_toolbar_state();
}

void GovDataProviderPanel::on_org_clicked(int row) {
    auto* item = orgs_table_->item(row, 0);
    if (!item) return;
    selected_org_ = item->data(Qt::UserRole).toString();

    show_loading("Loading datasets...");
    services::GovDataService::instance().execute(
        script_, "datasets", {selected_org_, "100"}, "gov_datasets_" + script_);
}

void GovDataProviderPanel::on_dataset_clicked(int row) {
    auto* item = datasets_table_->item(row, 0);
    if (!item) return;
    selected_dataset_ = item->data(Qt::UserRole).toString();

    show_loading("Loading resources...");
    services::GovDataService::instance().execute(
        script_, "resources", {selected_dataset_}, "gov_resources_" + script_);
}

void GovDataProviderPanel::on_search() {
    QString query = search_input_->text().trimmed();
    if (query.isEmpty()) return;

    show_loading("Searching...");
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
}

void GovDataProviderPanel::update_toolbar_state() {
    back_btn_->setVisible(current_view_ == Datasets || current_view_ == Resources);
}

// ─────────────────────────────────────────────────────────────────────────────
// Status helpers
// ─────────────────────────────────────────────────────────────────────────────

void GovDataProviderPanel::show_loading(const QString& message) {
    status_label_->setText(message);
    status_label_->setStyleSheet(
        QString("QLabel { color:%1; font-family:%2; font-size:13px; }")
            .arg(color_, fonts::DATA_FAMILY));
    content_stack_->setCurrentIndex(3);
}

void GovDataProviderPanel::show_error(const QString& message) {
    status_label_->setText("Error: " + message);
    status_label_->setStyleSheet(
        QString("QLabel { color:%1; font-family:%2; font-size:12px; }")
            .arg(colors::RED, fonts::DATA_FAMILY));
    content_stack_->setCurrentIndex(3);
}

void GovDataProviderPanel::show_empty(const QString& message) {
    status_label_->setText(message);
    status_label_->setStyleSheet(
        QString("QLabel { color:%1; font-family:%2; font-size:12px; }")
            .arg(colors::TEXT_SECONDARY, fonts::DATA_FAMILY));
    content_stack_->setCurrentIndex(3);
}

// ─────────────────────────────────────────────────────────────────────────────
// CSV export
// ─────────────────────────────────────────────────────────────────────────────

void GovDataProviderPanel::on_export_csv() {
    QTableWidget* table = nullptr;
    QString default_name;

    if (current_view_ == Orgs && orgs_table_->rowCount() > 0) {
        table = orgs_table_;
        default_name = "gov_" + org_label_.toLower() + ".csv";
    } else if ((current_view_ == Datasets || current_view_ == Search)
               && datasets_table_->rowCount() > 0) {
        table = datasets_table_;
        default_name = "gov_datasets.csv";
    } else if (current_view_ == Resources && resources_table_->rowCount() > 0) {
        table = resources_table_;
        default_name = "gov_resources.csv";
    }

    if (!table) return;

    QString path = QFileDialog::getSaveFileName(this, "Export CSV", default_name,
                                                 "CSV Files (*.csv)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    QTextStream out(&file);
    // Header
    QStringList headers;
    for (int c = 0; c < table->columnCount(); ++c) {
        auto* h = table->horizontalHeaderItem(c);
        headers << (h ? h->text() : QString::number(c));
    }
    out << headers.join(",") << "\n";

    // Rows
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
