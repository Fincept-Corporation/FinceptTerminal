// TaskRunScheduler.cpp — recurring runs: the timer, missed occurrences, overlap
// and daily-cap checks, persistence, and the schedule's view of its runs (WP6).
// The occurrence maths are in ScheduleLogic.cpp.

#include "services/agent_tasks/TaskRunScheduler.h"

#include "auth/AuthManager.h"
#include "core/logging/Logger.h"
#include "services/agent_tasks/RunTemplateLibrary.h"
#include "services/agent_tasks/ScheduleLogic.h"
#include "services/agent_tasks/TaskRunService.h"
#include "services/agent_tasks/TaskRunStore.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/notifications/NotificationService.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QLocale>
#include <QPointer>
#include <QSet>
#include <QThreadPool>
#include <QTime>
#include <QTimer>
#include <QUuid>

#include <algorithm>

namespace fincept::agent_tasks {

namespace sl = schedule_logic;

namespace task_run_scheduler_detail {

constexpr const char* kSchedTag = "RunSchedules";
constexpr const char* kSchedCapKey = "agent_runs.schedule_daily_credit_cap";
constexpr int kSchedHeartbeatMs = 60 * 1000; ///< re-check at least this often (sleep / clock changes)
constexpr int kSchedMinDelayMs = 1000;

QString tsr_tr(const char* text) {
    return QCoreApplication::translate("fincept::agent_tasks::TaskRunScheduler", text);
}

qint64 tsr_now_ms() {
    return QDateTime::currentMSecsSinceEpoch();
}

/// --smoke-test walks every screen and --selftest-* suites are headless: neither
/// may start a scheduled — paid — agent run.
bool tsr_headless() {
    for (const QString& a : QCoreApplication::arguments())
        if (a == QLatin1String("--smoke-test") || a.startsWith(QLatin1String("--selftest")))
            return true;
    return false;
}

QString tsr_when(qint64 ms, const RunSchedule& s) {
    return QLocale().toString(QDateTime::fromMSecsSinceEpoch(ms, sl::schedule_zone(s)), QLocale::ShortFormat);
}

} // namespace task_run_scheduler_detail

using namespace task_run_scheduler_detail;

struct TaskRunScheduler::Impl {
    TaskRunScheduler* q = nullptr;
    QThreadPool* writer = nullptr;
    QTimer* timer = nullptr;
    QVector<RunSchedule> list;
    QSet<QString> starting; ///< schedules whose run is being prepared (no second firing meanwhile)
    double cap = sl::kDefaultDailyCreditCap;
    bool initialized = false;
    bool loaded = false;
    bool headless = false;

    RunSchedule* find(const QString& id) {
        for (RunSchedule& s : list)
            if (s.id == id)
                return &s;
        return nullptr;
    }

    void persist(const RunSchedule& s) {
        const RunSchedule copy = s;
        writer->start([copy]() {
            auto r = TaskRunStore::instance().upsert_schedule(copy);
            if (r.is_err())
                LOG_WARN(kSchedTag, QStringLiteral("Saving schedule %1 failed: %2").arg(copy.id.left(8), QString::fromStdString(r.error())));
        });
    }

