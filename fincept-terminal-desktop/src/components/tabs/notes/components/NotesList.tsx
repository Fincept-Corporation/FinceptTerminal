// Notes Tab Notes List Component

import React from 'react';
import { Star, Tag } from 'lucide-react';
import { FINCEPT, getPriorityColor, getSentimentColor } from '../constants';
import type { Note } from '../types';

interface NotesListProps {
  notes: Note[];
  selectedNote: Note | null;
  onSelectNote: (note: Note) => void;
}

export const NotesList: React.FC<NotesListProps> = ({
  notes,
  selectedNote,
  onSelectNote
}) => {
  return (
    <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
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
        <span>NOTES ({notes.length})</span>
        <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
          SORTED BY: RECENTLY UPDATED
        </span>
      </div>

      <div style={{ flex: 1, overflow: 'auto', padding: '8px' }}>
        {notes.length === 0 ? (
          <div style={{
            padding: '40px',
            textAlign: 'center',
            color: FINCEPT.GRAY,
            fontSize: '11px'
          }}>
            No notes found. Create your first note!
          </div>
        ) : (
          notes.map(note => (
            <NoteItem
              key={note.id}
              note={note}
              isSelected={selectedNote?.id === note.id}
              onSelect={() => onSelectNote(note)}
            />
          ))
        )}
      </div>
    </div>
  );
};

interface NoteItemProps {
  note: Note;
  isSelected: boolean;
  onSelect: () => void;
}

const NoteItem: React.FC<NoteItemProps> = ({ note, isSelected, onSelect }) => {
  return (
    <div
      onClick={onSelect}
      style={{
        marginBottom: '8px',
        padding: '12px',
        backgroundColor: isSelected ? `${FINCEPT.ORANGE}15` : FINCEPT.HEADER_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderLeft: `4px solid ${note.color_code}`,
        cursor: 'pointer',
        transition: 'all 0.2s'
      }}
      onMouseEnter={(e) => {
        if (!isSelected) {
          e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
        }
      }}
      onMouseLeave={(e) => {
        if (!isSelected) {
          e.currentTarget.style.backgroundColor = FINCEPT.HEADER_BG;
        }
      }}
    >
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '6px' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px', flex: 1 }}>
          <span style={{ color: FINCEPT.WHITE, fontSize: '11px', fontWeight: 700 }}>
            {note.title}
          </span>
          {note.is_favorite && <Star size={12} fill={FINCEPT.YELLOW} color={FINCEPT.YELLOW} />}
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
          <span style={{
            padding: '2px 6px',
            backgroundColor: getPriorityColor(note.priority),
            color: FINCEPT.DARK_BG,
            fontSize: '8px',
            fontWeight: 700
          }}>
            {note.priority}
          </span>
          <span style={{
            padding: '2px 6px',
            backgroundColor: getSentimentColor(note.sentiment),
            color: FINCEPT.DARK_BG,
            fontSize: '8px',
            fontWeight: 700
          }}>
            {note.sentiment}
          </span>
        </div>
      </div>

      <div style={{ color: FINCEPT.GRAY, fontSize: '9px', lineHeight: '1.4', marginBottom: '6px' }}>
        {note.content.substring(0, 120)}{note.content.length > 120 ? '...' : ''}
      </div>

      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', fontSize: '8px' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', color: FINCEPT.MUTED }}>
          <span>{new Date(note.updated_at).toLocaleDateString()}</span>
          {note.word_count > 0 && <span>{note.word_count} words</span>}
          {note.tickers && <span style={{ color: FINCEPT.CYAN }}>{note.tickers}</span>}
        </div>
        {note.tags && (
          <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
            <Tag size={10} color={FINCEPT.PURPLE} />
            <span style={{ color: FINCEPT.PURPLE }}>{note.tags.split(',').slice(0, 2).join(', ')}</span>
          </div>
        )}
      </div>
    </div>
  );
};
