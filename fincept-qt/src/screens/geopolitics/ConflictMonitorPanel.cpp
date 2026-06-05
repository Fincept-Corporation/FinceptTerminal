// src/screens/geopolitics/ConflictMonitorPanel.cpp
#include "screens/geopolitics/ConflictMonitorPanel.h"

#include "core/logging/Logger.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/WorldMapWidget.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QSet>
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
    // Wrap the map in a container with a slim toolbar carrying the BASEMAP
    // selector, then add the container (not the bare map) to the splitter.
    auto* map_container = new QWidget(left_splitter);
    auto* mc_vl = new QVBoxLayout(map_container);
    mc_vl->setContentsMargins(0, 0, 0, 0);
    mc_vl->setSpacing(0);

    auto* map_toolbar = new QWidget(map_container);
    map_toolbar->setObjectName("gpMapToolbar");
    map_toolbar->setFixedHeight(32);
    map_toolbar->setStyleSheet(QString("#gpMapToolbar { background:%1; border-bottom:1px solid %2; }")
                                   .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_MED()));
    auto* mt_hl = new QHBoxLayout(map_toolbar);
    mt_hl->setContentsMargins(10, 0, 10, 0);
    mt_hl->setSpacing(8);

    auto* map_accent = new QLabel(map_toolbar);
    map_accent->setFixedSize(3, 12);
    map_accent->setStyleSheet(QString("background:%1; border-radius:1px;").arg(ui::colors::AMBER()));
    mt_hl->addWidget(map_accent);

    auto* map_title = new QLabel(tr("CONFLICT MAP"), map_toolbar);
    map_title->setStyleSheet(QString("color:%1; font-size:11px; font-weight:700; font-family:%2; letter-spacing:1px;")
                                 .arg(ui::colors::AMBER())
                                 .arg(ui::fonts::DATA_FAMILY));
    mt_hl->addWidget(map_title);
    mt_hl->addStretch();

    auto* basemap_cap = new QLabel(tr("BASEMAP"), map_toolbar);
    basemap_cap->setStyleSheet(QString("color:%1; font-size:8px; font-weight:700; font-family:%2; letter-spacing:1px;")
                                   .arg(ui::colors::TEXT_TERTIARY())
                                   .arg(ui::fonts::DATA_FAMILY));
    mt_hl->addWidget(basemap_cap);

    map_type_combo_ = new QComboBox(map_toolbar);
    map_type_combo_->setCursor(Qt::PointingHandCursor);
    map_type_combo_->setFixedWidth(132);
    map_type_combo_->setStyleSheet(
        QString("QComboBox { background:%1; color:%2; border:1px solid %3; border-radius:2px;"
                "font-family:%6; font-size:%7px; font-weight:700; padding:3px 8px; }"
                "QComboBox:hover, QComboBox:focus { border-color:%4; }"
                "QComboBox::drop-down { border:none; width:16px; }"
                "QComboBox::down-arrow { image:none; width:0; height:0;"
                "border-left:3px solid transparent; border-right:3px solid transparent;"
                "border-top:4px solid %4; margin-right:6px; }"
                "QComboBox QAbstractItemView { background:%1; color:%2; border:1px solid %3;"
                "selection-background-color:%5; selection-color:%4; outline:0;"
                "font-family:%6; font-size:%7px; }")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED(),
                 ui::colors::AMBER(), ui::colors::BG_HOVER())
            .arg(ui::fonts::DATA_FAMILY)
            .arg(ui::fonts::SMALL));
    mt_hl->addWidget(map_type_combo_);

    mc_vl->addWidget(map_toolbar);

    map_widget_ = new fincept::ui::WorldMapWidget(map_container);
    map_widget_->setMinimumHeight(280);
    mc_vl->addWidget(map_widget_, 1);

    left_splitter->addWidget(map_container);

    // Default geopolitics to the DARK political basemap (index 1) — preserves
    // the screen's prior look; the selector lets the user pick satellite, etc.
    map_type_combo_->addItems(fincept::ui::WorldMapWidget::basemap_labels());
    map_type_combo_->setCurrentIndex(1);  // 1 = DARK in basemap_labels()
    map_widget_->set_basemap(1);
    connect(map_type_combo_, &QComboBox::currentIndexChanged, this, [this](int idx) {
        if (map_widget_)
            map_widget_->set_basemap(idx);
    });

    // Map pin click → select the matching row in the events table. The row's
    // currentCellChanged handler (wired below) then fills the detail panel,
    // so the side panel and the table stay in sync from a single source.
    connect(map_widget_, &fincept::ui::WorldMapWidget::pin_clicked, this, [this](int event_id) {
        if (!events_table_)
            return;
        for (int r = 0; r < events_table_->rowCount(); ++r) {
            auto* it = events_table_->item(r, 0);
            if (it && it->data(Qt::UserRole + 1).toInt() == event_id) {
                events_table_->setCurrentCell(r, 0);
                events_table_->scrollToItem(it, QAbstractItemView::PositionAtCenter);
                break;
            }
        }
    });

    // ── Events Table ────────────────────────────────────────────────────────
    events_table_ = new QTableWidget(left_splitter);
    events_table_->setColumnCount(7);
    events_table_->setHorizontalHeaderLabels(
        {tr("Category"), tr("Country"), tr("City"), tr("Title"), tr("Date"), tr("Lat"), tr("Lng")});
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

        // Raw category id lives on UserRole of column 0 — needed for the
        // colour swatch (category_color expects "armed_conflict", not the
        // pretty-printed "Armed Conflict").
        auto* cat_cell = events_table_->item(row, 0);
        const QString raw_cat = cat_cell ? cat_cell->data(Qt::UserRole).toString() : QString();

        const QString title = get_text(3);
        if (detail_title_)
            detail_title_->setText(title.isEmpty() ? tr("Untitled event") : title);
        detail_category_->setText(get_text(0).toUpper());
        detail_country_->setText(get_text(1).isEmpty() ? QStringLiteral("—") : get_text(1));
        detail_city_->setText(get_text(2).isEmpty() ? QStringLiteral("—") : get_text(2));
        detail_date_->setText(get_text(4).isEmpty() ? QStringLiteral("—") : get_text(4));

        QString src_name;
        QString src_url;
        if (auto* it = events_table_->item(row, 3)) {
            src_name = it->data(Qt::UserRole).toString();
            src_url = it->data(Qt::UserRole + 1).toString();
        }
        if (detail_source_)
            detail_source_->setText(src_name.isEmpty() ? QStringLiteral("—") : src_name);

        current_url_ = src_url;
        if (detail_open_btn_)
            detail_open_btn_->setVisible(!src_url.isEmpty());

        const auto cat_color = category_color(raw_cat);
        const QString cat_hex = cat_color.name();
        const QString cat_rgb =
            QString("%1,%2,%3").arg(cat_color.red()).arg(cat_color.green()).arg(cat_color.blue());
        detail_category_->setStyleSheet(QString("color:%1; background:rgba(%2,0.12);"
                                                "border:1px solid rgba(%2,0.45);"
                                                "padding:3px 8px; font-size:%3px; font-weight:700;"
                                                "font-family:%4; letter-spacing:1px;")
                                            .arg(cat_hex)
                                            .arg(cat_rgb)
                                            .arg(ui::fonts::TINY)
                                            .arg(ui::fonts::DATA_FAMILY));

        if (empty_state_) empty_state_->setVisible(false);
        detail_panel_->setVisible(true);
    });
    left_splitter->addWidget(events_table_);

    events_table_->setMinimumHeight(180);
    left_splitter->setStretchFactor(0, 5);
    left_splitter->setStretchFactor(1, 5);
    // Initial split — keeps the map at a useful height even before the user
    // drags the handle. Without this, an empty/short table can let the map
    // collapse into a thin strip that paints garbage tile coverage.
    left_splitter->setSizes({360, 360});
    root->addWidget(left_splitter, 1);

    root->addWidget(build_sidebar());
}

