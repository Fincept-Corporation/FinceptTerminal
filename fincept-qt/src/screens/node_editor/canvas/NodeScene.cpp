#include "screens/node_editor/canvas/NodeScene.h"

#include "core/logging/Logger.h"
#include "screens/node_editor/canvas/EdgeItem.h"
#include "screens/node_editor/canvas/NodeItem.h"
#include "screens/node_editor/canvas/PortItem.h"
#include "screens/node_editor/canvas/TempEdge.h"
#include "services/workflow/NodeRegistry.h"

#include <QUuid>

namespace fincept::workflow {

NodeScene::NodeScene(QObject* parent) : QGraphicsScene(parent) {
    setSceneRect(-5000, -5000, 10000, 10000);
}

NodeItem* NodeScene::add_node(const NodeDef& def, const NodeTypeDef& type_def) {
    auto* item = new NodeItem(def, type_def);
    addItem(item);
    nodes_.insert(def.id, item);

    // Wire signals
    connect(item, &NodeItem::node_selected, this, &NodeScene::node_selection_changed);
    connect(item, &NodeItem::node_moved, this, [this](const QString& id, double x, double y) {
        adjust_edges_for_node(id);
        emit node_position_changed(id, x, y);
    });
    connect(item, &NodeItem::delete_requested, this, &NodeScene::remove_node);
    connect(item, &NodeItem::duplicate_requested, this, &NodeScene::node_duplicate_requested);
    connect(item, &NodeItem::execute_from_requested, this, &NodeScene::node_execute_from_requested);

    // Wire port connection signals
    for (auto* port : item->input_ports()) {
        connect(port, &PortItem::connection_started, this, &NodeScene::start_temp_edge);
    }
    for (auto* port : item->output_ports()) {
        connect(port, &PortItem::connection_started, this, &NodeScene::start_temp_edge);
    }

    emit node_added(def.id);
    LOG_INFO("NodeEditor", QString("Added node: %1 (%2)").arg(def.name, def.type));
    return item;
}

EdgeItem* NodeScene::add_edge(const EdgeDef& def) {
    auto* src_node = find_node(def.source_node);
    auto* tgt_node = find_node(def.target_node);
    if (!src_node || !tgt_node)
        return nullptr;

    auto* src_port = src_node->find_port(def.source_port);
    auto* tgt_port = tgt_node->find_port(def.target_port);
    if (!src_port || !tgt_port)
        return nullptr;

    auto* edge = new EdgeItem(def.id, src_port, tgt_port);
    addItem(edge);
    edges_.insert(def.id, edge);

    connect(edge, &EdgeItem::edge_selected, this, [this](const QString& id) { Q_UNUSED(id); });
    connect(edge, &EdgeItem::delete_requested, this, &NodeScene::remove_edge);

    emit edge_added(def.id);
    return edge;
}

void NodeScene::remove_node(const QString& node_id) {
    auto* item = find_node(node_id);
    if (!item)
        return;

    // Remove all connected edges first
    QStringList edge_ids;
    for (auto* port : item->input_ports())
        for (auto* edge : port->edges())
            edge_ids.append(edge->edge_id());
    for (auto* port : item->output_ports())
        for (auto* edge : port->edges())
            edge_ids.append(edge->edge_id());

    for (const auto& eid : edge_ids)
        remove_edge(eid);

    nodes_.remove(node_id);
    removeItem(item);
    delete item;

    emit node_removed(node_id);
    LOG_INFO("NodeEditor", QString("Removed node: %1").arg(node_id));
}

void NodeScene::remove_edge(const QString& edge_id) {
    auto it = edges_.find(edge_id);
    if (it == edges_.end())
        return;

    auto* edge = it.value();
    edges_.erase(it);
    removeItem(edge);
    delete edge;

    emit edge_removed(edge_id);
}

void NodeScene::clear_all() {
    QStringList node_ids = nodes_.keys();
    for (const auto& id : node_ids)
        remove_node(id);
    nodes_.clear();
    edges_.clear();
}

WorkflowDef NodeScene::serialize() const {
    WorkflowDef wf;
    for (auto it = nodes_.constBegin(); it != nodes_.constEnd(); ++it) {
        NodeDef nd = it.value()->node_def();
        nd.x = it.value()->pos().x();
        nd.y = it.value()->pos().y();
        wf.nodes.append(nd);
    }
    for (auto it = edges_.constBegin(); it != edges_.constEnd(); ++it) {
        EdgeDef ed;
        ed.id = it.value()->edge_id();
        ed.source_node = it.value()->source_port()->parent_node()->node_def().id;
        ed.target_node = it.value()->target_port()->parent_node()->node_def().id;
        ed.source_port = it.value()->source_port()->def().id;
        ed.target_port = it.value()->target_port()->def().id;
        wf.edges.append(ed);
    }
    return wf;
}

void NodeScene::deserialize(const WorkflowDef& workflow) {
    clear_all();

    auto& registry = NodeRegistry::instance();
    for (const auto& nd : workflow.nodes) {
        const auto* type_def = registry.find(nd.type);
        if (type_def)
            add_node(nd, *type_def);
        else
            LOG_WARN("NodeEditor", QString("Unknown node type: %1").arg(nd.type));
    }

    for (const auto& ed : workflow.edges)
        add_edge(ed);
}

NodeItem* NodeScene::find_node(const QString& id) const {
    auto it = nodes_.constFind(id);
    return it != nodes_.constEnd() ? it.value() : nullptr;
}

QVector<NodeItem*> NodeScene::node_items() const {
    QVector<NodeItem*> result;
    result.reserve(nodes_.size());
    for (auto it = nodes_.constBegin(); it != nodes_.constEnd(); ++it)
        result.append(it.value());
    return result;
}

void NodeScene::start_temp_edge(PortItem* from) {
    cancel_temp_edge();
    temp_edge_ = new TempEdge(from);
    addItem(temp_edge_);
}

void NodeScene::update_temp_edge(const QPointF& scene_pos) {
    if (temp_edge_)
        temp_edge_->update_target(scene_pos);
}

void NodeScene::finish_temp_edge(PortItem* target) {
    if (!temp_edge_)
        return;

    auto* source = temp_edge_->source_port();
    if (source->can_connect_to(target)) {
        // Determine which is output and which is input
        PortItem* out_port = (source->def().direction == PortDirection::Output) ? source : target;
        PortItem* in_port = (source->def().direction == PortDirection::Output) ? target : source;

        EdgeDef ed;
        ed.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        ed.source_node = out_port->parent_node()->node_def().id;
        ed.target_node = in_port->parent_node()->node_def().id;
        ed.source_port = out_port->def().id;
        ed.target_port = in_port->def().id;
        add_edge(ed);
    }

    cancel_temp_edge();
}

void NodeScene::cancel_temp_edge() {
    if (temp_edge_) {
        removeItem(temp_edge_);
        delete temp_edge_;
        temp_edge_ = nullptr;
    }
}

void NodeScene::adjust_edges_for_node(const QString& node_id) {
    auto* node = find_node(node_id);
    if (!node)
        return;

    for (auto* port : node->input_ports())
        for (auto* edge : port->edges())
            edge->adjust();
    for (auto* port : node->output_ports())
        for (auto* edge : port->edges())
            edge->adjust();
}

void NodeScene::set_edges_animated(const QString& node_id, bool animated) {
    auto* node = find_node(node_id);
    if (!node)
        return;

    for (auto* port : node->input_ports())
        for (auto* edge : port->edges())
            edge->set_animated(animated);
    for (auto* port : node->output_ports())
        for (auto* edge : port->edges())
            edge->set_animated(animated);
}

void NodeScene::stop_all_edge_animations() {
    for (auto it = edges_.constBegin(); it != edges_.constEnd(); ++it)
        it.value()->set_animated(false);
}

} // namespace fincept::workflow
