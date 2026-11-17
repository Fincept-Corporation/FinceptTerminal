#include "screens/dashboard/widgets/StockQuoteWidget.h"

#include "core/market/MarketSessions.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/dashboard/widgets/SymbolListDialog.h"
#include "ui/charts/RangeBar.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"

#include <QDialog>
#include <QFrame>

#include <cmath>

namespace fincept::screens::widgets {

namespace {
namespace sq_fmt = fincept::ui::formatting;

QString sq_normalize(const QString& s) {
    const QString t = s.trimmed().toUpper();
    return t.isEmpty() ? QStringLiteral("AAPL") : t;
}

sq_fmt::InstrumentFmt sq_instrument(const QString& sym, const services::QuoteData& q) {
    auto inst = sq_fmt::instrument_for_symbol(sym, q.currency);
    if (q.price_digits >= 0)
        inst.decimals = q.price_digits;
    return inst;
}

QString sq_price(double v, const sq_fmt::InstrumentFmt& inst) {
    return std::isfinite(v) && v > 0 ? sq_fmt::price(v, inst) : sq_fmt::placeholder();
}
} // namespace

StockQuoteWidget::StockQuoteWidget(const QString& symbol, QWidget* parent)
    : BaseWidget(tr("QUOTE: %1").arg(symbol.toUpper()), parent), symbol_(symbol.toUpper()) {
    auto* vl = content_layout();
    vl->setContentsMargins(12, 8, 12, 8);
    vl->setSpacing(8);

    // ── Price row ──
    auto* price_row = new QWidget(this);
    auto* prl = new QHBoxLayout(price_row);
    prl->setContentsMargins(0, 0, 0, 0);
    prl->setSpacing(8);

    price_label_ = new QLabel("--");
    prl->addWidget(price_label_);

    auto* change_col = new QWidget(this);
    auto* ccl = new QVBoxLayout(change_col);
    ccl->setContentsMargins(0, 4, 0, 0);
    ccl->setSpacing(0);

    arrow_label_ = new QLabel;
    ccl->addWidget(arrow_label_);

    change_label_ = new QLabel("--");
    ccl->addWidget(change_label_);

    prl->addWidget(change_col);
    prl->addStretch();

    ticker_label_ = new QLabel(symbol_);
    prl->addWidget(ticker_label_);
    enable_symbol_link(/*follows=*/true);
    link_symbol(ticker_label_, symbol_); // double-click the ticker → Equity Research
    link_symbol(price_label_, symbol_);

    vl->addWidget(price_row);

    // ── Separator ──
    sep_ = new QFrame;
    sep_->setFixedHeight(1);
    vl->addWidget(sep_);

    // ── Stats grid ──
    auto* stats = new QWidget(this);
    auto* gl = new QGridLayout(stats);
    gl->setContentsMargins(0, 0, 0, 0);
    gl->setSpacing(6);

    auto make_stat = [&](int row, int col, const char* key, QLabel*& val_out) {
        auto* cell = new QWidget(this);
        stat_cells_.append(cell);
        auto* cl = new QVBoxLayout(cell);
        cl->setContentsMargins(8, 6, 8, 6);
        cl->setSpacing(2);

        auto* lbl = new QLabel(tr(key));
        stat_labels_.append(lbl);
        stat_label_keys_.append(QString::fromLatin1(key));
        cl->addWidget(lbl);

        val_out = new QLabel("--");
        stat_values_.append(val_out);
        cl->addWidget(val_out);

        gl->addWidget(cell, row, col);
    };

    make_stat(0, 0, QT_TR_NOOP("OPEN"), open_val_);
    make_stat(0, 1, QT_TR_NOOP("PREV CLOSE"), prev_val_);
    make_stat(0, 2, QT_TR_NOOP("VOLUME"), volume_val_);
    make_stat(1, 0, QT_TR_NOOP("HIGH"), high_val_);
    make_stat(1, 1, QT_TR_NOOP("LOW"), low_val_);
    make_stat(1, 2, QT_TR_NOOP("REL VOLUME"), avgvol_val_);

    vl->addWidget(stats);

    // ── Ranges ──
    auto* ranges = new QWidget(this);
    auto* rgl = new QGridLayout(ranges);
    rgl->setContentsMargins(0, 0, 0, 0);
    rgl->setHorizontalSpacing(8);
    rgl->setVerticalSpacing(2);
    day_lbl_ = new QLabel(tr("DAY"), ranges);
    wk_lbl_ = new QLabel(tr("52W"), ranges);
    day_range_ = new ui::RangeBar(ranges);
    wk_range_ = new ui::RangeBar(ranges);
    for (auto* rb : {day_range_, wk_range_}) {
        rb->set_show_labels(true);
        rb->setMinimumHeight(28);
    }
    rgl->addWidget(day_lbl_, 0, 0);
    rgl->addWidget(day_range_, 0, 1);
    rgl->addWidget(wk_lbl_, 1, 0);
    rgl->addWidget(wk_range_, 1, 1);
    rgl->setColumnStretch(1, 1);
    vl->addWidget(ranges);

    // ── Chart (shared daily history) ──
    chart_ = new ui::TimeSeriesChart(this);
    chart_->set_show_mode_buttons(false);
    chart_->set_show_legend(false);
    chart_->set_range_presets({"1M", "3M", "6M", "1Y"});
    chart_->set_active_preset(QStringLiteral("3M"));
    chart_->set_state(ui::TimeSeriesChart::State::Loading);
    chart_->setMinimumHeight(70);
    vl->addWidget(chart_, 1);

    status_label_ = new QLabel(this);
    status_label_->setWordWrap(true);
    vl->addWidget(status_label_);

    set_configurable(true);
    connect(this, &BaseWidget::refresh_requested, this, &StockQuoteWidget::refresh_data);

    apply_styles();
    set_loading(true);
}

QJsonObject StockQuoteWidget::config() const {
    QJsonObject o;
    o.insert("symbol", symbol_);
    return o;
}

void StockQuoteWidget::apply_config(const QJsonObject& cfg) {
    const QString next = sq_normalize(cfg.value("symbol").toString(symbol_));
    if (next == symbol_)
        return;
    set_symbol(next);
}

void StockQuoteWidget::on_linked_symbol(const SymbolRef& ref) {
    if (sq_normalize(ref.symbol) == symbol_)
        return;
    set_symbol(ref.symbol);
    emit config_changed(config()); // the tile comes back on the linked symbol
}

SymbolRef StockQuoteWidget::linked_symbol() const {
    return SymbolRef::equity(symbol_);
}

QDialog* StockQuoteWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new SymbolListDialog(tr("Configure — Stock Quote"), {{symbol_, {}, {}}}, {}, parent, 1);
    connect(dlg, &QDialog::accepted, this, [this, dlg]() {
        const auto list = dlg->entries();
        if (list.isEmpty())
            return;
        QJsonObject cfg;
        cfg.insert("symbol", sq_normalize(list.first().symbol));
        apply_config(cfg);
        emit config_changed(cfg);
    });
    return dlg;
}

