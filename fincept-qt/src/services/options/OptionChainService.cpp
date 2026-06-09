#include "services/options/OptionChainService.h"

#include "core/logging/Logger.h"
#include "core/market/MarketHours.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "datahub/TopicPolicy.h"
#include "python/OptionGreeksWorker.h"
#include "python/PythonRunner.h"
#include "services/databento/DatabentoService.h"
#include "storage/repositories/IvHistoryRepository.h"
#include "storage/repositories/SettingsRepository.h"
#include "trading/AccountDataStream.h"
#include "trading/AccountManager.h"
#include "trading/BrokerInterface.h"
#include "trading/BrokerRegistry.h"
#include "trading/DataStreamManager.h"
#include "trading/brokers/BrokerHttp.h"
#include "trading/instruments/InstrumentService.h"

#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMetaObject>
#include <QPointer>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::services::options {

using fincept::trading::BrokerCredentials;
using fincept::trading::BrokerQuote;
using fincept::trading::BrokerRegistry;
using fincept::trading::IBroker;
using fincept::trading::Instrument;
using fincept::trading::InstrumentService;
using fincept::trading::InstrumentType;

namespace {

constexpr int kMaxRequestsPerSec = 5;          // chain refresh cap (REST batch heavy)
constexpr int kChainTtlMs = 300'000;           // 5 min — chain freshness window
constexpr int kChainMinIntervalMs = 60'000;    // 1 min floor between consecutive refreshes per topic
constexpr int kChainCoalesceMs = 250;          // collapse rapid republishes (e.g. WS Phase 3)
constexpr int kChainTimeoutMs = 30'000;

constexpr int kAtmIvTtlMs = 5'000;
constexpr int kAtmIvMinIntervalMs = 3'000;

// WS-first streaming: a chain is considered "live" while a tick arrived within
// kWsStaleMs; past that the REST poll resumes as the fallback. The WS feed
// covers ATM ± kWsWindowStrikes strikes per side (≈31 strikes ≈ 62 option legs).
constexpr int kWsStaleMs = 12'000;
constexpr int kWsWindowStrikes = 15;

const QString kChainPrefix = QStringLiteral("option:chain:");
const QString kTickPrefix = QStringLiteral("option:tick:");
const QString kAtmIvPrefix = QStringLiteral("option:atm_iv:");
const QString kPcrPrefix = QStringLiteral("fno:pcr:");
const QString kMaxPainPrefix = QStringLiteral("fno:max_pain:");

constexpr int kGreeksThrottleMs = 500;        // per-strike Greeks recompute floor
constexpr int kPerLegTickCoalesceMs = 100;    // option:tick coalesce window
constexpr double kDefaultRiskFreeRate = 0.067; // RBI 91-day T-bill ballpark
constexpr double kUSRiskFreeRate = 0.043;     // US 3-month T-bill ballpark

const QString kDatabentoBrokerId = QStringLiteral("databento");
const QString kDatabentoScript = QStringLiteral("databento_fno_chain.py");

QString cache_key(const QString& broker, const QString& underlying) {
    return broker + "|" + underlying;
}

// Canonical per-leg routing key: strike (2dp) + side. Format-independent so the
// REST chain and the live WS feed agree regardless of how each spells the option
// symbol (options-chain-v3 vs the HSM brsymbol differ — e.g. "...CE" vs "...C").
QString opt_leg_key(double strike, bool is_call) {
    return QString::number(strike, 'f', 2) + (is_call ? QLatin1Char('C') : QLatin1Char('P'));
}

// Parse a Fyers F&O brsymbol (e.g. "NIFTY09JUN26C22650") into strike + call/put.
// The strike is the trailing digit run; the char immediately before it is C/P.
bool parse_fyers_opt_symbol(const QString& sym, double& strike, bool& is_call) {
    int i = int(sym.size()) - 1;
    while (i >= 0 && sym.at(i).isDigit())
        --i;
    if (i < 0 || i == int(sym.size()) - 1)
        return false;  // no trailing strike digits
    const QChar cp = sym.at(i);
    if (cp != QLatin1Char('C') && cp != QLatin1Char('P'))
        return false;
    bool ok = false;
    strike = QStringView(sym).mid(i + 1).toDouble(&ok);
    is_call = (cp == QLatin1Char('C'));
    return ok;
}

}  // namespace

OptionChainService& OptionChainService::instance() {
    static OptionChainService s;
    return s;
}

OptionChainService::OptionChainService() = default;

void OptionChainService::ensure_registered_with_hub() {
    if (hub_registered_)
        return;
    auto& hub = fincept::datahub::DataHub::instance();
    hub.register_producer(this);

    fincept::datahub::TopicPolicy chain_pol;
    chain_pol.ttl_ms = kChainTtlMs;
    chain_pol.min_interval_ms = kChainMinIntervalMs;
    chain_pol.coalesce_within_ms = kChainCoalesceMs;
    chain_pol.refresh_timeout_ms = kChainTimeoutMs;
    chain_pol.pause_when_inactive = true;  // chain ticks are heavy; pause when window inactive
    hub.set_policy_pattern(QStringLiteral("option:chain:*"), chain_pol);

    // ATM IV is push-only — it's republished as a side effect of every chain
    // Greeks enrichment. The TTL/min_interval are advisory for stale-warn UI.
    fincept::datahub::TopicPolicy iv_pol;
    iv_pol.ttl_ms = kAtmIvTtlMs;
    iv_pol.min_interval_ms = kAtmIvMinIntervalMs;
    iv_pol.push_only = true;
    hub.set_policy_pattern(QStringLiteral("option:atm_iv:*"), iv_pol);

    // PCR + max_pain are derived from chain — push-only republishes during chain refresh
    fincept::datahub::TopicPolicy derived_pol;
    derived_pol.push_only = true;
    derived_pol.coalesce_within_ms = kChainCoalesceMs;
    hub.set_policy_pattern(QStringLiteral("fno:pcr:*"), derived_pol);
    hub.set_policy_pattern(QStringLiteral("fno:max_pain:*"), derived_pol);

    // Per-leg ticks — push-only fan-out during chain refresh. Phase 3 source
    // is the chain producer; broker-WS-driven publishes will replace it
    // without changing subscribers.
    fincept::datahub::TopicPolicy tick_pol;
    tick_pol.push_only = true;
    tick_pol.coalesce_within_ms = kPerLegTickCoalesceMs;
    hub.set_policy_pattern(QStringLiteral("option:tick:*"), tick_pol);

    hub_registered_ = true;
    LOG_INFO("OptionChain",
             "Registered with DataHub (option:chain:*, option:tick:*, option:atm_iv:*, fno:pcr:*, fno:max_pain:*)");
}

QStringList OptionChainService::topic_patterns() const {
    return {
        QStringLiteral("option:chain:*"),
        QStringLiteral("option:atm_iv:*"),
    };
}

int OptionChainService::max_requests_per_sec() const {
    return kMaxRequestsPerSec;
}

