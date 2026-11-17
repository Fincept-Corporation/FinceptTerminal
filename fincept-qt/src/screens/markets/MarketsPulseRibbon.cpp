#include "screens/markets/MarketsPulseRibbon.h"

#include "core/instrument/VenueSymbols.h"
#include "screens/markets/MarketsLogic.h"
#include "screens/markets/MarketsQuoteStore.h"
#include "screens/markets/MarketsStyle.h"
#include "services/markets/MarketDataService.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/StatTile.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QEvent>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QTimeZone>

#include <cmath>

namespace fincept::screens {

namespace fmt = fincept::ui::formatting;

namespace {

/// The tiles, in priority order (the rightmost drop first when narrow). Canonical
/// market-data symbols; labels translated at use.
struct PulseDef {
    const char* symbol;
    const char* label;
};
constexpr PulseDef kPulse[] = {
    {"^GSPC", QT_TRANSLATE_NOOP("MarketsPulseRibbon", "S&P 500")},
    {"^NDX", QT_TRANSLATE_NOOP("MarketsPulseRibbon", "NASDAQ 100")},
    {"^DJI", QT_TRANSLATE_NOOP("MarketsPulseRibbon", "DOW")},
    {"^RUT", QT_TRANSLATE_NOOP("MarketsPulseRibbon", "RUSSELL 2K")},
    {"^VIX", QT_TRANSLATE_NOOP("MarketsPulseRibbon", "VIX")},
    {"^TNX", QT_TRANSLATE_NOOP("MarketsPulseRibbon", "US 10Y")},
    {"DX-Y.NYB", QT_TRANSLATE_NOOP("MarketsPulseRibbon", "DOLLAR")},
    {"GC=F", QT_TRANSLATE_NOOP("MarketsPulseRibbon", "GOLD")},
    {"CL=F", QT_TRANSLATE_NOOP("MarketsPulseRibbon", "WTI")},
    {"BTC-USD", QT_TRANSLATE_NOOP("MarketsPulseRibbon", "BITCOIN")},
};

constexpr int kTileSpacing = 6;

QString mpr_sheet() {
    return QStringLiteral("#mprRoot{background:%1;border-bottom:1px solid %2;}")
        .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM());
}

} // namespace

MarketsPulseRibbon::MarketsPulseRibbon(MarketsQuoteStore* store, QWidget* parent) : QWidget(parent), store_(store) {
    setObjectName(QStringLiteral("mprRoot"));
    setAttribute(Qt::WA_StyledBackground, true);
    markets_set_sheet(this, mpr_sheet());
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(8, 6, 8, 6);
    row->setSpacing(kTileSpacing);
    for (const auto& d : kPulse) {
        Tile t;
        t.symbol = QString::fromLatin1(d.symbol);
        t.label = d.label;
        t.tile = new ui::StatTile(QCoreApplication::translate("MarketsPulseRibbon", d.label), this);
        t.tile->set_compact(true);
        t.tile->set_clickable(true);
        // Width comes from the ribbon (equal stretch); the visible count guarantees each
        // shown tile at least ui::metrics::width(124) (apply_visible_count).
        t.tile->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        t.tile->set_state(ui::StatTile::State::Loading);
        t.tile->installEventFilter(this);
        const QString sym = t.symbol;
        connect(t.tile, &ui::StatTile::clicked, this, [this, sym]() { emit symbol_selected(sym); });
        row->addWidget(t.tile, 1);
        tiles_.append(t);
    }
    if (store_) {
        connect(store_, &MarketsQuoteStore::rows_changed, this, &MarketsPulseRibbon::update_tiles);
        // Age alone can make a tile stale: re-evaluated with the coverage tick.
        connect(store_, &MarketsQuoteStore::coverage_changed, this, [this]() {
            for (int i = 0; i < tiles_.size(); ++i)
                if (!tiles_[i].tile->isHidden())
                    update_tile(i);
        });
    }
    for (int i = 0; i < tiles_.size(); ++i)
        update_tile(i);
    // Currency / number / clock format changed: re-render every tile from the store's rows.
    fmt::on_format_changed(this, [this]() {
        for (int i = 0; i < tiles_.size(); ++i)
            update_tile(i);
    });
}

QStringList MarketsPulseRibbon::symbols() const {
    QStringList out;
    for (const auto& t : tiles_)
        out << t.symbol;
    return out;
}

void MarketsPulseRibbon::update_tiles(const QSet<QString>& symbols) {
    for (int i = 0; i < tiles_.size(); ++i)
        if (symbols.contains(tiles_[i].symbol))
            update_tile(i);
}

