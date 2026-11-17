#include "screens/markets/MarketsDetailPane.h"

#include "core/instrument/VenueSymbols.h"
#include "core/market/MarketSessions.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/equity_research/EquityResearchLinks.h"
#include "screens/markets/MarketColumns.h"
#include "screens/markets/MarketsLogic.h"
#include "screens/markets/MarketsQuoteStore.h"
#include "screens/markets/MarketsStyle.h"
#include "services/markets/MarketDataService.h"
#include "services/markets/MarketPerformance.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/StatTile.h"
#include "ui/widgets/ThemeHelpers.h"
#include "ui/widgets/ThemeIcon.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyle>
#include <QTimeZone>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>
#include <limits>

namespace fincept::screens {

namespace colors = fincept::ui::colors;
namespace fmt = fincept::ui::formatting;
using core::instrument::AssetClass;

namespace {

constexpr int kHistoryDebounceMs = 350;

struct TileDef {
    const char* key;
    const char* label;
};
constexpr TileDef kTiles[] = {
    {"open", QT_TRANSLATE_NOOP("MarketsDetailPane", "OPEN")},
    {"prev", QT_TRANSLATE_NOOP("MarketsDetailPane", "PREV CLOSE")},
    {"day", QT_TRANSLATE_NOOP("MarketsDetailPane", "DAY RANGE")},
    {"w52", QT_TRANSLATE_NOOP("MarketsDetailPane", "52W RANGE")},
    {"vol", QT_TRANSLATE_NOOP("MarketsDetailPane", "VOLUME")},
    {"d50", QT_TRANSLATE_NOOP("MarketsDetailPane", "50D AVG")},
    {"d200", QT_TRANSLATE_NOOP("MarketsDetailPane", "200D AVG")},
    {"m1", QT_TRANSLATE_NOOP("MarketsDetailPane", "1M %")},
    {"ytd", QT_TRANSLATE_NOOP("MarketsDetailPane", "YTD %")},
    {"y1", QT_TRANSLATE_NOOP("MarketsDetailPane", "1Y %")},
    {"vol20", QT_TRANSLATE_NOOP("MarketsDetailPane", "20D VOL")},
    {"mkt", QT_TRANSLATE_NOOP("MarketsDetailPane", "MARKET")},
};

QString mdp_sheet() {
    return QStringLiteral("#mdpRoot{background:%1;border-left:1px solid %2;}"
                          "#mdpHeader{background:%3;border-bottom:1px solid %2;}"
                          "QLabel{background:transparent;color:%4;font-family:%5;}"
                          "QLabel#mdpSymbol{color:%6;font-size:%7px;font-weight:700;}"
                          "QLabel#mdpName{color:%8;font-size:%9px;}"
                          "QLabel#mdpPrice{color:%4;font-size:%10px;font-weight:700;}"
                          "QLabel#mdpPrice[tone=\"closed\"]{color:%8;}"
                          "QLabel#mdpPrice[tone=\"stale\"]{color:%11;}"
                          "QLabel#mdpChange{font-size:%9px;color:%8;}"
                          "QLabel#mdpChange[tone=\"pos\"]{color:%12;}"
                          "QLabel#mdpChange[tone=\"neg\"]{color:%13;}"
                          "QLabel#mdpSession{font-size:%14px;color:%8;}"
                          "QLabel#mdpSession[tone=\"ok\"]{color:%15;}"
                          "QLabel#mdpSession[tone=\"warn\"]{color:%16;}"
                          "QLabel#mdpSession[tone=\"dim\"]{color:%11;}"
                          "QLabel#mdpEmpty{color:%11;font-size:%9px;}"
                          "QToolButton#mdpClose{background:transparent;border:none;padding:2px;}"
                          "QToolButton#mdpClose:hover{background:%17;}"
                          "QScrollArea#mdpScroll{background:transparent;border:none;}"
                          "#mdpBody{background:%1;}")
               .arg(colors::BG_BASE(), colors::BORDER_DIM(), colors::BG_SURFACE(), colors::TEXT_PRIMARY(),
                    QString(ui::fonts::DATA_FAMILY), colors::AMBER())
               .arg(ui::fonts::px(2))
               .arg(colors::TEXT_SECONDARY())
               .arg(ui::fonts::px(-2))
               .arg(ui::fonts::px(4))
               .arg(colors::TEXT_TERTIARY(), colors::POSITIVE(), colors::NEGATIVE())
               .arg(ui::fonts::px(-3))
               .arg(colors::SUCCESS(), colors::WARNING(), colors::BG_HOVER()) +
           markets_bar_buttons_css() + markets_scrollbar_css();
}

fmt::InstrumentFmt mdp_inst(const QString& sym, const services::QuoteData& q) {
    fmt::InstrumentFmt inst = fmt::instrument_for_symbol(sym, q.currency);
    if (q.price_digits >= 0)
        inst.decimals = q.price_digits;
    return inst;
}

bool mdp_pos(double v) {
    return std::isfinite(v) && v > 0.0;
}

/// A price in the instrument's own currency ("$189.84", "117.70p"); an index level,
/// FX rate or yield (not money) and a quote without a currency get no unit.
QString mdp_price(const MarketRow& r, const fmt::InstrumentFmt& inst, double v) {
    if (!mdp_pos(v))
        return fmt::placeholder();
    const bool money = r.cls != AssetClass::Index && r.cls != AssetClass::Fx && !r.quote.currency.isEmpty() &&
                       fmt::is_money_quote(r.quote.quote_type, r.symbol);
    return money ? fmt::price_with_symbol(v, inst) : fmt::price(v, inst);
}

} // namespace

MarketsDetailPane::MarketsDetailPane(MarketsQuoteStore* store, QWidget* parent)
    : QWidget(parent), store_(store), perf_1m_(std::numeric_limits<double>::quiet_NaN()),
      perf_ytd_(std::numeric_limits<double>::quiet_NaN()), perf_1y_(std::numeric_limits<double>::quiet_NaN()),
      perf_vol20_(std::numeric_limits<double>::quiet_NaN()) {
    setObjectName(QStringLiteral("mdpRoot"));
    setAttribute(Qt::WA_StyledBackground, true);
    markets_set_sheet(this, mdp_sheet()); // before any child exists
    setMinimumWidth(ui::metrics::width(300));
    load_timer_ = new QTimer(this);
    load_timer_->setSingleShot(true);
    load_timer_->setInterval(kHistoryDebounceMs);
    connect(load_timer_, &QTimer::timeout, this, &MarketsDetailPane::load_histories);
    build_ui();
    if (store_)
        connect(store_, &MarketsQuoteStore::rows_changed, this, &MarketsDetailPane::on_rows_changed);
    // Currency / number format changed: re-render from the store's rows and the cached
    // performance figures (nothing is fetched). The chart re-reads its axis formatter.
    fmt::on_format_changed(this, [this]() {
        update_header();
        update_quote_tiles();
        update_perf_tiles();
        if (series_ >= 0)
            apply_chart_formatter();
    });
    retranslate();
}

void MarketsDetailPane::apply_chart_formatter() {
    const MarketRow* r = store_ ? store_->row(symbol_) : nullptr;
    if (!r || !r->has_quote)
        return;
    // Axis ticks are bare prices (the header names the currency), in the quote's decimals.
    const fmt::InstrumentFmt inst = mdp_inst(symbol_, r->quote);
    chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left, [inst](double v) { return fmt::price(v, inst); });
}

