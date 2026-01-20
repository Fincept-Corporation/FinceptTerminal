// Notes Tab Note Viewer Component

import React from 'react';
import { Star, Edit3, Archive, Trash2, FileText } from 'lucide-react';
import { FINCEPT, getPriorityColor, getSentimentColor } from '../constants';
import type { Note } from '../types';

interface NoteViewerProps {
  selectedNote: Note | null;
  onToggleFavorite: (note: Note) => void;
  onStartEditing: (note: Note) => void;
  onToggleArchive: (note: Note) => void;
  onDelete: (noteId: number) => void;
}

export const NoteViewer: React.FC<NoteViewerProps> = ({
  selectedNote,
  onToggleFavorite,
  onStartEditing,
  onToggleArchive,
  onDelete
}) => {
  if (!selectedNote) {
    return (
      <div style={{
        flex: 1,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        color: FINCEPT.GRAY,
        fontSize: '11px',
        flexDirection: 'column',
        gap: '12px'
      }}>
        <FileText size={48} color={FINCEPT.MUTED} />
        <div>Select a note to view or create a new one</div>
      </div>
    );
  }

  return (
    <>
      <div style={{
        padding: '8px 12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        fontSize: '10px',
        fontWeight: 700,
        color: FINCEPT.ORANGE,
        letterSpacing: '0.5px',
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center'
      }}>
        <span>NOTE DETAILS</span>
        <div style={{ display: 'flex', gap: '4px' }}>
          <button
            onClick={() => onToggleFavorite(selectedNote)}
            style={{
              padding: '4px 8px',
              backgroundColor: 'transparent',
              border: `1px solid ${FINCEPT.BORDER}`,
              color: selectedNote.is_favorite ? FINCEPT.YELLOW : FINCEPT.GRAY,
              cursor: 'pointer',
              fontSize: '10px',
              display: 'flex',
              alignItems: 'center',
              gap: '4px'
            }}
          >
            <Star size={12} fill={selectedNote.is_favorite ? FINCEPT.YELLOW : 'none'} />
          </button>
          <button
            onClick={() => onStartEditing(selectedNote)}
            style={{
              padding: '4px 8px',
              backgroundColor: FINCEPT.BLUE,
              border: 'none',
              color: FINCEPT.WHITE,
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
            onClick={() => onToggleArchive(selectedNote)}
            style={{
              padding: '4px 8px',
              backgroundColor: FINCEPT.PURPLE,
              border: 'none',
              color: FINCEPT.WHITE,
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
            onClick={() => onDelete(selectedNote.id!)}
            style={{
              padding: '4px 8px',
              backgroundColor: FINCEPT.RED,
              border: 'none',
              color: FINCEPT.WHITE,
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
          color: FINCEPT.WHITE,
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
          backgroundColor: FINCEPT.HEADER_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          fontSize: '9px'
        }}>
          <div>
            <span style={{ color: FINCEPT.GRAY }}>CATEGORY: </span>
            <span style={{ color: FINCEPT.CYAN, fontWeight: 700 }}>{selectedNote.category}</span>
          </div>
          <div>
            <span style={{ color: FINCEPT.GRAY }}>PRIORITY: </span>
            <span style={{ color: getPriorityColor(selectedNote.priority), fontWeight: 700 }}>{selectedNote.priority}</span>
          </div>
          <div>
            <span style={{ color: FINCEPT.GRAY }}>SENTIMENT: </span>
            <span style={{ color: getSentimentColor(selectedNote.sentiment), fontWeight: 700 }}>{selectedNote.sentiment}</span>
          </div>
          <div>
            <span style={{ color: FINCEPT.GRAY }}>WORDS: </span>
            <span style={{ color: FINCEPT.WHITE, fontWeight: 700 }}>{selectedNote.word_count}</span>
          </div>
          {selectedNote.tickers && (
            <div style={{ gridColumn: '1 / -1' }}>
              <span style={{ color: FINCEPT.GRAY }}>TICKERS: </span>
              <span style={{ color: FINCEPT.CYAN, fontWeight: 700 }}>{selectedNote.tickers}</span>
            </div>
          )}
          {selectedNote.tags && (
            <div style={{ gridColumn: '1 / -1' }}>
              <span style={{ color: FINCEPT.GRAY }}>TAGS: </span>
              <span style={{ color: FINCEPT.PURPLE, fontWeight: 700 }}>{selectedNote.tags}</span>
            </div>
          )}
          <div>
            <span style={{ color: FINCEPT.GRAY }}>CREATED: </span>
            <span style={{ color: FINCEPT.WHITE }}>{new Date(selectedNote.created_at).toLocaleString()}</span>
          </div>
          <div>
            <span style={{ color: FINCEPT.GRAY }}>UPDATED: </span>
            <span style={{ color: FINCEPT.WHITE }}>{new Date(selectedNote.updated_at).toLocaleString()}</span>
          </div>
          {selectedNote.reminder_date && (
            <div style={{ gridColumn: '1 / -1' }}>
              <span style={{ color: FINCEPT.GRAY }}>REMINDER: </span>
              <span style={{ color: FINCEPT.YELLOW, fontWeight: 700 }}>{new Date(selectedNote.reminder_date).toLocaleString()}</span>
            </div>
          )}
        </div>

        {/* Content */}
        <div style={{
          color: FINCEPT.WHITE,
          fontSize: '11px',
          lineHeight: '1.8',
          whiteSpace: 'pre-wrap',
          fontFamily: '"IBM Plex Mono", monospace'
        }}>
          {selectedNote.content}
        </div>
      </div>
    </>
  );
};