void StockQuoteWidget::apply_styles() {
    // The price / arrow / change labels carry the quote's direction colour
    // (populate()). The central re-style keeps those sheets in step with the
    // theme, so only a tile with no quote yet gets the neutral sheets here.
    if (arrow_label_->text().isEmpty()) {
        reset_css_cache(price_label_);
        reset_css_cache(arrow_label_);
        reset_css_cache(change_label_);
        set_css_cached(price_label_, QString("color: %1; font-size: " + fincept::ui::fonts::px(14) + "px; font-weight: bold; background: transparent;")
                                        .arg(ui::colors::TEXT_PRIMARY()));
        set_css_cached(arrow_label_, "font-size: " + fincept::ui::fonts::px(-2) + "px; background: transparent;");
        set_css_cached(change_label_, QString("color: %1; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: bold; background: transparent;")
                                         .arg(ui::colors::TEXT_SECONDARY()));
    }
    set_css_cached(ticker_label_, 
        QString("color: %1; font-size: " + fincept::ui::fonts::px(0) + "px; font-weight: bold; background: transparent;").arg(ui::colors::AMBER()));
    set_css_cached(sep_, QString("background: %1;").arg(ui::colors::BORDER_DIM()));
    for (auto* l : {day_lbl_, wk_lbl_})
        set_css_cached(l, QString("color: %1; font-size: %2px; background: transparent;")
                             .arg(ui::colors::TEXT_SECONDARY())
                             .arg(ui::fonts::px(-5)));
    set_css_cached(status_label_, QString("color: %1; font-size: %2px; background: transparent;")
                                     .arg(ui::colors::TEXT_TERTIARY())
                                     .arg(ui::fonts::px(-4)));

    for (auto* cell : stat_cells_)
        set_css_cached(cell, QString("background: %1; border-radius: 2px;").arg(ui::colors::BG_RAISED()));
    for (auto* lbl : stat_labels_)
        set_css_cached(lbl, 
            QString("color: %1; font-size: " + fincept::ui::fonts::px(-5) + "px; background: transparent;").arg(ui::colors::TEXT_SECONDARY()));
    for (auto* val : stat_values_)
        set_css_cached(val, QString("color: %1; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: bold; background: transparent;")
                               .arg(ui::colors::TEXT_PRIMARY()));
}

