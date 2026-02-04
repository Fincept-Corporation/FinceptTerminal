/**
 * Positions Panel Component - Fincept Style
 *
 * Displays open positions with P&L and action buttons with proper scrolling
 */

import React, { useState, useRef, useEffect } from 'react';
import {
  TrendingUp, TrendingDown, RefreshCw, X, ChevronDown, ChevronUp,
  AlertCircle, Loader2, Target
} from 'lucide-react';
import { useStockTradingData, useStockBrokerContext } from '@/contexts/StockBrokerContext';
import { withTimeout } from '@/services/core/apiUtils';
import type { Position, ProductType } from '@/brokers/stocks/types';

// Fincept color palette
const COLORS = {
  ORANGE: '#FF8800',
  GREEN: '#00D66F',
  RED: '#FF3B3B',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  GRAY: '#787878',
  MUTED: '#4A4A4A',
  WHITE: '#FFFFFF',
};

const PRODUCT_LABELS: Record<ProductType, string> = {
  CASH: 'CNC',
  INTRADAY: 'MIS',
  MARGIN: 'NRML',
  COVER: 'CO',
  BRACKET: 'BO',
};

interface PositionsPanelProps {
  onSquareOff?: (position: Position) => void;
}

export function PositionsPanel({ onSquareOff }: PositionsPanelProps) {
  const { positions, refreshPositions, isRefreshing, isReady } = useStockTradingData();
  const { adapter } = useStockBrokerContext();

  const [expandedPosition, setExpandedPosition] = useState<string | null>(null);
  const [isClosingAll, setIsClosingAll] = useState(false);
  const [closingPositionId, setClosingPositionId] = useState<string | null>(null);
  const mountedRef = useRef(true);

  useEffect(() => {
    mountedRef.current = true;
    return () => { mountedRef.current = false; };
  }, []);

  // Calculate totals
  const totals = React.useMemo(() => {
    return positions.reduce(
      (acc, pos) => ({
        totalPnl: acc.totalPnl + pos.pnl,
        totalValue: acc.totalValue + (pos.quantity * pos.lastPrice),
        dayPnl: acc.dayPnl + pos.dayPnl,
      }),
      { totalPnl: 0, totalValue: 0, dayPnl: 0 }
    );
  }, [positions]);

  // Handle square off single position
  const handleSquareOff = async (position: Position) => {
    if (!adapter) return;

    setClosingPositionId(`${position.symbol}-${position.exchange}-${position.productType}`);

    try {
      const side = position.quantity > 0 ? 'SELL' : 'BUY';
      const response = await withTimeout(adapter.placeOrder({
        symbol: position.symbol,
        exchange: position.exchange,
        side,
        quantity: Math.abs(position.quantity),
        orderType: 'MARKET',
        productType: position.productType,
      }), 30000);

      if (!mountedRef.current) return;
      if (response.success) {
        await withTimeout(refreshPositions(), 15000).catch(() => {});
        if (onSquareOff) {
          onSquareOff(position);
        }
      }
    } catch (err) {
      console.error('Failed to square off position:', err);
    } finally {
      if (mountedRef.current) setClosingPositionId(null);
    }
  };

  // Handle close all positions
  const handleCloseAll = async () => {
    if (!adapter || positions.length === 0) return;

    setIsClosingAll(true);

    try {
      const result = await withTimeout(adapter.closeAllPositions(), 30000);
      if (!mountedRef.current) return;
      if (result.success) {
        await withTimeout(refreshPositions(), 15000).catch(() => {});
      }
    } catch (err) {
      console.error('Failed to close all positions:', err);
    } finally {
      if (mountedRef.current) setIsClosingAll(false);
    }
  };

  if (!isReady) {
    return (
      <div style={{
        height: '100%',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        color: COLORS.GRAY,
        fontSize: '11px'
      }}>
        Connect to your broker to view positions
      </div>
    );
  }

  return (
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden',
      backgroundColor: COLORS.PANEL_BG,
      fontFamily: '"IBM Plex Mono", "Consolas", monospace'
    }}>
      {/* Header */}
      <div style={{
        padding: '6px 12px',
        borderBottom: `1px solid ${COLORS.BORDER}`,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        backgroundColor: COLORS.HEADER_BG,
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span style={{ fontSize: '11px', fontWeight: 600, color: COLORS.WHITE }}>POSITIONS</span>
          <span style={{ fontSize: '10px', color: COLORS.GRAY }}>({positions.length})</span>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          {/* Refresh */}
          <button
            onClick={() => refreshPositions()}
            disabled={isRefreshing}
            style={{
              padding: '4px',
              backgroundColor: 'transparent',
              border: `1px solid ${COLORS.BORDER}`,
              color: COLORS.GRAY,
              cursor: 'pointer'
            }}
          >
            <RefreshCw size={12} className={isRefreshing ? 'animate-spin' : ''} />
          </button>

          {/* Close All */}
          {positions.length > 0 && (
            <button
              onClick={handleCloseAll}
              disabled={isClosingAll}
              style={{
                padding: '3px 8px',
                fontSize: '9px',
                fontWeight: 600,
                backgroundColor: `${COLORS.RED}20`,
                color: COLORS.RED,
                border: `1px solid ${COLORS.RED}40`,
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '4px'
              }}
            >
              {isClosingAll ? (
                <Loader2 size={10} className="animate-spin" />
              ) : (
                <X size={10} />
              )}
              EXIT ALL
            </button>
          )}
        </div>
      </div>

      {/* Summary Row */}
      {positions.length > 0 && (
        <div style={{
          padding: '8px 12px',
          backgroundColor: COLORS.DARK_BG,
          borderBottom: `1px solid ${COLORS.BORDER}`,
          display: 'grid',
          gridTemplateColumns: 'repeat(3, 1fr)',
          gap: '12px',
          flexShrink: 0
        }}>
          <div>
            <div style={{ fontSize: '9px', color: COLORS.GRAY, marginBottom: '2px' }}>TOTAL VALUE</div>
            <div style={{ fontSize: '11px', fontFamily: 'monospace', color: COLORS.WHITE }}>
              ₹{totals.totalValue.toLocaleString('en-IN', { minimumFractionDigits: 2 })}
            </div>
          </div>
          <div>
            <div style={{ fontSize: '9px', color: COLORS.GRAY, marginBottom: '2px' }}>DAY P&L</div>
            <div style={{
              fontSize: '11px',
              fontFamily: 'monospace',
              color: totals.dayPnl >= 0 ? COLORS.GREEN : COLORS.RED
            }}>
              {totals.dayPnl >= 0 ? '+' : ''}₹{totals.dayPnl.toLocaleString('en-IN', { minimumFractionDigits: 2 })}
            </div>
          </div>
          <div>
            <div style={{ fontSize: '9px', color: COLORS.GRAY, marginBottom: '2px' }}>TOTAL P&L</div>
            <div style={{
              fontSize: '11px',
              fontFamily: 'monospace',
              color: totals.totalPnl >= 0 ? COLORS.GREEN : COLORS.RED
            }}>
              {totals.totalPnl >= 0 ? '+' : ''}₹{totals.totalPnl.toLocaleString('en-IN', { minimumFractionDigits: 2 })}
            </div>
          </div>
        </div>
      )}

      {/* Positions Table */}
      <div style={{ flex: 1, overflow: 'auto', minHeight: 0 }}>
        {positions.length === 0 ? (
          <div style={{
            padding: '24px',
            textAlign: 'center',
            color: COLORS.GRAY
          }}>
            <Target size={32} color={COLORS.MUTED} style={{ margin: '0 auto 12px' }} />
            <p style={{ fontSize: '11px' }}>No open positions</p>
          </div>
        ) : (
          <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '10px' }}>
            <thead style={{ position: 'sticky', top: 0, backgroundColor: COLORS.HEADER_BG, zIndex: 1 }}>
              <tr style={{ borderBottom: `1px solid ${COLORS.BORDER}` }}>
                <th style={{ padding: '6px 8px', textAlign: 'left', color: COLORS.GRAY, fontWeight: 600 }}>SYMBOL</th>
                <th style={{ padding: '6px 8px', textAlign: 'right', color: COLORS.GRAY, fontWeight: 600 }}>QTY</th>
                <th style={{ padding: '6px 8px', textAlign: 'right', color: COLORS.GRAY, fontWeight: 600 }}>AVG</th>
                <th style={{ padding: '6px 8px', textAlign: 'right', color: COLORS.GRAY, fontWeight: 600 }}>LTP</th>
                <th style={{ padding: '6px 8px', textAlign: 'right', color: COLORS.GRAY, fontWeight: 600 }}>P&L</th>
                <th style={{ padding: '6px 8px', textAlign: 'center', color: COLORS.GRAY, fontWeight: 600 }}>ACT</th>
              </tr>
            </thead>
            <tbody>
              {positions.map((position, idx) => {
                const positionKey = `${position.symbol}-${position.exchange}-${position.productType}`;
                const isExpanded = expandedPosition === positionKey;
                const isClosing = closingPositionId === positionKey;
                const isLong = position.quantity > 0;
                const isProfit = position.pnl >= 0;

                return (
                  <React.Fragment key={positionKey}>
                    <tr
                      style={{
                        borderBottom: `1px solid ${COLORS.BORDER}40`,
                        backgroundColor: idx % 2 === 0 ? 'transparent' : `${COLORS.HEADER_BG}40`
                      }}
                      onMouseEnter={(e) => e.currentTarget.style.backgroundColor = COLORS.HEADER_BG}
                      onMouseLeave={(e) => e.currentTarget.style.backgroundColor = idx % 2 === 0 ? 'transparent' : `${COLORS.HEADER_BG}40`}
                    >
                      <td style={{ padding: '8px' }}>
                        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                          <div style={{
                            width: '18px',
                            height: '18px',
                            display: 'flex',
                            alignItems: 'center',
                            justifyContent: 'center',
                            backgroundColor: isLong ? `${COLORS.GREEN}20` : `${COLORS.RED}20`,
                            flexShrink: 0
                          }}>
                            {isLong ? (
                              <TrendingUp size={10} color={COLORS.GREEN} />
                            ) : (
                              <TrendingDown size={10} color={COLORS.RED} />
                            )}
                          </div>
                          <div>
                            <span style={{ fontSize: '10px', color: COLORS.WHITE, fontWeight: 600 }}>{position.symbol}</span>
                            <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                              <span style={{ fontSize: '9px', color: COLORS.GRAY }}>{position.exchange}</span>
                              <span style={{
                                fontSize: '9px',
                                color: position.productType === 'INTRADAY' ? COLORS.ORANGE : COLORS.CYAN
                              }}>
                                {PRODUCT_LABELS[position.productType]}
                              </span>
                            </div>
                          </div>
                        </div>
                      </td>
                      <td style={{ padding: '8px', textAlign: 'right' }}>
                        <span style={{
                          fontFamily: 'monospace',
                          color: isLong ? COLORS.GREEN : COLORS.RED,
                          fontWeight: 600
                        }}>
                          {position.quantity > 0 ? '+' : ''}{position.quantity}
                        </span>
                      </td>
                      <td style={{ padding: '8px', textAlign: 'right', fontFamily: 'monospace', color: COLORS.WHITE }}>
                        {position.averagePrice.toFixed(2)}
                      </td>
                      <td style={{ padding: '8px', textAlign: 'right', fontFamily: 'monospace', color: COLORS.YELLOW }}>
                        {position.lastPrice.toFixed(2)}
                      </td>
                      <td style={{ padding: '8px', textAlign: 'right' }}>
                        <div style={{
                          fontFamily: 'monospace',
                          color: isProfit ? COLORS.GREEN : COLORS.RED
                        }}>
                          {isProfit ? '+' : ''}{position.pnl.toFixed(0)}
                        </div>
                        <div style={{
                          fontSize: '9px',
                          color: isProfit ? COLORS.GREEN : COLORS.RED
                        }}>
                          {isProfit ? '+' : ''}{position.pnlPercent.toFixed(2)}%
                        </div>
                      </td>
                      <td style={{ padding: '8px', textAlign: 'center' }}>
                        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px' }}>
                          <button
                            onClick={() => handleSquareOff(position)}
                            disabled={isClosing}
                            style={{
                              padding: '3px 8px',
                              fontSize: '9px',
                              fontWeight: 600,
                              backgroundColor: `${COLORS.RED}20`,
                              color: COLORS.RED,
                              border: `1px solid ${COLORS.RED}40`,
                              cursor: 'pointer',
                              display: 'flex',
                              alignItems: 'center',
                              gap: '2px'
                            }}
                            onMouseEnter={(e) => {
                              e.currentTarget.style.backgroundColor = `${COLORS.RED}40`;
                            }}
                            onMouseLeave={(e) => {
                              e.currentTarget.style.backgroundColor = `${COLORS.RED}20`;
                            }}
                          >
                            {isClosing ? (
                              <Loader2 size={10} className="animate-spin" />
                            ) : (
                              <X size={10} />
                            )}
                            EXIT
                          </button>
                          <button
                            onClick={() => setExpandedPosition(isExpanded ? null : positionKey)}
                            style={{
                              padding: '3px',
                              backgroundColor: 'transparent',
                              border: `1px solid ${COLORS.BORDER}`,
                              color: COLORS.GRAY,
                              cursor: 'pointer'
                            }}
                          >
                            {isExpanded ? <ChevronUp size={12} /> : <ChevronDown size={12} />}
                          </button>
                        </div>
                      </td>
                    </tr>

                    {/* Expanded Details */}
                    {isExpanded && (
                      <tr style={{ backgroundColor: COLORS.DARK_BG }}>
                        <td colSpan={6} style={{ padding: '8px 12px' }}>
                          <div style={{
                            display: 'grid',
                            gridTemplateColumns: 'repeat(5, 1fr)',
                            gap: '12px',
                            fontSize: '9px'
                          }}>
                            <div>
                              <div style={{ color: COLORS.GRAY, marginBottom: '2px' }}>BUY QTY</div>
                              <div style={{ color: COLORS.WHITE, fontFamily: 'monospace' }}>{position.buyQuantity}</div>
                            </div>
                            <div>
                              <div style={{ color: COLORS.GRAY, marginBottom: '2px' }}>SELL QTY</div>
                              <div style={{ color: COLORS.WHITE, fontFamily: 'monospace' }}>{position.sellQuantity}</div>
                            </div>
                            <div>
                              <div style={{ color: COLORS.GRAY, marginBottom: '2px' }}>BUY VALUE</div>
                              <div style={{ color: COLORS.WHITE, fontFamily: 'monospace' }}>{position.buyValue.toFixed(2)}</div>
                            </div>
                            <div>
                              <div style={{ color: COLORS.GRAY, marginBottom: '2px' }}>SELL VALUE</div>
                              <div style={{ color: COLORS.WHITE, fontFamily: 'monospace' }}>{position.sellValue.toFixed(2)}</div>
                            </div>
                            <div>
                              <div style={{ color: COLORS.GRAY, marginBottom: '2px' }}>DAY P&L</div>
                              <div style={{
                                fontFamily: 'monospace',
                                color: position.dayPnl >= 0 ? COLORS.GREEN : COLORS.RED
                              }}>
                                {position.dayPnl >= 0 ? '+' : ''}{position.dayPnl.toFixed(2)}
                              </div>
                            </div>
                            {position.overnight && (
                              <div style={{ gridColumn: 'span 5' }}>
                                <span style={{
                                  padding: '2px 6px',
                                  backgroundColor: `${COLORS.CYAN}20`,
                                  color: COLORS.CYAN,
                                  fontSize: '9px'
                                }}>
                                  OVERNIGHT POSITION
                                </span>
                              </div>
                            )}
                          </div>
                        </td>
                      </tr>
                    )}
                  </React.Fragment>
                );
              })}
            </tbody>
          </table>
        )}
      </div>
    </div>
  );
}
