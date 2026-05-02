#include "core/panel/PanelRegistry.h"

#include "core/logging/Logger.h"
#include "core/panel/PanelHandle.h"

namespace fincept {

namespace {
constexpr const char* kPanelRegistryTag = "PanelRegistry";
} // namespace

PanelRegistry& PanelRegistry::instance() {
    static PanelRegistry s;
    return s;
}

PanelHandle* PanelRegistry::register_panel(PanelHandle* handle) {
    if (!handle) {
        LOG_WARN(kPanelRegistryTag, "register_panel called with null handle");
        return nullptr;
    }
    const PanelInstanceId id = handle->instance_id();
    if (id.is_null()) {
        LOG_WARN(kPanelRegistryTag, "register_panel called with null instance_id");
        return nullptr;
    }

    const bool replacing = by_id_.contains(id);
    if (replacing) {
        // Tear down the old entry first so signal connections don't double-fire.
        PanelHandle* old = by_id_.value(id);
        if (old && old != handle)
            old->deleteLater();
    } else {
        insertion_order_.append(id);
    }

    handle->setParent(this);
    by_id_.insert(id, handle);
    wire_handle_signals(handle);

    if (replacing) {
        LOG_DEBUG(kPanelRegistryTag, QString("Replaced panel: %1 (%2)")
                                          .arg(id.to_string(), handle->type_id()));
        emit panel_replaced(id);
    } else {
        LOG_DEBUG(kPanelRegistryTag, QString("Registered panel: %1 (%2)")
                                          .arg(id.to_string(), handle->type_id()));
        emit panel_added(id);
    }
    return handle;
}

void PanelRegistry::unregister_panel(PanelInstanceId id) {
    auto it = by_id_.constFind(id);
    if (it == by_id_.constEnd())
        return;
    PanelHandle* handle = it.value();
    by_id_.erase(it);
    insertion_order_.removeAll(id);
    LOG_DEBUG(kPanelRegistryTag, QString("Unregistered panel: %1").arg(id.to_string()));
    emit panel_removed(id);
    if (handle)
        handle->deleteLater();
}

PanelHandle* PanelRegistry::find(PanelInstanceId id) const {
    return by_id_.value(id, nullptr);
}

QList<PanelHandle*> PanelRegistry::all_panels() const {
    QList<PanelHandle*> out;
    out.reserve(insertion_order_.size());
    for (const PanelInstanceId& id : insertion_order_) {
        if (PanelHandle* h = by_id_.value(id, nullptr))
            out.append(h);
    }
    return out;
}

QList<PanelHandle*> PanelRegistry::find_by_type(const QString& type_id) const {
    QList<PanelHandle*> out;
    for (const PanelInstanceId& id : insertion_order_) {
        PanelHandle* h = by_id_.value(id, nullptr);
        if (h && h->type_id() == type_id)
            out.append(h);
    }
    return out;
}

QList<PanelHandle*> PanelRegistry::find_by_frame(WindowId frame_id) const {
    QList<PanelHandle*> out;
    for (const PanelInstanceId& id : insertion_order_) {
        PanelHandle* h = by_id_.value(id, nullptr);
        if (h && h->frame_id() == frame_id)
            out.append(h);
    }
    return out;
}

QList<PanelHandle*> PanelRegistry::find_by_group(const QString& link_group) const {
    QList<PanelHandle*> out;
    for (const PanelInstanceId& id : insertion_order_) {
        PanelHandle* h = by_id_.value(id, nullptr);
        if (h && h->link_group() == link_group)
            out.append(h);
    }
    return out;
}

int PanelRegistry::size() const {
    return by_id_.size();
}

void PanelRegistry::wire_handle_signals(PanelHandle* handle) {
    // The registry doesn't maintain per-attribute index lists today —
    // find_by_* methods walk the insertion order each call. At expected
    // panel counts (≤200) this is faster than maintaining index hashes
    // because the working set fits in cache and the bulk consumers
    // (linking, palette) need ordered output anyway.
    //
    // We still re-emit a coarse `panel_metadata_changed` so consumers like
    // the status bar can refresh their summary without subscribing to
    // every PanelHandle individually.
    const PanelInstanceId id = handle->instance_id();
    connect(handle, &PanelHandle::title_changed, this,
            [this, id](const QString&) { emit panel_metadata_changed(id); });
    connect(handle, &PanelHandle::frame_id_changed, this,
            [this, id](WindowId) { emit panel_metadata_changed(id); });
    connect(handle, &PanelHandle::link_group_changed, this,
            [this, id](const QString&) { emit panel_metadata_changed(id); });
    connect(handle, &PanelHandle::state_changed, this,
            [this, id](PanelHandle::State) { emit panel_metadata_changed(id); });
}

} // namespace fincept
