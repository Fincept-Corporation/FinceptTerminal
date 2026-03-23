// src/screens/geopolitics/GeopoliticsScreen.cpp
#include "screens/geopolitics/GeopoliticsScreen.h"
#include "screens/geopolitics/ConflictMonitorPanel.h"
#include "screens/geopolitics/HDXDataPanel.h"
#include "screens/geopolitics/RelationshipPanel.h"
#include "screens/geopolitics/TradeAnalysisPanel.h"
#include "services/geopolitics/GeopoliticsService.h"

#include "core/logging/Logger.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QScrollArea>

namespace fincept::screens {

using namespace fincept::services::geo;

// ── Constructor ──────────────────────────────────────────────────────────────
GeopoliticsScreen::GeopoliticsScreen(QWidget* parent) : QWidget(parent) {
    build_ui();
    connect_service();

    refresh_timer_ = new QTimer(this);
    refresh_timer_->setInterval(5 * 60 * 1000); // 5 min auto-refresh
    connect(refresh_timer_, &QTimer::timeout, this, &GeopoliticsScreen::on_apply_filters);

    LOG_INFO("Geopolitics", "Screen constructed");
}

void GeopoliticsScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    refresh_timer_->start();
    if (first_show_) {
        first_show_ = false;
        country_edit_->setText("Ukraine");
        on_apply_filters();
        GeopoliticsService::instance().fetch_unique_countries();
        GeopoliticsService::instance().fetch_unique_categories();
    }
    LOG_INFO("Geopolitics", "Screen shown");
}

void GeopoliticsScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    refresh_timer_->stop();
}

void GeopoliticsScreen::connect_service() {
    auto& svc = GeopoliticsService::instance();
    connect(&svc, &GeopoliticsService::events_loaded,
            this, &GeopoliticsScreen::on_events_loaded);
    connect(&svc, &GeopoliticsService::error_occurred,
            this, &GeopoliticsScreen::on_error);
    connect(&svc, &GeopoliticsService::categories_loaded,
            this, [this](QVector<UniqueCategory> cats) {
        category_combo_->clear();
        category_combo_->addItem("All Categories", "");
        for (const auto& c : cats)
            category_combo_->addItem(
                QString("%1 (%2)").arg(c.category).arg(c.event_count), c.category);
    });
}

// ── Build UI ─────────────────────────────────────────────────────────────────
void GeopoliticsScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_top_bar());

    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);

    body->addWidget(build_filter_panel());

    // Content stack
    content_stack_ = new QStackedWidget(this);
    monitor_panel_ = new ConflictMonitorPanel(this);
    hdx_panel_ = new HDXDataPanel(this);
    relationship_panel_ = new RelationshipPanel(this);
    trade_panel_ = new TradeAnalysisPanel(this);

    content_stack_->addWidget(monitor_panel_);
    content_stack_->addWidget(hdx_panel_);
    content_stack_->addWidget(relationship_panel_);
    content_stack_->addWidget(trade_panel_);
    body->addWidget(content_stack_, 1);

    auto* body_w = new QWidget(this);
    body_w->setLayout(body);
    root->addWidget(body_w, 1);

    root->addWidget(build_status_bar());

    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));

    // Highlight the first tab now that content_stack_ exists
    on_tab_changed(0);
}

