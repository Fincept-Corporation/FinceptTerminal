#include "screens/fno/OptionChainTable.h"

#include "screens/fno/OptionChainModel.h"
#include "ui/theme/Theme.h"

#include <QContextMenuEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QStyledItemDelegate>

namespace fincept::screens::fno {

using namespace fincept::ui;

namespace {

class ChainCellDelegate : public QStyledItemDelegate {
  public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
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
            bg = QColor(0xfb, 0xc2, 0x4d); // soft amber tint = ITM call
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
                QColor bar_col(0x16, 0xa3, 0x4a); // green for CE OI
                bar_col.setAlpha(70);
                painter->fillRect(bar, bar_col);
            }
        } else if (col == OptionChainModel::ColPeOi) {
            const double r = index.data(OptionChainModel::PeOiBarRole).toDouble();
            if (r > 0) {
                QRect bar = option.rect;
                bar.setRight(option.rect.left() + int(option.rect.width() * r));
                QColor bar_col(0xdc, 0x26, 0x26); // red for PE OI
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

} // namespace

OptionChainTable::OptionChainTable(QWidget* parent) : QTableView(parent) {
    model_ = new OptionChainModel(this);
    setModel(model_);
    setItemDelegate(new ChainCellDelegate(this));

    setShowGrid(false);
    setAlternatingRowColors(false);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    // Focusable so a trader can arrow between strikes, Return to add a leg to
    // the Builder, and Menu-key to open Buy/Sell — the chain was mouse-only.
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(tr("Option chain"));
    setAccessibleDescription(tr("Strikes with call data on the left and put data on the right. "
                                "Arrow keys move between strikes, Return adds the focused leg to the "
                                "strategy builder, and the context-menu key offers Buy and Sell."));
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    horizontalHeader()->setHighlightSections(false);
    horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    verticalHeader()->setVisible(false);
    verticalHeader()->setDefaultSectionSize(22);

    connect(model_, &QAbstractItemModel::modelReset, this, [this]() {
        for (int row = 0; row < model_->rowCount(); ++row) {
            if (model_->index(row, 0).data(OptionChainModel::IsAtmRole).toBool()) {
                scrollTo(model_->index(row, OptionChainModel::ColStrike), QAbstractItemView::PositionAtCenter);
                break;
            }
        }
    });

    setStyleSheet(
        QStringLiteral(
            "QTableView { background:%1; color:%2; border:1px solid %3; gridline-color:%3; "
            "selection-background-color:%4; }"
            "QHeaderView::section { background:%5; color:%6; border:none; border-bottom:1px solid %3; padding:4px 6px; "
            "                       font-size:9px; font-weight:700; letter-spacing:0.4px; }"
            "QTableView::item { padding:0; }")
            .arg(colors::BG_BASE(), colors::TEXT_PRIMARY(), colors::BORDER_DIM(), colors::BG_HOVER(),
                 colors::BG_RAISED(), colors::TEXT_SECONDARY()));
}

bool OptionChainTable::leg_at(const QModelIndex& idx, qint64& token, double& strike, bool& is_call) const {
    if (!idx.isValid() || !model_)
        return false;
    if (idx.row() < 0 || idx.row() >= model_->rowCount())
        return false;
    const int col = idx.column();
    const bool ce_side = (col >= OptionChainModel::ColCeOi && col <= OptionChainModel::ColCeLtp);
    const bool pe_side = (col >= OptionChainModel::ColPeLtp && col <= OptionChainModel::ColPeOi);
    if (!ce_side && !pe_side)
        return false; // strike pivot — nothing to trade
    is_call = ce_side;
    token = idx.data(is_call ? OptionChainModel::CeTokenRole : OptionChainModel::PeTokenRole).toLongLong();
    strike = idx.data(OptionChainModel::StrikeRole).toDouble();
    // A zero token means the provider gave us no instrument identity for this
    // leg. Every downstream consumer resolves the contract *by token*, so a
    // zero would match the first zero-token row in the chain — a completely
    // different strike. Refuse rather than guess.
    return token != 0;
}

void OptionChainTable::mousePressEvent(QMouseEvent* e) {
    // Left-click an LTP cell adds the leg to the strategy builder. Right-click
    // is handled by contextMenuEvent (Buy/Sell/Add-to-Builder), so it must NOT
    // also fire leg_clicked here.
    if (e->button() == Qt::LeftButton) {
        const QModelIndex idx = indexAt(e->pos());
        const int col = idx.column();
        if (col == OptionChainModel::ColCeLtp || col == OptionChainModel::ColPeLtp) {
            qint64 token = 0;
            double strike = 0;
            bool is_call = false;
            if (leg_at(idx, token, strike, is_call))
                emit leg_clicked(token, strike, is_call, 1);
        }
    }
    QTableView::mousePressEvent(e);
}

void OptionChainTable::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        qint64 token = 0;
        double strike = 0;
        bool is_call = false;
        if (leg_at(currentIndex(), token, strike, is_call)) {
            emit leg_clicked(token, strike, is_call, 1);
            e->accept();
            return;
        }
    }
    QTableView::keyPressEvent(e);
}

void OptionChainTable::contextMenuEvent(QContextMenuEvent* e) {
    // A keyboard-triggered context menu carries no meaningful pos(), so resolve
    // from the focused cell instead — otherwise the Menu key would open a
    // Buy/Sell menu for whatever strike happens to sit under the widget centre.
    const QModelIndex idx =
        (e->reason() == QContextMenuEvent::Keyboard) ? currentIndex() : indexAt(e->pos());
    qint64 token = 0;
    double strike = 0;
    bool is_call = false;
    if (!leg_at(idx, token, strike, is_call)) {
        QTableView::contextMenuEvent(e);
        return;
    }

    // Prefer the broker contract symbol for the menu label; fall back to strike+side.
    QString label = QString::number(strike, 'f', 0) + (is_call ? QStringLiteral(" CE") : QStringLiteral(" PE"));
    const auto& rows = model_->chain().rows;
    if (idx.row() >= 0 && idx.row() < rows.size()) {
        const QString sym = is_call ? rows[idx.row()].ce_symbol : rows[idx.row()].pe_symbol;
        if (!sym.isEmpty())
            label = sym;
    }

    QMenu menu(this);
    QAction* buy = menu.addAction(tr("Buy %1").arg(label));
    QAction* sell = menu.addAction(tr("Sell %1").arg(label));
    menu.addSeparator();
    QAction* to_builder = menu.addAction(tr("Add to Builder"));
    // Keyboard invocation has no cursor position — anchor the menu on the cell.
    const QPoint anchor = (e->reason() == QContextMenuEvent::Keyboard)
                              ? viewport()->mapToGlobal(visualRect(idx).bottomLeft())
                              : e->globalPos();
    QAction* chosen = menu.exec(anchor);
    if (chosen == buy)
        emit order_requested(token, strike, is_call, /*is_buy=*/true);
    else if (chosen == sell)
        emit order_requested(token, strike, is_call, /*is_buy=*/false);
    else if (chosen == to_builder)
        emit leg_clicked(token, strike, is_call, 1);
}

} // namespace fincept::screens::fno
