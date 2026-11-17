// src/screens/backtesting/BtRunController.cpp
#include "screens/backtesting/BtRunController.h"

#include "core/logging/Logger.h"
#include "services/backtesting/BacktestingService.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QPointer>
#include <QTimeZone>
#include <QtConcurrent/QtConcurrentRun>

namespace fincept::screens::bt {

using namespace fincept::services::backtest;

namespace {

QString bt_rc_tr(const char* text) {
    return QCoreApplication::translate("fincept::screens::bt::BtRunController", text);
}

/// Parse a payload into the bundle's typed model (worker thread).
void bt_rc_parse(BtResultBundle& b) {
    switch (b.kind) {
        case BtKind::Backtest: {
            BtRun r = parse_backtest(b.payload);
            if (r.currency.isEmpty())
                r.currency = b.currency;
            b.currency = r.currency;
            b.run = std::make_shared<const BtRun>(std::move(r));
            break;
        }
        case BtKind::Optimize:
            b.optimize = std::make_shared<const BtOptimizeResult>(parse_optimize(b.payload));
            if (!b.optimize->provenance.currency.isEmpty())
                b.currency = b.optimize->provenance.currency;
            break;
        case BtKind::WalkForward:
            b.walk_forward = std::make_shared<const BtWalkForwardResult>(parse_walk_forward(b.payload));
            if (!b.walk_forward->provenance.currency.isEmpty())
                b.currency = b.walk_forward->provenance.currency;
            break;
        case BtKind::MonteCarlo:
            b.monte_carlo = std::make_shared<const BtMonteCarloResult>(parse_monte_carlo(b.payload));
            if (!b.monte_carlo->provenance.currency.isEmpty())
                b.currency = b.monte_carlo->provenance.currency;
            break;
        case BtKind::Research:
            break;
    }
}

QJsonObject bt_rc_meta(const BtResultBundle& b) {
    QJsonObject meta;
    meta[QStringLiteral("providerSlug")] = b.engine;
    meta[QStringLiteral("command")] = b.command;
    meta[QStringLiteral("strategy")] = b.request.value(QStringLiteral("strategy")).toObject().value(QStringLiteral("type"));
    meta[QStringLiteral("symbols")] = b.request.value(QStringLiteral("symbols")).toArray();
    meta[QStringLiteral("startDate")] = b.request.value(QStringLiteral("startDate"));
    meta[QStringLiteral("endDate")] = b.request.value(QStringLiteral("endDate"));
    meta[QStringLiteral("interval")] = b.request.value(QStringLiteral("interval"));
    return meta;
}

} // namespace

QString bt_command_of(BtKind kind) {
    switch (kind) {
        case BtKind::Backtest:
            return QStringLiteral("backtest");
        case BtKind::Optimize:
            return QStringLiteral("optimize");
        case BtKind::WalkForward:
            return QStringLiteral("walk_forward");
        case BtKind::MonteCarlo:
            return QStringLiteral("monte_carlo");
        case BtKind::Research:
            break;
    }
    return {};
}

BtKind bt_kind_of(const QString& command) {
    if (command == QLatin1String("backtest"))
        return BtKind::Backtest;
    if (command == QLatin1String("optimize"))
        return BtKind::Optimize;
    if (command == QLatin1String("walk_forward"))
        return BtKind::WalkForward;
    if (command == QLatin1String("monte_carlo"))
        return BtKind::MonteCarlo;
    return BtKind::Research;
}

BtRunController::BtRunController(QObject* parent) : QObject(parent) {
    auto& svc = BacktestingService::instance();
    connect(&svc, &BacktestingService::run_finished, this, &BtRunController::on_service_finished);
    connect(&svc, &BacktestingService::run_cancelled, this,
            [this](quint64 run_id, const QString&, const QString&) { on_service_cancelled(run_id); });
    connect(&svc, &BacktestingService::run_progress, this, &BtRunController::on_service_progress);
}

BtRunController::~BtRunController() {
    cancel_all();
}

bool BtRunController::is_heavy(const Job& j) {
    return j.kind != BtKind::Research || j.command == QLatin1String("indicator_sweep");
}

quint64 BtRunController::start(BtKind kind, const QString& engine, const QString& command, const QJsonObject& request,
                               const QString& signature, const QString& currency, QString* why) {
    Job job;
    job.token = next_token_++;
    job.kind = kind;
    job.engine = engine;
    job.command = command.isEmpty() ? bt_command_of(kind) : command;
    job.request = request;
    job.signature = signature;
    job.currency = currency;
    if (is_heavy(job) && heavy_running_ != 0) {
        if (queue_.size() >= kMaxQueued) {
            if (why)
                *why = bt_rc_tr("Three runs are already waiting. Cancel one or wait for the current run to finish.");
            return 0;
        }
        const quint64 token = job.token;
        queue_.append(std::move(job));
        LOG_DEBUG("Backtesting", logf("Run queued", {{"command", command}, {"position", queue_.size()}}));
        emit state_changed();
        return token;
    }
    const quint64 token = job.token;
    dispatch(std::move(job));
    return token;
}

void BtRunController::dispatch(Job job) {
    job.clock.start();
    job.service_id = BacktestingService::instance().execute(job.engine, job.command, job.request);
    if (is_heavy(job))
        heavy_running_ = job.token;
    const quint64 token = job.token;
    running_.insert(token, std::move(job));
    emit started(token);
    emit state_changed();
}

void BtRunController::pump() {
    if (heavy_running_ != 0 || queue_.isEmpty())
        return;
    Job next = queue_.takeFirst();
    dispatch(std::move(next));
}

bool BtRunController::cancel(quint64 token) {
    for (int i = 0; i < queue_.size(); ++i) {
        if (queue_[i].token == token) {
            queue_.removeAt(i);
            emit cancelled(token);
            emit state_changed();
            return true;
        }
    }
    auto it = running_.find(token);
    if (it == running_.end())
        return false;
    const quint64 sid = it->service_id;
    const bool heavy = is_heavy(*it);
    running_.erase(it);
    if (heavy && heavy_running_ == token)
        heavy_running_ = 0;
    BacktestingService::instance().cancel(sid); // emits run_cancelled for a run we no longer track: ignored
    emit cancelled(token);
    emit state_changed();
    pump();
    return true;
}

void BtRunController::cancel_all() {
    queue_.clear();
    const auto tokens = running_.keys();
    for (quint64 t : tokens) {
        auto it = running_.find(t);
        if (it == running_.end())
            continue;
        const quint64 sid = it->service_id;
        running_.erase(it);
        BacktestingService::instance().cancel(sid);
    }
    heavy_running_ = 0;
}

bool BtRunController::is_active(quint64 token) const {
    if (running_.contains(token))
        return true;
    for (const Job& j : queue_)
        if (j.token == token)
            return true;
    return false;
}

int BtRunController::queue_position(quint64 token) const {
    if (running_.contains(token))
        return 0;
    for (int i = 0; i < queue_.size(); ++i)
        if (queue_[i].token == token)
            return i + 1;
    return -1;
}

qint64 BtRunController::elapsed_ms(quint64 token) const {
    const auto it = running_.constFind(token);
    return it == running_.constEnd() ? -1 : it->clock.elapsed();
}

BtProgress BtRunController::last_progress(quint64 token) const {
    const auto it = running_.constFind(token);
    return it == running_.constEnd() ? BtProgress{} : it->last;
}

BtKind BtRunController::kind_of(quint64 token) const {
    const auto it = running_.constFind(token);
    if (it != running_.constEnd())
        return it->kind;
    for (const Job& j : queue_)
        if (j.token == token)
            return j.kind;
    return BtKind::Backtest;
}

QString BtRunController::engine_of(quint64 token) const {
    const auto it = running_.constFind(token);
    if (it != running_.constEnd())
        return it->engine;
    for (const Job& j : queue_)
        if (j.token == token)
            return j.engine;
    return {};
}

quint64 BtRunController::token_for_service(quint64 run_id) const {
    for (auto it = running_.constBegin(); it != running_.constEnd(); ++it)
        if (it->service_id == run_id)
            return it.key();
    return 0;
}

void BtRunController::on_service_progress(quint64 run_id, const BtProgress& p) {
    const quint64 token = token_for_service(run_id);
    if (!token)
        return;
    running_[token].last = p;
    emit progress(token, p);
}

void BtRunController::on_service_cancelled(quint64 run_id) {
    // A provider that stopped itself (exit 3) — cancels from this side were already untracked.
    const quint64 token = token_for_service(run_id);
    if (!token)
        return;
    const bool heavy = is_heavy(running_.value(token));
    running_.remove(token);
    if (heavy && heavy_running_ == token)
        heavy_running_ = 0;
    emit cancelled(token);
    emit state_changed();
    pump();
}

void BtRunController::on_service_finished(quint64 run_id, const BtOutcome& outcome) {
    const quint64 token = token_for_service(run_id);
    if (!token)
        return;
    if (outcome.cancelled())
        return; // run_cancelled follows and is handled there
    Job job = running_.take(token);
    if (is_heavy(job) && heavy_running_ == token)
        heavy_running_ = 0;
    emit state_changed();
    pump();
    if (!outcome.ok()) {
        emit failed(token, outcome);
        return;
    }
    // Parse + save off the GUI thread; deliver here.
    auto bundle = std::make_shared<BtResultBundle>();
    bundle->kind = job.kind;
    bundle->engine = job.engine;
    bundle->command = job.command;
    bundle->request = job.request;
    bundle->signature = job.signature;
    bundle->currency = job.currency;
    bundle->payload = outcome.data;
    bundle->as_of = QDateTime::currentDateTimeUtc();
    bundle->key = QStringLiteral("t%1").arg(token);
    QPointer<BtRunController> self = this;
    (void)QtConcurrent::run([self, token, bundle]() {
        bt_rc_parse(*bundle);
        if (bundle->kind != BtKind::Research && !bundle->payload.isEmpty()) {
            auto r = BacktestRunStore::instance().save(bt_rc_meta(*bundle), bundle->request, bundle->payload,
                                                       bundle->currency, bundle->signature);
            if (r.is_ok()) {
                bundle->store_id = r.value();
                bundle->key = QStringLiteral("s%1").arg(bundle->store_id);
            } else {
                LOG_WARN("Backtesting", logf("Run could not be saved", {{"error", QString::fromStdString(r.error())}}));
            }
        }
        BtBundlePtr done = bundle;
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, token, done]() {
                if (!self)
                    return;
                self->remember(done);
                emit self->finished(token, done);
            },
            Qt::QueuedConnection);
    });
}

