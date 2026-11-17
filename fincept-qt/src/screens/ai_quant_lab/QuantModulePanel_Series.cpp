// src/screens/ai_quant_lab/QuantModulePanel_Series.cpp
//
// D1 — the real-data source of the views that analyse a numeric series.
//
// Such a view used to open on an empty paste box (Statsmodels, Functime, GluonTS, Quant
// Reporting, Portfolio Optimisation, Factor Evaluation, GS, Feature Engineering, Strategy
// Builder). Now its form starts with a source row:
//
//   Source   [Ticker (daily prices) | Custom series]
//   Symbol   [shared symbol search]           default: the first watchlist symbol, else SPY
//   Versus   [shared symbol search]           ops on two series (default QQQ)
//   Symbols  [shared symbol search, multi]    ops on several (default SPY, QQQ, IWM, TLT, GLD)
//   Period   [6 months … 5 years]             default 1 year
//
// In Ticker mode a run first downloads the daily bars through the terminal's market-history
// topics (MarketDataService "market:history:<SYM>:<period>:1d" on the DataHub — free, cached,
// shared with every other screen), aligns the symbols on their common dates, and the run then
// reads, from each paste box marked with mark_series_input(), exactly the text a user would have
// pasted (closes, returns, a covariance matrix…): series_text(). The paste box stays as the
// "Custom series" source and LOAD SYNTHETIC SAMPLE fills it (and switches to it). The result
// says which data it was computed on (symbol, period, days, last date).
#include "screens/ai_quant_lab/QuantModulePanel.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"
#include "services/markets/MarketDataService.h"
#include "storage/repositories/WatchlistRepository.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QTimeZone>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <memory>