// ── Top Bar ──────────────────────────────────────────────────────────────────
QWidget* GeopoliticsScreen::build_top_bar() {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(44);
    bar->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;")
        .arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(12);

    // Branding
    auto* brand = new QLabel("GEO> GEOPOLITICAL CONFLICT MONITOR", bar);
    brand->setStyleSheet(QString(
        "color:#FF0000; font-size:%1px; font-weight:700; font-family:%2;"
        "padding:4px 12px; background:rgba(255,0,0,0.08);"
        "border:1px solid rgba(255,0,0,0.25); border-radius:2px;")
        .arg(ui::fonts::TINY).arg(ui::fonts::DATA_FAMILY));
    hl->addWidget(brand);

    auto* div = new QWidget(bar);
    div->setFixedSize(1, 20);
    div->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM));
    hl->addWidget(div);

    // Tab selector buttons
    struct TabDef { QString label; QString color; };
    QVector<TabDef> tabs = {
        {"MONITOR", "#FF0000"},
        {"HDX DATA", "#00E5FF"},
        {"RELATIONS", "#9D4EDD"},
        {"TRADE", "#FFC400"},
    };

    for (int i = 0; i < tabs.size(); ++i) {
        auto* btn = new QPushButton(tabs[i].label, bar);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString(
            "QPushButton { color:%1; font-size:9px; font-family:%2;"
            "padding:4px 10px; border:1px solid transparent; border-radius:2px;"
            "background:transparent; font-weight:400; }"
            "QPushButton:hover { background:rgba(%3,0.1); color:%4; }")
            .arg(ui::colors::TEXT_TERTIARY).arg(ui::fonts::DATA_FAMILY)
            .arg(tabs[i].color.mid(1)).arg(tabs[i].color)); // rough — color used directly
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_tab_changed(i); });
        hl->addWidget(btn);
        tab_buttons_.append(btn);
    }

    hl->addStretch(1);

    // Event count badge
    event_count_label_ = new QLabel("0 EVENTS", bar);
    event_count_label_->setStyleSheet(QString(
        "color:%1; font-size:9px; font-family:%2; padding:3px 8px;"
        "background:rgba(220,38,38,0.08); border:1px solid rgba(220,38,38,0.25);"
        "border-radius:2px; font-weight:700;")
        .arg(ui::colors::NEGATIVE).arg(ui::fonts::DATA_FAMILY));
    hl->addWidget(event_count_label_);

    // NOTE: don't call on_tab_changed(0) here — content_stack_ isn't created yet.
    // Initial tab highlight is applied at the end of build_ui().
    return bar;
}

