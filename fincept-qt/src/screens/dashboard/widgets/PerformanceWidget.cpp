#include "screens/dashboard/widgets/PerformanceWidget.h"

#include "core/market/MarketProfile.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/dashboard/widgets/QuoteTableWidget.h"
#include "screens/dashboard/widgets/RegionPin.h"
#include "screens/dashboard/widgets/SymbolListDialog.h"
#include "services/markets/MarketDataService.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/BarCellDelegate.h"
#include "ui/theme/Theme.h"

#include <QHeaderView>
#include <QJsonArray>
#include <QLabel>
#include <QResizeEvent>

#include <cmath>

namespace fincept::screens::widgets {

namespace {

namespace mp_fmt = fincept::ui::formatting;
using fincept::services::MarketDataService;

constexpr int kMpValueRole = Qt::UserRole + 5;
enum MpCol { MpName = 0, Mp1D, Mp1W, Mp1M, MpYtd, Mp1Y, MpCount };

} // namespace

PerformanceWidget::PerformanceWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget(tr("MARKET PERFORMANCE"), parent, ui::colors::SUCCESS()) {
    table_ = new ui::DataTable(this);
    table_->set_headers({tr("INDEX"), tr("1D"), tr("1W"), tr("1M"), tr("YTD"), tr("1Y")});
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setSectionResizeMode(MpName, QHeaderView::Stretch);
    table_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    for (int c = Mp1D; c < MpCount; ++c) {
        auto* d = new ui::BarCellDelegate(ui::BarCellDelegate::Mode::Heat, table_);
        d->set_value_role(kMpValueRole);
        // One scale per horizon: a 1Y move of 20 % and a 1D move of 1 % are not comparable.
        d->set_heat_clamp(c == Mp1D ? 2.0 : c == Mp1W ? 4.0 : c == Mp1M ? 8.0 : 20.0);
        table_->setItemDelegateForColumn(c, d);
    }
    table_->setToolTip(tr("Double-click a row to open it in Equity Research"));
    content_layout()->addWidget(table_, 1);
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (auto* it = table_->item(row, MpName))
            open_symbol(it->data(Qt::UserRole).toString());
    });
    install_symbol_menu(table_, MpName, Qt::UserRole);

    footer_ = new QLabel(this);
    footer_->setWordWrap(true);
    content_layout()->addWidget(footer_);

    connect(this, &BaseWidget::refresh_requested, this, &PerformanceWidget::refresh_data);
    connect(&core::market::MarketProfileStore::instance(), &core::market::MarketProfileStore::profile_changed, this,
            [this]() {
                if (!region_pin_.isEmpty())
                    return; // pinned to its own region (RegionPin.h)
                if (follow_region_)
                    set_entries(QuoteTableWidget::profile_entries(QuoteListKind::Indices));
                update_title();
            });
    set_configurable(true);
    on_theme_changed();
    set_loading(true);
    apply_config(cfg);
}

void PerformanceWidget::update_title() {
    QString t = tr("MARKET PERFORMANCE");
    if (follow_region_)
        t += QStringLiteral(" · ") + core::market::market_profile_name(tile_profile(region_pin_)).toUpper();
    else if (!list_label_.isEmpty())
        t += QStringLiteral(" · ") + list_label_.toUpper();
    set_title(t);
}

QJsonObject PerformanceWidget::config() const {
    QJsonObject o;
    store_region_pin(o, region_pin_);
    if (follow_region_) {
        o.insert("follow_region", true);
        return o;
    }
    QJsonArray arr;
    for (const auto& e : entries_)
        arr.append(QJsonObject{{"s", e.symbol}, {"l", e.label}});
    o.insert("symbols", arr);
    if (!list_label_.isEmpty())
        o.insert("list_label", list_label_);
    return o;
}

void PerformanceWidget::apply_config(const QJsonObject& cfg) {
    region_pin_ = region_pin_from(cfg);
    list_label_ = cfg.value("list_label").toString().trimmed().left(40);
    const auto list = QuoteTableWidget::entries_from_json(cfg);
    follow_region_ = list.isEmpty();
    set_entries(follow_region_ ? QuoteTableWidget::profile_entries(QuoteListKind::Indices, region_pin_) : list);
    update_title();
}

QDialog* PerformanceWidget::make_config_dialog(QWidget* parent) {
    const QString pin = region_pin_;
    auto* dlg = new SymbolListDialog(tr("Edit instruments — Market Performance"), entries_,
                                     [pin]() { return QuoteTableWidget::profile_entries(QuoteListKind::Indices, pin); },
                                     parent, 30);
    connect(dlg, &QDialog::accepted, this, [this, dlg]() {
        const auto list = dlg->entries();
        if (list.isEmpty())
            return;
        follow_region_ = dlg->follows_defaults();
        list_label_.clear(); // the user's own list now
        set_entries(follow_region_ ? QuoteTableWidget::profile_entries(QuoteListKind::Indices, region_pin_) : list);
        update_title();
        emit config_changed(config());
    });
    return dlg;
}

void PerformanceWidget::set_entries(const QVector<QuoteGridEntry>& entries) {
    entries_ = entries;
    table_->setRowCount(0);
    table_->setRowCount(entries_.size());
    for (int r = 0; r < entries_.size(); ++r) {
        for (int c = 0; c < MpCount; ++c) {
            auto* it = new QTableWidgetItem;
            it->setForeground(QColor(ui::colors::TEXT_PRIMARY()));
            if (c != MpName)
                it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            table_->setItem(r, c, it);
        }
        auto* name = table_->item(r, MpName);
        name->setText(entries_[r].label.isEmpty() ? entries_[r].symbol : entries_[r].label);
        name->setData(Qt::UserRole, entries_[r].symbol);
        name->setToolTip(entries_[r].symbol);
    }
    if (hub_active_ || isVisible()) {
        untrack_topics();
        hub_subscribe_all();
    }
    render();
}

void PerformanceWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_)
        hub_subscribe_all();
    update_columns();
}

void PerformanceWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    hub_active_ = false;
}

void PerformanceWidget::resizeEvent(QResizeEvent* e) {
    BaseWidget::resizeEvent(e);
    update_columns();
}

void PerformanceWidget::update_columns() {
    const int dw = std::max(6, table_->fontMetrics().horizontalAdvance(QLatin1Char('0')));
    const int w = table_->viewport()->width();
    const bool show[MpCount] = {true, true, w >= 44 * dw, w >= 36 * dw, true, w >= 52 * dw};
    for (int c = Mp1D; c < MpCount; ++c) {
        table_->setColumnHidden(c, !show[c]);
        table_->horizontalHeader()->setSectionResizeMode(c, QHeaderView::Fixed);
        table_->setColumnWidth(c, 8 * dw + 10);
    }
}

void PerformanceWidget::refresh_data() {
    QStringList topics;
    for (const auto& e : entries_) {
        topics << MarketDataService::quote_topic(e.symbol) << MarketDataService::performance_history_topic(e.symbol);
    }
    if (!topics.isEmpty())
        datahub::DataHub::instance().request(topics, /*force=*/true);
}

void PerformanceWidget::hub_subscribe_all() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    QStringList topics;
    for (const auto& e : entries_)
        topics << MarketDataService::quote_topic(e.symbol) << MarketDataService::performance_history_topic(e.symbol);
    track_topics(topics, /*drive_loading_overlay=*/true);
    for (const auto& e : entries_) {
        const QString sym = e.symbol;
        const QString qt = MarketDataService::quote_topic(sym);
        const QString ht = MarketDataService::performance_history_topic(sym);
        hub.subscribe(this, qt, [this, sym, qt](const QVariant& v) {
            if (!v.canConvert<services::QuoteData>())
                return;
            quotes_.insert(sym, v.value<services::QuoteData>());
            note_topic_value(qt);
            schedule_render([this]() { render(); });
        });
        hub.subscribe(this, ht, [this, sym, ht](const QVariant& v) {
            if (!v.canConvert<QVector<services::HistoryPoint>>())
                return;
            const auto bars = v.value<QVector<services::HistoryPoint>>();
            if (bars.isEmpty()) {
                note_topic_error(ht, tr("No daily history for %1").arg(sym));
                return;
            }
            const auto q = quotes_.constFind(sym);
            const double live = (q != quotes_.constEnd() && q->price > 0) ? q->price : services::kMarketDataAbsent;
            const bool crypto = q != quotes_.constEnd() && q->quote_type == QLatin1String("CRYPTOCURRENCY");
            stats_.insert(sym, services::compute_performance(bars, live, crypto ? 365 : 252));
            note_topic_value(ht);
            schedule_render([this]() { render(); });
        });
    }
    hub_active_ = true;
}

void PerformanceWidget::on_topic_errors_changed() {
    schedule_render([this]() { render(); });
}

void PerformanceWidget::render() {
    QDate as_of;
    for (int r = 0; r < entries_.size() && r < table_->rowCount(); ++r) {
        const QString& sym = entries_[r].symbol;
        const auto q = quotes_.constFind(sym);
        const auto s = stats_.constFind(sym);
        auto put = [&](int col, double v, const QString& err) {
            auto* it = table_->item(r, col);
            if (!it)
                return;
            if (std::isfinite(v)) {
                it->setText(mp_fmt::pct(v));
                it->setData(kMpValueRole, v);
                it->setToolTip(err);
            } else {
                it->setText(err.isEmpty() ? QString() : mp_fmt::placeholder());
                it->setData(kMpValueRole, QVariant());
                it->setToolTip(err);
            }
        };
        const QString qerr = topic_error_tooltip(MarketDataService::quote_topic(sym));
        const QString herr = topic_error_tooltip(MarketDataService::performance_history_topic(sym));
        put(Mp1D, q != quotes_.constEnd() ? q->change_pct : services::kMarketDataAbsent, qerr);
        const services::PerformanceStats none;
        const auto& st = s != stats_.constEnd() ? s.value() : none;
        put(Mp1W, st.change_1w_pct, herr);
        put(Mp1M, st.change_1m_pct, herr);
        put(MpYtd, st.change_ytd_pct, herr);
        put(Mp1Y, st.change_1y_pct, herr);
        if (st.as_of.isValid() && (!as_of.isValid() || st.as_of > as_of))
            as_of = st.as_of;
    }
    table_->viewport()->update();
    footer_->setText(as_of.isValid()
                         ? tr("1D from live quotes · 1W–1Y from daily closes to %1").arg(mp_fmt::calendar_date(as_of))
                         : tr("1D from live quotes · 1W–1Y from daily closes"));
}

void PerformanceWidget::on_theme_changed() {
    set_css_cached(footer_, QString("color:%1;font-size:%2px;padding:2px 6px;background:transparent;")
                               .arg(ui::colors::TEXT_TERTIARY())
                               .arg(ui::fonts::px(-4)));
    table_->viewport()->update();
}

void PerformanceWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    table_->set_headers({tr("INDEX"), tr("1D"), tr("1W"), tr("1M"), tr("YTD"), tr("1Y")});
    update_title();
    render();
}

} // namespace fincept::screens::widgets
