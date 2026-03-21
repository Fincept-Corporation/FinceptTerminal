#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QVector>
#include "services/news/NewsService.h"
#include "services/news/NewsClusterService.h"

namespace fincept::screens {

class BreakingBanner;

/// Center panel — breaking banner + wire feed / clustered view / by-source view.
class NewsWirePanel : public QWidget {
    Q_OBJECT
public:
    explicit NewsWirePanel(QWidget* parent = nullptr);

    void set_articles(const QVector<fincept::services::NewsArticle>& articles,
                      const QVector<fincept::services::NewsCluster>& clusters);
    void set_view_mode(const QString& mode);
    void set_selected_article(const QString& article_id);

signals:
    void article_selected(const fincept::services::NewsArticle& article);

private:
    void rebuild_wire();
    void rebuild_clustered();
    void rebuild_by_source();
    void clear_feed();

    BreakingBanner* banner_ = nullptr;
    QVBoxLayout*    feed_layout_ = nullptr;
    QWidget*        feed_container_ = nullptr;

    QVector<fincept::services::NewsArticle> articles_;
    QVector<fincept::services::NewsCluster> clusters_;
    QString view_mode_ = "WIRE";
    QString selected_id_;
    int     page_size_ = 50;
    int     visible_count_ = 50;
};

} // namespace fincept::screens
