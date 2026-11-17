#include "screens/node_editor/NodeEditorScreen.h"

#include "core/agent/AgentRiskMarkers.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/common/CommandRows.h"
#include "screens/node_editor/NodeEditorCommands.h"
#include "screens/node_editor/canvas/EdgeItem.h"
#include "screens/node_editor/canvas/MiniMap.h"
#include "screens/node_editor/canvas/NodeCanvas.h"
#include "screens/node_editor/canvas/NodeItem.h"
#include "screens/node_editor/canvas/NodeScene.h"
#include "screens/node_editor/palette/NodePalette.h"
#include "screens/node_editor/properties/ExecutionResultsPanel.h"
#include "screens/node_editor/properties/NodePropertiesPanel.h"
#include "screens/node_editor/toolbar/DeployDialog.h"
#include "screens/node_editor/toolbar/NodeEditorToolbar.h"
#include "services/cloud/CloudSyncEngine.h"
#include "services/workflow/NodeRegistry.h"
#include "services/workflow/WorkflowExecutor.h"
#include "services/workflow/WorkflowService.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/EmptyState.h"

#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QFileDialog>
#include <QFrame>
#include <QInputDialog>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSet>
#include <QSettings>
#include <QSplitter>
#include <QTime>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>
#include <iterator>

namespace fincept::workflow {

namespace {
// Templates offered as one-click rows on the empty canvas (indices into
// NodeEditorScreen::template_titles()): the ones that only read data.
constexpr int kNodeEditorSuggestedTemplates[] = {0, 1, 2, 4};

// "1. Hello World — Trigger → Set Variable → Display" → "Hello World" + the rest.
void node_editor_split_template_title(const QString& title, QString* label, QString* secondary) {
    static const QRegularExpression kNumberPrefix(QStringLiteral("^\\d+\\.\\s*"));
    QString t = title;
    t.remove(kNumberPrefix);
    const int dash = t.indexOf(QStringLiteral(" — "));
    *label = dash > 0 ? t.left(dash).trimmed() : t.trimmed();
    *secondary = dash > 0 ? t.mid(dash + 3).trimmed() : QString();
}
} // namespace

// The scene is a child of this screen and the views (canvas, minimap) live in the splitter,
// so on quit the scene can be destroyed while both views are still attached.
// ~QGraphicsScene() then calls setScene(nullptr) on each view, which re-reads the
// stylesheet of a window that is already half torn down and crashed Fincept on every
// Cmd+Q ("quit unexpectedly"). Detach the views and drop the scene first, while
// everything around them is still intact.
NodeEditorScreen::~NodeEditorScreen() {
    if (minimap_)
        minimap_->setScene(nullptr);
    if (canvas_)
        canvas_->setScene(nullptr);
    delete scene_;
    scene_ = nullptr;
}

NodeEditorScreen::NodeEditorScreen(QWidget* parent)
    : QWidget(parent), undo_stack_(new QUndoStack(this)), auto_save_timer_(new QTimer(this)) {
    build_ui();
    wire_signals();
    // keyPressEvent: Ctrl+Enter runs the workflow — the same kind as EXECUTE (`order`);
    // while a run is going it stops it instead and carries no marker (execution signals).
    setProperty(fincept::core::agent::kAgentRiskKeysProperty, QStringLiteral("Ctrl+Return=order;Ctrl+Enter=order"));

    // Auto-save every 30 seconds (P3: started/stopped in show/hideEvent)
    auto_save_timer_->setInterval(30000);
    connect(auto_save_timer_, &QTimer::timeout, this, &NodeEditorScreen::on_auto_save);

    LOG_INFO("NodeEditor", "Node editor screen created");
}

void NodeEditorScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);

    // Force the global stylesheet to re-polish this widget tree.
    // Without this, ADS internal container widgets created after qApp->setStyleSheet()
    // don't get the theme applied and fall back to Windows system grey.
    QApplication::style()->polish(this);

    auto_save_timer_->start();
    // Rate-gated pull of cloud workflows on screen entry (no-op when sync is off).
    fincept::services::cloud::CloudSyncEngine::instance().request_pull(QStringLiteral("workflow"));
    if (minimap_)
        minimap_->start_tracking();
    // Resume edge animations if execution is in progress
    if (scene_ && scene_->has_animated_edges())
        scene_->resume_edge_animations();

    // Restore workspace state
    QSettings settings;
    settings.beginGroup("NodeEditor");
    if (settings.contains("zoom")) {
        qreal zoom = settings.value("zoom", 1.0).toDouble();
        canvas_->resetTransform();
        canvas_->scale(zoom, zoom);
    }
    if (settings.contains("scroll_x")) {
        canvas_->horizontalScrollBar()->setValue(settings.value("scroll_x", 0).toInt());
        canvas_->verticalScrollBar()->setValue(settings.value("scroll_y", 0).toInt());
    }
    if (settings.contains("last_workflow_id")) {
        QString last_id = settings.value("last_workflow_id").toString();
        // Only restore into a blank editor. A template just loaded (id cleared, canvas
        // full) or an import must not be silently replaced by the last saved workflow
        // the moment the user switches tabs and comes back.
        if (!last_id.isEmpty() && current_workflow_id_.isEmpty() && scene_->is_empty())
            WorkflowService::instance().load_workflow(last_id);
    }
    settings.endGroup();

    LOG_DEBUG("NodeEditor", "Node editor shown");
}

void NodeEditorScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    auto_save_timer_->stop();
    if (minimap_)
        minimap_->stop_tracking();
    // Pause edge animations while hidden — no point rendering offscreen
    if (scene_)
        scene_->pause_edge_animations();

    // Save workspace state
    QSettings settings;
    settings.beginGroup("NodeEditor");
    settings.setValue("zoom", canvas_->transform().m11());
    settings.setValue("scroll_x", canvas_->horizontalScrollBar()->value());
    settings.setValue("scroll_y", canvas_->verticalScrollBar()->value());
    if (!current_workflow_id_.isEmpty())
        settings.setValue("last_workflow_id", current_workflow_id_);
    settings.endGroup();

    LOG_DEBUG("NodeEditor", "Node editor hidden");
}

void NodeEditorScreen::keyPressEvent(QKeyEvent* event) {
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
        (event->modifiers() & Qt::ControlModifier)) {
        on_execute(); // Ctrl+Enter — run (or stop a running workflow)
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape && WorkflowService::instance().is_executing()) {
        on_stop(); // Esc — cancel the run
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Undo)) {
        undo_stack_->undo();
        event->accept();
    } else if (event->matches(QKeySequence::Redo)) {
        undo_stack_->redo();
        event->accept();
    } else if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        // Delete selected nodes. Now undoable (RemoveNodesCommand restores the
        // nodes AND every edge that touched them), so no confirmation prompt —
        // Ctrl+Z is the recovery path.
        QStringList ids;
        for (auto* item : scene_->selectedItems()) {
            if (auto* node = dynamic_cast<NodeItem*>(item))
                ids.append(node->node_def().id);
        }
        if (!ids.isEmpty()) {
            push_remove_nodes(ids, ids.size() > 1 ? tr("Delete %1 nodes").arg(ids.size()) : tr("Delete node"));
            event->accept();
            return;
        }
        // Nothing but edges selected → delete those instead.
        QStringList edge_ids;
        for (auto* item : scene_->selectedItems()) {
            if (auto* edge = dynamic_cast<EdgeItem*>(item))
                edge_ids.append(edge->edge_id());
        }
        for (const auto& eid : edge_ids) {
            const EdgeDef ed = scene_->edge_def(eid);
            if (!ed.id.isEmpty())
                undo_stack_->push(new commands::RemoveEdgeCommand(scene_, ed));
        }
        event->accept();
    } else if (event->matches(QKeySequence::Save)) {
        on_save_workflow();
        event->accept();
    } else if (event->key() == Qt::Key_D && (event->modifiers() & Qt::ControlModifier)) {
        // Ctrl+D — duplicate the selection (same offset/rename as the context menu).
        QVector<NodeDef> copies;
        for (auto* item : scene_->selectedItems()) {
            auto* node = dynamic_cast<NodeItem*>(item);
            if (!node)
                continue;
            NodeDef copy = node->node_def();
            copy.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            copy.x += 40;
            copy.y += 40;
            copy.name += " (copy)";
            copies.append(copy);
        }
        push_add_nodes(copies, {}, tr("Duplicate %1 node(s)").arg(copies.size()));
        event->accept();
    } else if (event->matches(QKeySequence::SelectAll)) {
        // Ctrl+A — select all nodes
        for (auto* item : scene_->items())
            item->setSelected(true);
        event->accept();
    } else if (event->matches(QKeySequence::Copy)) {
        // Ctrl+C — copy selected nodes to clipboard buffer
        clipboard_nodes_.clear();
        clipboard_edges_.clear();
        QSet<QString> selected_ids;
        for (auto* item : scene_->selectedItems()) {
            if (auto* node = dynamic_cast<NodeItem*>(item)) {
                clipboard_nodes_.append(node->node_def());
                selected_ids.insert(node->node_def().id);
            }
        }
        paste_count_ = 0;
        // Copy edges that connect two selected nodes
        WorkflowDef wf = scene_->serialize();
        for (const auto& ed : wf.edges) {
            if (selected_ids.contains(ed.source_node) && selected_ids.contains(ed.target_node))
                clipboard_edges_.append(ed);
        }
        LOG_DEBUG("NodeEditor", QString("Copied %1 nodes").arg(clipboard_nodes_.size()));
        event->accept();
    } else if (event->matches(QKeySequence::Paste)) {
        // Ctrl+V — paste nodes with offset and new IDs
        if (clipboard_nodes_.isEmpty()) {
            event->accept();
            return;
        }

        QMap<QString, QString> id_remap;
        QVector<NodeDef> pasted_nodes;
        QVector<EdgeDef> pasted_edges;
        const double offset = 40.0 * ++paste_count_; // repeated pastes cascade instead of stacking
        for (const auto& nd : clipboard_nodes_) {
            NodeDef copy = nd;
            QString new_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            id_remap.insert(nd.id, new_id);
            copy.id = new_id;
            copy.x += offset;
            copy.y += offset;
            copy.name = nd.name + " (copy)";
            pasted_nodes.append(copy);
        }
        for (const auto& ed : clipboard_edges_) {
            EdgeDef copy = ed;
            copy.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            copy.source_node = id_remap.value(ed.source_node);
            copy.target_node = id_remap.value(ed.target_node);
            if (!copy.source_node.isEmpty() && !copy.target_node.isEmpty())
                pasted_edges.append(copy);
        }
        // Single undoable step for the whole paste, wiring included.
        push_add_nodes(pasted_nodes, pasted_edges, tr("Paste %1 node(s)").arg(pasted_nodes.size()));
        LOG_DEBUG("NodeEditor", QString("Pasted %1 nodes").arg(pasted_nodes.size()));
        event->accept();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void NodeEditorScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Toolbar ────────────────────────────────────────────────────
    toolbar_ = new NodeEditorToolbar(this);
    root->addWidget(toolbar_);

    // ── Separator ──────────────────────────────────────────────────
    auto* sep = new QFrame(this);
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BORDER_MED()));
    root->addWidget(sep);

    // ── 3-panel splitter ───────────────────────────────────────────
    // Every panel is created inside its final parent (the splitter, itself
    // already in this screen): adding a finished panel to a different parent
    // makes Qt recompute the stylesheet of every widget in it, and the palette
    // alone holds hundreds — that re-parenting was most of this screen's cost.
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(1);
    splitter->setStyleSheet(QString("QSplitter::handle { background: %1; }"
                                    "QSplitter::handle:hover { background: %2; }")
                                .arg(ui::colors::BORDER_MED(), ui::colors::TEXT_DIM()));

    // Left: Node palette
    palette_ = new NodePalette(splitter);
    splitter->addWidget(palette_);

    // Center: Canvas
    scene_ = new NodeScene(this);
    canvas_ = new NodeCanvas(scene_, splitter);
    splitter->addWidget(canvas_);

    // Right: Properties panel
    properties_ = new NodePropertiesPanel(splitter);
    splitter->addWidget(properties_);

    splitter->setSizes({240, 700, 280});
    splitter->setStretchFactor(0, 0); // palette fixed
    splitter->setStretchFactor(1, 1); // canvas stretches
    splitter->setStretchFactor(2, 0); // properties fixed

    root->addWidget(splitter, 1);

    // ── Execution results panel (bottom drawer) ────────────────────
    results_panel_ = new ExecutionResultsPanel(this);
    root->addWidget(results_panel_);

    // ── MiniMap (bottom-right corner of canvas) ────────────────────
    minimap_ = new MiniMap(scene_, canvas_, canvas_);
    minimap_->move(canvas_->width() - 210, canvas_->height() - 160);
    minimap_->show();

    build_canvas_empty_state();
}