    void rearm();
    void evaluate_all();
    void evaluate(const QString& id);
    /// Start the schedule's run. `manual`: "run now" (does not move the next occurrence).
    void fire(const QString& id, bool manual, bool caught_up, QObject* ctx, StartCallback cb);
    void on_run_changed(const QString& run_id, bool finished);
    void record(const QString& id, const QString& status, const QString& error, bool notify);
};

void TaskRunScheduler::Impl::rearm() {
    timer->stop();
    if (headless || !loaded)
        return;
    const qint64 now = tsr_now_ms();
    qint64 earliest = 0;
    for (const RunSchedule& s : std::as_const(list)) {
        if (!s.enabled || s.paused_for_review || s.next_run_at_ms <= 0)
            continue;
        if (earliest == 0 || s.next_run_at_ms < earliest)
            earliest = s.next_run_at_ms;
    }
    if (earliest == 0)
        return;
    const qint64 delay = std::clamp<qint64>(earliest - now, kSchedMinDelayMs, kSchedHeartbeatMs);
    timer->start(static_cast<int>(delay));
}

void TaskRunScheduler::Impl::evaluate_all() {
    QStringList ids;
    for (const RunSchedule& s : std::as_const(list))
        ids << s.id;
    for (const QString& id : ids)
        evaluate(id);
    rearm();
}

void TaskRunScheduler::Impl::evaluate(const QString& id) {
    RunSchedule* s = find(id);
    if (!s || starting.contains(id))
        return;
    const qint64 now = tsr_now_ms();
    const sl::DueDecision d = sl::decide_due(*s, now);
    if (!d.fire && !d.missed) {
        if (d.next_ms != s->next_run_at_ms && s->enabled && !s->paused_for_review) {
            s->next_run_at_ms = d.next_ms;
            s->updated_at_ms = now;
            persist(*s);
            emit q->schedules_changed();
        }
        return;
    }
    if (d.missed) {
        s->next_run_at_ms = d.next_ms;
        s->last_status = QStringLiteral("missed");
        s->last_error = tsr_tr("Missed the run of %1: the terminal was not running, and it is more than %2 minute(s) late.")
                            .arg(tsr_when(d.missed_at_ms, *s))
                            .arg(std::clamp(s->catch_up_minutes, 0, sl::kMaxCatchUpMinutes));
        if (s->kind == QLatin1String(kScheduleOnce))
            s->enabled = false;
        s->updated_at_ms = now;
        persist(*s);
        LOG_INFO(kSchedTag, QStringLiteral("Schedule %1: occurrence missed").arg(id.left(8)));
        emit q->schedules_changed();
        return;
    }
    // A cloud run needs a signed-in session: wait (the occurrence stays due; once
    // its catch-up window has passed it is recorded as missed). Local and harness
    // runs need no Fincept sign-in.
    const QString blocker =
        sl::start_blocker(s->engine, !auth::AuthManager::instance().session().api_key.isEmpty());
    if (!blocker.isEmpty()) {
        if (s->last_status != QLatin1String("waiting_login")) {
            s->last_status = QStringLiteral("waiting_login");
            s->last_error = blocker;
            s->updated_at_ms = now;
            persist(*s);
            emit q->schedules_changed();
        }
        return;
    }
    fire(id, false, d.caught_up, nullptr, {});
}

void TaskRunScheduler::Impl::record(const QString& id, const QString& status, const QString& error, bool notify) {
    RunSchedule* s = find(id);
    if (!s)
        return;
    s->last_status = status;
    s->last_error = error;
    s->updated_at_ms = tsr_now_ms();
    persist(*s);
    emit q->schedules_changed();
    if (notify && !error.isEmpty())
        ui::ToastService::instance().post(ui::ToastService::Severity::Warning,
                                          tsr_tr("Scheduled run “%1”: %2").arg(s->name, error), QStringLiteral("Chat Mode"));
}

void TaskRunScheduler::Impl::fire(const QString& id, bool manual, bool caught_up, QObject* ctx, StartCallback cb) {
    QPointer<QObject> guard(ctx);
    const bool has_ctx = ctx != nullptr;
    const auto reply = [guard, has_ctx, cb](bool ok, const QString& run_id, const QString& err) {
        if (cb && (!has_ctx || guard))
            cb(ok, run_id, err);
    };
    RunSchedule* s = find(id);
    if (!s) {
        reply(false, {}, tsr_tr("The schedule no longer exists."));
        return;
    }
    if (starting.contains(id)) {
        reply(false, {}, tsr_tr("This schedule is starting its run already."));
        return;
    }
    auto& runs = TaskRunService::instance();
    const qint64 now = tsr_now_ms();
    if (!manual) {
        // The next occurrence is stored before anything starts: a crash from here on
        // loses this occurrence at worst, it never runs it twice.
        s->next_run_at_ms = sl::next_occurrence_ms(*s, now);
        if (s->kind == QLatin1String(kScheduleOnce))
            s->enabled = false;
    }
    // Never overlap: the previous run of this schedule must have ended.
    std::optional<RunStatus> prev;
    if (const TaskRun* r = runs.find_run(s->last_run_id))
        prev = r->status;
    const QString overlap = sl::overlap_reason(prev);
    if (!overlap.isEmpty()) {
        s->updated_at_ms = now;
        record(id, QStringLiteral("skipped_overlap"), overlap, false);
        reply(false, {}, overlap);
        return;
    }
    const QString blocker =
        sl::start_blocker(s->engine, !auth::AuthManager::instance().session().api_key.isEmpty());
    if (!blocker.isEmpty()) {
        record(id, QStringLiteral("waiting_login"), blocker, false);
        reply(false, {}, blocker);
        return;
    }
    const RunRequest& base = s->request;
    const QString executor = executor_for(s->engine, base.plan_first);
    if (executor.isEmpty() || !runs.has_executor(executor)) {
        const QString err = tsr_tr("The engine of this schedule is not available in this build.");
        record(id, QStringLiteral("failed_to_start"), err, !manual);
        reply(false, {}, err);
        return;
    }
    RunRequest req = base;
    req.engine = s->engine;
    req.mode = s->mode.isEmpty() ? base.mode : s->mode;
    req.origin = QString::fromLatin1(kOriginSchedule);
    req.schedule_id = s->id;
    req.session_uuid.clear(); // every occurrence talks in a conversation of its own
    req.parent_run_id.clear();
    req.title = QStringLiteral("%1 · %2").arg(s->name, QLocale().toString(QDate::currentDate(), QLocale::ShortFormat));
    if (s->max_credits > 0)
        req.budget.max_credits = s->max_credits;
    // The day's credit cap across scheduled cloud runs (local and harness runs spend no credits).
    QString cap_note;
    if (sl::engine_spends_credits(s->engine)) {
        const double minimum = executor == QLatin1String(kExecutorCloudTurn) ? kCloudTurnCredits : estimate_cloud_plan_credits(1);
        const sl::CapDecision cap_decision = sl::apply_daily_cap(cap, q->committed_today(), req.budget.max_credits, minimum);
        if (!cap_decision.allowed) {
            record(id, QStringLiteral("skipped_cap"), cap_decision.reason, true);
            reply(false, {}, cap_decision.reason);
            return;
        }
        req.budget.max_credits = cap_decision.budget;
        cap_note = cap_decision.reason;
    }
    s->last_run_at_ms = now;
    s->last_status = QStringLiteral("starting");
    s->last_error.clear();
    s->updated_at_ms = now;
    persist(*s);
    starting.insert(id);
    emit q->schedules_changed();
    rearm();

    // A schedule saved without its own task text follows its template (relative
    // periods for today, holdings / watchlist symbols as they are now).
    const QString refill = s->prompt.trimmed().isEmpty() ? s->template_id : QString();
    if (!s->prompt.trimmed().isEmpty())
        req.prompt = s->prompt;
    LOG_INFO(kSchedTag, QStringLiteral("Schedule %1 fires%2").arg(id.left(8), manual      ? QStringLiteral(" (run now)")
                                                                                : caught_up ? QStringLiteral(" (catching up)")
                                                                                            : QString()));
    QPointer<TaskRunScheduler> self(q);
    RunTemplateLibrary::instance().prepare_request(
        req, refill, q, [self, id, reply, cap_note, caught_up](const RunTemplateLibrary::Prepared& p) {
            if (!self)
                return;
            Impl* d = self->d_.get();
            d->starting.remove(id);
            if (!p.ok) {
                d->record(id, QStringLiteral("failed_to_start"), p.error, true);
                reply(false, {}, p.error);
                return;
            }
            QString err;
            const QString run_id = TaskRunService::instance().start_run(p.request, &err);
            RunSchedule* s2 = d->find(id);
            if (run_id.isEmpty()) {
                d->record(id, QStringLiteral("failed_to_start"), err, true);
                reply(false, {}, err);
                return;
            }
            if (s2) {
                s2->last_run_id = run_id;
                QString note = caught_up ? tsr_tr("Caught up a run the terminal missed while it was closed.") : QString();
                if (!cap_note.isEmpty())
                    note = note.isEmpty() ? cap_note : note + QLatin1Char(' ') + cap_note;
                d->record(id, QStringLiteral("started"), note, false);
            }
            emit self->schedule_fired(id, run_id);
            reply(true, run_id, {});
        });
}

void TaskRunScheduler::Impl::on_run_changed(const QString& run_id, bool finished) {
    const TaskRun* r = TaskRunService::instance().find_run(run_id);
    if (!r || r->schedule_id.isEmpty())
        return;
    RunSchedule* s = find(r->schedule_id);
    if (!s || s->last_run_id != run_id)
        return;
    const QString status = run_status_id(r->status);
    if (!finished && s->last_status == status)
        return;
    if (!finished && !needs_attention(r->status) && r->status != RunStatus::Paused)
        return; // only the states a schedule list shows: "needs you", paused, the outcome
    s->last_status = status;
    if (finished) {
        if (r->status == RunStatus::Failed || r->status == RunStatus::BudgetStopped) {
            // Something went wrong unattended: no further runs until the user looks.
            s->paused_for_review = true;
            s->last_error = r->error.isEmpty() ? run_status_label(r->status) : r->error.left(500);
            LOG_INFO(kSchedTag, QStringLiteral("Schedule %1 paused for review (run %2 %3)")
                                    .arg(s->id.left(8), run_id.left(8), status));
        } else {
            s->last_error.clear();
        }
    }
    s->updated_at_ms = tsr_now_ms();
    persist(*s);
    emit q->schedules_changed();
    rearm();
}

// ── TaskRunScheduler ─────────────────────────────────────────────────────────

TaskRunScheduler& TaskRunScheduler::instance() {
    static TaskRunScheduler s;
    return s;
}

TaskRunScheduler::TaskRunScheduler() : QObject(nullptr), d_(std::make_unique<Impl>()) {
    d_->q = this;
    d_->writer = new QThreadPool(this);
    d_->writer->setMaxThreadCount(1);
    d_->timer = new QTimer(this);
    d_->timer->setSingleShot(true);
    connect(d_->timer, &QTimer::timeout, this, [this]() { d_->evaluate_all(); });
    if (auto* app = QCoreApplication::instance()) {
        if (thread() != app->thread())
            moveToThread(app->thread());
        connect(app, &QCoreApplication::aboutToQuit, this, [this]() {
            d_->timer->stop();
            d_->writer->waitForDone(3000);
        });
    }
}

TaskRunScheduler::~TaskRunScheduler() = default;

void TaskRunScheduler::initialize_deferred() {
    if (d_->initialized)
        return;
    d_->initialized = true;
    d_->headless = tsr_headless();
    auto& runs = TaskRunService::instance();
    connect(&runs, &TaskRunService::run_finished, this, [this](const QString& id) { d_->on_run_changed(id, true); });
    connect(&runs, &TaskRunService::run_updated, this, [this](const QString& id) { d_->on_run_changed(id, false); });
    QPointer<TaskRunScheduler> self(this);
    d_->writer->start([self]() {
        auto r = TaskRunStore::instance().load_schedules();
        QVector<RunSchedule> list = r.is_ok() ? r.value() : QVector<RunSchedule>{};
        if (r.is_err())
            LOG_WARN(kSchedTag, QStringLiteral("Loading schedules failed: %1").arg(QString::fromStdString(r.error())));
        auto cap_r = SettingsRepository::instance().get(QString::fromLatin1(kSchedCapKey));
        bool cap_ok = false;
        const double cap = cap_r.is_ok() ? cap_r.value().toDouble(&cap_ok) : 0.0;
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, list, cap, cap_ok]() {
                if (!self)
                    return;
                Impl* d = self->d_.get();
                d->list = list;
                if (cap_ok)
                    d->cap = cap;
                d->loaded = true;
                LOG_INFO(kSchedTag, QStringLiteral("Loaded %1 schedule(s)%2")
                                        .arg(list.size())
                                        .arg(d->headless ? QStringLiteral(" (headless run: none fires)") : QString()));
                emit self->schedules_changed();
                if (!d->headless)
                    d->evaluate_all(); // missed occurrences: once, within each schedule's window
            },
            Qt::QueuedConnection);
    });
}

