// src/services/equity/SecFilingsService.h
#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QVector>

#include <functional>

namespace fincept::services::equity {

/// One section of the Equity Research FILINGS tab — each is one
/// `mcp/edgar/main.py research_*` command (the script tree the MCP EDGAR tools use).
enum class SecSection { Filings, Financials, Insiders, Holders };

/// A loaded section. `data` is the script's "data" object, untouched.
struct SecSectionData {
    QString symbol;
    SecSection section = SecSection::Filings;
    QJsonObject data;
    qint64 fetched_at_ms = 0; ///< when SEC/Yahoo actually served it (not when the cache was read)
    bool stale = false;       ///< last-known-good copy, replayed while a refresh is in flight
};

/// SEC EDGAR data for the Equity Research FILINGS tab and the MCP EDGAR tools.
///
/// Everything runs `mcp/edgar/main.py` through PythonRunner (async, never on the GUI
/// thread's critical path), is cached in CacheManager, and identical requests that
/// arrive while one is running share its result instead of spawning another process.
/// One-shot per symbol (filings change a few times a quarter), so — like
/// EquityResearchService — results are signals/callbacks, not DataHub topics.
class SecFilingsService : public QObject {
    Q_OBJECT
  public:
    static SecFilingsService& instance();

    /// SEC EDGAR only covers issuers registered in the US. False — with a user-facing
    /// reason — for exchange-suffixed yfinance tickers (RELIANCE.NS, VOD.L), indices
    /// (^GSPC), FX / futures (EURUSD=X, ES=F) and crypto pairs (BTC-USD). US-listed
    /// ADRs (TSM, BABA) pass: they file 20-F / 6-K with the SEC.
    static bool is_us_listing(const QString& symbol, QString* reason = nullptr);

    /// Load one FILINGS-tab section for `symbol`. A fresh cached copy is emitted
    /// synchronously; otherwise a last-known-good copy (stale = true) is emitted at once
    /// and a refresh runs. `force` (user REFRESH) skips the fresh cache.
    /// Emits section_loaded / section_failed.
    void fetch(const QString& symbol, SecSection section, bool force = false);

    /// Generic cached runner for `mcp/edgar/main.py <argv…>` (MCP EDGAR tools). `done`
    /// runs on this object's thread with the script's full JSON object on success.
    using CommandCallback = std::function<void(bool ok, const QJsonObject& payload, const QString& error)>;
    void run_command(const QStringList& argv, int ttl_sec, CommandCallback done);

  signals:
    void section_loaded(fincept::services::equity::SecSectionData data);
    void section_failed(QString symbol, fincept::services::equity::SecSection section, QString error);

  private:
    explicit SecFilingsService(QObject* parent = nullptr);
    Q_DISABLE_COPY(SecFilingsService)

    /// Start (or join) the process for `key`; every waiter gets the same result.
    void run_shared(const QString& key, const QStringList& argv, CommandCallback done);

    QHash<QString, QVector<CommandCallback>> waiters_; ///< in-flight requests by cache key
};

} // namespace fincept::services::equity