void MarketsPulseRibbon::update_tile(int index) {
    Tile& t = tiles_[index];
    const MarketRow* r = store_ ? store_->row(t.symbol) : nullptr;
    QString ticker = core::instrument::display_symbol(t.symbol);
    if (ticker.isEmpty())
        ticker = t.symbol;
    if (!r || !r->has_quote) {
        if (r && !r->error.isEmpty())
            t.tile->set_state(ui::StatTile::State::Error, r->error);
        else
            t.tile->set_state(ui::StatTile::State::Loading);
        t.tile->set_tooltip_text(QStringLiteral("%1  (%2)").arg(store_ ? store_->display_name(t.symbol) : ticker, ticker));
        return;
    }
    const services::QuoteData& q = r->quote;
    fmt::InstrumentFmt inst = fmt::instrument_for_symbol(t.symbol, q.currency);
    if (q.price_digits >= 0)
        inst.decimals = q.price_digits;
    // Gold, oil and bitcoin are prices in their quote currency ("$2,650.10"); index levels
    // (S&P 500, VIX, the dollar index) and the 10Y yield are not money and get no unit.
    const bool money = r->cls != core::instrument::AssetClass::Index && r->cls != core::instrument::AssetClass::Fx &&
                       !q.currency.isEmpty() && fmt::is_money_quote(q.quote_type, t.symbol);
    t.tile->set_value(money ? fmt::price_with_symbol(q.price, inst) : fmt::price(q.price, inst));
    const int dir = markets_logic::direction_of(q.change);
    const QString arrow = dir > 0 ? QString::fromUtf8("\xe2\x96\xb2") // ▲
                                  : (dir < 0 ? QString::fromUtf8("\xe2\x96\xbc") : QStringLiteral("=")); // ▼
    t.tile->set_sub_label(QStringLiteral("%1 %2   %3")
                              .arg(arrow, fmt::pct(std::abs(q.change_pct), 2, false),
                                   fmt::price_change(q.change, q.price, inst)));
    // Direction for every tile (VIX and yields too): price direction, not good / bad.
    t.tile->set_tone(dir > 0 ? ui::StatTile::Tone::Positive
                             : (dir < 0 ? ui::StatTile::Tone::Negative : ui::StatTile::Tone::Neutral));
    if (r->spark.size() >= 2) {
        if (std::isfinite(q.previous_close) && q.previous_close > 0)
            t.tile->set_sparkline(r->spark, q.previous_close);
        else
            t.tile->set_sparkline(r->spark);
    }
    QStringList tip;
    tip << QStringLiteral("%1  (%2)").arg(store_->display_name(t.symbol), ticker);
    QStringList facts;
    if (q.market_state == QLatin1String("REGULAR"))
        facts << tr("Open");
    else if (q.market_state == QLatin1String("PRE"))
        facts << tr("Pre-market");
    else if (q.market_state == QLatin1String("POST"))
        facts << tr("Post-market");
    else if (q.market_state == QLatin1String("CLOSED"))
        facts << tr("Closed");
    if (q.market_time > 0)
        facts << tr("as of %1").arg(fmt::date_time(q.market_time, fmt::DateStyle::Medium, false,
                                                   q.exchange_timezone.isEmpty()
                                                       ? QTimeZone()
                                                       : QTimeZone(q.exchange_timezone.toUtf8())));
    if (!facts.isEmpty())
        tip << facts.join(QStringLiteral(" · "));
    tip << tr("Click: show it · double-click: Equity Research");
    t.tile->set_tooltip_text(tip.join(QLatin1Char('\n')));
    const qint64 age = QDateTime::currentMSecsSinceEpoch() - r->quote_ms;
    if (!r->error.isEmpty())
        t.tile->set_state(ui::StatTile::State::Stale, r->error);
    // A closed market's quote is kept for its session, not re-published every 30 s: old
    // but still current is not stale.
    else if (age > markets_logic::kStaleFloorMs && !services::MarketDataService::quote_is_current(t.symbol))
        t.tile->set_state(ui::StatTile::State::Stale,
                          tr("last refreshed %1").arg(fmt::relative(r->quote_ms / 1000)));
    else
        t.tile->set_state(ui::StatTile::State::Normal);
}

void MarketsPulseRibbon::apply_visible_count() {
    const int n = markets_logic::ribbon_tile_count(width(), ui::metrics::width(124), kTileSpacing,
                                                   static_cast<int>(tiles_.size()));
    if (n == shown_)
        return;
    shown_ = n;
    for (int i = 0; i < tiles_.size(); ++i)
        tiles_[i].tile->setVisible(i < n);
}

QSize MarketsPulseRibbon::minimumSizeHint() const {
    return {0, QWidget::minimumSizeHint().height()};
}

void MarketsPulseRibbon::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    apply_visible_count(); // setVisible in resizeEvent only, never in a theme handler
}

bool MarketsPulseRibbon::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::MouseButtonDblClick) {
        for (const auto& t : tiles_)
            if (t.tile == watched) {
                emit symbol_activated(t.symbol);
                return true;
            }
    }
    return QWidget::eventFilter(watched, event);
}

void MarketsPulseRibbon::refresh_theme() {
    // StatTile paints from the theme at paint time; tones are roles, not colours.
    for (const auto& t : tiles_)
        t.tile->update();
}

void MarketsPulseRibbon::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(e);
}

void MarketsPulseRibbon::retranslate() {
    for (int i = 0; i < tiles_.size(); ++i) {
        tiles_[i].tile->set_label(QCoreApplication::translate("MarketsPulseRibbon", tiles_[i].label));
        update_tile(i);
    }
}

} // namespace fincept::screens
