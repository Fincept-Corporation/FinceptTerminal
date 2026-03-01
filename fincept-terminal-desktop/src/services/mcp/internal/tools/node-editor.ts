/**
 * Node Editor MCP Tools
 *
 * Provides AI/Chat access to the node-based workflow editor.
 * Allows LLM to:
 * - List available nodes and their configurations
 * - Create complete workflows with nodes and edges
 * - Save workflows to the node editor for execution
 * - Execute and manage workflows
 *
 * Tools are split by domain group:
 *  - node-editor/constants.ts        — NODE_CATEGORIES, AVAILABLE_NODE_TYPES, getNodeColor
 *  - node-editor/smartBuilder.ts     — node_editor_build_workflow (primary LLM tool)
 *  - node-editor/discoveryTools.ts   — list_available_nodes, get_node_details, get_node_categories
 *  - node-editor/creationTools.ts    — create_workflow, add_node_to_workflow, connect_nodes, configure_node
 *  - node-editor/managementTools.ts  — list_workflows, get_workflow, update_workflow, delete_workflow, duplicate_workflow
 *  - node-editor/executionTools.ts   — execute_workflow, stop_workflow, get_execution_results, validate_workflow, open_workflow, export_workflow, import_workflow
 *  - node-editor/nlHelpers.ts        — describe_workflow, suggest_next_node, add_node_by_description
 *  - node-editor/editingTools.ts     — remove_node, disconnect_nodes, rename_node, clear_workflow, clone_node, move_node, get_workflow_stats, auto_layout
 */

import { InternalTool } from '../types';
import { smartBuilderTools } from './node-editor/smartBuilder';
import { discoveryTools } from './node-editor/discoveryTools';
import { creationTools } from './node-editor/creationTools';
import { managementTools } from './node-editor/managementTools';
import { executionTools } from './node-editor/executionTools';
import { nlHelperTools } from './node-editor/nlHelpers';
import { editingTools } from './node-editor/editingTools';

export const nodeEditorTools: InternalTool[] = [
  ...smartBuilderTools,
  ...discoveryTools,
  ...creationTools,
  ...managementTools,
  ...executionTools,
  ...nlHelperTools,
  ...editingTools,
];
