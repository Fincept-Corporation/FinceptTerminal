// ExchangeService — Bridge between Python ccxt and C++ paper trading engine
// Uses QProcess for subprocess management

#include "trading/ExchangeService.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "trading/OrderMatcher.h"
#include "trading/PaperTrading.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMutexLocker>
#include <QThread>
#include <QtConcurrent/QtConcurrent>

// Adapter: PythonRunner uses scripts_dir(), not resolve_script()
namespace {
QString resolve_script_path(const QString& relative) {
    QString dir = fincept::python::PythonRunner::instance().scripts_dir();
    if (dir.isEmpty())
        return {};
    return dir + "/" + relative;
}
} // anonymous namespace

namespace fincept::trading {

static const QString TAG = "ExchangeService";

// ============================================================================
// Singleton
// ============================================================================

ExchangeService& ExchangeService::instance() {
    static ExchangeService s;
    return s;
}

ExchangeService::ExchangeService() {
    feed_timer_ = new QTimer(this);
    connect(feed_timer_, &QTimer::timeout, this, &ExchangeService::poll_prices);

    // Pre-warm daemon in background so it's ready by the time the user opens Crypto Trading.
    // QTimer::singleShot(0) defers until after QApplication::exec() is running so PythonRunner
    // is fully initialised before we spawn the daemon process.
    QTimer::singleShot(0, this, [this]() {
        if (!is_daemon_running())
            start_daemon();
    });
}

ExchangeService::~ExchangeService() {
    stop_ws_stream();
    stop_price_feed();
    stop_daemon();
}

// ============================================================================
// Configuration
// ============================================================================

void ExchangeService::set_exchange(const QString& exchange_id) {
    QMutexLocker lock(&mutex_);
    exchange_id_ = exchange_id;
    LOG_INFO(TAG, "Exchange set to: " + exchange_id);
}

QString ExchangeService::get_exchange() const {
    QMutexLocker lock(&mutex_);
    return exchange_id_;
}

void ExchangeService::set_credentials(const ExchangeCredentials& creds) {
    QMutexLocker lock(&mutex_);
    credentials_ = creds;
    credentials_sent_to_daemon_ = false; // Will be sent on next daemon_call()
}

// ============================================================================
// Watch management
// ============================================================================

void ExchangeService::watch_symbol(const QString& symbol, const QString& portfolio_id) {
    QMutexLocker lock(&mutex_);
    watched_[symbol].insert(portfolio_id);
}

void ExchangeService::unwatch_symbol(const QString& symbol, const QString& portfolio_id) {
    QMutexLocker lock(&mutex_);
    auto it = watched_.find(symbol);
    if (it != watched_.end()) {
        it->remove(portfolio_id);
        if (it->isEmpty())
            watched_.erase(it);
    }
}

// ============================================================================
// Price Feed (polling via QTimer)
// ============================================================================

void ExchangeService::start_price_feed(int interval_seconds) {
    feed_interval_ = interval_seconds;
    feed_running_ = true;
    feed_timer_->start(interval_seconds * 1000);
    LOG_INFO(TAG, QString("Price feed started (interval=%1s)").arg(interval_seconds));
}

void ExchangeService::stop_price_feed() {
    feed_running_ = false;
    if (feed_timer_)
        feed_timer_->stop();
}

bool ExchangeService::is_feed_running() const {
    return feed_running_.load();
}

void ExchangeService::poll_prices() {
    // Guard: skip if a previous poll is still running (avoid piling up threads)
    if (poll_in_progress_.exchange(true))
        return;

    QMutexLocker lock(&mutex_);
    if (watched_.isEmpty()) {
        poll_in_progress_ = false;
        return;
    }

    QStringList symbols;
    // Snapshot watched portfolios per symbol for use after mutex release
    QHash<QString, QSet<QString>> watched_snapshot = watched_;
    for (auto it = watched_snapshot.constBegin(); it != watched_snapshot.constEnd(); ++it)
        symbols.append(it.key());
    QString exchange_id = exchange_id_;
    lock.unlock();

    // Run the blocking fetch_tickers() in a background thread (P1: never block UI)
    QPointer<ExchangeService> self = this;
    QtConcurrent::run([self, symbols, watched_snapshot, exchange_id]() {
        if (!self)
            return;

        // fetch_tickers uses call_script which does blocking QProcess — safe in background thread
        auto tickers = self->fetch_tickers(symbols);

        if (!self)
            return;

        // Process results: update cache and feed OrderMatcher (all outside UI thread)
        // Collect callbacks to invoke on UI thread
        struct TickerResult {
            TickerData ticker;
            QVector<QString> portfolio_ids;
        };
        QVector<TickerResult> results;
        results.reserve(tickers.size());

        for (const auto& ticker : tickers) {
            TickerResult r;
            r.ticker = ticker;

            auto it = watched_snapshot.find(ticker.symbol);
            if (it != watched_snapshot.end()) {
                PriceData pd;
                pd.last = ticker.last;
                pd.bid = ticker.bid;
                pd.ask = ticker.ask;
                pd.high = ticker.high;
                pd.low = ticker.low;
                pd.volume = ticker.base_volume;
                pd.change = ticker.change;
                pd.change_percent = ticker.percentage;
                pd.timestamp = ticker.timestamp;

                for (const auto& portfolio_id : *it) {
                    OrderMatcher::instance().check_orders(ticker.symbol, pd, portfolio_id);
                    OrderMatcher::instance().check_sl_tp_triggers(portfolio_id, ticker.symbol, ticker.last);
                    pt_update_position_price(portfolio_id, ticker.symbol, ticker.last);
                    r.portfolio_ids.append(portfolio_id);
                }
            }
            results.append(std::move(r));
        }

        if (!self)
            return;

        // Post cache update + callback invocation back to UI thread
        QMetaObject::invokeMethod(
            self,
            [self, results]() {
                if (!self)
                    return;

                for (const auto& r : results) {
                    {
                        QMutexLocker lock(&self->mutex_);
                        self->price_cache_[r.ticker.symbol] = r.ticker;
                    }

                    // Copy callbacks out, release mutex, then invoke
                    QVector<PriceUpdateCallback> cbs;
                    {
                        QMutexLocker lock(&self->mutex_);
                        cbs.reserve(self->price_callbacks_.size());
                        for (auto cb_it = self->price_callbacks_.constBegin();
                             cb_it != self->price_callbacks_.constEnd(); ++cb_it)
                            cbs.append(cb_it.value());
                    }
                    for (const auto& cb : cbs) {
                        try {
                            cb(r.ticker.symbol, r.ticker);
                        } catch (...) {
                        }
                    }
                }

                self->poll_in_progress_ = false;
            },
            Qt::QueuedConnection);
    });
}

// ============================================================================
// Callbacks
// ============================================================================

int ExchangeService::on_price_update(PriceUpdateCallback callback) {
    QMutexLocker lock(&mutex_);
    int id = next_callback_id_++;
    price_callbacks_[id] = std::move(callback);
    return id;
}

void ExchangeService::remove_price_callback(int id) {
    QMutexLocker lock(&mutex_);
    price_callbacks_.remove(id);
}

int ExchangeService::on_orderbook_update(OrderBookCallback callback) {
    QMutexLocker lock(&mutex_);
    int id = next_callback_id_++;
    orderbook_callbacks_[id] = std::move(callback);
    return id;
}

void ExchangeService::remove_orderbook_callback(int id) {
    QMutexLocker lock(&mutex_);
    orderbook_callbacks_.remove(id);
}

int ExchangeService::on_candle_update(CandleCallback callback) {
    QMutexLocker lock(&mutex_);
    int id = next_callback_id_++;
    candle_callbacks_[id] = std::move(callback);
    return id;
}

void ExchangeService::remove_candle_callback(int id) {
    QMutexLocker lock(&mutex_);
    candle_callbacks_.remove(id);
}

int ExchangeService::on_trade_update(TradeCallback callback) {
    QMutexLocker lock(&mutex_);
    int id = next_callback_id_++;
    trade_callbacks_[id] = std::move(callback);
    return id;
}

void ExchangeService::remove_trade_callback(int id) {
    QMutexLocker lock(&mutex_);
    trade_callbacks_.remove(id);
}

// ============================================================================
// Persistent Daemon (eliminates 600-1200ms Python startup per request)
// ============================================================================

void ExchangeService::start_daemon() {
    if (daemon_process_ && daemon_process_->state() == QProcess::Running)
        return;

    QString python_path = python::PythonRunner::instance().python_path();
    QString script_path = resolve_script_path("exchange/exchange_daemon.py");

    if (python_path.isEmpty() || script_path.isEmpty()) {
        LOG_WARN(TAG, "Cannot start daemon: Python or script not found");
        return;
    }

    daemon_process_ = new QProcess(this);
    daemon_process_->setProcessChannelMode(QProcess::SeparateChannels);

    connect(daemon_process_, &QProcess::readyReadStandardOutput, this, &ExchangeService::drain_daemon_buffer);
    connect(daemon_process_, &QProcess::readyReadStandardError, this, [this]() {
        if (daemon_process_) {
            QString err = QString::fromUtf8(daemon_process_->readAllStandardError()).trimmed();
            if (!err.isEmpty())
                LOG_WARN(TAG, "Daemon stderr: " + err);
        }
    });
    connect(daemon_process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus status) {
                LOG_WARN(
                    TAG,
                    QString("Daemon exited (code=%1, status=%2), will restart on next call").arg(code).arg(status));
                daemon_ready_ = false;
                if (daemon_process_) {
                    daemon_process_->deleteLater();
                    daemon_process_ = nullptr;
                }
            });

