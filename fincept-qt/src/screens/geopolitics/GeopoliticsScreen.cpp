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

#include <QCompleter>
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QScrollArea>

namespace fincept::screens {

using namespace fincept::services::geo;

namespace {
// Page size the screen requests - passed explicitly so the request key it records
// matches the one the service stamps on the reply.
constexpr int kGeoScreenEventsLimit = 100;
} // namespace

// ── Constructor ──────────────────────────────────────────────────────────────
GeopoliticsScreen::GeopoliticsScreen(QWidget* parent) : QWidget(parent) {
    // The timers must exist before build_ui(): build_top_bar() connects the UTC clock
    // label to clock_timer_, and with the timer still null that connect() was a no-op,
    // so the clock froze at the minute the screen was constructed.
    refresh_timer_ = new QTimer(this);
    refresh_timer_->setInterval(5 * 60 * 1000);
    connect(refresh_timer_, &QTimer::timeout, this, &GeopoliticsScreen::on_auto_refresh);

    clock_timer_ = new QTimer(this);
    clock_timer_->setInterval(1000);

    build_ui();
    connect_service();

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
        // Fetch latest events with no filter — newest first, server side.
        on_apply_filters();
        GeopoliticsService::instance().fetch_unique_countries();
        GeopoliticsService::instance().fetch_unique_categories();
        GeopoliticsService::instance().fetch_unique_cities();
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

void GeopoliticsScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void GeopoliticsScreen::retranslateUi() {
    // Tab buttons — re-apply the fixed labels in declared order.
    const QStringList labels = {tr("MONITOR"), tr("HDX DATA"), tr("RELATIONS"), tr("TRADE")};
    for (int i = 0; i < tab_buttons_.size() && i < labels.size(); ++i)
        if (tab_buttons_[i])
            tab_buttons_[i]->setText(labels[i]);

    // Filter panel
    if (filters_title_)
        filters_title_->setText(tr("FILTERS"));
    if (country_lbl_)
        country_lbl_->setText(tr("COUNTRY"));
    if (city_lbl_)
        city_lbl_->setText(tr("CITY"));
    if (category_lbl_)
        category_lbl_->setText(tr("CATEGORY"));
    if (country_edit_)
        country_edit_->setPlaceholderText(tr("e.g. Ukraine"));
    if (city_edit_)
        city_edit_->setPlaceholderText(tr("e.g. Kyiv"));
    if (apply_btn_)
        apply_btn_->setText(tr("APPLY FILTERS"));
    if (clear_btn_)
        clear_btn_->setText(tr("CLEAR"));
    if (prev_page_btn_)
        prev_page_btn_->setText(tr("◀ PREV"));
    if (next_page_btn_)
        next_page_btn_->setText(tr("NEXT ▶"));
    update_pager();
    if (legend_title_)
        legend_title_->setText(tr("LEGEND"));
    // Combo's first row is the fixed "All Categories" entry (others are data).
    if (category_combo_ && category_combo_->count() > 0)
        category_combo_->setItemText(0, tr("All Categories"));

    // Status bar — static label keys (values are tech/brand identifiers).
    if (status_source_lbl_)
        status_source_lbl_->setText(tr("SOURCE:"));
    if (status_engine_lbl_)
        status_engine_lbl_->setText(tr("ENGINE:"));

    // Dynamic state labels (status, credits, event count) reflect the most
    // recent data fetch and are re-applied on the next refresh.
}

void GeopoliticsScreen::connect_service() {
    auto& svc = GeopoliticsService::instance();
    connect(&svc, &GeopoliticsService::events_loaded, this, &GeopoliticsScreen::on_events_loaded);
    connect(&svc, &GeopoliticsService::error_occurred, this, &GeopoliticsScreen::on_error);
    connect(&svc, &GeopoliticsService::categories_loaded, this, &GeopoliticsScreen::on_categories_loaded);
    // The service already fetched these on first show but nothing consumed them -
    // they now feed the country / city autocomplete.
    connect(&svc, &GeopoliticsService::countries_loaded, this, &GeopoliticsScreen::on_countries_loaded);
    connect(&svc, &GeopoliticsService::cities_loaded, this, &GeopoliticsScreen::on_cities_loaded);
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

    // Relations tab drill-downs: a conflict card jumps to that country's events or
    // its HDX datasets, a crisis card to the matching HDX topic.
    connect(relationship_panel_, &RelationshipPanel::events_requested, this, [this](const QString& country) {
        country_edit_->setText(country);
        city_edit_->clear();
        category_combo_->setCurrentIndex(0);
        on_tab_changed(0);
        on_apply_filters();
    });
    connect(relationship_panel_, &RelationshipPanel::hdx_country_requested, this, [this](const QString& country) {
        // Start the search before the tab is shown so the panel's first-show
        // default fetch doesn't fire as well.
        hdx_panel_->explore_country(country);
        on_tab_changed(1);
    });
    connect(relationship_panel_, &RelationshipPanel::hdx_topic_requested, this, [this](const QString& topic) {
        hdx_panel_->explore_topic(topic);
        on_tab_changed(1);
    });

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
        {tr("MONITOR"), ui::colors::NEGATIVE},
        {tr("HDX DATA"), ui::colors::CYAN},
        {tr("RELATIONS"), ui::colors::INFO},
        {tr("TRADE"), ui::colors::WARNING},
    };
    tab_labels_.clear();
    for (const auto& t : tabs)
        tab_labels_ << t.label;

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

    clock_label_ = new QLabel(tr("UTC --:--"), bar);
    clock_label_->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; min-width:68px;")
                                    .arg(ui::colors::TEXT_TERTIARY())
                                    .arg(ui::fonts::TINY)
                                    .arg(ui::fonts::DATA_FAMILY));
    hl->addWidget(clock_label_);

