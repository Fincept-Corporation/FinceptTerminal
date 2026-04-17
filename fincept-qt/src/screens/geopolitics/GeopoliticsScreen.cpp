// src/screens/geopolitics/GeopoliticsScreen.cpp
#include "screens/geopolitics/GeopoliticsScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/geopolitics/ConflictMonitorPanel.h"
#include "screens/geopolitics/HDXDataPanel.h"
#include "screens/geopolitics/RelationshipPanel.h"
#include "screens/geopolitics/TradeAnalysisPanel.h"
#include "services/geopolitics/GeopoliticsService.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QScrollArea>

namespace fincept::screens {

using namespace fincept::services::geo;

// ── Constructor ──────────────────────────────────────────────────────────────
GeopoliticsScreen::GeopoliticsScreen(QWidget* parent) : QWidget(parent) {
    build_ui();
    connect_service();

    refresh_timer_ = new QTimer(this);
    refresh_timer_->setInterval(5 * 60 * 1000);
    connect(refresh_timer_, &QTimer::timeout, this, &GeopoliticsScreen::on_apply_filters);

    clock_timer_ = new QTimer(this);
    clock_timer_->setInterval(1000);

    connect(&fincept::ui::ThemeManager::instance(), &fincept::ui::ThemeManager::theme_changed, this,
            [this](const fincept::ui::ThemeTokens&) { refresh_theme(); });

    LOG_INFO("Geopolitics", "Screen constructed");
}

void GeopoliticsScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    refresh_timer_->start();
    clock_timer_->start();
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
    clock_timer_->stop();
}

void GeopoliticsScreen::refresh_theme() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    // Re-apply tab highlight so colors remain correct after theme change
    on_tab_changed(active_tab_);
}

void GeopoliticsScreen::connect_service() {
    auto& svc = GeopoliticsService::instance();
    connect(&svc, &GeopoliticsService::events_loaded, this, &GeopoliticsScreen::on_events_loaded);
    connect(&svc, &GeopoliticsService::error_occurred, this, &GeopoliticsScreen::on_error);
    connect(&svc, &GeopoliticsService::categories_loaded, this, [this](QVector<UniqueCategory> cats) {
        category_combo_->clear();
        category_combo_->addItem("All Categories", "");
        for (const auto& c : cats)
            category_combo_->addItem(QString("%1 (%2)").arg(c.category).arg(c.event_count), c.category);
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

    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    on_tab_changed(0);
}

// ── Top Bar ──────────────────────────────────────────────────────────────────
QWidget* GeopoliticsScreen::build_top_bar() {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(48);
    bar->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);
    hl->setSpacing(12);

    struct TabDef {
        QString label;
        QString color;
    };
    const QVector<TabDef> tabs = {
        {"MONITOR", ui::colors::NEGATIVE},
        {"HDX DATA", ui::colors::CYAN},
        {"RELATIONS", ui::colors::INFO},
        {"TRADE", ui::colors::WARNING},
    };

    for (int i = 0; i < tabs.size(); ++i) {
        auto* btn = new QPushButton(tabs[i].label, bar);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString("QPushButton { color:%1; font-size:%2px; font-family:%3;"
                                   "padding:4px 14px; border:none;"
                                   "background:transparent; font-weight:400; }"
                                   "QPushButton:hover { color:%4; background:rgba(%5,0.06); }")
                               .arg(ui::colors::TEXT_TERTIARY())
                               .arg(ui::fonts::TINY)
                               .arg(ui::fonts::DATA_FAMILY)
                               .arg(tabs[i].color)
                               .arg(tabs[i].color.mid(1)));
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_tab_changed(i); });
        hl->addWidget(btn);
        tab_buttons_.append(btn);
    }

    hl->addStretch(1);

    auto* clock_label = new QLabel("UTC --:--", bar);
    clock_label->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; min-width:68px;")
                                   .arg(ui::colors::TEXT_TERTIARY())
                                   .arg(ui::fonts::TINY)
                                   .arg(ui::fonts::DATA_FAMILY));
    hl->addWidget(clock_label);

    connect(clock_timer_, &QTimer::timeout, this, [clock_label]() {
        clock_label->setText(QString("UTC %1").arg(QDateTime::currentDateTimeUtc().toString("HH:mm")));
    });
    clock_label->setText(QString("UTC %1").arg(QDateTime::currentDateTimeUtc().toString("HH:mm")));

    auto* div2 = new QWidget(bar);
    div2->setFixedSize(1, 20);
    div2->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM()));
    hl->addWidget(div2);

    event_count_label_ = new QLabel("0 EVENTS", bar);
    event_count_label_->setFixedHeight(22);
    {
        QColor neg(ui::colors::NEGATIVE());
        auto neg_rgb = QString("%1,%2,%3").arg(neg.red()).arg(neg.green()).arg(neg.blue());
        event_count_label_->setStyleSheet(
            QString("color:%1; font-size:%2px; font-family:%3; padding:2px 6px;"
                    "background:rgba(%4,0.08); border:1px solid rgba(%4,0.25); font-weight:700;")
                .arg(ui::colors::NEGATIVE())
                .arg(ui::fonts::TINY)
                .arg(ui::fonts::DATA_FAMILY())
                .arg(neg_rgb));
    }
    hl->addWidget(event_count_label_);

    return bar;
}

