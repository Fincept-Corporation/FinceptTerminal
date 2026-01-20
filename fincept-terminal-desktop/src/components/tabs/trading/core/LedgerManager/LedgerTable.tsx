/**
 * Ledger Table
 * Displays account ledger entries (transactions, fees, funding, etc.)
 */

import React, { useState } from 'react';
import { Book, RefreshCw, AlertCircle, Info, ArrowUpRight, ArrowDownRight, Filter } from 'lucide-react';
import { useLedger } from '../../hooks/useLedger';
import { useBrokerContext } from '../../../../../contexts/BrokerContext';

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
  PURPLE: '#9D4EDD',
};

const TYPE_COLORS: Record<string, string> = {
  trade: FINCEPT.CYAN,
  fee: FINCEPT.ORANGE,
  transfer: FINCEPT.PURPLE,
  deposit: FINCEPT.GREEN,
  withdrawal: FINCEPT.RED,
  funding: FINCEPT.YELLOW,
  rebate: FINCEPT.GREEN,
  cashback: FINCEPT.GREEN,
  referral: FINCEPT.PURPLE,
  default: FINCEPT.GRAY,
};

interface LedgerTableProps {
  currency?: string;
  limit?: number;
}

export function LedgerTable({ currency, limit = 100 }: LedgerTableProps) {
  const { tradingMode, activeBroker } = useBrokerContext();
  const { entries, isLoading, error, isSupported, refresh } = useLedger(currency, undefined, limit);
  const [filterType, setFilterType] = useState<string>('all');

  if (tradingMode === 'paper') {
    return (
      <div style={{ padding: '40px', textAlign: 'center', color: FINCEPT.GRAY, background: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px' }}>
        <AlertCircle size={32} color={FINCEPT.YELLOW} style={{ marginBottom: '12px' }} />
        <div style={{ fontSize: '12px' }}>Ledger not available in paper trading</div>
      </div>
    );
  }

  if (!isSupported) {
    return (
      <div style={{ padding: '40px', textAlign: 'center', color: FINCEPT.GRAY, background: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px' }}>
        <Info size={32} color={FINCEPT.GRAY} style={{ marginBottom: '12px' }} />
        <div style={{ fontSize: '12px' }}>Ledger not supported by {activeBroker}</div>
      </div>
    );
  }

  // Get unique types for filter
  const types = ['all', ...new Set(entries.map((e) => e.type))];

  // Filter entries
  const filteredEntries = entries.filter((e) => filterType === 'all' || e.type === filterType);

  // Calculate summary
  const totalIn = filteredEntries.filter((e) => e.amount > 0).reduce((sum, e) => sum + e.amount, 0);
  const totalOut = filteredEntries.filter((e) => e.amount < 0).reduce((sum, e) => sum + Math.abs(e.amount), 0);
  const totalFees = filteredEntries.filter((e) => e.fee).reduce((sum, e) => sum + (e.fee || 0), 0);

  return (
    <div style={{ background: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px' }}>
      {/* Header */}
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: '12px 16px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Book size={14} color={FINCEPT.CYAN} />
          <span style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.WHITE }}>Account Ledger</span>
          {currency && (
            <span style={{ fontSize: '10px', color: FINCEPT.CYAN, marginLeft: '8px' }}>({currency})</span>
          )}
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          {/* Type Filter */}
          <select
            value={filterType}
            onChange={(e) => setFilterType(e.target.value)}
            style={{
              padding: '4px 8px',
              backgroundColor: FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '4px',
              color: FINCEPT.WHITE,
              fontSize: '10px',
            }}
          >
            {types.map((type) => (
              <option key={type} value={type}>
                {type.charAt(0).toUpperCase() + type.slice(1)}
              </option>
            ))}
          </select>
          <button
            onClick={refresh}
            disabled={isLoading}
            style={{
              padding: '4px',
              backgroundColor: 'transparent',
              border: 'none',
              cursor: 'pointer',
            }}
          >
            <RefreshCw size={12} color={FINCEPT.GRAY} style={{ animation: isLoading ? 'spin 1s linear infinite' : 'none' }} />
          </button>
        </div>
      </div>

      {/* Summary Stats */}
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '1px', backgroundColor: FINCEPT.BORDER }}>
        <div style={{ padding: '12px', backgroundColor: FINCEPT.HEADER_BG, textAlign: 'center' }}>
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px' }}>TOTAL IN</div>
          <div style={{ fontSize: '14px', fontFamily: '"IBM Plex Mono", monospace', color: FINCEPT.GREEN }}>
            +{totalIn.toFixed(4)}
          </div>
        </div>
        <div style={{ padding: '12px', backgroundColor: FINCEPT.HEADER_BG, textAlign: 'center' }}>
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px' }}>TOTAL OUT</div>
          <div style={{ fontSize: '14px', fontFamily: '"IBM Plex Mono", monospace', color: FINCEPT.RED }}>
            -{totalOut.toFixed(4)}
          </div>
        </div>
        <div style={{ padding: '12px', backgroundColor: FINCEPT.HEADER_BG, textAlign: 'center' }}>
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px' }}>TOTAL FEES</div>
          <div style={{ fontSize: '14px', fontFamily: '"IBM Plex Mono", monospace', color: FINCEPT.ORANGE }}>
            {totalFees.toFixed(4)}
          </div>
        </div>
      </div>

      {/* Error */}
      {error && (
        <div style={{ padding: '10px 16px', backgroundColor: `${FINCEPT.RED}20`, fontSize: '11px', color: FINCEPT.RED }}>
          {error.message}
        </div>
      )}

      {/* Table Header */}
      <div style={{ display: 'grid', gridTemplateColumns: '120px 80px 100px 100px 80px 80px', gap: '8px', padding: '10px 16px', backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
        <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>DATE/TIME</span>
        <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>TYPE</span>
        <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>CURRENCY</span>
        <span style={{ fontSize: '9px', color: FINCEPT.GRAY, textAlign: 'right' }}>AMOUNT</span>
        <span style={{ fontSize: '9px', color: FINCEPT.GRAY, textAlign: 'right' }}>FEE</span>
        <span style={{ fontSize: '9px', color: FINCEPT.GRAY, textAlign: 'right' }}>BALANCE</span>
      </div>

      {/* Table Body */}
      <div style={{ maxHeight: '400px', overflowY: 'auto' }}>
        {filteredEntries.map((entry, index) => {
          const typeColor = TYPE_COLORS[entry.type.toLowerCase()] || TYPE_COLORS.default;

          return (
            <div
              key={entry.id || index}
              style={{
                display: 'grid',
                gridTemplateColumns: '120px 80px 100px 100px 80px 80px',
                gap: '8px',
                padding: '10px 16px',
                borderBottom: `1px solid ${FINCEPT.BORDER}`,
                backgroundColor: index % 2 === 0 ? 'transparent' : `${FINCEPT.DARK_BG}50`,
              }}
            >
              {/* Date/Time */}
              <div>
                <div style={{ fontSize: '10px', color: FINCEPT.WHITE }}>
                  {new Date(entry.timestamp).toLocaleDateString()}
                </div>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                  {new Date(entry.timestamp).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}
                </div>
              </div>

              {/* Type */}
              <span
                style={{
                  display: 'inline-flex',
                  alignItems: 'center',
                  padding: '2px 8px',
                  backgroundColor: `${typeColor}20`,
                  borderRadius: '4px',
                  fontSize: '9px',
                  fontWeight: 600,
                  color: typeColor,
                  textTransform: 'uppercase',
                  height: 'fit-content',
                }}
              >
                {entry.type}
              </span>

              {/* Currency */}
              <span style={{ fontSize: '11px', color: FINCEPT.CYAN, fontFamily: '"IBM Plex Mono", monospace' }}>
                {entry.currency}
              </span>

              {/* Amount */}
              <div style={{ textAlign: 'right', display: 'flex', alignItems: 'center', justifyContent: 'flex-end', gap: '4px' }}>
                {entry.amount > 0 ? (
                  <ArrowUpRight size={10} color={FINCEPT.GREEN} />
                ) : (
                  <ArrowDownRight size={10} color={FINCEPT.RED} />
                )}
                <span
                  style={{
                    fontSize: '11px',
                    fontFamily: '"IBM Plex Mono", monospace',
                    color: entry.amount > 0 ? FINCEPT.GREEN : FINCEPT.RED,
                  }}
                >
                  {entry.amount > 0 ? '+' : ''}{entry.amount.toFixed(6)}
                </span>
              </div>

              {/* Fee */}
              <span style={{ fontSize: '10px', color: FINCEPT.ORANGE, fontFamily: '"IBM Plex Mono", monospace', textAlign: 'right' }}>
                {entry.fee ? entry.fee.toFixed(6) : '--'}
              </span>

              {/* Balance After */}
              <span style={{ fontSize: '10px', color: FINCEPT.WHITE, fontFamily: '"IBM Plex Mono", monospace', textAlign: 'right' }}>
                {entry.after !== undefined ? entry.after.toFixed(4) : '--'}
              </span>
            </div>
          );
        })}
      </div>

      {/* Empty State */}
      {!isLoading && filteredEntries.length === 0 && !error && (
        <div style={{ padding: '40px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: '11px' }}>
          No ledger entries found
        </div>
      )}

      {/* Loading */}
      {isLoading && entries.length === 0 && (
        <div style={{ padding: '40px', textAlign: 'center' }}>
          <RefreshCw size={20} color={FINCEPT.GRAY} style={{ animation: 'spin 1s linear infinite' }} />
        </div>
      )}

      <style>{`
        @keyframes spin {
          from { transform: rotate(0deg); }
          to { transform: rotate(360deg); }
        }
      `}</style>
    </div>
  );
}
