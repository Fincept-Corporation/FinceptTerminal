#include "screens/fno/OptionChainTable.h"

#include "screens/fno/OptionChainModel.h"
#include "ui/theme/Theme.h"

#include <QHeaderView>
#include <QMouseEvent>
#include <QPainter>
#include <QStyledItemDelegate>

namespace fincept::screens::fno {

using namespace fincept::ui;

namespace {

class ChainCellDelegate : public QStyledItemDelegate {
  public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        painter->save();

        // ── Background — ATM > ITM > base ──────────────────────────────────
        const bool is_atm = index.data(OptionChainModel::IsAtmRole).toBool();
        const bool is_call_itm = index.data(OptionChainModel::IsCallItmRole).toBool();
        const bool is_put_itm = index.data(OptionChainModel::IsPutItmRole).toBool();
        const int col = index.column();
        const bool is_ce_side = (col >= OptionChainModel::ColCeOi && col <= OptionChainModel::ColCeLtp);
        const bool is_pe_side = (col >= OptionChainModel::ColPeLtp && col <= OptionChainModel::ColPeOi);

        QColor bg = QColor(colors::BG_BASE());
        if (is_atm) {
            bg = QColor(colors::AMBER());
            bg.setAlpha(45);
        } else if (is_ce_side && is_call_itm) {
            bg = QColor(0xfb, 0xc2, 0x4d);  // soft amber tint = ITM call
            bg.setAlpha(30);
        } else if (is_pe_side && is_put_itm) {
            bg = QColor(0xfb, 0xc2, 0x4d);
            bg.setAlpha(30);
        }
        painter->fillRect(option.rect, bg);

        // ── OI bars — drawn behind the OI columns ──────────────────────────
        if (col == OptionChainModel::ColCeOi) {
            const double r = index.data(OptionChainModel::CeOiBarRole).toDouble();
            if (r > 0) {
                QRect bar = option.rect;
                bar.setLeft(option.rect.right() - int(option.rect.width() * r));
                QColor bar_col(0x16, 0xa3, 0x4a);  // green for CE OI
                bar_col.setAlpha(70);
                painter->fillRect(bar, bar_col);
            }
        } else if (col == OptionChainModel::ColPeOi) {
            const double r = index.data(OptionChainModel::PeOiBarRole).toDouble();
            if (r > 0) {
                QRect bar = option.rect;
                bar.setRight(option.rect.left() + int(option.rect.width() * r));
                QColor bar_col(0xdc, 0x26, 0x26);  // red for PE OI
                bar_col.setAlpha(70);
                painter->fillRect(bar, bar_col);
            }
        }

        // ── Strike pivot — bold center text on a slightly raised band ──────
        if (col == OptionChainModel::ColStrike) {
            QColor strike_bg = is_atm ? QColor(colors::AMBER()) : QColor(colors::BG_RAISED());
            if (!is_atm)
                strike_bg.setAlpha(220);
            painter->fillRect(option.rect, strike_bg);
            QColor txt_col = is_atm ? QColor(colors::BG_BASE()) : QColor(colors::TEXT_PRIMARY());
            QFont f = option.font;
            f.setBold(true);
            painter->setFont(f);
            painter->setPen(txt_col);
            painter->drawText(option.rect, Qt::AlignCenter, index.data(Qt::DisplayRole).toString());
            painter->restore();
            return;
        }

        // ── Default text rendering ─────────────────────────────────────────
        const QVariant fg = index.data(Qt::ForegroundRole);
        QColor pen = fg.isValid() ? fg.value<QColor>() : QColor(colors::TEXT_PRIMARY());
        painter->setPen(pen);
        const Qt::Alignment align = Qt::Alignment(index.data(Qt::TextAlignmentRole).toInt());
        QRect text_rect = option.rect.adjusted(6, 0, -6, 0);
        painter->drawText(text_rect, align, index.data(Qt::DisplayRole).toString());

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QSize s = QStyledItemDelegate::sizeHint(option, index);
        s.setHeight(22);
        return s;
    }
};

}  // namespace

OptionChainTable::OptionChainTable(QWidget* parent) : QTableView(parent) {
    model_ = new OptionChainModel(this);
    setModel(model_);
    setItemDelegate(new ChainCellDelegate(this));

    setShowGrid(false);
    setAlternatingRowColors(false);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setFocusPolicy(Qt::NoFocus);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    horizontalHeader()->setHighlightSections(false);
    horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    verticalHeader()->setVisible(false);
    verticalHeader()->setDefaultSectionSize(22);

    setStyleSheet(QStringLiteral(
        "QTableView { background:%1; color:%2; border:1px solid %3; gridline-color:%3; selection-background-color:%4; }"
        "QHeaderView::section { background:%5; color:%6; border:none; border-bottom:1px solid %3; padding:4px 6px; "
        "                       font-size:9px; font-weight:700; letter-spacing:0.4px; }"
        "QTableView::item { padding:0; }")
                      .arg(colors::BG_BASE(), colors::TEXT_PRIMARY(), colors::BORDER_DIM(),
                           colors::BG_HOVER(), colors::BG_RAISED(), colors::TEXT_SECONDARY()));
}

void OptionChainTable::mousePressEvent(QMouseEvent* e) {
    QModelIndex idx = indexAt(e->pos());
    if (idx.isValid()) {
        const int col = idx.column();
        const bool is_ce_ltp = (col == OptionChainModel::ColCeLtp);
        const bool is_pe_ltp = (col == OptionChainModel::ColPeLtp);
        if (is_ce_ltp || is_pe_ltp) {
            const qint64 token = is_ce_ltp ? idx.data(OptionChainModel::CeTokenRole).toLongLong()
                                           : idx.data(OptionChainModel::PeTokenRole).toLongLong();
            const double strike = idx.data(OptionChainModel::StrikeRole).toDouble();
            const int lots = (e->button() == Qt::RightButton) ? -1 : 1;
            if (token != 0)
                emit leg_clicked(token, strike, is_ce_ltp, lots);
        }
    }
    QTableView::mousePressEvent(e);
}

} // namespace fincept::screens::fno
