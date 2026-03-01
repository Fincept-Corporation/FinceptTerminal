/**
 * Node Editor MCP Tools — Editing Tools
 *
 * Quick CRUD operations: remove nodes, disconnect, rename, clear, clone,
 * move, get stats, and auto-layout.
 */

import { InternalTool } from '../../types';

export const editingTools: InternalTool[] = [
  // ============================================================================
  // QUICK WORKFLOW EDITING TOOLS (Essential CRUD Operations)
  // ============================================================================
  {
    name: 'node_editor_remove_node',
    description: `Remove a node from a workflow. Also removes all edges connected to this node.`,
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID',
        },
        node_id: {
          type: 'string',
          description: 'ID of the node to remove',
        },
      },
      required: ['workflow_id', 'node_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.removeNodeFromWorkflow) {
        return { success: false, error: 'Node editor context not available' };
      }

      const result = await contexts.removeNodeFromWorkflow(args.workflow_id, args.node_id);
      return {
        success: true,
        data: result,
        message: `Node '${args.node_id}' removed from workflow`,
      };
    },
  },

  {
    name: 'node_editor_disconnect_nodes',
    description: `Remove the connection (edge) between two nodes.`,
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID',
        },
        source_node_id: {
          type: 'string',
          description: 'ID of the source node',
        },
        target_node_id: {
          type: 'string',
          description: 'ID of the target node',
        },
        source_handle: {
          type: 'string',
          description: 'Optional: specific output handle to disconnect',
        },
      },
      required: ['workflow_id', 'source_node_id', 'target_node_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.disconnectNodes) {
        return { success: false, error: 'Node editor context not available' };
      }

      const result = await contexts.disconnectNodes({
        workflowId: args.workflow_id,
        sourceNodeId: args.source_node_id,
        targetNodeId: args.target_node_id,
        sourceHandle: args.source_handle,
      });

      return {
        success: true,
        data: result,
        message: `Disconnected '${args.source_node_id}' from '${args.target_node_id}'`,
      };
    },
  },

  {
    name: 'node_editor_rename_node',
    description: `Change the display label of a node in a workflow.`,
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID',
        },
        node_id: {
          type: 'string',
          description: 'ID of the node to rename',
        },
        new_label: {
          type: 'string',
          description: 'New display label for the node',
        },
      },
      required: ['workflow_id', 'node_id', 'new_label'],
    },
    handler: async (args, contexts) => {
      if (!contexts.renameNode) {
        return { success: false, error: 'Node editor context not available' };
      }

      const result = await contexts.renameNode(args.workflow_id, args.node_id, args.new_label);
      return {
        success: true,
        data: result,
        message: `Node '${args.node_id}' renamed to "${args.new_label}"`,
      };
    },
  },

  {
    name: 'node_editor_clear_workflow',
    description: `Remove all nodes and edges from a workflow, keeping only the workflow metadata. Use this to start fresh.`,
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID to clear',
        },
      },
      required: ['workflow_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.clearWorkflow) {
        return { success: false, error: 'Node editor context not available' };
      }

      const result = await contexts.clearWorkflow(args.workflow_id);
      return {
        success: true,
        data: result,
        message: `Workflow '${args.workflow_id}' cleared - all nodes and edges removed`,
      };
    },
  },

  {
    name: 'node_editor_clone_node',
    description: `Create a duplicate of an existing node in the workflow. The clone is placed to the right of the original.`,
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID',
        },
        node_id: {
          type: 'string',
          description: 'ID of the node to clone',
        },
        new_label: {
          type: 'string',
          description: 'Optional: label for the cloned node (defaults to "Copy of [original]")',
        },
      },
      required: ['workflow_id', 'node_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.cloneNode) {
        return { success: false, error: 'Node editor context not available' };
      }

      const result = await contexts.cloneNode(args.workflow_id, args.node_id, args.new_label);
      return {
        success: true,
        data: result,
        message: `Node '${args.node_id}' cloned as '${result.newNodeId}'`,
      };
    },
  },

  {
    name: 'node_editor_move_node',
    description: `Move/reposition a node in the workflow canvas. Accepts absolute coordinates or relative directions.`,
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID',
        },
        node_id: {
          type: 'string',
          description: 'ID of the node to move',
        },
        x: {
          type: 'number',
          description: 'New X position (absolute) or offset if relative=true',
        },
        y: {
          type: 'number',
          description: 'New Y position (absolute) or offset if relative=true',
        },
        relative: {
          type: 'string',
          description: 'If "yes", x and y are offsets from current position (e.g., x=100 moves 100px right)',
          enum: ['yes', 'no', ''],
        },
        direction: {
          type: 'string',
          description: 'Alternative: move in a direction with default offset (50px)',
          enum: ['up', 'down', 'left', 'right', ''],
        },
      },
      required: ['workflow_id', 'node_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.moveNode) {
        return { success: false, error: 'Node editor context not available' };
      }

      const result = await contexts.moveNode({
        workflowId: args.workflow_id,
        nodeId: args.node_id,
        x: args.x,
        y: args.y,
        relative: args.relative === 'yes',
        direction: args.direction,
      });

      return {
        success: true,
        data: result,
        message: `Node '${args.node_id}' moved to position (${result.newPosition.x}, ${result.newPosition.y})`,
      };
    },
  },

  {
    name: 'node_editor_get_workflow_stats',
    description: `Get execution statistics for a workflow: run count, last run time, success rate, average duration.`,
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID',
        },
      },
      required: ['workflow_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getWorkflowStats) {
        return { success: false, error: 'Node editor context not available' };
      }

      const stats = await contexts.getWorkflowStats(args.workflow_id);
      return {
        success: true,
        data: stats,
        message: `Workflow stats: ${stats.totalRuns} runs, ${stats.successRate}% success rate, last run: ${stats.lastRunAt || 'never'}`,
      };
    },
  },

  {
    name: 'node_editor_auto_layout',
    description: `Automatically reorganize node positions for better visual clarity. Uses left-to-right flow layout.`,
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID to auto-layout',
        },
        direction: {
          type: 'string',
          description: 'Layout direction: horizontal (left-to-right, default), vertical (top-to-bottom)',
          enum: ['horizontal', 'vertical', ''],
        },
        spacing_x: {
          type: 'number',
          description: 'Horizontal spacing between nodes (default: 250)',
        },
        spacing_y: {
          type: 'number',
          description: 'Vertical spacing between nodes (default: 100)',
        },
      },
      required: ['workflow_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.autoLayoutWorkflow) {
        return { success: false, error: 'Node editor context not available' };
      }

      const result = await contexts.autoLayoutWorkflow({
        workflowId: args.workflow_id,
        direction: args.direction || 'horizontal',
        spacingX: args.spacing_x || 250,
        spacingY: args.spacing_y || 100,
      });

      return {
        success: true,
        data: result,
        message: `Workflow auto-arranged: ${result.nodesRepositioned} nodes repositioned`,
      };
    },
  },
];