void MarketsDetailPane::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(1, 0, 0, 0);
    root->setSpacing(0);

    // Header: two rows.
    auto* header = new QWidget(this);
    header->setObjectName(QStringLiteral("mdpHeader"));
    header->setAttribute(Qt::WA_StyledBackground, true);
    auto* hv = new QVBoxLayout(header);
    hv->setContentsMargins(12, 6, 8, 6);
    hv->setSpacing(2);
    auto* r1 = new QHBoxLayout;
    r1->setSpacing(8);
    sym_label_ = new QLabel(header);
    sym_label_->setObjectName(QStringLiteral("mdpSymbol"));
    name_label_ = new QLabel(header);
    name_label_->setObjectName(QStringLiteral("mdpName"));
    name_label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); // elides instead of widening
    close_btn_ = new QToolButton(header);
    close_btn_->setObjectName(QStringLiteral("mdpClose"));
    close_btn_->setIcon(ui::theme_icon(ui::ThemeGlyph::Close));
    close_btn_->setCursor(Qt::PointingHandCursor);
    connect(close_btn_, &QToolButton::clicked, this, &MarketsDetailPane::close_requested);
    r1->addWidget(sym_label_);
    r1->addWidget(name_label_, 1);
    r1->addWidget(close_btn_);
    hv->addLayout(r1);
    auto* r2 = new QHBoxLayout;
    r2->setSpacing(10);
    price_label_ = new QLabel(header);
    price_label_->setObjectName(QStringLiteral("mdpPrice"));
    change_label_ = new QLabel(header);
    change_label_->setObjectName(QStringLiteral("mdpChange"));
    session_label_ = new QLabel(header);
    session_label_->setObjectName(QStringLiteral("mdpSession"));
    r2->addWidget(price_label_);
    r2->addWidget(change_label_);
    r2->addStretch(1);
    r2->addWidget(session_label_);
    hv->addLayout(r2);
    root->addWidget(header);

    stack_ = new QStackedWidget(this);
    empty_ = new ui::EmptyState(stack_);
    empty_->set_compact(true);
    stack_->addWidget(empty_);

    auto* scroll = new QScrollArea(stack_);
    scroll->setObjectName(QStringLiteral("mdpScroll"));
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* body = new QWidget(scroll);
    body->setObjectName(QStringLiteral("mdpBody"));
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(8, 8, 8, 10);
    bl->setSpacing(8);

    chart_ = new ui::TimeSeriesChart(body);
    chart_->set_range_presets({QStringLiteral("1D"), QStringLiteral("5D"), QStringLiteral("1M"), QStringLiteral("3M"),
                               QStringLiteral("6M"), QStringLiteral("YTD"), QStringLiteral("1Y"), QStringLiteral("5Y")});
    chart_->set_show_legend(false);
    chart_->set_show_mode_buttons(false);
    chart_->setMinimumHeight(ui::metrics::height(200));
    connect(chart_, &ui::TimeSeriesChart::range_preset_selected, this, [this](const QString& preset) {
        preset_ = preset;
        load_timer_->start(); // debounced: a burst of clicks issues one request
    });
    bl->addWidget(chart_);

    tiles_ = new ui::KpiStrip(body);
    tiles_->set_min_tile_width(ui::metrics::width(150));
    for (const auto& t : kTiles) {
        ui::StatTile* tile =
            tiles_->add_tile(QString::fromLatin1(t.key), QCoreApplication::translate("MarketsDetailPane", t.label));
        tile->set_compact(true);
    }
    bl->addWidget(tiles_);

    auto* actions = new QHBoxLayout;
    actions->setSpacing(6);
    auto make_btn = [&](const char* name) {
        auto* b = new QPushButton(body);
        b->setObjectName(QString::fromLatin1(name));
        b->setCursor(Qt::PointingHandCursor);
        b->setMinimumHeight(ui::metrics::height(24));
        actions->addWidget(b);
        return b;
    };
    research_btn_ = make_btn("mdpResearch");
    trade_btn_ = make_btn("mdpTrade");
    news_btn_ = make_btn("mdpNews");
    connect(research_btn_, &QPushButton::clicked, this,
            [this]() { emit open_requested(QStringLiteral("equity_research"), symbol_); });
    connect(trade_btn_, &QPushButton::clicked, this,
            [this]() { emit open_requested(QStringLiteral("equity_trading"), symbol_); });
    connect(news_btn_, &QPushButton::clicked, this, [this]() { emit open_requested(QStringLiteral("news"), symbol_); });

    watch_btn_ = new QToolButton(body);
    watch_btn_->setPopupMode(QToolButton::InstantPopup);
    watch_btn_->setCursor(Qt::PointingHandCursor);
    watch_btn_->setMinimumHeight(ui::metrics::height(24));
    auto* watch_menu = new QMenu(watch_btn_);
    markets_set_sheet(watch_menu, markets_menu_css());
    connect(watch_menu, &QMenu::aboutToShow, this, [this, watch_menu]() {
        watch_menu->clear();
        if (!symbol_.isEmpty())
            equity_links::add_watchlist_entries(watch_menu, this, symbol_);
    });
    watch_btn_->setMenu(watch_menu);
    actions->addWidget(watch_btn_);

    add_btn_ = new QToolButton(body);
    add_btn_->setPopupMode(QToolButton::InstantPopup);
    add_btn_->setCursor(Qt::PointingHandCursor);
    add_btn_->setMinimumHeight(ui::metrics::height(24));
    add_btn_->setProperty("active", true); // the one accent action while the symbol is unlisted
    auto* add_menu = new QMenu(add_btn_);
    markets_set_sheet(add_menu, markets_menu_css());
    connect(add_menu, &QMenu::aboutToShow, this, [this, add_menu]() {
        add_menu->clear();
        for (int i = 0; i < panel_titles_.size(); ++i)
            connect(add_menu->addAction(panel_titles_.at(i)), &QAction::triggered, this,
                    [this, i]() { emit add_to_panel_requested(symbol_, i); });
        add_menu->addSeparator();
        connect(add_menu->addAction(tr("New panel...")), &QAction::triggered, this,
                [this]() { emit add_to_panel_requested(symbol_, -1); });
    });
    add_btn_->setMenu(add_menu);
    add_btn_->setVisible(false);
    actions->insertWidget(0, add_btn_);
    actions->addStretch(1);
    bl->addLayout(actions);
    bl->addStretch(1);

    scroll->setWidget(body);
    stack_->addWidget(scroll);
    root->addWidget(stack_, 1);
}

