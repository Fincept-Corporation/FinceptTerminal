// src/ui/widgets/AutoLoadPolicy.cpp — see the header (QtCore only).

#include "ui/widgets/AutoLoadPolicy.h"

#include <QCryptographicHash>
#include <QDeadlineTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>
#include <QThreadPool>
#include <QWaitCondition>

#include <algorithm>
#include <initializer_list>
#include <utility>

namespace fincept::ui::autoload {

bool is_stale(const QDateTime& as_of, const QDateTime& now, int max_age_seconds) {
    if (max_age_seconds < 0)
        return false;
    if (max_age_seconds == 0 || !as_of.isValid())
        return true;
    return as_of.secsTo(now) > max_age_seconds;
}

namespace {

// QJsonObject already iterates its keys in sorted order and compact output has
// no whitespace, so the text is canonical; arrays keep their order (it matters).
QByteArray al_canonical(const QJsonValue& v) {
    if (v.isObject())
        return QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact);
    if (v.isArray())
        return QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact);
    QJsonArray wrap;
    wrap.append(v);
    return QJsonDocument(wrap).toJson(QJsonDocument::Compact);
}

} // namespace

QString signature_of(const QJsonValue& inputs) {
    const QByteArray hash = QCryptographicHash::hash(al_canonical(inputs), QCryptographicHash::Sha1).toHex();
    return QString::fromLatin1(hash.left(16));
}

QString signature_of(const QVariantMap& inputs) {
    return signature_of(QJsonValue(QJsonObject::fromVariantMap(inputs)));
}

bool should_auto_run(const ViewFacts& f) {
    if (!f.auto_policy || !f.requirements_ok || f.running || f.attempted_this_visit)
        return false;
    if (f.forced || !f.has_result)
        return true;
    return f.stale && !f.signature_mismatch;
}

RunGate gate_run(bool has_start, bool requirements_ok) {
    if (!has_start)
        return RunGate::NoAction;
    return requirements_ok ? RunGate::Start : RunGate::NeedsSetup;
}

QString effective_agent_risk(const QString& pane_kind, const QString& view_kind, double list_cost, bool included) {
    const QString view = view_kind.trimmed();
    if (!view.isEmpty())
        return view; // the view's explicit kind
    const QString pane = pane_kind.trimmed();
    if (!pane.isEmpty())
        return pane; // the pane's explicit kind
    // The rule: a run that costs this account credits spends.
    const bool costs = list_cost > 0; // false for NaN too
    return costs && !included ? QStringLiteral("spend") : QString();
}

// ── Memory budget ────────────────────────────────────────────────────────────

QStringList choose_evictions(const QList<MemorySlot>& lru_first, qint64 budget_bytes) {
    QStringList out;
    if (budget_bytes <= 0)
        return out;
    qint64 total = 0;
    for (const MemorySlot& m : lru_first)
        total += std::max<qint64>(0, m.bytes);
    if (total <= budget_bytes)
        return out;
    // Pass 1: what comes back without a run. Pass 2: AutoRun results (a free re-run).
    for (int pass = 0; pass < 2 && total > budget_bytes; ++pass) {
        for (const MemorySlot& m : lru_first) {
            if (total <= budget_bytes)
                break;
            if (m.pinned || m.bytes <= 0)
                continue;
            const bool eligible = pass == 0 ? m.reloadable : (!m.reloadable && !m.manual);
            if (!eligible)
                continue;
            out.append(m.key);
            total -= m.bytes;
        }
    }
    return out;
}

qint64 approx_json_bytes(const QJsonValue& v) {
    constexpr qint64 kCell = 16; // a value slot in Qt's JSON / CBOR containers
    switch (v.type()) {
        case QJsonValue::String:
            return kCell + 2 * qint64(v.toString().size());
        case QJsonValue::Array: {
            qint64 n = kCell;
            const QJsonArray a = v.toArray();
            for (const QJsonValue& e : a)
                n += approx_json_bytes(e);
            return n;
        }
        case QJsonValue::Object: {
            qint64 n = kCell;
            const QJsonObject o = v.toObject();
            for (auto it = o.constBegin(); it != o.constEnd(); ++it)
                n += kCell + 2 * qint64(it.key().size()) + approx_json_bytes(it.value());
            return n;
        }
        case QJsonValue::Null:
        case QJsonValue::Bool:
        case QJsonValue::Double:
        case QJsonValue::Undefined:
            return kCell;
    }
    return kCell;
}

// ── The persisted copy, off the GUI thread ───────────────────────────────────

