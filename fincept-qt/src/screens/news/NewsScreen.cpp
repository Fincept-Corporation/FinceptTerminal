#include "screens/news/NewsScreen.h"

#include "core/events/EventBus.h"
#include "core/keys/KeyConfigManager.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolDragSource.h"
#include "screens/news/NewsCommandBar.h"
#include "screens/news/NewsDetailPanel.h"
#include "screens/news/NewsFeedPanel.h"
#include "screens/news/NewsSidePanel.h"
#include "screens/news/NewsTickerStrip.h"
#include "screens/news/dialogs/RssFeedManagerDialog.h"
#include "services/cloud/CloudSyncEngine.h"
#include "services/news/NewsCorrelationService.h"
#include "services/news/NewsNlpService.h"
#include "services/notifications/NotificationService.h"
#include "storage/repositories/NewsArticleRepository.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/theme/StyleSheets.h"
#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QPointer>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

// Desktop notifications are only raised for articles this fresh — otherwise every
// FLASH / BREAKING / monitor hit already sitting in the feed fires at launch.
static constexpr int64_t kNewsNotifyMaxAgeSec = 1800;
// The notified-id sets only exist to dedupe; cap them so a long session can't
// grow them forever (clearing at worst re-notifies something still in the feed).
static constexpr int kNewsNotifiedSetCap = 20000;

NewsScreen::NewsScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("newsScreen");
    LOG_INFO("NewsScreen", "Applying news_screen_styles");
    setStyleSheet(ui::styles::news_screen_styles());
    LOG_INFO("NewsScreen", "news_screen_styles applied");

    // Restore persistent preferences
    QSettings settings;
    settings.beginGroup("news");
    active_category_ = settings.value("category", "ALL").toString();
    time_range_ = settings.value("time_range", "24H").toString();
    sort_mode_ = settings.value("sort_mode", "RELEVANCE").toString();
    view_mode_ = settings.value("view_mode", "WIRE").toString();
    // The variant and language filter were written to QSettings on change but
    // never read back, so a restart silently reset them.
    active_variant_ = settings.value("variant", "FULL").toString();
    active_lang_ = settings.value("language", "ALL").toString();
    settings.endGroup();

    build_ui();
    connect_signals();

    // Push the persisted preferences into the command bar so the pill
    // highlights (category / time / REL-NEW sort / WIRE-CLST view) reflect the
    // restored state on first paint instead of their hardcoded defaults.
    command_bar_->set_active_category(active_category_);
    command_bar_->set_active_time_range(time_range_);
    command_bar_->set_active_sort(sort_mode_);
    command_bar_->set_active_view(view_mode_);
    command_bar_->set_active_variant(active_variant_);
    command_bar_->set_active_language(active_lang_);

    // Drop a symbol anywhere on the News screen to filter the feed by that
    // ticker. Reuses the search-query pipeline so caching/highlighting stay
    // coherent with keyword search.
    symbol_dnd::installDropFilter(this, [this](const SymbolRef& ref, SymbolGroup) {
        if (ref.is_valid())
            on_group_symbol_changed(ref);
    });

    LOG_INFO("NewsScreen", "News screen constructed (no data fetch in constructor)");
}

void NewsScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Command bar with integrated intel strip (32px + 26px = 58px total)
    command_bar_ = new NewsCommandBar(this);
    root->addWidget(command_bar_);

    // Content area — horizontal layout: [drawer?] [feed] [detail?]
    auto* content_widget = new QWidget(this);
    content_widget->setObjectName("newsContentArea");
    content_layout_ = new QHBoxLayout(content_widget);
    content_layout_->setContentsMargins(0, 0, 0, 0);
    content_layout_->setSpacing(0);

    // Intel drawer (left, hidden by default, 280px)
    side_panel_ = new NewsSidePanel(content_widget);
    content_layout_->addWidget(side_panel_);

    // Feed panel (fills remaining space)
    feed_panel_ = new NewsFeedPanel(content_widget);
    content_layout_->addWidget(feed_panel_, 1);

    // Detail overlay (right, hidden by default, 420px)
    detail_panel_ = new NewsDetailPanel(content_widget);
    content_layout_->addWidget(detail_panel_);

    root->addWidget(content_widget, 1);

    // Ticker strip (22px)
    ticker_strip_ = new NewsTickerStrip(this);
    root->addWidget(ticker_strip_);
}