void MarketsDetailPane::set_tone(QLabel* label, const char* tone) {
    if (label->property("tone").toString() == QLatin1String(tone))
        return;
    label->setProperty("tone", QString::fromLatin1(tone));
    label->style()->unpolish(label);
    label->style()->polish(label);
}

void MarketsDetailPane::set_listed(bool listed) {
    listed_ = listed;
    add_btn_->setVisible(!listed && !symbol_.isEmpty());
}

void MarketsDetailPane::set_panel_titles(const QStringList& titles) {
    panel_titles_ = titles;
}

void MarketsDetailPane::set_symbol(const QString& symbol) {
    if (symbol == symbol_)
        return;
    drop_subscriptions();
    symbol_ = symbol;
    series_ = -1;
    perf_state_ = PerfState::None;
    perf_error_.clear();
    perf_1m_ = perf_ytd_ = perf_1y_ = perf_vol20_ = std::numeric_limits<double>::quiet_NaN();
    chart_->clear();
    stack_->setCurrentIndex(symbol.isEmpty() ? 0 : 1);
    add_btn_->setVisible(!listed_ && !symbol.isEmpty());
    update_header();
    update_quote_tiles();
    update_perf_tiles();
    if (!symbol.isEmpty() && isVisible())
        load_timer_->start(); // fast arrow-key travel issues one history request after it stops
}

