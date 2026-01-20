import React, { useState, useMemo } from 'react';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { TabFooter } from '@/components/common/TabFooter';
import { useTranslation } from 'react-i18next';
// @ts-ignore - react-plotly.js types are incomplete
import Plot from 'react-plotly.js';
import { RefreshCw, TrendingUp, BarChart3, Activity, Network } from 'lucide-react';

type ChartType = 'volatility' | 'correlation' | 'yield-curve' | 'pca';

const Visualization3DTab: React.FC = () => {
  const { t } = useTranslation();
  const { colors, fontSize, fontFamily } = useTerminalTheme();
  const [lastUpdate, setLastUpdate] = useState(new Date());
  const [activeChart, setActiveChart] = useState<ChartType>('volatility');

  // Generate dummy volatility surface data
  const volatilitySurface = useMemo(() => {
    const strikes = Array.from({ length: 20 }, (_, i) => 85 + i * 1.5);
    const expirations = Array.from({ length: 18 }, (_, i) => 7 + i * 21);
    const z: number[][] = [];

    for (let i = 0; i < expirations.length; i++) {
      const row: number[] = [];
      const maturity = expirations[i];
      for (let j = 0; j < strikes.length; j++) {
        const strike = strikes[j];
        const moneyness = strike / 100;
        const baseVol = 0.22;
        const skew = 0.18 * Math.exp(-moneyness);
        const smile = 0.06 * Math.pow(moneyness - 1, 2);
        const termStructure = 0.04 * Math.exp(-maturity / 180);
        const impliedVol = (baseVol + skew + smile + termStructure) * 100;
        row.push(impliedVol);
      }
      z.push(row);
    }
    return { strikes, expirations, z };
  }, []);

  // Generate correlation matrix over time (3D)
  const correlationMatrix = useMemo(() => {
    const assets = ['SPY', 'QQQ', 'IWM', 'GLD', 'TLT', 'VIX', 'EEM', 'HYG'];
    const timePoints = Array.from({ length: 20 }, (_, i) => i * 20);
    const z: number[][] = [];

    for (let t = 0; t < timePoints.length; t++) {
      const row: number[] = [];
      for (let i = 0; i < assets.length; i++) {
        for (let j = 0; j < assets.length; j++) {
          if (i === j) {
            row.push(1.0);
          } else {
            const baseCorr = 0.25 + 0.5 * Math.random();
            const timeFactor = Math.sin(t / 4) * 0.25;
            row.push(Math.max(-1, Math.min(1, baseCorr + timeFactor)));
          }
        }
      }
      z.push(row);
    }
    return { assets, timePoints, z };
  }, []);

  // Generate yield curve surface
  const yieldCurve = useMemo(() => {
    const maturities = [1, 2, 3, 6, 12, 24, 36, 60, 84, 120, 180, 240, 360];
    const timePoints = Array.from({ length: 30 }, (_, i) => i * 12);
    const z: number[][] = [];

    for (let t = 0; t < timePoints.length; t++) {
      const row: number[] = [];
      const trend = 2.8 + Math.sin(t / 6) * 1.8;
      for (let m = 0; m < maturities.length; m++) {
        const maturity = maturities[m];
        const yieldRate = trend + Math.log(maturity / 12 + 1) * 0.9 + (Math.random() - 0.5) * 0.4;
        row.push(yieldRate);
      }
      z.push(row);
    }
    return { maturities, timePoints, z };
  }, []);

  // Generate PCA variance
  const pcaVariance = useMemo(() => {
    const factors = Array.from({ length: 12 }, (_, i) => `PC${i + 1}`);
    const assets = Array.from({ length: 10 }, (_, i) => `Asset${i + 1}`);
    const z: number[][] = [];

    for (let a = 0; a < assets.length; a++) {
      const row: number[] = [];
      for (let f = 0; f < factors.length; f++) {
        const loading = Math.exp(-f / 2.5) * (Math.random() - 0.5) * 2.5;
        row.push(loading);
      }
      z.push(row);
    }
    return { factors, assets, z };
  }, []);

  const handleRefresh = () => {
    setLastUpdate(new Date());
  };

  const chartConfig = {
    displayModeBar: true,
    displaylogo: false,
    modeBarButtonsToRemove: ['select2d', 'lasso2d'],
    responsive: true
  };

  const chartMetrics = {
    volatility: [
      { label: 'ATM VOL', value: '22.4%', change: '+1.2%', positive: true },
      { label: '30D SKEW', value: '-8.5%', change: '-0.3%', positive: false },
      { label: 'TERM STRUCT', value: 'FLAT', change: 'NEUTRAL', positive: null },
      { label: 'IV RANK', value: '68.2', change: '+5.1', positive: true }
    ],
    correlation: [
      { label: 'AVG CORR', value: '0.58', change: '+0.12', positive: false },
      { label: 'MAX CORR', value: '0.94', change: '+0.05', positive: false },
      { label: 'MIN CORR', value: '-0.32', change: '-0.08', positive: true },
      { label: 'DISPERSION', value: '0.31', change: '+0.04', positive: true }
    ],
    'yield-curve': [
      { label: '2Y YIELD', value: '4.12%', change: '+8bp', positive: true },
      { label: '10Y YIELD', value: '4.28%', change: '+12bp', positive: true },
      { label: '2-10 SPREAD', value: '16bp', change: '+4bp', positive: true },
      { label: 'INVERSION', value: 'NONE', change: 'NORMAL', positive: null }
    ],
    pca: [
      { label: 'PC1 VAR', value: '68.3%', change: '+2.1%', positive: true },
      { label: 'PC2 VAR', value: '18.7%', change: '-0.5%', positive: false },
      { label: 'PC3 VAR', value: '7.2%', change: '+0.3%', positive: true },
      { label: 'TOTAL VAR', value: '94.2%', change: '+1.9%', positive: true }
    ]
  };

  const renderChart = () => {
    const commonLayout = {
      autosize: true,
      font: { family: fontFamily, color: colors.text, size: 11 },
      paper_bgcolor: 'transparent',
      plot_bgcolor: 'transparent',
      margin: { l: 10, r: 10, t: 10, b: 10 },
      scene: {
        bgcolor: '#000000',
        xaxis: {
          gridcolor: '#1a1a1a',
          color: colors.text,
          titlefont: { size: 12, color: colors.accent },
          tickfont: { size: 10 },
          showbackground: true,
          backgroundcolor: '#0a0a0a'
        },
        yaxis: {
          gridcolor: '#1a1a1a',
          color: colors.text,
          titlefont: { size: 12, color: colors.accent },
          tickfont: { size: 10 },
          showbackground: true,
          backgroundcolor: '#0a0a0a'
        },
        zaxis: {
          gridcolor: '#1a1a1a',
          color: colors.text,
          titlefont: { size: 12, color: colors.accent },
          tickfont: { size: 10 },
          showbackground: true,
          backgroundcolor: '#0a0a0a'
        },
        camera: {
          eye: { x: 1.6, y: 1.6, z: 1.4 },
          center: { x: 0, y: 0, z: 0 }
        }
      },
      hoverlabel: {
        bgcolor: '#1a1a1a',
        bordercolor: colors.accent,
        font: { family: fontFamily, color: colors.text, size: 11 }
      }
    };

    switch (activeChart) {
      case 'volatility':
        return (
          <Plot
            data={[{
              type: 'surface',
              x: volatilitySurface.strikes,
              y: volatilitySurface.expirations,
              z: volatilitySurface.z,
              colorscale: [
                [0, '#1a237e'], [0.2, '#1565c0'], [0.4, '#0288d1'],
                [0.6, '#26c6da'], [0.75, '#ffa726'], [0.9, '#ff7043'], [1, '#d32f2f']
              ],
              contours: {
                z: { show: true, usecolormap: true, highlightcolor: colors.accent, project: { z: true } }
              },
              colorbar: {
                title: { text: 'IV %', font: { size: 11, color: colors.accent } },
                tickfont: { size: 10, color: colors.text },
                len: 0.85,
                thickness: 18,
                x: 1.02,
                bgcolor: '#000000',
                bordercolor: colors.accent,
                borderwidth: 1,
                outlinecolor: colors.accent,
                outlinewidth: 1
              },
              hovertemplate: '<b>STRIKE:</b> %{x:.1f}<br><b>DTE:</b> %{y:.0f}<br><b>IV:</b> %{z:.2f}%<extra></extra>'
            }]}
            layout={{
              ...commonLayout,
              scene: {
                ...commonLayout.scene,
                xaxis: { ...commonLayout.scene.xaxis, title: 'STRIKE PRICE' },
                yaxis: { ...commonLayout.scene.yaxis, title: 'DAYS TO EXPIRY' },
                zaxis: { ...commonLayout.scene.zaxis, title: 'IMPLIED VOL (%)' }
              }
            }}
            config={chartConfig}
            style={{ width: '100%', height: '100%' }}
            useResizeHandler={true}
          />
        );

      case 'correlation':
        return (
          <Plot
            data={[{
              type: 'surface',
              z: correlationMatrix.z,
              colorscale: [
                [0, '#b71c1c'], [0.3, '#e53935'], [0.45, '#ff9800'],
                [0.5, '#ffeb3b'], [0.65, '#7cb342'], [0.8, '#43a047'], [1, '#2e7d32']
              ],
              contours: {
                z: { show: true, usecolormap: true, highlightcolor: colors.accent, project: { z: true } }
              },
              colorbar: {
                title: { text: 'ρ', font: { size: 12, color: colors.accent } },
                tickfont: { size: 10, color: colors.text },
                len: 0.85,
                thickness: 18,
                x: 1.02,
                bgcolor: '#000000',
                bordercolor: colors.accent,
                borderwidth: 1
              },
              hovertemplate: '<b>CORRELATION:</b> %{z:.3f}<extra></extra>'
            }]}
            layout={{
              ...commonLayout,
              scene: {
                ...commonLayout.scene,
                xaxis: { ...commonLayout.scene.xaxis, title: 'ASSET PAIR INDEX' },
                yaxis: { ...commonLayout.scene.yaxis, title: 'TIME (DAYS)' },
                zaxis: { ...commonLayout.scene.zaxis, title: 'CORRELATION COEFF' }
              }
            }}
            config={chartConfig}
            style={{ width: '100%', height: '100%' }}
            useResizeHandler={true}
          />
        );

      case 'yield-curve':
        return (
          <Plot
            data={[{
              type: 'surface',
              x: yieldCurve.maturities,
              y: yieldCurve.timePoints,
              z: yieldCurve.z,
              colorscale: [
                [0, '#1a237e'], [0.25, '#283593'], [0.4, '#3949ab'],
                [0.55, '#5e35b1'], [0.7, '#7e57c2'], [0.85, '#ba68c8'], [1, '#f06292']
              ],
              contours: {
                z: { show: true, usecolormap: true, highlightcolor: colors.accent, project: { z: true } }
              },
              colorbar: {
                title: { text: 'YIELD %', font: { size: 11, color: colors.accent } },
                tickfont: { size: 10, color: colors.text },
                len: 0.85,
                thickness: 18,
                x: 1.02,
                bgcolor: '#000000',
                bordercolor: colors.accent,
                borderwidth: 1
              },
              hovertemplate: '<b>MATURITY:</b> %{x}M<br><b>YIELD:</b> %{z:.2f}%<extra></extra>'
            }]}
            layout={{
              ...commonLayout,
              scene: {
                ...commonLayout.scene,
                xaxis: { ...commonLayout.scene.xaxis, title: 'MATURITY (MONTHS)' },
                yaxis: { ...commonLayout.scene.yaxis, title: 'HISTORICAL DAYS' },
                zaxis: { ...commonLayout.scene.zaxis, title: 'YIELD (%)' }
              }
            }}
            config={chartConfig}
            style={{ width: '100%', height: '100%' }}
            useResizeHandler={true}
          />
        );

      case 'pca':
        return (
          <Plot
            data={[{
              type: 'surface',
              z: pcaVariance.z,
              colorscale: [
                [0, '#0d47a1'], [0.2, '#1976d2'], [0.4, '#42a5f5'],
                [0.5, '#e0e0e0'], [0.6, '#ef5350'], [0.8, '#e53935'], [1, '#b71c1c']
              ],
              contours: {
                z: { show: true, usecolormap: true, highlightcolor: colors.accent, project: { z: true } }
              },
              colorbar: {
                title: { text: 'LOADING', font: { size: 11, color: colors.accent } },
                tickfont: { size: 10, color: colors.text },
                len: 0.85,
                thickness: 18,
                x: 1.02,
                bgcolor: '#000000',
                bordercolor: colors.accent,
                borderwidth: 1
              },
              hovertemplate: '<b>LOADING:</b> %{z:.3f}<extra></extra>'
            }]}
            layout={{
              ...commonLayout,
              scene: {
                ...commonLayout.scene,
                xaxis: { ...commonLayout.scene.xaxis, title: 'PRINCIPAL COMPONENT' },
                yaxis: { ...commonLayout.scene.yaxis, title: 'ASSET INDEX' },
                zaxis: { ...commonLayout.scene.zaxis, title: 'FACTOR LOADING' }
              }
            }}
            config={chartConfig}
            style={{ width: '100%', height: '100%' }}
            useResizeHandler={true}
          />
        );
    }
  };

  const currentMetrics = chartMetrics[activeChart];

  return (
    <div className="h-full flex flex-col" style={{ backgroundColor: '#000000', color: colors.text, fontFamily, overflow: 'hidden' }}>
      {/* Top Control Bar */}
      <div className="flex items-center justify-between px-3 py-1 border-b flex-shrink-0" style={{ borderColor: '#1a1a1a', backgroundColor: '#0a0a0a' }}>
        <div className="flex items-center gap-4">
          <div className="flex items-center gap-2">
            <BarChart3 size={14} style={{ color: colors.accent }} />
            <span className="text-xs font-bold" style={{ color: colors.accent }}>3D ANALYTICS</span>
          </div>
          <div className="h-3 w-px" style={{ backgroundColor: '#333' }} />
          <div className="flex gap-1">
            <button
              onClick={() => setActiveChart('volatility')}
              className="px-3 py-0.5 text-xs font-semibold transition-all"
              style={{
                backgroundColor: activeChart === 'volatility' ? colors.accent : '#1a1a1a',
                color: activeChart === 'volatility' ? '#000' : colors.text,
                border: `1px solid ${activeChart === 'volatility' ? colors.accent : '#333'}`
              }}
            >
              <div className="flex items-center gap-1">
                <TrendingUp size={12} />
                VOL SURFACE
              </div>
            </button>
            <button
              onClick={() => setActiveChart('correlation')}
              className="px-3 py-0.5 text-xs font-semibold transition-all"
              style={{
                backgroundColor: activeChart === 'correlation' ? colors.accent : '#1a1a1a',
                color: activeChart === 'correlation' ? '#000' : colors.text,
                border: `1px solid ${activeChart === 'correlation' ? colors.accent : '#333'}`
              }}
            >
              <div className="flex items-center gap-1">
                <Network size={12} />
                CORRELATION
              </div>
            </button>
            <button
              onClick={() => setActiveChart('yield-curve')}
              className="px-3 py-0.5 text-xs font-semibold transition-all"
              style={{
                backgroundColor: activeChart === 'yield-curve' ? colors.accent : '#1a1a1a',
                color: activeChart === 'yield-curve' ? '#000' : colors.text,
                border: `1px solid ${activeChart === 'yield-curve' ? colors.accent : '#333'}`
              }}
            >
              <div className="flex items-center gap-1">
                <Activity size={12} />
                YIELD CURVE
              </div>
            </button>
            <button
              onClick={() => setActiveChart('pca')}
              className="px-3 py-0.5 text-xs font-semibold transition-all"
              style={{
                backgroundColor: activeChart === 'pca' ? colors.accent : '#1a1a1a',
                color: activeChart === 'pca' ? '#000' : colors.text,
                border: `1px solid ${activeChart === 'pca' ? colors.accent : '#333'}`
              }}
            >
              <div className="flex items-center gap-1">
                <BarChart3 size={12} />
                PCA FACTORS
              </div>
            </button>
          </div>
        </div>
        <div className="flex items-center gap-3">
          <span className="text-xs" style={{ color: '#666' }}>
            LAST UPDATE: <span style={{ color: colors.text }}>{lastUpdate.toLocaleTimeString()}</span>
          </span>
          <button
            onClick={handleRefresh}
            className="flex items-center gap-1 px-2 py-0.5 text-xs font-semibold transition-all"
            style={{ backgroundColor: colors.accent, color: '#000', border: `1px solid ${colors.accent}` }}
          >
            <RefreshCw size={12} />
            REFRESH
          </button>
        </div>
      </div>

      {/* Main Content Area */}
      <div className="flex-1 flex overflow-hidden" style={{ minHeight: 0 }}>
        {/* Left Metrics Panel */}
        <div className="w-64 border-r flex flex-col flex-shrink-0" style={{ borderColor: '#1a1a1a', backgroundColor: '#0a0a0a' }}>
          <div className="px-3 py-2 border-b" style={{ borderColor: '#1a1a1a' }}>
            <div className="text-xs font-bold" style={{ color: colors.accent }}>MARKET METRICS</div>
          </div>
          <div className="flex-1 overflow-y-auto">
            {currentMetrics.map((metric, idx) => (
              <div
                key={idx}
                className="px-3 py-2 border-b"
                style={{ borderColor: '#1a1a1a' }}
              >
                <div className="text-xs" style={{ color: '#666' }}>{metric.label}</div>
                <div className="flex items-baseline justify-between mt-1">
                  <span className="text-base font-bold" style={{ color: colors.text }}>{metric.value}</span>
                  <span
                    className="text-xs font-semibold"
                    style={{
                      color: metric.positive === null ? '#666' : metric.positive ? '#22c55e' : '#ef4444'
                    }}
                  >
                    {metric.change}
                  </span>
                </div>
              </div>
            ))}
            <div className="px-3 py-2 mt-4">
              <div className="text-xs font-bold mb-2" style={{ color: colors.accent }}>DATA INFO</div>
              <div className="space-y-2 text-xs" style={{ color: '#666' }}>
                <div className="flex justify-between">
                  <span>SOURCE:</span>
                  <span style={{ color: colors.text }}>LIVE</span>
                </div>
                <div className="flex justify-between">
                  <span>FREQUENCY:</span>
                  <span style={{ color: colors.text }}>1MIN</span>
                </div>
                <div className="flex justify-between">
                  <span>QUALITY:</span>
                  <span style={{ color: '#22c55e' }}>99.8%</span>
                </div>
              </div>
            </div>
          </div>
        </div>

        {/* 3D Chart Display */}
        <div className="flex-1 flex flex-col overflow-hidden" style={{ minWidth: 0 }}>
          <div className="px-3 py-1 border-b flex items-center justify-between flex-shrink-0" style={{ borderColor: '#1a1a1a', backgroundColor: '#0a0a0a' }}>
            <div className="text-xs font-bold" style={{ color: colors.accent }}>
              {activeChart === 'volatility' && 'S&P 500 INDEX OPTIONS - IMPLIED VOLATILITY SURFACE'}
              {activeChart === 'correlation' && 'MULTI-ASSET CORRELATION MATRIX - ROLLING 30 DAY'}
              {activeChart === 'yield-curve' && 'U.S. TREASURY YIELD CURVE - HISTORICAL EVOLUTION'}
              {activeChart === 'pca' && 'PRINCIPAL COMPONENT ANALYSIS - FACTOR LOADINGS'}
            </div>
            <div className="text-xs" style={{ color: '#666' }}>
              {activeChart === 'volatility' && 'SPX | CBOE'}
              {activeChart === 'correlation' && 'MULTI-ASSET | LIVE'}
              {activeChart === 'yield-curve' && 'UST | FEDERAL RESERVE'}
              {activeChart === 'pca' && 'PORTFOLIO | COMPUTED'}
            </div>
          </div>
          <div className="flex-1 overflow-hidden" style={{ backgroundColor: '#000000' }}>
            {renderChart()}
          </div>
        </div>
      </div>

      {/* Footer */}
      <div className="flex-shrink-0">
        <TabFooter
          tabName="3D VISUALIZATION"
          statusInfo={`${activeChart.toUpperCase()} | ${currentMetrics.length} Metrics | Updated: ${lastUpdate.toLocaleTimeString()}`}
        />
      </div>
    </div>
  );
};

export default Visualization3DTab;
