// src/screens/geopolitics/ConflictMonitorPanel.cpp
#include "screens/geopolitics/ConflictMonitorPanel.h"

#include "core/logging/Logger.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/WorldMapWidget.h"

#include <QDesktopServices>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QSplitter>
#include <QUrl>

namespace fincept::screens {

using namespace fincept::services::geo;

ConflictMonitorPanel::ConflictMonitorPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void ConflictMonitorPanel::build_ui() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Left: map + table in a vertical splitter
    auto* left_splitter = new QSplitter(Qt::Vertical, this);
    left_splitter->setHandleWidth(2);
    left_splitter->setStyleSheet(QString("QSplitter::handle { background:%1; }").arg(ui::colors::BORDER_DIM()));

    // ── World Map ───────────────────────────────────────────────────────────
    map_widget_ = new fincept::ui::WorldMapWidget(left_splitter);
    map_widget_->setMinimumHeight(160);
    left_splitter->addWidget(map_widget_);

    // ── Events Table ────────────────────────────────────────────────────────
    events_table_ = new QTableWidget(left_splitter);
    events_table_->setColumnCount(7);
    events_table_->setHorizontalHeaderLabels({"Category", "Country", "City", "Keywords", "Date", "Lat", "Lng"});
    events_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    events_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    events_table_->setAlternatingRowColors(true);
    events_table_->horizontalHeader()->setStretchLastSection(false);
    events_table_->verticalHeader()->setVisible(false);
    events_table_->setSortingEnabled(true);