namespace fincept::screens {

using namespace fincept::screens::quant_styles;
using fincept::core::instrument::Instrument;
using fincept::services::HistoryPoint;
using fincept::services::MarketDataService;

namespace {

constexpr int kSeriesFetchTimeoutMs = 60 * 1000;
constexpr int kSeriesMomentumDays = 20;
constexpr int kSeriesHoldoutDays = 20; // a month of trading days held out for forecast evaluation
const char* const kSeriesPeriods[] = {"6mo", "1y", "2y", "3y", "5y"};

QString series_period_label(const QString& period) {
    if (period == QLatin1String("6mo"))
        return QuantModulePanel::tr("6 months");
    if (period == QLatin1String("2y"))
        return QuantModulePanel::tr("2 years");
    if (period == QLatin1String("3y"))
        return QuantModulePanel::tr("3 years");
    if (period == QLatin1String("5y"))
        return QuantModulePanel::tr("5 years");
    return QuantModulePanel::tr("1 year");
}

Instrument series_instrument(const QString& symbol) {
    Instrument inst;
    inst.symbol = symbol;
    inst.display = symbol;
    return inst;
}

QStringList series_split(const QString& text) {
    static const QRegularExpression seps(QStringLiteral("[,;\\s]+"));
    QStringList out;
    for (const QString& part : text.split(seps, Qt::SkipEmptyParts)) {
        const QString s = part.trimmed().toUpper();
        if (!s.isEmpty() && !out.contains(s))
            out.append(s);
    }
    return out;
}

QString series_join(const QVector<double>& v, int decimals) {
    QStringList parts;
    parts.reserve(v.size());
    for (double x : v)
        parts.append(QString::number(x, 'f', decimals));
    return parts.join(QLatin1Char(','));
}

QJsonArray series_json(const QVector<double>& v, int decimals) {
    QJsonArray a;
    const double scale = std::pow(10.0, decimals);
    for (double x : v)
        a.append(std::round(x * scale) / scale);
    return a;
}

QVector<double> series_returns(const QVector<double>& closes) {
    QVector<double> r;
    if (closes.size() < 2)
        return r;
    r.reserve(closes.size() - 1);
    for (int i = 1; i < closes.size(); ++i)
        r.append(closes[i - 1] > 0 ? closes[i] / closes[i - 1] - 1.0 : 0.0);
    return r;
}

double series_mean(const QVector<double>& v, int from, int to) { // [from, to)
    double s = 0;
    for (int i = from; i < to; ++i)
        s += v[i];
    return to > from ? s / (to - from) : 0.0;
}

double series_std(const QVector<double>& v, int from, int to) {
    const int n = to - from;
    if (n < 2)
        return 0.0;
    const double m = series_mean(v, from, to);
    double s = 0;
    for (int i = from; i < to; ++i)
        s += (v[i] - m) * (v[i] - m);
    return std::sqrt(s / (n - 1));
}

} // namespace

// ── The structures (declared in the header) ──────────────────────────────────

struct QuantModulePanel::SeriesSource {
    QString view;
    SeriesShape shape = SeriesShape::Single;
    QPointer<QComboBox> mode; ///< 0 = Ticker, 1 = Custom series
    /// The symbol pickers, remembered by the view's InputMemory under their "quantMemoryName"
    /// (ui::InputMemory::track(name, box): the picked instrument, restored without a network call).
    QPointer<ui::SymbolSearchBox> box1;
    QPointer<ui::SymbolSearchBox> box2;
    QPointer<ui::SymbolSearchBox> list; ///< SeriesShape::List: one multi-symbol box
    QStringList list_committed;         ///< its symbols as last applied (a single pick is added to them)
    QPointer<QComboBox> period;
    QList<QPointer<QWidget>> ticker_rows;
    QList<QPointer<QLineEdit>> inputs; ///< the marked paste boxes
    QList<QPointer<QWidget>> custom_only; ///< shown only with a Custom series
};

struct QuantModulePanel::SeriesData {
    QStringList symbols;
    QString period;
    QVector<QDate> dates;                ///< common trading days, oldest first
    QVector<QVector<double>> open, high, low, close, volume; ///< per symbol, aligned on `dates`
};

// ── The source row ───────────────────────────────────────────────────────────

QString QuantModulePanel::series_default_symbol() {
    if (!default_symbol_.isEmpty())
        return default_symbol_;
    default_symbol_ = QStringLiteral("SPY");
    // The user's context first: the first symbol of their default (else first) watchlist.
    auto lists = WatchlistRepository::instance().list_all();
    if (lists.is_ok()) {
        QVector<Watchlist> ordered = lists.value();
        std::stable_sort(ordered.begin(), ordered.end(), [](const Watchlist& a, const Watchlist& b) {
            if (a.is_default != b.is_default)
                return a.is_default;
            return a.sort_order < b.sort_order;
        });
        for (const Watchlist& w : std::as_const(ordered)) {
            auto stocks = WatchlistRepository::instance().get_stocks(w.id);
            if (!stocks.is_ok())
                continue;
            for (const WatchlistStock& s : stocks.value()) {
                if (!s.symbol.trimmed().isEmpty()) {
                    default_symbol_ = s.symbol.trimmed().toUpper();
                    return default_symbol_;
                }
            }
        }
    }
    return default_symbol_;
}

void QuantModulePanel::add_series_source(QWidget* page, QVBoxLayout* layout, SeriesShape shape,
                                         const QString& second_label) {
    auto vit = views_.find(building_view_);
    if (vit == views_.end() || !page || !layout)
        return;
    const QString view = building_view_;
    auto src = std::make_shared<SeriesSource>();
    src->view = view;
    src->shape = shape;

    auto* mode = new QComboBox(page);
    mode->setObjectName(QStringLiteral("quantSeriesMode_") + view);
    mode->setProperty("quantMemoryName", QStringLiteral("src_mode"));
    mode->addItem(tr("Ticker (daily prices)"));
    mode->addItem(tr("Custom series"));
    mode->setStyleSheet(combo_ss());
    mode->setToolTip(tr("Ticker: the analysis runs on the daily prices of the symbol(s) below.\n"
                        "Custom series: it runs on the values you paste (or on a synthetic sample)."));
    src->mode = mode;
    layout->addWidget(build_input_row(tr("Source"), mode, page));

    // A symbol picker in selector mode. The view's InputMemory tracks the box itself under its
    // "quantMemoryName" (attach_memory → ui::InputMemory::track(name, box)): the picked instrument
    // comes back without a network call; a value saved by the former hidden field (a bare symbol
    // under the same name) is shown as text and looked up in the local index.
    auto make_symbol = [this, page, layout, src](const QString& label, const QString& memory_name,
                                                 const QString& initial, QPointer<ui::SymbolSearchBox>& box_out) {
        auto* box = new ui::SymbolSearchBox(page);
        box->set_compact(true);
        box->set_clear_on_pick(false);
        box->set_recent_key(QStringLiteral("ai_quant_lab"));
        box->set_placeholder(tr("Symbol, name or ISIN"));
        box->setProperty("quantMemoryName", memory_name);
        box->set_instrument(series_instrument(initial));
        auto* row = build_input_row(label, box, page);
        layout->addWidget(row);
        src->ticker_rows.append(row);
        box_out = box;
    };

    const QString first = series_default_symbol();
    if (shape == SeriesShape::List) {
        auto* box = new ui::SymbolSearchBox(page);
        box->set_compact(true);
        box->set_multi(true);
        box->set_clear_on_pick(false);
        box->set_recent_key(QStringLiteral("ai_quant_lab"));
        box->set_placeholder(tr("Symbols, comma-separated (Enter to apply)"));
        box->setProperty("quantMemoryName", QStringLiteral("src_list"));
        src->list_committed = {QStringLiteral("SPY"), QStringLiteral("QQQ"), QStringLiteral("IWM"),
                               QStringLiteral("TLT"), QStringLiteral("GLD")};
        box->set_text(src->list_committed.join(QStringLiteral(", ")));
        const std::weak_ptr<SeriesSource> weak = src;
        // Shows `syms` as the list (also what a single pick is added to next).
        auto apply = [weak, box](const QStringList& syms) {
            if (const auto s = weak.lock())
                s->list_committed = syms;
            const QString shown = syms.join(QStringLiteral(", "));
            if (box->text() != shown)
                box->set_text(shown);
        };
        connect(box, &ui::SymbolSearchBox::instruments_picked, box, [apply](const QVector<Instrument>& v) {
            QStringList syms;
            for (const auto& inst : v)
                if (!inst.symbol.isEmpty() && !syms.contains(inst.symbol))
                    syms.append(inst.symbol);
            if (!syms.isEmpty())
                apply(syms);
        });
        // A single pick replaces the box text with that symbol: it is added to the list instead.
        connect(box, &ui::SymbolSearchBox::instrument_picked, box, [weak, apply](const Instrument& inst) {
            const auto s = weak.lock();
            if (!s || inst.symbol.isEmpty())
                return;
            QStringList syms = s->list_committed;
            if (!syms.contains(inst.symbol))
                syms.append(inst.symbol);
            apply(syms);
        });
        // Text that arrives while the box is not being edited (the remembered list restored) and
        // text the user leaves the box with become the list a later pick is added to.
        if (QLineEdit* edit = box->line_edit()) {
            connect(edit, &QLineEdit::textChanged, box, [weak, edit](const QString& text) {
                if (const auto s = weak.lock(); s && !edit->hasFocus())
                    s->list_committed = series_split(text);
            });
            connect(edit, &QLineEdit::editingFinished, box, [weak, edit]() {
                if (const auto s = weak.lock())
                    s->list_committed = series_split(edit->text());
            });
        }
        auto* row = build_input_row(tr("Symbols"), box, page);
        layout->addWidget(row);
        src->ticker_rows.append(row);
        src->list = box;
    } else {
        make_symbol(tr("Symbol"), QStringLiteral("src_symbol"), first, src->box1);
        if (shape == SeriesShape::Pair) {
            const QString second = first == QLatin1String("QQQ") ? QStringLiteral("SPY") : QStringLiteral("QQQ");
            make_symbol(second_label.isEmpty() ? tr("Versus") : second_label, QStringLiteral("src_symbol2"), second,
                        src->box2);
        }
    }

    auto* period = new QComboBox(page);
    period->setProperty("quantMemoryName", QStringLiteral("src_period"));
    for (const char* p : kSeriesPeriods)
        period->addItem(series_period_label(QLatin1String(p)), QLatin1String(p));
    period->setCurrentIndex(1); // 1 year
    period->setStyleSheet(combo_ss());
    auto* prow = build_input_row(tr("Period"), period, page);
    layout->addWidget(prow);
    src->ticker_rows.append(prow);
    src->period = period;

    vit->series = src;
    connect(mode, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, view](int) {
        sync_series_mode(view);
        if (pane_ && current_view_ == view)
            pane_->set_signature(view_signature(view));
    });
}