BtBundlePtr BtRunController::bundle(const QString& key) const {
    return cache_.value(key);
}

void BtRunController::remember(const BtBundlePtr& b) {
    if (!b || b->key.isEmpty())
        return;
    cache_.insert(b->key, b);
    cache_order_.removeAll(b->key);
    cache_order_.append(b->key);
    while (cache_order_.size() > kMaxCached)
        cache_.remove(cache_order_.takeFirst());
}

void BtRunController::load_saved(qint64 store_id, std::function<void(BtBundlePtr, QString)> done) {
    if (const BtBundlePtr hit = bundle(QStringLiteral("s%1").arg(store_id))) {
        QMetaObject::invokeMethod(this, [done, hit]() { done(hit, QString()); }, Qt::QueuedConnection);
        return;
    }
    QPointer<BtRunController> self = this;
    (void)QtConcurrent::run([self, store_id, done]() {
        auto& store = BacktestRunStore::instance();
        QString error;
        BtBundlePtr out;
        auto row = store.row(store_id);
        if (row.is_err() || row.value().id == 0) {
            error = bt_rc_tr("The saved run could not be found.");
        } else {
            auto payload = store.payload(store_id);
            if (payload.is_err()) {
                error = bt_rc_tr("The saved run could not be read: %1").arg(QString::fromStdString(payload.error()));
            } else {
                const SavedRun& s = row.value();
                auto b = std::make_shared<BtResultBundle>();
                b->kind = bt_kind_of(s.command);
                b->engine = s.provider;
                b->command = s.command;
                auto inputs = store.inputs(store_id);
                if (inputs.is_ok())
                    b->request = inputs.value();
                b->signature = s.signature();
                b->as_of = QDateTime::fromString(s.created_at, Qt::ISODate);
                b->as_of.setTimeZone(QTimeZone::UTC);
                b->store_id = s.id;
                b->label = s.label;
                b->currency = s.currency;
                b->payload = payload.value();
                b->key = QStringLiteral("s%1").arg(s.id);
                bt_rc_parse(*b);
                out = b;
            }
        }
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, out, error, done]() {
                if (!self)
                    return;
                if (out)
                    self->remember(out);
                done(out, error);
            },
            Qt::QueuedConnection);
    });
}

SavedRun BtRunController::latest_saved(const QString& command, const QString& engine, const QString& strategy) const {
    auto r = BacktestRunStore::instance().latest(command, engine, strategy);
    return r.is_ok() ? r.value() : SavedRun{};
}

} // namespace fincept::screens::bt