void NewsScreen::connect_signals() {
    // Command bar
    connect(command_bar_, &NewsCommandBar::category_changed, this, &NewsScreen::on_category_changed);
    connect(command_bar_, &NewsCommandBar::time_range_changed, this, &NewsScreen::on_time_range_changed);
    connect(command_bar_, &NewsCommandBar::sort_changed, this, &NewsScreen::on_sort_changed);
    connect(command_bar_, &NewsCommandBar::view_mode_changed, this, &NewsScreen::on_view_mode_changed);
    connect(command_bar_, &NewsCommandBar::search_changed, this, &NewsScreen::on_search_changed);
    connect(command_bar_, &NewsCommandBar::refresh_clicked, this, &NewsScreen::on_refresh);
    connect(command_bar_, &NewsCommandBar::drawer_toggle_requested, this, &NewsScreen::on_drawer_toggle);
    connect(command_bar_, &NewsCommandBar::manage_sources_clicked, this, &NewsScreen::on_manage_sources);

    // Live-feed badge: state mirrored from NewsService, click toggles
    // connect/disconnect.
    connect(&services::NewsService::instance(), &services::NewsService::live_state_changed, this,
            [this](bool connected) {
                if (command_bar_)
                    command_bar_->set_live_state(connected);
            });
    connect(command_bar_, &NewsCommandBar::live_toggle_clicked, this, []() {
        auto& svc = services::NewsService::instance();
        if (svc.is_live_connected()) {
            svc.disconnect_live_feed();
        } else {
            svc.connect_live_feed();
        }
    });

    // Auto-refresh cadence — 0 = manual, otherwise minutes.
    connect(command_bar_, &NewsCommandBar::refresh_interval_changed, this, [this](int minutes) {
        auto& svc = services::NewsService::instance();
        if (minutes <= 0) {
            svc.stop_auto_refresh();
        } else {
            svc.set_refresh_interval(minutes);
            if (visible_)
                svc.start_auto_refresh();
        }
        QSettings s;
        s.beginGroup("news");
        s.setValue("refresh_interval_minutes", minutes);
        s.endGroup();
    });

    // Feed panel
    connect(feed_panel_, &NewsFeedPanel::article_clicked, this, &NewsScreen::on_article_clicked);
    connect(feed_panel_, &NewsFeedPanel::cluster_clicked, this, &NewsScreen::on_cluster_clicked);
    connect(feed_panel_, &NewsFeedPanel::near_bottom, this, &NewsScreen::on_near_bottom);

    // Side panel (drawer)
    connect(side_panel_, &NewsSidePanel::category_clicked, this, &NewsScreen::on_sidebar_category_clicked);
    connect(side_panel_, &NewsSidePanel::article_clicked, this, &NewsScreen::on_sidebar_article_clicked);
    connect(side_panel_, &NewsSidePanel::monitor_added, this, &NewsScreen::on_monitor_added);
    connect(side_panel_, &NewsSidePanel::monitor_toggled, this, &NewsScreen::on_monitor_toggled);
    connect(side_panel_, &NewsSidePanel::monitor_deleted, this, &NewsScreen::on_monitor_deleted);
    connect(side_panel_, &NewsSidePanel::close_requested, this, &NewsScreen::on_drawer_toggle);

    // Detail panel (overlay)
    connect(detail_panel_, &NewsDetailPanel::analyze_requested, this, &NewsScreen::on_analyze_requested);
    connect(detail_panel_, &NewsDetailPanel::panel_closed, this, &NewsScreen::on_detail_closed);
    connect(detail_panel_, &NewsDetailPanel::bookmark_requested, this, [this](const services::NewsArticle& article) {
        // SQLite on a worker (P1). The article is upserted first so the toggle
        // can't fail with "Article not found" for a row that was never stored
        // (live pushes, or anything the 30-day prune already removed). On any
        // failure the button is re-synced from the DB instead of staying toggled.
        QPointer<NewsScreen> self = this;
        (void)QtConcurrent::run([self, article]() {
            auto& repo = fincept::NewsArticleRepository::instance();
            (void)repo.upsert_batch({article});
            auto r = repo.toggle_saved(article.id);
            QVector<services::NewsArticle> saved;
            if (r.is_ok()) {
                if (auto saved_r = repo.load_saved(); saved_r.is_ok())
                    saved = saved_r.value();
            }
            const bool ok = r.is_ok();
            const QString err = ok ? QString() : QString::fromStdString(r.error());
            if (!self)
                return;
            QMetaObject::invokeMethod(
                self,
                [self, ok, err, saved = std::move(saved)]() {
                    if (!self)
                        return;
                    if (ok)
                        self->side_panel_->update_saved(saved);
                    else
                        LOG_WARN("NewsScreen", "Bookmark toggle failed: " + err);
                    self->detail_panel_->refresh_bookmark_state();
                },
                Qt::QueuedConnection);
        });
    });

    // RTL toggle
    connect(command_bar_, &NewsCommandBar::rtl_toggled, this, []() { ui::set_rtl(!ui::is_rtl()); });

    // Variant selector
    connect(command_bar_, &NewsCommandBar::variant_changed, this, [this](const QString& variant) {
        active_variant_ = variant;
        QSettings s;
        s.beginGroup("news");
        s.setValue("variant", variant);
        s.endGroup();
        on_refresh();
    });

    // Language filter
    connect(command_bar_, &NewsCommandBar::language_filter_changed, this, [this](const QString& lang) {
        active_lang_ = lang.isEmpty() ? QStringLiteral("ALL") : lang;
        visible_article_count_ = PAGE_SIZE;
        QSettings s;
        s.beginGroup("news");
        s.setValue("language", active_lang_);
        s.endGroup();
        ScreenStateManager::instance().notify_changed(this);
        apply_filters_async();
    });

    // Feed context menu → "Filter feed by $TICKER" shows the ticker in the box.
    connect(feed_panel_, &NewsFeedPanel::ticker_filter_requested, this, [this](const QString& ticker) {
        SymbolRef ref;
        ref.symbol = ticker;
        on_group_symbol_changed(ref);
    });

    // Pulse animation timer (500ms cycle for new item glow)
    pulse_timer_ = new QTimer(this);
    pulse_timer_->setInterval(500);
    connect(pulse_timer_, &QTimer::timeout, this, [this]() {
        if (visible_)
            feed_panel_->model()->advance_pulse();
    });

    // Restore persisted auto-refresh cadence (default 10 min).
    {
        QSettings s;
        s.beginGroup("news");
        const int minutes = s.value("refresh_interval_minutes", 10).toInt();
        s.endGroup();
        command_bar_->set_refresh_interval_minutes(minutes);
        if (minutes > 0)
            services::NewsService::instance().set_refresh_interval(minutes);
    }

    // Enrichment debounce — coalesce rapid filter changes (search typing,
    // category clicks) into a single round of NER + geo + correlation +
    // prediction calls so we don't saturate the Python runner.
    enrichment_timer_ = new QTimer(this);
    enrichment_timer_->setInterval(350);
    enrichment_timer_->setSingleShot(true);
    connect(enrichment_timer_, &QTimer::timeout, this, &NewsScreen::run_enrichment_now);

    // Debounced DB seen-write timer
    seen_flush_timer_ = new QTimer(this);
    seen_flush_timer_->setInterval(1000);
    seen_flush_timer_->setSingleShot(true);
    connect(seen_flush_timer_, &QTimer::timeout, this, [this]() {
        if (pending_seen_ids_.isEmpty())
            return;
        const QSet<QString> ids = std::move(pending_seen_ids_);
        pending_seen_ids_.clear();
        const int flushed = static_cast<int>(ids.size());
        QPointer<NewsScreen> self = this;
        (void)QtConcurrent::run([ids, self, flushed]() {
            for (const auto& id : ids)
                fincept::NewsArticleRepository::instance().mark_seen(id);
            if (self) {
                QMetaObject::invokeMethod(
                    self,
                    [self, flushed]() {
                        if (self)
                            LOG_DEBUG("NewsScreen", QString("Flushed %1 seen IDs to DB").arg(flushed));
                    },
                    Qt::QueuedConnection);
            }
        });
    });

    // Scroll-based seen tracking. Only the rows between the first and last one
    // inside the viewport are looked at (indexAt on the viewport edges) — the
    // old loop asked visualRect() of EVERY row on every scroll tick.
    connect(feed_panel_->list_view()->verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        auto* lv = feed_panel_->list_view();
        auto* model = feed_panel_->model();
        const int row_count = model->rowCount();
        if (row_count == 0)
            return;
        const QModelIndex first = lv->indexAt(QPoint(4, 2));
        if (!first.isValid())
            return;
        const QModelIndex last = lv->indexAt(QPoint(4, lv->viewport()->height() - 2));
        const int first_row = first.row();
        const int last_row = last.isValid() ? last.row() : row_count - 1;
        for (int i = first_row; i <= last_row && i < row_count; ++i) {
            const QString id = model->article_at(i).id;
            if (id.isEmpty())
                continue;
            model->mark_seen(id);
            pending_seen_ids_.insert(id);
        }
        if (!pending_seen_ids_.isEmpty())
            seen_flush_timer_->start();
    });

    // Summarize button
    connect(command_bar_, &NewsCommandBar::summarize_clicked, this, [this]() {
        if (filtered_articles_.isEmpty())
            return;
        command_bar_->set_summarizing(true);
        QPointer<NewsScreen> self = this;
        services::NewsService::instance().summarize_headlines(filtered_articles_, 8, [self](bool ok, QString summary) {
            if (!self)
                return;
            self->command_bar_->set_summarizing(false);
            if (ok)
                self->command_bar_->show_summary(summary);
        });
    });
    connect(detail_panel_, &NewsDetailPanel::related_article_clicked, this, &NewsScreen::on_related_clicked);

    // Ticker strip
    connect(ticker_strip_, &NewsTickerStrip::article_clicked, this, &NewsScreen::on_article_clicked);

    // Service auto-refresh
    connect(&services::NewsService::instance(), &services::NewsService::articles_updated, this,
            [this](QVector<services::NewsArticle> articles) {
                if (!visible_)
                    return;
                all_articles_ = std::move(articles);
                apply_filters_async();
            });

    // Keyboard shortcuts via KeyConfigManager
    auto& km = KeyConfigManager::instance();

    auto* act_next = km.action(KeyAction::NewsNext);
    act_next->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(act_next);
    connect(act_next, &QAction::triggered, this, [this]() { feed_panel_->select_next(); });

    auto* act_prev = km.action(KeyAction::NewsPrev);
    act_prev->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(act_prev);
    connect(act_prev, &QAction::triggered, this, [this]() { feed_panel_->select_previous(); });

    auto* act_open = km.action(KeyAction::NewsOpen);
    act_open->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(act_open);
    connect(act_open, &QAction::triggered, this, [this]() {
        auto idx = feed_panel_->list_view()->currentIndex();
        if (idx.isValid()) {
            auto article = feed_panel_->model()->article_at(idx.row());
            // Feed links are untrusted — web URLs only.
            const QUrl url(article.link);
            if (url.isValid() && (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https")))
                QDesktopServices::openUrl(url);
        }
    });

    auto* act_close = km.action(KeyAction::NewsClose);
    act_close->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(act_close);
    connect(act_close, &QAction::triggered, this, [this]() {
        if (detail_panel_->is_panel_open())
            detail_panel_->close_panel();
        else if (side_panel_->is_drawer_open())
            side_panel_->toggle_drawer();
    });
}

// ── Lifecycle ───────────────────────────────────────────────────────────────

void NewsScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    visible_ = true;
    ticker_strip_->resume();
    pulse_timer_->start();
    // Rate-gated pull of cloud news monitors + feeds on screen entry (no-op when sync is off).
    fincept::services::cloud::CloudSyncEngine::instance().request_pull(QStringLiteral("news_monitor"));
    fincept::services::cloud::CloudSyncEngine::instance().request_pull(QStringLiteral("news_feed"));
    {
        QSettings s;
        s.beginGroup("news");
        const int minutes = s.value("refresh_interval_minutes", 10).toInt();
        s.endGroup();
        if (minutes > 0)
            services::NewsService::instance().start_auto_refresh();
    }
    services::NewsService::instance().connect_live_feed();
    subscribe_mcp_events();

    if (!first_show_) {
        // Returning to the screen: articles_updated is ignored while hidden, so
        // the list could be hours old with the next auto-refresh a full interval
        // away. force=false serves the 10-minute cache instantly when it is
        // still fresh and only hits the network when it has expired.
        refresh_data(/*force=*/false);
    }

    if (first_show_) {
        first_show_ = false;

        // First entry used to run a schema migration (PRAGMA table_info +
        // conditional ALTER TABLE) plus two unbounded SELECTs straight on the
        // GUI thread, so opening News stalled in proportion to all accumulated
        // history — invisible on a fresh install, worse every week. It now runs
        // on a worker (Database hands out a per-thread SQLite connection) and
        // the results are applied back here, guarded on both hops (§P1/§P8).
        //
        // TODO(cross-file): ensure_seen_column()/ensure_saved_column() are
        // startup schema work and belong next to the other migrations in
        // Database::open(); they sit here only until that move lands.
        const int64_t since_ts = QDateTime::currentSecsSinceEpoch() - (30LL * 86400);
        QPointer<NewsScreen> self = this;
        (void)QtConcurrent::run([self, since_ts]() {
            auto& repo = fincept::NewsArticleRepository::instance();
            repo.ensure_seen_column();
            repo.ensure_saved_column();

            QSet<QString> seen_ids;
            if (auto seen_result = repo.load_seen_ids(since_ts); seen_result.is_ok())
                seen_ids = seen_result.value();

            QVector<services::NewsArticle> saved;
            if (auto saved_result = repo.load_saved(); saved_result.is_ok())
                saved = saved_result.value();

            if (!self)
                return;
            QMetaObject::invokeMethod(
                self,
                [self, seen_ids = std::move(seen_ids), saved = std::move(saved)]() {
                    if (!self)
                        return;
                    for (const auto& id : seen_ids)
                        self->feed_panel_->model()->mark_seen(id);
                    self->side_panel_->update_saved(saved);
                    LOG_INFO("NewsScreen", QString("Restored %1 seen IDs and %2 bookmarks from DB")
                                               .arg(seen_ids.size())
                                               .arg(saved.size()));
                },
                Qt::QueuedConnection);
        });

        // Independent of the restore above — kick the feed fetch immediately so
        // the screen starts filling while the worker runs.
        LOG_INFO("NewsScreen", "showEvent: calling on_refresh");
        on_refresh();
        LOG_INFO("NewsScreen", "showEvent: on_refresh returned");
    }
}

void NewsScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    visible_ = false;
    ticker_strip_->pause();
    pulse_timer_->stop();
    enrichment_timer_->stop(); // no point spawning Python for a hidden screen
    services::NewsService::instance().stop_auto_refresh();
    services::NewsService::instance().disconnect_live_feed();
    filter_generation_.fetch_add(1, std::memory_order_relaxed);
    unsubscribe_mcp_events();
}

// ── MCP-driven UI sync ──────────────────────────────────────────────────────
// MCP tools (called from AI Chat / Finagent) publish events when the LLM
// mutates news state. We subscribe while visible so the UI reflects the
// change without the user having to manually refresh. EventBus handlers
// can fire on a worker thread; we marshal to the screen's thread before
// touching the model/panel.

void NewsScreen::subscribe_mcp_events() {
    if (!mcp_event_subs_.isEmpty())
        return; // idempotent

    QPointer<NewsScreen> self = this;
    auto on_monitors_changed = [self](const QVariantMap&) {
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self]() {
                if (!self || !self->visible_)
                    return;
                self->update_monitors();
            },
            Qt::QueuedConnection);
    };
    auto on_refresh_requested = [self](const QVariantMap&) {
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self]() {
                if (!self || !self->visible_)
                    return;
                self->refresh_data(/*force=*/true);
            },
            Qt::QueuedConnection);
    };

    auto& bus = EventBus::instance();
    mcp_event_subs_.append(bus.subscribe(this, "news.monitor_added", on_monitors_changed));
    mcp_event_subs_.append(bus.subscribe(this, "news.monitor_toggled", on_monitors_changed));
    mcp_event_subs_.append(bus.subscribe(this, "news.monitor_deleted", on_monitors_changed));
    mcp_event_subs_.append(bus.subscribe(this, "news.refresh_requested", on_refresh_requested));
}