// ── Sidebar: scrollable column with overview, top categories, hotspots, event details
QWidget* ConflictMonitorPanel::build_sidebar() {
    auto* sidebar = new QWidget(this);
    sidebar->setFixedWidth(280);
    sidebar->setStyleSheet(
        QString("background:%1; border-left:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    auto* outer = new QVBoxLayout(sidebar);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* scroll = new QScrollArea(sidebar);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QString("QScrollArea { background:transparent; border:none; }"
                                  "QScrollBar:vertical { background:%1; width:6px; }"
                                  "QScrollBar::handle:vertical { background:%2; border-radius:3px; }"
                                  "QScrollBar::add-line, QScrollBar::sub-line { height:0px; }")
                              .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_MED()));

    auto* content = new QWidget(scroll);
    content->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_SURFACE()));
    auto* svl = new QVBoxLayout(content);
    svl->setContentsMargins(14, 14, 14, 14);
    svl->setSpacing(14);

    svl->addWidget(build_overview_section(content));
    svl->addWidget(make_divider(content));
    svl->addWidget(build_top_categories_section(content));
    svl->addWidget(make_divider(content));
    svl->addWidget(build_hotspots_section(content));
    svl->addWidget(make_divider(content));
    svl->addWidget(build_event_details_section(content));
    svl->addStretch(1);

    scroll->setWidget(content);
    outer->addWidget(scroll);
    return sidebar;
}

