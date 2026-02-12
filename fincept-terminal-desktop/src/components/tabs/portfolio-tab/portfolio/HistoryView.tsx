import React, { useState } from 'react';
import { useTranslation } from 'react-i18next';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { Transaction } from '../../../../services/portfolio/portfolioService';
import { formatCurrency, formatNumber, formatDateTime } from './utils';
import { Edit2, Trash2 } from 'lucide-react';
import EditTransactionModal from '../modals/EditTransactionModal';

interface HistoryViewProps {
  transactions: Transaction[];
  currency: string;
  onUpdateTransaction?: (transactionId: string, quantity: number, price: number, transactionDate: string, notes?: string) => void;
  onDeleteTransaction?: (transactionId: string) => void;
}

const HistoryView: React.FC<HistoryViewProps> = ({ transactions, currency, onUpdateTransaction, onDeleteTransaction }) => {
  const { t } = useTranslation('portfolio');
  const { colors, fontSize, fontFamily } = useTerminalTheme();
  const [editingTransaction, setEditingTransaction] = useState<Transaction | null>(null);

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
            gridTemplateColumns: '1.5fr 1fr 0.8fr 1fr 1fr 1fr 1.5fr 0.5fr',
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
            <div style={{ color: colors.primary, textAlign: 'center' }}>{t('history.actions', 'ACTIONS')}</div>
          </div>

          {/* Transaction Rows */}
          {transactions.map((txn, index) => (
            <div
              key={txn.id}
              style={{
                display: 'grid',
                gridTemplateColumns: '1.5fr 1fr 0.8fr 1fr 1fr 1fr 1.5fr 0.5fr',
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
              <div style={{ color: colors.textMuted, fontSize: fontSize.tiny, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{txn.notes || '-'}</div>
              <div style={{ display: 'flex', gap: '4px', justifyContent: 'center' }}>
                {onUpdateTransaction && (
                  <button
                    onClick={() => setEditingTransaction(txn)}
                    style={{
                      background: 'none',
                      border: 'none',
                      color: colors.textMuted,
                      cursor: 'pointer',
                      padding: '4px',
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center',
                      transition: 'color 0.15s ease',
                    }}
                    onMouseEnter={(e) => { e.currentTarget.style.color = colors.primary; }}
                    onMouseLeave={(e) => { e.currentTarget.style.color = colors.textMuted; }}
                    title={t('history.edit', 'Edit transaction')}
                  >
                    <Edit2 size={12} />
                  </button>
                )}
                {onDeleteTransaction && (
                  <button
                    onClick={() => {
                      if (window.confirm(t('history.confirmDelete', 'Are you sure you want to delete this transaction?'))) {
                        onDeleteTransaction(txn.id);
                      }
                    }}
                    style={{
                      background: 'none',
                      border: 'none',
                      color: colors.textMuted,
                      cursor: 'pointer',
                      padding: '4px',
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center',
                      transition: 'color 0.15s ease',
                    }}
                    onMouseEnter={(e) => { e.currentTarget.style.color = colors.alert; }}
                    onMouseLeave={(e) => { e.currentTarget.style.color = colors.textMuted; }}
                    title={t('history.delete', 'Delete transaction')}
                  >
                    <Trash2 size={12} />
                  </button>
                )}
              </div>
            </div>
          ))}
        </div>
      )}

      {/* Edit Transaction Modal */}
      <EditTransactionModal
        show={!!editingTransaction}
        transaction={editingTransaction}
        onClose={() => setEditingTransaction(null)}
        onSave={(transactionId, quantity, price, transactionDate, notes) => {
          if (onUpdateTransaction) {
            onUpdateTransaction(transactionId, quantity, price, transactionDate, notes);
          }
          setEditingTransaction(null);
        }}
        onDelete={(transactionId) => {
          if (onDeleteTransaction) {
            onDeleteTransaction(transactionId);
          }
          setEditingTransaction(null);
        }}
      />
    </div>
  );
};

export default HistoryView;
