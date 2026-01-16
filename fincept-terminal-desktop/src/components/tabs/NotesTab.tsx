// File: src/components/tabs/NotesTab.tsx
// Professional Bloomberg Terminal-Grade Financial Note-Taking Interface

import React, { useState, useEffect, useCallback } from 'react';
import {
  FileText, Plus, Search, Star, Archive, Tag, Calendar, TrendingUp,
  TrendingDown, AlertCircle, Edit3, Trash2, Save, X, Clock, Filter,
  BookOpen, Briefcase, PieChart, DollarSign, Globe, ChevronDown,
  ChevronRight, Copy, Download, Upload, RefreshCw, Maximize2, Minimize2, Bell
} from 'lucide-react';
import { notesService, Note, NoteTemplate } from '../../services/core/notesService';
import { noteReminderService } from '../../services/core/noteReminderService.tsx';
import { toast } from 'sonner';
import { TabFooter } from '@/components/common/TabFooter';
import { useTranslation } from 'react-i18next';

// Bloomberg Professional Color Palette
const BLOOMBERG = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
};

const CATEGORIES = [
  { id: 'ALL', label: 'ALL NOTES', icon: FileText },
  { id: 'TRADE_IDEA', label: 'TRADE IDEAS', icon: TrendingUp },
  { id: 'RESEARCH', label: 'RESEARCH', icon: BookOpen },
  { id: 'MARKET_ANALYSIS', label: 'MARKET ANALYSIS', icon: PieChart },
  { id: 'EARNINGS', label: 'EARNINGS', icon: DollarSign },
  { id: 'ECONOMIC', label: 'ECONOMIC', icon: Globe },
  { id: 'PORTFOLIO', label: 'PORTFOLIO', icon: Briefcase },
  { id: 'GENERAL', label: 'GENERAL', icon: FileText }
];

const PRIORITIES = ['HIGH', 'MEDIUM', 'LOW'];
const SENTIMENTS = ['BULLISH', 'BEARISH', 'NEUTRAL'];
const COLOR_CODES = ['#FF8800', '#00D66F', '#FF3B3B', '#00E5FF', '#FFD700', '#9D4EDD', '#0088FF'];