    connect(clock_timer_, &QTimer::timeout, this, [this]() {
        if (clock_label_)
            clock_label_->setText(tr("UTC %1").arg(QDateTime::currentDateTimeUtc().toString("HH:mm")));
    });
    clock_label_->setText(tr("UTC %1").arg(QDateTime::currentDateTimeUtc().toString("HH:mm")));

    auto* div2 = new QWidget(bar);
    div2->setFixedSize(1, 20);
    div2->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM()));
    hl->addWidget(div2);

    event_count_label_ = new QLabel(tr("0 EVENTS"), bar);
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

    filters_title_ = new QLabel(tr("FILTERS"), panel);
    filters_title_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;"
                                          "letter-spacing:1px; padding-bottom:4px; border-bottom:1px solid %4;")
                                      .arg(ui::colors::NEGATIVE())
                                      .arg(ui::fonts::TINY)
                                      .arg(ui::fonts::DATA_FAMILY())
                                      .arg(ui::colors::BORDER_DIM()));
    vl->addWidget(filters_title_);

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

    country_lbl_ = new QLabel(tr("COUNTRY"), panel);
    country_lbl_->setStyleSheet(label_style);
    vl->addWidget(country_lbl_);
    country_edit_ = new QLineEdit(panel);
    country_edit_->setPlaceholderText(tr("e.g. Ukraine"));
    country_edit_->setStyleSheet(input_style);
    country_edit_->setAccessibleName(tr("Filter by country"));
    country_edit_->setClearButtonEnabled(true);
    connect(country_edit_, &QLineEdit::returnPressed, this, &GeopoliticsScreen::on_apply_filters);
    country_model_ = new QStringListModel(this);
    auto* country_completer = new QCompleter(country_model_, this);
    country_completer->setCaseSensitivity(Qt::CaseInsensitive);
    country_completer->setFilterMode(Qt::MatchContains);
    country_edit_->setCompleter(country_completer);
    vl->addWidget(country_edit_);

    city_lbl_ = new QLabel(tr("CITY"), panel);
    city_lbl_->setStyleSheet(label_style);
    vl->addWidget(city_lbl_);
    city_edit_ = new QLineEdit(panel);
    city_edit_->setPlaceholderText(tr("e.g. Kyiv"));
    city_edit_->setStyleSheet(input_style);
    city_edit_->setAccessibleName(tr("Filter by city"));
    city_edit_->setClearButtonEnabled(true);
    connect(city_edit_, &QLineEdit::returnPressed, this, &GeopoliticsScreen::on_apply_filters);
    city_model_ = new QStringListModel(this);
    auto* city_completer = new QCompleter(city_model_, this);
    city_completer->setCaseSensitivity(Qt::CaseInsensitive);
    city_completer->setFilterMode(Qt::MatchContains);
    city_edit_->setCompleter(city_completer);
    vl->addWidget(city_edit_);

    category_lbl_ = new QLabel(tr("CATEGORY"), panel);
    category_lbl_->setStyleSheet(label_style);
    vl->addWidget(category_lbl_);
    category_combo_ = new QComboBox(panel);
    category_combo_->setStyleSheet(input_style);
    category_combo_->setAccessibleName(tr("Filter by event category"));
    // Populated from API once categories_loaded fires — start with a single
    // "All" entry so the combo renders before the network round-trip.
    category_combo_->addItem(tr("All Categories"), "");
    vl->addWidget(category_combo_);

    // Keyboard order across the filter column.
    QWidget::setTabOrder(country_edit_, city_edit_);
    QWidget::setTabOrder(city_edit_, category_combo_);

    vl->addSpacing(4);

    apply_btn_ = new QPushButton(tr("APPLY FILTERS"), panel);
    apply_btn_->setCursor(Qt::PointingHandCursor);
    {
        QColor neg(ui::colors::NEGATIVE());
        apply_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%2; font-family:%3; font-size:%4px;"
                                          "font-weight:700; border:none; padding:6px 12px; }"
                                          "QPushButton:hover { background:%5; }")
                                      .arg(ui::colors::NEGATIVE())
                                      .arg(ui::colors::BG_BASE())
                                      .arg(ui::fonts::DATA_FAMILY())
                                      .arg(ui::fonts::SMALL)
                                      .arg(neg.darker(120).name()));
    }
    connect(apply_btn_, &QPushButton::clicked, this, &GeopoliticsScreen::on_apply_filters);
    vl->addWidget(apply_btn_);

    clear_btn_ = new QPushButton(tr("CLEAR"), panel);
    clear_btn_->setCursor(Qt::PointingHandCursor);
    clear_btn_->setStyleSheet(QString("QPushButton { background:transparent; color:%1; font-family:%2; font-size:%3px;"
                                      "border:1px solid %4; padding:5px 12px; }"
                                      "QPushButton:hover { background:%5; }")
                                  .arg(ui::colors::TEXT_SECONDARY())
                                  .arg(ui::fonts::DATA_FAMILY)
                                  .arg(ui::fonts::SMALL)
                                  .arg(ui::colors::BORDER_DIM())
                                  .arg(ui::colors::BG_HOVER()));
    connect(clear_btn_, &QPushButton::clicked, this, &GeopoliticsScreen::on_clear_filters);
    vl->addWidget(clear_btn_);

    auto* sep = new QWidget(panel);
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM()));
    vl->addSpacing(6);
    vl->addWidget(sep);
    vl->addSpacing(6);

    legend_title_ = new QLabel(tr("LEGEND"), panel);
    legend_title_->setStyleSheet(
        QString("color:%1; font-size:%2px; font-weight:700; font-family:%3; letter-spacing:1px;")
            .arg(ui::colors::TEXT_TERTIARY())
            .arg(ui::fonts::TINY)
            .arg(ui::fonts::DATA_FAMILY));
    vl->addWidget(legend_title_);

    // Legend rows are added by rebuild_legend() once categories_loaded fires.
    // Wrapped in a scroll area because the API now exposes ~26 categories;
    // without scrolling the legend would push the filter panel taller than
    // the viewport, which forces the parent screen to scroll and breaks the
    // map widget's painting (the map paints into whatever screen-space slot
    // the layout gives it on first show).
    auto* legend_scroll = new QScrollArea(panel);
    legend_scroll->setWidgetResizable(true);
    legend_scroll->setFrameShape(QFrame::NoFrame);
    legend_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    legend_scroll->setStyleSheet(QString("QScrollArea { background:transparent; border:none; }"
                                         "QScrollBar:vertical { background:%1; width:6px; }"
                                         "QScrollBar::handle:vertical { background:%2; border-radius:3px; }"
                                         "QScrollBar::add-line, QScrollBar::sub-line { height:0px; }")
                                     .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_MED()));

    legend_container_ = new QWidget(legend_scroll);
    legend_container_->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_SURFACE()));
    legend_layout_ = new QVBoxLayout(legend_container_);
    legend_layout_->setContentsMargins(0, 0, 4, 0);
    legend_layout_->setSpacing(2);
    legend_scroll->setWidget(legend_container_);
    vl->addWidget(legend_scroll, 1); // stretch — legend takes remaining space

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

    status_source_lbl_ = new QLabel(tr("SOURCE:"), bar);
    status_source_lbl_->setStyleSheet(s);
    // Tech/brand identifier — shown verbatim, not translated.
    status_source_val_ = new QLabel("NEWS-EVENTS API + HDX", bar);
    status_source_val_->setStyleSheet(sv);
    hl->addWidget(status_source_lbl_);
    hl->addWidget(status_source_val_);

    status_engine_lbl_ = new QLabel(tr("ENGINE:"), bar);
    status_engine_lbl_->setStyleSheet(s);
    // Tech/brand identifier — shown verbatim, not translated.
    status_engine_val_ = new QLabel("PYTHON + C++", bar);
    status_engine_val_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;")
                                          .arg(ui::colors::POSITIVE())
                                          .arg(ui::fonts::TINY)
                                          .arg(ui::fonts::DATA_FAMILY));
    hl->addWidget(status_engine_lbl_);
    hl->addWidget(status_engine_val_);

    hl->addStretch();

    // PREV / PAGE x / y / NEXT. One scoped stylesheet covers all three widgets.
    auto* pager = new QWidget(bar);
    pager->setStyleSheet(QString("QPushButton { color:%1; background:transparent; border:1px solid %2; padding:0 8px;"
                                 "font-size:%3px; font-family:%4; font-weight:700; }"
                                 "QPushButton:hover:enabled { color:%5; border-color:%5; }"
                                 "QPushButton:disabled { color:%6; border-color:%6; }"
                                 "QLabel { color:%1; font-size:%3px; font-family:%4; font-weight:700; }")
                             .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM())
                             .arg(ui::fonts::TINY)
                             .arg(ui::fonts::DATA_FAMILY())
                             .arg(ui::colors::NEGATIVE(), ui::colors::TEXT_TERTIARY()));
    auto* pl = new QHBoxLayout(pager);
    pl->setContentsMargins(0, 0, 0, 0);
    pl->setSpacing(6);
    prev_page_btn_ = new QPushButton(tr("◀ PREV"), pager);
    next_page_btn_ = new QPushButton(tr("NEXT ▶"), pager);
    page_lbl_ = new QLabel(tr("PAGE %1").arg(1), pager);
    for (auto* b : {prev_page_btn_, next_page_btn_}) {
        b->setFixedHeight(18);
        b->setCursor(Qt::PointingHandCursor);
    }
    prev_page_btn_->setToolTip(tr("Newer events (previous page)"));
    next_page_btn_->setToolTip(tr("Older events (next page) — each page uses API credits"));
    connect(prev_page_btn_, &QPushButton::clicked, this, &GeopoliticsScreen::on_prev_page);
    connect(next_page_btn_, &QPushButton::clicked, this, &GeopoliticsScreen::on_next_page);
    pl->addWidget(prev_page_btn_);
    pl->addWidget(page_lbl_);
    pl->addWidget(next_page_btn_);
    hl->addWidget(pager);
    update_pager();

    credits_label_ = new QLabel(tr("CREDITS: —"), bar);
    credits_label_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;")
                                      .arg(ui::colors::TEXT_TERTIARY())
                                      .arg(ui::fonts::TINY)
                                      .arg(ui::fonts::DATA_FAMILY));
    hl->addWidget(credits_label_);

    status_label_ = new QLabel(tr("READY"), bar);
    set_status_color(ui::colors::POSITIVE());
    hl->addWidget(status_label_);

    return bar;
}

