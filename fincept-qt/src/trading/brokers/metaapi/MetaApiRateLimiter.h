#pragma once

#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>

namespace fincept::trading {

namespace metaapi_credits {
inline constexpr int kTrade = 10;
inline constexpr int kRead = 50;
inline constexpr int kHistoryRange = 75;
inline constexpr int kSymbolList = 500;
inline constexpr int kWindowCredits = 5000;
inline constexpr int kWindowSeconds = 10;
} // namespace metaapi_credits

class MetaApiRateLimiter {
  public:
    static MetaApiRateLimiter& instance() {
        static MetaApiRateLimiter s;
        return s;
    }

    bool try_consume(const QString& account_id, int credits) {
        QMutexLocker lock(&mutex_);
        auto& b = buckets_[account_id];
        refill(b);
        if (b.remaining < credits)
            return false;
        b.remaining -= credits;
        return true;
    }

    void reset(const QString& account_id) {
        QMutexLocker lock(&mutex_);
        buckets_.remove(account_id);
    }

  private:
    MetaApiRateLimiter() = default;

    struct Bucket {
        int remaining = metaapi_credits::kWindowCredits;
        int64_t window_start = 0;
    };

    void refill(Bucket& b) {
        const int64_t now = QDateTime::currentSecsSinceEpoch();
        if (now - b.window_start >= metaapi_credits::kWindowSeconds) {
            b.remaining = metaapi_credits::kWindowCredits;
            b.window_start = now;
        }
    }

    QHash<QString, Bucket> buckets_;
    QMutex mutex_;
};

} // namespace fincept::trading