    QStringList args;
    args << "-u" << "-B" << script_path;
    daemon_process_->start(python_path, args);
    LOG_INFO(TAG, "Exchange daemon starting...");
}

void ExchangeService::stop_daemon() {
    if (daemon_process_) {
        daemon_ready_ = false;
        daemon_process_->closeWriteChannel();
        // Give it a moment to exit cleanly, then force kill
        auto* proc = daemon_process_;
        daemon_process_ = nullptr;
        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), proc, &QObject::deleteLater);
        QTimer::singleShot(2000, proc, [proc]() {
            if (proc->state() != QProcess::NotRunning)
                proc->kill();
        });
    }
}

bool ExchangeService::is_daemon_running() const {
    return daemon_process_ && daemon_process_->state() == QProcess::Running && daemon_ready_.load();
}

void ExchangeService::drain_daemon_buffer() {
    if (!daemon_process_)
        return;
    while (daemon_process_->canReadLine()) {
        QString line = QString::fromUtf8(daemon_process_->readLine()).trimmed();
        if (line.isEmpty() || line[0] != '{')
            continue;

        QJsonParseError err;
        auto doc = QJsonDocument::fromJson(line.toUtf8(), &err);
        if (err.error != QJsonParseError::NoError)
            continue;

        auto obj = doc.object();
        QString id = obj.value("id").toString();

        // Handle the init response
        if (id == "__init__") {
            daemon_ready_ = true;
            LOG_INFO(TAG, "Exchange daemon ready (pid=" +
                              obj.value("data").toObject().value("pid").toVariant().toString() + ")");
            // Send credentials if we have them
            if (!credentials_.api_key.isEmpty()) {
                QJsonObject creds;
                creds["api_key"] = credentials_.api_key;
                creds["secret"] = credentials_.secret;
                if (!credentials_.password.isEmpty())
                    creds["password"] = credentials_.password;

                QJsonObject req;
                req["id"] = "__creds__";
                req["method"] = "set_credentials";
                req["exchange"] = exchange_id_;
                req["args"] = creds;
                QByteArray data = QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n";
                daemon_process_->write(data);
                credentials_sent_to_daemon_ = true;
            }
            // Wake any thread waiting for daemon readiness
            daemon_response_ready_.wakeAll();
            continue;
        }

        // Store response and wake waiting thread
        {
            QMutexLocker lock(&daemon_mutex_);
            daemon_responses_[id] = obj;
        }
        daemon_response_ready_.wakeAll();
    }
}