QWidget* ConflictMonitorPanel::make_section_header(const QString& text, QWidget* parent) {
    auto* lbl = new QLabel(text, parent);
    lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;"
                               "letter-spacing:1.5px;")
                           .arg(ui::colors::NEGATIVE())
                           .arg(ui::fonts::TINY)
                           .arg(ui::fonts::DATA_FAMILY()));
    return lbl;
}

QWidget* ConflictMonitorPanel::make_divider(QWidget* parent) {
    auto* sep = new QWidget(parent);
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM()));
    return sep;
}

QWidget* ConflictMonitorPanel::make_stat_tile(const QString& label, QLabel** value_out, QWidget* parent) {
    auto* tile = new QWidget(parent);
    tile->setStyleSheet(QString("background:%1; border:1px solid %2;")
                            .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* tl = new QVBoxLayout(tile);
    tl->setContentsMargins(8, 6, 8, 6);
    tl->setSpacing(2);

    auto* val = new QLabel("0", tile);
    val->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;"
                               "background:transparent; border:none;")
                           .arg(ui::colors::TEXT_PRIMARY())
                           .arg(ui::fonts::HEADER)
                           .arg(ui::fonts::DATA_FAMILY()));
    val->setAlignment(Qt::AlignCenter);
    tl->addWidget(val);

    auto* lbl = new QLabel(label, tile);
    lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; letter-spacing:1px;"
                               "background:transparent; border:none;")
                           .arg(ui::colors::TEXT_TERTIARY())
                           .arg(ui::fonts::TINY)
                           .arg(ui::fonts::DATA_FAMILY()));
    lbl->setAlignment(Qt::AlignCenter);
    tl->addWidget(lbl);

    *value_out = val;
    return tile;
}

QWidget* ConflictMonitorPanel::build_overview_section(QWidget* parent) {
    auto* w = new QWidget(parent);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(8);

    auto* hdr = make_section_header(tr("OVERVIEW"), w);
    hdr_overview_ = qobject_cast<QLabel*>(hdr);
    vl->addWidget(hdr);

    auto* tiles = new QWidget(w);
    auto* tl = new QHBoxLayout(tiles);
    tl->setContentsMargins(0, 0, 0, 0);
    tl->setSpacing(6);
    auto* tile_events = make_stat_tile(tr("EVENTS"), &stat_total_, tiles);
    auto* tile_mapped = make_stat_tile(tr("MAPPED"), &stat_mapped_, tiles);
    auto* tile_nations = make_stat_tile(tr("NATIONS"), &stat_countries_, tiles);
    // Capture each tile's caption label (last direct-child QLabel) for retranslate.
    {
        auto labels = tile_events->findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly);
        if (!labels.isEmpty()) tile_events_lbl_ = labels.last();
        labels = tile_mapped->findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly);
        if (!labels.isEmpty()) tile_mapped_lbl_ = labels.last();
        labels = tile_nations->findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly);
        if (!labels.isEmpty()) tile_nations_lbl_ = labels.last();
    }
    tl->addWidget(tile_events, 1);
    tl->addWidget(tile_mapped, 1);
    tl->addWidget(tile_nations, 1);
    vl->addWidget(tiles);
    return w;
}

