// src/screens/notes/NotesModels.cpp
#include "screens/notes/NotesModels.h"

#include "screens/notes/NotesEditor.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"

#include <QDateTime>

#include <algorithm>
#include <limits>

namespace fincept::screens::notes {

namespace colors = fincept::ui::colors;

namespace {

constexpr qint64 kFlashMs = 1000;

QString ntm_date(qint64 ms) {
    if (ms <= 0)
        return {};
    const QDateTime t = QDateTime::fromMSecsSinceEpoch(ms).toLocalTime();
    return t.date() == QDate::currentDate() ? t.toString(QStringLiteral("HH:mm"))
                                            : t.toString(QStringLiteral("yyyy-MM-dd"));
}

} // namespace

NotesTableModel::NotesTableModel(QObject* parent) : QAbstractTableModel(parent) {
    clock_.start();
}

int NotesTableModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

int NotesTableModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : ColumnCount;
}

const NoteEntry* NotesTableModel::entry(int row) const {
    return (row >= 0 && row < rows_.size()) ? &rows_.at(row) : nullptr;
}

int NotesTableModel::row_of(int id) const {
    return row_by_id_.value(id, -1);
}

QVariant NotesTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal)
        return {};
    if (role == Qt::TextAlignmentRole)
        return QVariant::fromValue(section == Quote ? Qt::AlignRight | Qt::AlignVCenter : Qt::AlignLeft | Qt::AlignVCenter);
    if (role != Qt::DisplayRole && role != Qt::ToolTipRole)
        return {};
    switch (section) {
    case Title:
        return tr("TITLE");
    case Tickers:
        return tr("TICKERS");
    case Quote:
        return tr("LAST / CHG");
    case Status:
        return tr("STATUS");
    case Category:
        return tr("CATEGORY");
    case Priority:
        return tr("PRIORITY");
    case Tags:
        return tr("TAGS");
    case Review:
        return tr("REVIEW");
    case Edited:
        return tr("EDITED");
    case Created:
        return tr("CREATED");
    default:
        return {};
    }
}

QVariant NotesTableModel::data(const QModelIndex& index, int role) const {
    const NoteEntry* e = entry(index.row());
    if (!e)
        return {};
    const int col = index.column();
    const QString first = e->ticker_list.value(0);
    const CardQuote* q = (quotes_ && !first.isEmpty()) ? quotes_->find(first) : nullptr;
    switch (role) {
    case IdRole:
        return e->note.id;
    case Qt::DisplayRole:
    case Qt::EditRole:
        switch (col) {
        case Title:
            return e->note.title.isEmpty() && role == Qt::DisplayRole ? tr("Untitled note") : e->note.title;
        case Tickers:
            return e->ticker_list.isEmpty() ? QString() : QLatin1Char('$') + e->ticker_list.join(QStringLiteral("  $"));
        case Quote:
            if (!q || q->price <= 0)
                return first.isEmpty() ? QString() : QStringLiteral("--");
            return q->has_change ? QStringLiteral("%1  %2").arg(fincept::ui::formatting::price(q->price),
                                                                fincept::ui::formatting::pct(q->change_pct, 2, true))
                                 : fincept::ui::formatting::price(q->price);
        case Status:
            return e->note.is_archived ? status_label(QStringLiteral("ARCHIVED")) : status_label(e->meta.status);
        case Category:
            return category_label(e->category).toUpper();
        case Priority:
            return e->priority > 0 ? priority_label(priority_code(e->priority)) : QString();
        case Tags:
            return e->tag_list.isEmpty() ? QString() : QLatin1Char('#') + e->tag_list.join(QStringLiteral("  #"));
        case Review:
            return e->review.isValid() ? e->review.toString(QStringLiteral("yyyy-MM-dd")) : QString();
        case Edited:
            return ntm_date(e->updated_ms);
        case Created:
            return ntm_date(e->created_ms);
        default:
            return {};
        }
    case SortRole:
        switch (col) {
        case Title:
            return e->note.title.toLower();
        case Tickers:
            return first;
        case Quote:
            return q && q->has_change ? q->change_pct : -1e9;
        case Status:
            return e->note.is_archived ? 99 : static_cast<int>(fincept::notes_board::status_codes().indexOf(e->meta.status));
        case Category:
            return e->category;
        case Priority:
            return e->priority;
        case Tags:
            return e->tag_list.value(0).toLower();
        case Review:
            return e->review.isValid() ? e->review.toJulianDay() : std::numeric_limits<qint64>::max();
        case Edited:
            return e->updated_ms;
        case Created:
            return e->created_ms;
        default:
            return {};
        }
    case Qt::ForegroundRole:
        switch (col) {
        case Title:
            return e->note.title.isEmpty() ? QColor(colors::TEXT_TERTIARY()) : QColor(colors::TEXT_PRIMARY());
        case Tickers:
            return QColor(colors::AMBER());
        case Quote:
            if (q && q->has_change)
                return q->change_pct > 0   ? QColor(colors::POSITIVE())
                       : q->change_pct < 0 ? QColor(colors::NEGATIVE())
                                           : QColor(colors::TEXT_SECONDARY());
            return QColor(colors::TEXT_TERTIARY());
        case Status:
            return e->note.is_archived ? QColor(colors::TEXT_TERTIARY()) : status_color(e->meta.status);
        case Review:
            if (e->review.isValid() && is_open(*e) && e->review <= QDate::currentDate())
                return e->review < QDate::currentDate() ? QColor(colors::DANGER()) : QColor(colors::WARNING());
            return QColor(colors::TEXT_SECONDARY());
        case Category:
            return category_color(e->category);
        default:
            return QColor(colors::TEXT_SECONDARY());
        }
    case Qt::TextAlignmentRole:
        return QVariant::fromValue(col == Quote ? Qt::AlignRight | Qt::AlignVCenter : Qt::AlignLeft | Qt::AlignVCenter);
    case Qt::ToolTipRole:
        if (col == Title)
            return e->preview.left(300);
        if (col == Status && e->linked)
            return tr("Saved from a Chat Mode run");
        return {};
    case Qt::AccessibleTextRole:
        return data(index, Qt::DisplayRole);
    case FlashRole: {
        if (col != Quote)
            return 0.0;
        const auto it = flash_.constFind(index.row());
        if (it == flash_.constEnd())
            return 0.0;
        const qint64 age = clock_.elapsed() - it->first;
        if (age >= kFlashMs)
            return 0.0;
        return (1.0 - double(age) / double(kFlashMs)) * it->second;
    }
    default:
        return {};
    }
}