void QuantModulePanel::mark_series_input(QLineEdit* edit, SeriesKind kind, int role) {
    auto vit = views_.find(building_view_);
    if (!edit || vit == views_.end() || !vit->series)
        return;
    edit->setProperty("quantSeriesView", building_view_);
    edit->setProperty("quantSeriesKind", static_cast<int>(kind));
    edit->setProperty("quantSeriesRole", role);
    vit->series->inputs.append(edit);
}

void QuantModulePanel::mark_series_widget(QWidget* widget, bool ticker_only) {
    auto vit = views_.find(building_view_);
    if (!widget || vit == views_.end() || !vit->series)
        return;
    if (ticker_only)
        vit->series->ticker_rows.append(widget);
    else
        vit->series->custom_only.append(widget);
}

QuantModulePanel::SeriesSource* QuantModulePanel::series_source(const QString& view) const {
    const auto it = views_.constFind(view);
    return it == views_.cend() ? nullptr : it->series.get();
}

bool QuantModulePanel::series_ticker_mode(const QString& view) const {
    const SeriesSource* src = series_source(view);
    return src && src->mode && src->mode->currentIndex() == 0;
}

void QuantModulePanel::sync_series_mode(const QString& view) {
    SeriesSource* src = series_source(view);
    if (!src)
        return;
    const bool ticker = series_ticker_mode(view);
    for (const auto& row : std::as_const(src->ticker_rows))
        if (row && row->isHidden() == ticker)
            row->setVisible(ticker);
    for (const auto& w : std::as_const(src->custom_only))
        if (w && w->isHidden() != ticker)
            w->setVisible(!ticker);
    for (const auto& e : std::as_const(src->inputs)) {
        // build_input_row() puts the box in a row widget; that row is the "Custom series" input.
        QWidget* row = e ? e->parentWidget() : nullptr;
        if (row && row->isHidden() != ticker)
            row->setVisible(!ticker);
    }
}