void NewsScreen::unsubscribe_mcp_events() {
    auto& bus = EventBus::instance();
    for (auto id : mcp_event_subs_)
        bus.unsubscribe(id);
    mcp_event_subs_.clear();
}

// ── Slots ───────────────────────────────────────────────────────────────────

void NewsScreen::on_category_changed(const QString& category) {
    active_category_ = category;
    visible_article_count_ = PAGE_SIZE;
    QSettings s;
    s.beginGroup("news");
    s.setValue("category", category);
    s.endGroup();
    ScreenStateManager::instance().notify_changed(this);
    apply_filters_async();
}

void NewsScreen::on_time_range_changed(const QString& range) {
    time_range_ = range;
    visible_article_count_ = PAGE_SIZE;
    QSettings s;
    s.beginGroup("news");
    s.setValue("time_range", range);
    s.endGroup();
    ScreenStateManager::instance().notify_changed(this);
    apply_filters_async();
}

void NewsScreen::on_sort_changed(const QString& sort) {
    sort_mode_ = sort;
    QSettings s;
    s.beginGroup("news");
    s.setValue("sort_mode", sort);
    s.endGroup();
    ScreenStateManager::instance().notify_changed(this);
    apply_filters_async();
}

void NewsScreen::on_view_mode_changed(const QString& mode) {
    view_mode_ = mode;
    QSettings s;
    s.beginGroup("news");
    s.setValue("view_mode", mode);
    s.endGroup();
    ScreenStateManager::instance().notify_changed(this);
    feed_panel_->model()->set_view_mode(mode);
}

void NewsScreen::on_search_changed(const QString& query) {
    search_query_ = query;
    visible_article_count_ = PAGE_SIZE;
    ScreenStateManager::instance().notify_changed(this);
    apply_filters_async();
}

void NewsScreen::on_group_symbol_changed(const SymbolRef& ref) {
    if (!ref.is_valid())
        return;
    // Route through the same pipeline the search box uses so caching,
    // highlighting, and notifications stay consistent. The symbol is also
    // shown in the search box (without re-triggering it) — otherwise the feed
    // is silently narrowed to a handful of articles with no visible reason and
    // no way to clear it.
    command_bar_->set_search_text(ref.symbol);
    on_search_changed(ref.symbol);
}

// Drop hook installed in the constructor below is wired via symbol_dnd —
// see constructor edits for the installDropFilter call that forwards to
// on_search_changed().

void NewsScreen::on_refresh() {
    refresh_data(true);
}

void NewsScreen::on_manage_sources() {
    RssFeedManagerDialog dlg(this);
    dlg.exec();
    if (dlg.changed()) {
        LOG_INFO("NewsScreen", "RSS feed sources changed; reloading");
        refresh_data(true);
    }
}

void NewsScreen::on_article_clicked(const services::NewsArticle& article) {
    detail_panel_->show_article(article);
    feed_panel_->set_selected(article.id);

    // Re-show a previously-run AI analysis for this article, if one was saved.
    // Only a fresh ANALYZE click re-fetches/overwrites it.
    if (auto cached = services::NewsService::instance().cached_analysis(article.link))
        detail_panel_->show_analysis(*cached);

    // Find related articles from the same cluster
    for (const auto& cluster : clusters_) {
        if (cluster.lead_article.id == article.id ||
            std::any_of(cluster.articles.begin(), cluster.articles.end(),
                        [&](const services::NewsArticle& a) { return a.id == article.id; })) {
            QVector<services::NewsArticle> related;
            for (const auto& a : cluster.articles) {
                if (a.id != article.id)
                    related.append(a);
            }
            detail_panel_->show_related(related);
            break;
        }
    }

    // Check monitor matches for this article
    auto monitors = services::NewsMonitorService::instance().get_monitors();
    QVector<QPair<services::NewsMonitor, QStringList>> article_matches;
    for (const auto& monitor : monitors) {
        if (!monitor.enabled)
            continue;
        for (const auto& kw : monitor.keywords) {
            if (article.headline.contains(kw, Qt::CaseInsensitive) ||
                article.summary.contains(kw, Qt::CaseInsensitive)) {
                QStringList matched_kws;
                for (const auto& k : monitor.keywords) {
                    if (article.headline.contains(k, Qt::CaseInsensitive) ||
                        article.summary.contains(k, Qt::CaseInsensitive))
                        matched_kws.append(k);
                }
                article_matches.append({monitor, matched_kws});
                break;
            }
        }
    }
    detail_panel_->show_monitor_matches(article_matches);

    // Show NER entities for this article
    auto ner = services::NewsNlpService::instance().cached_ner();
    for (const auto& e : ner.per_article) {
        if (e.id == article.id) {
            detail_panel_->show_entities(e);
            break;
        }
    }

    // If geolocated, fetch nearby infrastructure
    auto geo = services::NewsNlpService::instance().cached_geo();
    for (const auto& g : geo) {
        if (g.id == article.id && !g.locations.isEmpty()) {
            QPointer<NewsScreen> geo_self = this;
            services::NewsNlpService::instance().nearby_infrastructure(
                g.primary_lat, g.primary_lon, 50, [geo_self](bool ok, QVector<services::InfrastructureItem> items) {
                    if (geo_self && ok)
                        geo_self->detail_panel_->show_infrastructure(items);
                });
            break;
        }
    }
}

void NewsScreen::on_cluster_clicked(const services::NewsCluster& cluster) {
    detail_panel_->show_article(cluster.lead_article);

    QVector<services::NewsArticle> related;
    for (const auto& a : cluster.articles) {
        if (a.id != cluster.lead_article.id)
            related.append(a);
    }
    detail_panel_->show_related(related);
}

void NewsScreen::on_near_bottom() {
    if (visible_article_count_ >= filtered_articles_.size())
        return;
    visible_article_count_ += PAGE_SIZE;
    auto visible = filtered_articles_.mid(0, visible_article_count_);
    feed_panel_->model()->set_wire_articles(visible);
    LOG_INFO("NewsScreen", QString("Lazy-loaded to %1 articles").arg(visible.size()));
}

void NewsScreen::on_sidebar_category_clicked(const QString& category) {
    active_category_ = category;
    command_bar_->set_active_category(category);
    visible_article_count_ = PAGE_SIZE;
    apply_filters_async();
}

void NewsScreen::on_sidebar_article_clicked(const services::NewsArticle& article) {
    on_article_clicked(article);
}

void NewsScreen::on_monitor_added(const QString& label, const QStringList& keywords) {
    services::NewsMonitorService::instance().add_monitor(label, keywords);
    update_monitors();
}

void NewsScreen::on_monitor_toggled(const QString& id) {
    services::NewsMonitorService::instance().toggle_monitor(id);
    update_monitors();
}

