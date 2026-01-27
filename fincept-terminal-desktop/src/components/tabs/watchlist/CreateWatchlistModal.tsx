import React, { useState } from 'react';
import { FINCEPT, FONT_FAMILY, WATCHLIST_COLORS } from './utils';

interface CreateWatchlistModalProps {
  onClose: () => void;
  onCreate: (name: string, description: string, color: string) => Promise<void>;
  existingColors: string[];
}

const CreateWatchlistModal: React.FC<CreateWatchlistModalProps> = ({ onClose, onCreate, existingColors }) => {
  const [name, setName] = useState('');
  const [description, setDescription] = useState('');
  const [selectedColor, setSelectedColor] = useState(
    WATCHLIST_COLORS.find(c => !existingColors.includes(c)) || WATCHLIST_COLORS[0]
  );
  const [loading, setLoading] = useState(false);

  const handleCreate = async () => {
    if (!name.trim()) {
      alert('Please enter a watchlist name');
      return;
    }
    setLoading(true);
    try {
      await onCreate(name.trim(), description.trim(), selectedColor);
      onClose();
    } catch (error) {
      console.error('Error creating watchlist:', error);
      alert('Failed to create watchlist');
    } finally {
      setLoading(false);
    }
  };

  return (
    <div style={{
      position: 'fixed',
      top: 0,
      left: 0,
      right: 0,
      bottom: 0,
      backgroundColor: 'rgba(0,0,0,0.85)',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      zIndex: 1000,
      fontFamily: FONT_FAMILY
    }}>
      <div style={{
        backgroundColor: FINCEPT.PANEL_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderRadius: '2px',
        minWidth: '400px',
        maxWidth: '480px',
        boxShadow: `0 4px 24px rgba(0,0,0,0.6), 0 0 1px ${FINCEPT.ORANGE}40`
      }}>
        {/* Modal Header */}
        <div style={{
          padding: '12px 16px',
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          alignItems: 'center',
          gap: '8px'
        }}>
          <div style={{
            width: '8px',
            height: '8px',
            backgroundColor: FINCEPT.ORANGE,
            borderRadius: '2px'
          }} />
          <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.ORANGE, letterSpacing: '0.5px' }}>
            CREATE NEW WATCHLIST
          </span>
        </div>

        {/* Modal Body */}
        <div style={{ padding: '16px' }}>
          {/* Name Field */}
          <div style={{ marginBottom: '12px' }}>
            <label style={{
              fontSize: '9px',
              fontWeight: 700,
              color: FINCEPT.GRAY,
              letterSpacing: '0.5px',
              display: 'block',
              marginBottom: '6px'
            }}>
              WATCHLIST NAME *
            </label>
            <input
              type="text"
              value={name}
              onChange={(e) => setName(e.target.value)}
              onKeyDown={(e) => e.key === 'Enter' && !loading && name.trim() && handleCreate()}
              style={{
                width: '100%',
                padding: '8px 10px',
                backgroundColor: FINCEPT.DARK_BG,
                color: FINCEPT.WHITE,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                fontSize: '10px',
                fontFamily: FONT_FAMILY,
                outline: 'none',
                boxSizing: 'border-box'
              }}
              onFocus={(e) => { e.currentTarget.style.borderColor = FINCEPT.ORANGE; }}
              onBlur={(e) => { e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
              placeholder="My Watchlist"
              maxLength={50}
              autoFocus
            />
          </div>

          {/* Description Field */}
          <div style={{ marginBottom: '12px' }}>
            <label style={{
              fontSize: '9px',
              fontWeight: 700,
              color: FINCEPT.GRAY,
              letterSpacing: '0.5px',
              display: 'block',
              marginBottom: '6px'
            }}>
              DESCRIPTION
            </label>
            <textarea
              value={description}
              onChange={(e) => setDescription(e.target.value)}
              style={{
                width: '100%',
                padding: '8px 10px',
                backgroundColor: FINCEPT.DARK_BG,
                color: FINCEPT.WHITE,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                fontSize: '10px',
                fontFamily: FONT_FAMILY,
                resize: 'vertical',
                minHeight: '60px',
                outline: 'none',
                boxSizing: 'border-box'
              }}
              onFocus={(e) => { e.currentTarget.style.borderColor = FINCEPT.ORANGE; }}
              onBlur={(e) => { e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
              placeholder="Optional description..."
              maxLength={200}
            />
          </div>

          {/* Color Picker */}
          <div style={{ marginBottom: '16px' }}>
            <label style={{
              fontSize: '9px',
              fontWeight: 700,
              color: FINCEPT.GRAY,
              letterSpacing: '0.5px',
              display: 'block',
              marginBottom: '8px'
            }}>
              COLOR
            </label>
            <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap' }}>
              {WATCHLIST_COLORS.map(color => (
                <button
                  key={color}
                  onClick={() => setSelectedColor(color)}
                  style={{
                    width: '28px',
                    height: '28px',
                    backgroundColor: color,
                    border: selectedColor === color
                      ? `2px solid ${FINCEPT.WHITE}`
                      : `1px solid ${FINCEPT.BORDER}`,
                    cursor: 'pointer',
                    borderRadius: '2px',
                    transition: 'all 0.2s',
                    boxShadow: selectedColor === color ? `0 0 6px ${color}60` : 'none'
                  }}
                  title={color}
                />
              ))}
            </div>
          </div>
        </div>

        {/* Modal Footer */}
        <div style={{
          padding: '12px 16px',
          borderTop: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          gap: '8px',
          justifyContent: 'flex-end'
        }}>
          <button
            onClick={onClose}
            disabled={loading}
            style={{
              padding: '6px 16px',
              backgroundColor: 'transparent',
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.GRAY,
              fontSize: '9px',
              fontWeight: 700,
              borderRadius: '2px',
              cursor: loading ? 'not-allowed' : 'pointer',
              opacity: loading ? 0.5 : 1,
              fontFamily: FONT_FAMILY,
              transition: 'all 0.2s'
            }}
          >
            CANCEL
          </button>
          <button
            onClick={handleCreate}
            disabled={loading || !name.trim()}
            style={{
              padding: '6px 16px',
              backgroundColor: (!name.trim() || loading) ? FINCEPT.MUTED : FINCEPT.ORANGE,
              color: FINCEPT.DARK_BG,
              border: 'none',
              borderRadius: '2px',
              fontSize: '9px',
              fontWeight: 700,
              cursor: (!name.trim() || loading) ? 'not-allowed' : 'pointer',
              fontFamily: FONT_FAMILY
            }}
          >
            {loading ? 'CREATING...' : 'CREATE'}
          </button>
        </div>
      </div>
    </div>
  );
};

export default CreateWatchlistModal;
