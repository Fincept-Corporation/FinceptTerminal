#pragma once
// CurveDataService — free rates and curves for every tab that needs them (cross-tab
// package X6): the U.S. Treasury daily par yield curve (keyless, public domain) and
// futures contract-month curves (Yahoo Finance, delayed), both fetched by
// scripts/derivatives/deriv_market.py through PythonRunner.
//
//   * Nothing blocks the GUI thread: the script runs as a child process with a time-out
//     and a cancel token, its reply is parsed on a worker; callbacks arrive on the GUI
//     thread and are dropped when their context object is gone or the request was
//     cancelled.
//   * Treasury curve: kept in memory for 6 hours and saved to <cache>/derivatives/;
//     when a refresh fails the last saved curve is returned, flagged stale, with the
//     reason. treasury_now() / rate_for() read it synchronously for rate prefills.
//   * Futures curves: kept in memory for 5 minutes per root.

#include "services/options/DerivMarketData.h"

#include <QHash>
#include <QObject>
#include <QPointer>

#include <atomic>
#include <functional>
#include <memory>

namespace fincept::services::options {

class CurveDataService : public QObject {
    Q_OBJECT
  public:
    using TreasuryPtr = std::shared_ptr<const market::TreasuryCurve>;
    using FuturesPtr = std::shared_ptr<const market::FuturesCurve>;

    struct TreasuryReply {
        TreasuryPtr curve;   ///< null when nothing could be loaded
        bool stale = false;  ///< the saved copy, because the refresh failed
        QString message;     ///< why the refresh failed (plain sentence), else empty
    };
    struct FuturesReply {
        FuturesPtr curve;
        QString message;
    };
    using TreasuryCallback = std::function<void(const TreasuryReply&)>;
    using FuturesCallback = std::function<void(const FuturesReply&)>;

    static CurveDataService& instance();

    /// The Treasury curve: from memory when younger than 6 h (and not `force`), else
    /// fetched. Returns a request id for cancel().
    quint64 treasury_curve(bool force, QObject* ctx, TreasuryCallback cb);
    /// Memory copy, else the saved copy (read once, ~1 KB), else null. GUI thread.
    TreasuryPtr treasury_now();
    /// Continuous risk-free rate for `years` from the bootstrapped Treasury curve.
    /// `label` gets "UST 3M 4.34 % (16 Nov)"-style text; `fallback` when no curve.
    double rate_for(double years, double fallback, QString* label = nullptr);
    /// Zero curve of treasury_now() (empty when none).
    rates::ZeroCurve zero_curve_now();

    quint64 futures_curve(const QString& root, int months, bool force, QObject* ctx, FuturesCallback cb);

    void cancel(quint64 request_id);

  signals:
    /// A fresh Treasury curve arrived.
    void treasury_updated();

  private:
    CurveDataService();
    struct Pending {
        std::shared_ptr<std::atomic<bool>> cancel;
    };
    QHash<quint64, Pending> pending_;
    quint64 next_id_ = 0;
    TreasuryPtr treasury_;
    QDateTime treasury_loaded_at_;
    bool disk_checked_ = false;
    struct FuturesEntry {
        FuturesPtr curve;
        QDateTime at;
    };
    QHash<QString, FuturesEntry> futures_;
};

} // namespace fincept::services::options
