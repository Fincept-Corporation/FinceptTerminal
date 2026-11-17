#include "screens/dashboard/widgets/QuoteTableWidget.h"

#include "core/market/MarketProfile.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/dashboard/widgets/RegionPin.h"
#include "screens/dashboard/widgets/SymbolListDialog.h"
#include "services/markets/MarketDataService.h"
#include "ui/formatting/DateFormat.h"
#include "ui/theme/Theme.h"

#include <QJsonArray>
#include <QLabel>
#include <QTimer>

namespace fincept::screens::widgets {

namespace {

namespace qtw_fmt = fincept::ui::formatting;
using fincept::services::MarketDataService;

QVector<QuoteGridEntry> qtw_from_profile(const QVector<core::market::ProfileInstrument>& list) {
    QVector<QuoteGridEntry> out;
    out.reserve(list.size());
    for (const auto& i : list)
        out.append({i.symbol, core::market::profile_text(i.label), core::market::profile_text(i.group)});
    return out;
}

int qtw_optional_columns(QuoteListKind kind) {
    switch (kind) {
        case QuoteListKind::Indices:
            return QuoteGrid::OptDay | QuoteGrid::OptTrend | QuoteGrid::Opt52w;
        case QuoteListKind::Crypto:
            return QuoteGrid::OptDay | QuoteGrid::OptTrend | QuoteGrid::OptVolume;
        case QuoteListKind::Custom:
            return QuoteGrid::OptDay | QuoteGrid::OptTrend | QuoteGrid::OptVolume | QuoteGrid::Opt52w;
        case QuoteListKind::GlobalIndices:
            return QuoteGrid::OptSession | QuoteGrid::OptTrend | QuoteGrid::OptDay;
        case QuoteListKind::Forex:
        case QuoteListKind::Commodities:
            break;
    }
    return QuoteGrid::OptDay | QuoteGrid::OptTrend;
}

QString qtw_accent(QuoteListKind kind) {
    switch (kind) {
        case QuoteListKind::Forex:
            return ui::colors::INFO();
        case QuoteListKind::Crypto:
            return ui::colors::AMBER();
        case QuoteListKind::Commodities:
            return ui::colors::WARNING();
        case QuoteListKind::Custom:
            return ui::colors::INFO();
        case QuoteListKind::GlobalIndices:
            return ui::colors::CYAN();
        case QuoteListKind::Indices:
            break;
    }
    return {};
}

/// Venue of every symbol in the global index universe (index tickers carry no suffix).
QHash<QString, QString> qtw_venue_hints() {
    QHash<QString, QString> h;
    for (const auto& g : core::market::global_index_universe())
        h.insert(g.symbol, g.venue);
    return h;
}

} // namespace

QuoteTableWidget::QuoteTableWidget(QuoteListKind kind, const QJsonObject& cfg, QWidget* parent)
    : BaseWidget(QString(), parent, qtw_accent(kind)), kind_(kind) {
    grid_ = new QuoteGrid(this);
    grid_->set_optional_columns(qtw_optional_columns(kind));
    grid_->set_venue_hints(qtw_venue_hints());
    grid_->setToolTip(tr("Double-click a row to open it in Equity Research · right-click for more"));
    content_layout()->addWidget(grid_, 1);

    footer_ = new QLabel(this);
    footer_->setObjectName("qtwFooter");
    footer_->setTextInteractionFlags(Qt::NoTextInteraction);
    content_layout()->addWidget(footer_);

    connect(grid_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        const QString sym = grid_->symbol_at(row);
        if (!sym.isEmpty())
            open_symbol(sym);
    });
    install_symbol_menu(grid_, QuoteGrid::ColName, QuoteGrid::SymbolRole);

