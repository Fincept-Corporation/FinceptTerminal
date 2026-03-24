#include "screens/news/NewsScreen.h"

#include "core/logging/Logger.h"
#include "screens/news/NewsCommandBar.h"
#include "screens/news/NewsDetailPanel.h"
#include "screens/news/NewsFeedPanel.h"
#include "screens/news/NewsSidePanel.h"
#include "screens/news/NewsTickerStrip.h"
#include "services/news/NewsCorrelationService.h"
#include "services/news/NewsNlpService.h"
#include "ui/theme/StyleSheets.h"
#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QPointer>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QSplitter>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

NewsScreen::NewsScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("newsScreen");
    setStyleSheet(ui::styles::news_screen_styles());

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

    // Command bar (32px)
    command_bar_ = new NewsCommandBar(this);
    root->addWidget(command_bar_);

    // Three-panel splitter
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(1);
    splitter->setObjectName("newsSplitter");

    side_panel_ = new NewsSidePanel(splitter);
    feed_panel_ = new NewsFeedPanel(splitter);
    detail_panel_ = new NewsDetailPanel(splitter);

    splitter->addWidget(side_panel_);
    splitter->addWidget(feed_panel_);
    splitter->addWidget(detail_panel_);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);

    // Restore saved splitter sizes (Fix #9)
    QSettings splitter_settings;
    splitter_settings.beginGroup("news");
    auto saved_sizes = splitter_settings.value("splitter_sizes").toByteArray();
    splitter_settings.endGroup();
    if (!saved_sizes.isEmpty())
        splitter->restoreState(saved_sizes);

    // Save splitter sizes on change
    connect(splitter, &QSplitter::splitterMoved, this, [splitter]() {
        QSettings s;
        s.beginGroup("news");
        s.setValue("splitter_sizes", splitter->saveState());
        s.endGroup();
    });

    root->addWidget(splitter, 1);

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

    // Feed panel
    connect(feed_panel_, &NewsFeedPanel::article_clicked, this, &NewsScreen::on_article_clicked);
    connect(feed_panel_, &NewsFeedPanel::cluster_clicked, this, &NewsScreen::on_cluster_clicked);
    connect(feed_panel_, &NewsFeedPanel::near_bottom, this, &NewsScreen::on_near_bottom);

    // Side panel
    connect(side_panel_, &NewsSidePanel::category_clicked, this, &NewsScreen::on_sidebar_category_clicked);
    connect(side_panel_, &NewsSidePanel::article_clicked, this, &NewsScreen::on_sidebar_article_clicked);
    connect(side_panel_, &NewsSidePanel::monitor_added, this, &NewsScreen::on_monitor_added);
    connect(side_panel_, &NewsSidePanel::monitor_toggled, this, &NewsScreen::on_monitor_toggled);
    connect(side_panel_, &NewsSidePanel::monitor_deleted, this, &NewsScreen::on_monitor_deleted);

    // Detail panel
    connect(detail_panel_, &NewsDetailPanel::analyze_requested, this, &NewsScreen::on_analyze_requested);

    // RTL toggle
    connect(command_bar_, &NewsCommandBar::rtl_toggled, this, []() { ui::set_rtl(!ui::is_rtl()); });

    // Variant selector — filter feeds by variant (Fix #11)
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
    // Started/stopped in showEvent/hideEvent (P3 compliance)

    // Scroll-based seen tracking
    connect(feed_panel_->list_view()->verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        // Mark visible articles as seen
        auto* lv = feed_panel_->list_view();
        for (int i = 0; i < feed_panel_->model()->rowCount(); ++i) {
            auto idx = feed_panel_->model()->index(i, 0);
            auto item_rect = lv->visualRect(idx);
            if (item_rect.intersects(lv->viewport()->rect())) {
                auto article = feed_panel_->model()->article_at(i);
                feed_panel_->model()->mark_seen(article.id);
            }
        }
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

    // Keyboard shortcuts
    auto* shortcut_j = new QShortcut(QKeySequence("J"), this);
    connect(shortcut_j, &QShortcut::activated, this, [this]() { feed_panel_->select_next(); });

    auto* shortcut_k = new QShortcut(QKeySequence("K"), this);
    connect(shortcut_k, &QShortcut::activated, this, [this]() { feed_panel_->select_previous(); });

    auto* shortcut_enter = new QShortcut(QKeySequence(Qt::Key_Return), this);
    connect(shortcut_enter, &QShortcut::activated, this, [this]() {
        auto idx = feed_panel_->list_view()->currentIndex();
        if (idx.isValid()) {
            auto article = feed_panel_->model()->article_at(idx.row());
            if (!article.link.isEmpty())
                QDesktopServices::openUrl(QUrl(article.link));
        }
    });
}