QStringList QuantModulePanel::series_symbols(const QString& view) const {
    const SeriesSource* src = series_source(view);
    if (!src)
        return {};
    // What the boxes show: the picked instrument's symbol while the text still shows the pick,
    // else the text typed (a symbol entered without picking it from the list).
    if (src->shape == SeriesShape::List)
        return src->list ? series_split(src->list->text()) : QStringList();
    auto symbol_of = [](const ui::SymbolSearchBox* box) -> QString {
        if (!box)
            return {};
        const Instrument inst = box->current_instrument();
        const QString sym = inst.is_valid() && !inst.symbol.isEmpty() ? inst.symbol : box->text();
        return sym.trimmed().toUpper();
    };
    QStringList out;
    if (const QString s = symbol_of(src->box1); !s.isEmpty())
        out.append(s);
    if (src->shape == SeriesShape::Pair)
        if (const QString s = symbol_of(src->box2); !s.isEmpty())
            out.append(s);
    return out;
}

QStringList QuantModulePanel::series_signature_skip(const QString& view) const {
    const SeriesSource* src = series_source(view);
    if (!src)
        return {};
    QStringList skip;
    if (series_ticker_mode(view)) {
        // The paste boxes are not what a ticker run reads.
        for (const auto& e : src->inputs)
            if (e)
                skip.append(text_inputs_.key(e.data()));
    } else {
        skip << QStringLiteral("src_symbol") << QStringLiteral("src_symbol2") << QStringLiteral("src_list")
             << QStringLiteral("src_period");
    }
    return skip;
}

// ── Downloading the bars ─────────────────────────────────────────────────────

