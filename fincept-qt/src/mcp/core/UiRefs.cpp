// UiRefs.cpp — the ref registry: short refs (w1 p3 d1 m1 e12, e12:r5, e12:w3)
// that stay valid while their element lives, never move to another object, follow
// a listed row when its table re-sorts, and — once their element is gone — fail
// with the nearest surviving match named. See UiEngine.h.

#include "mcp/core/CoreToolsCommon.h"
#include "mcp/core/UiEngine.h"
#include "mcp/core/UiErrors.h"

#include <QAbstractItemView>
#include <QAction>
#include <QRegularExpression>

namespace fincept::mcp::core::ui {

namespace ui_refs_detail {

constexpr int kSweepEvery = 2000;
constexpr int kMaxAnchorsPerView = 2000;
constexpr int kMaxListed = 6000; // what refs were listed as (also kept after they die), for stale-ref hints

} // namespace ui_refs_detail

using namespace ui_refs_detail;

// ═════════════════════════════════════════════════════════════════════════════
// Refs
// ═════════════════════════════════════════════════════════════════════════════

Refs& Refs::instance() {
    static Refs r;
    return r;
}

QString Refs::ref_for(QObject* obj, RefKind kind, int sub) {
    if (!obj)
        return {};
    const char k = static_cast<char>(kind);
    const QString key = QString(QLatin1Char(k)) + QString::number(reinterpret_cast<quintptr>(obj), 16) +
                        QLatin1Char(':') + QString::number(sub);
    const auto it = by_key_.constFind(key);
    if (it != by_key_.constEnd()) {
        const auto e = by_ref_.constFind(it.value());
        if (e != by_ref_.constEnd() && e->obj == obj)
            return it.value();
        by_ref_.remove(it.value()); // the old object died and its address was reused
        by_key_.remove(key);
    }
    const QString ref = QString(QLatin1Char(k)) + QString::number(++next_[k]);
    by_ref_.insert(ref, Entry{obj, kind, sub});
    by_key_.insert(key, ref);
    if (++allocations_since_sweep_ >= kSweepEvery)
        sweep();
    return ref;
}

void Refs::sweep() {
    allocations_since_sweep_ = 0;
    for (auto it = by_key_.begin(); it != by_key_.end();) {
        const auto e = by_ref_.constFind(it.value());
        if (e == by_ref_.constEnd() || !e->obj) {
            anchors_.remove(it.value());
            web_anchors_.remove(it.value());
            by_ref_.remove(it.value());
            it = by_key_.erase(it);
        } else {
            ++it;
        }
    }
}

bool Refs::was_issued(const QString& raw) const {
    // Numbers only grow, so any number up to the last one handed out was issued
    // (its entry may already be swept because its element died).
    const QString base = raw.trimmed().section(QLatin1Char(':'), 0, 0);
    if (base.size() < 2 || by_ref_.contains(base))
        return by_ref_.contains(base);
    bool ok = false;
    const quint64 n = base.mid(1).toULongLong(&ok);
    return ok && n >= 1 && n <= next_.value(base.at(0).toLatin1(), 0);
}

void Refs::note(const QString& ref, const QString& role, const QString& name, QWidget* scope) {
    if (ref.isEmpty())
        return;
    auto it = listed_.find(ref);
    if (it != listed_.end()) {
        it->role = role;
        it->name = name;
        it->scope = scope;
        return;
    }
    listed_.insert(ref, Listed{role, name, QPointer<QWidget>(scope)});
    listed_order_.append(ref);
    if (listed_order_.size() > kMaxListed) {
        const int drop = static_cast<int>(listed_order_.size()) - kMaxListed + kMaxListed / 4;
        for (int i = 0; i < drop; ++i)
            listed_.remove(listed_order_.at(i));
        listed_order_.erase(listed_order_.begin(), listed_order_.begin() + drop);
    }
}

QString Refs::stale_text(const QString& raw, const QString& base) {
    const auto l = listed_.constFind(base);
    QString what = raw.trimmed();
    QString hint = QStringLiteral("take a fresh ui_snapshot.");
    if (l != listed_.constEnd()) {
        const Listed info = l.value(); // find_by_name below may add to listed_
        if (!info.name.isEmpty())
            what += QStringLiteral(" (%1 %2)").arg(info.role, common::quoted(common::clip(info.name, 40)));
        QWidget* scope = info.scope ? info.scope.data() : active_frame();
        if (!info.name.isEmpty() && scope && scope->isVisible()) {
            // The nearest surviving match: the same words and role where it was.
            // Named, never acted on — the caller decides.
            QStringList same;
            for (const NameMatch& m : find_by_name(scope, info.name, info.role)) {
                if (m.ref.isEmpty() || m.ref == base || m.role != info.role ||
                    m.name.compare(info.name, Qt::CaseInsensitive) != 0)
                    continue;
                same << QStringLiteral("%1 %2 %3 in %4")
                            .arg(m.ref, m.role, common::quoted(common::clip(m.name, 40)), location_label(m.widget));
                if (same.size() >= 3)
                    break;
            }
            if (same.size() == 1)
                hint = QStringLiteral("the same control now: %1.").arg(same.first());
            else if (same.size() > 1)
                hint = QStringLiteral("several controls look like it now: %1 — take a ui_snapshot.")
                           .arg(same.join(QStringLiteral("; ")));
        }
    }
    return err_text(Err::StaleRef, QStringLiteral("%1 is gone — closed or rebuilt.").arg(what), hint);
}

void Refs::anchor_row(QAbstractItemView* view, const QString& base_ref, int row, const QModelIndex& first) {
    if (!view || base_ref.isEmpty() || row < 0 || !first.isValid())
        return;
    auto& rows = anchors_[base_ref];
    if (rows.size() >= kMaxAnchorsPerView && !rows.contains(row))
        rows.clear(); // a memo of what was last listed, not a store
    rows.insert(row, RowAnchor{QPersistentModelIndex(first), row_key(view, first)});
}

void Refs::anchor_web(const QString& base_ref, int index, const QString& key) {
    if (base_ref.isEmpty() || index < 0 || key.isEmpty())
        return;
    auto& controls = web_anchors_[base_ref];
    if (controls.size() >= kMaxAnchorsPerView && !controls.contains(index))
        controls.clear();
    controls.insert(index, key);
}

std::optional<Resolved> Refs::resolve(const QString& raw, QString* why) {
    static const QRegularExpression rx(QStringLiteral("^\\s*([wpdme])(\\d+)(?::([rw])(\\d+))?\\s*$"));
    const auto m = rx.match(raw);
    auto fail = [why](const QString& reason) -> std::optional<Resolved> {
        if (why)
            *why = reason;
        return std::nullopt;
    };
    if (!m.hasMatch())
        return fail(err_text(Err::InvalidValue,
                             QStringLiteral("\"%1\" is not a ref (refs look like p3, e12 or e12:r5).").arg(raw.trimmed())));
    const QString base = m.captured(1) + m.captured(2);
    const auto it = by_ref_.constFind(base);
    if (it == by_ref_.constEnd()) {
        if (was_issued(base))
            return fail(stale_text(raw, base));
        return fail(err_text(Err::NotFound, QStringLiteral("%1 was never listed.").arg(raw.trimmed()),
                             QStringLiteral("take a ui_snapshot for refs.")));
    }
    if (!it->obj)
        return fail(stale_text(raw, base));
    Resolved r;
    r.ref = raw.trimmed();
    r.kind = it->kind;
    r.sub = it->sub;
    const bool web_suffix = m.captured(3) == QLatin1String("w");
    r.row = m.captured(4).isEmpty() || web_suffix ? -1 : m.captured(4).toInt();
    QObject* obj = it->obj.data();
    if (host::is_panel(obj)) {
        auto* dock = static_cast<QWidget*>(obj);
        if (host::panel_closed(dock))
            return fail(err_text(Err::StaleRef, QStringLiteral("%1 was closed.").arg(base),
                                 QStringLiteral("ui_open reopens the screen; ui_overview lists what is open.")));
        r.dock = dock;
        r.widget = host::panel_content(dock);
    } else if (auto* action = qobject_cast<QAction*>(obj)) {
        r.action = action;
    } else if (auto* w = qobject_cast<QWidget*>(obj)) {
        r.widget = w;
    }
    if (!r.widget && !r.action)
        return fail(stale_text(raw, base));

    // A web control ref names the control that was LISTED under that number;
    // the page script finds it again by its role and name when it is used.
    if (web_suffix) {
        if (!web_view_of(r.widget))
            return fail(err_text(Err::StaleRef, QStringLiteral("%1 shows no web page now.").arg(base)));
        r.web = m.captured(4).toInt();
        const auto controls = web_anchors_.constFind(base);
        if (controls == web_anchors_.constEnd() || !controls->contains(r.web))
            return fail(err_text(Err::NotFound, QStringLiteral("%1 was never listed.").arg(r.ref),
                                 QStringLiteral("ui_read(ref=\"%1\") lists the page's controls.").arg(base)));
        r.web_key = controls->value(r.web);
        return r;
    }

    // A row ref means the row that was LISTED under that number. Follow it if
    // the view re-sorted or inserted rows; recognise it by its text if the model
    // was rebuilt; refuse if it is gone — never act on whatever row now has the
    // number (a live blotter re-sorts every tick).
    if (r.row >= 0) {
        auto* view = qobject_cast<QAbstractItemView*>(r.widget.data());
        const auto rows = anchors_.constFind(base);
        if (view && rows != anchors_.constEnd()) {
            const auto a = rows->constFind(r.row);
            if (a != rows->constEnd()) {
                int now = -1;
                if (a->idx.isValid() && a->idx.model() == view->model())
                    now = row_of_index(view, a->idx);
                if (now >= 0 && !a->key.isEmpty() &&
                    row_key(view, view_index(view, now, visible_columns(view).value(0, 0))) != a->key)
                    now = -1; // same slot, different contents: the model was refilled in place
                if (now < 0 && !a->key.isEmpty()) {
                    int found = -1;
                    int hits = 0;
                    int scanned = 0;
                    for_each_view_row(view, 0, [&](int row, const QModelIndex& first) {
                        if (row_key(view, first) == a->key) {
                            found = row;
                            ++hits;
                        }
                        return hits < 2 && ++scanned < kMaxRowScan;
                    });
                    if (hits == 1)
                        now = found;
                }
                if (now < 0)
                    return fail(err_text(Err::StaleRef,
                                         QStringLiteral("%1 (%2) is no longer in that list — it was removed or "
                                                        "changed.")
                                             .arg(r.ref, common::quoted(common::clip(a->key, 60))),
                                         QStringLiteral("read the list again: ui_read(ref=\"%1\").").arg(base)));
                if (now != r.row) {
                    r.listed_row = r.row;
                    r.row = now;
                }
            }
        }
    }
    return r;
}

} // namespace fincept::mcp::core::ui
