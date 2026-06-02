#include "services/cloud/NotesCloudAdapter.h"

#include "network/cloud/CloudClient.h"
#include "storage/repositories/NotesRepository.h"
#include "storage/sync/SyncMap.h"
#include "storage/sync/SyncOutbox.h"

#include <QJsonArray>

#include <memory>

namespace fincept::services::cloud {

using fincept::cloud::CloudClient;
using fincept::cloud::CloudResponse;

namespace {
QString note_entity() {
    return QStringLiteral("note");
}
QString note_path(const QString& rid) {
    return QStringLiteral("/notes/") + rid;
}

AdapterResult note_ok() {
    AdapterResult r;
    r.ok = true;
    return r;
}
AdapterResult note_retry(const QString& why) {
    AdapterResult r;
    r.retry = true;
    r.error = why;
    return r;
}
AdapterResult note_result(const CloudResponse& resp) {
    AdapterResult r;
    if (resp.ok) {
        r.ok = true;
        return r;
    }
    if (resp.is_insufficient_credits()) {
        r.credits_exhausted = true;
        r.error = QStringLiteral("insufficient_credits");
        return r;
    }
    r.retry = true;
    r.error = resp.error.isEmpty() ? QStringLiteral("push_failed") : resp.error;
    return r;
}

// Local priority is a string enum; the cloud stores an int. Map both ways.
int note_priority_to_int(const QString& p) {
    const QString u = p.toUpper();
    if (u == QLatin1String("LOW"))
        return 0;
    if (u == QLatin1String("HIGH"))
        return 2;
    if (u == QLatin1String("CRITICAL"))
        return 3;
    return 1; // MEDIUM / default
}
QString note_priority_from_int(int p) {
    switch (p) {
    case 0:
        return QStringLiteral("LOW");
    case 2:
        return QStringLiteral("HIGH");
    case 3:
        return QStringLiteral("CRITICAL");
    default:
        return QStringLiteral("MEDIUM");
    }
}

QJsonObject note_body(const FinancialNote& n) {
    QJsonObject b;
    b["title"] = n.title;
    b["content"] = n.content;
    b["category"] = n.category.isEmpty() ? QStringLiteral("general") : n.category;
    b["priority"] = note_priority_to_int(n.priority);
    b["tags"] = n.tags;       // comma-separated string on both sides
    b["tickers"] = n.tickers; // comma-separated string on both sides
    b["sentiment"] = n.sentiment;
    b["color_code"] = n.color_code;
    if (!n.reminder_date.isEmpty())
        b["reminder_date"] = n.reminder_date;
    return b; // word_count is computed server-side; attachments are device-local
}
} // namespace

NotesCloudAdapter& NotesCloudAdapter::instance() {
    static NotesCloudAdapter s;
    return s;
}

// ── push ─────────────────────────────────────────────────────────────────────

void NotesCloudAdapter::push(const OutboxRow& row, PushDone done) {
    if (row.op == QLatin1String("create"))
        push_create(row, std::move(done));
    else if (row.op == QLatin1String("update"))
        push_update(row, std::move(done));
    else if (row.op == QLatin1String("delete"))
        push_delete(row, std::move(done));
    else if (row.op == QLatin1String("favorite"))
        push_toggle(row, QStringLiteral("favorite"), std::move(done));
    else if (row.op == QLatin1String("archive"))
        push_toggle(row, QStringLiteral("archive"), std::move(done));
    else
        done(note_ok());
}

void NotesCloudAdapter::push_create(const OutboxRow& row, PushDone done) {
    auto note = NotesRepository::instance().get(row.local_id.toInt());
    if (note.is_err()) {
        done(note_ok()); // local row already gone
        return;
    }
    const QString local_id = row.local_id;
    CloudClient::instance().post(
        QStringLiteral("/notes"), note_body(note.value()),
        [local_id, done](CloudResponse resp) {
            if (resp.ok && resp.data.isObject()) {
                const QJsonObject o = resp.data.toObject();
                SyncMap::put(note_entity(), local_id, o.value("id").toString(), o.value("created_at").toString());
                done(note_ok());
            } else {
                done(note_result(resp));
            }
        },
        this);
}

void NotesCloudAdapter::push_update(const OutboxRow& row, PushDone done) {
    auto remote = SyncMap::remote_id(note_entity(), row.local_id);
    if (!remote) {
        done(note_retry(QStringLiteral("awaiting_create")));
        return;
    }
    auto note = NotesRepository::instance().get(row.local_id.toInt());
    if (note.is_err()) {
        done(note_ok());
        return;
    }
    CloudClient::instance().put(note_path(*remote), note_body(note.value()),
                                [done](CloudResponse resp) { done(note_result(resp)); }, this);
}

void NotesCloudAdapter::push_delete(const OutboxRow& row, PushDone done) {
    auto remote = SyncMap::remote_id(note_entity(), row.local_id);
    if (!remote) {
        done(note_ok());
        return;
    }
    const QString local_id = row.local_id;
    CloudClient::instance().del(
        note_path(*remote),
        [local_id, done](CloudResponse resp) {
            if (resp.ok || resp.status == 404) {
                SyncMap::remove_by_local(note_entity(), local_id);
                done(note_ok());
            } else {
                done(note_result(resp));
            }
        },
        this);
}

void NotesCloudAdapter::push_toggle(const OutboxRow& row, const QString& sub, PushDone done) {
    auto remote = SyncMap::remote_id(note_entity(), row.local_id);
    if (!remote) {
        done(note_retry(QStringLiteral("awaiting_create")));
        return;
    }
    // Toggle endpoints take no body; favourite/archive flips parity-preservingly.
    CloudClient::instance().put(note_path(*remote) + QStringLiteral("/") + sub, QJsonObject(),
                                [done](CloudResponse resp) { done(note_result(resp)); }, this);
}

// ── counts / first-enable ────────────────────────────────────────────────────

int NotesCloudAdapter::local_count() const {
    auto r = NotesRepository::instance().list_all(/*include_archived=*/true);
    return r.is_ok() ? static_cast<int>(r.value().size()) : 0;
}

void NotesCloudAdapter::remote_count(CountDone done) {
    CloudClient::instance().get(
        QStringLiteral("/notes?page_size=1"),
        [done](CloudResponse resp) {
            if (!resp.ok || !resp.data.isObject()) {
                done(false, 0);
                return;
            }
            const int total = resp.data.toObject().value("pagination").toObject().value("total").toInt();
            done(true, total);
        },
        this);
}

void NotesCloudAdapter::enqueue_all_local() {
    auto notes = NotesRepository::instance().list_all(/*include_archived=*/true);
    if (notes.is_err())
        return;
    for (const FinancialNote& n : notes.value()) {
        const QString lid = QString::number(n.id);
        SyncOutbox::record(note_entity(), lid, QStringLiteral("create"));
        // create doesn't carry favorite/archive → push the toggle so cloud matches.
        if (n.is_favorite)
            SyncOutbox::record(note_entity(), lid, QStringLiteral("favorite"));
        if (n.is_archived)
            SyncOutbox::record(note_entity(), lid, QStringLiteral("archive"));
    }
}

// ── pull ─────────────────────────────────────────────────────────────────────

void NotesCloudAdapter::pull(PullDone done) {
    // The list endpoint excludes archived by default, so fetch both pages and merge.
    auto active = std::make_shared<QJsonArray>();
    CloudClient::instance().get(
        QStringLiteral("/notes?page_size=500"),
        [this, active, done](CloudResponse r1) {
            if (!r1.ok) {
                done(false, r1.error);
                return;
            }
            if (r1.data.isObject())
                *active = r1.data.toObject().value("notes").toArray();
            CloudClient::instance().get(
                QStringLiteral("/notes?archived=true&page_size=500"),
                [this, active, done](CloudResponse r2) {
                    QJsonArray all = *active;
                    if (r2.ok && r2.data.isObject())
                        for (const QJsonValue& v : r2.data.toObject().value("notes").toArray())
                            all.append(v);

                    SyncOutbox::ApplyGuard guard; // suppress outbox for the whole apply pass
                    QSet<QString> remote_ids;
                    for (const QJsonValue& v : all) {
                        const QJsonObject o = v.toObject();
                        remote_ids.insert(o.value("id").toString());
                        apply_remote_note(o);
                    }
                    reconcile_deletes(remote_ids);
                    done(true, QString());
                },
                this);
        },
        this);
}

void NotesCloudAdapter::apply_remote_note(const QJsonObject& o) {
    auto& repo = NotesRepository::instance();
    const QString rid = o.value("id").toString();
    const QString updated = o.value("updated_at").toString();

    FinancialNote n;
    n.title = o.value("title").toString();
    n.content = o.value("content").toString();
    n.category = o.value("category").toString();
    n.priority = note_priority_from_int(o.value("priority").toInt());
    n.tags = o.value("tags").toString();
    n.tickers = o.value("tickers").toString();
    n.sentiment = o.value("sentiment").toString();
    n.is_favorite = o.value("is_favorite").toBool();
    n.is_archived = o.value("is_archived").toBool();
    n.color_code = o.value("color_code").toString();
    n.reminder_date = o.value("reminder_date").toString();
    n.word_count = o.value("word_count").toInt();

    auto existing = SyncMap::local_id(note_entity(), rid);
    QString local_id;
    if (existing) {
        n.id = existing->toInt();
        repo.update(n);
        local_id = *existing;
    } else {
        auto cr = repo.create(n);
        if (cr.is_err())
            return;
        local_id = QString::number(cr.value());
    }
    SyncMap::put(note_entity(), local_id, rid, updated);
}

void NotesCloudAdapter::reconcile_deletes(const QSet<QString>& remote_ids) {
    auto maps = SyncMap::all(note_entity());
    if (maps.is_err())
        return;
    auto& repo = NotesRepository::instance();
    for (const SyncMapping& m : maps.value()) {
        if (!remote_ids.contains(m.remote_id)) {
            repo.remove(m.local_id.toInt());
            SyncMap::remove_by_local(note_entity(), m.local_id);
        }
    }
}

} // namespace fincept::services::cloud
