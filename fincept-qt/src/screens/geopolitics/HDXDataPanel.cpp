// src/screens/geopolitics/HDXDataPanel.cpp
#include "screens/geopolitics/HDXDataPanel.h"

#include "services/geopolitics/GeopoliticsService.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QScrollArea>

namespace fincept::screens {

using namespace fincept::services::geo;

// Helper: get cyan accent RGB string for rgba() usage
static QString cyan_rgb() {
    QColor c(ui::colors::CYAN());
    return QString("%1,%2,%3").arg(c.red()).arg(c.green()).arg(c.blue());
}

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
    header->setFixedHeight(48);
    header->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* hhl = new QHBoxLayout(header);
    hhl->setContentsMargins(16, 0, 16, 0);
    hhl->setSpacing(8);

    auto* title = new QLabel("HDX HUMANITARIAN DATA", header);
    title->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3; letter-spacing:1px;")
                             .arg(ui::colors::CYAN())
                             .arg(ui::fonts::TINY)
                             .arg(ui::fonts::DATA_FAMILY()));
    hhl->addWidget(title);

    auto* div = new QWidget(header);
    div->setFixedSize(1, 20);
    div->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM()));
    hhl->addWidget(div);

    auto rgb = cyan_rgb();
    const QStringList views = {"Conflicts", "Humanitarian", "Explorer", "Datasets"};
    for (int i = 0; i < views.size(); ++i) {
        auto* btn = new QPushButton(views[i].toUpper(), header);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString("QPushButton { color:%1; font-size:%2px; font-family:%3;"
                                   "padding:4px 12px; border:none; background:transparent;"
                                   "font-weight:400; }"
                                   "QPushButton:hover { color:%4; background:rgba(%5,0.04); }")
                               .arg(ui::colors::TEXT_TERTIARY())
                               .arg(ui::fonts::TINY)
                               .arg(ui::fonts::DATA_FAMILY)
                               .arg(ui::colors::CYAN())
                               .arg(rgb));
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_view_changed(i); });
        hhl->addWidget(btn);
        view_buttons_.append(btn);
    }

    hhl->addStretch();

    search_edit_ = new QLineEdit(header);
    search_edit_->setPlaceholderText("Search HDX datasets...");
    search_edit_->setFixedWidth(240);
    search_edit_->setStyleSheet(QString("QLineEdit { background:%1; color:%2; border:1px solid %3;"
                                        "font-family:%4; font-size:%5px; padding:4px 8px; }"
                                        "QLineEdit:focus { border-color:%6; }")
                                    .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                                    .arg(ui::fonts::DATA_FAMILY)
                                    .arg(ui::fonts::SMALL)
                                    .arg(ui::colors::CYAN()));
    connect(search_edit_, &QLineEdit::returnPressed, this, [this]() {
        auto q = search_edit_->text().trimmed();
        if (!q.isEmpty()) {
            show_loading(true);
            GeopoliticsService::instance().search_hdx_advanced(q);
        }
    });
    hhl->addWidget(search_edit_);

    dataset_count_ = new QLabel("0 datasets", header);
    dataset_count_->setFixedHeight(22);
    dataset_count_->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; padding:2px 6px;"
                                          "background:rgba(%4,0.08); border:1px solid rgba(%4,0.25); font-weight:700;")
                                      .arg(ui::colors::CYAN())
                                      .arg(ui::fonts::TINY)
                                      .arg(ui::fonts::DATA_FAMILY())
                                      .arg(rgb));
    hhl->addWidget(dataset_count_);

    root->addWidget(header);

    // Datasets table
    datasets_table_ = new QTableWidget(this);
    datasets_table_->setColumnCount(5);
    datasets_table_->setHorizontalHeaderLabels({"Title", "Organization", "Date", "Resources", "Tags"});
    datasets_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    datasets_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    datasets_table_->setAlternatingRowColors(true);
    datasets_table_->verticalHeader()->setVisible(false);
    datasets_table_->setSortingEnabled(true);

    datasets_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    datasets_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    datasets_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    datasets_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    datasets_table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    datasets_table_->verticalHeader()->setDefaultSectionSize(26);

    datasets_table_->setStyleSheet(QString("QTableWidget { background:%1; color:%2; gridline-color:%3;"
                                           "font-family:%4; font-size:%5px; border:none; }"
                                           "QTableWidget::item { padding:3px 8px; }"
                                           "QTableWidget::item:selected { background:rgba(%6,0.15); }"
                                           "QHeaderView::section { background:%7; color:%8; font-weight:700;"
                                           "padding:5px 8px; border:1px solid %3; font-family:%4; font-size:%5px; }"
                                           "QTableWidget::item:alternate { background:%9; }")
                                       .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM())
                                       .arg(ui::fonts::DATA_FAMILY)
                                       .arg(ui::fonts::SMALL)
                                       .arg(rgb)
                                       .arg(ui::colors::BG_RAISED())
                                       .arg(ui::colors::TEXT_SECONDARY())
                                       .arg(ui::colors::ROW_ALT()));

    // Loading overlay
    loading_label_ = new QLabel("Loading HDX data...", this);
    loading_label_->setAlignment(Qt::AlignCenter);
    loading_label_->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; background:%4;")
                                      .arg(ui::colors::CYAN())
                                      .arg(ui::fonts::DATA)
                                      .arg(ui::fonts::DATA_FAMILY)
                                      .arg(ui::colors::BG_SURFACE()));
    loading_label_->hide();
    root->addWidget(loading_label_, 1);
    root->addWidget(datasets_table_, 1);

    // Explorer filter bar
    explorer_bar_ = new QWidget(this);
    explorer_bar_->setFixedHeight(44);
    explorer_bar_->setStyleSheet(
        QString("background:%1; border-top:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* ehl = new QHBoxLayout(explorer_bar_);
    ehl->setContentsMargins(16, 0, 16, 0);
    ehl->setSpacing(8);

    auto combo_style = QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:4px 6px; }")
                           .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                           .arg(ui::fonts::DATA_FAMILY)
                           .arg(ui::fonts::SMALL);

    auto bar_label_style = QString("color:%1; font-size:%2px; font-family:%3; font-weight:700; letter-spacing:1px;")
                               .arg(ui::colors::TEXT_TERTIARY())
                               .arg(ui::fonts::TINY)
                               .arg(ui::fonts::DATA_FAMILY);

    auto* country_lbl = new QLabel("COUNTRY:", explorer_bar_);
    country_lbl->setStyleSheet(bar_label_style);
    ehl->addWidget(country_lbl);
    country_combo_ = new QComboBox(explorer_bar_);
    country_combo_->setStyleSheet(combo_style);
    country_combo_->setEditable(true);
    country_combo_->setPlaceholderText("Select country");
    for (const auto& r : critical_regions())
        country_combo_->addItem(r);
    ehl->addWidget(country_combo_);

    auto* topic_lbl = new QLabel("TOPIC:", explorer_bar_);
    topic_lbl->setStyleSheet(bar_label_style);
    ehl->addWidget(topic_lbl);
    topic_combo_ = new QComboBox(explorer_bar_);
    topic_combo_->setStyleSheet(combo_style);
    topic_combo_->addItems(
        {"conflict", "humanitarian", "displacement", "food security", "health", "education", "refugees"});
    ehl->addWidget(topic_combo_);

    auto* explore_btn = new QPushButton("SEARCH", explorer_bar_);
    explore_btn->setCursor(Qt::PointingHandCursor);
    {
        QColor cy(ui::colors::CYAN());
        explore_btn->setStyleSheet(QString("QPushButton { background:%1; color:%2; font-family:%3;"
                                           "font-size:%4px; font-weight:700; border:none; padding:6px 16px; }"
                                           "QPushButton:hover { background:%5; }")
                                       .arg(ui::colors::CYAN())
                                       .arg(ui::colors::BG_BASE())
                                       .arg(ui::fonts::DATA_FAMILY())
                                       .arg(ui::fonts::SMALL)
                                       .arg(cy.darker(120).name()));
    }
    connect(explore_btn, &QPushButton::clicked, this, [this]() {
        auto country = country_combo_->currentText().trimmed();
        show_loading(true);
        if (!country.isEmpty()) {
            GeopoliticsService::instance().search_hdx_by_country(country);
        } else {
            auto topic = topic_combo_->currentText().trimmed();
            if (!topic.isEmpty())
                GeopoliticsService::instance().search_hdx_by_topic(topic);
            else
                show_loading(false);
        }
    });
    ehl->addWidget(explore_btn);
    ehl->addStretch();

    root->addWidget(explorer_bar_);
    on_view_changed(0);
}