void OptionChainService::refresh(const QStringList& topics) {
    for (const auto& topic : topics) {
        if (in_flight_.value(topic, false))
            continue;
        QString broker, underlying, expiry;
        if (topic.startsWith(kChainPrefix)) {
            if (!parse_chain_topic(topic, broker, underlying, expiry)) {
                LOG_WARN("OptionChain", QString("malformed chain topic: %1").arg(topic));
                fincept::datahub::DataHub::instance().publish_error(topic, "malformed topic");
                continue;
            }
            // WS-first: while a fresh native WebSocket feed drives this chain,
            // stand the REST poll down. on_ws_quote keeps last_chain_ current,
            // so re-publishing it clears in_flight + warms the TTL (the hub
            // scheduler won't re-poll for another TTL window) without a broker
            // round-trip. PCR / max-pain are recomputed from the live OI.
            if (ws_feed_fresh(topic) &&
                chain_topic(last_chain_.broker_id, last_chain_.underlying, last_chain_.expiry) == topic) {
                double tce = 0, tpe = 0;
                last_chain_.pcr = compute_pcr(last_chain_.rows, tce, tpe);
                last_chain_.total_ce_oi = tce;
                last_chain_.total_pe_oi = tpe;
                last_chain_.max_pain = compute_max_pain(last_chain_.rows);
                auto& hub = fincept::datahub::DataHub::instance();
                hub.publish(topic, QVariant::fromValue(last_chain_));
                hub.publish(kPcrPrefix + broker + ":" + underlying + ":" + expiry,
                            QVariant::fromValue(last_chain_.pcr));
                hub.publish(kMaxPainPrefix + broker + ":" + underlying + ":" + expiry,
                            QVariant::fromValue(last_chain_.max_pain));
                continue;
            }
            in_flight_[topic] = true;
            refresh_chain(broker, underlying, expiry);
        } else if (topic.startsWith(kAtmIvPrefix)) {
            // ATM IV is push-only — derived from the chain's ATM CE/PE IV when
            // the Greeks worker enriches a refresh. The hub policy marks it
            // push_only so refresh() shouldn't normally be called for it; this
            // branch exists only to log if a consumer requests force=true.
            LOG_DEBUG("OptionChain", QString("ATM IV refresh requested (push-only — drive via chain): %1").arg(topic));
        }
    }
}

QString OptionChainService::chain_topic(const QString& broker_id, const QString& underlying,
                                        const QString& expiry) const {
    return kChainPrefix + broker_id + ":" + underlying + ":" + expiry;
}

QStringList OptionChainService::list_underlyings(const QString& broker_id) const {
    if (broker_id == kDatabentoBrokerId)
        return databento_underlyings();
    return InstrumentService::instance().list_underlyings(broker_id, QStringLiteral("NFO"));
}

QStringList OptionChainService::list_expiries(const QString& broker_id, const QString& underlying) const {
    if (broker_id == kDatabentoBrokerId)
        return databento_expiry_cache_.value(underlying);
    return InstrumentService::instance().list_expiries(broker_id, underlying, QStringLiteral("NFO"));
}

QStringList OptionChainService::databento_underlyings() {
    return {
        QStringLiteral("SPY"),  QStringLiteral("QQQ"),  QStringLiteral("IWM"),  QStringLiteral("DIA"),
        QStringLiteral("AAPL"), QStringLiteral("MSFT"), QStringLiteral("AMZN"), QStringLiteral("GOOGL"),
        QStringLiteral("TSLA"), QStringLiteral("NVDA"), QStringLiteral("META"), QStringLiteral("AMD"),
        QStringLiteral("JPM"),  QStringLiteral("BAC"),  QStringLiteral("GS"),
        QStringLiteral("XLF"),  QStringLiteral("XLE"),  QStringLiteral("XLK"),
        QStringLiteral("GLD"),  QStringLiteral("SLV"),
    };
}

void OptionChainService::list_databento_expiries(const QString& underlying,
                                                  std::function<void(QStringList)> callback) {
    if (databento_expiry_cache_.contains(underlying)) {
        callback(databento_expiry_cache_.value(underlying));
        return;
    }
    auto& db_svc = fincept::DatabentoService::instance();
    if (!db_svc.has_api_key()) {
        callback({});
        return;
    }
    QJsonObject json_args;
    json_args[QStringLiteral("api_key")] = db_svc.api_key();
    json_args[QStringLiteral("symbol")] = underlying;
    QStringList script_args = {
        QStringLiteral("list_expiries"),
        QString::fromUtf8(QJsonDocument(json_args).toJson(QJsonDocument::Compact)),
    };
    QPointer<OptionChainService> self = this;
    fincept::python::PythonRunner::instance().run(
        kDatabentoScript, script_args,
        [self, underlying, callback](fincept::python::PythonResult result) {
            if (!self) return;
            QStringList expiries;
            if (result.success) {
                QJsonParseError err;
                auto doc = QJsonDocument::fromJson(result.output.toUtf8(), &err);
                if (err.error == QJsonParseError::NoError && doc.isObject()) {
                    auto j = doc.object();
                    if (!j[QStringLiteral("error")].toBool(false)) {
                        for (const auto& v : j[QStringLiteral("expiries")].toArray())
                            expiries.append(v.toString());
                    }
                }
            }
            if (!expiries.isEmpty())
                self->databento_expiry_cache_[underlying] = expiries;
            callback(expiries);
        });
}

bool OptionChainService::parse_chain_topic(const QString& topic, QString& broker, QString& underlying,
                                           QString& expiry) {
    // Format: option:chain:<broker>:<underlying>:<expiry>
    if (!topic.startsWith(kChainPrefix))
        return false;
    const QString tail = topic.mid(kChainPrefix.size());
    const QStringList parts = tail.split(QLatin1Char(':'));
    if (parts.size() != 3)
        return false;
    broker = parts.at(0);
    underlying = parts.at(1);
    expiry = parts.at(2);
    return !broker.isEmpty() && !underlying.isEmpty() && !expiry.isEmpty();
}

double OptionChainService::resolve_spot(const QString& broker_id, const QString& underlying) {
    const QString key = cache_key(broker_id, underlying);
    auto it = spot_cache_.find(key);
    if (it != spot_cache_.end())
        return *it;
    return 0.0;  // populated during refresh_chain when underlying quote is fetched
}

