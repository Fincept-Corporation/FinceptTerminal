// src/screens/maritime/MaritimeScreen.cpp
#include "screens/maritime/MaritimeScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "services/maritime/MaritimeService.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/WorldMapWidget.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QScrollArea>
#include <QSet>
#include <QSplitter>
#include <QTabWidget>

namespace fincept::screens {

using namespace fincept::services::maritime;

// ── Style helpers (live tokens) ───────────────────────────────────────────────

// Helper: token → QString (works for both ColorToken and FontFamilyToken)
static inline QString C(const ui::ColorToken& t) {
    return QString::fromLatin1(t.get());
}
static inline QString F(const ui::fonts::FontFamilyToken& t) {
    return QString::fromLatin1(t.get());
}

static QString table_ss() {
    return QString("QTableWidget { background:%1; color:%2; gridline-color:%3;"
                   "font-family:%4; font-size:%5px; border:none; }"
                   "QTableWidget::item { padding:4px 6px; }"
                   "QTableWidget::item:selected { background:%6; }"
                   "QTableWidget::item:alternate { background:%7; }"
                   "QHeaderView::section { background:%8; color:%9; font-weight:700;"
                   "padding:6px 8px; border:1px solid %3; font-family:%4; font-size:%5px; }")
        .arg(C(ui::colors::BG_BASE), C(ui::colors::TEXT_PRIMARY), C(ui::colors::BORDER_DIM), F(ui::fonts::DATA_FAMILY))
        .arg(ui::fonts::SMALL)
        .arg(C(ui::colors::BG_HOVER), C(ui::colors::ROW_ALT), C(ui::colors::BG_RAISED), C(ui::colors::TEXT_SECONDARY));
}

static QString input_ss() {
    return QString("QLineEdit, QDoubleSpinBox { background:%1; color:%2; border:1px solid %3;"
                   "font-family:%4; font-size:%5px; padding:6px 8px; border-radius:2px; }"
                   "QLineEdit:focus, QDoubleSpinBox:focus { border-color:%6; }")
        .arg(C(ui::colors::BG_SURFACE), C(ui::colors::TEXT_PRIMARY), C(ui::colors::BORDER_MED),
             F(ui::fonts::DATA_FAMILY))
        .arg(ui::fonts::SMALL)
        .arg(C(ui::colors::AMBER));
}

static QString btn_primary_ss() {
    return QString("QPushButton { background:%1; color:%2; font-family:%3; font-size:%4px;"
                   "font-weight:700; border:none; padding:8px; border-radius:2px; letter-spacing:1px; }"
                   "QPushButton:hover { background:%5; }")
        .arg(C(ui::colors::AMBER), C(ui::colors::BG_BASE), F(ui::fonts::DATA_FAMILY))
        .arg(ui::fonts::SMALL)
        .arg(C(ui::colors::AMBER_DIM));
}

static QString btn_outline_ss() {
    return QString("QPushButton { background:transparent; color:%1; font-family:%2; font-size:%3px;"
                   "font-weight:700; border:1px solid %1; padding:6px; border-radius:2px; }"
                   "QPushButton:hover { background:%4; }")
        .arg(C(ui::colors::WARNING), F(ui::fonts::DATA_FAMILY))
        .arg(ui::fonts::SMALL)
        .arg(C(ui::colors::BG_HOVER));
}

static QString section_label_ss() {
    return QString("color:%1; font-size:9px; font-weight:700; font-family:%2;"
                   "letter-spacing:1px; padding-bottom:4px; border-bottom:1px solid %3;")
        .arg(C(ui::colors::AMBER), F(ui::fonts::DATA_FAMILY), C(ui::colors::BORDER_DIM));
}

static QString tiny_label_ss() {
    return QString("color:%1; font-size:8px; font-weight:700; font-family:%2; letter-spacing:1px;")
        .arg(C(ui::colors::TEXT_TERTIARY), F(ui::fonts::DATA_FAMILY));
}

static QString detail_text_ss() {
    return QString("color:%1; font-size:9px; font-family:%2;")
        .arg(C(ui::colors::TEXT_SECONDARY), F(ui::fonts::DATA_FAMILY));
}

// ── Constructor ──────────────────────────────────────────────────────────────
MaritimeScreen::MaritimeScreen(QWidget* parent) : QWidget(parent) {
    // routes_ starts empty — populated dynamically from loaded vessels.
    build_ui();
    connect_service();

    refresh_timer_ = new QTimer(this);
    refresh_timer_->setInterval(5 * 60 * 1000); // 5 min
    connect(refresh_timer_, &QTimer::timeout, this, &MaritimeScreen::on_load_vessels);

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { apply_theme(); });

    LOG_INFO("Maritime", "Screen constructed");
}

void MaritimeScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    refresh_timer_->start();
    if (first_show_) {
        first_show_ = false;
        // Pull whatever bounding box the user last typed (restored from
        // ScreenStateManager) — no hardcoded "Mumbai area" load.
        if (area_min_lat_ && area_max_lat_ && area_min_lng_ && area_max_lng_) {
            const bool valid = area_max_lat_->value() > area_min_lat_->value()
                            && area_max_lng_->value() > area_min_lng_->value();
            if (valid)
                on_load_vessels();
        }
    }
    LOG_INFO("Maritime", "Screen shown");
}

void MaritimeScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    refresh_timer_->stop();
}

void MaritimeScreen::connect_service() {
    auto& svc = MaritimeService::instance();
    connect(&svc, &MaritimeService::vessels_loaded, this, &MaritimeScreen::on_vessels_loaded);
    connect(&svc, &MaritimeService::vessel_found, this, &MaritimeScreen::on_vessel_found);
    connect(&svc, &MaritimeService::vessel_history_loaded, this, &MaritimeScreen::on_vessel_history);
    connect(&svc, &MaritimeService::error_occurred, this, &MaritimeScreen::on_error);
}

// ── Theme apply (called on construction + theme_changed) ─────────────────────
void MaritimeScreen::apply_theme() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));

    if (top_bar_)
        top_bar_->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;")
                                    .arg(C(ui::colors::BG_RAISED), C(ui::colors::BORDER_MED)));

    if (brand_label_)
        brand_label_->setStyleSheet(
            QString("color:%1; font-size:%2px; font-weight:700; font-family:%3; letter-spacing:2px;")
                .arg(ui::colors::AMBER())
                .arg(ui::fonts::TINY)
                .arg(ui::fonts::DATA_FAMILY));

    if (classified_label_)
        classified_label_->setStyleSheet(QString("color:%1; font-size:9px; font-family:%2; letter-spacing:1px;")
                                             .arg(ui::colors::TEXT_TERTIARY())
                                             .arg(ui::fonts::DATA_FAMILY));

    if (credits_label_)
        credits_label_->setStyleSheet(
            QString("color:%1; font-size:9px; font-weight:700; font-family:%2;"
                    "padding:3px 10px; background:%3; border:1px solid %4; border-radius:2px;")
                .arg(ui::colors::TEXT_TERTIARY())
                .arg(ui::fonts::DATA_FAMILY)
                .arg(ui::colors::BG_SURFACE())
                .arg(ui::colors::BORDER_MED()));

    if (vessel_count_label_)
        vessel_count_label_->setStyleSheet(
            QString("color:%1; font-size:9px; font-weight:700; font-family:%2;"
                    "padding:3px 10px; background:%3; border:1px solid %4; border-radius:2px;")
                .arg(ui::colors::INFO())
                .arg(ui::fonts::DATA_FAMILY)
                .arg(ui::colors::BG_SURFACE())
                .arg(ui::colors::BORDER_MED()));

    if (left_panel_)
        left_panel_->setStyleSheet(QString("background:%1; border-right:1px solid %2;")
                                       .arg(C(ui::colors::BG_SURFACE), C(ui::colors::BORDER_DIM)));

    if (right_panel_)
        right_panel_->setStyleSheet(QString("background:%1; border-left:1px solid %2;")
                                        .arg(C(ui::colors::BG_SURFACE), C(ui::colors::BORDER_DIM)));

    if (routes_table_)
        routes_table_->setStyleSheet(table_ss());
    if (vessels_table_)
        vessels_table_->setStyleSheet(table_ss());

    if (status_bar_)
        status_bar_->setStyleSheet(QString("background:%1; border-top:1px solid %2;")
                                       .arg(C(ui::colors::BG_SURFACE), C(ui::colors::BORDER_DIM)));

    if (status_label_)
        set_status("READY", ui::colors::POSITIVE);

    if (route_detail_)
        route_detail_->setStyleSheet(
            QString("background:%1; border:1px solid %2; border-left:3px solid %2; border-radius:2px;")
                .arg(C(ui::colors::BG_SURFACE), C(ui::colors::AMBER)));

    if (search_result_card_)
        search_result_card_->setStyleSheet(QString("background:%1; border:1px solid %2; border-radius:2px;")
                                               .arg(C(ui::colors::BG_SURFACE), C(ui::colors::INFO)));

    if (imo_edit_)
        imo_edit_->setStyleSheet(input_ss());
}

void MaritimeScreen::set_status(const QString& text, const ui::ColorToken& color) {
    if (!status_label_)
        return;
    status_label_->setText(text);
    status_label_->setStyleSheet(
        QString("color:%1; font-size:8px; font-weight:700; font-family:%2;").arg(C(color) ).arg(ui::fonts::DATA_FAMILY));
}

// ── Build UI ─────────────────────────────────────────────────────────────────
void MaritimeScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    top_bar_ = build_top_bar();
    root->addWidget(top_bar_);

    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);

    left_panel_ = build_left_panel();
    right_panel_ = build_right_panel();
    body->addWidget(left_panel_);
    body->addWidget(build_center_panel(), 1);
    body->addWidget(right_panel_);

    auto* body_w = new QWidget(this);
    body_w->setLayout(body);
    root->addWidget(body_w, 1);

    status_bar_ = build_status_bar();
    root->addWidget(status_bar_);

    apply_theme();
}

// ── Top Bar ──────────────────────────────────────────────────────────────────
QWidget* MaritimeScreen::build_top_bar() {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(44);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(12);

    brand_label_ = new QLabel("FINCEPT MARITIME INTELLIGENCE", bar);
    hl->addWidget(brand_label_);

    classified_label_ = new QLabel("VESSEL TRACKING // FINCEPT MARINE API", bar);
    hl->addWidget(classified_label_);

    hl->addStretch(1);

    credits_label_ = new QLabel("CREDITS: —", bar);
    hl->addWidget(credits_label_);

    vessel_count_label_ = new QLabel("— VESSELS", bar);
    hl->addWidget(vessel_count_label_);

    return bar;
}

