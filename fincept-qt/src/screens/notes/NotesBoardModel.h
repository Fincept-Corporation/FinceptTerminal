// src/screens/notes/NotesBoardModel.h
//
// The board as a model: lanes are the top-level rows, cards their children. One
// card per note, painted by NotesCardDelegate (no widget per card), so thousands
// of notes cost nothing beyond their data. Moves keep persistent indexes (and so
// the selection) valid; a full rebuild is a reset the screen re-selects after.
//
// Drags carry the cards MIME (note ids + source lane) plus the shared symbol MIME
// of the first card's ticker, so a card dropped on a chart or watchlist opens its
// ticker there.
#pragma once

#include "screens/notes/NotesCardPainter.h"
#include "screens/notes/NotesStore.h"

#include <QAbstractItemModel>
#include <QHash>
#include <QSet>
#include <QStyledItemDelegate>
#include <QVector>

namespace fincept::screens::notes {

/// MIME type of note cards dragged inside the Notes screen.
inline constexpr const char* kNoteCardsMime = "application/x-fincept-note-cards";

class NotesBoardModel : public QAbstractItemModel {
    Q_OBJECT
  public:
    enum Roles { IdRole = Qt::UserRole + 1, LaneKeyRole, IsLaneRole, CountRole };

    struct Lane {
        QString key;
        QString title;   ///< display (translated / the tag's spelling)
        QVector<NoteEntry> cards;
        QStringList tickers;
        int due = 0;
        bool collapsed = false;
        int soft_limit = 0;
        QColor accent;   ///< header tick colour
    };

    explicit NotesBoardModel(QObject* parent = nullptr);

    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QStringList mimeTypes() const override;
    QMimeData* mimeData(const QModelIndexList& indexes) const override;
    Qt::DropActions supportedDropActions() const override;
    Qt::DropActions supportedDragActions() const override;

    /// Replace every lane (a reset).
    void set_lanes(QVector<Lane> lanes);
    int lane_count() const { return static_cast<int>(lanes_.size()); }
    const Lane& lane(int i) const { return lanes_.at(i); }
    int lane_of_key(const QString& key) const;
    QModelIndex lane_index(int lane) const;
    void set_lane_collapsed(int lane, bool collapsed);

    const NoteEntry* card(int lane, int row) const;
    const NoteEntry* card(const QModelIndex& index) const;
    QModelIndex card_index(int id) const;
    /// (lane, row) of a note, (-1, -1) when it is not on the board.
    QPair<int, int> position_of(int id) const;
    int card_count() const { return card_count_; }

    /// Replace a card's entry in place (same lane, same position); false when it is not shown.
    bool update_entry(const NoteEntry& e);
    /// Move cards (in the given order) to `to_lane` before row `to_row` of that lane
    /// as it is now; persistent indexes follow the cards.
    void move_cards(const QVector<int>& ids, int to_lane, int to_row);
    void remove_ids(const QSet<int>& ids);

    void set_highlight(const QStringList& terms);
    const QStringList& highlight() const { return highlight_; }
    /// Repaint every card (quotes or the theme changed).
    void refresh_cards();

  private:
    void reindex();

    QVector<Lane> lanes_;
    QHash<int, QPair<int, int>> pos_;
    QStringList highlight_;
    int card_count_ = 0;
};

/// Paints the cards of NotesBoardModel (lanes are painted by the view).
class NotesCardDelegate : public QStyledItemDelegate {
    Q_OBJECT
  public:
    explicit NotesCardDelegate(QObject* parent = nullptr);
    void set_metrics(const CardMetrics& m) { metrics_ = m; }
    const CardMetrics& metrics() const { return metrics_; }
    void set_quotes(const QuoteBook* quotes) { quotes_ = quotes; }
    void set_show_status(bool on) { show_status_ = on; }
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    /// Paint `index`'s card with explicit state (drag pixmap, peek).
    void paint_state(QPainter* painter, const QRect& rect, const QModelIndex& index, const CardState& st) const;

  private:
    CardMetrics metrics_;
    const QuoteBook* quotes_ = nullptr;
    bool show_status_ = false;
};

} // namespace fincept::screens::notes