void MarketsDetailPane::on_rows_changed(const QSet<QString>& symbols) {
    if (symbol_.isEmpty() || !symbols.contains(symbol_))
        return;
    update_header();
    update_quote_tiles();
}

void MarketsDetailPane::update_header() {
    const MarketRow* r = store_ ? store_->row(symbol_) : nullptr;
    QString ticker = core::instrument::display_symbol(symbol_);
    if (ticker.isEmpty())
        ticker = symbol_;
    sym_label_->setText(symbol_.isEmpty() ? QString() : ticker);
    QStringList name_parts;
    if (store_ && !symbol_.isEmpty()) {
        const QString name = store_->display_name(symbol_);
        if (name != ticker)
            name_parts << name;
    }
    if (r && !r->quote.exchange_name.isEmpty())
        name_parts << r->quote.exchange_name;
    const QString name_text = name_parts.join(QStringLiteral(" · "));
    name_label_->setText(name_text);
    name_label_->setToolTip(name_text);

    if (!r || !r->has_quote) {
        price_label_->setText(symbol_.isEmpty() ? QString()
                                                : (r && !r->error.isEmpty() ? QStringLiteral("-") : QStringLiteral("...")));
        change_label_->setText(QString());
        session_label_->setText(r && !r->error.isEmpty() ? r->error.left(60) : QString());
        set_tone(session_label_, r && !r->error.isEmpty() ? "warn" : "dim");
        set_tone(price_label_, "stale");
        return;
    }
    const services::QuoteData& q = r->quote;
    const fmt::InstrumentFmt inst = mdp_inst(symbol_, q);
    price_label_->setText(mdp_price(*r, inst, q.price));
    const auto state = store_->row_state(*r);
    set_tone(price_label_, state == markets_logic::RowState::Ok
                               ? ""
                               : (state == markets_logic::RowState::Closed ? "closed" : "stale"));
    change_label_->setText(QStringLiteral("%1 (%2)").arg(fmt::price_change(q.change, q.price, inst), fmt::pct(q.change_pct)));
    const int dir = markets_logic::direction_of(q.change);
    set_tone(change_label_, dir > 0 ? "pos" : (dir < 0 ? "neg" : ""));

    // Session: shape glyph first, colour second.
    const QTimeZone tz = q.exchange_timezone.isEmpty() ? QTimeZone() : QTimeZone(q.exchange_timezone.toUtf8());
    QString session;
    const char* tone = "dim";
    if (q.market_state == QLatin1String("REGULAR")) {
        session = QString::fromUtf8("\xe2\x97\x8f ") + tr("Open %1").arg(q.market_time > 0 ? fmt::time(q.market_time, false, tz)
                                                                                         : QString());
        tone = "ok";
    } else if (q.market_state == QLatin1String("PRE") || q.market_state == QLatin1String("POST")) {
        const bool pre = q.market_state == QLatin1String("PRE");
        const double ext = pre ? q.pre_market_change_pct : q.post_market_change_pct;
        const qint64 ext_t = pre ? q.pre_market_time : q.post_market_time;
        session = QString::fromUtf8("\xe2\x97\x90 ") + (pre ? tr("Pre-market") : tr("Post-market"));
        if (ext_t > 0)
            session += QLatin1Char(' ') + fmt::time(ext_t, false, tz);
        if (std::isfinite(ext))
            session += QLatin1Char(' ') + fmt::pct(ext);
        tone = "warn";
    } else {
        session = QString::fromUtf8("\xe2\x97\x8b ") + tr("Closed");
        if (q.market_time > 0)
            session += tr(" · as of %1").arg(fmt::date(q.market_time, fmt::DateStyle::Medium, tz));
        else if (!q.market_date.isEmpty())
            session += tr(" · as of %1").arg(q.market_date);
    }
    if (!r->error.isEmpty()) {
        session += tr(" · last refresh failed");
        tone = "warn";
    }
    session_label_->setText(session);
    session_label_->setToolTip(r->error);
    set_tone(session_label_, tone);

    const bool tradeable = r->cls == AssetClass::Equity || r->cls == AssetClass::Etf;
    if (trade_btn_->isVisible() != tradeable)
        trade_btn_->setVisible(tradeable);
}