QJsonObject ExchangeService::daemon_call(const QString& method, const QJsonObject& args, int timeout_ms) {
    // daemon_call() is called from background threads (QtConcurrent::run).
    // The daemon process lives on the main thread, so all writes must go
    // through invokeMethod. Responses arrive via drain_daemon_buffer() on
    // the main thread and are stored in daemon_responses_ under mutex.
    // We use QWaitCondition to sleep until our response arrives.

    // Auto-start daemon if not running (must happen on main thread)
    if (!is_daemon_running()) {
        QMetaObject::invokeMethod(this, [this]() { start_daemon(); }, Qt::BlockingQueuedConnection);
        // Wait for daemon_ready_ via wait condition
        {
            QMutexLocker lock(&daemon_mutex_);
            QElapsedTimer wait;
            wait.start();
            while (!daemon_ready_.load() && wait.elapsed() < 5000) {
                daemon_response_ready_.wait(&daemon_mutex_, 100);
            }
        }
        if (!daemon_ready_.load()) {
            LOG_WARN(TAG, "Daemon not ready after 5s, falling back to script");
            return {{"error", "Daemon not ready"}};
        }
    }

    // Generate unique request ID (atomic, safe from any thread)
    QString req_id = QString("r_%1").arg(daemon_req_id_.fetch_add(1));

    // Build request JSON
    QJsonObject req;
    req["id"] = req_id;
    req["method"] = method;
    req["exchange"] = exchange_id_;
    req["args"] = args;
    QByteArray data = QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n";

    // Send credentials if needed, then send the request — both on main thread
    QMetaObject::invokeMethod(
        this,
        [this, data]() {
            if (!daemon_process_)
                return;
            // Send credentials if not yet sent
            if (!credentials_sent_to_daemon_ && !credentials_.api_key.isEmpty()) {
                QJsonObject creds;
                creds["api_key"] = credentials_.api_key;
                creds["secret"] = credentials_.secret;
                if (!credentials_.password.isEmpty())
                    creds["password"] = credentials_.password;

                QJsonObject cred_req;
                cred_req["id"] = "__creds__";
                cred_req["method"] = "set_credentials";
                cred_req["exchange"] = exchange_id_;
                cred_req["args"] = creds;
                daemon_process_->write(QJsonDocument(cred_req).toJson(QJsonDocument::Compact) + "\n");
                credentials_sent_to_daemon_ = true;
            }
            daemon_process_->write(data);
        },
        Qt::BlockingQueuedConnection);

    // Wait for response using QWaitCondition (proper cross-thread signaling)
    QMutexLocker lock(&daemon_mutex_);
    QElapsedTimer elapsed;
    elapsed.start();
    while (elapsed.elapsed() < timeout_ms) {
        auto it = daemon_responses_.find(req_id);
        if (it != daemon_responses_.end()) {
            QJsonObject resp = it.value();
            daemon_responses_.erase(it);
            lock.unlock();

            // Unwrap: {"id":"...", "success":true, "data":{...}}
            if (resp.value("success").toBool(false) && resp.contains("data")) {
                auto d = resp.value("data");
                if (d.isObject())
                    return d.toObject();
            }
            if (resp.contains("error"))
                return resp;
            return resp;
        }
        // Sleep until signaled or 50ms timeout (re-check elapsed)
        daemon_response_ready_.wait(&daemon_mutex_, 50);
    }
    lock.unlock();

    LOG_WARN(TAG, "Daemon call timed out: " + method);
    return {{"error", "Daemon call timed out: " + method}};
}

