// src/screens/notes/NotesModels.h
//
// The LIST view of the Notes board: a dense, sortable table in the Watchlist
// style — one row per note (title, tickers, quote of the first ticker with the
// tick flash, status, category, priority, tags, review date, edited, created).
// Rows come from the same board build as the lanes (filter + search applied), in
// board order; the header sorts through a QSortFilterProxyModel on SortRole.
// The title is renamed in place (F2, or a click on the selected title).
// Cells are painted by ui::FlashCellDelegate (FlashRole on the quote column).
#pragma once

#include "screens/notes/NotesCardPainter.h"
#include "screens/notes/NotesStore.h"

#include <QAbstractTableModel>
#include <QElapsedTimer>
#include <QHash>
#include <QSet>
#include <QVector>

namespace fincept::screens::notes {

class NotesTableModel : public QAbstractTableModel {
    Q_OBJECT
  public:
    enum Column { Title = 0, Tickers, Quote, Status, Category, Priority, Tags, Review, Edited, Created, ColumnCount };
    enum Roles { IdRole = Qt::UserRole + 1, SortRole, FlashRole };

    explicit NotesTableModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    void set_rows(QVector<NoteEntry> rows);
    bool update_entry(const NoteEntry& e);
    void remove_ids(const QSet<int>& ids);
    const NoteEntry* entry(int row) const;
    int row_of(int id) const;
    int row_count() const { return static_cast<int>(rows_.size()); }

    void set_quotes(const QuoteBook* quotes) { quotes_ = quotes; }
    /// New quotes for `symbols`: their rows' quote cells flash. True while a flash runs.
    bool quotes_changed(const QStringList& symbols);
    /// Advance the flash fade; false when no flash is left.
    bool tick_flash();
    /// Re-read every cell (theme, language, day change).
    void refresh_all();

  signals:
    void rename_requested(int id, const QString& title);

  private:
    void reindex();

    QVector<NoteEntry> rows_;
    QHash<int, int> row_by_id_;
    QHash<QString, QVector<int>> rows_by_ticker_; ///< first ticker -> rows
    const QuoteBook* quotes_ = nullptr;
    QHash<int, QPair<qint64, int>> flash_;        ///< row -> (start ms, direction)
    QElapsedTimer clock_;
};

} // namespace fincept::screens::notes
