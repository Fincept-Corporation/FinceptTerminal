#include "screens/news/NewsFeedModel.h"

#include "core/logging/Logger.h"

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
    endResetModel();
}

void NewsFeedModel::set_clusters(const QVector<services::NewsCluster>& clusters) {
    beginResetModel();
    clusters_ = clusters;
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
        for (const auto& a : articles_)
            seen_ids_.insert(a.id);
    } else {
        for (const auto& c : clusters_)
            seen_ids_.insert(c.lead_article.id);
    }
    emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {IsNewRole});
}

void NewsFeedModel::mark_seen(const QString& article_id) {
    if (seen_ids_.contains(article_id))
        return;
    seen_ids_.insert(article_id);
    auto idx = index_for_article(article_id);
    if (idx.isValid())
        emit dataChanged(idx, idx, {IsNewRole});
}

int NewsFeedModel::unseen_count() const {
    int count = 0;
    if (view_mode_ == "WIRE") {
        for (const auto& a : articles_) {
            if (!seen_ids_.contains(a.id))
                ++count;
        }
    } else {
        for (const auto& c : clusters_) {
            if (!seen_ids_.contains(c.lead_article.id))
                ++count;
        }
    }
    return count;
}

QModelIndex NewsFeedModel::index_for_article(const QString& article_id) const {
    if (article_id.isEmpty())
        return {};
    if (view_mode_ == "WIRE") {
        for (int i = 0; i < articles_.size(); ++i) {
            if (articles_[i].id == article_id)
                return index(i, 0);
        }
    } else {
        for (int i = 0; i < clusters_.size(); ++i) {
            if (clusters_[i].lead_article.id == article_id)
                return index(i, 0);
        }
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
        QString aid = (view_mode_ == "WIRE" && i < articles_.size()) ? articles_[i].id
                      : (i < clusters_.size() ? clusters_[i].lead_article.id : QString());
        if (!aid.isEmpty() && !seen_ids_.contains(aid))
            emit dataChanged(index(i, 0), index(i, 0), {PulsePhaseRole});
    }
}

} // namespace fincept::screens
