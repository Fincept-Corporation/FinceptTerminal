#pragma once
#include <QWidget>
#include "services/news/NewsClusterService.h"

namespace fincept::screens {

/// Cluster card — lead article with expandable related articles.
class NewsClusterCard : public QWidget {
    Q_OBJECT
public:
    explicit NewsClusterCard(const fincept::services::NewsCluster& cluster, QWidget* parent = nullptr);

signals:
    void article_clicked(const fincept::services::NewsArticle& article);

private:
    fincept::services::NewsCluster cluster_;
    bool expanded_ = false;
    QWidget* related_container_ = nullptr;
};

} // namespace fincept::screens
