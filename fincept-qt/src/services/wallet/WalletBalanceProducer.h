#pragma once

#include "datahub/Producer.h"
#include "services/wallet/WalletTypes.h"

#include <QHash>
#include <QObject>
#include <QString>

class QTimer;

namespace fincept {
class WebSocketClient;
}

namespace fincept::wallet {

class SolanaRpcClient;

/// Refresh strategy for wallet balance data.
///   Poll   — REST refresh on hub TTL (default, simplest).
///   Stream — WebSocket `accountSubscribe`, push-only updates.
enum class BalanceMode {
    Poll,
    Stream,
};

/// Owns the `wallet:balance:<pubkey>` topic family. Two modes:
///
///   Poll:    `refresh()` hits Solana RPC for SOL + FNCPT, publishes a
///            WalletBalance. Hub TTL/min_interval drives cadence.
///
///   Stream:  Per-pubkey WebSocket connection to the RPC's WS endpoint.
///            `accountSubscribe` on the wallet pubkey (SOL) and on the
///            FNCPT associated token account (FNCPT). Push-only — every
///            update is published immediately, hub coalesces 250 ms.
class WalletBalanceProducer : public QObject, public fincept::datahub::Producer {
    Q_OBJECT
  public:
    explicit WalletBalanceProducer(QObject* parent = nullptr);
    ~WalletBalanceProducer() override;

    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override { return 4; }

    /// Set the active refresh strategy. Live — switching while subscribers
    /// exist tears down/spins up streams as needed.
    void set_mode(BalanceMode mode);
    BalanceMode mode() const noexcept { return mode_; }

    /// Reload mode from SecureStorage. Defaults to Poll. Idempotent.
    void reload_mode();

    /// User-driven hard refresh. In Poll mode this is equivalent to
    /// `refresh_one_poll(topic)`. In Stream mode it re-runs the seed (the
    /// only way to refresh FNCPT, which has no per-account WS subscription).
    /// Safe to call when not connected — it's a no-op.
    void force_refresh(const QString& pubkey);

  private:
    // Polling path
    void refresh_one_poll(const QString& topic);

    // Streaming path
    void start_stream_for(const QString& pubkey);
    void stop_stream_for(const QString& pubkey);
    void stop_all_streams();
    void seed_stream_state(const QString& pubkey);
    bool endpoint_supports_streaming() const;

    QString pubkey_from_topic(const QString& topic) const;
    QString topic_for_pubkey(const QString& pubkey) const;

    SolanaRpcClient* rpc_ = nullptr;
    BalanceMode mode_ = BalanceMode::Poll;

    struct StreamSession {
        WebSocketClient* ws = nullptr;
        QString pubkey;
        QString fncpt_token_account;  ///< owner's associated FNCPT token account
        WalletBalance latest;
        int sol_sub_id = 0;
        int fncpt_sub_id = 0;
        int next_request_id = 1;
        bool seeded = false;
        QTimer* fncpt_heartbeat = nullptr; ///< re-polls FNCPT every ~30s
    };
    QHash<QString, StreamSession*> streams_;  ///< keyed by pubkey
};

} // namespace fincept::wallet
