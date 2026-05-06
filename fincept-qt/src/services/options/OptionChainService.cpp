#include "services/options/OptionChainService.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "datahub/TopicPolicy.h"
#include "python/OptionGreeksWorker.h"
#include "storage/repositories/IvHistoryRepository.h"
#include "storage/repositories/SettingsRepository.h"
#include "trading/BrokerInterface.h"
#include "trading/BrokerRegistry.h"
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
constexpr int kChainTtlMs = 5'000;             // chain freshness window
constexpr int kChainMinIntervalMs = 3'000;     // floor between consecutive refreshes per topic
constexpr int kChainCoalesceMs = 250;          // collapse rapid republishes (e.g. WS Phase 3)
constexpr int kChainTimeoutMs = 30'000;

constexpr int kAtmIvTtlMs = 5'000;
constexpr int kAtmIvMinIntervalMs = 3'000;

const QString kChainPrefix = QStringLiteral("option:chain:");
const QString kTickPrefix = QStringLiteral("option:tick:");
const QString kAtmIvPrefix = QStringLiteral("option:atm_iv:");
const QString kPcrPrefix = QStringLiteral("fno:pcr:");
const QString kMaxPainPrefix = QStringLiteral("fno:max_pain:");

constexpr int kGreeksThrottleMs = 500;        // per-strike Greeks recompute floor
constexpr int kPerLegTickCoalesceMs = 100;    // option:tick coalesce window
constexpr double kDefaultRiskFreeRate = 0.067; // RBI 91-day T-bill ballpark

QString cache_key(const QString& broker, const QString& underlying) {
    return broker + "|" + underlying;
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
    return InstrumentService::instance().list_underlyings(broker_id, QStringLiteral("NFO"));
}

QStringList OptionChainService::list_expiries(const QString& broker_id, const QString& underlying) const {
    return InstrumentService::instance().list_expiries(broker_id, underlying, QStringLiteral("NFO"));
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
    const QString topic = chain_topic(broker_id, underlying, expiry);

    auto& reg = BrokerRegistry::instance();
    IBroker* broker = reg.get(broker_id);
    if (!broker) {
        in_flight_.remove(topic);
        fincept::datahub::DataHub::instance().publish_error(topic, "broker not registered: " + broker_id);
        return;
    }

    QVector<Instrument> instruments =
        InstrumentService::instance().find_options_for_underlying(broker_id, underlying, expiry, "NFO");
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
            quote_syms.append(inst.exchange + ":" + inst.brsymbol);
    }
    // Underlying spot — index symbols use NSE_INDEX exchange (Zerodha) or
    // appropriate per-broker mapping. Phase 1 assumes NSE for stocks and
    // NSE_INDEX for the four indices.
    static const QSet<QString> kIndexNames = {"NIFTY", "BANKNIFTY", "FINNIFTY", "MIDCPNIFTY"};
    const bool is_index = kIndexNames.contains(underlying);
    const QString underlying_sym =
        is_index ? "NSE_INDEX:" + underlying : "NSE:" + underlying;
    quote_syms.append(underlying_sym);

    BrokerCredentials creds = broker->load_credentials();

    QPointer<OptionChainService> self = this;
    (void)QtConcurrent::run([self, broker, creds, quote_syms, instruments, broker_id, underlying, expiry, topic,
                             underlying_sym]() {
        // Run on worker thread — broker REST calls block (BrokerHttp uses
        // QEventLoop internally per the broker_http_blocking memory).
        auto resp = broker->get_quotes(creds, QVector<QString>(quote_syms.begin(), quote_syms.end()));
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self, resp, instruments, broker_id, underlying, expiry, topic, underlying_sym]() {
                if (!self)
                    return;
                self->in_flight_.remove(topic);

                if (!resp.success || !resp.data.has_value()) {
                    fincept::datahub::DataHub::instance().publish_error(topic, resp.error);
                    emit self->chain_failed(topic, resp.error);
                    return;
                }

                // Build a quick lookup from "EXCH:brsymbol" → BrokerQuote.
                QHash<QString, BrokerQuote> by_sym;
                by_sym.reserve(resp.data->size());
                for (const auto& q : *resp.data) {
                    by_sym.insert(q.symbol, q);
                }

                // Pull underlying spot
                double spot = 0;
                if (auto it = by_sym.find(underlying_sym); it != by_sym.end())
                    spot = it->ltp;
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
                        const QString k = sit->ce.exchange + ":" + sit->ce.brsymbol;
                        if (auto qit = by_sym.find(k); qit != by_sym.end())
                            row.ce_quote = *qit;
                    }
                    if (sit->has_pe) {
                        row.pe_token = sit->pe.instrument_token;
                        row.pe_symbol = sit->pe.symbol;
                        if (row.lot_size == 0)
                            row.lot_size = sit->pe.lot_size;
                        const QString k = sit->pe.exchange + ":" + sit->pe.brsymbol;
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

    const double r = risk_free_rate();
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
