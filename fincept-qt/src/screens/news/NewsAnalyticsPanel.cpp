#include "screens/news/NewsAnalyticsPanel.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "services/markets/MarketDataService.h"
#include "ui/charts/HeatmapGrid.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/DateFormat.h"
#include "ui/tables/BarCellDelegate.h"
#include "ui/tables/DataTable.h"
#include "ui/tables/SparklineDelegate.h"
#include "ui/theme/ChartStyle.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QElapsedTimer>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

namespace {

namespace na = services::news_analytics;

enum NapTrendCol { kNapColKey = 0, kNapColMentions, kNapColDelta, kNapColTone, kNapColPrice, kNapColTrend, kNapColCount };

QString nap_tone_text(double v) {
    if (std::isnan(v))
        return QCoreApplication::translate("NewsAnalyticsPanel", "n/s");
    return (v > 0 ? QStringLiteral("+") : QString()) + QString::number(v, 'f', 2);
}

/// History request matching the visible span: intraday bars for a day or two,
/// hourly for a week, daily beyond.
QPair<QString, QString> nap_history_spec(qint64 span) {
    if (span <= 2 * 86400)
        return {QStringLiteral("5d"), QStringLiteral("15m")};
    if (span <= 8 * 86400)
        return {QStringLiteral("1mo"), QStringLiteral("1h")};
    return {QStringLiteral("3mo"), QStringLiteral("1d")};
}

} // namespace

NewsAnalyticsPanel::NewsAnalyticsPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("newsAnalyticsPanel");
    // Content is built on first show (ensure_built).
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this](const ui::ThemeTokens&) {
        // Item colours in the table are baked from the theme; the charts repaint themselves.
        if (built_)
            rebuild_trending();
    });
}

void NewsAnalyticsPanel::ensure_built() {
    if (built_)
        return;
    built_ = true;
    QElapsedTimer clock;
    clock.start();
    build_ui();
    refresh_all();
    set_window(window_from_, window_to_);
    LOG_DEBUG("NewsAnalyticsPanel", QStringLiteral("built on first show in %1 ms").arg(clock.elapsed()));
}

void NewsAnalyticsPanel::refresh_all() {
    const int pct = snap_.total > 0 ? static_cast<int>(std::lround(100.0 * snap_.scored / snap_.total)) : 0;
    scored_label_->setText(tr("%1 articles · tone scored for %2%").arg(snap_.total).arg(pct));
    scored_label_->setToolTip(tr("Tone is measured for English articles only; other languages are shown as "
                                 "not scored rather than neutral."));
    rebuild_timeline();
    rebuild_price_chart();
    rebuild_trending();
    rebuild_heatmap();
}

void NewsAnalyticsPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 4, 6, 4);
    root->setSpacing(4);

    // Header: title, selected window, scored share, mode toggles.
    auto* header = new QWidget(this);
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(6);
    title_ = new QLabel(header);
    title_->setObjectName("newsDrawerSectionTitle");
    hl->addWidget(title_);
    window_label_ = new QLabel(header);
    window_label_->setObjectName("newsIntelLabel");
    window_label_->hide();
    hl->addWidget(window_label_);
    window_clear_ = new QPushButton(QStringLiteral("x"), header);
    window_clear_->setObjectName("newsDetailCloseBtn");
    window_clear_->setFixedSize(18, 18);
    window_clear_->setCursor(Qt::PointingHandCursor);
    window_clear_->hide();
    connect(window_clear_, &QPushButton::clicked, this, [this]() {
        window_from_ = window_to_ = 0;
        window_label_->hide();
        window_clear_->hide();
        applying_range_ = true;
        timeline_->reset_x_range();
        if (price_chart_->isVisible())
            price_chart_->reset_x_range();
        applying_range_ = false;
        emit window_cleared();
    });
    hl->addWidget(window_clear_);
    scored_label_ = new QLabel(header);
    scored_label_->setObjectName("newsIntelLabel");
    hl->addWidget(scored_label_);
    hl->addStretch();

    auto make_toggle = [&](const QString& value, QVector<QPushButton*>& group, QString* state) {
        auto* b = new QPushButton(header);
        b->setObjectName("newsCommandBarPill");
        b->setCursor(Qt::PointingHandCursor);
        b->setProperty("pillValue", value);
        group.append(b);
        hl->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, value, &group, state]() {
            *state = value;
            for (auto* other : group) {
                other->setProperty("active", other->property("pillValue").toString() == value);
                other->style()->unpolish(other);
                other->style()->polish(other);
            }
            if (state == &heat_mode_)
                rebuild_heatmap();
            else
                rebuild_trending();
        });
    };
    make_toggle(QStringLiteral("TICKERS"), trend_btns_, &trend_mode_);
    make_toggle(QStringLiteral("ENTITIES"), trend_btns_, &trend_mode_);
    hl->addSpacing(8);
    make_toggle(QStringLiteral("CATEGORY"), heat_btns_, &heat_mode_);
    make_toggle(QStringLiteral("REGION"), heat_btns_, &heat_mode_);
    make_toggle(QStringLiteral("SECTOR"), heat_btns_, &heat_mode_);
    for (auto* b : trend_btns_ + heat_btns_) {
        const QString v = b->property("pillValue").toString();
        b->setProperty("active", v == trend_mode_ || v == heat_mode_);
    }
    root->addWidget(header);

    body_ = new QSplitter(Qt::Horizontal, this);
    body_->setChildrenCollapsible(false);

    // Column 1: price chart (symbol filter only) over the volume / tone timeline.
    auto* charts = new QSplitter(Qt::Vertical, body_);
    charts->setChildrenCollapsible(false);
    price_chart_ = new ui::TimeSeriesChart(charts);
    price_chart_->set_show_legend(false);
    price_chart_->set_zoom_enabled(true);
    price_chart_->hide();
    timeline_ = new ui::TimeSeriesChart(charts);
    timeline_->set_show_legend(true);
    timeline_->set_zoom_enabled(true);
    timeline_->set_crosshair_enabled(true);
    const int tone_pane = timeline_->add_pane(0.6);
    timeline_->set_y_range(tone_pane, ui::TimeSeriesChart::Axis::Left, -1.0, 1.0);
    // Tick labels are centred on their tick, so the count pane's bottom "0" and
    // the tone pane's top "+1.00" each hung half a line into the narrow gap
    // between the panes and overlapped; and with the price chart above, the
    // tone pane is only a line or two high, so its "+1.00 / 0.00 / -1.00"
    // landed on top of each other. Zero is unlabelled on both axes: the bars
    // stand on it and the tone pane draws its zero line.
    timeline_->set_value_formatter(
        ui::TimeSeriesChart::Axis::Left,
        [](double v) { return std::abs(v) < 1e-9 ? QString() : QString::number(v, 'f', 0); }, 0);
    timeline_->set_value_formatter(
        ui::TimeSeriesChart::Axis::Left,
        [](double v) { return std::abs(v) < 1e-9 ? QString() : nap_tone_text(v); }, tone_pane);
    charts->addWidget(price_chart_);
    charts->addWidget(timeline_);
    body_->addWidget(charts);

    // Zoom / pan on either chart picks the feed's window (debounced).
    range_debounce_ = new QTimer(this);
    range_debounce_->setSingleShot(true);
    range_debounce_->setInterval(350);
    connect(range_debounce_, &QTimer::timeout, this, [this]() {
        const qint64 from = static_cast<qint64>(pending_from_ms_ / 1000.0);
        const qint64 to = static_cast<qint64>(pending_to_ms_ / 1000.0);
        // The whole span (double-click reset) means "no window".
        if (from <= snap_.from + snap_.bucket && to >= snap_.to - snap_.bucket) {
            if (window_from_ != 0) {
                window_from_ = window_to_ = 0;
                window_label_->hide();
                window_clear_->hide();
                emit window_cleared();
            }
            return;
        }
        set_window(from, to);
        emit window_selected(from, to);
    });
    auto on_range = [this](double from, double to) {
        if (applying_range_)
            return;
        pending_from_ms_ = from;
        pending_to_ms_ = to;
        range_debounce_->start();
    };
    connect(timeline_, &ui::TimeSeriesChart::range_changed, this, on_range);
    connect(price_chart_, &ui::TimeSeriesChart::range_changed, this, on_range);
    // Linked crosshairs.
    connect(timeline_, &ui::TimeSeriesChart::crosshair_moved, price_chart_, &ui::TimeSeriesChart::set_linked_crosshair);
    connect(price_chart_, &ui::TimeSeriesChart::crosshair_moved, timeline_, &ui::TimeSeriesChart::set_linked_crosshair);

    // Column 2: trending table.
    auto* trend_host = new QWidget(body_);
    auto* tl = new QVBoxLayout(trend_host);
    tl->setContentsMargins(0, 0, 0, 0);
    tl->setSpacing(0);
    auto* table = new ui::DataTable(trend_host);
    table->setColumnCount(kNapColCount);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->hide();
    // Name column stretches (eliding), numbers and the sparkline keep their
    // content width; below that the optional columns are hidden
    // (fit_trend_columns) and, at the very least width, the table scrolls.
    {
        auto* hh = table->horizontalHeader();
        hh->setStretchLastSection(false);
        hh->setMinimumSectionSize(std::max(24, table->fontMetrics().averageCharWidth() * 4));
        for (int c = 0; c < kNapColCount; ++c)
            hh->setSectionResizeMode(c, c == kNapColKey ? QHeaderView::Stretch : QHeaderView::ResizeToContents);
    }
    table->setTextElideMode(Qt::ElideRight);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    // The viewport's own resize carries the width the columns must fit (the
    // table's arrives before the scroll area has laid the viewport out).
    table->viewport()->installEventFilter(this);
    auto* bar = new ui::BarCellDelegate(ui::BarCellDelegate::Mode::Bar, table);
    table->setItemDelegateForColumn(kNapColMentions, bar);
    auto* spark = new ui::SparklineDelegate(table);
    ui::SparklineStyle ss;
    ss.color_mode = ui::SparklineStyle::ColorMode::Accent; // a count series, not price direction
    ss.fill = true;
    spark->set_spark_style(ss);
    spark->set_width_chars(8);
    table->setItemDelegateForColumn(kNapColTrend, spark);
    connect(table, &QTableWidget::cellClicked, this, [this, table](int row, int) {
        auto* it = table->item(row, kNapColKey);
        if (it && trend_mode_ == QLatin1String("TICKERS"))
            emit ticker_clicked(it->data(Qt::UserRole).toString());
    });
    trend_table_ = table;
    trend_empty_ = new QLabel(trend_host);
    trend_empty_->setObjectName("newsEmptyStateHint");
    trend_empty_->setAlignment(Qt::AlignCenter);
    trend_empty_->setWordWrap(true);
    trend_empty_->hide();
    tl->addWidget(trend_table_, 1);
    tl->addWidget(trend_empty_, 1);
    body_->addWidget(trend_host);

    // Column 3: tone heatmap.
    heatmap_ = new ui::HeatmapGrid(body_);
    heatmap_->set_color_clamp(0.5);
    heatmap_->set_min_cell_size(QSize(92, 44));
    heatmap_->set_show_legend(true);
    heatmap_->set_value_formatter([](double v) { return nap_tone_text(v); });
    connect(heatmap_, &ui::HeatmapGrid::item_clicked, this, [this](const QString& id) {
        if (heat_mode_ == QLatin1String("CATEGORY"))
            emit category_clicked(id);
        else if (heat_mode_ == QLatin1String("REGION"))
            emit region_clicked(id);
    });
    body_->addWidget(heatmap_);
    body_->setStretchFactor(0, 5);
    body_->setStretchFactor(1, 4);
    body_->setStretchFactor(2, 3);
    root->addWidget(body_, 1);

    retranslate();
}

