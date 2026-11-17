#pragma once
#include "services/news/NewsService.h"

#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPair>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

class QMenu;
class QScrollArea;

namespace fincept::screens {

/// Two-row news header:
///   Row 1: [scrolling filter group: INTEL, search, category / time pills, sort,
///          view] [fixed control group: counts, UNREAD, MY, language, region,
///          variant, CHARTS, SAVED, BRIEF, refresh cadence, SRC, REFRESH, ⋯].
///          The fixed group stays reachable at any width: controls that do not
///          fit are hidden and every control (pills included) is in the ⋯ menu.
///   Row 2: Intel strip — live stats, tone gauge, feed health, monitor alerts,
///          deviation badges.
class NewsCommandBar : public QWidget {
    Q_OBJECT
  public:
    explicit NewsCommandBar(QWidget* parent = nullptr);

    void set_active_category(const QString& cat);
    /// Re-highlight the REL/NEW sort pills to match `sort` ("RELEVANCE" /
    /// "NEWEST"). Used to sync the UI with persisted/restored state without
    /// emitting sort_changed.
    void set_active_sort(const QString& sort);
    /// Re-highlight the WIRE/CLST view pills ("WIRE" / "CLUSTERS").
    void set_active_view(const QString& view);
    /// Re-highlight the time-range pills ("1H".."30D").
    void set_active_time_range(const QString& range);
    /// Select the FULL/FINANCE/CRYPTO/MACRO variant combo without emitting
    /// variant_changed (restores persisted state).
    void set_active_variant(const QString& variant);
    /// Select the language-filter combo ("ALL", "EN", ...) without emitting.
    void set_active_language(const QString& lang);
    /// Select the region-filter combo ("ALL", "EUROPE", "LATAM", ...) without emitting.
    void set_active_region(const QString& region);
    /// MY filter: "ALL", "HOLDINGS", "WATCHLISTS", "MY" (both) or "WL:<id>".
    void set_active_my(const QString& value);
    /// Watchlists offered by the MY filter (id, name).
    void set_watchlists(const QVector<QPair<QString, QString>>& lists);
    void set_unread_only(bool on);
    void set_charts_checked(bool on);
    /// Show `text` in the search box without emitting search_changed — used when
    /// the filter is applied from elsewhere (symbol link / drop) so the active
    /// filter is visible and can be cleared with the box's clear button.
    void set_search_text(const QString& text);
    void set_loading(bool loading);
    void set_loading_progress(int done, int total);
    void set_article_count(int count);
    void set_alert_count(int count);
    void set_unseen_count(int count);

    /// Plain-text line in the brief row (e.g. "nothing to brief").
    void show_summary(const QString& summary);
    /// The headline brief: KPI strip (headlines / categories / urgent) and one
    /// section per category, each headline a clickable row
    /// (brief_article_clicked). `items` most important first.
    void show_brief(const QVector<fincept::services::NewsArticle>& items);
    void hide_summary();
    void set_summarizing(bool busy);

    // Intel strip updates (moved from side panel)
    void update_stats(int feeds, int articles, int clusters, int sources);
    /// Tone of the current filter: label counts, articles not scored (non-English),
    /// and the mean measured tone (NaN when nothing was scored).
    void update_sentiment(int bullish, int bearish, int neutral, int not_scored, double mean_tone);
    void update_deviations(const QVector<QPair<QString, double>>& deviations);
    void update_monitor_summary(int total_monitors, int active_alerts);
    /// Feed health badge: feeds that answered on the last refresh / enabled
    /// feeds, how many are backing off, and a per-feed tooltip.
    void update_feed_health(int ok, int total, int backing_off, const QString& tooltip);
    /// Today's key events among the mentioned companies ("EARNINGS TODAY: AAPL ·
    /// 3 THIS WEEK"); empty text hides it.
    void update_events(const QString& text, const QString& tooltip);
    /// Drive the LIVE/OFFLINE badge in the intel strip.
    void set_live_state(bool connected);
    /// Select the refresh-interval combo to match `minutes` (0 = manual).
    /// Used to restore persisted state without emitting refresh_interval_changed.
    void set_refresh_interval_minutes(int minutes);
    /// Reflect the intel drawer's state on the INTEL toggle without emitting.
    void set_drawer_checked(bool checked);
    /// Anchor for the saved-searches menu.
    QPushButton* saved_button() const { return saved_btn_; }

  protected:
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

  signals:
    void category_changed(const QString& category);
    void time_range_changed(const QString& range);
    void sort_changed(const QString& sort);
    void view_mode_changed(const QString& mode);
    void search_changed(const QString& query);
    void refresh_clicked();
    void summarize_clicked();
    /// A headline in the brief was clicked — open that article.
    void brief_article_clicked(const fincept::services::NewsArticle& article);
    void rtl_toggled();
    void variant_changed(const QString& variant);
    void language_filter_changed(const QString& lang);
    void region_filter_changed(const QString& region);
    void my_filter_changed(const QString& value);
    void unread_only_toggled(bool on);
    void mark_all_read_clicked();
    void charts_toggled(bool on);
    void saved_searches_clicked();
    void feed_health_clicked();
    void drawer_toggle_requested();
    void manage_sources_clicked();
    /// Emitted when the user clicks the LIVE/OFFLINE badge to toggle
    /// the WebSocket live feed.
    void live_toggle_clicked();
    /// Emitted when the user picks a new auto-refresh cadence (minutes).
    void refresh_interval_changed(int minutes);

