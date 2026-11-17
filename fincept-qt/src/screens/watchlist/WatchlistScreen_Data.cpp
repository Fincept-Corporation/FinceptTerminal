// WatchlistScreen_Data.cpp — lists and entries, DataHub subscriptions (quotes
// for every row; fundamentals, sparklines and daily history as the visible
// columns / views need them), per-list layout, status, footer and the
// analytics views.

#include "screens/watchlist/WatchlistScreen.h"

#include "core/logging/Logger.h"
#include "core/market/InstrumentCurrency.h"
#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolContext.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/watchlist/WatchlistAnalyticsViews.h"
#include "screens/watchlist/WatchlistColumns.h"
#include "screens/watchlist/WatchlistDetailPane.h"
#include "screens/watchlist/WatchlistScreenInternal.h"
#include "screens/watchlist/WatchlistView.h"
#include "services/markets/FxRateService.h"
#include "services/markets/MarketPerformance.h"
#include "services/markets/WatchlistAlertService.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDateTime>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPointer>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::screens {

namespace fmt = fincept::ui::formatting;
namespace colors = fincept::ui::colors;
namespace fonts = fincept::ui::fonts;
namespace wcols = fincept::screens::watchlist_columns;
using namespace wl_internal;

namespace {

constexpr qint64 kWlsdStaleMs = 150'000;

/// An InfoData that says "nothing known" (every number NaN) — for a symbol
/// whose fundamentals request failed, so its cells stop showing "loading".
services::InfoData wlsd_empty_info(const QString& sym) {
    services::InfoData i;
    i.symbol = sym;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    i.market_cap = i.pe_ratio = i.forward_pe = i.price_to_book = i.dividend_yield = i.beta = nan;
    i.week52_high = i.week52_low = i.avg_volume = i.eps = i.roe = i.profit_margin = nan;
    i.debt_to_equity = i.current_ratio = nan;
    return i;
}

QString wlsd_horizon_column(const QString& horizon) {
    if (horizon == QLatin1String("1w"))
        return QStringLiteral("perf_1w");
    if (horizon == QLatin1String("1m"))
        return QStringLiteral("perf_1m");
    if (horizon == QLatin1String("3m"))
        return QStringLiteral("perf_3m");
    if (horizon == QLatin1String("ytd"))
        return QStringLiteral("perf_ytd");
    if (horizon == QLatin1String("1y"))
        return QStringLiteral("perf_1y");
    return QStringLiteral("change_pct");
}

} // namespace

// ── Lists ────────────────────────────────────────────────────────────────────

QString WatchlistScreen::current_list_name() const {
    for (const auto& wl : watchlists_) {
        if (wl.id == current_wl_id_)
            return wl.name;
    }
    return {};
}

int WatchlistScreen::row_of_list(const QString& id) const {
    for (int i = 0; i < watchlists_.size(); ++i) {
        if (watchlists_[i].id == id)
            return i;
    }
    return -1;
}

void WatchlistScreen::load_watchlists() {
    auto& repo = WatchlistRepository::instance();
    auto r = repo.list_all();
    if (r.is_err()) {
        LOG_ERROR("Watchlist", "Failed to load watchlists: " + QString::fromStdString(r.error()));
        watchlists_.clear();
    } else {
        watchlists_ = r.value();
    }
    // At least one list. Left empty on purpose (seeding symbols the user never
    // chose confused the AI-chat flow).
    if (watchlists_.isEmpty()) {
        auto cr = repo.create(QStringLiteral("Default"), QColor(colors::AMBER()).name());
        if (cr.is_ok()) {
            watchlists_.append(cr.value());
            repo.set_default(cr.value().id);
        }
    }
    if (auto c = repo.stock_counts(); c.is_ok())
        counts_ = c.value();

    rebuild_sidebar();
    wl_count_->setText(tr("%n list(s)", "", static_cast<int>(watchlists_.size())));

    if (watchlists_.isEmpty()) {
        current_wl_id_.clear();
        stocks_.clear();
        model_->set_stocks(QString(), {});
        update_empty_state();
        update_list_actions();
        return;
    }
    // Stay on the list being viewed (reloads come from cloud pulls and MCP events);
    // else the default list; else the first.
    int row = row_of_list(current_wl_id_);
    if (row < 0) {
        row = 0;
        for (int i = 0; i < watchlists_.size(); ++i) {
            if (watchlists_[i].is_default) {
                row = i;
                break;
            }
        }
    }
    if (wl_list_->currentRow() == row)
        on_watchlist_selected(row); // same row: currentRowChanged would not fire
    else
        wl_list_->setCurrentRow(row);
}