void NewsAnalyticsPanel::retranslate() {
    title_->setText(tr("NEWS ANALYTICS"));
    window_clear_->setToolTip(tr("Show the whole time range again"));
    const QHash<QString, QString> labels = {{"TICKERS", tr("TICKERS")},   {"ENTITIES", tr("ENTITIES")},
                                            {"CATEGORY", tr("CATEGORY")}, {"REGION", tr("REGION")},
                                            {"SECTOR", tr("SECTOR")}};
    for (auto* b : trend_btns_ + heat_btns_)
        b->setText(labels.value(b->property("pillValue").toString()));
    trend_table_->setHorizontalHeaderLabels(
        {trend_mode_ == QLatin1String("TICKERS") ? tr("TICKER") : tr("ENTITY"), tr("MENTIONS"), tr("VS PREV"),
         tr("TONE"), tr("PX %"), tr("TREND")});
    timeline_->set_title(tr("Articles and mean tone"));
    timeline_->setToolTip(tr("Articles per interval (bars) and the mean tone of the scored ones (area, -1 bearish … "
                             "+1 bullish).\nScroll to zoom, drag to pan: the feed then shows only that window. "
                             "Double-click to reset."));
    heatmap_->setToolTip(tr("Mean tone of the scored articles; click a cell to filter the feed"));
}

