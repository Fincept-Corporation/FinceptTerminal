#include "services/cloud/PortfolioCloudAdapter.h"

#include "network/cloud/CloudClient.h"
#include "storage/repositories/PortfolioRepository.h"
#include "storage/sync/SyncMap.h"
#include "storage/sync/SyncOutbox.h"

#include <QHash>

#include <functional>
#include <memory>

namespace fincept::services::cloud {

using fincept::cloud::CloudClient;
using fincept::cloud::CloudResponse;

namespace {
QString pf_entity() {
    return QStringLiteral("portfolio");
}
QString pf_txn_entity() {
    return QStringLiteral("portfolio_txn");
}
QString pf_path(const QString& pfl) {
    return QStringLiteral("/portfolios/") + pfl;
}

AdapterResult pf_ok() {
    AdapterResult r;
    r.ok = true;
    return r;
}
AdapterResult pf_retry(const QString& why) {
    AdapterResult r;
    r.retry = true;
    r.error = why;
    return r;
}
AdapterResult pf_credits() {
    AdapterResult r;
    r.credits_exhausted = true;
    r.error = QStringLiteral("insufficient_credits");
    return r;
}

// portfolio fields only — broker_account_id is device-local and never sent.
QJsonObject portfolio_body(const portfolio::Portfolio& p) {
    QJsonObject b;
    b["name"] = p.name;
    b["owner"] = p.owner;
    b["currency"] = p.currency.isEmpty() ? QStringLiteral("USD") : p.currency;
    b["description"] = p.description;
    return b;
}
QJsonObject txn_body(const portfolio::Transaction& t) {
    QJsonObject b;
    b["symbol"] = t.symbol;
    b["transaction_type"] = t.transaction_type;
    b["quantity"] = t.quantity;
    b["price"] = t.price;
    b["transaction_date"] = t.transaction_date;
    b["notes"] = t.notes;
    return b;
}
QJsonObject snap_body(const portfolio::PortfolioSnapshot& s) {
    QJsonObject b;
    b["total_value"] = s.total_value;
    b["total_cost_basis"] = s.total_cost_basis;
    b["total_pnl"] = s.total_pnl;
    b["total_pnl_percent"] = s.total_pnl_percent;
    b["snapshot_date"] = s.snapshot_date;
    return b;
}

// An async operation that reports (credits_exhausted, failed) on completion.
using Op = std::function<void(std::function<void(bool, bool)>)>;

// Fire all ops concurrently; aggregate into one push outcome.
void pf_run_ops(const QVector<Op>& ops, PushDone done) {
    if (ops.isEmpty()) {
        done(pf_ok());
        return;
    }
    auto pending = std::make_shared<int>(ops.size());
    auto credits = std::make_shared<bool>(false);
    auto failed = std::make_shared<bool>(false);
    std::function<void(bool, bool)> rep = [pending, credits, failed, done](bool c, bool f) {
        if (c)
            *credits = true;
        if (f)
            *failed = true;
        if (--*pending == 0) {
            if (*credits)
                done(pf_credits());
            else if (*failed)
                done(pf_retry(QStringLiteral("reconcile_partial")));
            else
                done(pf_ok());
        }
    };
    for (const Op& op : ops)
        op(rep);
}
} // namespace

PortfolioCloudAdapter& PortfolioCloudAdapter::instance() {
    static PortfolioCloudAdapter s;
    return s;
}

void PortfolioCloudAdapter::push(const OutboxRow& row, PushDone done) {
    if (row.op == QLatin1String("delete"))
        push_delete(row, std::move(done));
    else
        push_sync(row, std::move(done)); // "sync" (and any default)
}

void PortfolioCloudAdapter::push_delete(const OutboxRow& row, PushDone done) {
    auto remote = SyncMap::remote_id(pf_entity(), row.local_id);
    if (!remote) {
        done(pf_ok()); // never pushed
        return;
    }
    const QString local_id = row.local_id;
    CloudClient::instance().del(
        pf_path(*remote),
        [local_id, done](CloudResponse resp) {
            if (resp.ok || resp.status == 404) {
                SyncMap::remove_by_local(pf_entity(), local_id);
                done(pf_ok());
            } else if (resp.is_insufficient_credits()) {
                done(pf_credits());
            } else {
                done(pf_retry(QStringLiteral("delete_failed")));
            }
        },
        this);
}

void PortfolioCloudAdapter::push_sync(const OutboxRow& row, PushDone done) {
    const QString local_pid = row.local_id;
    auto p = PortfolioRepository::instance().get_portfolio(local_pid);
    if (p.is_err()) {
        done(pf_ok()); // portfolio gone locally — nothing to sync
        return;
    }
    const QJsonObject body = portfolio_body(p.value());
    auto remote = SyncMap::remote_id(pf_entity(), local_pid);
    if (remote) {
        const QString pfl = *remote;
        CloudClient::instance().put(
            pf_path(pfl), body,
            [this, local_pid, pfl, done](CloudResponse r) {
                if (r.is_insufficient_credits()) {
                    done(pf_credits());
                    return;
                }
                if (!r.ok && r.status != 404) {
                    done(pf_retry(QStringLiteral("portfolio_put_failed")));
                    return;
                }
                reconcile_up(local_pid, pfl, done);
            },
            this);
    } else {
        CloudClient::instance().post(
            QStringLiteral("/portfolios"), body,
            [this, local_pid, done](CloudResponse r) {
                if (r.is_insufficient_credits()) {
                    done(pf_credits());
                    return;
                }
                if (!r.ok || !r.data.isObject()) {
                    done(pf_retry(QStringLiteral("portfolio_post_failed")));
                    return;
                }
                const QJsonObject o = r.data.toObject();
                const QString pfl = o.value("id").toString();
                SyncMap::put(pf_entity(), local_pid, pfl, o.value("created_at").toString());
                reconcile_up(local_pid, pfl, done);
            },
            this);
    }
}

void PortfolioCloudAdapter::reconcile_up(const QString& local_pid, const QString& pfl, PushDone done) {
    // Chain the (free) GETs: detail (assets) → transactions → snapshots, then
    // compute and fire all child mutations under one barrier.
    CloudClient::instance().get(
        pf_path(pfl),
        [this, local_pid, pfl, done](CloudResponse dr) {
            if (!dr.ok || !dr.data.isObject()) {
                done(pf_retry(QStringLiteral("detail_failed")));
                return;
            }
            const QJsonArray cloud_assets = dr.data.toObject().value("assets").toArray();
            CloudClient::instance().get(
                pf_path(pfl) + QStringLiteral("/transactions?page_size=500"),
                [this, local_pid, pfl, cloud_assets, done](CloudResponse tr) {
                    const QJsonArray cloud_txns =
                        (tr.ok && tr.data.isObject()) ? tr.data.toObject().value("transactions").toArray()
                                                      : QJsonArray();
                    CloudClient::instance().get(
                        pf_path(pfl) + QStringLiteral("/snapshots?days=3650"),
                        [this, local_pid, pfl, cloud_assets, cloud_txns, done](CloudResponse sr) {
                            const QJsonArray cloud_snaps =
                                (sr.ok && sr.data.isArray()) ? sr.data.toArray() : QJsonArray();

                            auto& repo = PortfolioRepository::instance();
                            QVector<Op> ops;

                            auto post_op = [this](const QString& ep, const QJsonObject& b) -> Op {
                                return [this, ep, b](std::function<void(bool, bool)> rep) {
                                    CloudClient::instance().post(ep, b, [rep](CloudResponse r) {
                                        rep(r.is_insufficient_credits(), !r.ok && r.status != 404);
                                    }, this);
                                };
                            };
                            auto put_op = [this](const QString& ep, const QJsonObject& b) -> Op {
                                return [this, ep, b](std::function<void(bool, bool)> rep) {
                                    CloudClient::instance().put(ep, b, [rep](CloudResponse r) {
                                        rep(r.is_insufficient_credits(), !r.ok && r.status != 404);
                                    }, this);
                                };
                            };
                            auto del_op = [this](const QString& ep) -> Op {
                                return [this, ep](std::function<void(bool, bool)> rep) {
                                    CloudClient::instance().del(ep, [rep](CloudResponse r) {
                                        rep(r.is_insufficient_credits(), !r.ok && r.status != 404);
                                    }, this);
                                };
                            };

                            // ── assets (by symbol): PUT existing / POST new / DELETE gone ──
                            QHash<QString, QJsonObject> cloud_by_sym;
                            for (const QJsonValue& v : cloud_assets)
                                cloud_by_sym.insert(v.toObject().value("symbol").toString().toUpper(), v.toObject());
                            QSet<QString> local_syms;
                            auto la = repo.get_assets(local_pid);
                            if (la.is_ok()) {
                                for (const portfolio::PortfolioAsset& a : la.value()) {
                                    const QString sym = a.symbol.toUpper();
                                    local_syms.insert(sym);
                                    if (cloud_by_sym.contains(sym)) {
                                        const QString pfa = cloud_by_sym.value(sym).value("id").toString();
                                        QJsonObject b;
                                        b["quantity"] = a.quantity;
                                        b["avg_buy_price"] = a.avg_buy_price;
                                        b["sector"] = a.sector;
                                        b["broker_symbol"] = a.broker_symbol;
                                        b["exchange"] = a.exchange;
                                        ops.append(put_op(pf_path(pfl) + "/assets/" + pfa, b));
                                    } else {
                                        QJsonObject b;
                                        b["symbol"] = a.symbol;
                                        b["quantity"] = a.quantity;
                                        b["avg_buy_price"] = a.avg_buy_price;
                                        b["purchase_date"] = a.first_purchase_date;
                                        b["sector"] = a.sector;
                                        b["broker_symbol"] = a.broker_symbol;
                                        b["exchange"] = a.exchange;
                                        ops.append(post_op(pf_path(pfl) + "/assets", b));
                                    }
                                }
                            }
                            for (auto it = cloud_by_sym.constBegin(); it != cloud_by_sym.constEnd(); ++it)
                                if (!local_syms.contains(it.key()))
                                    ops.append(del_op(pf_path(pfl) + "/assets/" + it.value().value("id").toString()));

                            // ── transactions: POST unmapped locals / DELETE orphaned cloud ──
                            QSet<QString> local_txn_ids;
                            auto lt = repo.get_transactions(local_pid, 1000000);
                            if (lt.is_ok()) {
                                for (const portfolio::Transaction& t : lt.value()) {
                                    local_txn_ids.insert(t.id);
                                    if (!SyncMap::remote_id(pf_txn_entity(), t.id)) {
                                        const QString uuid = t.id;
                                        const QString ep = pf_path(pfl) + "/transactions";
                                        const QJsonObject b = txn_body(t);
                                        ops.append([this, ep, b, uuid](std::function<void(bool, bool)> rep) {
                                            CloudClient::instance().post(ep, b, [rep, uuid](CloudResponse r) {
                                                if (r.ok && r.data.isObject())
                                                    SyncMap::put(pf_txn_entity(), uuid,
                                                                 r.data.toObject().value("id").toString(), {});
                                                rep(r.is_insufficient_credits(), !r.ok);
                                            }, this);
                                        });
                                    }
                                }
                            }
                            for (const QJsonValue& v : cloud_txns) {
                                const QString ptx = v.toObject().value("id").toString();
                                auto loc = SyncMap::local_id(pf_txn_entity(), ptx);
                                if (loc && !local_txn_ids.contains(*loc)) {
                                    const QString uuid = *loc;
                                    const QString ep = pf_path(pfl) + "/transactions/" + ptx;
                                    ops.append([this, ep, uuid](std::function<void(bool, bool)> rep) {
                                        CloudClient::instance().del(ep, [rep, uuid](CloudResponse r) {
                                            if (r.ok || r.status == 404)
                                                SyncMap::remove_by_local(pf_txn_entity(), uuid);
                                            rep(r.is_insufficient_credits(), !r.ok && r.status != 404);
                                        }, this);
                                    });
                                }
                            }

                            // ── snapshots (by date): POST dates missing in cloud ──
                            QSet<QString> cloud_dates;
                            for (const QJsonValue& v : cloud_snaps)
                                cloud_dates.insert(v.toObject().value("snapshot_date").toString());
                            auto ls = repo.get_snapshots(local_pid, 3650);
                            if (ls.is_ok())
                                for (const portfolio::PortfolioSnapshot& s : ls.value())
                                    if (!cloud_dates.contains(s.snapshot_date))
                                        ops.append(post_op(pf_path(pfl) + "/snapshots", snap_body(s)));

                            pf_run_ops(ops, done);
                        },
                        this);
                },
                this);
        },
        this);
}

// ── counts / first-enable ────────────────────────────────────────────────────

int PortfolioCloudAdapter::local_count() const {
    auto r = PortfolioRepository::instance().list_portfolios();
    return r.is_ok() ? static_cast<int>(r.value().size()) : 0;
}

void PortfolioCloudAdapter::remote_count(CountDone done) {
    CloudClient::instance().get(
        QStringLiteral("/portfolios"),
        [done](CloudResponse resp) {
            if (!resp.ok) {
                done(false, 0);
                return;
            }
            done(true, resp.data.isArray() ? static_cast<int>(resp.data.toArray().size()) : 0);
        },
        this);
}

void PortfolioCloudAdapter::enqueue_all_local() {
    auto ps = PortfolioRepository::instance().list_portfolios();
    if (ps.is_err())
        return;
    for (const portfolio::Portfolio& p : ps.value())
        SyncOutbox::record_unique(pf_entity(), p.id, QStringLiteral("sync"));
}

// ── pull ─────────────────────────────────────────────────────────────────────

void PortfolioCloudAdapter::pull(PullDone done) {
    CloudClient::instance().get(
        QStringLiteral("/portfolios"),
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
                reconcile_portfolio_deletes(*remote_ids);
                done(true, QString());
                return;
            }

            auto pending = std::make_shared<int>(static_cast<int>(arr.size()));
            auto had_error = std::make_shared<bool>(false);
            auto finish_one = [this, pending, had_error, remote_ids, done]() {
                if (--*pending == 0) {
                    reconcile_portfolio_deletes(*remote_ids);
                    done(!*had_error, *had_error ? QStringLiteral("pull_partial") : QString());
                }
            };

            for (const QJsonValue& v : arr) {
                const QString pfl = v.toObject().value("id").toString();
                CloudClient::instance().get(
                    pf_path(pfl),
                    [this, pfl, had_error, finish_one](CloudResponse dr) {
                        if (!dr.ok || !dr.data.isObject()) {
                            *had_error = true;
                            finish_one();
                            return;
                        }
                        const QJsonObject detail = dr.data.toObject();
                        CloudClient::instance().get(
                            pf_path(pfl) + QStringLiteral("/transactions?page_size=500"),
                            [this, pfl, detail, had_error, finish_one](CloudResponse tr) {
                                const QJsonArray txns =
                                    (tr.ok && tr.data.isObject()) ? tr.data.toObject().value("transactions").toArray()
                                                                  : QJsonArray();
                                CloudClient::instance().get(
                                    pf_path(pfl) + QStringLiteral("/snapshots?days=3650"),
                                    [this, detail, txns, finish_one](CloudResponse sr) {
                                        const QJsonArray snaps =
                                            (sr.ok && sr.data.isArray()) ? sr.data.toArray() : QJsonArray();
                                        apply_remote_portfolio(detail, txns, snaps);
                                        finish_one();
                                    },
                                    this);
                            },
                            this);
                    },
                    this);
            }
        },
        this);
}

