#include "screens/fno/LegEditorTable.h"

#include "ui/theme/StyleSheets.h"
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

const fincept::services::options::OptionChainRow*
find_row_in_chain(const fincept::services::options::OptionChain& chain, double strike) {
    for (const auto& row : chain.rows) {
        if (std::abs(row.strike - strike) < 1e-6)
            return &row;
    }
    return nullptr;
}

double lookup_ltp(const StrategyLeg& leg,
                  const fincept::services::options::OptionChain& chain) {
    const auto* row = find_row_in_chain(chain, leg.strike);
    if (!row)
        return 0;
    const bool is_call = (leg.type == InstrumentType::CE);
    return is_call ? row->ce_quote.ltp : row->pe_quote.ltp;
}

double lookup_delta(const StrategyLeg& leg,
                    const fincept::services::options::OptionChain& chain) {
    const auto* row = find_row_in_chain(chain, leg.strike);
    if (!row)
        return 0;
    const bool is_call = (leg.type == InstrumentType::CE);
    const auto& g = is_call ? row->ce_greeks : row->pe_greeks;
    return g.valid ? g.delta : 0;
}

}  // namespace

// ── LegEditorModel ─────────────────────────────────────────────────────────

LegEditorModel::LegEditorModel(QObject* parent) : QAbstractTableModel(parent) {}

double LegEditorModel::resolve_ltp(const StrategyLeg& leg) const {
    return lookup_ltp(leg, chain_);
}

double LegEditorModel::resolve_delta(const StrategyLeg& leg) const {
    return lookup_delta(leg, chain_);
}

double LegEditorModel::resolve_pnl(const StrategyLeg& leg) const {
    double ltp = resolve_ltp(leg);
    if (ltp <= 0)
        return 0;
    int lot_size = leg.lot_size > 0 ? leg.lot_size : 1;
    return (ltp - leg.entry_price) * double(leg.lots) * double(lot_size);
}

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
        if (col == ColStrike || col == ColLots || col == ColEntry || col == ColIv
            || col == ColLtp || col == ColDelta || col == ColPnl)
            return int(Qt::AlignRight | Qt::AlignVCenter);
        return int(Qt::AlignCenter | Qt::AlignVCenter);
    }

    if (role == Qt::ForegroundRole) {
        if (col == ColBuySell)
            return leg.lots >= 0 ? QColor(colors::POSITIVE()) : QColor(colors::NEGATIVE());
        if (col == ColDelete)
            return QColor(colors::TEXT_TERTIARY());
        if (col == ColLtp)
            return QColor(colors::TEXT_PRIMARY());
        if (col == ColDelta) {
            double delta = resolve_delta(leg);
            if (delta != 0)
                return delta > 0 ? QColor(colors::POSITIVE()) : QColor(colors::NEGATIVE());
            return QColor(colors::TEXT_PRIMARY());
        }
        if (col == ColPnl) {
            double pnl = resolve_pnl(leg);
            if (pnl != 0)
                return pnl > 0 ? QColor(colors::POSITIVE()) : QColor(colors::NEGATIVE());
            return QColor(colors::TEXT_PRIMARY());
        }
    }

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (col) {
            case ColActive:   return QVariant();  // checkstate-only
            case ColBuySell:  return leg.lots >= 0 ? tr("BUY") : tr("SELL");
            case ColType:     return type_str(leg.type);
            case ColStrike:   return QString::number(leg.strike, 'f', leg.strike < 100 ? 2 : 0);
            case ColLots:     return leg.lots;
            case ColEntry:    return QString::number(leg.entry_price, 'f', 2);
            case ColIv:       return leg.iv_at_entry > 0
                                  ? QString::number(leg.iv_at_entry * 100.0, 'f', 2) + "%"
                                  : QString::fromUtf8("—");
            case ColLtp: {
                double ltp = resolve_ltp(leg);
                return ltp > 0 ? QString::number(ltp, 'f', 2) : QString::fromUtf8("—");
            }
            case ColDelta: {
                double delta = resolve_delta(leg);
                return delta != 0 ? QString::number(delta, 'f', 2) : QString::fromUtf8("—");
            }
            case ColPnl: {
                double pnl = resolve_pnl(leg);
                if (pnl == 0 && resolve_ltp(leg) <= 0)
                    return QString::fromUtf8("—");
                return QString::number(pnl, 'f', 2);
            }
            case ColDelete:   return QString::fromUtf8("✕");
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
    // tr() per-call — owning QHeaderView re-polls on QEvent::LanguageChange.
    const QString headers[ColCount] = {
        tr("On"), tr("B/S"), tr("Type"), tr("Strike"), tr("Lots"), tr("Entry"),
        tr("IV"), tr("LTP"), tr("Delta"), tr("P&L"), QString()
    };
    if (section < 0 || section >= ColCount)
        return {};
    return headers[section];
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

void LegEditorModel::set_chain(const fincept::services::options::OptionChain& chain) {
    chain_ = chain;
    if (legs_.isEmpty())
        return;
    const QModelIndex top_left = index(0, ColLtp);
    const QModelIndex bottom_right = index(legs_.size() - 1, ColPnl);
    emit dataChanged(top_left, bottom_right, {Qt::DisplayRole, Qt::ForegroundRole});
}

// ── LegEditorTable ─────────────────────────────────────────────────────────

LegEditorTable::LegEditorTable(QWidget* parent) : QTableView(parent) {
    model_ = new LegEditorModel(this);
    setModel(model_);

    setShowGrid(false);
    setAlternatingRowColors(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                    QAbstractItemView::SelectedClicked);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    horizontalHeader()->setStretchLastSection(true);
    horizontalHeader()->setHighlightSections(false);
    horizontalHeader()->setMinimumSectionSize(28);
    horizontalHeader()->resizeSection(LegEditorModel::ColActive, 32);
    horizontalHeader()->resizeSection(LegEditorModel::ColBuySell, 40);
    horizontalHeader()->resizeSection(LegEditorModel::ColType, 36);
    horizontalHeader()->resizeSection(LegEditorModel::ColStrike, 65);
    horizontalHeader()->resizeSection(LegEditorModel::ColLots, 45);
    horizontalHeader()->resizeSection(LegEditorModel::ColEntry, 65);
    horizontalHeader()->resizeSection(LegEditorModel::ColIv, 52);
    horizontalHeader()->resizeSection(LegEditorModel::ColLtp, 60);
    horizontalHeader()->resizeSection(LegEditorModel::ColDelta, 52);
    horizontalHeader()->resizeSection(LegEditorModel::ColPnl, 60);
    horizontalHeader()->resizeSection(LegEditorModel::ColDelete, 28);
    horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    verticalHeader()->setVisible(false);
    verticalHeader()->setDefaultSectionSize(24);

    setStyleSheet(styles::fno_dense_table());
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