    events_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    events_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    events_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    events_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    events_table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    events_table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    events_table_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);

    events_table_->setColumnHidden(5, true);
    events_table_->setColumnHidden(6, true);
    events_table_->verticalHeader()->setDefaultSectionSize(26);

    events_table_->setStyleSheet(QString("QTableWidget { background:%1; color:%2; gridline-color:%3;"
                                         "font-family:%4; font-size:%5px; border:none; }"
                                         "QTableWidget::item { padding:3px 8px; }"
                                         "QTableWidget::item:selected { background:rgba(255,0,0,0.15); }"
                                         "QHeaderView::section { background:%6; color:%7; font-weight:700;"
                                         "padding:5px 8px; border:1px solid %3; font-family:%4; font-size:%5px; }"
                                         "QTableWidget::item:alternate { background:%8; }")
                                     .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM())
                                     .arg(ui::fonts::DATA_FAMILY)
                                     .arg(ui::fonts::SMALL)
                                     .arg(ui::colors::BG_RAISED())
                                     .arg(ui::colors::TEXT_SECONDARY())
                                     .arg(ui::colors::ROW_ALT()));

    connect(events_table_, &QTableWidget::currentCellChanged, this, [this](int row, int, int, int) {
        if (row < 0 || !detail_panel_)
            return;

        auto get_text = [this, row](int col) -> QString {
            return events_table_->item(row, col) ? events_table_->item(row, col)->text() : QString();
        };

        detail_category_->setText(get_text(0).toUpper());
        detail_country_->setText(get_text(1));
        detail_city_->setText(get_text(2));
        detail_keywords_->setText(get_text(3));
        detail_date_->setText(get_text(4));

        auto cat_color = category_color(get_text(0));
        detail_category_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;")
                                            .arg(cat_color.name())
                                            .arg(ui::fonts::SMALL)
                                            .arg(ui::fonts::DATA_FAMILY));

        detail_panel_->setVisible(true);
    });
    left_splitter->addWidget(events_table_);

    left_splitter->setStretchFactor(0, 4);
    left_splitter->setStretchFactor(1, 6);
    root->addWidget(left_splitter, 1);

    // Right sidebar: stats + selected detail
    auto* sidebar = new QWidget(this);
    sidebar->setFixedWidth(240);
    sidebar->setStyleSheet(
        QString("background:%1; border-left:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    auto* svl = new QVBoxLayout(sidebar);
    svl->setContentsMargins(12, 12, 12, 12);
    svl->setSpacing(8);

    auto* stats_title = new QLabel("TOP CATEGORIES", sidebar);
    stats_title->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;"
                                       "letter-spacing:1px; padding-bottom:6px; border-bottom:1px solid %4;")
                                   .arg(ui::colors::NEGATIVE())
                                   .arg(ui::fonts::TINY)
                                   .arg(ui::fonts::DATA_FAMILY())
                                   .arg(ui::colors::BORDER_DIM()));
    svl->addWidget(stats_title);

    auto* stats_container = new QWidget(sidebar);
    stats_layout_ = new QVBoxLayout(stats_container);
    stats_layout_->setContentsMargins(0, 0, 0, 0);
    stats_layout_->setSpacing(3);
    svl->addWidget(stats_container);

    stats_label_ = new QLabel("No data", sidebar);
    stats_label_->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                    .arg(ui::colors::TEXT_TERTIARY())
                                    .arg(ui::fonts::SMALL)
                                    .arg(ui::fonts::DATA_FAMILY));
    svl->addWidget(stats_label_);

    svl->addSpacing(8);

    // Selected event detail panel
    detail_panel_ = new QWidget(sidebar);
    detail_panel_->setVisible(false);
    {
        QColor neg(ui::colors::NEGATIVE());
        auto neg_rgb = QString("%1,%2,%3").arg(neg.red()).arg(neg.green()).arg(neg.blue());
        detail_panel_->setStyleSheet(QString("background:rgba(%1,0.04); border:1px solid rgba(%1,0.2);"
                                             "border-left:3px solid %2;")
                                         .arg(neg_rgb)
                                         .arg(ui::colors::NEGATIVE()));
    }

    auto* dvl = new QVBoxLayout(detail_panel_);
    dvl->setContentsMargins(10, 10, 10, 10);
    dvl->setSpacing(6);

    auto* detail_title = new QLabel("SELECTED EVENT", detail_panel_);
    detail_title->setStyleSheet(
        QString("color:%1; font-size:%2px; font-weight:700; font-family:%3; letter-spacing:1px;")
            .arg(ui::colors::NEGATIVE())
            .arg(ui::fonts::TINY)
            .arg(ui::fonts::DATA_FAMILY()));
    dvl->addWidget(detail_title);

    auto* grid_widget = new QWidget(detail_panel_);
    auto* grid = new QGridLayout(grid_widget);
    grid->setContentsMargins(0, 4, 0, 0);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(4);
    grid->setColumnStretch(1, 1);

    auto field_label_style = QString("color:%1; font-size:%2px; font-family:%3;")
                                 .arg(ui::colors::TEXT_TERTIARY())
                                 .arg(ui::fonts::TINY)
                                 .arg(ui::fonts::DATA_FAMILY);
    auto value_style = QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;")
                           .arg(ui::colors::TEXT_PRIMARY())
                           .arg(ui::fonts::SMALL)
                           .arg(ui::fonts::DATA_FAMILY);

    struct FieldDef {
        const char* label;
        QLabel** target;
    };
    FieldDef fields[] = {
        {"CATEGORY", &detail_category_}, {"COUNTRY", &detail_country_}, {"CITY", &detail_city_},
        {"KEYWORDS", &detail_keywords_}, {"DATE", &detail_date_},       {"SOURCE", &detail_source_},
    };

    for (int r = 0; r < 6; ++r) {
        auto* fl = new QLabel(fields[r].label, grid_widget);
        fl->setStyleSheet(field_label_style);
        fl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(fl, r, 0);

        *fields[r].target = new QLabel(grid_widget);
        (*fields[r].target)->setStyleSheet(value_style);
        (*fields[r].target)->setWordWrap(true);
        grid->addWidget(*fields[r].target, r, 1);
    }

    dvl->addWidget(grid_widget);
    svl->addWidget(detail_panel_);
    svl->addStretch();

    root->addWidget(sidebar);
}

