// CryptoChartArea.tsx - Center Chart Area for Crypto Trading
import React from 'react';
import { FINCEPT } from '../constants';
import type { CenterViewType, TradeData } from '../types';
import { TradingChart, DepthChart } from '../../trading/charts';

interface CryptoChartAreaProps {
  selectedSymbol: string;
  selectedView: CenterViewType;
  tradesData: TradeData[];
  isBottomPanelMinimized: boolean;
  onViewChange: (view: CenterViewType) => void;
}

export function CryptoChartArea({
  selectedSymbol,
  selectedView,
  tradesData,
  isBottomPanelMinimized,
  onViewChange,
}: CryptoChartAreaProps) {
  return (
    <div style={{
      flex: isBottomPanelMinimized ? 1 : '0 0 55%',
      backgroundColor: FINCEPT.PANEL_BG,
      borderBottom: `1px solid ${FINCEPT.BORDER}`,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden',
      transition: 'flex 0.3s ease'
    }}>
      <div style={{
        padding: '8px 12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        alignItems: 'center',
        gap: '8px'
      }}>
        {(['CHART', 'DEPTH', 'TRADES'] as const).map((view) => (
          <button
            key={view}
            onClick={() => onViewChange(view.toLowerCase() as CenterViewType)}
            style={{
              padding: '4px 12px',
              backgroundColor: selectedView === view.toLowerCase() ? FINCEPT.ORANGE : 'transparent',
              border: 'none',
              color: selectedView === view.toLowerCase() ? FINCEPT.DARK_BG : FINCEPT.GRAY,
              cursor: 'pointer',
              fontSize: '10px',
              fontWeight: 700,
              transition: 'all 0.2s'
            }}
          >
            {view}
          </button>
        ))}
      </div>
      <div style={{
        flex: 1,
        display: 'flex',
        alignItems: 'stretch',
        justifyContent: 'center',
        color: FINCEPT.GRAY,
        fontSize: '12px',
        overflow: 'visible',
        minHeight: 0
      }}>
        {selectedView === 'chart' && (
          <div style={{ width: '100%', height: '100%', display: 'flex', flexDirection: 'column' }}>
            <TradingChart
              symbol={selectedSymbol}
              showVolume={true}
            />
          </div>
        )}
        {selectedView === 'depth' && (
          <div style={{ width: '100%', height: '100%', padding: '8px' }}>
            <DepthChart
              symbol={selectedSymbol}
            />
          </div>
        )}
        {selectedView === 'trades' && (
          <div style={{ width: '100%', height: '100%', overflow: 'auto', padding: '8px' }}>
            <table style={{ width: '100%', fontSize: '10px', borderCollapse: 'collapse' }}>
              <thead>
                <tr style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                  <th style={{ padding: '6px', textAlign: 'left', color: FINCEPT.GRAY }}>TIME</th>
                  <th style={{ padding: '6px', textAlign: 'right', color: FINCEPT.GRAY }}>PRICE</th>
                  <th style={{ padding: '6px', textAlign: 'right', color: FINCEPT.GRAY }}>SIZE</th>
                </tr>
              </thead>
              <tbody>
                {tradesData.slice(0, 20).map((trade, idx) => (
                  <tr key={idx} style={{ borderBottom: `1px solid ${FINCEPT.BORDER}40` }}>
                    <td style={{ padding: '4px 6px', color: FINCEPT.GRAY, fontSize: '9px' }}>
                      {new Date(trade.timestamp).toLocaleTimeString()}
                    </td>
                    <td style={{
                      padding: '4px 6px',
                      textAlign: 'right',
                      color: trade.side === 'buy' ? FINCEPT.GREEN : FINCEPT.RED
                    }}>
                      ${trade.price?.toFixed(2)}
                    </td>
                    <td style={{ padding: '4px 6px', textAlign: 'right', color: FINCEPT.WHITE }}>
                      {trade.quantity?.toFixed(4)}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </div>
    </div>
  );
}

export default CryptoChartArea;
