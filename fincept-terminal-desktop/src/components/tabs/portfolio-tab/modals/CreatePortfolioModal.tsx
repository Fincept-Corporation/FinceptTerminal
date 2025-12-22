import React from 'react';
import { getBloombergColors } from '../portfolio/utils';

interface CreatePortfolioModalProps {
  show: boolean;
  formState: {
    name: string;
    owner: string;
    currency: string;
  };
  onNameChange: (value: string) => void;
  onOwnerChange: (value: string) => void;
  onCurrencyChange: (value: string) => void;
  onClose: () => void;
  onCreate: () => void;
}

const CreatePortfolioModal: React.FC<CreatePortfolioModalProps> = ({
  show,
  formState,
  onNameChange,
  onOwnerChange,
  onCurrencyChange,
  onClose,
  onCreate
}) => {
  if (!show) return null;

  const { ORANGE, WHITE, GRAY, DARK_BG, PANEL_BG } = getBloombergColors();

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
        border: `2px solid ${ORANGE}`,
        padding: '24px',
        minWidth: '400px'
      }}>
        <div style={{ color: ORANGE, fontSize: '14px', fontWeight: 'bold', marginBottom: '16px' }}>
          CREATE NEW PORTFOLIO
        </div>

        <div style={{ marginBottom: '12px' }}>
          <label style={{ color: GRAY, fontSize: '10px', display: 'block', marginBottom: '4px' }}>
            PORTFOLIO NAME *
          </label>
          <input
            type="text"
            value={formState.name}
            onChange={(e) => onNameChange(e.target.value)}
            style={{
              width: '100%',
              background: PANEL_BG,
              border: `1px solid ${GRAY}`,
              color: WHITE,
              padding: '8px',
              fontSize: '11px'
            }}
            placeholder="My Portfolio"
          />
        </div>

        <div style={{ marginBottom: '12px' }}>
          <label style={{ color: GRAY, fontSize: '10px', display: 'block', marginBottom: '4px' }}>
            OWNER *
          </label>
          <input
            type="text"
            value={formState.owner}
            onChange={(e) => onOwnerChange(e.target.value)}
            style={{
              width: '100%',
              background: PANEL_BG,
              border: `1px solid ${GRAY}`,
              color: WHITE,
              padding: '8px',
              fontSize: '11px'
            }}
            placeholder="John Doe"
          />
        </div>

        <div style={{ marginBottom: '16px' }}>
          <label style={{ color: GRAY, fontSize: '10px', display: 'block', marginBottom: '4px' }}>
            CURRENCY
          </label>
          <select
            value={formState.currency}
            onChange={(e) => onCurrencyChange(e.target.value)}
            style={{
              width: '100%',
              background: PANEL_BG,
              border: `1px solid ${GRAY}`,
              color: WHITE,
              padding: '8px',
              fontSize: '11px'
            }}
          >
            <option value="USD">USD - US Dollar</option>
            <option value="EUR">EUR - Euro</option>
            <option value="GBP">GBP - British Pound</option>
            <option value="INR">INR - Indian Rupee</option>
          </select>
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
            onClick={onCreate}
            style={{
              background: ORANGE,
              color: 'black',
              border: 'none',
              padding: '8px 16px',
              fontSize: '10px',
              fontWeight: 'bold',
              cursor: 'pointer'
            }}
          >
            CREATE
          </button>
        </div>
      </div>
    </div>
  );
};

export default CreatePortfolioModal;
