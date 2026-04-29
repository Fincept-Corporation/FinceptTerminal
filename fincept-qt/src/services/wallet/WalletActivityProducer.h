#pragma once

#include "datahub/Producer.h"
#include "services/wallet/WalletTypes.h"

#include <QObject>
#include <QString>

class QNetworkAccessManager;

namespace fincept::wallet {

class SolanaRpcClient;

/// Producer for `wallet:activity:<pubkey>` — last 50 parsed wallet
/// operations. Phase 2 §2B.
///
/// Source priority:
///   - **Helius parsed-tx** (`api.helius.xyz/v0/addresses/<pk>/transactions`)
///     when `solana.helius_api_key` is set in SecureStorage. Helius
///     classifies SWAP / RECEIVE / SEND directly and resolves token symbols
///     server-side.
///   - **Fallback**: `SolanaRpcClient::get_signatures_for_address` returns
///     raw signatures + slot + block_time. `kind` defaults to `Other`,
///     `asset`/`amount_ui` are empty. The ActivityTab shows a footer hint
///     "Add a Helius key for parsed activity".
///
/// The producer never blocks: each refresh fires one HTTP call (or one RPC
/// call in fallback mode). Topics that haven't published yet show empty in
/// the table; subscribers see the first publish on the next 30 s tick.
class WalletActivityProducer : public QObject, public fincept::datahub::Producer {
    Q_OBJECT
  public:
    explicit WalletActivityProducer(QObject* parent = nullptr);
    ~WalletActivityProducer() override;

    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override { return 4; }

  private:
    /// Extract the pubkey from `wallet:activity:<pubkey>`. Empty if invalid.
    static QString pubkey_from_topic(const QString& topic);

    /// Helius parsed-tx path. Calls `api.helius.xyz/v0/addresses/<pk>/transactions`
    /// and publishes a vector of fully-classified activities.
    void refresh_via_helius(const QString& topic, const QString& pubkey,
                            const QString& helius_key);

    /// Fallback path. Uses our existing RPC wrapper to fetch raw signatures.
    /// Each row carries `kind=Other`, the signature, and a unix block_time.
    void refresh_via_rpc(const QString& topic, const QString& pubkey);

    QNetworkAccessManager* nam_ = nullptr;
    SolanaRpcClient* rpc_ = nullptr;
};

} // namespace fincept::wallet
