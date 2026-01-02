import React from 'react';
import { BLOOMBERG, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES, createFocusHandlers } from '../bloombergStyles';

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

  return (
    <div style={COMMON_STYLES.modalOverlay}>
      <div style={{
        ...COMMON_STYLES.modalPanel,
        fontFamily: TYPOGRAPHY.MONO
      }}>
        <div style={{
          color: BLOOMBERG.ORANGE,
          fontSize: TYPOGRAPHY.HEADING,
          fontWeight: TYPOGRAPHY.BOLD,
          marginBottom: SPACING.LARGE,
          letterSpacing: TYPOGRAPHY.WIDE
        }}>
          CREATE NEW PORTFOLIO
        </div>

        <div style={{ marginBottom: SPACING.DEFAULT }}>
          <label style={{
            ...COMMON_STYLES.dataLabel,
            display: 'block',
            marginBottom: SPACING.SMALL
          }}>
            PORTFOLIO NAME *
          </label>
          <input
            type="text"
            value={formState.name}
            onChange={(e) => onNameChange(e.target.value)}
            style={COMMON_STYLES.inputField}
            {...createFocusHandlers()}
            placeholder="My Portfolio"
          />
        </div>

        <div style={{ marginBottom: SPACING.DEFAULT }}>
          <label style={{
            ...COMMON_STYLES.dataLabel,
            display: 'block',
            marginBottom: SPACING.SMALL
          }}>
            OWNER *
          </label>
          <input
            type="text"
            value={formState.owner}
            onChange={(e) => onOwnerChange(e.target.value)}
            style={COMMON_STYLES.inputField}
            {...createFocusHandlers()}
            placeholder="John Doe"
          />
        </div>

        <div style={{ marginBottom: SPACING.LARGE }}>
          <label style={{
            ...COMMON_STYLES.dataLabel,
            display: 'block',
            marginBottom: SPACING.SMALL
          }}>
            CURRENCY
          </label>
          <select
            value={formState.currency}
            onChange={(e) => onCurrencyChange(e.target.value)}
            style={{
              ...COMMON_STYLES.inputField,
              cursor: 'pointer'
            }}
          >
            <option value="USD">USD - US Dollar</option>
            <option value="EUR">EUR - Euro</option>
            <option value="GBP">GBP - British Pound</option>
            <option value="INR">INR - Indian Rupee</option>
          </select>
        </div>

        <div style={{ display: 'flex', gap: SPACING.MEDIUM, justifyContent: 'flex-end' }}>
          <button
            onClick={onClose}
            style={{
              ...COMMON_STYLES.buttonSecondary,
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER;
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.backgroundColor = 'transparent';
            }}
          >
            CANCEL
          </button>
          <button
            onClick={onCreate}
            style={COMMON_STYLES.buttonPrimary}
            onMouseEnter={(e) => {
              e.currentTarget.style.opacity = '0.85';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.opacity = '1';
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
