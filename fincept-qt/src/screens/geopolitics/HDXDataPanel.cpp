// src/screens/geopolitics/HDXDataPanel.cpp
#include "screens/geopolitics/HDXDataPanel.h"

#include "services/geopolitics/GeopoliticsService.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QScrollArea>

namespace fincept::screens {

using namespace fincept::services::geo;

HDXDataPanel::HDXDataPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
    connect_service();
}

void HDXDataPanel::connect_service() {
    auto& svc = GeopoliticsService::instance();
    connect(&svc, &GeopoliticsService::hdx_results_loaded, this, &HDXDataPanel::on_hdx_results);
}

void HDXDataPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Header with view tabs
    auto* header = new QWidget(this);
    header->setFixedHeight(40);
    header->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));
    auto* hhl = new QHBoxLayout(header);
    hhl->setContentsMargins(12, 0, 12, 0);
    hhl->setSpacing(8);

    auto* title = new QLabel("HDX HUMANITARIAN DATA", header);
    title->setStyleSheet(QString("color:#00E5FF; font-size:%1px; font-weight:700; font-family:%2; letter-spacing:1px;")
                             .arg(ui::fonts::TINY)
                             .arg(ui::fonts::DATA_FAMILY));
    hhl->addWidget(title);

    auto* div = new QWidget(header);
    div->setFixedSize(1, 16);
    div->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM));
    hhl->addWidget(div);

    QStringList views = {"Conflicts", "Humanitarian", "Explorer", "Datasets"};
    for (int i = 0; i < views.size(); ++i) {
        auto* btn = new QPushButton(views[i].toUpper(), header);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString("QPushButton { color:%1; font-size:9px; font-family:%2;"
                                   "padding:4px 8px; border:none; background:transparent; }"
                                   "QPushButton:hover { color:#00E5FF; }")
                               .arg(ui::colors::TEXT_TERTIARY)
                               .arg(ui::fonts::DATA_FAMILY));
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_view_changed(i); });
        hhl->addWidget(btn);
        view_buttons_.append(btn);
    }

    hhl->addStretch();

    // Search
    search_edit_ = new QLineEdit(header);
    search_edit_->setPlaceholderText("Search HDX datasets...");
    search_edit_->setFixedWidth(250);
    search_edit_->setStyleSheet(QString("QLineEdit { background:%1; color:%2; border:1px solid %3;"
                                        "font-family:%4; font-size:%5px; padding:4px 8px; border-radius:2px; }"
                                        "QLineEdit:focus { border-color:#00E5FF; }")
                                    .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                                    .arg(ui::fonts::DATA_FAMILY)
                                    .arg(ui::fonts::SMALL));
    connect(search_edit_, &QLineEdit::returnPressed, this, [this]() {
        auto q = search_edit_->text().trimmed();
        if (!q.isEmpty())
            GeopoliticsService::instance().search_hdx_advanced(q);
    });
    hhl->addWidget(search_edit_);

    dataset_count_ = new QLabel("0 datasets", header);
    dataset_count_->setStyleSheet(
        QString("color:%1; font-size:9px; font-family:%2;").arg(ui::colors::TEXT_TERTIARY).arg(ui::fonts::DATA_FAMILY));
    hhl->addWidget(dataset_count_);

    root->addWidget(header);

    // Content: main table (shared across views)
    datasets_table_ = new QTableWidget(this);
    datasets_table_->setColumnCount(5);
    datasets_table_->setHorizontalHeaderLabels({"Title", "Organization", "Date", "Resources", "Tags"});
    datasets_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    datasets_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    datasets_table_->setAlternatingRowColors(true);
    datasets_table_->horizontalHeader()->setStretchLastSection(true);
    datasets_table_->verticalHeader()->setVisible(false);
    datasets_table_->setSortingEnabled(true);
    datasets_table_->setStyleSheet(QString("QTableWidget { background:%1; color:%2; gridline-color:%3;"
                                           "font-family:%4; font-size:%5px; border:none; }"
                                           "QTableWidget::item { padding:4px 8px; }"
                                           "QTableWidget::item:selected { background:rgba(0,229,255,0.15); }"
                                           "QHeaderView::section { background:%6; color:%7; font-weight:700;"
                                           "padding:6px 8px; border:1px solid %3; font-family:%4; font-size:%5px; }"
                                           "QTableWidget::item:alternate { background:%8; }")
                                       .arg(ui::colors::BG_SURFACE, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM)
                                       .arg(ui::fonts::DATA_FAMILY)
                                       .arg(ui::fonts::SMALL)
                                       .arg(ui::colors::BG_RAISED)
                                       .arg(ui::colors::TEXT_SECONDARY)
                                       .arg(ui::colors::ROW_ALT));
    root->addWidget(datasets_table_, 1);

    // Explorer filter bar (shown at bottom when Explorer view active)
    auto* explorer_bar = new QWidget(this);
    explorer_bar->setFixedHeight(40);
    explorer_bar->setStyleSheet(
        QString("background:%1; border-top:1px solid %2;").arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));
    auto* ehl = new QHBoxLayout(explorer_bar);
    ehl->setContentsMargins(12, 0, 12, 0);
    ehl->setSpacing(8);

    auto combo_style = QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:4px 6px; }")
                           .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                           .arg(ui::fonts::DATA_FAMILY)
                           .arg(ui::fonts::SMALL);

    auto* country_lbl = new QLabel("Country:", explorer_bar);
    country_lbl->setStyleSheet(
        QString("color:%1; font-size:9px; font-family:%2;").arg(ui::colors::TEXT_TERTIARY).arg(ui::fonts::DATA_FAMILY));
    ehl->addWidget(country_lbl);
    country_combo_ = new QComboBox(explorer_bar);
    country_combo_->setStyleSheet(combo_style);
    country_combo_->setEditable(true);
    country_combo_->setPlaceholderText("Select country");
    for (const auto& r : critical_regions())
        country_combo_->addItem(r);
    ehl->addWidget(country_combo_);

    auto* topic_lbl = new QLabel("Topic:", explorer_bar);
    topic_lbl->setStyleSheet(country_lbl->styleSheet());
    ehl->addWidget(topic_lbl);
    topic_combo_ = new QComboBox(explorer_bar);
    topic_combo_->setStyleSheet(combo_style);
    topic_combo_->addItems(
        {"conflict", "humanitarian", "displacement", "food security", "health", "education", "refugees"});
    ehl->addWidget(topic_combo_);

    auto* explore_btn = new QPushButton("SEARCH", explorer_bar);
    explore_btn->setCursor(Qt::PointingHandCursor);
    explore_btn->setStyleSheet(QString("QPushButton { background:#00E5FF; color:%1; font-family:%2;"
                                       "font-size:%3px; font-weight:700; border:none; padding:4px 16px;"
                                       "border-radius:2px; }"
                                       "QPushButton:hover { background:#00B8D4; }")
                                   .arg(ui::colors::BG_BASE)
                                   .arg(ui::fonts::DATA_FAMILY)
                                   .arg(ui::fonts::SMALL));
    connect(explore_btn, &QPushButton::clicked, this, [this]() {
        auto country = country_combo_->currentText().trimmed();
        if (!country.isEmpty())
            GeopoliticsService::instance().search_hdx_by_country(country);
        else {
            auto topic = topic_combo_->currentText().trimmed();
            if (!topic.isEmpty())
                GeopoliticsService::instance().search_hdx_by_topic(topic);
        }
    });
    ehl->addWidget(explore_btn);
    ehl->addStretch();

    root->addWidget(explorer_bar);
    on_view_changed(0);
}