void MarketsDetailPane::update_quote_tiles() {
    const MarketRow* r = store_ ? store_->row(symbol_) : nullptr;
    auto tile = [this](const char* key) { return tiles_->tile(QString::fromLatin1(key)); };
    const QStringList quote_keys = {"open", "prev", "day", "w52", "vol", "d50", "d200", "mkt"};
    if (!r || !r->has_quote) {
        for (const QString& k : quote_keys)
            if (auto* t = tiles_->tile(k)) {
                if (r && !r->error.isEmpty())
                    t->set_state(ui::StatTile::State::Error, r->error);
                else
                    t->set_state(ui::StatTile::State::Loading);
            }
        return;
    }
    const services::QuoteData& q = r->quote;
    const fmt::InstrumentFmt inst = mdp_inst(symbol_, q);
    auto price = [&](double v) { return mdp_price(*r, inst, v); };
    const auto normal = store_->is_stale(*r) ? ui::StatTile::State::Stale : ui::StatTile::State::Normal;
    const QString stale_reason = r->error.isEmpty() ? QString() : r->error;

    auto set = [&](const char* key, const QString& value, const QString& sub = {}) {
        if (auto* t = tile(key)) {
            t->set_value(value);
            t->set_sub_label(sub);
            t->set_state(normal, stale_reason);
        }
    };
    set("open", price(q.open));
    set("prev", price(q.previous_close));
    set("day", mdp_pos(q.low) && mdp_pos(q.high) ? QStringLiteral("%1-%2").arg(price(q.low), price(q.high))
                                                 : fmt::placeholder());
    if (mdp_pos(q.fifty_two_week_low) && mdp_pos(q.fifty_two_week_high))
        set("w52", QStringLiteral("%1-%2").arg(price(q.fifty_two_week_low), price(q.fifty_two_week_high)),
            tr("%1 from high").arg(fmt::pct((q.price / q.fifty_two_week_high - 1.0) * 100.0, 1)));
    else
        set("w52", fmt::placeholder());
    // VOLUME: not reported for currency pairs; for an index only when the feed has one.
    if (!mcols::applies(mcols::Volume, r->cls, q)) {
        set("vol", tr("n/a"));
        if (auto* t = tile("vol"))
            t->set_tooltip_text(tr("Not reported for %1.").arg(mcols::class_word(r->cls)));
    } else {
        set("vol", q.volume > 0 ? fmt::volume(q.volume) : fmt::placeholder(),
            mdp_pos(q.average_volume) && q.volume > 0 ? tr("%1x of average").arg(fmt::number(q.volume / q.average_volume, 2))
                                                      : QString());
        if (auto* t = tile("vol"))
            t->set_tooltip_text(QString());
    }
    set("d50", price(q.fifty_day_average));
    set("d200", price(q.two_hundred_day_average));

    // MARKET: the session in words, with the next change from the venue calendar.
    QString market = q.market_state == QLatin1String("REGULAR") ? tr("Open")
                     : q.market_state == QLatin1String("PRE")   ? tr("Pre-market")
                     : q.market_state == QLatin1String("POST")  ? tr("Post-market")
                     : q.market_state == QLatin1String("CLOSED") ? tr("Closed")
                                                                 : fmt::placeholder();
    QString sub;
    if (const auto* venue = core::market::venue_for_symbol(symbol_, q.exchange)) {
        const auto st = core::market::session_status(*venue, QDateTime::currentDateTimeUtc());
        if (q.market_state.isEmpty())
            market = core::market::session_state_label(st.state);
        if (st.next_change_utc.isValid()) {
            const qint64 secs = QDateTime::currentDateTimeUtc().secsTo(st.next_change_utc);
            const QString in = secs >= 3600 ? tr("%1h %2m").arg(secs / 3600).arg((secs % 3600) / 60)
                                            : tr("%1m").arg(std::max<qint64>(1, secs / 60));
            sub = st.state == core::market::SessionState::Open ? tr("closes in %1").arg(in) : tr("opens in %1").arg(in);
        }
    }
    set("mkt", market, sub);
}

