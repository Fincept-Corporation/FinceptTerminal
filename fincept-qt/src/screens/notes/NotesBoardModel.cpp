// src/screens/notes/NotesBoardModel.cpp
#include "screens/notes/NotesBoardModel.h"

#include "core/symbol/SymbolMime.h"

#include <QAbstractItemView>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QPainter>

#include <algorithm>
#include <memory>

namespace fincept::screens::notes {

// ── Model ────────────────────────────────────────────────────────────────────

NotesBoardModel::NotesBoardModel(QObject* parent) : QAbstractItemModel(parent) {}

QModelIndex NotesBoardModel::index(int row, int column, const QModelIndex& parent) const {
    if (column != 0 || row < 0)
        return {};
    if (!parent.isValid())
        return row < lanes_.size() ? createIndex(row, 0, quintptr(0)) : QModelIndex();
    if (parent.internalId() != 0)
        return {}; // cards have no children
    const int lane = parent.row();
    if (lane < 0 || lane >= lanes_.size() || row >= lanes_.at(lane).cards.size())
        return {};
    return createIndex(row, 0, quintptr(lane + 1));
}

QModelIndex NotesBoardModel::parent(const QModelIndex& child) const {
    if (!child.isValid() || child.internalId() == 0)
        return {};
    return createIndex(static_cast<int>(child.internalId() - 1), 0, quintptr(0));
}

int NotesBoardModel::rowCount(const QModelIndex& parent) const {
    if (!parent.isValid())
        return static_cast<int>(lanes_.size());
    if (parent.internalId() != 0 || parent.row() < 0 || parent.row() >= lanes_.size())
        return 0;
    return static_cast<int>(lanes_.at(parent.row()).cards.size());
}

int NotesBoardModel::columnCount(const QModelIndex&) const {
    return 1;
}

const NoteEntry* NotesBoardModel::card(int lane, int row) const {
    if (lane < 0 || lane >= lanes_.size())
        return nullptr;
    const auto& cards = lanes_.at(lane).cards;
    return (row >= 0 && row < cards.size()) ? &cards.at(row) : nullptr;
}

const NoteEntry* NotesBoardModel::card(const QModelIndex& index) const {
    if (!index.isValid() || index.internalId() == 0)
        return nullptr;
    return card(static_cast<int>(index.internalId() - 1), index.row());
}

QVariant NotesBoardModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid())
        return {};
    if (index.internalId() == 0) {
        const int l = index.row();
        if (l < 0 || l >= lanes_.size())
            return {};
        const Lane& lane = lanes_.at(l);
        switch (role) {
        case Qt::DisplayRole:
        case Qt::AccessibleTextRole:
            return tr("%1, %n card(s)", nullptr, static_cast<int>(lane.cards.size())).arg(lane.title);
        case LaneKeyRole:
            return lane.key;
        case IsLaneRole:
            return true;
        case CountRole:
            return static_cast<int>(lane.cards.size());
        default:
            return {};
        }
    }
    const NoteEntry* e = card(index);
    if (!e)
        return {};
    switch (role) {
    case Qt::DisplayRole:
    case Qt::AccessibleTextRole:
        return e->note.title.isEmpty() ? tr("Untitled note") : e->note.title;
    case Qt::AccessibleDescriptionRole:
        return e->preview;
    case Qt::ToolTipRole: {
        QString tip = QStringLiteral("<b>%1</b>").arg((e->note.title.isEmpty() ? tr("Untitled note") : e->note.title)
                                                          .toHtmlEscaped());
        if (!e->preview.isEmpty())
            tip += QStringLiteral("<br>") + e->preview.left(240).toHtmlEscaped();
        if (!e->ticker_list.isEmpty())
            tip += QStringLiteral("<br>$") + e->ticker_list.join(QStringLiteral("  $")).toHtmlEscaped();
        if (!e->tag_list.isEmpty())
            tip += QStringLiteral("<br>#") + e->tag_list.join(QStringLiteral("  #")).toHtmlEscaped();
        if (e->linked)
            tip += QStringLiteral("<br>") + tr("Saved from a Chat Mode run").toHtmlEscaped();
        return tip;
    }
    case IdRole:
        return e->note.id;
    case LaneKeyRole:
        return lanes_.at(static_cast<int>(index.internalId() - 1)).key;
    case IsLaneRole:
        return false;
    default:
        return {};
    }
}

Qt::ItemFlags NotesBoardModel::flags(const QModelIndex& index) const {
    if (!index.isValid())
        return Qt::NoItemFlags;
    if (index.internalId() == 0)
        return Qt::ItemIsEnabled | Qt::ItemIsDropEnabled;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
}

