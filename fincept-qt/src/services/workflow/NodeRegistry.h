#pragma once

#include "screens/node_editor/NodeEditorTypes.h"

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::workflow {

/// Singleton registry of all available node type definitions.
class NodeRegistry {
  public:
    static NodeRegistry& instance();

    void register_type(NodeTypeDef def);
    const NodeTypeDef* find(const QString& type_id) const;
    QVector<NodeTypeDef> all() const;
    QVector<NodeTypeDef> by_category(const QString& category) const;
    QStringList categories() const;

  private:
    NodeRegistry();
    void register_builtin_nodes();

    QMap<QString, NodeTypeDef> registry_;
};

} // namespace fincept::workflow
