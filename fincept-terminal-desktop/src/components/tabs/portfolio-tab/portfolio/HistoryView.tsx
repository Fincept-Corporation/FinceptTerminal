import React from 'react';
import { useTranslation } from 'react-i18next';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { Transaction } from '../../../../services/portfolio/portfolioService';
import { formatCurrency, formatNumber, formatDateTime } from './utils';

interface HistoryViewProps {
  transactions: Transaction[];
  currency: string;
}

const HistoryView: React.FC<HistoryViewProps> = ({ transactions, currency }) => {
  const { t } = useTranslation('portfolio');
  const { colors, fontSize, fontFamily } = useTerminalTheme();

  return (
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: colors.background,
      overflow: 'hidden',
      fontFamily,
    }}>
      {/* Section Header */}
      <div style={{
        padding: '12px',
        backgroundColor: 'var(--ft-color-header, #1A1A1A)',
        borderBottom: '1px solid var(--ft-color-border, #2A2A2A)',
        color: colors.primary,
        fontSize: fontSize.body,
        fontWeight: 700,
        letterSpacing: '0.5px',
        textTransform: 'uppercase' as const,
        marginBottom: '0px',
      }}>
        {t('history.transactionHistory', 'TRANSACTION HISTORY')}
      </div>

      {transactions.length === 0 ? (
        <div style={{
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          justifyContent: 'center',
          padding: '24px',
          height: 'auto',
          flex: 1,
          color: '#4A4A4A',
          fontSize: fontSize.small,
          textAlign: 'center',
        }}>
          {t('history.noTransactions', 'No transactions yet')}
        </div>
      ) : (
        <div style={{ flex: 1, overflow: 'auto' }}>
          {/* Table Header */}
          <div style={{
            display: 'grid',
            gridTemplateColumns: '1.5fr 1fr 0.8fr 1fr 1fr 1fr 2fr',
            gap: '8px',
            padding: '8px 12px',
            backgroundColor: 'var(--ft-color-header, #1A1A1A)',
            fontSize: fontSize.tiny,
            fontWeight: 700,
            letterSpacing: '0.5px',
            borderBottom: `1px solid ${colors.primary}`,
            position: 'sticky',
            top: 0,
            zIndex: 1,
          }}>
            <div style={{ color: colors.primary }}>{t('history.date', 'DATE')}</div>
            <div style={{ color: colors.primary }}>{t('history.symbol', 'SYMBOL')}</div>
            <div style={{ color: colors.primary }}>{t('history.type', 'TYPE')}</div>
            <div style={{ color: colors.primary, textAlign: 'right' }}>{t('history.qty', 'QTY')}</div>
            <div style={{ color: colors.primary, textAlign: 'right' }}>{t('history.price', 'PRICE')}</div>
            <div style={{ color: colors.primary, textAlign: 'right' }}>{t('history.total', 'TOTAL')}</div>
            <div style={{ color: colors.primary }}>{t('history.notes', 'NOTES')}</div>
          </div>

          {/* Transaction Rows */}
          {transactions.map((txn, index) => (
            <div
              key={txn.id}
              style={{
                display: 'grid',
                gridTemplateColumns: '1.5fr 1fr 0.8fr 1fr 1fr 1fr 2fr',
                gap: '8px',
                padding: '8px 12px',
                backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.03)' : 'transparent',
                borderLeft: `3px solid ${txn.transaction_type === 'BUY' ? colors.success : colors.alert}`,
                fontSize: fontSize.small,
                marginBottom: '1px',
                fontFamily,
                minHeight: '32px',
                alignItems: 'center',
                transition: 'all 0.2s ease',
              }}
              onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = 'var(--ft-color-hover, #1F1F1F)'; }}
              onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = index % 2 === 0 ? 'rgba(255,255,255,0.03)' : 'transparent'; }}
            >
              <div style={{ color: colors.textMuted }}>
                {formatDateTime(txn.transaction_date)}
              </div>
              <div style={{ color: 'var(--ft-color-accent, #00E5FF)', fontWeight: 600 }}>{txn.symbol}</div>
              <div>
                <span style={{
                  padding: '2px 6px',
                  backgroundColor: txn.transaction_type === 'BUY' ? `${colors.success}20` : `${colors.alert}20`,
                  color: txn.transaction_type === 'BUY' ? colors.success : colors.alert,
                  fontSize: '8px',
                  fontWeight: 700,
                  borderRadius: '2px',
                }}>
                  {txn.transaction_type}
                </span>
              </div>
              <div style={{ color: 'var(--ft-color-accent, #00E5FF)', textAlign: 'right' }}>{formatNumber(txn.quantity, 4)}</div>
              <div style={{ color: 'var(--ft-color-accent, #00E5FF)', textAlign: 'right' }}>{formatCurrency(txn.price, currency)}</div>
              <div style={{ color: 'var(--ft-color-warning, #FFD700)', textAlign: 'right', fontWeight: 600 }}>
                {formatCurrency(txn.total_value, currency)}
              </div>
              <div style={{ color: colors.textMuted, fontSize: fontSize.tiny }}>{txn.notes || '-'}</div>
            </div>
          ))}
        </div>
      )}
    </div>
  );
};

export default HistoryView;