void OptionChainService::refresh_chain(const QString& broker_id, const QString& underlying,
                                       const QString& expiry) {
    if (broker_id == kDatabentoBrokerId) {
        refresh_chain_databento(underlying, expiry);
        return;
    }
    if (broker_id == QLatin1String("fyers")) {
        refresh_chain_fyers(broker_id, underlying, expiry);
        return;
    }

    const QString topic = chain_topic(broker_id, underlying, expiry);
    LOG_INFO("OptionChain", QString("refresh_chain: broker='%1' underlying='%2' expiry='%3' topic='%4'")
                                .arg(broker_id, underlying, expiry, topic));

    auto& reg = BrokerRegistry::instance();
    IBroker* broker = reg.get(broker_id);
    if (!broker) {
        LOG_ERROR("OptionChain", QString("refresh_chain: broker '%1' not in registry").arg(broker_id));
        in_flight_.remove(topic);
        fincept::datahub::DataHub::instance().publish_error(topic, "broker not registered: " + broker_id);
        return;
    }

    QVector<Instrument> instruments =
        InstrumentService::instance().find_options_for_underlying(broker_id, underlying, expiry, "NFO");
    LOG_INFO("OptionChain", QString("refresh_chain: find_options_for_underlying → %1 instruments").arg(instruments.size()));
    if (instruments.isEmpty()) {
        in_flight_.remove(topic);
        fincept::datahub::DataHub::instance().publish_error(topic,
                                                            "no instruments cached for " + underlying + " / " + expiry);
        return;
    }

    // Build the symbol list for batched get_quotes. Underlying spot quote is
    // appended last so we can pull it out by index.
    QStringList quote_syms;
    for (const auto& inst : instruments) {
        if (inst.instrument_type == InstrumentType::CE || inst.instrument_type == InstrumentType::PE)
            quote_syms.append(inst.brexchange + ":" + inst.brsymbol);
    }
    // Nearest FUT as spot price fallback — works across all brokers since
    // it uses brexchange:brsymbol (the broker's native symbol format).
    QString fut_sym;
    for (const auto& inst : instruments) {
        if (inst.instrument_type == InstrumentType::FUT) {
            fut_sym = inst.brexchange + ":" + inst.brsymbol;
            quote_syms.append(fut_sym);
            break;
        }
    }
    // Underlying spot — index symbols use NSE_INDEX exchange (Zerodha) or
    // appropriate per-broker mapping. Phase 1 assumes NSE for stocks and
    // NSE_INDEX for the four indices. Brokers that use different index
    // formats (e.g. Fyers: NSE:NIFTY50-INDEX) will miss this and fall
    // back to the FUT price above.
    static const QSet<QString> kIndexNames = {"NIFTY", "BANKNIFTY", "FINNIFTY", "MIDCPNIFTY"};
    const bool is_index = kIndexNames.contains(underlying);
    const QString underlying_sym =
        is_index ? "NSE_INDEX:" + underlying : "NSE:" + underlying;
    quote_syms.append(underlying_sym);

    // Load credentials via AccountManager (keys are account.{uuid}.*),
    // not IBroker::load_credentials (which looks for broker.{id}.*).
    BrokerCredentials creds;
    auto accounts = fincept::trading::AccountManager::instance().list_accounts(broker_id);
    for (const auto& acct : accounts) {
        if (acct.is_active) {
            creds = fincept::trading::AccountManager::instance().load_credentials(acct.account_id);
            break;
        }
    }
    if (creds.access_token.isEmpty() && !accounts.isEmpty())
        creds = fincept::trading::AccountManager::instance().load_credentials(accounts.first().account_id);
    LOG_INFO("OptionChain", QString("refresh_chain: credentials loaded, token_len=%1").arg(creds.access_token.size()));

    QPointer<OptionChainService> self = this;
    (void)QtConcurrent::run([self, broker, creds, quote_syms, instruments, broker_id, underlying, expiry, topic,
                             underlying_sym, fut_sym]() {
        // Run on worker thread — broker REST calls block (BrokerHttp uses
        // QEventLoop internally per the broker_http_blocking memory).
        auto resp = broker->get_quotes(creds, QVector<QString>(quote_syms.begin(), quote_syms.end()));
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self, resp, instruments, broker_id, underlying, expiry, topic, underlying_sym, fut_sym]() {
                if (!self)
                    return;
                self->in_flight_.remove(topic);

                if (!resp.success || !resp.data.has_value()) {
                    LOG_ERROR("OptionChain", QString("get_quotes failed: %1").arg(resp.error));
                    fincept::datahub::DataHub::instance().publish_error(topic, resp.error);
                    emit self->chain_failed(topic, resp.error);
                    return;
                }

                LOG_INFO("OptionChain", QString("get_quotes returned %1 quotes for %2")
                                            .arg(resp.data->size()).arg(topic));

                // Build a quick lookup from "EXCH:brsymbol" → BrokerQuote.
                QHash<QString, BrokerQuote> by_sym;
                by_sym.reserve(resp.data->size());
                for (const auto& q : *resp.data) {
                    by_sym.insert(q.symbol, q);
                }

                // Pull underlying spot (+ day change) — index symbol first, FUT fallback
                double spot = 0, spot_chg = 0, spot_chg_pct = 0;
                if (auto it = by_sym.find(underlying_sym); it != by_sym.end()) {
                    spot = it->ltp;
                    spot_chg = it->change;
                    spot_chg_pct = it->change_pct;
                }
                if (spot <= 0 && !fut_sym.isEmpty()) {
                    if (auto it = by_sym.find(fut_sym); it != by_sym.end()) {
                        spot = it->ltp;
                        spot_chg = it->change;
                        spot_chg_pct = it->change_pct;
                    }
                }
                LOG_INFO("OptionChain", QString("spot=%1 (underlying_sym='%2', fut_sym='%3')")
                                            .arg(spot).arg(underlying_sym, fut_sym));
                if (spot > 0)
                    self->spot_cache_.insert(cache_key(broker_id, underlying), spot);

                // Group CE/PE by strike
                struct Pair {
                    Instrument ce;
                    Instrument pe;
                    bool has_ce = false, has_pe = false;
                };
                QMap<double, Pair> by_strike;
                for (const auto& inst : instruments) {
                    if (inst.instrument_type == InstrumentType::CE) {
                        by_strike[inst.strike].ce = inst;
                        by_strike[inst.strike].has_ce = true;
                    } else if (inst.instrument_type == InstrumentType::PE) {
                        by_strike[inst.strike].pe = inst;
                        by_strike[inst.strike].has_pe = true;
                    }
                }

                OptionChain chain;
                chain.broker_id = broker_id;
                chain.underlying = underlying;
                chain.expiry = expiry;
                chain.spot = spot;
                chain.spot_change = spot_chg;
                chain.spot_change_pct = spot_chg_pct;
                chain.kind = (underlying == "NIFTY" || underlying == "BANKNIFTY"
                              || underlying == "FINNIFTY" || underlying == "MIDCPNIFTY")
                                 ? UnderlyingKind::Index
                                 : UnderlyingKind::Stock;
                chain.timestamp_ms = QDateTime::currentMSecsSinceEpoch();
                chain.rows.reserve(by_strike.size());

                for (auto sit = by_strike.constBegin(); sit != by_strike.constEnd(); ++sit) {
                    OptionChainRow row;
                    row.strike = sit.key();
                    if (sit->has_ce) {
                        row.ce_token = sit->ce.instrument_token;
                        row.ce_symbol = sit->ce.symbol;
                        row.lot_size = sit->ce.lot_size;
                        const QString k = sit->ce.brexchange + ":" + sit->ce.brsymbol;
                        if (auto qit = by_sym.find(k); qit != by_sym.end())
                            row.ce_quote = *qit;
                    }
                    if (sit->has_pe) {
                        row.pe_token = sit->pe.instrument_token;
                        row.pe_symbol = sit->pe.symbol;
                        if (row.lot_size == 0)
                            row.lot_size = sit->pe.lot_size;
                        const QString k = sit->pe.brexchange + ":" + sit->pe.brsymbol;
                        if (auto qit = by_sym.find(k); qit != by_sym.end())
                            row.pe_quote = *qit;
                    }
                    chain.rows.append(row);
                }

                chain.atm_strike = compute_atm(chain.rows, spot);
                chain.pcr = compute_pcr(chain.rows, chain.total_ce_oi, chain.total_pe_oi);
                chain.max_pain = compute_max_pain(chain.rows);
                for (auto& row : chain.rows)
                    row.is_atm = (std::abs(row.strike - chain.atm_strike) < 1e-6);

                // First publish — Greeks/IV are zero. The Greeks worker fills
                // them in asynchronously and republishes the same topic with
                // populated rows; the hub coalesce window collapses the two
                // publishes when the worker is fast.

                // Publish chain + derived stats. Hub fans out to subscribers.
                auto& hub = fincept::datahub::DataHub::instance();
                hub.publish(topic, QVariant::fromValue(chain));
                hub.publish(kPcrPrefix + broker_id + ":" + underlying + ":" + expiry,
                            QVariant::fromValue(chain.pcr));
                hub.publish(kMaxPainPrefix + broker_id + ":" + underlying + ":" + expiry,
                            QVariant::fromValue(chain.max_pain));
                self->publish_per_leg_ticks(chain);
                self->last_chain_ = chain;
                emit self->chain_published(chain);
                LOG_INFO("OptionChain",
                         QString("Published %1 strikes for %2/%3/%4 (spot=%5, ATM=%6, PCR=%7)")
                             .arg(chain.rows.size())
                             .arg(broker_id, underlying, expiry)
                             .arg(spot, 0, 'f', 2)
                             .arg(chain.atm_strike, 0, 'f', 2)
                             .arg(chain.pcr, 0, 'f', 3));

                // Kick off Greeks/IV enrichment — async; republishes when ready.
                self->enrich_with_greeks(chain, topic);
            },
            Qt::QueuedConnection);
    });
}

