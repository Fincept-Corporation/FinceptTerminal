// Notes Tab Note Editor Component

import React from 'react';
import { Save, X } from 'lucide-react';
import { FINCEPT, CATEGORIES, PRIORITIES, SENTIMENTS, COLOR_CODES } from '../constants';
import type { EditorState } from '../types';

interface NoteEditorProps {
  isCreating: boolean;
  editorState: EditorState;
  onUpdateField: <K extends keyof EditorState>(field: K, value: EditorState[K]) => void;
  onSave: () => void;
  onCancel: () => void;
}

export const NoteEditor: React.FC<NoteEditorProps> = ({
  isCreating,
  editorState,
  onUpdateField,
  onSave,
  onCancel
}) => {
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
        <span>{isCreating ? 'CREATE NOTE' : 'EDIT NOTE'}</span>
        <div style={{ display: 'flex', gap: '4px' }}>
          <button
            onClick={onSave}
            style={{
              padding: '4px 8px',
              backgroundColor: FINCEPT.GREEN,
              border: 'none',
              color: FINCEPT.DARK_BG,
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
            onClick={onCancel}
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
            <X size={12} />
            CANCEL
          </button>
        </div>
      </div>

      <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
        {/* Title */}
        <div style={{ marginBottom: '12px' }}>
          <label style={{ color: FINCEPT.GRAY, fontSize: '9px', fontWeight: 700, marginBottom: '4px', display: 'block' }}>
            TITLE *
          </label>
          <input
            type="text"
            value={editorState.title}
            onChange={(e) => onUpdateField('title', e.target.value)}
            style={{
              width: '100%',
              padding: '8px',
              backgroundColor: FINCEPT.HEADER_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.WHITE,
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
            <label style={{ color: FINCEPT.GRAY, fontSize: '9px', fontWeight: 700, marginBottom: '4px', display: 'block' }}>
              CATEGORY
            </label>
            <select
              value={editorState.category}
              onChange={(e) => onUpdateField('category', e.target.value)}
              style={{
                width: '100%',
                padding: '6px',
                backgroundColor: FINCEPT.HEADER_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                color: FINCEPT.WHITE,
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
            <label style={{ color: FINCEPT.GRAY, fontSize: '9px', fontWeight: 700, marginBottom: '4px', display: 'block' }}>
              PRIORITY
            </label>
            <select
              value={editorState.priority}
              onChange={(e) => onUpdateField('priority', e.target.value)}
              style={{
                width: '100%',
                padding: '6px',
                backgroundColor: FINCEPT.HEADER_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                color: FINCEPT.WHITE,
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
            <label style={{ color: FINCEPT.GRAY, fontSize: '9px', fontWeight: 700, marginBottom: '4px', display: 'block' }}>
              SENTIMENT
            </label>
            <select
              value={editorState.sentiment}
              onChange={(e) => onUpdateField('sentiment', e.target.value)}
              style={{
                width: '100%',
                padding: '6px',
                backgroundColor: FINCEPT.HEADER_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                color: FINCEPT.WHITE,
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
            <label style={{ color: FINCEPT.GRAY, fontSize: '9px', fontWeight: 700, marginBottom: '4px', display: 'block' }}>
              COLOR
            </label>
            <div style={{ display: 'flex', gap: '4px' }}>
              {COLOR_CODES.map(color => (
                <div
                  key={color}
                  onClick={() => onUpdateField('colorCode', color)}
                  style={{
                    width: '24px',
                    height: '24px',
                    backgroundColor: color,
                    border: editorState.colorCode === color ? `2px solid ${FINCEPT.WHITE}` : `1px solid ${FINCEPT.BORDER}`,
                    cursor: 'pointer'
                  }}
                />
              ))}
            </div>
          </div>
        </div>

        {/* Tags */}
        <div style={{ marginBottom: '12px' }}>
          <label style={{ color: FINCEPT.GRAY, fontSize: '9px', fontWeight: 700, marginBottom: '4px', display: 'block' }}>
            TAGS (comma-separated)
          </label>
          <input
            type="text"
            value={editorState.tags}
            onChange={(e) => onUpdateField('tags', e.target.value)}
            style={{
              width: '100%',
              padding: '6px',
              backgroundColor: FINCEPT.HEADER_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.WHITE,
              fontSize: '10px',
              outline: 'none'
            }}
            placeholder="growth, tech, semiconductor"
          />
        </div>

        {/* Tickers */}
        <div style={{ marginBottom: '12px' }}>
          <label style={{ color: FINCEPT.GRAY, fontSize: '9px', fontWeight: 700, marginBottom: '4px', display: 'block' }}>
            TICKERS (comma-separated)
          </label>
          <input
            type="text"
            value={editorState.tickers}
            onChange={(e) => onUpdateField('tickers', e.target.value)}
            style={{
              width: '100%',
              padding: '6px',
              backgroundColor: FINCEPT.HEADER_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.WHITE,
              fontSize: '10px',
              outline: 'none'
            }}
            placeholder="AAPL, MSFT, NVDA"
          />
        </div>

        {/* Reminder Date */}
        <div style={{ marginBottom: '12px' }}>
          <label style={{ color: FINCEPT.GRAY, fontSize: '9px', fontWeight: 700, marginBottom: '4px', display: 'block' }}>
            REMINDER DATE
          </label>
          <input
            type="datetime-local"
            value={editorState.reminderDate}
            onChange={(e) => onUpdateField('reminderDate', e.target.value)}
            style={{
              width: '100%',
              padding: '6px',
              backgroundColor: FINCEPT.HEADER_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.WHITE,
              fontSize: '10px',
              outline: 'none'
            }}
          />
        </div>

        {/* Content */}
        <div>
          <label style={{ color: FINCEPT.GRAY, fontSize: '9px', fontWeight: 700, marginBottom: '4px', display: 'block' }}>
            CONTENT *
          </label>
          <textarea
            value={editorState.content}
            onChange={(e) => onUpdateField('content', e.target.value)}
            style={{
              width: '100%',
              height: '400px',
              padding: '8px',
              backgroundColor: FINCEPT.HEADER_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.WHITE,
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
  );
};
