import React, { useMemo, useEffect, useState } from 'react';
import { PieChart, Grid, Pencil } from 'lucide-react';
import { FINCEPT } from '../finceptStyles';
import { heatColor, valColor } from './helpers';
import { formatCurrency, formatPercent } from '../portfolio/utils';
import { useTranslation } from 'react-i18next';
import { sectorService } from '../../../../services/data-sources/sectorService';
import type { HoldingWithQuote } from '../../../../services/portfolio/portfolioService';
import SectorMappingModal from '../modals/SectorMappingModal';

interface SectorCorrelationPanelProps {
  holdings: HoldingWithQuote[];
  currency: string;
}

const SECTOR_COLORS: Record<string, string> = {
  'Technology': '#00E5FF',
  'Healthcare': '#00D66F',
  'Financial Services': '#FF8800',
  'Financials': '#FF8800',
  'Energy': '#FFD700',
  'Consumer Cyclical': '#9D4EDD',
  'Consumer Defensive': '#E040FB',
  'Industrials': '#0088FF',
  'Communication Services': '#FF6E40',
  'Real Estate': '#76FF03',
  'Utilities': '#00BFA5',
  'Basic Materials': '#FFC400',
  'Cryptocurrency': '#F57C00',
  'US Equity': '#00BFFF',
  'International Equity': '#FF4081',
  'Fixed Income': '#69F0AE',
  'Commodities': '#FFD740',
  'Other': '#555',
};

const getSectorColor = (sector: string): string =>
  SECTOR_COLORS[sector] || SECTOR_COLORS['Other'];