// ── Actions ──────────────────────────────────────────────────────────────────
void GeopoliticsScreen::on_apply_filters() {
    request_events(1, /*reuse_filters=*/false);
}

void GeopoliticsScreen::on_prev_page() {
    if (current_page_ > 1)
        request_events(current_page_ - 1, /*reuse_filters=*/true);
}

void GeopoliticsScreen::on_next_page() {
    if (current_page_ < total_pages_)
        request_events(current_page_ + 1, /*reuse_filters=*/true);
}

void GeopoliticsScreen::on_auto_refresh() {
    // Refresh the page the user is on (and the filters they applied), not whatever is
    // typed in the boxes right now.
    request_events(current_page_, /*reuse_filters=*/true);
    auto_refresh_pending_ = true;
}

void GeopoliticsScreen::request_events(int page, bool reuse_filters) {
    if (!reuse_filters) {
        applied_country_ = country_edit_->text().trimmed();
        applied_city_ = city_edit_->text().trimmed();
        applied_category_ = category_combo_->currentData().toString();
    }
    status_label_->setText(tr("LOADING..."));
    set_status_color(ui::colors::WARNING());
    // A manual request always re-frames the map; on_auto_refresh() flips this back
    // right after it calls us.
    auto_refresh_pending_ = false;
    events_inflight_ = true;
    update_pager();
    last_request_key_ =
        events_request_key(applied_country_, applied_city_, applied_category_, kGeoScreenEventsLimit, page, {}, {}, {});
    GeopoliticsService::instance().fetch_events(applied_country_, applied_city_, applied_category_,
                                                kGeoScreenEventsLimit, page);
}

