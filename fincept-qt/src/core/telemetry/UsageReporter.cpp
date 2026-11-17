#include "core/telemetry/UsageReporter.h"

#include "app/DockScreenRouter.h"
#include "auth/AuthManager.h"
#include "auth/InactivityGuard.h"
#include "core/config/AppPaths.h"
#include "core/logging/Logger.h"
#include "core/telemetry/ClientInfo.h"
#include "core/telemetry/UsageScrub.h"
#include "network/http/HttpClient.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaEnum>
#include <QMutex>
#include <QMutexLocker>
#include <QNetworkReply>
#include <QSaveFile>
#include <QTimeZone>
#include <QUrl>
#include <QWindow>

#include <algorithm>
#include <atomic>
#include <utility>

// Helpers are prefixed ur_ (unity builds batch this file with others).

namespace fincept::telemetry {

namespace {

constexpr const char* kUrTag = "Usage";
constexpr const char* kUrEndpoint = "/client/usage";
constexpr int kUrMaxScreens = 80;
constexpr int kUrMaxIssues = 50;
constexpr int kUrMaxSavedRuns = 5;
constexpr qint64 kUrMinGapMs = 60 * 1000;  ///< at most one report a minute, whatever the reason
constexpr qint64 kUrMinTimerMs = 60 * 1000; ///< the activity timer never fires more often
constexpr int kUrDefaultIntervalS = 900;
constexpr int kUrMinIntervalS = 300;
constexpr int kUrMaxIntervalS = 86400;
constexpr int kUrMaxBackoffS = 3600;

/// Set while the reporter runs; the hooks read it on any thread.
std::atomic<bool> g_ur_running{false};

struct UrIssue {
    QString kind;
    QString where;
    QString code;
    QString sample;
    qint64 count = 0;
    quint64 order = 0; ///< when first seen; the older one wins a tie when the list is full
};
using UrIssueMap = QHash<QString, UrIssue>;

/// Issues arrive from any thread. The book lives outside the reporter and is
/// never destroyed, so a late note from a worker during exit cannot touch a
/// destroyed object.
struct UrIssueBook {
    QMutex mutex;
    UrIssueMap pending;
    UrIssueMap inflight;
    quint64 next_order = 0;
};

UrIssueBook& ur_book() {
    static auto* book = new UrIssueBook; // intentionally leaked (see above)
    return *book;
}

/// Add `issue` to `map` (counts add up, the newer sample wins). Past
/// kUrMaxIssues the least frequent entry is dropped.
void ur_add_issue(UrIssueMap& map, const UrIssue& issue) {
    const QString key = issue.kind + QChar(0x1f) + issue.where + QChar(0x1f) + issue.code;
    auto it = map.find(key);
    if (it != map.end()) {
        it->count += issue.count;
        if (!issue.sample.isEmpty())
            it->sample = issue.sample;
        it->order = std::min(it->order, issue.order);
        return;
    }
    map.insert(key, issue);
    if (map.size() <= kUrMaxIssues)
        return;
    auto victim = map.begin();
    for (auto i = map.begin(); i != map.end(); ++i) {
        if (i->count < victim->count || (i->count == victim->count && i->order > victim->order))
            victim = i;
    }
    map.erase(victim);
}

/// Past kUrMaxScreens the screen with the least time (then fewest opens) goes.
void ur_cap_screens(QHash<QString, UsageScreenDelta>& map) {
    while (map.size() > kUrMaxScreens) {
        auto victim = map.begin();
        for (auto i = map.begin(); i != map.end(); ++i) {
            if (i->active_ms < victim->active_ms || (i->active_ms == victim->active_ms && i->opens < victim->opens))
                victim = i;
        }
        map.erase(victim);
    }
}

QJsonObject ur_issue_json(const UrIssue& i) {
    return QJsonObject{{QStringLiteral("kind"), i.kind},     {QStringLiteral("where"), i.where},
                       {QStringLiteral("code"), i.code},     {QStringLiteral("count"), i.count},
                       {QStringLiteral("sample"), i.sample}};
}

/// "<base>#dup<N>" panels are the same screen.
QString ur_screen_key(const QString& screen_id) {
    return scrub_label(screen_id.section(QLatin1Char('#'), 0, 0), 64);
}

qint64 ur_now_ms() {
    return QDateTime::currentMSecsSinceEpoch();
}

QString ur_iso_utc(qint64 ms) {
    return QDateTime::fromMSecsSinceEpoch(ms, QTimeZone::UTC).toString(Qt::ISODate);
}

/// The window half of InactivityGuard::user_active(): a top-level window is
/// shown and not minimised (always true without a GUI).
bool ur_window_visible() {
    if (!qobject_cast<QGuiApplication*>(QCoreApplication::instance()))
        return true;
    const auto windows = QGuiApplication::topLevelWindows();
    return std::any_of(windows.cbegin(), windows.cend(), [](const QWindow* w) {
        const Qt::WindowType type = w->type();
        const bool frame = type == Qt::Window || type == Qt::Dialog || type == Qt::Tool;
        return frame && w->isVisible() && !(w->windowStates() & Qt::WindowMinimized);
    });
}

/// --smoke-test / --selftest* / --dump-tools, or no window system at all.
bool ur_headless_run() {
    const QStringList args = QCoreApplication::arguments();
    for (const QString& a : args) {
        if (a == QLatin1String("--smoke-test") || a.startsWith(QLatin1String("--selftest")) ||
            a == QLatin1String("--dump-tools"))
            return true;
    }
    const QString platform = QGuiApplication::platformName();
    return platform == QLatin1String("offscreen") || platform == QLatin1String("minimal");
}

} // namespace

UsageReporter& UsageReporter::instance() {
    static UsageReporter s;
    return s;
}

// ── Lifecycle ────────────────────────────────────────────────────────────────

void UsageReporter::start(const QString& previous_exit) {
    if (running_ || !QCoreApplication::instance())
        return;
    if (ur_headless_run()) {
        LOG_DEBUG(kUrTag, "Non-interactive run — usage report off");
        return;
    }
    running_ = true;
    previous_exit_ = previous_exit;
    started_ms_ = ur_now_ms();
    ClientInfo::prime();

    auto& guard = auth::InactivityGuard::instance();
    guard.ensure_input_tracking(); // idempotent; SessionGuard asks for it too
    last_accrue_ms_ = started_ms_;
    last_input_ms_ = started_ms_ - guard.ms_since_input();
    gate_open_ = !guard.is_terminal_locked() && ur_window_visible();

    idle_timer_.setSingleShot(true);
    idle_timer_.setTimerType(Qt::VeryCoarseTimer);
    connect(&idle_timer_, &QTimer::timeout, this, &UsageReporter::on_activity_changed);
    connect(&guard, &auth::InactivityGuard::terminal_locked_changed, this, [this](bool) { on_activity_changed(); });
    if (auto* gui = qobject_cast<QGuiApplication*>(QCoreApplication::instance()))
        connect(gui, &QGuiApplication::applicationStateChanged, this,
                [this](Qt::ApplicationState) { on_activity_changed(); });
    auto& auth_mgr = auth::AuthManager::instance();
    connect(&auth_mgr, &auth::AuthManager::auth_state_changed, this, &UsageReporter::on_auth_state_changed);
    connect(&auth_mgr, &auth::AuthManager::login_succeeded, this, [this]() {
        pending_reason_ = QStringLiteral("login");
        authenticated_ = true; // the auth_state_changed that follows is this same sign-in
        maybe_send();
    });

    g_ur_running.store(true, std::memory_order_release);
    if (previous_exit_ == QLatin1String("crash"))
        note_issue(QStringLiteral("crash"), QStringLiteral("previous_run"), QString());
    arm_activity_watch();
    LOG_DEBUG(kUrTag, "Usage report on");
}

void UsageReporter::shutdown() {
    if (!running_)
        return;
    accrue();
    g_ur_running.store(false, std::memory_order_release);
    set_input_watch(false);
    idle_timer_.stop();
    // A report still on the wire is abandoned with the network stack; its fate
    // is unknown, so it goes into the saved file and the next start sends it.
    if (in_flight_ && inflight_current_)
        merge_back();
    in_flight_ = false;
    if (!disabled_)
        save_unsent(/*final_report=*/true);
    running_ = false;
}

// ── Hooks ────────────────────────────────────────────────────────────────────

void UsageReporter::screen_visibility(DockScreenRouter* router, const QString& screen_id, bool visible, bool bulk) {
    if (!router || !g_ur_running.load(std::memory_order_acquire))
        return;
    instance().on_screen_visibility(router, screen_id, visible, bulk);
}

void UsageReporter::note_issue(const QString& kind, const QString& where, const QString& code,
                               const QString& sample) {
    if (!g_ur_running.load(std::memory_order_acquire))
        return;
    UrIssue issue;
    issue.kind = scrub_label(kind, 24);
    if (issue.kind.isEmpty())
        return;
    issue.where = scrub_label(where, 80);
    issue.code = scrub_label(code, 40);
    issue.sample = scrub_sample(sample, 300);
    issue.count = 1;
    UrIssueBook& book = ur_book();
    QMutexLocker lock(&book.mutex);
    issue.order = ++book.next_order;
    ur_add_issue(book.pending, issue);
}

void UsageReporter::note_ui_stall(const QString& context, qint64 duration_ms) {
    if (!g_ur_running.load(std::memory_order_acquire))
        return;
    QString where = context;
    if (where.startsWith(QLatin1String("screen=")))
        where = where.mid(7).section(QLatin1Char('#'), 0, 0);
    const char* bucket = duration_ms < 250    ? "<250ms"
                         : duration_ms < 500  ? "250-500ms"
                         : duration_ms < 1000 ? "0.5-1s"
                         : duration_ms < 3000 ? "1-3s"
                                              : ">3s";
    note_issue(QStringLiteral("ui_stall"), where, QString::fromLatin1(bucket));
}

void UsageReporter::note_python_error(const QString& script, const QString& code, const QString& error_text) {
    if (!g_ur_running.load(std::memory_order_acquire))
        return;
    // The file name only: never a folder (it may carry the user's name) or arguments.
    const qsizetype cut = std::max(script.lastIndexOf(QLatin1Char('/')), script.lastIndexOf(QLatin1Char('\\')));
    const QString file = script.mid(cut + 1).section(QLatin1Char(' '), 0, 0);
    note_issue(QStringLiteral("python_error"), file, code, error_text);
}

void UsageReporter::note_http_error(const QUrl& url, int status, int network_error, const QString& error_text) {
    if (!g_ur_running.load(std::memory_order_acquire))
        return;
    const QString route = scrub_route(url);
    if (route.endsWith(QLatin1String(kUrEndpoint)))
        return; // the report's own failures are handled by its retry, not counted
    QString code;
    if (status > 0) {
        code = QStringLiteral("status=%1").arg(status);
    } else {
        const char* name = QMetaEnum::fromType<QNetworkReply::NetworkError>().valueToKey(network_error);
        code = QStringLiteral("net=") + (name ? QString::fromLatin1(name) : QString::number(network_error));
    }
    note_issue(QStringLiteral("http_error"), route, code, error_text);
}

// ── Screens ──────────────────────────────────────────────────────────────────

void UsageReporter::watch_router(DockScreenRouter* router) {
    visible_.insert(router, {});
    connect(router, &DockScreenRouter::screen_changed, this,
            [this, router](const QString& id) { on_screen_changed(router, id); });
    // `router` is only a key here: never dereferenced once it is going away.
    connect(router, &QObject::destroyed, this, [this, router]() {
        accrue();
        visible_.remove(router);
    });
}

void UsageReporter::on_screen_visibility(DockScreenRouter* router, const QString& screen_id, bool visible,
                                         bool bulk) {
    accrue(); // the elapsed span belongs to the screens visible until now
    if (!visible_.contains(router))
        watch_router(router);
    QHash<QString, bool>& shown = visible_[router];
    if (!visible) {
        shown.remove(screen_id);
        return;
    }
    if (shown.contains(screen_id))
        return;
    // Shown while the router places or registers panels: an open only if a
    // navigation to it follows (on_screen_changed); restored panels are not.
    shown.insert(screen_id, !bulk);
    if (!bulk)
        count_open(screen_id);
}

void UsageReporter::on_screen_changed(DockScreenRouter* router, const QString& screen_id) {
    accrue();
    QHash<QString, bool>& shown = visible_[router];
    auto it = shown.find(screen_id);
    if (it == shown.end()) {
        shown.insert(screen_id, true);
        count_open(screen_id);
    } else if (!it.value()) {
        it.value() = true;
        count_open(screen_id);
    }
    maybe_send();
}

void UsageReporter::count_open(const QString& screen_id) {
    const QString key = ur_screen_key(screen_id);
    if (key.isEmpty())
        return;
    ++screens_[key].opens;
    ur_cap_screens(screens_);
}

// ── Time ─────────────────────────────────────────────────────────────────────

void UsageReporter::accrue() {
    if (!running_)
        return;
    auto& guard = auth::InactivityGuard::instance();
    const qint64 now = ur_now_ms();
    const qint64 from = last_accrue_ms_;
    const qint64 prev_input = last_input_ms_;
    const qint64 input = std::max(prev_input, now - guard.ms_since_input());
    const bool counted = gate_open_; // lock / window state held for the whole span (changes call accrue())
    last_accrue_ms_ = now;
    last_input_ms_ = input;
    gate_open_ = !guard.is_terminal_locked() && ur_window_visible();
    if (!counted || now <= from)
        return;

    // The user is "active" until kUserIdleAfterMs after an input. Inputs seen
    // only as the latest timestamp: when it moved by less than the idle window
    // there was no idle gap; otherwise the gap's end is not known and only the
    // part after the latest input counts (never more than really happened).
    const qint64 idle = auth::InactivityGuard::kUserIdleAfterMs;
    qint64 active = 0;
    if (input > prev_input && input <= prev_input + idle) {
        active = std::min(now, input + idle) - from;
    } else {
        active = std::max<qint64>(0, std::min(now, prev_input + idle) - from);
        if (input > prev_input)
            active += std::max<qint64>(0, std::min(now, input + idle) - std::max(from, input));
    }
    active = std::clamp<qint64>(active, 0, now - from);
    if (active == 0)
        return;
    active_ms_ += active;

    qsizetype shown = 0;
    for (const auto& per_router : std::as_const(visible_))
        shown += per_router.size();
    if (shown == 0)
        return;
    const qint64 share = active / shown; // equal split: the sum never exceeds `active`
    if (share == 0)
        return;
    for (const auto& per_router : std::as_const(visible_)) {
        for (auto s = per_router.cbegin(); s != per_router.cend(); ++s) {
            const QString key = ur_screen_key(s.key());
            if (!key.isEmpty())
                screens_[key].active_ms += share;
        }
    }
    ur_cap_screens(screens_);
}

void UsageReporter::arm_activity_watch() {
    if (!running_)
        return;
    const qint64 since = auth::InactivityGuard::instance().ms_since_input();
    const qint64 idle = auth::InactivityGuard::kUserIdleAfterMs;
    if (since < idle) {
        // Wake when the user would turn idle (accrue() caps at that moment even
        // when the timer is late), never more often than once a minute.
        set_input_watch(false);
        idle_timer_.start(static_cast<int>(std::max<qint64>(kUrMinTimerMs, idle - since + 1000)));
    } else {
        // Idle: no timer at all; the first input brings us back.
        idle_timer_.stop();
        set_input_watch(true);
    }
}

void UsageReporter::set_input_watch(bool on) {
    if (on == watching_input_ || !QCoreApplication::instance())
        return;
    watching_input_ = on;
    input_seen_ = false;
    if (on)
        QCoreApplication::instance()->installEventFilter(this);
    else
        QCoreApplication::instance()->removeEventFilter(this);
}

bool UsageReporter::eventFilter(QObject* watched, QEvent* event) {
    if (watching_input_ && !input_seen_) {
        switch (event->type()) {
            case QEvent::MouseMove:
            case QEvent::MouseButtonPress:
            case QEvent::KeyPress:
            case QEvent::Wheel:
            case QEvent::TouchBegin:
            case QEvent::ApplicationActivate:
                // InactivityGuard's filter (installed earlier) runs after this
                // one: settle once the event is through and its timestamp moved.
                input_seen_ = true;
                QTimer::singleShot(0, this, &UsageReporter::on_activity_changed);
                break;
            default:
                break;
        }
    }
    return QObject::eventFilter(watched, event);
}

void UsageReporter::on_activity_changed() {
    accrue();
    arm_activity_watch();
    maybe_send();
}

void UsageReporter::on_auth_state_changed() {
    const auto& auth_mgr = auth::AuthManager::instance();
    const bool authed = auth_mgr.is_authenticated();
    // Signed in after having been signed out in this run: a login, whichever
    // flow (password, OTP, MFA, Google) got there.
    if (authed && !authenticated_ && seen_signed_out_)
        pending_reason_ = QStringLiteral("login");
    if (!authed && !auth_mgr.is_loading())
        seen_signed_out_ = true;
    authenticated_ = authed;
    if (authed)
        maybe_send();
}

// ── Sending ──────────────────────────────────────────────────────────────────

void UsageReporter::maybe_send() {
    if (!running_ || disabled_ || in_flight_)
        return;
    // Signed in for real (not while a saved session is still being checked),
    // someone at the terminal, and the API not refusing this IP.
    const auto& auth_mgr = auth::AuthManager::instance();
    if (!auth_mgr.is_authenticated() || auth_mgr.is_loading() || !auth::InactivityGuard::instance().user_active() ||
        HttpClient::instance().is_ip_blocked())
        return;
    const qint64 now = ur_now_ms();
    if (now < retry_not_before_ms_ || (last_attempt_ms_ > 0 && now - last_attempt_ms_ < kUrMinGapMs))
        return;
    if (!saved_runs_scanned_)
        scan_saved_runs();
    if (!saved_runs_.isEmpty()) {
        send_saved_run(saved_runs_.first()); // an earlier run's "quit" report goes first
        return;
    }
    QString reason = pending_reason_;
    if (reason.isEmpty()) {
        if (now - last_report_ms_ < qint64(interval_s_) * 1000)
            return;
        reason = QStringLiteral("periodic");
    }
    send_current(reason);
}

void UsageReporter::send_current(const QString& reason) {
    accrue();
    const qint64 now = ur_now_ms();

    // Whole seconds go out; the remainder stays for the next report.
    inflight_active_s_ = active_ms_ / 1000;
    active_ms_ %= 1000;
    inflight_screens_.clear();
    QJsonArray screens;
    for (auto it = screens_.begin(); it != screens_.end();) {
        const qint64 secs = it->active_ms / 1000;
        if (secs > 0 || it->opens > 0) {
            inflight_screens_.insert(it.key(), UsageScreenDelta{secs * 1000, it->opens});
            screens.append(QJsonObject{{QStringLiteral("id"), it.key()},
                                       {QStringLiteral("active_s"), secs},
                                       {QStringLiteral("opens"), it->opens}});
            it->active_ms -= secs * 1000;
            it->opens = 0;
        }
        if (it->active_ms == 0 && it->opens == 0)
            it = screens_.erase(it);
        else
            ++it;
    }
    QJsonArray issues;
    {
        UrIssueBook& book = ur_book();
        QMutexLocker lock(&book.mutex);
        book.inflight = std::exchange(book.pending, UrIssueMap{});
        for (const UrIssue& i : std::as_const(book.inflight))
            issues.append(ur_issue_json(i));
    }

    if (reason == QLatin1String("periodic") && inflight_active_s_ == 0 && screens.isEmpty() && issues.isEmpty()) {
        last_report_ms_ = now; // nothing happened since the last report: nothing to say
        return;
    }

    const QJsonObject run{{QStringLiteral("started_at"), ur_iso_utc(started_ms_)},
                          {QStringLiteral("uptime_s"), (now - started_ms_) / 1000},
                          {QStringLiteral("active_s"), inflight_active_s_},
                          {QStringLiteral("previous_exit"), previous_exit_}};
    const QJsonObject body{{QStringLiteral("v"), 1},
                           {QStringLiteral("install_id"), ClientInfo::install_id()},
                           {QStringLiteral("run_id"), ClientInfo::run_id()},
                           {QStringLiteral("seq"), seq_++},
                           {QStringLiteral("reason"), reason},
                           {QStringLiteral("client"), ClientInfo::client_json()},
                           {QStringLiteral("run"), run},
                           {QStringLiteral("screens"), screens},
                           {QStringLiteral("issues"), issues}};
    in_flight_ = true;
    inflight_current_ = true;
    last_attempt_ms_ = now;
    HttpClient::instance().post(
        QString::fromLatin1(kUrEndpoint), body,
        [this](Result<QJsonDocument> r) { on_current_answer(r.is_ok(), r.is_ok() ? r.value() : QJsonDocument()); },
        this);
}

void UsageReporter::on_current_answer(bool ok, const QJsonDocument& doc) {
    if (!running_ || !in_flight_)
        return; // shut down meanwhile: the batch is already in the saved file
    in_flight_ = false;
    inflight_current_ = false;
    // The HTTP status of this answer (0 = no answer), whatever the body was.
    const int status = HttpClient::response_status();
    const qint64 now = ur_now_ms();

    if (status >= 400 && status < 500) {
        // Refused: retrying the same batch would only be refused again.
        LOG_DEBUG(kUrTag, QStringLiteral("Usage report refused (HTTP %1) — dropped").arg(status));
        inflight_active_s_ = 0;
        inflight_screens_.clear();
        {
            UrIssueBook& book = ur_book();
            QMutexLocker lock(&book.mutex);
            book.inflight.clear();
        }
        pending_reason_.clear();
        last_report_ms_ = now;
        QFile::remove(own_pending_path());
        if (status == 404) {
            disabled_ = true; // a server without the endpoint (self-hosted): quiet for this run
            LOG_DEBUG(kUrTag, "The API has no /client/usage — usage report off for this run");
        }
        return;
    }
    if (status < 200 || status >= 300) {
        LOG_DEBUG(kUrTag, QStringLiteral("Usage report not delivered (HTTP %1) — kept for the next one").arg(status));
        merge_back();
        back_off();
        save_unsent(/*final_report=*/false); // in case this run does not get to quit cleanly
        return;
    }

    inflight_active_s_ = 0;
    inflight_screens_.clear();
    {
        UrIssueBook& book = ur_book();
        QMutexLocker lock(&book.mutex);
        book.inflight.clear();
    }
    pending_reason_.clear();
    last_report_ms_ = now;
    backoff_s_ = 0;
    retry_not_before_ms_ = 0;
    QFile::remove(own_pending_path());

    int next_s = kUrDefaultIntervalS;
    if (ok && doc.isObject()) {
        const QJsonObject o = doc.object();
        QJsonValue v = o.value(QStringLiteral("next_s"));
        if (!v.isDouble() && o.value(QStringLiteral("data")).isObject())
            v = o.value(QStringLiteral("data")).toObject().value(QStringLiteral("next_s"));
        if (v.isDouble())
            next_s = v.toInt(kUrDefaultIntervalS);
    }
    interval_s_ = std::clamp(next_s, kUrMinIntervalS, kUrMaxIntervalS);
    LOG_DEBUG(kUrTag, QStringLiteral("Usage report sent; next in >= %1 s").arg(interval_s_));
}

void UsageReporter::merge_back() {
    active_ms_ += inflight_active_s_ * 1000;
    inflight_active_s_ = 0;
    for (auto it = inflight_screens_.cbegin(); it != inflight_screens_.cend(); ++it) {
        UsageScreenDelta& d = screens_[it.key()];
        d.active_ms += it->active_ms;
        d.opens += it->opens;
    }
    inflight_screens_.clear();
    ur_cap_screens(screens_);
    UrIssueBook& book = ur_book();
    QMutexLocker lock(&book.mutex);
    UrIssueMap merged = std::exchange(book.inflight, UrIssueMap{});
    for (const UrIssue& i : std::as_const(book.pending))
        ur_add_issue(merged, i); // the newer samples win
    book.pending = std::move(merged);
}

void UsageReporter::back_off() {
    backoff_s_ = backoff_s_ == 0 ? 60 : std::min(backoff_s_ * 2, kUrMaxBackoffS);
    retry_not_before_ms_ = ur_now_ms() + qint64(backoff_s_) * 1000;
}

void UsageReporter::send_saved_run(const QString& path) {
    QJsonObject saved;
    {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly) && f.size() < 1024 * 1024)
            saved = QJsonDocument::fromJson(f.readAll()).object();
    }
    if (saved.value(QStringLiteral("run_id")).toString().isEmpty()) {
        LOG_DEBUG(kUrTag, logf("Unsent usage file unreadable, discarded", {{"path", log_path(path)}}));
        QFile::remove(path); // unreadable: nothing to say about that run
        saved_runs_.removeAll(path);
        return;
    }
    QJsonObject body = saved;
    body.remove(QStringLiteral("next_seq"));
    body.remove(QStringLiteral("final"));
    body.insert(QStringLiteral("v"), 1);
    body.insert(QStringLiteral("seq"), saved.value(QStringLiteral("next_seq")).toInt(1));
    body.insert(QStringLiteral("reason"), QStringLiteral("quit"));

    in_flight_ = true;
    inflight_current_ = false;
    last_attempt_ms_ = ur_now_ms();
    HttpClient::instance().post(
        QString::fromLatin1(kUrEndpoint), body,
        [this, path](Result<QJsonDocument>) {
            if (!running_ || !in_flight_)
                return;
            in_flight_ = false;
            const int status = HttpClient::response_status();
            if (status < 200 || status >= 500) {
                LOG_DEBUG(kUrTag, QStringLiteral("Earlier run's report not delivered (HTTP %1)").arg(status));
                back_off();
                return;
            }
            // Delivered, or refused for good: either way that run is done.
            QFile::remove(path);
            saved_runs_.removeAll(path);
            if (status == 404) {
                disabled_ = true;
                LOG_DEBUG(kUrTag, "The API has no /client/usage — usage report off for this run");
                return;
            }
            backoff_s_ = 0;
            retry_not_before_ms_ = 0;
            last_attempt_ms_ = 0; // this run's own report may follow right away
            QTimer::singleShot(0, this, &UsageReporter::maybe_send);
        },
        this);
}

