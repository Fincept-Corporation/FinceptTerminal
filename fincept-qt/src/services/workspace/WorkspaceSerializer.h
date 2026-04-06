#pragma once
#include "services/workspace/WorkspaceTypes.h"
#include "core/result/Result.h"

#include <QJsonDocument>
#include <QString>

namespace fincept {

/// Pure static utility — converts WorkspaceDef ↔ QJsonDocument.
/// No file I/O, no Qt signals. Stateless.
class WorkspaceSerializer {
  public:
    /// Serialize a full workspace to JSON.
    static QJsonDocument to_json(const WorkspaceDef& ws);

    /// Deserialize a workspace from JSON.
    /// Returns Err if version is unsupported or required fields are missing.
    static Result<WorkspaceDef> from_json(const QJsonDocument& doc);

    /// Read only the metadata block from a .fwsp file path.
    /// Fast — does not parse the full document.
    static Result<WorkspaceSummary> summary_from_file(const QString& path);

  private:
    WorkspaceSerializer() = delete;

    static QJsonObject  serialize_layout(const screens::GridLayout& layout);
    static screens::GridLayout deserialize_layout(const QJsonObject& obj);
};

} // namespace fincept