void WatchlistScreen::rebuild_sidebar() {
    QSignalBlocker block(wl_list_);
    wl_list_->clear();
    int current = -1;
    for (int i = 0; i < watchlists_.size(); ++i) {
        const auto& wl = watchlists_[i];
        auto* item = new QListWidgetItem(wl.name);
        item->setData(kListIdRole, wl.id);
        item->setData(kListColorRole, wl.color);
        item->setData(kListCountRole, counts_.value(wl.id));
        item->setData(kListDefaultRole, wl.is_default);
        QString tip = wl.name;
        if (!wl.description.isEmpty())
            tip += QLatin1Char('\n') + wl.description;
        tip += QLatin1Char('\n') + tr("%n symbol(s)", "", counts_.value(wl.id));
        if (wl.is_default)
            tip += QLatin1Char('\n') + tr("Default list");
        item->setToolTip(tip);
        wl_list_->addItem(item);
        if (wl.id == current_wl_id_)
            current = i;
    }
    if (current >= 0)
        wl_list_->setCurrentRow(current);
}

void WatchlistScreen::on_watchlist_selected(int row) {
    if (row < 0 || row >= watchlists_.size())
        return;
    const QString id = watchlists_[row].id;
    const bool switched = id != current_wl_id_;
    if (switched) {
        if (layout_save_timer_->isActive())
            save_layout_now(); // the previous list's layout
        view_->clearSelection();
        saved_selection_.clear();
        saved_current_.clear();
        saved_scroll_ = 0;
        if (!filter_input_->text().isEmpty()) {
            QSignalBlocker block(filter_input_);
            filter_input_->clear();
            proxy_->set_filter_text(QString());
        }
    }
    current_wl_id_ = id;
    panel_title_->setText(watchlists_[row].name.toUpper());
    panel_title_->setToolTip(watchlists_[row].description);
    if (switched)
        apply_layout_for_current_list();
    load_stocks();
    ScreenStateManager::instance().notify_changed(this);
    update_list_actions();
}

void WatchlistScreen::load_stocks() {
    if (current_wl_id_.isEmpty())
        return;
    auto r = WatchlistRepository::instance().get_stocks(current_wl_id_);
    if (r.is_err()) {
        LOG_WARN("Watchlist", "Could not load the list: " + QString::fromStdString(r.error()));
        stocks_.clear();
    } else {
        stocks_ = r.value();
    }
    if (counts_.value(current_wl_id_) != stocks_.size()) {
        counts_.insert(current_wl_id_, static_cast<int>(stocks_.size()));
        const int row = row_of_list(current_wl_id_);
        if (QListWidgetItem* item = row >= 0 ? wl_list_->item(row) : nullptr)
            item->setData(kListCountRole, static_cast<int>(stocks_.size()));
    }
    // Same entries in the same order (a re-show, an MCP / cloud reload): update
    // in place — no model reset, so selection, scroll and expansion stay put.
    const QStringList before = model_->symbols();
    const bool same_list = model_->list_id() == current_wl_id_;
    bool same_rows = same_list && before.size() == stocks_.size();
    for (qsizetype i = 0; same_rows && i < stocks_.size(); ++i)
        same_rows = before[i] == stocks_[i].symbol;
    if (same_rows) {
        for (const auto& s : stocks_) {
            const auto* st = model_->state(s.symbol);
            if (st && (st->stock.notes != s.notes || st->stock.group_name != s.group_name))
                model_->set_stock_fields(s.symbol, s.notes, s.group_name);
        }
    } else {
        model_->set_stocks(current_wl_id_, stocks_);
    }
    refresh_alert_summaries();
    QSet<QString> now_set;
    for (const auto& s : stocks_)
        now_set.insert(s.symbol);
    const bool set_changed = !same_list || QSet<QString>(before.cbegin(), before.cend()) != now_set;
    if (isVisible() && (set_changed || !hub_active_))
        hub_resubscribe(/*force=*/set_changed); // a re-show alone refetches nothing that is fresh
    update_empty_state();
    update_quote_status();
    schedule_footer();
    schedule_views();
    // Updated in place (no model reset, whose handler does this): a re-show with
    // nothing selected still opens on the first row's detail.
    if (same_rows)
        select_first_row_if_none();
}

// ── Layout ───────────────────────────────────────────────────────────────────

