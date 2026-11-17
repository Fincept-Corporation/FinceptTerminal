// SearchModel.cpp — see SearchModel.h.
#include "ui/navigation/search/SearchModel.h"

#include "core/agent/AgentRiskMarkers.h"

namespace fincept::ui::search {

SearchModel::SearchModel(QObject* parent) : QAbstractListModel(parent) {}

int SearchModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

Qt::ItemFlags SearchModel::flags(const QModelIndex& index) const {
    const SearchRow* r = row(index.row());
    if (!r)
        return Qt::NoItemFlags;
    if (r->selectable())
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    return Qt::ItemIsEnabled; // headings / loading rows: shown, never current
}

QVariant SearchModel::data(const QModelIndex& index, int role) const {
    const SearchRow* r = row(index.row());
    if (!r)
        return {};
    switch (role) {
        case RowRole:
            return QVariant::fromValue(static_cast<const void*>(r));
        case KindRole:
            return static_cast<int>(r->kind);
        case KeyRole:
            return r->key;
        case GroupRole:
            return r->group;
        case Qt::DisplayRole: {
            QString text = r->lead.isEmpty() ? r->title : r->lead + QLatin1Char(' ') + r->title;
            if (!r->context.isEmpty())
                text = r->context + QStringLiteral(" › ") + text;
            return text;
        }
        case Qt::AccessibleTextRole: {
            if (r->kind == SearchRow::Kind::Heading)
                return tr("%1, %n result(s)", nullptr, r->count).arg(r->title);
            QString text = r->lead.isEmpty() ? r->title : r->lead + QLatin1Char(' ') + r->title;
            if (!r->context.isEmpty())
                text = r->context + QStringLiteral(", ") + text;
            return text;
        }
        case Qt::AccessibleDescriptionRole: {
            QStringList parts;
            if (!r->chip.isEmpty())
                parts << r->chip;
            if (!r->subtitle.isEmpty())
                parts << r->subtitle;
            if (!r->actions.isEmpty())
                parts << tr("More actions: %1 (Tab)").arg(r->actions.join(QStringLiteral(", ")));
            if (r->pinned)
                parts << tr("Pinned");
            return parts.join(QStringLiteral(". "));
        }
        case Qt::ToolTipRole:
            return r->kind == SearchRow::Kind::Item ? r->subtitle : QString();
        case core::agent::kAgentRiskRole:
            return r->agent_risk.isEmpty() ? QVariant() : QVariant(r->agent_risk);
        default:
            return {};
    }
}

int SearchModel::row_of(const QString& key) const {
    for (int i = 0; i < rows_.size(); ++i)
        if (rows_[i].key == key)
            return i;
    return -1;
}

int SearchModel::next_selectable(int from, int step) const {
    for (int i = from; i >= 0 && i < rows_.size(); i += step)
        if (rows_[i].selectable())
            return i;
    return -1;
}

void SearchModel::set_rows(const QList<SearchRow>& rows) {
    QStringList old_keys;
    old_keys.reserve(rows_.size());
    for (const SearchRow& r : rows_)
        old_keys.append(r.key);
    QStringList new_keys;
    new_keys.reserve(rows.size());
    for (const SearchRow& r : rows)
        new_keys.append(r.key);

    if (old_keys == new_keys) {
        for (int i = 0; i < rows.size(); ++i) {
            if (!rows_[i].same_content(rows[i])) {
                rows_[i] = rows[i];
                emit dataChanged(index(i), index(i));
            }
        }
        return;
    }

    const QList<DiffOp> ops = diff_keys(old_keys, new_keys);
    for (const DiffOp& op : ops) {
        if (op.type == DiffOp::Type::Remove) {
            beginRemoveRows({}, op.first, op.first + op.count - 1);
            rows_.remove(op.first, op.count);
            endRemoveRows();
        } else {
            beginInsertRows({}, op.first, op.first + op.count - 1);
            for (int k = 0; k < op.count; ++k)
                rows_.insert(op.first + k, rows[op.source + k]);
            endInsertRows();
        }
    }
    // Kept rows: same key, maybe new text (highlight, count, subtitle).
    for (int i = 0; i < rows.size() && i < rows_.size(); ++i) {
        if (!rows_[i].same_content(rows[i])) {
            rows_[i] = rows[i];
            emit dataChanged(index(i), index(i));
        }
    }
}

} // namespace fincept::ui::search
