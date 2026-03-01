/**
 * Node Editor MCP Tools — Execution & I/O Tools
 *
 * Tools for executing workflows, retrieving results, validating, and
 * importing/exporting workflow JSON.
 */

import { InternalTool } from '../../types';

export const executionTools: InternalTool[] = [
  // ============================================================================
  // WORKFLOW EXECUTION TOOLS
  // ============================================================================
  {
    name: 'node_editor_execute_workflow',
    description: 'Execute a workflow. The workflow will run all nodes from triggers to outputs.',
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID to execute',
        },
        input_data: {
          type: 'string',
          description: 'Optional JSON input data for the workflow (passed to trigger nodes)',
        },
      },
      required: ['workflow_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.executeNodeWorkflow) {
        return { success: false, error: 'Node editor context not available' };
      }

      let inputData;
      if (args.input_data) {
        try {
          inputData = typeof args.input_data === 'string' ? JSON.parse(args.input_data) : args.input_data;
        } catch (e) {
          return { success: false, error: `Invalid JSON in input_data: ${e}` };
        }
      }

      const result = await contexts.executeNodeWorkflow(args.workflow_id, inputData);
      return {
        success: true,
        data: result,
        message: `Workflow execution ${result.success ? 'completed' : 'failed'}`,
      };
    },
  },

  {
    name: 'node_editor_stop_workflow',
    description: 'Stop a running workflow execution.',
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID to stop',
        },
      },
      required: ['workflow_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.stopNodeWorkflow) {
        return { success: false, error: 'Node editor context not available' };
      }
      await contexts.stopNodeWorkflow(args.workflow_id);
      return {
        success: true,
        message: `Workflow '${args.workflow_id}' stopped`,
      };
    },
  },

  {
    name: 'node_editor_get_execution_results',
    description: 'Get the results of a workflow execution including output from each node.',
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID to get results for',
        },
      },
      required: ['workflow_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getWorkflowResults) {
        return { success: false, error: 'Node editor context not available' };
      }
      const results = await contexts.getWorkflowResults(args.workflow_id);
      return {
        success: true,
        data: results,
        message: results ? 'Execution results retrieved' : 'No results available',
      };
    },
  },

  // ============================================================================
  // HELPER TOOLS
  // ============================================================================
  {
    name: 'node_editor_validate_workflow',
    description: 'Validate a workflow for errors before execution. Checks for missing connections, invalid parameters, etc.',
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID to validate',
        },
      },
      required: ['workflow_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.validateNodeWorkflow) {
        return { success: false, error: 'Node editor context not available' };
      }
      const validation = await contexts.validateNodeWorkflow(args.workflow_id);
      return {
        success: true,
        data: validation,
        message: validation.valid ? 'Workflow is valid' : `Found ${validation.errors.length} issues`,
      };
    },
  },

  {
    name: 'node_editor_open_workflow',
    description: 'Open a workflow in the Node Editor tab (tab: "nodes") for visual editing. Navigates to the Node Editor and loads the workflow.',
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID to open in the editor',
        },
      },
      required: ['workflow_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.openWorkflowInEditor) {
        return { success: false, error: 'Node editor context not available' };
      }
      await contexts.openWorkflowInEditor(args.workflow_id);
      return {
        success: true,
        message: `Workflow opened in Node Editor. You can now visually edit it.`,
      };
    },
  },

  {
    name: 'node_editor_export_workflow',
    description: 'Export a workflow as JSON for sharing or backup.',
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID to export',
        },
      },
      required: ['workflow_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.exportNodeWorkflow) {
        return { success: false, error: 'Node editor context not available' };
      }
      const exported = await contexts.exportNodeWorkflow(args.workflow_id);
      return {
        success: true,
        data: exported,
        message: 'Workflow exported as JSON',
      };
    },
  },

  {
    name: 'node_editor_import_workflow',
    description: 'Import a workflow from JSON.',
    inputSchema: {
      type: 'object',
      properties: {
        workflow_json: {
          type: 'string',
          description: 'JSON string of the workflow to import',
        },
        new_name: {
          type: 'string',
          description: 'Optional: Override the workflow name',
        },
      },
      required: ['workflow_json'],
    },
    handler: async (args, contexts) => {
      if (!contexts.importNodeWorkflow) {
        return { success: false, error: 'Node editor context not available' };
      }

      let workflowData;
      try {
        workflowData = typeof args.workflow_json === 'string'
          ? JSON.parse(args.workflow_json)
          : args.workflow_json;
      } catch (e) {
        return { success: false, error: `Invalid workflow JSON: ${e}` };
      }

      if (args.new_name) {
        workflowData.name = args.new_name;
      }

      const result = await contexts.importNodeWorkflow(workflowData);
      return {
        success: true,
        data: result,
        message: `Workflow "${result.name}" imported with ID: ${result.id}`,
      };
    },
  },
];