void MarketsDetailPane::update_perf_tiles() {
    const QStringList keys = {"m1", "ytd", "y1", "vol20"};
    if (symbol_.isEmpty())
        return;
    if (perf_state_ != PerfState::Ready) {
        for (const QString& k : keys)
            if (auto* t = tiles_->tile(k)) {
                if (perf_state_ == PerfState::Failed)
                    t->set_state(ui::StatTile::State::Error, tr("Daily history unavailable"));
                else
                    t->set_state(ui::StatTile::State::Loading);
            }
        return;
    }
    auto pct_tile = [this](const char* key, double v) {
        auto* t = tiles_->tile(QString::fromLatin1(key));
        if (!t)
            return;
        t->set_state(ui::StatTile::State::Normal);
        if (!std::isfinite(v)) {
            t->set_value(fmt::placeholder());
            t->set_tone(ui::StatTile::Tone::Neutral);
            return;
        }
        t->set_value(fmt::pct(v));
        const int d = markets_logic::direction_of(v);
        t->set_tone(d > 0 ? ui::StatTile::Tone::Positive
                          : (d < 0 ? ui::StatTile::Tone::Negative : ui::StatTile::Tone::Neutral));
    };
    pct_tile("m1", perf_1m_);
    pct_tile("ytd", perf_ytd_);
    pct_tile("y1", perf_1y_);
    if (auto* t = tiles_->tile(QStringLiteral("vol20"))) {
        t->set_state(ui::StatTile::State::Normal);
        t->set_value(std::isfinite(perf_vol20_) ? fmt::pct(perf_vol20_, 1, false) : fmt::placeholder());
    }
}