    footer_timer_ = new QTimer(this);
    footer_timer_->setInterval(30'000);
    connect(footer_timer_, &QTimer::timeout, this, &QuoteTableWidget::update_footer);

    connect(this, &BaseWidget::refresh_requested, this, &QuoteTableWidget::refresh_data);
    connect(&core::market::MarketProfileStore::instance(), &core::market::MarketProfileStore::profile_changed, this,
            [this]() {
                if (!region_pin_.isEmpty())
                    return; // pinned to its own region (RegionPin.h)
                if (follow_region_)
                    set_entries(default_entries());
                update_title();
                update_footer();
            });

    set_configurable(true);
    apply_styles();
    set_loading(true);
    // Subclasses that override default_entries() call apply_config() again
    // from their own constructor once their state exists.
    apply_config(cfg);
}

QString QuoteTableWidget::base_title() const {
    switch (kind_) {
        case QuoteListKind::Indices:
            return tr("MARKET INDICES");
        case QuoteListKind::Forex:
            return tr("FX");
        case QuoteListKind::Crypto:
            return tr("CRYPTO");
        case QuoteListKind::Commodities:
            return tr("COMMODITIES");
        case QuoteListKind::GlobalIndices:
            return tr("GLOBAL INDICES");
        case QuoteListKind::Custom:
            break;
    }
    return tr("QUOTES");
}

void QuoteTableWidget::update_title() {
    if (!follow_region_ && !list_title_.isEmpty()) {
        set_title(list_title_.toUpper()); // a template's own named list ("US RATES")
        return;
    }
    QString t = base_title();
    if (follow_region_ && kind_ != QuoteListKind::Custom && kind_ != QuoteListKind::GlobalIndices)
        t += QStringLiteral(" · ") + core::market::market_profile_name(tile_profile(region_pin_)).toUpper();
    else if (!follow_region_ && !list_label_.isEmpty())
        t += QStringLiteral(" · ") + list_label_.toUpper();
    set_title(t);
}

QString QuoteTableWidget::source_text() const {
    if (follow_region_ && kind_ == QuoteListKind::GlobalIndices)
        return tr("World headline indices · session state from exchange calendars");
    if (follow_region_)
        return tr("%1 defaults").arg(core::market::market_profile_name(tile_profile(region_pin_)));
    if (!list_label_.isEmpty())
        return tr("%1 list").arg(list_label_);
    return tr("Custom list");
}

void QuoteTableWidget::update_footer() {
    QStringList bits{source_text()};
    const qint64 t = grid_->latest_market_time();
    if (t > 0)
        bits << tr("as of %1 (%2)").arg(qtw_fmt::time(t), qtw_fmt::relative(t));
    const int delay = grid_->max_delay_min();
    if (delay > 0)
        bits << tr("delayed up to %1 min").arg(delay);
    else if (delay == 0)
        bits << tr("real-time");
    footer_->setText(bits.join(QStringLiteral(" · ")));
}

QVector<QuoteGridEntry> QuoteTableWidget::profile_entries(QuoteListKind kind, const QString& region_pin) {
    const auto& p = tile_profile(region_pin);
    switch (kind) {
        case QuoteListKind::Indices:
            return qtw_from_profile(p.indices);
        case QuoteListKind::Forex:
            return qtw_from_profile(p.fx);
        case QuoteListKind::Crypto:
            return qtw_from_profile(p.crypto);
        case QuoteListKind::Commodities:
            return qtw_from_profile(p.commodities);
        case QuoteListKind::GlobalIndices: {
            QVector<QuoteGridEntry> out;
            for (const auto& g : core::market::global_index_universe())
                out.append({g.symbol, g.label, core::market::profile_text(g.region)});
            return out;
        }
        case QuoteListKind::Custom:
            break;
    }
    QVector<QuoteGridEntry> out;
    for (const auto& s : p.movers.mid(0, 10))
        out.append({s, {}, {}});
    return out;
}

QVector<QuoteGridEntry> QuoteTableWidget::entries_from_json(const QJsonObject& cfg) {
    QVector<QuoteGridEntry> out;
    QSet<QString> seen;
    for (const auto& v : cfg.value("symbols").toArray()) {
        QuoteGridEntry e;
        if (v.isString()) {
            e.symbol = v.toString().trimmed().toUpper();
        } else if (v.isObject()) {
            const auto o = v.toObject();
            e.symbol = o.value("s").toString().trimmed();
            e.label = o.value("l").toString();
            e.group = o.value("g").toString();
        }
        if (e.symbol.isEmpty() || seen.contains(e.symbol))
            continue;
        seen.insert(e.symbol);
        out.append(e);
    }
    return out;
}

QJsonObject QuoteTableWidget::config() const {
    QJsonObject o;
    store_region_pin(o, region_pin_);
    if (follow_region_) {
        o.insert("follow_region", true);
        return o;
    }
    if (!list_label_.isEmpty())
        o.insert("list_label", list_label_);
    if (!list_title_.isEmpty())
        o.insert("list_title", list_title_);
    QJsonArray arr;
    for (const auto& e : grid_->entries()) {
        QJsonObject item{{"s", e.symbol}};
        if (!e.label.isEmpty())
            item.insert("l", e.label);
        if (!e.group.isEmpty())
            item.insert("g", e.group);
        arr.append(item);
    }
    o.insert("symbols", arr);
    return o;
}

void QuoteTableWidget::apply_config(const QJsonObject& cfg) {
    region_pin_ = region_pin_from(cfg);
    list_label_ = cfg.value("list_label").toString().trimmed().left(40);
    list_title_ = cfg.value("list_title").toString().trimmed().left(40);
    const auto list = entries_from_json(cfg);
    follow_region_ = list.isEmpty();
    set_entries(follow_region_ ? default_entries() : list);
    update_title();
    update_footer();
}

QDialog* QuoteTableWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new SymbolListDialog(tr("Edit symbols — %1").arg(base_title()), grid_->entries(),
                                     [this]() { return default_entries(); }, parent);
    connect(dlg, &QDialog::accepted, this, [this, dlg]() {
        const auto list = dlg->entries();
        if (list.isEmpty())
            return;
        follow_region_ = dlg->follows_defaults();
        list_label_.clear(); // the user's own list now, not the template's named one
        list_title_.clear();
        set_entries(follow_region_ ? default_entries() : list);
        update_title();
        update_footer();
        emit config_changed(config());
    });
    return dlg;
}