void WatchlistScreen::apply_layout_for_current_list() {
    applying_layout_ = true;
    layout_ = WatchlistColumnPrefs::load(current_wl_id_);
    {
        QSignalBlocker block(group_box_);
        const int gi = group_box_->findData(layout_.grouping);
        group_box_->setCurrentIndex(gi >= 0 ? gi : 0);
    }
    model_->set_grouping(watchlist_grouping_from_key(layout_.grouping));
    view_->setRootIsDecorated(model_->grouped());
    view_->apply_layout(layout_);
    map_view_->set_mode(layout_.map_size, layout_.map_horizon);
    set_view_mode(layout_.view, false);
    set_detail_visible(layout_.detail_visible, false);
    applying_layout_ = false;
    update_data_needs();
}

void WatchlistScreen::layout_edited() {
    if (applying_layout_ || current_wl_id_.isEmpty())
        return;
    view_->capture_layout(layout_);
    // Right after a reset (regroup, reload) the view shows every group collapsed until
    // the queued restore runs; capturing that would collapse them all for good.
    if (model_->grouped() && !expansion_restore_pending_)
        layout_.collapsed = view_->collapsed_groups();
    layout_save_timer_->start();
}

void WatchlistScreen::save_layout_now() {
    layout_save_timer_->stop();
    if (!current_wl_id_.isEmpty())
        WatchlistColumnPrefs::save(current_wl_id_, layout_);
}

// ── DataHub ──────────────────────────────────────────────────────────────────

void WatchlistScreen::hub_resubscribe(bool force) {
    auto& hub = datahub::DataHub::instance();
    // The symbol set changed (another list, an add / remove): drop every topic
    // this screen holds and subscribe the current set.
    hub.unsubscribe(this);
    info_subs_.clear();
    spark_subs_.clear();
    hist_subs_.clear();
    hub_active_ = false;

    const QStringList syms = model_->symbols();
    {
        const QSet<QString> keep(syms.cbegin(), syms.cend());
        for (auto it = history_.begin(); it != history_.end();) {
            if (keep.contains(it.key()))
                ++it;
            else
                it = history_.erase(it);
        }
        history_failed_.intersect(keep);
    }
    if (syms.isEmpty())
        return;

    // Display names (disk-cached by the service; the first call per symbol resolves them).
    QPointer<WatchlistScreen> self = this;
    services::MarketDataService::instance().resolve_names(syms, [self](const QHash<QString, QString>& m) {
        if (self && !m.isEmpty())
            self->model_->set_names(m);
    });

    QStringList topics;
    topics.reserve(syms.size());
    for (const QString& sym : syms) {
        const QString topic = services::MarketDataService::quote_topic(sym);
        topics << topic;
        hub.subscribe(this, topic, [this, sym](const QVariant& v) {
            if (v.canConvert<services::QuoteData>())
                on_quote(sym, v.value<services::QuoteData>());
        });
        // The producer's reason ("Python is not set up yet", "rate limited",
        // unknown symbol) goes on the row and, when every row fails, the banner.
        hub.subscribe_errors(this, topic, [this, sym](const QString& err) {
            QString reason = err.trimmed();
            if (reason.isEmpty())
                reason = tr("The data source reported an error.");
            model_->set_error(sym, reason);
            schedule_footer();
        });
    }
    // force: the list changed by the user's hand (or REFRESH); resolve new symbols now, not at
    // the next tick. A re-show does not: subscribe() hands over every fresh cached quote and
    // cold-starts only the topics without one, so switching tabs sends no request for them.
    if (force)
        hub.request(topics, /*force=*/true);
    hub_active_ = true;
    update_data_needs();
}

void WatchlistScreen::hub_unsubscribe_all() {
    if (!hub_active_)
        return;
    datahub::DataHub::instance().unsubscribe(this);
    info_subs_.clear();
    spark_subs_.clear();
    hist_subs_.clear();
    hub_active_ = false;
}

bool WatchlistScreen::history_columns_visible() const {
    for (const QString& id : view_->visible_column_ids()) {
        const int c = wcols::index_of(id);
        if (c >= 0 && (wcols::def(c).needs & kWlNeedHistory))
            return true;
    }
    return false;
}

bool WatchlistScreen::needs_info_for_all() const {
    for (const QString& id : view_->visible_column_ids()) {
        const int c = wcols::index_of(id);
        if (c >= 0 && (wcols::def(c).needs & kWlNeedInfo))
            return true;
    }
    const WatchlistGrouping g = model_->grouping();
    if (g == WatchlistGrouping::Sector || g == WatchlistGrouping::Industry || g == WatchlistGrouping::Country)
        return true;
    return layout_.view == QLatin1String("map") && layout_.map_size == QLatin1String("cap");
}