void GeopoliticsScreen::update_pager() {
    if (!page_lbl_ || !prev_page_btn_ || !next_page_btn_)
        return;
    page_lbl_->setText(total_pages_ > 0 ? tr("PAGE %1 / %2").arg(current_page_).arg(total_pages_)
                                        : tr("PAGE %1").arg(current_page_));
    prev_page_btn_->setEnabled(!events_inflight_ && current_page_ > 1);
    next_page_btn_->setEnabled(!events_inflight_ && current_page_ < total_pages_);
}

void GeopoliticsScreen::set_status_color(const QString& color) {
    if (!status_label_)
        return;
    status_label_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;")
                                     .arg(color)
                                     .arg(ui::fonts::TINY)
                                     .arg(ui::fonts::DATA_FAMILY));
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

void GeopoliticsScreen::on_events_loaded(services::geo::EventsPage page) {
    // Shared signal: the dashboard's hub refresh and MCP tools fire it too. Only
    // the reply to our own last request may replace the (possibly filtered) table.
    if (page.request_key != last_request_key_)
        return;
    events_inflight_ = false;
    current_page_ = qMax(1, page.current_page);
    total_pages_ = page.total_pages;
    update_pager();
    const int shown = page.events.size();
    const int total = page.total_events;
    int mapped = 0;
    for (const auto& ev : page.events)
        if (ev.has_coords)
            ++mapped;

    auto fmt_count = [](int n) -> QString {
        if (n >= 1'000'000)
            return QString::number(n / 1'000'000.0, 'f', 1) + "M";
        if (n >= 1'000)
            return QString::number(n / 1'000.0, 'f', 1) + "K";
        return QString::number(n);
    };
    // "30 MAPPED / 100 LOADED · 1.2K TOTAL" — surfaces the API gap (most events
    // arrive without lat/lng, so the map only ever shows the geocoded subset).
    event_count_label_->setText(tr("%1 MAPPED / %2 LOADED").arg(fmt_count(mapped), fmt_count(shown)));
    event_count_label_->setToolTip(
        tr("Events on map: %1\nEvents loaded: %2\nTotal in API: %3").arg(mapped).arg(shown).arg(total));
    const QString color = mapped > 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE();
    QColor clr(color);
    auto clr_rgb = QString("%1,%2,%3").arg(clr.red()).arg(clr.green()).arg(clr.blue());
    event_count_label_->setStyleSheet(
        QString("color:%1; font-size:%2px; font-family:%3; padding:2px 6px;"
                "background:rgba(%4,0.08); border:1px solid rgba(%4,0.25); font-weight:700;")
            .arg(color)
            .arg(ui::fonts::TINY)
            .arg(ui::fonts::DATA_FAMILY)
            .arg(clr_rgb));

    // Credits — colour-coded so depletion is glanceable.
    if (page.remaining_credits >= 0 && credits_label_) {
        const int rc = page.remaining_credits;
        const QString credit_color = rc < 20    ? ui::colors::NEGATIVE()
                                     : rc < 100 ? ui::colors::WARNING()
                                                : ui::colors::POSITIVE();
        credits_label_->setText(tr("CREDITS: %1").arg(rc));
        credits_label_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;")
                                          .arg(credit_color)
                                          .arg(ui::fonts::TINY)
                                          .arg(ui::fonts::DATA_FAMILY));
    }

    status_label_->setText(tr("READY"));
    status_label_->setToolTip(QString()); // clear any stale error detail
    set_status_color(ui::colors::POSITIVE());
    // The 5-minute refresh must not throw away the user's pan/zoom.
    monitor_panel_->set_events(page.events, /*fit_map=*/!auto_refresh_pending_);
    auto_refresh_pending_ = false;
}

void GeopoliticsScreen::on_countries_loaded(QVector<services::geo::UniqueCountry> countries) {
    if (!country_model_)
        return;
    // Most active countries first so the popup leads with the useful ones.
    std::sort(countries.begin(), countries.end(),
              [](const services::geo::UniqueCountry& a, const services::geo::UniqueCountry& b) {
                  return a.event_count > b.event_count;
              });
    QStringList names;
    names.reserve(countries.size());
    for (const auto& c : countries)
        if (!c.country.isEmpty())
            names.append(c.country);
    country_model_->setStringList(names);
}

void GeopoliticsScreen::on_cities_loaded(QStringList cities) {
    if (!city_model_)
        return;
    // The API lists a city once per country, so names repeat (e.g. "San Jose").
    cities.removeDuplicates();
    city_model_->setStringList(cities);
}

void GeopoliticsScreen::on_categories_loaded(QVector<services::geo::UniqueCategory> cats) {
    // Sort by event_count desc — most active categories first in both filter
    // dropdown and legend.
    std::sort(cats.begin(), cats.end(),
              [](const services::geo::UniqueCategory& a, const services::geo::UniqueCategory& b) {
                  return a.event_count > b.event_count;
              });

    if (category_combo_) {
        const QString prev = category_combo_->currentData().toString();
        category_combo_->clear();
        category_combo_->addItem(tr("All Categories"), "");
        for (const auto& c : cats)
            category_combo_->addItem(
                QString("%1 (%2)").arg(services::geo::pretty_category(c.category)).arg(c.event_count), c.category);
        if (!prev.isEmpty()) {
            const int idx = category_combo_->findData(prev);
            if (idx >= 0)
                category_combo_->setCurrentIndex(idx);
        }
    }
    rebuild_legend(cats);
}

void GeopoliticsScreen::rebuild_legend(const QVector<services::geo::UniqueCategory>& cats) {
    if (!legend_layout_)
        return;
    while (legend_layout_->count() > 0) {
        auto* item = legend_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    for (const auto& c : cats) {
        const QColor cat_col = services::geo::category_color(c.category);
        const QString col_hex = cat_col.name();
        const QString col_rgb = QString("%1,%2,%3").arg(cat_col.red()).arg(cat_col.green()).arg(cat_col.blue());

        auto* row = new QWidget(legend_container_);
        row->setObjectName("legend_row");
        row->setStyleSheet(QString("QWidget#legend_row { background:transparent; }"
                                   "QWidget#legend_row:hover { background:rgba(%1,0.06); }")
                               .arg(col_rgb));
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(4, 3, 4, 3);
        rl->setSpacing(8);

        // Square swatch — colored fill with a subtle darker outline so the
        // chip reads cleanly against the surface background.
        auto* swatch = new QWidget(row);
        swatch->setFixedSize(11, 11);
        swatch->setStyleSheet(QString("background:%1; border:1px solid rgba(0,0,0,0.35);").arg(col_hex));
        rl->addWidget(swatch, 0, Qt::AlignVCenter);

        auto* lbl = new QLabel(services::geo::pretty_category(c.category), row);
        lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;"
                                   "background:transparent;")
                               .arg(ui::colors::TEXT_SECONDARY())
                               .arg(ui::fonts::SMALL)
                               .arg(ui::fonts::DATA_FAMILY));
        rl->addWidget(lbl, 1);

        if (c.event_count > 0) {
            auto* cnt = new QLabel(QString::number(c.event_count), row);
            cnt->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; font-weight:700;"
                                       "background:rgba(%4,0.10); border:1px solid rgba(%4,0.30);"
                                       "padding:1px 5px;")
                                   .arg(col_hex)
                                   .arg(ui::fonts::TINY)
                                   .arg(ui::fonts::DATA_FAMILY())
                                   .arg(col_rgb));
            cnt->setAlignment(Qt::AlignCenter);
            rl->addWidget(cnt, 0, Qt::AlignRight | Qt::AlignVCenter);
        }
        legend_layout_->addWidget(row);
    }
    legend_layout_->addStretch();
}

