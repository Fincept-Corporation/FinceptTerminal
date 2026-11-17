#include "screens/polymarket/PolymarketMarketCard.h"

#include "ui/theme/Theme.h"

#include <QFontMetrics>
#include <QPainter>

namespace fincept::screens::polymarket {

using namespace fincept::ui;
using namespace fincept::services::prediction;

namespace {

// Card fonts and the vertical space they need, from the user's font size.
QFont card_title_font() {
    return fonts::font(-1, QFont::DemiBold);
}
QFont card_price_font() {
    return fonts::font(-2, QFont::Bold);
}
int card_title_height() { // two wrapped title lines
    return 2 * QFontMetrics(card_title_font()).lineSpacing();
}
int card_bar_pitch() { // one outcome bar row, price label included
    return qMax(12, QFontMetrics(card_price_font()).height());
}

} // namespace

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
    if (view_mode_ != QStringLiteral("markets"))
        return false;
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

PolymarketMarketCardDelegate::PolymarketMarketCardDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

QSize PolymarketMarketCardDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const {
    // 10 top pad + title + 2 gap + two bar rows + bottom pad; 80 at the default size.
    return {280, qMax(80, 10 + card_title_height() + 2 + 2 * card_bar_pitch() + 10)};
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
        sel_bg.setAlphaF(0.10f);
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
    if (!model) {
        painter->restore();
        return;
    }
    const auto& pres = model->presentation();

    const int left_pad = selected ? r.left() + 10 : r.left() + 10;
    int x = left_pad;
    int y = r.top() + 10;
    const int w = r.right() - 12 - x;

    const PredictionMarket* mkt = model->market_at(index.row());
    const PredictionEvent* evt = model->event_at(index.row());

    QString title;
    double vol = 0;
    QVector<Outcome> outcomes;
    int sub_market_count = 0;
    bool is_active = true;
    bool is_closed = false;

    if (mkt) {
        title = mkt->question;
        vol = mkt->volume;
        outcomes = mkt->outcomes;
        is_active = mkt->active;
        is_closed = mkt->closed;
    } else if (evt) {
        title = evt->title;
        vol = evt->volume;
        sub_market_count = evt->markets.size();
        is_active = evt->active;
        is_closed = evt->closed;
        if (!evt->markets.isEmpty())
            outcomes = evt->markets[0].outcomes;
    } else {
        painter->restore();
        return;
    }

    // ── Title ─────────────────────────────────────────────────────────────
    painter->setFont(card_title_font());

    QColor title_color = selected ? accent : QColor(colors::TEXT_PRIMARY());
    if (is_closed)
        title_color = QColor(colors::TEXT_DIM());
    painter->setPen(title_color);

    const int title_h = card_title_height();
    const QRect title_rect(x, y, w, title_h);
    painter->drawText(title_rect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, title.left(90));
    y += title_h + 2;

    // ── Outcome probability bars ──────────────────────────────────────────
    const QFont price_font = card_price_font();
    const QFontMetrics price_fm(price_font);
    const int pitch = card_bar_pitch();
    const int bars_to_draw = qMin(2, outcomes.size());
    if (bars_to_draw >= 1) {
        // Wide enough for the longest price either style prints: "100.0¢", or a
        // contract price with its currency unit per the user's settings ("USD 1.00").
        const int label_w = qMax(56, qMax(price_fm.horizontalAdvance(QStringLiteral("100.0¢")),
                                          price_fm.horizontalAdvance(pres.format_price(1.0))) +
                                         4);
        const int bar_w = w - label_w - 6; // leave space for price label
        const int bar_h = 8;

        // Kalshi outcome prices are bids; the presentation turns them into a
        // displayable probability (last / mid, NO = 1 − YES).
        const PredictionMarket* bar_market = mkt ? mkt : (evt && !evt->markets.isEmpty() ? &evt->markets[0] : nullptr);
        for (int i = 0; i < bars_to_draw; ++i) {
            const auto& o = outcomes[i];
            const double pct = bar_market ? pres.display_probability(*bar_market, i) : qBound(0.0, o.price, 1.0);
            const int filled = static_cast<int>(bar_w * pct);

            // First outcome in the exchange accent, the second in the theme's
            // next categorical chart colour.
            QColor bar_color = (i == 0) ? pres.accent : colors::chart_color(i);
            if (is_closed)
                bar_color = QColor(colors::BORDER_BRIGHT());

            const int bar_y = y + (pitch - bar_h) / 2;
            // Track (background)
            painter->fillRect(x, bar_y, bar_w, bar_h, QColor(colors::BG_RAISED()));
            // Fill
            if (filled > 0)
                painter->fillRect(x, bar_y, filled, bar_h, bar_color);

            // Price label (right-aligned to bar gutter)
            painter->setFont(price_font);
            painter->setPen(is_closed ? QColor(colors::TEXT_DIM()) : bar_color);
            const QString price_str = pres.format_price(pct);
            painter->drawText(x + bar_w + 4, y, label_w, pitch, Qt::AlignLeft | Qt::AlignVCenter, price_str);

            y += pitch;
        }
    } else {
        // Single-outcome or no-outcome: show volume
        painter->setFont(fonts::font(-2));
        painter->setPen(QColor(colors::TEXT_SECONDARY()));
        const QString vol_str = pres.format_volume(vol);
        painter->drawText(x, y, w, pitch, Qt::AlignLeft | Qt::AlignVCenter, vol_str);
        if (evt && sub_market_count > 0) {
            const int count_x = qMax(80, price_fm.horizontalAdvance(vol_str) + 12);
            painter->drawText(x + count_x, y, w - count_x, pitch, Qt::AlignLeft | Qt::AlignVCenter,
                              tr("%1 mkts").arg(sub_market_count));
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
        painter->setBrush(QColor(colors::SUCCESS()));
        painter->drawEllipse(dot_x, dot_y, 5, 5);
    }

    painter->restore();
}

} // namespace fincept::screens::polymarket
