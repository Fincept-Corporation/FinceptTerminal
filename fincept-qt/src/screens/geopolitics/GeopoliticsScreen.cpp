// src/screens/geopolitics/GeopoliticsScreen.cpp
#include "screens/geopolitics/GeopoliticsScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/geopolitics/ConflictMonitorPanel.h"
#include "screens/geopolitics/HDXDataPanel.h"
#include "screens/geopolitics/RelationshipPanel.h"
#include "screens/geopolitics/TradeAnalysisPanel.h"
#include "auth/AuthManager.h"
#include "auth/AuthTypes.h"
#include "datahub/DataHub.h"
#include "network/http/HttpClient.h"
#include "services/geopolitics/GeopoliticsService.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/EmptyState.h"
#include "services/billing/PlanAccess.h"
#include "ui/widgets/EnterprisePromo.h"
#include "ui/widgets/PlanHint.h"

#include <QCompleter>
#include <QDateTime>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMouseEvent>
#include <QScrollArea>
#include <QStyle>

#include <algorithm>
#include <functional>

namespace fincept::screens {

using namespace fincept::services::geo;

namespace {
// Page size the screen requests - passed explicitly so the request key it records
// matches the one the service stamps on the reply.
constexpr int kGeoScreenEventsLimit = 100;
/// How long the Monitor's AutoLoadPane keeps a loaded page (shown, free, with
/// its "as of" on a later start).
constexpr int kGeoScreenKeepDays = 14;

QJsonObject geo_scr_event_to_json(const NewsEvent& e) {
    QJsonObject o;
    o[QStringLiteral("url")] = e.url;
    o[QStringLiteral("source")] = e.source;
    o[QStringLiteral("event_category")] = e.event_category;
    o[QStringLiteral("title")] = e.title;
    o[QStringLiteral("city")] = e.city;
    o[QStringLiteral("country")] = e.country;
    if (e.has_coords) {
        o[QStringLiteral("latitude")] = e.latitude;
        o[QStringLiteral("longitude")] = e.longitude;
    }
    o[QStringLiteral("extracted_date")] = e.extracted_date;
    o[QStringLiteral("created_at")] = e.created_at;
    return o;
}

NewsEvent geo_scr_event_from_json(const QJsonObject& o) {
    NewsEvent e;
    e.url = o.value(QStringLiteral("url")).toString();
    e.source = o.value(QStringLiteral("source")).toString();
    e.event_category = o.value(QStringLiteral("event_category")).toString();
    e.title = o.value(QStringLiteral("title")).toString();
    e.city = o.value(QStringLiteral("city")).toString();
    e.country = o.value(QStringLiteral("country")).toString();
    const QJsonValue lat = o.value(QStringLiteral("latitude"));
    const QJsonValue lng = o.value(QStringLiteral("longitude"));
    e.has_coords = lat.isDouble() && lng.isDouble();
    e.latitude = e.has_coords ? lat.toDouble() : 0.0;
    e.longitude = e.has_coords ? lng.toDouble() : 0.0;
    e.extracted_date = o.value(QStringLiteral("extracted_date")).toString();
    e.created_at = o.value(QStringLiteral("created_at")).toString();
    return e;
}

/// An events page as the AutoLoadPane payload (remembered and persisted).
QJsonObject geo_scr_page_to_json(const EventsPage& page) {
    QJsonArray arr;
    for (const auto& e : page.events)
        arr.append(geo_scr_event_to_json(e));
    QJsonObject o;
    o[QStringLiteral("events")] = arr;
    o[QStringLiteral("total_events")] = page.total_events;
    o[QStringLiteral("current_page")] = page.current_page;
    o[QStringLiteral("total_pages")] = page.total_pages;
    o[QStringLiteral("credits_used")] = page.credits_used;
    o[QStringLiteral("remaining_credits")] = page.remaining_credits;
    o[QStringLiteral("request_key")] = page.request_key;
    return o;
}

EventsPage geo_scr_page_from_json(const QJsonObject& o) {
    EventsPage page;
    const QJsonArray arr = o.value(QStringLiteral("events")).toArray();
    page.events.reserve(arr.size());
    for (const auto& v : arr)
        page.events.append(geo_scr_event_from_json(v.toObject()));
    page.total_events = o.value(QStringLiteral("total_events")).toInt(static_cast<int>(page.events.size()));
    page.current_page = std::max(1, o.value(QStringLiteral("current_page")).toInt(1));
    page.total_pages = o.value(QStringLiteral("total_pages")).toInt(0);
    page.credits_used = o.value(QStringLiteral("credits_used")).toDouble(0.0);
    page.remaining_credits = o.value(QStringLiteral("remaining_credits")).toDouble(-1.0);
    page.request_key = o.value(QStringLiteral("request_key")).toString();
    return page;
}
/// Below this balance the credit counter turns red and PLANS appears.
constexpr double kGeoLowCredits = 20.0;

/// Height of a status-bar control: `design_px` at the default font size, more
/// when the 12px status text needs it.
int status_control_height(int design_px) {
    return std::max(design_px, QFontMetrics(ui::fonts::font(-2)).height() + 4);
}

/// A legend row that toggles its category on the map when clicked.
class GeoLegendToggleRow : public QWidget {
  public:
    explicit GeoLegendToggleRow(QWidget* parent) : QWidget(parent) { setCursor(Qt::PointingHandCursor); }
    void set_on_click(std::function<void()> fn) { on_click_ = std::move(fn); }

  protected:
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton)
            e->accept(); // become the press target so the release comes back here
        else
            QWidget::mousePressEvent(e);
    }
    void mouseReleaseEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton && rect().contains(e->position().toPoint()) && on_click_)
            on_click_();
        QWidget::mouseReleaseEvent(e);
    }

  private:
    std::function<void()> on_click_;
};
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

    // The root and tab sheets are built from theme tokens: the theme manager
    // re-styles them on a theme / font change. Only the status-bar control
    // heights (they follow the font size) are re-fitted, once the new
    // appearance has settled — nothing in theme_changed.
    connect(&fincept::ui::ThemeManager::instance(), &fincept::ui::ThemeManager::appearance_settled, this,
            [this]() { fit_status_controls(); });

    LOG_DEBUG("Geopolitics", "Screen constructed");
}

void GeopoliticsScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    // Auto-refresh is opt-in: every events fetch costs API credits.
    if (auto_refresh_btn_ && auto_refresh_btn_->isChecked())
        refresh_timer_->start();
    clock_timer_->start();
    if (first_show_) {
        first_show_ = false;
        // Opening the screen never spends credits: every events page and every
        // reference list is a paid call (3 credits each — this used to cost 12
        // credits on a cold start). Reference lists come from their 6-hour
        // CacheManager copies only; the events from the hub / the last load,
        // else the Monitor shows what LOAD EVENTS fetches and what it costs.
        auto& svc = GeopoliticsService::instance();
        svc.fetch_unique_countries(/*cache_only=*/true);
        svc.fetch_unique_categories(/*cache_only=*/true);
        svc.fetch_unique_cities(/*cache_only=*/true);
        // The Monitor's pane (Manual) shows the page last loaded for the restored
        // filters with its "as of", else the preview. The dashboard's fresh copy
        // of the unfiltered page (hub, 30 min) is newer than any remembered one:
        // taken once, free.
        applied_country_ = country_edit_->text().trimmed();
        applied_city_ = city_edit_->text().trimmed();
        applied_category_ = category_combo_->currentData().toString();
        requested_page_ = 1;
        configure_events_preview();
        auto* pane = monitor_panel_->events_pane();
        pane->set_view(events_view_key(1));
        if (applied_country_.isEmpty() && applied_city_.isEmpty() && applied_category_.isEmpty()) {
            const QVariant hub_copy = datahub::DataHub::instance().peek(QStringLiteral("geopolitics:events"));
            if (hub_copy.canConvert<EventsPage>() && hub_copy.value<EventsPage>().request_key == events_view_key(1)) {
                use_hub_copy_once_ = true;
                pane->run();
            }
        }
    }
    LOG_DEBUG("Geopolitics", "Screen shown");
}

void GeopoliticsScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    refresh_timer_->stop();
    clock_timer_->stop();
}

void GeopoliticsScreen::refresh_theme() {
    // "* { }" = the old bare "background:" declaration, now that the sheet also has rules.
    setStyleSheet(QString("* { background:%1; }"
                          "QLabel#geoCredits { color:%2; font-size:%3px; font-weight:700; font-family:%4; }"
                          "QLabel#geoCredits[state=\"ok\"] { color:%5; }"
                          "QLabel#geoCredits[state=\"warn\"] { color:%6; }"
                          "QLabel#geoCredits[state=\"low\"] { color:%7; }"
                          "QPushButton#geoPlansBtn { color:%8; background:%6; border:none; padding:0 8px;"
                          " font-size:%3px; font-weight:700; font-family:%4; }"
                          "QPushButton#geoAutoBtn { color:%9; background:transparent; border:1px solid %10;"
                          " padding:0 8px; font-size:%3px; font-weight:700; font-family:%4; }"
                          "QPushButton#geoAutoBtn:checked { color:%6; border-color:%6; }")
                      .arg(ui::colors::BG_BASE(), ui::colors::TEXT_SECONDARY())
                      .arg(ui::fonts::TINY.get())
                      .arg(ui::fonts::DATA_FAMILY(), ui::colors::SUCCESS(), ui::colors::WARNING(),
                           ui::colors::DANGER(), ui::colors::TEXT_ON_ACCENT(), ui::colors::TEXT_SECONDARY(),
                           ui::colors::BORDER_DIM()));
    // Tab highlight for the active tab.
    on_tab_changed(active_tab_);
    fit_status_controls();
}

void GeopoliticsScreen::fit_status_controls() {
    // Status-bar controls follow the user's font size.
    for (QWidget* w : {static_cast<QWidget*>(prev_page_btn_), static_cast<QWidget*>(next_page_btn_),
                       static_cast<QWidget*>(refresh_btn_), static_cast<QWidget*>(auto_refresh_btn_),
                       static_cast<QWidget*>(plans_btn_)}) {
        if (w)
            w->setFixedHeight(status_control_height(18));
    }
    if (event_count_label_)
        event_count_label_->setFixedHeight(std::max(22, QFontMetrics(ui::fonts::font(-2, QFont::Bold)).height() + 8));
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
    // The balance moves with every billed call app-wide (and with purchases).
    connect(&auth::AuthManager::instance(), &auth::AuthManager::credit_balance_changed, this,
            [this](double balance) { update_credits(balance, 0); });
}

// ── Build UI ─────────────────────────────────────────────────────────────────
void GeopoliticsScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_top_bar());

    // The body and everything in it are created inside their final parents:
    // the old code built them under the screen and then moved them into the
    // body with setLayout(), which re-computed the stylesheet of every widget
    // of all four panels.
    auto* body_w = new QWidget(this);
    auto* body = new QHBoxLayout(body_w);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    root->addWidget(body_w, 1);

    filter_panel_ = build_filter_panel(body_w);
    body->addWidget(filter_panel_);

    content_stack_ = new QStackedWidget(body_w);
    monitor_panel_ = new ConflictMonitorPanel(content_stack_);
    content_stack_->addWidget(monitor_panel_);
    // The map's camera / basemap is part of the saved screen state.
    connect(monitor_panel_, &ConflictMonitorPanel::map_view_changed, this,
            [this]() { ScreenStateManager::instance().notify_changed(this); });
    // Events cost 3 credits: the shared AutoLoadPane (Manual) under the map keeps
    // every loaded page (per filters + page, 14 days) and shows it with its "as
    // of"; with none, its preview says what LOAD fetches and costs.
    {
        auto* pane = monitor_panel_->events_pane();
        pane->set_cache_key(QStringLiteral("geopolitics/events"), kGeoScreenKeepDays);
        pane->set_loading_text(tr("LOADING THE LATEST EVENTS…"));
        pane->set_start([this](quint64 id) { start_events_request(id); });
        pane->set_cancel([this](quint64 id) {
            if (events_request_id_ == id) {
                events_request_id_ = 0;
                events_inflight_ = false;
                update_pager();
            }
        });
        pane->set_render([this](const QJsonValue& payload, const QDateTime& as_of, bool from_cache) {
            render_events_payload(payload, as_of, from_cache);
        });
        connect(pane, &ui::AutoLoadPane::state_changed, this, [this](ui::AutoLoadPane::State st) {
            if (st != ui::AutoLoadPane::State::Preview)
                return;
            monitor_panel_->set_map_message(tr("Events not loaded yet — LOAD EVENTS (3 credits) plots them here"));
            status_label_->setText(tr("NOT LOADED"));
            status_label_->setToolTip(tr("Events are loaded on request: LOAD EVENTS or ⟳ REFRESH (3 credits)."));
            set_status_color(ui::colors::TEXT_SECONDARY());
        });
        connect(pane->preview(), &ui::EmptyState::secondary_clicked, this, [this]() { on_tab_changed(1); });
        connect(monitor_panel_->no_match_state(), &ui::EmptyState::primary_clicked, this,
                &GeopoliticsScreen::on_clear_filters);
    }
    for (int i = 1; i <= 3; ++i) // HDX, Relations, Trade: built on first show (ensure_tab_panel)
        content_stack_->addWidget(new QWidget(content_stack_));
    body->addWidget(content_stack_, 1);

    root->addWidget(build_status_bar());

    refresh_theme(); // root sheet + tab highlight
}

