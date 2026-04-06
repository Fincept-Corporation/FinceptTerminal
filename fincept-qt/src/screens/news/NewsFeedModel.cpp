#include "screens/news/NewsFeedModel.h"

#include "core/logging/Logger.h"
#include "services/news/NewsService.h"

#include <QStringList>

namespace fincept::screens {

NewsFeedModel::NewsFeedModel(QObject* parent) : QAbstractListModel(parent) {
    qRegisterMetaType<services::NewsArticle>("NewsArticle");
    qRegisterMetaType<services::NewsCluster>("NewsCluster");
}

int NewsFeedModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid())
        return 0;
    return view_mode_ == "WIRE" ? articles_.size() : clusters_.size();
}

QVariant NewsFeedModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid())
        return {};

    const int row = index.row();

    if (view_mode_ == "WIRE") {
        if (row < 0 || row >= articles_.size())
            return {};
        const auto& article = articles_[row];

        switch (role) {
            case Qt::DisplayRole:
                return article.headline;
            case ArticleRole:
                return QVariant::fromValue(article);
            case ViewModeRole:
                return view_mode_;
            case IsNewRole:
                return !seen_ids_.contains(article.id);
            case MonitorColorRole:
                return monitor_color_for(article.id);
            case IsSelectedRole:
                return article.id == selected_id_;
            case SourceTierRole:
                return article.tier;
            case ThreatLevelRole:
                return static_cast<int>(article.threat.level);
            case SourceFlagRole:
                return static_cast<int>(article.source_flag);
            case LanguageRole:
                return article.lang;
            case HasGeoRole:
                return geo_article_ids_.contains(article.id);
            case PulsePhaseRole:
                return (!seen_ids_.contains(article.id)) ? pulse_phase_ : -1;
            // Pre-formatted display strings — no allocation in paint()
            case FormattedSourceRole:
                return row < formatted_rows_.size() ? formatted_rows_[row].source   : QVariant{};
            case FormattedLangRole:
                return row < formatted_rows_.size() ? formatted_rows_[row].lang     : QVariant{};
            case FormattedThreatRole:
                return row < formatted_rows_.size() ? formatted_rows_[row].threat   : QVariant{};
            case FormattedTickersRole:
                return row < formatted_rows_.size() ? formatted_rows_[row].tickers  : QVariant{};
            case ThreatColorRole:
                return row < formatted_rows_.size() ? formatted_rows_[row].threat_color   : QVariant{};
            case PriorityColorRole:
                return row < formatted_rows_.size() ? formatted_rows_[row].priority_color : QVariant{};
            default:
                return {};
        }
    }

    // CLUSTERS mode
    if (row < 0 || row >= clusters_.size())
        return {};
    const auto& cluster = clusters_[row];

    switch (role) {
        case Qt::DisplayRole:
            return cluster.lead_article.headline;
        case ClusterRole:
            return QVariant::fromValue(cluster);
        case ArticleRole:
            return QVariant::fromValue(cluster.lead_article);
        case ViewModeRole:
            return view_mode_;
        case IsNewRole:
            return !seen_ids_.contains(cluster.lead_article.id);
        case MonitorColorRole:
            return monitor_color_for(cluster.lead_article.id);
        case IsSelectedRole:
            return cluster.lead_article.id == selected_id_;
        case SourceTierRole:
            return cluster.tier;
        case VelocityTextRole:
            if (cluster.velocity == "rising" && cluster.source_count > 1)
                return QString("+%1 src").arg(cluster.source_count);
            return QString();
        default:
            return {};
    }
}

void NewsFeedModel::set_wire_articles(const QVector<services::NewsArticle>& articles) {
    beginResetModel();
    articles_ = articles;

    // Rebuild O(1) lookup cache, incremental unseen counter, and pre-formatted rows
    article_id_to_row_.clear();
    article_id_to_row_.reserve(articles.size());
    formatted_rows_.clear();
    formatted_rows_.reserve(articles.size());
    unseen_count_ = 0;

    for (int i = 0; i < articles.size(); ++i) {
        const auto& a = articles[i];
        article_id_to_row_.insert(a.id, i);
        if (!seen_ids_.contains(a.id))
            ++unseen_count_;

        // Pre-format once — avoids allocation in paint() on every frame
        FormattedRow fr;
        fr.source  = a.source.left(10).toUpper();
        fr.lang    = (a.lang.isEmpty() || a.lang == "en") ? QString() : a.lang.toUpper();
        const bool show_threat = a.threat.level != services::ThreatLevel::INFO &&
                                 a.threat.level != services::ThreatLevel::LOW;
        fr.threat       = show_threat ? services::threat_level_string(a.threat.level).left(4) : QString();
        fr.threat_color = show_threat ? services::threat_level_color(a.threat.level) : QString();
        fr.priority_color = services::priority_color(a.priority);

        // Pre-join tickers: "$AAPL $MSFT" (up to 2)
        QStringList ticker_parts;
        for (int t = 0; t < std::min(2, static_cast<int>(a.tickers.size())); ++t)
            ticker_parts << ('$' + a.tickers[t]);
        fr.tickers = ticker_parts.join(' ');

        formatted_rows_.append(std::move(fr));
    }

    endResetModel();
}

