#include "screens/dashboard/widgets/TopMoversWidget.h"

#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/dashboard/widgets/RegionPin.h"
#include "screens/dashboard/widgets/SymbolListDialog.h"
#include "services/markets/MarketDataService.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/DataTable.h"
#include "ui/theme/Theme.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace fincept::screens::widgets {

namespace {

namespace tm_fmt = fincept::ui::formatting;
using fincept::services::MarketDataService;
using fincept::services::ScreenerService;

enum TmCol { TmSym = 0, TmLast, TmPct, TmVol, TmRel, TmCap, TmCount };

} // namespace

TopMoversWidget::TopMoversWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget(tr("TOP MOVERS"), parent, ui::colors::AMBER()) {
    auto* bar = new QWidget(this);
    bar->setObjectName("tmTabs");
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(6, 3, 6, 3);
    hl->setSpacing(2);
    tabs_ = new QButtonGroup(this);
    tabs_->setExclusive(true);
    const QString labels[] = {tr("GAINERS"), tr("LOSERS"), tr("MOST ACTIVE")};
    for (int i = 0; i < 3; ++i) {
        auto* b = new QPushButton(labels[i], bar);
        b->setObjectName("tmTab");
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        tabs_->addButton(b, i);
        hl->addWidget(b);
    }
    hl->addStretch();
    content_layout()->addWidget(bar);
    connect(tabs_, &QButtonGroup::idClicked, this, [this](int id) {
        tab_ = id;
        rows_.clear();
        if (list_mode_)
            render();
        else
            run_screen(false);
        emit config_changed(config());
    });

    table_ = new ui::DataTable(this);
    table_->set_headers({tr("SYMBOL"), tr("LAST"), tr("CHG%"), tr("VOL"), tr("REL VOL"), tr("MCAP")});
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setSectionResizeMode(TmSym, QHeaderView::Stretch);
    table_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table_->setToolTip(tr("Double-click a row to open it in Equity Research · right-click for more"));
    content_layout()->addWidget(table_, 1);
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (auto* it = table_->item(row, TmSym))
            open_symbol(it->data(Qt::UserRole).toString());
    });
    install_symbol_menu(table_, TmSym, Qt::UserRole);

    footer_ = new QLabel(this);
    footer_->setWordWrap(true);
    content_layout()->addWidget(footer_);

    auto& svc = ScreenerService::instance();
    connect(&svc, &ScreenerService::results_ready, this, [this](const services::ScreenerResult& r) {
        if (!list_mode_ && r.key == pending_key_)
            apply_result(r);
    });
    connect(&svc, &ScreenerService::run_failed, this, [this](const QString& key, const QString& err) {
        if (list_mode_ || key != pending_key_)
            return;
        if (rows_.isEmpty())
            set_error(tr("Movers unavailable: %1").arg(err));
        else
            footer_->setText(tr("Last refresh failed — showing earlier results: %1").arg(err));
    });
    connect(&core::market::MarketProfileStore::instance(), &core::market::MarketProfileStore::profile_changed, this,
            [this]() {
                if (!region_pin_.isEmpty())
                    return; // pinned to its own region (RegionPin.h)
                update_title();
                if (!list_mode_) {
                    rows_.clear();
                    if (isVisible())
                        run_screen(false);
                }
            });

    refresh_timer_ = new QTimer(this);
    refresh_timer_->setInterval(5 * 60 * 1000);
    connect(refresh_timer_, &QTimer::timeout, this, [this]() {
        if (!list_mode_)
            run_screen(false);
    });
    connect(this, &BaseWidget::refresh_requested, this, [this]() {
        if (list_mode_) {
            QStringList t;
            for (const auto& s : symbols_)
                t << MarketDataService::quote_topic(s);
            datahub::DataHub::instance().request(t, true);
        } else {
            run_screen(true);
        }
    });

    // Settings > Currency & Number Format: re-format the rows held, no fetch.
    tm_fmt::on_format_changed(this, [this]() { schedule_render([this]() { render(); }); });

    set_configurable(true);
    on_theme_changed();
    set_loading(true);
    apply_config(cfg);
}