// ── Lifecycle ───────────────────────────────────────────────────────────────

void NewsScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    visible_ = true;
    ticker_strip_->resume();
    pulse_timer_->start();
    services::NewsService::instance().start_auto_refresh();
    services::NewsService::instance().connect_live_feed(); // Fix #10: WebSocket

    if (first_show_) {
        first_show_ = false;
        on_refresh();
    }
}

void NewsScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    visible_ = false;
    ticker_strip_->pause();
    pulse_timer_->stop();
    services::NewsService::instance().stop_auto_refresh();
    services::NewsService::instance().disconnect_live_feed(); // Fix #10
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
    apply_filters_async();
}

void NewsScreen::on_time_range_changed(const QString& range) {
    time_range_ = range;
    visible_article_count_ = PAGE_SIZE;
    QSettings s;
    s.beginGroup("news");
    s.setValue("time_range", range);
    s.endGroup();
    apply_filters_async();
}

void NewsScreen::on_sort_changed(const QString& sort) {
    sort_mode_ = sort;
    QSettings s;
    s.beginGroup("news");
    s.setValue("sort_mode", sort);
    s.endGroup();
    apply_filters_async();
}

void NewsScreen::on_view_mode_changed(const QString& mode) {
    view_mode_ = mode;
    QSettings s;
    s.beginGroup("news");
    s.setValue("view_mode", mode);
    s.endGroup();
    feed_panel_->model()->set_view_mode(mode);
}