QWidget* ConflictMonitorPanel::build_top_categories_section(QWidget* parent) {
    auto* w = new QWidget(parent);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(6);
    auto* hdr = make_section_header(tr("TOP CATEGORIES"), w);
    hdr_top_categories_ = qobject_cast<QLabel*>(hdr);
    vl->addWidget(hdr);

    auto* container = new QWidget(w);
    stats_layout_ = new QVBoxLayout(container);
    stats_layout_->setContentsMargins(0, 0, 0, 0);
    stats_layout_->setSpacing(6);
    vl->addWidget(container);

    // Transient empty-state — replaced by update_stats() once data arrives, so
    // it is not cached for retranslate (would dangle after the layout is cleared).
    auto* empty = new QLabel(tr("Waiting for events…"), w);
    empty->setObjectName("topcat_empty");
    empty->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; font-style:italic;")
                             .arg(ui::colors::TEXT_TERTIARY())
                             .arg(ui::fonts::SMALL)
                             .arg(ui::fonts::DATA_FAMILY()));
    stats_layout_->addWidget(empty);
    return w;
}

QWidget* ConflictMonitorPanel::build_hotspots_section(QWidget* parent) {
    auto* w = new QWidget(parent);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(6);
    auto* hdr = make_section_header(tr("HOTSPOTS"), w);
    hdr_hotspots_ = qobject_cast<QLabel*>(hdr);
    vl->addWidget(hdr);

    auto* container = new QWidget(w);
    hotspots_layout_ = new QVBoxLayout(container);
    hotspots_layout_->setContentsMargins(0, 0, 0, 0);
    hotspots_layout_->setSpacing(5);
    vl->addWidget(container);

    // Transient empty-state (replaced by update_hotspots) — not cached.
    auto* empty = new QLabel(tr("Waiting for events…"), w);
    empty->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; font-style:italic;")
                             .arg(ui::colors::TEXT_TERTIARY())
                             .arg(ui::fonts::SMALL)
                             .arg(ui::fonts::DATA_FAMILY()));
    hotspots_layout_->addWidget(empty);
    return w;
}