QByteArray serialize_remembered(const QJsonValue& payload, const QDateTime& as_of, const QString& signature) {
    QJsonObject o;
    o.insert(QStringLiteral("at"), as_of.toString(Qt::ISODate));
    o.insert(QStringLiteral("p"), payload);
    if (!signature.isEmpty())
        o.insert(QStringLiteral("sig"), signature);
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

RememberedRead parse_remembered(const QString& text, qint64 ceiling_chars) {
    RememberedRead r;
    if (text.isEmpty() || (ceiling_chars > 0 && text.size() > ceiling_chars))
        return r; // Miss
    const QByteArray utf8 = text.toUtf8();
    const QJsonDocument doc = QJsonDocument::fromJson(utf8);
    const QJsonObject o = doc.object();
    const QDateTime at = QDateTime::fromString(o.value(QStringLiteral("at")).toString(), Qt::ISODate);
    if (!doc.isObject() || !o.contains(QStringLiteral("p")) || !at.isValid()) {
        r.outcome = ReadOutcome::Corrupt;
        return r;
    }
    r.outcome = ReadOutcome::Hit;
    r.payload = o.value(QStringLiteral("p"));
    r.as_of = at;
    r.signature = o.value(QStringLiteral("sig")).toString();
    r.text_bytes = utf8.size();
    return r;
}

ReadVerdict decide_read(const ReadFacts& f) {
    if (f.started_gen != f.current_gen || f.started_view != f.current_view || !f.visible || f.run_for_view ||
        f.has_result)
        return ReadVerdict::Drop;
    return ReadVerdict::Accept;
}

qint64 bytes_from_json_text(qint64 utf8_bytes) {
    return std::max<qint64>(0, utf8_bytes) * 2;
}

qint64 shallow_json_bytes(const QJsonValue& v) {
    constexpr qint64 kCell = 16;
    constexpr qint64 kEntry = 64; // a typical row / field with its key, once parsed
    switch (v.type()) {
        case QJsonValue::String:
            return kCell + 2 * qint64(v.toString().size());
        case QJsonValue::Array:
            return kCell + kEntry * qint64(v.toArray().size()); // size() only: O(1), no walk
        case QJsonValue::Object:
            return kCell + kEntry * qint64(v.toObject().size());
        case QJsonValue::Null:
        case QJsonValue::Bool:
        case QJsonValue::Double:
        case QJsonValue::Undefined:
            return kCell;
    }
    return kCell;
}

// ── Write-behind queue ───────────────────────────────────────────────────────

WriteQueue::WriteQueue(int max_in_flight, int max_queued)
    : max_in_flight_(std::max(1, max_in_flight)), max_queued_(std::max(1, max_queued)) {}

void WriteQueue::enqueue(const QString& key, quint64 seq, quint64* replaced, quint64* dropped) {
    if (replaced)
        *replaced = 0;
    if (dropped)
        *dropped = 0;
    for (auto& entry : queue_) {
        if (entry.first == key) {
            // Latest payload wins; it keeps the place of the one it replaces.
            if (replaced)
                *replaced = entry.second;
            entry.second = seq;
            return;
        }
    }
    if (queue_.size() >= max_queued_) {
        const auto oldest = queue_.takeFirst();
        if (dropped)
            *dropped = oldest.second;
    }
    queue_.append({key, seq});
}

QList<QPair<QString, quint64>> WriteQueue::take_ready() {
    QList<QPair<QString, quint64>> out;
    for (int i = 0; i < queue_.size() && int(in_flight_.size()) < max_in_flight_;) {
        const auto entry = queue_.at(i);
        if (in_flight_.contains(entry.first)) {
            ++i; // that key's previous write is still running: it waits, in order
            continue;
        }
        queue_.removeAt(i);
        in_flight_.insert(entry.first, entry.second);
        out.append(entry);
    }
    return out;
}

void WriteQueue::finished(const QString& key) {
    in_flight_.remove(key);
}

quint64 WriteQueue::queued_seq(const QString& key) const {
    for (const auto& entry : queue_) {
        if (entry.first == key)
            return entry.second;
    }
    return 0;
}

QList<QPair<QString, quint64>> WriteQueue::drain() {
    return std::exchange(queue_, {});
}

/// What pool tasks share with the writer: the keys still running (for the bounded
/// wait at quit) and whether the writer still exists (for the queued callback).
struct WriteBehind::Shared {
    QMutex mutex;
    QWaitCondition changed;
    QSet<QString> running;
    WriteBehind* owner = nullptr; ///< null once the writer is destroyed
};

WriteBehind::WriteBehind(QThreadPool* pool, int max_in_flight, int max_queued, QObject* parent)
    : QObject(parent), pool_(pool ? pool : QThreadPool::globalInstance()), queue_(max_in_flight, max_queued),
      shared_(std::make_shared<Shared>()) {
    shared_->owner = this;
}

WriteBehind::~WriteBehind() {
    // Tasks still running post nothing to a dead writer (their queued calls are
    // removed with it, and none is posted after this).
    QMutexLocker lock(&shared_->mutex);
    shared_->owner = nullptr;
}

bool WriteBehind::submit(Op op) {
    if (closed_)
        return false;
    const QString key = op.key;
    const quint64 seq = ++next_seq_;
    ops_.insert(seq, std::move(op));
    quint64 replaced = 0;
    quint64 dropped = 0;
    queue_.enqueue(key, seq, &replaced, &dropped);
    if (replaced)
        ops_.remove(replaced); // superseded: never runs, never called back
    if (dropped) {
        ops_.remove(dropped);
        ++dropped_;
    }
    start_ready();
    return true;
}

bool WriteBehind::pending(const QString& key, Op* out) const {
    quint64 seq = queue_.queued_seq(key);
    if (!seq)
        seq = queue_.in_flight_seq(key);
    const auto it = ops_.constFind(seq);
    if (!seq || it == ops_.constEnd())
        return false;
    if (out)
        *out = it.value();
    return true;
}

bool WriteBehind::busy(const QString& key) const {
    return queue_.queued_seq(key) != 0 || queue_.in_flight(key);
}

void WriteBehind::start_ready() {
    const auto ready = queue_.take_ready();
    for (const auto& entry : ready) {
        const QString key = entry.first;
        const quint64 seq = entry.second;
        const std::function<qint64()> work = ops_.value(seq).work;
        {
            QMutexLocker lock(&shared_->mutex);
            shared_->running.insert(key);
        }
        const std::shared_ptr<Shared> shared = shared_;
        pool_->start([shared, key, seq, work]() {
            const qint64 bytes = work ? work() : -1;
            QMutexLocker lock(&shared->mutex);
            shared->running.remove(key);
            shared->changed.wakeAll();
            // Posted while the lock is held, so the writer cannot be destroyed in between.
            if (WriteBehind* owner = shared->owner) {
                QMetaObject::invokeMethod(
                    owner, [owner, key, seq, bytes]() { owner->on_finished(key, seq, bytes); },
                    Qt::QueuedConnection);
            }
        });
    }
}

void WriteBehind::on_finished(const QString& key, quint64 seq, qint64 bytes) {
    queue_.finished(key);
    const Op op = ops_.take(seq);
    if (op.done)
        op.done(bytes);
    if (!closed_)
        start_ready();
}

int WriteBehind::flush(int wait_ms) {
    closed_ = true;
    QSet<QString> still_running;
    {
        QMutexLocker lock(&shared_->mutex);
        const QDeadlineTimer deadline(std::max(0, wait_ms));
        while (!shared_->running.isEmpty()) {
            if (!shared_->changed.wait(&shared_->mutex, deadline))
                break;
        }
        still_running = shared_->running;
    }
    int skipped = 0;
    for (const auto& entry : queue_.drain()) {
        const Op op = ops_.take(entry.second);
        if (still_running.contains(entry.first)) {
            ++skipped; // its older write is still running and could land after this one
            continue;
        }
        if (op.work)
            op.work(); // on this thread, now
    }
    return skipped;
}

// ── Queue ────────────────────────────────────────────────────────────────────

AutoLoadQueue::AutoLoadQueue(int capacity) : capacity_(std::max(1, capacity)) {}

void AutoLoadQueue::set_capacity(int capacity) {
    capacity_ = std::max(1, capacity);
}

bool AutoLoadQueue::is_queued(quint64 ticket) const {
    return position(ticket) >= 0;
}

int AutoLoadQueue::position(quint64 ticket) const {
    for (int i = 0; i < waiting_.size(); ++i) {
        if (waiting_.at(i).ticket == ticket)
            return i;
    }
    return -1;
}

bool AutoLoadQueue::request(quint64 ticket, int priority) {
    if (ticket == 0 || running_.contains(ticket) || is_queued(ticket))
        return running_.contains(ticket);
    // A free slot and nobody of the same or higher priority waiting: go now.
    const bool ahead = std::any_of(waiting_.cbegin(), waiting_.cend(),
                                   [priority](const Waiting& w) { return w.priority >= priority; });
    if (running_.size() < capacity_ && !ahead) {
        running_.append(ticket);
        return true;
    }
    // Insert after every waiting ticket of the same or higher priority (FIFO within a priority).
    int at = 0;
    while (at < waiting_.size() && waiting_.at(at).priority >= priority)
        ++at;
    waiting_.insert(at, Waiting{ticket, priority});
    return false;
}

QList<quint64> AutoLoadQueue::release(quint64 ticket) {
    running_.removeAll(ticket);
    for (int i = 0; i < waiting_.size(); ++i) {
        if (waiting_.at(i).ticket == ticket) {
            waiting_.removeAt(i);
            break;
        }
    }
    return fill();
}

QList<quint64> AutoLoadQueue::fill() {
    QList<quint64> started;
    while (running_.size() < capacity_ && !waiting_.isEmpty()) {
        const quint64 t = waiting_.takeFirst().ticket;
        running_.append(t);
        started.append(t);
    }
    return started;
}

// ── Feed mode ────────────────────────────────────────────────────────────────

bool feed_is_stale(const QDateTime& last_update, const QDateTime& now, int stale_after_seconds) {
    if (stale_after_seconds <= 0 || !last_update.isValid() || !now.isValid())
        return false;
    return last_update.msecsTo(now) > qint64(stale_after_seconds) * 1000;
}

qint64 feed_ms_until_stale(const QDateTime& last_update, const QDateTime& now, int stale_after_seconds) {
    if (stale_after_seconds <= 0 || !last_update.isValid() || !now.isValid())
        return -1;
    const qint64 limit = qint64(stale_after_seconds) * 1000;
    const qint64 left = limit - last_update.msecsTo(now);
    // Stale only once MORE than the limit has passed; a clock that went back never waits longer than the limit.
    return left < 0 ? 0 : std::min(left + 1, limit + 1);
}

FeedDecision decide_feed(const FeedFacts& f) {
    FeedDecision d;
    d.subscribe = f.requirements_ok;
    if (!f.requirements_ok) {
        d.page = f.has_data ? FeedPage::Content : FeedPage::NeedsSetup;
        return d;
    }
    if (f.has_data) {
        d.page = FeedPage::Content;
        const bool stale = feed_is_stale(f.last_update, f.now, f.stale_after_seconds);
        switch (f.report) {
            case FeedReport::Error:
                d.marker = FeedMarker::Stopped;
                break;
            case FeedReport::Idle:
                d.marker = FeedMarker::Idle;
                break;
            case FeedReport::Data:
                d.marker = stale ? FeedMarker::Stale : FeedMarker::None;
                break;
            case FeedReport::None:
            case FeedReport::Waiting:
                d.marker = stale ? FeedMarker::Stale : FeedMarker::Connecting;
                break;
        }
        return d;
    }
    switch (f.report) {
        case FeedReport::Error:
            d.page = FeedPage::Error;
            break;
        case FeedReport::Idle:
            d.page = FeedPage::Idle;
            break;
        case FeedReport::None:
        case FeedReport::Waiting:
        case FeedReport::Data: // data without a content widget: nothing to draw it in yet
            d.page = FeedPage::Waiting;
            break;
    }
    return d;
}

} // namespace fincept::ui::autoload