bool TaskRunScheduler::is_loaded() const {
    return d_->loaded;
}

bool TaskRunScheduler::is_active() const {
    return !d_->headless;
}

QVector<RunSchedule> TaskRunScheduler::schedules() const {
    QVector<RunSchedule> out = d_->list;
    std::sort(out.begin(), out.end(), [](const RunSchedule& a, const RunSchedule& b) {
        return a.created_at_ms != b.created_at_ms ? a.created_at_ms < b.created_at_ms : a.id < b.id;
    });
    return out;
}

const RunSchedule* TaskRunScheduler::find_schedule(const QString& id) const {
    for (const RunSchedule& s : d_->list)
        if (s.id == id)
            return &s;
    return nullptr;
}

QString TaskRunScheduler::save_schedule(const RunSchedule& schedule, QString* error) {
    const qint64 now = tsr_now_ms();
    const QStringList problems = sl::validate_schedule(schedule, now);
    if (!problems.isEmpty()) {
        if (error)
            *error = problems.join(QLatin1Char(' '));
        return {};
    }
    RunSchedule s = schedule;
    s.name = s.name.trimmed().left(120);
    s.catch_up_minutes = std::clamp(s.catch_up_minutes, 0, sl::kMaxCatchUpMinutes);
    if (s.kind == QLatin1String(kScheduleInterval))
        s.interval_minutes = std::clamp(s.interval_minutes, sl::kMinIntervalMinutes, sl::kMaxIntervalMinutes);
    RunSchedule* existing = s.id.isEmpty() ? nullptr : d_->find(s.id);
    if (!existing) {
        s.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        s.created_at_ms = now;
        s.last_run_at_ms = 0;
        s.last_run_id.clear();
        s.last_status.clear();
        s.last_error.clear();
    } else {
        // What the user cannot edit stays as it is.
        s.created_at_ms = existing->created_at_ms;
        s.last_run_at_ms = existing->last_run_at_ms;
        s.last_run_id = existing->last_run_id;
        s.last_status = existing->last_status;
        s.last_error = existing->last_error;
    }
    s.updated_at_ms = now;
    s.next_run_at_ms = s.enabled ? sl::next_occurrence_ms(s, now) : 0;
    if (existing)
        *existing = s;
    else
        d_->list.append(s);
    d_->persist(s);
    LOG_INFO(kSchedTag, QStringLiteral("Schedule %1 saved (%2, next %3)")
                            .arg(s.id.left(8), s.kind,
                                 s.next_run_at_ms > 0 ? QDateTime::fromMSecsSinceEpoch(s.next_run_at_ms).toString(Qt::ISODate)
                                                      : QStringLiteral("none")));
    emit schedules_changed();
    d_->rearm();
    return s.id;
}

