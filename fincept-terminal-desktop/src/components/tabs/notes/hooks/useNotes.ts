// Hook for managing notes data

import { useState, useEffect, useCallback } from 'react';
import { notesService, Note, NoteTemplate } from '@/services/core/notesService';
import { noteReminderService } from '@/services/core/noteReminderService.tsx';
import { toast } from 'sonner';
import { showConfirm, showError } from '@/utils/notifications';
import type { NoteStatistics, UpcomingReminder, FilterState } from '../types';

export interface UseNotesReturn {
  notes: Note[];
  filteredNotes: Note[];
  templates: NoteTemplate[];
  statistics: NoteStatistics | null;
  upcomingReminders: UpcomingReminder[];
  selectedNote: Note | null;
  setSelectedNote: (note: Note | null) => void;
  selectedCategory: string;
  setSelectedCategory: (category: string) => void;
  filters: FilterState;
  setFilters: (filters: FilterState) => void;
  searchQuery: string;
  setSearchQuery: (query: string) => void;
  loadNotes: () => Promise<void>;
  loadTemplates: () => Promise<void>;
  loadStatistics: () => Promise<void>;
  loadUpcomingReminders: () => Promise<void>;
  createNote: (note: Omit<Note, 'id'>) => Promise<void>;
  updateNote: (id: number, updates: Partial<Note>) => Promise<void>;
  handleDeleteNote: (id: number) => Promise<void>;
  handleToggleFavorite: (note: Note) => Promise<void>;
  handleToggleArchive: (note: Note) => Promise<void>;
}

