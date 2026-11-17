#pragma once
#include "core/events/EventBus.h"
#include "core/symbol/IGroupLinked.h"
#include "screens/common/ISectionLinked.h"
#include "screens/common/IStatefulScreen.h"
#include "services/news/NewsAnalytics.h"
#include "services/news/NewsClusterService.h"
#include "services/news/NewsMonitorService.h"
#include "services/news/NewsService.h"
#include "storage/repositories/NewsAnalyticsRepository.h"

#include <QHBoxLayout>
#include <QHash>
#include <QHideEvent>
#include <QList>
#include <QSet>
#include <QShowEvent>
#include <QWidget>

#include <atomic>
#include <memory>

class QSplitter;

namespace fincept::screens {

class NewsCommandBar;
class NewsSidePanel;
class NewsFeedPanel;
class NewsDetailPanel;
class NewsTickerStrip;
class NewsAnalyticsPanel;
struct NewsFilterResult; // filter-worker output, defined in NewsScreen.cpp

/// Main news screen orchestrator.
///
/// Layout (top to bottom):
///   1. Command bar — filter pills (scrolling) + controls (always reachable, ⋯ menu)
///   2. Intel strip — live stats, tone gauge, feed health, monitors, deviations
///   3. Content — a splitter: [intel drawer] [analytics over feed] [article detail];
///      every pane is resizable and the sizes persist
///   4. Ticker strip — scrolling breaking headlines
class NewsScreen : public QWidget, public IStatefulScreen, public IGroupLinked, public fincept::screens::ISectionLinked {
    Q_OBJECT
    Q_INTERFACES(fincept::IGroupLinked)
  public:
    explicit NewsScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "news"; }
    int state_version() const override { return 1; }

    // ISectionLinked — the views: "wire", "clusters" ("stories" too), as the WIRE / CLST pills.
    // `focus` = a saved search (its id, "search:<id>", or its name): applied as the SAVED menu's
    // Apply does, before the view named.
    bool open_section(const QString& section, const QString& focus) override;

    // IGroupLinked — subscribe-only, and a follower that never hides the feed on
    // its own (the convention of Markets / Watchlist / Screener, which highlight
    // a linked symbol rather than replace their content):
    //  * another panel of the group changing its symbol only OFFERS it (context
    //    bar: "Group A is on AAPL — SHOW ITS NEWS"); with FOLLOW LINK on (the
    //    user's opt-in, a preference) the feed narrows to stories TAGGED with it,
    //    and while nothing is tagged it keeps showing everything, saying so;
    //  * a direct hand-off ("Open in News", ui_open, a ticker dropped on the
    //    screen, linking this tab to a group from its badge) narrows the feed at
    //    once — a removable context chip, never the search box; a typed search
    //    always wins over either.
    // The symbol filter is never saved with the screen state.
    void set_group(SymbolGroup g) override;
    SymbolGroup group() const override { return link_group_; }
    void on_group_symbol_changed(const SymbolRef& ref) override;
    SymbolRef current_symbol() const override { return {}; }

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    void on_category_changed(const QString& category);
    void on_time_range_changed(const QString& range);
    void on_sort_changed(const QString& sort);
    void on_view_mode_changed(const QString& mode);
    void on_search_changed(const QString& query);
    void on_refresh();

    void on_article_clicked(const services::NewsArticle& article);
    void on_cluster_clicked(const services::NewsCluster& cluster);
    void on_near_bottom();

    void on_sidebar_category_clicked(const QString& category);
    void on_sidebar_article_clicked(const services::NewsArticle& article);
    void on_monitor_added(const QString& label, const QStringList& keywords);
    void on_monitor_toggled(const QString& id);
    void on_monitor_deleted(const QString& id);

    void on_analyze_requested(const QString& url);
    void on_related_clicked(const services::NewsArticle& article);

    void on_drawer_toggle();
    /// Open / close the intel drawer and keep the INTEL button in sync.
    void set_drawer_open(bool open);
    void on_detail_closed();
    void on_manage_sources();

  private:
    void build_ui();
    void connect_signals();
    /// Root stylesheet: the shared news sheet + this screen's state rules
    /// (priority stripes, signal / odds colours, live badge). Re-run on theme change.
    void apply_news_styles();

