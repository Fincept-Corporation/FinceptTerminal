#pragma once
// SearchModel — the popup's rows. set_rows() applies a key diff (insert / remove /
// dataChanged) instead of a reset, so the list never flickers and the current row
// stays on the same result when a slower group arrives. Roles carry what the
// delegate paints, the accessible text and the agent-risk kind.

#include "ui/navigation/search/SearchRow.h"

#include <QAbstractListModel>
#include <QList>

namespace fincept::ui::search {

class SearchModel : public QAbstractListModel {
    Q_OBJECT
  public:
    enum Role {
        RowRole = Qt::UserRole + 1, ///< const SearchRow* (valid until the next set_rows)
        KindRole,
        KeyRole,
        GroupRole,
    };

    explicit SearchModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void set_rows(const QList<SearchRow>& rows);
    const SearchRow* row(int i) const { return i >= 0 && i < rows_.size() ? &rows_[i] : nullptr; }
    int row_of(const QString& key) const;
    int size() const { return static_cast<int>(rows_.size()); }
    /// First selectable row at or after `from` (-1 = none).
    int next_selectable(int from, int step = 1) const;

  private:
    QList<SearchRow> rows_;
};

} // namespace fincept::ui::search