// ── Filter Panel ─────────────────────────────────────────────────────────────
QWidget* GeopoliticsScreen::build_filter_panel() {
    auto* panel = new QWidget(this);
    panel->setFixedWidth(220);
    panel->setStyleSheet(
        QString("background:%1; border-right:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(8);

    auto* title = new QLabel("FILTERS", panel);
    title->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;"
                                 "letter-spacing:1px; padding-bottom:4px; border-bottom:1px solid %4;")
                             .arg(ui::colors::NEGATIVE())
                             .arg(ui::fonts::TINY)
                             .arg(ui::fonts::DATA_FAMILY())
                             .arg(ui::colors::BORDER_DIM()));
    vl->addWidget(title);

    auto input_style = QString("QLineEdit, QComboBox { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:5px 8px; }"
                               "QLineEdit:focus, QComboBox:focus { border-color:%6; }")
                           .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                           .arg(ui::fonts::DATA_FAMILY())
                           .arg(ui::fonts::SMALL)
                           .arg(ui::colors::NEGATIVE());

    auto label_style = QString("color:%1; font-size:%2px; font-weight:700; font-family:%3; letter-spacing:1px;")
                           .arg(ui::colors::TEXT_TERTIARY())
                           .arg(ui::fonts::TINY)
                           .arg(ui::fonts::DATA_FAMILY);

    auto* country_lbl = new QLabel("COUNTRY", panel);
    country_lbl->setStyleSheet(label_style);
    vl->addWidget(country_lbl);
    country_edit_ = new QLineEdit(panel);
    country_edit_->setPlaceholderText("e.g. Ukraine");
    country_edit_->setStyleSheet(input_style);
    connect(country_edit_, &QLineEdit::returnPressed, this, &GeopoliticsScreen::on_apply_filters);
    vl->addWidget(country_edit_);

    auto* city_lbl = new QLabel("CITY", panel);
    city_lbl->setStyleSheet(label_style);
    vl->addWidget(city_lbl);
    city_edit_ = new QLineEdit(panel);
    city_edit_->setPlaceholderText("e.g. Kyiv");
    city_edit_->setStyleSheet(input_style);
    connect(city_edit_, &QLineEdit::returnPressed, this, &GeopoliticsScreen::on_apply_filters);
    vl->addWidget(city_edit_);

    auto* cat_lbl = new QLabel("CATEGORY", panel);
    cat_lbl->setStyleSheet(label_style);
    vl->addWidget(cat_lbl);
    category_combo_ = new QComboBox(panel);
    category_combo_->setStyleSheet(input_style);
    category_combo_->addItem("All Categories", "");
    for (const auto& cat : event_categories())
        category_combo_->addItem(cat, cat);
    vl->addWidget(category_combo_);

    vl->addSpacing(4);

    auto* apply_btn = new QPushButton("APPLY FILTERS", panel);
    apply_btn->setCursor(Qt::PointingHandCursor);
    {
        QColor neg(ui::colors::NEGATIVE());
        apply_btn->setStyleSheet(QString("QPushButton { background:%1; color:%2; font-family:%3; font-size:%4px;"
                                         "font-weight:700; border:none; padding:6px 12px; }"
                                         "QPushButton:hover { background:%5; }")
                                     .arg(ui::colors::NEGATIVE())
                                     .arg(ui::colors::BG_BASE())
                                     .arg(ui::fonts::DATA_FAMILY())
                                     .arg(ui::fonts::SMALL)
                                     .arg(neg.darker(120).name()));
    }
    connect(apply_btn, &QPushButton::clicked, this, &GeopoliticsScreen::on_apply_filters);
    vl->addWidget(apply_btn);

    auto* clear_btn = new QPushButton("CLEAR", panel);
    clear_btn->setCursor(Qt::PointingHandCursor);
    clear_btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; font-family:%2; font-size:%3px;"
                                     "border:1px solid %4; padding:5px 12px; }"
                                     "QPushButton:hover { background:%5; }")
                                 .arg(ui::colors::TEXT_SECONDARY())
                                 .arg(ui::fonts::DATA_FAMILY)
                                 .arg(ui::fonts::SMALL)
                                 .arg(ui::colors::BORDER_DIM())
                                 .arg(ui::colors::BG_HOVER()));
    connect(clear_btn, &QPushButton::clicked, this, &GeopoliticsScreen::on_clear_filters);
    vl->addWidget(clear_btn);

    auto* sep = new QWidget(panel);
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM()));
    vl->addSpacing(6);
    vl->addWidget(sep);
    vl->addSpacing(6);

    auto* legend_title = new QLabel("LEGEND", panel);
    legend_title->setStyleSheet(
        QString("color:%1; font-size:%2px; font-weight:700; font-family:%3; letter-spacing:1px;")
            .arg(ui::colors::TEXT_TERTIARY())
            .arg(ui::fonts::TINY)
            .arg(ui::fonts::DATA_FAMILY));
    vl->addWidget(legend_title);

    for (const auto& cat : event_categories()) {
        auto* row = new QWidget(panel);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 1, 0, 1);
        rl->setSpacing(6);
        auto* dot = new QWidget(row);
        dot->setFixedSize(8, 8);
        dot->setStyleSheet(QString("background:%1; border-radius:4px;").arg(category_color(cat).name()));
        rl->addWidget(dot);
        auto* lbl = new QLabel(cat, row);
        lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                               .arg(ui::colors::TEXT_SECONDARY())
                               .arg(ui::fonts::SMALL)
                               .arg(ui::fonts::DATA_FAMILY));
        rl->addWidget(lbl, 1);
        vl->addWidget(row);
    }

    vl->addStretch();
    return panel;
}

