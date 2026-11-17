#pragma once
#include "screens/news/NewsFeedDelegate.h"
#include "screens/news/NewsFeedModel.h"
#include "services/news/NewsClusterService.h"
#include "services/news/NewsService.h"

#include <QEvent>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <functional>

namespace fincept::screens {

class NewsFeedPanel : public QWidget {
    Q_OBJECT
  public:
    explicit NewsFeedPanel(QWidget* parent = nullptr);

    NewsFeedModel* model() { return model_; }
    QListView* list_view() { return list_view_; }

    /// Breaking banner: up to kMaxBannerItems fresh breaking stories, newest
    /// first, with a pager (‹ 1/3 ›) to step through them.
    void show_breaking(const QVector<services::NewsCluster>& breaking_clusters);
    void clear_breaking();

    /// Loading page (a titled progress state over placeholder rows) instead of
    /// the list. Shown while nothing that passes the filters has arrived yet.
    void set_loading(bool loading);
    /// "N of M sources answered · K articles" on the loading page.
    void set_loading_progress(int sources_done, int sources_total, int articles);
    /// Articles have arrived but none pass the filters: the loading page says
    /// so and offers CLEAR FILTERS instead of looking like nothing is happening.
    void set_loading_filtered_out(bool filtered_out);
    /// The article list (leaves the loading / empty pages).
    void show_list();

    /// Why the list is empty — each case says something different:
    ///   NoArticles  feeds returned nothing (yet) — check sources / retry;
    ///   Filtered    articles exist but the filters exclude all of them;
    ///   Error       the fetch failed — `detail` lists the failing feeds;
    ///   Symbol      nothing is tagged with the symbol the feed is narrowed to
    ///               (`detail` = the symbol) — SHOW ALL NEWS / search headlines.
    enum class EmptyKind { NoArticles, Filtered, Error, Symbol };
    /// Show the empty state of `kind` (with an optional detail line) or, with
    /// `empty` false, the list again.
    void set_empty_state(bool empty, EmptyKind kind = EmptyKind::NoArticles, const QString& detail = {});

    /// The symbol context bar above the list, the loading page and the empty
    /// state, so a narrowed feed always says why and how to undo it:
    ///   Active — "NEWS FOR [AAPL]" (+ "following link group A", or "nothing
    ///            tagged yet — showing all news"), FOLLOW LINK, SHOW ALL NEWS;
    ///   Offer  — the link group's symbol, not applied: "Group A is on [AAPL]",
    ///            SHOW ITS NEWS, FOLLOW LINK, dismiss.
    struct ContextBar {
        enum class Mode { Hidden, Active, Offer };
        Mode mode = Mode::Hidden;
        QString symbol;
        QChar group;            ///< link group letter; null = not linked
        bool from_link = false; ///< Active: applied by following the link group
        bool follow = false;    ///< FOLLOW LINK is on
        bool fallback = false;  ///< Active + from_link: nothing tagged, the whole feed is shown
    };
    void set_context_bar(const ContextBar& state);
    /// Uniform row heights are a big win for the dense WIRE list but wrong for
    /// the variable-height cluster cards.
    void set_view_mode_hint(const QString& mode);
    void scroll_to(const QString& article_id);
    void set_selected(const QString& article_id);
    void select_next();
    void select_previous();

    /// Append the "act on this ticker" entries for `ticker` to `menu`: open it in
    /// Equity Research / Equity Trading (chart + order ticket) / Backtesting via the
    /// app-wide nav.open_symbol event, add it to a watchlist, filter the feed by it
    /// (`filter_cb`), copy it. Shared by the feed's context menu and the detail
    /// panel's ticker links. `host` scopes the actions' lifetime and anchors tooltips.
    static void add_ticker_actions(QMenu* menu, const QString& ticker, QWidget* host,
                                   const std::function<void(const QString&)>& filter_cb);

  signals:
    void article_clicked(const services::NewsArticle& article);
    void cluster_clicked(const services::NewsCluster& cluster);
    void near_bottom();
    /// Context-menu "filter feed by $TICKER" — the screen routes it through the
    /// same path as a typed search.
    void ticker_filter_requested(const QString& ticker);
    /// Empty state's "Clear filters" button.
    void clear_filters_requested();
    /// Empty / error state's "Retry" button.
    void retry_requested();
    /// Error state's "Sources" button (open the feed manager).
    void manage_sources_requested();
    /// Context bar: SHOW ITS NEWS (Offer mode).
    void context_show_requested();
    /// Context bar / symbol empty state: SHOW ALL NEWS (Active) or dismiss (Offer).
    void context_clear_requested();
    /// Context bar: FOLLOW LINK toggled by the user.
    void follow_link_toggled(bool on);
    /// Symbol empty state: search the headlines for the symbol as text instead.
    void symbol_text_search_requested(const QString& symbol);