void StockQuoteWidget::on_theme_changed() {
    apply_styles();
}

void StockQuoteWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_)
        hub_resubscribe();
}

void StockQuoteWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe_all();
}

void StockQuoteWidget::set_symbol(const QString& symbol) {
    symbol_ = sq_normalize(symbol);
    set_title(tr("QUOTE: %1").arg(symbol_));
    ticker_label_->setText(symbol_);
    link_symbol(ticker_label_, symbol_);
    link_symbol(price_label_, symbol_);

    // Blank the previous symbol's numbers immediately — leaving AAPL's price
    // under an "MSFT" heading until the first delivery is actively misleading.
    price_label_->setText(QStringLiteral("--"));
    change_label_->setText(QStringLiteral("--"));
    arrow_label_->setText(QString());
    for (auto* v : stat_values_)
        v->setText(QStringLiteral("--"));
    status_label_->clear();
    day_range_->set_data({});
    wk_range_->set_data({});
    chart_->clear();
    chart_->set_state(ui::TimeSeriesChart::State::Loading);
    last_prev_close_ = 0;

    // Re-subscribe to the new topic; old sub for previous symbol is
    // dropped wholesale by `unsubscribe(this)` inside hub_resubscribe().
    if (isVisible()) {
        set_loading(true);
        hub_resubscribe();
    } else {
        untrack_topics(); // the old symbol's quote / error no longer describe this tile
    }
}

void StockQuoteWidget::refresh_data() {
    // User-triggered refresh: force past the topic TTL / min_interval (the
    // producer-side rate limit still applies). Without `force` the title-bar
    // refresh button was a no-op whenever the cached quote was still fresh.
    datahub::DataHub::instance().request(
        QStringList{QStringLiteral("market:quote:") + symbol_,
                    services::MarketDataService::performance_history_topic(symbol_)},
        /*force=*/true);
}

void StockQuoteWidget::hub_resubscribe() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    const QString topic = QStringLiteral("market:quote:") + symbol_;
    // Producer errors: with no quote yet the reason replaces the spinner; once
    // a quote is up a failed refresh only marks the tile STALE (BaseWidget).
    track_topics({topic});
    hub.subscribe(this, topic, [this, topic](const QVariant& v) {
        if (!v.canConvert<services::QuoteData>())
            return;
        note_topic_value(topic);
        set_loading(false);
        populate(v.value<services::QuoteData>());
    });
    const QString ht = services::MarketDataService::performance_history_topic(symbol_);
    hub.subscribe(this, ht, [this](const QVariant& v) {
        if (v.canConvert<QVector<services::HistoryPoint>>())
            render_chart(v.value<QVector<services::HistoryPoint>>());
    });
    hub.subscribe_errors(this, ht, [this](const QString& err) {
        if (chart_->state() != ui::TimeSeriesChart::State::Ready)
            chart_->set_state(ui::TimeSeriesChart::State::Error, err);
    });
    hub_active_ = true;
}

void StockQuoteWidget::render_chart(const QVector<services::HistoryPoint>& bars) {
    QVector<QPointF> pts;
    pts.reserve(bars.size());
    for (const auto& b : bars)
        if (b.close > 0)
            pts.append(QPointF(double(b.timestamp) * 1000.0, b.close));
    chart_->clear();
    chart_->clear_annotations();
    if (pts.size() < 2) {
        chart_->set_state(ui::TimeSeriesChart::State::Empty, tr("No daily history for %1").arg(symbol_));
        return;
    }
    ui::TimeSeriesChart::SeriesSpec s;
    s.name = symbol_;
    s.points = pts;
    s.style = ui::TimeSeriesChart::Style::Area;
    s.color = pts.last().y() >= pts.first().y() ? ui::ChartColorRole::Positive : ui::ChartColorRole::Negative;
    chart_->add_series(s);
    if (last_prev_close_ > 0)
        chart_->add_hline(last_prev_close_, tr("Prev close"), ui::ChartColorRole::Muted);
    chart_->set_state(ui::TimeSeriesChart::State::Ready);
}