bool WatchlistScreen::needs_history_for_all() const {
    if (layout_.view == QLatin1String("correlation"))
        return true;
    if (layout_.view == QLatin1String("map") && layout_.map_horizon != QLatin1String("1d"))
        return true;
    // Sorting by a performance column needs every row's value.
    if (view_->is_sorted()) {
        const int c = view_->header()->sortIndicatorSection();
        if (c >= 0 && c < wcols::count() && (wcols::def(c).needs & kWlNeedHistory))
            return true;
    }
    return false;
}

void WatchlistScreen::update_data_needs() {
    if (!hub_active_)
        return;
    const QStringList syms = model_->symbols();
    const QStringList vis = view_->visible_column_ids();
    if (needs_info_for_all())
        ensure_info(syms);
    if (vis.contains(QStringLiteral("spark")))
        ensure_spark(syms);
    if (needs_history_for_all())
        ensure_history(syms);
    else if (history_columns_visible())
        history_timer_->start();
    if (detail_->isVisible()) {
        const QString sym = view_->current_symbol();
        if (!sym.isEmpty()) {
            ensure_info({sym});
            ensure_history({sym});
        }
    }
    sync_fx();
}

void WatchlistScreen::ensure_info(const QStringList& symbols) {
    if (!hub_active_)
        return;
    auto& hub = datahub::DataHub::instance();
    QStringList topics;
    for (const QString& sym : symbols) {
        if (sym.isEmpty() || info_subs_.contains(sym) || !model_->state(sym))
            continue;
        info_subs_.insert(sym);
        const QString topic = services::MarketDataService::info_topic(sym);
        hub.subscribe(this, topic, [this, sym](const QVariant& v) {
            if (v.canConvert<services::InfoData>())
                model_->set_info(sym, v.value<services::InfoData>());
        });
        hub.subscribe_errors(this, topic, [this, sym](const QString&) {
            // Indices / FX / crypto have no fundamentals: stop showing "loading".
            const auto* r = model_->state(sym);
            if (r && !r->has_info)
                model_->set_info(sym, wlsd_empty_info(sym));
        });
        topics << topic;
    }
    if (!topics.isEmpty())
        hub.request(topics);
}

void WatchlistScreen::ensure_spark(const QStringList& symbols) {
    if (!hub_active_)
        return;
    auto& hub = datahub::DataHub::instance();
    QStringList topics;
    for (const QString& sym : symbols) {
        if (sym.isEmpty() || spark_subs_.contains(sym) || !model_->state(sym))
            continue;
        spark_subs_.insert(sym);
        const QString topic = services::MarketDataService::sparkline_topic(sym);
        hub.subscribe(this, topic, [this, sym](const QVariant& v) {
            if (v.canConvert<QVector<double>>())
                model_->set_spark(sym, v.value<QVector<double>>());
        });
        topics << topic;
    }
    if (!topics.isEmpty())
        hub.request(topics);
}

void WatchlistScreen::ensure_history(const QStringList& symbols) {
    if (!hub_active_)
        return;
    auto& hub = datahub::DataHub::instance();
    QStringList topics;
    for (const QString& sym : symbols) {
        if (sym.isEmpty() || hist_subs_.contains(sym) || !model_->state(sym))
            continue;
        hist_subs_.insert(sym);
        // The shared 1y/1d series: one fetch per symbol for every screen that derives performance.
        const QString topic = services::MarketDataService::performance_history_topic(sym);
        if (!history_.contains(sym))
            model_->set_history_state(sym, WatchlistHistoryState::Loading);
        hub.subscribe(this, topic, [this, sym](const QVariant& v) {
            if (v.canConvert<QVector<services::HistoryPoint>>())
                on_history(sym, v.value<QVector<services::HistoryPoint>>());
        });
        hub.subscribe_errors(this, topic, [this, sym](const QString&) {
            if (history_.contains(sym))
                return; // keep the last good series
            history_failed_.insert(sym);
            model_->set_history_state(sym, WatchlistHistoryState::Failed);
            schedule_views();
        });
        topics << topic;
    }
    if (!topics.isEmpty())
        hub.request(topics);
}

void WatchlistScreen::request_visible_history() {
    if (!hub_active_ || !history_columns_visible() || view_stack_->currentWidget() != table_page_)
        return;
    ensure_history(view_->visible_symbols(5));
}