void NewsScreen::on_monitor_deleted(const QString& id) {
    services::NewsMonitorService::instance().delete_monitor(id);
    update_monitors();
}

void NewsScreen::on_analyze_requested(const QString& url) {
    QPointer<NewsScreen> self = this;
    services::NewsService::instance().analyze_article(url, [self, url](bool ok, services::NewsAnalysis analysis) {
        if (!self)
            return;
        // The request can take many seconds; the user may have opened another
        // article since. Don't paint this article's analysis over a different one.
        if (self->detail_panel_->current_article_link() != url)
            return;
        if (ok)
            self->detail_panel_->show_analysis(analysis);
        else
            self->detail_panel_->show_analysis_failed(); // used to leave the button spinning for 30 s
    });
}

void NewsScreen::on_related_clicked(const services::NewsArticle& article) {
    on_article_clicked(article);
}

void NewsScreen::on_drawer_toggle() {
    side_panel_->toggle_drawer();
}

void NewsScreen::on_detail_closed() {
    // Feed gets full width back when detail panel closes
    feed_panel_->model()->set_selected_id("");
}

// ── Core data pipeline ──────────────────────────────────────────────────────

void NewsScreen::refresh_data(bool force) {
    LOG_INFO("NewsScreen", "refresh_data: start");
    // A refresh over an existing list keeps showing that list (P11: never blank
    // the screen while waiting) instead of swapping in the skeleton, and the
    // partial snapshots below don't replace it — they used to shrink a full list
    // to the first feed's handful of articles and grow it back every refresh.
    const bool had_articles = !all_articles_.isEmpty();
    loading_ = true;
    command_bar_->set_loading(true);
    if (!had_articles)
        feed_panel_->set_loading(true);

    auto* svc = &services::NewsService::instance();

    QPointer<NewsScreen> self = this;
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(svc, &services::NewsService::articles_partial, this,
                    [self, conn, had_articles](QVector<services::NewsArticle> articles, int done, int total) {
                        if (!self)
                            return;
                        self->command_bar_->set_loading_progress(done, total);
                        if (!had_articles && !articles.isEmpty()) {
                            self->all_articles_ = std::move(articles);
                            // Hide skeleton as soon as we have any articles to render —
                            // otherwise the overlay traps progressive partials and the
                            // user sees a blank list until the slowest feed times out.
                            self->feed_panel_->set_loading(false);
                            self->apply_filters_async();
                        }
                        if (done == total)
                            QObject::disconnect(*conn);
                    });

    svc->fetch_all_news_progressive(force, [self, conn, svc](bool ok, QVector<services::NewsArticle> articles) {
        if (!self)
            return;
        // The fetch is over — drop the partial-snapshot connection even if the
        // service never emitted a done == total partial (otherwise each refresh
        // would leave another handler behind).
        QObject::disconnect(*conn);
        self->loading_ = false;
        self->command_bar_->set_loading(false);
        self->command_bar_->set_loading_progress(0, 0);
        self->feed_panel_->set_loading(false);
        if (!ok) {
            LOG_ERROR("NewsScreen", "Failed to fetch news");
            if (self->all_articles_.isEmpty())
                self->feed_panel_->set_empty_state(true);
            return;
        }
        // Every feed failing (network loss) yields an empty result: keep the
        // last list rather than wiping it. An empty result with ZERO enabled
        // feeds is legitimate and does clear the screen.
        if (articles.isEmpty() && !self->all_articles_.isEmpty() && svc->feed_count() > 0) {
            LOG_WARN("NewsScreen", "Refresh returned no articles — keeping the previous list");
            return;
        }
        self->all_articles_ = std::move(articles);
        // Show empty state only when literally no articles came back across
        // every feed — otherwise apply_filters_async will populate the list.
        self->feed_panel_->set_empty_state(self->all_articles_.isEmpty());
        self->apply_filters_async();
    });
}

