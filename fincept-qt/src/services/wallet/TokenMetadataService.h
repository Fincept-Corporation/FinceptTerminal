#pragma once

#include "services/wallet/WalletTypes.h"

#include <QHash>
#include <QObject>
#include <QReadWriteLock>
#include <QString>
#include <optional>

class QNetworkAccessManager;

namespace fincept::wallet {

/// Singleton lookup service for SPL token metadata (symbol, name, decimals,
/// icon, verified flag).
///
/// Why not a Producer: callers need a synchronous lookup keyed by mint —
/// "given this mint, what's the symbol?" — which doesn't fit DataHub's
/// pub/sub shape. Instead this is a read-mostly cache with one slow
/// background refresh per day, persisted to SecureStorage so cold starts
/// don't block on a 1 MB download.
///
/// Source: Jupiter's verified-tagged token list at
/// `https://api.jup.ag/tokens/v2/tagged/verified`. Anything not in the
/// list is treated as unverified and falls back to a truncated mint as
/// the symbol.
class TokenMetadataService : public QObject {
    Q_OBJECT
  public:
    static TokenMetadataService& instance();

    /// Restore the cache from SecureStorage. Cheap (<5 ms even at 1 MB).
    /// Idempotent. Called once from `main()` before WalletService boots.
    void load_from_storage();

    /// Async background fetch from Jupiter. Refreshes the cache and
    /// re-persists. Fires `metadata_changed()` on success. Idempotent on
    /// failure (we keep whatever we had).
    void refresh_from_jupiter_async();

    /// Synchronous lookup. Returns metadata if known, nullopt otherwise.
    /// Thread-safe (read lock).
    std::optional<TokenMetadata> lookup(const QString& mint) const;

    /// Convenience: is this mint in the verified list?
    bool is_verified(const QString& mint) const;

    /// Number of cached tokens (for diagnostics).
    int size() const;

  signals:
    /// Fired after a successful refresh. UI should re-render any rows that
    /// previously showed truncated mints.
    void metadata_changed();

  private:
    explicit TokenMetadataService(QObject* parent = nullptr);
    ~TokenMetadataService() override;
    TokenMetadataService(const TokenMetadataService&) = delete;
    TokenMetadataService& operator=(const TokenMetadataService&) = delete;

    void persist_to_storage();
    bool cache_is_stale() const; ///< true if last refresh > 24h ago or never

    mutable QReadWriteLock lock_;
    QHash<QString, TokenMetadata> cache_;
    qint64 last_refreshed_ms_ = 0;
    QNetworkAccessManager* nam_ = nullptr;
    bool refresh_in_flight_ = false;
};

} // namespace fincept::wallet