void OptionChainService::refresh_chain_databento(const QString& underlying, const QString& expiry) {
    const QString topic = chain_topic(kDatabentoBrokerId, underlying, expiry);
    if (in_flight_.value(topic, false))
        return;

    auto& db_svc = fincept::DatabentoService::instance();
    if (!db_svc.has_api_key()) {
        fincept::datahub::DataHub::instance().publish_error(topic, "No Databento API key configured");
        return;
    }

    in_flight_[topic] = true;

    QJsonObject json_args;
    json_args[QStringLiteral("api_key")] = db_svc.api_key();
    json_args[QStringLiteral("symbol")] = underlying;
    json_args[QStringLiteral("expiry")] = expiry;
    QStringList script_args = {
        QStringLiteral("get_chain"),
        QString::fromUtf8(QJsonDocument(json_args).toJson(QJsonDocument::Compact)),
    };

    QPointer<OptionChainService> self = this;
    fincept::python::PythonRunner::instance().run(
        kDatabentoScript, script_args,
        [self, topic, underlying, expiry](fincept::python::PythonResult result) {
            if (!self) return;
            self->in_flight_.remove(topic);

            if (!result.success) {
                fincept::datahub::DataHub::instance().publish_error(topic, result.error);
                emit self->chain_failed(topic, result.error);
                return;
            }

            QJsonParseError err;
            auto doc = QJsonDocument::fromJson(result.output.toUtf8(), &err);
            if (err.error != QJsonParseError::NoError || !doc.isObject()) {
                fincept::datahub::DataHub::instance().publish_error(topic, "JSON parse error: " + err.errorString());
                return;
            }
            auto j = doc.object();
            if (j[QStringLiteral("error")].toBool(false)) {
                QString msg = j[QStringLiteral("message")].toString(QStringLiteral("Unknown error"));
                fincept::datahub::DataHub::instance().publish_error(topic, msg);
                emit self->chain_failed(topic, msg);
                return;
            }

            double spot = j[QStringLiteral("spot")].toDouble(0);
            QJsonArray rows_arr = j[QStringLiteral("rows")].toArray();

            OptionChain chain;
            chain.broker_id = kDatabentoBrokerId;
            chain.underlying = underlying;
            chain.expiry = expiry;
            chain.spot = spot;
            chain.kind = UnderlyingKind::Stock;
            chain.timestamp_ms = QDateTime::currentMSecsSinceEpoch();
            chain.rows.reserve(rows_arr.size());

            for (const auto& rv : rows_arr) {
                auto ro = rv.toObject();
                OptionChainRow row;
                row.strike = ro[QStringLiteral("strike")].toDouble();
                row.lot_size = ro[QStringLiteral("lot_size")].toInt(100);
                row.ce_token = static_cast<qint64>(ro[QStringLiteral("ce_token")].toDouble());
                row.pe_token = static_cast<qint64>(ro[QStringLiteral("pe_token")].toDouble());
                row.ce_symbol = ro[QStringLiteral("ce_symbol")].toString();
                row.pe_symbol = ro[QStringLiteral("pe_symbol")].toString();

                row.ce_quote.symbol = row.ce_symbol;
                row.ce_quote.ltp = ro[QStringLiteral("ce_ltp")].toDouble();
                row.ce_quote.bid = ro[QStringLiteral("ce_bid")].toDouble();
                row.ce_quote.ask = ro[QStringLiteral("ce_ask")].toDouble();
                row.ce_quote.volume = static_cast<qint64>(ro[QStringLiteral("ce_volume")].toDouble());
                row.ce_quote.oi = static_cast<qint64>(ro[QStringLiteral("ce_oi")].toDouble());

                row.pe_quote.symbol = row.pe_symbol;
                row.pe_quote.ltp = ro[QStringLiteral("pe_ltp")].toDouble();
                row.pe_quote.bid = ro[QStringLiteral("pe_bid")].toDouble();
                row.pe_quote.ask = ro[QStringLiteral("pe_ask")].toDouble();
                row.pe_quote.volume = static_cast<qint64>(ro[QStringLiteral("pe_volume")].toDouble());
                row.pe_quote.oi = static_cast<qint64>(ro[QStringLiteral("pe_oi")].toDouble());

                chain.rows.append(row);
            }

            if (spot > 0)
                self->spot_cache_.insert(cache_key(kDatabentoBrokerId, underlying), spot);

            chain.atm_strike = compute_atm(chain.rows, spot);
            chain.pcr = compute_pcr(chain.rows, chain.total_ce_oi, chain.total_pe_oi);
            chain.max_pain = compute_max_pain(chain.rows);
            for (auto& row : chain.rows)
                row.is_atm = (std::abs(row.strike - chain.atm_strike) < 1e-6);

            auto& hub = fincept::datahub::DataHub::instance();
            hub.publish(topic, QVariant::fromValue(chain));
            hub.publish(kPcrPrefix + kDatabentoBrokerId + ":" + underlying + ":" + expiry,
                        QVariant::fromValue(chain.pcr));
            hub.publish(kMaxPainPrefix + kDatabentoBrokerId + ":" + underlying + ":" + expiry,
                        QVariant::fromValue(chain.max_pain));
            self->publish_per_leg_ticks(chain);
            self->last_chain_ = chain;
            emit self->chain_published(chain);
            LOG_INFO("OptionChain",
                     QString("Published %1 strikes from Databento for %2/%3 (spot=%4, ATM=%5)")
                         .arg(chain.rows.size())
                         .arg(underlying, expiry)
                         .arg(spot, 0, 'f', 2)
                         .arg(chain.atm_strike, 0, 'f', 2));

            self->enrich_with_greeks(chain, topic);
        });
}

// ── Fyers option chain via /data/options-chain-v3 ─────────────────────────────
// Single REST call returns OI, Greeks, IV, volume, spot, expiries — no Python.