// ============================================================================
// WebSocket Stream (QProcess-based)
// ============================================================================

void ExchangeService::start_ws_stream(const QString& primary_symbol, const QStringList& all_symbols) {
    stop_ws_stream();

    ws_primary_symbol_ = primary_symbol;
    ws_all_symbols_ = all_symbols;

    // Find Python and script paths via PythonRunner
    QString python_path = python::PythonRunner::instance().python_path();
    QString script_path = resolve_script_path("exchange/ws_stream.py");

    if (python_path.isEmpty() || script_path.isEmpty()) {
        LOG_ERROR(TAG, "Python or ws_stream.py not found");
        return;
    }

    QStringList args;
    args << "-u" << "-B" << script_path << exchange_id_;
    for (const auto& sym : all_symbols)
        args << sym;

    ws_process_ = new QProcess(this);
    // Throttle: process max 8 lines per event loop cycle via drain_ws_buffer()
    // This prevents 50+ synchronous JSON parses from starving the event loop
    connect(ws_process_, &QProcess::readyReadStandardOutput, this, &ExchangeService::drain_ws_buffer);
    connect(ws_process_, &QProcess::readyReadStandardError, this, [this]() {
        QString err = QString::fromUtf8(ws_process_->readAllStandardError()).trimmed();
        if (!err.isEmpty())
            LOG_WARN(TAG, "WS stderr: " + err);
    });

    ws_process_->start(python_path, args);
    LOG_INFO(TAG, "WS stream started for " + primary_symbol);
}

void ExchangeService::stop_ws_stream() {
    if (ws_process_) {
        auto* proc = ws_process_;
        ws_process_ = nullptr;
        ws_connected_ = false;

        // Disconnect all signals BEFORE cleanup to prevent stale reads
        proc->disconnect(this);

        proc->terminate();
        // Non-blocking cleanup: kill after timeout if still running, then deleteLater
        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), proc, &QObject::deleteLater);
        // If terminate doesn't work within 3s, force kill
        QTimer::singleShot(3000, proc, [proc]() {
            if (proc->state() != QProcess::NotRunning) {
                proc->kill();
            }
        });
    }
    ws_connected_ = false;
}

bool ExchangeService::is_ws_connected() const {
    return ws_connected_.load();
}

void ExchangeService::set_ws_primary_symbol(const QString& symbol) {
    QMutexLocker lock(&mutex_);
    ws_primary_symbol_ = symbol;
}

QString ExchangeService::get_ws_primary_symbol() const {
    QMutexLocker lock(&mutex_);
    return ws_primary_symbol_;
}

void ExchangeService::handle_ws_line(const QString& line) {
    if (line.isEmpty() || line[0] != '{')
        return;

    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(line.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError)
        return;

    auto msg = doc.object();
    QString type = msg.value("type").toString();

    if (type == "status") {
        ws_connected_ = msg.value("connected").toBool(false);
        return;
    }

    if (type == "ticker") {
        // ws_stream.py emits snake_case fields; REST parse_ticker uses camelCase
        QJsonObject normalised = msg;
        if (!msg.contains("baseVolume") && msg.contains("base_volume"))
            normalised["baseVolume"] = msg.value("base_volume");
        if (!msg.contains("quoteVolume") && msg.contains("quote_volume"))
            normalised["quoteVolume"] = msg.value("quote_volume");

        TickerData ticker = parse_ticker(normalised);
        if (ticker.symbol.isEmpty() || ticker.last <= 0.0)
            return;

        // Copy callbacks out, then release mutex BEFORE firing
        QVector<PriceUpdateCallback> cbs;
        {
            QMutexLocker lock(&mutex_);
            price_cache_[ticker.symbol] = ticker;
            cbs.reserve(price_callbacks_.size());
            for (auto it = price_callbacks_.constBegin(); it != price_callbacks_.constEnd(); ++it)
                cbs.append(it.value());
        }
        for (const auto& cb : cbs) {
            try {
                cb(ticker.symbol, ticker);
            } catch (...) {
            }
        }
        return;
    }

    if (type == "orderbook") {
        OrderBookData ob = parse_orderbook(msg);
        QVector<OrderBookCallback> cbs;
        {
            QMutexLocker lock(&mutex_);
            cbs.reserve(orderbook_callbacks_.size());
            for (auto it = orderbook_callbacks_.constBegin(); it != orderbook_callbacks_.constEnd(); ++it)
                cbs.append(it.value());
        }
        for (const auto& cb : cbs) {
            try {
                cb(ob.symbol, ob);
            } catch (...) {
            }
        }
        return;
    }

    if (type == "candle" || type == "ohlcv") {
        QJsonObject candle_obj = (type == "ohlcv") ? msg.value("candle").toObject() : msg;
        Candle c = parse_candle(candle_obj);
        QString sym = msg.value("symbol").toString();
        if (sym.isEmpty() || c.timestamp == 0)
            return;
        QVector<CandleCallback> cbs;
        {
            QMutexLocker lock(&mutex_);
            cbs.reserve(candle_callbacks_.size());
            for (auto it = candle_callbacks_.constBegin(); it != candle_callbacks_.constEnd(); ++it)
                cbs.append(it.value());
        }
        for (const auto& cb : cbs) {
            try {
                cb(sym, c);
            } catch (...) {
            }
        }
        return;
    }

    if (type == "trade") {
        TradeData td;
        td.symbol = msg.value("symbol").toString();
        td.side = msg.value("side").toString();
        td.price = msg.value("price").toDouble();
        td.amount = msg.value("amount").toDouble();
        td.timestamp = msg.value("timestamp").toVariant().toLongLong();
        QVector<TradeCallback> cbs;
        {
            QMutexLocker lock(&mutex_);
            cbs.reserve(trade_callbacks_.size());
            for (auto it = trade_callbacks_.constBegin(); it != trade_callbacks_.constEnd(); ++it)
                cbs.append(it.value());
        }
        for (const auto& cb : cbs) {
            try {
                cb(td.symbol, td);
            } catch (...) {
            }
        }
    }
}

