#include "services/workflow/nodes/TriggerNodes.h"

#include "python/PythonRunner.h"
#include "services/workflow/NodeRegistry.h"
#include "storage/repositories/NewsArticleRepository.h"

#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QTimeZone>
#include <QVector>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>

namespace fincept::workflow {

// ── Market-hours cron helpers ─────────────────────────────────────────────────
// Named namespace (not anonymous) so the symbols stay unique inside a unity-build batch.
namespace trigger_nodes_detail {

/// One parsed cron field: the set of allowed values over [min, max].
struct TrigCronField {
    int min = 0;
    QVector<bool> allowed; // index = value - min
    bool wildcard = false; // field was "*" (or "*/1") — matters for the day-of-month/day-of-week OR rule

    bool matches(int v) const {
        const int i = v - min;
        return i >= 0 && i < allowed.size() && allowed[i];
    }
};

/// Parses one field: "*", "*/n", "a", "a-b", "a-b/n", "a/n" (= a..max step n) and comma lists of those.
/// `names` (optional) maps three-letter names to values starting at `names_base` (JAN=1, SUN=0).
bool trig_parse_cron_field(const QString& text, int min, int max, const QStringList& names, int names_base,
                           TrigCronField& out, QString& err) {
    out = TrigCronField{};
    out.min = min;
    out.allowed = QVector<bool>(max - min + 1, false);
    const QString field = text.trimmed().toUpper();
    if (field.isEmpty()) {
        err = QStringLiteral("empty field");
        return false;
    }
    auto value_of = [&](const QString& tok, int& v) {
        bool ok = false;
        v = tok.toInt(&ok);
        if (ok)
            return true;
        const int idx = names.indexOf(tok);
        if (idx >= 0) {
            v = idx + names_base;
            return true;
        }
        return false;
    };
    for (const QString& part : field.split(QLatin1Char(','))) {
        QString range = part;
        int step = 1;
        const int slash = part.indexOf(QLatin1Char('/'));
        if (slash >= 0) {
            bool ok = false;
            step = part.mid(slash + 1).toInt(&ok);
            if (!ok || step < 1) {
                err = QStringLiteral("bad step in '%1'").arg(part);
                return false;
            }
            range = part.left(slash);
        }
        int lo = min;
        int hi = max;
        if (range == QLatin1String("*")) {
            if (step == 1 && field == QLatin1String("*"))
                out.wildcard = true;
        } else {
            const int dash = range.indexOf(QLatin1Char('-'));
            if (dash > 0) {
                if (!value_of(range.left(dash), lo) || !value_of(range.mid(dash + 1), hi)) {
                    err = QStringLiteral("bad range '%1'").arg(range);
                    return false;
                }
            } else {
                if (!value_of(range, lo)) {
                    err = QStringLiteral("bad value '%1'").arg(range);
                    return false;
                }
                hi = (slash >= 0) ? max : lo; // "a/n" = from a to the end in steps of n
            }
        }
        if (lo < min || hi > max || lo > hi) {
            err = QStringLiteral("'%1' is outside %2-%3").arg(part).arg(min).arg(max);
            return false;
        }
        for (int v = lo; v <= hi; v += step)
            out.allowed[v - min] = true;
    }
    return true;
}

/// Standard five-field cron expression (minute hour day-of-month month day-of-week).
struct TrigCronExpr {
    TrigCronField minute, hour, dom, month, dow;

