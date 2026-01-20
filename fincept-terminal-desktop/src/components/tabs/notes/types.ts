// Notes Tab Types

import type { Note, NoteTemplate } from '@/services/core/notesService';

// Re-export service types
export type { Note, NoteTemplate };

export interface NoteStatistics {
  total: number;
  favorites: number;
  totalWords: number;
  byCategory: Array<{ category: string; count: number }>;
}

export interface UpcomingReminder {
  note: Note;
  hoursUntil: number;
}

export interface EditorState {
  title: string;
  content: string;
  category: string;
  priority: string;
  sentiment: string;
  tags: string;
  tickers: string;
  colorCode: string;
  reminderDate: string;
}

export interface FilterState {
  filterPriority: string | null;
  filterSentiment: string | null;
  showFavorites: boolean;
  showArchived: boolean;
}

export type Category = {
  id: string;
  label: string;
  icon: React.ComponentType<{ size?: number; color?: string }>;
};