void ExchangeService::drain_ws_buffer() {
    if (!ws_process_)
        return;

    // Process up to 50 lines OR 4ms, whichever comes first.
    // Old limit of 8 lines caused unbounded buffer buildup during volatility.
    // The 4ms time cap prevents UI starvation if lines are expensive to parse.
    int processed = 0;
    constexpr int MAX_LINES_PER_CYCLE = 50;
    constexpr qint64 MAX_MS_PER_CYCLE = 4;
    QElapsedTimer elapsed;
    elapsed.start();

    while (ws_process_->canReadLine() && processed < MAX_LINES_PER_CYCLE) {
        QString line = QString::fromUtf8(ws_process_->readLine()).trimmed();
        if (!line.isEmpty())
            handle_ws_line(line);
        ++processed;
        if (elapsed.elapsed() >= MAX_MS_PER_CYCLE)
            break;
    }
    // Reschedule if more data remains — use singleShot(1ms) instead of 0ms
    // to yield one event loop iteration for UI repaints
    if (ws_process_ && ws_process_->canReadLine())
        QTimer::singleShot(1, this, &ExchangeService::drain_ws_buffer);
}

// ============================================================================
// Python Script Calls
// ============================================================================

QJsonObject ExchangeService::call_script(const QString& script, const QStringList& args) {
    QString python_path = python::PythonRunner::instance().python_path();
    QString script_path = resolve_script_path(script);

    if (python_path.isEmpty() || script_path.isEmpty()) {
        return {{"error", "Python or script not found: " + script}};
    }

    QStringList full_args;
    full_args << "-u" << "-B" << script_path << exchange_id_;
    full_args.append(args);

    QProcess proc;
    proc.start(python_path, full_args);
    proc.waitForFinished(30000);

    QString output = proc.readAllStandardOutput().trimmed();
    if (output.isEmpty()) {
        QString err = proc.readAllStandardError().trimmed();
        return {{"error", err.isEmpty() ? "No output from script" : err}};
    }

    QJsonParseError parseErr;
    auto doc = QJsonDocument::fromJson(output.toUtf8(), &parseErr);
    if (parseErr.error != QJsonParseError::NoError) {
        return {{"error", "JSON parse error: " + parseErr.errorString()}};
    }

    auto obj = doc.object();

    // Python scripts wrap output as {"success": true, "data": {...}}.
    // Unwrap the "data" layer so callers can access keys directly.
    if (obj.value("success").toBool(false) && obj.contains("data")) {
        auto data = obj.value("data");
        if (data.isObject())
            return data.toObject();
    }

    // If there was an error, propagate it directly
    if (obj.contains("error"))
        return obj;

    // Fallback: return as-is (e.g. scripts that don't use output_success wrapper)
    return obj;
}