    bool matches(const QDateTime& local) const {
        const QDate d = local.date();
        const QTime t = local.time();
        if (!minute.matches(t.minute()) || !hour.matches(t.hour()) || !month.matches(d.month()))
            return false;
        const bool dom_ok = dom.matches(d.day());
        const bool dow_ok = dow.matches(d.dayOfWeek() % 7); // Qt: Mon=1..Sun=7; cron: Sun=0..Sat=6
        // Vixie-cron rule: when both day fields are restricted, either one may match.
        if (!dom.wildcard && !dow.wildcard)
            return dom_ok || dow_ok;
        return dom_ok && dow_ok;
    }
};

bool trig_parse_cron(QString expr, TrigCronExpr& out, QString& err) {
    expr = expr.trimmed();
    static const QHash<QString, QString> kMacros = {
        {QStringLiteral("@hourly"), QStringLiteral("0 * * * *")},
        {QStringLiteral("@daily"), QStringLiteral("0 0 * * *")},
        {QStringLiteral("@midnight"), QStringLiteral("0 0 * * *")},
        {QStringLiteral("@weekly"), QStringLiteral("0 0 * * 0")},
        {QStringLiteral("@monthly"), QStringLiteral("0 0 1 * *")},
        {QStringLiteral("@yearly"), QStringLiteral("0 0 1 1 *")},
        {QStringLiteral("@annually"), QStringLiteral("0 0 1 1 *")},
    };
    if (expr.startsWith(QLatin1Char('@')))
        expr = kMacros.value(expr.toLower(), expr);
    const QStringList f = expr.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (f.size() != 5) {
        err = QStringLiteral("expected 5 fields (minute hour day-of-month month day-of-week), got %1").arg(f.size());
        return false;
    }
    static const QStringList kMonths = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                        "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    static const QStringList kDays = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    QString field_err;
    if (!trig_parse_cron_field(f[0], 0, 59, {}, 0, out.minute, field_err)) {
        err = QStringLiteral("minute: ") + field_err;
        return false;
    }
    if (!trig_parse_cron_field(f[1], 0, 23, {}, 0, out.hour, field_err)) {
        err = QStringLiteral("hour: ") + field_err;
        return false;
    }
    if (!trig_parse_cron_field(f[2], 1, 31, {}, 0, out.dom, field_err)) {
        err = QStringLiteral("day-of-month: ") + field_err;
        return false;
    }
    if (!trig_parse_cron_field(f[3], 1, 12, kMonths, 1, out.month, field_err)) {
        err = QStringLiteral("month: ") + field_err;
        return false;
    }
    // 0-7, both 0 and 7 meaning Sunday.
    if (!trig_parse_cron_field(f[4], 0, 7, kDays, 0, out.dow, field_err)) {
        err = QStringLiteral("day-of-week: ") + field_err;
        return false;
    }
    if (out.dow.allowed.size() == 8 && out.dow.allowed[7])
        out.dow.allowed[0] = true;
    return true;
}

/// Trading session of an exchange in its own local time. Exchange holidays are NOT modelled.
struct TrigMarketSession {
    QByteArray tz_id;      // IANA id
    int fallback_utc_offset_min = 0; // standard-time offset, used only when the tz database is unavailable
    int open_min = 0;      // regular session, minutes after local midnight
    int close_min = 0;
    int pre_open_min = 0;  // start of the pre-market / pre-open window (ends at open_min)
    int lunch_start_min = -1; // optional midday break (TSE)
    int lunch_end_min = -1;
};

TrigMarketSession trig_session_for(const QString& exchange) {
    if (exchange == QLatin1String("LSE"))
        return {"Europe/London", 0, 8 * 60, 16 * 60 + 30, 7 * 60 + 50, -1, -1};
    if (exchange == QLatin1String("TSE"))
        return {"Asia/Tokyo", 9 * 60, 9 * 60, 15 * 60 + 30, 8 * 60, 11 * 60 + 30, 12 * 60 + 30};
    if (exchange == QLatin1String("NSE"))
        return {"Asia/Kolkata", 5 * 60 + 30, 9 * 60 + 15, 15 * 60 + 30, 9 * 60, -1, -1};
    // NYSE / NASDAQ
    return {"America/New_York", -5 * 60, 9 * 60 + 30, 16 * 60, 4 * 60, -1, -1};
}

/// "regular", "pre_market" or "closed" for `local` (a time already in the exchange's zone).
QString trig_session_state(const TrigMarketSession& s, const QDateTime& local) {
    const int dow = local.date().dayOfWeek();
    if (dow == 6 || dow == 7)
        return QStringLiteral("closed");
    const int m = local.time().hour() * 60 + local.time().minute();
    const bool in_lunch = s.lunch_start_min >= 0 && m >= s.lunch_start_min && m < s.lunch_end_min;
    if (m >= s.open_min && m < s.close_min && !in_lunch)
        return QStringLiteral("regular");
    if (m >= s.pre_open_min && m < s.open_min)
        return QStringLiteral("pre_market");
    return QStringLiteral("closed");
}

} // namespace trigger_nodes_detail

void register_trigger_nodes(NodeRegistry& registry) {
    // ManualTrigger — already registered as builtin in NodeRegistry constructor.
    // ScheduleTrigger — already registered as builtin.

    // ── Price Alert Trigger ────────────────────────────────────────
    registry.register_type({
        .type_id = "trigger.price_alert",
        .display_name = "Price Alert",
        .category = "Triggers",
        .description = "Trigger when price crosses a threshold",
        .icon_text = ">>",
        .accent_color = "#d97706",
        .version = 1,
        .inputs = {},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"symbol", "Symbol", "string", "AAPL", {}, "Ticker symbol", true},
                {"condition", "Condition", "select", "above", {"above", "below", "crosses"}, ""},
                {"price", "Price", "number", 0.0, {}, "Threshold price", true},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                const QString symbol = params.value("symbol").toString().trimmed();
                const QString condition = params.value("condition").toString("above");
                const double threshold = params.value("price").toDouble();
                if (symbol.isEmpty()) {
                    cb(false, {}, "Price Alert needs a symbol");
                    return;
                }

                // This trigger used to report "triggered" unconditionally, so a workflow like
                // "buy AAPL when it is above $200" fired its order at ANY price. A run is a
                // manual "check now" (there is no scheduler), so look at the live quote and only
                // let the workflow continue when the condition actually holds.
                fincept::python::PythonRunner::instance().run(
                    "yfinance_data.py", {"quote", symbol},
                    [cb, symbol, condition, threshold](const fincept::python::PythonResult& res) {
                        if (!res.success) {
                            cb(false, {}, res.error.isEmpty() ? QStringLiteral("Quote request failed") : res.error);
                            return;
                        }
                        const auto doc = QJsonDocument::fromJson(fincept::python::extract_json(res.output).trimmed().toUtf8());
                        const QJsonObject quote = doc.object();
                        const double price = quote.value("price").toDouble(0);
                        if (!doc.isObject() || price <= 0) {
                            cb(false, {}, QString("No price available for %1").arg(symbol));
                            return;
                        }

                        bool met = false;
                        if (condition == "below") {
                            met = price < threshold;
                        } else if (condition == "crosses") {
                            // Crossed since the previous close, in either direction.
                            const double prev = quote.value("previous_close").toDouble(0);
                            met = prev > 0 && ((prev < threshold && price >= threshold) ||
                                               (prev > threshold && price <= threshold));
                        } else {
                            met = price > threshold;
                        }

                        QJsonObject out;
                        out["symbol"] = symbol;
                        out["condition"] = condition;
                        out["price"] = threshold; // the configured threshold (unchanged output key)
                        out["current_price"] = price;
                        out["triggered"] = met;
                        if (!met) {
                            // Not met: nothing downstream should run. "_skipped" is the executor's
                            // marker for a node that produced no data for its consumers.
                            out["_skipped"] = true;
                            out["reason"] = QString("%1 is %2 (alert: %3 %4)")
                                                .arg(symbol)
                                                .arg(price)
                                                .arg(condition)
                                                .arg(threshold);
                        }
                        cb(true, out, {});
                    });
            },
    });

    // ── News Event Trigger ─────────────────────────────────────────
    registry.register_type({
        .type_id = "trigger.news_event",
        .display_name = "News Event",
        .category = "Triggers",
        .description = "Trigger on news keyword match",
        .icon_text = ">>",
        .accent_color = "#d97706",
        .version = 1,
        .inputs = {},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"keywords", "Keywords", "string", "", {}, "Comma-separated keywords", true},
                {"lookback_hours", "Lookback (hours)", "number", 24, {}, "Search headlines from the last N hours"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                // This used to answer triggered=true on every run without looking at any news. A run
                // is a "check now": search the headlines the News service has stored for the
                // keywords and fire only when at least one matches.
                QStringList keywords;
                for (const QString& k : params.value("keywords").toString().split(QLatin1Char(','), Qt::SkipEmptyParts)) {
                    const QString t = k.trimmed().toLower();
                    if (!t.isEmpty())
                        keywords << t;
                }
                if (keywords.isEmpty()) {
                    cb(false, {}, "News Event needs at least one keyword");
                    return;
                }
                const int hours = std::clamp(static_cast<int>(params.value("lookback_hours").toDouble(24)), 1, 24 * 30);
                const qint64 cutoff = QDateTime::currentSecsSinceEpoch() - qint64(hours) * 3600;
                // SQLite read off the UI thread (the executor's callback is thread-safe).
                (void)QtConcurrent::run([cb, keywords, hours, cutoff]() {
                    const auto r = NewsArticleRepository::instance().load_recent(cutoff, {}, 2000);
                    if (r.is_err()) {
                        cb(false, {}, "Could not read stored news: " + QString::fromStdString(r.error()));
                        return;
                    }
                    const auto& articles = r.value();
                    QJsonArray matches;
                    int total = 0;
                    for (const auto& a : articles) {
                        const QString hay = (a.headline + QLatin1Char(' ') + a.summary).toLower();
                        QJsonArray hit;
                        for (const QString& k : keywords) {
                            bool found = hay.contains(k);
                            for (int i = 0; !found && i < a.tickers.size(); ++i)
                                found = a.tickers[i].compare(k, Qt::CaseInsensitive) == 0;
                            if (found)
                                hit.append(k);
                        }
                        if (hit.isEmpty())
                            continue;
                        ++total;
                        if (matches.size() < 20)
                            matches.append(QJsonObject{
                                {"headline", a.headline},
                                {"source", a.source},
                                {"published", QDateTime::fromSecsSinceEpoch(a.sort_ts, QTimeZone::UTC).toString(Qt::ISODate)},
                                {"link", a.link},
                                {"keywords", hit},
                            });
                    }
                    QJsonObject out;
                    out["keywords"] = QJsonArray::fromStringList(keywords);
                    out["lookback_hours"] = hours;
                    out["articles_scanned"] = static_cast<int>(articles.size());
                    out["matches_total"] = total;
                    out["matches"] = matches;
                    out["triggered"] = total > 0;
                    if (total == 0) {
                        out["_skipped"] = true;
                        out["reason"] = articles.isEmpty()
                                            ? QString("No headlines stored from the last %1 h - the News screen "
                                                      "collects them while it runs")
                                                  .arg(hours)
                                            : QString("None of %1 headlines from the last %2 h mention %3")
                                                  .arg(articles.size())
                                                  .arg(hours)
                                                  .arg(keywords.join(QStringLiteral(", ")));
                    }
                    cb(true, out, {});
                });
            },
    });

    // ── Webhook Trigger ────────────────────────────────────────────
    registry.register_type({
        .type_id = "trigger.webhook",
        .display_name = "Webhook",
        .category = "Triggers",
        .description = "Trigger from an external webhook call (needs the workflow scheduler; never fires yet)",
        .icon_text = ">>",
        .accent_color = "#d97706",
        .version = 1,
        .inputs = {},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"path", "Path", "string", "/webhook", {}, "/my-hook"},
                {"method", "Method", "select", "POST", {"GET", "POST", "PUT"}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                // This used to answer triggered=true on every run, so anything wired behind it ran
                // as if a webhook had arrived. Nothing listens for inbound HTTP calls: webhook
                // triggers need the workflow scheduler (a resident listener that starts runs), which
                // does not exist. Report that instead of pretending, and let downstream nodes skip.
                QJsonObject out;
                out["path"] = params.value("path").toString();
                out["method"] = params.value("method").toString();
                out["triggered"] = false;
                out["_skipped"] = true;
                out["reason"] = QStringLiteral(
                    "Webhook triggers need the workflow scheduler, which is not available: no inbound "
                    "webhook listener is running, so this trigger cannot fire. Use a Manual, Price Alert or "
                    "Market Hours Cron trigger instead.");
                cb(true, out, {});
            },
    });

    // ── Tier 1 additions ───────────────────────────────────────────

    registry.register_type({
        .type_id = "trigger.cron_market",
        .display_name = "Market Hours Cron",
        .category = "Triggers",
        .description = "Fires when the cron expression is due now (exchange local time) and the market is open",
        .icon_text = ">>",
        .accent_color = "#d97706",
        .version = 1,
        .inputs = {},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"cron", "Cron Expression", "string", "*/15 * * * *", {}, "minute hour day month weekday"},
                {"exchange", "Exchange", "select", "NYSE", {"NYSE", "NASDAQ", "LSE", "TSE", "NSE"}, ""},
                {"include_premarket", "Include Pre-Market", "boolean", false, {}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                using namespace trigger_nodes_detail;
                // This used to report triggered=true on every run, whatever the time or the cron
                // expression. There is no scheduler, so a run is a "check now": the trigger fires only
                // when the expression is due in the current minute (evaluated in the exchange's local
                // time) AND the exchange is in session. Exchange holidays are not modelled.
                const QString cron_text = params.value("cron").toString(QStringLiteral("*/15 * * * *")).trimmed();
                const QString exchange = params.value("exchange").toString(QStringLiteral("NYSE"));
                const bool include_pre = params.value("include_premarket").toBool(false);

                TrigCronExpr cron;
                QString parse_err;
                if (!trig_parse_cron(cron_text, cron, parse_err)) {
                    cb(false, {}, QString("Invalid cron expression '%1': %2").arg(cron_text, parse_err));
                    return;
                }

                const TrigMarketSession session = trig_session_for(exchange);
                QTimeZone tz(session.tz_id);
                bool tz_fallback = false;
                if (!tz.isValid()) {
                    // No tz database (seen on some Windows installs): use the standard-time offset.
                    tz = QTimeZone(session.fallback_utc_offset_min * 60);
                    tz_fallback = true;
                }
                const QDateTime now_local = QDateTime::currentDateTimeUtc().toTimeZone(tz);

                const bool due = cron.matches(now_local);
                const QString state = trig_session_state(session, now_local);
                const bool in_session =
                    state == QLatin1String("regular") || (include_pre && state == QLatin1String("pre_market"));
                const bool fire = due && in_session;

                // Next minute (within 8 days) at which the expression is due during an allowed session.
                QString next_fire;
                QDateTime probe = now_local.addSecs(60 - now_local.time().second());
                for (int i = 0; i < 8 * 24 * 60; ++i, probe = probe.addSecs(60)) {
                    const QString st = trig_session_state(session, probe);
                    const bool open = st == QLatin1String("regular") ||
                                      (include_pre && st == QLatin1String("pre_market"));
                    if (open && cron.matches(probe)) {
                        next_fire = probe.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
                        break;
                    }
                }

                QJsonObject out;
                out["cron"] = cron_text;
                out["exchange"] = exchange;
                out["timezone"] = QString::fromLatin1(session.tz_id);
                out["local_time"] = now_local.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
                out["session"] = state;
                out["due"] = due;
                out["triggered"] = fire;
                out["next_fire_local"] = next_fire.isEmpty() ? QJsonValue() : QJsonValue(next_fire);
                out["holidays_checked"] = false;
                if (tz_fallback)
                    out["timezone_note"] = QStringLiteral("Time-zone database unavailable - standard-time offset "
                                                          "used, daylight saving not applied");
                if (!fire) {
                    out["_skipped"] = true;
                    if (!due)
                        out["reason"] = QString("Cron '%1' is not due at %2 %3 (next: %4)")
                                            .arg(cron_text, out["local_time"].toString(), exchange,
                                                 next_fire.isEmpty() ? QStringLiteral("none within 8 days")
                                                                     : next_fire);
                    else
                        out["reason"] = QString("%1 is %2 at %3 local time")
                                            .arg(exchange, state == QLatin1String("pre_market")
                                                               ? QStringLiteral("in pre-market (not included)")
                                                               : QStringLiteral("closed"),
                                                 out["local_time"].toString());
                }
                cb(true, out, {});
            },
    });

    // (A "Portfolio Drift" trigger — trigger.portfolio_drift — used to be registered here. It had no
    // input and the app stores no target allocation to drift from, so it could not evaluate anything
    // and answered triggered=true on every run, firing whatever was wired behind it. It was removed
    // from the palette; a saved workflow that still holds one fails that node as an unknown type.)
}

} // namespace fincept::workflow
