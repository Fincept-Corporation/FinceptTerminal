#pragma once
#include "services/feeds/FeedTypes.h"

#include <QVector>
#include <QWidget>

class QVBoxLayout;
class QScrollArea;

namespace fincept::feeds {

/// Renders feed items as news-style cards in a vertical scroll area.
/// Source dot + relative time, bold title, dim summary; click -> open link.
class FeedCardView : public QWidget {
    Q_OBJECT
  public:
    explicit FeedCardView(QWidget* parent = nullptr);
    void set_items(const QVector<FeedItem>& items);

  private:
    QWidget* build_card(const FeedItem& it);
    QScrollArea* scroll_ = nullptr;
    QVBoxLayout* list_ = nullptr;
};

} // namespace fincept::feeds