void NewsScreen::apply_filters_async() {
    int gen = filter_generation_.fetch_add(1, std::memory_order_relaxed) + 1;

    QPointer<NewsScreen> self = this;
    auto articles_copy = all_articles_;
    const QString category = active_category_;
    const QString time_range = time_range_;
    const QString search_lower = search_query_.toLower();
    const QString sort = sort_mode_;
    const QString variant = active_variant_;
    const QString lang_filter = active_lang_.toLower();

    (void)QtConcurrent::run([self, gen, articles_copy = std::move(articles_copy), category, time_range, search_lower,
                             sort, variant, lang_filter]() mutable {
        // 7D/30D history merge — moved OFF the UI thread. Reading up to 5000 rows
        // from SQLite on the UI thread stuttered the UI on each filter keystroke.
        // Safe on a worker: Database hands out per-thread cloned connections.
        if (time_range == "7D" || time_range == "30D") {
            const int64_t hist_window = (time_range == "30D") ? (30LL * 86400) : (7LL * 86400);
            const int64_t cutoff = QDateTime::currentSecsSinceEpoch() - hist_window;
            QSet<QString> live_ids;
            live_ids.reserve(articles_copy.size());
            for (const auto& a : std::as_const(articles_copy))
                live_ids.insert(a.id);
            auto merge_into = [&](const QVector<services::NewsArticle>& extras) {
                for (const auto& a : extras)
                    if (!live_ids.contains(a.id))
                        articles_copy.append(a);
            };
            if (!search_lower.isEmpty()) {
                auto fts_result = fincept::NewsArticleRepository::instance().search_fts(search_lower, cutoff, 1000);
                if (fts_result.is_ok())
                    merge_into(fts_result.value());
            } else {
                auto db_result = fincept::NewsArticleRepository::instance().load_recent(cutoff, {}, 5000);
                if (db_result.is_ok())
                    merge_into(db_result.value());
            }
        }
        int64_t window_sec = 0;
        if (time_range == "1H")
            window_sec = 3600;
        else if (time_range == "6H")
            window_sec = 21600;
        else if (time_range == "24H")
            window_sec = 86400;
        else if (time_range == "48H")
            window_sec = 172800;
        else if (time_range == "7D")
            window_sec = 604800;
        else if (time_range == "30D")
            window_sec = 30LL * 86400;

        int64_t cutoff = 0;
        if (window_sec > 0)
            cutoff = QDateTime::currentSecsSinceEpoch() - window_sec;

        QVector<services::NewsArticle> filtered;
        filtered.reserve(articles_copy.size());

        QMap<QString, int> category_counts;
        int bullish = 0, bearish = 0, neutral = 0;

        // Per-stage rejection counters — surface the cause when we silently
        // filter the entire input down to zero (the single highest-impact
        // failure mode in this pipeline).
        int rejected_time = 0, rejected_variant = 0, rejected_category = 0, rejected_search = 0;
        int rejected_lang = 0;

        for (const auto& a : articles_copy) {
            if (cutoff > 0 && a.sort_ts < cutoff) {
                ++rejected_time;
                continue;
            }

            // Language filter — drop articles whose detected lang doesn't
            // match the picked ISO code. Empty `a.lang` is treated as
            // unknown and passes through any filter (parser only sets it
            // for feeds where the source advertises a language).
            if (lang_filter != "all" && !a.lang.isEmpty() && a.lang.toLower() != lang_filter) {
                ++rejected_lang;
                continue;
            }

            // Variant filter
            if (variant == "FINANCE" && a.category != "MARKETS" && a.category != "EARNINGS" &&
                a.category != "ECONOMIC" && a.category != "REGULATORY") {
                ++rejected_variant;
                continue;
            }
            if (variant == "CRYPTO" && a.category != "CRYPTO" && a.category != "TECH") {
                ++rejected_variant;
                continue;
            }
            if (variant == "MACRO" && a.category != "ECONOMIC" && a.category != "REGULATORY" &&
                a.category != "GEOPOLITICS" && a.category != "ENERGY") {
                ++rejected_variant;
                continue;
            }

            // Category filter
            if (category != "ALL") {
                static const QHash<QString, QString> cat_map = {
                    {"MKT", "MARKETS"}, {"EARN", "EARNINGS"}, {"ECO", "ECONOMIC"},    {"TECH", "TECH"},
                    {"NRG", "ENERGY"},  {"CRPT", "CRYPTO"},   {"GEO", "GEOPOLITICS"}, {"DEF", "DEFENSE"},
                };
                auto it = cat_map.find(category);
                if (it != cat_map.end() && a.category != it.value()) {
                    ++rejected_category;
                    continue;
                }
            }

            // Search filter
            if (!search_lower.isEmpty()) {
                const QString hl = a.headline.toLower();
                const QString sum = a.summary.toLower();
                bool match = hl.contains(search_lower) || sum.contains(search_lower) ||
                             a.source.toLower().contains(search_lower);
                if (!match) {
                    for (const auto& t : std::as_const(a.tickers)) {
                        if (t.toLower().contains(search_lower)) {
                            match = true;
                            break;
                        }
                    }
                }
                if (!match) {
                    ++rejected_search;
                    continue;
                }
            }

            filtered.append(a);
            category_counts[a.category]++;

            if (a.sentiment == services::Sentiment::BULLISH)
                bullish++;
            else if (a.sentiment == services::Sentiment::BEARISH)
                bearish++;
            else
                neutral++;
        }

        // Sort
        if (sort == "NEWEST") {
            std::sort(filtered.begin(), filtered.end(),
                      [](const auto& a, const auto& b) { return a.sort_ts > b.sort_ts; });
        } else {
            std::sort(filtered.begin(), filtered.end(), [](const auto& a, const auto& b) {
                if (a.priority != b.priority)
                    return static_cast<int>(a.priority) < static_cast<int>(b.priority);
                return a.sort_ts > b.sort_ts;
            });
        }

        auto clusters = services::cluster_articles(filtered);

        // When the entire input is filtered to zero, surface why so the cause
        // is visible in logs without users digging into the source.
        if (!articles_copy.isEmpty() && filtered.isEmpty()) {
            LOG_WARN("NewsScreen", QString("Filter rejected ALL %1 articles "
                                           "(time=%2 lang=%3 variant=%4 category=%5 search=%6 range=%7)")
                                       .arg(articles_copy.size())
                                       .arg(rejected_time)
                                       .arg(rejected_lang)
                                       .arg(rejected_variant)
                                       .arg(rejected_category)
                                       .arg(rejected_search)
                                       .arg(time_range));
        }

        QMetaObject::invokeMethod(
            self,
            [self, gen, filtered, clusters, category_counts, bullish, bearish, neutral]() {
                if (!self)
                    return;
                if (gen < self->filter_generation_.load(std::memory_order_relaxed)) {
                    LOG_INFO("NewsScreen", QString("Rejected stale filter gen %1").arg(gen));
                    return;
                }
                self->update_ui_from_filtered(gen, filtered, clusters, category_counts, bullish, bearish, neutral);
            },
            Qt::QueuedConnection);
    });
}

void NewsScreen::update_ui_from_filtered(int /*generation*/, const QVector<services::NewsArticle>& filtered,
                                         const QVector<services::NewsCluster>& clusters,
                                         const QMap<QString, int>& category_counts, int bullish, int bearish,
                                         int neutral) {

    LOG_INFO("NewsScreen",
             QString("update_ui_from_filtered: %1 articles, %2 clusters").arg(filtered.size()).arg(clusters.size()));
    filtered_articles_ = filtered;
    clusters_ = clusters;

    // Update feed model
    auto visible = filtered.mid(0, visible_article_count_);
    feed_panel_->model()->set_wire_articles(visible);
    feed_panel_->model()->set_clusters(clusters);
    feed_panel_->model()->set_view_mode(view_mode_);

    // Empty-state toggle: only show "no articles" when we're not still loading
    // and the filter genuinely produced nothing. Hide it as soon as the user
    // has any rows to look at.
    if (!loading_)
        feed_panel_->set_empty_state(filtered.isEmpty());
    else if (!filtered.isEmpty())
        feed_panel_->set_empty_state(false);

    // Command bar counts
    command_bar_->set_article_count(filtered.size());
    int alert_count = static_cast<int>(services::get_breaking_clusters(clusters).size());
    command_bar_->set_alert_count(alert_count);
    command_bar_->set_unseen_count(feed_panel_->model()->unseen_count());

    // Intel strip stats + sentiment (replaces side panel stats)
    command_bar_->update_stats(services::NewsService::instance().feed_count(), filtered.size(), clusters.size(),
                               services::NewsService::instance().active_sources().size());
    command_bar_->update_sentiment(bullish, bearish, neutral);

    // Side panel (drawer) — still gets data for when user opens it
    side_panel_->update_sentiment(bullish, bearish, neutral);

    // Top 5 stories
    QVector<services::NewsArticle> top5;
    for (int i = 0; i < std::min(5, static_cast<int>(filtered.size())); i++)
        top5.append(filtered[i]);
    side_panel_->update_top_stories(top5);
    side_panel_->update_categories(category_counts);

    // Breaking banner
    auto breaking = services::get_breaking_clusters(clusters);
    if (!breaking.isEmpty()) {
        feed_panel_->show_breaking(breaking);

        auto& repo = fincept::SettingsRepository::instance();
        auto get_bool = [&](const QString& key, bool def) -> bool {
            auto r = repo.get(key);
            return r.is_ok() && !r.value().isEmpty() ? (r.value() == "1") : def;
        };
        if (get_bool("notifications.news_breaking", true)) {
            using namespace fincept::notifications;
            const int64_t notify_now = QDateTime::currentSecsSinceEpoch();
            for (const auto& cluster : breaking) {
                const QString& lead_id = cluster.lead_article.id;
                if (notify_now - cluster.lead_article.sort_ts > kNewsNotifyMaxAgeSec)
                    continue; // stale: already in the feed when we started
                if (notified_breaking_.contains(lead_id))
                    continue;
                if (notified_breaking_.size() > kNewsNotifiedSetCap)
                    notified_breaking_.clear();
                notified_breaking_.insert(lead_id);

                NotificationRequest req;
                req.title = QString("BREAKING: %1").arg(cluster.lead_article.headline.left(80));
                req.message = cluster.lead_article.summary.isEmpty() ? cluster.lead_article.source
                                                                     : cluster.lead_article.summary.left(160);
                req.level = cluster.lead_article.priority == services::Priority::FLASH ? NotifLevel::Critical
                                                                                       : NotifLevel::Alert;
                req.trigger = NotifTrigger::NewsAlert;
                NotificationService::instance().send(req);
            }
        }
    }

    // Ticker strip
    QVector<services::NewsArticle> ticker_articles;
    for (const auto& a : filtered) {
        if (a.priority == services::Priority::FLASH || a.priority == services::Priority::URGENT ||
            a.priority == services::Priority::BREAKING)
            ticker_articles.append(a);
    }
    ticker_strip_->set_articles(ticker_articles);

    // Monitors
    update_monitors();

    // Deviations
    compute_deviations();

    // Intel-drawer enrichment (NER, geolocation, correlation signals,
    // prediction-market odds) — debounced to avoid swamping the Python
    // runner during rapid filter changes.
    request_enrichment();
}

