#pragma once
#include "core/actions/ActionDef.h"

#include <QList>
#include <QObject>
#include <QString>

namespace fincept::ui {

/// Phase 9 / decision 7.6: unified search index over ActionRegistry +
/// (later) symbols + open panels + layouts + profiles.
///
/// v1 ships the action-registry slice. Symbols/panels/layouts integration
/// arrives when their respective registries gain change-notification
/// signals. The interface is shaped so adding a new source is one
/// `query()` switch case.
class SuggestionIndex : public QObject {
    Q_OBJECT
  public:
    enum class SourceKind {
        Action,
        Layout,
        Panel,
        Symbol,
    };

    struct Match {
        SourceKind source = SourceKind::Action;
        QString id;          ///< action id, layout uuid, panel uuid, etc.
        QString display;     ///< user-visible text
        QString category;    ///< for grouping in palette UI
        int score = 0;       ///< higher = better match (relevance)
    };

    static SuggestionIndex& instance();

    /// Top N matches across all sources.
    QList<Match> query(const QString& prefix, int limit = 25);

  private:
    SuggestionIndex() = default;
};

} // namespace fincept::ui
