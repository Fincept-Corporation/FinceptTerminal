/**
 * Node Editor MCP Tools — Management Tools
 *
 * Tools for listing, retrieving, updating, deleting, and duplicating workflows.
 */

import { InternalTool } from '../../types';

export const managementTools: InternalTool[] = [
  // ============================================================================
  // WORKFLOW MANAGEMENT TOOLS
  // ============================================================================
  {
    name: 'node_editor_list_workflows',
    description: 'List all saved workflows in the node editor. Returns workflow IDs, names, descriptions, and status.',
    inputSchema: {
      type: 'object',
      properties: {
        status: {
          type: 'string',
          description: 'Filter by status: idle, running, completed, error, draft',
          enum: ['idle', 'running', 'completed', 'error', 'draft', ''],
        },
      },
      required: [],
    },
    handler: async (args, contexts) => {
      if (!contexts.listNodeWorkflows) {
        return { success: false, error: 'Node editor context not available' };
      }
      const workflows = await contexts.listNodeWorkflows(args.status);
      return {
        success: true,
        data: workflows,
        message: `Found ${workflows.length} workflows`,
      };
    },
  },

  {
    name: 'node_editor_get_workflow',
    description: 'Get complete details of a workflow including all nodes, edges, and their configurations.',
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID to retrieve',
        },
      },
      required: ['workflow_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getNodeWorkflow) {
        return { success: false, error: 'Node editor context not available' };
      }
      const workflow = await contexts.getNodeWorkflow(args.workflow_id);
      if (!workflow) {
        return { success: false, error: `Workflow '${args.workflow_id}' not found` };
      }
      return {
        success: true,
        data: workflow,
        message: `Workflow "${workflow.name}" with ${(workflow.nodes || []).length} nodes`,
      };
    },
  },

  {
    name: 'node_editor_update_workflow',
    description: 'Update an existing workflow (name, description, or full node/edge replacement).',
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID to update',
        },
        name: {
          type: 'string',
          description: 'New workflow name',
        },
        description: {
          type: 'string',
          description: 'New workflow description',
        },
        nodes: {
          type: 'string',
          description: 'Optional: JSON array of nodes to replace existing nodes',
        },
        edges: {
          type: 'string',
          description: 'Optional: JSON array of edges to replace existing edges',
        },
      },
      required: ['workflow_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.updateNodeWorkflow) {
        return { success: false, error: 'Node editor context not available' };
      }

      let nodes, edges;
      if (args.nodes) {
        try {
          nodes = typeof args.nodes === 'string' ? JSON.parse(args.nodes) : args.nodes;
        } catch (e) {
          return { success: false, error: `Invalid JSON in nodes: ${e}` };
        }
      }
      if (args.edges) {
        try {
          edges = typeof args.edges === 'string' ? JSON.parse(args.edges) : args.edges;
        } catch (e) {
          return { success: false, error: `Invalid JSON in edges: ${e}` };
        }
      }

      const result = await contexts.updateNodeWorkflow({
        workflowId: args.workflow_id,
        name: args.name,
        description: args.description,
        nodes,
        edges,
      });

      return {
        success: true,
        data: result,
        message: `Workflow '${args.workflow_id}' updated successfully`,
      };
    },
  },

  {
    name: 'node_editor_delete_workflow',
    description: 'Delete a workflow from the node editor.',
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID to delete',
        },
      },
      required: ['workflow_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.deleteNodeWorkflow) {
        return { success: false, error: 'Node editor context not available' };
      }
      await contexts.deleteNodeWorkflow(args.workflow_id);
      return {
        success: true,
        message: `Workflow '${args.workflow_id}' deleted`,
      };
    },
  },

  {
    name: 'node_editor_duplicate_workflow',
    description: 'Duplicate an existing workflow with a new name.',
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID to duplicate',
        },
        new_name: {
          type: 'string',
          description: 'Name for the duplicated workflow',
        },
      },
      required: ['workflow_id', 'new_name'],
    },
    handler: async (args, contexts) => {
      if (!contexts.duplicateNodeWorkflow) {
        return { success: false, error: 'Node editor context not available' };
      }
      const result = await contexts.duplicateNodeWorkflow(args.workflow_id, args.new_name);
      return {
        success: true,
        data: result,
        message: `Workflow duplicated as "${args.new_name}" with ID: ${result.id}`,
      };
    },
  },
];