// ── Left Panel ───────────────────────────────────────────────────────────────
QWidget* MaritimeScreen::build_left_panel() {
    auto* panel = new QWidget(this);
    panel->setFixedWidth(260);

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(8);

    auto* load_btn = new QPushButton("LOAD VESSELS IN AREA", panel);
    load_btn->setCursor(Qt::PointingHandCursor);
    load_btn->setStyleSheet(btn_primary_ss());
    connect(load_btn, &QPushButton::clicked, this, &MaritimeScreen::on_load_vessels);
    vl->addWidget(load_btn);

    auto* intel_title = new QLabel("INTELLIGENCE", panel);
    intel_title->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; font-family:%2; letter-spacing:1px;")
                                   .arg(ui::colors::AMBER())
                                   .arg(ui::fonts::DATA_FAMILY));
    vl->addWidget(intel_title);

    auto* grid = new QGridLayout;
    grid->setSpacing(6);

    auto make_stat = [&](const QString& label, const QString& value, const ui::ColorToken& val_color) {
        auto* box = new QWidget(panel);
        box->setStyleSheet(QString("background:%1; border:1px solid %2; border-radius:2px;")
                               .arg(C(ui::colors::BG_BASE), C(ui::colors::BORDER_DIM)));
        auto* bvl = new QVBoxLayout(box);
        bvl->setContentsMargins(8, 6, 8, 6);
        bvl->setSpacing(2);
        auto* lbl = new QLabel(label, box);
        lbl->setStyleSheet(QString("color:%1; font-size:7px; font-family:%2;")
                               .arg(ui::colors::TEXT_TERTIARY())
                               .arg(ui::fonts::DATA_FAMILY));
        auto* val = new QLabel(value, box);
        val->setStyleSheet(QString("color:%1; font-size:12px; font-weight:700; font-family:%2;")
                               .arg(C(val_color))
                               .arg(ui::fonts::DATA_FAMILY));
        val->setObjectName(label);
        bvl->addWidget(lbl);
        bvl->addWidget(val);
        return box;
    };

    // All stats start at "—" and are populated from the loaded vessel set.
    // No hardcoded trade-volume, route count, or satellite numbers — the API
    // does not expose those, so we don't fabricate them.
    auto* sv = make_stat("TOTAL IN AREA", "—", ui::colors::INFO);
    stat_vessels_ = sv->findChild<QLabel*>("TOTAL IN AREA");
    grid->addWidget(sv, 0, 0);

    auto* sd = make_stat("DISPLAYED", "—", ui::colors::POSITIVE);
    stat_displayed_ = sd->findChild<QLabel*>("DISPLAYED");
    grid->addWidget(sd, 0, 1);

    auto* sr = make_stat("ROUTES", "—", ui::colors::INFO);
    stat_routes_ = sr->findChild<QLabel*>("ROUTES");
    grid->addWidget(sr, 1, 0);

    auto* sp = make_stat("PORTS", "—", ui::colors::WARNING);
    stat_ports_ = sp->findChild<QLabel*>("PORTS");
    grid->addWidget(sp, 1, 1);

    vl->addLayout(grid);

    vl->addSpacing(8);
    auto* routes_title = new QLabel("TRADE CORRIDORS", panel);
    routes_title->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; font-family:%2; letter-spacing:1px;")
                                    .arg(ui::colors::AMBER())
                                    .arg(ui::fonts::DATA_FAMILY));
    vl->addWidget(routes_title);

    routes_table_ = new QTableWidget(panel);
    routes_table_->setColumnCount(3);
    routes_table_->setHorizontalHeaderLabels({"Route", "Vessels", "Status"});
    routes_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    routes_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    routes_table_->horizontalHeader()->setStretchLastSection(true);
    routes_table_->verticalHeader()->setVisible(false);
    routes_table_->setStyleSheet(table_ss());
    connect(routes_table_, &QTableWidget::currentCellChanged, this,
            [this](int row, int, int, int) { on_route_selected(row); });
    vl->addWidget(routes_table_, 1);

    return panel;
}