void OptionChainService::refresh_chain_fyers(const QString& broker_id, const QString& underlying,
                                              const QString& expiry) {
    const QString topic = chain_topic(broker_id, underlying, expiry);
    LOG_INFO("OptionChain", QString("refresh_chain_fyers: %1/%2/%3").arg(broker_id, underlying, expiry));

    // Map underlying to Fyers API symbol
    static const QHash<QString, QString> kIndexMap = {
        {"NIFTY", "NSE:NIFTY50-INDEX"},
        {"BANKNIFTY", "NSE:NIFTYBANK-INDEX"},
        {"FINNIFTY", "NSE:FINNIFTY-INDEX"},
        {"MIDCPNIFTY", "NSE:MIDCPNIFTY-INDEX"},
    };
    QString api_sym = kIndexMap.value(underlying);
    if (api_sym.isEmpty())
        api_sym = "NSE:" + underlying + "-EQ";

    // Convert expiry "26-MAY-26" → unix timestamp for the API
    qint64 expiry_ts = 0;
    if (expiry.length() >= 9 && expiry[2] == QLatin1Char('-') && expiry[6] == QLatin1Char('-')) {
        static const QHash<QString, int> kMon = {
            {"JAN",1},{"FEB",2},{"MAR",3},{"APR",4},{"MAY",5},{"JUN",6},
            {"JUL",7},{"AUG",8},{"SEP",9},{"OCT",10},{"NOV",11},{"DEC",12},
        };
        int day = QStringView(expiry).left(2).toInt();
        int month = kMon.value(expiry.mid(3, 3).toUpper(), 0);
        int year = 2000 + QStringView(expiry).mid(7, 2).toInt();
        if (month > 0) {
            QDateTime dt(QDate(year, month, day), QTime(15, 30), QTimeZone::UTC);
            constexpr int kIstOffset = 5 * 3600 + 30 * 60;
            expiry_ts = dt.toSecsSinceEpoch() - kIstOffset;
        }
    }

    // Load credentials via AccountManager
    BrokerCredentials creds;
    auto accounts = fincept::trading::AccountManager::instance().list_accounts(broker_id);
    for (const auto& acct : accounts) {
        if (acct.is_active) {
            creds = fincept::trading::AccountManager::instance().load_credentials(acct.account_id);
            break;
        }
    }
    if (creds.access_token.isEmpty() && !accounts.isEmpty())
        creds = fincept::trading::AccountManager::instance().load_credentials(accounts.first().account_id);

    QString url = QString("https://api-t1.fyers.in/data/options-chain-v3?symbol=%1&strikecount=50&greeks=1")
                      .arg(api_sym);
    if (expiry_ts > 0)
        url += QString("&timestamp=%1").arg(expiry_ts);

    QPointer<OptionChainService> self = this;
    (void)QtConcurrent::run([self, creds, url, broker_id, underlying, expiry, topic]() {
        QMap<QString, QString> headers = {
            {"Authorization", creds.api_key + ":" + creds.access_token},
            {"Content-Type", "application/json"},
            {"Accept", "application/json"},
            {"version", "3"},
        };
        auto resp = fincept::trading::BrokerHttp::instance().get(url, headers);
        if (!self)
            return;

        QMetaObject::invokeMethod(self.data(), [self, resp, broker_id, underlying, expiry, topic]() {
            if (!self)
                return;
            self->in_flight_.remove(topic);

            if (!resp.success || resp.json.value("s").toString() != "ok") {
                const QString err = resp.success ? resp.json.value("message").toString("API error")
                                                 : resp.error;
                LOG_ERROR("OptionChain", QString("Fyers option chain failed: %1").arg(err));
                fincept::datahub::DataHub::instance().publish_error(topic, err);
                emit self->chain_failed(topic, err);
                return;
            }

            auto data = resp.json.value("data").toObject();
            auto arr = data.value("optionsChain").toArray();
            LOG_INFO("OptionChain", QString("Fyers option chain: %1 entries").arg(arr.size()));

            double spot = 0, spot_chg = 0, spot_chg_pct = 0;
            static const QSet<QString> kIdx = {"NIFTY","BANKNIFTY","FINNIFTY","MIDCPNIFTY"};

            // Group by strike: CE + PE
            struct StrikePair {
                OptionChainRow row;
                bool has_ce = false, has_pe = false;
            };
            QMap<double, StrikePair> by_strike;

            for (const auto& v : arr) {
                auto o = v.toObject();
                double strike = o.value("strike_price").toDouble();
                QString opt_type = o.value("option_type").toString();

                if (strike < 0 || opt_type.isEmpty()) {
                    spot = o.value("ltp").toDouble();
                    spot_chg = o.value("ltpch").toDouble();
                    spot_chg_pct = o.value("ltpchp").toDouble();
                    continue;
                }

                BrokerQuote q;
                q.symbol = o.value("symbol").toString();
                q.ltp = o.value("ltp").toDouble();
                q.bid = o.value("bid").toDouble();
                q.ask = o.value("ask").toDouble();
                q.volume = o.value("volume").toDouble();
                q.change = o.value("ltpch").toDouble();
                q.change_pct = o.value("ltpchp").toDouble();
                q.oi = qint64(o.value("oi").toDouble());
                q.oi_change_pct = o.value("oichp").toDouble();

                auto greeks_obj = o.value("greeks").toObject();
                OptionGreeks greeks;
                greeks.delta = greeks_obj.value("delta").toDouble();
                greeks.gamma = greeks_obj.value("gamma").toDouble();
                greeks.theta = greeks_obj.value("theta").toDouble();
                greeks.vega = greeks_obj.value("vega").toDouble();
                greeks.valid = true;
                double iv = greeks_obj.value("iv").toDouble() / 100.0; // API returns %, we store decimal

                qint64 token = qint64(o.value("fyToken").toString().toLongLong());

                auto& pair = by_strike[strike];
                pair.row.strike = strike;
                if (opt_type == "CE") {
                    pair.has_ce = true;
                    pair.row.ce_quote = q;
                    pair.row.ce_token = token;
                    pair.row.ce_symbol = q.symbol;
                    pair.row.ce_greeks = greeks;
                    pair.row.ce_iv = iv;
                } else if (opt_type == "PE") {
                    pair.has_pe = true;
                    pair.row.pe_quote = q;
                    pair.row.pe_token = token;
                    pair.row.pe_symbol = q.symbol;
                    pair.row.pe_greeks = greeks;
                    pair.row.pe_iv = iv;
                }
            }

            OptionChain chain;
            chain.broker_id = broker_id;
            chain.underlying = underlying;
            chain.expiry = expiry;
            chain.spot = spot;
            chain.spot_change = spot_chg;
            chain.spot_change_pct = spot_chg_pct;
            chain.kind = kIdx.contains(underlying) ? UnderlyingKind::Index : UnderlyingKind::Stock;
            chain.timestamp_ms = QDateTime::currentMSecsSinceEpoch();
            chain.rows.reserve(by_strike.size());

            for (auto it = by_strike.constBegin(); it != by_strike.constEnd(); ++it)
                chain.rows.append(it->row);

            // options-chain-v3 omits lot size — backfill from the instrument
            // master (all NFO legs of an underlying share one lot) so the chain,
            // strategy builder, and right-click order path all have it.
            int lot_size = 0;
            for (const auto& in : InstrumentService::instance().find_options_for_underlying(
                     broker_id, underlying, expiry, QStringLiteral("NFO"))) {
                if (in.lot_size > 0) {
                    lot_size = in.lot_size;
                    break;
                }
            }
            if (lot_size > 0)
                for (auto& row : chain.rows)
                    row.lot_size = lot_size;

            chain.atm_strike = compute_atm(chain.rows, spot);
            chain.pcr = compute_pcr(chain.rows, chain.total_ce_oi, chain.total_pe_oi);
            chain.max_pain = compute_max_pain(chain.rows);
            for (auto& row : chain.rows)
                row.is_atm = (std::abs(row.strike - chain.atm_strike) < 1e-6);

            if (spot > 0)
                self->spot_cache_.insert(cache_key(broker_id, underlying), spot);

            auto& hub = fincept::datahub::DataHub::instance();
            hub.publish(topic, QVariant::fromValue(chain));
            hub.publish(kPcrPrefix + broker_id + ":" + underlying + ":" + expiry,
                        QVariant::fromValue(chain.pcr));
            hub.publish(kMaxPainPrefix + broker_id + ":" + underlying + ":" + expiry,
                        QVariant::fromValue(chain.max_pain));
            self->publish_per_leg_ticks(chain);
            self->publish_atm_iv(chain);
            self->last_chain_ = chain;
            emit self->chain_published(chain);

            // Hand off to the live WS feed (Fyers + market open + connected).
            // While it stays fresh this REST path is suppressed in refresh().
            self->maybe_start_ws_stream(chain, topic);

            LOG_INFO("OptionChain",
                     QString("Fyers chain: %1 strikes, spot=%2, ATM=%3, PCR=%4, OI CE=%5 PE=%6")
                         .arg(chain.rows.size())
                         .arg(spot, 0, 'f', 2)
                         .arg(chain.atm_strike, 0, 'f', 0)
                         .arg(chain.pcr, 0, 'f', 3)
                         .arg(chain.total_ce_oi, 0, 'f', 0)
                         .arg(chain.total_pe_oi, 0, 'f', 0));
        }, Qt::QueuedConnection);
    });
}