QString MarketsDetailPane::chart_topic_for(const QString& preset) const {
    using services::MarketDataService;
    if (preset == QLatin1String("1D"))
        return MarketDataService::history_topic(symbol_, QStringLiteral("1d"), QStringLiteral("5m"));
    if (preset == QLatin1String("5D"))
        return MarketDataService::history_topic(symbol_, QStringLiteral("5d"), QStringLiteral("30m"));
    if (preset == QLatin1String("5Y"))
        return MarketDataService::history_topic(symbol_, QStringLiteral("5y"), QStringLiteral("1wk"));
    return MarketDataService::performance_history_topic(symbol_); // 1y/1d, shared with the tiles
}

void MarketsDetailPane::load_histories() {
    if (symbol_.isEmpty() || !isVisible())
        return;
    auto& hub = datahub::DataHub::instance();
    // Performance series (1M / YTD / 1Y / 20D VOL tiles): subscribed while the pane is visible.
    const QString perf = services::MarketDataService::performance_history_topic(symbol_);
    if (perf != perf_topic_) {
        if (!perf_topic_.isEmpty()) {
            hub.unsubscribe(this, perf_topic_);
            hub.unsubscribe_errors(this, perf_topic_);
        }
        perf_topic_ = perf;
        perf_state_ = PerfState::Loading;
        update_perf_tiles();
        hub.subscribe(this, perf, [this, perf](const QVariant& v) {
            if (perf != perf_topic_)
                return;
            apply_perf(v);
            if (chart_topic_ == perf)
                apply_history(perf, v);
        });
        hub.subscribe_errors(this, perf, [this, perf](const QString& err) {
            if (perf != perf_topic_)
                return;
            perf_state_ = PerfState::Failed;
            perf_error_ = err;
            update_perf_tiles();
            if (chart_topic_ == perf && series_ < 0)
                chart_->set_state(ui::ChartState::Error, err);
        });
    }
    // Chart: the preset's series (the same topic as above for 1M..1Y).
    const QString topic = chart_topic_for(preset_);
    if (topic == chart_topic_ && series_ >= 0) {
        chart_->set_active_preset(preset_);
        return;
    }
    if (!chart_topic_.isEmpty() && chart_topic_ != perf_topic_) {
        hub.unsubscribe(this, chart_topic_);
        hub.unsubscribe_errors(this, chart_topic_);
    }
    chart_topic_ = topic;
    series_ = -1;
    chart_->set_state(ui::ChartState::Loading, tr("Loading price history..."));
    if (topic == perf_topic_) {
        const QVariant cached = hub.peek(topic);
        if (cached.isValid())
            apply_history(topic, cached);
        hub.request(topic);
        return;
    }
    hub.subscribe(this, topic, [this, topic](const QVariant& v) { apply_history(topic, v); });
    hub.subscribe_errors(this, topic, [this, topic](const QString& err) {
        if (topic == chart_topic_ && series_ < 0)
            chart_->set_state(ui::ChartState::Error, err);
    });
    hub.request(topic);
}

