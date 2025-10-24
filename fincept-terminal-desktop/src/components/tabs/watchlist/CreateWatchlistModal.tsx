import React, { useState } from 'react';
import { BLOOMBERG_COLORS, WATCHLIST_COLORS } from './utils';

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
      backgroundColor: 'rgba(0,0,0,0.8)',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      zIndex: 1000
    }}>
      <div style={{
        backgroundColor: BLOOMBERG_COLORS.DARK_BG,
        border: `2px solid ${BLOOMBERG_COLORS.ORANGE}`,
        padding: '24px',
        minWidth: '400px',
        maxWidth: '500px'
      }}>
        <div style={{
          color: BLOOMBERG_COLORS.ORANGE,
          fontSize: '14px',
          fontWeight: 'bold',
          marginBottom: '16px'
        }}>
          CREATE NEW WATCHLIST
        </div>

        <div style={{ marginBottom: '12px' }}>
          <label style={{
            color: BLOOMBERG_COLORS.GRAY,
            fontSize: '10px',
            display: 'block',
            marginBottom: '4px'
          }}>
            WATCHLIST NAME *
          </label>
          <input
            type="text"
            value={name}
            onChange={(e) => setName(e.target.value)}
            style={{
              width: '100%',
              background: BLOOMBERG_COLORS.PANEL_BG,
              border: `1px solid ${BLOOMBERG_COLORS.GRAY}`,
              color: BLOOMBERG_COLORS.WHITE,
              padding: '8px',
              fontSize: '11px',
              fontFamily: 'Consolas, monospace'
            }}
            placeholder="My Watchlist"
            maxLength={50}
          />
        </div>

        <div style={{ marginBottom: '12px' }}>
          <label style={{
            color: BLOOMBERG_COLORS.GRAY,
            fontSize: '10px',
            display: 'block',
            marginBottom: '4px'
          }}>
            DESCRIPTION
          </label>
          <textarea
            value={description}
            onChange={(e) => setDescription(e.target.value)}
            style={{
              width: '100%',
              background: BLOOMBERG_COLORS.PANEL_BG,
              border: `1px solid ${BLOOMBERG_COLORS.GRAY}`,
              color: BLOOMBERG_COLORS.WHITE,
              padding: '8px',
              fontSize: '11px',
              fontFamily: 'Consolas, monospace',
              resize: 'vertical',
              minHeight: '60px'
            }}
            placeholder="Optional description..."
            maxLength={200}
          />
        </div>

        <div style={{ marginBottom: '16px' }}>
          <label style={{
            color: BLOOMBERG_COLORS.GRAY,
            fontSize: '10px',
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
                  width: '32px',
                  height: '32px',
                  backgroundColor: color,
                  border: selectedColor === color
                    ? `3px solid ${BLOOMBERG_COLORS.WHITE}`
                    : `1px solid ${BLOOMBERG_COLORS.GRAY}`,
                  cursor: 'pointer',
                  borderRadius: '2px'
                }}
                title={color}
              />
            ))}
          </div>
        </div>

        <div style={{ display: 'flex', gap: '8px', justifyContent: 'flex-end' }}>
          <button
            onClick={onClose}
            disabled={loading}
            style={{
              background: BLOOMBERG_COLORS.GRAY,
              color: 'black',
              border: 'none',
              padding: '8px 16px',
              fontSize: '10px',
              fontWeight: 'bold',
              cursor: loading ? 'not-allowed' : 'pointer',
              opacity: loading ? 0.5 : 1
            }}
          >
            CANCEL
          </button>
          <button
            onClick={handleCreate}
            disabled={loading || !name.trim()}
            style={{
              background: (!name.trim() || loading) ? BLOOMBERG_COLORS.GRAY : BLOOMBERG_COLORS.ORANGE,
              color: 'black',
              border: 'none',
              padding: '8px 16px',
              fontSize: '10px',
              fontWeight: 'bold',
              cursor: (!name.trim() || loading) ? 'not-allowed' : 'pointer'
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
