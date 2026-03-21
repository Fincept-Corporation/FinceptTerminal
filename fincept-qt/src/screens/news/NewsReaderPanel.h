#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include "services/news/NewsService.h"

namespace fincept::screens {

/// Right panel (320px) — article reader + AI analysis.
class NewsReaderPanel : public QWidget {
    Q_OBJECT
public:
    explicit NewsReaderPanel(QWidget* parent = nullptr);

    void show_article(const fincept::services::NewsArticle& article);
    void show_analysis(const fincept::services::NewsAnalysis& analysis);

private:
    QWidget* build_empty_state();
    QWidget* build_article_view();
    QWidget* build_analysis_view();
    void update_article_view(const fincept::services::NewsArticle& article);
    void update_analysis_view(const fincept::services::NewsAnalysis& analysis);

    QWidget* empty_state_    = nullptr;
    QWidget* article_view_   = nullptr;
    QWidget* analysis_view_  = nullptr;

    // Article view labels
    QLabel* headline_label_  = nullptr;
    QLabel* priority_badge_  = nullptr;
    QLabel* sentiment_badge_ = nullptr;
    QLabel* category_badge_  = nullptr;
    QLabel* source_label_    = nullptr;
    QLabel* time_label_      = nullptr;
    QLabel* summary_label_   = nullptr;
    QLabel* impact_label_    = nullptr;
    QLabel* tickers_label_   = nullptr;

    // Analysis view labels
    QLabel* ai_summary_label_   = nullptr;
    QLabel* ai_sentiment_label_ = nullptr;
    QLabel* ai_urgency_label_   = nullptr;
    QVBoxLayout* key_points_layout_ = nullptr;
    QVBoxLayout* risk_layout_       = nullptr;
    QVBoxLayout* topics_layout_     = nullptr;
    QVBoxLayout* keywords_layout_   = nullptr;

    fincept::services::NewsArticle current_article_;
    bool has_article_ = false;
};

} // namespace fincept::screens