Qt::ItemFlags NotesTableModel::flags(const QModelIndex& index) const {
    if (!index.isValid())
        return Qt::NoItemFlags;
    Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (index.column() == Title)
        f |= Qt::ItemIsEditable;
    return f;
}

bool NotesTableModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (role != Qt::EditRole || index.column() != Title)
        return false;
    const NoteEntry* e = entry(index.row());
    if (!e)
        return false;
    const QString t = value.toString().trimmed();
    if (t.isEmpty() || t == e->note.title)
        return false;
    emit rename_requested(e->note.id, t.left(500));
    return true;
}

void NotesTableModel::set_rows(QVector<NoteEntry> rows) {
    beginResetModel();
    rows_ = std::move(rows);
    flash_.clear();
    reindex();
    endResetModel();
}

bool NotesTableModel::update_entry(const NoteEntry& e) {
    const int row = row_of(e.note.id);
    if (row < 0)
        return false;
    const bool first_changed = rows_.at(row).ticker_list.value(0) != e.ticker_list.value(0);
    rows_[row] = e;
    if (first_changed)
        reindex();
    emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
    return true;
}

void NotesTableModel::remove_ids(const QSet<int>& ids) {
    for (int row = static_cast<int>(rows_.size()) - 1; row >= 0; --row) {
        if (!ids.contains(rows_.at(row).note.id))
            continue;
        beginRemoveRows(QModelIndex(), row, row);
        rows_.removeAt(row);
        endRemoveRows();
    }
    flash_.clear();
    reindex();
}

bool NotesTableModel::quotes_changed(const QStringList& symbols) {
    const qint64 now = clock_.elapsed();
    for (const QString& s : symbols) {
        const auto it = rows_by_ticker_.constFind(s);
        if (it == rows_by_ticker_.constEnd())
            continue;
        const CardQuote* q = quotes_ ? quotes_->find(s) : nullptr;
        const int dir = q ? q->flash : 0;
        for (int row : it.value()) {
            if (dir != 0)
                flash_.insert(row, {now, dir});
            const QModelIndex i = index(row, Quote);
            emit dataChanged(i, i);
        }
    }
    return !flash_.isEmpty();
}

bool NotesTableModel::tick_flash() {
    const qint64 now = clock_.elapsed();
    for (auto it = flash_.begin(); it != flash_.end();) {
        const QModelIndex i = index(it.key(), Quote);
        if (now - it->first >= kFlashMs) {
            it = flash_.erase(it);
        } else {
            ++it;
        }
        emit dataChanged(i, i);
    }
    return !flash_.isEmpty();
}

void NotesTableModel::refresh_all() {
    if (!rows_.isEmpty())
        emit dataChanged(index(0, 0), index(static_cast<int>(rows_.size()) - 1, ColumnCount - 1));
    emit headerDataChanged(Qt::Horizontal, 0, ColumnCount - 1);
}

void NotesTableModel::reindex() {
    row_by_id_.clear();
    rows_by_ticker_.clear();
    row_by_id_.reserve(rows_.size());
    for (int i = 0; i < rows_.size(); ++i) {
        row_by_id_.insert(rows_.at(i).note.id, i);
        const QString first = rows_.at(i).ticker_list.value(0);
        if (!first.isEmpty())
            rows_by_ticker_[first] << i;
    }
}

} // namespace fincept::screens::notes