// ── EmptyState auto-compaction ───────────────────────────────────────────────

namespace fincept::ui::empty_state_fit {

namespace {

int esf_stack(std::initializer_list<int> blocks, int spacing) {
    int total = 0;
    int shown = 0;
    for (const int h : blocks) {
        if (h <= 0)
            continue;
        total += h;
        ++shown;
    }
    return shown > 1 ? total + spacing * (shown - 1) : total;
}

} // namespace

int needed_height(const Parts& p, Level level) {
    switch (level) {
        case Level::Full:
            return p.margins + esf_stack({p.glyph, p.title, p.message, p.technical, p.note, p.details,
                                          p.suggestions, p.actions},
                                         p.spacing);
        case Level::NoGlyph:
            return p.tight_margins + esf_stack({p.title, p.message, p.technical, p.note, p.details,
                                                p.suggestions, p.actions},
                                               p.spacing);
        case Level::NoDetails:
            return p.tight_margins + esf_stack({p.title, p.message, p.note, p.actions}, p.spacing);
        case Level::Inline:
            return p.tight_margins + std::max(p.inline_row, p.title_line);
        case Level::TitleOnly:
            return p.tight_margins + p.title_line;
    }
    return p.margins;
}

Level choose_level(const Parts& p, int available_height) {
    if (available_height <= 0)
        return Level::Full;
    for (const Level level : {Level::Full, Level::NoGlyph, Level::NoDetails, Level::Inline}) {
        if (needed_height(p, level) <= available_height)
            return level;
    }
    return Level::TitleOnly;
}

} // namespace fincept::ui::empty_state_fit