void TaskRunScheduler::delete_schedule(const QString& id) {
    const auto before = d_->list.size();
    d_->list.erase(std::remove_if(d_->list.begin(), d_->list.end(), [&id](const RunSchedule& s) { return s.id == id; }),
                   d_->list.end());
    if (d_->list.size() == before)
        return;
    d_->writer->start([id]() {
        auto r = TaskRunStore::instance().delete_schedule(id);
        if (r.is_err())
            LOG_WARN(kSchedTag, QStringLiteral("Deleting schedule %1 failed: %2").arg(id.left(8), QString::fromStdString(r.error())));
    });
    emit schedules_changed();
    d_->rearm();
}

void TaskRunScheduler::set_enabled(const QString& id, bool enabled) {
    RunSchedule* s = d_->find(id);
    if (!s || s->enabled == enabled)
        return;
    const qint64 now = tsr_now_ms();
    s->enabled = enabled;
    s->next_run_at_ms = enabled ? sl::next_occurrence_ms(*s, now) : 0;
    s->updated_at_ms = now;
    d_->persist(*s);
    emit schedules_changed();
    d_->rearm();
}

void TaskRunScheduler::resume_after_review(const QString& id) {
    RunSchedule* s = d_->find(id);
    if (!s || !s->paused_for_review)
        return;
    const qint64 now = tsr_now_ms();
    s->paused_for_review = false;
    s->next_run_at_ms = s->enabled ? sl::next_occurrence_ms(*s, now) : 0; // missed meanwhile: not caught up
    s->updated_at_ms = now;
    d_->persist(*s);
    emit schedules_changed();
    d_->rearm();
}