void MarketsDetailPane::apply_history(const QString& topic, const QVariant& value) {
    if (topic != chart_topic_ || !value.canConvert<QVector<services::HistoryPoint>>())
        return;
    const auto points = value.value<QVector<services::HistoryPoint>>();
    QVector<double> xs;
    QVector<double> ys;
    xs.reserve(points.size());
    ys.reserve(points.size());
    for (const auto& p : points) {
        if (p.timestamp <= 0 || !(p.close > 0))
            continue;
        xs.append(static_cast<double>(p.timestamp) * 1000.0);
        ys.append(p.close);
    }
    chart_->clear();
    series_ = -1;
    if (ys.size() < 2) {
        chart_->set_state(ui::ChartState::Empty, tr("No price history for this range"));
        return;
    }
    apply_chart_formatter();
    ui::TimeSeriesChart::SeriesSpec spec;
    spec.name = core::instrument::display_symbol(symbol_).isEmpty() ? symbol_ : core::instrument::display_symbol(symbol_);
    spec.xs = xs;
    spec.ys = ys;
    spec.style = ui::TimeSeriesChart::Style::Area;
    spec.color = ys.last() >= ys.first() ? ui::ChartColorRole::Positive : ui::ChartColorRole::Negative;
    series_ = chart_->add_series(spec);
    chart_->set_export_name(spec.name + QLatin1Char('_') + preset_);
    chart_->set_state(ui::ChartState::Ready);
    const bool own_range = preset_ == QLatin1String("1D") || preset_ == QLatin1String("5D") || preset_ == QLatin1String("5Y");
    chart_->set_active_preset(own_range ? QString() : preset_);
}

void MarketsDetailPane::apply_perf(const QVariant& value) {
    if (!value.canConvert<QVector<services::HistoryPoint>>())
        return;
    const auto bars = value.value<QVector<services::HistoryPoint>>();
    const MarketRow* r = store_ ? store_->row(symbol_) : nullptr;
    const double live = r && r->has_quote ? r->quote.price : services::kMarketDataAbsent;
    const int ppy = r && r->cls == AssetClass::Crypto ? 365 : 252;
    const auto stats = services::compute_performance(bars, live, ppy);
    perf_1m_ = stats.change_1m_pct;
    perf_ytd_ = stats.change_ytd_pct;
    perf_1y_ = stats.change_1y_pct;
    perf_vol20_ = stats.volatility_20d_pct;
    perf_state_ = PerfState::Ready;
    update_perf_tiles();
}

void MarketsDetailPane::drop_subscriptions() {
    load_timer_->stop();
    auto& hub = datahub::DataHub::instance();
    for (const QString& t : {chart_topic_, perf_topic_}) {
        if (t.isEmpty())
            continue;
        hub.unsubscribe(this, t);
        hub.unsubscribe_errors(this, t);
    }
    chart_topic_.clear();
    perf_topic_.clear();
    series_ = -1;
}

void MarketsDetailPane::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (!symbol_.isEmpty())
        load_timer_->start();
}

void MarketsDetailPane::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    // No history subscription while nobody can see the pane.
    drop_subscriptions();
    if (perf_state_ == PerfState::Loading)
        perf_state_ = PerfState::None;
}

void MarketsDetailPane::refresh_theme() {
    // Tiles and the chart paint from the theme; labels use tone properties.
    update_header();
}

void MarketsDetailPane::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(e);
}

void MarketsDetailPane::retranslate() {
    empty_->set_title(tr("No symbol selected"));
    empty_->set_message(tr("Select a row to see its chart and key figures."));
    close_btn_->setToolTip(tr("Close the detail pane (Ctrl+D)"));
    close_btn_->setAccessibleName(tr("Close the detail pane"));
    research_btn_->setText(tr("RESEARCH"));
    trade_btn_->setText(tr("TRADE"));
    news_btn_->setText(tr("NEWS"));
    watch_btn_->setText(tr("+ WATCHLIST"));
    add_btn_->setText(tr("ADD TO PANEL"));
    for (const auto& t : kTiles)
        if (auto* tile = tiles_->tile(QString::fromLatin1(t.key)))
            tile->set_label(QCoreApplication::translate("MarketsDetailPane", t.label));
    update_header();
    update_quote_tiles();
    update_perf_tiles();
}

} // namespace fincept::screens
