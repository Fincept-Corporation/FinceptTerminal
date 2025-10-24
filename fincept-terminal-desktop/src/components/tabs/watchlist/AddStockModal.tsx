import React, { useState } from 'react';
import { BLOOMBERG_COLORS } from './utils';

interface AddStockModalProps {
  onClose: () => void;
  onAdd: (symbol: string, notes: string) => Promise<void>;
  existingSymbols: string[];
}

const AddStockModal: React.FC<AddStockModalProps> = ({ onClose, onAdd, existingSymbols }) => {
  const [symbol, setSymbol] = useState('');
  const [notes, setNotes] = useState('');
  const [loading, setLoading] = useState(false);

  const handleAdd = async () => {
    const trimmedSymbol = symbol.trim().toUpperCase();

    if (!trimmedSymbol) {
      alert('Please enter a stock symbol');
      return;
    }

    if (existingSymbols.includes(trimmedSymbol)) {
      alert(`${trimmedSymbol} is already in this watchlist`);
      return;
    }

    setLoading(true);
    try {
      await onAdd(trimmedSymbol, notes.trim());
      onClose();
    } catch (error) {
      console.error('Error adding stock:', error);
      alert(error instanceof Error ? error.message : 'Failed to add stock');
    } finally {
      setLoading(false);
    }
  };

  const handleKeyPress = (e: React.KeyboardEvent) => {
    if (e.key === 'Enter' && !loading && symbol.trim()) {
      handleAdd();
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
        border: `2px solid ${BLOOMBERG_COLORS.GREEN}`,
        padding: '24px',
        minWidth: '400px'
      }}>
        <div style={{
          color: BLOOMBERG_COLORS.GREEN,
          fontSize: '14px',
          fontWeight: 'bold',
          marginBottom: '16px'
        }}>
          ADD STOCK TO WATCHLIST
        </div>

        <div style={{ marginBottom: '12px' }}>
          <label style={{
            color: BLOOMBERG_COLORS.GRAY,
            fontSize: '10px',
            display: 'block',
            marginBottom: '4px'
          }}>
            SYMBOL *
          </label>
          <input
            type="text"
            value={symbol}
            onChange={(e) => setSymbol(e.target.value.toUpperCase())}
            onKeyPress={handleKeyPress}
            style={{
              width: '100%',
              background: BLOOMBERG_COLORS.PANEL_BG,
              border: `1px solid ${BLOOMBERG_COLORS.GRAY}`,
              color: BLOOMBERG_COLORS.WHITE,
              padding: '8px',
              fontSize: '14px',
              fontFamily: 'Consolas, monospace',
              textTransform: 'uppercase',
              fontWeight: 'bold',
              letterSpacing: '1px'
            }}
            placeholder="AAPL"
            maxLength={10}
            autoFocus
          />
          <div style={{
            color: BLOOMBERG_COLORS.GRAY,
            fontSize: '9px',
            marginTop: '4px'
          }}>
            Enter stock ticker symbol (e.g., AAPL, MSFT, GOOGL)
          </div>
        </div>

        <div style={{ marginBottom: '16px' }}>
          <label style={{
            color: BLOOMBERG_COLORS.GRAY,
            fontSize: '10px',
            display: 'block',
            marginBottom: '4px'
          }}>
            NOTES (OPTIONAL)
          </label>
          <textarea
            value={notes}
            onChange={(e) => setNotes(e.target.value)}
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
            placeholder="Add notes about this stock..."
            maxLength={200}
          />
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
            onClick={handleAdd}
            disabled={loading || !symbol.trim()}
            style={{
              background: (!symbol.trim() || loading) ? BLOOMBERG_COLORS.GRAY : BLOOMBERG_COLORS.GREEN,
              color: 'black',
              border: 'none',
              padding: '8px 16px',
              fontSize: '10px',
              fontWeight: 'bold',
              cursor: (!symbol.trim() || loading) ? 'not-allowed' : 'pointer'
            }}
          >
            {loading ? 'ADDING...' : 'ADD STOCK'}
          </button>
        </div>
      </div>
    </div>
  );
};

export default AddStockModal;
