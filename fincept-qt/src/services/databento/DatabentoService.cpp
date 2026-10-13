#include "services/databento/DatabentoService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "storage/secure/SecureStorage.h"

#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <utility>

namespace fincept {

using namespace fincept::python;

// Extract the last JSON object/array from Python script stdout
static QString extract_json_from_output(const QString& output) {
    // Find last '{' or '[' to skip any print/log lines before JSON
    int start = -1;
    for (int i = output.size() - 1; i >= 0; --i) {
        if (output[i] == '}' || output[i] == ']') {
            // scan backwards for matching open
            QChar close = output[i];
            QChar open = (close == '}') ? '{' : '[';
            int depth = 0;
            for (int j = i; j >= 0; --j) {
                if (output[j] == close)
                    ++depth;
                else if (output[j] == open) {
                    --depth;
                    if (depth == 0) {
                        start = j;
                        break;
                    }
                }
            }
            if (start >= 0)
                return output.mid(start, i - start + 1);
            break;
        }
    }
    return output.trimmed();
}

// ── Rates / FX curve maths (pure helpers) ──────────────────────────────────
// Named namespace so the symbols stay unique inside a unity-build batch.
namespace databento_curve_detail {

/// Yield (percent, bond-equivalent) of a bond paying a 6% semi-annual coupon that matures in `years`
/// and is priced at `price` per 100 — the CBOT notional bond behind every Treasury future. Pricing a
/// future's settlement this way gives the usual "futures-implied yield" approximation: it ignores
/// the cheapest-to-deliver's own coupon and the basis, so it can differ from the cash yield of that
/// maturity by roughly 5-20 bp. NaN when no yield in [-5%, 40%] reproduces the price.
double db_notional_bond_yield_pct(double price, double years) {
    const int periods = std::max(1, static_cast<int>(std::lround(years * 2.0)));
    auto pv = [periods](double y) {
        const double r = y / 2.0;
        if (std::abs(r) < 1e-12)
            return 3.0 * periods + 100.0;
        const double v = std::pow(1.0 + r, -periods);
        return 3.0 * (1.0 - v) / r + 100.0 * v;
    };
    double lo = -0.05;
    double hi = 0.40;
    if (!(price > 0.0) || price > pv(lo) || price < pv(hi))
        return std::numeric_limits<double>::quiet_NaN();
    for (int i = 0; i < 100; ++i) { // pv() falls as the yield rises
        const double mid = 0.5 * (lo + hi);
        if (pv(mid) > price)
            lo = mid;
        else
            hi = mid;
    }
    return 50.0 * (lo + hi); // midpoint, as a percentage
}

/// Rate implied by a "100 minus rate" futures price (fed funds ZQ, 1M SOFR SR1). NaN outside a
/// sane band, so a bad print cannot become a 40% rate.
double db_implied_rate_pct(double price) {
    if (!(price > 80.0) || price > 100.5)
        return std::numeric_limits<double>::quiet_NaN();
    return 100.0 - price;
}

/// Latest settlement of `symbol` (and its date), NaN when the symbol has none.
double db_latest_settlement(const QHash<QString, QMap<QString, double>>& s, const QString& symbol,
                            QString* date = nullptr) {
    const auto it = s.constFind(symbol);
    if (it == s.constEnd() || it->isEmpty())
        return std::numeric_limits<double>::quiet_NaN();
    if (date)
        *date = it->lastKey();
    return it->last();
}

/// Implied monthly rates of `root`.c.0, .c.1, ... — contiguous from the front month, stopping at the
/// first contract without a usable settlement.
std::vector<double> db_monthly_rate_strip(const QHash<QString, QMap<QString, double>>& s, const QString& root,
                                          int max_months, QString* front_date) {
    std::vector<double> out;
    for (int i = 0; i < max_months; ++i) {
        QString d;
        const double r = db_implied_rate_pct(db_latest_settlement(s, QString("%1.c.%2").arg(root).arg(i), &d));
        if (std::isnan(r))
            break;
        if (i == 0 && front_date)
            *front_date = d;
        out.push_back(r);
    }
    return out;
}

} // namespace databento_curve_detail

DatabentoService& DatabentoService::instance() {
    static DatabentoService inst;
    return inst;
}

DatabentoService::DatabentoService() {}

// ── API key ────────────────────────────────────────────────────────────────
// We accept both the legacy SECURE_KEY ("databento.api_key") and the canonical
// env-style key ("DATABENTO_API_KEY") used by Settings + PythonRunner env
// injection. Reads check both; writes mirror to both so the two paths stay
// in sync regardless of which screen entered the key.
static constexpr const char* DATABENTO_ENV_KEY = "DATABENTO_API_KEY";

bool DatabentoService::has_api_key() const {
    auto r = SecureStorage::instance().retrieve(SECURE_KEY);
    if (r.is_ok() && !r.value().isEmpty())
        return true;
    auto r2 = SecureStorage::instance().retrieve(DATABENTO_ENV_KEY);
    return r2.is_ok() && !r2.value().isEmpty();
}

void DatabentoService::set_api_key(const QString& key) {
    QString trimmed = key.trimmed();
    SecureStorage::instance().store(SECURE_KEY, trimmed);
    SecureStorage::instance().store(DATABENTO_ENV_KEY, trimmed);
    LOG_INFO("Databento", "API key stored");
}

QString DatabentoService::api_key() const {
    // The canonical slot first: it is the one Settings > Credentials writes, and the one
    // PythonRunner injects into the script's environment. The legacy slot is only
    // written by set_api_key() (which mirrors to both), so after the user rotates the
    // key in Settings it is the stale one — preferring it kept sending the old key.
    auto r2 = SecureStorage::instance().retrieve(DATABENTO_ENV_KEY);
    if (r2.is_ok() && !r2.value().isEmpty())
        return r2.value();
    auto r = SecureStorage::instance().retrieve(SECURE_KEY);
    return r.is_ok() ? r.value() : QString{};
}

void DatabentoService::clear_api_key() {
    SecureStorage::instance().remove(SECURE_KEY);
    SecureStorage::instance().remove(DATABENTO_ENV_KEY);
}

// ── Core runner — wraps PythonRunner per P4 ────────────────────────────────
// Python script expects:  databento_provider.py <command> <json_args>
// where json_args is a JSON object with api_key and other parameters.
void DatabentoService::run_script(const QStringList& args, std::function<void(bool, const QJsonObject&)> cb) {
    QString key = api_key();
    if (key.isEmpty()) {
        cb(false, {{"error", "No Databento API key configured"}});
        return;
    }

    // args[0] = command name, args[1..] = key=value pairs to pack into JSON
    if (args.isEmpty()) {
        cb(false, {{"error", "No command specified"}});
        return;
    }

    QString command = args[0];

    // SECURITY: the key reaches the script through the DATABENTO_API_KEY environment
    // variable that PythonRunner injects from SecureStorage on every spawn — NOT as an
    // "api_key" entry in argv, where any local process can read it from the process
    // list (and where it sat for the lifetime of every request). A key that exists only
    // in the legacy slot is copied to the canonical one first so the injection finds it.
    {
        const auto canonical = SecureStorage::instance().retrieve(DATABENTO_ENV_KEY);
        if (!canonical.is_ok() || canonical.value().isEmpty())
            SecureStorage::instance().store(DATABENTO_ENV_KEY, key);
    }

    QJsonObject json_args;

    // Pack remaining args as key=value pairs into the JSON object
    for (int i = 1; i < args.size(); i += 2) {
        if (i + 1 < args.size()) {
            json_args[args[i]] = args[i + 1];
        }
    }

    QStringList script_args = {command, QString::fromUtf8(QJsonDocument(json_args).toJson(QJsonDocument::Compact))};

    QPointer<DatabentoService> self = this;

    QString cmd_label = command;
    PythonRunner::instance().run(SCRIPT, script_args, [self, cb, cmd_label](PythonResult result) {
        if (!self)
            return;
        // Surface the full stdout (includes warnings + JSON) so the inspector's
        // "View raw response" modal has something useful to show.
        emit self->raw_response(cmd_label, result.output);
        if (!result.success) {
            // The script reports its own failures (no API key, `databento` package
            // missing, bad key) as {"error": true, "message": "..."} on STDOUT and then
            // exits 1, so result.error (stderr) is usually empty or a bare traceback.
            // Prefer the script's message; fall back to stderr, then to the exit code.
            QString reason = result.error.trimmed();
            const QJsonDocument fail_doc = QJsonDocument::fromJson(result.output.toUtf8());
            if (fail_doc.isObject()) {
                const QString message = fail_doc.object().value("message").toString().trimmed();
                if (!message.isEmpty())
                    reason = message;
            }
            if (reason.isEmpty())
                reason = QStringLiteral("Databento script failed (exit code %1)").arg(result.exit_code);
            LOG_ERROR("Databento", "Script error: " + reason.left(300));
            cb(false, {{"error", reason}});
            return;
        }
        // Whole stdout first: a clean JSON document needs no scanning, and the brace
        // matcher in extract_json_from_output() miscounts a '{' or '}' inside a string
        // value (Python exception text routinely contains them), turning a readable
        // error message into "JSON parse error". Only fall back to it when the output
        // carries warning/log lines around the document.
        QJsonParseError err;
        auto doc = QJsonDocument::fromJson(result.output.toUtf8(), &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            const QString json_str = extract_json_from_output(result.output);
            doc = QJsonDocument::fromJson(json_str.toUtf8(), &err);
        }
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            LOG_ERROR("Databento", "JSON parse error: " + err.errorString());
            cb(false, {{"error", "JSON parse error: " + err.errorString()}});
            return;
        }
        cb(true, doc.object());
    });
}

