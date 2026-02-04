/**
 * EquityWatchlist - Left sidebar with watchlist, indices, and sectors
 */
import React from 'react';
import { FINCEPT, formatCurrency, getCurrencyFromSymbol, MARKET_INDICES } from '../constants';
import type { EquityWatchlistProps, LeftSidebarView } from '../types';

export const EquityWatchlist: React.FC<EquityWatchlistProps> = ({
  watchlist,
  watchlistQuotes,
  selectedSymbol,
  selectedExchange,
  leftSidebarView,
  useWebSocketForWatchlist,
  onSymbolSelect,
  onViewChange,
  onWebSocketToggle,
}) => {
  return (
    <div style={{
      width: '280px',
      minWidth: '280px',
      backgroundColor: FINCEPT.PANEL_BG,
      borderRight: `1px solid ${FINCEPT.BORDER}`,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden',
      flexShrink: 0
    }}>
      {/* Toggle Header */}
      <div style={{
        padding: '6px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        gap: '4px'
      }}>
        {(['watchlist', 'indices', 'sectors'] as LeftSidebarView[]).map((view) => (
          <button
            key={view}
            onClick={() => onViewChange(view)}
            style={{
              flex: 1,
              padding: '6px 8px',
              backgroundColor: leftSidebarView === view ? FINCEPT.ORANGE : 'transparent',
              border: 'none',
              color: leftSidebarView === view ? FINCEPT.DARK_BG : FINCEPT.GRAY,
              cursor: 'pointer',
              fontSize: '9px',
              fontWeight: 700,
              letterSpacing: '0.5px',
              transition: 'all 0.15s',
              borderRadius: '2px'
            }}
          >
            {view.toUpperCase()}
          </button>
        ))}
      </div>

      {/* WebSocket Toggle for Watchlist */}
      {leftSidebarView === 'watchlist' && (
        <div style={{
          padding: '4px 8px',
          backgroundColor: FINCEPT.PANEL_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
        }}>
          <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
            {useWebSocketForWatchlist ? '\u26A1 REALTIME' : '\uD83D\uDD04 POLLING'}
          </span>
          <button
            onClick={onWebSocketToggle}
            style={{
              padding: '3px 8px',
              backgroundColor: useWebSocketForWatchlist ? FINCEPT.GREEN : FINCEPT.MUTED,
              border: 'none',
              color: useWebSocketForWatchlist ? FINCEPT.DARK_BG : FINCEPT.WHITE,
              cursor: 'pointer',
              fontSize: '8px',
              fontWeight: 700,
              borderRadius: '2px',
              transition: 'all 0.15s',
            }}
            title={useWebSocketForWatchlist ? 'Switch to REST polling' : 'Switch to WebSocket real-time'}
          >
            {useWebSocketForWatchlist ? 'WS ON' : 'WS OFF'}
          </button>
        </div>
      )}

      {/* Content */}
      <div style={{ flex: 1, overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
        {leftSidebarView === 'watchlist' && (
          <div style={{
            flex: 1,
            overflow: 'auto',
            display: 'grid',
            gridTemplateColumns: '1fr 1fr',
            gap: '0',
            alignContent: 'start'
          }}>
            {watchlist.map((symbol, idx) => {
              const q = watchlistQuotes[symbol];
              const isSelected = symbol === selectedSymbol;

              return (
                <div
                  key={symbol}
                  onClick={() => onSymbolSelect(symbol, selectedExchange)}
                  style={{
                    padding: '8px 10px',
                    cursor: 'pointer',
                    backgroundColor: isSelected ? `${FINCEPT.ORANGE}15` : 'transparent',
                    borderLeft: isSelected ? `3px solid ${FINCEPT.ORANGE}` : '3px solid transparent',
                    borderBottom: `1px solid ${FINCEPT.BORDER}`,
                    borderRight: idx % 2 === 0 ? `1px solid ${FINCEPT.BORDER}` : 'none',
                    transition: 'all 0.15s'
                  }}
                  onMouseEnter={(e) => {
                    if (!isSelected) e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                  }}
                  onMouseLeave={(e) => {
                    if (!isSelected) e.currentTarget.style.backgroundColor = 'transparent';
                  }}
                >
                  <div style={{
                    display: 'flex',
                    justifyContent: 'space-between',
                    alignItems: 'center',
                    gap: '8px'
                  }}>
                    <div style={{
                      fontSize: '11px',
                      fontWeight: 700,
                      color: isSelected ? FINCEPT.ORANGE : FINCEPT.WHITE,
                      whiteSpace: 'nowrap',
                      overflow: 'hidden',
                      textOverflow: 'ellipsis',
                      maxWidth: '60px'
                    }}>
                      {symbol}
                    </div>

                    {q && q.lastPrice != null ? (
                      <div style={{
                        display: 'flex',
                        flexDirection: 'column',
                        alignItems: 'flex-end',
                        gap: '2px'
                      }}>
                        <div style={{
                          fontSize: '10px',
                          color: FINCEPT.WHITE,
                          fontFamily: 'monospace',
                          fontWeight: 600
                        }}>
                          {formatCurrency(q.lastPrice, getCurrencyFromSymbol(symbol, selectedExchange))}
                        </div>
                        <div style={{
                          fontSize: '9px',
                          color: (q.changePercent ?? 0) >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
                          fontFamily: 'monospace',
                          fontWeight: 700
                        }}>
                          {(q.changePercent ?? 0) >= 0 ? '\u25B2' : '\u25BC'} {Math.abs(q.changePercent ?? 0).toFixed(2)}%
                        </div>
                      </div>
                    ) : (
                      <div style={{
                        fontSize: '9px',
                        color: FINCEPT.GRAY,
                        fontFamily: 'monospace'
                      }}>
                        ...
                      </div>
                    )}
                  </div>
                </div>
              );
            })}
          </div>
        )}

        {leftSidebarView === 'indices' && (
          <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
            <div style={{
              fontSize: '10px',
              fontWeight: 700,
              color: FINCEPT.GRAY,
              marginBottom: '12px',
              letterSpacing: '0.5px'
            }}>
              MARKET INDICES
            </div>
            {MARKET_INDICES.map((index) => (
              <div
                key={index.symbol}
                style={{
                  padding: '12px',
                  backgroundColor: FINCEPT.HEADER_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  marginBottom: '8px',
                  cursor: 'pointer'
                }}
                onMouseEnter={(e) => e.currentTarget.style.borderColor = FINCEPT.ORANGE}
                onMouseLeave={(e) => e.currentTarget.style.borderColor = FINCEPT.BORDER}
              >
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                  <div>
                    <div style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.WHITE }}>{index.symbol}</div>
                    <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>{index.exchange}</div>
                  </div>
                  <div style={{ textAlign: 'right' }}>
                    <div style={{ fontSize: '11px', fontWeight: 600, color: FINCEPT.MUTED, fontStyle: 'italic' }}>
                      Connect broker
                    </div>
                  </div>
                </div>
              </div>
            ))}
          </div>
        )}

        {leftSidebarView === 'sectors' && (
          <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
            <div style={{
              fontSize: '10px',
              fontWeight: 700,
              color: FINCEPT.GRAY,
              marginBottom: '12px',
              letterSpacing: '0.5px'
            }}>
              SECTOR PERFORMANCE
            </div>
            {['IT', 'Banking', 'Pharma', 'Auto', 'FMCG', 'Metal', 'Realty', 'Energy'].map((sector) => (
              <div
                key={sector}
                style={{
                  padding: '10px 12px',
                  backgroundColor: FINCEPT.HEADER_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  marginBottom: '6px',
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'center',
                  cursor: 'pointer'
                }}
                onMouseEnter={(e) => e.currentTarget.style.borderColor = FINCEPT.CYAN}
                onMouseLeave={(e) => e.currentTarget.style.borderColor = FINCEPT.BORDER}
              >
                <span style={{ fontSize: '11px', color: FINCEPT.WHITE }}>{sector}</span>
                <span style={{ fontSize: '10px', color: FINCEPT.MUTED, fontStyle: 'italic' }}>No data</span>
              </div>
            ))}
          </div>
        )}
      </div>

      {/* Footer Stats */}
      <div style={{
        padding: '4px 8px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderTop: `1px solid ${FINCEPT.BORDER}`,
        fontSize: '9px',
        color: FINCEPT.MUTED,
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        flexShrink: 0,
      }}>
        <span>{watchlist.length} symbols</span>
        <span style={{ color: useWebSocketForWatchlist ? FINCEPT.GREEN : FINCEPT.GRAY }}>
          {useWebSocketForWatchlist ? 'LIVE' : 'POLLING'}
        </span>
      </div>
    </div>
  );
};