void NewsAnalyticsPanel::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange && built_)
        retranslate();
    QWidget::changeEvent(e);
}

bool NewsAnalyticsPanel::eventFilter(QObject* watched, QEvent* event) {
    if (trend_table_ && watched == trend_table_->viewport() &&
        (event->type() == QEvent::Resize || event->type() == QEvent::Show))
        fit_trend_columns();
    return QWidget::eventFilter(watched, event);
}

void NewsAnalyticsPanel::fit_trend_columns() {
    if (!trend_table_)
        return;
    auto* hh = trend_table_->horizontalHeader();
    const int avail = trend_table_->viewport()->width();
    // (QAbstractItemView's public sizeHintForColumn; QTableView's override is protected.)
    const QAbstractItemView* view = trend_table_;
    auto need = [view, hh](int col) { return std::max(hh->sectionSizeHint(col), view->sizeHintForColumn(col)); };
    const bool tickers = trend_mode_ == QLatin1String("TICKERS");
    // Always shown: the name (at least a short ticker), mentions and tone.
    int used = trend_table_->fontMetrics().averageCharWidth() * 8 + need(kNapColMentions) + need(kNapColTone);
    // Optional, most useful first; each is shown only when it fits whole.
    const struct {
        int col;
        bool wanted;
    } optional[] = {{kNapColPrice, tickers}, {kNapColDelta, true}, {kNapColTrend, true}};
    for (const auto& o : optional) {
        const bool show = o.wanted && used + need(o.col) <= avail;
        if (show)
            used += need(o.col);
        if (trend_table_->isColumnHidden(o.col) == show)
            trend_table_->setColumnHidden(o.col, !show);
    }
}

void NewsAnalyticsPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    ensure_built();
    visible_ = true;
    subscribe_market();
}

void NewsAnalyticsPanel::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    visible_ = false;
    datahub::DataHub::instance().unsubscribe(this);
    subscribed_.clear();
}

void NewsAnalyticsPanel::set_snapshot(const NewsAnalyticsSnapshot& snap) {
    const bool symbol_changed = snap.symbol != snap_.symbol;
    snap_ = snap;
    if (!built_)
        return; // shown from snap_ when first built
    refresh_all();
    if (visible_ || symbol_changed)
        subscribe_market();
}

void NewsAnalyticsPanel::set_entities(const QVector<services::news_analytics::TrendRow>& rows) {
    entities_ = rows;
    if (built_ && trend_mode_ == QLatin1String("ENTITIES"))
        rebuild_trending();
}

void NewsAnalyticsPanel::set_sectors(const QHash<QString, QString>& sector_by_symbol) {
    if (sector_by_symbol == sectors_)
        return;
    sectors_ = sector_by_symbol;
    if (built_ && heat_mode_ == QLatin1String("SECTOR"))
        rebuild_heatmap();
}

void NewsAnalyticsPanel::set_window(qint64 from, qint64 to) {
    window_from_ = from;
    window_to_ = to;
    if (!built_)
        return;
    const bool on = from > 0 && to > from;
    window_label_->setVisible(on);
    window_clear_->setVisible(on);
    if (!on)
        return;
    namespace fmt = ui::formatting;
    window_label_->setText(tr("Window %1 – %2").arg(fmt::date_time(from), fmt::time(to)));
    applying_range_ = true;
    timeline_->set_x_range(static_cast<double>(from) * 1000.0, static_cast<double>(to) * 1000.0);
    if (price_chart_->isVisible())
        price_chart_->set_x_range(static_cast<double>(from) * 1000.0, static_cast<double>(to) * 1000.0);
    applying_range_ = false;
}