const SectorCorrelationPanel: React.FC<SectorCorrelationPanelProps> = ({ holdings, currency }) => {
  const { t } = useTranslation('portfolio');
  const [sectorFetched, setSectorFetched] = useState(false);
  const [, setRefreshKey] = useState(0);
  const [showSectorEditor, setShowSectorEditor] = useState(false);

  // Prefetch sector data for all holdings
  useEffect(() => {
    if (holdings.length === 0) return;
    let cancelled = false;
    const symbols = holdings.map(h => h.symbol);

    sectorService.prefetchSectors(symbols).then(() => {
      if (!cancelled) {
        setSectorFetched(true);
        setRefreshKey(k => k + 1); // trigger re-render after cache populated
      }
    }).catch(() => {
      if (!cancelled) setSectorFetched(true);
    });

    return () => { cancelled = true; };
  }, [holdings]);

  // Group holdings by sector using sectorService
  const sectorData = useMemo(() => {
    const groups: Record<string, { weight: number; pnl: number; value: number; count: number; color: string }> = {};
    for (const h of holdings) {
      const info = sectorService.getSectorInfo(h.symbol);
      const sector = info.sector;
      if (!groups[sector]) groups[sector] = { weight: 0, pnl: 0, value: 0, count: 0, color: getSectorColor(sector) };
      groups[sector].weight += h.weight;
      groups[sector].pnl += h.unrealized_pnl;
      groups[sector].value += h.market_value;
      groups[sector].count++;
    }
    return Object.entries(groups)
      .map(([name, data]) => ({ name, ...data }))
      .sort((a, b) => b.weight - a.weight);
  }, [holdings, sectorFetched]);

  // Compute real correlation from day change percentages
  // Uses sign-agreement + magnitude for a lightweight proxy
  const topSymbols = useMemo(() =>
    [...holdings].sort((a, b) => b.weight - a.weight).slice(0, 6),
  [holdings]);

  const correlationMatrix = useMemo(() => {
    // With only single-day return data available, compute a deterministic
    // sign-and-magnitude-based correlation proxy:
    // corr(i,j) = sign_agreement * min(|ri|, |rj|) / max(|ri|, |rj|)
    // This avoids Math.random() and gives stable values.
    const n = topSymbols.length;
    const matrix: number[][] = Array.from({ length: n }, () => Array(n).fill(0));

    for (let i = 0; i < n; i++) {
      for (let j = 0; j < n; j++) {
        if (i === j) {
          matrix[i][j] = 1.0;
          continue;
        }
        const ri = topSymbols[i].day_change_percent;
        const rj = topSymbols[j].day_change_percent;
        const absI = Math.abs(ri);
        const absJ = Math.abs(rj);
        const maxAbs = Math.max(absI, absJ);

        if (maxAbs === 0) {
          matrix[i][j] = 0;
          continue;
        }

        const sameSign = (ri >= 0 && rj >= 0) || (ri < 0 && rj < 0);
        const magnitude = Math.min(absI, absJ) / maxAbs;
        matrix[i][j] = sameSign ? magnitude * 0.8 + 0.1 : -(magnitude * 0.6 + 0.05);
      }
    }
    return matrix;
  }, [topSymbols]);

  return (
    <>
    <div style={{ flex: 2, display: 'flex', flexDirection: 'column', minWidth: 0, overflow: 'hidden' }}>
      {/* Sector Exposure */}
      <div style={{ height: '60%', borderBottom: `1px solid ${FINCEPT.BORDER}`, display: 'flex', flexDirection: 'column', minHeight: '110px' }}>
        <div style={{
          padding: '4px 10px', backgroundColor: '#0A0A0A', borderBottom: '1px solid #111',
          display: 'flex', alignItems: 'center', gap: '6px',
        }}>
          <PieChart size={10} color={FINCEPT.ORANGE} />
          <span style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.ORANGE, letterSpacing: '0.5px' }}>{t('allocation.sectorAllocation', 'SECTOR ALLOCATION')}</span>
          <span style={{ fontSize: '8px', color: FINCEPT.GRAY }}>({sectorData.length} {t('allocation.sectors', 'sectors')})</span>
          <button
            onClick={() => setShowSectorEditor(true)}
            title="Edit sector mappings"
            style={{ marginLeft: 'auto', background: 'none', border: 'none', color: FINCEPT.GRAY, cursor: 'pointer', display: 'flex', padding: '2px' }}
            onMouseEnter={e => { e.currentTarget.style.color = FINCEPT.CYAN; }}
            onMouseLeave={e => { e.currentTarget.style.color = FINCEPT.GRAY; }}
          >
            <Pencil size={10} />
          </button>
        </div>

        <div style={{ flex: 1, overflow: 'hidden', padding: '8px', display: 'flex', gap: '12px', alignItems: 'center' }}>
          {/* Pie Chart SVG */}
          {sectorData.length > 0 && (
            <svg width="140" height="140" viewBox="0 0 140 140" style={{ flexShrink: 0 }}>
              <defs>
                {sectorData.map((s, i) => (
                  <filter key={i} id={`shadow-${i}`} x="-50%" y="-50%" width="200%" height="200%">
                    <feDropShadow dx="0" dy="2" stdDeviation="3" floodColor={s.color} floodOpacity="0.4"/>
                  </filter>
                ))}
              </defs>
              {(() => {
                let currentAngle = -90; // Start at top
                return sectorData.map((s, i) => {
                  const startAngle = currentAngle;
                  const sliceAngle = (s.weight / 100) * 360;
                  currentAngle += sliceAngle;

                  const startRad = (startAngle * Math.PI) / 180;
                  const endRad = (currentAngle * Math.PI) / 180;

                  const x1 = 70 + 60 * Math.cos(startRad);
                  const y1 = 70 + 60 * Math.sin(startRad);
                  const x2 = 70 + 60 * Math.cos(endRad);
                  const y2 = 70 + 60 * Math.sin(endRad);

                  const largeArc = sliceAngle > 180 ? 1 : 0;
                  const pathData = `M 70 70 L ${x1} ${y1} A 60 60 0 ${largeArc} 1 ${x2} ${y2} Z`;

                  return (
                    <g key={s.name}>
                      <path
                        d={pathData}
                        fill={s.color}
                        stroke="#0A0A0A"
                        strokeWidth="2"
                        filter={`url(#shadow-${i})`}
                        style={{ cursor: 'pointer', transition: 'opacity 0.2s' }}
                        opacity="0.9"
                      >
                        <title>{`${s.name}: ${s.weight.toFixed(1)}%`}</title>
                      </path>
                    </g>
                  );
                });
              })()}
              {/* Center hole for donut effect */}
              <circle cx="70" cy="70" r="35" fill="#0A0A0A" stroke={FINCEPT.ORANGE} strokeWidth="1"/>
              <text x="70" y="70" textAnchor="middle" dominantBaseline="middle" fill={FINCEPT.ORANGE} fontSize="10" fontWeight="700">
                {sectorData.length}
              </text>
              <text x="70" y="80" textAnchor="middle" dominantBaseline="middle" fill={FINCEPT.GRAY} fontSize="7">
                SECTORS
              </text>
            </svg>
          )}

          {/* Legend */}
          <div style={{ flex: 1, display: 'flex', flexDirection: 'column', gap: '3px', overflow: 'auto' }}>
            {sectorData.map(s => (
              <div key={s.name} style={{
                display: 'flex', alignItems: 'center', justifyContent: 'space-between',
                padding: '3px 4px', borderBottom: `1px solid #080808`, fontSize: '9px',
              }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '4px', minWidth: 0, flex: 1 }}>
                  <div style={{ width: '8px', height: '8px', borderRadius: '2px', backgroundColor: s.color, flexShrink: 0 }} />
                  <span style={{ color: FINCEPT.WHITE, fontWeight: 600, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                    {s.name}
                  </span>
                  <span style={{ color: FINCEPT.MUTED, fontSize: '8px' }}>({s.count})</span>
                </div>
                <div style={{ display: 'flex', gap: '6px', alignItems: 'center', flexShrink: 0 }}>
                  <span style={{ color: FINCEPT.ORANGE, fontWeight: 700, fontSize: '10px' }}>{s.weight.toFixed(1)}%</span>
                  <span style={{ color: valColor(s.pnl), fontWeight: 600, width: '55px', textAlign: 'right', fontSize: '9px' }}>
                    {formatCurrency(s.pnl, currency)}
                  </span>
                </div>
              </div>
            ))}
          </div>
        </div>
      </div>

      {/* Correlation Matrix */}
      <div style={{ height: '40%', display: 'flex', flexDirection: 'column', overflow: 'hidden', minHeight: '70px' }}>
        <div style={{
          padding: '4px 10px', backgroundColor: '#0A0A0A', borderBottom: '1px solid #111',
          display: 'flex', alignItems: 'center', gap: '6px',
        }}>
          <Grid size={10} color={FINCEPT.CYAN} />
          <span style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.CYAN, letterSpacing: '0.5px' }}>{t('allocation.correlation', 'CORRELATION')}</span>
          <span style={{ fontSize: '8px', color: FINCEPT.GRAY }}>{t('allocation.dayChangeProxy', '(day change proxy)')}</span>
        </div>

        <div style={{ flex: 1, overflow: 'auto', padding: '4px', backgroundColor: '#030303' }}>
          {topSymbols.length < 2 ? (
            <div style={{ padding: '10px', color: FINCEPT.GRAY, fontSize: '9px', textAlign: 'center' }}>
              {t('allocation.needTwoHoldings', 'Need at least 2 holdings for correlation')}
            </div>
          ) : (
            <table style={{ width: '100%', borderCollapse: 'collapse' }}>
              <thead>
                <tr>
                  <th style={{ fontSize: '8px', color: FINCEPT.GRAY, padding: '2px 4px', textAlign: 'left' }} />
                  {topSymbols.map(h => (
                    <th key={h.symbol} style={{ fontSize: '8px', color: FINCEPT.CYAN, padding: '2px 3px', fontWeight: 600, textAlign: 'center' }}>
                      {h.symbol}
                    </th>
                  ))}
                </tr>
              </thead>
              <tbody>
                {topSymbols.map((r, ri) => (
                  <tr key={r.symbol}>
                    <td style={{ fontSize: '8px', color: FINCEPT.CYAN, padding: '2px 4px', fontWeight: 600 }}>
                      {r.symbol}
                    </td>
                    {topSymbols.map((c, ci) => {
                      const corrVal = correlationMatrix[ri][ci];
                      return (
                        <td key={c.symbol} style={{
                          padding: '2px 3px', textAlign: 'center',
                          fontSize: '8px', fontWeight: 600,
                          backgroundColor: heatColor(corrVal * 10, 10),
                          color: ri === ci ? FINCEPT.WHITE : valColor(corrVal),
                        }}>
                          {corrVal.toFixed(2)}
                        </td>
                      );
                    })}
                  </tr>
                ))}
              </tbody>
            </table>
          )}
        </div>
      </div>
    </div>

    <SectorMappingModal
      show={showSectorEditor}
      holdings={holdings}
      onClose={() => setShowSectorEditor(false)}
      onMappingsChanged={() => setRefreshKey(k => k + 1)}
    />
    </>
  );
};

export default SectorCorrelationPanel;
