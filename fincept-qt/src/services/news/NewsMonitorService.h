#pragma once
#include <QObject>
#include <QVector>
#include <QMap>
#include <QString>
#include "services/news/NewsService.h"

namespace fincept::services {

struct NewsMonitor {
    QString     id;
    QString     label;
    QStringList keywords;
    QString     color;
    bool        enabled = true;
};

const QStringList& monitor_colors();

class NewsMonitorService : public QObject {
    Q_OBJECT
public:
    static NewsMonitorService& instance();

    QVector<NewsMonitor> get_monitors() const;
    void add_monitor(const QString& label, const QStringList& keywords, const QString& color = {});
    void update_monitor(const NewsMonitor& monitor);
    void delete_monitor(const QString& id);
    void toggle_monitor(const QString& id);

    /// Scan articles against all enabled monitors; returns monitor_id → matching articles.
    QMap<QString, QVector<NewsArticle>> scan_monitors(
        const QVector<NewsMonitor>& monitors,
        const QVector<NewsArticle>& articles) const;

private:
    NewsMonitorService();

    QVector<NewsMonitor> monitors_;
    void load_from_db();
    void save_to_db(const NewsMonitor& m);
    void remove_from_db(const QString& id);
};

} // namespace fincept::services