void HDXDataPanel::on_view_changed(int index) {
    active_view_ = index;
    auto rgb = cyan_rgb();
    for (int i = 0; i < view_buttons_.size(); ++i) {
        const bool active = (i == index);
        if (active) {
            view_buttons_[i]->setStyleSheet(
                QString("QPushButton { color:%1; font-size:%2px; font-family:%3;"
                        "padding:4px 12px;"
                        "border-bottom:2px solid %1; border-top:none; border-left:none; border-right:none;"
                        "background:rgba(%4,0.06); font-weight:700; }"
                        "QPushButton:hover { background:rgba(%4,0.10); }")
                    .arg(ui::colors::CYAN())
                    .arg(ui::fonts::TINY)
                    .arg(ui::fonts::DATA_FAMILY)
                    .arg(rgb));
        } else {
            view_buttons_[i]->setStyleSheet(QString("QPushButton { color:%1; font-size:%2px; font-family:%3;"
                                                    "padding:4px 12px; border:none; background:transparent;"
                                                    "font-weight:400; }"
                                                    "QPushButton:hover { color:%4; background:rgba(%5,0.04); }")
                                                .arg(ui::colors::TEXT_TERTIARY())
                                                .arg(ui::fonts::TINY)
                                                .arg(ui::fonts::DATA_FAMILY)
                                                .arg(ui::colors::CYAN())
                                                .arg(rgb));
        }
    }

    if (explorer_bar_)
        explorer_bar_->setVisible(index == 2);

    auto& svc = GeopoliticsService::instance();
    switch (index) {
        case 0:
            if (!cache_conflicts_.isEmpty())
                populate_table(cache_conflicts_);
            else {
                show_loading(true);
                svc.search_hdx_conflicts();
            }
            break;
        case 1:
            if (!cache_humanitarian_.isEmpty())
                populate_table(cache_humanitarian_);
            else {
                show_loading(true);
                svc.search_hdx_humanitarian();
            }
            break;
        case 2:
            break;
        case 3:
            if (!cache_datasets_.isEmpty())
                populate_table(cache_datasets_);
            else {
                show_loading(true);
                svc.search_hdx_advanced("humanitarian crisis conflict displacement");
            }
            break;
    }
}

void HDXDataPanel::show_loading(bool on) {
    loading_label_->setVisible(on);
    datasets_table_->setVisible(!on);
}

void HDXDataPanel::on_hdx_results(const QString& context, QVector<HDXDataset> datasets) {
    if (context == "conflicts")
        cache_conflicts_ = datasets;
    else if (context == "humanitarian")
        cache_humanitarian_ = datasets;
    else if (context == "search" || context == "topic" || context == "country")
        cache_datasets_ = datasets;

    bool relevant = (active_view_ == 0 && context == "conflicts") || (active_view_ == 1 && context == "humanitarian") ||
                    (active_view_ == 2 && (context == "country" || context == "topic")) ||
                    (active_view_ == 3 && context == "search");
    if (relevant)
        populate_table(datasets);
}

void HDXDataPanel::populate_table(const QVector<HDXDataset>& datasets) {
    show_loading(false);
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
    dataset_count_->setText(QString("%1 datasets").arg(datasets.size()));
}

} // namespace fincept::screens
