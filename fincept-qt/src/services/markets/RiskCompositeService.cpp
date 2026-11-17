#include "services/markets/RiskCompositeService.h"

#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "services/markets/MarketDataService.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QMap>
#include <QTimeZone>

#include <algorithm>
#include <cmath>

namespace fincept::services {

namespace {

constexpr const char* kRcsCtx = "fincept::services::RiskCompositeService";
constexpr int kRcsWindow = 252;  ///< standardisation window (trading days)
constexpr int kRcsMinObs = 60;   ///< fewest observations a z-score needs
constexpr int kRcsReturnDays = 20;
constexpr int kRcsTrendDays = 50;
constexpr double kRcsNaN = std::numeric_limits<double>::quiet_NaN();

using RcsSeries = QVector<QPair<QDate, double>>;

QString rcs_tr(const char* s) {
    return QCoreApplication::translate(kRcsCtx, s);
}

/// z of the last element of `xs` against the last kRcsWindow elements, clipped to ±2.
double rcs_z_last(const QVector<double>& xs_in) {
    QVector<double> xs;
    for (double v : xs_in)
        if (std::isfinite(v))
            xs.append(v);
    if (xs.size() > kRcsWindow)
        xs = xs.mid(xs.size() - kRcsWindow);
    if (xs.size() < kRcsMinObs)
        return kRcsNaN;
    double mean = 0;
    for (double v : xs)
        mean += v;
    mean /= xs.size();
    double var = 0;
    for (double v : xs)
        var += (v - mean) * (v - mean);
    const double sd = std::sqrt(var / (xs.size() - 1));
    if (!(sd > 0))
        return kRcsNaN;
    return std::clamp((xs.last() - mean) / sd, -2.0, 2.0);
}

/// Ratio of two series on their common dates, ascending.
RcsSeries rcs_ratio(const RcsSeries& a, const RcsSeries& b) {
    QMap<QDate, double> bm;
    for (const auto& p : b)
        bm.insert(p.first, p.second);
    RcsSeries out;
    for (const auto& p : a) {
        const auto it = bm.constFind(p.first);
        if (it != bm.constEnd() && it.value() > 0 && p.second > 0)
            out.append({p.first, p.second / it.value()});
    }
    return out;
}

/// N-day log returns of a positive series.
QVector<double> rcs_log_returns(const RcsSeries& s, int n) {
    QVector<double> out;
    for (int i = n; i < s.size(); ++i)
        if (s[i].second > 0 && s[i - n].second > 0)
            out.append(std::log(s[i].second / s[i - n].second));
    return out;
}

RiskCompositeInput rcs_signal(const char* id, const QString& name, const QString& description, const QStringList& symbols) {
    RiskCompositeInput s;
    s.id = QString::fromLatin1(id);
    s.name = name;
    s.description = description;
    s.symbols = symbols;
    return s;
}

QString rcs_pct(double v) {
    return QString::number(v * 100.0, 'f', 1) + QLatin1Char('%');
}

} // namespace

RiskCompositeService& RiskCompositeService::instance() {
    static RiskCompositeService s;
    return s;
}

QStringList RiskCompositeService::input_symbols() {
    return {"^GSPC", "^VIX", "HYG", "IEF", "AUDJPY=X", "HG=F", "GC=F"};
}

QString RiskCompositeService::methodology() {
    return rcs_tr("Risk-on / risk-off composite from five global signals, each standardised against its own "
                  "last year (z-score, clipped to ±2):\n"
                  "• Equity trend — S&P 500 vs its 50-day average\n"
                  "• Volatility — VIX level, inverted\n"
                  "• Credit — 20-day return of high yield (HYG) over Treasuries (IEF)\n"
                  "• Risk FX — 20-day return of AUD/JPY\n"
                  "• Copper / gold — 20-day return of the copper-to-gold ratio\n"
                  "Score = 50 + 25 × average z (0 = deep risk-off, 100 = strong risk-on), from daily closes. "
                  "No score is shown unless all five inputs are available.");
}

void RiskCompositeService::attach(QObject* consumer) {
    if (!consumer || consumers_.contains(consumer))
        return;
    consumers_.insert(consumer);
    connect(consumer, &QObject::destroyed, this, [this, consumer]() { detach(consumer); });
    if (!subscribed_)
        subscribe_inputs();
}

void RiskCompositeService::detach(QObject* consumer) {
    if (!consumers_.remove(consumer))
        return;
    disconnect(consumer, &QObject::destroyed, this, nullptr);
    if (consumers_.isEmpty() && subscribed_) {
        datahub::DataHub::instance().unsubscribe(this);
        subscribed_ = false;
    }
}

void RiskCompositeService::refresh() {
    QStringList topics;
    for (const auto& s : input_symbols())
        topics << MarketDataService::performance_history_topic(s);
    datahub::DataHub::instance().request(topics, /*force=*/true);
}

void RiskCompositeService::subscribe_inputs() {
    auto& hub = datahub::DataHub::instance();
    for (const QString& sym : input_symbols()) {
        const QString t = MarketDataService::performance_history_topic(sym);
        hub.subscribe(this, t, [this, sym](const QVariant& v) {
            if (!v.canConvert<QVector<HistoryPoint>>())
                return;
            RcsSeries s;
            for (const auto& b : v.value<QVector<HistoryPoint>>())
                if (b.close > 0)
                    s.append({QDateTime::fromSecsSinceEpoch(b.timestamp, QTimeZone::UTC).date(), b.close});
            std::sort(s.begin(), s.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
            closes_.insert(sym, s);
            errors_.remove(sym);
            recompute();
        });
        hub.subscribe_errors(this, t, [this, sym](const QString& err) {
            errors_.insert(sym, err);
            recompute();
        });
    }
    subscribed_ = true;
}

void RiskCompositeService::recompute() {
    RiskComposite c;
    auto missing_reason = [this](const QStringList& syms) {
        QStringList why;
        for (const auto& s : syms) {
            if (errors_.contains(s))
                why << s + QStringLiteral(": ") + errors_.value(s);
            else if (closes_.value(s).size() < kRcsMinObs)
                why << rcs_tr("%1: not enough history yet").arg(s);
        }
        return why.join(QStringLiteral("; "));
    };
    auto add = [&](RiskCompositeInput sig) {
        if (!sig.available() && sig.error.isEmpty())
            sig.error = missing_reason(sig.symbols);
        if (!sig.available())
            c.missing << sig.name;
        c.inputs.append(sig);
    };
    QDate as_of;
    auto note_date = [&as_of](const RcsSeries& s) {
        if (!s.isEmpty() && (!as_of.isValid() || s.last().first > as_of))
            as_of = s.last().first;
    };

    // 1. Equity trend: S&P 500 vs its 50-day average.
    {
        RiskCompositeInput s = rcs_signal("equity_trend", rcs_tr("Equity trend"), rcs_tr("S&P 500 vs its 50-day average"), {"^GSPC"});
        const auto& px = closes_.value("^GSPC");
        QVector<double> dev;
        double sum = 0;
        for (int i = 0; i < px.size(); ++i) {
            sum += px[i].second;
            if (i >= kRcsTrendDays)
                sum -= px[i - kRcsTrendDays].second;
            if (i >= kRcsTrendDays - 1)
                dev.append(px[i].second / (sum / kRcsTrendDays) - 1.0);
        }
        s.z = rcs_z_last(dev);
        if (!dev.isEmpty()) {
            s.raw = dev.last();
            s.raw_text = rcs_tr("%1 vs 50-day avg").arg(rcs_pct(dev.last()));
        }
        note_date(px);
        add(s);
    }
    // 2. Volatility: VIX level, inverted.
    {
        RiskCompositeInput s = rcs_signal("volatility", rcs_tr("Volatility"), rcs_tr("VIX level, inverted (low = risk-on)"), {"^VIX"});
        const auto& px = closes_.value("^VIX");
        QVector<double> lv;
        for (const auto& p : px)
            lv.append(-p.second);
        s.z = rcs_z_last(lv);
        if (!px.isEmpty()) {
            s.raw = px.last().second;
            s.raw_text = rcs_tr("VIX %1").arg(QString::number(px.last().second, 'f', 1));
        }
        note_date(px);
        add(s);
    }
    // 3. Credit: HYG / IEF, 20-day return.
    {
        RiskCompositeInput s = rcs_signal("credit", rcs_tr("Credit"), rcs_tr("High yield (HYG) vs Treasuries (IEF), 20-day"),
                     {"HYG", "IEF"});
        const auto r = rcs_log_returns(rcs_ratio(closes_.value("HYG"), closes_.value("IEF")), kRcsReturnDays);
        s.z = rcs_z_last(r);
        if (!r.isEmpty()) {
            s.raw = r.last();
            s.raw_text = rcs_tr("%1 relative, 20d").arg(rcs_pct(std::expm1(r.last())));
        }
        note_date(closes_.value("HYG"));
        add(s);
    }
    // 4. Risk FX: AUD/JPY 20-day return.
    {
        RiskCompositeInput s = rcs_signal("risk_fx", rcs_tr("Risk FX"), rcs_tr("AUD/JPY, 20-day"), {"AUDJPY=X"});
        const auto r = rcs_log_returns(closes_.value("AUDJPY=X"), kRcsReturnDays);
        s.z = rcs_z_last(r);
        if (!r.isEmpty()) {
            s.raw = r.last();
            s.raw_text = rcs_tr("%1, 20d").arg(rcs_pct(std::expm1(r.last())));
        }
        add(s);
    }
    // 5. Copper / gold ratio, 20-day return.
    {
        RiskCompositeInput s = rcs_signal("copper_gold", rcs_tr("Copper / gold"), rcs_tr("Copper-to-gold ratio, 20-day"), {"HG=F", "GC=F"});
        const auto r = rcs_log_returns(rcs_ratio(closes_.value("HG=F"), closes_.value("GC=F")), kRcsReturnDays);
        s.z = rcs_z_last(r);
        if (!r.isEmpty()) {
            s.raw = r.last();
            s.raw_text = rcs_tr("%1 relative, 20d").arg(rcs_pct(std::expm1(r.last())));
        }
        add(s);
    }

    c.as_of = as_of;
    c.complete = c.missing.isEmpty();
    if (c.complete) {
        double sum = 0;
        for (const auto& s : c.inputs)
            sum += s.z;
        c.score = std::clamp(50.0 + 25.0 * sum / c.inputs.size(), 0.0, 100.0);
        c.regime = c.score >= 65   ? rcs_tr("Risk-on")
                   : c.score >= 55 ? rcs_tr("Leaning risk-on")
                   : c.score > 45  ? rcs_tr("Neutral")
                   : c.score > 35  ? rcs_tr("Leaning risk-off")
                                   : rcs_tr("Risk-off");
    }
    current_ = c;
    emit updated();
}

} // namespace fincept::services