void GeopoliticsScreen::ensure_tab_panel(int index) {
    if (!content_stack_ || index < 1 || index > 3)
        return;
    QWidget* panel = nullptr;
    if (index == 1 && !hdx_panel_) {
        hdx_panel_ = new HDXDataPanel(content_stack_);
        panel = hdx_panel_;
    } else if (index == 2 && !relationship_panel_) {
        relationship_panel_ = new RelationshipPanel(content_stack_);
        panel = relationship_panel_;
        wire_relationship_panel();
    } else if (index == 3 && !trade_panel_) {
        trade_panel_ = new TradeAnalysisPanel(content_stack_);
        panel = trade_panel_;
    }
    if (!panel)
        return;
    // Same index, so the tab buttons keep mapping to the right page.
    QWidget* placeholder = content_stack_->widget(index);
    content_stack_->insertWidget(index, panel);
    if (placeholder) {
        content_stack_->removeWidget(placeholder);
        placeholder->deleteLater();
    }
}

void GeopoliticsScreen::wire_relationship_panel() {
    if (have_countries_)
        relationship_panel_->set_countries(last_countries_);
    if (have_categories_)
        relationship_panel_->set_categories(last_categories_);
    // The Monitor's page counts (free) stand in until the paid lists are loaded.
    if (page_event_count_ > 0)
        relationship_panel_->set_page_statistics(page_countries_, page_categories_, page_event_count_);
    connect(relationship_panel_, &RelationshipPanel::stats_requested, this, &GeopoliticsScreen::load_reference_stats);

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
        ensure_tab_panel(1);
        hdx_panel_->explore_country(country);
        on_tab_changed(1);
    });
    connect(relationship_panel_, &RelationshipPanel::hdx_topic_requested, this, [this](const QString& topic) {
        ensure_tab_panel(1);
        hdx_panel_->explore_topic(topic);
        on_tab_changed(1);
    });
    connect(relationship_panel_, &RelationshipPanel::hdx_organization_requested, this, [this](const QString& org) {
        ensure_tab_panel(1);
        hdx_panel_->explore_organization(org);
        on_tab_changed(1);
    });
}

// ── Top Bar ──────────────────────────────────────────────────────────────────
QWidget* GeopoliticsScreen::build_top_bar() {
    auto* bar = new QWidget(this);
    bar->setMinimumHeight(48);
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
        {tr("MONITOR"), ui::colors::DANGER},
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
                                   "QPushButton:hover { color:%4; background:%5; }")
                               .arg(ui::colors::TEXT_SECONDARY())
                               .arg(ui::fonts::TINY.get())
                               .arg(ui::fonts::DATA_FAMILY)
                               .arg(tabs[i].color)
                               .arg(ui::colors::alpha(QColor(tabs[i].color), 0.06)));
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_tab_changed(i); });
        hl->addWidget(btn);
        tab_buttons_.append(btn);
    }

    hl->addStretch(1);

    clock_label_ = new QLabel(tr("UTC --:--"), bar);
    clock_label_->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; min-width:68px;")
                                    .arg(ui::colors::TEXT_SECONDARY())
                                    .arg(ui::fonts::TINY.get())
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
    event_count_label_->setStyleSheet(
        QString("color:%1; font-size:%2px; font-family:%3; padding:2px 6px;"
                "background:%4; border:1px solid %5; font-weight:700;")
            .arg(ui::colors::DANGER())
            .arg(ui::fonts::TINY.get())
            .arg(ui::fonts::DATA_FAMILY())
            .arg(ui::colors::alpha(ui::colors::DANGER, 0.08))
            .arg(ui::colors::alpha(ui::colors::DANGER, 0.25)));
    hl->addWidget(event_count_label_);

    return bar;
}