void QuantModulePanel::fetch_series(quint64 pane_id, const QString& view,
                                    std::function<void(const QJsonObject& meta)> then) {
    SeriesSource* src = series_source(view);
    const QStringList symbols = series_symbols(view);
    const QString period = src && src->period ? src->period->currentData().toString() : QStringLiteral("1y");
    const int need = src && src->shape == SeriesShape::Pair ? 2 : src && src->shape == SeriesShape::List ? 2 : 1;
    if (symbols.size() < need) {
        pane_->fail(pane_id,
                    need > 1 ? tr("Pick at least %1 symbols, or switch the source to Custom series.").arg(need)
                             : tr("Pick a symbol, or switch the source to Custom series."),
                    tr("Check the inputs"));
        return;
    }
    pane_->set_loading_text(tr("Downloading %1 of daily prices for %2…")
                                .arg(series_period_label(period), symbols.join(QStringLiteral(", "))));

    cancel_series_fetch(pane_id);
    auto* owner = new QObject(this);
    fetches_.insert(pane_id, owner);
    struct Job {
        QHash<QString, QVector<HistoryPoint>> bars;
        QStringList waiting;
        bool done = false;
    };
    auto job = std::make_shared<Job>();
    job->waiting = symbols;

    QPointer<QObject> guard(owner);
    const QString joiner = src && src->shape == SeriesShape::Pair ? tr(" vs ") : QStringLiteral(", ");
    // Ends the download once: builds the aligned series and hands over to the action.
    auto finish = [this, pane_id, view, symbols, period, job, guard, then, joiner](const QString& error) {
        if (job->done)
            return;
        job->done = true;
        if (guard)
            guard->deleteLater(); // drops the hub subscriptions
        fetches_.remove(pane_id);
        if (!pane_ || !pane_->is_current(pane_id))
            return;
        if (!error.isEmpty()) {
            pane_->fail(pane_id, error, tr("Could not download prices"));
            return;
        }
        // Align on the dates every symbol has.
        auto data = std::make_shared<SeriesData>();
        data->symbols = symbols;
        data->period = period;
        QMap<QDate, int> count;
        QVector<QHash<QDate, HistoryPoint>> by_date(symbols.size());
        for (int s = 0; s < symbols.size(); ++s) {
            for (const HistoryPoint& p : job->bars.value(symbols[s])) {
                if (p.close <= 0 || !std::isfinite(p.close))
                    continue;
                const QDate d = QDateTime::fromSecsSinceEpoch(p.timestamp, QTimeZone::UTC).date();
                if (!by_date[s].contains(d))
                    count[d] += 1;
                by_date[s].insert(d, p);
            }
        }
        for (auto it = count.cbegin(); it != count.cend(); ++it)
            if (it.value() == symbols.size())
                data->dates.append(it.key());
        const int n = static_cast<int>(data->dates.size());
        if (n < 30) {
            pane_->fail(pane_id,
                        tr("Only %1 common trading days for %2 — pick a longer period or other symbols.")
                            .arg(n)
                            .arg(symbols.join(QStringLiteral(", "))),
                        tr("Not enough history"));
            return;
        }
        for (int s = 0; s < symbols.size(); ++s) {
            QVector<double> o, h, l, c, v;
            o.reserve(n);
            h.reserve(n);
            l.reserve(n);
            c.reserve(n);
            v.reserve(n);
            for (const QDate& d : std::as_const(data->dates)) {
                const HistoryPoint p = by_date[s].value(d);
                o.append(p.open > 0 ? p.open : p.close);
                h.append(p.high > 0 ? p.high : p.close);
                l.append(p.low > 0 ? p.low : p.close);
                c.append(p.close);
                v.append(static_cast<double>(p.volume));
            }
            data->open.append(o);
            data->high.append(h);
            data->low.append(l);
            data->close.append(c);
            data->volume.append(v);
        }
        series_data_.insert(view, data);
        const QString last = QLocale().toString(data->dates.last(), QLocale::ShortFormat);
        QJsonObject meta;
        meta.insert(QStringLiteral("source"),
                    tr("Data: %1 · %2 of daily prices · %3 trading days · last %4")
                        .arg(symbols.join(joiner), series_period_label(period))
                        .arg(n)
                        .arg(last));
        meta.insert(QStringLiteral("symbols"), QJsonArray::fromStringList(symbols));
        meta.insert(QStringLiteral("period"), period);
        meta.insert(QStringLiteral("days"), n);
        meta.insert(QStringLiteral("last"), data->dates.last().toString(Qt::ISODate));
        then(meta);
    };

    auto& hub = datahub::DataHub::instance();
    QStringList to_request;
    for (const QString& sym : symbols) {
        const QString topic = MarketDataService::history_topic(sym, period, QStringLiteral("1d"));
        const QVariant cached = hub.peek(topic);
        if (cached.isValid() && !cached.value<QVector<HistoryPoint>>().isEmpty()) {
            job->bars.insert(sym, cached.value<QVector<HistoryPoint>>());
            job->waiting.removeAll(sym);
            continue;
        }
        hub.subscribe(owner, topic, [job, sym, finish](const QVariant& v) {
            if (job->done || !v.canConvert<QVector<HistoryPoint>>())
                return;
            const auto points = v.value<QVector<HistoryPoint>>();
            if (points.isEmpty())
                return;
            job->bars.insert(sym, points);
            job->waiting.removeAll(sym);
            if (job->waiting.isEmpty())
                finish(QString());
        });
        hub.subscribe_errors(owner, topic, [job, sym, topic, finish](const QString& err) {
            if (job->done || job->bars.contains(sym))
                return;
            // Past daily closes do not change: an older copy in the hub still serves.
            const QVariant stale = datahub::DataHub::instance().peek_raw(topic);
            if (stale.isValid() && !stale.value<QVector<HistoryPoint>>().isEmpty()) {
                job->bars.insert(sym, stale.value<QVector<HistoryPoint>>());
                job->waiting.removeAll(sym);
                if (job->waiting.isEmpty())
                    finish(QString());
                return;
            }
            finish(tr("No daily prices for %1: %2").arg(sym, err.isEmpty() ? tr("the data source did not answer") : err));
        });
        to_request.append(topic);
    }
    if (job->waiting.isEmpty()) {
        // Everything was in the hub: continue on the next turn (the pane is still inside begin()).
        QTimer::singleShot(0, owner, [finish]() { finish(QString()); });
        return;
    }
    hub.request(to_request);
    QTimer::singleShot(kSeriesFetchTimeoutMs, owner, [job, finish]() {
        finish(tr("No answer from the price source for %1 within %2 s — check the connection and press RETRY.")
                   .arg(job->waiting.join(QStringLiteral(", ")))
                   .arg(kSeriesFetchTimeoutMs / 1000));
    });
}