// ── Saved reports ────────────────────────────────────────────────────────────

QString UsageReporter::pending_dir() const {
    // Per profile, like everything under AppPaths::data(): two profiles running
    // at once (each its own process) never pick up each other's files.
    return AppPaths::data() + QStringLiteral("/usage");
}

QString UsageReporter::own_pending_path() const {
    return pending_dir() + QStringLiteral("/usage-pending-") + ClientInfo::run_id() + QStringLiteral(".json");
}

void UsageReporter::scan_saved_runs() {
    saved_runs_scanned_ = true;
    const QDir dir(pending_dir());
    if (!dir.exists())
        return;
    const QString own = QFileInfo(own_pending_path()).fileName();
    const QFileInfoList files = dir.entryInfoList({QStringLiteral("usage-pending-*.json")}, QDir::Files,
                                                  QDir::Time | QDir::Reversed); // oldest first
    for (const QFileInfo& fi : files) {
        if (fi.fileName() != own)
            saved_runs_ << fi.absoluteFilePath();
    }
    while (saved_runs_.size() > kUrMaxSavedRuns)
        QFile::remove(saved_runs_.takeFirst());
}

void UsageReporter::save_unsent(bool final_report) {
    const qint64 now = ur_now_ms();
    QJsonArray screens;
    for (auto it = screens_.cbegin(); it != screens_.cend(); ++it) {
        const qint64 secs = it->active_ms / 1000;
        if (secs > 0 || it->opens > 0)
            screens.append(QJsonObject{{QStringLiteral("id"), it.key()},
                                       {QStringLiteral("active_s"), secs},
                                       {QStringLiteral("opens"), it->opens}});
    }
    QJsonArray issues;
    {
        UrIssueBook& book = ur_book();
        QMutexLocker lock(&book.mutex);
        for (const UrIssue& i : std::as_const(book.pending))
            issues.append(ur_issue_json(i));
    }
    const QJsonObject run{{QStringLiteral("started_at"), ur_iso_utc(started_ms_)},
                          {QStringLiteral("uptime_s"), (now - started_ms_) / 1000},
                          {QStringLiteral("active_s"), active_ms_ / 1000},
                          {QStringLiteral("previous_exit"), previous_exit_}};
    const QJsonObject saved{{QStringLiteral("v"), 1},
                            {QStringLiteral("install_id"), ClientInfo::install_id()},
                            {QStringLiteral("run_id"), ClientInfo::run_id()},
                            {QStringLiteral("next_seq"), seq_},
                            {QStringLiteral("client"), ClientInfo::client_json()},
                            {QStringLiteral("run"), run},
                            {QStringLiteral("screens"), screens},
                            {QStringLiteral("issues"), issues},
                            {QStringLiteral("final"), final_report}};

    const QString dir_path = pending_dir();
    if (!QDir().mkpath(dir_path)) {
        LOG_DEBUG(kUrTag, "Cannot create the usage folder — unsent usage dropped");
        return;
    }
    QSaveFile file(own_pending_path());
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_DEBUG(kUrTag, "Cannot write the unsent usage file — dropped");
        return;
    }
    file.write(QJsonDocument(saved).toJson(QJsonDocument::Compact));
    if (!file.commit()) {
        LOG_DEBUG(kUrTag, "Cannot write the unsent usage file — dropped");
        return;
    }

    // Bounded: a terminal that is never signed in must not pile files up.
    QDir dir(dir_path);
    QFileInfoList files = dir.entryInfoList({QStringLiteral("usage-pending-*.json")}, QDir::Files,
                                            QDir::Time | QDir::Reversed); // oldest first
    while (files.size() > kUrMaxSavedRuns + 1)
        QFile::remove(files.takeFirst().absoluteFilePath());
}

} // namespace fincept::telemetry
