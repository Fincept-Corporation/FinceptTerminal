import React from 'react';
import { Transaction } from '../../../../services/portfolio/portfolioService';
import { formatCurrency, formatNumber, formatDateTime } from './utils';
import { BLOOMBERG, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES } from '../bloombergStyles';

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
      backgroundColor: BLOOMBERG.DARK_BG,
      padding: SPACING.DEFAULT,
      overflow: 'hidden'
    }}>
      {/* Section Header */}
      <div style={{
        ...COMMON_STYLES.sectionHeader,
        marginBottom: SPACING.MEDIUM
      }}>
        TRANSACTION HISTORY
      </div>

      {transactions.length === 0 ? (
        <div style={{
          padding: SPACING.XLARGE,
          textAlign: 'center',
          color: BLOOMBERG.GRAY,
          fontSize: TYPOGRAPHY.DEFAULT,
          fontFamily: TYPOGRAPHY.MONO
        }}>
          No transactions yet
        </div>
      ) : (
        <div style={{ flex: 1, overflow: 'auto' }}>
          {/* Table Header */}
          <div style={{
            display: 'grid',
            gridTemplateColumns: '1.5fr 1fr 0.8fr 1fr 1fr 1fr 2fr',
            gap: SPACING.MEDIUM,
            padding: SPACING.MEDIUM,
            backgroundColor: BLOOMBERG.HEADER_BG,
            fontSize: TYPOGRAPHY.BODY,
            fontWeight: TYPOGRAPHY.BOLD,
            borderBottom: BORDERS.ORANGE,
            position: 'sticky',
            top: 0,
            zIndex: 1
          }}>
            <div style={{ color: BLOOMBERG.ORANGE }}>DATE</div>
            <div style={{ color: BLOOMBERG.ORANGE }}>SYMBOL</div>
            <div style={{ color: BLOOMBERG.ORANGE }}>TYPE</div>
            <div style={{ color: BLOOMBERG.ORANGE, textAlign: 'right' }}>QTY</div>
            <div style={{ color: BLOOMBERG.ORANGE, textAlign: 'right' }}>PRICE</div>
            <div style={{ color: BLOOMBERG.ORANGE, textAlign: 'right' }}>TOTAL</div>
            <div style={{ color: BLOOMBERG.ORANGE }}>NOTES</div>
          </div>

          {/* Transaction Rows */}
          {transactions.map((txn, index) => (
            <div
              key={txn.id}
              style={{
                display: 'grid',
                gridTemplateColumns: '1.5fr 1fr 0.8fr 1fr 1fr 1fr 2fr',
                gap: SPACING.MEDIUM,
                padding: SPACING.MEDIUM,
                backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.03)' : 'transparent',
                borderLeft: `3px solid ${txn.transaction_type === 'BUY' ? BLOOMBERG.GREEN : BLOOMBERG.RED}`,
                fontSize: TYPOGRAPHY.BODY,
                marginBottom: '1px',
                fontFamily: TYPOGRAPHY.MONO,
                minHeight: '32px',
                alignItems: 'center'
              }}
            >
              <div style={{ color: BLOOMBERG.GRAY }}>
                {formatDateTime(txn.transaction_date)}
              </div>
              <div style={{ color: BLOOMBERG.CYAN, fontWeight: TYPOGRAPHY.SEMIBOLD }}>{txn.symbol}</div>
              <div style={{
                color: txn.transaction_type === 'BUY' ? BLOOMBERG.GREEN : BLOOMBERG.RED,
                fontWeight: TYPOGRAPHY.BOLD
              }}>
                {txn.transaction_type}
              </div>
              <div style={{ color: BLOOMBERG.WHITE, textAlign: 'right' }}>{formatNumber(txn.quantity, 4)}</div>
              <div style={{ color: BLOOMBERG.WHITE, textAlign: 'right' }}>{formatCurrency(txn.price, currency)}</div>
              <div style={{ color: BLOOMBERG.YELLOW, textAlign: 'right', fontWeight: TYPOGRAPHY.SEMIBOLD }}>
                {formatCurrency(txn.total_value, currency)}
              </div>
              <div style={{ color: BLOOMBERG.GRAY, fontSize: TYPOGRAPHY.SMALL }}>{txn.notes || '-'}</div>
            </div>
          ))}
        </div>
      )}
    </div>
  );
};

export default HistoryView;