// ── Test connection ─────────────────────────────────────────────────────────
void DatabentoService::test_connection() {
    emit fetch_started("Testing Databento connection...");
    QPointer<DatabentoService> self = this;
    run_script({"test_connection"}, [self](bool ok, const QJsonObject& j) {
        if (!self)
            return;
        if (!ok) {
            emit self->connection_tested(false, j["error"].toString());
            return;
        }
        bool err = j["error"].toBool(false);
        QString msg = j["message"].toString("Connected");
        emit self->connection_tested(!err, msg);
    });
}

// ── OHLCV fetch (correlation, PCA, drawdown, beta surfaces) ─────────────────
void DatabentoService::fetch_ohlcv(const QStringList& symbols, int days) {
    emit fetch_started("Fetching OHLCV data for " + QString::number(symbols.size()) + " symbols...");
    LOG_INFO("Databento", "fetch_ohlcv symbols=" + symbols.join(",") + " days=" + QString::number(days));

    QPointer<DatabentoService> self = this;
    QStringList args = {"historical_ohlcv", "symbols", symbols.join(","), "days", QString::number(days)};

    run_script(args, [self, symbols](bool ok, const QJsonObject& j) {
        if (!self)
            return;
        DatabentoOhlcvResult res;
        if (!ok || j["error"].toBool(false)) {
            res.success = false;
            res.error = ok ? j["message"].toString("Unknown error") : j["error"].toString();
            LOG_WARN("Databento", "OHLCV fetch failed: " + res.error);
            emit self->fetch_failed(res.error);
            emit self->ohlcv_ready(res);
            return;
        }
        res.success = true;
        QJsonObject data = j["data"].toObject();
        for (const QString& sym : data.keys()) {
            QVector<QJsonObject> records;
            for (const auto& v : data[sym].toArray())
                records.push_back(v.toObject());
            res.data[sym] = records;
        }
        self->cached_ohlcv_ = res;
        self->last_fetch_time_ = QDateTime::currentDateTime();
        LOG_INFO("Databento", "OHLCV ready, symbols=" + QString::number(res.data.size()));
        emit self->ohlcv_ready(res);
    });
}

// ── Options surface fetch ─────────────────────────────────────────────────
void DatabentoService::fetch_options_surface(const QString& symbol, float spot) {
    emit fetch_started("Fetching options chain for " + symbol + "...");
    LOG_INFO("Databento", "fetch_options_surface symbol=" + symbol);

    QPointer<DatabentoService> self = this;
    QStringList args = {"options_chain", "symbol", symbol, "spot", QString::number((double)spot, 'f', 2)};

    run_script(args, [self, symbol, spot](bool ok, const QJsonObject& j) {
        if (!self)
            return;
        DatabentoVolSurfaceResult res;
        if (!ok || j["error"].toBool(false)) {
            res.success = false;
            res.error = ok ? j["message"].toString("Unknown error") : j["error"].toString();
            LOG_WARN("Databento", "Options fetch failed: " + res.error);
            emit self->fetch_failed(res.error);
            emit self->vol_surface_ready(res);
            return;
        }
        res.success = true;
        res.vol = parse_vol_surface(j, symbol.toStdString(), spot);
        res.delta = parse_greek_surface(j, "Delta", symbol.toStdString(), spot);
        res.gamma = parse_greek_surface(j, "Gamma", symbol.toStdString(), spot);
        res.vega = parse_greek_surface(j, "Vega", symbol.toStdString(), spot);
        res.theta = parse_greek_surface(j, "Theta", symbol.toStdString(), spot);
        res.skew = parse_skew(j, symbol.toStdString());
        self->cached_vol_ = res;
        self->last_fetch_time_ = QDateTime::currentDateTime();
        LOG_INFO("Databento", "Vol surface ready for " + symbol);
        emit self->vol_surface_ready(res);
    });
}

