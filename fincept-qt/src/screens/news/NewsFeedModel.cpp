#include "screens/news/NewsFeedModel.h"

#include "core/logging/Logger.h"
#include "services/news/NewsService.h"
#include "ui/formatting/DateFormat.h"
#include "ui/theme/ThemeManager.h"

#include <QDateTime>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

namespace {

/// Short label of a market-impact event for the narrow feed column.
QString nfm_event_label(const QString& event_type) {
    if (event_type == QLatin1String("EARNINGS"))
        return QStringLiteral("EARN");
    if (event_type == QLatin1String("CORPORATE"))
        return QStringLiteral("CORP");
    if (event_type == QLatin1String("POLICY"))
        return QStringLiteral("POL");
    return event_type; // RATES, M&A, RATING, MACRO, LEGAL
}

} // namespace

NewsFeedModel::NewsFeedModel(QObject* parent) : QAbstractListModel(parent) {
    qRegisterMetaType<services::NewsArticle>("NewsArticle");
    qRegisterMetaType<services::NewsCluster>("NewsCluster");

    // The pre-formatted rows hold colour strings taken from the theme; refresh
    // them (colours only — the rows themselves are unchanged) when it changes.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this](const ui::ThemeTokens&) {
        const int n = std::min(static_cast<int>(formatted_rows_.size()), static_cast<int>(articles_.size()));
        if (n == 0)
            return;
        for (int i = 0; i < n; ++i) {
            const auto& a = articles_[i];
            FormattedRow& fr = formatted_rows_[i];
            if (!fr.threat_color.isEmpty())
                fr.threat_color = services::threat_level_color(a.threat.level);
            fr.priority_color = services::priority_color(a.priority);
        }
        if (view_mode_ == "WIRE")
            emit dataChanged(index(0), index(n - 1), {ThreatColorRole, PriorityColorRole});
    });
}

int NewsFeedModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid())
        return 0;
    return view_mode_ == "WIRE" ? articles_.size() : clusters_.size();
}

QString NewsFeedModel::earnings_badge_for(const services::NewsArticle& a) const {
    if (earnings_days_.isEmpty())
        return {};
    int best = -1;
    for (const auto& t : a.tickers) {
        const auto it = earnings_days_.constFind(t.toUpper());
        if (it != earnings_days_.constEnd() && (best < 0 || it.value() < best))
            best = it.value();
    }
    if (best < 0)
        return {};
    return best == 0 ? tr("ER today") : tr("ER %1d").arg(best);
}

