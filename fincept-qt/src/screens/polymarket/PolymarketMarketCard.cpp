#include "screens/polymarket/PolymarketMarketCard.h"

#include "ui/theme/Theme.h"

#include <QPainter>

namespace fincept::screens::polymarket {

using namespace fincept::ui;
using namespace fincept::services::prediction;

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
    beginResetModel();
    endResetModel();
}

const PredictionMarket* PolymarketMarketCardModel::market_at(int row) const {
    if (view_mode_ == "markets" && row >= 0 && row < markets_.size())
        return &markets_[row];
    return nullptr;
}

bool PolymarketMarketCardModel::update_market(const PredictionMarket& market) {
    if (view_mode_ != QStringLiteral("markets")) return false;
    for (int i = 0; i < markets_.size(); ++i) {
        if (markets_[i].key.market_id == market.key.market_id) {
            markets_[i] = market;
            const auto idx = index(i);
            emit dataChanged(idx, idx);
            return true;
        }
    }
    return false;
}

const PredictionEvent* PolymarketMarketCardModel::event_at(int row) const {
    if (view_mode_ == "events" && row >= 0 && row < events_.size())
        return &events_[row];
    return nullptr;
}

// ── Delegate ────────────────────────────────────────────────────────────────

PolymarketMarketCardDelegate::PolymarketMarketCardDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

QSize PolymarketMarketCardDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const {
    return {280, 80};
}

void PolymarketMarketCardDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                         const QModelIndex& index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);

    QColor accent(colors::AMBER());
    if (const auto* m = qobject_cast<const PolymarketMarketCardModel*>(index.model()))
        accent = m->presentation().accent;

    const QRect r = option.rect;
    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = option.state & QStyle::State_MouseOver;

    // ── Background ────────────────────────────────────────────────────────
    if (selected) {
        // Selected: deep tinted background, left accent rail
        QColor sel_bg = accent;
        sel_bg.setAlphaF(0.10);
        painter->fillRect(r, sel_bg);
        // Left accent rail (3px)
        painter->fillRect(r.left(), r.top(), 3, r.height(), accent);
    } else if (hovered) {
        painter->fillRect(r, QColor(colors::BG_HOVER()));
    } else {
        painter->fillRect(r, QColor(colors::BG_BASE()));
    }

    // Bottom divider
    painter->setPen(QColor(colors::BORDER_DIM()));
    painter->drawLine(r.left() + (selected ? 3 : 0), r.bottom(), r.right(), r.bottom());

    auto* model = qobject_cast<const PolymarketMarketCardModel*>(index.model());
    if (!model) { painter->restore(); return; }
    const auto& pres = model->presentation();

    const int left_pad = selected ? r.left() + 10 : r.left() + 10;
    int x = left_pad;
    int y = r.top() + 10;
    const int w = r.right() - 12 - x;

    const PredictionMarket* mkt = model->market_at(index.row());
    const PredictionEvent* evt  = model->event_at(index.row());

    QString title;
    double vol = 0;
    QVector<Outcome> outcomes;
    int sub_market_count = 0;
    bool is_active = true;
    bool is_closed = false;

    if (mkt) {
        title     = mkt->question;
        vol       = mkt->volume;
        outcomes  = mkt->outcomes;
        is_active = mkt->active;
        is_closed = mkt->closed;
    } else if (evt) {
        title            = evt->title;
        vol              = evt->volume;
        sub_market_count = evt->markets.size();
        is_active        = evt->active;
        is_closed        = evt->closed;
        if (!evt->markets.isEmpty())
            outcomes = evt->markets[0].outcomes;
    } else {
        painter->restore();
        return;
    }

    // ── Title ─────────────────────────────────────────────────────────────
    QFont title_font(fonts::DATA_FAMILY, 10);
    title_font.setWeight(QFont::DemiBold);
    painter->setFont(title_font);

    QColor title_color = selected ? accent : QColor(colors::TEXT_PRIMARY());
    if (is_closed) title_color = QColor(colors::TEXT_DIM());
    painter->setPen(title_color);

    const QRect title_rect(x, y, w, 28);
    painter->drawText(title_rect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                      title.left(90));
    y += 30;

    // ── Outcome probability bars ──────────────────────────────────────────
    const int bars_to_draw = qMin(2, outcomes.size());
    if (bars_to_draw >= 1) {
        const int bar_w   = w - 62;  // leave space for price label
        const int bar_h   = 8;
        const int gap     = 4;

        QFont price_font(fonts::DATA_FAMILY, 9);
        price_font.setWeight(QFont::Bold);

        for (int i = 0; i < bars_to_draw; ++i) {
            const auto& o   = outcomes[i];
            const double pct = qBound(0.0, o.price, 1.0);
            const int filled = static_cast<int>(bar_w * pct);

            QColor bar_color = (i == 0) ? pres.accent : QColor(OUTCOME_BAR_COLORS[qMin(i, 4)]);
            if (is_closed) bar_color = QColor(colors::BORDER_BRIGHT());

            // Track (background)
            painter->fillRect(x, y, bar_w, bar_h, QColor(colors::BG_RAISED()));
            // Fill
            if (filled > 0)
                painter->fillRect(x, y, filled, bar_h, bar_color);

            // Price label (right-aligned to bar gutter)
            painter->setFont(price_font);
            painter->setPen(is_closed ? QColor(colors::TEXT_DIM()) : bar_color);
            const QString price_str = pres.format_price(pct);
            painter->drawText(x + bar_w + 4, y - 1, 56, bar_h + 2,
                              Qt::AlignLeft | Qt::AlignVCenter, price_str);

            y += bar_h + gap;
        }
    } else {
        // Single-outcome or no-outcome: show volume
        QFont meta_font(fonts::DATA_FAMILY, 9);
        painter->setFont(meta_font);
        painter->setPen(QColor(colors::TEXT_SECONDARY()));
        painter->drawText(x, y, w, 14, Qt::AlignLeft | Qt::AlignVCenter,
                          pres.format_volume(vol));
        if (evt && sub_market_count > 0) {
            painter->drawText(x + 80, y, w - 80, 14, Qt::AlignLeft | Qt::AlignVCenter,
                              QString("%1 mkts").arg(sub_market_count));
        }
    }

    // ── Status dot (top-right corner) ─────────────────────────────────────
    const int dot_x = r.right() - 10;
    const int dot_y = r.top() + 10;
    if (is_closed) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(colors::TEXT_DIM()));
        painter->drawEllipse(dot_x, dot_y, 5, 5);
    } else if (is_active) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(colors::POSITIVE()));
        painter->drawEllipse(dot_x, dot_y, 5, 5);
    }

    painter->restore();
}

} // namespace fincept::screens::polymarket