void NodeEditorScreen::build_canvas_empty_state() {
    canvas_empty_ = new ui::EmptyState(canvas_->viewport());
    apply_canvas_empty_texts();
    connect(canvas_empty_, &ui::EmptyState::primary_clicked, this, &NodeEditorScreen::on_show_templates);
    connect(canvas_empty_, &ui::EmptyState::secondary_clicked, this, &NodeEditorScreen::on_load_workflow);
    connect(canvas_empty_, &ui::EmptyState::suggestion_clicked, this, [this](int row) {
        if (row >= 0 && row < static_cast<int>(std::size(kNodeEditorSuggestedTemplates)))
            load_template(kNodeEditorSuggestedTemplates[row]);
    });
    // The canvas shows it while the scene has no node and keeps it the viewport's size.
    canvas_->set_empty_state(canvas_empty_);
}

void NodeEditorScreen::apply_canvas_empty_texts() {
    if (!canvas_empty_)
        return;
    const QStringList titles = template_titles();
    ui::EmptyState::Spec spec;
    spec.glyph = QStringLiteral("◇");
    spec.glyph_tone = ui::EmptyState::Tone::Accent;
    spec.title = tr("Build a workflow");
    spec.message = tr("Drag nodes from the palette, right-click the canvas for quick add, or start from a "
                      "ready-made template.");
    spec.note = tr("Nothing runs until you press EXECUTE");
    spec.note_tone = ui::EmptyState::Tone::Info;
    spec.primary = tr("START FROM A TEMPLATE");
    spec.primary_tooltip = tr("Choose one of %n built-in templates", "", static_cast<int>(titles.size()));
    spec.secondary = tr("LOAD SAVED WORKFLOW");
    spec.secondary_tooltip = tr("Open a workflow you saved earlier");
    for (const int idx : kNodeEditorSuggestedTemplates) {
        if (idx < 0 || idx >= titles.size())
            continue;
        ui::EmptyState::Suggestion row;
        node_editor_split_template_title(titles.at(idx), &row.label, &row.secondary);
        row.tooltip = tr("Load this template onto the canvas");
        spec.suggestions.append(row);
    }
    spec.suggestions_heading = tr("OR START WITH");
    canvas_empty_->apply(spec);
}

void NodeEditorScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        apply_canvas_empty_texts();
    QWidget::changeEvent(event);
}

void NodeEditorScreen::select_entry_node() {
    NodeItem* pick = nullptr;
    bool pick_is_trigger = false;
    for (auto* item : scene_->node_items()) {
        const bool trigger = item->node_def().type.startsWith(QLatin1String("trigger."));
        if (pick) {
            if (pick_is_trigger && !trigger)
                continue;
            const QPointF a = item->pos();
            const QPointF b = pick->pos();
            const bool before = (trigger && !pick_is_trigger) || a.x() < b.x() || (a.x() == b.x() && a.y() < b.y());
            if (!before)
                continue;
        }
        pick = item;
        pick_is_trigger = trigger;
    }
    if (!pick) {
        properties_->clear();
        return;
    }
    scene_->clearSelection();
    pick->setSelected(true); // NodeItem::node_selected → on_node_selected fills the properties panel
}

void NodeEditorScreen::wire_signals() {
    // ── Canvas: drop from palette ──────────────────────────────────
    connect(canvas_, &NodeCanvas::node_drop_requested, this, &NodeEditorScreen::on_node_drop);

    // ── Scene: selection changed ───────────────────────────────────
    connect(scene_, &NodeScene::node_selection_changed, this, &NodeEditorScreen::on_node_selected);

    // ── Scene: duplicate and execute-from ──────────────────────────
    connect(scene_, &NodeScene::node_duplicate_requested, this, [this](const QString& node_id) {
        auto* item = scene_->find_node(node_id);
        if (!item)
            return;
        NodeDef copy = item->node_def();
        copy.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        copy.x += 40;
        copy.y += 40;
        copy.name += " (copy)";
        push_add_nodes({copy}, {}, tr("Duplicate %1").arg(item->node_def().name));
    });

    // ── Undoable-intent signals from the canvas ────────────────────────────
    // NodeScene announces user-initiated deletes/connects instead of applying
    // them, so every structural mutation lands on the QUndoStack.
    connect(scene_, &NodeScene::node_delete_requested, this, &NodeEditorScreen::on_delete_node);
    connect(scene_, &NodeScene::edge_delete_requested, this, [this](const QString& edge_id) {
        const EdgeDef ed = scene_->edge_def(edge_id);
        if (ed.id.isEmpty()) {
            scene_->remove_edge(edge_id); // dangling edge — drop it without history
            return;
        }
        undo_stack_->push(new commands::RemoveEdgeCommand(scene_, ed));
    });
    connect(scene_, &NodeScene::edge_create_requested, this,
            [this](const EdgeDef& ed) { undo_stack_->push(new commands::AddEdgeCommand(scene_, ed)); });
    connect(scene_, &NodeScene::node_move_started, this, &NodeEditorScreen::begin_move_snapshot);
    connect(scene_, &NodeScene::node_move_finished, this, &NodeEditorScreen::commit_move_snapshot);

    // ── Node execution flags (SETTINGS block in the properties panel) ──────
    connect(properties_, &NodePropertiesPanel::node_flag_changed, this,
            [this](const QString& node_id, const QString& flag, bool value) {
                auto* item = scene_->find_node(node_id);
                if (!item)
                    return;
                if (flag == QLatin1String("disabled"))
                    item->set_disabled(value);
                else if (flag == QLatin1String("continue_on_fail"))
                    item->set_continue_on_fail(value);
                else if (flag == QLatin1String("retry_on_fail"))
                    item->node_def_mutable().retry_on_fail = value; // no visual; reaches serialize()
                mark_dirty();
            });
    connect(properties_, &NodePropertiesPanel::node_max_tries_changed, this,
            [this](const QString& node_id, int max_tries) {
                auto* item = scene_->find_node(node_id);
                if (!item)
                    return;
                item->node_def_mutable().max_tries = max_tries;
                mark_dirty();
            });
    connect(scene_, &NodeScene::node_execute_from_requested, this, [this](const QString& node_id) {
        if (WorkflowService::instance().is_executing())
            return; // a run is already in progress — Stop it first
        start_run(node_id, false);
    });

    // ── Properties: parameter changes ──────────────────────────────
    connect(properties_, &NodePropertiesPanel::param_changed, this, &NodeEditorScreen::on_param_changed);
    connect(properties_, &NodePropertiesPanel::name_changed, this, &NodeEditorScreen::on_name_changed);
    connect(properties_, &NodePropertiesPanel::delete_requested, this, &NodeEditorScreen::on_delete_node);

    // ── Toolbar actions ────────────────────────────────────────────
    connect(toolbar_, &NodeEditorToolbar::undo_clicked, undo_stack_, &QUndoStack::undo);
    connect(toolbar_, &NodeEditorToolbar::redo_clicked, undo_stack_, &QUndoStack::redo);
    connect(toolbar_, &NodeEditorToolbar::save_clicked, this, &NodeEditorScreen::on_save_workflow);
    connect(toolbar_, &NodeEditorToolbar::load_clicked, this, &NodeEditorScreen::on_load_workflow);
    connect(toolbar_, &NodeEditorToolbar::clear_clicked, this, &NodeEditorScreen::on_clear_workflow);
    connect(toolbar_, &NodeEditorToolbar::import_clicked, this, &NodeEditorScreen::on_import_workflow);
    connect(toolbar_, &NodeEditorToolbar::export_clicked, this, &NodeEditorScreen::on_export_workflow);
    connect(toolbar_, &NodeEditorToolbar::execute_clicked, this, &NodeEditorScreen::on_execute);
    connect(toolbar_, &NodeEditorToolbar::templates_clicked, this, &NodeEditorScreen::on_show_templates);
    connect(toolbar_, &NodeEditorToolbar::deploy_clicked, this, &NodeEditorScreen::on_deploy);
    connect(toolbar_, &NodeEditorToolbar::name_changed, this, [this]() { mark_dirty(); });

    // ── Undo stack state ───────────────────────────────────────────
    connect(undo_stack_, &QUndoStack::canUndoChanged, toolbar_, &NodeEditorToolbar::set_can_undo);
    connect(undo_stack_, &QUndoStack::canRedoChanged, toolbar_, &NodeEditorToolbar::set_can_redo);

    // ── Scene: deselection → clear properties ──────────────────────
    connect(scene_, &QGraphicsScene::selectionChanged, this, [this]() {
        auto selected = scene_->selectedItems();
        if (selected.isEmpty()) {
            properties_->clear();
        }
    });

    // ── WorkflowService signals ──────────────────────────────────
    auto& svc = WorkflowService::instance();
    connect(&svc, &WorkflowService::workflow_saved, this, [this](const QString& id) {
        current_workflow_id_ = id;
        LOG_INFO("NodeEditor", QString("Workflow saved: %1").arg(id));
    });
    connect(&svc, &WorkflowService::workflow_loaded, this, [this](const WorkflowDef& wf) {
        scene_->deserialize(wf);
        toolbar_->set_workflow_name(wf.name);
        current_workflow_id_ = wf.id;
        current_description_ = wf.description;
        static_data_ = wf.static_data;
        static_data_.remove(QStringLiteral("parent_input")); // transient sub-workflow payload
        set_live_mode(WorkflowExecutor::is_live_mode(wf));
        dirty_ = false;
        properties_->clear();
        // The stack's commands reference nodes from the previous graph — undoing
        // into them after a load would resurrect foreign nodes / drop live ones.
        reset_undo_history();
        toolbar_->set_status_text(tr("SAVED"));
        if (static_cast<int>(scene_->node_items().size()) < wf.nodes.size()) {
            // deserialize() drops nodes whose type is not registered; saving this
            // canvas would silently delete them from the stored workflow.
            LOG_WARN("NodeEditor", QString("Workflow '%1': %2 node(s) of unknown type were not loaded")
                                       .arg(wf.name)
                                       .arg(wf.nodes.size() - static_cast<int>(scene_->node_items().size())));
            toolbar_->set_status_text(tr("PARTIAL LOAD"));
        }
        LOG_INFO("NodeEditor", QString("Workflow loaded: %1").arg(wf.name));
        select_entry_node(); // the properties panel opens on the workflow's entry node
    });
    connect(&svc, &WorkflowService::workflow_load_failed, this,
            [](const QString& err) { LOG_ERROR("NodeEditor", QString("Load failed: %1").arg(err)); });

    // Save-state badge: UNSAVED as soon as the graph changes, SAVED hh:mm once persisted.
    connect(undo_stack_, &QUndoStack::cleanChanged, this, [this](bool clean) {
        if (!clean)
            toolbar_->set_status_text(tr("UNSAVED"));
    });

    // ── Execution signals ────────────────────────────────────────────
    connect(&svc, &WorkflowService::execution_started, this, [this](const QString& wf_id) {
        results_panel_->set_started(wf_id);
        results_panel_->set_mode_banner(pending_banner_, pending_banner_live_);
        toolbar_->set_executing(true);
        setProperty(fincept::core::agent::kAgentRiskKeysProperty, QVariant()); // Ctrl+Enter now stops
    });
    connect(&svc, &WorkflowService::node_execution_started, this, [this](const QString& node_id) {
        auto* item = scene_->find_node(node_id);
        if (item)
            item->set_execution_state("running");
        scene_->set_edges_animated(node_id, true);
        results_panel_->add_node_running(node_id, item ? item->node_def().name : QString());
    });
    connect(&svc, &WorkflowService::node_execution_completed, this,
            [this](const QString& node_id, const NodeExecutionResult& result) {
                auto* item = scene_->find_node(node_id);
                const bool skipped = result.success && result.output.isObject() &&
                                     result.output.toObject().value("_skipped").toBool(false);
                if (item)
                    item->set_execution_state(!result.success ? "error" : skipped ? "skipped" : "completed");
                scene_->set_edges_animated(node_id, false);
                results_panel_->add_node_result(result, item ? item->node_def().name : QString(),
                                                item ? item->node_def().type : QString());
            });
    connect(&svc, &WorkflowService::execution_finished, this, [this](const WorkflowExecutionResult& result) {
        scene_->stop_all_edge_animations();
        results_panel_->set_finished(result);
        toolbar_->set_executing(false);
        setProperty(fincept::core::agent::kAgentRiskKeysProperty, QStringLiteral("Ctrl+Return=order;Ctrl+Enter=order"));
        QString msg = result.success ? QString("Execution completed in %1ms").arg(result.total_duration_ms)
                                     : QString("Execution failed: %1").arg(result.error);
        LOG_INFO("NodeEditor", msg);
    });
}

