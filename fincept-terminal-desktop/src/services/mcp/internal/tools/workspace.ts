import { InternalTool } from '../types';

export const workspaceTools: InternalTool[] = [
  {
    name: 'save_workspace',
    description: 'Save the current terminal state as a workspace',
    inputSchema: {
      type: 'object',
      properties: {
        name: { type: 'string', description: 'Workspace name' },
      },
      required: ['name'],
    },
    handler: async (args, contexts) => {
      if (!contexts.saveWorkspace) {
        return { success: false, error: 'Workspace context not available' };
      }
      const result = await contexts.saveWorkspace(args.name);
      return { success: true, data: result, message: `Workspace "${args.name}" saved` };
    },
  },
  {
    name: 'load_workspace',
    description: 'Load a previously saved workspace',
    inputSchema: {
      type: 'object',
      properties: {
        workspace_id: { type: 'string', description: 'Workspace ID to load' },
      },
      required: ['workspace_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.loadWorkspace) {
        return { success: false, error: 'Workspace context not available' };
      }
      await contexts.loadWorkspace(args.workspace_id);
      return { success: true, message: `Workspace loaded` };
    },
  },
  {
    name: 'list_workspaces',
    description: 'List all saved workspaces',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.listWorkspaces) {
        return { success: false, error: 'Workspace context not available' };
      }
      const workspaces = await contexts.listWorkspaces();
      return { success: true, data: workspaces };
    },
  },
  {
    name: 'delete_workspace',
    description: 'Delete a saved workspace',
    inputSchema: {
      type: 'object',
      properties: {
        workspace_id: { type: 'string', description: 'Workspace ID to delete' },
      },
      required: ['workspace_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.deleteWorkspace) {
        return { success: false, error: 'Workspace context not available' };
      }
      await contexts.deleteWorkspace(args.workspace_id);
      return { success: true, message: `Workspace deleted` };
    },
  },
];