// ── Futures term structure ─────────────────────────────────────────────────
void DatabentoService::fetch_futures_term_structure(const QStringList& commodities) {
    emit fetch_started("Fetching futures term structure...");
    LOG_INFO("Databento", "fetch_futures_term_structure");

    QPointer<DatabentoService> self = this;
    QStringList args = {"futures_term_structure", "root_symbol", commodities.join(","), "num_contracts",
                        QString::number(6)};

    run_script(args, [self](bool ok, const QJsonObject& j) {
        if (!self)
            return;
        DatabentoFuturesResult res;
        if (!ok || j["error"].toBool(false)) {
            res.success = false;
            res.error = ok ? j["message"].toString("Unknown error") : j["error"].toString();
            LOG_WARN("Databento", "Futures fetch failed: " + res.error);
            emit self->fetch_failed(res.error);
            emit self->futures_ready(res);
            return;
        }
        res.success = true;
        res.forward = parse_commodity_forward(j);
        res.contango = parse_contango(j);
        self->cached_futures_ = res;
        self->last_fetch_time_ = QDateTime::currentDateTime();
        LOG_INFO("Databento", "Futures term structure ready");
        emit self->futures_ready(res);
    });
}

// ── Generic surface fetch helper ──────────────────────────────────────────
// All 10 extended surface fetches use this: call Python, parse into DatabentoSurfaceResult.
void DatabentoService::fetch_local_vol(const QString& symbol, float spot) {
    emit fetch_started("Computing local vol for " + symbol + "...");
    QPointer<DatabentoService> self = this;
    run_script({"local_vol", "symbol", symbol, "spot", QString::number((double)spot, 'f', 2)},
               [self](bool ok, const QJsonObject& j) {
                   if (!self)
                       return;
                   DatabentoSurfaceResult res;
                   res.type = "local_vol";
                   if (!ok || j["error"].toBool(false)) {
                       res.success = false;
                       res.error = ok ? j["message"].toString("Unknown error") : j["error"].toString();
                       emit self->fetch_failed(res.error);
                   } else {
                       res.success = true;
                       for (const auto& v : j["strikes"].toArray())
                           res.x_axis.push_back((float)v.toDouble());
                       for (const auto& v : j["expirations"].toArray())
                           res.y_axis.push_back(v.toInt());
                       for (const auto& row : j["z"].toArray()) {
                           std::vector<float> r;
                           for (const auto& v : row.toArray())
                               r.push_back((float)v.toDouble());
                           res.z.push_back(r);
                       }
                       self->last_fetch_time_ = QDateTime::currentDateTime();
                   }
                   emit self->surface_ready(res);
               });
}

void DatabentoService::fetch_implied_dividend(const QString& symbol, float spot) {
    emit fetch_started("Computing implied dividends for " + symbol + "...");
    QPointer<DatabentoService> self = this;
    run_script({"implied_dividend", "symbol", symbol, "spot", QString::number((double)spot, 'f', 2)},
               [self](bool ok, const QJsonObject& j) {
                   if (!self)
                       return;
                   DatabentoSurfaceResult res;
                   res.type = "implied_dividend";
                   if (!ok || j["error"].toBool(false)) {
                       res.success = false;
                       res.error = ok ? j["message"].toString("Unknown error") : j["error"].toString();
                       emit self->fetch_failed(res.error);
                   } else {
                       res.success = true;
                       for (const auto& v : j["strikes"].toArray())
                           res.x_axis.push_back((float)v.toDouble());
                       for (const auto& v : j["expirations"].toArray())
                           res.y_axis.push_back(v.toInt());
                       for (const auto& row : j["z"].toArray()) {
                           std::vector<float> r;
                           for (const auto& v : row.toArray())
                               r.push_back((float)v.toDouble());
                           res.z.push_back(r);
                       }
                       self->last_fetch_time_ = QDateTime::currentDateTime();
                   }
                   emit self->surface_ready(res);
               });
}

void DatabentoService::fetch_liquidity(const QString& symbol, float spot) {
    emit fetch_started("Building liquidity heatmap for " + symbol + "...");
    QPointer<DatabentoService> self = this;
    run_script({"liquidity_heatmap", "symbol", symbol, "spot", QString::number((double)spot, 'f', 2)},
               [self](bool ok, const QJsonObject& j) {
                   if (!self)
                       return;
                   DatabentoSurfaceResult res;
                   res.type = "liquidity";
                   if (!ok || j["error"].toBool(false)) {
                       res.success = false;
                       res.error = ok ? j["message"].toString("Unknown error") : j["error"].toString();
                       emit self->fetch_failed(res.error);
                   } else {
                       res.success = true;
                       for (const auto& v : j["strikes"].toArray())
                           res.x_axis.push_back((float)v.toDouble());
                       for (const auto& v : j["expirations"].toArray())
                           res.y_axis.push_back(v.toInt());
                       for (const auto& row : j["z"].toArray()) {
                           std::vector<float> r;
                           for (const auto& v : row.toArray())
                               r.push_back((float)v.toDouble());
                           res.z.push_back(r);
                       }
                       self->last_fetch_time_ = QDateTime::currentDateTime();
                   }
                   emit self->surface_ready(res);
               });
}

void DatabentoService::fetch_commodity_vol(const QString& root_symbol) {
    emit fetch_started("Computing commodity vol for " + root_symbol + "...");
    QPointer<DatabentoService> self = this;
    run_script({"commodity_vol", "root_symbol", root_symbol}, [self](bool ok, const QJsonObject& j) {
        if (!self)
            return;
        DatabentoSurfaceResult res;
        res.type = "commodity_vol";
        if (!ok || j["error"].toBool(false)) {
            res.success = false;
            res.error = ok ? j["message"].toString("Unknown error") : j["error"].toString();
            emit self->fetch_failed(res.error);
        } else {
            res.success = true;
            for (const auto& v : j["strikes"].toArray())
                res.x_axis.push_back((float)v.toDouble());
            for (const auto& v : j["expirations"].toArray())
                res.y_axis.push_back(v.toInt());
            for (const auto& row : j["z"].toArray()) {
                std::vector<float> r;
                for (const auto& v : row.toArray())
                    r.push_back((float)v.toDouble());
                res.z.push_back(r);
            }
            self->last_fetch_time_ = QDateTime::currentDateTime();
        }
        emit self->surface_ready(res);
    });
}

