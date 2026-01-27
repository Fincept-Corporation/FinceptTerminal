import React from 'react';
import { Transaction } from '../../../../services/portfolio/portfolioService';
import { formatCurrency, formatNumber, formatDateTime } from './utils';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, EFFECTS, COMMON_STYLES } from '../finceptStyles';

interface HistoryViewProps {
  transactions: Transaction[];
  currency: string;
}

const HistoryView: React.FC<HistoryViewProps> = ({ transactions, currency }) => {
  return (
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: FINCEPT.DARK_BG,
      overflow: 'hidden',
      fontFamily: TYPOGRAPHY.MONO,
    }}>
      {/* Section Header */}
      <div style={{
        ...COMMON_STYLES.sectionHeader,
        marginBottom: '0px',
      }}>
        TRANSACTION HISTORY
      </div>

      {transactions.length === 0 ? (
        <div style={{
          ...COMMON_STYLES.emptyState,
          padding: '24px',
          height: 'auto',
          flex: 1,
        }}>
          No transactions yet
        </div>
      ) : (
        <div style={{ flex: 1, overflow: 'auto' }}>
          {/* Table Header */}
          <div style={{
            display: 'grid',
            gridTemplateColumns: '1.5fr 1fr 0.8fr 1fr 1fr 1fr 2fr',
            gap: '8px',
            padding: '8px 12px',
            backgroundColor: FINCEPT.HEADER_BG,
            fontSize: '9px',
            fontWeight: 700,
            letterSpacing: '0.5px',
            borderBottom: `1px solid ${FINCEPT.ORANGE}`,
            position: 'sticky',
            top: 0,
            zIndex: 1,
          }}>
            <div style={{ color: FINCEPT.ORANGE }}>DATE</div>
            <div style={{ color: FINCEPT.ORANGE }}>SYMBOL</div>
            <div style={{ color: FINCEPT.ORANGE }}>TYPE</div>
            <div style={{ color: FINCEPT.ORANGE, textAlign: 'right' }}>QTY</div>
            <div style={{ color: FINCEPT.ORANGE, textAlign: 'right' }}>PRICE</div>
            <div style={{ color: FINCEPT.ORANGE, textAlign: 'right' }}>TOTAL</div>
            <div style={{ color: FINCEPT.ORANGE }}>NOTES</div>
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
                borderLeft: `3px solid ${txn.transaction_type === 'BUY' ? FINCEPT.GREEN : FINCEPT.RED}`,
                fontSize: '10px',
                marginBottom: '1px',
                fontFamily: TYPOGRAPHY.MONO,
                minHeight: '32px',
                alignItems: 'center',
                transition: EFFECTS.TRANSITION_STANDARD,
              }}
              onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = FINCEPT.HOVER; }}
              onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = index % 2 === 0 ? 'rgba(255,255,255,0.03)' : 'transparent'; }}
            >
              <div style={{ color: FINCEPT.GRAY }}>
                {formatDateTime(txn.transaction_date)}
              </div>
              <div style={{ color: FINCEPT.CYAN, fontWeight: 600 }}>{txn.symbol}</div>
              <div>
                <span style={{
                  padding: '2px 6px',
                  backgroundColor: txn.transaction_type === 'BUY' ? `${FINCEPT.GREEN}20` : `${FINCEPT.RED}20`,
                  color: txn.transaction_type === 'BUY' ? FINCEPT.GREEN : FINCEPT.RED,
                  fontSize: '8px',
                  fontWeight: 700,
                  borderRadius: '2px',
                }}>
                  {txn.transaction_type}
                </span>
              </div>
              <div style={{ color: FINCEPT.CYAN, textAlign: 'right' }}>{formatNumber(txn.quantity, 4)}</div>
              <div style={{ color: FINCEPT.CYAN, textAlign: 'right' }}>{formatCurrency(txn.price, currency)}</div>
              <div style={{ color: FINCEPT.YELLOW, textAlign: 'right', fontWeight: 600 }}>
                {formatCurrency(txn.total_value, currency)}
              </div>
              <div style={{ color: FINCEPT.GRAY, fontSize: '9px' }}>{txn.notes || '-'}</div>
            </div>
          ))}
        </div>
      )}
    </div>
  );
};

export default HistoryView;
