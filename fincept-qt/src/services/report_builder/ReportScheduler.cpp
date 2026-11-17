// ReportScheduler.cpp — see ReportScheduler.h.

#include "services/report_builder/ReportScheduler.h"

#include "core/config/AppPaths.h"
#include "core/logging/Logger.h"
#include "services/report_builder/ReportRegenerator.h"
#include "ui/notifications/NotificationService.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QSaveFile>
#include <QThreadPool>
#include <QTimer>

namespace fincept::services {

namespace sch = ::fincept::report::schedule;

namespace {

constexpr const char* kRsTag = "Research"; // the log tag of report_builder (logging policy)
constexpr int kRsCheckMs = 60'000;
constexpr int kRsMaxRuns = 60;

QString rs_tr(const char* text, int n = -1) {
    return QCoreApplication::translate("fincept::services::ReportScheduler", text, nullptr, n);
}

} // namespace

ReportScheduler& ReportScheduler::instance() {
    static ReportScheduler* s = [] {
        auto* r = new ReportScheduler();
        if (auto* app = QCoreApplication::instance(); app && r->thread() != app->thread())
            r->moveToThread(app->thread());
        return r;
    }();
    return *s;
}

ReportScheduler::ReportScheduler() {
    timer_ = new QTimer(this);
    timer_->setInterval(kRsCheckMs);
    connect(timer_, &QTimer::timeout, this, &ReportScheduler::check);
    io_ = new QThreadPool(this);
    io_->setMaxThreadCount(1); // the file is written in order
}

QString ReportScheduler::store_path() {
    return AppPaths::data() + QStringLiteral("/report_schedules.json");
}

void ReportScheduler::start() {
    if (started_)
        return;
    started_ = true;
    session_start_ = QDateTime::currentDateTime();
    const QPointer<ReportScheduler> self(this);
    io_->start([self]() {
        QFile f(store_path());
        QByteArray json;
        if (f.open(QIODevice::ReadOnly))
            json = f.readAll();
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, json]() {
                if (!self)
                    return;
                self->load_into(json);
                self->timer_->start();
                self->check();
            },
            Qt::QueuedConnection);
    });
}

void ReportScheduler::load_into(const QByteArray& json) {
    loaded_ = true;
    const QJsonObject root = QJsonDocument::fromJson(json).object();
    schedules_.clear();
    for (const QJsonValue& v : root.value(QStringLiteral("schedules")).toArray()) {
        const sch::Schedule s = sch::Schedule::from_json(v.toObject());
        if (s.report_id > 0)
            schedules_.append(s);
    }
    runs_.clear();
    for (const QJsonValue& v : root.value(QStringLiteral("runs")).toArray())
        runs_.append(sch::RunRecord::from_json(v.toObject()));
    if (!schedules_.isEmpty())
        LOG_INFO(kRsTag, logf("Report schedules loaded", {{"count", schedules_.size()}}));
    emit changed();
}

void ReportScheduler::save() {
    if (!loaded_)
        return; // never overwrite a file we have not read
    QJsonArray s, r;
    for (const sch::Schedule& x : schedules_)
        s.append(x.to_json());
    for (const sch::RunRecord& x : runs_)
        r.append(x.to_json());
    const QByteArray json = QJsonDocument(QJsonObject{{QStringLiteral("schedules"), s}, {QStringLiteral("runs"), r},
                                                      {QStringLiteral("v"), 1}})
                                .toJson(QJsonDocument::Indented);
    io_->start([json]() {
        QSaveFile f(store_path());
        if (!f.open(QIODevice::WriteOnly) || f.write(json) != json.size() || !f.commit())
            LOG_WARN(kRsTag, logf("Report schedules could not be saved", {{"path", log_path(store_path())}}));
    });
}

bool ReportScheduler::schedule_for(qint64 report_id, sch::Schedule* out) const {
    for (const sch::Schedule& s : schedules_) {
        if (s.report_id == report_id) {
            if (out)
                *out = s;
            return true;
        }
    }
    return false;
}