    void refresh_data(bool force);
    void apply_filters_async();
    /// Coalesced apply_filters_async() for bursts (feeds arriving during a
    /// fetch): at most one filter pass per kFilterCoalesceMs.
    void schedule_filters();
    /// Move the current network list into stored_articles_ (deduplicated,
    /// trimmed to kStoredWindowSec) before a new list replaces it, so a refresh
    /// never makes stories vanish and then reappear as feeds answer.
    void fold_into_stored();
    bool has_any_articles() const { return !all_articles_.isEmpty() || !stored_articles_.isEmpty(); }
    void update_ui_from_filtered(const std::shared_ptr<const NewsFilterResult>& result);
    void update_monitors();
    /// Volume spikes per category: the last hour against the persisted hourly
    /// history (news_volume_hourly, 7 days). Needs 24 observed hours.
    void compute_deviations();
    /// Reload the volume baselines from the database (worker) at most every
    /// kBaselineReloadSec; compute_deviations() runs again when they arrive.
    void refresh_volume_baseline(bool force = false);
    /// Narrow the feed to stories tagged with `symbol` (the context chip).
    /// `from_link`: applied by following the link group — then the whole feed
    /// stays listed while nothing is tagged with it. An explicit one (ticker
    /// click, drop, "Open in News") replaces a typed search.
    void set_symbol_context(const QString& symbol, bool from_link);
    void clear_symbol_context();
    /// A symbol the link group broadcast (another panel changed its symbol).
    void on_link_symbol(const QString& symbol);
    void set_follow_link(bool on);
    /// The feed panel's context bar from the symbol / link state.
    void update_context_bar();
    /// Re-read the symbols held across all portfolios (worker thread) so the intel
    /// drawer can flag holdings that are in the news.
    void refresh_holdings();
    /// Every filter back to its default (the empty state's CLEAR FILTERS).
    void clear_all_filters();
    /// Mark every listed article read (model + database).
    void mark_all_read();
    /// Show / hide the analytics panel.
    void set_charts_open(bool open);
    /// Feed health: badge text + the per-feed popup.
    void update_feed_health_badge();
    void show_feed_health();

    /// Enrich the visible articles (NER + places, correlation signals) and route the
    /// results into the intel drawer + feed model. Debounced via `enrichment_timer_`.
    void request_enrichment();
    void run_enrichment_now();
    /// Prediction-market odds feed only the intel drawer: fetched (from a 10-min
    /// cache) only while the drawer is open, for the feed's top topics.
    void maybe_fetch_predictions();

    /// market:info subscriptions (earnings date, sector) for the most mentioned
    /// tickers of the current filter, while the screen is visible.
    void subscribe_ticker_info(const QStringList& symbols);

    // Saved searches & alerts
    void load_saved_searches();
    void show_saved_menu();
    void save_current_search();
    void apply_saved_search(const fincept::NewsSavedSearchRow& row);
    void delete_saved_search(const QString& id);
    /// Fire saved-search alerts (ticker / tone threshold / volume spike) for fresh
    /// articles, honouring each alert's quiet hours.
    void check_saved_alerts(const QVector<QPair<QString, double>>& deviations);
    QVariantMap current_filter_state() const;
    void apply_filter_state(const QVariantMap& state);

    // Panels
    NewsCommandBar* command_bar_ = nullptr;
    NewsSidePanel* side_panel_ = nullptr;
    NewsFeedPanel* feed_panel_ = nullptr;
    NewsDetailPanel* detail_panel_ = nullptr;
    NewsTickerStrip* ticker_strip_ = nullptr;
    NewsAnalyticsPanel* analytics_panel_ = nullptr;
    QSplitter* content_splitter_ = nullptr;
    QSplitter* center_splitter_ = nullptr;