void DatabentoService::fetch_crack_spread() {
    emit fetch_started("Computing crack spreads...");
    QPointer<DatabentoService> self = this;
    run_script({"crack_spread"}, [self](bool ok, const QJsonObject& j) {
        if (!self)
            return;
        DatabentoSurfaceResult res;
        res.type = "crack_spread";
        if (!ok || j["error"].toBool(false)) {
            res.success = false;
            res.error = ok ? j["message"].toString("Unknown error") : j["error"].toString();
            emit self->fetch_failed(res.error);
        } else {
            res.success = true;
            for (const auto& v : j["spread_types"].toArray())
                res.x_labels.push_back(v.toString().toStdString());
            for (const auto& v : j["contract_months"].toArray())
                res.y_axis.push_back(v.toInt());
            for (const auto& row : j["z"].toArray()) {
                std::vector<float> r;
                for (const auto& v : row.toArray())
                    r.push_back((float)v.toDouble());
                res.z.push_back(r);
            }
            self->last_fetch_time_ = QDateTime::currentDateTime();
        }
        emit self->surface_ready(res);
    });
}

void DatabentoService::fetch_stress_test(const QStringList& symbols) {
    emit fetch_started("Running stress test scenarios...");
    QPointer<DatabentoService> self = this;
    run_script({"stress_test", "symbols", symbols.join(",")}, [self](bool ok, const QJsonObject& j) {
        if (!self)
            return;
        DatabentoSurfaceResult res;
        res.type = "stress_test";
        if (!ok || j["error"].toBool(false)) {
            res.success = false;
            res.error = ok ? j["message"].toString("Unknown error") : j["error"].toString();
            emit self->fetch_failed(res.error);
        } else {
            res.success = true;
            for (const auto& v : j["scenarios"].toArray())
                res.x_labels.push_back(v.toString().toStdString());
            for (const auto& v : j["portfolios"].toArray())
                res.y_labels.push_back(v.toString().toStdString());
            for (const auto& row : j["z"].toArray()) {
                std::vector<float> r;
                for (const auto& v : row.toArray())
                    r.push_back((float)v.toDouble());
                res.z.push_back(r);
            }
            self->last_fetch_time_ = QDateTime::currentDateTime();
        }
        emit self->surface_ready(res);
    });
}

void DatabentoService::fetch_futures_settlements(const QStringList& symbols, int days,
                                                 std::function<void(bool, const SettlementsBySymbol&, const QString&)> cb) {
    run_script({"futures_data", "symbols", symbols.join(","), "days", QString::number(days), "schema", "ohlcv-1d",
                "stype_in", "continuous"},
               [cb](bool ok, const QJsonObject& j) {
                   if (!ok) {
                       cb(false, {}, j["error"].toString(QStringLiteral("Databento request failed")));
                       return;
                   }
                   if (j["error"].toBool(false)) {
                       cb(false, {}, j["message"].toString(QStringLiteral("Unknown error")));
                       return;
                   }
                   SettlementsBySymbol out;
                   const QJsonObject data = j["data"].toObject();
                   for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
                       QMap<QString, double> by_date;
                       for (const QJsonValue& bar : it.value().toArray()) {
                           const QJsonObject b = bar.toObject();
                           const QString date = b.value("date").toString().left(10);
                           const double close = b.value("close").toDouble();
                           if (QDate::fromString(date, Qt::ISODate).isValid() && close > 0.0 && std::isfinite(close))
                               by_date.insert(date, close);
                       }
                       if (!by_date.isEmpty())
                           out.insert(it.key(), by_date);
                   }
                   cb(true, out, {});
               });
}

// Treasury curve from CBOT Treasury futures. The old Python builder turned a ~110 settlement into a
// "yield" of ~198% ((100 - price / 100) * 2); this prices each settlement as the 6% notional bond at a
// maturity close to the usual cheapest-to-deliver (see db_notional_bond_yield_pct for the error band).
void DatabentoService::fetch_yield_curve() {
    emit fetch_started("Fetching CBOT Treasury futures settlements (ZT, ZF, ZN, TN, ZB, UB)...");
    struct Leg {
        const char* root;
        const char* label;
        double ctd_years; // typical remaining maturity of the cheapest-to-deliver
    };
    static const Leg kLegs[] = {{"ZT", "ZT 2Y", 1.9},       {"ZF", "ZF 5Y", 4.2},  {"ZN", "ZN 10Y", 6.6},
                                {"TN", "TN Ultra 10Y", 9.6}, {"ZB", "ZB Bond", 15.3}, {"UB", "UB Ultra", 25.5}};
    QStringList symbols;
    for (const Leg& leg : kLegs)
        symbols << QString("%1.c.0").arg(leg.root);

    QPointer<DatabentoService> self = this;
    fetch_futures_settlements(symbols, 45, [self](bool ok, const SettlementsBySymbol& s, const QString& err) {
        if (!self)
            return;
        DatabentoSurfaceResult res;
        res.type = "yield_curve";
        if (!ok) {
            res.error = err;
            emit self->fetch_failed(res.error);
            emit self->surface_ready(res);
            return;
        }
        std::vector<const Leg*> legs;
        for (const Leg& leg : kLegs) {
            if (!s.value(QString("%1.c.0").arg(leg.root)).isEmpty())
                legs.push_back(&leg);
        }
        if (legs.size() < 2) {
            res.error = QStringLiteral("Fewer than two Treasury futures returned settlements - a curve needs at "
                                       "least two maturities (GLBX.MDP3 access required)");
            emit self->fetch_failed(res.error);
            emit self->surface_ready(res);
            return;
        }
        // Settlement dates every maturity has, newest first (at most 30 sessions).
        QStringList dates = s.value(QString("%1.c.0").arg(legs.front()->root)).keys();
        for (const Leg* leg : legs) {
            const auto m = s.value(QString("%1.c.0").arg(leg->root));
            QStringList kept;
            for (const QString& d : dates)
                if (m.contains(d))
                    kept << d;
            dates = kept;
        }
        std::reverse(dates.begin(), dates.end());
        for (const QString& d : dates) {
            if (res.z.size() >= 30)
                break;
            std::vector<float> row;
            bool complete = true;
            for (const Leg* leg : legs) {
                const double y = databento_curve_detail::db_notional_bond_yield_pct(
                    s.value(QString("%1.c.0").arg(leg->root)).value(d), leg->ctd_years);
                if (std::isnan(y)) {
                    complete = false;
                    break;
                }
                row.push_back(static_cast<float>(y));
            }
            if (!complete)
                continue;
            res.y_labels.push_back(d.toStdString());
            res.y_axis.push_back(static_cast<int>(res.z.size()));
            res.z.push_back(std::move(row));
        }
        if (res.z.empty()) {
            res.error = QStringLiteral("No settlement date has prices for every Treasury future");
            emit self->fetch_failed(res.error);
            emit self->surface_ready(res);
            return;
        }
        for (const Leg* leg : legs) {
            res.x_axis.push_back(static_cast<float>(std::lround(leg->ctd_years * 12.0)));
            res.x_labels.push_back(leg->label);
        }
        res.success = true;
        self->last_fetch_time_ = QDateTime::currentDateTime();
        emit self->surface_ready(res);
    });
}

