#include "screens/dashboard/widgets/DashboardCandleWidget.h"

#include "core/market/MarketProfile.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/dashboard/widgets/SymbolListDialog.h"
#include "ui/charts/CandleChart.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

#include <algorithm>

namespace fincept::screens::widgets {

namespace {

namespace dc_fmt = fincept::ui::formatting;
using fincept::services::MarketDataService;

struct DcTimeframe {
    const char* label;
    const char* period;
    const char* interval;
    const char* description;
};
constexpr DcTimeframe kDcTimeframes[] = {
    {"1D", "1d", "5m", QT_TRANSLATE_NOOP("fincept::screens::widgets::DashboardCandleWidget", "1 day, 5-minute bars")},
    {"5D", "5d", "15m", QT_TRANSLATE_NOOP("fincept::screens::widgets::DashboardCandleWidget", "5 days, 15-minute bars")},
    {"1M", "1mo", "1h", QT_TRANSLATE_NOOP("fincept::screens::widgets::DashboardCandleWidget", "1 month, hourly bars")},
    {"6M", "6mo", "1d", QT_TRANSLATE_NOOP("fincept::screens::widgets::DashboardCandleWidget", "6 months, daily bars")},
    {"1Y", "1y", "1d", QT_TRANSLATE_NOOP("fincept::screens::widgets::DashboardCandleWidget", "1 year, daily bars")},
    {"5Y", "5y", "1wk", QT_TRANSLATE_NOOP("fincept::screens::widgets::DashboardCandleWidget", "5 years, weekly bars")},
};
constexpr int kDcTfCount = int(sizeof(kDcTimeframes) / sizeof(kDcTimeframes[0]));

QString dc_default_symbol() {
    const auto& p = core::market::MarketProfileStore::instance().current();
    return p.movers.isEmpty() ? QStringLiteral("AAPL") : p.movers.first();
}

QString dc_normalize(const QString& s) {
    const QString t = s.trimmed().toUpper();
    return t.isEmpty() ? dc_default_symbol() : t;
}

} // namespace

DashboardCandleWidget::DashboardCandleWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget(QString(), parent), symbol_(dc_normalize(cfg.value("symbol").toString())) {
    auto* vl = content_layout();
    vl->setContentsMargins(4, 2, 4, 2);
    vl->setSpacing(2);

    auto* bar = new QWidget(this);
    bar->setObjectName("dcBar");
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(2, 1, 2, 1);
    hl->setSpacing(2);
    tfs_ = new QButtonGroup(this);
    for (int i = 0; i < kDcTfCount; ++i) {
        auto* b = new QPushButton(QString::fromLatin1(kDcTimeframes[i].label), bar);
        b->setObjectName("dcTf");
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(tr(kDcTimeframes[i].description));
        tfs_->addButton(b, i);
        hl->addWidget(b);
    }
    hl->addStretch();
    vl->addWidget(bar);
    connect(tfs_, &QButtonGroup::idClicked, this, [this](int id) {
        if (id == tf_)
            return;
        tf_ = id;
        chart_->clear();
        update_title();
        if (isVisible()) {
            set_loading(true);
            hub_resubscribe();
        } else {
            untrack_topics();
        }
        emit config_changed(config());
    });

    chart_ = new ui::CandleChart(this);
    chart_->set_show_volume(true);
    chart_->set_price_formatter([this](double v) {
        auto inst = dc_fmt::instrument_for_symbol(symbol_, currency_);
        if (price_digits_ >= 0)
            inst.decimals = price_digits_;
        return dc_fmt::price(v, inst);
    });
    chart_->set_volume_formatter([](double v) { return dc_fmt::volume(v); });
    vl->addWidget(chart_, 1);

    footer_ = new QLabel(this);
    vl->addWidget(footer_);

    enable_symbol_link(/*follows=*/true);
    link_symbol(footer_, symbol_);
    set_configurable(true);
    connect(this, &BaseWidget::refresh_requested, this, &DashboardCandleWidget::refresh_data);

    const QString tf = cfg.value("tf").toString(QStringLiteral("1Y"));
    for (int i = 0; i < kDcTfCount; ++i)
        if (tf == QLatin1String(kDcTimeframes[i].label))
            tf_ = i;
    if (auto* b = tfs_->button(tf_))
        b->setChecked(true);
    update_title();
    on_theme_changed();
    set_loading(true);
}

void DashboardCandleWidget::update_title() {
    set_title(tr("CANDLES · %1 · %2").arg(symbol_, QString::fromLatin1(kDcTimeframes[tf_].label)));
    chart_->set_export_name(symbol_ + QStringLiteral("_") + QString::fromLatin1(kDcTimeframes[tf_].label));
    footer_->setText(tr("%1 · %2 · double-click to open in Equity Research")
                         .arg(symbol_, tr(kDcTimeframes[tf_].description)));
}

QString DashboardCandleWidget::history_topic() const {
    return MarketDataService::history_topic(symbol_, QString::fromLatin1(kDcTimeframes[tf_].period),
                                            QString::fromLatin1(kDcTimeframes[tf_].interval));
}

QJsonObject DashboardCandleWidget::config() const {
    QJsonObject o;
    o.insert("symbol", symbol_);
    o.insert("tf", QString::fromLatin1(kDcTimeframes[tf_].label));
    return o;
}

void DashboardCandleWidget::apply_config(const QJsonObject& cfg) {
    const QString next = dc_normalize(cfg.value("symbol").toString(symbol_));
    int tf = tf_;
    const QString tfs = cfg.value("tf").toString();
    for (int i = 0; i < kDcTfCount; ++i)
        if (tfs == QLatin1String(kDcTimeframes[i].label))
            tf = i;
    if (next == symbol_ && tf == tf_)
        return;
    symbol_ = next;
    tf_ = tf;
    if (auto* b = tfs_->button(tf_))
        b->setChecked(true);
    currency_.clear();
    price_digits_ = -1;
    link_symbol(footer_, symbol_);
    chart_->clear();
    update_title();
    if (isVisible()) {
        set_loading(true);
        hub_resubscribe();
    } else {
        untrack_topics(); // the old symbol's chart / error no longer describe this tile
    }
}

void DashboardCandleWidget::on_linked_symbol(const SymbolRef& ref) {
    if (dc_normalize(ref.symbol) == symbol_)
        return;
    QJsonObject cfg = config();
    cfg.insert("symbol", ref.symbol);
    apply_config(cfg);
    emit config_changed(config()); // the tile comes back on the linked symbol
}

SymbolRef DashboardCandleWidget::linked_symbol() const {
    return SymbolRef::equity(symbol_);
}

QDialog* DashboardCandleWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new SymbolListDialog(tr("Configure — Candle Chart"), {{symbol_, {}, {}}}, {}, parent, 1);
    connect(dlg, &QDialog::accepted, this, [this, dlg]() {
        const auto list = dlg->entries();
        if (list.isEmpty())
            return;
        QJsonObject cfg = config();
        cfg.insert("symbol", dc_normalize(list.first().symbol));
        apply_config(cfg);
        emit config_changed(config());
    });
    return dlg;
}