void QuoteTableWidget::set_entries(const QVector<QuoteGridEntry>& entries) {
    grid_->set_entries(entries);
    if (hub_active_ || isVisible()) {
        untrack_topics();
        hub_subscribe_all();
    }
}

void QuoteTableWidget::apply_styles() {
    set_css_cached(footer_, QString("QLabel#qtwFooter{color:%1;font-size:%2px;padding:2px 6px;background:transparent;}")
                               .arg(ui::colors::TEXT_TERTIARY())
                               .arg(ui::fonts::px(-4)));
}

void QuoteTableWidget::on_theme_changed() {
    apply_styles();
    grid_->rerender(); // state dots and tones are baked into the items
}

void QuoteTableWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    grid_->set_headers({tr("NAME"), tr("LAST"), tr("CHG"), tr("CHG%"), tr("DAY RANGE"), tr("5D"), tr("52W RANGE"),
                        tr("VOL"), tr("SESSION")});
    update_title();
    update_footer();
}

void QuoteTableWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_)
        hub_subscribe_all();
    footer_timer_->start();
    update_footer();
}

void QuoteTableWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    footer_timer_->stop();
    if (hub_active_)
        hub_unsubscribe_all();
}

void QuoteTableWidget::refresh_data() {
    QStringList topics;
    for (const auto& e : grid_->entries()) {
        topics.append(MarketDataService::quote_topic(e.symbol));
        topics.append(MarketDataService::sparkline_topic(e.symbol));
    }
    if (!topics.isEmpty())
        datahub::DataHub::instance().request(topics, /*force=*/true);
}

void QuoteTableWidget::hub_subscribe_all() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    QStringList quote_topics;
    for (const auto& e : grid_->entries())
        quote_topics.append(MarketDataService::quote_topic(e.symbol));
    // BaseWidget drives the overlay from the quote topics (not the sparklines:
    // a missing trend line must not mark the row failed).
    track_topics(quote_topics, /*drive_loading_overlay=*/true);
    for (const auto& e : grid_->entries()) {
        const QString sym = e.symbol;
        const QString qt = MarketDataService::quote_topic(sym);
        hub.subscribe(this, qt, [this, sym, qt](const QVariant& v) {
            if (!v.canConvert<services::QuoteData>())
                return;
            grid_->update_quote(sym, v.value<services::QuoteData>());
            note_topic_value(qt);
            schedule_render([this]() { update_footer(); });
        });
        hub.subscribe(this, MarketDataService::sparkline_topic(sym), [this, sym](const QVariant& v) {
            if (v.canConvert<QVector<double>>())
                grid_->update_sparkline(sym, v.value<QVector<double>>());
        });
    }
    hub_active_ = true;
}

void QuoteTableWidget::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void QuoteTableWidget::on_topic_errors_changed() {
    for (const auto& e : grid_->entries())
        grid_->set_row_error(e.symbol, topic_error_tooltip(MarketDataService::quote_topic(e.symbol)));
}

} // namespace fincept::screens::widgets