QWidget* ConflictMonitorPanel::build_event_details_section(QWidget* parent) {
    auto* w = new QWidget(parent);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(8);
    auto* hdr = make_section_header(tr("EVENT DETAILS"), w);
    hdr_event_details_ = qobject_cast<QLabel*>(hdr);
    vl->addWidget(hdr);

    // Empty-state placeholder
    empty_state_ = new QLabel(tr("⌖  Select an event from the map or table\n    to inspect details here."), w);
    empty_state_->setWordWrap(true);
    empty_state_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    empty_state_->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;"
                                        "background:%4; border:1px dashed %5;"
                                        "padding:14px 12px;")
                                    .arg(ui::colors::TEXT_TERTIARY())
                                    .arg(ui::fonts::SMALL)
                                    .arg(ui::fonts::DATA_FAMILY())
                                    .arg(ui::colors::BG_RAISED())
                                    .arg(ui::colors::BORDER_DIM()));
    vl->addWidget(empty_state_);

    // Detail panel
    detail_panel_ = new QWidget(w);
    detail_panel_->setVisible(false);
    {
        QColor neg(ui::colors::NEGATIVE());
        auto neg_rgb = QString("%1,%2,%3").arg(neg.red()).arg(neg.green()).arg(neg.blue());
        detail_panel_->setStyleSheet(QString("QWidget#geo_detail_root { background:rgba(%1,0.04);"
                                             "border:1px solid rgba(%1,0.18);"
                                             "border-left:3px solid %2; }")
                                         .arg(neg_rgb)
                                         .arg(ui::colors::NEGATIVE()));
    }
    detail_panel_->setObjectName("geo_detail_root");

    auto* dvl = new QVBoxLayout(detail_panel_);
    dvl->setContentsMargins(12, 12, 12, 12);
    dvl->setSpacing(8);

    // Category badge (colored pill)
    detail_category_ = new QLabel(detail_panel_);
    detail_category_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    detail_category_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    dvl->addWidget(detail_category_, 0, Qt::AlignLeft);

    // Headline title
    detail_title_ = new QLabel(detail_panel_);
    detail_title_->setWordWrap(true);
    detail_title_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;"
                                         "background:transparent; border:none; line-height:1.3;")
                                     .arg(ui::colors::TEXT_PRIMARY())
                                     .arg(ui::fonts::BODY)
                                     .arg(ui::fonts::DATA_FAMILY()));
    dvl->addWidget(detail_title_);

    // Field rows: label (caps) on its own line above value
    auto field_label_style = QString("color:%1; font-size:%2px; font-family:%3;"
                                     "letter-spacing:1px; background:transparent; border:none;")
                                 .arg(ui::colors::TEXT_TERTIARY())
                                 .arg(ui::fonts::TINY)
                                 .arg(ui::fonts::DATA_FAMILY());
    auto value_style = QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;"
                               "background:transparent; border:none;")
                           .arg(ui::colors::TEXT_PRIMARY())
                           .arg(ui::fonts::SMALL)
                           .arg(ui::fonts::DATA_FAMILY());

    auto* fields_grid_w = new QWidget(detail_panel_);
    fields_grid_w->setStyleSheet("background:transparent;");
    auto* fg = new QGridLayout(fields_grid_w);
    fg->setContentsMargins(0, 4, 0, 0);
    fg->setHorizontalSpacing(10);
    fg->setVerticalSpacing(8);
    fg->setColumnStretch(0, 1);
    fg->setColumnStretch(1, 1);

    struct FieldDef {
        QString label;
        QLabel** target;       // value label
        QLabel** label_target; // caption label (cached for retranslate)
        int row;
        int col;
        int colspan;
    };
    FieldDef fields[] = {
        {tr("COUNTRY"), &detail_country_, &field_country_lbl_, 0, 0, 1},
        {tr("CITY"),    &detail_city_,    &field_city_lbl_,    0, 1, 1},
        {tr("DATE"),    &detail_date_,    &field_date_lbl_,    1, 0, 1},
        {tr("SOURCE"),  &detail_source_,  &field_source_lbl_,  1, 1, 1},
    };

    for (const auto& f : fields) {
        auto* cell = new QWidget(fields_grid_w);
        cell->setStyleSheet("background:transparent;");
        auto* cl = new QVBoxLayout(cell);
        cl->setContentsMargins(0, 0, 0, 0);
        cl->setSpacing(2);

        auto* l = new QLabel(f.label, cell);
        l->setStyleSheet(field_label_style);
        cl->addWidget(l);
        if (f.label_target) *f.label_target = l;

        *f.target = new QLabel(QStringLiteral("—"), cell);
        (*f.target)->setStyleSheet(value_style);
        (*f.target)->setWordWrap(true);
        cl->addWidget(*f.target);

        fg->addWidget(cell, f.row, f.col, 1, f.colspan);
    }
    dvl->addWidget(fields_grid_w);

    // Open-source button
    detail_open_btn_ = new QPushButton(tr("OPEN SOURCE  ↗"), detail_panel_);
    detail_open_btn_->setCursor(Qt::PointingHandCursor);
    detail_open_btn_->setVisible(false);
    {
        QColor neg(ui::colors::NEGATIVE());
        detail_open_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%2;"
                                                 "font-family:%3; font-size:%4px; font-weight:700;"
                                                 "border:none; padding:7px 12px; letter-spacing:1px; }"
                                                 "QPushButton:hover { background:%5; }")
                                             .arg(ui::colors::NEGATIVE())
                                             .arg(ui::colors::BG_BASE())
                                             .arg(ui::fonts::DATA_FAMILY())
                                             .arg(ui::fonts::SMALL)
                                             .arg(neg.darker(120).name()));
    }
    connect(detail_open_btn_, &QPushButton::clicked, this, [this]() {
        if (!current_url_.isEmpty())
            QDesktopServices::openUrl(QUrl(current_url_));
    });
    dvl->addWidget(detail_open_btn_);

    vl->addWidget(detail_panel_);
    return w;
}

