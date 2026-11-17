// CommandRegistry.cpp — see CommandRegistry.h.
#include "core/command/CommandRegistry.h"

#include <QCoreApplication>
#include <QHash>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>
#include <QThread>

#include <algorithm>
#include <atomic>
#include <exception>
#include <memory>

namespace fincept::command {

namespace {

QString creg_owner_of(const QString& explicit_owner, const QString& id) {
    if (!explicit_owner.isEmpty())
        return explicit_owner;
    const int dot = id.indexOf(QLatin1Char('.'));
    return dot > 0 ? id.left(dot) : id;
}

bool creg_valid(const CommandItem& item) {
    return !item.id.trimmed().isEmpty() && !item.title.trimmed().isEmpty() && !item.group.isEmpty();
}

} // namespace

struct CommandRegistry::Impl {
    QList<QString> order;                 // registration order of row ids
    QHash<QString, CommandItem> rows;     // id -> row
    QList<QString> provider_order;
    QHash<QString, ProviderSpec> providers;
    quint64 revision = 1;

    struct Request {
        QPointer<QObject> receiver;
        Delivery deliver;
        QSet<QString> pending;            // providers that have not answered final yet
        QHash<QString, CancelFn> cancels; // provider id -> its cancel fn (snapshot)
        QHash<QString, int> caps;
        QHash<QString, QString> group_of;
    };
    QHash<quint64, std::shared_ptr<Request>> requests;
    quint64 next_request = 1;
};

CommandRegistry& CommandRegistry::instance() {
    // Process lifetime; never destroyed during static teardown. Pinned to the application's (GUI) thread
    // even when a worker happens to ask first, so re-posted calls land where the box lives.
    static CommandRegistry* s = []() {
        auto* r = new CommandRegistry();
        if (QCoreApplication* app = QCoreApplication::instance(); app && r->thread() != app->thread())
            r->moveToThread(app->thread());
        return r;
    }();
    return *s;
}

CommandRegistry::CommandRegistry(QObject* parent) : QObject(parent), d_(new Impl) {}

CommandRegistry::~CommandRegistry() {
    delete d_;
}

bool CommandRegistry::register_item(CommandItem item) {
    if (!creg_valid(item))
        return false;
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, item]() { register_item(item); }, Qt::QueuedConnection);
        return true;
    }
    item.owner = creg_owner_of(item.owner, item.id);
    if (!d_->rows.contains(item.id))
        d_->order.append(item.id);
    d_->rows.insert(item.id, std::move(item));
    ++d_->revision;
    emit changed();
    return true;
}

int CommandRegistry::register_items(const QList<CommandItem>& items) {
    if (QThread::currentThread() != thread()) {
        int valid = 0;
        for (const CommandItem& it : items)
            valid += creg_valid(it) ? 1 : 0;
        QMetaObject::invokeMethod(this, [this, items]() { register_items(items); }, Qt::QueuedConnection);
        return valid;
    }
    int accepted = 0;
    for (CommandItem item : items) {
        if (!creg_valid(item))
            continue;
        item.owner = creg_owner_of(item.owner, item.id);
        if (!d_->rows.contains(item.id))
            d_->order.append(item.id);
        d_->rows.insert(item.id, std::move(item));
        ++accepted;
    }
    if (accepted > 0) {
        ++d_->revision;
        emit changed();
    }
    return accepted;
}

bool CommandRegistry::unregister_item(const QString& id) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, id]() { unregister_item(id); }, Qt::QueuedConnection);
        return true;
    }
    if (!d_->rows.remove(id))
        return false;
    d_->order.removeAll(id);
    ++d_->revision;
    emit changed();
    return true;
}

int CommandRegistry::unregister_owner(const QString& owner) {
    if (owner.isEmpty())
        return 0;
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, owner]() { unregister_owner(owner); }, Qt::QueuedConnection);
        return 0;
    }
    int removed = 0;
    for (auto it = d_->rows.begin(); it != d_->rows.end();) {
        if (it->owner == owner) {
            d_->order.removeAll(it.key());
            it = d_->rows.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    for (auto it = d_->providers.begin(); it != d_->providers.end();) {
        if (it->owner == owner) {
            d_->provider_order.removeAll(it.key());
            it = d_->providers.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    if (removed > 0) {
        ++d_->revision;
        emit changed();
    }
    return removed;
}

QList<CommandItem> CommandRegistry::items() const {
    QList<CommandItem> out;
    out.reserve(d_->order.size());
    for (const QString& id : d_->order) {
        auto it = d_->rows.constFind(id);
        if (it != d_->rows.constEnd())
            out.append(it.value());
    }
    return out;
}

bool CommandRegistry::contains(const QString& id) const {
    return d_->rows.contains(id);
}

CommandItem CommandRegistry::item(const QString& id) const {
    return d_->rows.value(id);
}

int CommandRegistry::size() const {
    return static_cast<int>(d_->rows.size());
}

quint64 CommandRegistry::revision() const {
    return d_->revision;
}

bool CommandRegistry::register_provider(ProviderSpec spec) {
    if (spec.id.trimmed().isEmpty() || spec.group.isEmpty() || !spec.query)
        return false;
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, spec]() { register_provider(spec); }, Qt::QueuedConnection);
        return true;
    }
    spec.owner = creg_owner_of(spec.owner, spec.id);
    spec.cap = std::max(1, spec.cap);
    spec.min_chars = std::max(0, spec.min_chars);
    if (!d_->providers.contains(spec.id))
        d_->provider_order.append(spec.id);
    d_->providers.insert(spec.id, std::move(spec));
    ++d_->revision;
    emit changed();
    return true;
}