// Forward fed-funds rates from the CME 30-day fed funds futures strip (ZQ.c.0 = current month).
// Each contract settles at 100 minus the average effective fed funds rate of its month, so the
// average rate over [start, start + window) months is the mean of those months' implied rates.
void DatabentoService::fetch_forward_rate() {
    emit fetch_started("Fetching CME 30-day fed funds futures (ZQ) strip...");
    QStringList symbols;
    for (int i = 0; i < 12; ++i)
        symbols << QString("ZQ.c.%1").arg(i);
    QPointer<DatabentoService> self = this;
    fetch_futures_settlements(symbols, 10, [self](bool ok, const SettlementsBySymbol& s, const QString& err) {
        if (!self)
            return;
        DatabentoSurfaceResult res;
        res.type = "forward_rate";
        if (!ok) {
            res.error = err;
            emit self->fetch_failed(res.error);
            emit self->surface_ready(res);
            return;
        }
        const std::vector<double> rates = databento_curve_detail::db_monthly_rate_strip(s, "ZQ", 12, nullptr);
        const int n = static_cast<int>(rates.size());
        std::vector<int> windows;
        for (int w : {1, 3, 6})
            if (w <= n)
                windows.push_back(w);
        const int max_window = windows.empty() ? n + 1 : windows.back();
        std::vector<int> starts;
        for (int st = 0; st + max_window <= n; ++st)
            starts.push_back(st);
        if (windows.size() < 2 || starts.size() < 2) {
            res.error = QString("Only %1 fed funds futures month(s) returned a settlement - the forward surface "
                                "needs at least 4 consecutive months")
                            .arg(n);
            emit self->fetch_failed(res.error);
            emit self->surface_ready(res);
            return;
        }
        for (int st : starts) {
            std::vector<float> row;
            for (int w : windows) {
                double sum = 0.0;
                for (int k = st; k < st + w; ++k)
                    sum += rates[size_t(k)];
                row.push_back(static_cast<float>(sum / w));
            }
            res.z.push_back(std::move(row));
            res.x_axis.push_back(static_cast<float>(st));
        }
        res.y_axis = windows;
        res.success = true;
        self->last_fetch_time_ = QDateTime::currentDateTime();
        emit self->surface_ready(res);
    });
}

// US policy-rate path implied by the monthly CME rate futures: 30-day fed funds (ZQ) and 1-month
// SOFR (SR1), both "100 minus the month's average rate". The old Python builder also showed "ECB" and
// "BoJ" rows that were EUR/JPY currency-futures prices or hard-coded fallbacks, not rates; there is no
// GLBX source for those banks, so only the two US strips are shown.
void DatabentoService::fetch_rate_path() {
    emit fetch_started("Fetching CME fed funds (ZQ) and 1M SOFR (SR1) futures strips...");
    QStringList symbols;
    for (int i = 0; i < 12; ++i)
        symbols << QString("ZQ.c.%1").arg(i) << QString("SR1.c.%1").arg(i);
    QPointer<DatabentoService> self = this;
    fetch_futures_settlements(symbols, 10, [self](bool ok, const SettlementsBySymbol& s, const QString& err) {
        if (!self)
            return;
        DatabentoSurfaceResult res;
        res.type = "rate_path";
        if (!ok) {
            res.error = err;
            emit self->fetch_failed(res.error);
            emit self->surface_ready(res);
            return;
        }
        QString front_date;
        const std::vector<double> ff = databento_curve_detail::db_monthly_rate_strip(s, "ZQ", 12, &front_date);
        QString sofr_date;
        const std::vector<double> sofr = databento_curve_detail::db_monthly_rate_strip(s, "SR1", 12, &sofr_date);
        if (ff.size() < 2 && sofr.size() < 2) {
            res.error = QStringLiteral("Neither the fed funds (ZQ) nor the 1M SOFR (SR1) futures strip returned "
                                       "settlements (GLBX.MDP3 access required)");
            emit self->fetch_failed(res.error);
            emit self->surface_ready(res);
            return;
        }
        // Rows must share the month columns: keep both strips over their common length when SOFR covers
        // at least six months, otherwise show the longer strip alone.
        std::vector<std::pair<std::string, const std::vector<double>*>> rows;
        size_t months = 0;
        if (ff.size() >= 2 && sofr.size() >= 6) {
            months = std::min(ff.size(), sofr.size());
            rows = {{"Fed funds (ZQ)", &ff}, {"SOFR 1M (SR1)", &sofr}};
        } else if (ff.size() >= sofr.size()) {
            months = ff.size();
            rows = {{"Fed funds (ZQ)", &ff}};
        } else {
            months = sofr.size();
            rows = {{"SOFR 1M (SR1)", &sofr}};
            front_date = sofr_date;
        }
        // c.0 is the current contract month at the settlement date.
        const QDate front = QDate::fromString(front_date, Qt::ISODate);
        for (size_t m = 0; m < months; ++m) {
            res.y_axis.push_back(static_cast<int>(m));
            res.y_labels.push_back(front.isValid() ? front.addMonths(static_cast<int>(m)).toString("MMM yy").toStdString()
                                                   : QString("M+%1").arg(static_cast<int>(m)).toStdString());
        }
        for (const auto& [label, strip] : rows) {
            res.x_labels.push_back(label);
            std::vector<float> row;
            for (size_t m = 0; m < months; ++m)
                row.push_back(static_cast<float>((*strip)[m]));
            res.z.push_back(std::move(row));
        }
        res.success = true;
        self->last_fetch_time_ = QDateTime::currentDateTime();
        emit self->surface_ready(res);
    });
}

