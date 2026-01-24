import { InternalTool } from '../types';

export const notesTools: InternalTool[] = [
  {
    name: 'create_note',
    description: 'Create a note or save important information',
    inputSchema: {
      type: 'object',
      properties: {
        title: { type: 'string', description: 'Note title' },
        content: { type: 'string', description: 'Note content (supports markdown)' },
        tags: { type: 'string', description: 'Comma-separated tags for categorization' },
      },
      required: ['title', 'content'],
    },
    handler: async (args, contexts) => {
      if (!contexts.createNote) {
        return { success: false, error: 'Notes context not available' };
      }
      const result = await contexts.createNote(args.title, args.content);
      return { success: true, data: result, message: `Note "${args.title}" created` };
    },
  },
  {
    name: 'list_notes',
    description: 'List all saved notes',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.listNotes) {
        return { success: false, error: 'Notes context not available' };
      }
      const notes = await contexts.listNotes();
      return { success: true, data: notes };
    },
  },
  {
    name: 'delete_note',
    description: 'Delete a saved note',
    inputSchema: {
      type: 'object',
      properties: {
        note_id: { type: 'string', description: 'Note ID to delete' },
      },
      required: ['note_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.deleteNote) {
        return { success: false, error: 'Notes context not available' };
      }
      await contexts.deleteNote(args.note_id);
      return { success: true, message: 'Note deleted' };
    },
  },
  {
    name: 'generate_report',
    description: 'Generate a formatted report from portfolio/market data',
    inputSchema: {
      type: 'object',
      properties: {
        type: { type: 'string', description: 'Report type', enum: ['portfolio', 'market', 'backtest', 'custom'] },
        title: { type: 'string', description: 'Report title' },
        parameters: { type: 'string', description: 'JSON string of report parameters (symbols, date range, etc.)' },
      },
      required: ['type', 'title'],
    },
    handler: async (args, contexts) => {
      if (!contexts.generateReport) {
        return { success: false, error: 'Report generation context not available' };
      }
      const params = { ...args };
      if (args.parameters && typeof args.parameters === 'string') {
        try { params.parameters = JSON.parse(args.parameters); } catch { /* keep as string */ }
      }
      const result = await contexts.generateReport(params);
      return { success: true, data: result, message: `Report "${args.title}" generated` };
    },
  },
];
