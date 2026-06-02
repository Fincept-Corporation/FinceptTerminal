#include "services/cloud/WatchlistCloudAdapter.h"

#include "core/logging/Logger.h"
#include "network/cloud/CloudClient.h"
#include "storage/repositories/WatchlistRepository.h"
#include "storage/sync/SyncMap.h"
#include "storage/sync/SyncOutbox.h"

#include <QJsonArray>

#include <memory>

namespace fincept::services::cloud {

using fincept::cloud::CloudClient;
using fincept::cloud::CloudResponse;

namespace {
QString wl_entity() {
    return QStringLiteral("watchlist");
}

QString wl_path(const QString& remote_id) {
    return QStringLiteral("/watchlists/") + remote_id;
}

AdapterResult wl_ok() {
    AdapterResult r;
    r.ok = true;
    return r;
}

AdapterResult wl_retry(const QString& why) {
    AdapterResult r;
    r.retry = true;
    r.error = why;
    return r;
}

// Map a non-ok cloud response to a push outcome.
AdapterResult wl_push_result(const CloudResponse& resp) {
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
    r.ok = false; // auth / network / 5xx → keep the row and retry later
    r.retry = true;
    r.error = resp.error.isEmpty() ? QStringLiteral("push_failed") : resp.error;
    return r;
}
} // namespace

WatchlistCloudAdapter& WatchlistCloudAdapter::instance() {
    static WatchlistCloudAdapter s;
    return s;
}

// ── push ─────────────────────────────────────────────────────────────────────

void WatchlistCloudAdapter::push(const OutboxRow& row, PushDone done) {
    if (row.op == QLatin1String("create"))
        push_create(row, std::move(done));
    else if (row.op == QLatin1String("update"))
        push_update(row, std::move(done));
    else if (row.op == QLatin1String("delete"))
        push_delete(row, std::move(done));
    else if (row.op == QLatin1String("stock_add"))
        push_stock_add(row, std::move(done));
    else if (row.op == QLatin1String("stock_remove"))
        push_stock_remove(row, std::move(done));
    else
        done(wl_ok()); // unknown op — drop
}

void WatchlistCloudAdapter::push_create(const OutboxRow& row, PushDone done) {
    auto wl = WatchlistRepository::instance().get(row.local_id);
    if (wl.is_err()) {
        done(wl_ok()); // local row already gone — nothing to create
        return;
    }
    const Watchlist w = wl.value();
    QJsonObject body;
    body["name"] = w.name;
    body["description"] = w.description;
    body["color"] = w.color;
    body["is_default"] = w.is_default;
    const QString local_id = row.local_id;
    CloudClient::instance().post(
        QStringLiteral("/watchlists"), body,
        [local_id, done](CloudResponse resp) {
            if (resp.ok && resp.data.isObject()) {
                const QJsonObject o = resp.data.toObject();
                SyncMap::put(wl_entity(), local_id, o.value("id").toString(), o.value("created_at").toString());
                done(wl_ok());
            } else {
                done(wl_push_result(resp));
            }
        },
        this);
}

void WatchlistCloudAdapter::push_update(const OutboxRow& row, PushDone done) {
    auto remote = SyncMap::remote_id(wl_entity(), row.local_id);
    if (!remote) {
        done(wl_retry(QStringLiteral("awaiting_create"))); // create not pushed yet — retry after it
        return;
    }
    auto wl = WatchlistRepository::instance().get(row.local_id);
    if (wl.is_err()) {
        done(wl_ok());
        return;
    }
    const Watchlist w = wl.value();
    QJsonObject body;
    body["name"] = w.name;
    body["description"] = w.description;
    body["color"] = w.color;
    body["sort_order"] = w.sort_order;
    body["is_default"] = w.is_default;
    CloudClient::instance().put(wl_path(*remote), body, [done](CloudResponse resp) { done(wl_push_result(resp)); },
                                this);
}

void WatchlistCloudAdapter::push_delete(const OutboxRow& row, PushDone done) {
    auto remote = SyncMap::remote_id(wl_entity(), row.local_id);
    if (!remote) {
        done(wl_ok()); // never pushed — nothing remote to delete
        return;
    }
    const QString local_id = row.local_id;
    CloudClient::instance().del(
        wl_path(*remote),
        [local_id, done](CloudResponse resp) {
            if (resp.ok || resp.status == 404) {
                SyncMap::remove_by_local(wl_entity(), local_id);
                done(wl_ok());
            } else {
                done(wl_push_result(resp));
            }
        },
        this);
}

void WatchlistCloudAdapter::push_stock_add(const OutboxRow& row, PushDone done) {
    auto remote = SyncMap::remote_id(wl_entity(), row.local_id);
    if (!remote) {
        done(wl_retry(QStringLiteral("awaiting_create")));
        return;
    }
    const QString sym = row.payload.toUpper();
    QString name, exch;
    auto stocks = WatchlistRepository::instance().get_stocks(row.local_id);
    if (stocks.is_ok()) {
        for (const WatchlistStock& s : stocks.value()) {
            if (s.symbol.toUpper() == sym) {
                name = s.name;
                exch = s.exchange;
                break;
            }
        }
    }
    QJsonObject body;
    body["symbol"] = sym;
    body["name"] = name;
    body["exchange"] = exch;
    CloudClient::instance().post(wl_path(*remote) + QStringLiteral("/stocks"), body,
                                 [done](CloudResponse resp) { done(wl_push_result(resp)); }, this);
}

void WatchlistCloudAdapter::push_stock_remove(const OutboxRow& row, PushDone done) {
    auto remote = SyncMap::remote_id(wl_entity(), row.local_id);
    if (!remote) {
        done(wl_ok()); // watchlist not in cloud — nothing to remove
        return;
    }
    const QString sym = row.payload.toUpper();
    CloudClient::instance().del(wl_path(*remote) + QStringLiteral("/stocks/") + sym,
                                [done](CloudResponse resp) {
                                    if (resp.ok || resp.status == 404)
                                        done(wl_ok());
                                    else
                                        done(wl_push_result(resp));
                                },
                                this);
}

// ── counts / first-enable ────────────────────────────────────────────────────

int WatchlistCloudAdapter::local_count() const {
    auto r = WatchlistRepository::instance().list_all();
    return r.is_ok() ? static_cast<int>(r.value().size()) : 0;
}

void WatchlistCloudAdapter::remote_count(CountDone done) {
    CloudClient::instance().get(
        QStringLiteral("/watchlists"),
        [done](CloudResponse resp) {
            if (!resp.ok) {
                done(false, 0);
                return;
            }
            done(true, resp.data.isArray() ? static_cast<int>(resp.data.toArray().size()) : 0);
        },
        this);
}

void WatchlistCloudAdapter::enqueue_all_local() {
    auto wls = WatchlistRepository::instance().list_all();
    if (wls.is_err())
        return;
    for (const Watchlist& w : wls.value()) {
        SyncOutbox::record(wl_entity(), w.id, QStringLiteral("create"));
        auto stocks = WatchlistRepository::instance().get_stocks(w.id);
        if (stocks.is_ok())
            for (const WatchlistStock& s : stocks.value())
                SyncOutbox::record(wl_entity(), w.id, QStringLiteral("stock_add"), s.symbol);
    }
}

// ── pull ─────────────────────────────────────────────────────────────────────

void WatchlistCloudAdapter::pull(PullDone done) {
    CloudClient::instance().get(
        QStringLiteral("/watchlists"),
        [this, done](CloudResponse resp) {
            if (!resp.ok) {
                done(false, resp.error);
                return;
            }
            const QJsonArray arr = resp.data.toArray();
            auto remote_ids = std::make_shared<QSet<QString>>();
            for (const QJsonValue& v : arr)
                remote_ids->insert(v.toObject().value("id").toString());

            if (arr.isEmpty()) {
                reconcile_deletes(*remote_ids);
                done(true, QString());
                return;
            }

            auto pending = std::make_shared<int>(static_cast<int>(arr.size()));
            auto had_error = std::make_shared<bool>(false);
            for (const QJsonValue& v : arr) {
                const QString rid = v.toObject().value("id").toString();
                CloudClient::instance().get(
                    wl_path(rid),
                    [this, pending, had_error, remote_ids, done](CloudResponse dr) {
                        if (dr.ok && dr.data.isObject())
                            apply_remote_watchlist(dr.data.toObject());
                        else
                            *had_error = true;
                        if (--(*pending) == 0) {
                            reconcile_deletes(*remote_ids);
                            done(!*had_error, *had_error ? QStringLiteral("pull_partial") : QString());
                        }
                    },
                    this);
            }
        },
        this);
}

void WatchlistCloudAdapter::apply_remote_watchlist(const QJsonObject& o) {
    SyncOutbox::ApplyGuard guard; // suppress outbox while writing remote data (no echo loop)
    auto& repo = WatchlistRepository::instance();
    const QString rid = o.value("id").toString();
    const QString updated = o.value("updated_at").toString();

    auto existing = SyncMap::local_id(wl_entity(), rid);
    QString local_id;
    if (existing) {
        local_id = *existing;
    } else {
        auto cr = repo.create(o.value("name").toString(), o.value("color").toString());
        if (cr.is_err())
            return;
        local_id = cr.value().id;
    }

    Watchlist w;
    w.id = local_id;
    w.name = o.value("name").toString();
    w.description = o.value("description").toString();
    w.color = o.value("color").toString();
    w.sort_order = o.value("sort_order").toInt();
    w.is_default = o.value("is_default").toBool();
    repo.update(w);
    SyncMap::put(wl_entity(), local_id, rid, updated);

    // Reconcile stocks: add everything remote, drop locals not present remotely.
    const QJsonArray rstocks = o.value("stocks").toArray();
    QSet<QString> remote_syms;
    for (const QJsonValue& sv : rstocks) {
        const QJsonObject so = sv.toObject();
        const QString sym = so.value("symbol").toString().toUpper();
        remote_syms.insert(sym);
        repo.add_stock(local_id, sym, so.value("name").toString(), so.value("exchange").toString());
    }
    auto lstocks = repo.get_stocks(local_id);
    if (lstocks.is_ok())
        for (const WatchlistStock& ls : lstocks.value())
            if (!remote_syms.contains(ls.symbol.toUpper()))
                repo.remove_stock(local_id, ls.symbol);
}

void WatchlistCloudAdapter::reconcile_deletes(const QSet<QString>& remote_ids) {
    SyncOutbox::ApplyGuard guard;
    auto maps = SyncMap::all(wl_entity());
    if (maps.is_err())
        return;
    auto& repo = WatchlistRepository::instance();
    for (const SyncMapping& m : maps.value()) {
        if (!remote_ids.contains(m.remote_id)) {
            repo.remove(m.local_id);
            SyncMap::remove_by_local(wl_entity(), m.local_id);
        }
    }
}

} // namespace fincept::services::cloud