// ── Center Panel ─────────────────────────────────────────────────────────────
QWidget* MaritimeScreen::build_center_panel() {
    auto* panel = new QWidget(this);
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* header = new QWidget(panel);
    header->setFixedHeight(36);
    header->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(C(ui::colors::BG_RAISED), C(ui::colors::BORDER_DIM)));
    auto* hhl = new QHBoxLayout(header);
    hhl->setContentsMargins(16, 0, 16, 0);
    auto* title = new QLabel("VESSEL TRACKING", header);
    title->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3; letter-spacing:1px;")
                             .arg(ui::colors::AMBER())
                             .arg(ui::fonts::TINY)
                             .arg(ui::fonts::DATA_FAMILY));
    hhl->addWidget(title);
    hhl->addStretch();
    vl->addWidget(header);

    auto* splitter = new QSplitter(Qt::Vertical, panel);
    splitter->setHandleWidth(2);
    splitter->setStyleSheet(QString("QSplitter::handle { background:%1; }").arg(ui::colors::BORDER_DIM()));

    map_widget_ = new fincept::ui::WorldMapWidget(splitter);
    map_widget_->setMinimumHeight(280);
    splitter->addWidget(map_widget_);

    vessels_table_ = new QTableWidget(splitter);
    vessels_table_->setColumnCount(9);
    vessels_table_->setHorizontalHeaderLabels(
        {"Name", "IMO", "Lat", "Lng", "Speed (kn)", "Angle", "From", "To", "Progress"});
    vessels_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    vessels_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    vessels_table_->setAlternatingRowColors(true);
    vessels_table_->horizontalHeader()->setStretchLastSection(true);
    vessels_table_->verticalHeader()->setVisible(false);
    vessels_table_->setSortingEnabled(true);
    vessels_table_->setStyleSheet(table_ss());
    splitter->addWidget(vessels_table_);

    vessels_table_->setMinimumHeight(180);
    splitter->setStretchFactor(0, 5);
    splitter->setStretchFactor(1, 5);
    splitter->setSizes({380, 360});
    vl->addWidget(splitter, 1);

    return panel;
}