// FX forward points from the CME FX futures term structure: each deferred contract's settlement minus
// the front contract's, in quote points (0.0001; 0.000001 for the yen contract, which is quoted in
// USD per JPY). The futures quote is foreign currency in USD, so pairs are labelled that way (JPY/USD,
// not USD/JPY). Contracts are counted in expiry order (C+1 = first deferred); their calendar months
// are not in the settlement feed, so tenors are not converted to months.
void DatabentoService::fetch_fx_forward_points() {
    emit fetch_started("Fetching CME FX futures term structures...");
    struct FxLeg {
        const char* root;
        const char* pair;
        double point;
    };
    static const FxLeg kFx[] = {{"6E", "EUR/USD", 1e-4}, {"6B", "GBP/USD", 1e-4}, {"6J", "JPY/USD", 1e-6},
                                {"6A", "AUD/USD", 1e-4}, {"6C", "CAD/USD", 1e-4}, {"6S", "CHF/USD", 1e-4}};
    static constexpr int kDeferred = 3; // C+1 .. C+3
    QStringList symbols;
    for (const FxLeg& leg : kFx)
        for (int i = 0; i <= kDeferred; ++i)
            symbols << QString("%1.c.%2").arg(leg.root).arg(i);

    QPointer<DatabentoService> self = this;
    fetch_futures_settlements(symbols, 10, [self](bool ok, const SettlementsBySymbol& s, const QString& err) {
        if (!self)
            return;
        DatabentoSurfaceResult res;
        res.type = "fx_forward_points";
        if (!ok) {
            res.error = err;
            emit self->fetch_failed(res.error);
            emit self->surface_ready(res);
            return;
        }
        using databento_curve_detail::db_latest_settlement;
        // Per pair: points of each deferred contract vs the front, all from the front's latest session
        // (a deferred contract with no bar that day ends the strip — thin back months trade rarely).
        std::vector<std::pair<std::string, std::vector<float>>> strips;
        for (const FxLeg& leg : kFx) {
            QString front_date;
            const double front = db_latest_settlement(s, QString("%1.c.0").arg(leg.root), &front_date);
            if (std::isnan(front))
                continue;
            std::vector<float> row;
            for (int i = 1; i <= kDeferred; ++i) {
                const auto bars = s.value(QString("%1.c.%2").arg(leg.root).arg(i));
                if (!bars.contains(front_date))
                    break;
                row.push_back(static_cast<float>((bars.value(front_date) - front) / leg.point));
            }
            if (!row.empty())
                strips.emplace_back(leg.pair, std::move(row));
        }
        // A surface needs one column count for every row: use the depth all pairs share.
        size_t depth = kDeferred;
        for (const auto& st : strips)
            depth = std::min(depth, st.second.size());
        if (strips.size() < 2 || depth == 0) {
            res.error = QStringLiteral("Fewer than two FX futures term structures had a deferred contract settling "
                                       "with the front month (GLBX.MDP3 access required)");
            emit self->fetch_failed(res.error);
            emit self->surface_ready(res);
            return;
        }
        for (auto& [pair_name, row] : strips) {
            row.resize(depth);
            res.x_labels.push_back(pair_name);
            res.z.push_back(row);
        }
        for (int i = 1; i <= static_cast<int>(depth); ++i) {
            res.y_axis.push_back(i);
            res.y_labels.push_back(QString("C+%1").arg(i).toStdString());
        }
        res.success = true;
        self->last_fetch_time_ = QDateTime::currentDateTime();
        emit self->surface_ready(res);
    });
}

// ── Parsers ────────────────────────────────────────────────────────────────
surface::VolatilitySurfaceData DatabentoService::parse_vol_surface(const QJsonObject& j, const std::string& symbol,
                                                                   float spot) {
    surface::VolatilitySurfaceData data;
    data.underlying = symbol;
    data.spot_price = spot;

    // Expected: j["options"] = array of {strike, expiry_days, iv}
    QJsonArray opts = j["options"].toArray();
    if (opts.isEmpty()) {
        // Fallback: try j["data"]
        opts = j["data"].toArray();
    }

    std::set<float> strikes_set;
    std::set<int> dtes_set;
    std::map<std::pair<int, float>, float> iv_map;

    for (const auto& v : opts) {
        QJsonObject o = v.toObject();
        float strike = (float)o["strike"].toDouble();
        int dte = o["expiry_days"].toInt(o["dte"].toInt(30));
        float iv = (float)o["iv"].toDouble(o["implied_volatility"].toDouble());
        if (strike <= 0 || iv <= 0)
            continue;
        strikes_set.insert(strike);
        dtes_set.insert(dte);
        iv_map[{dte, strike}] = iv * 100.0f; // to percent
    }

    if (strikes_set.empty())
        return data;

    data.strikes.assign(strikes_set.begin(), strikes_set.end());
    data.expirations.assign(dtes_set.begin(), dtes_set.end());

    for (int dte : data.expirations) {
        std::vector<float> row;
        for (float s : data.strikes) {
            auto it = iv_map.find({dte, s});
            row.push_back(it != iv_map.end() ? it->second : 0.0f);
        }
        data.z.push_back(row);
    }
    return data;
}

surface::GreeksSurfaceData DatabentoService::parse_greek_surface(const QJsonObject& j, const std::string& greek,
                                                                 const std::string& symbol, float spot) {
    surface::GreeksSurfaceData data;
    data.underlying = symbol;
    data.spot_price = spot;
    data.greek_name = greek;

    QString greek_key = QString::fromStdString(greek).toLower();
    QJsonArray opts = j["options"].toArray();
    if (opts.isEmpty())
        opts = j["data"].toArray();

    std::set<float> strikes_set;
    std::set<int> dtes_set;
    std::map<std::pair<int, float>, float> val_map;

    for (const auto& v : opts) {
        QJsonObject o = v.toObject();
        float strike = (float)o["strike"].toDouble();
        int dte = o["expiry_days"].toInt(o["dte"].toInt(30));
        float val = (float)o[greek_key].toDouble();
        if (strike <= 0)
            continue;
        strikes_set.insert(strike);
        dtes_set.insert(dte);
        val_map[{dte, strike}] = val;
    }

    if (strikes_set.empty())
        return data;

    data.strikes.assign(strikes_set.begin(), strikes_set.end());
    data.expirations.assign(dtes_set.begin(), dtes_set.end());

    for (int dte : data.expirations) {
        std::vector<float> row;
        for (float s : data.strikes) {
            auto it = val_map.find({dte, s});
            row.push_back(it != val_map.end() ? it->second : 0.0f);
        }
        data.z.push_back(row);
    }
    return data;
}

