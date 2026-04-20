#include "screens/polymarket/PolymarketMarketCard.h"

#include "ui/theme/Theme.h"

#include <QPainter>

namespace fincept::screens::polymarket {

using namespace fincept::ui;
using namespace fincept::services::prediction;

// Outcome palette for multi-outcome markets. Slots 0–1 are the primary
// yes/no pair and are accent-colored dynamically at paint time; slots 2+
// are fallbacks used only for 3+-way Polymarket markets.
static const char* OUTCOME_BAR_COLORS[] = {"#00D66F", "#FF3B3B", "#FF8800", "#4F8EF7", "#A855F7"};

static QChar outcome_initial(const QString& name) {
    return name.isEmpty() ? QChar('?') : name.at(0).toUpper();
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

void PolymarketMarketCardModel::set_markets(const QVector<PredictionMarket>& markets) {
    beginResetModel();
    markets_ = markets;
    events_.clear();
    view_mode_ = "markets";
    endResetModel();
}

void PolymarketMarketCardModel::set_events(const QVector<PredictionEvent>& events) {
    beginResetModel();
    events_ = events;
    markets_.clear();
    view_mode_ = "events";
    endResetModel();
}

void PolymarketMarketCardModel::set_view_mode(const QString& mode) {
    view_mode_ = mode;
}

void PolymarketMarketCardModel::set_presentation(const ExchangePresentation& p) {
    presentation_ = p;
    // Full reset — the delegate re-paints from scratch and picks up the
    // new accent/formatter. A targeted dataChanged(0..N) would be cheaper
    // but the list is bounded (20/page) so the difference is invisible.
    beginResetModel();
    endResetModel();
}

const PredictionMarket* PolymarketMarketCardModel::market_at(int row) const {
    if (view_mode_ == "markets" && row >= 0 && row < markets_.size())
        return &markets_[row];
    return nullptr;
}

const PredictionEvent* PolymarketMarketCardModel::event_at(int row) const {
    if (view_mode_ == "events" && row >= 0 && row < events_.size())
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

    if (selected) {
        painter->fillRect(r, QColor(colors::AMBER()).darker(400));
        painter->setPen(QPen(QColor(colors::AMBER()), 2));
        painter->drawLine(r.left(), r.top(), r.left(), r.bottom());
    } else if (hovered) {
        painter->fillRect(r, QColor(colors::BG_HOVER()));
    } else {
        painter->fillRect(r, QColor(colors::BG_BASE()));
    }

    painter->setPen(QColor(colors::BORDER_DIM()));
    painter->drawLine(r.left(), r.bottom(), r.right(), r.bottom());

    auto* model = qobject_cast<const PolymarketMarketCardModel*>(index.model());
    if (!model) {
        painter->restore();
        return;
    }
    const auto& pres = model->presentation();

    int row = index.row();
    int x = r.left() + 10;
    int y = r.top() + 8;
    int w = r.width() - 20;

    const PredictionMarket* mkt = model->market_at(row);
    const PredictionEvent* evt = model->event_at(row);

    QString title;
    double vol = 0;
    QVector<Outcome> outcomes;
    int sub_market_count = 0;

    if (mkt) {
        title = mkt->question;
        vol = mkt->volume;
        outcomes = mkt->outcomes;
    } else if (evt) {
        title = evt->title;
        vol = evt->volume;
        sub_market_count = evt->markets.size();
        if (!evt->markets.isEmpty())
            outcomes = evt->markets[0].outcomes;
    } else {
        painter->restore();
        return;
    }

    // Title (2 lines max). Selected row gets painted in the exchange accent.
    QFont font(fonts::DATA_FAMILY, 11);
    font.setWeight(QFont::Bold);
    painter->setFont(font);
    painter->setPen(QColor(selected ? pres.accent : QColor(colors::TEXT_PRIMARY())));
    QRect title_rect(x, y, w, 30);
    painter->drawText(title_rect, Qt::AlignLeft | Qt::TextWordWrap, title.left(80));
    y += 32;

    // Dynamic outcome bars — read names/prices from prediction::Outcome so
    // Polymarket (Yes/No), Kalshi (yes/no), and custom multi-outcome markets
    // all render without code changes. Render up to 2 rows. Price labels
    // are formatted by the presentation (cents suffix on Polymarket,
    // dollars on Kalshi).
    const int bars_to_draw = qMin(2, outcomes.size());
    if (bars_to_draw >= 2) {
        const int bar_w = w - 80;  // wider right gutter for "$0.52" vs "Y 52¢"
        const int bar_h = 14;
        QFont small_font(fonts::DATA_FAMILY, 9);
        small_font.setWeight(QFont::Bold);

        for (int i = 0; i < bars_to_draw; ++i) {
            const auto& o = outcomes[i];
            const double pct = qBound(0.0, o.price, 1.0);
            const int filled = static_cast<int>(bar_w * pct);
            // Slot 0 tracks the exchange accent; slot 1 stays red so the
            // primary/secondary outcome split remains readable even when
            // the accent itself shifts.
            QColor bar_color = (i == 0) ? pres.accent : QColor(OUTCOME_BAR_COLORS[qMin(i, 4)]);
            painter->fillRect(x, y, filled, bar_h, bar_color);
            painter->fillRect(x + filled, y, bar_w - filled, bar_h, QColor(colors::BG_RAISED()));

            painter->setFont(small_font);
            painter->setPen(QColor(colors::TEXT_PRIMARY()));
            painter->drawText(x + bar_w + 4, y, 72, bar_h, Qt::AlignLeft | Qt::AlignVCenter,
                              QString("%1 %2").arg(outcome_initial(o.name)).arg(pres.format_price(pct)));
            y += bar_h + 2;
        }
    } else {
        QFont small_font(fonts::DATA_FAMILY, 10);
        painter->setFont(small_font);
        painter->setPen(QColor(colors::TEXT_SECONDARY()));
        painter->drawText(x, y, w, 16, Qt::AlignLeft | Qt::AlignVCenter, pres.format_volume(vol));
        if (evt && sub_market_count > 0) {
            painter->drawText(x + 80, y, w - 80, 16, Qt::AlignLeft | Qt::AlignVCenter,
                              QString("%1 markets").arg(sub_market_count));
        }
    }

    painter->restore();
}

} // namespace fincept::screens::polymarket