// ── Right Panel ───────────────────────────────────────────────────────────────
QWidget* MaritimeScreen::build_right_panel() {
    auto* panel = new QWidget(this);
    panel->setFixedWidth(280);

    auto* scroll = new QScrollArea(panel);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border:none; background:transparent; }");

    auto* content = new QWidget(scroll);
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(10);

    // ── Vessel Search ──────────────────────────────────────────────────────
    auto* search_title = new QLabel("VESSEL SEARCH", content);
    search_title->setStyleSheet(section_label_ss());
    vl->addWidget(search_title);

    auto* imo_lbl = new QLabel("IMO NUMBER", content);
    imo_lbl->setStyleSheet(tiny_label_ss());
    vl->addWidget(imo_lbl);

    imo_edit_ = new QLineEdit(content);
    imo_edit_->setPlaceholderText("e.g. 9344745");
    imo_edit_->setStyleSheet(input_ss());
    connect(imo_edit_, &QLineEdit::returnPressed, this, &MaritimeScreen::on_search_vessel);
    connect(imo_edit_, &QLineEdit::textChanged, this,
            [this](const QString&) { fincept::ScreenStateManager::instance().notify_changed(this); });
    vl->addWidget(imo_edit_);

    auto* track_btn = new QPushButton("TRACK", content);
    track_btn->setCursor(Qt::PointingHandCursor);
    track_btn->setStyleSheet(btn_primary_ss());
    connect(track_btn, &QPushButton::clicked, this, &MaritimeScreen::on_search_vessel);
    vl->addWidget(track_btn);

    auto* history_btn = new QPushButton("VOYAGE HISTORY", content);
    history_btn->setCursor(Qt::PointingHandCursor);
    history_btn->setStyleSheet(btn_outline_ss());
    connect(history_btn, &QPushButton::clicked, this, [this]() {
        auto imo = imo_edit_->text().trimmed();
        if (imo.isEmpty())
            return;
        set_status("LOADING HISTORY...", ui::colors::WARNING);
        MaritimeService::instance().get_vessel_history(imo);
    });
    vl->addWidget(history_btn);

    // Search result card
    search_result_card_ = new QWidget(content);
    search_result_card_->setVisible(false);
    auto* srvl = new QVBoxLayout(search_result_card_);
    srvl->setContentsMargins(10, 10, 10, 10);
    srvl->setSpacing(4);

    sr_name_ = new QLabel(search_result_card_);
    sr_name_->setStyleSheet(QString("color:%1; font-size:11px; font-weight:700; font-family:%2;")
                                .arg(ui::colors::INFO())
                                .arg(ui::fonts::DATA_FAMILY));
    srvl->addWidget(sr_name_);
    sr_imo_ = new QLabel(search_result_card_);
    sr_imo_->setStyleSheet(detail_text_ss());
    srvl->addWidget(sr_imo_);
    sr_position_ = new QLabel(search_result_card_);
    sr_position_->setStyleSheet(detail_text_ss());
    srvl->addWidget(sr_position_);
    sr_speed_ = new QLabel(search_result_card_);
    sr_speed_->setStyleSheet(detail_text_ss());
    srvl->addWidget(sr_speed_);
    sr_from_ = new QLabel(search_result_card_);
    sr_from_->setStyleSheet(detail_text_ss());
    srvl->addWidget(sr_from_);
    sr_to_ = new QLabel(search_result_card_);
    sr_to_->setStyleSheet(detail_text_ss());
    srvl->addWidget(sr_to_);
    vl->addWidget(search_result_card_);

    search_result_label_ = new QLabel(content);
    search_result_label_->setStyleSheet(
        QString("color:%1; font-size:9px; font-family:%2;").arg(ui::colors::NEGATIVE()).arg(ui::fonts::DATA_FAMILY));
    search_result_label_->setVisible(false);
    vl->addWidget(search_result_label_);

    // ── Area Search ────────────────────────────────────────────────────────
    vl->addSpacing(8);
    auto* area_title = new QLabel("AREA SEARCH", content);
    area_title->setStyleSheet(section_label_ss());
    vl->addWidget(area_title);

    auto make_coord = [&](const QString& label, double def_val) {
        auto* lbl = new QLabel(label, content);
        lbl->setStyleSheet(tiny_label_ss());
        vl->addWidget(lbl);
        auto* spin = new QDoubleSpinBox(content);
        spin->setRange(-180, 180);
        spin->setDecimals(4);
        spin->setValue(def_val);
        spin->setStyleSheet(input_ss());
        vl->addWidget(spin);
        return spin;
    };

    // Spinners start at zero — the user supplies a real bbox before the
    // first call (or restore_state injects the last-used coords).
    area_min_lat_ = make_coord("MIN LATITUDE", 0.0);
    area_max_lat_ = make_coord("MAX LATITUDE", 0.0);
    area_min_lng_ = make_coord("MIN LONGITUDE", 0.0);
    area_max_lng_ = make_coord("MAX LONGITUDE", 0.0);

    auto* area_btn = new QPushButton("SEARCH AREA", content);
    area_btn->setCursor(Qt::PointingHandCursor);
    area_btn->setStyleSheet(btn_primary_ss());
    connect(area_btn, &QPushButton::clicked, this, [this]() {
        AreaSearchParams params;
        params.min_lat = area_min_lat_->value();
        params.max_lat = area_max_lat_->value();
        params.min_lng = area_min_lng_->value();
        params.max_lng = area_max_lng_->value();
        set_status("SEARCHING...", ui::colors::WARNING);
        MaritimeService::instance().search_vessels_by_area(params);
    });
    vl->addWidget(area_btn);

    // ── Route Detail ───────────────────────────────────────────────────────
    vl->addSpacing(8);
    route_detail_ = new QWidget(content);
    route_detail_->setVisible(false);
    auto* rdvl = new QVBoxLayout(route_detail_);
    rdvl->setContentsMargins(10, 10, 10, 10);
    rdvl->setSpacing(4);

    auto* rd_title = new QLabel("SELECTED ROUTE", route_detail_);
    rd_title->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; font-family:%2; letter-spacing:1px;")
                                .arg(ui::colors::AMBER())
                                .arg(ui::fonts::DATA_FAMILY));
    rdvl->addWidget(rd_title);

    rd_name_ = new QLabel(route_detail_);
    rd_name_->setStyleSheet(QString("color:%1; font-size:11px; font-weight:700; font-family:%2;")
                                .arg(ui::colors::INFO())
                                .arg(ui::fonts::DATA_FAMILY));
    rdvl->addWidget(rd_name_);
    rd_value_ = new QLabel(route_detail_);
    rd_value_->setStyleSheet(detail_text_ss());
    rdvl->addWidget(rd_value_);
    rd_status_ = new QLabel(route_detail_);
    rd_status_->setStyleSheet(detail_text_ss());
    rdvl->addWidget(rd_status_);
    rd_vessels_ = new QLabel(route_detail_);
    rd_vessels_->setStyleSheet(detail_text_ss());
    rdvl->addWidget(rd_vessels_);
    vl->addWidget(route_detail_);

    // ── System Status ──────────────────────────────────────────────────────
    vl->addSpacing(8);
    not_found_label_ = new QLabel(content);
    not_found_label_->setVisible(false);
    not_found_label_->setWordWrap(true);
    not_found_label_->setStyleSheet(QString("color:%1; font-size:9px; font-family:%2;"
                                            "padding:6px; border:1px solid %3; background:%4; border-radius:2px;")
                                        .arg(ui::colors::WARNING())
                                        .arg(ui::fonts::DATA_FAMILY)
                                        .arg(ui::colors::BORDER_MED())
                                        .arg(ui::colors::BG_SURFACE()));
    vl->addWidget(not_found_label_);

    // Suppressed: hardcoded "system status" placeholders (AIS Transponders /
    // Satellite Imagery / 10 corridors / 13 SATs / 6 ports) — none of these
    // figures come from the API. Restore once a real backend exposes them.
    QVector<QPair<QString, QString>> statuses;
    for (const auto& [text, color] : statuses) {
        auto* lbl = new QLabel(QString::fromUtf8("\u25CF ") + text, content);
        lbl->setStyleSheet(QString("color:%1; font-size:8px; font-family:%2;").arg(color).arg(ui::fonts::DATA_FAMILY));
        vl->addWidget(lbl);
    }

    auto* cls = new QLabel("CLASSIFIED — AUTHORIZED PERSONNEL ONLY", content);
    cls->setWordWrap(true);
    cls->setAlignment(Qt::AlignCenter);
    cls->setStyleSheet(QString("color:%1; font-size:8px; font-family:%2; font-weight:700;"
                               "padding:8px; border:1px solid %3; background:%4; border-radius:2px;")
                           .arg(ui::colors::WARNING())
                           .arg(ui::fonts::DATA_FAMILY)
                           .arg(ui::colors::BORDER_MED())
                           .arg(ui::colors::BG_SURFACE()));
    vl->addWidget(cls);

    vl->addStretch();
    scroll->setWidget(content);

    auto* outer = new QVBoxLayout(panel);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);
    return panel;
}