// ── Status Bar ───────────────────────────────────────────────────────────────
QWidget* GeopoliticsScreen::build_status_bar() {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(26);
    bar->setStyleSheet(
        QString("background:%1; border-top:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(16);

    auto s = QString("color:%1; font-size:%2px; font-family:%3;")
                 .arg(ui::colors::TEXT_TERTIARY())
                 .arg(ui::fonts::TINY)
                 .arg(ui::fonts::DATA_FAMILY);
    auto sv = QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;")
                  .arg(ui::colors::TEXT_PRIMARY())
                  .arg(ui::fonts::TINY)
                  .arg(ui::fonts::DATA_FAMILY);

    auto* lbl1 = new QLabel("SOURCE:", bar);
    lbl1->setStyleSheet(s);
    auto* val1 = new QLabel("NEWS-EVENTS API + HDX", bar);
    val1->setStyleSheet(sv);
    hl->addWidget(lbl1);
    hl->addWidget(val1);

    auto* lbl2 = new QLabel("ENGINE:", bar);
    lbl2->setStyleSheet(s);
    auto* val2 = new QLabel("PYTHON + C++", bar);
    val2->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;")
                            .arg(ui::colors::POSITIVE())
                            .arg(ui::fonts::TINY)
                            .arg(ui::fonts::DATA_FAMILY));
    hl->addWidget(lbl2);
    hl->addWidget(val2);

    hl->addStretch();

    status_label_ = new QLabel("READY", bar);
    status_label_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;")
                                     .arg(ui::colors::POSITIVE())
                                     .arg(ui::fonts::TINY)
                                     .arg(ui::fonts::DATA_FAMILY));
    hl->addWidget(status_label_);

    return bar;
}

