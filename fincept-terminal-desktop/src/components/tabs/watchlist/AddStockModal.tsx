import React, { useState } from 'react';
import { FINCEPT, FONT_FAMILY } from './utils';
import { showWarning, showError } from '@/utils/notifications';

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
      showWarning('Please enter a stock symbol');
      return;
    }
    if (existingSymbols.includes(trimmedSymbol)) {
      showWarning(`${trimmedSymbol} is already in this watchlist`);
      return;
    }
    setLoading(true);
    try {
      await onAdd(trimmedSymbol, notes.trim());
      onClose();
    } catch (error) {
      showError(error instanceof Error ? error.message : 'Failed to add stock');
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
        boxShadow: `0 4px 24px rgba(0,0,0,0.6), 0 0 1px ${FINCEPT.GREEN}40`
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
            backgroundColor: FINCEPT.GREEN,
            borderRadius: '2px'
          }} />
          <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.GREEN, letterSpacing: '0.5px' }}>
            ADD STOCK TO WATCHLIST
          </span>
        </div>

        {/* Modal Body */}
        <div style={{ padding: '16px' }}>
          {/* Symbol Field */}
          <div style={{ marginBottom: '12px' }}>
            <label style={{
              fontSize: '9px',
              fontWeight: 700,
              color: FINCEPT.GRAY,
              letterSpacing: '0.5px',
              display: 'block',
              marginBottom: '6px'
            }}>
              SYMBOL *
            </label>
            <input
              type="text"
              value={symbol}
              onChange={(e) => setSymbol(e.target.value.toUpperCase())}
              onKeyDown={(e) => e.key === 'Enter' && !loading && symbol.trim() && handleAdd()}
              style={{
                width: '100%',
                padding: '10px 10px',
                backgroundColor: FINCEPT.DARK_BG,
                color: FINCEPT.WHITE,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                fontSize: '14px',
                fontFamily: FONT_FAMILY,
                fontWeight: 700,
                letterSpacing: '1px',
                textTransform: 'uppercase',
                outline: 'none',
                boxSizing: 'border-box'
              }}
              onFocus={(e) => { e.currentTarget.style.borderColor = FINCEPT.GREEN; }}
              onBlur={(e) => { e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
              placeholder="AAPL"
              maxLength={10}
              autoFocus
            />
            <div style={{
              color: FINCEPT.MUTED,
              fontSize: '9px',
              marginTop: '4px'
            }}>
              ENTER STOCK TICKER (E.G., AAPL, MSFT, GOOGL)
            </div>
          </div>

          {/* Notes Field */}
          <div style={{ marginBottom: '16px' }}>
            <label style={{
              fontSize: '9px',
              fontWeight: 700,
              color: FINCEPT.GRAY,
              letterSpacing: '0.5px',
              display: 'block',
              marginBottom: '6px'
            }}>
              NOTES (OPTIONAL)
            </label>
            <textarea
              value={notes}
              onChange={(e) => setNotes(e.target.value)}
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
              onFocus={(e) => { e.currentTarget.style.borderColor = FINCEPT.GREEN; }}
              onBlur={(e) => { e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
              placeholder="Add notes about this stock..."
              maxLength={200}
            />
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
            onClick={handleAdd}
            disabled={loading || !symbol.trim()}
            style={{
              padding: '6px 16px',
              backgroundColor: (!symbol.trim() || loading) ? FINCEPT.MUTED : FINCEPT.GREEN,
              color: FINCEPT.DARK_BG,
              border: 'none',
              borderRadius: '2px',
              fontSize: '9px',
              fontWeight: 700,
              cursor: (!symbol.trim() || loading) ? 'not-allowed' : 'pointer',
              fontFamily: FONT_FAMILY
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
