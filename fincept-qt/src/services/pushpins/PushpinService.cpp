#include "services/pushpins/PushpinService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

namespace fincept {

PushpinService& PushpinService::instance() {
    static PushpinService s;
    return s;
}

PushpinService::PushpinService() {
    load();
}

bool PushpinService::contains(const SymbolRef& ref) const {
    for (const SymbolRef& p : pins_)
        if (p == ref)
            return true;
    return false;
}

void PushpinService::pin(const SymbolRef& ref) {
    if (!ref.is_valid() || contains(ref))
        return;
    pins_.append(ref);
    save();
    emit pins_changed();
}

void PushpinService::unpin(const SymbolRef& ref) {
    const int before = static_cast<int>(pins_.size());
    pins_.erase(std::remove_if(pins_.begin(), pins_.end(),
                               [&ref](const SymbolRef& p) { return p == ref; }),
                pins_.end());
    if (static_cast<int>(pins_.size()) != before) {
        save();
        emit pins_changed();
    }
}

void PushpinService::clear() {
    if (pins_.isEmpty())
        return;
    pins_.clear();
    save();
    emit pins_changed();
}

void PushpinService::move(const SymbolRef& ref, int new_index) {
    const int old = pins_.indexOf(ref);
    if (old < 0 || new_index < 0 || new_index >= pins_.size() || old == new_index)
        return;
    pins_.move(old, new_index);
    save();
    emit pins_changed();
}

void PushpinService::load() {
    QSettings settings("Fincept", "FinceptTerminal");
    const QByteArray blob = settings.value("pushpins/list").toByteArray();
    const QJsonDocument doc = QJsonDocument::fromJson(blob);
    if (!doc.isArray())
        return;
    for (const QJsonValue& v : doc.array()) {
        if (!v.isObject())
            continue;
        const SymbolRef r = SymbolRef::from_json(v.toObject());
        if (r.is_valid())
            pins_.append(r);
    }
}

void PushpinService::save() const {
    QJsonArray arr;
    for (const SymbolRef& p : pins_)
        arr.append(p.to_json());
    QSettings settings("Fincept", "FinceptTerminal");
    settings.setValue("pushpins/list", QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

} // namespace fincept
