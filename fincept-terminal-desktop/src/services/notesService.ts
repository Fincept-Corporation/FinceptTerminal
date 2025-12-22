// Notes Service - Now using high-performance Rust SQLite backend via Tauri commands

import { invoke } from '@tauri-apps/api/core';

export interface Note {
  id?: number;
  title: string;
  content: string;
  category: string; // RESEARCH, TRADE_IDEA, MARKET_ANALYSIS, EARNINGS, ECONOMIC, PORTFOLIO, GENERAL
  priority: string; // HIGH, MEDIUM, LOW
  tags: string; // Comma-separated tags
  tickers: string; // Comma-separated ticker symbols
  sentiment: string; // BULLISH, BEARISH, NEUTRAL
  is_favorite: boolean;
  is_archived: boolean;
  color_code: string; // Hex color for visual organization
  attachments: string; // JSON array of attachment paths
  created_at: string;
  updated_at: string;
  reminder_date: string | null; // ISO date string for reminders
  word_count: number;
}

export interface NoteTemplate {
  id?: number;
  name: string;
  description: string;
  content: string;
  category: string;
  created_at: string;
}

class NotesService {
  // No initialization needed - Rust backend handles it

  async initialize() {
    console.log('[NotesService] Using Rust SQLite backend');
  }

  async createNote(note: Omit<Note, 'id' | 'created_at' | 'updated_at'>): Promise<number> {
    try {
      const now = new Date().toISOString();
      const noteData = {
        ...note,
        created_at: now,
        updated_at: now
      };
      const id = await invoke<number>('db_create_note', { note: noteData });
      console.log('[NotesService] Created note with ID:', id);
      return id;
    } catch (error) {
      console.error('[NotesService] Failed to create note:', error);
      throw error;
    }
  }

  async getAllNotes(includeArchived: boolean = false): Promise<Note[]> {
    try {
      return await invoke<Note[]>('db_get_all_notes', { includeArchived });
    } catch (error) {
      console.error('[NotesService] Failed to get notes:', error);
      throw error;
    }
  }

  async updateNote(id: number, updates: Partial<Omit<Note, 'id' | 'created_at' | 'updated_at'>>): Promise<void> {
    try {
      const notes = await this.getAllNotes(true);
      const existingNote = notes.find(n => n.id === id);
      if (!existingNote) {
        throw new Error('Note not found');
      }

      const updatedNote = {
        ...existingNote,
        ...updates,
        updated_at: new Date().toISOString()
      };

      await invoke('db_update_note', { id, updates: updatedNote });
      console.log('[NotesService] Updated note:', id);
    } catch (error) {
      console.error('[NotesService] Failed to update note:', error);
      throw error;
    }
  }

  async deleteNote(id: number): Promise<void> {
    try {
      await invoke('db_delete_note', { id });
      console.log('[NotesService] Deleted note:', id);
    } catch (error) {
      console.error('[NotesService] Failed to delete note:', error);
      throw error;
    }
  }

  async searchNotes(query: string): Promise<Note[]> {
    try {
      return await invoke<Note[]>('db_search_notes', { query });
    } catch (error) {
      console.error('[NotesService] Failed to search notes:', error);
      throw error;
    }
  }

  async getTemplates(): Promise<NoteTemplate[]> {
    try {
      return await invoke<NoteTemplate[]>('db_get_note_templates');
    } catch (error) {
      console.error('[NotesService] Failed to get templates:', error);
      throw error;
    }
  }

  // Helper methods
  async getNotesByCategory(category: string, includeArchived: boolean = false): Promise<Note[]> {
    const allNotes = await this.getAllNotes(includeArchived);
    return allNotes.filter(note => note.category === category);
  }

  async getFavoriteNotes(): Promise<Note[]> {
    const allNotes = await this.getAllNotes(false);
    return allNotes.filter(note => note.is_favorite);
  }

  async getNotesWithReminders(): Promise<Note[]> {
    const allNotes = await this.getAllNotes(false);
    return allNotes.filter(note => note.reminder_date !== null);
  }

  async toggleFavorite(id: number): Promise<void> {
    const notes = await this.getAllNotes(true);
    const note = notes.find(n => n.id === id);
    if (!note) {
      throw new Error('Note not found');
    }
    await this.updateNote(id, { is_favorite: !note.is_favorite });
  }

  async archiveNote(id: number): Promise<void> {
    await this.updateNote(id, { is_archived: true });
  }

  async unarchiveNote(id: number): Promise<void> {
    await this.updateNote(id, { is_archived: false });
  }

  async getNoteById(id: number): Promise<Note | null> {
    const notes = await this.getAllNotes(true);
    return notes.find(n => n.id === id) || null;
  }

  async getStatistics() {
    const notes = await this.getAllNotes(false);
    return {
      total: notes.length,
      favorites: notes.filter(n => n.is_favorite).length,
      byCategory: notes.reduce((acc, note) => {
        acc[note.category] = (acc[note.category] || 0) + 1;
        return acc;
      }, {} as Record<string, number>),
      byPriority: notes.reduce((acc, note) => {
        acc[note.priority] = (acc[note.priority] || 0) + 1;
        return acc;
      }, {} as Record<string, number>)
    };
  }
}

export const notesService = new NotesService();
export default notesService;
