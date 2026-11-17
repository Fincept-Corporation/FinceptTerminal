// InstrumentSync.cpp — see InstrumentSync.h. The loop only; the production Ops and
// instance() live in InstrumentResolverSources.cpp. File-scope names carry an isy_ /
// kIsy prefix (unity builds).

#include "services/instruments/InstrumentSync.h"

#include "core/logging/Logger.h"

#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <utility>

namespace fincept::services {

namespace {

constexpr const char* kIsyTag = "InstrumentSync";

/// Back-off after the n-th consecutive failure (1-based), capped by the normal interval.
int isy_backoff_ms(int failures, int interval_ms) {
    static constexpr int kSteps[] = {5 * 60 * 1000, 30 * 60 * 1000, 2 * 60 * 60 * 1000};
    const int i = std::clamp(failures - 1, 0, 2);
    return std::min(kSteps[i], std::max(1, interval_ms));
}

} // namespace

InstrumentSync::InstrumentSync(Ops ops, QObject* parent) : QObject(parent), ops_(std::move(ops)) {
    timer_ = new QTimer(this);
    timer_->setSingleShot(true);
    connect(timer_, &QTimer::timeout, this, &InstrumentSync::run_cycle);
}

InstrumentSync::~InstrumentSync() = default;

void InstrumentSync::start() {
    if (ops_.start_index)
        ops_.start_index();
    if (started_)
        return;
    started_ = true;
    if (!disabled_)
        schedule(std::max(0, ops_.first_delay_ms));
}

void InstrumentSync::sync_now() {
    if (disabled_ || running_)
        return;
    timer_->stop();
    run_cycle();
}

void InstrumentSync::schedule(int ms) {
    if (disabled_)
        return;
    timer_->start(std::max(0, ms));
}

void InstrumentSync::disable(const QString& why) {
    if (disabled_)
        return;
    disabled_ = true;
    running_ = false;
    timer_->stop();
    LOG_INFO(kIsyTag, QStringLiteral("symbol master sync off for this session: %1").arg(why));
    emit endpoint_unavailable();
}

void InstrumentSync::run_cycle() {
    if (disabled_ || running_)
        return;
    if (!ops_.get || !ops_.read_cursor || !ops_.apply_delta) {
        disable(QStringLiteral("no transport"));
        return;
    }
    running_ = true;
    pages_ = 0;
    ops_.read_cursor(this, [this](const QString& cursor) {
        if (!running_)
            return;
        cursor_ = cursor;
        fetch(cursor_);
    });
}

void InstrumentSync::fetch(const QString& cursor) {
    QString path = changes_path() + QStringLiteral("?limit=%1").arg(std::max(1, ops_.page_limit));
    if (!cursor.isEmpty())
        path += QStringLiteral("&since=") + QString::fromUtf8(QUrl::toPercentEncoding(cursor));
    ops_.get(path, this, [this](const HttpReply& reply) { on_reply(reply); });
}

void InstrumentSync::on_reply(const HttpReply& reply) {
    if (!running_)
        return;
    if (reply.status == 404) {
        disable(QStringLiteral("the backend has no %1 endpoint yet").arg(changes_path()));
        return;
    }
    if (reply.status == 401 || reply.status == 403) {
        disable(QStringLiteral("the backend refused %1 (HTTP %2)").arg(changes_path()).arg(reply.status));
        return;
    }
    if (reply.status == 410 && !full_refetch_done_) {
        // The cursor is older than the backend keeps changes for: start from the beginning
        // (a page without "since" is accepted whatever cursor is stored).
        full_refetch_done_ = true;
        LOG_INFO(kIsyTag, QStringLiteral("sync cursor %1 expired; fetching the full symbol master").arg(cursor_));
        cursor_.clear();
        fetch(QString());
        return;
    }
    if (reply.status < 200 || reply.status >= 300 || !reply.body.isObject()) {
        LOG_WARN(kIsyTag, QStringLiteral("symbol master sync failed (HTTP %1): %2")
                              .arg(reply.status)
                              .arg(reply.error.left(200)));
        finish_run(false);
        return;
    }
    const QJsonObject delta = reply.body.object();
    if (!delta.contains(QStringLiteral("next"))) {
        LOG_WARN(kIsyTag, QStringLiteral("symbol master sync: the reply has no \"next\" cursor; ignored"));
        finish_run(false);
        return;
    }
    ops_.apply_delta(delta, this, [this](bool ok, const QString& cursor_or_error, bool has_more) {
        if (!running_)
            return;
        if (!ok) {
            LOG_WARN(kIsyTag, QStringLiteral("symbol master delta not applied: %1").arg(cursor_or_error.left(200)));
            finish_run(false);
            return;
        }
        ++pages_;
        const bool advanced = cursor_or_error != cursor_;
        cursor_ = cursor_or_error;
        if (has_more && advanced && pages_ < ops_.max_pages_per_run) {
            fetch(cursor_);
            return;
        }
        finish_run(true);
    });
}

void InstrumentSync::finish_run(bool ok) {
    running_ = false;
    if (ok) {
        failures_ = 0;
        full_refetch_done_ = false;
        if (pages_ > 0)
            LOG_INFO(kIsyTag, QStringLiteral("symbol master synced: %1 page(s), cursor %2").arg(pages_).arg(cursor_));
        emit sync_finished(pages_, cursor_);
        schedule(ops_.interval_ms);
        return;
    }
    ++failures_;
    emit sync_finished(pages_, cursor_);
    schedule(isy_backoff_ms(failures_, ops_.interval_ms));
}

} // namespace fincept::services