    // State
    /// The current network list (latest fetch / partial snapshot), newest first.
    QVector<services::NewsArticle> all_articles_;
    /// Articles known from before: the local database (loaded on first show,
    /// so the screen has content at once) and earlier fetches of this session.
    /// The filter pass shows all_articles_ ∪ stored_articles_ (network wins).
    QVector<services::NewsArticle> stored_articles_;
    static constexpr qint64 kStoredWindowSec = 48 * 3600;
    QTimer* filter_coalesce_timer_ = nullptr;
    static constexpr int kFilterCoalesceMs = 200;
    /// This session switched the sort to NEWEST (one-time migration): a
    /// workspace state saved by an older build must not switch it back.
    bool sort_migrated_ = false;
    /// Progress of the running fetch (loading page).
    int fetch_done_ = 0;
    int fetch_total_ = 0;
    QVector<services::NewsArticle> filtered_articles_; // after every filter, duplicates included
    QVector<services::NewsArticle> wire_articles_;     // filtered_articles_ minus collapsed duplicates
    QVector<services::NewsCluster> clusters_;
    QString active_category_ = "ALL";
    QString time_range_ = "24H";
    QString sort_mode_ = "NEWEST"; // publication time, newest first (RELEVANCE on request)
    QString view_mode_ = "WIRE";
    /// Typed text only (substring match).
    QString search_query_;
    /// Symbol the feed is narrowed to (upper case; empty = none). Matched as a
    /// ticker — validated tags, or the ticker as written ("AAPL", "$AAPL") —
    /// never as a substring. Not persisted.
    QString symbol_context_;
    /// symbol_context_ was applied by following the link group (soft: nothing
    /// tagged -> the whole feed, see symbol_fallback_).
    bool symbol_from_link_ = false;
    /// The last filter pass found nothing tagged with a followed symbol and
    /// listed the whole feed instead.
    bool symbol_fallback_ = false;
    /// The link group's latest symbol (offered on the context bar) and whether
    /// the user dismissed that offer.
    QString linked_symbol_;
    bool link_offer_dismissed_ = false;
    /// FOLLOW LINK: the user's opt-in to narrow the feed to the link group's
    /// symbol (QSettings news/follow_link).
    bool follow_link_ = false;
    /// Telling a link-group broadcast from a direct hand-off: both arrive through
    /// on_group_symbol_changed(); a broadcast is the one SymbolContext emits at
    /// the same time (marked by our own connection, for one event-loop pass).
    QString link_broadcast_marker_;
    QString pending_group_symbol_;
    /// Symbols held in any portfolio (sorted) — matched against the feed for the
    /// "holdings in the news" list.
    QStringList holding_symbols_;
    QString active_region_ = "ALL";
    QString my_filter_ = "ALL";
    bool unread_only_ = false;
    bool charts_open_ = false;
    /// Time window picked on the analytics timeline (unix seconds); 0 = none.
    qint64 window_from_ = 0;
    qint64 window_to_ = 0;
    bool loading_ = false;
    bool visible_ = false;
    bool first_show_ = true;
    /// The last refresh ended with every feed failing (error state, not "empty").
    bool last_fetch_failed_ = false;

    // Generation ID for stale async result rejection
    std::atomic<int> filter_generation_{0};

    // Lazy loading
    int visible_article_count_ = 50;
    static constexpr int PAGE_SIZE = 50;

    // Volume baselines (persisted hourly history, see compute_deviations()).
    QHash<QString, services::news_analytics::Baseline> baselines_;
    int baseline_observed_hours_ = 0;
    qint64 baseline_loaded_at_ = 0;
    bool baseline_loading_ = false;
    static constexpr qint64 kBaselineReloadSec = 600;
    static constexpr int kBaselineMinHours = 24;
    QVector<QPair<QString, double>> last_deviations_;

    // Earnings / sector of mentioned tickers (market:info).
    QHash<QString, int> earnings_days_;   // symbol -> days until the report (0..kEarningsHorizonDays)
    QHash<QString, QString> sectors_;     // symbol -> sector
    QSet<QString> info_subscribed_;
    static constexpr int kEarningsHorizonDays = 14;

    // Saved searches
    QVector<fincept::NewsSavedSearchRow> saved_searches_;
    QSet<QString> notified_alerts_;

    // Notification dedup
    QSet<QString> notified_breaking_;
    QSet<QString> notified_monitors_;
    QSet<QString> notified_deviations_;
    QSet<QString> notified_flash_;

    // Pulse animation timer
    QTimer* pulse_timer_ = nullptr;

    // Debounced DB seen-writes
    QTimer* seen_flush_timer_ = nullptr;
    QSet<QString> pending_seen_ids_;

    // Debounce timer for NER + geo + correlation enrichment calls.
    // Single-shot, fired by request_enrichment(); the timeout slot runs
    // run_enrichment_now() with the current filtered_articles_ snapshot.
    QTimer* enrichment_timer_ = nullptr;
    // Monotonic counter so late async results from earlier filter
    // generations don't clobber the intel sections.
    std::atomic<int> enrichment_generation_{0};
    // Last enriched article set (hash of ids) and when — an unchanged set is not
    // re-enriched within kEnrichMinIntervalSec (signals only move with the clock).
    size_t last_enrich_signature_ = 0;
    int64_t last_enrich_at_ = 0;
    static constexpr int64_t kEnrichMinIntervalSec = 60;
    /// Topics (entities / tickers) the predictions are matched to.
    QStringList prediction_terms_;

    // Active variant for feed filtering
    QString active_variant_ = "FULL";

    // Active language filter (ISO code, lowercased) or "ALL".
    QString active_lang_ = "ALL";

    // Symbol group link — SymbolGroup::None when unlinked.
    SymbolGroup link_group_ = SymbolGroup::None;

    // EventBus subscriptions — registered in showEvent, released in hideEvent.
    // MCP news tools publish news.monitor_added / news.monitor_toggled /
    // news.monitor_deleted / news.refresh_requested when the LLM mutates
    // monitor state via Finagent or AI Chat. Handlers may fire on a worker
    // thread; each callback marshals to this widget's thread before
    // touching UI state.
    QList<EventBus::HandlerId> mcp_event_subs_;
    void subscribe_mcp_events();
    void unsubscribe_mcp_events();
};

} // namespace fincept::screens