void ReportScheduler::set_schedule(const sch::Schedule& s) {
    if (s.report_id <= 0)
        return;
    bool replaced = false;
    for (sch::Schedule& x : schedules_) {
        if (x.report_id == s.report_id) {
            sch::Schedule n = s;
            n.last_run = x.last_run; // the editor never moves the last run
            x = n;
            replaced = true;
        }
    }
    if (!replaced)
        schedules_.append(s);
    LOG_INFO(kRsTag, logf("Report schedule saved", {{"report", s.report_id},
                                                    {"when", sch::describe(s)},
                                                    {"formats", s.formats.join(QLatin1Char(','))},
                                                    {"enabled", s.enabled}}));
    save();
    emit changed();
}

void ReportScheduler::remove_schedule(qint64 report_id) {
    for (int i = 0; i < schedules_.size(); ++i) {
        if (schedules_[i].report_id == report_id) {
            schedules_.removeAt(i);
            LOG_INFO(kRsTag, logf("Report schedule removed", {{"report", report_id}}));
            save();
            emit changed();
            return;
        }
    }
}

QVector<sch::RunRecord> ReportScheduler::runs(qint64 report_id) const {
    if (report_id <= 0)
        return runs_;
    QVector<sch::RunRecord> out;
    for (const sch::RunRecord& r : runs_)
        if (r.report_id == report_id)
            out.append(r);
    return out;
}

QDateTime ReportScheduler::next_run(qint64 report_id) const {
    sch::Schedule s;
    if (!schedule_for(report_id, &s) || !s.enabled)
        return {};
    return sch::next_slot(s, QDateTime::currentDateTime());
}

bool ReportScheduler::is_queued_or_running(qint64 report_id) const {
    if (running_ == report_id && report_id > 0)
        return true;
    for (const auto& q : queue_)
        if (q.first == report_id)
            return true;
    return false;
}

void ReportScheduler::run_now(qint64 report_id) {
    enqueue(report_id, QStringLiteral("manual"));
}

void ReportScheduler::check() {
    if (!loaded_)
        return;
    const QDateTime now = QDateTime::currentDateTime();
    for (const sch::Schedule& s : schedules_) {
        if (is_queued_or_running(s.report_id))
            continue;
        const sch::Decision d = sch::decide(s, now, session_start_, ran_on_start_.contains(s.report_id));
        switch (d) {
            case sch::Decision::NotDue:
                break;
            case sch::Decision::Due:
                enqueue(s.report_id, QStringLiteral("scheduled"));
                break;
            case sch::Decision::Missed:
                enqueue(s.report_id, QStringLiteral("missed"));
                break;
            case sch::Decision::OnStart:
                ran_on_start_.insert(s.report_id);
                enqueue(s.report_id, QStringLiteral("start"));
                break;
        }
    }
}

void ReportScheduler::enqueue(qint64 report_id, const QString& trigger) {
    if (report_id <= 0 || is_queued_or_running(report_id))
        return;
    queue_.append({report_id, trigger});
    pump();
}

void ReportScheduler::pump() {
    if (running_ > 0 || queue_.isEmpty())
        return;
    const auto [report_id, trigger] = queue_.takeFirst();
    sch::Schedule s;
    if (!schedule_for(report_id, &s)) {
        pump();
        return;
    }
    running_ = report_id;
    emit run_started(report_id);
    emit changed();
    RegenerateRequest req;
    req.report_id = report_id;
    req.refresh_data = s.refresh_data;
    req.formats = s.formats;
    req.folder = s.folder;
    req.trigger = trigger;
    ReportRegenerator::run(req, this, [this, report_id](const sch::RunRecord& r) {
        running_ = 0;
        for (sch::Schedule& x : schedules_)
            if (x.report_id == report_id)
                x.last_run = r.started; // the slot it ran for is done, whatever the outcome
        runs_.prepend(r);
        while (runs_.size() > kRsMaxRuns)
            runs_.removeLast();
        save();
        const QString title = r.title.isEmpty() ? rs_tr("Report") : r.title;
        if (r.ok)
            ui::ToastService::instance().post(
                r.warnings.isEmpty() ? ui::ToastService::Severity::Success : ui::ToastService::Severity::Warning,
                rs_tr("Scheduled report “%1” written: %n file(s)", static_cast<int>(r.files.size())).arg(title),
                QStringLiteral("report_builder"));
        else
            ui::ToastService::instance().post(ui::ToastService::Severity::Error,
                                              rs_tr("Scheduled report “%1” failed: %2").arg(title, r.error),
                                              QStringLiteral("report_builder"));
        emit run_finished(r);
        emit changed();
        pump();
    });
}

} // namespace fincept::services