double OptionChainService::compute_atm(const QVector<OptionChainRow>& rows, double spot) {
    if (rows.isEmpty() || spot <= 0)
        return 0;
    double best_strike = rows.first().strike;
    double best_diff = std::abs(best_strike - spot);
    for (const auto& r : rows) {
        const double d = std::abs(r.strike - spot);
        if (d < best_diff) {
            best_diff = d;
            best_strike = r.strike;
        }
    }
    return best_strike;
}

double OptionChainService::compute_pcr(const QVector<OptionChainRow>& rows, double& total_ce_oi,
                                       double& total_pe_oi) {
    total_ce_oi = 0;
    total_pe_oi = 0;
    for (const auto& r : rows) {
        total_ce_oi += double(r.ce_quote.oi);
        total_pe_oi += double(r.pe_quote.oi);
    }
    if (total_ce_oi <= 0)
        return 0;
    return total_pe_oi / total_ce_oi;
}

// ── Phase 3 — Greeks/IV enrichment, ATM IV, per-leg ticks ──────────────────

double OptionChainService::risk_free_rate() {
    if (risk_free_rate_loaded_)
        return risk_free_rate_;
    risk_free_rate_loaded_ = true;
    auto r = fincept::SettingsRepository::instance().get(QStringLiteral("fno.risk_free_rate"),
                                                         QString::number(kDefaultRiskFreeRate, 'f', 4));
    if (r.is_err()) {
        risk_free_rate_ = kDefaultRiskFreeRate;
        return risk_free_rate_;
    }
    bool ok = false;
    const double v = r.value().toDouble(&ok);
    risk_free_rate_ = (ok && v > 0 && v < 0.5) ? v : kDefaultRiskFreeRate;
    LOG_INFO("OptionChain", QString("Risk-free rate r=%1").arg(risk_free_rate_, 0, 'f', 4));
    return risk_free_rate_;
}

double OptionChainService::compute_t_years(const QString& expiry) {
    QDate exp = QDate::fromString(expiry, "dd-MMM-yy");
    if (!exp.isValid())
        exp = QDate::fromString(expiry, "yyyy-MM-dd");
    if (!exp.isValid())
        return 1.0 / 365.0;
    const QDate today = QDate::currentDate();
    const int days = today.daysTo(exp);
    if (days <= 0)
        return 1.0 / 365.0;  // expiry day or past — clamp to one day
    return double(days) / 365.0;
}

void OptionChainService::publish_per_leg_ticks(const OptionChain& chain) {
    auto& hub = fincept::datahub::DataHub::instance();
    int published = 0;
    for (const auto& row : chain.rows) {
        if (row.ce_token != 0) {
            const QString t = kTickPrefix + chain.broker_id + ":" + QString::number(row.ce_token);
            hub.publish(t, QVariant::fromValue(row.ce_quote));
            ++published;
        }
        if (row.pe_token != 0) {
            const QString t = kTickPrefix + chain.broker_id + ":" + QString::number(row.pe_token);
            hub.publish(t, QVariant::fromValue(row.pe_quote));
            ++published;
        }
    }
    LOG_DEBUG("OptionChain", QString("Per-leg tick fan-out: %1 publishes").arg(published));
}

// ── Pin contracts ──────────────────────────────────────────────────────────

void OptionChainService::pin_contracts(const QString& topic, const QStringList& symbols) {
    if (symbols.isEmpty())
        pinned_contracts_.remove(topic);
    else
        pinned_contracts_[topic] = symbols;

    // If this is the currently-streamed topic, re-arm the stream so the newly
    // pinned symbols are subscribed immediately without waiting for next REST refresh.
    if (!ws_topic_.isEmpty() && ws_topic_ == topic && !last_chain_.rows.isEmpty()) {
        maybe_start_ws_stream(last_chain_, topic);
    } else {
        LOG_DEBUG("OptionChain", QString("pin_contracts: stored %1 pin(s) for %2 (applies at next WS refresh)")
                                     .arg(symbols.size()).arg(topic));
    }
}

// ── WS-first live streaming ────────────────────────────────────────────────

bool OptionChainService::ws_feed_fresh(const QString& topic) const {
    return !ws_topic_.isEmpty() && topic == ws_topic_ && ws_last_tick_ms_ > 0 &&
           (QDateTime::currentMSecsSinceEpoch() - ws_last_tick_ms_) < kWsStaleMs;
}

void OptionChainService::maybe_start_ws_stream(const OptionChain& chain, const QString& topic) {
    using fincept::trading::AccountManager;
    using fincept::trading::ConnectionState;
    using fincept::trading::DataStreamManager;

    // Only Fyers exposes a native option WebSocket feed in this build; every
    // other broker (and Databento) keeps the REST path. Market must be open and
    // an account connected, else fall back to REST.
    if (chain.broker_id != QLatin1String("fyers") || !fincept::core::market::nse_fo_market_open()) {
        stop_ws_stream();
        return;
    }

    QString account_id;
    for (const auto& acct : AccountManager::instance().list_accounts(chain.broker_id)) {
        if (AccountManager::instance().connection_state(acct.account_id) == ConnectionState::Connected) {
            account_id = acct.account_id;
            break;
        }
    }
    if (account_id.isEmpty()) {
        stop_ws_stream();  // not connected → REST fallback
        return;
    }

    // ATM ± window. Find the ATM row, take kWsWindowStrikes either side.
    int atm_idx = 0;
    double best = std::numeric_limits<double>::max();
    for (int i = 0; i < chain.rows.size(); ++i) {
        const double d = std::abs(chain.rows[i].strike - chain.atm_strike);
        if (d < best) { best = d; atm_idx = i; }
    }
    const int lo = std::max(0, atm_idx - kWsWindowStrikes);
    const int hi = std::min(int(chain.rows.size()) - 1, atm_idx + kWsWindowStrikes);

    auto fyers_sym = [](const QString& s) {
        return s.contains(QLatin1Char(':')) ? s : QStringLiteral("NSE:") + s;
    };

    // Key the routing map by (strike, side) — see opt_leg_key. The WS subscribe
    // still uses the broker symbol; only the tick→row match is format-agnostic.
    QHash<QString, qint64> sym_token;
    QStringList sub_syms;
    for (int i = lo; i <= hi; ++i) {
        const auto& r = chain.rows[i];
        if (r.ce_token != 0 && !r.ce_symbol.isEmpty()) {
            sub_syms.append(fyers_sym(r.ce_symbol));
            sym_token.insert(opt_leg_key(r.strike, true), r.ce_token);
        }
        if (r.pe_token != 0 && !r.pe_symbol.isEmpty()) {
            sub_syms.append(fyers_sym(r.pe_symbol));
            sym_token.insert(opt_leg_key(r.strike, false), r.pe_token);
        }
    }
    if (sub_syms.isEmpty()) {
        stop_ws_stream();
        return;
    }

    // Union in any extra symbols pinned by the algo engine (e.g. specific legs
    // for an active deployment). When nothing is pinned, sub_syms is unchanged.
    const QStringList& pins = pinned_contracts_.value(topic);
    for (const QString& pin : pins) {
        if (!pin.isEmpty() && !sub_syms.contains(pin))
            sub_syms.append(pin);
    }

    ws_topic_ = topic;
    ws_account_id_ = account_id;
    ws_sym_token_ = sym_token;

    auto& dsm = DataStreamManager::instance();
    dsm.start_stream(account_id);  // idempotent: creates+starts or resume()s
    auto* stream = dsm.stream_for(account_id);
    if (!stream) {
        stop_ws_stream();
        return;
    }

    // One persistent binding to the account's quote feed; on_ws_quote filters
    // by ws_account_id_ + ws_sym_token_. Rebind to the (possibly new) stream.
    if (ws_quote_conn_)
        disconnect(ws_quote_conn_);
    ws_quote_conn_ = connect(stream, &fincept::trading::AccountDataStream::quote_updated,
                             this, &OptionChainService::on_ws_quote);

    // Tear the feed down when the chain topic loses its last subscriber (the
    // Chain tab was hidden or switched to another expiry/underlying).
    if (!ws_idle_conn_) {
        QPointer<OptionChainService> self = this;
        ws_idle_conn_ = connect(&fincept::datahub::DataHub::instance(),
                                &fincept::datahub::DataHub::topic_idle, this,
                                [self](const QString& t) {
                                    if (self && t == self->ws_topic_)
                                        self->stop_ws_stream();
                                });
    }

    // Pushes immediately if the socket is already connected (it is, when the
    // equity feed is live); otherwise the WS connect handler picks up the union.
    stream->subscribe_symbols(QStringLiteral("fno:chain"), sub_syms);
    LOG_INFO("OptionChain", QString("WS stream armed: %1 legs for %2 (acct=%3, ATM=%4)")
                                .arg(sub_syms.size())
                                .arg(topic, account_id)
                                .arg(chain.atm_strike, 0, 'f', 0));
}