void TaskRunScheduler::run_schedule_now(const QString& id, QObject* ctx, StartCallback cb) {
    d_->fire(id, true, false, ctx, std::move(cb));
}

QStringList TaskRunScheduler::runs_of(const QString& schedule_id) const {
    QStringList out;
    auto& runs = TaskRunService::instance();
    for (const QString& id : runs.run_ids()) // newest first
        if (const TaskRun* r = runs.find_run(id); r && r->schedule_id == schedule_id)
            out << id;
    return out;
}

double TaskRunScheduler::daily_credit_cap() const {
    return d_->cap;
}

void TaskRunScheduler::set_daily_credit_cap(double credits) {
    d_->cap = std::max(0.0, credits);
    const QString value = QString::number(d_->cap, 'f', 2);
    d_->writer->start([value]() {
        auto r = SettingsRepository::instance().set(QString::fromLatin1(kSchedCapKey), value, QStringLiteral("agent_runs"));
        if (r.is_err())
            LOG_WARN(kSchedTag, QStringLiteral("Saving the daily cap failed: %1").arg(QString::fromStdString(r.error())));
    });
    emit schedules_changed();
}

double TaskRunScheduler::committed_today() const {
    const qint64 day_start = QDateTime(QDate::currentDate(), QTime(0, 0)).toMSecsSinceEpoch();
    double total = 0;
    auto& runs = TaskRunService::instance();
    for (const QString& id : runs.run_ids()) {
        const TaskRun* r = runs.find_run(id);
        if (!r || r->origin != QLatin1String(kOriginSchedule) || r->created_at_ms < day_start ||
            !sl::engine_spends_credits(r->engine))
            continue;
        // A run still going may spend up to its budget: that much is reserved.
        double spent = r->credits_spent + r->credits_unreported * kCloudTurnCredits;
        if (!is_terminal(r->status) && r->budget.max_credits > spent)
            spent = r->budget.max_credits;
        total += spent;
    }
    return total;
}

qint64 TaskRunScheduler::next_run_ms(const RunSchedule& schedule, qint64 after_ms) {
    if (!schedule.enabled)
        return 0;
    return sl::next_occurrence_ms(schedule, after_ms);
}

} // namespace fincept::agent_tasks