QString NewsFeedModel::tooltip_for(const services::NewsArticle& a, int duplicates) const {
    namespace fmt = ui::formatting;
    QStringList lines;
    lines << a.headline.toHtmlEscaped();
    QString meta = a.source.toHtmlEscaped();
    if (a.undated) {
        meta += QStringLiteral(" · ") + tr("undated — the feed gives no publication time");
    } else if (a.sort_ts > 0) {
        const QDateTime dt = QDateTime::fromSecsSinceEpoch(a.sort_ts);
        meta += QStringLiteral(" · ") + fmt::date_time(dt) + QLatin1Char(' ') + fmt::zone_label(dt) +
                QStringLiteral(" (") + fmt::relative(static_cast<qint64>(a.sort_ts)) + QLatin1Char(')');
    }
    if (!a.lang.isEmpty())
        meta += QStringLiteral(" · ") + a.lang.toUpper();
    if (!a.region.isEmpty())
        meta += QStringLiteral(" · ") + a.region;
    lines << meta;
    if (a.sentiment_scored)
        lines << tr("Tone %1 (%2)")
                     .arg(QString::number(a.sentiment_score, 'f', 2), services::sentiment_string(a.sentiment));
    else
        lines << tr("Tone not scored (%1)").arg(a.lang.isEmpty() ? tr("unknown language") : a.lang.toUpper());
    QString impact = tr("Market impact %1").arg(services::impact_string(a.impact));
    if (!a.event_type.isEmpty())
        impact += QStringLiteral(" · ") + a.event_type;
    lines << impact;
    if (!a.tickers.isEmpty()) {
        QStringList tags;
        for (int i = 0; i < a.tickers.size(); ++i) {
            const double c = i < a.ticker_confidence.size() ? a.ticker_confidence[i] : -1.0;
            tags << (c >= 0 ? QStringLiteral("%1 %2%").arg(a.tickers[i]).arg(std::lround(c * 100))
                            : a.tickers[i]);
        }
        lines << tr("Tickers: %1").arg(tags.join(QStringLiteral(", ")).toHtmlEscaped());
    }
    if (!a.unverified_tickers.isEmpty())
        lines << tr("Unverified: %1").arg(a.unverified_tickers.join(QStringLiteral(", ")).toHtmlEscaped());
    if (duplicates > 0)
        lines << tr("Also carried by %n other source(s)", "", duplicates);
    return QStringLiteral("<qt>") + lines.join(QStringLiteral("<br>")) + QStringLiteral("</qt>");
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
            case Qt::ToolTipRole:
                return tooltip_for(article, duplicate_counts_.value(article.id));
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
            case DuplicateCountRole:
                return duplicate_counts_.value(article.id);
            case EarningsBadgeRole:
                return earnings_badge_for(article);
            // Pre-formatted display strings — no allocation in paint()
            case FormattedSourceRole:
                return row < formatted_rows_.size() ? formatted_rows_[row].source : QVariant{};
            case FormattedLangRole:
                return row < formatted_rows_.size() ? formatted_rows_[row].lang : QVariant{};
            case FormattedThreatRole:
                return row < formatted_rows_.size() ? formatted_rows_[row].threat : QVariant{};
            case FormattedTickersRole:
                return row < formatted_rows_.size() ? formatted_rows_[row].tickers : QVariant{};
            case ThreatColorRole:
                return row < formatted_rows_.size() ? formatted_rows_[row].threat_color : QVariant{};
            case PriorityColorRole:
                return row < formatted_rows_.size() ? formatted_rows_[row].priority_color : QVariant{};
            case EventLabelRole:
                return row < formatted_rows_.size() ? formatted_rows_[row].event : QVariant{};
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
        case Qt::ToolTipRole:
            return tooltip_for(cluster.lead_article, std::max(0, cluster.source_count - 1));
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
        case HasGeoRole:
            return geo_article_ids_.contains(cluster.lead_article.id);
        case EarningsBadgeRole:
            return earnings_badge_for(cluster.lead_article);
        case VelocityTextRole:
            if (cluster.velocity == "rising")
                return cluster.source_count > 1 ? tr("RISING +%1 src").arg(cluster.source_count) : tr("RISING");
            if (cluster.velocity == "falling")
                return tr("FADING");
            return QString();
        case CardHeightRole: {
            // "Also:" line only when other sources exist.
            QSet<QString> sources;
            for (const auto& a : cluster.articles)
                sources.insert(a.source);
            return sources.size() > 1 ? 1 : 0;
        }
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

    for (int i = 0; i < articles.size(); ++i) {
        const auto& a = articles[i];
        article_id_to_row_.insert(a.id, i);

        // Pre-format once — avoids allocation in paint() on every frame
        FormattedRow fr;
        fr.source = a.source.toUpper();
        fr.lang = (a.lang.isEmpty() || a.lang == "en") ? QString() : a.lang.toUpper();
        const bool show_threat =
            a.threat.level != services::ThreatLevel::INFO && a.threat.level != services::ThreatLevel::LOW;
        fr.threat = show_threat ? services::threat_level_string(a.threat.level).left(4) : QString();
        fr.threat_color = show_threat ? services::threat_level_color(a.threat.level) : QString();
        fr.priority_color = services::priority_color(a.priority);
        fr.event = nfm_event_label(a.event_type);

        // Pre-join tickers: "$AAPL $MSFT" (up to 2; the tooltip lists all)
        QStringList ticker_parts;
        for (int t = 0; t < std::min(2, static_cast<int>(a.tickers.size())); ++t)
            ticker_parts << ('$' + a.tickers[t]);
        if (a.tickers.size() > 2)
            ticker_parts << QStringLiteral("+%1").arg(a.tickers.size() - 2);
        fr.tickers = ticker_parts.join(' ');

        formatted_rows_.append(std::move(fr));
    }

    recount_unseen();
    endResetModel();
}