void QuantModulePanel::cancel_series_fetch(quint64 pane_id) {
    if (QPointer<QObject> owner = fetches_.take(pane_id))
        owner->deleteLater();
}

// ── What a run reads ─────────────────────────────────────────────────────────

QString QuantModulePanel::series_text(const QString& key) const {
    QLineEdit* e = text_inputs_.value(key);
    if (!e)
        return {};
    const QString view = e->property("quantSeriesView").toString();
    if (view.isEmpty() || !series_ticker_mode(view))
        return e->text();
    const auto it = series_data_.constFind(view);
    if (it == series_data_.cend() || !it.value())
        return e->text();
    return format_series(*it.value(), static_cast<SeriesKind>(e->property("quantSeriesKind").toInt()),
                         e->property("quantSeriesRole").toInt());
}

QString QuantModulePanel::format_series(const SeriesData& d, SeriesKind kind, int role) const {
    const int s = std::clamp(role, 0, static_cast<int>(d.close.size()) - 1);
    if (s < 0)
        return {};
    const QVector<double>& c = d.close[s];
    const int n = static_cast<int>(c.size());
    // Signal / forward returns / factors share one index range: t in [20, n-2].
    const int t0 = kSeriesMomentumDays;
    const int t1 = n - 1; // exclusive (t+1 must exist)
    switch (kind) {
        case SeriesKind::Closes:
            return series_join(c, 4);
        case SeriesKind::Open:
            return series_join(d.open[s], 4);
        case SeriesKind::High:
            return series_join(d.high[s], 4);
        case SeriesKind::Low:
            return series_join(d.low[s], 4);
        case SeriesKind::Volume:
            return series_join(d.volume[s], 0);
        case SeriesKind::Returns:
            return series_join(series_returns(c), 6);
        case SeriesKind::Signal: {
            QVector<double> v;
            for (int t = t0; t < t1; ++t)
                v.append(c[t] / c[t - kSeriesMomentumDays] - 1.0);
            return series_join(v, 6);
        }
        case SeriesKind::FwdReturns: {
            QVector<double> v;
            for (int t = t0; t < t1; ++t)
                v.append(c[t + 1] / c[t] - 1.0);
            return series_join(v, 6);
        }
        case SeriesKind::OhlcvJson: {
            QJsonObject o{{QStringLiteral("open"), series_json(d.open[s], 4)},
                          {QStringLiteral("high"), series_json(d.high[s], 4)},
                          {QStringLiteral("low"), series_json(d.low[s], 4)},
                          {QStringLiteral("close"), series_json(c, 4)},
                          {QStringLiteral("volume"), series_json(d.volume[s], 0)}};
            return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
        }
        case SeriesKind::FactorsJson: {
            // Classic single-name factors, each known at the close of day t (no look-ahead),
            // aligned with FwdReturns (the next day's return).
            const QVector<double> r = series_returns(c); // r[i] = return of day i+1
            const QVector<double>& vol = d.volume[s];
            QVector<double> mom, rev, vola, vratio, range;
            for (int t = t0; t < t1; ++t) {
                mom.append(c[t] / c[t - 20] - 1.0);
                rev.append(-(c[t] / c[t - 5] - 1.0));
                vola.append(series_std(r, t - 20, t) * std::sqrt(252.0));
                const double vm = series_mean(vol, t - 19, t + 1);
                vratio.append(vm > 0 ? vol[t] / vm : 1.0);
                range.append(c[t] > 0 ? (d.high[s][t] - d.low[s][t]) / c[t] : 0.0);
            }
            QJsonObject o{{QStringLiteral("momentum_20d"), series_json(mom, 6)},
                          {QStringLiteral("reversal_5d"), series_json(rev, 6)},
                          {QStringLiteral("volatility_20d"), series_json(vola, 6)},
                          {QStringLiteral("volume_ratio_20d"), series_json(vratio, 4)},
                          {QStringLiteral("range_pct"), series_json(range, 6)}};
            return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
        }
        case SeriesKind::ReturnsMatrixJson: {
            QVector<QVector<double>> rets;
            for (const auto& cl : d.close)
                rets.append(series_returns(cl));
            QJsonArray rows;
            const int m = rets.isEmpty() ? 0 : static_cast<int>(rets.first().size());
            for (int t = 0; t < m; ++t) {
                QJsonArray row;
                for (const auto& rr : rets)
                    row.append(std::round(rr[t] * 1e6) / 1e6);
                rows.append(row);
            }
            return QString::fromUtf8(QJsonDocument(rows).toJson(QJsonDocument::Compact));
        }
        case SeriesKind::CovarianceJson: {
            QVector<QVector<double>> rets;
            for (const auto& cl : d.close)
                rets.append(series_returns(cl));
            const int k = static_cast<int>(rets.size());
            const int m = k ? static_cast<int>(rets.first().size()) : 0;
            QVector<double> mean(k);
            for (int a = 0; a < k; ++a)
                mean[a] = series_mean(rets[a], 0, m);
            QJsonArray rows;
            for (int a = 0; a < k; ++a) {
                QJsonArray row;
                for (int b = 0; b < k; ++b) {
                    double cov = 0;
                    for (int t = 0; t < m; ++t)
                        cov += (rets[a][t] - mean[a]) * (rets[b][t] - mean[b]);
                    cov = m > 1 ? cov / (m - 1) * 252.0 : 0.0;
                    row.append(std::round(cov * 1e8) / 1e8);
                }
                rows.append(row);
            }
            return QString::fromUtf8(QJsonDocument(rows).toJson(QJsonDocument::Compact));
        }
        case SeriesKind::Assets:
            return d.symbols.join(QLatin1Char(','));
        case SeriesKind::ExpReturns: {
            QVector<double> mu;
            for (const auto& cl : d.close) {
                const QVector<double> r = series_returns(cl);
                mu.append(series_mean(r, 0, static_cast<int>(r.size())) * 252.0);
            }
            return series_join(mu, 6);
        }
        case SeriesKind::SeriesMatrixJson: {
            QJsonObject o;
            for (int a = 0; a < d.symbols.size(); ++a)
                o.insert(d.symbols[a], series_json(series_returns(d.close[a]), 6));
            return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
        }
        case SeriesKind::EqualWeights: {
            QStringList ones;
            for (int a = 0; a < d.symbols.size(); ++a)
                ones.append(QStringLiteral("1"));
            return ones.join(QLatin1Char(','));
        }
        case SeriesKind::ActualCloses:
            return n > 1 ? series_join(c.mid(1), 4) : QString();
        case SeriesKind::NaiveForecast:
            return n > 1 ? series_join(c.mid(0, n - 1), 4) : QString();
        case SeriesKind::HoldoutTrain:
            return n > kSeriesHoldoutDays ? series_join(c.mid(0, n - kSeriesHoldoutDays), 4) : QString();
        case SeriesKind::HoldoutActual:
            return n > kSeriesHoldoutDays ? series_join(c.mid(n - kSeriesHoldoutDays), 4) : QString();
        case SeriesKind::HoldoutNaive: {
            if (n <= kSeriesHoldoutDays)
                return {};
            return series_join(QVector<double>(kSeriesHoldoutDays, c[n - kSeriesHoldoutDays - 1]), 4);
        }
        case SeriesKind::CrossMomentum: {
            QVector<double> m;
            for (const auto& cl : d.close) {
                const int last = static_cast<int>(cl.size()) - 1;
                m.append(last >= kSeriesMomentumDays ? cl[last] / cl[last - kSeriesMomentumDays] - 1.0 : 0.0);
            }
            return series_join(m, 6);
        }
    }
    return {};
}

} // namespace fincept::screens