void ConflictMonitorPanel::set_events(const QVector<NewsEvent>& events) {
    events_table_->setSortingEnabled(false);
    events_table_->setRowCount(events.size());

    for (int i = 0; i < events.size(); ++i) {
        const auto& ev = events[i];

        auto* cat_item = new QTableWidgetItem(pretty_category(ev.event_category));
        cat_item->setForeground(category_color(ev.event_category));
        // UserRole carries the raw category id (used by the detail panel
        // to recover the colour); UserRole+1 carries the original event
        // index so map-pin clicks can resolve back to the correct row even
        // after the user re-sorts.
        cat_item->setData(Qt::UserRole, ev.event_category);
        cat_item->setData(Qt::UserRole + 1, i);
        events_table_->setItem(i, 0, cat_item);

        events_table_->setItem(i, 1, new QTableWidgetItem(ev.country));
        events_table_->setItem(i, 2, new QTableWidgetItem(ev.city));
        auto* title_item = new QTableWidgetItem(ev.title);
        title_item->setData(Qt::UserRole, ev.source);
        title_item->setData(Qt::UserRole + 1, ev.url);
        title_item->setToolTip(ev.title);
        events_table_->setItem(i, 3, title_item);
        events_table_->setItem(i, 4, new QTableWidgetItem(ev.extracted_date));

        auto* lat_item = new QTableWidgetItem(ev.has_coords ? QString::number(ev.latitude, 'f', 4) : QString());
        lat_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        events_table_->setItem(i, 5, lat_item);

        auto* lng_item = new QTableWidgetItem(ev.has_coords ? QString::number(ev.longitude, 'f', 4) : QString());
        lng_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        events_table_->setItem(i, 6, lng_item);
    }

    events_table_->setSortingEnabled(true);
    update_stats(events);
    update_hotspots(events);
    update_map(events);

    // Overview tiles
    if (stat_total_) stat_total_->setText(QString::number(events.size()));
    int mapped = 0;
    QSet<QString> countries;
    for (const auto& ev : events) {
        if (ev.has_coords) ++mapped;
        if (!ev.country.isEmpty()) countries.insert(ev.country);
    }
    if (stat_mapped_) stat_mapped_->setText(QString::number(mapped));
    if (stat_countries_) stat_countries_->setText(QString::number(countries.size()));
}