void TopMoversWidget::apply_result(const services::ScreenerResult& r) {
    rows_.clear();
    for (const auto& s : r.rows) {
        Row row;
        row.symbol = s.symbol;
        row.name = s.name;
        row.currency = s.currency;
        row.price = s.price;
        row.change_pct = s.change_pct;
        row.volume = s.volume;
        row.rel_volume = (std::isfinite(s.volume) && std::isfinite(s.avg_volume_3m) && s.avg_volume_3m > 0)
                             ? s.volume / s.avg_volume_3m
                             : services::kMarketDataAbsent;
        row.market_cap = s.market_cap;
        row.mcap_currency = s.market_cap_currency.isEmpty() ? s.currency : s.market_cap_currency;
        rows_.append(row);
    }
    set_loading(false);
    footer_->setText(dash_screens::result_footer(r, tile_profile(region_pin_)));
    render();
}

dash_screens::MoverKind TopMoversWidget::kind() const {
    return tab_ == 1 ? dash_screens::MoverKind::Losers
                     : tab_ == 2 ? dash_screens::MoverKind::Active : dash_screens::MoverKind::Gainers;
}

void TopMoversWidget::update_title() {
    set_title(list_mode_ ? tr("TOP MOVERS · MY LIST")
                         : tr("TOP MOVERS · %1")
                               .arg(core::market::market_profile_name(tile_profile(region_pin_))
                                        .toUpper()));
}

QJsonObject TopMoversWidget::config() const {
    QJsonObject o{{"universe", list_mode_ ? "list" : "region"}, {"tab", tab_}};
    if (list_mode_)
        o.insert("symbols", QJsonArray::fromStringList(symbols_));
    store_region_pin(o, region_pin_);
    return o;
}

void TopMoversWidget::apply_config(const QJsonObject& cfg) {
    region_pin_ = region_pin_from(cfg);
    QStringList syms;
    for (const auto& v : cfg.value("symbols").toArray()) {
        const QString s = v.toString().trimmed().toUpper();
        if (!s.isEmpty() && !syms.contains(s))
            syms << s;
    }
    const QString universe = cfg.value("universe").toString();
    // Legacy configs carried only "symbols": keep showing that list.
    list_mode_ = universe == QLatin1String("list") || (universe.isEmpty() && !syms.isEmpty());
    symbols_ = syms;
    tab_ = std::clamp(cfg.value("tab").toInt(0), 0, 2);
    if (auto* b = tabs_->button(tab_))
        b->setChecked(true);
    rows_.clear();
    quotes_.clear();
    update_title();
    if (isVisible()) {
        datahub::DataHub::instance().unsubscribe(this);
        hub_active_ = false;
        untrack_topics();
        if (list_mode_)
            subscribe_list();
        else
            run_screen(false);
    }
}

QDialog* TopMoversWidget::make_config_dialog(QWidget* parent) {
    QVector<QuoteGridEntry> current;
    const QStringList base = list_mode_ ? symbols_ : tile_profile(region_pin_).movers;
    for (const auto& s : base)
        current.append({s, {}, {}});
    auto* dlg = new SymbolListDialog(tr("Movers universe — your own list, or Region defaults for the region's "
                                        "large-cap screener"),
                                     current,
                                     [pin = region_pin_]() {
                                         QVector<QuoteGridEntry> d;
                                         for (const auto& s : tile_profile(pin).movers)
                                             d.append({s, {}, {}});
                                         return d;
                                     },
                                     parent, 100);
    connect(dlg, &QDialog::accepted, this, [this, dlg]() {
        QJsonObject cfg{{"tab", tab_}};
        store_region_pin(cfg, region_pin_);
        if (dlg->follows_defaults()) {
            cfg.insert("universe", "region");
        } else {
            QJsonArray arr;
            for (const auto& e : dlg->entries())
                arr.append(e.symbol);
            cfg.insert("universe", "list");
            cfg.insert("symbols", arr);
        }
        apply_config(cfg);
        emit config_changed(config());
    });
    return dlg;
}

