#pragma once
#include "services/news/NewsClusterService.h"
#include "services/news/NewsMonitorService.h"
#include "services/news/NewsService.h"

#include <QAbstractListModel>
#include <QHash>
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
    // Pre-formatted display strings — computed once in set_wire_articles,
    // read directly in paint() to avoid per-frame allocations (P7/Per.19)
    FormattedSourceRole,  // source.left(10).toUpper()
    FormattedLangRole,    // lang.toUpper()
    FormattedThreatRole,  // threat_level_string().left(4)
    FormattedTickersRole, // "$AAPL $MSFT" pre-joined string
    ThreatColorRole,      // threat_level_color() string
    PriorityColorRole,    // priority_color() string
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
    QHash<QString, int> article_id_to_row_;         // O(1) index lookup for WIRE mode
    QHash<QString, int> cluster_id_to_row_;         // O(1) index lookup for CLUSTERS mode
    QHash<QString, QString> article_monitor_color_; // O(1) vs QMap's O(log n)
    QSet<QString> geo_article_ids_;
    int pulse_phase_ = 0;  // 0-3 for animation cycle
    int unseen_count_ = 0; // incremental counter, avoids O(n) scan

    // Per-row pre-formatted display strings — avoids string allocations in paint()
    struct FormattedRow {
        QString source;  // source.left(10).toUpper()
        QString lang;    // lang.toUpper()
        QString threat;  // threat_level_string().left(4) or ""
        QString tickers; // "$AAPL $MSFT" or ""
        QString threat_color;
        QString priority_color;
    };
    QVector<FormattedRow> formatted_rows_;
};

} // namespace fincept::screens