export function NotesTab() {
  const { t } = useTranslation('notes');
  const [currentTime, setCurrentTime] = useState(new Date());
  const [notes, setNotes] = useState<Note[]>([]);
  const [filteredNotes, setFilteredNotes] = useState<Note[]>([]);
  const [templates, setTemplates] = useState<NoteTemplate[]>([]);
  const [selectedCategory, setSelectedCategory] = useState('ALL');
  const [selectedNote, setSelectedNote] = useState<Note | null>(null);
  const [isEditing, setIsEditing] = useState(false);
  const [isCreating, setIsCreating] = useState(false);
  const [searchQuery, setSearchQuery] = useState('');
  const [showTemplates, setShowTemplates] = useState(false);
  const [showFilters, setShowFilters] = useState(false);
  const [statistics, setStatistics] = useState<any>(null);
  const [upcomingReminders, setUpcomingReminders] = useState<Array<{ note: Note; hoursUntil: number }>>([]);
  const [showReminders, setShowReminders] = useState(false);

  // Editor state
  const [editTitle, setEditTitle] = useState('');
  const [editContent, setEditContent] = useState('');
  const [editCategory, setEditCategory] = useState('GENERAL');
  const [editPriority, setEditPriority] = useState('MEDIUM');
  const [editSentiment, setEditSentiment] = useState('NEUTRAL');
  const [editTags, setEditTags] = useState('');
  const [editTickers, setEditTickers] = useState('');
  const [editColorCode, setEditColorCode] = useState('#FF8800');
  const [editReminderDate, setEditReminderDate] = useState('');

  // Filters
  const [filterPriority, setFilterPriority] = useState<string | null>(null);
  const [filterSentiment, setFilterSentiment] = useState<string | null>(null);
  const [showFavorites, setShowFavorites] = useState(false);
  const [showArchived, setShowArchived] = useState(false);

  // UI State
  const [isRightPanelMinimized, setIsRightPanelMinimized] = useState(false);

  // Update time
  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  // Initialize reminder service
  useEffect(() => {
    noteReminderService.start();
    loadUpcomingReminders();

    // Listen for openNote events from reminder notifications
    const handleOpenNote = async (event: CustomEvent) => {
      const { noteId } = event.detail;
      const note = await notesService.getNoteById(noteId);
      if (note) {
        setSelectedNote(note);
        setIsEditing(false);
        setIsCreating(false);
        toast.success('Note opened from reminder');
      }
    };

    window.addEventListener('openNote', handleOpenNote as unknown as EventListener);

    return () => {
      window.removeEventListener('openNote', handleOpenNote as unknown as EventListener);
    };
  }, []);

  // Load upcoming reminders
  const loadUpcomingReminders = useCallback(async () => {
    try {
      const upcoming = await noteReminderService.getUpcomingReminders();
      setUpcomingReminders(upcoming);
    } catch (error) {
      console.error('[NotesTab] Failed to load upcoming reminders:', error);
    }
  }, []);

  // Load notes
  const loadNotes = useCallback(async () => {
    try {
      let loadedNotes: Note[];
      if (showFavorites) {
        loadedNotes = await notesService.getFavoriteNotes();
      } else if (selectedCategory === 'ALL') {
        loadedNotes = await notesService.getAllNotes(showArchived);
      } else {
        loadedNotes = await notesService.getNotesByCategory(selectedCategory, showArchived);
      }
      setNotes(loadedNotes);
      applyFilters(loadedNotes);
    } catch (error) {
      console.error('[NotesTab] Failed to load notes:', error);
    }
  }, [selectedCategory, showFavorites, showArchived]);

  // Load templates
  const loadTemplates = useCallback(async () => {
    try {
      const loadedTemplates = await notesService.getTemplates();
      setTemplates(loadedTemplates);
    } catch (error) {
      console.error('[NotesTab] Failed to load templates:', error);
    }
  }, []);

  // Load statistics
  const loadStatistics = useCallback(async () => {
    try {
      const stats = await notesService.getStatistics();
      setStatistics(stats);
    } catch (error) {
      console.error('[NotesTab] Failed to load statistics:', error);
    }
  }, []);

  // Apply filters
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

    if (filterPriority) {
      filtered = filtered.filter(note => note.priority === filterPriority);
    }

    if (filterSentiment) {
      filtered = filtered.filter(note => note.sentiment === filterSentiment);
    }

    setFilteredNotes(filtered);
  }, [searchQuery, filterPriority, filterSentiment]);

  useEffect(() => {
    loadNotes();
    loadTemplates();
    loadStatistics();
  }, [loadNotes, loadTemplates, loadStatistics]);

  useEffect(() => {
    applyFilters(notes);
  }, [notes, applyFilters]);

  // Create new note
  const handleCreateNote = async () => {
    if (!editTitle.trim()) {
      alert('Please enter a title');
      return;
    }

    try {
      const wordCount = editContent.trim().split(/\s+/).filter(w => w.length > 0).length;
      const newNote: Omit<Note, 'id'> = {
        title: editTitle,
        content: editContent,
        category: editCategory,
        priority: editPriority,
        tags: editTags,
        tickers: editTickers,
        sentiment: editSentiment,
        is_favorite: false,
        is_archived: false,
        color_code: editColorCode,
        attachments: '[]',
        created_at: new Date().toISOString(),
        updated_at: new Date().toISOString(),
        reminder_date: editReminderDate || null,
        word_count: wordCount
      };

      await notesService.createNote(newNote);
      await loadNotes();
      await loadStatistics();
      await loadUpcomingReminders();
      if (editReminderDate) {
        toast.success(`Note created with reminder set for ${new Date(editReminderDate).toLocaleString()}`);
      }
      resetEditor();
      setIsCreating(false);
    } catch (error) {
      console.error('[NotesTab] Failed to create note:', error);
      alert('Failed to create note');
    }
  };

  // Update note
  const handleUpdateNote = async () => {
    if (!selectedNote) return;

    try {
      const wordCount = editContent.trim().split(/\s+/).filter(w => w.length > 0).length;
      await notesService.updateNote(selectedNote.id!, {
        title: editTitle,
        content: editContent,
        category: editCategory,
        priority: editPriority,
        tags: editTags,
        tickers: editTickers,
        sentiment: editSentiment,
        color_code: editColorCode,
        reminder_date: editReminderDate || null,
        word_count: wordCount
      });

      await loadNotes();
      await loadStatistics();
      await loadUpcomingReminders();
      if (editReminderDate) {
        toast.success(`Reminder updated for ${new Date(editReminderDate).toLocaleString()}`);
      }
      setIsEditing(false);

      // Update selectedNote to reflect changes
      const updatedNote = await notesService.getNoteById(selectedNote.id!);
      setSelectedNote(updatedNote);
    } catch (error) {
      console.error('[NotesTab] Failed to update note:', error);
      alert('Failed to update note');
    }
  };

  // Delete note
  const handleDeleteNote = async (id: number) => {
    if (!confirm('Are you sure you want to delete this note?')) return;

    try {
      await notesService.deleteNote(id);
      await loadNotes();
      await loadStatistics();
      if (selectedNote?.id === id) {
        setSelectedNote(null);
      }
    } catch (error) {
      console.error('[NotesTab] Failed to delete note:', error);
      alert('Failed to delete note');
    }
  };

  // Toggle favorite
  const handleToggleFavorite = async (note: Note) => {
    try {
      await notesService.updateNote(note.id!, {
        is_favorite: !note.is_favorite
      });
      await loadNotes();
    } catch (error) {
      console.error('[NotesTab] Failed to toggle favorite:', error);
    }
  };

  // Toggle archive
  const handleToggleArchive = async (note: Note) => {
    try {
      await notesService.updateNote(note.id!, {
        is_archived: !note.is_archived
      });
      await loadNotes();
      await loadStatistics();
      if (selectedNote?.id === note.id) {
        setSelectedNote(null);
      }
    } catch (error) {
      console.error('[NotesTab] Failed to toggle archive:', error);
    }
  };

  // Start editing
  const startEditing = (note: Note) => {
    setSelectedNote(note);
    setEditTitle(note.title);
    setEditContent(note.content);
    setEditCategory(note.category);
    setEditPriority(note.priority);
    setEditSentiment(note.sentiment);
    setEditTags(note.tags || '');
    setEditTickers(note.tickers || '');
    setEditColorCode(note.color_code);
    setEditReminderDate(note.reminder_date || '');
    setIsEditing(true);
  };

  // Start creating from template
  const createFromTemplate = (template: NoteTemplate) => {
    setEditTitle(`New ${template.name}`);
    setEditContent(template.content);
    setEditCategory(template.category);
    setEditPriority('MEDIUM');
    setEditSentiment('NEUTRAL');
    setEditTags('');
    setEditTickers('');
    setEditColorCode('#FF8800');
    setEditReminderDate('');
    setIsCreating(true);
    setShowTemplates(false);
  };

  // Reset editor
  const resetEditor = () => {
    setEditTitle('');
    setEditContent('');
    setEditCategory('GENERAL');
    setEditPriority('MEDIUM');
    setEditSentiment('NEUTRAL');
    setEditTags('');
    setEditTickers('');
    setEditColorCode('#FF8800');
    setEditReminderDate('');
  };

  const getPriorityColor = (priority: string) => {
    switch (priority) {
      case 'HIGH': return BLOOMBERG.RED;
      case 'MEDIUM': return BLOOMBERG.YELLOW;
      case 'LOW': return BLOOMBERG.GREEN;
      default: return BLOOMBERG.GRAY;
    }
  };

  const getSentimentColor = (sentiment: string) => {
    switch (sentiment) {
      case 'BULLISH': return BLOOMBERG.GREEN;
      case 'BEARISH': return BLOOMBERG.RED;
      case 'NEUTRAL': return BLOOMBERG.YELLOW;
      default: return BLOOMBERG.GRAY;
    }
  };

  return (
    <div style={{
      height: '100%',
      backgroundColor: BLOOMBERG.DARK_BG,
      color: BLOOMBERG.WHITE,
      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column'
    }}>
      <style>{`
        @import url('https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;500;600;700&display=swap');

        *::-webkit-scrollbar { width: 6px; height: 6px; }
        *::-webkit-scrollbar-track { background: ${BLOOMBERG.DARK_BG}; }
        *::-webkit-scrollbar-thumb { background: ${BLOOMBERG.BORDER}; border-radius: 3px; }
        *::-webkit-scrollbar-thumb:hover { background: ${BLOOMBERG.MUTED}; }

        .terminal-glow {
          text-shadow: 0 0 10px ${BLOOMBERG.ORANGE}40;
        }
      `}</style>

      {/* ========== TOP NAVIGATION BAR ========== */}
      <div style={{
        backgroundColor: BLOOMBERG.HEADER_BG,
        borderBottom: `2px solid ${BLOOMBERG.ORANGE}`,
        padding: '6px 12px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        flexShrink: 0,
        boxShadow: `0 2px 8px ${BLOOMBERG.ORANGE}20`
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <FileText size={18} color={BLOOMBERG.ORANGE} style={{ filter: 'drop-shadow(0 0 4px ' + BLOOMBERG.ORANGE + ')' }} />
            <span style={{
              color: BLOOMBERG.ORANGE,
              fontWeight: 700,
              fontSize: '14px',
              letterSpacing: '0.5px',
              textShadow: `0 0 10px ${BLOOMBERG.ORANGE}40`
            }}>
              {t('title')}
            </span>
          </div>

          <div style={{ height: '16px', width: '1px', backgroundColor: BLOOMBERG.BORDER }} />

          {statistics && (
            <>
              <div style={{ fontSize: '10px', color: BLOOMBERG.CYAN }}>
                {t('header.total')}: <span style={{ fontWeight: 700 }}>{statistics.total}</span>
              </div>
              <div style={{ fontSize: '10px', color: BLOOMBERG.YELLOW }}>
                {t('header.favorites')}: <span style={{ fontWeight: 700 }}>{statistics.favorites}</span>
              </div>
              <div style={{ fontSize: '10px', color: BLOOMBERG.PURPLE }}>
                {t('header.words')}: <span style={{ fontWeight: 700 }}>{statistics.totalWords.toLocaleString()}</span>
              </div>
            </>
          )}
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '4px', fontSize: '10px', color: BLOOMBERG.CYAN }}>
            <Clock size={12} />
            {currentTime.toLocaleTimeString('en-US', { hour12: false })}
          </div>

          <button
            onClick={() => {
              loadNotes();
              loadUpcomingReminders();
            }}
            style={{
              padding: '4px 8px',
              backgroundColor: 'transparent',
              border: `1px solid ${BLOOMBERG.BORDER}`,
              color: BLOOMBERG.GRAY,
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              fontSize: '10px',
              transition: 'all 0.2s'
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.borderColor = BLOOMBERG.ORANGE;
              e.currentTarget.style.color = BLOOMBERG.ORANGE;
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.borderColor = BLOOMBERG.BORDER;
              e.currentTarget.style.color = BLOOMBERG.GRAY;
            }}
          >
            <RefreshCw size={12} />
            {t('header.refresh')}
          </button>

          <button
            onClick={() => setShowReminders(!showReminders)}
            style={{
              padding: '4px 8px',
              backgroundColor: showReminders ? BLOOMBERG.YELLOW : 'transparent',
              border: `1px solid ${upcomingReminders.length > 0 ? BLOOMBERG.YELLOW : BLOOMBERG.BORDER}`,
              color: showReminders ? BLOOMBERG.DARK_BG : upcomingReminders.length > 0 ? BLOOMBERG.YELLOW : BLOOMBERG.GRAY,
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              fontSize: '10px',
              fontWeight: 700,
              transition: 'all 0.2s'
            }}
            title="View upcoming reminders"
          >
            <Bell size={12} />
            {upcomingReminders.length > 0 && `(${upcomingReminders.length})`}
          </button>
        </div>
      </div>

      {/* ========== TOOLBAR ========== */}
      <div style={{
        backgroundColor: BLOOMBERG.PANEL_BG,
        borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
        padding: '8px 12px',
        display: 'flex',
        alignItems: 'center',
        gap: '12px',
        flexShrink: 0
      }}>
        {/* Search */}
        <div style={{ position: 'relative', flex: 1, maxWidth: '400px' }}>
          <Search size={14} style={{ position: 'absolute', left: '8px', top: '50%', transform: 'translateY(-50%)', color: BLOOMBERG.GRAY }} />
          <input
            type="text"
            placeholder={t('toolbar.search')}
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            style={{
              width: '100%',
              padding: '6px 8px 6px 32px',
              backgroundColor: BLOOMBERG.HEADER_BG,
              border: `1px solid ${BLOOMBERG.BORDER}`,
              color: BLOOMBERG.WHITE,
              fontSize: '11px',
              outline: 'none'
            }}
          />
        </div>

        {/* Action Buttons */}
        <button
          onClick={() => {
            resetEditor();
            setIsCreating(true);
          }}
          style={{
            padding: '6px 12px',
            backgroundColor: BLOOMBERG.GREEN,
            border: 'none',
            color: BLOOMBERG.DARK_BG,
            cursor: 'pointer',
            fontSize: '10px',
            fontWeight: 700,
            display: 'flex',
            alignItems: 'center',
            gap: '4px'
          }}
        >
          <Plus size={14} />
          {t('toolbar.newNote')}
        </button>

        <button
          onClick={() => setShowTemplates(!showTemplates)}
          style={{
            padding: '6px 12px',
            backgroundColor: showTemplates ? BLOOMBERG.ORANGE : BLOOMBERG.PANEL_BG,
            border: `1px solid ${BLOOMBERG.BORDER}`,
            color: showTemplates ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
            cursor: 'pointer',
            fontSize: '10px',
            fontWeight: 700,
            display: 'flex',
            alignItems: 'center',
            gap: '4px'
          }}
        >
          <BookOpen size={14} />
          {t('toolbar.templates')}
        </button>

        <button
          onClick={() => setShowFavorites(!showFavorites)}
          style={{
            padding: '6px 12px',
            backgroundColor: showFavorites ? BLOOMBERG.YELLOW : BLOOMBERG.PANEL_BG,
            border: `1px solid ${BLOOMBERG.BORDER}`,
            color: showFavorites ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
            cursor: 'pointer',
            fontSize: '10px',
            fontWeight: 700,
            display: 'flex',
            alignItems: 'center',
            gap: '4px'
          }}
        >
          <Star size={14} fill={showFavorites ? BLOOMBERG.DARK_BG : 'none'} />
          {t('toolbar.favorites')}
        </button>

        <button
          onClick={() => setShowFilters(!showFilters)}
          style={{
            padding: '6px 12px',
            backgroundColor: showFilters ? BLOOMBERG.CYAN : BLOOMBERG.PANEL_BG,
            border: `1px solid ${BLOOMBERG.BORDER}`,
            color: showFilters ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
            cursor: 'pointer',
            fontSize: '10px',
            fontWeight: 700,
            display: 'flex',
            alignItems: 'center',
            gap: '4px'
          }}
        >
          <Filter size={14} />
          {t('toolbar.filters')}
        </button>

        <button
          onClick={() => setShowArchived(!showArchived)}
          style={{
            padding: '6px 12px',
            backgroundColor: showArchived ? BLOOMBERG.PURPLE : BLOOMBERG.PANEL_BG,
            border: `1px solid ${BLOOMBERG.BORDER}`,
            color: showArchived ? BLOOMBERG.WHITE : BLOOMBERG.GRAY,
            cursor: 'pointer',
            fontSize: '10px',
            fontWeight: 700,
            display: 'flex',
            alignItems: 'center',
            gap: '4px'
          }}
        >
          <Archive size={14} />
          {t('toolbar.archived')}
        </button>
      </div>

      {/* ========== FILTERS PANEL ========== */}
      {showFilters && (
        <div style={{
          backgroundColor: BLOOMBERG.PANEL_BG,
          borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
          padding: '8px 12px',
          display: 'flex',
          alignItems: 'center',
          gap: '16px',
          flexShrink: 0,
          fontSize: '10px'
        }}>
          <span style={{ color: BLOOMBERG.ORANGE, fontWeight: 700 }}>FILTERS:</span>

          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
            <span style={{ color: BLOOMBERG.GRAY }}>Priority:</span>
            {PRIORITIES.map(priority => (
              <button
                key={priority}
                onClick={() => setFilterPriority(filterPriority === priority ? null : priority)}
                style={{
                  padding: '4px 8px',
                  backgroundColor: filterPriority === priority ? getPriorityColor(priority) : BLOOMBERG.HEADER_BG,
                  border: `1px solid ${BLOOMBERG.BORDER}`,
                  color: filterPriority === priority ? BLOOMBERG.DARK_BG : getPriorityColor(priority),
                  cursor: 'pointer',
                  fontSize: '9px',
                  fontWeight: 700
                }}
              >
                {priority}
              </button>
            ))}
          </div>

          <div style={{ height: '16px', width: '1px', backgroundColor: BLOOMBERG.BORDER }} />

          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
            <span style={{ color: BLOOMBERG.GRAY }}>Sentiment:</span>
            {SENTIMENTS.map(sentiment => (
              <button
                key={sentiment}
                onClick={() => setFilterSentiment(filterSentiment === sentiment ? null : sentiment)}
                style={{
                  padding: '4px 8px',
                  backgroundColor: filterSentiment === sentiment ? getSentimentColor(sentiment) : BLOOMBERG.HEADER_BG,
                  border: `1px solid ${BLOOMBERG.BORDER}`,
                  color: filterSentiment === sentiment ? BLOOMBERG.DARK_BG : getSentimentColor(sentiment),
                  cursor: 'pointer',
                  fontSize: '9px',
                  fontWeight: 700
                }}
              >
                {sentiment}
              </button>
            ))}
          </div>

          <button
            onClick={() => {
              setFilterPriority(null);
              setFilterSentiment(null);
            }}
            style={{
              marginLeft: 'auto',
              padding: '4px 8px',
              backgroundColor: BLOOMBERG.RED,
              border: 'none',
              color: BLOOMBERG.WHITE,
              cursor: 'pointer',
              fontSize: '9px',
              fontWeight: 700
            }}
          >
            CLEAR FILTERS
          </button>
        </div>
      )}

      {/* ========== TEMPLATES PANEL ========== */}
      {showTemplates && (
        <div style={{
          backgroundColor: BLOOMBERG.PANEL_BG,
          borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
          padding: '12px',
          flexShrink: 0
        }}>
          <div style={{ color: BLOOMBERG.ORANGE, fontSize: '11px', fontWeight: 700, marginBottom: '8px' }}>
            SELECT TEMPLATE
          </div>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(200px, 1fr))', gap: '8px' }}>
            {templates.map(template => (
              <div
                key={template.id}
                onClick={() => createFromTemplate(template)}
                style={{
                  padding: '12px',
                  backgroundColor: BLOOMBERG.HEADER_BG,
                  border: `1px solid ${BLOOMBERG.BORDER}`,
                  cursor: 'pointer',
                  transition: 'all 0.2s'
                }}
                onMouseEnter={(e) => {
                  e.currentTarget.style.borderColor = BLOOMBERG.ORANGE;
                  e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER;
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.borderColor = BLOOMBERG.BORDER;
                  e.currentTarget.style.backgroundColor = BLOOMBERG.HEADER_BG;
                }}
              >
                <div style={{ color: BLOOMBERG.CYAN, fontSize: '11px', fontWeight: 700, marginBottom: '4px' }}>
                  {template.name}
                </div>
                <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', lineHeight: '1.4' }}>
                  {template.description}
                </div>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* ========== UPCOMING REMINDERS PANEL ========== */}
      {showReminders && (
        <div style={{
          backgroundColor: BLOOMBERG.PANEL_BG,
          borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
          padding: '12px',
          flexShrink: 0
        }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px' }}>
            <div style={{ color: BLOOMBERG.YELLOW, fontSize: '11px', fontWeight: 700, display: 'flex', alignItems: 'center', gap: '6px' }}>
              <Bell size={14} />
              UPCOMING REMINDERS (24H)
            </div>
            <button
              onClick={async () => {
                await loadUpcomingReminders();
                toast.success('Reminders refreshed');
              }}
              style={{
                padding: '4px 8px',
                backgroundColor: BLOOMBERG.HEADER_BG,
                border: `1px solid ${BLOOMBERG.BORDER}`,
                color: BLOOMBERG.GRAY,
                cursor: 'pointer',
                fontSize: '9px',
                fontWeight: 700,
                display: 'flex',
                alignItems: 'center',
                gap: '4px'
              }}
            >
              <RefreshCw size={10} />
              REFRESH
            </button>
          </div>
          {upcomingReminders.length === 0 ? (
            <div style={{
              padding: '20px',
              textAlign: 'center',
              color: BLOOMBERG.GRAY,
              fontSize: '10px',
              backgroundColor: BLOOMBERG.HEADER_BG,
              border: `1px solid ${BLOOMBERG.BORDER}`
            }}>
              No upcoming reminders in the next 24 hours
            </div>
          ) : (
            <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
              {upcomingReminders.map(({ note, hoursUntil }) => {
                const isUrgent = hoursUntil < 1;
                const minutesUntil = Math.round(hoursUntil * 60);
                const timeText = hoursUntil < 1
                  ? `${minutesUntil} minute${minutesUntil !== 1 ? 's' : ''}`
                  : `${Math.round(hoursUntil)} hour${Math.round(hoursUntil) !== 1 ? 's' : ''}`;

                return (
                  <div
                    key={note.id}
                    onClick={() => {
                      setSelectedNote(note);
                      setIsEditing(false);
                      setIsCreating(false);
                      setShowReminders(false);
                    }}
                    style={{
                      padding: '10px',
                      backgroundColor: isUrgent ? `${BLOOMBERG.RED}10` : BLOOMBERG.HEADER_BG,
                      border: `1px solid ${isUrgent ? BLOOMBERG.RED : BLOOMBERG.BORDER}`,
                      borderLeft: `4px solid ${note.color_code}`,
                      cursor: 'pointer',
                      transition: 'all 0.2s'
                    }}
                    onMouseEnter={(e) => {
                      e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER;
                    }}
                    onMouseLeave={(e) => {
                      e.currentTarget.style.backgroundColor = isUrgent ? `${BLOOMBERG.RED}10` : BLOOMBERG.HEADER_BG;
                    }}
                  >
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '4px' }}>
                      <div style={{ color: BLOOMBERG.WHITE, fontSize: '11px', fontWeight: 700 }}>
                        {note.title}
                      </div>
                      <div style={{
                        padding: '2px 6px',
                        backgroundColor: isUrgent ? BLOOMBERG.RED : BLOOMBERG.YELLOW,
                        color: BLOOMBERG.DARK_BG,
                        fontSize: '8px',
                        fontWeight: 700
                      }}>
                        {isUrgent ? '🔴 URGENT' : ` ${timeText.toUpperCase()}`}
                      </div>
                    </div>
                    <div style={{ display: 'flex', gap: '8px', fontSize: '9px', color: BLOOMBERG.GRAY }}>
                      <span style={{ color: getPriorityColor(note.priority) }}>{note.priority}</span>
                      <span>•</span>
                      <span style={{ color: getSentimentColor(note.sentiment) }}>{note.sentiment}</span>
                      <span>•</span>
                      <span>{note.category}</span>
                      {note.tickers && (
                        <>
                          <span>•</span>
                          <span style={{ color: BLOOMBERG.CYAN }}>{note.tickers}</span>
                        </>
                      )}
                    </div>
                    <div style={{ fontSize: '8px', color: BLOOMBERG.MUTED, marginTop: '4px' }}>
                      Due: {new Date(note.reminder_date!).toLocaleString()}
                    </div>
                  </div>
                );
              })}
            </div>
          )}
        </div>
      )}

      {/* ========== MAIN CONTENT ========== */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* LEFT SIDEBAR - Categories */}
        <div style={{
          width: '220px',
          backgroundColor: BLOOMBERG.PANEL_BG,
          borderRight: `1px solid ${BLOOMBERG.BORDER}`,
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden'
        }}>
          <div style={{
            padding: '8px 12px',
            backgroundColor: BLOOMBERG.HEADER_BG,
            borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
            fontSize: '10px',
            fontWeight: 700,
            color: BLOOMBERG.ORANGE,
            letterSpacing: '0.5px'
          }}>
            CATEGORIES
          </div>

          <div style={{ flex: 1, overflow: 'auto' }}>
            {CATEGORIES.map(category => {
              const Icon = category.icon;
              const count = statistics?.byCategory?.find((s: any) => s.category === category.id)?.count || 0;

              return (
                <div
                  key={category.id}
                  onClick={() => {
                    setSelectedCategory(category.id);
                    setShowFavorites(false);
                  }}
                  style={{
                    padding: '10px 12px',
                    cursor: 'pointer',
                    backgroundColor: selectedCategory === category.id ? `${BLOOMBERG.ORANGE}20` : 'transparent',
                    borderLeft: selectedCategory === category.id ? `3px solid ${BLOOMBERG.ORANGE}` : '3px solid transparent',
                    transition: 'all 0.2s',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'space-between'
                  }}
                  onMouseEnter={(e) => {
                    if (selectedCategory !== category.id) {
                      e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER;
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (selectedCategory !== category.id) {
                      e.currentTarget.style.backgroundColor = 'transparent';
                    }
                  }}
                >
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                    <Icon size={14} color={selectedCategory === category.id ? BLOOMBERG.ORANGE : BLOOMBERG.GRAY} />
                    <span style={{
                      fontSize: '10px',
                      fontWeight: 600,
                      color: selectedCategory === category.id ? BLOOMBERG.ORANGE : BLOOMBERG.WHITE
                    }}>
                      {category.label}
                    </span>
                  </div>
                  {category.id !== 'ALL' && (
                    <span style={{
                      fontSize: '9px',
                      color: BLOOMBERG.CYAN,
                      fontWeight: 700
                    }}>
                      {count}
                    </span>
                  )}
                </div>
              );
            })}
          </div>
        </div>

        {/* CENTER - Notes List */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
          <div style={{
            padding: '8px 12px',
            backgroundColor: BLOOMBERG.HEADER_BG,
            borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
            fontSize: '10px',
            fontWeight: 700,
            color: BLOOMBERG.ORANGE,
            letterSpacing: '0.5px',
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center'
          }}>
            <span>NOTES ({filteredNotes.length})</span>
            <span style={{ fontSize: '9px', color: BLOOMBERG.GRAY }}>
              SORTED BY: RECENTLY UPDATED
            </span>
          </div>

          <div style={{ flex: 1, overflow: 'auto', padding: '8px' }}>
            {filteredNotes.length === 0 ? (
              <div style={{
                padding: '40px',
                textAlign: 'center',
                color: BLOOMBERG.GRAY,
                fontSize: '11px'
              }}>
                {searchQuery ? 'No notes found matching your search' : 'No notes yet. Create your first note!'}
              </div>
            ) : (
              filteredNotes.map(note => (
                <div
                  key={note.id}
                  onClick={() => {
                    setSelectedNote(note);
                    setIsEditing(false);
                    setIsCreating(false);
                  }}
                  style={{
                    marginBottom: '8px',
                    padding: '12px',
                    backgroundColor: selectedNote?.id === note.id ? `${BLOOMBERG.ORANGE}15` : BLOOMBERG.HEADER_BG,
                    border: `1px solid ${BLOOMBERG.BORDER}`,
                    borderLeft: `4px solid ${note.color_code}`,
                    cursor: 'pointer',
                    transition: 'all 0.2s'
                  }}
                  onMouseEnter={(e) => {
                    if (selectedNote?.id !== note.id) {
                      e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER;
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (selectedNote?.id !== note.id) {
                      e.currentTarget.style.backgroundColor = BLOOMBERG.HEADER_BG;
                    }
                  }}
                >
                  <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '6px' }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '6px', flex: 1 }}>
                      <span style={{ color: BLOOMBERG.WHITE, fontSize: '11px', fontWeight: 700 }}>
                        {note.title}
                      </span>
                      {note.is_favorite && <Star size={12} fill={BLOOMBERG.YELLOW} color={BLOOMBERG.YELLOW} />}
                    </div>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                      <span style={{
                        padding: '2px 6px',
                        backgroundColor: getPriorityColor(note.priority),
                        color: BLOOMBERG.DARK_BG,
                        fontSize: '8px',
                        fontWeight: 700
                      }}>
                        {note.priority}
                      </span>
                      <span style={{
                        padding: '2px 6px',
                        backgroundColor: getSentimentColor(note.sentiment),
                        color: BLOOMBERG.DARK_BG,
                        fontSize: '8px',
                        fontWeight: 700
                      }}>
                        {note.sentiment}
                      </span>
                    </div>
                  </div>

                  <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', lineHeight: '1.4', marginBottom: '6px' }}>
                    {note.content.substring(0, 120)}{note.content.length > 120 ? '...' : ''}
                  </div>

                  <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', fontSize: '8px' }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '8px', color: BLOOMBERG.MUTED }}>
                      <span>{new Date(note.updated_at).toLocaleDateString()}</span>
                      {note.word_count > 0 && <span>{note.word_count} words</span>}
                      {note.tickers && <span style={{ color: BLOOMBERG.CYAN }}>{note.tickers}</span>}
                    </div>
                    {note.tags && (
                      <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                        <Tag size={10} color={BLOOMBERG.PURPLE} />
                        <span style={{ color: BLOOMBERG.PURPLE }}>{note.tags.split(',').slice(0, 2).join(', ')}</span>
                      </div>
                    )}
                  </div>
                </div>
              ))
            )}
          </div>
        </div>

        {/* RIGHT PANEL - Note Viewer/Editor */}
        {!isRightPanelMinimized && (
          <div style={{
            width: '500px',
            backgroundColor: BLOOMBERG.PANEL_BG,
            borderLeft: `1px solid ${BLOOMBERG.BORDER}`,
            display: 'flex',
            flexDirection: 'column',
            overflow: 'hidden'
          }}>
            {(isEditing || isCreating) ? (
              // EDITOR MODE
              <>
                <div style={{
                  padding: '8px 12px',
                  backgroundColor: BLOOMBERG.HEADER_BG,
                  borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
                  fontSize: '10px',
                  fontWeight: 700,
                  color: BLOOMBERG.ORANGE,
                  letterSpacing: '0.5px',
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'center'
                }}>
                  <span>{isCreating ? 'CREATE NOTE' : 'EDIT NOTE'}</span>
                  <div style={{ display: 'flex', gap: '4px' }}>
                    <button
                      onClick={isCreating ? handleCreateNote : handleUpdateNote}
                      style={{
                        padding: '4px 8px',
                        backgroundColor: BLOOMBERG.GREEN,
                        border: 'none',
                        color: BLOOMBERG.DARK_BG,
                        cursor: 'pointer',
                        fontSize: '10px',
                        fontWeight: 700,
                        display: 'flex',
                        alignItems: 'center',
                        gap: '4px'
                      }}
                    >
                      <Save size={12} />
                      SAVE
                    </button>
                    <button
                      onClick={() => {
                        setIsEditing(false);
                        setIsCreating(false);
                        resetEditor();
                      }}
                      style={{
                        padding: '4px 8px',
                        backgroundColor: BLOOMBERG.RED,
                        border: 'none',
                        color: BLOOMBERG.WHITE,
                        cursor: 'pointer',
                        fontSize: '10px',
                        fontWeight: 700,
                        display: 'flex',
                        alignItems: 'center',
                        gap: '4px'
                      }}
                    >
                      <X size={12} />
                      CANCEL
                    </button>
                  </div>
                </div>

                <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
                  {/* Title */}
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ color: BLOOMBERG.GRAY, fontSize: '9px', fontWeight: 700, marginBottom: '4px', display: 'block' }}>
                      TITLE *
                    </label>
                    <input
                      type="text"
                      value={editTitle}
                      onChange={(e) => setEditTitle(e.target.value)}
                      style={{
                        width: '100%',
                        padding: '8px',
                        backgroundColor: BLOOMBERG.HEADER_BG,
                        border: `1px solid ${BLOOMBERG.BORDER}`,
                        color: BLOOMBERG.WHITE,
                        fontSize: '12px',
                        fontWeight: 700,
                        outline: 'none'
                      }}
                      placeholder="Enter note title..."
                    />
                  </div>

                  {/* Metadata Row 1 */}
                  <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px', marginBottom: '12px' }}>
                    <div>
                      <label style={{ color: BLOOMBERG.GRAY, fontSize: '9px', fontWeight: 700, marginBottom: '4px', display: 'block' }}>
                        CATEGORY
                      </label>
                      <select
                        value={editCategory}
                        onChange={(e) => setEditCategory(e.target.value)}
                        style={{
                          width: '100%',
                          padding: '6px',
                          backgroundColor: BLOOMBERG.HEADER_BG,
                          border: `1px solid ${BLOOMBERG.BORDER}`,
                          color: BLOOMBERG.WHITE,
                          fontSize: '10px',
                          outline: 'none'
                        }}
                      >
                        {CATEGORIES.filter(c => c.id !== 'ALL').map(cat => (
                          <option key={cat.id} value={cat.id}>{cat.label}</option>
                        ))}
                      </select>
                    </div>

                    <div>
                      <label style={{ color: BLOOMBERG.GRAY, fontSize: '9px', fontWeight: 700, marginBottom: '4px', display: 'block' }}>
                        PRIORITY
                      </label>
                      <select
                        value={editPriority}
                        onChange={(e) => setEditPriority(e.target.value)}
                        style={{
                          width: '100%',
                          padding: '6px',
                          backgroundColor: BLOOMBERG.HEADER_BG,
                          border: `1px solid ${BLOOMBERG.BORDER}`,
                          color: BLOOMBERG.WHITE,
                          fontSize: '10px',
                          outline: 'none'
                        }}
                      >
                        {PRIORITIES.map(p => (
                          <option key={p} value={p}>{p}</option>
                        ))}
                      </select>
                    </div>
                  </div>

                  {/* Metadata Row 2 */}
                  <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px', marginBottom: '12px' }}>
                    <div>
                      <label style={{ color: BLOOMBERG.GRAY, fontSize: '9px', fontWeight: 700, marginBottom: '4px', display: 'block' }}>
                        SENTIMENT
                      </label>
                      <select
                        value={editSentiment}
                        onChange={(e) => setEditSentiment(e.target.value)}
                        style={{
                          width: '100%',
                          padding: '6px',
                          backgroundColor: BLOOMBERG.HEADER_BG,
                          border: `1px solid ${BLOOMBERG.BORDER}`,
                          color: BLOOMBERG.WHITE,
                          fontSize: '10px',
                          outline: 'none'
                        }}
                      >
                        {SENTIMENTS.map(s => (
                          <option key={s} value={s}>{s}</option>
                        ))}
                      </select>
                    </div>

                    <div>
                      <label style={{ color: BLOOMBERG.GRAY, fontSize: '9px', fontWeight: 700, marginBottom: '4px', display: 'block' }}>
                        COLOR
                      </label>
                      <div style={{ display: 'flex', gap: '4px' }}>
                        {COLOR_CODES.map(color => (
                          <div
                            key={color}
                            onClick={() => setEditColorCode(color)}
                            style={{
                              width: '24px',
                              height: '24px',
                              backgroundColor: color,
                              border: editColorCode === color ? `2px solid ${BLOOMBERG.WHITE}` : `1px solid ${BLOOMBERG.BORDER}`,
                              cursor: 'pointer'
                            }}
                          />
                        ))}
                      </div>
                    </div>
                  </div>

                  {/* Tags & Tickers */}
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ color: BLOOMBERG.GRAY, fontSize: '9px', fontWeight: 700, marginBottom: '4px', display: 'block' }}>
                      TAGS (comma-separated)
                    </label>
                    <input
                      type="text"
                      value={editTags}
                      onChange={(e) => setEditTags(e.target.value)}
                      style={{
                        width: '100%',
                        padding: '6px',
                        backgroundColor: BLOOMBERG.HEADER_BG,
                        border: `1px solid ${BLOOMBERG.BORDER}`,
                        color: BLOOMBERG.WHITE,
                        fontSize: '10px',
                        outline: 'none'
                      }}
                      placeholder="growth, tech, semiconductor"
                    />
                  </div>

                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ color: BLOOMBERG.GRAY, fontSize: '9px', fontWeight: 700, marginBottom: '4px', display: 'block' }}>
                      TICKERS (comma-separated)
                    </label>
                    <input
                      type="text"
                      value={editTickers}
                      onChange={(e) => setEditTickers(e.target.value)}
                      style={{
                        width: '100%',
                        padding: '6px',
                        backgroundColor: BLOOMBERG.HEADER_BG,
                        border: `1px solid ${BLOOMBERG.BORDER}`,
                        color: BLOOMBERG.WHITE,
                        fontSize: '10px',
                        outline: 'none'
                      }}
                      placeholder="AAPL, MSFT, NVDA"
                    />
                  </div>

                  {/* Reminder Date */}
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ color: BLOOMBERG.GRAY, fontSize: '9px', fontWeight: 700, marginBottom: '4px', display: 'block' }}>
                      REMINDER DATE
                    </label>
                    <input
                      type="datetime-local"
                      value={editReminderDate}
                      onChange={(e) => setEditReminderDate(e.target.value)}
                      style={{
                        width: '100%',
                        padding: '6px',
                        backgroundColor: BLOOMBERG.HEADER_BG,
                        border: `1px solid ${BLOOMBERG.BORDER}`,
                        color: BLOOMBERG.WHITE,
                        fontSize: '10px',
                        outline: 'none'
                      }}
                    />
                  </div>

                  {/* Content */}
                  <div>
                    <label style={{ color: BLOOMBERG.GRAY, fontSize: '9px', fontWeight: 700, marginBottom: '4px', display: 'block' }}>
                      CONTENT *
                    </label>
                    <textarea
                      value={editContent}
                      onChange={(e) => setEditContent(e.target.value)}
                      style={{
                        width: '100%',
                        height: '400px',
                        padding: '8px',
                        backgroundColor: BLOOMBERG.HEADER_BG,
                        border: `1px solid ${BLOOMBERG.BORDER}`,
                        color: BLOOMBERG.WHITE,
                        fontSize: '11px',
                        lineHeight: '1.6',
                        outline: 'none',
                        resize: 'vertical',
                        fontFamily: '"IBM Plex Mono", monospace'
                      }}
                      placeholder="Write your note content here... Supports Markdown formatting."
                    />
                  </div>
                </div>
              </>
            ) : selectedNote ? (
              // VIEWER MODE
              <>
                <div style={{
                  padding: '8px 12px',
                  backgroundColor: BLOOMBERG.HEADER_BG,
                  borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
                  fontSize: '10px',
                  fontWeight: 700,
                  color: BLOOMBERG.ORANGE,
                  letterSpacing: '0.5px',
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'center'
                }}>
                  <span>NOTE DETAILS</span>
                  <div style={{ display: 'flex', gap: '4px' }}>
                    <button
                      onClick={() => handleToggleFavorite(selectedNote)}
                      style={{
                        padding: '4px 8px',
                        backgroundColor: 'transparent',
                        border: `1px solid ${BLOOMBERG.BORDER}`,
                        color: selectedNote.is_favorite ? BLOOMBERG.YELLOW : BLOOMBERG.GRAY,
                        cursor: 'pointer',
                        fontSize: '10px',
                        display: 'flex',
                        alignItems: 'center',
                        gap: '4px'
                      }}
                    >
                      <Star size={12} fill={selectedNote.is_favorite ? BLOOMBERG.YELLOW : 'none'} />
                    </button>
                    <button
                      onClick={() => startEditing(selectedNote)}
                      style={{
                        padding: '4px 8px',
                        backgroundColor: BLOOMBERG.BLUE,
                        border: 'none',
                        color: BLOOMBERG.WHITE,
                        cursor: 'pointer',
                        fontSize: '10px',
                        fontWeight: 700,
                        display: 'flex',
                        alignItems: 'center',
                        gap: '4px'
                      }}
                    >
                      <Edit3 size={12} />
                      EDIT
                    </button>
                    <button
                      onClick={() => handleToggleArchive(selectedNote)}
                      style={{
                        padding: '4px 8px',
                        backgroundColor: BLOOMBERG.PURPLE,
                        border: 'none',
                        color: BLOOMBERG.WHITE,
                        cursor: 'pointer',
                        fontSize: '10px',
                        fontWeight: 700,
                        display: 'flex',
                        alignItems: 'center',
                        gap: '4px'
                      }}
                    >
                      <Archive size={12} />
                      {selectedNote.is_archived ? 'UNARCHIVE' : 'ARCHIVE'}
                    </button>
                    <button
                      onClick={() => handleDeleteNote(selectedNote.id!)}
                      style={{
                        padding: '4px 8px',
                        backgroundColor: BLOOMBERG.RED,
                        border: 'none',
                        color: BLOOMBERG.WHITE,
                        cursor: 'pointer',
                        fontSize: '10px',
                        fontWeight: 700,
                        display: 'flex',
                        alignItems: 'center',
                        gap: '4px'
                      }}
                    >
                      <Trash2 size={12} />
                      DELETE
                    </button>
                  </div>
                </div>

                <div style={{ flex: 1, overflow: 'auto', padding: '16px' }}>
                  {/* Title */}
                  <div style={{
                    color: BLOOMBERG.WHITE,
                    fontSize: '16px',
                    fontWeight: 700,
                    marginBottom: '12px',
                    paddingBottom: '12px',
                    borderBottom: `2px solid ${selectedNote.color_code}`
                  }}>
                    {selectedNote.title}
                  </div>

                  {/* Metadata */}
                  <div style={{
                    display: 'grid',
                    gridTemplateColumns: '1fr 1fr',
                    gap: '8px',
                    marginBottom: '16px',
                    padding: '12px',
                    backgroundColor: BLOOMBERG.HEADER_BG,
                    border: `1px solid ${BLOOMBERG.BORDER}`,
                    fontSize: '9px'
                  }}>
                    <div>
                      <span style={{ color: BLOOMBERG.GRAY }}>CATEGORY: </span>
                      <span style={{ color: BLOOMBERG.CYAN, fontWeight: 700 }}>{selectedNote.category}</span>
                    </div>
                    <div>
                      <span style={{ color: BLOOMBERG.GRAY }}>PRIORITY: </span>
                      <span style={{ color: getPriorityColor(selectedNote.priority), fontWeight: 700 }}>{selectedNote.priority}</span>
                    </div>
                    <div>
                      <span style={{ color: BLOOMBERG.GRAY }}>SENTIMENT: </span>
                      <span style={{ color: getSentimentColor(selectedNote.sentiment), fontWeight: 700 }}>{selectedNote.sentiment}</span>
                    </div>
                    <div>
                      <span style={{ color: BLOOMBERG.GRAY }}>WORDS: </span>
                      <span style={{ color: BLOOMBERG.WHITE, fontWeight: 700 }}>{selectedNote.word_count}</span>
                    </div>
                    {selectedNote.tickers && (
                      <div style={{ gridColumn: '1 / -1' }}>
                        <span style={{ color: BLOOMBERG.GRAY }}>TICKERS: </span>
                        <span style={{ color: BLOOMBERG.CYAN, fontWeight: 700 }}>{selectedNote.tickers}</span>
                      </div>
                    )}
                    {selectedNote.tags && (
                      <div style={{ gridColumn: '1 / -1' }}>
                        <span style={{ color: BLOOMBERG.GRAY }}>TAGS: </span>
                        <span style={{ color: BLOOMBERG.PURPLE, fontWeight: 700 }}>{selectedNote.tags}</span>
                      </div>
                    )}
                    <div>
                      <span style={{ color: BLOOMBERG.GRAY }}>CREATED: </span>
                      <span style={{ color: BLOOMBERG.WHITE }}>{new Date(selectedNote.created_at).toLocaleString()}</span>
                    </div>
                    <div>
                      <span style={{ color: BLOOMBERG.GRAY }}>UPDATED: </span>
                      <span style={{ color: BLOOMBERG.WHITE }}>{new Date(selectedNote.updated_at).toLocaleString()}</span>
                    </div>
                    {selectedNote.reminder_date && (
                      <div style={{ gridColumn: '1 / -1' }}>
                        <span style={{ color: BLOOMBERG.GRAY }}>REMINDER: </span>
                        <span style={{ color: BLOOMBERG.YELLOW, fontWeight: 700 }}>{new Date(selectedNote.reminder_date).toLocaleString()}</span>
                      </div>
                    )}
                  </div>

                  {/* Content */}
                  <div style={{
                    color: BLOOMBERG.WHITE,
                    fontSize: '11px',
                    lineHeight: '1.8',
                    whiteSpace: 'pre-wrap',
                    fontFamily: '"IBM Plex Mono", monospace'
                  }}>
                    {selectedNote.content}
                  </div>
                </div>
              </>
            ) : (
              // NO SELECTION
              <div style={{
                flex: 1,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                color: BLOOMBERG.GRAY,
                fontSize: '11px',
                flexDirection: 'column',
                gap: '12px'
              }}>
                <FileText size={48} color={BLOOMBERG.MUTED} />
                <div>Select a note to view or create a new one</div>
              </div>
            )}
          </div>
        )}

        {/* Minimize/Maximize Toggle for Right Panel */}
        <div style={{
          width: '20px',
          backgroundColor: BLOOMBERG.HEADER_BG,
          borderLeft: `1px solid ${BLOOMBERG.BORDER}`,
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          cursor: 'pointer'
        }}
          onClick={() => setIsRightPanelMinimized(!isRightPanelMinimized)}
        >
          {isRightPanelMinimized ? <ChevronRight size={14} color={BLOOMBERG.GRAY} /> : <ChevronDown size={14} color={BLOOMBERG.GRAY} />}
        </div>
      </div>

      {/* ========== STATUS BAR ========== */}
      <TabFooter
        tabName="FINANCIAL NOTES"
        leftInfo={[
          { label: 'Professional Note-Taking', color: BLOOMBERG.GRAY },
        ]}
        statusInfo={
          <span>
            Notes: <span style={{ color: BLOOMBERG.ORANGE }}>{statistics?.total || 0}</span> |
            Category: <span style={{ color: BLOOMBERG.CYAN }}>{selectedCategory}</span> |
            Displayed: <span style={{ color: BLOOMBERG.YELLOW }}>{filteredNotes.length}</span>
          </span>
        }
        backgroundColor={BLOOMBERG.HEADER_BG}
        borderColor={BLOOMBERG.BORDER}
      />
    </div>
  );
}
