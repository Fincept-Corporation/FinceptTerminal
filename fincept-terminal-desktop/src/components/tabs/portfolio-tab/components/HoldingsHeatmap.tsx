import React from 'react';
import {
  LayoutGrid, Shield, Zap, Gauge, TrendingUp, TrendingDown,
  Briefcase, ChevronDown, Plus,
} from 'lucide-react';
import { FINCEPT } from '../finceptStyles';
import { heatColor, valColor } from './helpers';
import { formatCurrency, formatPercent } from '../portfolio/utils';
import { useTranslation } from 'react-i18next';
import type { HoldingWithQuote, Portfolio } from '../../../../services/portfolio/portfolioService';
import type { HeatmapMode, ComputedMetrics } from '../types';

interface HoldingsHeatmapProps {
  holdings: HoldingWithQuote[];
  selectedSymbol: string | null;
  onSelectSymbol: (symbol: string) => void;
  heatmapMode: HeatmapMode;
  onSetHeatmapMode: (mode: HeatmapMode) => void;
  metrics: ComputedMetrics;
  currency: string;
  portfolios: Portfolio[];
  selectedPortfolio: Portfolio | null;
  onSelectPortfolio: (p: Portfolio) => void;
  onCreatePortfolio: () => void;
}

const HoldingsHeatmap: React.FC<HoldingsHeatmapProps> = ({
  holdings, selectedSymbol, onSelectSymbol, heatmapMode, onSetHeatmapMode,
  metrics, currency, portfolios, selectedPortfolio, onSelectPortfolio, onCreatePortfolio,
}) => {
  const { t } = useTranslation('portfolio');
  const selectedH = holdings.find(h => h.symbol === selectedSymbol);
  const sorted = [...holdings].sort((a, b) => b.weight - a.weight);

  // Find top and bottom mover by day change
  const byDayChange = [...holdings].sort((a, b) => b.day_change_percent - a.day_change_percent);
  const topMover = byDayChange[0];
  const bottomMover = byDayChange[byDayChange.length - 1];

  return (
    <div style={{
      width: '220px', flexShrink: 0,
      backgroundColor: '#050505',
      borderRight: `1px solid ${FINCEPT.BORDER}`,
      display: 'flex', flexDirection: 'column',
    }}>
      {/* Heatmap Header */}
      <div style={{
        padding: '5px 8px', backgroundColor: '#0F0F0F',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex', alignItems: 'center', justifyContent: 'space-between',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
          <LayoutGrid size={10} color={FINCEPT.ORANGE} />
          <span style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.ORANGE, letterSpacing: '0.5px' }}>{t('heatmap.holdings', 'HOLDINGS')}</span>
        </div>
        <div style={{ display: 'flex', gap: '2px' }}>
          {(['pnl', 'weight', 'dayChg'] as const).map(m => (
            <button key={m} onClick={() => onSetHeatmapMode(m)} style={{
              padding: '2px 5px', fontSize: '8px', fontWeight: 700,
              backgroundColor: heatmapMode === m ? FINCEPT.ORANGE : 'transparent',
              color: heatmapMode === m ? '#000' : FINCEPT.GRAY,
              border: heatmapMode === m ? 'none' : `1px solid ${FINCEPT.BORDER}`,
              cursor: 'pointer', textTransform: 'uppercase',
            }}>
              {m === 'dayChg' ? 'DAY' : m}
            </button>
          ))}
        </div>
      </div>

      {/* Heatmap Grid */}
      <div style={{ flex: 1, overflow: 'auto', padding: '4px' }}>
        {holdings.length === 0 ? (
          <div style={{ padding: '20px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: '9px' }}>
            {t('heatmap.noHoldings', 'No holdings yet. Click BUY to add positions.')}
          </div>
        ) : (
          <>
            <div style={{ display: 'flex', flexWrap: 'wrap', gap: '2px' }}>
              {sorted.map(h => {
                const heatVal = heatmapMode === 'pnl' ? h.unrealized_pnl_percent
                  : heatmapMode === 'dayChg' ? h.day_change_percent : h.weight;
                const bgColor = heatmapMode === 'weight'
                  ? `rgba(255,136,0,${0.1 + (h.weight / 25) * 0.4})`
                  : heatColor(heatVal);
                const isSelected = h.symbol === selectedSymbol;
                const blockSize = Math.max(h.weight * 3.2, 50);

                return (
                  <div
                    key={h.symbol}
                    onClick={() => onSelectSymbol(h.symbol)}
                    style={{
                      width: 'calc(50% - 1px)', height: `${blockSize}px`,
                      backgroundColor: bgColor,
                      border: isSelected ? `1px solid ${FINCEPT.ORANGE}` : '1px solid #111',
                      padding: '4px 5px', cursor: 'pointer',
                      display: 'flex', flexDirection: 'column', justifyContent: 'space-between',
                      transition: 'all 0.1s ease', overflow: 'hidden',
                    }}
                  >
                    <div>
                      <div style={{ fontSize: '11px', fontWeight: 800, color: isSelected ? FINCEPT.ORANGE : FINCEPT.WHITE }}>
                        {h.symbol}
                      </div>
                      <div style={{ fontSize: '8px', color: FINCEPT.GRAY, marginTop: '1px' }}>{h.weight.toFixed(1)}%</div>
                    </div>
                    <div>
                      <div style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.WHITE }}>
                        {formatCurrency(h.current_price, currency)}
                      </div>
                      <div style={{ fontSize: '9px', fontWeight: 700, color: valColor(h.day_change_percent) }}>
                        {h.day_change_percent >= 0 ? '+' : ''}{h.day_change_percent.toFixed(2)}%
                      </div>
                    </div>
                  </div>
                );
              })}
            </div>

            {/* Selected Holding Detail Card */}
            {selectedH && (
              <div style={{
                marginTop: '6px', padding: '6px',
                backgroundColor: '#0A0A0A', border: `1px solid ${FINCEPT.BORDER}`,
              }}>
                <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '5px' }}>
                  <span style={{ fontSize: '12px', fontWeight: 800, color: FINCEPT.CYAN }}>{selectedH.symbol}</span>
                </div>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '3px', fontSize: '9px' }}>
                  {[
                    { l: 'PRICE', v: formatCurrency(selectedH.current_price, currency), c: FINCEPT.WHITE },
                    { l: 'CHG', v: `${formatPercent(selectedH.day_change_percent)}`, c: valColor(selectedH.day_change_percent) },
                    { l: 'QTY', v: selectedH.quantity.toString(), c: FINCEPT.WHITE },
                    { l: 'COST', v: formatCurrency(selectedH.avg_buy_price, currency), c: FINCEPT.GRAY },
                    { l: 'MKT VAL', v: formatCurrency(selectedH.market_value, currency), c: FINCEPT.YELLOW },
                    { l: 'P&L', v: `${selectedH.unrealized_pnl >= 0 ? '+' : ''}${formatCurrency(selectedH.unrealized_pnl, currency)}`, c: valColor(selectedH.unrealized_pnl) },
                    { l: 'P&L%', v: formatPercent(selectedH.unrealized_pnl_percent), c: valColor(selectedH.unrealized_pnl_percent) },
                    { l: 'WEIGHT', v: `${selectedH.weight.toFixed(1)}%`, c: FINCEPT.ORANGE },
                    { l: 'DAY CHG', v: `${selectedH.day_change >= 0 ? '+' : ''}${formatCurrency(selectedH.day_change, currency)}`, c: valColor(selectedH.day_change) },
                    { l: 'COST BASIS', v: formatCurrency(selectedH.cost_basis, currency), c: FINCEPT.GRAY },
                  ].map(d => (
                    <div key={d.l} style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: FINCEPT.GRAY, fontWeight: 600 }}>{d.l}</span>
                      <span style={{ color: d.c, fontWeight: 600 }}>{d.v}</span>
                    </div>
                  ))}
                </div>
              </div>
            )}
          </>
        )}
      </div>

      {/* Bottom Section: Risk + Movers + Quick Stats + Switcher */}
      <div style={{
        borderTop: `1px solid ${FINCEPT.BORDER}`, backgroundColor: '#0A0A0A',
        display: 'flex', flexDirection: 'column', flexShrink: 0,
      }}>
        {/* Risk Score Gauge */}
        {metrics.riskScore !== null && (
          <div style={{ padding: '8px 10px', borderBottom: '1px solid #111' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '5px', marginBottom: '6px' }}>
              <Shield size={10} color={FINCEPT.ORANGE} />
              <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.ORANGE, letterSpacing: '0.5px' }}>{t('heatmap.riskScore', 'RISK SCORE')}</span>
            </div>
            <div style={{ position: 'relative', height: '10px', backgroundColor: '#111', marginBottom: '4px' }}>
              <div style={{
                position: 'absolute', left: 0, top: 0, bottom: 0,
                width: `${metrics.riskScore}%`,
                background: 'linear-gradient(90deg, #00D66F 0%, #FFD700 50%, #FF8800 100%)',
                opacity: 0.8,
              }} />
              <div style={{
                position: 'absolute', left: `${metrics.riskScore}%`, top: '-2px', bottom: '-2px',
                width: '2px', backgroundColor: '#FFF',
                boxShadow: '0 0 4px rgba(255,255,255,0.6)',
              }} />
            </div>
            <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '8px' }}>
              <span style={{ color: FINCEPT.GREEN }}>{t('heatmap.low', 'LOW')}</span>
              <span style={{ color: FINCEPT.YELLOW, fontWeight: 700 }}>{metrics.riskScore} / 100</span>
              <span style={{ color: FINCEPT.RED }}>{t('heatmap.high', 'HIGH')}</span>
            </div>
          </div>
        )}

        {/* Top Movers */}
        {holdings.length >= 2 && (
          <div style={{ padding: '6px 10px', borderBottom: '1px solid #111' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '5px', marginBottom: '5px' }}>
              <Zap size={10} color={FINCEPT.YELLOW} />
              <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.YELLOW, letterSpacing: '0.5px' }}>{t('heatmap.topMovers', 'TOP MOVERS')}</span>
            </div>
            {topMover && (
              <div style={{
                display: 'flex', alignItems: 'center', justifyContent: 'space-between',
                padding: '4px 6px', backgroundColor: `${FINCEPT.GREEN}08`,
                border: `1px solid ${FINCEPT.GREEN}20`, marginBottom: '3px',
              }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '5px' }}>
                  <TrendingUp size={9} color={FINCEPT.GREEN} />
                  <span style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.CYAN }}>{topMover.symbol}</span>
                </div>
                <span style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.GREEN }}>
                  {topMover.day_change_percent >= 0 ? '+' : ''}{topMover.day_change_percent.toFixed(2)}%
                </span>
              </div>
            )}
            {bottomMover && bottomMover.symbol !== topMover?.symbol && (
              <div style={{
                display: 'flex', alignItems: 'center', justifyContent: 'space-between',
                padding: '4px 6px', backgroundColor: `${FINCEPT.RED}08`,
                border: `1px solid ${FINCEPT.RED}20`,
              }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '5px' }}>
                  <TrendingDown size={9} color={FINCEPT.RED} />
                  <span style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.CYAN }}>{bottomMover.symbol}</span>
                </div>
                <span style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.RED }}>
                  {bottomMover.day_change_percent >= 0 ? '+' : ''}{bottomMover.day_change_percent.toFixed(2)}%
                </span>
              </div>
            )}
          </div>
        )}

        {/* Quick Stats */}
        <div style={{ padding: '6px 10px', borderBottom: '1px solid #111' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '5px', marginBottom: '5px' }}>
            <Gauge size={10} color={FINCEPT.CYAN} />
            <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.CYAN, letterSpacing: '0.5px' }}>{t('heatmap.quickStats', 'QUICK STATS')}</span>
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '3px' }}>
            {[
              { l: t('heatmap.holdings', 'HOLDINGS'), v: `${holdings.length}`, c: FINCEPT.WHITE },
              { l: t('heatmap.concTop3', 'CONC. TOP3'), v: metrics.concentrationTop3 !== null ? `${metrics.concentrationTop3}%` : '--', c: FINCEPT.ORANGE },
              { l: t('heatmap.volatility', 'VOLATILITY'), v: metrics.volatility !== null ? `${metrics.volatility}%` : '--', c: FINCEPT.YELLOW },
            ].map(s => (
              <div key={s.l} style={{ display: 'flex', justifyContent: 'space-between', fontSize: '9px' }}>
                <span style={{ color: FINCEPT.GRAY, fontWeight: 600 }}>{s.l}</span>
                <span style={{ color: s.c, fontWeight: 700 }}>{s.v}</span>
              </div>
            ))}
          </div>
        </div>

        {/* Portfolio Switcher */}
        <div style={{ padding: '6px 8px' }}>
          <button onClick={onCreatePortfolio} style={{
            width: '100%', padding: '5px', backgroundColor: 'transparent',
            border: `1px dashed ${FINCEPT.BORDER}`, color: FINCEPT.GRAY,
            fontSize: '9px', fontWeight: 600, cursor: 'pointer',
            display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px',
          }}>
            <Plus size={9} /> {t('heatmap.newPortfolio', 'NEW PORTFOLIO')}
          </button>
        </div>
      </div>
    </div>
  );
};

export default HoldingsHeatmap;