// ── Filter Panel ─────────────────────────────────────────────────────────────
QWidget* GeopoliticsScreen::build_filter_panel() {
    auto* panel = new QWidget(this);
    panel->setFixedWidth(220);
    panel->setStyleSheet(QString("background:%1; border-right:1px solid %2;")
        .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(10);

    // Title
    auto* title = new QLabel("FILTERS", panel);
    title->setStyleSheet(QString(
        "color:#FF0000; font-size:9px; font-weight:700; font-family:%1;"
        "letter-spacing:1px; padding-bottom:4px;"
        "border-bottom:1px solid %2;")
        .arg(ui::fonts::DATA_FAMILY).arg(ui::colors::BORDER_DIM));
    vl->addWidget(title);

    auto input_style = QString(
        "QLineEdit, QComboBox { background:%1; color:%2; border:1px solid %3;"
        "font-family:%4; font-size:%5px; padding:6px 8px; border-radius:2px; }"
        "QLineEdit:focus, QComboBox:focus { border-color:#FF0000; }")
        .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
        .arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL);

    auto label_style = QString("color:%1; font-size:8px; font-weight:700;"
        "font-family:%2; letter-spacing:1px;")
        .arg(ui::colors::TEXT_TERTIARY).arg(ui::fonts::DATA_FAMILY);

    // Country filter
    auto* country_lbl = new QLabel("COUNTRY", panel);
    country_lbl->setStyleSheet(label_style);
    vl->addWidget(country_lbl);
    country_edit_ = new QLineEdit(panel);
    country_edit_->setPlaceholderText("e.g. Ukraine");
    country_edit_->setStyleSheet(input_style);
    connect(country_edit_, &QLineEdit::returnPressed, this, &GeopoliticsScreen::on_apply_filters);
    vl->addWidget(country_edit_);

    // City filter
    auto* city_lbl = new QLabel("CITY", panel);
    city_lbl->setStyleSheet(label_style);
    vl->addWidget(city_lbl);
    city_edit_ = new QLineEdit(panel);
    city_edit_->setPlaceholderText("e.g. Kyiv");
    city_edit_->setStyleSheet(input_style);
    connect(city_edit_, &QLineEdit::returnPressed, this, &GeopoliticsScreen::on_apply_filters);
    vl->addWidget(city_edit_);

    // Category filter
    auto* cat_lbl = new QLabel("EVENT CATEGORY", panel);
    cat_lbl->setStyleSheet(label_style);
    vl->addWidget(cat_lbl);
    category_combo_ = new QComboBox(panel);
    category_combo_->setStyleSheet(input_style);
    category_combo_->addItem("All Categories", "");
    for (const auto& cat : event_categories())
        category_combo_->addItem(cat, cat);
    vl->addWidget(category_combo_);

    // Buttons
    auto* apply_btn = new QPushButton("APPLY", panel);
    apply_btn->setCursor(Qt::PointingHandCursor);
    apply_btn->setStyleSheet(QString(
        "QPushButton { background:#FF0000; color:%1; font-family:%2; font-size:%3px;"
        "font-weight:700; border:none; padding:8px; border-radius:2px; letter-spacing:1px; }"
        "QPushButton:hover { background:#CC0000; }")
        .arg(ui::colors::BG_BASE).arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL));
    connect(apply_btn, &QPushButton::clicked, this, &GeopoliticsScreen::on_apply_filters);
    vl->addWidget(apply_btn);

    auto* clear_btn = new QPushButton("CLEAR", panel);
    clear_btn->setCursor(Qt::PointingHandCursor);
    clear_btn->setStyleSheet(QString(
        "QPushButton { background:transparent; color:%1; font-family:%2; font-size:%3px;"
        "border:1px solid %4; padding:6px; border-radius:2px; }"
        "QPushButton:hover { background:%5; }")
        .arg(ui::colors::TEXT_SECONDARY).arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL)
        .arg(ui::colors::BORDER_DIM).arg(ui::colors::BG_HOVER));
    connect(clear_btn, &QPushButton::clicked, this, &GeopoliticsScreen::on_clear_filters);
    vl->addWidget(clear_btn);

    // Legend
    vl->addSpacing(12);
    auto* legend_title = new QLabel("LEGEND", panel);
    legend_title->setStyleSheet(label_style);
    vl->addWidget(legend_title);

    for (const auto& cat : event_categories()) {
        auto* row = new QWidget(panel);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 1, 0, 1);
        rl->setSpacing(6);
        auto* dot = new QWidget(row);
        dot->setFixedSize(8, 8);
        dot->setStyleSheet(QString("background:%1; border-radius:4px;")
            .arg(category_color(cat).name()));
        rl->addWidget(dot);
        auto* lbl = new QLabel(cat, row);
        lbl->setStyleSheet(QString("color:%1; font-size:8px; font-family:%2;")
            .arg(ui::colors::TEXT_SECONDARY).arg(ui::fonts::DATA_FAMILY));
        rl->addWidget(lbl, 1);
        vl->addWidget(row);
    }

    vl->addStretch();
    return panel;
}