QStringList NotesBoardModel::mimeTypes() const {
    return {QString::fromLatin1(kNoteCardsMime), QString::fromLatin1(fincept::symbol_mime::kMimeType),
            QStringLiteral("text/plain")};
}

QMimeData* NotesBoardModel::mimeData(const QModelIndexList& indexes) const {
    // Display order: lane, then row.
    QModelIndexList sorted = indexes;
    std::sort(sorted.begin(), sorted.end(), [](const QModelIndex& a, const QModelIndex& b) {
        if (a.internalId() != b.internalId())
            return a.internalId() < b.internalId();
        return a.row() < b.row();
    });
    QJsonArray ids;
    QStringList titles;
    QString first_ticker;
    QString from_lane;
    for (const QModelIndex& i : sorted) {
        const NoteEntry* e = card(i);
        if (!e)
            continue;
        ids.append(e->note.id);
        titles << e->note.title;
        if (from_lane.isNull())
            from_lane = lanes_.at(static_cast<int>(i.internalId() - 1)).key;
        if (first_ticker.isEmpty() && !e->ticker_list.isEmpty())
            first_ticker = e->ticker_list.first();
    }
    if (ids.isEmpty())
        return nullptr;
    auto* mime = new QMimeData;
    mime->setData(QString::fromLatin1(kNoteCardsMime),
                  QJsonDocument(QJsonObject{{QStringLiteral("ids"), ids}, {QStringLiteral("from"), from_lane}})
                      .toJson(QJsonDocument::Compact));
    mime->setText(titles.join(QLatin1Char('\n')));
    if (!first_ticker.isEmpty()) {
        // A card dropped on another panel opens its ticker there.
        std::unique_ptr<QMimeData> sym(fincept::symbol_mime::pack(fincept::SymbolRef::equity(first_ticker)));
        if (sym)
            mime->setData(QString::fromLatin1(fincept::symbol_mime::kMimeType),
                          sym->data(QString::fromLatin1(fincept::symbol_mime::kMimeType)));
    }
    return mime;
}

Qt::DropActions NotesBoardModel::supportedDropActions() const {
    return Qt::MoveAction | Qt::CopyAction;
}

Qt::DropActions NotesBoardModel::supportedDragActions() const {
    return Qt::MoveAction | Qt::CopyAction;
}

void NotesBoardModel::set_lanes(QVector<Lane> lanes) {
    beginResetModel();
    lanes_ = std::move(lanes);
    reindex();
    endResetModel();
}

int NotesBoardModel::lane_of_key(const QString& key) const {
    for (int i = 0; i < lanes_.size(); ++i)
        if (lanes_.at(i).key == key)
            return i;
    return -1;
}

QModelIndex NotesBoardModel::lane_index(int lane) const {
    return (lane >= 0 && lane < lanes_.size()) ? createIndex(lane, 0, quintptr(0)) : QModelIndex();
}

void NotesBoardModel::set_lane_collapsed(int lane, bool collapsed) {
    if (lane < 0 || lane >= lanes_.size() || lanes_.at(lane).collapsed == collapsed)
        return;
    lanes_[lane].collapsed = collapsed;
    const QModelIndex i = lane_index(lane);
    emit dataChanged(i, i);
}

QModelIndex NotesBoardModel::card_index(int id) const {
    const auto it = pos_.constFind(id);
    if (it == pos_.constEnd())
        return {};
    return createIndex(it->second, 0, quintptr(it->first + 1));
}

QPair<int, int> NotesBoardModel::position_of(int id) const {
    return pos_.value(id, {-1, -1});
}

bool NotesBoardModel::update_entry(const NoteEntry& e) {
    const auto it = pos_.constFind(e.note.id);
    if (it == pos_.constEnd())
        return false;
    lanes_[it->first].cards[it->second] = e;
    const QModelIndex i = createIndex(it->second, 0, quintptr(it->first + 1));
    emit dataChanged(i, i);
    return true;
}

