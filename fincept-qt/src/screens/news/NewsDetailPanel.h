#pragma once
#include "services/news/NewsCorrelationService.h"
#include "services/news/NewsMonitorService.h"
#include "services/news/NewsNlpService.h"
#include "services/news/NewsService.h"

#include <QEvent>
#include <QHash>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::ui {
class EmptyState;
class Gauge;
class KpiStrip;
} // namespace fincept::ui

namespace fincept::screens {

/// Overlay detail panel — 420px wide, appears from the right when an article
/// is selected. Replaces the old fixed 340px panel. Has a close button to
/// dismiss and return to full-width feed view.
class NewsDetailPanel : public QWidget {
    Q_OBJECT
  public:
    explicit NewsDetailPanel(QWidget* parent = nullptr);

    void show_article(const services::NewsArticle& article);
    void show_analysis(const services::NewsAnalysis& analysis);
    void show_related(const QVector<services::NewsArticle>& related);
    void show_monitor_matches(const QVector<QPair<services::NewsMonitor, QStringList>>& matches);
    void show_entities(const services::EntityResult& entities);
    void show_infrastructure(const QVector<services::InfrastructureItem>& items);
    /// Tell the user the ANALYZE request failed (and re-arm the button) instead
    /// of leaving it spinning until the progress guard fires. `error` is the service's
    /// failure text; an HTTP 402 (out of credits) offers Plans & Pricing.
    void show_analysis_failed(const QString& error = {});
    /// Re-read the bookmark state of the current article from the DB and
    /// reflect it on the BOOKMARK button (also reverts a failed toggle).
    void refresh_bookmark_state();
    /// Link of the article currently shown (empty when none) — lets the screen
    /// drop an analysis result that belongs to an article the user has left.
    QString current_article_link() const { return has_article_ ? current_article_.link : QString(); }
    /// The article currently shown (default-constructed when none) — its headline
    /// and feed summary go with an ANALYZE request.
    services::NewsArticle current_article() const { return has_article_ ? current_article_ : services::NewsArticle{}; }
    void clear();
    /// Days until the next earnings report per symbol (badges next to tickers).
    void set_earnings_days(const QHash<QString, int>& days);
    /// First-open width (the screen's splitter sizes the panel afterwards).
    QSize sizeHint() const override;

    /// Show/hide the panel
    void open_panel();
    void close_panel();
    bool is_panel_open() const { return panel_open_; }

  protected:
    void changeEvent(QEvent* event) override;

  signals:
    void analyze_requested(const QString& article_url);
    void related_article_clicked(const services::NewsArticle& article);
    void bookmark_requested(const services::NewsArticle& article);
    /// "Filter feed by $TICKER" from a ticker link's menu.
    void ticker_filter_requested(const QString& ticker);
    void panel_closed();

  private:
    QWidget* build_empty_state();
    QWidget* build_content_view();
    /// Build the article view (stack page 1) on first use; every method that
    /// touches it calls this first. The panel starts hidden with the screen.
    void ensure_content();
    bool content_built_ = false;

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    bool panel_open_ = false;

    // Static header / empty-state / section titles (cached for retranslateUi).
    QLabel* header_title_ = nullptr;
    fincept::ui::EmptyState* empty_state_ = nullptr; // stack page 0: no article selected
    /// In place of the AI analysis until one exists for the article: what it
    /// returns, its credit cost and ANALYZE (never run automatically).
    fincept::ui::EmptyState* analysis_preview_ = nullptr;
    /// The preview's action mirrors analyze_btn_ (text / enabled).
    void sync_analysis_preview();
    QLabel* ai_title_ = nullptr;
    QLabel* monitor_title_ = nullptr;
    QLabel* related_title_ = nullptr;
    QLabel* entities_section_title_ = nullptr;
    QLabel* infra_title_ = nullptr;

    // Article section
    QLabel* headline_label_ = nullptr;
    QLabel* priority_badge_ = nullptr;
    QLabel* sentiment_badge_ = nullptr;
    QLabel* tier_badge_ = nullptr;
    QLabel* category_label_ = nullptr;
    QLabel* source_label_ = nullptr;
    QLabel* time_label_ = nullptr;
    QLabel* summary_label_ = nullptr;
    QLabel* impact_label_ = nullptr;
    QLabel* tickers_label_ = nullptr;

    // AI analysis section
    QWidget* analysis_section_ = nullptr;
    QLabel* ai_fetch_note_ = nullptr; // publisher-block / metadata-only banner
    QLabel* ai_summary_ = nullptr;
    /// Headline KPIs: sentiment / confidence / urgency / outlook.
    fincept::ui::KpiStrip* ai_kpis_ = nullptr;
    /// Sentiment score on a -1 … +1 scale.
    fincept::ui::Gauge* ai_gauge_ = nullptr;
    QLabel* ai_keywords_ = nullptr;
    QLabel* ai_credits_ = nullptr;
    QLabel* key_points_title_ = nullptr;
    QVBoxLayout* key_points_layout_ = nullptr;
    QLabel* risk_title_ = nullptr;
    QVBoxLayout* risk_layout_ = nullptr;
    QLabel* topics_title_ = nullptr;
    QVBoxLayout* topics_layout_ = nullptr;
    QLabel* ai_entities_title_ = nullptr;
    QVBoxLayout* ai_entities_layout_ = nullptr;
    QPushButton* analyze_btn_ = nullptr;
    QTimer* analyze_timeout_ = nullptr;
    /// The running ANALYZE already got its "still working" notice (second
    /// phase of analyze_timeout_).
    bool analyze_slow_notice_shown_ = false;
    /// Shown when analysis credits are exhausted / low — opens Plans & Pricing.
    QPushButton* plans_btn_ = nullptr;
    /// Why an ANALYZE produced no analysis (article unreadable / not found — nothing
    /// charged — or the service busy). Hidden otherwise.
    QLabel* analysis_notice_ = nullptr;

    // Monitor matches section
    QWidget* monitor_section_ = nullptr;
    QVBoxLayout* monitor_matches_layout_ = nullptr;

    // Related articles section
    QWidget* related_section_ = nullptr;
    QVBoxLayout* related_layout_ = nullptr;

    // Translate button
    QPushButton* translate_btn_ = nullptr;

    // Entities section
    QWidget* entities_section_ = nullptr;
    QVBoxLayout* entities_detail_layout_ = nullptr;

    // Infrastructure section
    QWidget* infra_section_ = nullptr;
    QVBoxLayout* infra_layout_ = nullptr;

    // Action buttons
    QPushButton* open_btn_ = nullptr;
    QPushButton* copy_btn_ = nullptr;
    QPushButton* copy_title_btn_ = nullptr;
    QPushButton* save_btn_ = nullptr;
    QPushButton* bookmark_btn_ = nullptr;
    QPushButton* close_btn_ = nullptr;

    QStackedWidget* stack_ = nullptr;
    services::NewsArticle current_article_;
    bool has_article_ = false;
    QHash<QString, int> earnings_days_;
};

} // namespace fincept::screens