void TopMoversWidget::run_screen(bool force) {
    const auto& p = tile_profile(region_pin_);
    auto& svc = ScreenerService::instance();
    pending_key_ = svc.run(dash_screens::movers_query(p, kind(), 25), force);
    if (const auto* last = svc.last_result(pending_key_); last && rows_.isEmpty()) {
        // Show the last-known result at once; the run's answer replaces it.
        apply_result(*last);
    } else if (rows_.isEmpty()) {
        set_loading(true);
    }
}

void TopMoversWidget::subscribe_list() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    QStringList topics;
    for (const auto& s : symbols_)
        topics << MarketDataService::quote_topic(s);
    track_topics(topics, /*drive_loading_overlay=*/true);
    for (const auto& s : symbols_) {
        const QString t = MarketDataService::quote_topic(s);
        hub.subscribe(this, t, [this, s, t](const QVariant& v) {
            if (!v.canConvert<services::QuoteData>())
                return;
            quotes_.insert(s, v.value<services::QuoteData>());
            note_topic_value(t);
            schedule_render([this]() { render(); });
        });
    }
    hub_active_ = true;
    footer_->setText(tr("Ranked from live quotes of your %n symbol(s)", "", int(symbols_.size())));
}

void TopMoversWidget::on_topic_errors_changed() {
    schedule_render([this]() { render(); });
}

int TopMoversWidget::visible_rows() const {
    const int rh = std::max(12, table_->verticalHeader()->defaultSectionSize());
    const int h = table_->viewport()->height();
    return std::max(3, h / rh);
}

void TopMoversWidget::render() {
    QVector<Row> rows = rows_;
    if (list_mode_) {
        rows.clear();
        for (const auto& s : symbols_) {
            const auto q = quotes_.constFind(s);
            if (q == quotes_.constEnd())
                continue;
            Row r;
            r.symbol = s;
            r.name = q->name;
            r.currency = q->currency;
            r.price = q->price;
            r.change_pct = q->change_pct;
            r.volume = q->volume;
            r.rel_volume = (q->volume > 0 && std::isfinite(q->average_volume) && q->average_volume > 0)
                               ? q->volume / q->average_volume
                               : services::kMarketDataAbsent;
            rows.append(r);
        }
        const auto k = kind();
        std::sort(rows.begin(), rows.end(), [k](const Row& a, const Row& b) {
            if (k == dash_screens::MoverKind::Active)
                return (std::isfinite(a.volume) ? a.volume : -1) > (std::isfinite(b.volume) ? b.volume : -1);
            const double x = std::isfinite(a.change_pct) ? a.change_pct : 0;
            const double y = std::isfinite(b.change_pct) ? b.change_pct : 0;
            return k == dash_screens::MoverKind::Losers ? x < y : x > y;
        });
        if (k == dash_screens::MoverKind::Gainers)
            rows.erase(std::remove_if(rows.begin(), rows.end(), [](const Row& r) { return !(r.change_pct > 0); }),
                       rows.end());
        if (k == dash_screens::MoverKind::Losers)
            rows.erase(std::remove_if(rows.begin(), rows.end(), [](const Row& r) { return !(r.change_pct < 0); }),
                       rows.end());
    }
    const int n = std::min<int>(rows.size(), visible_rows());
    // Market caps keep their own currency; a column that mixes currencies says which.
    tm_fmt::CurrencyMix cap_mix;
    for (int i = 0; i < n; ++i)
        cap_mix.add(rows[i].mcap_currency);
    table_->setRowCount(n);
    for (int i = 0; i < n; ++i) {
        const Row& r = rows[i];
        auto cell = [&](int c) {
            auto* it = table_->item(i, c);
            if (!it) {
                it = new QTableWidgetItem;
                if (c != TmSym)
                    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table_->setItem(i, c, it);
            }
            it->setForeground(QColor(ui::colors::TEXT_PRIMARY()));
            return it;
        };
        auto* sym = cell(TmSym);
        sym->setText(r.symbol);
        sym->setData(Qt::UserRole, r.symbol);
        sym->setToolTip(r.name.isEmpty() ? r.symbol : r.symbol + QStringLiteral(" — ") + r.name);
        cell(TmLast)->setText(tm_fmt::price(r.price, tm_fmt::instrument_for_symbol(r.symbol, r.currency)));
        auto* pct = cell(TmPct);
        pct->setText(tm_fmt::pct(r.change_pct));
        pct->setForeground(QColor(!std::isfinite(r.change_pct) || r.change_pct == 0 ? ui::colors::TEXT_SECONDARY()
                                  : r.change_pct > 0                              ? ui::colors::POSITIVE()
                                                                                  : ui::colors::NEGATIVE()));
        cell(TmVol)->setText(tm_fmt::volume(r.volume));
        auto* rel = cell(TmRel);
        rel->setText(std::isfinite(r.rel_volume) ? tm_fmt::ratio(r.rel_volume, 1) : tm_fmt::placeholder());
        rel->setToolTip(tr("Today's volume over the 3-month average"));
        if (std::isfinite(r.rel_volume) && r.rel_volume >= 2.0)
            rel->setForeground(QColor(ui::colors::WARNING()));
        cell(TmCap)->setText(std::isfinite(r.market_cap)
                                 ? tm_fmt::money_in_column(r.market_cap, r.mcap_currency, cap_mix,
                                                           tm_fmt::Compact::FromSettings)
                                 : tm_fmt::placeholder());
    }
    // Columns drop out as the tile narrows.
    const int dw = std::max(6, table_->fontMetrics().horizontalAdvance(QLatin1Char('0')));
    const int w = table_->viewport()->width();
    const bool show[TmCount] = {true, true, true, w >= 40 * dw, w >= 48 * dw, w >= 58 * dw && !list_mode_};
    for (int c = TmLast; c < TmCount; ++c) {
        table_->setColumnHidden(c, !show[c]);
        table_->horizontalHeader()->setSectionResizeMode(c, QHeaderView::Fixed);
        // + 18: DataTable cells pad 8 px each side ("+39.1…" was elided with + 10).
        table_->setColumnWidth(c, (c == TmCap ? 10 : 8) * dw + 18);
    }
}

void TopMoversWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (list_mode_) {
        if (!hub_active_)
            subscribe_list();
    } else {
        run_screen(false);
        refresh_timer_->start();
    }
}

void TopMoversWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    hub_active_ = false;
    refresh_timer_->stop();
}

void TopMoversWidget::resizeEvent(QResizeEvent* e) {
    BaseWidget::resizeEvent(e);
    schedule_render([this]() { render(); });
}

void TopMoversWidget::on_theme_changed() {
    QWidget* tab_bar = tabs_->buttons().isEmpty() ? nullptr : tabs_->buttons().first()->parentWidget();
    if (tab_bar)
        set_css_cached(tab_bar, QString("QWidget#tmTabs{background:transparent;}"
                          "QPushButton#tmTab{background:%1;color:%2;border:1px solid %3;padding:1px 8px;"
                          "font-size:%4px;font-weight:bold;}"
                          "QPushButton#tmTab:checked{background:%5;color:%6;border-color:%6;}"
                          "QPushButton#tmTab:hover{color:%7;}")
                      .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM())
                      .arg(ui::fonts::px(-4))
                      .arg(ui::colors::ACCENT_BG(), ui::colors::AMBER(), ui::colors::TEXT_PRIMARY()));
    set_css_cached(footer_, QString("color:%1;font-size:%2px;padding:0 6px 2px 6px;background:transparent;")
                               .arg(ui::colors::TEXT_TERTIARY())
                               .arg(ui::fonts::px(-4)));
    render();
}

void TopMoversWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    const QString labels[] = {tr("GAINERS"), tr("LOSERS"), tr("MOST ACTIVE")};
    for (int i = 0; i < 3; ++i)
        if (auto* b = tabs_->button(i))
            b->setText(labels[i]);
    table_->set_headers({tr("SYMBOL"), tr("LAST"), tr("CHG%"), tr("VOL"), tr("REL VOL"), tr("MCAP")});
    update_title();
}

} // namespace fincept::screens::widgets
