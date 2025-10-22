import React from 'react';
import { Transaction } from '../../../services/portfolioService';
import { BLOOMBERG_COLORS, formatCurrency, formatNumber, formatDateTime } from './utils';

interface HistoryViewProps {
  transactions: Transaction[];
  currency: string;
}

const HistoryView: React.FC<HistoryViewProps> = ({ transactions, currency }) => {
  const { ORANGE, WHITE, RED, GREEN, GRAY, CYAN, YELLOW } = BLOOMBERG_COLORS;

  return (
    <div>
      <div style={{
        color: ORANGE,
        fontSize: '12px',
        fontWeight: 'bold',
        marginBottom: '12px'
      }}>
        TRANSACTION HISTORY
      </div>

      {transactions.length === 0 ? (
        <div style={{ padding: '32px', textAlign: 'center', color: GRAY, fontSize: '11px' }}>
          No transactions yet
        </div>
      ) : (
        <div>
          {/* Table Header */}
          <div style={{
            display: 'grid',
            gridTemplateColumns: '1.5fr 1fr 0.8fr 1fr 1fr 1fr 2fr',
            gap: '8px',
            padding: '8px',
            backgroundColor: 'rgba(255,165,0,0.1)',
            fontSize: '10px',
            fontWeight: 'bold',
            marginBottom: '4px'
          }}>
            <div style={{ color: WHITE }}>DATE</div>
            <div style={{ color: WHITE }}>SYMBOL</div>
            <div style={{ color: WHITE }}>TYPE</div>
            <div style={{ color: WHITE, textAlign: 'right' }}>QTY</div>
            <div style={{ color: WHITE, textAlign: 'right' }}>PRICE</div>
            <div style={{ color: WHITE, textAlign: 'right' }}>TOTAL</div>
            <div style={{ color: WHITE }}>NOTES</div>
          </div>

          {transactions.map((txn, index) => (
            <div
              key={txn.id}
              style={{
                display: 'grid',
                gridTemplateColumns: '1.5fr 1fr 0.8fr 1fr 1fr 1fr 2fr',
                gap: '8px',
                padding: '8px',
                backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent',
                borderLeft: `3px solid ${txn.transaction_type === 'BUY' ? GREEN : RED}`,
                fontSize: '10px',
                marginBottom: '2px'
              }}
            >
              <div style={{ color: GRAY }}>
                {formatDateTime(txn.transaction_date)}
              </div>
              <div style={{ color: CYAN }}>{txn.symbol}</div>
              <div style={{
                color: txn.transaction_type === 'BUY' ? GREEN : RED,
                fontWeight: 'bold'
              }}>
                {txn.transaction_type}
              </div>
              <div style={{ color: WHITE, textAlign: 'right' }}>{formatNumber(txn.quantity, 4)}</div>
              <div style={{ color: WHITE, textAlign: 'right' }}>{formatCurrency(txn.price, currency)}</div>
              <div style={{ color: YELLOW, textAlign: 'right', fontWeight: 'bold' }}>
                {formatCurrency(txn.total_value, currency)}
              </div>
              <div style={{ color: GRAY, fontSize: '9px' }}>{txn.notes || '-'}</div>
            </div>
          ))}
        </div>
      )}
    </div>
  );
};

export default HistoryView;
