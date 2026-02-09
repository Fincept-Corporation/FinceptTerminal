import React from 'react';
import { useTranslation } from 'react-i18next';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { AlertTriangle } from 'lucide-react';

interface ConfirmDeleteModalProps {
  show: boolean;
  portfolioName: string;
  onClose: () => void;
  onConfirm: () => void;
}

const ConfirmDeleteModal: React.FC<ConfirmDeleteModalProps> = ({
  show,
  portfolioName,
  onClose,
  onConfirm
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
        border: `2px solid ${colors.alert}`,
        padding: '24px',
        minWidth: '400px',
        maxWidth: '450px',
        boxShadow: `0 4px 16px ${colors.background}80`,
        fontFamily,
      }}>
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: '12px',
          marginBottom: '16px'
        }}>
          <AlertTriangle size={24} color={colors.alert} />
          <div style={{
            color: colors.alert,
            fontSize: fontSize.heading,
            fontWeight: 700,
            letterSpacing: '0.5px'
          }}>
            DELETE PORTFOLIO
          </div>
        </div>

        <div style={{
          color: colors.text,
          fontSize: fontSize.small,
          marginBottom: '16px',
          lineHeight: 1.5
        }}>
          {t('extracted.areYouSureYou').split('"')[0]}<span style={{ color: colors.primary, fontWeight: 700 }}>"{portfolioName}"</span>?
        </div>

        <div style={{
          color: colors.textMuted,
          fontSize: fontSize.tiny,
          marginBottom: '16px',
          padding: '12px',
          backgroundColor: colors.background,
          border: `1px solid ${colors.alert}`
        }}>
          This action cannot be undone. All positions, transactions, and history will be permanently deleted.
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
            onClick={onConfirm}
            style={{
              padding: '8px 16px',
              backgroundColor: colors.alert,
              color: colors.text,
              border: `1px solid ${colors.alert}`,
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
            DELETE
          </button>
        </div>
      </div>
    </div>
  );
};

export default ConfirmDeleteModal;
