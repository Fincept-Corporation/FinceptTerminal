#pragma once
// MappingInbox — "Send to Data Mapping": another tab hands a source (and the rows it already
// fetched, so the mapping grid fills without a second fetch) to the Data Mapping tab.
//
//   services::datamapping::MappingInbox::instance().post(handoff);   // then switch tab
//
// The Data Mapping tab takes the pending hand-off when it is shown (it may not exist yet when
// post() runs) and on posted() while it is already open. GUI thread.

#include "services/data_mapping/MappingSpec.h"
#include "services/data_sources/TabularData.h"

#include <QObject>
#include <QString>

#include <optional>

namespace fincept::services::datamapping {

struct MappingHandoff {
    SourceRef source;                                    ///< what the mapping will read
    fincept::services::datasources::TabularResult sample; ///< rows already on screen (may be empty)
    QString label;                                       ///< "BCB · selic", "prices_db · prices"
};

class MappingInbox : public QObject {
    Q_OBJECT
  public:
    static MappingInbox& instance();

    /// Keep `handoff` for the Data Mapping tab (replaces an untaken one) and switch to that tab.
    void post(const MappingHandoff& handoff, bool switch_tab = true);
    bool has_pending() const { return pending_.has_value(); }
    std::optional<MappingHandoff> take();

  signals:
    void posted();

  private:
    explicit MappingInbox(QObject* parent = nullptr) : QObject(parent) {}
    std::optional<MappingHandoff> pending_;
};

} // namespace fincept::services::datamapping
