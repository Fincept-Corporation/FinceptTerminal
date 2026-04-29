#include "services/databento/DatabentoService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "storage/secure/SecureStorage.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>

#include <algorithm>
#include <cmath>
#include <set>

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
    auto r = SecureStorage::instance().retrieve(SECURE_KEY);
    if (r.is_ok() && !r.value().isEmpty())
        return r.value();
    auto r2 = SecureStorage::instance().retrieve(DATABENTO_ENV_KEY);
    return r2.is_ok() ? r2.value() : QString{};
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

    // Build JSON args object — always includes api_key
    QJsonObject json_args;
    json_args["api_key"] = key;

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
            LOG_ERROR("Databento", "Script error: " + result.error);
            cb(false, {{"error", result.error}});
            return;
        }
        QString json_str = extract_json_from_output(result.output);
        QJsonParseError err;
        auto doc = QJsonDocument::fromJson(json_str.toUtf8(), &err);
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

void DatabentoService::fetch_yield_curve() {
    emit fetch_started("Building yield curve from treasury futures...");
    QPointer<DatabentoService> self = this;
    run_script({"yield_curve"}, [self](bool ok, const QJsonObject& j) {
        if (!self)
            return;
        DatabentoSurfaceResult res;
        res.type = "yield_curve";
        if (!ok || j["error"].toBool(false)) {
            res.success = false;
            res.error = ok ? j["message"].toString("Unknown error") : j["error"].toString();
            emit self->fetch_failed(res.error);
        } else {
            res.success = true;
            for (const auto& v : j["time_points"].toArray())
                res.y_axis.push_back(v.toInt());
            for (const auto& v : j["maturities"].toArray())
                res.x_axis.push_back((float)v.toInt());
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

void DatabentoService::fetch_forward_rate() {
    emit fetch_started("Building forward rate surface...");
    QPointer<DatabentoService> self = this;
    run_script({"forward_rate"}, [self](bool ok, const QJsonObject& j) {
        if (!self)
            return;
        DatabentoSurfaceResult res;
        res.type = "forward_rate";
        if (!ok || j["error"].toBool(false)) {
            res.success = false;
            res.error = ok ? j["message"].toString("Unknown error") : j["error"].toString();
            emit self->fetch_failed(res.error);
        } else {
            res.success = true;
            for (const auto& v : j["start_tenors"].toArray())
                res.x_axis.push_back((float)v.toInt());
            for (const auto& v : j["forward_periods"].toArray())
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

void DatabentoService::fetch_rate_path() {
    emit fetch_started("Building monetary policy rate path...");
    QPointer<DatabentoService> self = this;
    run_script({"rate_path"}, [self](bool ok, const QJsonObject& j) {
        if (!self)
            return;
        DatabentoSurfaceResult res;
        res.type = "rate_path";
        if (!ok || j["error"].toBool(false)) {
            res.success = false;
            res.error = ok ? j["message"].toString("Unknown error") : j["error"].toString();
            emit self->fetch_failed(res.error);
        } else {
            res.success = true;
            for (const auto& v : j["central_banks"].toArray())
                res.x_labels.push_back(v.toString().toStdString());
            for (const auto& v : j["meetings_ahead"].toArray())
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

void DatabentoService::fetch_fx_forward_points() {
    emit fetch_started("Building FX forward points...");
    QPointer<DatabentoService> self = this;
    run_script({"fx_forward_points"}, [self](bool ok, const QJsonObject& j) {
        if (!self)
            return;
        DatabentoSurfaceResult res;
        res.type = "fx_forward_points";
        if (!ok || j["error"].toBool(false)) {
            res.success = false;
            res.error = ok ? j["message"].toString("Unknown error") : j["error"].toString();
            emit self->fetch_failed(res.error);
        } else {
            res.success = true;
            for (const auto& v : j["pairs"].toArray())
                res.x_labels.push_back(v.toString().toStdString());
            for (const auto& v : j["tenors"].toArray())
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
    QStringList args = {"get_cost_estimate",
                        "dataset", q.dataset,
                        "symbols", q.symbols.join(","),
                        "schema", q.schema,
                        "start_date", q.start.toString("yyyy-MM-dd"),
                        "end_date", q.end.toString("yyyy-MM-dd")};
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

    if (chart == "Volatility" || chart == "DeltaSurface" || chart == "GammaSurface" ||
        chart == "VegaSurface" || chart == "ThetaSurface" || chart == "SkewSurface") {
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
    } else if (chart == "Correlation" || chart == "PCA" || chart == "Drawdown" ||
               chart == "BetaSurface" || chart == "VaR" || chart == "FactorExposure") {
        QDate start = params.start_date.isValid() ? params.start_date
                                                  : QDate::currentDate().addDays(-60);
        QDate end = params.end_date.isValid() ? params.end_date : QDate::currentDate();
        int days = std::max(1, (int)start.daysTo(end));
        QStringList syms = params.basket.isEmpty() ? QStringList{params.symbol} : params.basket;
        fetch_ohlcv(syms, days);
    } else if (chart == "StressTestPnL") {
        fetch_stress_test(params.basket.isEmpty() ? QStringList{params.symbol} : params.basket);
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
