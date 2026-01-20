/**
 * Liquidations Panel
 * Displays market liquidations and personal liquidation history
 */

import React, { useState } from 'react';
import { Flame, RefreshCw, AlertCircle, AlertTriangle } from 'lucide-react';
import { useLiquidations, useMyLiquidations } from '../../hooks/useDerivatives';
import { useBrokerContext } from '../../../../../contexts/BrokerContext';
import { formatCurrency, formatQuantity, formatDateTime } from '../../utils/formatters';

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
};

interface LiquidationsPanelProps {
  symbol?: string;
  showMyLiquidations?: boolean;
}

export function LiquidationsPanel({ symbol, showMyLiquidations = false }: LiquidationsPanelProps) {
  const { tradingMode } = useBrokerContext();
  const [viewMode, setViewMode] = useState<'market' | 'mine'>(showMyLiquidations ? 'mine' : 'market');

  const {
    liquidations: marketLiquidations,
    isLoading: marketLoading,
    isSupported: marketSupported,
    refresh: refreshMarket,
  } = useLiquidations(symbol);

  const {
    liquidations: myLiquidations,
    isLoading: myLoading,
    isSupported: mySupported,
    refresh: refreshMine,
  } = useMyLiquidations(symbol);

  const liquidations = viewMode === 'market' ? marketLiquidations : myLiquidations;
  const isLoading = viewMode === 'market' ? marketLoading : myLoading;
  const refresh = viewMode === 'market' ? refreshMarket : refreshMine;

  // Calculate totals
  const totalLiquidated = liquidations.reduce((sum, l) => sum + l.cost, 0);
  const longLiquidations = liquidations.filter((l) => l.side === 'sell'); // Liquidated longs become sells
  const shortLiquidations = liquidations.filter((l) => l.side === 'buy'); // Liquidated shorts become buys

  if (tradingMode === 'paper') {
    return (
      <div style={{ padding: '20px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: '11px' }}>
        <AlertCircle size={20} color={FINCEPT.YELLOW} style={{ marginBottom: '8px' }} />
        <div>Liquidations not available in paper trading mode</div>
      </div>
    );
  }

  if (!marketSupported && !mySupported) {
    return (
      <div style={{ padding: '20px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: '11px' }}>
        Liquidation data not supported by this exchange
      </div>
    );
  }

  return (
    <div style={{ background: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '12px' }}>
      {/* Header */}
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '12px' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Flame size={14} color={FINCEPT.RED} />
          <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE }}>LIQUIDATIONS</span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <div style={{ display: 'flex', gap: '4px' }}>
            <button
              onClick={() => setViewMode('market')}
              style={{
                padding: '4px 8px',
                fontSize: '9px',
                backgroundColor: viewMode === 'market' ? FINCEPT.ORANGE : FINCEPT.DARK_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                color: viewMode === 'market' ? FINCEPT.WHITE : FINCEPT.GRAY,
                cursor: 'pointer',
              }}
            >
              MARKET
            </button>
            <button
              onClick={() => setViewMode('mine')}
              style={{
                padding: '4px 8px',
                fontSize: '9px',
                backgroundColor: viewMode === 'mine' ? FINCEPT.ORANGE : FINCEPT.DARK_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                color: viewMode === 'mine' ? FINCEPT.WHITE : FINCEPT.GRAY,
                cursor: 'pointer',
              }}
            >
              MY HISTORY
            </button>
          </div>
          <button
            onClick={refresh}
            disabled={isLoading}
            style={{ background: 'transparent', border: 'none', cursor: 'pointer', padding: '4px' }}
          >
            <RefreshCw size={12} color={FINCEPT.GRAY} style={{ animation: isLoading ? 'spin 1s linear infinite' : 'none' }} />
          </button>
        </div>
      </div>

      {/* Summary Stats */}
      {liquidations.length > 0 && viewMode === 'market' && (
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px', marginBottom: '12px' }}>
          <div style={{ padding: '10px', backgroundColor: FINCEPT.DARK_BG, borderRadius: '4px', textAlign: 'center' }}>
            <div style={{ fontSize: '8px', color: FINCEPT.GRAY }}>TOTAL</div>
            <div style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.RED, fontFamily: '"IBM Plex Mono", monospace' }}>
              {formatCurrency(totalLiquidated)}
            </div>
          </div>
          <div style={{ padding: '10px', backgroundColor: FINCEPT.DARK_BG, borderRadius: '4px', textAlign: 'center' }}>
            <div style={{ fontSize: '8px', color: FINCEPT.GRAY }}>LONGS REKT</div>
            <div style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.RED, fontFamily: '"IBM Plex Mono", monospace' }}>
              {longLiquidations.length}
            </div>
          </div>
          <div style={{ padding: '10px', backgroundColor: FINCEPT.DARK_BG, borderRadius: '4px', textAlign: 'center' }}>
            <div style={{ fontSize: '8px', color: FINCEPT.GRAY }}>SHORTS REKT</div>
            <div style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.GREEN, fontFamily: '"IBM Plex Mono", monospace' }}>
              {shortLiquidations.length}
            </div>
          </div>
        </div>
      )}

      {/* My Liquidations Warning */}
      {viewMode === 'mine' && myLiquidations.length > 0 && (
        <div style={{ padding: '10px', backgroundColor: `${FINCEPT.RED}20`, borderRadius: '4px', marginBottom: '12px', display: 'flex', alignItems: 'center', gap: '8px' }}>
          <AlertTriangle size={16} color={FINCEPT.RED} />
          <span style={{ fontSize: '10px', color: FINCEPT.RED }}>
            You have {myLiquidations.length} liquidation(s) in your history
          </span>
        </div>
      )}

      {/* Liquidations List */}
      <div style={{ maxHeight: '250px', overflowY: 'auto' }}>
        {isLoading && liquidations.length === 0 ? (
          <div style={{ padding: '20px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: '10px' }}>
            Loading liquidations...
          </div>
        ) : liquidations.length === 0 ? (
          <div style={{ padding: '20px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: '10px' }}>
            {viewMode === 'mine' ? 'No liquidations in your history' : 'No recent liquidations'}
          </div>
        ) : (
          <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '9px' }}>
            <thead>
              <tr style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                <th style={{ padding: '6px', textAlign: 'left', color: FINCEPT.GRAY }}>TIME</th>
                <th style={{ padding: '6px', textAlign: 'left', color: FINCEPT.GRAY }}>SYMBOL</th>
                <th style={{ padding: '6px', textAlign: 'center', color: FINCEPT.GRAY }}>SIDE</th>
                <th style={{ padding: '6px', textAlign: 'right', color: FINCEPT.GRAY }}>PRICE</th>
                <th style={{ padding: '6px', textAlign: 'right', color: FINCEPT.GRAY }}>AMOUNT</th>
                <th style={{ padding: '6px', textAlign: 'right', color: FINCEPT.GRAY }}>VALUE</th>
              </tr>
            </thead>
            <tbody>
              {liquidations.slice(0, 20).map((liq, idx) => {
                const isLongLiq = liq.side === 'sell';
                return (
                  <tr key={liq.id || idx} style={{ borderBottom: `1px solid ${FINCEPT.BORDER}`, backgroundColor: idx % 2 === 0 ? FINCEPT.DARK_BG : 'transparent' }}>
                    <td style={{ padding: '6px', color: FINCEPT.GRAY, fontSize: '8px' }}>
                      {new Date(liq.timestamp).toLocaleTimeString()}
                    </td>
                    <td style={{ padding: '6px', color: FINCEPT.WHITE, fontWeight: 600 }}>
                      {liq.symbol}
                    </td>
                    <td style={{ padding: '6px', textAlign: 'center' }}>
                      <span style={{
                        padding: '2px 6px',
                        backgroundColor: isLongLiq ? `${FINCEPT.RED}20` : `${FINCEPT.GREEN}20`,
                        color: isLongLiq ? FINCEPT.RED : FINCEPT.GREEN,
                        fontSize: '8px',
                        fontWeight: 700,
                        borderRadius: '2px',
                      }}>
                        {isLongLiq ? 'LONG' : 'SHORT'}
                      </span>
                    </td>
                    <td style={{ padding: '6px', textAlign: 'right', color: FINCEPT.WHITE, fontFamily: '"IBM Plex Mono", monospace' }}>
                      {formatCurrency(liq.price)}
                    </td>
                    <td style={{ padding: '6px', textAlign: 'right', color: FINCEPT.WHITE, fontFamily: '"IBM Plex Mono", monospace' }}>
                      {formatQuantity(liq.amount)}
                    </td>
                    <td style={{ padding: '6px', textAlign: 'right', color: FINCEPT.RED, fontFamily: '"IBM Plex Mono", monospace', fontWeight: 600 }}>
                      {formatCurrency(liq.cost)}
                    </td>
                  </tr>
                );
              })}
            </tbody>
          </table>
        )}
      </div>

      <style>{`
        @keyframes spin {
          from { transform: rotate(0deg); }
          to { transform: rotate(360deg); }
        }
      `}</style>
    </div>
  );
}