void NewsScreen::on_search_changed(const QString& query) {
    search_query_ = query;
    visible_article_count_ = PAGE_SIZE;
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
    // Show lead article in detail panel
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
    LOG_DEBUG("NewsScreen", QString("Lazy-loaded to %1 articles").arg(visible.size()));
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

// ── Core data pipeline ──────────────────────────────────────────────────────

void NewsScreen::refresh_data(bool force) {
    loading_ = true;
    command_bar_->set_loading(true);
    feed_panel_->set_loading(true);

    auto* svc = &services::NewsService::instance();

    // Progressive callback: show articles as each feed arrives
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
    auto category = active_category_;
    auto time_range = time_range_;
    auto search = search_query_;
    auto sort = sort_mode_;
    auto variant = active_variant_;
    int visible_count = visible_article_count_;

    QtConcurrent::run([self, gen, articles_copy, category, time_range, search, sort, variant, visible_count]() {
        // Time filter
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

        int64_t cutoff = 0;
        if (window_sec > 0)
            cutoff = QDateTime::currentSecsSinceEpoch() - window_sec;

        QVector<services::NewsArticle> filtered;
        filtered.reserve(articles_copy.size());

        QMap<QString, int> category_counts;
        int bullish = 0, bearish = 0, neutral = 0;

        for (const auto& a : articles_copy) {
            // Time filter
            if (cutoff > 0 && a.sort_ts < cutoff)
                continue;

            // Variant filter (Fix #11)
            if (variant == "FINANCE" && a.category != "MARKETS" && a.category != "EARNINGS" &&
                a.category != "ECONOMIC" && a.category != "REGULATORY")
                continue;
            if (variant == "CRYPTO" && a.category != "CRYPTO" && a.category != "TECH")
                continue;
            if (variant == "MACRO" && a.category != "ECONOMIC" && a.category != "REGULATORY" &&
                a.category != "GEOPOLITICS" && a.category != "ENERGY")
                continue;
            // "FULL" shows everything

            // Category filter
            if (category != "ALL") {
                static const QMap<QString, QStringList> cat_map = {
                    {"MKT", {"MARKETS"}}, {"EARN", {"EARNINGS"}}, {"ECO", {"ECONOMIC"}},    {"TECH", {"TECH"}},
                    {"NRG", {"ENERGY"}},  {"CRPT", {"CRYPTO"}},   {"GEO", {"GEOPOLITICS"}}, {"DEF", {"DEFENSE"}},
                };
                auto it = cat_map.find(category);
                if (it != cat_map.end() && !it.value().contains(a.category))
                    continue;
            }

            // Search filter
            if (!search.isEmpty()) {
                bool match = a.headline.contains(search, Qt::CaseInsensitive) ||
                             a.summary.contains(search, Qt::CaseInsensitive) ||
                             a.source.contains(search, Qt::CaseInsensitive);
                for (const auto& t : a.tickers) {
                    if (t.contains(search, Qt::CaseInsensitive))
                        match = true;
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

        // Cluster
        auto clusters = services::cluster_articles(filtered);

        // Post back to UI thread
        QMetaObject::invokeMethod(
            self,
            [self, gen, filtered, clusters, category_counts, bullish, bearish, neutral]() {
                if (!self)
                    return;
                if (gen != self->filter_generation_.load(std::memory_order_relaxed)) {
                    LOG_DEBUG("NewsScreen", QString("Rejected stale filter gen %1").arg(gen));
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

    filtered_articles_ = filtered;
    clusters_ = clusters;

    // Update feed model
    auto visible = filtered.mid(0, visible_article_count_);
    feed_panel_->model()->set_wire_articles(visible);
    feed_panel_->model()->set_clusters(clusters);
    feed_panel_->model()->set_view_mode(view_mode_);

    // Command bar
    command_bar_->set_article_count(filtered.size());
    int alert_count = static_cast<int>(services::get_breaking_clusters(clusters).size());
    command_bar_->set_alert_count(alert_count);
    command_bar_->set_unseen_count(feed_panel_->model()->unseen_count());

    // Side panel stats
    side_panel_->update_stats(services::NewsService::instance().feed_count(), filtered.size(), clusters.size(),
                              services::NewsService::instance().active_sources().size());
    side_panel_->update_sentiment(bullish, bearish, neutral);

    // Top 5 stories
    QVector<services::NewsArticle> top5;
    for (int i = 0; i < std::min(5, static_cast<int>(filtered.size())); i++)
        top5.append(filtered[i]);
    side_panel_->update_top_stories(top5);
    side_panel_->update_categories(category_counts);

    // Breaking banner
    auto breaking = services::get_breaking_clusters(clusters);
    if (!breaking.isEmpty())
        feed_panel_->show_breaking(breaking);

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

    // ── NLP pipeline (async, results update UI when ready) ──────────────
    QPointer<NewsScreen> nlp_self = this;

    // Semantic clustering (Fix #13) — upgrade clusters with TF-IDF similarity
    services::NewsNlpService::instance().cluster_semantic(filtered, [nlp_self](bool ok, QJsonArray semantic_clusters) {
        if (!nlp_self || !ok || semantic_clusters.isEmpty())
            return;
        LOG_DEBUG("NewsScreen", QString("Semantic clustering found %1 groups").arg(semantic_clusters.size()));
    });

    // Entity extraction
    services::NewsNlpService::instance().extract_entities(filtered, [nlp_self](bool ok, services::NerResult ner) {
        if (!nlp_self || !ok)
            return;
        // Update side panel with top entities
        nlp_self->side_panel_->update_entities(ner);
    });

    // Geolocation + focal point detection (Fix #17)
    services::NewsNlpService::instance().geolocate_articles(
        filtered, [nlp_self](bool ok, QVector<services::ArticleGeo> geo) {
            if (!nlp_self || !ok)
                return;
            QSet<QString> geo_ids;
            for (const auto& g : geo)
                geo_ids.insert(g.id);
            nlp_self->feed_panel_->model()->set_geo_articles(geo_ids);
            nlp_self->side_panel_->update_locations(geo);

            // Focal point detection from geolocated articles
            QJsonArray geo_json;
            for (const auto& g : geo) {
                QJsonObject obj;
                obj["id"] = g.id;
                obj["primary_lat"] = g.primary_lat;
                obj["primary_lon"] = g.primary_lon;
                obj["category"] = "GENERAL";
                obj["source"] = "";
                obj["headline"] = "";
                // Find matching article for metadata
                for (const auto& a : nlp_self->filtered_articles_) {
                    if (a.id == g.id) {
                        obj["category"] = a.category;
                        obj["source"] = a.source;
                        obj["headline"] = a.headline;
                        break;
                    }
                }
                geo_json.append(obj);
            }
            if (geo_json.size() >= 3) {
                services::NewsCorrelationService::instance().detect_focal_points(
                    geo_json, [nlp_self](bool fp_ok, QVector<services::FocalPoint> points) {
                        if (!nlp_self || !fp_ok || points.isEmpty())
                            return;
                        LOG_INFO("NewsScreen", QString("Detected %1 focal points").arg(points.size()));
                        // Focal points shown as part of signals in side panel
                        // Convert to CorrelationSignal for display
                        QVector<services::CorrelationSignal> fp_sigs;
                        for (const auto& fp : points) {
                            services::CorrelationSignal sig;
                            sig.type = "geo_convergence";
                            sig.severity = fp.severity;
                            sig.value = fp.event_count;
                            sig.detail = QString("Focal: %1 events at (%2, %3)")
                                             .arg(fp.event_count)
                                             .arg(fp.lat, 0, 'f', 1)
                                             .arg(fp.lon, 0, 'f', 1);
                            fp_sigs.append(sig);
                        }
                        // Append to existing signals in side panel
                        auto existing = services::NewsCorrelationService::instance().cached_signals();
                        existing.append(fp_sigs);
                        nlp_self->side_panel_->update_signals(existing);
                    });
            }
        });

    // Correlation signals
    services::NewsCorrelationService::instance().detect_signals(
        filtered, [nlp_self](bool ok, QVector<services::CorrelationSignal> sigs) {
            if (!nlp_self || !ok)
                return;
            nlp_self->side_panel_->update_signals(sigs);

            // Compute CII for top mentioned countries
            auto ner = services::NewsNlpService::instance().cached_ner();
            for (int i = 0; i < std::min(5, static_cast<int>(ner.top_countries.size())); ++i) {
                QString code = ner.top_countries[i].name;
                services::NewsCorrelationService::instance().compute_instability(
                    code, sigs, [nlp_self, code](bool ok2, services::InstabilityScore score) {
                        if (!nlp_self || !ok2)
                            return;
                        nlp_self->side_panel_->update_instability(code, score);
                    });
            }
        });

    // Prediction markets (fetch once per refresh cycle)
    services::NewsCorrelationService::instance().fetch_predictions(
        [nlp_self](bool ok, QVector<services::PredictionMarket> preds) {
            if (!nlp_self || !ok)
                return;
            nlp_self->side_panel_->update_predictions(preds);
        });
}

void NewsScreen::update_monitors() {
    auto monitors = services::NewsMonitorService::instance().get_monitors();
    auto matches = services::NewsMonitorService::instance().scan_monitors(monitors, filtered_articles_);
    side_panel_->update_monitors(monitors, matches);
    feed_panel_->model()->set_monitor_matches(matches, monitors);
}

void NewsScreen::compute_deviations() {
    // Count articles per category in the last hour
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

        // Keep last 168 hours (7 days)
        while (baseline.hourly_counts.size() > 168)
            baseline.hourly_counts.removeFirst();

        if (baseline.hourly_counts.size() < 24)
            continue; // need at least 24 hours of data

        // Compute mean and stddev
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
            continue; // too little variance

        double z_score = (it.value() - baseline.mean_count) / baseline.stddev;
        if (z_score >= 3.0)
            deviations.append({it.key(), z_score});
    }

    // Sort by z-score descending
    std::sort(deviations.begin(), deviations.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

    side_panel_->update_deviations(deviations);

    // Persist baselines via correlation service (Fix #5)
    services::NewsCorrelationService::instance().update_baseline(
        current_counts, [](bool /*ok*/, QMap<QString, services::CategoryBaseline> /*baselines*/) {
            // Baselines saved to Python cache; no UI action needed
        });
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

} // namespace fincept::screens