void WatchlistScreen::on_quote(const QString& sym, const services::QuoteData& q) {
    const qint64 published = datahub::DataHub::instance().last_publish_ms(services::MarketDataService::quote_topic(sym));
    // The quote's age runs from when Yahoo answered (a cached quote keeps that time).
    model_->set_quote(sym, q, services::MarketDataService::quote_fetched_ms(q, published));
    last_quote_ms_ = std::max(last_quote_ms_, published > 0 ? published : QDateTime::currentMSecsSinceEpoch());
    // Performance runs to the live price: recompute from the cached daily series.
    const auto it = history_.constFind(sym);
    if (it != history_.constEnd() && q.price > 0) {
        const int ppy = watchlist_asset_class(sym, q.quote_type) == QLatin1String("crypto") ? 365 : 252;
        model_->set_perf(sym, services::compute_performance(it.value(), q.price, ppy));
    }
}

void WatchlistScreen::on_history(const QString& sym, const QVector<services::HistoryPoint>& bars) {
    history_.insert(sym, bars);
    history_failed_.remove(sym);
    const auto* r = model_->state(sym);
    const double live = r && r->has_quote && r->quote.price > 0 ? r->quote.price : services::kMarketDataAbsent;
    const int ppy = r && r->asset_class() == QLatin1String("crypto") ? 365 : 252;
    model_->set_perf(sym, services::compute_performance(bars, live, ppy));
    schedule_views();
}

// ── Status ───────────────────────────────────────────────────────────────────

void WatchlistScreen::update_quote_status() {
    if (current_wl_id_.isEmpty() || stocks_.isEmpty()) {
        stock_count_->setText(current_wl_id_.isEmpty() ? QString() : tr("0 symbols"));
        stock_count_->setToolTip(QString());
        status_banner_->setVisible(false);
        return;
    }
    int failed = 0;
    int failed_without_quote = 0;
    QStringList reasons;
    QStringList lines;
    for (const auto& r : model_->states()) {
        if (r.error.isEmpty())
            continue;
        ++failed;
        if (!r.has_quote)
            ++failed_without_quote;
        if (!reasons.contains(r.error))
            reasons << r.error;
        lines << QStringLiteral("%1: %2").arg(r.stock.symbol, r.error);
    }
    const int n = static_cast<int>(model_->states().size());
    QString text = tr("%n symbol(s)", "", n);
    if (failed > 0)
        text += tr(" · %1 failed").arg(failed);
    stock_count_->setText(text);
    QString tip;
    if (failed > 0) {
        tip = tr("Quote errors (hover a row for its own):") + QLatin1Char('\n') + lines.mid(0, 15).join(QLatin1Char('\n'));
        if (lines.size() > 15)
            tip += QLatin1Char('\n') + tr("…and %1 more").arg(lines.size() - 15);
    }
    stock_count_->setToolTip(tip);

    // The banner only when EVERY symbol fails: then it is a list-wide condition
    // (Python not set up, offline, rate limited) and its reason is said once.
    if (failed != n) {
        status_banner_->setVisible(false);
        return;
    }
    QString msg;
    if (reasons.size() == 1)
        msg = n == 1 ? QStringLiteral("%1: %2").arg(model_->states().first().stock.symbol, reasons.first())
                     : reasons.first();
    else
        msg = tr("No quote could be loaded for any symbol — hover a row for its reason.");
    if (failed_without_quote < failed)
        msg += QLatin1Char(' ') + tr("Showing the last quotes received.");
    msg += QLatin1Char(' ') + tr("Press REFRESH to retry.");
    status_banner_->setText(msg);
    // Status, not direction: DANGER when nothing is on screen, WARNING when older quotes are shown.
    const QString fg = failed_without_quote == failed ? QString(colors::DANGER()) : QString(colors::WARNING());
    const QString css = QStringLiteral("QLabel { color:%1; background:%2; border-bottom:1px solid %3; padding:6px 14px; "
                                       "font-size:%4px; }")
                            .arg(fg, colors::BG_RAISED(), colors::BORDER_DIM())
                            .arg(fonts::px(-1));
    if (css != status_banner_css_) {
        status_banner_css_ = css;
        status_banner_->setStyleSheet(css);
    }
    status_banner_->setVisible(true);
}

