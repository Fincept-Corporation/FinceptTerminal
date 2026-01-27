// Notes MCP Bridge - Connects notesService to MCP Internal Tools

import { terminalMCPProvider } from './TerminalMCPProvider';
import { notesService } from '@/services/core/notesService';

/**
 * Bridge that connects notesService to MCP tools
 * Allows AI to manage notes via chat
 */
export class NotesMCPBridge {
  private connected: boolean = false;

  /**
   * Connect notes service to MCP contexts
   */
  connect(): void {
    if (this.connected) return;

    terminalMCPProvider.setContexts({
      // Create note
      createNote: async (title: string, content: string) => {
        const noteId = await notesService.createNote({
          title,
          content,
          category: 'GENERAL',
          priority: 'MEDIUM',
          sentiment: 'NEUTRAL',
          tags: '',
          tickers: '',
          is_favorite: false,
          is_archived: false,
          color_code: '#1E293B',
          attachments: '[]',
          reminder_date: null,
          word_count: content.split(/\s+/).length,
        });
        return { id: noteId, title, content };
      },

      // List notes
      listNotes: async () => {
        const notes = await notesService.getAllNotes();
        return notes.map(note => ({
          id: note.id,
          title: note.title,
          content: note.content.substring(0, 200) + (note.content.length > 200 ? '...' : ''),
          category: note.category,
          priority: note.priority,
          tags: note.tags,
          created_at: note.created_at,
          updated_at: note.updated_at,
          is_favorite: note.is_favorite,
        }));
      },

      // Delete note
      deleteNote: async (id: string) => {
        await notesService.deleteNote(Number(id));
      },

      // Generate report (placeholder)
      generateReport: async (params: any) => {
        // TODO: Implement report generation
        return {
          success: false,
          message: 'Report generation not yet implemented',
          params,
        };
      },
    });

    this.connected = true;
    console.log('[NotesMCPBridge] Connected notes service to MCP');
  }

  /**
   * Disconnect and clear contexts
   */
  disconnect(): void {
    if (!this.connected) return;

    terminalMCPProvider.setContexts({
      createNote: undefined,
      listNotes: undefined,
      deleteNote: undefined,
      generateReport: undefined,
    });

    this.connected = false;
    console.log('[NotesMCPBridge] Disconnected notes service from MCP');
  }

  /**
   * Check if bridge is connected
   */
  isConnected(): boolean {
    return this.connected;
  }
}

// Singleton instance
export const notesMCPBridge = new NotesMCPBridge();