// ── Status Bar ───────────────────────────────────────────────────────────────
QWidget* GeopoliticsScreen::build_status_bar() {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(24);
    bar->setStyleSheet(QString("background:%1; border-top:1px solid %2;")
        .arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(16);

    auto s = QString("color:%1; font-size:8px; font-family:%2;")
        .arg(ui::colors::TEXT_TERTIARY).arg(ui::fonts::DATA_FAMILY);
    auto sv = QString("color:%1; font-size:8px; font-weight:700; font-family:%2;")
        .arg(ui::colors::TEXT_PRIMARY).arg(ui::fonts::DATA_FAMILY);

    auto* lbl1 = new QLabel("SOURCE:", bar); lbl1->setStyleSheet(s);
    auto* val1 = new QLabel("NEWS-EVENTS API + HDX", bar); val1->setStyleSheet(sv);
    hl->addWidget(lbl1); hl->addWidget(val1);

    auto* lbl2 = new QLabel("ENGINE:", bar); lbl2->setStyleSheet(s);
    auto* val2 = new QLabel("PYTHON + C++", bar);
    val2->setStyleSheet(QString("color:%1; font-size:8px; font-weight:700; font-family:%2;")
        .arg(ui::colors::POSITIVE).arg(ui::fonts::DATA_FAMILY));
    hl->addWidget(lbl2); hl->addWidget(val2);

    hl->addStretch();

    status_label_ = new QLabel("READY", bar);
    status_label_->setStyleSheet(QString("color:%1; font-size:8px; font-weight:700; font-family:%2;")
        .arg(ui::colors::POSITIVE).arg(ui::fonts::DATA_FAMILY));
    hl->addWidget(status_label_);

    return bar;
}

// ── Actions ──────────────────────────────────────────────────────────────────
void GeopoliticsScreen::on_apply_filters() {
    auto country = country_edit_->text().trimmed();
    auto city = city_edit_->text().trimmed();
    auto category = category_combo_->currentData().toString();
    status_label_->setText("LOADING...");
    status_label_->setStyleSheet(QString("color:%1; font-size:8px; font-weight:700; font-family:%2;")
        .arg(ui::colors::WARNING).arg(ui::fonts::DATA_FAMILY));
    GeopoliticsService::instance().fetch_events(country, city, category);
}

void GeopoliticsScreen::on_clear_filters() {
    country_edit_->clear();
    city_edit_->clear();
    category_combo_->setCurrentIndex(0);
    on_apply_filters();
}

void GeopoliticsScreen::on_tab_changed(int index) {
    if (index < 0 || index >= tab_buttons_.size()) return;
    active_tab_ = index;
    if (content_stack_)
        content_stack_->setCurrentIndex(index);

    QStringList colors = {"#FF0000", "#00E5FF", "#9D4EDD", "#FFC400"};
    for (int i = 0; i < tab_buttons_.size(); ++i) {
        bool active = (i == index);
        tab_buttons_[i]->setStyleSheet(QString(
            "QPushButton { color:%1; font-size:9px; font-family:%2;"
            "padding:4px 10px; border:1px solid %3; border-radius:2px;"
            "background:%4; font-weight:%5; }"
            "QPushButton:hover { background:rgba(%6,0.1); }")
            .arg(active ? colors[i] : ui::colors::TEXT_TERTIARY)
            .arg(ui::fonts::DATA_FAMILY)
            .arg(active ? QString("rgba(%1,0.3)").arg(colors[i].mid(1)) : "transparent")
            .arg(active ? QString("rgba(%1,0.12)").arg(colors[i].mid(1)) : "transparent")
            .arg(active ? "700" : "400")
            .arg(colors[i].mid(1)));
    }
}

void GeopoliticsScreen::on_events_loaded(QVector<services::geo::NewsEvent> events, int total) {
    event_count_label_->setText(QString("%1 EVENTS").arg(total));
    auto color = total > 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE;
    event_count_label_->setStyleSheet(QString(
        "color:%1; font-size:9px; font-family:%2; padding:3px 8px;"
        "background:rgba(%3,0.08); border:1px solid rgba(%3,0.25);"
        "border-radius:2px; font-weight:700;")
        .arg(color).arg(ui::fonts::DATA_FAMILY)
        .arg(QString(color).mid(1)));
    status_label_->setText("READY");
    status_label_->setStyleSheet(QString("color:%1; font-size:8px; font-weight:700; font-family:%2;")
        .arg(ui::colors::POSITIVE).arg(ui::fonts::DATA_FAMILY));
    // Forward to monitor panel
    monitor_panel_->set_events(events);
}

void GeopoliticsScreen::on_error(const QString& context, const QString& message) {
    status_label_->setText("ERROR");
    status_label_->setStyleSheet(QString("color:%1; font-size:8px; font-weight:700; font-family:%2;")
        .arg(ui::colors::NEGATIVE).arg(ui::fonts::DATA_FAMILY));
    LOG_ERROR("Geopolitics", QString("[%1] %2").arg(context, message));
}

} // namespace fincept::screens
