#pragma once
#include "services/news/NewsClusterService.h"
#include "services/news/NewsMonitorService.h"
#include "services/news/NewsService.h"

#include <QAbstractListModel>
#include <QSet>
#include <QVector>

namespace fincept::screens {

/// Custom roles for the delegate to query article data without creating widgets.
enum NewsFeedRole {
    ArticleRole = Qt::UserRole + 1,
    ClusterRole,
    ViewModeRole,
    IsNewRole,
    MonitorColorRole,
    IsSelectedRole,
    SourceTierRole,
    VelocityTextRole,
    ThreatLevelRole,
    SourceFlagRole,
    LanguageRole,
    HasGeoRole,
    PulsePhaseRole,
};

/// QAbstractListModel holding article/cluster data for QListView.
/// Zero widget allocation — data is queried via roles and painted by delegate.
class NewsFeedModel : public QAbstractListModel {
    Q_OBJECT
  public:
    explicit NewsFeedModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    void set_wire_articles(const QVector<services::NewsArticle>& articles);
    void set_clusters(const QVector<services::NewsCluster>& clusters);
    void set_view_mode(const QString& mode);
    void set_selected_id(const QString& article_id);
    void set_monitor_matches(const QMap<QString, QVector<services::NewsArticle>>& matches,
                             const QVector<services::NewsMonitor>& monitors);

    void mark_all_seen();
    void mark_seen(const QString& article_id);
    int unseen_count() const;

    void set_geo_articles(const QSet<QString>& geolocated_ids);
    void advance_pulse(); // called by timer to animate new-item pulse

    QModelIndex index_for_article(const QString& article_id) const;
    services::NewsArticle article_at(int row) const;
    services::NewsCluster cluster_at(int row) const;
    QString view_mode() const { return view_mode_; }

  private:
    QString monitor_color_for(const QString& article_id) const;

    QVector<services::NewsArticle> articles_;
    QVector<services::NewsCluster> clusters_;
    QString view_mode_ = "WIRE";
    QString selected_id_;
    QSet<QString> seen_ids_;
    QMap<QString, QString> article_monitor_color_;
    QSet<QString> geo_article_ids_;
    int pulse_phase_ = 0; // 0-3 for animation cycle
};

} // namespace fincept::screens
