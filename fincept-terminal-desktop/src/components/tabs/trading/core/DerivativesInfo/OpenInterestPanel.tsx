/**
 * Open Interest Panel
 * Displays open interest data for futures/perpetuals
 */

import React from 'react';
import { Activity, RefreshCw, AlertCircle, TrendingUp, TrendingDown } from 'lucide-react';
import { useOpenInterest } from '../../hooks/useDerivatives';
import { useBrokerContext } from '../../../../../contexts/BrokerContext';
import { formatVolume, formatCurrency } from '../../utils/formatters';

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

interface OpenInterestPanelProps {
  symbol?: string;
}

export function OpenInterestPanel({ symbol }: OpenInterestPanelProps) {
  const { tradingMode } = useBrokerContext();
  const { openInterest, isLoading, error, isSupported, refresh } = useOpenInterest(symbol);

  if (tradingMode === 'paper') {
    return (
      <div style={{ padding: '20px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: '11px' }}>
        <AlertCircle size={20} color={FINCEPT.YELLOW} style={{ marginBottom: '8px' }} />
        <div>Open interest not available in paper trading mode</div>
      </div>
    );
  }

  if (!isSupported) {
    return (
      <div style={{ padding: '20px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: '11px' }}>
        Open interest not supported by this exchange
      </div>
    );
  }

  if (!symbol) {
    return (
      <div style={{ padding: '20px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: '11px' }}>
        Select a symbol to view open interest
      </div>
    );
  }

  return (
    <div style={{ background: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '12px' }}>
      {/* Header */}
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '12px' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Activity size={14} color={FINCEPT.PURPLE} />
          <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE }}>OPEN INTEREST</span>
        </div>
        <button
          onClick={refresh}
          disabled={isLoading}
          style={{ background: 'transparent', border: 'none', cursor: 'pointer', padding: '4px' }}
        >
          <RefreshCw size={12} color={FINCEPT.GRAY} style={{ animation: isLoading ? 'spin 1s linear infinite' : 'none' }} />
        </button>
      </div>

      {error ? (
        <div style={{ padding: '20px', textAlign: 'center', color: FINCEPT.RED, fontSize: '10px' }}>
          Failed to load open interest
        </div>
      ) : isLoading && !openInterest ? (
        <div style={{ padding: '20px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: '10px' }}>
          Loading...
        </div>
      ) : openInterest ? (
        <div>
          {/* Main OI Display */}
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px', marginBottom: '12px' }}>
            {/* Amount */}
            <div style={{ padding: '12px', backgroundColor: FINCEPT.DARK_BG, borderRadius: '4px' }}>
              <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '6px' }}>
                OI AMOUNT
              </div>
              <div style={{ fontSize: '18px', fontWeight: 700, fontFamily: '"IBM Plex Mono", monospace', color: FINCEPT.PURPLE }}>
                {formatVolume(openInterest.openInterestAmount)}
              </div>
              <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                contracts
              </div>
            </div>

            {/* Value */}
            <div style={{ padding: '12px', backgroundColor: FINCEPT.DARK_BG, borderRadius: '4px' }}>
              <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '6px' }}>
                OI VALUE
              </div>
              <div style={{ fontSize: '18px', fontWeight: 700, fontFamily: '"IBM Plex Mono", monospace', color: FINCEPT.CYAN }}>
                {formatCurrency(openInterest.openInterestValue)}
              </div>
              <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                USD equivalent
              </div>
            </div>
          </div>

          {/* Last Updated */}
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, textAlign: 'center' }}>
            Last updated: {new Date(openInterest.timestamp).toLocaleTimeString()}
          </div>

          {/* OI Interpretation */}
          <div style={{ marginTop: '12px', padding: '10px', backgroundColor: `${FINCEPT.PURPLE}15`, borderRadius: '4px', fontSize: '10px', color: FINCEPT.GRAY }}>
            <strong style={{ color: FINCEPT.PURPLE }}>What is Open Interest?</strong>
            <div style={{ marginTop: '4px' }}>
              Total number of outstanding derivative contracts that have not been settled.
              Rising OI with price = Strong trend. Falling OI = Trend weakening.
            </div>
          </div>
        </div>
      ) : (
        <div style={{ padding: '20px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: '10px' }}>
          No open interest data available
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
