/**
 * VBBottomTabs - Bottom tabbed panel: Trade Log | Monthly Returns | Drawdowns | Raw
 */

import React, { useState } from 'react';
import { FINCEPT, COMMON_STYLES } from '../../../portfolio-tab/finceptStyles';
import { VBTradeLog } from './VBTradeLog';
import { VBReturnsHeatmap } from './VBReturnsHeatmap';
import { VBDrawdownChart } from './VBDrawdownChart';
import { ResultRaw } from '../../results/ResultRaw';
import type { BacktestingState } from '../../types';

interface VBBottomTabsProps {
  state: BacktestingState;
}

type BottomTab = 'trades' | 'returns' | 'drawdowns' | 'raw';

const TABS: { id: BottomTab; label: string }[] = [
  { id: 'trades', label: 'TRADE LOG' },
  { id: 'returns', label: 'MONTHLY RETURNS' },
  { id: 'drawdowns', label: 'DRAWDOWNS' },
  { id: 'raw', label: 'RAW' },
];

export const VBBottomTabs: React.FC<VBBottomTabsProps> = ({ state }) => {
  const [activeTab, setActiveTab] = useState<BottomTab>('trades');
  const { result } = state;

  const trades = result?.data?.trades;
  const equity = result?.data?.equity;
  const tradeCount = Array.isArray(trades) ? trades.length : 0;

  return (
    <div style={{
      height: '220px',
      flexShrink: 0,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden',
    }}>
      {/* Tab strip */}
      <div style={{
        height: '28px',
        flexShrink: 0,
        backgroundColor: FINCEPT.HEADER_BG,
        borderTop: `1px solid ${FINCEPT.BORDER}`,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        alignItems: 'stretch',
        paddingLeft: '4px',
      }}>
        {TABS.map(tab => {
          const isActive = activeTab === tab.id;
          return (
            <button
              key={tab.id}
              onClick={() => setActiveTab(tab.id)}
              style={{
                padding: '0 14px',
                backgroundColor: isActive ? FINCEPT.ORANGE : 'transparent',
                color: isActive ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                border: 'none',
                fontSize: '10px',
                fontWeight: 700,
                cursor: 'pointer',
                letterSpacing: '0.3px',
                transition: 'all 0.15s ease',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
              }}
            >
              {tab.label}
              {tab.id === 'trades' && tradeCount > 0 && (
                <span style={{
                  padding: '0 4px',
                  backgroundColor: isActive ? 'rgba(0,0,0,0.2)' : `${FINCEPT.ORANGE}20`,
                  color: isActive ? FINCEPT.DARK_BG : FINCEPT.ORANGE,
                  fontSize: '9px',
                  fontWeight: 700,
                  borderRadius: '2px',
                }}>
                  {tradeCount}
                </span>
              )}
            </button>
          );
        })}
      </div>

      {/* Tab content */}
      <div style={{ flex: 1, overflow: 'hidden' }}>
        {activeTab === 'trades' && <VBTradeLog trades={trades} />}
        {activeTab === 'returns' && <VBReturnsHeatmap equity={equity} />}
        {activeTab === 'drawdowns' && <VBDrawdownChart equity={equity} />}
        {activeTab === 'raw' && (
          <div style={{ height: '100%', overflow: 'auto', padding: '8px' }}>
            {result ? <ResultRaw result={result} /> : (
              <div style={{ color: FINCEPT.MUTED, fontSize: '10px', textAlign: 'center', padding: '24px' }}>
                No result data
              </div>
            )}
          </div>
        )}
      </div>
    </div>
  );
};