QJsonObject ExchangeService::call_script_with_credentials(const QString& script, const QStringList& args) {
    // Authenticated scripts expect credentials via stdin JSON, not command-line flags.
    // Build the credentials JSON to pipe to the process.
    QMutexLocker lock(&mutex_);
    QJsonObject creds_json;
    if (!credentials_.api_key.isEmpty()) {
        creds_json["api_key"] = credentials_.api_key;
        creds_json["secret"] = credentials_.secret;
        if (!credentials_.password.isEmpty())
            creds_json["password"] = credentials_.password;
    }
    lock.unlock();

    QString python_path = python::PythonRunner::instance().python_path();
    QString script_path = resolve_script_path(script);

    if (python_path.isEmpty() || script_path.isEmpty()) {
        return {{"error", "Python or script not found: " + script}};
    }

    QStringList full_args;
    full_args << "-u" << "-B" << script_path << exchange_id_;
    full_args.append(args);

    QProcess proc;
    proc.start(python_path, full_args);
    proc.waitForStarted(5000);

    // Write credentials to stdin
    if (!creds_json.isEmpty()) {
        QByteArray creds_data = QJsonDocument(creds_json).toJson(QJsonDocument::Compact);
        proc.write(creds_data);
    }
    proc.closeWriteChannel();

    proc.waitForFinished(30000);

    QString output = proc.readAllStandardOutput().trimmed();
    if (output.isEmpty()) {
        QString err = proc.readAllStandardError().trimmed();
        return {{"error", err.isEmpty() ? "No output from script" : err}};
    }

    QJsonParseError parseErr;
    auto doc = QJsonDocument::fromJson(output.toUtf8(), &parseErr);
    if (parseErr.error != QJsonParseError::NoError) {
        return {{"error", "JSON parse error: " + parseErr.errorString()}};
    }

    auto obj = doc.object();
    if (obj.value("success").toBool(false) && obj.contains("data")) {
        auto data = obj.value("data");
        if (data.isObject())
            return data.toObject();
    }
    if (obj.contains("error"))
        return obj;
    return obj;
}

// ============================================================================
// One-shot Fetches
// ============================================================================

TickerData ExchangeService::parse_ticker(const QJsonObject& j) {
    TickerData t;
    t.symbol = j.value("symbol").toString();
    t.last = j.value("last").toDouble();
    t.bid = j.value("bid").toDouble();
    t.ask = j.value("ask").toDouble();
    t.high = j.value("high").toDouble();
    t.low = j.value("low").toDouble();
    t.open = j.value("open").toDouble();
    t.close = j.value("close").toDouble();
    t.change = j.value("change").toDouble();
    t.percentage = j.value("percentage").toDouble();
    t.base_volume = j.value("baseVolume").toDouble();
    t.quote_volume = j.value("quoteVolume").toDouble();
    t.timestamp = j.value("timestamp").toVariant().toLongLong();
    return t;
}

OrderBookData ExchangeService::parse_orderbook(const QJsonObject& j) {
    OrderBookData ob;
    ob.symbol = j.value("symbol").toString();

    auto bids = j.value("bids").toArray();
    for (const auto& b : bids) {
        auto arr = b.toArray();
        if (arr.size() >= 2) {
            ob.bids.append({arr[0].toDouble(), arr[1].toDouble()});
        }
    }

    auto asks = j.value("asks").toArray();
    for (const auto& a : asks) {
        auto arr = a.toArray();
        if (arr.size() >= 2) {
            ob.asks.append({arr[0].toDouble(), arr[1].toDouble()});
        }
    }

    if (!ob.bids.isEmpty())
        ob.best_bid = ob.bids.first().first;
    if (!ob.asks.isEmpty())
        ob.best_ask = ob.asks.first().first;
    if (ob.best_bid > 0 && ob.best_ask > 0) {
        ob.spread = ob.best_ask - ob.best_bid;
        ob.spread_pct = (ob.spread / ob.best_ask) * 100.0;
    }
    return ob;
}

Candle ExchangeService::parse_candle(const QJsonObject& j) {
    Candle c;
    c.timestamp = j.value("timestamp").toVariant().toLongLong();
    c.open = j.value("open").toDouble();
    c.high = j.value("high").toDouble();
    c.low = j.value("low").toDouble();
    c.close = j.value("close").toDouble();
    c.volume = j.value("volume").toDouble();
    return c;
}

MarketInfo ExchangeService::parse_market(const QJsonObject& j) {
    MarketInfo m;
    m.symbol = j.value("symbol").toString();
    m.base = j.value("base").toString();
    m.quote = j.value("quote").toString();
    m.type = j.value("type").toString();
    m.active = j.value("active").toBool(true);
    m.maker_fee = j.value("maker").toDouble();
    m.taker_fee = j.value("taker").toDouble();
    m.min_amount = j.value("minAmount").toDouble();
    m.min_cost = j.value("minCost").toDouble();
    return m;
}

TickerData ExchangeService::fetch_ticker(const QString& symbol) {
    // Daemon path: ~100ms (reuses warm ccxt instance)
    // Legacy path: ~900ms (spawns Python, imports ccxt, creates exchange)
    if (is_daemon_running()) {
        auto j = daemon_call("fetch_ticker", {{"symbol", symbol}});
        if (!j.contains("error"))
            return parse_ticker(j);
        LOG_WARN(TAG, "Daemon fetch_ticker failed, falling back to script");
    }
    auto j = call_script("exchange/fetch_ticker.py", {symbol});
    if (j.contains("error"))
        return {};
    return parse_ticker(j);
}