void NewsScreen::request_enrichment() {
    if (!enrichment_timer_)
        return;
    enrichment_timer_->start();
}

void NewsScreen::run_enrichment_now() {
    if (filtered_articles_.isEmpty())
        return;

    // Cap input size — Python NER on 1000+ articles is slow and adds
    // little for the sidebar's top-N rendering. The top of the filtered
    // list is what the user is looking at.
    constexpr int kMaxEnrichArticles = 200;
    const auto subset = filtered_articles_.mid(0, kMaxEnrichArticles);

    const int gen = enrichment_generation_.fetch_add(1, std::memory_order_relaxed) + 1;
    QPointer<NewsScreen> self = this;

    // NER → entities sidebar + per-article cache (used by detail panel
    // "key actors" lookup at on_article_clicked).
    services::NewsNlpService::instance().extract_entities(subset, [self, gen](bool ok, services::NerResult ner) {
        if (!self || !ok)
            return;
        if (gen < self->enrichment_generation_.load(std::memory_order_relaxed) - 1)
            return;
        self->side_panel_->update_entities(ner);
    });

    // Geolocation → locations sidebar + feed-row geo dot flag.
    services::NewsNlpService::instance().geolocate_articles(
        subset, [self, gen](bool ok, QVector<services::ArticleGeo> geo) {
            if (!self || !ok)
                return;
            if (gen < self->enrichment_generation_.load(std::memory_order_relaxed) - 1)
                return;
            self->side_panel_->update_locations(geo);
            QSet<QString> geo_ids;
            geo_ids.reserve(geo.size());
            for (const auto& g : geo)
                if (!g.locations.isEmpty())
                    geo_ids.insert(g.id);
            self->feed_panel_->model()->set_geo_articles(geo_ids);
        });

    // Correlation signals → signals sidebar.
    services::NewsCorrelationService::instance().detect_signals(
        subset, [self, gen](bool ok, QVector<services::CorrelationSignal> sigs) {
            if (!self || !ok)
                return;
            if (gen < self->enrichment_generation_.load(std::memory_order_relaxed) - 1)
                return;
            self->side_panel_->update_signals(sigs);
        });

    // Prediction markets → predictions sidebar (one-shot per session,
    // not article-dependent — but keeping it inside the same enrichment
    // pass keeps the UI lit consistently after a refresh).
    services::NewsCorrelationService::instance().fetch_predictions(
        [self, gen](bool ok, QVector<services::PredictionMarket> preds) {
            if (!self || !ok)
                return;
            if (gen < self->enrichment_generation_.load(std::memory_order_relaxed) - 1)
                return;
            self->side_panel_->update_predictions(preds);
        });
}

void NewsScreen::update_monitors() {
    auto monitors = services::NewsMonitorService::instance().get_monitors();
    auto matches = services::NewsMonitorService::instance().scan_monitors(monitors, filtered_articles_);
    side_panel_->update_monitors(monitors, matches);
    feed_panel_->model()->set_monitor_matches(matches, monitors);

    // Update intel strip monitor summary
    int total_alerts = 0;
    for (auto it = matches.begin(); it != matches.end(); ++it)
        total_alerts += it.value().size();
    command_bar_->update_monitor_summary(monitors.size(), total_alerts);

    // Notifications
    auto& repo = fincept::SettingsRepository::instance();
    auto get_bool = [&](const QString& key, bool def) -> bool {
        auto r = repo.get(key);
        return r.is_ok() && !r.value().isEmpty() ? (r.value() == "1") : def;
    };
    if (get_bool("notifications.news_monitors", true)) {
        using namespace fincept::notifications;
        const int64_t notify_now = QDateTime::currentSecsSinceEpoch();
        for (const auto& monitor : monitors) {
            if (!monitor.enabled)
                continue;
            const auto& articles = matches.value(monitor.id);
            for (const auto& article : articles) {
                if (notify_now - article.sort_ts > kNewsNotifyMaxAgeSec)
                    continue; // stale: already in the feed when we started
                const QString dedup_key = monitor.id + ":" + article.id;
                if (notified_monitors_.contains(dedup_key))
                    continue;
                if (notified_monitors_.size() > kNewsNotifiedSetCap)
                    notified_monitors_.clear();
                notified_monitors_.insert(dedup_key);

                NotificationRequest req;
                req.title = QString("WATCH \"%1\": %2").arg(monitor.label, article.headline.left(70));
                req.message = article.summary.isEmpty() ? article.source : article.summary.left(160);
                req.level = NotifLevel::Warning;
                req.trigger = NotifTrigger::NewsAlert;
                NotificationService::instance().send(req);
            }
        }
    }
}