void ConflictMonitorPanel::set_events(const QVector<NewsEvent>& events) {
    events_table_->setSortingEnabled(false);
    events_table_->setRowCount(events.size());

    for (int i = 0; i < events.size(); ++i) {
        const auto& ev = events[i];

        auto* cat_item = new QTableWidgetItem(ev.event_category);
        cat_item->setForeground(category_color(ev.event_category));
        events_table_->setItem(i, 0, cat_item);

        events_table_->setItem(i, 1, new QTableWidgetItem(ev.country));
        events_table_->setItem(i, 2, new QTableWidgetItem(ev.city));
        events_table_->setItem(i, 3, new QTableWidgetItem(ev.matched_keywords));
        events_table_->setItem(i, 4, new QTableWidgetItem(ev.extracted_date));

        auto* lat_item = new QTableWidgetItem(QString::number(ev.latitude, 'f', 4));
        lat_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        events_table_->setItem(i, 5, lat_item);

        auto* lng_item = new QTableWidgetItem(QString::number(ev.longitude, 'f', 4));
        lng_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        events_table_->setItem(i, 6, lng_item);
    }

    events_table_->setSortingEnabled(true);
    update_stats(events);
    update_map(events);
}

void ConflictMonitorPanel::update_map(const QVector<NewsEvent>& events) {
    QVector<fincept::ui::MapPin> pins;
    pins.reserve(events.size());

    QHash<QString, int> coord_counts;
    auto* rng = QRandomGenerator::global();

    int skipped = 0;
    for (const auto& ev : events) {
        if (ev.latitude == 0.0 && ev.longitude == 0.0) {
            ++skipped;
            continue;
        }

        double lat = ev.latitude;
        double lng = ev.longitude;

        QString key = QString("%1,%2").arg(lat, 0, 'f', 4).arg(lng, 0, 'f', 4);
        int count = coord_counts.value(key, 0);
        coord_counts[key] = count + 1;
        if (count > 0) {
            double angle = (count * 137.508) * 3.14159265 / 180.0;
            double dist = 0.02 + 0.005 * count;
            lat += dist * std::cos(angle);
            lng += dist * std::sin(angle);
        }

        pins.append({lat, lng, QString("%1 — %2, %3").arg(ev.event_category, ev.city, ev.country),
                     category_color(ev.event_category), 5.0});
    }

    LOG_INFO("Geopolitics", QString("Map: %1 events → %2 pins (%3 skipped, %4 unique coords)")
                                .arg(events.size())
                                .arg(pins.size())
                                .arg(skipped)
                                .arg(coord_counts.size()));

    map_widget_->set_pins(pins);
    map_widget_->fit_to_pins();
}

void ConflictMonitorPanel::update_stats(const QVector<NewsEvent>& events) {
    while (stats_layout_->count() > 0) {
        auto* item = stats_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    QHash<QString, int> counts;
    for (const auto& ev : events)
        counts[ev.event_category]++;

    QVector<QPair<QString, int>> sorted;
    for (auto it = counts.begin(); it != counts.end(); ++it)
        sorted.append({it.key(), it.value()});
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

    for (int i = 0; i < qMin(sorted.size(), 10); ++i) {
        const auto& [cat, count] = sorted[i];
        const QColor cat_col = category_color(cat);
        const QString col_hex = cat_col.name();
        const QString col_rgb = QString("%1,%2,%3").arg(cat_col.red()).arg(cat_col.green()).arg(cat_col.blue());

        auto* row = new QWidget(this);
        row->setStyleSheet(QString("border-left:2px solid %1; padding-left:4px;"
                                   "background:rgba(%2,0.04);")
                               .arg(col_hex)
                               .arg(col_rgb));
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(6, 2, 4, 2);
        rl->setSpacing(6);

        auto* name = new QLabel(cat, row);
        name->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; border:none; background:transparent;")
                                .arg(ui::colors::TEXT_SECONDARY())
                                .arg(ui::fonts::SMALL)
                                .arg(ui::fonts::DATA_FAMILY));
        rl->addWidget(name, 1);

        auto* cnt = new QLabel(QString::number(count), row);
        cnt->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;"
                                   "border:none; background:transparent;")
                               .arg(ui::colors::TEXT_PRIMARY())
                               .arg(ui::fonts::SMALL)
                               .arg(ui::fonts::DATA_FAMILY));
        rl->addWidget(cnt);

        stats_layout_->addWidget(row);
    }

    stats_label_->setText(QString("%1 events loaded").arg(events.size()));
}

} // namespace fincept::screens
