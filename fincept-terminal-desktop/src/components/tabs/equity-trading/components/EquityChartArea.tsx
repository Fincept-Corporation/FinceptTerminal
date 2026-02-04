/**
 * EquityChartArea - Center panel with chart, depth, and trades views
 */
import React from 'react';
import { BarChart3, RefreshCw } from 'lucide-react';
import { StockTradingChart } from './StockTradingChart';
import { FINCEPT, formatCurrency, getCurrencyFromSymbol } from '../constants';
import type { EquityChartAreaProps, CenterView } from '../types';

export const EquityChartArea: React.FC<EquityChartAreaProps> = ({
  selectedSymbol,
  selectedExchange,
  centerView,
  chartInterval,
  quote,
  symbolTrades,
  isBottomPanelMinimized,
  isRefreshing,
  onViewChange,
  onIntervalChange,
  onRefresh,
  fmtPrice,
}) => {
  return (
    <div style={{
      flex: isBottomPanelMinimized ? 1 : '1 1 auto',
      backgroundColor: FINCEPT.PANEL_BG,
      borderBottom: `1px solid ${FINCEPT.BORDER}`,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden',
      minHeight: '300px',
      transition: 'flex 0.3s ease'
    }}>
      {/* Chart Header */}
      <div style={{
        padding: '4px 12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        alignItems: 'center',
        gap: '8px',
        flexShrink: 0
      }}>
        {(['CHART', 'DEPTH', 'TRADES'] as const).map((view) => (
          <button
            key={view}
            onClick={() => onViewChange(view.toLowerCase() as CenterView)}
            style={{
              padding: '3px 10px',
              backgroundColor: centerView === view.toLowerCase() ? FINCEPT.ORANGE : 'transparent',
              border: 'none',
              color: centerView === view.toLowerCase() ? FINCEPT.DARK_BG : FINCEPT.GRAY,
              cursor: 'pointer',
              fontSize: '9px',
              fontWeight: 700,
              transition: 'all 0.2s'
            }}
          >
            {view}
          </button>
        ))}

        <div style={{ flex: 1 }} />

        {/* Refresh Button */}
        <button
          onClick={onRefresh}
          disabled={isRefreshing}
          style={{
            padding: '3px 6px',
            backgroundColor: 'transparent',
            border: `1px solid ${FINCEPT.BORDER}`,
            color: FINCEPT.GRAY,
            cursor: 'pointer',
            fontSize: '9px',
            display: 'flex',
            alignItems: 'center',
            gap: '4px'
          }}
        >
          <RefreshCw size={10} className={isRefreshing ? 'animate-spin' : ''} />
        </button>
      </div>

      {/* Chart Content */}
      <div style={{
        flex: 1,
        display: 'flex',
        alignItems: 'stretch',
        justifyContent: 'stretch',
        color: FINCEPT.GRAY,
        fontSize: '12px',
        overflow: 'hidden',
        minHeight: 0,
        position: 'relative'
      }}>
        {centerView === 'chart' && (
          <div style={{
            width: '100%',
            height: '100%',
            display: 'flex',
            flexDirection: 'column',
            position: 'absolute',
            top: 0,
            left: 0,
            right: 0,
            bottom: 0,
            paddingBottom: '4px'
          }}>
            <StockTradingChart
              symbol={selectedSymbol}
              exchange={selectedExchange}
              interval={chartInterval}
              onIntervalChange={onIntervalChange}
              liveQuote={quote ? {
                lastPrice: quote.lastPrice,
                change: quote.change || 0,
                changePercent: quote.changePercent || 0,
                previousClose: quote.previousClose,
                open: quote.open,
                high: quote.high,
                low: quote.low,
                volume: quote.volume
              } : undefined}
            />
          </div>
        )}
        {centerView === 'depth' && (
          <div style={{
            width: '100%',
            height: '100%',
            padding: '20px',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            flexDirection: 'column',
            gap: '12px'
          }}>
            <BarChart3 size={48} color={FINCEPT.GRAY} />
            <span style={{ fontSize: '14px', color: FINCEPT.GRAY }}>Market Depth Chart</span>
            <span style={{ fontSize: '11px', color: FINCEPT.MUTED }}>Connect to broker for real-time depth data</span>
          </div>
        )}
        {centerView === 'trades' && (
          <div style={{ width: '100%', height: '100%', overflow: 'auto', padding: '8px' }}>
            {symbolTrades.length === 0 ? (
              <div style={{
                padding: '40px',
                textAlign: 'center',
                color: FINCEPT.GRAY,
                fontSize: '11px'
              }}>
                No recent trades for {selectedSymbol || 'selected symbol'}
              </div>
            ) : (
              <table style={{ width: '100%', fontSize: '10px', borderCollapse: 'collapse' }}>
                <thead>
                  <tr style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                    <th style={{ padding: '6px', textAlign: 'left', color: FINCEPT.GRAY }}>TIME</th>
                    <th style={{ padding: '6px', textAlign: 'left', color: FINCEPT.GRAY }}>SIDE</th>
                    <th style={{ padding: '6px', textAlign: 'right', color: FINCEPT.GRAY }}>PRICE</th>
                    <th style={{ padding: '6px', textAlign: 'right', color: FINCEPT.GRAY }}>QTY</th>
                  </tr>
                </thead>
                <tbody>
                  {symbolTrades.slice(0, 20).map((trade, idx) => (
                    <tr key={trade.tradeId || idx} style={{ borderBottom: `1px solid ${FINCEPT.BORDER}40` }}>
                      <td style={{ padding: '4px 6px', color: FINCEPT.GRAY, fontSize: '9px' }}>
                        {new Date(trade.tradedAt).toLocaleTimeString()}
                      </td>
                      <td style={{
                        padding: '4px 6px',
                        color: trade.side === 'BUY' ? FINCEPT.GREEN : FINCEPT.RED,
                        fontWeight: 600,
                        fontSize: '9px'
                      }}>
                        {trade.side}
                      </td>
                      <td style={{
                        padding: '4px 6px',
                        textAlign: 'right',
                        color: trade.side === 'BUY' ? FINCEPT.GREEN : FINCEPT.RED
                      }}>
                        {formatCurrency(trade.price || 0, getCurrencyFromSymbol(trade.symbol, selectedExchange))}
                      </td>
                      <td style={{ padding: '4px 6px', textAlign: 'right', color: FINCEPT.WHITE }}>
                        {trade.quantity}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            )}
          </div>
        )}
      </div>
    </div>
  );
};