// ── Status Bar ───────────────────────────────────────────────────────────────
QWidget* MaritimeScreen::build_status_bar() {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(24);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(16);

    auto s =
        QString("color:%1; font-size:8px; font-family:%2;").arg(ui::colors::TEXT_TERTIARY()).arg(ui::fonts::DATA_FAMILY);
    auto sv = QString("color:%1; font-size:8px; font-weight:700; font-family:%2;")
                  .arg(ui::colors::TEXT_PRIMARY())
                  .arg(ui::fonts::DATA_FAMILY);

    auto* lbl1 = new QLabel("SOURCE:", bar);
    lbl1->setStyleSheet(s);
    auto* val1 = new QLabel("AIS FEED + FINCEPT API", bar);
    val1->setStyleSheet(sv);
    hl->addWidget(lbl1);
    hl->addWidget(val1);

    auto* lbl2 = new QLabel("REFRESH:", bar);
    lbl2->setStyleSheet(s);
    auto* val2 = new QLabel("5 MIN", bar);
    val2->setStyleSheet(sv);
    hl->addWidget(lbl2);
    hl->addWidget(val2);

    hl->addStretch();

    status_label_ = new QLabel("READY", bar);
    hl->addWidget(status_label_);

    return bar;
}

// ── Populate routes table ────────────────────────────────────────────────────
void MaritimeScreen::populate_routes_table() {
    routes_table_->setRowCount(routes_.size());
    for (int i = 0; i < routes_.size(); ++i) {
        const auto& r = routes_[i];
        auto* name_item = new QTableWidgetItem(r.name);
        name_item->setForeground(QBrush(QColor(ui::colors::INFO.get())));
        routes_table_->setItem(i, 0, name_item);
        // The API doesn't expose trade-volume in $ — repurpose the middle
        // column for the live vessel count derived from the loaded set.
        auto* count_item = new QTableWidgetItem(QString::number(r.vessels));
        count_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        routes_table_->setItem(i, 1, count_item);
        auto* status_item = new QTableWidgetItem(r.status.isEmpty() ? QStringLiteral("-") : r.status.toUpper());
        status_item->setForeground(route_status_color(r.status));
        routes_table_->setItem(i, 2, status_item);
    }
    routes_table_->resizeColumnsToContents();
}

// ── Actions ──────────────────────────────────────────────────────────────────
void MaritimeScreen::on_load_vessels() {
    AreaSearchParams params;
    if (area_min_lat_) params.min_lat = area_min_lat_->value();
    if (area_max_lat_) params.max_lat = area_max_lat_->value();
    if (area_min_lng_) params.min_lng = area_min_lng_->value();
    if (area_max_lng_) params.max_lng = area_max_lng_->value();
    if (params.max_lat <= params.min_lat || params.max_lng <= params.min_lng) {
        set_status("INVALID BBOX", ui::colors::WARNING);
        return;
    }
    set_status("LOADING...", ui::colors::WARNING);
    MaritimeService::instance().search_vessels_by_area(params);
}

void MaritimeScreen::on_search_vessel() {
    auto imo = imo_edit_->text().trimmed();
    if (imo.isEmpty())
        return;
    search_result_card_->setVisible(false);
    search_result_label_->setVisible(false);
    set_status("TRACKING...", ui::colors::WARNING);
    MaritimeService::instance().get_vessel_position(imo);
}

void MaritimeScreen::on_vessels_loaded(VesselsPage page) {
    // Render limit — area-search can return 14k+ rows. Pin/render the top
    // (newest-first) batch to keep the UI responsive; full count still
    // shown in the status badge.
    static constexpr int kRenderLimit = 500;
    const int render_n = qMin(page.vessels.size(), kRenderLimit);

    vessels_table_->setSortingEnabled(false);
    vessels_table_->setRowCount(render_n);

    for (int i = 0; i < render_n; ++i) {
        const auto& v = page.vessels[i];

        auto* name_item = new QTableWidgetItem(v.name);
        name_item->setForeground(QBrush(QColor(ui::colors::INFO.get())));
        vessels_table_->setItem(i, 0, name_item);
        vessels_table_->setItem(i, 1, new QTableWidgetItem(v.imo));

        auto* lat = new QTableWidgetItem(QString::number(v.latitude, 'f', 4));
        lat->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        vessels_table_->setItem(i, 2, lat);

        auto* lng = new QTableWidgetItem(QString::number(v.longitude, 'f', 4));
        lng->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        vessels_table_->setItem(i, 3, lng);

        auto* spd = new QTableWidgetItem(QString::number(v.speed, 'f', 1));
        spd->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        vessels_table_->setItem(i, 4, spd);

        auto* ang = new QTableWidgetItem(QString::number(v.angle, 'f', 0));
        ang->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        vessels_table_->setItem(i, 5, ang);

        vessels_table_->setItem(i, 6, new QTableWidgetItem(v.from_port));
        vessels_table_->setItem(i, 7, new QTableWidgetItem(v.to_port));

        auto* prog = new QTableWidgetItem(v.route_progress > 0 ? QString("%1%").arg(v.route_progress, 0, 'f', 0) : "-");
        prog->setTextAlignment(Qt::AlignCenter);
        vessels_table_->setItem(i, 8, prog);
    }

    vessels_table_->setSortingEnabled(true);
    vessels_table_->resizeColumnsToContents();

    // Surface multi-vessel "not_found" warnings if any IMOs were missing.
    if (not_found_label_) {
        if (!page.not_found.isEmpty()) {
            not_found_label_->setText(QString("Not found in DB: %1").arg(page.not_found.join(", ")));
            not_found_label_->setVisible(true);
        } else {
            not_found_label_->setVisible(false);
        }
    }

    update_intelligence(page.total_count);
    update_credits(page.remaining_credits);
    update_map(page.vessels.mid(0, kRenderLimit));
    rebuild_routes_from_vessels(page.vessels);
    populate_routes_table();

    if (page.vessels.size() > kRenderLimit)
        set_status(QString("READY (showing %1 of %2)").arg(render_n).arg(page.vessels.size()), ui::colors::WARNING);
    else
        set_status("READY", ui::colors::POSITIVE);
}

