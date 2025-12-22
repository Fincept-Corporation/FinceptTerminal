import React from 'react';
import { getBloombergColors } from '../portfolio/utils';

interface AddAssetModalProps {
  show: boolean;
  formState: {
    symbol: string;
    quantity: string;
    price: string;
  };
  onSymbolChange: (value: string) => void;
  onQuantityChange: (value: string) => void;
  onPriceChange: (value: string) => void;
  onClose: () => void;
  onAdd: () => void;
}

const AddAssetModal: React.FC<AddAssetModalProps> = ({
  show,
  formState,
  onSymbolChange,
  onQuantityChange,
  onPriceChange,
  onClose,
  onAdd
}) => {
  if (!show) return null;

  const { WHITE, GREEN, GRAY, DARK_BG, PANEL_BG } = getBloombergColors();

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
        backgroundColor: DARK_BG,
        border: `2px solid ${GREEN}`,
        padding: '24px',
        minWidth: '400px'
      }}>
        <div style={{ color: GREEN, fontSize: '14px', fontWeight: 'bold', marginBottom: '16px' }}>
          ADD ASSET TO PORTFOLIO
        </div>

        <div style={{ marginBottom: '12px' }}>
          <label style={{ color: GRAY, fontSize: '10px', display: 'block', marginBottom: '4px' }}>
            SYMBOL *
          </label>
          <input
            type="text"
            value={formState.symbol}
            onChange={(e) => onSymbolChange(e.target.value.toUpperCase())}
            style={{
              width: '100%',
              background: PANEL_BG,
              border: `1px solid ${GRAY}`,
              color: WHITE,
              padding: '8px',
              fontSize: '11px',
              textTransform: 'uppercase'
            }}
            placeholder="AAPL"
          />
        </div>

        <div style={{ marginBottom: '12px' }}>
          <label style={{ color: GRAY, fontSize: '10px', display: 'block', marginBottom: '4px' }}>
            QUANTITY *
          </label>
          <input
            type="number"
            value={formState.quantity}
            onChange={(e) => onQuantityChange(e.target.value)}
            style={{
              width: '100%',
              background: PANEL_BG,
              border: `1px solid ${GRAY}`,
              color: WHITE,
              padding: '8px',
              fontSize: '11px'
            }}
            placeholder="100"
            step="0.0001"
          />
        </div>

        <div style={{ marginBottom: '16px' }}>
          <label style={{ color: GRAY, fontSize: '10px', display: 'block', marginBottom: '4px' }}>
            BUY PRICE *
          </label>
          <input
            type="number"
            value={formState.price}
            onChange={(e) => onPriceChange(e.target.value)}
            style={{
              width: '100%',
              background: PANEL_BG,
              border: `1px solid ${GRAY}`,
              color: WHITE,
              padding: '8px',
              fontSize: '11px'
            }}
            placeholder="150.00"
            step="0.01"
          />
        </div>

        <div style={{ display: 'flex', gap: '8px', justifyContent: 'flex-end' }}>
          <button
            onClick={onClose}
            style={{
              background: GRAY,
              color: 'black',
              border: 'none',
              padding: '8px 16px',
              fontSize: '10px',
              fontWeight: 'bold',
              cursor: 'pointer'
            }}
          >
            CANCEL
          </button>
          <button
            onClick={onAdd}
            style={{
              background: GREEN,
              color: 'black',
              border: 'none',
              padding: '8px 16px',
              fontSize: '10px',
              fontWeight: 'bold',
              cursor: 'pointer'
            }}
          >
            ADD
          </button>
        </div>
      </div>
    </div>
  );
};

export default AddAssetModal;
