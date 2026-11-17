#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/news/NewsService.h"

#include <QHideEvent>
#include <QJsonObject>
#include <QScrollArea>
#include <QSet>
#include <QShowEvent>
#include <QVector>

class QComboBox;
class QPushButton;

namespace fincept::screens::widgets {

/// Market news widget — consumes the DataHub `news:general` topic (RSS via
/// NewsService; read-only use of NewsArticle). Each headline shows its age,
/// the article's sentiment (bullish / bearish marker), its impact, the tickers
/// it mentions and the source. Filters: feed region (the user's MarketProfile
/// region is offered first), minimum impact, and "my symbols" (articles naming
/// a holding). Config: {"region": "", "impact": 0..2, "mine": false}.
class NewsWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit NewsWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void retranslateUi() override;
    /// Click a headline row to open its article in the browser.
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    void apply_styles();
    void hub_subscribe();
    void hub_unsubscribe();
    void populate(const QVector<services::NewsArticle>& articles);
    void fill_regions();

    QScrollArea* scroll_area_ = nullptr;
    QVBoxLayout* news_layout_ = nullptr;
    QComboBox* region_ = nullptr;
    QComboBox* impact_ = nullptr;
    QPushButton* mine_ = nullptr;
    QVector<services::NewsArticle> last_articles_; // cached for theme-change re-populate
    QSet<QString> held_;
    QString region_filter_;
    int impact_filter_ = 0;
    bool mine_only_ = false;
    bool filling_ = false;
    bool hub_active_ = false;
    bool rows_stale_ = false; // theme changed while hidden: rows are rebuilt on the next show
};

} // namespace fincept::screens::widgets