  private:
    QPushButton* make_pill(const QString& text, const QString& value, QHBoxLayout* layout);
    void update_pill_group(const QVector<QPushButton*>& btns, const QString& active_value);
    void build_command_row(QVBoxLayout* root);
    void build_intel_row(QVBoxLayout* root);
    /// Sizes the rows and controls for the user's font (the design sizes at the
    /// default font size, larger when the text needs it).
    void fit_to_font();
    /// Hide the least important fixed-group controls that do not fit (they stay
    /// in the ⋯ menu).
    void fit_to_width();
    /// The ⋯ menu: every control of the bar.
    void show_overflow_menu();
    void fill_language_combo();
    void fill_region_combo();

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    // Row 1 — command bar
    QLineEdit* search_input_ = nullptr;
    QTimer* search_debounce_ = nullptr; // coalesces keystrokes into one filter pass
    QVector<QPushButton*> category_btns_;
    QVector<QPushButton*> time_btns_;
    QPushButton* sort_relevance_ = nullptr;
    QPushButton* sort_newest_ = nullptr;
    QPushButton* view_wire_ = nullptr;
    QPushButton* view_clusters_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QPushButton* summarize_btn_ = nullptr;
    QPushButton* drawer_btn_ = nullptr;
    QPushButton* sources_btn_ = nullptr;
    QPushButton* rtl_btn_ = nullptr;
    QPushButton* unread_btn_ = nullptr;
    QPushButton* mark_read_btn_ = nullptr;
    QPushButton* charts_btn_ = nullptr;
    QPushButton* saved_btn_ = nullptr;
    QPushButton* more_btn_ = nullptr;
    // AI-brief row (label + dismiss button) — laid out under the intel strip.
    QWidget* summary_row_ = nullptr;
    QLabel* summary_label_ = nullptr;
    QPushButton* summary_close_btn_ = nullptr;
    /// Scrollable host of the structured brief (replaces the label while shown).
    QWidget* brief_scroll_ = nullptr;
    /// Height with the summary row hidden: 32px command row + 28px intel strip.
    static constexpr int kBaseHeight = 60;
    /// kBaseHeight grown to fit the user's font (see fit_to_font).
    int base_height_ = kBaseHeight;
    QScrollArea* command_scroll_ = nullptr;
    QWidget* command_row_ = nullptr;   // the whole first row
    QWidget* filter_group_ = nullptr;  // scrolling left part
    QWidget* control_group_ = nullptr; // fixed right part
    QWidget* intel_row_ = nullptr;
    QLabel* count_label_ = nullptr;
    QLabel* alert_label_ = nullptr;
    QLabel* unseen_label_ = nullptr;
    QLabel* progress_label_ = nullptr;

    QComboBox* variant_combo_ = nullptr;
    QComboBox* lang_filter_combo_ = nullptr;
    QComboBox* region_combo_ = nullptr;
    QComboBox* my_combo_ = nullptr;
    QComboBox* refresh_combo_ = nullptr;

    // Row 2 — intel strip
    QLabel* intel_feeds_ = nullptr;
    QLabel* intel_articles_ = nullptr;
    QLabel* intel_clusters_ = nullptr;
    QLabel* intel_sources_ = nullptr;
    // Fixed text-label captions for the intel stats (cached for retranslateUi).
    QLabel* intel_feeds_lbl_ = nullptr;
    QLabel* intel_articles_lbl_ = nullptr;
    QLabel* intel_clusters_lbl_ = nullptr;
    QLabel* intel_sources_lbl_ = nullptr;
    QLabel* sentiment_caption_ = nullptr;
    QWidget* sentiment_bull_ = nullptr;
    QWidget* sentiment_neut_ = nullptr;
    QWidget* sentiment_bear_ = nullptr;
    QWidget* sentiment_unscored_ = nullptr;
    QLabel* sentiment_score_ = nullptr;
    QLabel* sentiment_counts_ = nullptr;
    QPushButton* feed_health_btn_ = nullptr;
    QLabel* intel_monitors_ = nullptr;
    QLabel* intel_deviations_ = nullptr;
    QLabel* intel_events_ = nullptr;
    QPushButton* live_badge_ = nullptr;

    QString active_category_ = "ALL";
    QString active_time_ = "24H";
    QString active_sort_ = "NEWEST";
    QString active_view_ = "WIRE";
    QString active_my_ = "ALL";
    QVector<QPair<QString, QString>> watchlists_;
};

} // namespace fincept::screens
