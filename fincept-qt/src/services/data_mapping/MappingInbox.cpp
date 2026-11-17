// MappingInbox.cpp — see MappingInbox.h.

#include "services/data_mapping/MappingInbox.h"

#include "core/events/EventBus.h"

#include <QVariantMap>

namespace fincept::services::datamapping {

MappingInbox& MappingInbox::instance() {
    static MappingInbox s;
    return s;
}

void MappingInbox::post(const MappingHandoff& handoff, bool switch_tab) {
    pending_ = handoff;
    emit posted();
    if (switch_tab)
        fincept::EventBus::instance().publish(QStringLiteral("nav.switch_screen"),
                                              QVariantMap{{QStringLiteral("screen_id"), QStringLiteral("data_mapping")}});
}

std::optional<MappingHandoff> MappingInbox::take() {
    std::optional<MappingHandoff> out;
    out.swap(pending_);
    return out;
}

} // namespace fincept::services::datamapping