void DashboardCandleWidget::on_theme_changed() {
    if (auto* bar = findChild<QWidget*>(QStringLiteral("dcBar")))
        set_css_cached(bar, QString("QWidget#dcBar{background:transparent;}"
                                   "QPushButton#dcTf{background:%1;color:%2;border:1px solid %3;padding:0 7px;"
                                   "font-size:%4px;font-weight:bold;}"
                                   "QPushButton#dcTf:checked{background:%5;color:%6;border-color:%6;}")
                               .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM())
                               .arg(ui::fonts::px(-4))
                               .arg(ui::colors::ACCENT_BG(), ui::colors::AMBER()));
    set_css_cached(footer_, QString("color:%1;font-size:%2px;background:transparent;")
                               .arg(ui::colors::TEXT_TERTIARY())
                               .arg(ui::fonts::px(-4)));
}

void DashboardCandleWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_)
        hub_resubscribe();
}

void DashboardCandleWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    hub_active_ = false;
}

void DashboardCandleWidget::refresh_data() {
    datahub::DataHub::instance().request(
        QStringList{history_topic(), MarketDataService::quote_topic(symbol_)}, /*force=*/true);
}

void DashboardCandleWidget::hub_resubscribe() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    const QString topic = history_topic();
    // Producer errors: with no chart yet the reason replaces the spinner; once
    // candles are up a failed refresh keeps them and only marks the tile STALE.
    track_topics({topic});
    hub.subscribe<QVector<services::HistoryPoint>>(
        this, topic, [this, topic](const QVector<services::HistoryPoint>& pts) {
            if (pts.isEmpty()) {
                note_topic_error(topic, tr("No price history for %1 (%2).")
                                            .arg(symbol_, tr(kDcTimeframes[tf_].description)));
                return;
            }
            QVector<ui::CandleChart::Bar> bars;
            bars.reserve(pts.size());
            for (const auto& p : pts) {
                if (!(p.close > 0) || !(p.high > 0) || !(p.low > 0) || !(p.open > 0))
                    continue;
                bars.append({double(p.timestamp) * 1000.0, p.open, p.high, p.low, p.close, double(p.volume)});
            }
            note_topic_value(topic);
            set_loading(false);
            chart_->set_bars(bars);
            chart_->set_default_visible_bars(std::max(30, int(bars.size())));
            chart_->reset_view();
        });
    // The quote supplies the currency and decimals for the price axis.
    hub.subscribe(this, MarketDataService::quote_topic(symbol_), [this](const QVariant& v) {
        if (!v.canConvert<services::QuoteData>())
            return;
        const auto q = v.value<services::QuoteData>();
        if (q.currency != currency_ || q.price_digits != price_digits_) {
            currency_ = q.currency;
            price_digits_ = q.price_digits;
            chart_->update();
        }
    });
    hub_active_ = true;
}

void DashboardCandleWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    for (int i = 0; i < kDcTfCount; ++i)
        if (auto* b = tfs_->button(i))
            b->setToolTip(tr(kDcTimeframes[i].description));
    update_title();
}

} // namespace fincept::screens::widgets
