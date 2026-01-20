/**
 * Trades Table
 * Displays user's trade history using fetchMyTrades
 * Universal component - works with ALL exchanges
 */

import React from 'react';
import { useTrades } from '../../hooks/useTrades';
import { formatCurrency, formatDateTime } from '../../utils/formatters';

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
};

interface TradesTableProps {
  symbol?: string;
  limit?: number;
}

export function TradesTable({ symbol, limit = 50 }: TradesTableProps) {
  const { trades, isLoading, error, isSupported, refresh } = useTrades(symbol, limit, false);

  if (!isSupported) {
    return (
      <div
        style={{
          padding: '20px',
          textAlign: 'center',
          color: FINCEPT.GRAY,
          fontSize: '11px',
        }}
      >
        This exchange does not support trade history
      </div>
    );
  }

  if (error) {
    return (
      <div
        style={{
          padding: '20px',
          textAlign: 'center',
          color: FINCEPT.RED,
          fontSize: '11px',
        }}
      >
        Failed to load trade history
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

  if (isLoading && trades.length === 0) {
    return (
      <div style={{ padding: '20px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: '11px' }}>
        Loading trade history...
      </div>
    );
  }

  if (trades.length === 0) {
    return (
      <div style={{ padding: '20px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: '11px' }}>
        No trades found
      </div>
    );
  }

  return (
    <div style={{ width: '100%', height: '100%', overflowX: 'auto', overflowY: 'auto' }}>
      <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '10px' }}>
        <thead style={{ position: 'sticky', top: 0, zIndex: 1 }}>
          <tr style={{ backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
            <th style={{ padding: '8px 10px', textAlign: 'left', color: FINCEPT.GRAY, fontWeight: 700 }}>
              TIME
            </th>
            <th style={{ padding: '8px 10px', textAlign: 'left', color: FINCEPT.GRAY, fontWeight: 700 }}>
              SYMBOL
            </th>
            <th style={{ padding: '8px 10px', textAlign: 'left', color: FINCEPT.GRAY, fontWeight: 700 }}>
              SIDE
            </th>
            <th style={{ padding: '8px 10px', textAlign: 'right', color: FINCEPT.GRAY, fontWeight: 700 }}>
              PRICE
            </th>
            <th style={{ padding: '8px 10px', textAlign: 'right', color: FINCEPT.GRAY, fontWeight: 700 }}>
              QTY
            </th>
            <th style={{ padding: '8px 10px', textAlign: 'right', color: FINCEPT.GRAY, fontWeight: 700 }}>
              COST
            </th>
            <th style={{ padding: '8px 10px', textAlign: 'right', color: FINCEPT.GRAY, fontWeight: 700 }}>
              FEE
            </th>
            <th style={{ padding: '8px 10px', textAlign: 'center', color: FINCEPT.GRAY, fontWeight: 700 }}>
              TYPE
            </th>
          </tr>
        </thead>
        <tbody>
          {trades.map((trade, index) => (
            <tr
              key={trade.id}
              style={{
                borderBottom: `1px solid ${FINCEPT.BORDER}`,
                backgroundColor: index % 2 === 0 ? FINCEPT.DARK_BG : FINCEPT.PANEL_BG,
              }}
            >
              {/* Time */}
              <td style={{ padding: '10px', color: FINCEPT.GRAY, fontSize: '9px' }}>
                {formatDateTime(trade.timestamp)}
              </td>

              {/* Symbol */}
              <td style={{ padding: '10px', color: FINCEPT.WHITE, fontWeight: 600 }}>
                {trade.symbol}
              </td>

              {/* Side */}
              <td style={{ padding: '10px' }}>
                <span
                  style={{
                    padding: '2px 6px',
                    backgroundColor: trade.side === 'buy' ? `${FINCEPT.GREEN}20` : `${FINCEPT.RED}20`,
                    color: trade.side === 'buy' ? FINCEPT.GREEN : FINCEPT.RED,
                    fontSize: '9px',
                    fontWeight: 700,
                    borderRadius: '2px',
                  }}
                >
                  {trade.side.toUpperCase()}
                </span>
              </td>

              {/* Price */}
              <td style={{ padding: '10px', textAlign: 'right', color: FINCEPT.WHITE }}>
                {formatCurrency(trade.price)}
              </td>

              {/* Quantity */}
              <td style={{ padding: '10px', textAlign: 'right', color: FINCEPT.WHITE }}>
                {trade.quantity.toFixed(6)}
              </td>

              {/* Cost */}
              <td style={{ padding: '10px', textAlign: 'right', color: FINCEPT.ORANGE }}>
                {formatCurrency(trade.cost)}
              </td>

              {/* Fee */}
              <td style={{ padding: '10px', textAlign: 'right', color: FINCEPT.GRAY }}>
                {trade.fee ? `${trade.fee.cost.toFixed(4)} ${trade.fee.currency}` : '--'}
              </td>

              {/* Taker/Maker */}
              <td style={{ padding: '10px', textAlign: 'center' }}>
                {trade.takerOrMaker && (
                  <span
                    style={{
                      padding: '2px 6px',
                      backgroundColor: trade.takerOrMaker === 'maker' ? `${FINCEPT.GREEN}20` : `${FINCEPT.ORANGE}20`,
                      color: trade.takerOrMaker === 'maker' ? FINCEPT.GREEN : FINCEPT.ORANGE,
                      fontSize: '8px',
                      fontWeight: 700,
                      borderRadius: '2px',
                    }}
                  >
                    {trade.takerOrMaker.toUpperCase()}
                  </span>
                )}
              </td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}
