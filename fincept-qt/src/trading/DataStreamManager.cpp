#include "trading/DataStreamManager.h"

#include "core/logging/Logger.h"
#include "trading/AccountManager.h"

namespace fincept::trading {

namespace {
constexpr const char* DSM_TAG = "DataStreamManager";
}

DataStreamManager& DataStreamManager::instance() {
    static DataStreamManager s;
    return s;
}

DataStreamManager::DataStreamManager() = default;

// ── Stream lifecycle ────────────────────────────────────────────────────────

AccountDataStream* DataStreamManager::stream_for(const QString& account_id) {
    auto it = streams_.find(account_id);
    if (it != streams_.end())
        return it.value();
    return nullptr;
}

void DataStreamManager::start_stream(const QString& account_id) {
    if (streams_.contains(account_id)) {
        // Already exists — just resume
        streams_[account_id]->resume();
        return;
    }

    // Verify account exists
    if (!AccountManager::instance().has_account(account_id)) {
        LOG_WARN(DSM_TAG, QString("Cannot start stream: account %1 not found").arg(account_id));
        return;
    }

    auto* stream = new AccountDataStream(account_id, this);
    wire_stream_signals(stream);
    streams_.insert(account_id, stream);
    stream->start();

    LOG_INFO(DSM_TAG, QString("Started data stream for account %1").arg(account_id));
}

void DataStreamManager::stop_stream(const QString& account_id) {
    auto it = streams_.find(account_id);
    if (it == streams_.end())
        return;

    it.value()->stop();
    it.value()->deleteLater();
    streams_.erase(it);

    LOG_INFO(DSM_TAG, QString("Stopped data stream for account %1").arg(account_id));
}

void DataStreamManager::start_all_active() {
    const auto accounts = AccountManager::instance().active_accounts();
    for (const auto& account : accounts) {
        if (!streams_.contains(account.account_id))
            start_stream(account.account_id);
    }
    LOG_INFO(DSM_TAG, QString("Started %1 data streams for active accounts").arg(streams_.size()));
}

void DataStreamManager::stop_all() {
    for (auto it = streams_.begin(); it != streams_.end(); ++it) {
        it.value()->stop();
        it.value()->deleteLater();
    }
    streams_.clear();
    LOG_INFO(DSM_TAG, "All data streams stopped");
}

void DataStreamManager::pause_all() {
    for (auto* stream : streams_)
        stream->pause();
}

void DataStreamManager::resume_all() {
    for (auto* stream : streams_)
        stream->resume();
}

// ── Query ───────────────────────────────────────────────────────────────────

bool DataStreamManager::has_stream(const QString& account_id) const {
    return streams_.contains(account_id);
}

QStringList DataStreamManager::active_stream_ids() const {
    return QStringList(streams_.keys().begin(), streams_.keys().end());
}

// ── Signal wiring ───────────────────────────────────────────────────────────

void DataStreamManager::wire_stream_signals(AccountDataStream* stream) {
    // Forward all per-stream signals to aggregated manager signals
    connect(stream, &AccountDataStream::quote_updated,
            this, &DataStreamManager::quote_updated);
    connect(stream, &AccountDataStream::watchlist_updated,
            this, &DataStreamManager::watchlist_updated);
    connect(stream, &AccountDataStream::positions_updated,
            this, &DataStreamManager::positions_updated);
    connect(stream, &AccountDataStream::holdings_updated,
            this, &DataStreamManager::holdings_updated);
    connect(stream, &AccountDataStream::orders_updated,
            this, &DataStreamManager::orders_updated);
    connect(stream, &AccountDataStream::funds_updated,
            this, &DataStreamManager::funds_updated);
    connect(stream, &AccountDataStream::candles_fetched,
            this, &DataStreamManager::candles_fetched);
    connect(stream, &AccountDataStream::orderbook_fetched,
            this, &DataStreamManager::orderbook_fetched);
    connect(stream, &AccountDataStream::time_sales_fetched,
            this, &DataStreamManager::time_sales_fetched);
    connect(stream, &AccountDataStream::latest_trade_fetched,
            this, &DataStreamManager::latest_trade_fetched);
    connect(stream, &AccountDataStream::calendar_fetched,
            this, &DataStreamManager::calendar_fetched);
    connect(stream, &AccountDataStream::clock_fetched,
            this, &DataStreamManager::clock_fetched);
    connect(stream, &AccountDataStream::connection_state_changed,
            this, &DataStreamManager::connection_state_changed);
    connect(stream, &AccountDataStream::token_expired,
            this, &DataStreamManager::token_expired);
}

} // namespace fincept::trading