// ── Filter Panel ─────────────────────────────────────────────────────────────
QWidget* GeopoliticsScreen::build_filter_panel(QWidget* parent) {
    auto* panel = new QWidget(parent);
    // Wide enough for the longest legend row ("Political Violence" + a four-digit
    // count + the legend's scroll bar): at 220 px the count chips were cut off.
    panel->setFixedWidth(std::max(252, QFontMetrics(ui::fonts::font(-1)).horizontalAdvance(QString(31, QLatin1Char('M')))));
    panel->setStyleSheet(
        QString("background:%1; border-right:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(8);

    filters_title_ = new QLabel(tr("FILTERS"), panel);
    filters_title_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;"
                                          "letter-spacing:1px; padding-bottom:4px; border-bottom:1px solid %4;")
                                      .arg(ui::colors::DANGER())
                                      .arg(ui::fonts::TINY.get())
                                      .arg(ui::fonts::DATA_FAMILY())
                                      .arg(ui::colors::BORDER_DIM()));
    vl->addWidget(filters_title_);

    auto input_style = QString("QLineEdit, QComboBox { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:5px 8px; }"
                               "QLineEdit:focus, QComboBox:focus { border-color:%6; }")
                           .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                           .arg(ui::fonts::DATA_FAMILY())
                           .arg(ui::fonts::SMALL.get())
                           .arg(ui::colors::DANGER());

    auto label_style = QString("color:%1; font-size:%2px; font-weight:700; font-family:%3; letter-spacing:1px;")
                           .arg(ui::colors::TEXT_SECONDARY())
                           .arg(ui::fonts::TINY.get())
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
    apply_btn_->setProperty("fincept_agent_risk", "spend"); // agents: reloads events (3 credits)
    apply_btn_->setCursor(Qt::PointingHandCursor);
    apply_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%2; font-family:%3; font-size:%4px;"
                                      "font-weight:700; border:none; padding:6px 12px; }"
                                      "QPushButton:hover { background:%5; }")
                                  .arg(ui::colors::DANGER())
                                  .arg(ui::colors::TEXT_ON_ACCENT())
                                  .arg(ui::fonts::DATA_FAMILY())
                                  .arg(ui::fonts::SMALL.get())
                                  .arg(ui::colors::alpha(ui::colors::DANGER, 0.85)));
    connect(apply_btn_, &QPushButton::clicked, this, &GeopoliticsScreen::on_apply_filters);
    vl->addWidget(apply_btn_);

    clear_btn_ = new QPushButton(tr("CLEAR"), panel);
    clear_btn_->setCursor(Qt::PointingHandCursor);
    clear_btn_->setStyleSheet(QString("QPushButton { background:transparent; color:%1; font-family:%2; font-size:%3px;"
                                      "border:1px solid %4; padding:5px 12px; }"
                                      "QPushButton:hover { background:%5; }")
                                  .arg(ui::colors::TEXT_SECONDARY())
                                  .arg(ui::fonts::DATA_FAMILY)
                                  .arg(ui::fonts::SMALL.get())
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
            .arg(ui::fonts::TINY.get())
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
    bar->setMinimumHeight(26);
    bar->setStyleSheet(
        QString("background:%1; border-top:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(16);

    auto s = QString("color:%1; font-size:%2px; font-family:%3;")
                 .arg(ui::colors::TEXT_SECONDARY())
                 .arg(ui::fonts::TINY.get())
                 .arg(ui::fonts::DATA_FAMILY);
    auto sv = QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;")
                  .arg(ui::colors::TEXT_PRIMARY())
                  .arg(ui::fonts::TINY.get())
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
                                          .arg(ui::colors::SUCCESS())
                                          .arg(ui::fonts::TINY.get())
                                          .arg(ui::fonts::DATA_FAMILY));
    hl->addWidget(status_engine_lbl_);
    hl->addWidget(status_engine_val_);

    hl->addStretch();

    // PREV / PAGE x / y / NEXT. One scoped stylesheet covers all three widgets.
    auto* pager = new QWidget(bar);
    pager_ = pager;
    pager->setStyleSheet(QString("QPushButton { color:%1; background:transparent; border:1px solid %2; padding:0 8px;"
                                 "font-size:%3px; font-family:%4; font-weight:700; }"
                                 "QPushButton:hover:enabled { color:%5; border-color:%5; }"
                                 "QPushButton:disabled { color:%6; border-color:%6; }"
                                 "QLabel { color:%1; font-size:%3px; font-family:%4; font-weight:700; }")
                             .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM())
                             .arg(ui::fonts::TINY.get())
                             .arg(ui::fonts::DATA_FAMILY())
                             .arg(ui::colors::DANGER(), ui::colors::TEXT_TERTIARY()));
    auto* pl = new QHBoxLayout(pager);
    pl->setContentsMargins(0, 0, 0, 0);
    pl->setSpacing(6);
    // Re-buy the page on screen (same filters / page): the explicit refresh for a
    // page shown from the cache.
    refresh_btn_ = new QPushButton(tr("⟳ REFRESH · 3 CR"), pager);
    refresh_btn_->setProperty("fincept_agent_risk", "spend"); // agents: reloads events (3 credits)
    refresh_btn_->setToolTip(tr("Fetch the latest events for the filters and page on screen (3 credits)"));
    connect(refresh_btn_, &QPushButton::clicked, this, [this]() { request_events(current_page_, /*reuse_filters=*/true); });
    pl->addWidget(refresh_btn_);
    prev_page_btn_ = new QPushButton(tr("◀ PREV"), pager);
    next_page_btn_ = new QPushButton(tr("NEXT ▶"), pager);
    next_page_btn_->setProperty("fincept_agent_risk", "spend"); // agents: loads a page of events (credits)
    page_lbl_ = new QLabel(tr("PAGE %1").arg(1), pager);
    for (auto* b : {refresh_btn_, prev_page_btn_, next_page_btn_}) {
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

    // Opt-in auto refresh: each events fetch costs API credits (the 5-min timer
    // used to run whenever the screen was visible).
    auto_refresh_btn_ = new QPushButton(tr("AUTO ⟳ OFF"), bar);
    auto_refresh_btn_->setProperty("fincept_agent_risk", "spend"); // agents: turns on billed auto-refresh
    auto_refresh_btn_->setObjectName("geoAutoBtn");
    auto_refresh_btn_->setCheckable(true);
    auto_refresh_btn_->setFixedHeight(18);
    auto_refresh_btn_->setCursor(Qt::PointingHandCursor);
    auto_refresh_btn_->setToolTip(tr("Re-fetch the current events page every 5 minutes while this screen is "
                                     "visible. Each fetch uses API credits; off by default."));
    connect(auto_refresh_btn_, &QPushButton::toggled, this, [this](bool on) {
        set_auto_refresh(on);
        ScreenStateManager::instance().notify_changed(this);
    });
    hl->addWidget(auto_refresh_btn_);

    // Styled by the root sheet (#geoCredits[state=...]).
    credits_label_ = new QLabel(tr("CREDITS: —"), bar);
    credits_label_->setObjectName("geoCredits");
    credits_label_->setToolTip(tr("Fincept API credit balance after the last events request"));
    hl->addWidget(credits_label_);
    // What the plan changes for events (included with Pro: no credits used) —
    // hidden until the server has sent its plan table.
    auto* plan_hint = new ui::PlanHintLabel(QStringLiteral("news_events"), bar);
    plan_hint->set_compact(true);
    hl->addWidget(plan_hint);

    plans_btn_ = new QPushButton(tr("PLANS"), bar);
    plans_btn_->setObjectName("geoPlansBtn");
    plans_btn_->setFixedHeight(18);
    plans_btn_->setCursor(Qt::PointingHandCursor);
    plans_btn_->setToolTip(tr("Credits are running low — see Plans & Pricing"));
    plans_btn_->hide();
    connect(plans_btn_, &QPushButton::clicked, this, [this]() { ui::billing::open_plans(this); });
    hl->addWidget(plans_btn_);

    status_label_ = new QLabel(tr("READY"), bar);
    set_status_color(ui::colors::SUCCESS());
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
    requested_page_ = std::max(1, page);
    // One remembered result per filters + page (the shared pane keeps each with
    // its "as of"); run() then buys this page (3 credits).
    configure_events_preview();
    auto* pane = monitor_panel_->events_pane();
    pane->set_view(events_view_key(requested_page_));
    pane->run();
}

QString GeopoliticsScreen::events_view_key(int page) const {
    return events_request_key(applied_country_, applied_city_, applied_category_, kGeoScreenEventsLimit, page, {}, {},
                              {});
}

void GeopoliticsScreen::start_events_request(quint64 id) {
    events_request_id_ = id;
    last_request_key_ = events_view_key(requested_page_);
    // Opening the screen: the dashboard's copy of the unfiltered first page
    // (hub, 30 min — the feed's own cadence) is taken once, free.
    if (std::exchange(use_hub_copy_once_, false)) {
        auto& hub = datahub::DataHub::instance();
        const QString topic = QStringLiteral("geopolitics:events");
        const QVariant cached = hub.peek(topic);
        if (cached.canConvert<EventsPage>()) {
            EventsPage p = cached.value<EventsPage>();
            if (p.request_key == last_request_key_) {
                const qint64 age_ms = std::max<qint64>(0, hub.age_ms(topic));
                p.remaining_credits = -1; // that page's figure is old; the session's is current
                events_request_id_ = 0;
                // Labelled with the time the dashboard fetched it, not now.
                monitor_panel_->events_pane()->deliver(id, geo_scr_page_to_json(p),
                                                       QDateTime::currentDateTime().addMSecs(-age_ms));
                const int minutes = static_cast<int>(age_ms / 60000);
                status_label_->setText(minutes < 1 ? tr("READY · JUST NOW") : tr("READY · %1 MIN OLD").arg(minutes));
                status_label_->setToolTip(
                    tr("Events fetched at %1 and reused without spending credits. ⟳ REFRESH fetches the latest "
                       "(3 credits).")
                        .arg(QDateTime::currentDateTime().addMSecs(-age_ms).toString(QStringLiteral("HH:mm"))));
                LOG_DEBUG("Geopolitics", QString("Reused events page fetched %1 s ago (no request)").arg(age_ms / 1000));
                return;
            }
        }
    }
    status_label_->setText(tr("LOADING..."));
    status_label_->setToolTip(QString());
    set_status_color(ui::colors::WARNING());
    monitor_panel_->set_map_message(QString());
    events_inflight_ = true;
    update_pager();
    GeopoliticsService::instance().fetch_events(applied_country_, applied_city_, applied_category_,
                                                kGeoScreenEventsLimit, requested_page_);
}

void GeopoliticsScreen::configure_events_preview() {
    QStringList filters;
    if (!applied_country_.isEmpty())
        filters << applied_country_;
    if (!applied_city_.isEmpty())
        filters << applied_city_;
    if (!applied_category_.isEmpty())
        filters << pretty_category(applied_category_);
    const auto& session = auth::AuthManager::instance().session();
    auto* es = monitor_panel_->events_pane()->preview();
    es->set_glyph(QStringLiteral("◉"), ui::EmptyState::Tone::Warning);
    es->set_title(tr("Latest geopolitical events"));
    es->set_message(tr("Conflict, protest and crisis events from the Fincept news-event feed: plotted on the map, "
                       "ranked by category and hotspot, newest first. Events are loaded on request — opening this "
                       "screen never spends credits."));
    es->set_details({{tr("Filters"), filters.isEmpty() ? tr("All countries · all categories")
                                                       : filters.join(QStringLiteral(" · "))},
                     {tr("Page"), tr("%1 newest events").arg(kGeoScreenEventsLimit)},
                     {tr("Kept"), tr("Shown again on your next start, free")}},
                    tr("WHAT LOAD FETCHES"));
    // Events are included with Pro: no price on the button for such an account.
    const bool included = billing::PlanAccess::instance().included(QStringLiteral("news_events"));
    if (included)
        es->set_note(tr("Included with your Pro plan — no credits used"), ui::EmptyState::Tone::Info);
    else
        es->set_note(session.authenticated ? tr("3 credits per load · balance %1")
                                                 .arg(auth::format_credits(session.user_info.credit_balance))
                                           : tr("3 credits per load"),
                     ui::EmptyState::Tone::Warning);
    es->set_primary_action(included ? tr("LOAD EVENTS") : tr("LOAD EVENTS (3 CREDITS)"));
    es->set_secondary_action(tr("BROWSE HDX DATA (FREE)"));
}

void GeopoliticsScreen::configure_no_match_state() {
    auto* es = monitor_panel_->no_match_state();
    es->set_glyph(QStringLiteral("∅"), ui::EmptyState::Tone::Info);
    es->set_title(tr("No events match these filters"));
    QStringList filters;
    for (const QString& f : {applied_country_, applied_city_, pretty_category(applied_category_)})
        if (!f.trimmed().isEmpty())
            filters << f;
    es->set_message(filters.isEmpty() ? tr("The feed returned no events for this page.")
                                      : tr("Nothing was reported for %1 on this page. Widen the filters or clear "
                                           "them for the global feed.")
                                            .arg(filters.join(QStringLiteral(" · "))));
    const bool included = billing::PlanAccess::instance().included(QStringLiteral("news_events"));
    if (included)
        es->set_note(tr("Included with your Pro plan — no credits used"), ui::EmptyState::Tone::Info);
    else
        es->set_note(tr("3 credits per load"), ui::EmptyState::Tone::Warning);
    es->set_primary_action(filters.isEmpty() ? QString()
                                             : (included ? tr("CLEAR FILTERS") : tr("CLEAR FILTERS (3 CREDITS)")));
}

void GeopoliticsScreen::render_events_payload(const QJsonValue& payload, const QDateTime& as_of, bool from_cache) {
    const EventsPage page = geo_scr_page_from_json(payload.toObject());
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
    const QString color = mapped > 0 ? ui::colors::SUCCESS() : ui::colors::DANGER();
    const QColor clr(color);
    event_count_label_->setStyleSheet(
        QString("color:%1; font-size:%2px; font-family:%3; padding:2px 6px;"
                "background:%4; border:1px solid %5; font-weight:700;")
            .arg(color)
            .arg(ui::fonts::TINY.get())
            .arg(ui::fonts::DATA_FAMILY)
            .arg(ui::colors::alpha(clr, 0.08))
            .arg(ui::colors::alpha(clr, 0.25)));

    const auto& session = auth::AuthManager::instance().session();
    if (!from_cache && page.remaining_credits >= 0)
        update_credits(page.remaining_credits, page.credits_used);
    else if (session.authenticated)
        update_credits(session.user_info.credit_balance, 0); // a remembered page carries an old balance

    if (from_cache) {
        // Remembered from an earlier session: no credits spent; say how old it is.
        const QDateTime local = as_of.toLocalTime();
        const bool today = local.date() == QDate::currentDate();
        status_label_->setText(tr("AS OF %1").arg(
            local.toString(today ? QStringLiteral("HH:mm") : QStringLiteral("dd MMM HH:mm")).toUpper()));
        status_label_->setToolTip(tr("The events of your last load (%1), shown without spending credits. "
                                     "⟳ REFRESH fetches the latest (3 credits).")
                                      .arg(QLocale().toString(local, QLocale::ShortFormat)));
        set_status_color(ui::colors::INFO());
    } else {
        status_label_->setText(tr("READY"));
        status_label_->setToolTip(QString()); // clear any stale error detail
        set_status_color(ui::colors::SUCCESS());
    }
    // The 5-minute refresh must not throw away the user's pan/zoom.
    monitor_panel_->set_map_message(QString()); // events arrived: drop an earlier failure note
    if (page.events.isEmpty())
        configure_no_match_state();
    monitor_panel_->set_events(page.events, /*fit_map=*/!auto_refresh_pending_);
    auto_refresh_pending_ = false;
    apply_page_reference(page.events);
}

void GeopoliticsScreen::apply_page_reference(const QVector<NewsEvent>& events) {
    QHash<QString, int> by_country, by_category;
    QStringList cities;
    for (const auto& e : events) {
        if (!e.country.isEmpty() && e.country.compare(QLatin1String("null"), Qt::CaseInsensitive) != 0)
            ++by_country[e.country];
        if (!e.event_category.isEmpty())
            ++by_category[e.event_category];
        if (!e.city.isEmpty())
            cities << e.city;
    }
    page_countries_.clear();
    for (auto it = by_country.cbegin(); it != by_country.cend(); ++it)
        page_countries_.append({it.key(), it.value()});
    std::sort(page_countries_.begin(), page_countries_.end(),
              [](const UniqueCountry& a, const UniqueCountry& b) { return a.event_count > b.event_count; });
    page_categories_.clear();
    for (auto it = by_category.cbegin(); it != by_category.cend(); ++it)
        page_categories_.append({it.key(), it.value()});
    std::sort(page_categories_.begin(), page_categories_.end(),
              [](const UniqueCategory& a, const UniqueCategory& b) { return a.event_count > b.event_count; });
    page_event_count_ = static_cast<int>(events.size());

    // Paid reference lists win wherever they are loaded.
    if (!have_categories_) {
        fill_category_combo(page_categories_);
        rebuild_legend(page_categories_);
    }
    if (!have_countries_ && country_model_) {
        QStringList names;
        for (const auto& c : std::as_const(page_countries_))
            names << c.country;
        country_model_->setStringList(names);
    }
    if (!have_cities_ && city_model_) {
        cities.removeDuplicates();
        city_model_->setStringList(cities);
    }
    if (relationship_panel_)
        relationship_panel_->set_page_statistics(page_countries_, page_categories_, page_event_count_);
}

void GeopoliticsScreen::fill_category_combo(const QVector<UniqueCategory>& cats) {
    if (!category_combo_)
        return;
    QString prev = category_combo_->currentData().toString();
    if (prev.isEmpty())
        prev = applied_category_;
    const QSignalBlocker block(category_combo_);
    category_combo_->clear();
    category_combo_->addItem(tr("All Categories"), "");
    for (const auto& c : cats)
        category_combo_->addItem(
            QString("%1 (%2)").arg(services::geo::pretty_category(c.category)).arg(c.event_count), c.category);
    if (!prev.isEmpty()) {
        int idx = category_combo_->findData(prev);
        if (idx < 0) {
            category_combo_->addItem(services::geo::pretty_category(prev), prev);
            idx = category_combo_->count() - 1;
        }
        category_combo_->setCurrentIndex(idx);
    }
}

void GeopoliticsScreen::load_reference_stats() {
    if (stats_inflight_)
        return;
    stats_inflight_ = true;
    if (relationship_panel_)
        relationship_panel_->set_stats_loading(true);
    auto& svc = GeopoliticsService::instance();
    // Either list may already be cached (free); only the missing ones are bought.
    svc.fetch_unique_countries();
    svc.fetch_unique_categories();
}

void GeopoliticsScreen::update_pager() {
    if (!page_lbl_ || !prev_page_btn_ || !next_page_btn_)
        return;
    page_lbl_->setText(total_pages_ > 0 ? tr("PAGE %1 / %2").arg(current_page_).arg(total_pages_)
                                        : tr("PAGE %1").arg(current_page_));
    prev_page_btn_->setEnabled(!events_inflight_ && current_page_ > 1);
    next_page_btn_->setEnabled(!events_inflight_ && current_page_ < total_pages_);
    if (refresh_btn_)
        refresh_btn_->setEnabled(!events_inflight_);
}

void GeopoliticsScreen::set_status_color(const QString& color) {
    if (!status_label_)
        return;
    status_label_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;")
                                     .arg(color)
                                     .arg(ui::fonts::TINY.get())
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
    ensure_tab_panel(index);
    if (content_stack_)
        content_stack_->setCurrentIndex(index);
    ScreenStateManager::instance().notify_changed(this);
    // The filters, events pager, AUTO refresh and event count belong to the
    // Monitor tab: on the other tabs they had no effect on what is shown, and
    // APPLY FILTERS silently fetched (billed) events for the hidden Monitor tab.
    const bool monitor = index == 0;
    for (QWidget* w : {filter_panel_, pager_, static_cast<QWidget*>(auto_refresh_btn_),
                       static_cast<QWidget*>(event_count_label_)})
        if (w)
            w->setVisible(monitor);

    const QStringList colors = {ui::colors::DANGER, ui::colors::CYAN, ui::colors::INFO, ui::colors::WARNING};
    for (int i = 0; i < tab_buttons_.size(); ++i) {
        const bool active = (i == index);
        const QColor c(colors[i]);
        if (active) {
            tab_buttons_[i]->setStyleSheet(
                QString("QPushButton { color:%1; font-size:%2px; font-family:%3;"
                        "padding:4px 14px;"
                        "border-bottom:2px solid %1; border-top:none; border-left:none; border-right:none;"
                        "background:%4; font-weight:700; }"
                        "QPushButton:hover { background:%5; }")
                    .arg(colors[i])
                    .arg(ui::fonts::TINY.get())
                    .arg(ui::fonts::DATA_FAMILY)
                    .arg(ui::colors::alpha(c, 0.06))
                    .arg(ui::colors::alpha(c, 0.10)));
        } else {
            tab_buttons_[i]->setStyleSheet(QString("QPushButton { color:%1; font-size:%2px; font-family:%3;"
                                                   "padding:4px 14px; border:none;"
                                                   "background:transparent; font-weight:400; }"
                                                   "QPushButton:hover { color:%4; background:%5; }")
                                               .arg(ui::colors::TEXT_SECONDARY())
                                               .arg(ui::fonts::TINY.get())
                                               .arg(ui::fonts::DATA_FAMILY)
                                               .arg(colors[i])
                                               .arg(ui::colors::alpha(c, 0.06)));
        }
    }
}

void GeopoliticsScreen::on_events_loaded(services::geo::EventsPage page) {
    // Shared signal: the dashboard's hub refresh and MCP tools fire it too. Only
    // the reply to our own last request may replace the (possibly filtered) table.
    if (page.request_key != last_request_key_)
        return;
    events_inflight_ = false;
    update_pager();
    // The pane remembers (and persists) the page and draws it through
    // render_events_payload(); a stale or cancelled request id is dropped there.
    const quint64 id = std::exchange(events_request_id_, 0);
    if (id != 0)
        monitor_panel_->events_pane()->deliver(id, geo_scr_page_to_json(page));
}

void GeopoliticsScreen::update_credits(double remaining, double used) {
    if (!credits_label_ || remaining < 0)
        return;
    // Colour-coded so depletion is glanceable; fractional balances (0.35-credit
    // calls) used to be dropped entirely because they were parsed with toInt().
    const char* state = remaining < kGeoLowCredits ? "low" : (remaining < 100.0 ? "warn" : "ok");
    credits_label_->setText(tr("CREDITS: %1").arg(auth::format_credits(remaining)));
    credits_label_->setToolTip(used > 0 ? tr("Balance after the last request (it used %1 credits)")
                                              .arg(auth::format_credits(used))
                                        : tr("Fincept API credit balance"));
    credits_label_->setProperty("state", QString::fromLatin1(state));
    credits_label_->style()->unpolish(credits_label_);
    credits_label_->style()->polish(credits_label_);
    if (plans_btn_)
        plans_btn_->setVisible(remaining < kGeoLowCredits);
}

void GeopoliticsScreen::set_auto_refresh(bool on) {
    if (auto_refresh_btn_) {
        QSignalBlocker block(auto_refresh_btn_);
        auto_refresh_btn_->setChecked(on);
        auto_refresh_btn_->setText(on ? tr("AUTO ⟳ 5m") : tr("AUTO ⟳ OFF"));
    }
    if (on && isVisible())
        refresh_timer_->start();
    else if (!on)
        refresh_timer_->stop();
}

void GeopoliticsScreen::on_countries_loaded(QVector<services::geo::UniqueCountry> countries) {
    // The Relations tab is built from the same live statistics (kept for a
    // Relations panel built later).
    last_countries_ = countries;
    have_countries_ = true;
    if (relationship_panel_)
        relationship_panel_->set_countries(countries);
    if (stats_inflight_ && have_categories_) {
        stats_inflight_ = false;
        if (relationship_panel_)
            relationship_panel_->set_stats_loading(false);
    }
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
        // The server stores an unknown country as the string "null" (the events
        // query maps it to null, the countries query does not): never offer it.
        if (!c.country.isEmpty() && c.country.compare(QLatin1String("null"), Qt::CaseInsensitive) != 0)
            names.append(c.country);
    country_model_->setStringList(names);
}

void GeopoliticsScreen::on_cities_loaded(QStringList cities) {
    have_cities_ = true;
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
    last_categories_ = cats;
    have_categories_ = true;
    if (relationship_panel_)
        relationship_panel_->set_categories(cats);
    if (stats_inflight_ && have_countries_) {
        stats_inflight_ = false;
        if (relationship_panel_)
            relationship_panel_->set_stats_loading(false);
    }
    fill_category_combo(cats);
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
        const QString col_hex = ui::colors::legible(cat_col);
        const QString category = c.category;

        // Clicking a row hides / shows that category's pins on the map (the
        // table keeps every event; no request is made).
        auto* row = new GeoLegendToggleRow(legend_container_);
        auto restyle = [this, row, category, cat_col]() {
            const bool hidden = monitor_panel_ && monitor_panel_->hidden_categories().contains(category);
            auto* sw = row->findChild<QWidget*>(QStringLiteral("legend_swatch"));
            auto* lb = row->findChild<QLabel*>(QStringLiteral("legend_label"));
            if (sw)
                sw->setStyleSheet(QString("background:%1; border:1px solid %2;")
                                      .arg(hidden ? QStringLiteral("transparent") : ui::colors::legible(cat_col),
                                           hidden ? ui::colors::legible(cat_col) : ui::colors::scrim(0.35)));
            if (lb)
                lb->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; background:transparent;%4")
                                      .arg(hidden ? ui::colors::TEXT_TERTIARY() : ui::colors::TEXT_SECONDARY())
                                      .arg(ui::fonts::SMALL.get())
                                      .arg(ui::fonts::DATA_FAMILY())
                                      .arg(hidden ? QStringLiteral(" text-decoration:line-through;") : QString()));
            row->setToolTip(hidden ? tr("Hidden on the map — click to show these events again")
                                   : tr("Click to hide these events on the map"));
        };
        row->set_on_click([this, category, restyle]() {
            if (!monitor_panel_)
                return;
            QSet<QString> hidden = monitor_panel_->hidden_categories();
            if (!hidden.remove(category))
                hidden.insert(category);
            monitor_panel_->set_hidden_categories(hidden);
            restyle();
            ScreenStateManager::instance().notify_changed(this);
        });
        row->setObjectName("legend_row");
        row->setStyleSheet(QString("QWidget#legend_row { background:transparent; }"
                                   "QWidget#legend_row:hover { background:%1; }")
                               .arg(ui::colors::tint(cat_col, 0.06)));
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(4, 3, 4, 3);
        rl->setSpacing(8);

        // Square swatch in the pin colour (legible on the active theme, as the
        // map draws it), with a subtle outline so the chip reads on the surface.
        auto* swatch = new QWidget(row);
        swatch->setObjectName(QStringLiteral("legend_swatch"));
        swatch->setFixedSize(11, 11);
        rl->addWidget(swatch, 0, Qt::AlignVCenter);

        auto* lbl = new QLabel(services::geo::pretty_category(c.category), row);
        lbl->setObjectName(QStringLiteral("legend_label"));
        // The name gives way before the count does if the row is ever too narrow.
        lbl->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        lbl->setMinimumWidth(0);
        rl->addWidget(lbl, 1);
        restyle();

        if (c.event_count > 0) {
            auto* cnt = new QLabel(QString::number(c.event_count), row);
            cnt->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; font-weight:700;"
                                       "background:%4; border:1px solid %5;"
                                       "padding:1px 5px;")
                                   .arg(col_hex)
                                   .arg(ui::fonts::TINY.get())
                                   .arg(ui::fonts::DATA_FAMILY())
                                   .arg(ui::colors::tint(cat_col, 0.10))
                                   .arg(ui::colors::tint(cat_col, 0.30)));
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
    LOG_DEBUG("Geopolitics", QString("[%1] %2").arg(context, message));
    // The Relations tab's LOAD LIVE STATISTICS asked for these two lists.
    if (stats_inflight_ && (context == QStringLiteral("countries") || context == QStringLiteral("categories"))) {
        stats_inflight_ = false;
        if (relationship_panel_)
            relationship_panel_->set_stats_error(HttpClient::message_from_error(message.toStdString()));
    }
    // Reference-list failures (autocomplete / legend) are cosmetic: keep the status
    // bar for the events feed the user is actually looking at.
    if (context != QStringLiteral("events"))
        return;
    // "ERROR" alone told the user nothing and left the real cause in a log
    // file they can't see. Surface a short reason inline, the full text on
    // hover, and point at the retry that actually exists (APPLY FILTERS).
    // The map says it too, so an empty map is never unexplained.
    const int http_status = HttpClient::status_from_error(message.toStdString());
    if (http_status == 401)
        monitor_panel_->set_map_message(tr("Sign in to Fincept to load geopolitical events"));
    else if (http_status == 402)
        monitor_panel_->set_map_message(tr("Out of Fincept credits — events cannot load (3 credits per update)"));
    else if (http_status == 0)
        monitor_panel_->set_map_message(tr("Events could not be loaded (network) — APPLY FILTERS retries"));
    else
        monitor_panel_->set_map_message(
            tr("Events could not be loaded: %1").arg(HttpClient::message_from_error(message.toStdString()).left(80)));
    if (http_status == 402) {
        // Out of credits — retrying cannot help; point at the plans instead.
        status_label_->setText(tr("OUT OF CREDITS"));
        status_label_->setToolTip(HttpClient::message_from_error(message.toStdString()));
        set_status_color(ui::colors::DANGER());
        update_credits(0, 0);
    } else {
        const QString brief = message.simplified().left(72);
        status_label_->setText(brief.isEmpty() ? tr("ERROR") : tr("ERROR: %1").arg(brief));
        status_label_->setToolTip(tr("%1\n\n%2\n\nClick APPLY FILTERS to retry.").arg(context, message));
        set_status_color(ui::colors::DANGER());
    }
    // Our own load failed: the pane shows the reason with RETRY in place of the
    // table when nothing was on screen, else keeps the page ("refresh failed").
    if (const quint64 id = std::exchange(events_request_id_, 0); id != 0)
        monitor_panel_->events_pane()->fail(
            id, http_status == 402 ? tr("Out of Fincept credits — loading events costs 3 credits. Top up or change "
                                        "your plan (PLANS in the status bar), then retry.")
                                   : tr("Events could not be loaded: %1")
                                         .arg(HttpClient::message_from_error(message.toStdString()).left(160)));
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
    if (auto_refresh_btn_)
        state["auto_refresh"] = auto_refresh_btn_->isChecked();
    if (monitor_panel_) {
        state["map"] = monitor_panel_->map_view_state();
        state["map_hidden_categories"] = QStringList(monitor_panel_->hidden_categories().cbegin(),
                                                     monitor_panel_->hidden_categories().cend());
    }
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
    set_auto_refresh(state.value("auto_refresh", false).toBool());
    if (monitor_panel_) {
        // Last map view (camera, basemap) and the categories hidden from it.
        monitor_panel_->restore_map_view_state(state.value("map").toMap());
        const QStringList hidden = state.value("map_hidden_categories").toStringList();
        monitor_panel_->set_hidden_categories(QSet<QString>(hidden.cbegin(), hidden.cend()));
    }
}

// ── ISectionLinked ────────────────────────────────────────────────────────────

bool GeopoliticsScreen::open_section(const QString& section, const QString& /*focus*/) {
    // The tab buttons' own path (on_tab_changed), in their order.
    static const char* const kTabs[] = {"monitor", "hdx", "relations", "trade"};
    const QString key = section.trimmed().toLower();
    if (key.isEmpty())
        return true;
    for (int i = 0; i < int(sizeof(kTabs) / sizeof(kTabs[0])); ++i)
        if (key == QLatin1String(kTabs[i])) {
            on_tab_changed(i);
            return true;
        }
    return false;
}

} // namespace fincept::screens