void HDXDataPanel::on_view_changed(int index) {
    active_view_ = index;
    for (int i = 0; i < view_buttons_.size(); ++i) {
        bool active = (i == index);
        view_buttons_[i]->setStyleSheet(QString("QPushButton { color:%1; font-size:9px; font-family:%2;"
                                                "padding:4px 8px; border:none; background:%3; font-weight:%4; }"
                                                "QPushButton:hover { color:#00E5FF; }")
                                            .arg(active ? "#00E5FF" : ui::colors::TEXT_TERTIARY)
                                            .arg(ui::fonts::DATA_FAMILY)
                                            .arg(active ? "rgba(0,229,255,0.1)" : "transparent")
                                            .arg(active ? "700" : "400"));
    }

    // Load data for the selected view
    auto& svc = GeopoliticsService::instance();
    switch (index) {
        case 0:
            svc.search_hdx_conflicts();
            break;
        case 1:
            svc.search_hdx_humanitarian();
            break;
        case 2: /* Explorer — user-driven */
            break;
        case 3: /* Datasets — search-driven */
            break;
    }
}

void HDXDataPanel::on_hdx_results(const QString& context, QVector<HDXDataset> datasets) {
    populate_table(datasets);
}

void HDXDataPanel::populate_table(const QVector<HDXDataset>& datasets) {
    datasets_table_->setSortingEnabled(false);
    datasets_table_->setRowCount(datasets.size());

    for (int i = 0; i < datasets.size(); ++i) {
        const auto& d = datasets[i];
        auto* title_item = new QTableWidgetItem(d.title);
        title_item->setToolTip(d.notes);
        datasets_table_->setItem(i, 0, title_item);
        datasets_table_->setItem(i, 1, new QTableWidgetItem(d.organization));
        datasets_table_->setItem(i, 2, new QTableWidgetItem(d.date));

        auto* res_item = new QTableWidgetItem(QString::number(d.num_resources));
        res_item->setTextAlignment(Qt::AlignCenter);
        datasets_table_->setItem(i, 3, res_item);

        datasets_table_->setItem(i, 4, new QTableWidgetItem(d.tags.join(", ")));
    }

    datasets_table_->setSortingEnabled(true);
    datasets_table_->resizeColumnsToContents();
    dataset_count_->setText(QString("%1 datasets").arg(datasets.size()));
}

} // namespace fincept::screens