void MaritimeScreen::on_vessel_found(VesselData vessel) {
    search_result_card_->setVisible(true);
    search_result_label_->setVisible(false);
    sr_name_->setText(vessel.name);
    sr_imo_->setText("IMO: " + vessel.imo);
    sr_position_->setText(QString("Position: %1, %2").arg(vessel.latitude, 0, 'f', 4).arg(vessel.longitude, 0, 'f', 4));
    sr_speed_->setText(QString("Speed: %1 kn").arg(vessel.speed, 0, 'f', 1));
    sr_from_->setText("From: " + (vessel.from_port.isEmpty() ? "—" : vessel.from_port));
    sr_to_->setText("To: " + (vessel.to_port.isEmpty() ? "—" : vessel.to_port));
    set_status("READY", ui::colors::POSITIVE);
}

void MaritimeScreen::on_error(const QString& context, const QString& message) {
    if (context == "vessel_position") {
        search_result_card_->setVisible(false);
        search_result_label_->setText("Error: " + message);
        search_result_label_->setVisible(true);
    }
    set_status("ERROR", ui::colors::NEGATIVE);
    LOG_ERROR("Maritime", QString("[%1] %2").arg(context, message));
}

void MaritimeScreen::on_route_selected(int row) {
    if (row < 0 || row >= routes_.size()) {
        route_detail_->setVisible(false);
        return;
    }
    const auto& r = routes_[row];
    route_detail_->setVisible(true);
    rd_name_->setText(r.name);
    rd_value_->setText(r.value.isEmpty() ? QStringLiteral("Trade Value: n/a") : "Trade Value: " + r.value);
    rd_status_->setText("Status: " + (r.status.isEmpty() ? QStringLiteral("-") : r.status.toUpper()));
    rd_status_->setStyleSheet(QString("color:%1; font-size:9px; font-family:%2;")
                                  .arg(route_status_color(r.status).name())
                                  .arg(ui::fonts::DATA_FAMILY));
    rd_vessels_->setText(QString("Active Vessels: %1").arg(r.vessels));
}

void MaritimeScreen::update_intelligence(int vessel_count) {
    auto fmt = [](int n) -> QString {
        if (n >= 1'000'000) return QString::number(n / 1'000'000.0, 'f', 1) + "M";
        if (n >= 1'000)     return QString::number(n / 1'000.0,     'f', 1) + "K";
        return QString::number(n);
    };
    const QString count_str = fmt(vessel_count);
    vessel_count_label_->setText(QString("%1 VESSELS").arg(count_str));
    if (stat_vessels_)
        stat_vessels_->setText(count_str);
    if (stat_displayed_)
        stat_displayed_->setText(QString::number(qMin(vessel_count, 500)));
    if (stat_routes_)
        stat_routes_->setText(QString::number(routes_.size()));
}

void MaritimeScreen::update_credits(int remaining) {
    if (!credits_label_ || remaining < 0)
        return;
    const QString c = remaining < 20  ? ui::colors::NEGATIVE()
                    : remaining < 100 ? ui::colors::WARNING()
                                      : ui::colors::POSITIVE();
    QColor cc(c);
    auto rgb = QString("%1,%2,%3").arg(cc.red()).arg(cc.green()).arg(cc.blue());
    credits_label_->setText(QString("CREDITS: %1").arg(remaining));
    credits_label_->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; font-family:%2;"
                                          "padding:3px 10px; background:rgba(%3,0.08); border:1px solid rgba(%3,0.3);"
                                          "border-radius:2px;")
                                      .arg(c)
                                      .arg(ui::fonts::DATA_FAMILY)
                                      .arg(rgb));
}

void MaritimeScreen::update_map(const QVector<VesselData>& vessels) {
    QVector<fincept::ui::MapPin> pins;
    pins.reserve(vessels.size());
    QSet<QString> port_seen;
    for (const auto& v : vessels) {
        if (v.latitude == 0.0 && v.longitude == 0.0)
            continue;
        pins.append({v.latitude, v.longitude, QString("%1 (%2)").arg(v.name, v.imo),
                     ui::colors::POSITIVE, 4.0});
        // Track unique destination ports for the PORTS stat.
        if (!v.to_port.isEmpty())
            port_seen.insert(v.to_port);
    }
    if (stat_ports_)
        stat_ports_->setText(QString::number(port_seen.size()));
    map_widget_->set_pins(pins);
    map_widget_->fit_to_pins();
}

