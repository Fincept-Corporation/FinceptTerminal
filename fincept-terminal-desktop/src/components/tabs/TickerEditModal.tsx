// Ticker Edit Modal - UI for managing tickers in a category/region
import React, { useState, useEffect } from 'react';
import { X, Plus, Trash2, GripVertical, ChevronUp, ChevronDown } from 'lucide-react';

interface TickerEditModalProps {
  isOpen: boolean;
  onClose: () => void;
  title: string;
  tickers: string[];
  onSave: (tickers: string[]) => void;
}

const TickerEditModal: React.FC<TickerEditModalProps> = ({
  isOpen,
  onClose,
  title,
  tickers,
  onSave
}) => {
  const [editedTickers, setEditedTickers] = useState<string[]>([...tickers]);
  const [newTicker, setNewTicker] = useState('');

  // Update editedTickers when tickers prop changes
  useEffect(() => {
    setEditedTickers([...tickers]);
  }, [tickers]);

  if (!isOpen) return null;

  const handleAdd = () => {
    const trimmed = newTicker.trim().toUpperCase();
    if (trimmed && !editedTickers.includes(trimmed)) {
      setEditedTickers([...editedTickers, trimmed]);
      setNewTicker('');
    }
  };

  const handleRemove = (index: number) => {
    setEditedTickers(editedTickers.filter((_, i) => i !== index));
  };

  const handleMoveUp = (index: number) => {
    if (index === 0) return;
    const newList = [...editedTickers];
    [newList[index - 1], newList[index]] = [newList[index], newList[index - 1]];
    setEditedTickers(newList);
  };

  const handleMoveDown = (index: number) => {
    if (index === editedTickers.length - 1) return;
    const newList = [...editedTickers];
    [newList[index], newList[index + 1]] = [newList[index + 1], newList[index]];
    setEditedTickers(newList);
  };

  const handleSave = () => {
    onSave(editedTickers);
    onClose();
  };

  const BLOOMBERG_ORANGE = '#FFA500';
  const BLOOMBERG_WHITE = '#FFFFFF';
  const BLOOMBERG_GRAY = '#787878';
  const BLOOMBERG_DARK_BG = '#1a1a1a';
  const BLOOMBERG_PANEL_BG = '#000000';

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
      zIndex: 9999
    }}>
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        border: `2px solid ${BLOOMBERG_ORANGE}`,
        width: '90%',
        maxWidth: '600px',
        maxHeight: '80vh',
        display: 'flex',
        flexDirection: 'column',
        fontFamily: 'Consolas, monospace'
      }}>
        {/* Header */}
        <div style={{
          backgroundColor: BLOOMBERG_DARK_BG,
          color: BLOOMBERG_ORANGE,
          padding: '12px 16px',
          fontSize: '14px',
          fontWeight: 'bold',
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          borderBottom: `1px solid ${BLOOMBERG_GRAY}`
        }}>
          <span>EDIT {title.toUpperCase()}</span>
          <X
            size={20}
            style={{ cursor: 'pointer', color: BLOOMBERG_WHITE }}
            onClick={onClose}
          />
        </div>

        {/* Add Ticker Section */}
        <div style={{
          padding: '16px',
          borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
          backgroundColor: BLOOMBERG_DARK_BG
        }}>
          <div style={{ display: 'flex', gap: '8px' }}>
            <input
              type="text"
              value={newTicker}
              onChange={(e) => setNewTicker(e.target.value)}
              onKeyPress={(e) => e.key === 'Enter' && handleAdd()}
              placeholder="Enter ticker symbol (e.g., AAPL)"
              style={{
                flex: 1,
                backgroundColor: BLOOMBERG_PANEL_BG,
                border: `1px solid ${BLOOMBERG_GRAY}`,
                color: BLOOMBERG_WHITE,
                padding: '8px 12px',
                fontSize: '12px',
                fontFamily: 'Consolas, monospace'
              }}
            />
            <button
              onClick={handleAdd}
              style={{
                backgroundColor: BLOOMBERG_ORANGE,
                border: 'none',
                color: 'black',
                padding: '8px 16px',
                fontSize: '11px',
                fontWeight: 'bold',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '4px'
              }}
            >
              <Plus size={14} /> ADD
            </button>
          </div>
        </div>

        {/* Ticker List */}
        <div style={{
          flex: 1,
          overflow: 'auto',
          padding: '8px'
        }}>
          {editedTickers.length === 0 ? (
            <div style={{
              color: BLOOMBERG_GRAY,
              fontSize: '12px',
              textAlign: 'center',
              padding: '32px'
            }}>
              No tickers added. Add tickers using the form above.
            </div>
          ) : (
            editedTickers.map((ticker, index) => (
              <div
                key={index}
                style={{
                  backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.05)' : 'transparent',
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  padding: '8px 12px',
                  marginBottom: '4px',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '12px',
                  fontSize: '12px'
                }}
              >
                {/* Position number */}
                <div style={{
                  color: BLOOMBERG_GRAY,
                  fontSize: '10px',
                  minWidth: '20px',
                  textAlign: 'center'
                }}>
                  #{index + 1}
                </div>

                {/* Reorder buttons */}
                <div style={{
                  display: 'flex',
                  flexDirection: 'column',
                  gap: '2px'
                }}>
                  <button
                    onClick={() => handleMoveUp(index)}
                    disabled={index === 0}
                    style={{
                      backgroundColor: index === 0 ? 'transparent' : BLOOMBERG_DARK_BG,
                      border: `1px solid ${BLOOMBERG_GRAY}`,
                      color: index === 0 ? BLOOMBERG_GRAY : BLOOMBERG_ORANGE,
                      cursor: index === 0 ? 'not-allowed' : 'pointer',
                      padding: '2px',
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center',
                      opacity: index === 0 ? 0.3 : 1
                    }}
                    title="Move up"
                  >
                    <ChevronUp size={12} />
                  </button>
                  <button
                    onClick={() => handleMoveDown(index)}
                    disabled={index === editedTickers.length - 1}
                    style={{
                      backgroundColor: index === editedTickers.length - 1 ? 'transparent' : BLOOMBERG_DARK_BG,
                      border: `1px solid ${BLOOMBERG_GRAY}`,
                      color: index === editedTickers.length - 1 ? BLOOMBERG_GRAY : BLOOMBERG_ORANGE,
                      cursor: index === editedTickers.length - 1 ? 'not-allowed' : 'pointer',
                      padding: '2px',
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center',
                      opacity: index === editedTickers.length - 1 ? 0.3 : 1
                    }}
                    title="Move down"
                  >
                    <ChevronDown size={12} />
                  </button>
                </div>

                {/* Ticker symbol */}
                <span style={{
                  color: BLOOMBERG_WHITE,
                  flex: 1,
                  fontWeight: 'bold'
                }}>{ticker}</span>

                {/* Delete button */}
                <button
                  onClick={() => handleRemove(index)}
                  style={{
                    backgroundColor: 'transparent',
                    border: `1px solid ${BLOOMBERG_GRAY}`,
                    color: '#FF0000',
                    cursor: 'pointer',
                    padding: '4px 8px',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '4px',
                    fontSize: '10px'
                  }}
                  title="Remove ticker"
                >
                  <Trash2 size={12} />
                  REMOVE
                </button>
              </div>
            ))
          )}
        </div>

        {/* Footer */}
        <div style={{
          padding: '16px',
          borderTop: `1px solid ${BLOOMBERG_GRAY}`,
          display: 'flex',
          justifyContent: 'flex-end',
          gap: '8px',
          backgroundColor: BLOOMBERG_DARK_BG
        }}>
          <button
            onClick={onClose}
            style={{
              backgroundColor: BLOOMBERG_GRAY,
              border: 'none',
              color: 'black',
              padding: '8px 16px',
              fontSize: '11px',
              fontWeight: 'bold',
              cursor: 'pointer'
            }}
          >
            CANCEL
          </button>
          <button
            onClick={handleSave}
            style={{
              backgroundColor: BLOOMBERG_ORANGE,
              border: 'none',
              color: 'black',
              padding: '8px 16px',
              fontSize: '11px',
              fontWeight: 'bold',
              cursor: 'pointer'
            }}
          >
            SAVE CHANGES
          </button>
        </div>
      </div>
    </div>
  );
};

export default TickerEditModal;