surface::SkewSurfaceData DatabentoService::parse_skew(const QJsonObject& j, const std::string& symbol) {
    surface::SkewSurfaceData data;
    data.underlying = symbol;
    data.deltas = {10, 25, 50, 75, 90};

    QJsonArray skew_arr = j["skew"].toArray();
    if (skew_arr.isEmpty())
        return data;

    std::set<int> dtes_set;
    for (const auto& v : skew_arr) {
        int dte = v.toObject()["dte"].toInt();
        if (dte > 0)
            dtes_set.insert(dte);
    }
    data.expirations.assign(dtes_set.begin(), dtes_set.end());

    std::map<std::pair<int, int>, float> skew_map;
    for (const auto& v : skew_arr) {
        QJsonObject o = v.toObject();
        int dte = o["dte"].toInt();
        int delta = o["delta"].toInt();
        float val = (float)o["skew"].toDouble();
        skew_map[{dte, delta}] = val;
    }

    for (int dte : data.expirations) {
        std::vector<float> row;
        for (float d : data.deltas) {
            auto it = skew_map.find({dte, (int)d});
            row.push_back(it != skew_map.end() ? it->second : 0.0f);
        }
        data.z.push_back(row);
    }
    return data;
}

surface::CommodityForwardData DatabentoService::parse_commodity_forward(const QJsonObject& j) {
    surface::CommodityForwardData data;
    QJsonObject term = j["term_structure"].toObject();
    if (term.isEmpty())
        return data;

    std::set<int> months_set;
    for (const QString& sym : term.keys()) {
        data.commodities.push_back(sym.toStdString());
        QJsonArray curve = term[sym].toArray();
        for (int i = 0; i < curve.size(); i++)
            months_set.insert(i + 1);
    }

    data.contract_months.assign(months_set.begin(), months_set.end());

    for (const QString& sym : term.keys()) {
        QJsonArray curve = term[sym].toArray();
        std::vector<float> row;
        for (int i = 0; i < (int)data.contract_months.size(); i++) {
            if (i < curve.size())
                row.push_back((float)curve[i].toObject()["price"].toDouble(curve[i].toDouble()));
            else
                row.push_back(0.0f);
        }
        data.z.push_back(row);
    }
    return data;
}

// ── Metadata: list datasets ────────────────────────────────────────────────
void DatabentoService::list_datasets(std::function<void(QStringList)> cb) {
    if (!cached_datasets_.isEmpty()) {
        cb(cached_datasets_);
        return;
    }
    QPointer<DatabentoService> self = this;
    run_script({"list_datasets"}, [self, cb](bool ok, const QJsonObject& j) {
        QStringList out;
        if (self && ok && !j["error"].toBool(false)) {
            for (const auto& v : j["datasets"].toArray()) {
                QString id = v.toObject()["id"].toString();
                if (!id.isEmpty())
                    out << id;
            }
            self->cached_datasets_ = out;
        }
        cb(out);
    });
}

// ── Metadata: list schemas for a dataset ───────────────────────────────────
void DatabentoService::list_schemas(const QString& dataset, std::function<void(QStringList)> cb) {
    if (cached_schemas_.contains(dataset)) {
        cb(cached_schemas_.value(dataset));
        return;
    }
    QPointer<DatabentoService> self = this;
    QString ds = dataset;
    run_script({"list_schemas", "dataset", dataset}, [self, ds, cb](bool ok, const QJsonObject& j) {
        QStringList out;
        if (self && ok && !j["error"].toBool(false)) {
            for (const auto& v : j["schemas"].toArray()) {
                QString id = v.toObject()["id"].toString();
                if (!id.isEmpty())
                    out << id;
            }
            self->cached_schemas_[ds] = out;
        }
        cb(out);
    });
}

// ── Metadata: list publishers (optionally filtered to one dataset) ─────────
void DatabentoService::list_publishers(const QString& dataset, std::function<void(QList<DbPublisher>)> cb) {
    QString cache_key = dataset.isEmpty() ? "*" : dataset;
    if (cached_publishers_.contains(cache_key)) {
        cb(cached_publishers_.value(cache_key));
        return;
    }
    QPointer<DatabentoService> self = this;
    QStringList args = {"list_publishers"};
    if (!dataset.isEmpty()) {
        args << "dataset" << dataset;
    }
    run_script(args, [self, cache_key, cb](bool ok, const QJsonObject& j) {
        QList<DbPublisher> out;
        if (self && ok && !j["error"].toBool(false)) {
            for (const auto& v : j["publishers"].toArray()) {
                QJsonObject o = v.toObject();
                DbPublisher p;
                p.publisher_id = o["publisher_id"].toInt();
                p.dataset = o["dataset"].toString();
                p.venue = o["venue"].toString();
                p.description = o["description"].toString();
                out.push_back(p);
            }
            self->cached_publishers_[cache_key] = out;
        }
        cb(out);
    });
}

// ── Metadata: dataset date range ───────────────────────────────────────────
void DatabentoService::get_dataset_range(const QString& dataset, std::function<void(DbDatasetRange)> cb) {
    if (cached_ranges_.contains(dataset)) {
        cb(cached_ranges_.value(dataset));
        return;
    }
    QPointer<DatabentoService> self = this;
    run_script({"dataset_range", "dataset", dataset}, [self, dataset, cb](bool ok, const QJsonObject& j) {
        DbDatasetRange out;
        out.dataset = dataset;
        if (self && ok && !j["error"].toBool(false)) {
            QString s = j["start"].toString();
            QString e = j["end"].toString();
            // Accept "YYYY-MM-DD..." or full ISO with 'T'
            if (!s.isEmpty())
                out.start = QDate::fromString(s.left(10), "yyyy-MM-dd");
            if (!e.isEmpty())
                out.end = QDate::fromString(e.left(10), "yyyy-MM-dd");
            self->cached_ranges_[dataset] = out;
        }
        cb(out);
    });
}