void OptionChainService::on_ws_quote(const QString& account_id, const QString& symbol,
                                     const fincept::trading::BrokerQuote& quote) {
    if (account_id != ws_account_id_)
        return;
    // Match the live WS symbol to a chain leg by (strike, side). Equity-watchlist
    // ticks (e.g. "RELIANCE") don't parse as options and are skipped here.
    double k_strike = 0;
    bool k_call = false;
    if (!parse_fyers_opt_symbol(symbol, k_strike, k_call))
        return;
    auto it = ws_sym_token_.constFind(opt_leg_key(k_strike, k_call));
    if (it == ws_sym_token_.constEnd()) {
        static qint64 s_last_unmatched_ms = 0;
        const qint64 t_now = QDateTime::currentMSecsSinceEpoch();
        if (t_now - s_last_unmatched_ms > 5'000) {
            s_last_unmatched_ms = t_now;
            LOG_DEBUG("OptionChain", QString("WS tick unmatched: %1 (strike=%2 %3) not in window")
                                         .arg(symbol).arg(k_strike, 0, 'f', 0).arg(k_call ? "CE" : "PE"));
        }
        return;
    }
    const qint64 token = it.value();

    // Merge into last_chain_ so the suppressed-REST re-publish stays live.
    // The Fyers `sf` tick has no prev-day OI baseline, so carry the seed's
    // oi_change_pct forward; keep the last known OI if a lite tick omitted it.
    fincept::trading::BrokerQuote q = quote;
    bool matched = false;
    for (auto& row : last_chain_.rows) {
        if (row.ce_token == token) {
            q.oi_change_pct = row.ce_quote.oi_change_pct;
            if (q.oi == 0) q.oi = row.ce_quote.oi;
            row.ce_quote = q;
            matched = true;
            break;
        }
        if (row.pe_token == token) {
            q.oi_change_pct = row.pe_quote.oi_change_pct;
            if (q.oi == 0) q.oi = row.pe_quote.oi;
            row.pe_quote = q;
            matched = true;
            break;
        }
    }
    if (!matched)
        return;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    // Throttled liveness log — confirms WS→chain routing without per-tick spam.
    static qint64 s_last_ws_log_ms = 0;
    if (now - s_last_ws_log_ms > 5'000) {
        s_last_ws_log_ms = now;
        LOG_INFO("OptionChain", QString("WS tick live: %1 ltp=%2 oi=%3 → token %4")
                                    .arg(symbol).arg(q.ltp, 0, 'f', 2).arg(q.oi).arg(token));
    }
    ws_last_tick_ms_ = now;

    // Fast path to the table: per-leg tick → OptionChainModel::update_leg_quote.
    const QString t = kTickPrefix + last_chain_.broker_id + ":" + QString::number(token);
    fincept::datahub::DataHub::instance().publish(t, QVariant::fromValue(q));
}

void OptionChainService::stop_ws_stream() {
    if (ws_topic_.isEmpty())
        return;
    if (!ws_account_id_.isEmpty()) {
        if (auto* stream = fincept::trading::DataStreamManager::instance().stream_for(ws_account_id_))
            stream->unsubscribe_consumer(QStringLiteral("fno:chain"));
    }
    if (ws_quote_conn_) {
        disconnect(ws_quote_conn_);
        ws_quote_conn_ = {};
    }
    LOG_INFO("OptionChain", QString("WS stream stopped: %1").arg(ws_topic_));
    ws_topic_.clear();
    ws_account_id_.clear();
    ws_sym_token_.clear();
    ws_last_tick_ms_ = 0;
}

void OptionChainService::publish_atm_iv(const OptionChain& chain) {
    if (chain.atm_strike <= 0)
        return;
    double atm_ce_iv = 0, atm_pe_iv = 0;
    int hits = 0;
    for (const auto& row : chain.rows) {
        if (!row.is_atm)
            continue;
        if (row.ce_iv > 0) { atm_ce_iv = row.ce_iv; ++hits; }
        if (row.pe_iv > 0) { atm_pe_iv = row.pe_iv; ++hits; }
        break;
    }
    if (hits == 0)
        return;
    double atm_iv;
    if (atm_ce_iv > 0 && atm_pe_iv > 0)
        atm_iv = 0.5 * (atm_ce_iv + atm_pe_iv);
    else
        atm_iv = atm_ce_iv > 0 ? atm_ce_iv : atm_pe_iv;
    const QString iv_topic = kAtmIvPrefix + chain.broker_id + ":" + chain.underlying;
    fincept::datahub::DataHub::instance().publish(iv_topic, QVariant::fromValue(atm_iv));

    // Persist the latest ATM IV for the day — IV percentile pill reads
    // back the trailing 90-day window from this table. Idempotent UPSERT
    // keyed on (underlying, today) so the last refresh of the day wins.
    if (atm_iv > 0) {
        const QString today = QDate::currentDate().toString(Qt::ISODate);
        auto r = fincept::IvHistoryRepository::instance().upsert(chain.underlying, today, atm_iv);
        if (r.is_err()) {
            LOG_DEBUG("OptionChain", QString("iv_history upsert failed: %1")
                                          .arg(QString::fromStdString(r.error())));
        }
    }

    LOG_DEBUG("OptionChain",
              QString("Published ATM IV %1=%2 (CE=%3 PE=%4)")
                  .arg(chain.underlying)
                  .arg(atm_iv, 0, 'f', 4)
                  .arg(atm_ce_iv, 0, 'f', 4)
                  .arg(atm_pe_iv, 0, 'f', 4));
}

