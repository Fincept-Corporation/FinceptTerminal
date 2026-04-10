#include "screens/news/NewsScreen.h"

#include "core/keys/KeyConfigManager.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/news/NewsCommandBar.h"
#include "screens/news/NewsDetailPanel.h"
#include "screens/news/NewsFeedPanel.h"
#include "screens/news/NewsSidePanel.h"
#include "screens/news/NewsTickerStrip.h"
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
    settings.endGroup();

    build_ui();
    connect_signals();
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
        auto r = fincept::NewsArticleRepository::instance().toggle_saved(article.id);
        if (r.is_ok()) {
            auto saved_r = fincept::NewsArticleRepository::instance().load_saved();
            if (saved_r.is_ok())
                side_panel_->update_saved(saved_r.value());
        }
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
        Q_UNUSED(lang);
        apply_filters_async();
    });

    // Pulse animation timer (500ms cycle for new item glow)
    pulse_timer_ = new QTimer(this);
    pulse_timer_->setInterval(500);
    connect(pulse_timer_, &QTimer::timeout, this, [this]() {
        if (visible_)
            feed_panel_->model()->advance_pulse();
    });

    // Debounced DB seen-write timer
    seen_flush_timer_ = new QTimer(this);
    seen_flush_timer_->setInterval(1000);
    seen_flush_timer_->setSingleShot(true);
    connect(seen_flush_timer_, &QTimer::timeout, this, [this]() {
        if (pending_seen_ids_.isEmpty())
            return;
        const QSet<QString> ids = std::move(pending_seen_ids_);
        pending_seen_ids_.clear();
        QPointer<NewsScreen> self = this;
        QtConcurrent::run([ids, self]() {
            for (const auto& id : ids)
                fincept::NewsArticleRepository::instance().mark_seen(id);
            if (self) {
                QMetaObject::invokeMethod(
                    self,
                    [self]() {
                        if (self)
                            LOG_INFO("NewsScreen", QString("Flushed %1 seen IDs to DB").arg(0));
                    },
                    Qt::QueuedConnection);
            }
        });
    });

    // Scroll-based seen tracking
    connect(feed_panel_->list_view()->verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        auto* lv = feed_panel_->list_view();
        const QRect viewport_rect = lv->viewport()->rect();
        for (int i = 0; i < feed_panel_->model()->rowCount(); ++i) {
            const auto idx = feed_panel_->model()->index(i, 0);
            if (lv->visualRect(idx).intersects(viewport_rect)) {
                const QString id = feed_panel_->model()->article_at(i).id;
                feed_panel_->model()->mark_seen(id);
                pending_seen_ids_.insert(id);
            }
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
    act_next->setParent(this);
    connect(act_next, &QAction::triggered, this, [this]() { feed_panel_->select_next(); });

    auto* act_prev = km.action(KeyAction::NewsPrev);
    act_prev->setParent(this);
    connect(act_prev, &QAction::triggered, this, [this]() { feed_panel_->select_previous(); });

    auto* act_open = km.action(KeyAction::NewsOpen);
    act_open->setParent(this);
    connect(act_open, &QAction::triggered, this, [this]() {
        auto idx = feed_panel_->list_view()->currentIndex();
        if (idx.isValid()) {
            auto article = feed_panel_->model()->article_at(idx.row());
            if (!article.link.isEmpty())
                QDesktopServices::openUrl(QUrl(article.link));
        }
    });

    auto* act_close = km.action(KeyAction::NewsClose);
    act_close->setParent(this);
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
    services::NewsService::instance().start_auto_refresh();
    services::NewsService::instance().connect_live_feed();

    if (first_show_) {
        first_show_ = false;

        fincept::NewsArticleRepository::instance().ensure_seen_column();
        fincept::NewsArticleRepository::instance().ensure_saved_column();

        int64_t since_ts = QDateTime::currentSecsSinceEpoch() - (30LL * 86400);
        auto seen_result = fincept::NewsArticleRepository::instance().load_seen_ids(since_ts);
        if (seen_result.is_ok()) {
            for (const auto& id : seen_result.value())
                feed_panel_->model()->mark_seen(id);
        }

        auto saved_result = fincept::NewsArticleRepository::instance().load_saved();
        if (saved_result.is_ok())
            side_panel_->update_saved(saved_result.value());

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
    services::NewsService::instance().stop_auto_refresh();
    services::NewsService::instance().disconnect_live_feed();
    filter_generation_.fetch_add(1, std::memory_order_relaxed);
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

void NewsScreen::on_refresh() {
    refresh_data(true);
}

void NewsScreen::on_article_clicked(const services::NewsArticle& article) {
    detail_panel_->show_article(article);
    feed_panel_->set_selected(article.id);

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
    services::NewsService::instance().analyze_article(url, [self](bool ok, services::NewsAnalysis analysis) {
        if (!self)
            return;
        if (ok)
            self->detail_panel_->show_analysis(analysis);
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
    loading_ = true;
    command_bar_->set_loading(true);
    feed_panel_->set_loading(true);

    auto* svc = &services::NewsService::instance();

    QPointer<NewsScreen> self = this;
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(svc, &services::NewsService::articles_partial, this,
                    [self, conn](QVector<services::NewsArticle> articles, int done, int total) {
                        if (!self)
                            return;
                        self->all_articles_ = std::move(articles);
                        self->command_bar_->set_loading_progress(done, total);
                        self->apply_filters_async();
                        if (done == total)
                            QObject::disconnect(*conn);
                    });

    svc->fetch_all_news_progressive(force, [self](bool ok, QVector<services::NewsArticle> articles) {
        if (!self)
            return;
        self->loading_ = false;
        self->command_bar_->set_loading(false);
        self->feed_panel_->set_loading(false);
        if (!ok) {
            LOG_ERROR("NewsScreen", "Failed to fetch news");
            return;
        }
        self->all_articles_ = std::move(articles);
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
    int visible_count = visible_article_count_;

    // For 7D / 30D ranges, merge DB history
    if (time_range == "7D" || time_range == "30D") {
        const int64_t window_sec = (time_range == "30D") ? (30LL * 86400) : (7LL * 86400);
        const int64_t cutoff = QDateTime::currentSecsSinceEpoch() - window_sec;

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

    QtConcurrent::run([self, gen, articles_copy = std::move(articles_copy), category, time_range, search_lower, sort,
                       variant, visible_count]() {
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

        for (const auto& a : articles_copy) {
            if (cutoff > 0 && a.sort_ts < cutoff)
                continue;

            // Variant filter
            if (variant == "FINANCE" && a.category != "MARKETS" && a.category != "EARNINGS" &&
                a.category != "ECONOMIC" && a.category != "REGULATORY")
                continue;
            if (variant == "CRYPTO" && a.category != "CRYPTO" && a.category != "TECH")
                continue;
            if (variant == "MACRO" && a.category != "ECONOMIC" && a.category != "REGULATORY" &&
                a.category != "GEOPOLITICS" && a.category != "ENERGY")
                continue;

            // Category filter
            if (category != "ALL") {
                static const QHash<QString, QString> cat_map = {
                    {"MKT", "MARKETS"}, {"EARN", "EARNINGS"}, {"ECO", "ECONOMIC"},    {"TECH", "TECH"},
                    {"NRG", "ENERGY"},  {"CRPT", "CRYPTO"},   {"GEO", "GEOPOLITICS"}, {"DEF", "DEFENSE"},
                };
                auto it = cat_map.find(category);
                if (it != cat_map.end() && a.category != it.value())
                    continue;
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
                if (!match)
                    continue;
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
            for (const auto& cluster : breaking) {
                const QString& lead_id = cluster.lead_article.id;
                if (notified_breaking_.contains(lead_id))
                    continue;
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
        for (const auto& monitor : monitors) {
            if (!monitor.enabled)
                continue;
            const auto& articles = matches.value(monitor.id);
            for (const auto& article : articles) {
                const QString dedup_key = monitor.id + ":" + article.id;
                if (notified_monitors_.contains(dedup_key))
                    continue;
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

    QMap<QString, int> current_counts;
    for (const auto& a : filtered_articles_) {
        if (a.sort_ts >= hour_ago)
            current_counts[a.category]++;
    }

    QVector<QPair<QString, double>> deviations;

    for (auto it = current_counts.begin(); it != current_counts.end(); ++it) {
        auto& baseline = baselines_[it.key()];
        baseline.hourly_counts.append(it.value());

        while (baseline.hourly_counts.size() > 168)
            baseline.hourly_counts.removeFirst();

        if (baseline.hourly_counts.size() < 24)
            continue;

        double sum = 0;
        for (int c : baseline.hourly_counts)
            sum += c;
        baseline.mean_count = sum / baseline.hourly_counts.size();

        double var_sum = 0;
        for (int c : baseline.hourly_counts) {
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

    std::sort(deviations.begin(), deviations.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

    side_panel_->update_deviations(deviations);
    command_bar_->update_deviations(deviations); // intel strip

    // Notifications
    auto& repo = fincept::SettingsRepository::instance();
    auto get_bool = [&](const QString& key, bool def) -> bool {
        auto r = repo.get(key);
        return r.is_ok() && !r.value().isEmpty() ? (r.value() == "1") : def;
    };

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
            if (notified_flash_.contains(article.id))
                continue;
            notified_flash_.insert(article.id);

            NotificationRequest req;
            req.title = QString("FLASH: %1").arg(article.headline.left(80));
            req.message = article.summary.isEmpty() ? article.source : article.summary.left(160);
            req.level = NotifLevel::Critical;
            req.trigger = NotifTrigger::NewsAlert;
            NotificationService::instance().send(req);
        }
    }

    // Persist baselines
    services::NewsCorrelationService::instance().update_baseline(
        current_counts, [](bool /*ok*/, QMap<QString, services::CategoryBaseline> /*baselines*/) {});
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
    return {
        {"category", active_category_}, {"time_range", time_range_},     {"sort_mode", sort_mode_},
        {"view_mode", view_mode_},      {"search_query", search_query_}, {"variant", active_variant_},
    };
}

void NewsScreen::restore_state(const QVariantMap& state) {
    active_category_ = state.value("category", "ALL").toString();
    time_range_ = state.value("time_range", "24H").toString();
    sort_mode_ = state.value("sort_mode", "RELEVANCE").toString();
    view_mode_ = state.value("view_mode", "WIRE").toString();
    search_query_ = state.value("search_query").toString();
    active_variant_ = state.value("variant", "FULL").toString();

    if (command_bar_)
        command_bar_->set_active_category(active_category_);
}

} // namespace fincept::screens
