#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/news/NewsService.h"

#include <QHideEvent>
#include <QScrollArea>
#include <QShowEvent>
#include <QVector>

namespace fincept::screens::widgets {

/// Market news widget — consumes the DataHub `news:general` topic
/// (RSS-driven via `NewsService`). All fetch cadence is owned by the hub
/// scheduler; this widget only subscribes/unsubscribes on visibility.
class NewsWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit NewsWidget(QWidget* parent = nullptr);

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    void apply_styles();
    void hub_subscribe();
    void hub_unsubscribe();
    void populate(const QVector<services::NewsArticle>& articles);

    QScrollArea* scroll_area_ = nullptr;
    QVBoxLayout* news_layout_ = nullptr;
    QVector<services::NewsArticle> last_articles_; // cached for theme-change re-populate
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