// ── Metadata: cost estimate ────────────────────────────────────────────────
void DatabentoService::get_cost(const DbCostQuery& q, std::function<void(DbCostResult)> cb) {
    // Don't spend a Python process (and an API round-trip) on a request that cannot
    // be valid: the script substitutes SPY / XNAS.ITCH / 30 days for blank arguments,
    // so an empty selection would come back as a confident estimate for the wrong query.
    if (q.dataset.trimmed().isEmpty() || q.schema.trimmed().isEmpty() || q.symbols.join(QString()).trimmed().isEmpty() ||
        !q.start.isValid() || !q.end.isValid() || q.end < q.start) {
        DbCostResult invalid;
        invalid.success = false;
        invalid.error = tr("Choose a dataset, schema, symbol and a valid date range first.");
        cb(invalid);
        return;
    }
    QStringList args = {"get_cost_estimate",
                        "dataset",
                        q.dataset,
                        "symbols",
                        q.symbols.join(","),
                        "schema",
                        q.schema,
                        "start_date",
                        q.start.toString("yyyy-MM-dd"),
                        "end_date",
                        q.end.toString("yyyy-MM-dd")};
    run_script(args, [cb](bool ok, const QJsonObject& j) {
        DbCostResult res;
        if (!ok || j["error"].toBool(false)) {
            res.success = false;
            res.error = ok ? j["message"].toString("Unknown error") : j["error"].toString();
        } else {
            res.success = true;
            res.record_count = (qint64)j["record_count"].toDouble(0);
            res.cost_usd = j["cost_usd"].toDouble(0);
        }
        cb(res);
    });
}

// ── Metadata: symbol search ────────────────────────────────────────────────
void DatabentoService::search_symbols(const QString& query, const QString& dataset,
                                      std::function<void(QStringList)> cb) {
    if (query.trimmed().isEmpty()) { // nothing to search for — don't spawn Python
        cb({});
        return;
    }
    QStringList args = {"symbol_search", "query", query};
    if (!dataset.isEmpty()) {
        args << "dataset" << dataset;
    }
    run_script(args, [cb](bool ok, const QJsonObject& j) {
        QStringList out;
        if (ok && !j["error"].toBool(false)) {
            for (const auto& v : j["matches"].toArray()) {
                QString rs = v.toObject()["raw_symbol"].toString();
                if (!rs.isEmpty())
                    out << rs;
            }
        }
        cb(out);
    });
}

// ── Unified surface fetch dispatcher ───────────────────────────────────────
// Takes a DatabentoFetchParams produced by the SurfaceControlPanel and routes
// to the correct underlying fetch, which still emits the appropriate
// vol_surface_ready / ohlcv_ready / futures_ready / surface_ready signal.
void DatabentoService::fetch_with_params(const DatabentoFetchParams& params) {
    const QString& chart = params.chart_type;

    // Resolve spot for option-surface fetches. If the caller provided one,
    // honour it; otherwise default to a placeholder; the screen can later
    // consult DataHub for a live quote.
    float spot = params.spot_override > 0.0f ? params.spot_override : 100.0f;

    if (chart == "Volatility" || chart == "DeltaSurface" || chart == "GammaSurface" || chart == "VegaSurface" ||
        chart == "ThetaSurface" || chart == "SkewSurface") {
        fetch_options_surface(params.symbol, spot);
    } else if (chart == "LocalVolSurface") {
        fetch_local_vol(params.symbol, spot);
    } else if (chart == "ImpliedDividend") {
        fetch_implied_dividend(params.symbol, spot);
    } else if (chart == "LiquidityHeatmap") {
        fetch_liquidity(params.symbol, spot);
    } else if (chart == "CommodityForward" || chart == "ContangoBackwardation") {
        QStringList roots = params.basket;
        if (roots.isEmpty() && !params.symbol.isEmpty())
            roots << params.symbol;
        fetch_futures_term_structure(roots);
    } else if (chart == "CommodityVol") {
        fetch_commodity_vol(params.symbol);
    } else if (chart == "CrackSpread") {
        fetch_crack_spread();
    } else if (chart == "Correlation" || chart == "PCA" || chart == "Drawdown" || chart == "BetaSurface" ||
               chart == "VaR" || chart == "FactorExposure") {
        QDate start = params.start_date.isValid() ? params.start_date : QDate::currentDate().addDays(-60);
        QDate end = params.end_date.isValid() ? params.end_date : QDate::currentDate();
        int days = std::max(1, (int)start.daysTo(end));
        QStringList syms = params.basket.isEmpty() ? QStringList{params.symbol} : params.basket;
        fetch_ohlcv(syms, days);
    } else if (chart == "StressTestPnL") {
        fetch_stress_test(params.basket.isEmpty() ? QStringList{params.symbol} : params.basket);
    } else if (chart == "YieldCurve") {
        fetch_yield_curve();
    } else if (chart == "ForwardRate") {
        fetch_forward_rate();
    } else if (chart == "MonetaryPolicyPath") {
        fetch_rate_path();
    } else if (chart == "FXForwardPoints") {
        fetch_fx_forward_points();
    } else {
        // Surfaces with no Databento source — emit failure so UI shows DEMO state
        DatabentoSurfaceResult res;
        res.success = false;
        res.type = chart;
        res.error = "No Databento source for this surface";
        emit surface_ready(res);
    }
}

surface::ContangoData DatabentoService::parse_contango(const QJsonObject& j) {
    surface::ContangoData data;
    QJsonObject term = j["term_structure"].toObject();
    if (term.isEmpty())
        return data;

    std::set<int> months_set;
    for (const QString& sym : term.keys()) {
        data.commodities.push_back(sym.toStdString());
        QJsonArray curve = term[sym].toArray();
        for (int i = 0; i < curve.size(); i++)
            months_set.insert(i + 1);
    }
    data.contract_months.assign(months_set.begin(), months_set.end());

    for (const QString& sym : term.keys()) {
        QJsonArray curve = term[sym].toArray();
        float spot_price = curve.isEmpty() ? 1.0f : (float)curve[0].toObject()["price"].toDouble(curve[0].toDouble());
        if (spot_price == 0)
            spot_price = 1.0f;

        std::vector<float> row;
        for (int i = 0; i < (int)data.contract_months.size(); i++) {
            float price =
                (i < curve.size()) ? (float)curve[i].toObject()["price"].toDouble(curve[i].toDouble()) : spot_price;
            row.push_back((price / spot_price - 1.0f) * 100.0f);
        }
        data.z.push_back(row);
    }
    return data;
}

} // namespace fincept