  protected:
    void changeEvent(QEvent* event) override;
    /// Clicking the breaking banner (outside its dismiss button) opens its story.
    bool eventFilter(QObject* watched, QEvent* event) override;

  private slots:
    void on_item_clicked(const QModelIndex& index);
    void check_scroll_position();
    void show_context_menu(const QPoint& pos);

  private:
    void build_breaking_banner();
    void build_skeleton();
    void remove_skeleton();
    bool is_banner_duplicate(const QString& headline) const;

    /// Capture / restore the reading position across a model reset. A feed
    /// refresh (auto-refresh, live WS article, filter re-apply) resets the
    /// model, which drops the QListView back to row 0 — scrolling a user away
    /// from the item they were reading. We anchor on the article that was at
    /// the top of the viewport and re-scroll to it once the reset completes.
    void capture_scroll_anchor();
    void restore_scroll_anchor();

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    QStackedWidget* stack_ = nullptr;
    QListView* list_view_ = nullptr;
    NewsFeedModel* model_ = nullptr;
    NewsFeedDelegate* delegate_ = nullptr;

    // Breaking banner
    QWidget* banner_widget_ = nullptr;
    QLabel* banner_tag_ = nullptr;
    QLabel* banner_headline_ = nullptr;
    QLabel* banner_source_ = nullptr;
    QTimer* banner_dismiss_timer_ = nullptr;
    QLabel* banner_pos_ = nullptr;        // "1/3"
    QPushButton* banner_prev_ = nullptr;
    QPushButton* banner_next_ = nullptr;
    /// Lead article of the story on the banner (opened when the banner is clicked).
    services::NewsArticle banner_article_;
    /// Stories the banner steps through (newest first) and the one shown.
    QVector<services::NewsArticle> banner_items_;
    int banner_index_ = 0;
    static constexpr int kMaxBannerItems = 3;
    void show_banner_item(int index);

    struct BreakingEntry {
        QString headline_key;
        int64_t shown_at = 0;
    };
    QVector<BreakingEntry> recent_banners_;
    int64_t global_cooldown_until_ = 0;
    static constexpr int BANNER_DEDUP_WINDOW_SEC = 1800;   // 30 min
    static constexpr int BANNER_GLOBAL_COOLDOWN_SEC = 120; // 2 min
    static constexpr int SOUND_COOLDOWN_SEC = 300;         // 5 min
    int64_t last_sound_at_ = 0;

    // Loading page: title, progress, "filtered out" note + CLEAR FILTERS, then
    // placeholder rows. Colours come from the screen's sheet (theme-refreshed).
    QWidget* skeleton_overlay_ = nullptr;
    QLabel* loading_title_ = nullptr;
    QLabel* loading_progress_ = nullptr;
    QLabel* loading_filtered_note_ = nullptr;
    QPushButton* loading_clear_btn_ = nullptr;
    int loading_done_ = 0;
    int loading_total_ = 0;
    int loading_articles_ = 0;
    bool is_loading_ = false;
    void apply_loading_texts();

    // Empty-state widget (shown when loading is done + no articles)
    QWidget* empty_state_ = nullptr;
    QLabel* empty_state_title_ = nullptr;
    QLabel* empty_state_hint_ = nullptr;
    QLabel* empty_state_detail_ = nullptr;
    QPushButton* empty_clear_btn_ = nullptr;
    QPushButton* empty_retry_btn_ = nullptr;
    QPushButton* empty_sources_btn_ = nullptr;
    QPushButton* empty_show_all_btn_ = nullptr;
    QPushButton* empty_search_btn_ = nullptr;
    EmptyKind empty_kind_ = EmptyKind::NoArticles;
    QString empty_symbol_; ///< EmptyKind::Symbol

    // Symbol context bar (see set_context_bar).
    void build_context_bar();
    void apply_context_texts();
    QWidget* context_bar_ = nullptr;
    QLabel* context_lead_ = nullptr;   // "NEWS FOR" / "GROUP A IS ON"
    QLabel* context_symbol_ = nullptr; // the symbol chip
    QLabel* context_note_ = nullptr;   // "following link group A" / "nothing tagged yet …"
    QPushButton* context_show_btn_ = nullptr;
    QPushButton* context_follow_btn_ = nullptr;
    QPushButton* context_clear_btn_ = nullptr;
    ContextBar context_;
    /// Re-apply the texts of the current empty-state kind (also on language change).
    void apply_empty_texts();

    // Scroll/selection preservation across model resets.
    QString anchor_article_id_;  // article at the top of the viewport
    QString current_article_id_; // article under the keyboard cursor
    bool anchor_was_at_top_ = true;
};

} // namespace fincept::screens
