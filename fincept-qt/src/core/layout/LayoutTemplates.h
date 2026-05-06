#pragma once
// Built-in workspace templates seeded for new users on first run.
//
// Each persona = 1 frame containing 1 representative panel for that user
// type. Real seeded multi-panel layouts (watchlist + chart + news for
// equity_trader, etc.) can land later — v1 just gets the user into a
// useful screen so they're not staring at an empty dock.
//
// Persisted as kind="builtin" workspaces in LayoutCatalog so the user can
// re-load them, save modified copies as kind="user", or delete them.

#include "core/layout/LayoutTypes.h"

#include <QList>
#include <QString>

namespace fincept::layout {

class LayoutTemplates {
  public:
    struct Persona {
        QString id;            ///< Stable id ("equity_trader", ...). Used by make().
        QString display_name;  ///< Card title in the picker.
        QString description;   ///< 1-line marketing blurb under the title.
    };

    /// Returns the v1 persona list. Order matters — used by the picker grid.
    static QList<Persona> personas();

    /// Builds a kind="builtin" Workspace seeded for the given persona id.
    /// v1: 1 frame with 1 representative panel + persona name as workspace name.
    /// Returns a blank Workspace if persona_id is unknown.
    static Workspace make(const QString& persona_id);

  private:
    LayoutTemplates() = delete;
};

} // namespace fincept::layout