void GeopoliticsScreen::on_error(const QString& context, const QString& message) {
    // error_occurred is shared with the HDX / Trade panels, which report their own
    // failures inline - an HDX hiccup must not flip the Monitor's status bar to
    // ERROR (it stayed that way until the next successful events fetch). Only the
    // Monitor's own feeds belong here.
    static const QStringList kMonitorContexts = {QStringLiteral("events"), QStringLiteral("countries"),
                                                 QStringLiteral("categories"), QStringLiteral("cities")};
    if (!kMonitorContexts.contains(context))
        return;
    LOG_ERROR("Geopolitics", QString("[%1] %2").arg(context, message));
    // Reference-list failures (autocomplete / legend) are cosmetic: keep the status
    // bar for the events feed the user is actually looking at.
    if (context != QStringLiteral("events"))
        return;
    // "ERROR" alone told the user nothing and left the real cause in a log
    // file they can't see. Surface a short reason inline, the full text on
    // hover, and point at the retry that actually exists (APPLY FILTERS).
    const QString brief = message.simplified().left(72);
    status_label_->setText(brief.isEmpty() ? tr("ERROR") : tr("ERROR: %1").arg(brief));
    status_label_->setToolTip(tr("%1\n\n%2\n\nClick APPLY FILTERS to retry.").arg(context, message));
    set_status_color(ui::colors::NEGATIVE());
    // A failed auto-refresh must not leave the flag set for the next manual apply.
    auto_refresh_pending_ = false;
    events_inflight_ = false;
    update_pager();
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap GeopoliticsScreen::save_state() const {
    QVariantMap state{{"tab_index", active_tab_}};
    if (country_edit_)
        state["country"] = country_edit_->text();
    if (city_edit_)
        state["city"] = city_edit_->text();
    return state;
}

void GeopoliticsScreen::restore_state(const QVariantMap& state) {
    const int idx = state.value("tab_index", 0).toInt();
    if (idx >= 0 && idx < tab_buttons_.size())
        on_tab_changed(idx);
    if (country_edit_ && state.contains("country"))
        country_edit_->setText(state.value("country").toString());
    if (city_edit_ && state.contains("city"))
        city_edit_->setText(state.value("city").toString());
}

} // namespace fincept::screens