void ConflictMonitorPanel::update_map(const QVector<NewsEvent>& events) {
    QVector<fincept::ui::MapPin> pins;
    pins.reserve(events.size());

    QHash<QString, int> coord_counts;

    int skipped = 0;
    for (int i = 0; i < events.size(); ++i) {
        const auto& ev = events[i];
        if (!ev.has_coords) {
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

        // pin.id = event index — lets the click handler scroll/select the
        // matching row in the table (UserRole+1 stamp on column 0).
        pins.append({lat, lng,
                     QString("%1 — %2, %3").arg(ev.event_category, ev.city, ev.country),
                     category_color(ev.event_category), 5.0, i});
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

    if (sorted.isEmpty()) {
        auto* empty = new QLabel(tr("No events match the current filters."), this);
        empty->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; font-style:italic;")
                                 .arg(ui::colors::TEXT_TERTIARY())
                                 .arg(ui::fonts::SMALL)
                                 .arg(ui::fonts::DATA_FAMILY()));
        stats_layout_->addWidget(empty);
        return;
    }

    const int max_count = sorted.first().second;

    for (int i = 0; i < qMin(sorted.size(), 8); ++i) {
        const auto& [cat, count] = sorted[i];
        const QColor cat_col = category_color(cat);
        const QString col_hex = cat_col.name();
        const QString col_rgb = QString("%1,%2,%3").arg(cat_col.red()).arg(cat_col.green()).arg(cat_col.blue());
        const double pct = max_count > 0 ? double(count) / max_count : 0.0;

        auto* row = new QWidget(this);
        row->setStyleSheet("background:transparent;");
        auto* rvl = new QVBoxLayout(row);
        rvl->setContentsMargins(0, 0, 0, 0);
        rvl->setSpacing(3);

        // Top line: dot + name + count
        auto* top = new QWidget(row);
        top->setStyleSheet("background:transparent;");
        auto* tl = new QHBoxLayout(top);
        tl->setContentsMargins(0, 0, 0, 0);
        tl->setSpacing(6);

        auto* dot = new QWidget(top);
        dot->setFixedSize(8, 8);
        dot->setStyleSheet(QString("background:%1; border-radius:4px;").arg(col_hex));
        tl->addWidget(dot, 0, Qt::AlignVCenter);

        auto* name = new QLabel(pretty_category(cat), top);
        name->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;"
                                    "background:transparent; border:none;")
                                .arg(ui::colors::TEXT_SECONDARY())
                                .arg(ui::fonts::SMALL)
                                .arg(ui::fonts::DATA_FAMILY()));
        tl->addWidget(name, 1);

        auto* cnt = new QLabel(QString::number(count), top);
        cnt->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;"
                                    "background:transparent; border:none;")
                                .arg(ui::colors::TEXT_PRIMARY())
                                .arg(ui::fonts::SMALL)
                                .arg(ui::fonts::DATA_FAMILY()));
        tl->addWidget(cnt, 0, Qt::AlignRight);
        rvl->addWidget(top);

        // Proportional bar (track + fill)
        auto* track = new QWidget(row);
        track->setFixedHeight(4);
        track->setStyleSheet(QString("background:rgba(%1,0.10); border:none;").arg(col_rgb));
        auto* trackHl = new QHBoxLayout(track);
        trackHl->setContentsMargins(0, 0, 0, 0);
        trackHl->setSpacing(0);

        auto* fill = new QWidget(track);
        fill->setStyleSheet(QString("background:%1; border:none;").arg(col_hex));
        fill->setFixedHeight(4);
        trackHl->addWidget(fill, qMax(1, int(pct * 1000)));
        trackHl->addStretch(qMax(0, int((1.0 - pct) * 1000)));

        rvl->addWidget(track);
        stats_layout_->addWidget(row);
    }
}

