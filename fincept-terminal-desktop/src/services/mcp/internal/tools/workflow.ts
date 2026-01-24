import { InternalTool } from '../types';

export const workflowTools: InternalTool[] = [
  {
    name: 'create_workflow',
    description: 'Create a new data processing workflow',
    inputSchema: {
      type: 'object',
      properties: {
        name: { type: 'string', description: 'Workflow name' },
        description: { type: 'string', description: 'Workflow description' },
      },
      required: ['name'],
    },
    handler: async (args, contexts) => {
      if (!contexts.createWorkflow) {
        return { success: false, error: 'Workflow context not available' };
      }
      const result = await contexts.createWorkflow(args);
      return { success: true, data: result, message: `Workflow "${args.name}" created` };
    },
  },
  {
    name: 'run_workflow',
    description: 'Execute a saved workflow by ID',
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: { type: 'string', description: 'Workflow ID to execute' },
      },
      required: ['workflow_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.runWorkflow) {
        return { success: false, error: 'Workflow context not available' };
      }
      const result = await contexts.runWorkflow(args.workflow_id);
      return { success: true, data: result, message: `Workflow executed` };
    },
  },
  {
    name: 'stop_workflow',
    description: 'Stop a running workflow',
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: { type: 'string', description: 'Workflow ID to stop' },
      },
      required: ['workflow_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.stopWorkflow) {
        return { success: false, error: 'Workflow context not available' };
      }
      await contexts.stopWorkflow(args.workflow_id);
      return { success: true, message: `Workflow stopped` };
    },
  },
  {
    name: 'list_workflows',
    description: 'List all saved workflows',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.listWorkflows) {
        return { success: false, error: 'Workflow context not available' };
      }
      const workflows = await contexts.listWorkflows();
      return { success: true, data: workflows };
    },
  },
];