void NewsAnalyticsPanel::rebuild_timeline() {
    applying_range_ = true;
    timeline_->clear();
    QVector<QPointF> counts;
    QVector<QPointF> tone;
    counts.reserve(snap_.series.size());
    for (const auto& p : snap_.series) {
        const double x = static_cast<double>(p.start) * 1000.0;
        counts.append({x, static_cast<double>(p.count)});
        tone.append({x, p.mean_score}); // NaN = no scored article: a gap, not zero
    }
    using TSC = ui::TimeSeriesChart;
    TSC::SeriesSpec vol;
    vol.name = tr("Articles");
    vol.points = counts;
    vol.style = TSC::Style::Bars;
    vol.color = TSC::ColorRole::Info;
    vol.baseline = 0.0;
    vol.rebase = false;
    vol.formatter = [](double v) { return QString::number(v, 'f', 0); };
    timeline_->add_series(vol);
    TSC::SeriesSpec tn;
    tn.name = tr("Mean tone");
    tn.points = tone;
    tn.style = TSC::Style::Baseline;
    tn.pane = 1;
    tn.baseline = 0.0;
    tn.rebase = false;
    tn.formatter = [](double v) { return nap_tone_text(v); };
    timeline_->add_series(tn);
    if (snap_.series.isEmpty())
        timeline_->set_state(TSC::State::Empty, tr("No articles in this range"));
    else
        timeline_->set_state(TSC::State::Ready);
    if (window_from_ > 0 && window_to_ > window_from_)
        timeline_->set_x_range(static_cast<double>(window_from_) * 1000.0, static_cast<double>(window_to_) * 1000.0);
    applying_range_ = false;
}

void NewsAnalyticsPanel::rebuild_price_chart() {
    const bool on = !snap_.symbol.isEmpty();
    price_chart_->setVisible(on);
    // Two stacked charts need the room of two: at the screen's 180 px floor the
    // price chart squeezed the timeline until its tone pane was a few pixels
    // high and its "+1.00" / "-1.00" labels sat on top of each other. The
    // timeline keeps enough height for both panes' labels, and the panel asks
    // for the extra height while the price chart is shown.
    const int fh = fontMetrics().height();
    timeline_->setMinimumHeight(fh * 9);
    price_chart_->setMinimumHeight(fh * 6);
    setMinimumHeight(std::max(180, fh * 12) + (on ? fh * 7 : 0));
    if (!on)
        return;
    price_chart_->set_title(tr("%1 price with headlines").arg(snap_.symbol));
    // The series arrives from the history subscription; markers are re-added
    // whenever either side changes (see subscribe_market()).
}

void NewsAnalyticsPanel::subscribe_market() {
    if (!visible_)
        return;
    auto& hub = datahub::DataHub::instance();
    QSet<QString> wanted;
    // Quotes for the trending tickers on screen.
    QStringList quote_syms;
    for (const auto& r : std::as_const(snap_.tickers)) {
        if (quote_syms.size() >= 12)
            break;
        quote_syms.append(r.key);
    }
    for (const QString& sym : std::as_const(quote_syms))
        wanted.insert(services::MarketDataService::quote_topic(sym));
    QString history_topic;
    if (!snap_.symbol.isEmpty()) {
        const auto spec = nap_history_spec(snap_.to - snap_.from);
        history_topic = services::MarketDataService::history_topic(snap_.symbol, spec.first, spec.second);
        wanted.insert(history_topic);
    }
    if (wanted == subscribed_)
        return;
    hub.unsubscribe(this);
    subscribed_ = wanted;
    QStringList topics;
    for (const QString& sym : std::as_const(quote_syms)) {
        const QString topic = services::MarketDataService::quote_topic(sym);
        topics.append(topic);
        hub.subscribe(this, topic, [this, sym](const QVariant& v) {
            if (v.canConvert<services::QuoteData>())
                on_quote(sym, v.value<services::QuoteData>().change_pct);
        });
    }
    if (!history_topic.isEmpty()) {
        topics.append(history_topic);
        const QString sym = snap_.symbol;
        hub.subscribe(this, history_topic, [this, sym](const QVariant& v) {
            if (!v.canConvert<QVector<services::HistoryPoint>>() || sym != snap_.symbol)
                return;
            const auto hist = v.value<QVector<services::HistoryPoint>>();
            using TSC = ui::TimeSeriesChart;
            applying_range_ = true;
            price_chart_->clear();
            QVector<QPointF> pts;
            pts.reserve(hist.size());
            for (const auto& h : hist)
                if (h.close > 0)
                    pts.append({static_cast<double>(h.timestamp) * 1000.0, h.close});
            TSC::SeriesSpec s;
            s.name = sym;
            s.points = pts;
            s.style = TSC::Style::Line;
            s.color = TSC::ColorRole::Accent;
            s.rebase = false;
            const int sid = price_chart_->add_series(s);
            // One news marker per headline about the symbol, on the price line.
            for (const auto& m : std::as_const(snap_.symbol_news)) {
                if (pts.isEmpty() || m.ts * 1000.0 < pts.first().x())
                    continue;
                QString tip = ui::formatting::date_time(m.ts) + QLatin1Char('\n') + m.headline;
                if (!std::isnan(m.tone))
                    tip += QLatin1Char('\n') + tr("Tone %1").arg(nap_tone_text(m.tone));
                price_chart_->add_marker(static_cast<double>(m.ts) * 1000.0, sid, TSC::MarkerKind::News, {}, tip);
            }
            price_chart_->set_state(pts.isEmpty() ? TSC::State::Empty : TSC::State::Ready,
                                    pts.isEmpty() ? tr("No price history for %1").arg(sym) : QString());
            const double lo = static_cast<double>(window_from_ > 0 ? window_from_ : snap_.from) * 1000.0;
            const double hi = static_cast<double>(window_to_ > 0 ? window_to_ : snap_.to) * 1000.0;
            price_chart_->set_x_range(lo, hi);
            applying_range_ = false;
        });
        price_chart_->set_state(ui::TimeSeriesChart::State::Loading);
    }
    if (!topics.isEmpty())
        hub.request(topics);
}

