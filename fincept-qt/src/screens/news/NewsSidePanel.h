#pragma once
#include "services/news/NewsCorrelationService.h"
#include "services/news/NewsMonitorService.h"
#include "services/news/NewsNlpService.h"
#include "services/news/NewsService.h"

#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

/// Intelligence drawer — slides in from the left over the feed panel.
/// Contains all advanced intelligence sections: monitors, top stories,
/// categories, entities, signals, instability, predictions, bookmarks.
/// Toggled via the INTEL button in the command bar.
class NewsSidePanel : public QWidget {
    Q_OBJECT
  public:
    explicit NewsSidePanel(QWidget* parent = nullptr);

    void update_stats(int feed_count, int article_count, int cluster_count, int source_count);
    void update_sentiment(int bullish, int bearish, int neutral);
    void update_top_stories(const QVector<services::NewsArticle>& top);
    void update_categories(const QMap<QString, int>& counts);
    void update_monitors(const QVector<services::NewsMonitor>& monitors,
                         const QMap<QString, QVector<services::NewsArticle>>& matches);
    void update_deviations(const QVector<QPair<QString, double>>& deviations);
    void update_entities(const services::NerResult& ner);
    void update_locations(const QVector<services::ArticleGeo>& geo);
    void update_signals(const QVector<services::CorrelationSignal>& sigs);
    void update_instability(const QString& country, const services::InstabilityScore& score);
    void update_predictions(const QVector<services::PredictionMarket>& predictions);
    void update_saved(const QVector<services::NewsArticle>& saved);

    /// Toggle drawer visibility with animation
    void toggle_drawer();
    bool is_drawer_open() const { return drawer_open_; }

  signals:
    void category_clicked(const QString& category);
    void article_clicked(const services::NewsArticle& article);
    void monitor_added(const QString& label, const QStringList& keywords);
    void monitor_toggled(const QString& id);
    void monitor_deleted(const QString& id);
    void close_requested();

  private:
    void build_top_stories_section(QVBoxLayout* parent);
    void build_categories_section(QVBoxLayout* parent);
    void build_monitors_section(QVBoxLayout* parent);
    void build_deviations_section(QVBoxLayout* parent);

    bool drawer_open_ = false;

    // Top stories
    QVBoxLayout* top_stories_layout_ = nullptr;

    // Categories
    QVBoxLayout* categories_layout_ = nullptr;

    // Monitors
    QVBoxLayout* monitors_layout_ = nullptr;
    QLineEdit* monitor_input_ = nullptr;

    // Deviations
    QVBoxLayout* deviations_layout_ = nullptr;
    QWidget* deviations_section_ = nullptr;

    // Entities (NER)
    QVBoxLayout* entities_layout_ = nullptr;
    QWidget* entities_section_ = nullptr;

    // Locations
    QVBoxLayout* locations_layout_ = nullptr;
    QWidget* locations_section_ = nullptr;

    // Correlation signals
    QVBoxLayout* signals_layout_ = nullptr;
    QWidget* signals_section_ = nullptr;

    // Country Instability
    QVBoxLayout* cii_layout_ = nullptr;
    QWidget* cii_section_ = nullptr;

    // Prediction markets
    QVBoxLayout* predictions_layout_ = nullptr;
    QWidget* predictions_section_ = nullptr;

    // Saved / bookmarked articles
    QVBoxLayout* saved_layout_ = nullptr;
    QWidget* saved_section_ = nullptr;
};

} // namespace fincept::screens