void NewsFeedModel::set_clusters(const QVector<services::NewsCluster>& clusters) {
    beginResetModel();
    clusters_ = clusters;
    // Rebuild O(1) lookup cache and incremental unseen counter
    cluster_id_to_row_.clear();
    cluster_id_to_row_.reserve(clusters.size());
    for (int i = 0; i < clusters.size(); ++i)
        cluster_id_to_row_.insert(clusters[i].lead_article.id, i);
    // Both set_wire_articles() and set_clusters() run on every refresh; each used
    // to overwrite the counter with its own tally, so the WIRE view reported the
    // cluster count (and vice versa). Only the active view's rows are counted.
    recount_unseen();
    endResetModel();
}

void NewsFeedModel::recount_unseen() {
    unseen_count_ = 0;
    if (view_mode_ == "WIRE") {
        for (const auto& a : std::as_const(articles_))
            if (!seen_ids_.contains(a.id))
                ++unseen_count_;
    } else {
        for (const auto& c : std::as_const(clusters_))
            if (!seen_ids_.contains(c.lead_article.id))
                ++unseen_count_;
    }
}

void NewsFeedModel::set_view_mode(const QString& mode) {
    if (view_mode_ == mode)
        return;
    beginResetModel();
    view_mode_ = mode;
    recount_unseen();
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
    // An empty model has no valid index range to report.
    if (rowCount() > 0)
        emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {MonitorColorRole});
}

QStringList NewsFeedModel::mark_all_seen() {
    QStringList newly;
    if (view_mode_ == "WIRE") {
        for (const auto& a : std::as_const(articles_))
            if (!seen_ids_.contains(a.id)) {
                seen_ids_.insert(a.id);
                newly.append(a.id);
            }
    } else {
        for (const auto& c : std::as_const(clusters_)) {
            // A story is read as a whole: every member of the cluster.
            for (const auto& a : c.articles)
                if (!seen_ids_.contains(a.id)) {
                    seen_ids_.insert(a.id);
                    newly.append(a.id);
                }
        }
    }
    unseen_count_ = 0;
    if (rowCount() > 0)
        emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {IsNewRole, PulsePhaseRole});
    return newly;
}

void NewsFeedModel::mark_seen(const QString& article_id) {
    if (seen_ids_.contains(article_id))
        return;
    seen_ids_.insert(article_id);
    // Ids restored from the DB (or belonging to the other view) are not rows of
    // the current list, so only a displayed row counts against the tally.
    const auto idx = index_for_article(article_id);
    if (idx.isValid()) {
        if (unseen_count_ > 0)
            --unseen_count_;
        emit dataChanged(idx, idx, {IsNewRole, PulsePhaseRole});
    }
}

int NewsFeedModel::unseen_count() const {
    return unseen_count_; // O(1) — maintained incrementally
}

void NewsFeedModel::set_duplicate_counts(const QHash<QString, int>& counts) {
    duplicate_counts_ = counts;
    if (view_mode_ == "WIRE" && rowCount() > 0)
        emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {DuplicateCountRole, Qt::ToolTipRole});
}

void NewsFeedModel::set_earnings_days(const QHash<QString, int>& days) {
    if (days == earnings_days_)
        return;
    earnings_days_ = days;
    if (rowCount() > 0)
        emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {EarningsBadgeRole});
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
    if (rowCount() > 0)
        emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {HasGeoRole});
}

void NewsFeedModel::advance_pulse() {
    pulse_phase_ = (pulse_phase_ + 1) % 4;
    if (unseen_count_ <= 0 || rowCount() == 0)
        return; // nothing is pulsing
    // One range notification instead of one per unseen row — the view only
    // repaints what is visible, and PulsePhaseRole already answers -1 for rows
    // that have been seen. (This used to emit up to a few hundred separate
    // dataChanged signals every 500 ms.)
    emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {PulsePhaseRole});
}

} // namespace fincept::screens