export function useNotes(): UseNotesReturn {
  const [notes, setNotes] = useState<Note[]>([]);
  const [filteredNotes, setFilteredNotes] = useState<Note[]>([]);
  const [templates, setTemplates] = useState<NoteTemplate[]>([]);
  const [statistics, setStatistics] = useState<NoteStatistics | null>(null);
  const [upcomingReminders, setUpcomingReminders] = useState<UpcomingReminder[]>([]);
  const [selectedNote, setSelectedNote] = useState<Note | null>(null);
  const [selectedCategory, setSelectedCategory] = useState('ALL');
  const [searchQuery, setSearchQuery] = useState('');
  const [filters, setFilters] = useState<FilterState>({
    filterPriority: null,
    filterSentiment: null,
    showFavorites: false,
    showArchived: false
  });

  // Apply filters to notes
  const applyFilters = useCallback((notesToFilter: Note[]) => {
    let filtered = [...notesToFilter];

    if (searchQuery) {
      filtered = filtered.filter(note =>
        note.title.toLowerCase().includes(searchQuery.toLowerCase()) ||
        note.content.toLowerCase().includes(searchQuery.toLowerCase()) ||
        note.tags?.toLowerCase().includes(searchQuery.toLowerCase()) ||
        note.tickers?.toLowerCase().includes(searchQuery.toLowerCase())
      );
    }

    if (filters.filterPriority) {
      filtered = filtered.filter(note => note.priority === filters.filterPriority);
    }

    if (filters.filterSentiment) {
      filtered = filtered.filter(note => note.sentiment === filters.filterSentiment);
    }

    setFilteredNotes(filtered);
  }, [searchQuery, filters.filterPriority, filters.filterSentiment]);

  // Load notes
  const loadNotes = useCallback(async () => {
    try {
      let loadedNotes: Note[];
      if (filters.showFavorites) {
        loadedNotes = await notesService.getFavoriteNotes();
      } else if (selectedCategory === 'ALL') {
        loadedNotes = await notesService.getAllNotes(filters.showArchived);
      } else {
        loadedNotes = await notesService.getNotesByCategory(selectedCategory, filters.showArchived);
      }
      setNotes(loadedNotes);
      applyFilters(loadedNotes);
    } catch (error) {
      console.error('[useNotes] Failed to load notes:', error);
    }
  }, [selectedCategory, filters.showFavorites, filters.showArchived, applyFilters]);

  // Load templates
  const loadTemplates = useCallback(async () => {
    try {
      const loadedTemplates = await notesService.getTemplates();
      setTemplates(loadedTemplates);
    } catch (error) {
      console.error('[useNotes] Failed to load templates:', error);
    }
  }, []);

  // Load statistics
  const loadStatistics = useCallback(async () => {
    try {
      const stats = await notesService.getStatistics();
      // Load all notes to calculate total words
      const allNotes = await notesService.getAllNotes(false);
      const totalWords = allNotes.reduce((sum, note) => sum + (note.word_count || 0), 0);

      // Transform byCategory from Record<string, number> to Array<{ category: string; count: number }>
      const byCategoryArray = Object.entries(stats.byCategory || {}).map(([category, count]) => ({
        category,
        count: count as number
      }));

      setStatistics({
        total: stats.total || 0,
        favorites: stats.favorites || 0,
        totalWords,
        byCategory: byCategoryArray
      });
    } catch (error) {
      console.error('[useNotes] Failed to load statistics:', error);
    }
  }, []);

  // Load upcoming reminders
  const loadUpcomingReminders = useCallback(async () => {
    try {
      const upcoming = await noteReminderService.getUpcomingReminders();
      setUpcomingReminders(upcoming);
    } catch (error) {
      console.error('[useNotes] Failed to load upcoming reminders:', error);
    }
  }, []);

  // Create note
  const createNote = useCallback(async (note: Omit<Note, 'id'>) => {
    try {
      await notesService.createNote(note);
      await loadNotes();
      await loadStatistics();
      await loadUpcomingReminders();
      if (note.reminder_date) {
        toast.success(`Note created with reminder set for ${new Date(note.reminder_date).toLocaleString()}`);
      }
    } catch (error) {
      console.error('[useNotes] Failed to create note:', error);
      throw error;
    }
  }, [loadNotes, loadStatistics, loadUpcomingReminders]);

  // Update note
  const updateNote = useCallback(async (id: number, updates: Partial<Note>) => {
    try {
      await notesService.updateNote(id, updates);
      await loadNotes();
      await loadStatistics();
      await loadUpcomingReminders();

      // Update selectedNote if it's the one being updated
      if (selectedNote?.id === id) {
        const updatedNote = await notesService.getNoteById(id);
        setSelectedNote(updatedNote);
      }

      if (updates.reminder_date) {
        toast.success(`Reminder updated for ${new Date(updates.reminder_date).toLocaleString()}`);
      }
    } catch (error) {
      console.error('[useNotes] Failed to update note:', error);
      throw error;
    }
  }, [loadNotes, loadStatistics, loadUpcomingReminders, selectedNote]);

  // Delete note
  const handleDeleteNote = useCallback(async (id: number) => {
    const confirmed = await showConfirm(
      'This action cannot be undone.',
      {
        title: 'Delete this note?',
        type: 'danger'
      }
    );
    if (!confirmed) return;

    try {
      await notesService.deleteNote(id);
      await loadNotes();
      await loadStatistics();
      if (selectedNote?.id === id) {
        setSelectedNote(null);
      }
    } catch (error) {
      console.error('[useNotes] Failed to delete note:', error);
      showError('Failed to delete note');
    }
  }, [loadNotes, loadStatistics, selectedNote]);

  // Toggle favorite
  const handleToggleFavorite = useCallback(async (note: Note) => {
    try {
      await notesService.updateNote(note.id!, { is_favorite: !note.is_favorite });
      await loadNotes();
    } catch (error) {
      console.error('[useNotes] Failed to toggle favorite:', error);
    }
  }, [loadNotes]);

  // Toggle archive
  const handleToggleArchive = useCallback(async (note: Note) => {
    try {
      await notesService.updateNote(note.id!, { is_archived: !note.is_archived });
      await loadNotes();
      await loadStatistics();
      if (selectedNote?.id === note.id) {
        setSelectedNote(null);
      }
    } catch (error) {
      console.error('[useNotes] Failed to toggle archive:', error);
    }
  }, [loadNotes, loadStatistics, selectedNote]);

  // Load data on mount and when dependencies change
  useEffect(() => {
    loadNotes();
    loadTemplates();
    loadStatistics();
  }, [loadNotes, loadTemplates, loadStatistics]);

  // Apply filters when notes change
  useEffect(() => {
    applyFilters(notes);
  }, [notes, applyFilters]);

  return {
    notes,
    filteredNotes,
    templates,
    statistics,
    upcomingReminders,
    selectedNote,
    setSelectedNote,
    selectedCategory,
    setSelectedCategory,
    filters,
    setFilters,
    searchQuery,
    setSearchQuery,
    loadNotes,
    loadTemplates,
    loadStatistics,
    loadUpcomingReminders,
    createNote,
    updateNote,
    handleDeleteNote,
    handleToggleFavorite,
    handleToggleArchive
  };
}
