#pragma once
// OrderThreadPool.h — dedicated executors for broker order traffic.
//
// Order dispatch used to share QThreadPool::globalInstance() with ~150 other
// QtConcurrent::run sites, several of which sleep or block for seconds to
// minutes (scale-in tranches, post-placement status polls, basket/split
// spacing). A burst of those could leave a user's BUY / CANCEL / SQUARE-OFF
// queued behind unrelated work.
//
//   order_thread_pool()     latency-critical, single-shot order dispatch:
//                           place / modify / cancel / close / square-off /
//                           cancel-all / close-all / broadcast. Never use it for
//                           anything that sleeps or polls by design. Its threads
//                           never expire, so BrokerHttp's per-thread
//                           QNetworkAccessManager (and its warm TLS connections)
//                           survives idle periods — the first order after a lull
//                           does not pay a fresh handshake.
//
//   blocking_thread_pool()  long-running order jobs that sleep or wait by
//                           design (basket / split batches, scale-in tranches,
//                           post-placement rejection polls). Kept off both the
//                           global pool and the order pool so they starve
//                           neither.
//
// Both pools are created on first use and deliberately leaked: they must outlive
// every singleton their jobs call into, and a static QThreadPool would block
// process exit waiting for an in-flight broker call.

class QThreadPool;

namespace fincept::trading {

/// Executor for latency-critical order dispatch. 4 threads, never expire.
QThreadPool& order_thread_pool();

/// Executor for order jobs that sleep / poll by design. Up to 16 threads.
QThreadPool& blocking_thread_pool();

} // namespace fincept::trading
