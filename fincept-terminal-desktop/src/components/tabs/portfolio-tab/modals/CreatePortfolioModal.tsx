import React from 'react';
import { useTranslation } from 'react-i18next';
import { useTerminalTheme } from '@/contexts/ThemeContext';

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
  const { t } = useTranslation('portfolio');
  const { colors, fontSize, fontFamily } = useTerminalTheme();

  if (!show) return null;

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
      zIndex: 9999,
    }}>
      <div style={{
        backgroundColor: colors.panel,
        border: `2px solid ${colors.primary}`,
        padding: '24px',
        minWidth: '400px',
        maxWidth: '600px',
        boxShadow: `0 4px 16px ${colors.background}80`,
        fontFamily,
      }}>
        <div style={{
          color: colors.primary,
          fontSize: fontSize.heading,
          fontWeight: 700,
          marginBottom: '16px',
          letterSpacing: '0.5px',
        }}>
          {t('modals.createNew')}
        </div>

        <div style={{ marginBottom: '12px' }}>
          <label style={{
            color: colors.textMuted,
            fontSize: fontSize.tiny,
            fontWeight: 700,
            letterSpacing: '0.5px',
            textTransform: 'uppercase',
            display: 'block',
            marginBottom: '4px',
          }}>
            {t('modals.portfolioName')} *
          </label>
          <input
            type="text"
            value={formState.name}
            onChange={(e) => onNameChange(e.target.value)}
            style={{
              width: '100%',
              padding: '8px 10px',
              backgroundColor: colors.background,
              color: colors.text,
              border: '1px solid var(--ft-color-border, #2A2A2A)',
              borderRadius: '2px',
              fontSize: fontSize.small,
              fontFamily,
              outline: 'none',
            }}
            onFocus={(e) => { e.currentTarget.style.borderColor = colors.primary; }}
            onBlur={(e) => { e.currentTarget.style.borderColor = 'var(--ft-color-border, #2A2A2A)'; }}
            placeholder="My Portfolio"
          />
        </div>

        <div style={{ marginBottom: '12px' }}>
          <label style={{
            color: colors.textMuted,
            fontSize: fontSize.tiny,
            fontWeight: 700,
            letterSpacing: '0.5px',
            textTransform: 'uppercase',
            display: 'block',
            marginBottom: '4px',
          }}>
            {t('modals.owner')} *
          </label>
          <input
            type="text"
            value={formState.owner}
            onChange={(e) => onOwnerChange(e.target.value)}
            style={{
              width: '100%',
              padding: '8px 10px',
              backgroundColor: colors.background,
              color: colors.text,
              border: '1px solid var(--ft-color-border, #2A2A2A)',
              borderRadius: '2px',
              fontSize: fontSize.small,
              fontFamily,
              outline: 'none',
            }}
            onFocus={(e) => { e.currentTarget.style.borderColor = colors.primary; }}
            onBlur={(e) => { e.currentTarget.style.borderColor = 'var(--ft-color-border, #2A2A2A)'; }}
            placeholder="John Doe"
          />
        </div>

        <div style={{ marginBottom: '16px' }}>
          <label style={{
            color: colors.textMuted,
            fontSize: fontSize.tiny,
            fontWeight: 700,
            letterSpacing: '0.5px',
            textTransform: 'uppercase',
            display: 'block',
            marginBottom: '4px',
          }}>
            {t('modals.currency')}
          </label>
          <select
            value={formState.currency}
            onChange={(e) => onCurrencyChange(e.target.value)}
            style={{
              width: '100%',
              padding: '8px 10px',
              backgroundColor: colors.background,
              color: colors.text,
              border: '1px solid var(--ft-color-border, #2A2A2A)',
              borderRadius: '2px',
              fontSize: fontSize.small,
              fontFamily,
              cursor: 'pointer',
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
              padding: '6px 10px',
              backgroundColor: 'transparent',
              border: '1px solid var(--ft-color-border, #2A2A2A)',
              color: colors.textMuted,
              borderRadius: '2px',
              fontSize: fontSize.tiny,
              fontWeight: 700,
              cursor: 'pointer',
              transition: 'all 0.2s ease',
              textTransform: 'uppercase',
              letterSpacing: '0.5px',
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = 'var(--ft-color-hover, #1F1F1F)';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.backgroundColor = 'transparent';
            }}
          >
            {t('modals.cancel')}
          </button>
          <button
            onClick={onCreate}
            style={{
              padding: '8px 16px',
              backgroundColor: colors.primary,
              color: colors.background,
              border: 'none',
              borderRadius: '2px',
              fontSize: fontSize.tiny,
              fontWeight: 700,
              cursor: 'pointer',
              transition: 'all 0.2s ease',
              textTransform: 'uppercase',
              letterSpacing: '0.5px',
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.opacity = '0.85';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.opacity = '1';
            }}
          >
            {t('modals.create')}
          </button>
        </div>
      </div>
    </div>
  );
};

export default CreatePortfolioModal;
