#pragma once
#include "services/workspace/WorkspaceTypes.h"
#include <QVector>

namespace fincept {

/// Built-in workspace templates. Stateless, returns pre-configured WorkspaceDef objects.
/// Metadata id/name/timestamps are filled in by WorkspaceManager::new_workspace().
class WorkspaceTemplates {
  public:
    /// Return a WorkspaceDef seeded from the named template.
    /// Falls back to blank() if template_id is empty or unknown.
    static WorkspaceDef make(const QString& template_id);

    /// All available templates as summaries (for the New Workspace dialog).
    static QVector<WorkspaceSummary> available();

  private:
    WorkspaceTemplates() = delete;

    static WorkspaceDef blank();
    static WorkspaceDef equity_trader();
    static WorkspaceDef crypto_trader();
    static WorkspaceDef portfolio_manager();
    static WorkspaceDef research_analyst();
    static WorkspaceDef macro_economist();
};

} // namespace fincept
