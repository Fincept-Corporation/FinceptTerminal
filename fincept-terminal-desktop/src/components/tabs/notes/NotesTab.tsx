// Notes Tab - Main Component
// Professional Fincept Terminal-Grade Financial Note-Taking Interface

import React, { useState, useEffect, useCallback } from 'react';
import { ChevronRight, ChevronDown } from 'lucide-react';
import { notesService, Note, NoteTemplate } from '../../../services/core/notesService';
import { noteReminderService } from '../../../services/core/noteReminderService.tsx';
import { toast } from 'sonner';
import { TabFooter } from '@/components/common/TabFooter';
import { useTranslation } from 'react-i18next';

import { FINCEPT } from './constants';
import type { NoteStatistics, UpcomingReminder, EditorState, FilterState } from './types';
import { useNotes } from './hooks/useNotes';
import { useNoteEditor } from './hooks/useNoteEditor';
import {
  Header,
  Toolbar,
  FiltersPanel,
  TemplatesPanel,
  RemindersPanel,
  CategorySidebar,
  NotesList,
  NoteEditor,
  NoteViewer
} from './components';

export function NotesTab() {
  const { t } = useTranslation('notes');
  const [currentTime, setCurrentTime] = useState(new Date());

  // UI State
  const [showTemplates, setShowTemplates] = useState(false);
  const [showFilters, setShowFilters] = useState(false);
  const [showReminders, setShowReminders] = useState(false);
  const [isRightPanelMinimized, setIsRightPanelMinimized] = useState(false);

  // Use custom hooks
  const {
    notes,
    filteredNotes,
    templates,
    statistics,
    selectedCategory,
    selectedNote,
    setSelectedNote,
    upcomingReminders,
    filters,
    setFilters,
    searchQuery,
    setSearchQuery,
    loadNotes,
    loadUpcomingReminders,
    loadStatistics,
    handleDeleteNote,
    handleToggleFavorite,
    handleToggleArchive,
    setSelectedCategory
  } = useNotes();

  const {
    isEditing,
    isCreating,
    editorState,
    updateField,
    startEditing,
    startCreating,
    createFromTemplate,
    handleSave,
    handleCancel
  } = useNoteEditor({
    selectedNote,
    setSelectedNote,
    loadNotes,
    loadUpcomingReminders,
    loadStatistics
  });

  // Update time
  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  // Initialize reminder service
  useEffect(() => {
    noteReminderService.start();
    loadUpcomingReminders();

    const handleOpenNote = async (event: CustomEvent) => {
      const { noteId } = event.detail;
      const note = await notesService.getNoteById(noteId);
      if (note) {
        setSelectedNote(note);
        toast.success('Note opened from reminder');
      }
    };

    window.addEventListener('openNote', handleOpenNote as unknown as EventListener);
    return () => {
      window.removeEventListener('openNote', handleOpenNote as unknown as EventListener);
    };
  }, []);

  const handleRefresh = useCallback(() => {
    loadNotes();
    loadUpcomingReminders();
  }, [loadNotes, loadUpcomingReminders]);

  const handleTemplateSelect = (template: NoteTemplate) => {
    createFromTemplate(template);
    setShowTemplates(false);
  };

  const handleStartEditing = (note: Note) => {
    startEditing(note);
  };

  const handleStartCreating = () => {
    startCreating();
  };

  return (
    <div style={{
      height: '100%',
      backgroundColor: FINCEPT.DARK_BG,
      color: FINCEPT.WHITE,
      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column'
    }}>
      <style>{`
        @import url('https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;500;600;700&display=swap');

        *::-webkit-scrollbar { width: 6px; height: 6px; }
        *::-webkit-scrollbar-track { background: ${FINCEPT.DARK_BG}; }
        *::-webkit-scrollbar-thumb { background: ${FINCEPT.BORDER}; border-radius: 3px; }
        *::-webkit-scrollbar-thumb:hover { background: ${FINCEPT.MUTED}; }

        .terminal-glow {
          text-shadow: 0 0 10px ${FINCEPT.ORANGE}40;
        }
      `}</style>

      {/* Header */}
      <Header
        statistics={statistics}
        currentTime={currentTime}
        upcomingReminders={upcomingReminders}
        showReminders={showReminders}
        onRefresh={handleRefresh}
        onToggleReminders={() => setShowReminders(!showReminders)}
      />

      {/* Toolbar */}
      <Toolbar
        searchQuery={searchQuery}
        showFavorites={filters.showFavorites}
        showArchived={filters.showArchived}
        showFilters={showFilters}
        onSearchChange={setSearchQuery}
        onToggleFavorites={() => setFilters({ ...filters, showFavorites: !filters.showFavorites })}
        onToggleArchived={() => setFilters({ ...filters, showArchived: !filters.showArchived })}
        onToggleFilters={() => setShowFilters(!showFilters)}
        onShowTemplates={() => setShowTemplates(!showTemplates)}
        onCreateNew={handleStartCreating}
      />

      {/* Filters Panel */}
      {showFilters && (
        <FiltersPanel
          filterPriority={filters.filterPriority}
          filterSentiment={filters.filterSentiment}
          onPriorityChange={(priority: string | null) => setFilters({ ...filters, filterPriority: priority })}
          onSentimentChange={(sentiment: string | null) => setFilters({ ...filters, filterSentiment: sentiment })}
          onClearFilters={() => setFilters({ ...filters, filterPriority: null, filterSentiment: null })}
        />
      )}

      {/* Templates Panel */}
      {showTemplates && (
        <TemplatesPanel
          templates={templates}
          onSelectTemplate={handleTemplateSelect}
        />
      )}

      {/* Reminders Panel */}
      {showReminders && (
        <RemindersPanel
          upcomingReminders={upcomingReminders}
          onRefresh={loadUpcomingReminders}
          onSelectNote={(note) => {
            setSelectedNote(note);
            setShowReminders(false);
          }}
          onClose={() => setShowReminders(false)}
        />
      )}

      {/* Main Content */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* Category Sidebar */}
        <CategorySidebar
          selectedCategory={selectedCategory}
          statistics={statistics}
          onSelectCategory={setSelectedCategory}
        />

        {/* Notes List */}
        <div style={{
          width: '320px',
          backgroundColor: FINCEPT.PANEL_BG,
          borderRight: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden'
        }}>
          <NotesList
            notes={filteredNotes}
            selectedNote={selectedNote}
            onSelectNote={setSelectedNote}
          />
        </div>

        {/* Right Panel - Note Content Area */}
        {!isRightPanelMinimized && (
          <div style={{
            flex: 1,
            backgroundColor: FINCEPT.PANEL_BG,
            display: 'flex',
            flexDirection: 'column',
            overflow: 'hidden'
          }}>
            {isCreating || isEditing ? (
              <NoteEditor
                isCreating={isCreating}
                editorState={editorState}
                onUpdateField={updateField}
                onSave={handleSave}
                onCancel={handleCancel}
              />
            ) : (
              <NoteViewer
                selectedNote={selectedNote}
                onToggleFavorite={handleToggleFavorite}
                onStartEditing={handleStartEditing}
                onToggleArchive={handleToggleArchive}
                onDelete={handleDeleteNote}
              />
            )}
          </div>
        )}

        {/* Minimize/Maximize Toggle for Right Panel */}
        <div style={{
          width: '20px',
          backgroundColor: FINCEPT.HEADER_BG,
          borderLeft: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          cursor: 'pointer'
        }}
          onClick={() => setIsRightPanelMinimized(!isRightPanelMinimized)}
        >
          {isRightPanelMinimized ? <ChevronRight size={14} color={FINCEPT.GRAY} /> : <ChevronDown size={14} color={FINCEPT.GRAY} />}
        </div>
      </div>

      {/* Status Bar */}
      <TabFooter
        tabName="FINANCIAL NOTES"
        leftInfo={[
          { label: 'Professional Note-Taking', color: FINCEPT.GRAY },
        ]}
        statusInfo={
          <span>
            Notes: <span style={{ color: FINCEPT.ORANGE }}>{statistics?.total || 0}</span> |
            Category: <span style={{ color: FINCEPT.CYAN }}>{selectedCategory}</span> |
            Displayed: <span style={{ color: FINCEPT.YELLOW }}>{filteredNotes.length}</span>
          </span>
        }
        backgroundColor={FINCEPT.HEADER_BG}
        borderColor={FINCEPT.BORDER}
      />
    </div>
  );
}