void WatchlistScreen::update_last_updated() {
    if (last_quote_ms_ <= 0 || stocks_.isEmpty()) {
        updated_label_->clear();
        updated_label_->setToolTip(QString());
        return;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const bool stale = now - last_quote_ms_ > kWlsdStaleMs;
    updated_label_->setText(tr("Updated %1").arg(fmt::time(last_quote_ms_ / 1000, true)));
    updated_label_->setToolTip(stale ? tr("No quote has arrived for %1 — the data may be stale.")
                                           .arg(fmt::relative(last_quote_ms_ / 1000))
                                     : tr("Latest quote received %1").arg(fmt::relative(last_quote_ms_ / 1000)));
    updated_label_->setStyleSheet(QStringLiteral("color:%1; font-size:%2px; background:transparent;")
                                      .arg(stale ? colors::WARNING() : colors::TEXT_TERTIARY())
                                      .arg(fonts::px(-3)));
}

// ── Footer ───────────────────────────────────────────────────────────────────

void WatchlistScreen::schedule_footer() {
    if (!footer_timer_->isActive())
        footer_timer_->start();
}

void WatchlistScreen::update_footer() {
    update_quote_status();
    update_last_updated();
    if (stocks_.isEmpty()) {
        footer_->set_cells({});
        return;
    }
    const QStringList shown = view_->display_order();
    QVector<const WatchlistRowState*> rows;
    rows.reserve(shown.size());
    for (const QString& s : shown) {
        if (const auto* r = model_->state(s))
            rows.append(r);
    }
    const int total = static_cast<int>(model_->states().size());
    QHash<int, WatchlistFooter::Cell> cells;
    cells.insert(0, {rows.size() == total ? tr("%n symbol(s)", "", total)
                                          : tr("%1 of %2").arg(rows.size()).arg(total),
                     QColor(colors::TEXT_SECONDARY()), tr("Rows shown / rows in the list")});

    for (const QString& id : view_->visible_column_ids()) {
        const int c = wcols::index_of(id);
        if (c <= 0)
            continue;
        const WatchlistColKind kind = wcols::def(c).kind;
        if (id == QLatin1String("name")) {
            int up = 0, down = 0, flat = 0;
            const int pc = wcols::index_of(QStringLiteral("change_pct"));
            for (const auto* r : rows) {
                const double v = model_->value(*r, pc);
                if (!std::isfinite(v))
                    continue;
                if (v > 0)
                    ++up;
                else if (v < 0)
                    ++down;
                else
                    ++flat;
            }
            // Compact: the NAME column is ~20 characters wide.
            cells.insert(c, {tr("▲ %1  ▼ %2  = %3").arg(up).arg(down).arg(flat), QColor(),
                             tr("Advancers ▲ / decliners ▼ / unchanged = on the day")});
            continue;
        }
        if (id == QLatin1String("alert")) {
            int active = 0;
            for (const auto* r : rows)
                active += r->alerts_active;
            if (active > 0)
                cells.insert(c, {QStringLiteral("◆ %1").arg(active), QColor(colors::AMBER()),
                                 tr("Active alerts on the rows shown")});
            continue;
        }
        if (id == QLatin1String("market_cap")) {
            // Total in the display currency; only when every capitalisation converts.
            const QString ccy = model_->display_currency();
            double sum = 0;
            int n = 0;
            int missing = 0;
            for (const auto* r : rows) {
                const double v = model_->value(*r, c);
                if (!std::isfinite(v))
                    continue;
                const auto conv =
                    services::FxRateService::instance().convert(v, core::market::major_currency(r->currency()), ccy);
                if (conv) {
                    sum += *conv;
                    ++n;
                } else {
                    ++missing;
                }
            }
            if (n > 0)
                cells.insert(c, {QStringLiteral("Σ ") + fmt::money(sum, ccy, fmt::Compact::FromSettings),
                                 QColor(),
                                 missing > 0 ? tr("Total of %1 symbols in %2; %3 could not be converted yet")
                                                   .arg(n)
                                                   .arg(ccy)
                                                   .arg(missing)
                                             : tr("Total of %1 symbols in %2").arg(n).arg(ccy)});
            continue;
        }
        const bool averaged = kind == WatchlistColKind::ChangePct || kind == WatchlistColKind::Pct ||
                              kind == WatchlistColKind::HeatPct || kind == WatchlistColKind::RelVolume ||
                              id == QLatin1String("vol_20d");
        if (!averaged)
            continue;
        double sum = 0;
        int n = 0;
        for (const auto* r : rows) {
            const double v = model_->value(*r, c);
            if (std::isfinite(v)) {
                sum += v;
                ++n;
            }
        }
        if (n == 0)
            continue;
        const double avg = sum / n;
        QString text;
        if (kind == WatchlistColKind::RelVolume)
            text = fmt::ratio(avg, 2);
        else if (id == QLatin1String("vol_20d"))
            text = fmt::pct(avg, 1, false);
        else
            text = fmt::pct(avg, kind == WatchlistColKind::HeatPct ? 1 : 2);
        QColor color;
        if (id == QLatin1String("change_pct") || id == QLatin1String("ext_change_pct") ||
            kind == WatchlistColKind::HeatPct) {
            // Colour by the value as shown: a -0.004 % average reads "0.00%" and must not be red.
            const double shown_eps = kind == WatchlistColKind::HeatPct ? 0.05 : 0.005;
            if (avg >= shown_eps)
                color = QColor(colors::POSITIVE());
            else if (avg <= -shown_eps)
                color = QColor(colors::NEGATIVE());
        }
        cells.insert(c, {QStringLiteral("Ø ") + text, color,
                         tr("Equal-weighted average of %n symbol(s) with data", "", n)});
    }
    footer_->set_cells(cells);
}

// ── Alerts ───────────────────────────────────────────────────────────────────

void WatchlistScreen::refresh_alert_summaries() {
    auto& svc = services::WatchlistAlertService::instance();
    for (const QString& sym : model_->symbols()) {
        const auto alerts = svc.alerts_for(sym);
        int active = 0;
        bool fired = false;
        QStringList tip;
        for (const auto& a : alerts) {
            if (a.enabled)
                ++active;
            const bool today = services::WatchlistAlertService::fired_today(a);
            fired = fired || today;
            QString line = (a.enabled ? QStringLiteral("◆ ") : QStringLiteral("◇ ")) +
                           services::WatchlistAlertService::describe(a);
            if (!a.last_triggered_at.isEmpty()) {
                const QDateTime dt = QDateTime::fromString(a.last_triggered_at, Qt::ISODate);
                if (dt.isValid())
                    line += QStringLiteral("  —  ") + tr("fired %1").arg(fmt::relative(dt));
            }
            if (!a.enabled)
                line += QStringLiteral("  ") + tr("(off)");
            tip << line;
        }
        if (!alerts.isEmpty())
            tip << QString() << tr("Right-click the row ▸ Alerts… to edit");
        model_->set_alert_summary(sym, active, static_cast<int>(alerts.size()), fired, tip.join(QLatin1Char('\n')));
    }
    schedule_footer();
}

// ── Analytics views ──────────────────────────────────────────────────────────

void WatchlistScreen::schedule_views() {
    if (view_stack_->currentWidget() == table_page_)
        return;
    if (!views_timer_->isActive())
        views_timer_->start();
}

void WatchlistScreen::refresh_map_view() {
    if (view_stack_->currentWidget() != map_view_)
        return;
    const QString horizon = map_view_->horizon();
    const bool by_cap = map_view_->size_mode() == QLatin1String("cap");
    const int vcol = wcols::index_of(wlsd_horizon_column(horizon));
    const int cap_col = wcols::index_of(QStringLiteral("market_cap"));
    const bool needs_hist = horizon != QLatin1String("1d");

    QVector<ui::TreemapItem> items;
    QVector<int> no_cap;
    int loading = 0;
    double min_cap = std::numeric_limits<double>::infinity();
    for (const QString& sym : view_->display_order()) {
        const auto* r = model_->state(sym);
        if (!r)
            continue;
        ui::TreemapItem it;
        it.id = sym;
        it.label = sym;
        it.sublabel = r->display_name();
        it.group = model_->grouped() ? model_->group_label(model_->group_key_of(*r)) : QString();
        it.value = model_->value(*r, vcol);
        if (needs_hist && r->history == WatchlistHistoryState::Loading)
            ++loading;
        it.weight = 1.0;
        if (by_cap) {
            const double cap = model_->value(*r, cap_col);
            if (std::isfinite(cap) && cap > 0) {
                // Comparable sizes across currencies: USD where the rate is known. Deliberate USD:
                // a size-normalisation target (every leg is held against USD), never shown.
                const auto usd = services::FxRateService::instance().convert(
                    cap, core::market::major_currency(r->currency()), QStringLiteral("USD"));
                it.weight = usd ? *usd : cap;
                min_cap = std::min(min_cap, it.weight);
            } else {
                no_cap.append(static_cast<int>(items.size()));
            }
        }
        items.append(it);
    }
    for (int i : no_cap)
        items[i].weight = std::isfinite(min_cap) ? min_cap : 1.0;

    QStringList notes;
    if (loading > 0)
        notes << tr("Loading history for %n symbol(s)…", "", loading);
    if (by_cap && !no_cap.isEmpty())
        notes << tr("%n without a market cap shown at the smallest size", "", static_cast<int>(no_cap.size()));
    map_view_->set_items(items, model_->grouped(), notes.join(QStringLiteral("  ·  ")));
}

void WatchlistScreen::refresh_correlation_view() {
    if (view_stack_->currentWidget() != corr_view_)
        return;
    const QStringList shown = view_->display_order();
    QHash<QString, QVector<services::HistoryPoint>> series;
    int pending = 0;
    QStringList failed;
    for (const QString& s : shown) {
        const auto it = history_.constFind(s);
        if (it != history_.constEnd())
            series.insert(s, it.value());
        else if (history_failed_.contains(s))
            failed << s;
        else
            ++pending;
    }
    corr_view_->set_data(shown, series, pending, failed);
}

// ── FX / display currency ────────────────────────────────────────────────────

void WatchlistScreen::sync_fx() {
    const QStringList vis = view_->visible_column_ids();
    const bool convert = vis.contains(QStringLiteral("display_price")) || vis.contains(QStringLiteral("market_cap"));
    const bool map_usd = layout_.view == QLatin1String("map") && layout_.map_size == QLatin1String("cap");
    if (!convert && !map_usd)
        return;
    QStringList ccys;
    for (const auto& r : model_->states()) {
        const QString c = core::market::major_currency(r.currency());
        if (!c.isEmpty() && !ccys.contains(c))
            ccys << c;
    }
    if (ccys.isEmpty())
        return;
    auto& fx = services::FxRateService::instance();
    if (convert)
        fx.ensure_currencies(ccys, model_->display_currency());
    if (map_usd)
        fx.ensure_currencies(ccys, QStringLiteral("USD")); // the map's USD size normalisation (refresh_map_view)
}

void WatchlistScreen::set_display_currency(const QString& ccy) {
    if (ccy.isEmpty())
        return;
    model_->set_display_currency(ccy);
    (void)SettingsRepository::instance().set(QString::fromLatin1(kDisplayCcyKey), ccy, QStringLiteral("watchlist"));
    sync_fx();
    schedule_footer();
}

// ── Selection ────────────────────────────────────────────────────────────────

void WatchlistScreen::on_current_row_changed() {
    const QString sym = view_->current_symbol();
    publish_selection_to_group();
    if (detail_->isVisible()) {
        detail_->set_row(sym.isEmpty() ? nullptr : model_->state(sym));
        if (!sym.isEmpty()) {
            ensure_info({sym});
            ensure_history({sym});
        }
    }
    update_list_actions();
}

void WatchlistScreen::select_first_row_if_none() {
    if (current_wl_id_.isEmpty() || stocks_.isEmpty() || !view_->current_symbol().isEmpty())
        return;
    // First row in display order (sort, filter and grouping applied); group header
    // rows and the rows of collapsed groups are skipped.
    QString first;
    for (QModelIndex idx = proxy_->index(0, 0); idx.isValid(); idx = view_->indexBelow(idx)) {
        if (idx.data(WatchlistModel::IsGroupRole).toBool())
            continue;
        first = idx.data(WatchlistModel::SymbolRole).toString();
        if (!first.isEmpty())
            break;
    }
    if (first.isEmpty())
        return;
    view_->select_symbols({first}, first); // silent: no selection signals
    // Deliberately not publish_selection_to_group(): an automatic selection must not
    // retarget the linked screens — only the user's own selections publish.
    if (!detail_->isHidden()) {
        // The pane loads its chart / headlines only while visible (and on its next show).
        detail_->set_row(model_->state(first));
        ensure_info({first});
        ensure_history({first});
    }
    update_list_actions();
}

void WatchlistScreen::save_selection() {
    saved_selection_ = view_->selected_symbols();
    saved_current_ = view_->current_symbol();
    saved_scroll_ = view_->verticalScrollBar() ? view_->verticalScrollBar()->value() : 0;
}

void WatchlistScreen::restore_selection() {
    if (!saved_selection_.isEmpty() || !saved_current_.isEmpty())
        view_->select_symbols(saved_selection_, saved_current_);
    if (view_->verticalScrollBar())
        view_->verticalScrollBar()->setValue(saved_scroll_);
}

void WatchlistScreen::on_refresh() {
    if (current_wl_id_.isEmpty())
        return;
    if (isVisible())
        hub_resubscribe(/*force=*/true); // force-requests every quote; failed topics retry immediately
    else
        load_stocks();
}

} // namespace fincept::screens