void StockQuoteWidget::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void StockQuoteWidget::populate(const services::QuoteData& q) {
    // Decimals and currency per instrument (Yahoo priceHint, FX pips,
    // minor-unit listings); the currency code rides in the status line.
    const auto inst = sq_instrument(symbol_, q);
    price_label_->setText(sq_price(q.price, inst));

    const int sign = q.change_pct > 0 ? 1 : q.change_pct < 0 ? -1 : 0;
    const QString color = sign > 0 ? ui::colors::POSITIVE() : sign < 0 ? ui::colors::NEGATIVE() : ui::colors::TEXT_SECONDARY();

    arrow_label_->setText(sign > 0 ? QString(QChar(0x25B2)) : sign < 0 ? QString(QChar(0x25BC)) : QString(QChar(0x25C6)));
    change_label_->setText(
        QString("%1 (%2)").arg(sq_fmt::price_change(q.change, q.price, inst), sq_fmt::pct(q.change_pct)));
    // Colours only change when the sign does — set_css_cached skips the CSS
    // re-parse on every other tick.
    set_css_cached(arrow_label_, QString("color: %1; font-size: " + fincept::ui::fonts::px(-2) + "px; background: transparent;").arg(color));
    set_css_cached(change_label_,
                   QString("color: %1; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: bold; background: transparent;").arg(color));
    set_css_cached(price_label_,
                   QString("color: %1; font-size: " + fincept::ui::fonts::px(14) + "px; font-weight: bold; background: transparent;").arg(color));

    // Session open and previous close come from the quote row itself (the
    // script always sent them; QuoteData now carries them — NaN when absent,
    // which dash_fmt::price() shows as "--"). The open used to be a permanent
    // "--" and the previous close was back-computed as price - change.
    open_val_->setText(sq_price(q.open, inst));
    high_val_->setText(sq_price(q.high, inst));
    low_val_->setText(sq_price(q.low, inst));
    const double prev = (std::isfinite(q.previous_close) && q.previous_close > 0)
                            ? q.previous_close
                            : (q.price > 0 ? q.price - q.change : 0.0); // a source without previous_close
    prev_val_->setText(sq_price(prev, inst));
    volume_val_->setText(q.volume > 0 ? sq_fmt::volume(q.volume) : sq_fmt::placeholder());
    if (q.volume > 0 && std::isfinite(q.average_volume) && q.average_volume > 0) {
        avgvol_val_->setText(sq_fmt::ratio(q.volume / q.average_volume, 2));
        avgvol_val_->setToolTip(tr("Today's volume over the 3-month average (%1)").arg(sq_fmt::volume(q.average_volume)));
    } else {
        avgvol_val_->setText(sq_fmt::placeholder());
    }

    if (q.low > 0 && q.high >= q.low && q.price > 0) {
        day_range_->set_range(q.low, q.high);
        day_range_->set_current(q.price);
    }
    if (std::isfinite(q.fifty_two_week_low) && std::isfinite(q.fifty_two_week_high) &&
        q.fifty_two_week_high > q.fifty_two_week_low && q.price > 0) {
        wk_range_->set_range(q.fifty_two_week_low, q.fifty_two_week_high);
        wk_range_->set_current(q.price);
    }

    // Session state (exchange calendar when the venue is known) · as of · delay · currency
    QStringList st;
    if (const auto* v = core::market::venue_for_symbol(symbol_, q.exchange)) {
        const auto s = core::market::session_status(*v, QDateTime::currentDateTimeUtc());
        QString part = core::market::venue_display_name(*v) + QStringLiteral(" ") +
                       core::market::session_state_label(s.state);
        if (s.next_change_utc.isValid())
            part += QStringLiteral(" · ") + tr("%1 %2", "next session state, then when (e.g. \"Open in 2h\")")
                                                .arg(core::market::session_state_label(s.next_state),
                                                     sq_fmt::relative(s.next_change_utc));
        st << part;
    } else if (!q.market_state.isEmpty()) {
        st << q.market_state;
    }
    if (q.market_time > 0)
        st << tr("as of %1").arg(sq_fmt::date_time(q.market_time, sq_fmt::DateStyle::Short));
    if (q.exchange_delay_min > 0)
        st << tr("delayed %1 min").arg(q.exchange_delay_min);
    if (!q.currency.isEmpty())
        st << q.currency;
    status_label_->setText(st.join(QStringLiteral(" · ")));

    if (prev > 0 && std::abs(prev - last_prev_close_) > 1e-12) {
        last_prev_close_ = prev;
        chart_->clear_annotations();
        chart_->add_hline(prev, tr("Prev close"), ui::ChartColorRole::Muted);
    }
}

void StockQuoteWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("QUOTE: %1").arg(symbol_));
    // Re-run tr() on the kept stat-cell headings. (This previously called
    // refresh_data(), which fires a network request and never touched a
    // single label.)
    for (int i = 0; i < stat_labels_.size() && i < stat_label_keys_.size(); ++i)
        stat_labels_[i]->setText(tr(stat_label_keys_[i].toUtf8().constData()));
}

} // namespace fincept::screens::widgets