void PortfolioCloudAdapter::apply_remote_portfolio(const QJsonObject& detail, const QJsonArray& txns,
                                                   const QJsonArray& snaps) {
    SyncOutbox::ApplyGuard guard; // suppress outbox for the whole apply pass
    auto& repo = PortfolioRepository::instance();

    const QString pfl = detail.value("id").toString();
    const QString updated = detail.value("updated_at").toString();
    auto existing = SyncMap::local_id(pf_entity(), pfl);
    QString local_pid;
    if (existing) {
        local_pid = *existing;
        repo.update_portfolio(local_pid, detail.value("name").toString(), detail.value("owner").toString(),
                              detail.value("currency").toString(), detail.value("description").toString());
    } else {
        // broker_account_id is device-local — never imported from cloud.
        auto cr = repo.create_portfolio(detail.value("name").toString(), detail.value("owner").toString(),
                                        detail.value("currency").toString(), detail.value("description").toString());
        if (cr.is_err())
            return;
        local_pid = cr.value();
    }
    SyncMap::put(pf_entity(), local_pid, pfl, updated);

    // ── assets (cloud → local) ──
    const QJsonArray cassets = detail.value("assets").toArray();
    QSet<QString> cloud_syms;
    QSet<QString> local_syms;
    auto la = repo.get_assets(local_pid);
    if (la.is_ok())
        for (const portfolio::PortfolioAsset& a : la.value())
            local_syms.insert(a.symbol.toUpper());
    for (const QJsonValue& v : cassets) {
        const QJsonObject a = v.toObject();
        const QString sym = a.value("symbol").toString();
        cloud_syms.insert(sym.toUpper());
        const double qty = a.value("quantity").toDouble();
        const double avg = a.value("avg_buy_price").toDouble();
        const QString sector = a.value("sector").toString();
        if (local_syms.contains(sym.toUpper())) {
            repo.update_asset(local_pid, sym, qty, avg);
            if (!sector.isEmpty())
                repo.set_asset_sector(local_pid, sym, sector);
        } else {
            repo.add_asset(local_pid, sym, qty, avg, a.value("purchase_date").toString(), sector,
                           a.value("broker_symbol").toString(), a.value("exchange").toString());
        }
    }
    if (la.is_ok())
        for (const portfolio::PortfolioAsset& a : la.value())
            if (!cloud_syms.contains(a.symbol.toUpper()))
                repo.remove_asset(local_pid, a.symbol);

    // ── transactions (cloud → local) ──
    QSet<QString> cloud_ptx;
    QSet<QString> local_txn_ids;
    auto lt = repo.get_transactions(local_pid, 1000000);
    if (lt.is_ok())
        for (const portfolio::Transaction& t : lt.value())
            local_txn_ids.insert(t.id);
    for (const QJsonValue& v : txns) {
        const QJsonObject t = v.toObject();
        const QString ptx = t.value("id").toString();
        cloud_ptx.insert(ptx);
        if (!SyncMap::local_id(pf_txn_entity(), ptx)) {
            auto cr = repo.add_transaction(local_pid, t.value("symbol").toString(),
                                           t.value("transaction_type").toString(), t.value("quantity").toDouble(),
                                           t.value("price").toDouble(), t.value("transaction_date").toString(),
                                           t.value("notes").toString());
            if (cr.is_ok())
                SyncMap::put(pf_txn_entity(), cr.value(), ptx, {});
        }
    }
    if (lt.is_ok())
        for (const portfolio::Transaction& t : lt.value()) {
            auto rm = SyncMap::remote_id(pf_txn_entity(), t.id);
            if (rm && !cloud_ptx.contains(*rm)) {
                repo.delete_transaction(t.id);
                SyncMap::remove_by_local(pf_txn_entity(), t.id);
            }
        }

    // ── snapshots (cloud → local): upsert by date ──
    for (const QJsonValue& v : snaps) {
        const QJsonObject s = v.toObject();
        repo.save_snapshot(local_pid, s.value("total_value").toDouble(), s.value("total_cost_basis").toDouble(),
                           s.value("total_pnl").toDouble(), s.value("total_pnl_percent").toDouble(),
                           s.value("snapshot_date").toString());
    }
}

void PortfolioCloudAdapter::reconcile_portfolio_deletes(const QSet<QString>& remote_ids) {
    SyncOutbox::ApplyGuard guard;
    auto maps = SyncMap::all(pf_entity());
    if (maps.is_err())
        return;
    auto& repo = PortfolioRepository::instance();
    for (const SyncMapping& m : maps.value()) {
        if (!remote_ids.contains(m.remote_id)) {
            repo.delete_portfolio(m.local_id);
            SyncMap::remove_by_local(pf_entity(), m.local_id);
        }
    }
}

} // namespace fincept::services::cloud