void ConflictMonitorPanel::update_hotspots(const QVector<NewsEvent>& events) {
    if (!hotspots_layout_) return;
    while (hotspots_layout_->count() > 0) {
        auto* item = hotspots_layout_->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    QHash<QString, int> counts;
    for (const auto& ev : events) {
        if (ev.country.isEmpty()) continue;
        counts[ev.country]++;
    }

    QVector<QPair<QString, int>> sorted;
    for (auto it = counts.begin(); it != counts.end(); ++it)
        sorted.append({it.key(), it.value()});
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

    if (sorted.isEmpty()) {
        auto* empty = new QLabel(tr("No country data."), this);
        empty->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; font-style:italic;")
                                 .arg(ui::colors::TEXT_TERTIARY())
                                 .arg(ui::fonts::SMALL)
                                 .arg(ui::fonts::DATA_FAMILY()));
        hotspots_layout_->addWidget(empty);
        return;
    }

    const int max_count = sorted.first().second;
    const QColor accent(ui::colors::NEGATIVE());
    const QString accent_rgb = QString("%1,%2,%3").arg(accent.red()).arg(accent.green()).arg(accent.blue());

    for (int i = 0; i < qMin(sorted.size(), 5); ++i) {
        const auto& [country, count] = sorted[i];
        const double pct = max_count > 0 ? double(count) / max_count : 0.0;

        auto* row = new QWidget(this);
        row->setStyleSheet("background:transparent;");
        auto* rvl = new QVBoxLayout(row);
        rvl->setContentsMargins(0, 0, 0, 0);
        rvl->setSpacing(3);

        auto* top = new QWidget(row);
        top->setStyleSheet("background:transparent;");
        auto* tl = new QHBoxLayout(top);
        tl->setContentsMargins(0, 0, 0, 0);
        tl->setSpacing(6);

        auto* rank = new QLabel(QString("%1.").arg(i + 1), top);
        rank->setFixedWidth(18);
        rank->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;"
                                    "background:transparent; border:none;")
                                .arg(ui::colors::TEXT_TERTIARY())
                                .arg(ui::fonts::TINY)
                                .arg(ui::fonts::DATA_FAMILY()));
        tl->addWidget(rank);

        auto* name = new QLabel(country, top);
        name->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;"
                                    "background:transparent; border:none;")
                                .arg(ui::colors::TEXT_PRIMARY())
                                .arg(ui::fonts::SMALL)
                                .arg(ui::fonts::DATA_FAMILY()));
        tl->addWidget(name, 1);

        auto* cnt = new QLabel(QString::number(count), top);
        cnt->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;"
                                    "background:transparent; border:none;")
                                .arg(ui::colors::NEGATIVE())
                                .arg(ui::fonts::SMALL)
                                .arg(ui::fonts::DATA_FAMILY()));
        tl->addWidget(cnt, 0, Qt::AlignRight);
        rvl->addWidget(top);

        auto* track = new QWidget(row);
        track->setFixedHeight(3);
        track->setStyleSheet(QString("background:rgba(%1,0.10); border:none;").arg(accent_rgb));
        auto* trackHl = new QHBoxLayout(track);
        trackHl->setContentsMargins(0, 0, 0, 0);
        trackHl->setSpacing(0);

        auto* fill = new QWidget(track);
        fill->setStyleSheet(QString("background:%1; border:none;").arg(ui::colors::NEGATIVE()));
        fill->setFixedHeight(3);
        trackHl->addWidget(fill, qMax(1, int(pct * 1000)));
        trackHl->addStretch(qMax(0, int((1.0 - pct) * 1000)));
        rvl->addWidget(track);

        hotspots_layout_->addWidget(row);
    }
}

void ConflictMonitorPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void ConflictMonitorPanel::retranslateUi() {
    // Table headers
    if (events_table_)
        events_table_->setHorizontalHeaderLabels(
            {tr("Category"), tr("Country"), tr("City"), tr("Title"), tr("Date"), tr("Lat"), tr("Lng")});

    // Section headers
    if (hdr_overview_)       hdr_overview_->setText(tr("OVERVIEW"));
    if (hdr_top_categories_) hdr_top_categories_->setText(tr("TOP CATEGORIES"));
    if (hdr_hotspots_)       hdr_hotspots_->setText(tr("HOTSPOTS"));
    if (hdr_event_details_)  hdr_event_details_->setText(tr("EVENT DETAILS"));

    // Overview tile captions
    if (tile_events_lbl_)  tile_events_lbl_->setText(tr("EVENTS"));
    if (tile_mapped_lbl_)  tile_mapped_lbl_->setText(tr("MAPPED"));
    if (tile_nations_lbl_) tile_nations_lbl_->setText(tr("NATIONS"));

    // Detail field captions
    if (field_country_lbl_) field_country_lbl_->setText(tr("COUNTRY"));
    if (field_city_lbl_)    field_city_lbl_->setText(tr("CITY"));
    if (field_date_lbl_)    field_date_lbl_->setText(tr("DATE"));
    if (field_source_lbl_)  field_source_lbl_->setText(tr("SOURCE"));

    // Static placeholders / actions
    if (empty_state_)
        empty_state_->setText(tr("⌖  Select an event from the map or table\n    to inspect details here."));
    if (detail_open_btn_) detail_open_btn_->setText(tr("OPEN SOURCE  ↗"));
}

} // namespace fincept::screens
