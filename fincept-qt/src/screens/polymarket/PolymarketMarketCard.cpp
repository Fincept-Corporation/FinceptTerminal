#include "screens/polymarket/PolymarketMarketCard.h"

#include "ui/theme/Theme.h"

#include <QPainter>

namespace fincept::screens::polymarket {

using namespace fincept::ui;
using namespace fincept::services::polymarket;

static QString fmt_vol(double v) {
    if (v >= 1e9)
        return QString("$%1B").arg(v / 1e9, 0, 'f', 1);
    if (v >= 1e6)
        return QString("$%1M").arg(v / 1e6, 0, 'f', 1);
    if (v >= 1e3)
        return QString("$%1K").arg(v / 1e3, 0, 'f', 1);
    return QString("$%1").arg(v, 0, 'f', 0);
}

// ── Model ───────────────────────────────────────────────────────────────────

PolymarketMarketCardModel::PolymarketMarketCardModel(QObject* parent) : QAbstractListModel(parent) {}

int PolymarketMarketCardModel::rowCount(const QModelIndex&) const {
    return (view_mode_ == "events") ? events_.size() : markets_.size();
}

QVariant PolymarketMarketCardModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid())
        return {};
    if (role == Qt::DisplayRole) {
        if (view_mode_ == "events" && index.row() < events_.size())
            return events_[index.row()].title;
        if (index.row() < markets_.size())
            return markets_[index.row()].question;
    }
    return {};
}

void PolymarketMarketCardModel::set_markets(const QVector<Market>& markets) {
    beginResetModel();
    markets_ = markets;
    view_mode_ = "markets";
    endResetModel();
}

void PolymarketMarketCardModel::set_events(const QVector<Event>& events) {
    beginResetModel();
    events_ = events;
    view_mode_ = "events";
    endResetModel();
}

void PolymarketMarketCardModel::set_view_mode(const QString& mode) {
    view_mode_ = mode;
}

const Market* PolymarketMarketCardModel::market_at(int row) const {
    if (row >= 0 && row < markets_.size())
        return &markets_[row];
    return nullptr;
}

const Event* PolymarketMarketCardModel::event_at(int row) const {
    if (row >= 0 && row < events_.size())
        return &events_[row];
    return nullptr;
}

// ── Delegate ────────────────────────────────────────────────────────────────

PolymarketMarketCardDelegate::PolymarketMarketCardDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

QSize PolymarketMarketCardDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const {
    return {280, 86};
}

void PolymarketMarketCardDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                         const QModelIndex& index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);

    QRect r = option.rect;
    bool selected = option.state & QStyle::State_Selected;
    bool hovered = option.state & QStyle::State_MouseOver;

    // Background
    if (selected) {
        painter->fillRect(r, QColor(colors::AMBER).darker(400));
        painter->setPen(QPen(QColor(colors::AMBER), 2));
        painter->drawLine(r.left(), r.top(), r.left(), r.bottom());
    } else if (hovered) {
        painter->fillRect(r, QColor(colors::BG_HOVER));
    } else {
        painter->fillRect(r, QColor(colors::BG_BASE));
    }

    // Bottom border
    painter->setPen(QColor(colors::BORDER_DIM));
    painter->drawLine(r.left(), r.bottom(), r.right(), r.bottom());

    auto* model = qobject_cast<const PolymarketMarketCardModel*>(index.model());
    if (!model) {
        painter->restore();
        return;
    }

    int row = index.row();
    int x = r.left() + 10;
    int y = r.top() + 8;
    int w = r.width() - 20;

    const Market* mkt = model->market_at(row);
    const Event* evt = model->event_at(row);

    QString title;
    double vol = 0;
    QVector<Outcome> outcomes;

    if (mkt) {
        title = mkt->question;
        vol = mkt->volume;
        outcomes = mkt->outcomes;
    } else if (evt) {
        title = evt->title;
        vol = evt->volume;
        if (!evt->markets.isEmpty())
            outcomes = evt->markets[0].outcomes;
    } else {
        painter->restore();
        return;
    }

    // Title (2 lines max)
    QFont font(fonts::DATA_FAMILY, 11);
    font.setWeight(QFont::Bold);
    painter->setFont(font);
    painter->setPen(QColor(selected ? colors::AMBER : colors::TEXT_PRIMARY));
    QRect title_rect(x, y, w, 30);
    painter->drawText(title_rect, Qt::AlignLeft | Qt::TextWordWrap, title.left(80));
    y += 32;

    // Probability bars
    if (outcomes.size() >= 2) {
        double yes_pct = outcomes[0].price;
        double no_pct = outcomes[1].price;
        int bar_w = w - 60;
        int bar_h = 14;

        // YES bar
        int yes_w = static_cast<int>(bar_w * yes_pct);
        painter->fillRect(x, y, yes_w, bar_h, QColor("#00D66F"));
        painter->fillRect(x + yes_w, y, bar_w - yes_w, bar_h, QColor(colors::BG_RAISED));

        QFont small_font(fonts::DATA_FAMILY, 9);
        small_font.setWeight(QFont::Bold);
        painter->setFont(small_font);
        painter->setPen(QColor(colors::TEXT_PRIMARY));
        painter->drawText(x + bar_w + 4, y, 52, bar_h, Qt::AlignLeft | Qt::AlignVCenter,
                          QString("Y %1c").arg(qRound(yes_pct * 100)));
        y += bar_h + 2;

        // NO bar
        int no_w = static_cast<int>(bar_w * no_pct);
        painter->fillRect(x, y, no_w, bar_h, QColor("#FF3B3B"));
        painter->fillRect(x + no_w, y, bar_w - no_w, bar_h, QColor(colors::BG_RAISED));
        painter->drawText(x + bar_w + 4, y, 52, bar_h, Qt::AlignLeft | Qt::AlignVCenter,
                          QString("N %1c").arg(qRound(no_pct * 100)));
    } else {
        // Volume line for events without outcomes
        QFont small_font(fonts::DATA_FAMILY, 10);
        painter->setFont(small_font);
        painter->setPen(QColor(colors::TEXT_SECONDARY));
        painter->drawText(x, y, w, 16, Qt::AlignLeft | Qt::AlignVCenter, fmt_vol(vol));

        if (evt) {
            painter->drawText(x + 80, y, w - 80, 16, Qt::AlignLeft | Qt::AlignVCenter,
                              QString("%1 markets").arg(evt->markets.size()));
        }
    }

    painter->restore();
}

} // namespace fincept::screens::polymarket
