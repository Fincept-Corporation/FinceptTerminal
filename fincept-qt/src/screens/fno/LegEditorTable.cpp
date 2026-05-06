#include "screens/fno/LegEditorTable.h"

#include "ui/theme/Theme.h"

#include <QHeaderView>
#include <QMouseEvent>

#include <cmath>

namespace fincept::screens::fno {

using fincept::services::options::StrategyLeg;
using fincept::trading::InstrumentType;
using namespace fincept::ui;

namespace {

QString type_str(InstrumentType t) {
    switch (t) {
        case InstrumentType::CE:    return "CE";
        case InstrumentType::PE:    return "PE";
        case InstrumentType::FUT:   return "FUT";
        default:                    return "—";
    }
}

}  // namespace

// ── LegEditorModel ─────────────────────────────────────────────────────────

LegEditorModel::LegEditorModel(QObject* parent) : QAbstractTableModel(parent) {}

int LegEditorModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid())
        return 0;
    return legs_.size();
}

int LegEditorModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid())
        return 0;
    return ColCount;
}

QVariant LegEditorModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= legs_.size())
        return {};
    const StrategyLeg& leg = legs_.at(index.row());
    const int col = index.column();

    if (role == Qt::CheckStateRole && col == ColActive)
        return leg.is_active ? Qt::Checked : Qt::Unchecked;

    if (role == Qt::TextAlignmentRole) {
        if (col == ColStrike || col == ColLots || col == ColEntry || col == ColIv)
            return int(Qt::AlignRight | Qt::AlignVCenter);
        return int(Qt::AlignCenter | Qt::AlignVCenter);
    }

    if (role == Qt::ForegroundRole) {
        if (col == ColBuySell)
            return leg.lots >= 0 ? QColor(colors::POSITIVE()) : QColor(colors::NEGATIVE());
        if (col == ColDelete)
            return QColor(colors::TEXT_TERTIARY());
    }

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (col) {
            case ColActive:   return QVariant();  // checkstate-only
            case ColBuySell:  return leg.lots >= 0 ? "BUY" : "SELL";
            case ColType:     return type_str(leg.type);
            case ColStrike:   return QString::number(leg.strike, 'f', leg.strike < 100 ? 2 : 0);
            case ColLots:     return leg.lots;
            case ColEntry:    return QString::number(leg.entry_price, 'f', 2);
            case ColIv:       return leg.iv_at_entry > 0
                                  ? QString::number(leg.iv_at_entry * 100.0, 'f', 2) + "%"
                                  : "—";
            case ColDelete:   return "✕";
        }
    }
    return {};
}

bool LegEditorModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || index.row() < 0 || index.row() >= legs_.size())
        return false;
    StrategyLeg& leg = legs_[index.row()];
    const int col = index.column();
    bool mutated = false;

    if (role == Qt::CheckStateRole && col == ColActive) {
        const bool active = value.toInt() == Qt::Checked;
        if (active != leg.is_active) {
            leg.is_active = active;
            mutated = true;
        }
    } else if (role == Qt::EditRole) {
        if (col == ColLots) {
            const int v = value.toInt();
            if (v != 0 && v != leg.lots) {
                leg.lots = v;
                mutated = true;
            }
        } else if (col == ColEntry) {
            bool ok = false;
            const double v = value.toDouble(&ok);
            if (ok && v >= 0 && std::abs(v - leg.entry_price) > 1e-6) {
                leg.entry_price = v;
                mutated = true;
            }
        }
    }
    if (mutated) {
        const QModelIndex left = this->index(index.row(), 0);
        const QModelIndex right = this->index(index.row(), ColCount - 1);
        emit dataChanged(left, right);
        emit legs_changed();
    }
    return mutated;
}

Qt::ItemFlags LegEditorModel::flags(const QModelIndex& index) const {
    if (!index.isValid())
        return Qt::NoItemFlags;
    Qt::ItemFlags f = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    const int col = index.column();
    if (col == ColActive)
        f |= Qt::ItemIsUserCheckable;
    if (col == ColLots || col == ColEntry)
        f |= Qt::ItemIsEditable;
    return f;
}

QVariant LegEditorModel::headerData(int section, Qt::Orientation orient, int role) const {
    if (orient != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    static const char* kHeaders[ColCount] = {"On", "B/S", "Type", "Strike", "Lots", "Entry", "IV", ""};
    if (section < 0 || section >= ColCount)
        return {};
    return QString::fromLatin1(kHeaders[section]);
}

void LegEditorModel::set_legs(const QVector<StrategyLeg>& legs) {
    beginResetModel();
    legs_ = legs;
    endResetModel();
    emit legs_changed();
}

void LegEditorModel::append_leg(const StrategyLeg& leg) {
    beginInsertRows({}, legs_.size(), legs_.size());
    legs_.append(leg);
    endInsertRows();
    emit legs_changed();
}

void LegEditorModel::remove_row(int row) {
    if (row < 0 || row >= legs_.size())
        return;
    beginRemoveRows({}, row, row);
    legs_.removeAt(row);
    endRemoveRows();
    emit legs_changed();
}

// ── LegEditorTable ─────────────────────────────────────────────────────────

LegEditorTable::LegEditorTable(QWidget* parent) : QTableView(parent) {
    model_ = new LegEditorModel(this);
    setModel(model_);

    setShowGrid(false);
    setAlternatingRowColors(false);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                    QAbstractItemView::SelectedClicked);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    horizontalHeader()->setHighlightSections(false);
    horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    verticalHeader()->setVisible(false);
    verticalHeader()->setDefaultSectionSize(24);

    setStyleSheet(QStringLiteral(
        "QTableView { background:%1; color:%2; border:1px solid %3; gridline-color:%3; selection-background-color:%4; }"
        "QHeaderView::section { background:%5; color:%6; border:none; border-bottom:1px solid %3; padding:4px 6px; "
        "                       font-size:9px; font-weight:700; letter-spacing:0.4px; }"
        "QTableView::item { padding:0 6px; }")
                      .arg(colors::BG_BASE(), colors::TEXT_PRIMARY(), colors::BORDER_DIM(),
                           colors::BG_HOVER(), colors::BG_RAISED(), colors::TEXT_SECONDARY()));
}

void LegEditorTable::mousePressEvent(QMouseEvent* event) {
    const QModelIndex idx = indexAt(event->pos());
    if (idx.isValid() && idx.column() == LegEditorModel::ColDelete && event->button() == Qt::LeftButton) {
        model_->remove_row(idx.row());
        return;
    }
    QTableView::mousePressEvent(event);
}

} // namespace fincept::screens::fno
