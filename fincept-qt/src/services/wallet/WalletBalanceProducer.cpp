#include "services/wallet/WalletBalanceProducer.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/TopicPolicy.h"
#include "network/websocket/WebSocketClient.h"
#include "services/wallet/SolanaRpcClient.h"
#include "services/wallet/TokenMetadataService.h"
#include "services/wallet/WalletTypes.h"
#include "storage/secure/SecureStorage.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QPointer>
#include <QTimer>
#include <QVariant>

#include <algorithm>

namespace fincept::wallet {

namespace {

// $FNCPT mint is the canonical inline constexpr in WalletTypes.h — re-using it
// here avoids a unity-build ambiguity when this TU and that header collide.
constexpr const char* kTopicPrefix = "wallet:balance:";
constexpr const char* kModeSettingsKey = "wallet.balance_mode";
constexpr int kFncptHeartbeatMs = 30 * 1000; // re-poll FNCPT in stream mode

quint64 parse_uint64(const QJsonValue& v) {
    if (v.isDouble()) {
        return static_cast<quint64>(v.toDouble());
    }
    if (v.isString()) {
        bool ok = false;
        const auto n = v.toString().toULongLong(&ok);
        return ok ? n : 0;
    }
    return 0;
}

} // namespace

WalletBalanceProducer::WalletBalanceProducer(QObject* parent) : QObject(parent) {
    rpc_ = new SolanaRpcClient(this);
    reload_mode();
}

WalletBalanceProducer::~WalletBalanceProducer() {
    stop_all_streams();
}

QStringList WalletBalanceProducer::topic_patterns() const {
    return QStringList{QStringLiteral("wallet:balance:*")};
}

QString WalletBalanceProducer::pubkey_from_topic(const QString& topic) const {
    return topic.mid(static_cast<int>(qstrlen(kTopicPrefix)));
}

QString WalletBalanceProducer::topic_for_pubkey(const QString& pubkey) const {
    return QString::fromLatin1(kTopicPrefix) + pubkey;
}

bool WalletBalanceProducer::endpoint_supports_streaming() const {
    if (!rpc_) return false;
    const auto http = rpc_->http_endpoint();
    // The public mainnet endpoint advertises wss:// but is rate-limited so
    // aggressively that accountSubscribe is unreliable in practice. Force
    // the caller to fall back to polling for that case.
    if (http.contains(QStringLiteral("api.mainnet-beta.solana.com"),
                      Qt::CaseInsensitive)) {
        return false;
    }
    const auto ws = rpc_->ws_endpoint();
    return !ws.isEmpty() && (ws.startsWith(QStringLiteral("ws://"))
                             || ws.startsWith(QStringLiteral("wss://")));
}

void WalletBalanceProducer::force_refresh(const QString& pubkey) {
    if (pubkey.isEmpty()) {
        return;
    }
    if (mode_ == BalanceMode::Poll) {
        refresh_one_poll(topic_for_pubkey(pubkey));
        return;
    }
    // Stream mode — re-run the seed so SOL + FNCPT both refresh now.
    if (streams_.contains(pubkey)) {
        seed_stream_state(pubkey);
    } else {
        // No active session yet; spin one up. start_stream_for() seeds.
        start_stream_for(pubkey);
    }
}

// ── Mode switching ─────────────────────────────────────────────────────────

void WalletBalanceProducer::reload_mode() {
    auto r = SecureStorage::instance().retrieve(QString::fromLatin1(kModeSettingsKey));
    BalanceMode wanted = BalanceMode::Poll;
    if (r.is_ok() && r.value().compare(QStringLiteral("stream"), Qt::CaseInsensitive) == 0) {
        wanted = BalanceMode::Stream;
    }
    if (wanted != mode_) {
        set_mode(wanted);
    }
}

void WalletBalanceProducer::set_mode(BalanceMode mode) {
    if (mode == mode_) {
        return;
    }
    LOG_INFO("WalletBalanceProducer",
             QStringLiteral("mode switch: %1")
                 .arg(mode == BalanceMode::Stream ? "stream" : "poll"));
    mode_ = mode;

    auto& hub = fincept::datahub::DataHub::instance();
    fincept::datahub::TopicPolicy policy;
    if (mode_ == BalanceMode::Stream) {
        policy.push_only = true;
        policy.coalesce_within_ms = 250;
        policy.refresh_timeout_ms = 30 * 1000;
    } else {
        policy.ttl_ms = 30 * 1000;
        policy.min_interval_ms = 10 * 1000;
        policy.refresh_timeout_ms = 15 * 1000;
        policy.push_only = false;
    }
    hub.set_policy_pattern(QStringLiteral("wallet:balance:*"), policy);

    if (mode_ == BalanceMode::Poll) {
        // Tear down any active streams; the hub scheduler will resume polling.
        stop_all_streams();
    }
}

// ── Hub entry point ────────────────────────────────────────────────────────

void WalletBalanceProducer::refresh(const QStringList& topics) {
    for (const auto& topic : topics) {
        if (mode_ == BalanceMode::Poll) {
            refresh_one_poll(topic);
        } else {
            const auto pubkey = pubkey_from_topic(topic);
            if (pubkey.isEmpty()) {
                continue;
            }
            // Stream mode is push-only; the hub still calls refresh() once
            // when the first subscriber attaches. Use that opportunity to
            // ensure the WS session is up.
            if (!streams_.contains(pubkey)) {
                start_stream_for(pubkey);
            } else if (streams_.value(pubkey)->seeded) {
                // Re-publish last cached value so a freshly subscribing
                // consumer sees current data immediately.
                fincept::datahub::DataHub::instance().publish(
                    topic, QVariant::fromValue(streams_.value(pubkey)->latest));
            }
        }
    }
}

// ── Polling path ───────────────────────────────────────────────────────────

void WalletBalanceProducer::refresh_one_poll(const QString& topic) {
    const QString pubkey = pubkey_from_topic(topic);
    if (pubkey.isEmpty()) {
        fincept::datahub::DataHub::instance().publish_error(topic,
                                                            QStringLiteral("invalid topic"));
        return;
    }
    rpc_->reload_endpoint();

    QPointer<WalletBalanceProducer> self = this;
    auto state = std::make_shared<WalletBalance>();
    state->pubkey_b58 = pubkey;
    state->ts_ms = QDateTime::currentMSecsSinceEpoch();
    auto pending = std::make_shared<int>(2);
    auto fail = std::make_shared<bool>(false);

    auto try_publish = [self, topic, state, pending, fail]() {
        if (!self) {
            return;
        }
        if (--(*pending) > 0) {
            return;
        }
        auto& hub = fincept::datahub::DataHub::instance();
        if (*fail) {
            // partial success: publish what we have anyway so the UI shows
            // last-known-good. publish_error already emitted in the failing
            // branch, so the topic_error signal fires in parallel.
        }
        hub.publish(topic, QVariant::fromValue(*state));
    };

    rpc_->get_sol_balance(pubkey, [self, topic, state, try_publish, fail](Result<quint64> r) {
        if (!self) {
            return;
        }
        if (r.is_err()) {
            *fail = true;
            fincept::datahub::DataHub::instance().publish_error(
                topic, QString::fromStdString(r.error()));
        } else {
            state->sol_lamports = r.value();
        }
        try_publish();
    });

    // Phase 2 §2A.5: fetch every SPL token the wallet holds in one call,
    // then merge with TokenMetadataService for symbol/name/icon. Tokens
    // are sorted at publish time; the price-based sort happens upstream
    // once TokenPriceProducer has cached USD values, so here we only
    // ensure FNCPT and any verified token surface above unverified junk.
    rpc_->get_all_token_balances(
        pubkey,
        [self, topic, state, try_publish, fail](Result<std::vector<SplTokenBalance>> r) {
            if (!self) {
                return;
            }
            if (r.is_err()) {
                *fail = true;
                fincept::datahub::DataHub::instance().publish_error(
                    topic, QString::fromStdString(r.error()));
                try_publish();
                return;
            }
            auto& meta = fincept::wallet::TokenMetadataService::instance();
            for (const auto& b : r.value()) {
                if (b.amount_raw.isEmpty() || b.amount_raw == QStringLiteral("0")) {
                    continue;
                }
                fincept::wallet::TokenHolding h;
                h.mint = b.mint;
                h.amount_raw = b.amount_raw;
                h.decimals = b.decimals;
                if (auto m = meta.lookup(b.mint)) {
                    h.symbol = m->symbol;
                    h.name = m->name;
                    h.icon_url = m->icon_url;
                    h.verified = m->verified;
                } else {
                    // Fallback: truncated mint as the symbol so the row is
                    // still readable. Verified flag stays false.
                    if (b.mint.size() >= 12) {
                        h.symbol = b.mint.left(4) + QStringLiteral("…")
                                   + b.mint.right(4);
                    } else {
                        h.symbol = b.mint;
                    }
                    h.verified = false;
                }
                state->tokens.append(std::move(h));
            }

            // Sort: verified first, then by raw atomic amount as a rough
            // proxy until TokenPriceProducer's cache lets us re-sort by
            // USD value upstream. FNCPT always pinned to position 0 so
            // legacy fncpt_holding() consumers find it cheaply.
            std::sort(state->tokens.begin(), state->tokens.end(),
                      [](const fincept::wallet::TokenHolding& a,
                         const fincept::wallet::TokenHolding& b) {
                          const auto fncpt = QString::fromLatin1(
                              fincept::wallet::kFncptMint);
                          if (a.mint == fncpt) return true;
                          if (b.mint == fncpt) return false;
                          if (a.verified != b.verified) return a.verified;
                          return a.ui_amount() > b.ui_amount();
                      });

            try_publish();
        });
}

// ── Streaming path ─────────────────────────────────────────────────────────

void WalletBalanceProducer::stop_all_streams() {
    for (auto* s : streams_) {
        if (s->fncpt_heartbeat) {
            s->fncpt_heartbeat->stop();
            s->fncpt_heartbeat->deleteLater();
        }
        if (s->ws) {
            s->ws->disconnect();
            s->ws->deleteLater();
        }
        delete s;
    }
    streams_.clear();
}

void WalletBalanceProducer::stop_stream_for(const QString& pubkey) {
    auto* s = streams_.take(pubkey);
    if (!s) {
        return;
    }
    if (s->fncpt_heartbeat) {
        s->fncpt_heartbeat->stop();
        s->fncpt_heartbeat->deleteLater();
    }
    if (s->ws) {
        s->ws->disconnect();
        s->ws->deleteLater();
    }
    delete s;
}

void WalletBalanceProducer::seed_stream_state(const QString& pubkey) {
    rpc_->reload_endpoint();
    QPointer<WalletBalanceProducer> self = this;

    // Step 1: get SOL balance to bootstrap the stat box.
    rpc_->get_sol_balance(pubkey, [self, pubkey](Result<quint64> r) {
        if (!self) {
            return;
        }
        auto it = self->streams_.find(pubkey);
        if (it == self->streams_.end()) {
            return;
        }
        auto* s = it.value();
        if (r.is_ok()) {
            s->latest.sol_lamports = r.value();
        }
        s->latest.pubkey_b58 = pubkey;
        s->latest.ts_ms = QDateTime::currentMSecsSinceEpoch();
        if (s->seeded) {
            fincept::datahub::DataHub::instance().publish(
                self->topic_for_pubkey(pubkey), QVariant::fromValue(s->latest));
        }
    });

    // Step 2: locate the FNCPT token account address + amount.
    rpc_->get_token_balance(
        pubkey, QString::fromLatin1(kFncptMint),
        [self, pubkey](Result<std::vector<SplTokenBalance>> r) {
            if (!self) {
                return;
            }
            auto it = self->streams_.find(pubkey);
            if (it == self->streams_.end()) {
                return;
            }
            auto* s = it.value();
            quint64 total = 0;
            int decimals = 0;
            for (const auto& b : r.is_ok() ? r.value() : std::vector<SplTokenBalance>{}) {
                bool ok = false;
                const auto v = b.amount_raw.toULongLong(&ok);
                if (ok) {
                    total += v;
                }
                decimals = b.decimals;
            }
            // Stage 2A.5: WalletBalance.tokens carries every holding. The
            // streaming path still only refreshes FNCPT; replace the FNCPT
            // entry in `tokens` (or append it) instead of writing the old
            // dedicated fields.
            const auto fncpt_mint = QString::fromLatin1(fincept::wallet::kFncptMint);
            bool replaced = false;
            for (auto& t : s->latest.tokens) {
                if (t.mint == fncpt_mint) {
                    t.amount_raw = QString::number(total);
                    if (decimals > 0) t.decimals = decimals;
                    replaced = true;
                    break;
                }
            }
            if (!replaced) {
                fincept::wallet::TokenHolding t;
                t.mint = fncpt_mint;
                t.symbol = QStringLiteral("$FNCPT");
                t.name = QStringLiteral("FinceptTerminal");
                t.amount_raw = QString::number(total);
                t.decimals = decimals > 0 ? decimals : 6;
                s->latest.tokens.append(std::move(t));
            }
            s->seeded = true;

            // We need the on-chain token-account address to subscribe by
            // account — but our `get_token_balance` doesn't return it.
            // For Phase 1 streaming, we subscribe to the wallet pubkey for
            // SOL only; FNCPT updates fall back to a per-30s poll inside
            // the same producer (driven from the WS heartbeat below). A
            // future enhancement is to surface the token account address
            // via SolanaRpcClient and `accountSubscribe` on it.

            fincept::datahub::DataHub::instance().publish(
                self->topic_for_pubkey(pubkey), QVariant::fromValue(s->latest));
        });
}

void WalletBalanceProducer::start_stream_for(const QString& pubkey) {
    if (streams_.contains(pubkey)) {
        return;
    }
    rpc_->reload_endpoint();

    // Block streaming on RPC endpoints that don't reliably serve
    // accountSubscribe (today: the public mainnet RPC). Surface the reason
    // through the hub so the screen can show it (bug fix #6).
    if (!endpoint_supports_streaming()) {
        LOG_WARN("WalletBalanceProducer",
                 "stream mode unavailable on current RPC — falling back to poll for "
                     + pubkey);
        fincept::datahub::DataHub::instance().publish_error(
            topic_for_pubkey(pubkey),
            QStringLiteral("STREAM unavailable on this RPC — using poll. "
                           "Add a Helius API key in Settings for live updates."));
        refresh_one_poll(topic_for_pubkey(pubkey));
        return;
    }

    const QString ws_url = rpc_->ws_endpoint();

    auto* s = new StreamSession;
    s->pubkey = pubkey;
    s->ws = new WebSocketClient(this);
    streams_.insert(pubkey, s);

    // Per-session FNCPT heartbeat — there's no per-account WS subscription
    // for the FNCPT token account in this phase, so we re-fetch via REST
    // on a slow cadence (bug fix #5).
    s->fncpt_heartbeat = new QTimer(this);
    s->fncpt_heartbeat->setInterval(kFncptHeartbeatMs);
    s->fncpt_heartbeat->setSingleShot(false);
    QPointer<WalletBalanceProducer> hb_self = this;
    QObject::connect(s->fncpt_heartbeat, &QTimer::timeout, this, [hb_self, pubkey]() {
        if (!hb_self) return;
        if (!hb_self->streams_.contains(pubkey)) return;
        hb_self->seed_stream_state(pubkey);
    });
    s->fncpt_heartbeat->start();

    QPointer<WalletBalanceProducer> self = this;

    QObject::connect(s->ws, &WebSocketClient::connected, this, [self, pubkey]() {
        if (!self) {
            return;
        }
        auto it = self->streams_.find(pubkey);
        if (it == self->streams_.end()) {
            return;
        }
        auto* sess = it.value();
        // accountSubscribe(pubkey) — get notified on every lamport change.
        QJsonObject opts;
        opts[QStringLiteral("encoding")] = QStringLiteral("jsonParsed");
        opts[QStringLiteral("commitment")] = QStringLiteral("confirmed");
        QJsonObject req{
            {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
            {QStringLiteral("id"), sess->next_request_id++},
            {QStringLiteral("method"), QStringLiteral("accountSubscribe")},
            {QStringLiteral("params"), QJsonArray{pubkey, opts}}};
        sess->ws->send(QString::fromUtf8(QJsonDocument(req).toJson(QJsonDocument::Compact)));
        LOG_INFO("WalletBalanceProducer", "WS connected, subscribed to " + pubkey);

        // Bootstrap balances with one REST round-trip.
        self->seed_stream_state(pubkey);
    });

    QObject::connect(s->ws, &WebSocketClient::disconnected, this, [self, pubkey]() {
        if (!self) {
            return;
        }
        LOG_WARN("WalletBalanceProducer", "WS disconnected for " + pubkey);
        // The WebSocketClient auto-reconnects with exponential backoff.
    });

    QObject::connect(s->ws, &WebSocketClient::error_occurred, this,
                     [self, pubkey](const QString& err) {
                         if (!self) {
                             return;
                         }
                         LOG_WARN("WalletBalanceProducer",
                                  QStringLiteral("WS error (%1): %2").arg(pubkey).arg(err));
                         fincept::datahub::DataHub::instance().publish_error(
                             self->topic_for_pubkey(pubkey), err);
                     });

    QObject::connect(s->ws, &WebSocketClient::message_received, this,
                     [self, pubkey](const QString& msg) {
                         if (!self) {
                             return;
                         }
                         auto it = self->streams_.find(pubkey);
                         if (it == self->streams_.end()) {
                             return;
                         }
                         auto* sess = it.value();
                         QJsonParseError pe;
                         const auto doc = QJsonDocument::fromJson(msg.toUtf8(), &pe);
                         if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
                             return;
                         }
                         const auto root = doc.object();
                         // Subscription confirmation: { result: <int>, id: ... }
                         if (root.contains(QStringLiteral("id"))) {
                             const auto sub_id = root.value(QStringLiteral("result")).toInt();
                             if (sub_id > 0 && sess->sol_sub_id == 0) {
                                 sess->sol_sub_id = sub_id;
                                 LOG_INFO("WalletBalanceProducer",
                                          QStringLiteral("subscription %1 active for %2")
                                              .arg(sub_id)
                                              .arg(pubkey));
                             }
                             return;
                         }
                         // Notification: { method: "accountNotification",
                         //                 params: { result: { value: { lamports: N, ... } } } }
                         if (root.value(QStringLiteral("method")).toString()
                             != QStringLiteral("accountNotification")) {
                             return;
                         }
                         const auto params = root.value(QStringLiteral("params")).toObject();
                         const auto result = params.value(QStringLiteral("result")).toObject();
                         const auto value = result.value(QStringLiteral("value")).toObject();
                         const auto lamports =
                             parse_uint64(value.value(QStringLiteral("lamports")));
                         sess->latest.pubkey_b58 = pubkey;
                         sess->latest.sol_lamports = lamports;
                         sess->latest.ts_ms = QDateTime::currentMSecsSinceEpoch();
                         fincept::datahub::DataHub::instance().publish(
                             self->topic_for_pubkey(pubkey),
                             QVariant::fromValue(sess->latest));
                     });

    s->ws->connect_to(ws_url);
}

} // namespace fincept::wallet