void NewsAnalyticsPanel::on_quote(const QString& symbol, double change_pct) {
    quote_change_.insert(symbol, change_pct);
    if (!built_ || trend_mode_ != QLatin1String("TICKERS"))
        return;
    for (int r = 0; r < trend_table_->rowCount(); ++r) {
        auto* key = trend_table_->item(r, kNapColKey);
        if (!key || key->data(Qt::UserRole).toString() != symbol)
            continue;
        auto* it = trend_table_->item(r, kNapColPrice);
        if (!it)
            continue;
        it->setText(ui::chart_style::percent_text(change_pct, 2, true));
        it->setForeground(QColor(change_pct >= 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
    }
}

void NewsAnalyticsPanel::rebuild_trending() {
    const bool tickers = trend_mode_ == QLatin1String("TICKERS");
    const auto& rows = tickers ? snap_.tickers : entities_;
    trend_table_->setSortingEnabled(false);
    trend_table_->setRowCount(0);
    trend_table_->setHorizontalHeaderLabels(
        {tickers ? tr("TICKER") : tr("ENTITY"), tr("MENTIONS"), tr("VS PREV"), tr("TONE"), tr("PX %"), tr("TREND")});
    const bool empty = rows.isEmpty();
    trend_table_->setVisible(!empty);
    trend_empty_->setVisible(empty);
    if (empty) {
        trend_empty_->setText(tickers ? tr("No ticker is mentioned twice in this range.")
                                      : tr("Entities appear once the headlines have been analysed (open the "
                                           "INTEL drawer or wait for the next pass)."));
        return;
    }
    trend_table_->setRowCount(static_cast<int>(rows.size()));
    for (int r = 0; r < rows.size(); ++r) {
        const auto& row = rows[r];
        auto* key = new QTableWidgetItem(row.key);
        key->setData(Qt::UserRole, row.key);
        key->setToolTip(tickers ? tr("Click to filter the feed by %1").arg(row.key) : row.key);
        key->setForeground(QColor(tickers ? ui::colors::WARNING() : ui::colors::TEXT_PRIMARY()));
        auto* mentions = new QTableWidgetItem();
        mentions->setData(Qt::EditRole, row.current);
        mentions->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        QString delta_text;
        const double pct = na::change_pct(row.current, row.previous);
        if (row.previous == 0)
            delta_text = tr("new");
        else
            delta_text = ui::chart_style::percent_text(pct, 0, true);
        auto* delta = new QTableWidgetItem(delta_text);
        delta->setToolTip(tr("%1 mentions now, %2 in the previous window of the same length")
                              .arg(row.current)
                              .arg(row.previous));
        delta->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        delta->setForeground(QColor(ui::colors::TEXT_SECONDARY()));
        auto* tone = new QTableWidgetItem(nap_tone_text(row.mean_score));
        tone->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        tone->setToolTip(row.scored > 0 ? tr("Mean tone of %n scored article(s)", "", row.scored)
                                        : tr("No article about it was tone-scored (non-English sources)"));
        tone->setForeground(std::isnan(row.mean_score) ? QColor(ui::colors::TEXT_TERTIARY())
                            : row.mean_score >= 0.05  ? QColor(ui::colors::POSITIVE())
                            : row.mean_score <= -0.05 ? QColor(ui::colors::NEGATIVE())
                                                      : QColor(ui::colors::TEXT_SECONDARY()));
        auto* price = new QTableWidgetItem(QStringLiteral("—"));
        price->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        if (tickers && quote_change_.contains(row.key)) {
            const double c = quote_change_.value(row.key);
            price->setText(ui::chart_style::percent_text(c, 2, true));
            price->setForeground(QColor(c >= 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
        }
        auto* trend = new QTableWidgetItem();
        trend->setData(ui::SparklineDelegate::kDefaultRole, QVariant::fromValue(row.spark));
        trend->setToolTip(tr("Mentions over the two windows (oldest left)"));
        trend_table_->setItem(r, kNapColKey, key);
        trend_table_->setItem(r, kNapColMentions, mentions);
        trend_table_->setItem(r, kNapColDelta, delta);
        trend_table_->setItem(r, kNapColTone, tone);
        trend_table_->setItem(r, kNapColPrice, price);
        trend_table_->setItem(r, kNapColTrend, trend);
    }
    fit_trend_columns(); // header modes size the visible columns
}

void NewsAnalyticsPanel::rebuild_heatmap() {
    QVector<ui::TreemapItem> items;
    auto add = [&items](const QString& id, const QString& label, int count, double tone, int scored) {
        ui::TreemapItem it;
        it.id = id;
        it.label = label.isEmpty() ? QStringLiteral("—") : label;
        it.sublabel = QCoreApplication::translate("NewsAnalyticsPanel", "%1 art · %2")
                          .arg(count)
                          .arg(scored > 0 ? nap_tone_text(tone) : QCoreApplication::translate("NewsAnalyticsPanel", "n/s"));
        it.weight = count;
        it.value = scored > 0 ? tone : std::numeric_limits<double>::quiet_NaN();
        items.append(it);
    };
    if (heat_mode_ == QLatin1String("SECTOR")) {
        // Ticker mentions folded into their sectors (tickers whose sector is known).
        struct Acc {
            int count = 0;
            int scored = 0;
            double sum = 0;
        };
        QHash<QString, Acc> acc;
        for (const auto& r : std::as_const(snap_.tickers)) {
            const QString sector = sectors_.value(r.key);
            if (sector.isEmpty())
                continue;
            auto& a = acc[sector];
            a.count += r.current;
            if (r.scored > 0 && !std::isnan(r.mean_score)) {
                a.scored += r.scored;
                a.sum += r.mean_score * r.scored;
            }
        }
        QStringList keys = acc.keys();
        std::sort(keys.begin(), keys.end(), [&acc](const QString& a, const QString& b) {
            return acc.value(a).count > acc.value(b).count;
        });
        for (const QString& k : keys) {
            const Acc a = acc.value(k);
            add(k, k, a.count, a.scored > 0 ? a.sum / a.scored : 0.0, a.scored);
        }
        heatmap_->set_items(items);
        heatmap_->set_state(items.isEmpty() ? ui::PaintedChart::Empty : ui::PaintedChart::Ready,
                            items.isEmpty() ? tr("Sectors appear as the mentioned tickers' profiles load") : QString());
        return;
    }
    const auto& rows = heat_mode_ == QLatin1String("REGION") ? snap_.regions : snap_.categories;
    for (const auto& r : rows)
        add(r.key, r.key, r.current, r.mean_score, r.scored);
    heatmap_->set_items(items);
    heatmap_->set_state(items.isEmpty() ? ui::PaintedChart::Empty : ui::PaintedChart::Ready);
}

} // namespace fincept::screens
