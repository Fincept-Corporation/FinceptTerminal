#pragma once
#include "services/news/NewsClusterService.h"
#include "services/news/NewsMonitorService.h"
#include "services/news/NewsService.h"

#include <QAbstractListModel>
#include <QHash>
#include <QSet>
#include <QStringList>
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
    DuplicateCountRole,   // other sources carrying the same headline (WIRE view, collapsed)
    EventLabelRole,       // short market-impact event label ("RATES", "EARN", "M&A" ...)
    EarningsBadgeRole,    // "ER 3d" when a tagged ticker reports soon; empty otherwise
    CardHeightRole,       // CLUSTERS: number of "also" lines the card shows
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

    /// Mark every row of the active view seen; returns the ids that were unseen
    /// (the caller persists them).
    QStringList mark_all_seen();
    void mark_seen(const QString& article_id);
    int unseen_count() const;
    bool is_seen(const QString& article_id) const { return seen_ids_.contains(article_id); }
    /// Snapshot of the read set (implicitly shared — cheap to copy) for the
    /// filter worker's UNREAD filter.
    QSet<QString> seen_ids() const { return seen_ids_; }

    /// WIRE view: how many other sources carried each shown headline (lead id -> count).
    void set_duplicate_counts(const QHash<QString, int>& counts);
    /// Upcoming earnings per symbol ("AAPL" -> days until the report, 0 = today).
    void set_earnings_days(const QHash<QString, int>& days);

    void set_geo_articles(const QSet<QString>& geolocated_ids);
    void advance_pulse(); // called by timer to animate new-item pulse

    QModelIndex index_for_article(const QString& article_id) const;
    services::NewsArticle article_at(int row) const;
    services::NewsCluster cluster_at(int row) const;
    QString view_mode() const { return view_mode_; }

  private:
    QString monitor_color_for(const QString& article_id) const;
    /// Recount unseen rows for the ACTIVE view mode (WIRE rows or cluster leads).
    void recount_unseen();
    /// Full hover text of an article: headline, source, absolute time with zone,
    /// language, tone, impact, tags with confidence.
    QString tooltip_for(const services::NewsArticle& a, int duplicates) const;
    QString earnings_badge_for(const services::NewsArticle& a) const;
    QHash<QString, int> duplicate_counts_;
    QHash<QString, int> earnings_days_;

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
    int unseen_count_ = 0; // incremental counter for the active view mode, avoids O(n) scans

    // Per-row pre-formatted display strings — avoids string allocations in paint()
    struct FormattedRow {
        QString source;  // source.toUpper() (the delegate elides to the column)
        QString lang;    // lang.toUpper()
        QString threat;  // threat_level_string().left(4) or ""
        QString tickers; // "$AAPL $MSFT" or ""
        QString threat_color;
        QString priority_color;
        QString event;   // "RATES", "EARN", "M&A" ...
    };
    QVector<FormattedRow> formatted_rows_;
};

} // namespace fincept::screens
