#pragma once
#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPair>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

namespace fincept::screens {

/// Redesigned two-row news header:
///   Row 1 (32px): Search, category pills, time pills, sort/view toggles, controls
///   Row 2 (26px): Intel strip — live stats, sentiment gauge, monitor alerts, deviation badges
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
    void set_loading(bool loading);
    void set_loading_progress(int done, int total);
    void set_article_count(int count);
    void set_alert_count(int count);
    void set_unseen_count(int count);

    void show_summary(const QString& summary);
    void hide_summary();
    void set_summarizing(bool busy);

    // Intel strip updates (moved from side panel)
    void update_stats(int feeds, int articles, int clusters, int sources);
    void update_sentiment(int bullish, int bearish, int neutral);
    void update_deviations(const QVector<QPair<QString, double>>& deviations);
    void update_monitor_summary(int total_monitors, int active_alerts);
    /// Drive the LIVE/OFFLINE badge in the intel strip.
    void set_live_state(bool connected);
    /// Select the refresh-interval combo to match `minutes` (0 = manual).
    /// Used to restore persisted state without emitting refresh_interval_changed.
    void set_refresh_interval_minutes(int minutes);

  protected:
    void changeEvent(QEvent* event) override;

  signals:
    void category_changed(const QString& category);
    void time_range_changed(const QString& range);
    void sort_changed(const QString& sort);
    void view_mode_changed(const QString& mode);
    void search_changed(const QString& query);
    void refresh_clicked();
    void summarize_clicked();
    void rtl_toggled();
    void variant_changed(const QString& variant);
    void language_filter_changed(const QString& lang);
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

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    // Row 1 — command bar
    QLineEdit* search_input_ = nullptr;
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
    QLabel* summary_label_ = nullptr;
    QLabel* count_label_ = nullptr;
    QLabel* alert_label_ = nullptr;
    QLabel* unseen_label_ = nullptr;
    QLabel* progress_label_ = nullptr;

    QComboBox* variant_combo_ = nullptr;
    QComboBox* lang_filter_combo_ = nullptr;
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
    QLabel* sentiment_score_ = nullptr;
    QLabel* intel_monitors_ = nullptr;
    QLabel* intel_deviations_ = nullptr;
    QPushButton* live_badge_ = nullptr;

    QString active_category_ = "ALL";
    QString active_time_ = "24H";
    QString active_sort_ = "RELEVANCE";
    QString active_view_ = "WIRE";
};

} // namespace fincept::screens
