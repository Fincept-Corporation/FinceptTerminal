import React from 'react';
import { AlertTriangle } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES } from '../finceptStyles';

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
  if (!show) return null;

  return (
    <div style={COMMON_STYLES.modalOverlay}>
      <div style={{
        ...COMMON_STYLES.modalPanel,
        fontFamily: TYPOGRAPHY.MONO,
        maxWidth: '450px'
      }}>
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: SPACING.MEDIUM,
          marginBottom: SPACING.LARGE
        }}>
          <AlertTriangle size={24} color={FINCEPT.RED} />
          <div style={{
            color: FINCEPT.RED,
            fontSize: TYPOGRAPHY.HEADING,
            fontWeight: TYPOGRAPHY.BOLD,
            letterSpacing: TYPOGRAPHY.WIDE
          }}>
            DELETE PORTFOLIO
          </div>
        </div>

        <div style={{
          color: FINCEPT.WHITE,
          fontSize: TYPOGRAPHY.DEFAULT,
          marginBottom: SPACING.LARGE,
          lineHeight: 1.5
        }}>
          Are you sure you want to delete <span style={{ color: FINCEPT.ORANGE, fontWeight: TYPOGRAPHY.BOLD }}>"{portfolioName}"</span>?
        </div>

        <div style={{
          color: FINCEPT.GRAY,
          fontSize: TYPOGRAPHY.BODY,
          marginBottom: SPACING.LARGE,
          padding: SPACING.MEDIUM,
          backgroundColor: FINCEPT.DARK_BG,
          border: BORDERS.RED
        }}>
          This action cannot be undone. All positions, transactions, and history will be permanently deleted.
        </div>

        <div style={{ display: 'flex', gap: SPACING.MEDIUM, justifyContent: 'flex-end' }}>
          <button
            onClick={onClose}
            style={{
              ...COMMON_STYLES.buttonSecondary,
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.backgroundColor = 'transparent';
            }}
          >
            CANCEL
          </button>
          <button
            onClick={onConfirm}
            style={{
              ...COMMON_STYLES.buttonPrimary,
              backgroundColor: FINCEPT.RED,
              border: BORDERS.RED
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
