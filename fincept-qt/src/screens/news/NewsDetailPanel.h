#pragma once
#include "services/news/NewsCorrelationService.h"
#include "services/news/NewsMonitorService.h"
#include "services/news/NewsNlpService.h"
#include "services/news/NewsService.h"

#include <QEvent>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

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
    void clear();

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
    void panel_closed();

  private:
    QWidget* build_empty_state();
    QWidget* build_content_view();

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    bool panel_open_ = false;

    // Static header / empty-state / section titles (cached for retranslateUi).
    QLabel* header_title_ = nullptr;
    QLabel* empty_label_ = nullptr;
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
    QLabel* ai_sentiment_ = nullptr;
    QLabel* ai_urgency_ = nullptr;
    QLabel* ai_prediction_ = nullptr;
    QLabel* ai_confidence_ = nullptr;
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
};

} // namespace fincept::screens