void NotesBoardModel::move_cards(const QVector<int>& ids, int to_lane, int to_row) {
    if (to_lane < 0 || to_lane >= lanes_.size() || ids.isEmpty())
        return;
    emit layoutAboutToBeChanged({}, QAbstractItemModel::VerticalSortHint);
    // Persistent card indexes -> note ids, to re-point after the move.
    const QModelIndexList persistent = persistentIndexList();
    QVector<int> persistent_ids;
    persistent_ids.reserve(persistent.size());
    for (const QModelIndex& i : persistent) {
        const NoteEntry* e = card(i);
        persistent_ids << (e ? e->note.id : 0);
    }
    // Insertion point counted among the cards that stay in the target lane.
    QSet<int> moving(ids.cbegin(), ids.cend());
    int insert_at = 0;
    {
        const auto& target = lanes_.at(to_lane).cards;
        for (int r = 0; r < target.size() && r < to_row; ++r)
            if (!moving.contains(target.at(r).note.id))
                ++insert_at;
    }
    QVector<NoteEntry> taken;
    taken.reserve(ids.size());
    for (int id : ids) {
        const auto it = pos_.constFind(id);
        if (it == pos_.constEnd())
            continue;
        taken << lanes_.at(it->first).cards.at(it->second);
    }
    for (auto& lane : lanes_)
        lane.cards.erase(std::remove_if(lane.cards.begin(), lane.cards.end(),
                                        [&moving](const NoteEntry& e) { return moving.contains(e.note.id); }),
                         lane.cards.end());
    auto& target = lanes_[to_lane].cards;
    insert_at = std::clamp(insert_at, 0, static_cast<int>(target.size()));
    for (int i = 0; i < taken.size(); ++i)
        target.insert(insert_at + i, taken.at(i));
    reindex();
    QModelIndexList to;
    to.reserve(persistent.size());
    for (int i = 0; i < persistent.size(); ++i) {
        const QModelIndex& from = persistent.at(i);
        if (from.internalId() == 0) {
            to << from; // lanes do not move
            continue;
        }
        to << card_index(persistent_ids.at(i));
    }
    changePersistentIndexList(persistent, to);
    emit layoutChanged({}, QAbstractItemModel::VerticalSortHint);
}

void NotesBoardModel::remove_ids(const QSet<int>& ids) {
    for (int l = 0; l < lanes_.size(); ++l) {
        auto& cards = lanes_[l].cards;
        for (int row = static_cast<int>(cards.size()) - 1; row >= 0; --row) {
            if (!ids.contains(cards.at(row).note.id))
                continue;
            beginRemoveRows(lane_index(l), row, row);
            cards.removeAt(row);
            endRemoveRows();
        }
    }
    reindex();
}

void NotesBoardModel::set_highlight(const QStringList& terms) {
    if (terms == highlight_)
        return;
    highlight_ = terms;
    refresh_cards();
}

void NotesBoardModel::refresh_cards() {
    for (int l = 0; l < lanes_.size(); ++l) {
        const int n = static_cast<int>(lanes_.at(l).cards.size());
        if (n > 0)
            emit dataChanged(createIndex(0, 0, quintptr(l + 1)), createIndex(n - 1, 0, quintptr(l + 1)));
    }
}

void NotesBoardModel::reindex() {
    pos_.clear();
    card_count_ = 0;
    for (int l = 0; l < lanes_.size(); ++l) {
        const auto& cards = lanes_.at(l).cards;
        card_count_ += static_cast<int>(cards.size());
        for (int r = 0; r < cards.size(); ++r)
            pos_.insert(cards.at(r).note.id, {l, r});
    }
}

// ── Delegate ─────────────────────────────────────────────────────────────────

NotesCardDelegate::NotesCardDelegate(QObject* parent) : QStyledItemDelegate(parent) {
    metrics_ = CardMetrics::compute();
}

QSize NotesCardDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const {
    return {option.rect.width() > 0 ? option.rect.width() : metrics_.card_w, metrics_.card_h};
}

void NotesCardDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    CardState st;
    st.selected = option.state & QStyle::State_Selected;
    st.hover = option.state & QStyle::State_MouseOver;
    st.current = option.state & QStyle::State_HasFocus;
    const auto* view = qobject_cast<const QAbstractItemView*>(option.widget);
    st.view_focused = view && view->hasFocus();
    st.show_status = show_status_;
    paint_state(painter, option.rect, index, st);
}

void NotesCardDelegate::paint_state(QPainter* painter, const QRect& rect, const QModelIndex& index,
                                    const CardState& st) const {
    const auto* model = qobject_cast<const NotesBoardModel*>(index.model());
    const NoteEntry* e = model ? model->card(index) : nullptr;
    if (!e)
        return;
    const QDateTime now = QDateTime::currentDateTime();
    paint_card(painter, rect, *e, metrics_, st, quotes_, model->highlight(), now.toMSecsSinceEpoch(), now.date());
}

} // namespace fincept::screens::notes