QVector<TickerData> ExchangeService::fetch_tickers(const QStringList& symbols) {
    if (is_daemon_running()) {
        QJsonArray sym_arr;
        for (const auto& s : symbols)
            sym_arr.append(s);
        auto j = daemon_call("fetch_tickers", {{"symbols", sym_arr}});
        if (!j.contains("error") && j.contains("tickers")) {
            QVector<TickerData> result;
            auto arr = j.value("tickers").toArray();
            for (const auto& item : arr)
                result.append(parse_ticker(item.toObject()));
            return result;
        }
        LOG_WARN(TAG, "Daemon fetch_tickers failed, falling back to script");
    }
    QStringList args = symbols;
    auto j = call_script("exchange/fetch_tickers.py", args);
    QVector<TickerData> result;
    if (j.contains("tickers")) {
        auto arr = j.value("tickers").toArray();
        for (const auto& item : arr)
            result.append(parse_ticker(item.toObject()));
    }
    return result;
}

OrderBookData ExchangeService::fetch_orderbook(const QString& symbol, int limit) {
    if (is_daemon_running()) {
        auto j = daemon_call("fetch_orderbook", {{"symbol", symbol}, {"limit", limit}});
        if (!j.contains("error"))
            return parse_orderbook(j);
        LOG_WARN(TAG, "Daemon fetch_orderbook failed, falling back to script");
    }
    auto j = call_script("exchange/fetch_orderbook.py", {symbol, QString::number(limit)});
    if (j.contains("error"))
        return {};
    return parse_orderbook(j);
}

QVector<Candle> ExchangeService::fetch_ohlcv(const QString& symbol, const QString& timeframe, int limit) {
    if (is_daemon_running()) {
        auto j = daemon_call("fetch_ohlcv", {{"symbol", symbol}, {"timeframe", timeframe}, {"limit", limit}});
        if (!j.contains("error") && j.contains("candles")) {
            QVector<Candle> result;
            auto arr = j.value("candles").toArray();
            for (const auto& item : arr)
                result.append(parse_candle(item.toObject()));
            return result;
        }
        LOG_WARN(TAG, "Daemon fetch_ohlcv failed, falling back to script");
    }
    auto j = call_script("exchange/fetch_ohlcv.py", {symbol, timeframe, QString::number(limit)});
    QVector<Candle> result;
    if (j.contains("candles")) {
        auto arr = j.value("candles").toArray();
        for (const auto& item : arr)
            result.append(parse_candle(item.toObject()));
    }
    return result;
}

QVector<MarketInfo> ExchangeService::fetch_markets(const QString& type) {
    if (is_daemon_running()) {
        QJsonObject args;
        if (!type.isEmpty())
            args["type"] = type;
        auto j = daemon_call("fetch_markets", args);
        if (!j.contains("error") && j.contains("markets")) {
            QVector<MarketInfo> result;
            auto arr = j.value("markets").toArray();
            for (const auto& item : arr)
                result.append(parse_market(item.toObject()));
            return result;
        }
        LOG_WARN(TAG, "Daemon fetch_markets failed, falling back to script");
    }
    QStringList args;
    if (!type.isEmpty())
        args << type;
    auto j = call_script("exchange/fetch_markets.py", args);
    QVector<MarketInfo> result;
    if (j.contains("markets")) {
        auto arr = j.value("markets").toArray();
        for (const auto& item : arr)
            result.append(parse_market(item.toObject()));
    }
    return result;
}

QStringList ExchangeService::list_exchange_ids() {
    if (is_daemon_running()) {
        auto j = daemon_call("list_exchange_ids", {});
        if (!j.contains("error") && j.contains("exchanges")) {
            QStringList result;
            auto arr = j.value("exchanges").toArray();
            for (const auto& item : arr)
                result.append(item.toString());
            return result;
        }
    }
    auto j = call_script("exchange/list_exchanges.py", {});
    QStringList result;
    if (j.contains("exchanges")) {
        auto arr = j.value("exchanges").toArray();
        for (const auto& item : arr)
            result.append(item.toString());
    }
    return result;
}

QVector<TradeData> ExchangeService::fetch_trades(const QString& symbol, int limit) {
    if (is_daemon_running()) {
        auto j = daemon_call("fetch_trades", {{"symbol", symbol}, {"limit", limit}});
        if (!j.contains("error") && j.contains("trades")) {
            QVector<TradeData> result;
            auto arr = j.value("trades").toArray();
            for (const auto& item : arr) {
                auto o = item.toObject();
                TradeData td;
                td.id = o.value("id").toString();
                td.symbol = o.value("symbol").toString();
                td.side = o.value("side").toString();
                td.price = o.value("price").toDouble();
                td.amount = o.value("amount").toDouble();
                td.cost = o.value("cost").toDouble();
                td.timestamp = o.value("timestamp").toVariant().toLongLong();
                result.append(td);
            }
            return result;
        }
    }
    auto j = call_script("exchange/fetch_trades.py", {symbol, QString::number(limit)});
    QVector<TradeData> result;
    if (j.contains("trades")) {
        auto arr = j.value("trades").toArray();
        for (const auto& item : arr) {
            auto o = item.toObject();
            TradeData td;
            td.id = o.value("id").toString();
            td.symbol = o.value("symbol").toString();
            td.side = o.value("side").toString();
            td.price = o.value("price").toDouble();
            td.amount = o.value("amount").toDouble();
            td.cost = o.value("cost").toDouble();
            td.timestamp = o.value("timestamp").toVariant().toLongLong();
            result.append(td);
        }
    }
    return result;
}