void OptionChainService::enrich_with_greeks(const OptionChain& chain, const QString& topic) {
    if (chain.rows.isEmpty() || chain.spot <= 0)
        return;

    const double r = (chain.broker_id == kDatabentoBrokerId) ? kUSRiskFreeRate : risk_free_rate();
    const double t = compute_t_years(chain.expiry);
    // q=0 for indices and stocks v1 (no per-stock dividend lookup yet).
    const double q = 0.0;
    const qint64 now_ms = QDateTime::currentMSecsSinceEpoch();

    QJsonArray contracts;

    auto add_leg = [&](qint64 token, double strike, const fincept::trading::BrokerQuote& quote, bool is_call) {
        if (token == 0)
            return;
        const qint64 last = last_greeks_compute_ms_.value(token, 0);
        if (now_ms - last < kGreeksThrottleMs && (greeks_cache_.contains(token) || iv_cache_.contains(token)))
            return;  // throttled — cached values will be applied below
        // Pick mid price when both bid/ask are present, else LTP. Skip when
        // we have no usable price — IV solver would just fail.
        double price = 0;
        if (quote.bid > 0 && quote.ask > 0)
            price = 0.5 * (quote.bid + quote.ask);
        else if (quote.ltp > 0)
            price = quote.ltp;
        if (price <= 0)
            return;

        QJsonObject c;
        c["token"] = QJsonValue(qint64(token));
        c["S"] = chain.spot;
        c["K"] = strike;
        c["t"] = t;
        c["r"] = r;
        c["q"] = q;
        c["flag"] = is_call ? "c" : "p";
        c["market_price"] = price;
        contracts.append(c);
        last_greeks_compute_ms_[token] = now_ms;
    };

    for (const auto& row : chain.rows) {
        add_leg(row.ce_token, row.strike, row.ce_quote, /*is_call*/ true);
        add_leg(row.pe_token, row.strike, row.pe_quote, /*is_call*/ false);
    }

    // Apply cached values to a working copy so we can republish even when
    // every leg was throttled (no contracts to compute).
    OptionChain enriched = chain;
    auto apply_cached = [this](OptionChain& ch) {
        for (auto& row : ch.rows) {
            if (row.ce_token != 0) {
                if (auto it = iv_cache_.constFind(row.ce_token); it != iv_cache_.constEnd())
                    row.ce_iv = it.value();
                if (auto it = greeks_cache_.constFind(row.ce_token); it != greeks_cache_.constEnd())
                    row.ce_greeks = it.value();
            }
            if (row.pe_token != 0) {
                if (auto it = iv_cache_.constFind(row.pe_token); it != iv_cache_.constEnd())
                    row.pe_iv = it.value();
                if (auto it = greeks_cache_.constFind(row.pe_token); it != greeks_cache_.constEnd())
                    row.pe_greeks = it.value();
            }
        }
    };
    apply_cached(enriched);

    if (contracts.isEmpty()) {
        // Fully throttled — republish with cached Greeks + ATM IV side-effect
        // and exit. Only worth doing when at least one cache entry exists.
        if (!iv_cache_.isEmpty() || !greeks_cache_.isEmpty()) {
            fincept::datahub::DataHub::instance().publish(topic, QVariant::fromValue(enriched));
            publish_atm_iv(enriched);
        }
        return;
    }

    QJsonObject payload;
    payload["contracts"] = contracts;

    QPointer<OptionChainService> self = this;
    fincept::python::OptionGreeksWorker::instance().submit(
        QStringLiteral("option_greeks_batch"), payload,
        [self, enriched, topic](bool ok, QJsonObject result, QString error) mutable {
            if (!self)
                return;
            if (!ok) {
                LOG_WARN("OptionChain", QString("Greeks batch failed: %1").arg(error));
                return;
            }
            const QJsonArray results = result.value(QStringLiteral("results")).toArray();

            // Index cached results by token for O(N) row patching.
            QHash<qint64, OptionGreeks> new_greeks;
            QHash<qint64, double> new_iv;
            new_greeks.reserve(results.size());
            new_iv.reserve(results.size());
            for (const auto& v : results) {
                const QJsonObject row = v.toObject();
                if (!row.value(QStringLiteral("valid")).toBool())
                    continue;
                const qint64 token = qint64(row.value(QStringLiteral("token")).toDouble());
                if (token == 0)
                    continue;
                OptionGreeks g;
                g.delta = row.value(QStringLiteral("delta")).toDouble();
                g.gamma = row.value(QStringLiteral("gamma")).toDouble();
                g.theta = row.value(QStringLiteral("theta")).toDouble();
                g.vega = row.value(QStringLiteral("vega")).toDouble();
                g.rho = row.value(QStringLiteral("rho")).toDouble();
                g.valid = true;
                new_greeks.insert(token, g);
                new_iv.insert(token, row.value(QStringLiteral("iv")).toDouble());
            }

            // Merge into per-token caches so future throttled refreshes can
            // reuse them.
            for (auto it = new_greeks.constBegin(); it != new_greeks.constEnd(); ++it)
                self->greeks_cache_.insert(it.key(), it.value());
            for (auto it = new_iv.constBegin(); it != new_iv.constEnd(); ++it)
                self->iv_cache_.insert(it.key(), it.value());

            // Apply caches into the working chain copy and republish.
            for (auto& row : enriched.rows) {
                if (row.ce_token != 0) {
                    if (auto it = self->iv_cache_.constFind(row.ce_token); it != self->iv_cache_.constEnd())
                        row.ce_iv = it.value();
                    if (auto it = self->greeks_cache_.constFind(row.ce_token); it != self->greeks_cache_.constEnd())
                        row.ce_greeks = it.value();
                }
                if (row.pe_token != 0) {
                    if (auto it = self->iv_cache_.constFind(row.pe_token); it != self->iv_cache_.constEnd())
                        row.pe_iv = it.value();
                    if (auto it = self->greeks_cache_.constFind(row.pe_token); it != self->greeks_cache_.constEnd())
                        row.pe_greeks = it.value();
                }
            }

            fincept::datahub::DataHub::instance().publish(topic, QVariant::fromValue(enriched));
            self->publish_atm_iv(enriched);
            LOG_DEBUG("OptionChain",
                      QString("Greeks enriched %1 contracts; republished %2").arg(new_iv.size()).arg(topic));
        });
}

double OptionChainService::compute_max_pain(const QVector<OptionChainRow>& rows) {
    // Max pain = strike that minimises total cash payout to OTM option holders
    // at expiry. For each candidate strike S*, sum:
    //   CE pain = sum over strikes K < S* of (S* - K) × CE_OI(K)
    //   PE pain = sum over strikes K > S* of (K - S*) × PE_OI(K)
    // Total pain = CE pain + PE pain. The strike with minimum total pain is
    // the one option writers would prefer the underlying to settle at.
    if (rows.isEmpty())
        return 0;
    double best_strike = rows.first().strike;
    double best_pain = std::numeric_limits<double>::infinity();
    for (const auto& candidate : rows) {
        double pain = 0;
        const double S = candidate.strike;
        for (const auto& r : rows) {
            if (r.strike < S)
                pain += (S - r.strike) * double(r.ce_quote.oi);
            else if (r.strike > S)
                pain += (r.strike - S) * double(r.pe_quote.oi);
        }
        if (pain < best_pain) {
            best_pain = pain;
            best_strike = S;
        }
    }
    return best_strike;
}

} // namespace fincept::services::options
