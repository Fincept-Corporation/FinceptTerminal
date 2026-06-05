#pragma once
#include "services/feeds/FeedTypes.h"

#include <QVector>
#include <QWidget>

namespace fincept::ui {
class DataTable;
}

namespace fincept::feeds {

/// Renders feed items as a sortable table. Columns: Time, Title, + any mapped
/// extra fields (union across items, stable order). Double-click a row -> open link.
class FeedTableView : public QWidget {
    Q_OBJECT
  public:
    explicit FeedTableView(QWidget* parent = nullptr);
    void set_items(const QVector<FeedItem>& items);

  private:
    ui::DataTable* table_ = nullptr;
    QVector<FeedItem> items_;
};

} // namespace fincept::feeds