void NewsFeedModel::set_clusters(const QVector<services::NewsCluster>& clusters) {
    beginResetModel();
    clusters_ = clusters;
    // Rebuild O(1) lookup cache and incremental unseen counter
    cluster_id_to_row_.clear();
    cluster_id_to_row_.reserve(clusters.size());
    unseen_count_ = 0;
    for (int i = 0; i < clusters.size(); ++i) {
        cluster_id_to_row_.insert(clusters[i].lead_article.id, i);
        if (!seen_ids_.contains(clusters[i].lead_article.id))
            ++unseen_count_;
    }
    endResetModel();
}

void NewsFeedModel::set_view_mode(const QString& mode) {
    if (view_mode_ == mode)
        return;
    beginResetModel();
    view_mode_ = mode;
    endResetModel();
}

void NewsFeedModel::set_selected_id(const QString& article_id) {
    if (selected_id_ == article_id)
        return;

    auto old_idx = index_for_article(selected_id_);
    selected_id_ = article_id;
    auto new_idx = index_for_article(selected_id_);

    if (old_idx.isValid())
        emit dataChanged(old_idx, old_idx, {IsSelectedRole});
    if (new_idx.isValid())
        emit dataChanged(new_idx, new_idx, {IsSelectedRole});
}

void NewsFeedModel::set_monitor_matches(const QMap<QString, QVector<services::NewsArticle>>& matches,
                                        const QVector<services::NewsMonitor>& monitors) {
    article_monitor_color_.clear();
    for (const auto& monitor : monitors) {
        if (!monitor.enabled)
            continue;
        auto it = matches.find(monitor.id);
        if (it == matches.end())
            continue;
        for (const auto& article : it.value()) {
            if (!article_monitor_color_.contains(article.id))
                article_monitor_color_[article.id] = monitor.color;
        }
    }
    emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {MonitorColorRole});
}

void NewsFeedModel::mark_all_seen() {
    if (view_mode_ == "WIRE") {
        for (const auto& a : std::as_const(articles_))
            seen_ids_.insert(a.id);
    } else {
        for (const auto& c : std::as_const(clusters_))
            seen_ids_.insert(c.lead_article.id);
    }
    unseen_count_ = 0;
    emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {IsNewRole});
}

void NewsFeedModel::mark_seen(const QString& article_id) {
    if (seen_ids_.contains(article_id))
        return;
    seen_ids_.insert(article_id);
    if (unseen_count_ > 0)
        --unseen_count_;
    const auto idx = index_for_article(article_id);
    if (idx.isValid())
        emit dataChanged(idx, idx, {IsNewRole, PulsePhaseRole});
}

int NewsFeedModel::unseen_count() const {
    return unseen_count_; // O(1) — maintained incrementally
}

QModelIndex NewsFeedModel::index_for_article(const QString& article_id) const {
    if (article_id.isEmpty())
        return {};
    if (view_mode_ == "WIRE") {
        const auto it = article_id_to_row_.find(article_id);
        if (it != article_id_to_row_.end())
            return index(it.value(), 0);
    } else {
        const auto it = cluster_id_to_row_.find(article_id);
        if (it != cluster_id_to_row_.end())
            return index(it.value(), 0);
    }
    return {};
}

services::NewsArticle NewsFeedModel::article_at(int row) const {
    if (view_mode_ == "WIRE") {
        if (row >= 0 && row < articles_.size())
            return articles_[row];
    } else {
        if (row >= 0 && row < clusters_.size())
            return clusters_[row].lead_article;
    }
    return {};
}

services::NewsCluster NewsFeedModel::cluster_at(int row) const {
    if (row >= 0 && row < clusters_.size())
        return clusters_[row];
    return {};
}

QString NewsFeedModel::monitor_color_for(const QString& article_id) const {
    auto it = article_monitor_color_.find(article_id);
    return it != article_monitor_color_.end() ? it.value() : QString();
}

void NewsFeedModel::set_geo_articles(const QSet<QString>& geolocated_ids) {
    geo_article_ids_ = geolocated_ids;
    emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {HasGeoRole});
}

void NewsFeedModel::advance_pulse() {
    pulse_phase_ = (pulse_phase_ + 1) % 4;
    // Only emit for unseen articles (optimization)
    for (int i = 0; i < rowCount(); ++i) {
        QString aid = (view_mode_ == "WIRE" && i < articles_.size())
                          ? articles_[i].id
                          : (i < clusters_.size() ? clusters_[i].lead_article.id : QString());
        if (!aid.isEmpty() && !seen_ids_.contains(aid))
            emit dataChanged(index(i, 0), index(i, 0), {PulsePhaseRole});
    }
}

} // namespace fincept::screens
