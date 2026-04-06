#pragma once
#include <QFont>
#include <QFontMetrics>
#include <QStyledItemDelegate>

namespace fincept::screens {

/// Paints news rows directly with QPainter — zero widget allocation per item.
/// Handles WIRE (dense 26px rows) and CLUSTERS (multi-line cards) modes.
class NewsFeedDelegate : public QStyledItemDelegate {
    Q_OBJECT
  public:
    explicit NewsFeedDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

  private:
    void paint_wire_row(QPainter* painter, const QRect& rect, const QModelIndex& index, bool selected,
                        bool hovered) const;
    void paint_cluster_card(QPainter* painter, const QRect& rect, const QModelIndex& index, bool selected,
                            bool hovered) const;

    QFont data_font_;
    QFont bold_font_;
    QFont tiny_font_;
    mutable QFontMetrics data_fm_;
    mutable QFontMetrics bold_fm_;
    mutable QFontMetrics tiny_fm_;
};

} // namespace fincept::screens