// ── Actions ──────────────────────────────────────────────────────────────────
void GeopoliticsScreen::on_apply_filters() {
    auto country = country_edit_->text().trimmed();
    auto city = city_edit_->text().trimmed();
    auto category = category_combo_->currentData().toString();
    status_label_->setText("LOADING...");
    status_label_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;")
                                     .arg(ui::colors::WARNING())
                                     .arg(ui::fonts::TINY)
                                     .arg(ui::fonts::DATA_FAMILY));
    GeopoliticsService::instance().fetch_events(country, city, category);
}

void GeopoliticsScreen::on_clear_filters() {
    country_edit_->clear();
    city_edit_->clear();
    category_combo_->setCurrentIndex(0);
    on_apply_filters();
}

void GeopoliticsScreen::on_tab_changed(int index) {
    if (index < 0 || index >= tab_buttons_.size())
        return;
    active_tab_ = index;
    if (content_stack_)
        content_stack_->setCurrentIndex(index);
    ScreenStateManager::instance().notify_changed(this);

    const QStringList colors = {ui::colors::NEGATIVE, ui::colors::CYAN, ui::colors::INFO, ui::colors::WARNING};
    for (int i = 0; i < tab_buttons_.size(); ++i) {
        const bool active = (i == index);
        QColor c(colors[i]);
        auto rgb = QString("%1,%2,%3").arg(c.red()).arg(c.green()).arg(c.blue());
        if (active) {
            tab_buttons_[i]->setStyleSheet(
                QString("QPushButton { color:%1; font-size:%2px; font-family:%3;"
                        "padding:4px 14px;"
                        "border-bottom:2px solid %1; border-top:none; border-left:none; border-right:none;"
                        "background:rgba(%4,0.06); font-weight:700; }"
                        "QPushButton:hover { background:rgba(%4,0.10); }")
                    .arg(colors[i])
                    .arg(ui::fonts::TINY)
                    .arg(ui::fonts::DATA_FAMILY)
                    .arg(rgb));
        } else {
            tab_buttons_[i]->setStyleSheet(QString("QPushButton { color:%1; font-size:%2px; font-family:%3;"
                                                   "padding:4px 14px; border:none;"
                                                   "background:transparent; font-weight:400; }"
                                                   "QPushButton:hover { color:%4; background:rgba(%5,0.06); }")
                                               .arg(ui::colors::TEXT_TERTIARY())
                                               .arg(ui::fonts::TINY)
                                               .arg(ui::fonts::DATA_FAMILY)
                                               .arg(colors[i])
                                               .arg(rgb));
        }
    }
}

void GeopoliticsScreen::on_events_loaded(QVector<services::geo::NewsEvent> events, int total) {
    event_count_label_->setText(QString("%1 EVENTS").arg(total));
    const QString color = total > 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE();
    QColor clr(color);
    auto clr_rgb = QString("%1,%2,%3").arg(clr.red()).arg(clr.green()).arg(clr.blue());
    event_count_label_->setStyleSheet(
        QString("color:%1; font-size:%2px; font-family:%3; padding:2px 6px;"
                "background:rgba(%4,0.08); border:1px solid rgba(%4,0.25); font-weight:700;")
            .arg(color)
            .arg(ui::fonts::TINY)
            .arg(ui::fonts::DATA_FAMILY)
            .arg(clr_rgb));
    status_label_->setText("READY");
    status_label_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;")
                                     .arg(ui::colors::POSITIVE())
                                     .arg(ui::fonts::TINY)
                                     .arg(ui::fonts::DATA_FAMILY));
    monitor_panel_->set_events(events);
}

void GeopoliticsScreen::on_error(const QString& context, const QString& message) {
    status_label_->setText("ERROR");
    status_label_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;")
                                     .arg(ui::colors::NEGATIVE())
                                     .arg(ui::fonts::TINY)
                                     .arg(ui::fonts::DATA_FAMILY));
    LOG_ERROR("Geopolitics", QString("[%1] %2").arg(context, message));
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap GeopoliticsScreen::save_state() const {
    return {{"tab_index", active_tab_}};
}

void GeopoliticsScreen::restore_state(const QVariantMap& state) {
    const int idx = state.value("tab_index", 0).toInt();
    if (idx >= 0 && idx < tab_buttons_.size())
        on_tab_changed(idx);
}

} // namespace fincept::screens
