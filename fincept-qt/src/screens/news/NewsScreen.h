#pragma once
#include <QWidget>
#include <QSplitter>
#include <QVector>
#include <QShowEvent>
#include <QHideEvent>
#include "services/news/NewsService.h"
#include "services/news/NewsClusterService.h"

namespace fincept::screens {

class NewsFilterBar;
class NewsSidebarPanel;
class NewsWirePanel;
class NewsReaderPanel;

/// Main news terminal screen — three-panel Bloomberg-style layout.
class NewsScreen : public QWidget {
    Q_OBJECT
public:
    explicit NewsScreen(QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    void on_refresh();
    void on_filter_changed(const QString& category);
    void on_time_range_changed(const QString& range);
    void on_view_mode_changed(const QString& mode);
    void on_search_changed(const QString& query);
    void on_article_selected(const fincept::services::NewsArticle& article);

private:
    void apply_filters();

    NewsFilterBar*    filter_bar_ = nullptr;
    NewsSidebarPanel* sidebar_    = nullptr;
    NewsWirePanel*    wire_panel_ = nullptr;
    NewsReaderPanel*  reader_     = nullptr;

    // State
    QVector<fincept::services::NewsArticle>  all_articles_;
    QVector<fincept::services::NewsArticle>  filtered_articles_;
    QVector<fincept::services::NewsCluster>  clusters_;
    QString active_filter_ = "ALL";
    QString time_range_    = "24H";
    QString view_mode_     = "WIRE";
    QString search_query_;
    bool    loading_ = false;
};

} // namespace fincept::screens