// ── Undo plumbing ──────────────────────────────────────────────────────

void NodeEditorScreen::push_add_nodes(const QVector<NodeDef>& nodes, const QVector<EdgeDef>& edges,
                                      const QString& label) {
    if (nodes.isEmpty())
        return;
    undo_stack_->push(new commands::AddNodesCommand(scene_, nodes, edges, label));
}

void NodeEditorScreen::push_remove_nodes(const QStringList& ids, const QString& label) {
    if (ids.isEmpty())
        return;
    QVector<NodeDef> defs;
    QSet<QString> id_set;
    defs.reserve(ids.size());
    for (const auto& id : ids) {
        if (auto* item = scene_->find_node(id)) {
            defs.append(item->node_def()); // carries the live x/y (itemChange keeps it current)
            id_set.insert(id);
        }
    }
    if (defs.isEmpty())
        return;
    const QVector<EdgeDef> edges = commands::edges_touching(scene_, id_set);
    undo_stack_->push(new commands::RemoveNodesCommand(scene_, defs, edges, label));
    properties_->clear();
}

void NodeEditorScreen::begin_move_snapshot() {
    // A QGraphicsScene delivers the press to exactly one item, so a press always
    // starts a fresh gesture — no nesting to reconcile. Snapshotting every node
    // (not just the pressed one) is what makes a multi-select drag undoable as a
    // single step.
    move_snapshot_.clear();
    for (auto* item : scene_->node_items())
        move_snapshot_.insert(item->node_def().id, item->pos());
}

void NodeEditorScreen::commit_move_snapshot() {
    if (move_snapshot_.isEmpty())
        return;

    QHash<QString, QPointF> before;
    QHash<QString, QPointF> after;
    for (auto* item : scene_->node_items()) {
        const QString id = item->node_def().id;
        auto it = move_snapshot_.constFind(id);
        if (it == move_snapshot_.constEnd())
            continue;
        if (it.value() != item->pos()) {
            before.insert(id, it.value());
            after.insert(id, item->pos());
        }
    }
    move_snapshot_.clear();
    if (after.isEmpty())
        return; // a click, not a drag
    undo_stack_->push(new commands::MoveNodesCommand(scene_, before, after));
}

void NodeEditorScreen::reset_undo_history() {
    if (undo_stack_)
        undo_stack_->clear();
    move_snapshot_.clear();
}

// ── Action handlers ────────────────────────────────────────────────────

void NodeEditorScreen::on_node_drop(const QString& type_id, const QPointF& scene_pos) {
    auto& registry = NodeRegistry::instance();
    const auto* type_def = registry.find(type_id);
    if (!type_def) {
        LOG_WARN("NodeEditor", QString("Unknown node type for drop: %1").arg(type_id));
        return;
    }

    NodeDef def;
    def.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    def.type = type_id;
    def.name = type_def->display_name;
    def.type_version = type_def->version;
    def.x = scene_pos.x();
    def.y = scene_pos.y();

    // Set default parameter values
    for (const auto& param : type_def->parameters) {
        if (!param.default_value.isUndefined() && !param.default_value.isNull())
            def.parameters[param.key] = param.default_value;
    }

    push_add_nodes({def}, {}, tr("Add %1").arg(type_def->display_name));
}

void NodeEditorScreen::on_node_selected(const QString& node_id) {
    auto* node_item = scene_->find_node(node_id);
    if (!node_item) {
        properties_->clear();
        return;
    }

    auto& registry = NodeRegistry::instance();
    const auto* type_def = registry.find(node_item->node_def().type);

    properties_->show_properties(&node_item->node_def(), type_def);
}

// Parameter / name edits go on the undo stack (consecutive keystrokes into one field merge
// into one step). They used to be applied directly, so Ctrl+Z after an edit reverted the
// previous structural change and left the edit in place.
void NodeEditorScreen::on_param_changed(const QString& node_id, const QString& key, QJsonValue value) {
    auto* node_item = scene_->find_node(node_id);
    if (!node_item)
        return;
    const QJsonValue before = node_item->node_def().parameters.value(key);
    if (before == value)
        return;
    undo_stack_->push(new commands::SetNodeFieldCommand(scene_, node_id, key, before, value,
                                                        [this](const QString& id) { reshow_properties(id); }));
    mark_dirty();
}

void NodeEditorScreen::on_name_changed(const QString& node_id, const QString& new_name) {
    auto* node_item = scene_->find_node(node_id);
    if (!node_item)
        return;
    const QString before = node_item->node_def().name;
    if (before == new_name)
        return;
    undo_stack_->push(new commands::SetNodeFieldCommand(scene_, node_id, QString(), QJsonValue(before),
                                                        QJsonValue(new_name),
                                                        [this](const QString& id) { reshow_properties(id); }));
    mark_dirty();
}

void NodeEditorScreen::reshow_properties(const QString& node_id) {
    // An undo / redo of a field edit: show the restored value if that node is on display.
    if (properties_ && properties_->current_node_id() == node_id)
        on_node_selected(node_id);
    mark_dirty();
}

void NodeEditorScreen::on_delete_node(const QString& node_id) {
    auto* item = scene_->find_node(node_id);
    push_remove_nodes({node_id}, item ? tr("Delete %1").arg(item->node_def().name) : tr("Delete node"));
}

