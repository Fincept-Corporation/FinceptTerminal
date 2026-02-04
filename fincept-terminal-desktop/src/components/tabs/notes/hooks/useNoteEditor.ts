// Hook for managing note editor state

import { useState, useCallback } from 'react';
import { notesService, Note, NoteTemplate } from '@/services/core/notesService';
import { noteReminderService } from '@/services/core/noteReminderService.tsx';
import { toast } from '@/components/ui/terminal-toast';
import { showWarning, showError } from '@/utils/notifications';
import type { EditorState } from '../types';
import { DEFAULT_EDITOR_STATE } from '../constants';

interface UseNoteEditorProps {
  selectedNote: Note | null;
  setSelectedNote: (note: Note | null) => void;
  loadNotes: () => Promise<void>;
  loadUpcomingReminders: () => Promise<void>;
  loadStatistics: () => Promise<void>;
}

export interface UseNoteEditorReturn {
  editorState: EditorState;
  isEditing: boolean;
  isCreating: boolean;
  setEditorState: React.Dispatch<React.SetStateAction<EditorState>>;
  setIsEditing: (editing: boolean) => void;
  setIsCreating: (creating: boolean) => void;
  updateField: <K extends keyof EditorState>(field: K, value: EditorState[K]) => void;
  startEditing: (note: Note) => void;
  startCreating: () => void;
  createFromTemplate: (template: NoteTemplate) => void;
  resetEditor: () => void;
  handleSave: () => Promise<void>;
  handleCancel: () => void;
  buildNoteFromEditor: () => Omit<Note, 'id'>;
}

export function useNoteEditor({
  selectedNote,
  setSelectedNote,
  loadNotes,
  loadUpcomingReminders,
  loadStatistics
}: UseNoteEditorProps): UseNoteEditorReturn {
  const [editorState, setEditorState] = useState<EditorState>(DEFAULT_EDITOR_STATE);
  const [isEditing, setIsEditing] = useState(false);
  const [isCreating, setIsCreating] = useState(false);

  // Update a single field
  const updateField = useCallback(<K extends keyof EditorState>(field: K, value: EditorState[K]) => {
    setEditorState(prev => ({ ...prev, [field]: value }));
  }, []);

  // Reset editor to default state
  const resetEditor = useCallback(() => {
    setEditorState(DEFAULT_EDITOR_STATE);
  }, []);

  // Start editing an existing note
  const startEditing = useCallback((note: Note) => {
    setEditorState({
      title: note.title,
      content: note.content,
      category: note.category,
      priority: note.priority,
      sentiment: note.sentiment,
      tags: note.tags || '',
      tickers: note.tickers || '',
      colorCode: note.color_code,
      reminderDate: note.reminder_date || ''
    });
    setSelectedNote(note);
    setIsEditing(true);
    setIsCreating(false);
  }, [setSelectedNote]);

  // Start creating a new note
  const startCreating = useCallback(() => {
    resetEditor();
    setIsCreating(true);
    setIsEditing(false);
  }, [resetEditor]);

  // Create from template
  const createFromTemplate = useCallback((template: NoteTemplate) => {
    setEditorState({
      title: `New ${template.name}`,
      content: template.content,
      category: template.category,
      priority: 'MEDIUM',
      sentiment: 'NEUTRAL',
      tags: '',
      tickers: '',
      colorCode: '#FF8800',
      reminderDate: ''
    });
    setIsCreating(true);
    setIsEditing(false);
  }, []);

  // Build note object from editor state
  const buildNoteFromEditor = useCallback((): Omit<Note, 'id'> => {
    const wordCount = editorState.content.trim().split(/\s+/).filter(w => w.length > 0).length;

    return {
      title: editorState.title,
      content: editorState.content,
      category: editorState.category,
      priority: editorState.priority,
      tags: editorState.tags,
      tickers: editorState.tickers,
      sentiment: editorState.sentiment,
      is_favorite: false,
      is_archived: false,
      color_code: editorState.colorCode,
      attachments: '[]',
      created_at: new Date().toISOString(),
      updated_at: new Date().toISOString(),
      reminder_date: editorState.reminderDate || null,
      word_count: wordCount
    };
  }, [editorState]);

  // Handle save - create or update note
  const handleSave = useCallback(async () => {
    if (!editorState.title.trim()) {
      showWarning('Please enter a title');
      return;
    }

    try {
      const wordCount = editorState.content.trim().split(/\s+/).filter(w => w.length > 0).length;

      if (isCreating) {
        // Create new note
        const newNote: Omit<Note, 'id'> = {
          title: editorState.title,
          content: editorState.content,
          category: editorState.category,
          priority: editorState.priority,
          tags: editorState.tags,
          tickers: editorState.tickers,
          sentiment: editorState.sentiment,
          is_favorite: false,
          is_archived: false,
          color_code: editorState.colorCode,
          attachments: '[]',
          created_at: new Date().toISOString(),
          updated_at: new Date().toISOString(),
          reminder_date: editorState.reminderDate || null,
          word_count: wordCount
        };

        await notesService.createNote(newNote);
        await loadNotes();
        await loadStatistics();
        await loadUpcomingReminders();

        if (editorState.reminderDate) {
          toast.success(`Note created with reminder set for ${new Date(editorState.reminderDate).toLocaleString()}`);
        }

        resetEditor();
        setIsCreating(false);
      } else if (isEditing && selectedNote) {
        // Update existing note
        await notesService.updateNote(selectedNote.id!, {
          title: editorState.title,
          content: editorState.content,
          category: editorState.category,
          priority: editorState.priority,
          tags: editorState.tags,
          tickers: editorState.tickers,
          sentiment: editorState.sentiment,
          color_code: editorState.colorCode,
          reminder_date: editorState.reminderDate || null,
          word_count: wordCount
        });

        await loadNotes();
        await loadStatistics();
        await loadUpcomingReminders();

        if (editorState.reminderDate) {
          toast.success(`Reminder updated for ${new Date(editorState.reminderDate).toLocaleString()}`);
        }

        setIsEditing(false);

        // Update selectedNote to reflect changes
        const updatedNote = await notesService.getNoteById(selectedNote.id!);
        setSelectedNote(updatedNote);
      }
    } catch (error) {
      console.error('[useNoteEditor] Failed to save note:', error);
      showError('Failed to save note');
    }
  }, [editorState, isCreating, isEditing, selectedNote, loadNotes, loadStatistics, loadUpcomingReminders, resetEditor, setSelectedNote]);

  // Handle cancel
  const handleCancel = useCallback(() => {
    setIsEditing(false);
    setIsCreating(false);
    resetEditor();
  }, [resetEditor]);

  return {
    editorState,
    isEditing,
    isCreating,
    setEditorState,
    setIsEditing,
    setIsCreating,
    updateField,
    startEditing,
    startCreating,
    createFromTemplate,
    resetEditor,
    handleSave,
    handleCancel,
    buildNoteFromEditor
  };
}