FundingRateData ExchangeService::fetch_funding_rate(const QString& symbol) {
    if (is_daemon_running()) {
        auto j = daemon_call("fetch_funding_rate", {{"symbol", symbol}});
        if (!j.contains("error")) {
            FundingRateData fr;
            fr.symbol = j.value("symbol").toString();
            fr.funding_rate = j.value("funding_rate").toDouble();
            fr.mark_price = j.value("mark_price").toDouble();
            fr.index_price = j.value("index_price").toDouble();
            fr.funding_timestamp = j.value("funding_timestamp").toVariant().toLongLong();
            fr.next_funding_timestamp = j.value("next_funding_timestamp").toVariant().toLongLong();
            return fr;
        }
    }
    auto j = call_script("exchange/fetch_funding_rate.py", {symbol});
    FundingRateData fr;
    if (!j.contains("error")) {
        fr.symbol = j.value("symbol").toString();
        fr.funding_rate = j.value("funding_rate").toDouble();
        fr.mark_price = j.value("mark_price").toDouble();
        fr.index_price = j.value("index_price").toDouble();
        fr.funding_timestamp = j.value("funding_timestamp").toVariant().toLongLong();
        fr.next_funding_timestamp = j.value("next_funding_timestamp").toVariant().toLongLong();
    }
    return fr;
}

OpenInterestData ExchangeService::fetch_open_interest(const QString& symbol) {
    if (is_daemon_running()) {
        auto j = daemon_call("fetch_open_interest", {{"symbol", symbol}});
        if (!j.contains("error")) {
            OpenInterestData oi;
            oi.symbol = j.value("symbol").toString();
            oi.open_interest = j.value("open_interest").toDouble();
            oi.open_interest_value = j.value("open_interest_value").toDouble();
            oi.timestamp = j.value("timestamp").toVariant().toLongLong();
            return oi;
        }
    }
    auto j = call_script("exchange/fetch_open_interest.py", {symbol});
    OpenInterestData oi;
    if (!j.contains("error")) {
        oi.symbol = j.value("symbol").toString();
        oi.open_interest = j.value("open_interest").toDouble();
        oi.open_interest_value = j.value("open_interest_value").toDouble();
        oi.timestamp = j.value("timestamp").toVariant().toLongLong();
    }
    return oi;
}

QJsonObject ExchangeService::fetch_balance() {
    if (is_daemon_running()) {
        auto j = daemon_call("fetch_balance", {});
        if (!j.contains("error"))
            return j;
    }
    return call_script_with_credentials("exchange/fetch_balance.py", {});
}

QJsonObject ExchangeService::place_exchange_order(const QString& symbol, const QString& side, const QString& type,
                                                  double amount, double price) {
    if (is_daemon_running()) {
        QJsonObject args;
        args["symbol"] = symbol;
        args["side"] = side;
        args["type"] = type;
        args["amount"] = amount;
        if (price > 0)
            args["price"] = price;
        auto j = daemon_call("place_order", args);
        if (!j.contains("error"))
            return j;
        LOG_WARN(TAG, "Daemon place_order failed, falling back to script");
    }
    QStringList args = {symbol, side, type, QString::number(amount, 'f', 8)};
    if (price > 0)
        args << QString::number(price, 'f', 8);
    return call_script_with_credentials("exchange/place_order.py", args);
}

QJsonObject ExchangeService::cancel_exchange_order(const QString& order_id, const QString& symbol) {
    if (is_daemon_running()) {
        auto j = daemon_call("cancel_order", {{"order_id", order_id}, {"symbol", symbol}});
        if (!j.contains("error"))
            return j;
    }
    return call_script_with_credentials("exchange/cancel_order.py", {order_id, symbol});
}

QJsonObject ExchangeService::fetch_positions_live(const QString& symbol) {
    if (is_daemon_running()) {
        QJsonObject args;
        if (!symbol.isEmpty())
            args["symbol"] = symbol;
        auto j = daemon_call("fetch_positions", args);
        if (!j.contains("error"))
            return j;
    }
    QStringList args;
    if (!symbol.isEmpty())
        args << symbol;
    return call_script_with_credentials("exchange/fetch_positions.py", args);
}

QJsonObject ExchangeService::fetch_open_orders_live(const QString& symbol) {
    if (is_daemon_running()) {
        QJsonObject args;
        if (!symbol.isEmpty())
            args["symbol"] = symbol;
        auto j = daemon_call("fetch_open_orders", args);
        if (!j.contains("error"))
            return j;
    }
    QStringList args;
    if (!symbol.isEmpty())
        args << symbol;
    return call_script_with_credentials("exchange/fetch_open_orders.py", args);
}

const QHash<QString, TickerData>& ExchangeService::get_price_cache() const {
    return price_cache_;
}

TickerData ExchangeService::get_cached_price(const QString& symbol) const {
    QMutexLocker lock(&mutex_);
    return price_cache_.value(symbol);
}

} // namespace fincept::trading
