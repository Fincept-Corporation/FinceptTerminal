/**
 * Transactions Table
 * Displays deposit and withdrawal history
 */

import React, { useState } from 'react';
import { ArrowDownLeft, ArrowUpRight, ExternalLink, Copy, Check, RefreshCw } from 'lucide-react';
import { useTransactions } from '../../hooks/useFunding';
import { formatDateTime } from '../../utils/formatters';

const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  YELLOW: '#FFD700',
  CYAN: '#00E5FF',
  BLUE: '#0088FF',
};

interface TransactionsTableProps {
  currency?: string;
  limit?: number;
  type?: 'all' | 'deposits' | 'withdrawals';
}

export function TransactionsTable({ currency, limit = 50, type = 'all' }: TransactionsTableProps) {
  const { transactions, deposits, withdrawals, isLoading, error, isSupported, refresh } = useTransactions(currency, limit);
  const [copiedId, setCopiedId] = useState<string | null>(null);

  // Filter based on type
  const displayedTransactions = type === 'deposits' ? deposits : type === 'withdrawals' ? withdrawals : transactions;

  const copyToClipboard = (text: string, id: string) => {
    navigator.clipboard.writeText(text);
    setCopiedId(id);
    setTimeout(() => setCopiedId(null), 2000);
  };

  const getStatusConfig = (status: string) => {
    switch (status) {
      case 'ok':
        return { color: FINCEPT.GREEN, label: 'Completed' };
      case 'pending':
        return { color: FINCEPT.YELLOW, label: 'Pending' };
      case 'failed':
        return { color: FINCEPT.RED, label: 'Failed' };
      case 'canceled':
        return { color: FINCEPT.GRAY, label: 'Canceled' };
      default:
        return { color: FINCEPT.GRAY, label: status };
    }
  };

  if (!isSupported) {
    return (
      <div style={{ padding: '20px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: '11px' }}>
        Transaction history not available in paper trading mode
      </div>
    );
  }

  if (error) {
    return (
      <div style={{ padding: '20px', textAlign: 'center', color: FINCEPT.RED, fontSize: '11px' }}>
        Failed to load transactions
        <button
          onClick={refresh}
          style={{
            marginLeft: '10px',
            padding: '4px 8px',
            backgroundColor: FINCEPT.ORANGE,
            border: 'none',
            color: FINCEPT.WHITE,
            fontSize: '9px',
            cursor: 'pointer',
          }}
        >
          RETRY
        </button>
      </div>
    );
  }

  if (isLoading && displayedTransactions.length === 0) {
    return (
      <div style={{ padding: '20px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: '11px' }}>
        Loading transaction history...
      </div>
    );
  }

  if (displayedTransactions.length === 0) {
    return (
      <div style={{ padding: '20px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: '11px' }}>
        No transactions found
      </div>
    );
  }

  return (
    <div style={{ width: '100%', height: '100%', overflowX: 'auto', overflowY: 'auto' }}>
      {/* Refresh Button */}
      <div style={{ padding: '8px 12px', display: 'flex', justifyContent: 'flex-end' }}>
        <button
          onClick={refresh}
          disabled={isLoading}
          style={{
            display: 'flex',
            alignItems: 'center',
            gap: '4px',
            padding: '4px 8px',
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            color: FINCEPT.GRAY,
            fontSize: '9px',
            cursor: isLoading ? 'wait' : 'pointer',
            borderRadius: '2px',
          }}
        >
          <RefreshCw size={10} style={{ animation: isLoading ? 'spin 1s linear infinite' : 'none' }} />
          REFRESH
        </button>
      </div>

      <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '10px' }}>
        <thead style={{ position: 'sticky', top: 0, zIndex: 1 }}>
          <tr style={{ backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
            <th style={{ padding: '8px 10px', textAlign: 'left', color: FINCEPT.GRAY, fontWeight: 700 }}>TYPE</th>
            <th style={{ padding: '8px 10px', textAlign: 'left', color: FINCEPT.GRAY, fontWeight: 700 }}>TIME</th>
            <th style={{ padding: '8px 10px', textAlign: 'left', color: FINCEPT.GRAY, fontWeight: 700 }}>CURRENCY</th>
            <th style={{ padding: '8px 10px', textAlign: 'right', color: FINCEPT.GRAY, fontWeight: 700 }}>AMOUNT</th>
            <th style={{ padding: '8px 10px', textAlign: 'center', color: FINCEPT.GRAY, fontWeight: 700 }}>STATUS</th>
            <th style={{ padding: '8px 10px', textAlign: 'left', color: FINCEPT.GRAY, fontWeight: 700 }}>ADDRESS</th>
            <th style={{ padding: '8px 10px', textAlign: 'right', color: FINCEPT.GRAY, fontWeight: 700 }}>FEE</th>
            <th style={{ padding: '8px 10px', textAlign: 'center', color: FINCEPT.GRAY, fontWeight: 700 }}>TXID</th>
          </tr>
        </thead>
        <tbody>
          {displayedTransactions.map((tx, index) => {
            const statusConfig = getStatusConfig(tx.status);
            const isDeposit = tx.type === 'deposit';

            return (
              <tr
                key={tx.id}
                style={{
                  borderBottom: `1px solid ${FINCEPT.BORDER}`,
                  backgroundColor: index % 2 === 0 ? FINCEPT.DARK_BG : FINCEPT.PANEL_BG,
                }}
              >
                {/* Type */}
                <td style={{ padding: '10px' }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                    {isDeposit ? (
                      <ArrowDownLeft size={14} color={FINCEPT.GREEN} />
                    ) : (
                      <ArrowUpRight size={14} color={FINCEPT.RED} />
                    )}
                    <span
                      style={{
                        fontSize: '9px',
                        fontWeight: 700,
                        color: isDeposit ? FINCEPT.GREEN : FINCEPT.RED,
                      }}
                    >
                      {tx.type.toUpperCase()}
                    </span>
                  </div>
                </td>

                {/* Time */}
                <td style={{ padding: '10px', color: FINCEPT.GRAY, fontSize: '9px' }}>
                  {formatDateTime(tx.timestamp)}
                </td>

                {/* Currency */}
                <td style={{ padding: '10px', color: FINCEPT.WHITE, fontWeight: 600 }}>
                  {tx.currency}
                  {tx.network && (
                    <span style={{ fontSize: '8px', color: FINCEPT.GRAY, marginLeft: '4px' }}>
                      ({tx.network})
                    </span>
                  )}
                </td>

                {/* Amount */}
                <td
                  style={{
                    padding: '10px',
                    textAlign: 'right',
                    color: isDeposit ? FINCEPT.GREEN : FINCEPT.RED,
                    fontFamily: '"IBM Plex Mono", monospace',
                    fontWeight: 600,
                  }}
                >
                  {isDeposit ? '+' : '-'}
                  {tx.amount.toFixed(8)}
                </td>

                {/* Status */}
                <td style={{ padding: '10px', textAlign: 'center' }}>
                  <span
                    style={{
                      padding: '2px 8px',
                      backgroundColor: `${statusConfig.color}20`,
                      color: statusConfig.color,
                      fontSize: '8px',
                      fontWeight: 700,
                      borderRadius: '2px',
                    }}
                  >
                    {statusConfig.label.toUpperCase()}
                  </span>
                </td>

                {/* Address */}
                <td style={{ padding: '10px' }}>
                  {tx.address ? (
                    <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                      <span
                        style={{
                          fontSize: '9px',
                          color: FINCEPT.CYAN,
                          fontFamily: '"IBM Plex Mono", monospace',
                          maxWidth: '120px',
                          overflow: 'hidden',
                          textOverflow: 'ellipsis',
                          whiteSpace: 'nowrap',
                        }}
                        title={tx.address}
                      >
                        {tx.address.slice(0, 8)}...{tx.address.slice(-6)}
                      </span>
                      <button
                        onClick={() => copyToClipboard(tx.address!, `addr-${tx.id}`)}
                        style={{
                          background: 'transparent',
                          border: 'none',
                          cursor: 'pointer',
                          padding: '2px',
                        }}
                        title="Copy address"
                      >
                        {copiedId === `addr-${tx.id}` ? (
                          <Check size={10} color={FINCEPT.GREEN} />
                        ) : (
                          <Copy size={10} color={FINCEPT.GRAY} />
                        )}
                      </button>
                    </div>
                  ) : (
                    <span style={{ color: FINCEPT.GRAY }}>--</span>
                  )}
                </td>

                {/* Fee */}
                <td style={{ padding: '10px', textAlign: 'right', color: FINCEPT.GRAY, fontSize: '9px' }}>
                  {tx.fee ? `${tx.fee.cost.toFixed(8)} ${tx.fee.currency}` : '--'}
                </td>

                {/* TXID */}
                <td style={{ padding: '10px', textAlign: 'center' }}>
                  {tx.txid ? (
                    <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px' }}>
                      <span
                        style={{
                          fontSize: '8px',
                          color: FINCEPT.BLUE,
                          fontFamily: '"IBM Plex Mono", monospace',
                        }}
                        title={tx.txid}
                      >
                        {tx.txid.slice(0, 6)}...
                      </span>
                      <button
                        onClick={() => copyToClipboard(tx.txid!, `txid-${tx.id}`)}
                        style={{
                          background: 'transparent',
                          border: 'none',
                          cursor: 'pointer',
                          padding: '2px',
                        }}
                        title="Copy transaction ID"
                      >
                        {copiedId === `txid-${tx.id}` ? (
                          <Check size={10} color={FINCEPT.GREEN} />
                        ) : (
                          <Copy size={10} color={FINCEPT.GRAY} />
                        )}
                      </button>
                    </div>
                  ) : (
                    <span style={{ color: FINCEPT.GRAY }}>--</span>
                  )}
                </td>
              </tr>
            );
          })}
        </tbody>
      </table>

      <style>{`
        @keyframes spin {
          from { transform: rotate(0deg); }
          to { transform: rotate(360deg); }
        }
      `}</style>
    </div>
  );
}