void NewsScreen::compute_deviations() {
    int64_t now = QDateTime::currentSecsSinceEpoch();
    int64_t hour_ago = now - 3600;

    // Last-hour volume per category across the WHOLE feed. This used to count
    // the UI-filtered list, so picking a category/time range/search changed the
    // "normal" level the spike was judged against.
    QMap<QString, int> current_counts;
    for (const auto& a : std::as_const(all_articles_)) {
        if (a.sort_ts >= hour_ago)
            current_counts[a.category]++;
    }

    // The history is HOURLY (168 = 7 days, 24 needed before judging), but this
    // function runs on every filter change and every progressive partial — it
    // used to append a sample each time, so "24 hours of history" was reached
    // within one refresh and spikes were scored against half-loaded lists
    // (spurious "DEVIATION" notifications). Judge against the history as it
    // stands, and add a new sample at most once an hour from a complete list.
    QVector<QPair<QString, double>> deviations;

    for (auto it = current_counts.begin(); it != current_counts.end(); ++it) {
        if (loading_) // partial list — counts are not comparable to the baseline
            break;
        auto found = baselines_.find(it.key());
        if (found == baselines_.end() || found->hourly_counts.size() < 24)
            continue;
        auto& baseline = *found;

        double sum = 0;
        for (int c : std::as_const(baseline.hourly_counts))
            sum += c;
        baseline.mean_count = sum / baseline.hourly_counts.size();

        double var_sum = 0;
        for (int c : std::as_const(baseline.hourly_counts)) {
            double diff = c - baseline.mean_count;
            var_sum += diff * diff;
        }
        baseline.stddev = std::sqrt(var_sum / baseline.hourly_counts.size());

        if (baseline.stddev < 0.5)
            continue;

        double z_score = (it.value() - baseline.mean_count) / baseline.stddev;
        if (z_score >= 3.0)
            deviations.append({it.key(), z_score});
    }

    const bool take_sample = !loading_ && !all_articles_.isEmpty() && (now - last_baseline_sample_ts_ >= 3600);
    if (take_sample) {
        last_baseline_sample_ts_ = now;
        QSet<QString> categories;
        for (auto it = current_counts.cbegin(); it != current_counts.cend(); ++it)
            categories.insert(it.key());
        for (auto it = baselines_.cbegin(); it != baselines_.cend(); ++it)
            categories.insert(it.key());
        for (const auto& category : std::as_const(categories)) {
            auto& baseline = baselines_[category];
            baseline.hourly_counts.append(current_counts.value(category, 0)); // quiet hours count as 0
            while (baseline.hourly_counts.size() > 168)
                baseline.hourly_counts.removeFirst();
        }
    }

    std::sort(deviations.begin(), deviations.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

    side_panel_->update_deviations(deviations);
    command_bar_->update_deviations(deviations); // intel strip

    // Notifications
    auto& repo = fincept::SettingsRepository::instance();
    auto get_bool = [&](const QString& key, bool def) -> bool {
        auto r = repo.get(key);
        return r.is_ok() && !r.value().isEmpty() ? (r.value() == "1") : def;
    };

    // Re-arm categories that have calmed down, so a later spike notifies again
    // (the dedup set used to hold a category for the rest of the session).
    for (auto it = notified_deviations_.begin(); it != notified_deviations_.end();) {
        const QString cat = *it;
        const bool still_spiking = std::any_of(deviations.cbegin(), deviations.cend(),
                                               [&cat](const auto& d) { return d.first == cat; });
        it = still_spiking ? std::next(it) : notified_deviations_.erase(it);
    }

    if (get_bool("notifications.news_deviations", true)) {
        using namespace fincept::notifications;
        for (const auto& [category, z_score] : deviations) {
            if (notified_deviations_.contains(category))
                continue;
            notified_deviations_.insert(category);

            NotificationRequest req;
            req.title = QString("DEVIATION: %1 news spike").arg(category);
            req.message = QString("Unusual volume detected (z-score: %1). More %2 articles than normal.")
                              .arg(z_score, 0, 'f', 1)
                              .arg(category);
            req.level = z_score >= 5.0 ? NotifLevel::Critical : NotifLevel::Warning;
            req.trigger = NotifTrigger::NewsAlert;
            NotificationService::instance().send(req);
        }
    }

    // FLASH articles
    if (get_bool("notifications.news_flash", true)) {
        using namespace fincept::notifications;
        for (const auto& article : filtered_articles_) {
            if (article.priority != services::Priority::FLASH)
                continue;
            if (article.impact != services::Impact::HIGH)
                continue;
            // Only fresh items: every FLASH item of the last day used to fire a
            // Critical notification at launch.
            if (now - article.sort_ts > kNewsNotifyMaxAgeSec)
                continue;
            if (notified_flash_.contains(article.id))
                continue;
            if (notified_flash_.size() > kNewsNotifiedSetCap)
                notified_flash_.clear();
            notified_flash_.insert(article.id);

            NotificationRequest req;
            req.title = QString("FLASH: %1").arg(article.headline.left(80));
            req.message = article.summary.isEmpty() ? article.source : article.summary.left(160);
            req.level = NotifLevel::Critical;
            req.trigger = NotifTrigger::NewsAlert;
            NotificationService::instance().send(req);
        }
    }

    // Persist baselines — once per hourly sample. This used to spawn a Python
    // process on every UI update (dozens per refresh) and discard the result.
    if (take_sample) {
        services::NewsCorrelationService::instance().update_baseline(
            current_counts, [](bool /*ok*/, QMap<QString, services::CategoryBaseline> /*baselines*/) {});
    }
}

void NewsScreen::sort_articles(QVector<services::NewsArticle>& articles) const {
    if (sort_mode_ == "NEWEST") {
        std::sort(articles.begin(), articles.end(), [](const auto& a, const auto& b) { return a.sort_ts > b.sort_ts; });
    } else {
        std::sort(articles.begin(), articles.end(), [](const auto& a, const auto& b) {
            if (a.priority != b.priority)
                return static_cast<int>(a.priority) < static_cast<int>(b.priority);
            return a.sort_ts > b.sort_ts;
        });
    }
}

int64_t NewsScreen::time_window_seconds() const {
    if (time_range_ == "1H")
        return 3600;
    if (time_range_ == "6H")
        return 21600;
    if (time_range_ == "24H")
        return 86400;
    if (time_range_ == "48H")
        return 172800;
    if (time_range_ == "7D")
        return 604800;
    return 86400;
}

// ── IStatefulScreen ─────────────────────────────────────────────────────────

QVariantMap NewsScreen::save_state() const {
    // search_query_ is intentionally NOT persisted — it's a transient
    // typed-in filter, and restoring it would silently filter the feed
    // on next launch (the search input widget doesn't display restored
    // queries, so users can't see what's hiding articles).
    return {
        {"category", active_category_}, {"time_range", time_range_},  {"sort_mode", sort_mode_},
        {"view_mode", view_mode_},      {"variant", active_variant_}, {"language", active_lang_},
    };
}

void NewsScreen::restore_state(const QVariantMap& state) {
    active_category_ = state.value("category", "ALL").toString();
    time_range_ = state.value("time_range", "24H").toString();
    sort_mode_ = state.value("sort_mode", "RELEVANCE").toString();
    view_mode_ = state.value("view_mode", "WIRE").toString();
    // Drop any legacy "search_query" stored by an older build — see save_state().
    search_query_.clear();
    active_variant_ = state.value("variant", active_variant_).toString();
    active_lang_ = state.value("language", active_lang_).toString();

    if (command_bar_) {
        command_bar_->set_active_category(active_category_);
        command_bar_->set_active_time_range(time_range_);
        command_bar_->set_active_sort(sort_mode_);
        command_bar_->set_active_view(view_mode_);
        // The variant/language filters apply to the feed, so the combos must show
        // them — restoring only the member left a hidden filter behind.
        command_bar_->set_active_variant(active_variant_);
        command_bar_->set_active_language(active_lang_);
    }
}

} // namespace fincept::screens