void NodeEditorScreen::on_clear_workflow() {
    if (scene_->is_empty())
        return;
    auto result = QMessageBox::question(
        this, tr("Clear Workflow"),
        tr("Clear all %1 node(s) and their connections?\n\nThis can be undone with Ctrl+Z.")
            .arg(scene_->node_items().size()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (result == QMessageBox::Yes) {
        undo_stack_->push(
            new commands::ReplaceGraphCommand(scene_, scene_->serialize(), WorkflowDef{}, tr("Clear workflow")));
        properties_->clear();
        LOG_DEBUG("NodeEditor", "Workflow cleared");
    }
}

void NodeEditorScreen::on_import_workflow() {
    // Importing replaces the whole canvas — guard the work already on it.
    if (!scene_->is_empty()) {
        const auto ans = QMessageBox::question(
            this, tr("Import Workflow"),
            tr("Importing replaces the %1 node(s) currently on the canvas.\n\nContinue?")
                .arg(scene_->node_items().size()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ans != QMessageBox::Yes)
            return;
    }

    QString path =
        QFileDialog::getOpenFileName(this, tr("Import Workflow"), {}, tr("JSON Files (*.json);;All Files (*)"));
    if (path.isEmpty())
        return;

    // The parsing lives in WorkflowService (it also repairs missing/duplicate ids).
    auto imported = WorkflowService::instance().import_from_json(path);
    if (imported.is_err()) {
        const QString err = QString::fromStdString(imported.error());
        LOG_ERROR("NodeEditor", QString("Import failed: %1").arg(err));
        QMessageBox::warning(this, tr("Import Workflow"), tr("Could not import the workflow:\n\n%1").arg(err));
        return;
    }

    WorkflowDef wf = imported.value();
    if (wf.name.isEmpty())
        wf.name = tr("Imported Workflow");

    scene_->deserialize(wf);
    toolbar_->set_workflow_name(wf.name);
    properties_->clear();
    reset_undo_history(); // the previous graph's commands no longer apply
    // The import is a NEW, unsaved workflow. Keeping the previous workflow's id would
    // make the next Save — or the 30 s auto-save — overwrite that workflow with the
    // imported graph.
    current_workflow_id_.clear();
    current_description_ = wf.description;
    // An imported file never arrives armed for LIVE trading (import drops static_data).
    static_data_ = QJsonObject{};
    set_live_mode(false);
    dirty_ = true;
    toolbar_->set_status_text(tr("DRAFT"));

    const int loaded_nodes = static_cast<int>(scene_->node_items().size());
    if (loaded_nodes < wf.nodes.size()) {
        QMessageBox::warning(this, tr("Import Workflow"),
                             tr("%1 of %2 node(s) use a node type this version does not know and were skipped "
                                "(together with their connections).")
                                 .arg(wf.nodes.size() - loaded_nodes)
                                 .arg(wf.nodes.size()));
    }
    LOG_INFO("NodeEditor", QString("Imported workflow: %1").arg(wf.name));
    select_entry_node();
}

void NodeEditorScreen::on_export_workflow() {
    if (scene_->is_empty()) {
        QMessageBox::information(this, tr("Export Workflow"), tr("There is nothing on the canvas to export."));
        return;
    }
    QString suggested = toolbar_->workflow_name().trimmed();
    suggested.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_\\- ]")), QStringLiteral("_"));
    QString path = QFileDialog::getSaveFileName(this, tr("Export Workflow"),
                                                (suggested.isEmpty() ? QStringLiteral("workflow") : suggested) + ".json",
                                                tr("JSON Files (*.json);;All Files (*)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(QLatin1String(".json"), Qt::CaseInsensitive))
        path += QLatin1String(".json");

    // Same serializer the import uses, so every node setting (continue-on-fail,
    // retry, version…) survives an export → import round trip. The execution mode is
    // deliberately not exported — an imported workflow always starts in PAPER.
    auto res = WorkflowService::instance().export_to_json(current_workflow(), path);
    if (res.is_err()) {
        const QString err = QString::fromStdString(res.error());
        LOG_ERROR("NodeEditor", QString("Export failed: %1").arg(err));
        QMessageBox::warning(this, tr("Export Workflow"), tr("Could not export the workflow:\n\n%1").arg(err));
        return;
    }
    toolbar_->set_status_text(tr("EXPORTED %1").arg(QTime::currentTime().toString("HH:mm")));
    LOG_INFO("NodeEditor", QString("Exported workflow to: %1").arg(path));
}

WorkflowDef NodeEditorScreen::current_workflow() const {
    WorkflowDef wf = scene_->serialize();
    wf.id = current_workflow_id_;
    wf.name = toolbar_->workflow_name();
    wf.description = current_description_;
    wf.static_data = static_data_;
    return wf;
}

void NodeEditorScreen::set_live_mode(bool live) {
    live_mode_ = live;
    static_data_[QLatin1String(WorkflowExecutor::kExecutionModeKey)] = live ? QStringLiteral("live")
                                                                            : QStringLiteral("paper");
    toolbar_->set_live_mode(live);
}

void NodeEditorScreen::mark_dirty() {
    dirty_ = true;
    toolbar_->set_status_text(tr("UNSAVED"));
}

bool NodeEditorScreen::persist(bool interactive) {
    if (current_workflow_id_.isEmpty())
        current_workflow_id_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const WorkflowDef wf = current_workflow();

    // save_workflow() is synchronous and reports a failure only through save_failed.
    auto& svc = WorkflowService::instance();
    QString save_error;
    const auto failed_conn =
        connect(&svc, &WorkflowService::save_failed, this, [&save_error](const QString& e) { save_error = e; });
    svc.save_workflow(wf);
    disconnect(failed_conn);

    if (!save_error.isEmpty()) {
        if (interactive)
            QMessageBox::warning(this, tr("Save Workflow"),
                                 tr("The workflow could not be saved:\n\n%1").arg(save_error));
        else
            toolbar_->set_status_text(tr("SAVE FAILED")); // no modal every 30 s
        return false;
    }
    dirty_ = false;
    undo_stack_->setClean();
    toolbar_->set_status_text(tr("SAVED %1").arg(QTime::currentTime().toString("HH:mm")));
    ScreenStateManager::instance().notify_changed(this);
    return true;
}

void NodeEditorScreen::on_save_workflow() {
    persist(true);
}

void NodeEditorScreen::on_load_workflow() {
    // Request workflow list, then show picker when signal arrives
    auto& svc = WorkflowService::instance();

    // One-shot connection for the list response
    auto* conn = new QMetaObject::Connection;
    *conn =
        connect(&svc, &WorkflowService::workflows_listed, this, [this, conn](const QVector<WorkflowDef>& workflows) {
            disconnect(*conn);
            delete conn;

            if (workflows.isEmpty()) {
                QMessageBox::information(this, tr("Load Workflow"), tr("No saved workflows found."));
                return;
            }

            // Build a simple picker dialog
            QStringList items;
            for (const auto& wf : workflows)
                items << QString("%1  (%2)").arg(wf.name, wf.updated_at);

            bool ok = false;
            QString chosen =
                QInputDialog::getItem(this, tr("Load Workflow"), tr("Select a workflow:"), items, 0, false, &ok);
            if (!ok)
                return;

            int idx = items.indexOf(chosen);
            if (idx >= 0 && idx < workflows.size()) {
                // load_workflow() is synchronous; a failure is only signalled, so catch it here
                // and tell the user instead of leaving the canvas unchanged without a word.
                auto& load_svc = WorkflowService::instance();
                QString load_error;
                const auto failed_conn = connect(&load_svc, &WorkflowService::workflow_load_failed, this,
                                                 [&load_error](const QString& e) { load_error = e; });
                load_svc.load_workflow(workflows[idx].id);
                disconnect(failed_conn);
                if (!load_error.isEmpty())
                    QMessageBox::warning(this, tr("Load Workflow"),
                                         tr("The workflow could not be loaded:\n\n%1").arg(load_error));
            }
        });

    svc.list_workflows();
}

void NodeEditorScreen::on_auto_save() {
    if (scene_->is_empty())
        return; // nothing to save
    // Only when something changed: re-saving an unchanged graph every 30 s also queued
    // a cloud-sync upload each time.
    if (!dirty_ && undo_stack_->isClean())
        return;
    if (persist(false))
        LOG_DEBUG("NodeEditor", "Auto-saved workflow");
}

void NodeEditorScreen::on_stop() {
    if (!WorkflowService::instance().is_executing())
        return;
    WorkflowService::instance().stop_execution();
    LOG_INFO("NodeEditor", "Execution stopped by user");
}

void NodeEditorScreen::on_execute() {
    if (WorkflowService::instance().is_executing()) {
        on_stop();
        return;
    }
    start_run({}, false);
}

void NodeEditorScreen::start_run(const QString& from_node_id, bool already_confirmed) {
    if (scene_->is_empty()) {
        LOG_WARN("NodeEditor", "No nodes to execute");
        return;
    }
    // Audit-log rows (workflow / node / order events) are keyed by workflow id.
    if (current_workflow_id_.isEmpty())
        current_workflow_id_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const WorkflowDef wf = current_workflow();

    // The part of the graph that will actually run: everything, or the start node
    // and what is downstream of it ("Execute From Here").
    WorkflowDef will_run = wf;
    if (!from_node_id.isEmpty()) {
        QSet<QString> downstream{from_node_id};
        QStringList frontier{from_node_id};
        while (!frontier.isEmpty()) {
            const QString cur = frontier.takeFirst();
            for (const auto& ed : wf.edges) {
                if (ed.source_node == cur && !downstream.contains(ed.target_node)) {
                    downstream.insert(ed.target_node);
                    frontier.append(ed.target_node);
                }
            }
        }
        will_run.nodes.clear();
        for (const auto& nd : wf.nodes) {
            if (downstream.contains(nd.id))
                will_run.nodes.append(nd);
        }
    }

    if (!validate_before_run(will_run))
        return;
    if (!confirm_run(will_run, from_node_id.isEmpty() ? tr("Execute Workflow") : tr("Execute From Here"),
                     already_confirmed))
        return;

    // Run-log banner: say which mode this run is in and what that means.
    if (live_mode_) {
        const auto rows = WorkflowService::live_action_rows(will_run, true);
        int money = 0;
        for (const auto& r : rows)
            money += r.moves_money ? 1 : 0;
        pending_banner_ = money > 0 ? tr("MODE: LIVE — %n node(s) can send real orders", "", money)
                                    : tr("MODE: LIVE — no order node is set to live; nothing reaches a broker");
        pending_banner_live_ = money > 0;
    } else {
        const QStringList simulated = WorkflowService::simulated_live_rows(will_run);
        pending_banner_ = simulated.isEmpty()
                              ? tr("MODE: PAPER — every order is simulated")
                              : tr("MODE: PAPER — %n order node(s) set to live will be simulated", "",
                                   static_cast<int>(simulated.size()));
        pending_banner_live_ = false;
    }

    for (auto* item : scene_->node_items())
        item->set_execution_state("idle");

    if (from_node_id.isEmpty())
        WorkflowService::instance().execute_workflow(wf);
    else
        WorkflowService::instance().execute_from_node(wf, from_node_id);
}

bool NodeEditorScreen::validate_before_run(const WorkflowDef& wf) {
    for (auto* item : scene_->node_items())
        item->set_execution_state("idle");

    const auto issues = WorkflowService::validate(wf);
    if (issues.isEmpty())
        return true;

    // Group messages per node and mark each offending node on the canvas.
    QHash<QString, QStringList> per_node;
    QStringList order;
    for (const auto& is : issues) {
        if (!per_node.contains(is.node_id))
            order << is.node_id;
        per_node[is.node_id] << is.message;
    }
    QStringList lines;
    NodeItem* first = nullptr;
    for (const auto& id : order) {
        auto* item = scene_->find_node(id);
        if (item) {
            item->set_validation_error(per_node.value(id).join(QLatin1Char('\n')));
            if (!first)
                first = item;
        }
        const QString name = item ? item->node_def().name : (id.isEmpty() ? tr("Workflow") : id.left(8));
        for (const auto& m : per_node.value(id))
            lines << QStringLiteral("• %1: %2").arg(name, m);
    }
    if (first) {
        scene_->clearSelection();
        first->setSelected(true);
        canvas_->centerOn(first);
    }
    const int shown = std::min<int>(static_cast<int>(lines.size()), 12);
    QString text = lines.mid(0, shown).join(QLatin1Char('\n'));
    if (lines.size() > shown)
        text += QLatin1Char('\n') + tr("…and %n more", "", static_cast<int>(lines.size()) - shown);
    QMessageBox::warning(this, tr("Workflow not ready"),
                         tr("Fix these before running (offending nodes are outlined in red):\n\n%1").arg(text));
    LOG_WARN("NodeEditor", QString("Run blocked by %1 validation issue(s)").arg(issues.size()));
    return false;
}

bool NodeEditorScreen::confirm_run(const WorkflowDef& wf, const QString& action, bool already_confirmed) {
    const auto rows = WorkflowService::live_action_rows(wf, live_mode_);
    QStringList money_rows;
    QStringList other_rows;
    for (const auto& r : rows)
        (r.moves_money ? money_rows : other_rows) << r.text;

    // PAPER: the order nodes (WorkflowExecutor::is_moded_order_type) trade the paper
    // account without a prompt — say so once per run, unless the user turned it off.
    static const QString kPaperSkipKey = QStringLiteral("node_editor.paper_run_confirm_skip");
    int paper_placing = 0;
    QStringList paper_rows;
    bool calls_subworkflow = false;
    if (!live_mode_ && !already_confirmed) {
        for (const auto& nd : wf.nodes) {
            if (nd.disabled)
                continue;
            if (nd.type == QLatin1String("control.execute_workflow"))
                calls_subworkflow = true;
            if (!WorkflowExecutor::is_moded_order_type(nd.type))
                continue;
            if (nd.type != QLatin1String("trading.cancel_order") && nd.type != QLatin1String("trading.modify_order"))
                ++paper_placing;
            paper_rows << QStringLiteral("  • %1  [%2]").arg(nd.name.isEmpty() ? nd.type : nd.name, nd.type);
        }
        // A failed read counts as "ask" (never skip a confirmation on an unknown value).
        if (!paper_rows.isEmpty()) {
            const auto skip = SettingsRepository::instance().get(kPaperSkipKey);
            if (skip.is_ok() && skip.value() == QLatin1String("1"))
                paper_rows.clear();
        }
    }
    if (rows.isEmpty() && paper_rows.isEmpty())
        return true; // nothing acts for real — proceed silently

    auto cap = [](const QStringList& l) {
        QStringList out = l.mid(0, 15);
        if (l.size() > 15)
            out << tr("  …and %n more", "", static_cast<int>(l.size()) - 15);
        return out.join(QLatin1Char('\n'));
    };

    bool go = true;
    if (!money_rows.isEmpty()) {
        if (!already_confirmed) {
            // Real orders: a click is not enough — the user types LIVE.
            bool ok = false;
            const QString typed = QInputDialog::getText(
                this, action,
                tr("LIVE MODE — this run sends REAL orders with REAL money:\n\n%1\n\nType LIVE to run, or Cancel.")
                    .arg(cap(money_rows + other_rows)),
                QLineEdit::Normal, QString(), &ok);
            go = ok && typed.trimmed() == QLatin1String("LIVE");
            if (ok && !go)
                QMessageBox::information(this, action, tr("Not confirmed — nothing was run."));
        }
    } else if (!paper_rows.isEmpty()) {
        QMessageBox box(this);
        box.setIcon(QMessageBox::Question);
        box.setWindowTitle(action);
        box.setText(paper_placing > 0 ? tr("This run can place %n paper order(s).", "", paper_placing)
                                      : tr("This run can change or cancel paper orders."));
        QString info = tr("PAPER mode: these order nodes trade the paper account only — nothing reaches a "
                          "broker.\n\n%1")
                           .arg(cap(paper_rows));
        if (calls_subworkflow)
            info += QStringLiteral("\n\n") + tr("Sub-workflows it calls can place more paper orders.");
        if (!other_rows.isEmpty())
            info += QStringLiteral("\n\n") + tr("These nodes also call destructive tools:\n\n%1").arg(cap(other_rows));
        box.setInformativeText(info);
        auto* dont_ask = new QCheckBox(tr("Don't ask again for paper runs"), &box);
        box.setCheckBox(dont_ask);
        auto* proceed = box.addButton(tr("Run"), QMessageBox::AcceptRole);
        proceed->setProperty("fincept_agent_risk", "order"); // same kind as EXECUTE
        box.addButton(QMessageBox::Cancel);
        box.setDefaultButton(QMessageBox::Cancel); // Enter must not fire it
        box.exec();
        go = (box.clickedButton() == proceed);
        if (go && dont_ask->isChecked()) {
            const auto saved = SettingsRepository::instance().set(kPaperSkipKey, QStringLiteral("1"),
                                                                  QStringLiteral("node_editor"));
            if (saved.is_err())
                LOG_WARN("NodeEditor", "Could not save the paper-run confirmation choice");
        }
    } else {
        QMessageBox box(this);
        box.setIcon(QMessageBox::Warning);
        box.setWindowTitle(action);
        box.setText(tr("This workflow changes data outside the editor."));
        box.setInformativeText(tr("These nodes call destructive tools:\n\n%1").arg(cap(other_rows)));
        auto* proceed = box.addButton(tr("Run"), QMessageBox::AcceptRole);
        box.addButton(QMessageBox::Cancel);
        box.setDefaultButton(QMessageBox::Cancel); // Enter must not fire it
        box.exec();
        go = (box.clickedButton() == proceed);
    }
    LOG_WARN("NodeEditor", QString("%1 [%2]: %3 live order node(s), %4 destructive tool(s) — user %5")
                               .arg(action, live_mode_ ? "LIVE" : "PAPER")
                               .arg(money_rows.size())
                               .arg(other_rows.size())
                               .arg(go ? "CONFIRMED" : "cancelled"));
    return go;
}

QStringList NodeEditorScreen::template_titles() const {
    return {
        // Beginner
        tr("1. Hello World — Trigger → Set Variable → Display"),
        tr("2. Stock Quote Lookup — Fetch quote for multiple tickers"),
        // Market Analysis
        tr("3. Multi-Indicator Scanner — RSI + MACD + Bollinger on a stock"),
        tr("4. Sector Correlation — Compare correlations across sector ETFs"),
        tr("5. Economic Dashboard — Fetch GDP, CPI, unemployment data"),
        // Trading
        tr("6. Price Alert Trading — Auto-trade when price crosses threshold"),
        tr("7. Mean Reversion Strategy — Buy oversold / sell overbought with risk checks"),
        tr("8. Portfolio Rebalancer — Holdings → optimize → target weights"),
        // Risk & Safety
        tr("9. Daily Risk Monitor — 9:30 weekdays: VaR → threshold → alert"),
        tr("10. Pre-Trade Compliance — Full validation before order execution"),
        // Data & AI
        tr("11. News Sentiment Pipeline — Fetch news → NLP → filter → alert"),
        tr("12. AI Research Agent — Multi-agent analysis with mediator"),
        // Operations
        tr("13. Scheduled Report — 17:00 weekdays: positions, P&L & balance to email"),
        tr("14. Data Export Pipeline — Fetch history → filter → sort → CSV export"),
        tr("15. Signal Automation — Signal → validate → risk check → order"),
    };
}

void NodeEditorScreen::on_show_templates() {
    const QStringList templates = template_titles();
    bool ok = false;
    const QString chosen =
        QInputDialog::getItem(this, tr("Workflow Templates"), tr("Select a template:"), templates, 0, false, &ok);
    if (!ok)
        return;
    load_template(static_cast<int>(templates.indexOf(chosen)));
}

void NodeEditorScreen::load_template(int idx) {
    const QStringList templates = template_titles();
    if (idx < 0 || idx >= templates.size())
        return;
    const QString chosen = templates.at(idx);

    // Loading a template wipes the canvas. Confirm, and capture the current graph
    // so the whole swap is a single Ctrl+Z.
    const WorkflowDef before = scene_->serialize();
    if (!before.nodes.isEmpty()) {
        const auto ans = QMessageBox::question(
            this, tr("Workflow Templates"),
            tr("Loading a template replaces the %1 node(s) on the canvas.\n\nThis can be undone with Ctrl+Z. Continue?")
                .arg(before.nodes.size()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ans != QMessageBox::Yes)
            return;
    }

    scene_->clear_all();
    properties_->clear();
    auto& reg = NodeRegistry::instance();

    auto make_node = [&](const QString& type, const QString& name, double x, double y) -> NodeDef {
        NodeDef nd;
        nd.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        nd.type = type;
        nd.name = name;
        nd.x = x;
        nd.y = y;
        const auto* td = reg.find(type);
        if (td) {
            nd.type_version = td->version;
            for (const auto& p : td->parameters) {
                if (!p.default_value.isUndefined() && !p.default_value.isNull())
                    nd.parameters[p.key] = p.default_value;
            }
            scene_->add_node(nd, *td);
        }
        return nd;
    };

    auto make_edge = [&](const QString& src_id, const QString& src_port, const QString& tgt_id,
                         const QString& tgt_port) {
        EdgeDef ed;
        ed.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        ed.source_node = src_id;
        ed.target_node = tgt_id;
        ed.source_port = src_port;
        ed.target_port = tgt_port;
        scene_->add_edge(ed);
    };

    // Helper to set params on a node after creation
    auto set_param = [&](NodeDef& nd, const QString& key, const QJsonValue& val) {
        nd.parameters[key] = val;
        auto* item = scene_->find_node(nd.id);
        if (item)
            item->set_parameter(key, val);
    };

    if (idx == 0) {
        // ── 1. Hello World ─────────────────────────────────────────
        auto n1 = make_node("trigger.manual", "Start", 100, 200);
        auto n2 = make_node("core.set", "Set Greeting", 400, 200);
        set_param(n2, "key", "message");
        set_param(n2, "value", "Hello, World!");
        auto n3 = make_node("output.results_display", "Show Result", 700, 200);
        make_edge(n1.id, "output_main", n2.id, "input_0");
        make_edge(n2.id, "output_main", n3.id, "input_0");
        toolbar_->set_workflow_name("Hello World");
    } else if (idx == 1) {
        // ── 2. Stock Quote Lookup ──────────────────────────────────
        auto n1 = make_node("trigger.manual", "Start", 100, 200);
        auto n2 = make_node("market.get_quote", "AAPL Quote", 400, 100);
        set_param(n2, "symbol", "AAPL");
        auto n3 = make_node("market.get_quote", "MSFT Quote", 400, 250);
        set_param(n3, "symbol", "MSFT");
        auto n4 = make_node("market.get_quote", "GOOGL Quote", 400, 400);
        set_param(n4, "symbol", "GOOGL");
        auto n5 = make_node("control.merge", "Merge Quotes", 700, 250);
        auto n6 = make_node("output.results_display", "All Quotes", 1000, 250);
        make_edge(n1.id, "output_main", n2.id, "input_0");
        make_edge(n1.id, "output_main", n3.id, "input_0");
        make_edge(n1.id, "output_main", n4.id, "input_0");
        make_edge(n2.id, "output_main", n5.id, "input_0");
        make_edge(n3.id, "output_main", n5.id, "input_1");
        make_edge(n4.id, "output_main", n5.id, "input_2");
        make_edge(n5.id, "output_main", n6.id, "input_0");
        toolbar_->set_workflow_name("Stock Quote Lookup");
    } else if (idx == 2) {
        // ── 3. Multi-Indicator Scanner ─────────────────────────────
        auto n1 = make_node("trigger.manual", "Start", 100, 250);
        auto n2 = make_node("market.get_historical", "Get AAPL History", 400, 250);
        set_param(n2, "symbol", "AAPL");
        set_param(n2, "period", "6mo");
        auto n3 = make_node("analytics.technical_indicators", "RSI (14)", 750, 100);
        set_param(n3, "indicator", "RSI");
        set_param(n3, "period", 14);
        auto n4 = make_node("analytics.technical_indicators", "MACD", 750, 250);
        set_param(n4, "indicator", "MACD");
        auto n5 = make_node("analytics.technical_indicators", "Bollinger", 750, 400);
        set_param(n5, "indicator", "BBANDS");
        set_param(n5, "period", 20);
        auto n6 = make_node("control.merge", "Combine Signals", 1050, 250);
        auto n7 = make_node("output.results_display", "Indicator Report", 1350, 250);
        make_edge(n1.id, "output_main", n2.id, "input_0");
        make_edge(n2.id, "output_main", n3.id, "input_0");
        make_edge(n2.id, "output_main", n4.id, "input_0");
        make_edge(n2.id, "output_main", n5.id, "input_0");
        make_edge(n3.id, "output_main", n6.id, "input_0");
        make_edge(n4.id, "output_main", n6.id, "input_1");
        make_edge(n5.id, "output_main", n6.id, "input_2");
        make_edge(n6.id, "output_main", n7.id, "input_0");
        toolbar_->set_workflow_name("Multi-Indicator Scanner");
    } else if (idx == 3) {
        // ── 4. Sector Correlation ──────────────────────────────────
        auto n1 = make_node("trigger.manual", "Start", 100, 200);
        auto n2 = make_node("market.get_historical", "SPY", 400, 100);
        set_param(n2, "symbol", "SPY");
        set_param(n2, "period", "1y");
        auto n3 = make_node("market.get_historical", "XLK (Tech)", 400, 250);
        set_param(n3, "symbol", "XLK");
        set_param(n3, "period", "1y");
        auto n4 = make_node("market.get_historical", "XLF (Finance)", 400, 400);
        set_param(n4, "symbol", "XLF");
        set_param(n4, "period", "1y");
        auto n5 = make_node("control.merge", "Merge", 700, 250);
        auto n6 = make_node("analytics.correlation_matrix", "Correlation", 1000, 250);
        set_param(n6, "method", "pearson");
        auto n7 = make_node("output.results_display", "Correlation Matrix", 1300, 250);
        make_edge(n1.id, "output_main", n2.id, "input_0");
        make_edge(n1.id, "output_main", n3.id, "input_0");
        make_edge(n1.id, "output_main", n4.id, "input_0");
        make_edge(n2.id, "output_main", n5.id, "input_0");
        make_edge(n3.id, "output_main", n5.id, "input_1");
        make_edge(n4.id, "output_main", n5.id, "input_2");
        make_edge(n5.id, "output_main", n6.id, "input_0");
        make_edge(n6.id, "output_main", n7.id, "input_0");
        toolbar_->set_workflow_name("Sector Correlation Analysis");
    } else if (idx == 4) {
        // ── 5. Economic Dashboard ──────────────────────────────────
        auto n1 = make_node("trigger.manual", "Start", 100, 250);
        auto n2 = make_node("market.get_economics", "GDP", 400, 100);
        set_param(n2, "series_id", "GDPC1");
        auto n3 = make_node("market.get_economics", "CPI", 400, 250);
        set_param(n3, "series_id", "CPIAUCSL");
        auto n4 = make_node("market.get_economics", "Unemployment", 400, 400);
        set_param(n4, "series_id", "UNRATE");
        auto n5 = make_node("control.merge", "Merge Indicators", 700, 250);
        auto n6 = make_node("output.results_display", "Econ Dashboard", 1000, 250);
        make_edge(n1.id, "output_main", n2.id, "input_0");
        make_edge(n1.id, "output_main", n3.id, "input_0");
        make_edge(n1.id, "output_main", n4.id, "input_0");
        make_edge(n2.id, "output_main", n5.id, "input_0");
        make_edge(n3.id, "output_main", n5.id, "input_1");
        make_edge(n4.id, "output_main", n5.id, "input_2");
        make_edge(n5.id, "output_main", n6.id, "input_0");
        toolbar_->set_workflow_name("Economic Dashboard");
    } else if (idx == 5) {
        // ── 6. Price Alert Trading ─────────────────────────────────
        auto n1 = make_node("trigger.price_alert", "AAPL > $200", 100, 200);
        set_param(n1, "symbol", "AAPL");
        set_param(n1, "condition", "above");
        set_param(n1, "price", 200.0);
        auto n2 = make_node("market.get_quote", "Latest Quote", 400, 200);
        set_param(n2, "symbol", "AAPL");
        // Risk Check sizes the trade from quantity x price against portfolio_value and
        // fails closed without them — a bare quote has neither, so add them here.
        auto n2q = make_node("core.set", "Order Qty: 10", 650, 200);
        set_param(n2q, "key", "quantity");
        set_param(n2q, "value", "10");
        auto n2p = make_node("core.set", "Portfolio Value (edit)", 900, 200);
        set_param(n2p, "key", "portfolio_value");
        set_param(n2p, "value", "100000");
        auto n3 = make_node("safety.risk_check", "Risk Check", 1150, 200);
        auto n4 = make_node("trading.place_order", "Buy AAPL", 1450, 100);
        set_param(n4, "symbol", "AAPL");
        set_param(n4, "exchange", "NASDAQ");
        set_param(n4, "side", "buy");
        set_param(n4, "quantity", 10);
        auto n5 = make_node("notify.telegram", "Alert: Blocked", 1450, 350);
        auto n6 = make_node("output.results_display", "Order Result", 1750, 100);
        auto n7 = make_node("output.results_display", "Risk Alert", 1750, 350);
        make_edge(n1.id, "output_main", n2.id, "input_0");
        make_edge(n2.id, "output_main", n2q.id, "input_0");
        make_edge(n2q.id, "output_main", n2p.id, "input_0");
        make_edge(n2p.id, "output_main", n3.id, "input_0");
        make_edge(n3.id, "output_pass", n4.id, "input_0");
        make_edge(n3.id, "output_fail", n5.id, "input_0");
        make_edge(n4.id, "output_main", n6.id, "input_0");
        make_edge(n5.id, "output_main", n7.id, "input_0");
        toolbar_->set_workflow_name("Price Alert Auto-Trade");
    } else if (idx == 6) {
        // ── 7. Mean Reversion Strategy ─────────────────────────────
        auto n1 = make_node("trigger.manual", "Start", 100, 250);
        auto n2 = make_node("market.get_historical", "Get TSLA 3mo", 400, 250);
        set_param(n2, "symbol", "TSLA");
        set_param(n2, "period", "3mo");
        auto n3 = make_node("analytics.technical_indicators", "RSI (14)", 700, 250);
        set_param(n3, "indicator", "RSI");
        set_param(n3, "period", 14);
        // The indicator node reports the latest RSI as "value" (an empty condition was plain
        // truthiness — always TRUE).
        auto n4 = make_node("control.if_else", "RSI < 30?", 1000, 250);
        set_param(n4, "condition", "value < 30");
        // Size Check needs quantity and price in the payload (it fails closed without them):
        // take the last close from the indicator's "latest" bar and add the order size.
        auto n4p = make_node("core.set", "Price: Last Close", 1300, 100);
        set_param(n4p, "key", "price");
        set_param(n4p, "value", "{{$input.latest.close}}");
        auto n4q = make_node("core.set", "Order Qty: 5", 1550, 100);
        set_param(n4q, "key", "quantity");
        set_param(n4q, "value", "5");
        auto n5 = make_node("safety.position_size_limit", "Size Check", 1800, 100);
        auto n6 = make_node("trading.place_order", "Buy TSLA", 2100, 50);
        set_param(n6, "symbol", "TSLA");
        set_param(n6, "exchange", "NASDAQ");
        set_param(n6, "side", "buy");
        set_param(n6, "quantity", 5);
        auto n7 = make_node("core.set", "Skip: Not Oversold", 1300, 400);
        set_param(n7, "key", "action");
        set_param(n7, "value", "no_trade");
        auto n8 = make_node("output.results_display", "Trade Result", 2400, 50);
        auto n9 = make_node("output.results_display", "Skip Result", 1600, 400);
        auto n10 = make_node("output.results_display", "Size Rejected", 2100, 250);
        make_edge(n1.id, "output_main", n2.id, "input_0");
        make_edge(n2.id, "output_main", n3.id, "input_0");
        make_edge(n3.id, "output_main", n4.id, "input_0");
        make_edge(n4.id, "output_true", n4p.id, "input_0");
        make_edge(n4p.id, "output_main", n4q.id, "input_0");
        make_edge(n4q.id, "output_main", n5.id, "input_0");
        make_edge(n4.id, "output_false", n7.id, "input_0");
        make_edge(n5.id, "output_pass", n6.id, "input_0");
        make_edge(n5.id, "output_fail", n10.id, "input_0");
        make_edge(n6.id, "output_main", n8.id, "input_0");
        make_edge(n7.id, "output_main", n9.id, "input_0");
        toolbar_->set_workflow_name("Mean Reversion Strategy");
    } else if (idx == 7) {
        // ── 8. Portfolio Rebalancer ─────────────────────────────────
        // Holdings → optimizer → target weights to review. (The old chain fed the optimizer's
        // weights object into Risk Analysis, which needs a price series, and ended in a
        // single "Rebalance Order" with no symbol — it could never pass validation. One
        // fixed order cannot express a rebalance; place the orders from the weights shown.)
        // Get Holdings reads the broker account (live read; no paper holdings exist).
        auto n1 = make_node("trigger.manual", "Start", 100, 200);
        auto n2 = make_node("trading.get_holdings", "Current Holdings", 400, 200);
        auto n3 = make_node("analytics.portfolio_optimization", "Optimize", 700, 200);
        set_param(n3, "method", "mean_variance");
        auto n4 = make_node("output.results_display", "Target Weights", 1000, 200);
        set_param(n4, "label", "Target Weights");
        make_edge(n1.id, "output_main", n2.id, "input_0");
        make_edge(n2.id, "output_main", n3.id, "input_0");
        make_edge(n3.id, "output_main", n4.id, "input_0");
        toolbar_->set_workflow_name("Portfolio Rebalancer");
    } else if (idx == 8) {
        // ── 9. Daily Risk Monitor ───────────────────────────────────
        // The Schedule trigger holds the run until 9:30 on the next weekday, then fires once
        // (no background scheduler: press EXECUTE and keep the app open).
        // VaR needs a price series — positions rows carry none — so it is computed from the
        // benchmark history; the If/Else replaces a Loss Limit gate that needs P&L figures.
        auto n1 = make_node("trigger.schedule", "Start (cron 9:30 Mon-Fri)", 100, 200);
        set_param(n1, "cron", "30 9 * * 1-5");
        auto n2 = make_node("market.get_historical", "SPY 1y (portfolio proxy)", 400, 200);
        set_param(n2, "symbol", "SPY");
        set_param(n2, "period", "1y");
        auto n3 = make_node("analytics.risk_analysis", "VaR Analysis", 700, 200);
        set_param(n3, "method", "historical_var");
        set_param(n3, "confidence", 0.99);
        auto n4 = make_node("control.if_else", "1-day VaR under 3%?", 1000, 200);
        set_param(n4, "condition", "var < 0.03");
        auto n5 = make_node("notify.slack", "All Clear", 1300, 100);
        set_param(n5, "channel", "#risk-alerts");
        auto n6 = make_node("notify.discord", "RISK ALERT!", 1300, 350);
        auto n7 = make_node("output.results_display", "Risk Report", 1000, 450);
        set_param(n7, "label", "Risk Report");
        make_edge(n1.id, "output_main", n2.id, "input_0");
        make_edge(n2.id, "output_main", n3.id, "input_0");
        make_edge(n3.id, "output_main", n4.id, "input_0");
        make_edge(n4.id, "output_true", n5.id, "input_0");
        make_edge(n4.id, "output_false", n6.id, "input_0");
        make_edge(n3.id, "output_main", n7.id, "input_0");
        toolbar_->set_workflow_name("Daily Risk Monitor");
    } else if (idx == 9) {
        // ── 10. Pre-Trade Compliance ────────────────────────────────
        // The gates check the order's own figures (they fail closed without them), so the
        // order is described first: live price from a quote, then size and account value.
        // (A Loss Limit gate needs daily / weekly P&L, which nothing upstream can supply —
        // it always blocked; it was dropped.)
        auto n1 = make_node("trigger.manual", "New Order", 100, 250);
        auto n2 = make_node("safety.trading_hours", "Market Open?", 400, 250);
        set_param(n2, "exchange", "NYSE");
        auto n2a = make_node("market.get_quote", "Order Price (AAPL quote)", 700, 150);
        set_param(n2a, "symbol", "AAPL");
        auto n2b = make_node("core.set", "Order Qty: 10", 950, 150);
        set_param(n2b, "key", "quantity");
        set_param(n2b, "value", "10");
        auto n2c = make_node("core.set", "Portfolio Value (edit)", 1200, 150);
        set_param(n2c, "key", "portfolio_value");
        set_param(n2c, "value", "100000");
        auto n3 = make_node("safety.risk_check", "Position Limits", 1450, 150);
        auto n4 = make_node("safety.position_size_limit", "Size Limits", 1700, 150);
        auto n6 = make_node("trading.place_order", "Execute Order", 1950, 150);
        set_param(n6, "symbol", "AAPL");
        set_param(n6, "exchange", "NASDAQ");
        set_param(n6, "side", "buy");
        set_param(n6, "quantity", 10);
        auto n7 = make_node("core.set", "Market Closed", 700, 400);
        set_param(n7, "key", "reason");
        set_param(n7, "value", "Market is closed");
        auto n8 = make_node("output.results_display", "Execution Result", 2250, 150);
        auto n9 = make_node("output.results_display", "Rejected", 1000, 400);
        auto n10 = make_node("output.results_display", "Blocked by Limits", 1950, 350);
        make_edge(n1.id, "output_main", n2.id, "input_0");
        make_edge(n2.id, "output_open", n2a.id, "input_0");
        make_edge(n2a.id, "output_main", n2b.id, "input_0");
        make_edge(n2b.id, "output_main", n2c.id, "input_0");
        make_edge(n2c.id, "output_main", n3.id, "input_0");
        make_edge(n2.id, "output_closed", n7.id, "input_0");
        make_edge(n3.id, "output_pass", n4.id, "input_0");
        make_edge(n3.id, "output_fail", n10.id, "input_0");
        make_edge(n4.id, "output_pass", n6.id, "input_0");
        make_edge(n4.id, "output_fail", n10.id, "input_0");
        make_edge(n6.id, "output_main", n8.id, "input_0");
        make_edge(n7.id, "output_main", n9.id, "input_0");
        toolbar_->set_workflow_name("Pre-Trade Compliance");
    } else if (idx == 10) {
        // ── 11. News Sentiment Pipeline ─────────────────────────────
        auto n1 = make_node("trigger.news_event", "Tech News", 100, 200);
        set_param(n1, "keywords", "AAPL,earnings,revenue,guidance");
        auto n2 = make_node("market.get_news", "Fetch News", 400, 200);
        set_param(n2, "symbol", "AAPL");
        set_param(n2, "limit", 20);
        // The fetched articles go straight to the agent. (A "relevance > 0.7" filter used to
        // sit here, but articles carry no relevance field, and transform.filter treats the
        // {articles: [...]} object as one item — so it always passed nothing on.)
        auto n4 = make_node("agent.run", "Sentiment Agent", 700, 200);
        set_param(n4, "extra_instructions",
                  "Analyze the sentiment of the news articles in the context. "
                  "Answer with one word, BULLISH or BEARISH, followed by a one-sentence reason.");
        auto n5 = make_node("control.if_else", "Bullish?", 1300, 200);
        set_param(n5, "condition", "response contains BULLISH");
        auto n6 = make_node("notify.telegram", "Buy Signal", 1600, 100);
        auto n7 = make_node("core.set", "Hold", 1600, 350);
        set_param(n7, "key", "action");
        set_param(n7, "value", "hold_position");
        auto n8 = make_node("output.results_display", "Signal", 1900, 100);
        auto n9 = make_node("output.results_display", "No Action", 1900, 350);
        make_edge(n1.id, "output_main", n2.id, "input_0");
        make_edge(n2.id, "output_main", n4.id, "input_0");
        make_edge(n4.id, "output_main", n5.id, "input_0");
        make_edge(n5.id, "output_true", n6.id, "input_0");
        make_edge(n5.id, "output_false", n7.id, "input_0");
        make_edge(n6.id, "output_main", n8.id, "input_0");
        make_edge(n7.id, "output_main", n9.id, "input_0");
        toolbar_->set_workflow_name("News Sentiment Pipeline");
    } else if (idx == 11) {
        // ── 12. AI Research Agent ───────────────────────────────────
        auto n1 = make_node("trigger.manual", "Start Research", 100, 250);
        auto n2 = make_node("market.get_quote", "Current Price", 400, 100);
        set_param(n2, "symbol", "NVDA");
        auto n3 = make_node("market.get_fundamentals", "Fundamentals", 400, 300);
        set_param(n3, "symbol", "NVDA");
        set_param(n3, "type", "overview");
        auto n4 = make_node("market.get_news", "Recent News", 400, 500);
        set_param(n4, "symbol", "NVDA");
        auto n5 = make_node("control.merge", "Combine Data", 700, 250);
        auto n6 = make_node("agent.run", "Bull Case Agent", 1000, 100);
        set_param(n6, "extra_instructions", "Make the bull case for this stock based on the data in the context.");
        auto n7 = make_node("agent.run", "Bear Case Agent", 1000, 400);
        set_param(n7, "extra_instructions", "Make the bear case for this stock. Identify the key risks.");
        auto n8 = make_node("agent.run", "Synthesize", 1300, 250);
        set_param(n8, "extra_instructions",
                  "The context holds a bull case and a bear case for the same stock. "
                  "Synthesize them into a balanced investment thesis.");
        auto n9 = make_node("output.results_display", "Research Report", 1600, 250);
        make_edge(n1.id, "output_main", n2.id, "input_0");
        make_edge(n1.id, "output_main", n3.id, "input_0");
        make_edge(n1.id, "output_main", n4.id, "input_0");
        make_edge(n2.id, "output_main", n5.id, "input_0");
        make_edge(n3.id, "output_main", n5.id, "input_1");
        make_edge(n4.id, "output_main", n5.id, "input_2");
        make_edge(n5.id, "output_main", n6.id, "input_0");
        make_edge(n5.id, "output_main", n7.id, "input_0");
        make_edge(n6.id, "output_main", n8.id, "input_0");
        make_edge(n7.id, "output_main", n8.id, "input_0"); // the agent node has one Context port
        make_edge(n8.id, "output_main", n9.id, "input_0");
        toolbar_->set_workflow_name("AI Research Agent");
    } else if (idx == 12) {
        // ── 13. Scheduled Report ────────────────────────────────────
        // The Schedule trigger holds the run until 17:00 on the next weekday, then fires once.
        // Positions (with their realized / unrealized P&L) and the balance are merged and
        // mailed. (Performance Metrics used to sit here, but it needs a price series and
        // always failed on positions + balance.)
        auto n1 = make_node("trigger.schedule", "Start (cron 17:00 Mon-Fri)", 100, 200);
        set_param(n1, "cron", "0 17 * * 1-5");
        auto n2 = make_node("trading.get_positions", "Positions", 400, 100);
        auto n3 = make_node("trading.get_balance", "Balance", 400, 300);
        auto n4 = make_node("control.merge", "Merge", 700, 200);
        auto n6 = make_node("notify.email", "Send Report", 1000, 200);
        set_param(n6, "title", "Daily Portfolio Report");
        auto n7 = make_node("output.results_display", "Report", 1300, 200);
        set_param(n7, "label", "Daily Portfolio Report");
        make_edge(n1.id, "output_main", n2.id, "input_0");
        make_edge(n1.id, "output_main", n3.id, "input_0");
        make_edge(n2.id, "output_main", n4.id, "input_0");
        make_edge(n3.id, "output_main", n4.id, "input_1");
        make_edge(n4.id, "output_main", n6.id, "input_0");
        make_edge(n6.id, "output_main", n7.id, "input_0");
        toolbar_->set_workflow_name("Scheduled Daily Report");
    } else if (idx == 13) {
        // ── 14. Data Export Pipeline ────────────────────────────────
        // A keyless source that returns rows (the old example.com URL always failed). Swap in
        // an HTTP Request / API Call node for your own endpoint.
        auto n1 = make_node("trigger.manual", "Start", 100, 200);
        auto n2 = make_node("market.get_historical", "Fetch AAPL 3mo", 400, 200);
        set_param(n2, "symbol", "AAPL");
        set_param(n2, "period", "3mo");
        auto n3 = make_node("transform.filter", "Filter Traded Days", 700, 200);
        set_param(n3, "field", "volume");
        set_param(n3, "operator", "greater_than");
        set_param(n3, "value", "0");
        auto n4 = make_node("transform.sort", "Sort by Date", 1000, 200);
        set_param(n4, "field", "timestamp");
        set_param(n4, "direction", "desc");
        auto n5 = make_node("transform.deduplicate", "Remove Dupes", 1300, 200);
        set_param(n5, "field", "timestamp");
        auto n6 = make_node("file.convert", "Export CSV", 1600, 200);
        set_param(n6, "format", "csv");
        set_param(n6, "path", "output/export.csv");
        auto n7 = make_node("output.results_display", "Export Done", 1900, 200);
        make_edge(n1.id, "output_main", n2.id, "input_0");
        make_edge(n2.id, "output_main", n3.id, "input_0");
        make_edge(n3.id, "output_main", n4.id, "input_0");
        make_edge(n4.id, "output_main", n5.id, "input_0");
        make_edge(n5.id, "output_main", n6.id, "input_0");
        make_edge(n6.id, "output_main", n7.id, "input_0");
        toolbar_->set_workflow_name("Data Export Pipeline");
    } else if (idx == 14) {
        // ── 15. Signal Automation ───────────────────────────────────
        // (Was driven by trigger.webhook, which needs the workflow scheduler and
        // therefore never fires — the template was inert. Start it manually and put
        // the signal in "Signal".)
        // Risk Check fails closed without quantity / price / portfolio_value, and a bare
        // {signal} payload has none of them: price the order from a quote and size it first.
        auto n1 = make_node("trigger.manual", "Start", 100, 200);
        auto n2 = make_node("core.set", "Signal", 400, 200);
        set_param(n2, "key", "signal");
        set_param(n2, "value", "BUY");
        auto n3 = make_node("control.if_else", "Valid Signal?", 700, 200);
        set_param(n3, "condition", "signal == BUY");
        auto n3a = make_node("market.get_quote", "Order Price (AAPL quote)", 1000, 100);
        set_param(n3a, "symbol", "AAPL");
        auto n3b = make_node("core.set", "Order Qty: 10", 1250, 100);
        set_param(n3b, "key", "quantity");
        set_param(n3b, "value", "10");
        auto n3c = make_node("core.set", "Portfolio Value (edit)", 1500, 100);
        set_param(n3c, "key", "portfolio_value");
        set_param(n3c, "value", "100000");
        auto n4 = make_node("safety.risk_check", "Risk Check", 1750, 100);
        auto n5 = make_node("trading.place_order", "Execute Trade", 2000, 50);
        set_param(n5, "symbol", "AAPL");
        set_param(n5, "exchange", "NASDAQ");
        set_param(n5, "side", "buy");
        set_param(n5, "quantity", 10);
        auto n6 = make_node("notify.discord", "Invalid Signal", 1000, 350);
        auto n7 = make_node("output.results_display", "Trade Executed", 2300, 50);
        auto n8 = make_node("output.results_display", "Rejected", 1300, 350);
        auto n9 = make_node("output.results_display", "Blocked by Risk Check", 2000, 250);
        make_edge(n1.id, "output_main", n2.id, "input_0");
        make_edge(n2.id, "output_main", n3.id, "input_0");
        make_edge(n3.id, "output_true", n3a.id, "input_0");
        make_edge(n3a.id, "output_main", n3b.id, "input_0");
        make_edge(n3b.id, "output_main", n3c.id, "input_0");
        make_edge(n3c.id, "output_main", n4.id, "input_0");
        make_edge(n3.id, "output_false", n6.id, "input_0");
        make_edge(n4.id, "output_pass", n5.id, "input_0");
        make_edge(n4.id, "output_fail", n9.id, "input_0");
        make_edge(n5.id, "output_main", n7.id, "input_0");
        make_edge(n6.id, "output_main", n8.id, "input_0");
        toolbar_->set_workflow_name("Signal Automation");
    }

    current_workflow_id_.clear();
    current_description_.clear();
    static_data_ = QJsonObject{};
    set_live_mode(false); // a template is a new workflow — it always starts in PAPER
    dirty_ = true;
    // The scene already holds the template; record the swap so Ctrl+Z restores
    // whatever was on the canvas before. (push() re-applies `after`, which
    // rebuilds an identical graph.)
    undo_stack_->push(new commands::ReplaceGraphCommand(scene_, before, scene_->serialize(), tr("Load template")));
    LOG_DEBUG("NodeEditor", QString("Loaded template: %1").arg(chosen));
    // push() rebuilt the graph from `after` (same ids): select on the final items.
    select_entry_node();
}

void NodeEditorScreen::on_deploy() {
    if (WorkflowService::instance().is_executing()) {
        QMessageBox::information(this, tr("Save & Run"), tr("A run is in progress — stop it first."));
        return;
    }
    const WorkflowDef preview = current_workflow();
    QStringList live_rows;
    for (const auto& r : WorkflowService::live_action_rows(preview, true)) {
        if (r.moves_money)
            live_rows << r.text;
    }

    DeployDialog dlg(toolbar_->workflow_name(), current_description_, live_mode_, live_rows, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    toolbar_->set_workflow_name(dlg.workflow_name());
    current_description_ = dlg.workflow_description();
    const bool live = dlg.live_mode();
    if (live != live_mode_)
        LOG_WARN("NodeEditor", QString("Workflow '%1' execution mode set to %2")
                                   .arg(dlg.workflow_name(), live ? "LIVE" : "PAPER"));
    set_live_mode(live);
    // There is no scheduler or webhook runtime, so a saved workflow is never a
    // live deployment: it is persisted as a Draft and runs once when asked.
    if (!persist(true))
        return;

    if (dlg.is_deploy()) {
        LOG_INFO("NodeEditor", QString("Saved and ran workflow once: %1").arg(dlg.workflow_name()));
        // The user typed LIVE in the dialog a moment ago — don't ask twice.
        start_run({}, live);
    } else {
        LOG_INFO("NodeEditor", QString("Saved draft: %1").arg(dlg.workflow_name()));
    }
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap NodeEditorScreen::save_state() const {
    return {{"workflow_id", current_workflow_id_}};
}

void NodeEditorScreen::restore_state(const QVariantMap& state) {
    const QString wf_id = state.value("workflow_id").toString();
    if (wf_id.isEmpty())
        return;

    // wire_signals() already connected workflow_loaded → scene_->deserialize().
    // Just trigger the load — the existing handler in wire_signals() does the rest.
    current_workflow_id_ = wf_id;
    WorkflowService::instance().load_workflow(wf_id);
}

bool NodeEditorScreen::open_section(const QString& section, const QString& focus) {
    Q_UNUSED(focus);
    const QString key = section.trimmed().toLower();
    if (key.isEmpty())
        return true;
    // Both pickers are modal: opened after the link has landed, as the toolbar buttons open them.
    if (key == QLatin1String("templates")) {
        QMetaObject::invokeMethod(this, [this]() { on_show_templates(); }, Qt::QueuedConnection);
        return true;
    }
    if (key == QLatin1String("load")) {
        QMetaObject::invokeMethod(this, [this]() { on_load_workflow(); }, Qt::QueuedConnection);
        return true;
    }
    return false;
}

// ── Search box rows (core/command/CommandRegistry.h; screens/common/CommandRows.h) ──
namespace {

QList<fincept::command::CommandItem> ne_command_rows() {
    namespace rows = fincept::screens::command_rows;
    const QString where = NodeEditorScreen::tr("Node Editor");
    const QString screen = QStringLiteral("node_editor");
    return {
        rows::section(screen, QStringLiteral("templates"), NodeEditorScreen::tr("Workflow templates"),
                      {QStringLiteral("templates"), QStringLiteral("workflow"), QStringLiteral("automation"),
                       QStringLiteral("examples"), QStringLiteral("pipeline")},
                      where),
        rows::section(screen, QStringLiteral("load"), NodeEditorScreen::tr("Open saved workflow"),
                      {QStringLiteral("load workflow"), QStringLiteral("saved workflows"), QStringLiteral("my workflows"),
                       QStringLiteral("open workflow")},
                      where),
    };
}

void ne_install_commands() {
    fincept::screens::command_rows::install(QStringLiteral("node_editor"), &ne_command_rows);
}

} // namespace

Q_COREAPP_STARTUP_FUNCTION(ne_install_commands)

} // namespace fincept::workflow