QString CommandRegistry::register_provider(const QString& group, ProviderFn fn, const QString& owner, int cap) {
    ProviderSpec spec;
    spec.group = group;
    spec.query = std::move(fn);
    spec.owner = owner;
    spec.cap = cap;
    static std::atomic<int> generated{0};
    spec.id = QStringLiteral("%1.provider.%2").arg(owner.isEmpty() ? group : owner).arg(++generated);
    const QString id = spec.id;
    return register_provider(std::move(spec)) ? id : QString();
}

bool CommandRegistry::unregister_provider(const QString& id) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, id]() { unregister_provider(id); }, Qt::QueuedConnection);
        return true;
    }
    if (!d_->providers.remove(id))
        return false;
    d_->provider_order.removeAll(id);
    ++d_->revision;
    emit changed();
    return true;
}

QStringList CommandRegistry::provider_ids() const {
    return d_->provider_order;
}

quint64 CommandRegistry::query_providers(const QString& query, QObject* receiver, Delivery deliver, bool include_slow,
                                         QStringList* asked) {
    if (asked)
        asked->clear();
    if (!deliver || QThread::currentThread() != thread())
        return 0; // driven from the GUI thread only (the box)
    const int chars = static_cast<int>(query.trimmed().size());
    auto req = std::make_shared<Impl::Request>();
    req->receiver = receiver;
    req->deliver = std::move(deliver);
    QList<ProviderSpec> to_ask;
    for (const QString& pid : d_->provider_order) {
        const ProviderSpec& p = d_->providers[pid];
        if (chars < p.min_chars || (p.slow && !include_slow))
            continue;
        to_ask.append(p);
        req->pending.insert(p.id);
        req->caps.insert(p.id, p.cap);
        req->group_of.insert(p.id, p.group);
        if (p.cancel)
            req->cancels.insert(p.id, p.cancel);
    }
    if (to_ask.isEmpty())
        return 0;
    const quint64 id = d_->next_request++;
    d_->requests.insert(id, req);
    QPointer<CommandRegistry> self(this);
    for (const ProviderSpec& p : to_ask) {
        if (asked)
            asked->append(p.id);
        const QString pid = p.id;
        ResultSink sink = [self, id, pid](const QList<CommandItem>& rows, bool final) {
            if (!self)
                return;
            // Always queued: never re-entrant into the box, and safe from any thread.
            QMetaObject::invokeMethod(
                self.data(), [self, id, pid, rows, final]() {
                    if (self)
                        self->deliver_from_sink(id, pid, rows, final);
                },
                Qt::QueuedConnection);
        };
        try {
            p.query(query, id, sink);
        } catch (const std::exception&) {
            sink({}, true); // a throwing provider answers "nothing"
        } catch (...) {
            sink({}, true);
        }
    }
    return id;
}

void CommandRegistry::deliver_from_sink(quint64 request_id, const QString& provider_id, QList<CommandItem> rows,
                                        bool final) {
    auto it = d_->requests.find(request_id);
    if (it == d_->requests.end())
        return; // cancelled, finished or never existed
    std::shared_ptr<Impl::Request> req = it.value();
    if (!req->receiver) {
        d_->requests.erase(it);
        return;
    }
    if (!req->pending.contains(provider_id))
        return; // already answered final
    const int cap = req->caps.value(provider_id, 8);
    QList<CommandItem> kept;
    kept.reserve(std::min(cap, static_cast<int>(rows.size())));
    for (CommandItem& row : rows) {
        if (kept.size() >= cap)
            break;
        if (!creg_valid(row))
            continue;
        row.owner = creg_owner_of(row.owner, row.id);
        kept.append(std::move(row));
    }
    if (final)
        req->pending.remove(provider_id);
    req->deliver(request_id, provider_id, req->group_of.value(provider_id), kept, final);
    finish_if_done(request_id);
}

void CommandRegistry::finish_if_done(quint64 request_id) {
    auto it = d_->requests.find(request_id);
    if (it != d_->requests.end() && it.value()->pending.isEmpty())
        d_->requests.erase(it);
}

void CommandRegistry::cancel(quint64 request_id) {
    if (request_id == 0)
        return;
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, request_id]() { cancel(request_id); }, Qt::QueuedConnection);
        return;
    }
    auto it = d_->requests.find(request_id);
    if (it == d_->requests.end())
        return;
    std::shared_ptr<Impl::Request> req = it.value();
    d_->requests.erase(it);
    for (auto c = req->cancels.constBegin(); c != req->cancels.constEnd(); ++c) {
        if (!req->pending.contains(c.key()))
            continue; // already finished: nothing to stop
        try {
            c.value()(request_id);
        } catch (...) {
        }
    }
}

int CommandRegistry::open_requests() const {
    return static_cast<int>(d_->requests.size());
}

} // namespace fincept::command