void MaritimeScreen::rebuild_routes_from_vessels(const QVector<VesselData>& vessels) {
    // Aggregate vessels by from_port -> to_port. Skip rows with missing
    // endpoints. Sort by vessel count desc so the busiest corridors top
    // the list.
    QHash<QString, TradeRoute> agg;
    for (const auto& v : vessels) {
        if (v.from_port.isEmpty() || v.to_port.isEmpty())
            continue;
        const QString key = v.from_port + " -> " + v.to_port;
        auto& r = agg[key];
        if (r.name.isEmpty()) {
            r.name = key;
            r.status = QStringLiteral("active");
        }
        r.vessels++;
    }
    routes_.clear();
    routes_.reserve(agg.size());
    for (auto it = agg.cbegin(); it != agg.cend(); ++it)
        routes_.append(it.value());
    std::sort(routes_.begin(), routes_.end(),
              [](const TradeRoute& a, const TradeRoute& b) { return a.vessels > b.vessels; });
}

void MaritimeScreen::on_vessel_history(VesselHistoryPage page) {
    update_credits(page.remaining_credits);
    const auto& history = page.history;
    if (history.isEmpty()) {
        set_status("NO HISTORY", ui::colors::WARNING);
        return;
    }

    QVector<fincept::ui::MapPin> pins;
    pins.reserve(history.size());

    QString vessel_name = history.first().name;
    int total = history.size();

    for (int i = 0; i < total; ++i) {
        const auto& h = history[i];
        if (h.latitude == 0.0 && h.longitude == 0.0)
            continue;

        double t = static_cast<double>(total - 1 - i) / std::max(total - 1, 1);
        int r_c = 255;
        int g_c = static_cast<int>(165 + t * 90);
        int b_c = static_cast<int>(t * 100);
        int alpha = static_cast<int>(100 + t * 155);
        QColor color(r_c, g_c, b_c, alpha);

        double radius = 3.0 + t * 4.0;
        QString label = QString("%1 — %2 → %3 [%4]").arg(vessel_name, h.from_port, h.to_port, h.last_updated.left(10));
        pins.append({h.latitude, h.longitude, label, color, radius});
    }

    const auto& current = history.first();
    if (current.latitude != 0.0 || current.longitude != 0.0) {
        pins.append({current.latitude, current.longitude, QString("%1 — CURRENT POSITION").arg(vessel_name),
                     ui::colors::POSITIVE, 8.0});
    }

    map_widget_->set_pins(pins);
    map_widget_->fit_to_pins();

    vessels_table_->setSortingEnabled(false);
    vessels_table_->setRowCount(history.size());
    for (int i = 0; i < history.size(); ++i) {
        const auto& v = history[i];
        auto* name_item = new QTableWidgetItem(v.name);
        name_item->setForeground(QBrush(QColor(ui::colors::WARNING.get())));
        vessels_table_->setItem(i, 0, name_item);
        vessels_table_->setItem(i, 1, new QTableWidgetItem(v.imo));
        auto* lat = new QTableWidgetItem(QString::number(v.latitude, 'f', 4));
        lat->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        vessels_table_->setItem(i, 2, lat);
        auto* lng = new QTableWidgetItem(QString::number(v.longitude, 'f', 4));
        lng->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        vessels_table_->setItem(i, 3, lng);
        auto* spd = new QTableWidgetItem(QString::number(v.speed, 'f', 1));
        spd->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        vessels_table_->setItem(i, 4, spd);
        auto* ang = new QTableWidgetItem(QString::number(v.angle, 'f', 0));
        ang->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        vessels_table_->setItem(i, 5, ang);
        vessels_table_->setItem(i, 6, new QTableWidgetItem(v.from_port));
        vessels_table_->setItem(i, 7, new QTableWidgetItem(v.to_port));
        vessels_table_->setItem(i, 8, new QTableWidgetItem(v.last_updated.left(10)));
    }
    vessels_table_->setSortingEnabled(true);
    vessels_table_->resizeColumnsToContents();

    const int reported = page.total_records > 0 ? page.total_records : total;
    set_status(QString("HISTORY: %1 (%2 positions)").arg(vessel_name).arg(reported), ui::colors::WARNING);
    LOG_INFO("Maritime", QString("Vessel history [%1]: %2 / %3 positions").arg(vessel_name).arg(total).arg(reported));
}

// ── IStatefulScreen ──────────────────────────────────────────────────────────

QVariantMap MaritimeScreen::save_state() const {
    QVariantMap s;
    if (imo_edit_)
        s.insert("imo", imo_edit_->text());
    if (area_min_lat_)
        s.insert("min_lat", area_min_lat_->value());
    if (area_max_lat_)
        s.insert("max_lat", area_max_lat_->value());
    if (area_min_lng_)
        s.insert("min_lng", area_min_lng_->value());
    if (area_max_lng_)
        s.insert("max_lng", area_max_lng_->value());
    return s;
}

void MaritimeScreen::restore_state(const QVariantMap& state) {
    if (imo_edit_)
        imo_edit_->setText(state.value("imo").toString());
    if (area_min_lat_ && state.contains("min_lat"))
        area_min_lat_->setValue(state.value("min_lat").toDouble());
    if (area_max_lat_ && state.contains("max_lat"))
        area_max_lat_->setValue(state.value("max_lat").toDouble());
    if (area_min_lng_ && state.contains("min_lng"))
        area_min_lng_->setValue(state.value("min_lng").toDouble());
    if (area_max_lng_ && state.contains("max_lng"))
        area_max_lng_->setValue(state.value("max_lng").toDouble());
}

} // namespace fincept::screens
