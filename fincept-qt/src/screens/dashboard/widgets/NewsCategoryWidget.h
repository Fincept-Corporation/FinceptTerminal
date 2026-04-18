#pragma once
// NewsCategoryWidget — category-filtered news headlines. Subscribes to
// `news:category:<category>` (QVector<NewsArticle>) and renders the most
// recent N entries.
//
// Config (persisted):
//   { "category": "<category-id>", "max_rows": <int> }

#include "screens/dashboard/widgets/BaseWidget.h"

#include <QString>

class QListWidget;
class QLabel;

namespace fincept::screens::widgets {

class NewsCategoryWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit NewsCategoryWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    QDialog* make_config_dialog(QWidget* parent) override;

  private:
    void apply_styles();
    void on_articles(const QVariant& v);
    void hub_resubscribe();
    void hub_unsubscribe_all();

    QString category_ = "markets";
    int max_rows_ = 12;
    QListWidget* list_ = nullptr;
    QLabel* badge_ = nullptr;
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
