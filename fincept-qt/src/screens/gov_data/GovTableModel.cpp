// src/screens/gov_data/GovTableModel.cpp
#include "screens/gov_data/GovTableModel.h"

#include <QBrush>

namespace fincept::screens {

GovTableModel::GovTableModel(QObject* parent) : QAbstractTableModel(parent) {}

void GovTableModel::set_columns(const QList<Column>& columns) {
    beginResetModel();
    columns_ = columns;
    endResetModel();
}

void GovTableModel::set_headers(const QStringList& headers) {
    headers_ = headers;
    if (columns_.isEmpty())
        return;
    emit headerDataChanged(Qt::Horizontal, 0, int(columns_.size()) - 1);
    // A language change: cells with translated text ("↗ OPEN") repaint (the view redraws the visible ones).
    if (!rows_.isEmpty())
        emit dataChanged(index(0, 0), index(int(rows_.size()) - 1, int(columns_.size()) - 1), {Qt::DisplayRole});
}

void GovTableModel::set_ink(InkFn ink) {
    ink_ = std::move(ink);
    refresh_ink();
}

void GovTableModel::set_rows(const QJsonArray& rows) {
    beginResetModel();
    rows_ = rows;
    endResetModel();
}

void GovTableModel::clear_rows() {
    if (rows_.isEmpty())
        return;
    beginResetModel();
    rows_ = QJsonArray();
    endResetModel();
}

QJsonObject GovTableModel::row(int r) const {
    if (r < 0 || r >= rows_.size())
        return {};
    return rows_.at(r).toObject();
}

void GovTableModel::refresh_ink() {
    if (rows_.isEmpty() || columns_.isEmpty())
        return;
    emit dataChanged(index(0, 0), index(int(rows_.size()) - 1, int(columns_.size()) - 1), {Qt::ForegroundRole});
}

int GovTableModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : int(rows_.size());
}

int GovTableModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : int(columns_.size());
}

QVariant GovTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= rows_.size() || index.column() >= columns_.size())
        return {};
    const Column& col = columns_.at(index.column());
    switch (role) {
        case Qt::DisplayRole:
        case Qt::AccessibleTextRole:
            return col.text ? col.text(rows_.at(index.row()).toObject()) : QString();
        case Qt::ToolTipRole: {
            if (!col.tooltip)
                return {};
            const QString tip = col.tooltip(rows_.at(index.row()).toObject());
            return tip.isEmpty() ? QVariant() : QVariant(tip);
        }
        case Qt::TextAlignmentRole:
            return QVariant::fromValue(col.align);
        case Qt::ForegroundRole: {
            if (col.ink == Ink::Default || !ink_)
                return {};
            if (col.ink_if && !col.ink_if(rows_.at(index.row()).toObject()))
                return {};
            return QBrush(ink_(col.ink));
        }
        case ExportRole: {
            const QJsonObject r = rows_.at(index.row()).toObject();
            if (col.export_text)
                return col.export_text(r);
            return col.text ? col.text(r) : QString();
        }
        default:
            return {};
    }
}

QVariant GovTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section >= 0 && section < headers_.size())
        return headers_.at(section);
    return QAbstractTableModel::headerData(section, orientation, role);
}

Qt::ItemFlags GovTableModel::flags(const QModelIndex& index) const {
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

} // namespace fincept::screens
