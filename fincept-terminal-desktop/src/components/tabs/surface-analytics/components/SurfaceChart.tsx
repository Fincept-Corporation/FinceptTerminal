/**
 * Surface Analytics - 3D Surface Chart Component
 * Renders Plotly 3D surface visualizations
 * Follows UI Design System (UI_DESIGN_SYSTEM.md)
 */

import React, { Suspense, lazy, useMemo } from 'react';
import { RefreshCw, AlertCircle, TrendingUp } from 'lucide-react';
import type { ChartType, VolatilitySurfaceData, CorrelationMatrixData, YieldCurveData, PCAData } from '../types';
import { FINCEPT_COLORS, TYPOGRAPHY } from '../constants';

// Lazy load Plotly
const Plot = lazy(() => import('react-plotly.js'));

// Color scales as arrays (mutable for Plotly)
const COLOR_SCALES = {
  volatility: [
    [0, '#1a237e'], [0.2, '#1565c0'], [0.4, '#0288d1'],
    [0.6, '#26c6da'], [0.75, '#ffa726'], [0.9, '#ff7043'], [1, '#d32f2f']
  ] as Array<[number, string]>,
  correlation: [
    [0, '#b71c1c'], [0.3, '#e53935'], [0.45, '#ff9800'],
    [0.5, '#ffeb3b'], [0.65, '#7cb342'], [0.8, '#43a047'], [1, '#2e7d32']
  ] as Array<[number, string]>,
  yieldCurve: [
    [0, '#1a237e'], [0.25, '#283593'], [0.4, '#3949ab'],
    [0.55, '#5e35b1'], [0.7, '#7e57c2'], [0.85, '#ba68c8'], [1, '#f06292']
  ] as Array<[number, string]>,
  pca: [
    [0, '#0d47a1'], [0.2, '#1976d2'], [0.4, '#42a5f5'],
    [0.5, '#e0e0e0'], [0.6, '#ef5350'], [0.8, '#e53935'], [1, '#b71c1c']
  ] as Array<[number, string]>,
};

interface SurfaceChartProps {
  chartType: ChartType;
  volatilityData: VolatilitySurfaceData | null;
  correlationData: CorrelationMatrixData | null;
  yieldCurveData: YieldCurveData | null;
  pcaData: PCAData | null;
  loading: boolean;
  error: string | null;
  textColor: string;
  accentColor: string;
  fontFamily: string;
  isDemoMode?: boolean;
}

export const SurfaceChart: React.FC<SurfaceChartProps> = ({
  chartType,
  volatilityData,
  correlationData,
  yieldCurveData,
  pcaData,
  loading,
  error,
  textColor,
  accentColor,
  fontFamily,
  isDemoMode = false,
}) => {
  const plotConfig = useMemo(() => ({
    displayModeBar: true,
    displaylogo: false,
    modeBarButtonsToRemove: ['select2d', 'lasso2d'] as any[],
    responsive: true,
  }), []);

  const colorbarConfig = useMemo(() => ({
    tickfont: { size: 10, color: textColor },
    len: 0.85,
    thickness: 18,
    x: 1.02,
    bgcolor: FINCEPT_COLORS.BLACK,
    bordercolor: accentColor,
    borderwidth: 1,
  }), [textColor, accentColor]);

  const sceneConfig = useMemo(() => ({
    bgcolor: FINCEPT_COLORS.BLACK,
    xaxis: {
      gridcolor: FINCEPT_COLORS.BORDER,
      color: textColor,
      titlefont: { size: 12, color: accentColor },
      tickfont: { size: 10 },
      showbackground: true,
      backgroundcolor: FINCEPT_COLORS.PANEL_BG,
    },
    yaxis: {
      gridcolor: FINCEPT_COLORS.BORDER,
      color: textColor,
      titlefont: { size: 12, color: accentColor },
      tickfont: { size: 10 },
      showbackground: true,
      backgroundcolor: FINCEPT_COLORS.PANEL_BG,
    },
    zaxis: {
      gridcolor: FINCEPT_COLORS.BORDER,
      color: textColor,
      titlefont: { size: 12, color: accentColor },
      tickfont: { size: 10 },
      showbackground: true,
      backgroundcolor: FINCEPT_COLORS.PANEL_BG,
    },
    camera: {
      eye: { x: 1.6, y: 1.6, z: 1.4 },
      center: { x: 0, y: 0, z: 0 },
    },
  }), [textColor, accentColor]);

  const baseLayout = useMemo(() => ({
    autosize: true,
    font: { family: fontFamily, color: textColor, size: 11 },
    paper_bgcolor: 'transparent',
    plot_bgcolor: 'transparent',
    margin: { l: 10, r: 10, t: 10, b: 10 },
    hoverlabel: {
      bgcolor: FINCEPT_COLORS.BORDER,
      bordercolor: accentColor,
      font: { family: fontFamily, color: textColor, size: 11 },
    },
  }), [textColor, accentColor, fontFamily]);

  const renderVolatilitySurface = () => {
    if (!volatilityData) return null;

    return (
      <Plot
        data={[{
          type: 'surface' as const,
          x: volatilityData.strikes,
          y: volatilityData.expirations,
          z: volatilityData.z,
          colorscale: COLOR_SCALES.volatility,
          colorbar: {
            title: { text: 'IV %', font: { size: 11, color: accentColor } },
            ...colorbarConfig,
          },
          hovertemplate: '<b>STRIKE:</b> %{x:.1f}<br><b>DTE:</b> %{y:.0f}<br><b>IV:</b> %{z:.2f}%<extra></extra>'
        } as any]}
        layout={{
          ...baseLayout,
          scene: {
            ...sceneConfig,
            xaxis: { ...sceneConfig.xaxis, title: 'STRIKE PRICE' },
            yaxis: { ...sceneConfig.yaxis, title: 'DAYS TO EXPIRY' },
            zaxis: { ...sceneConfig.zaxis, title: 'IMPLIED VOL (%)' },
          }
        } as any}
        config={plotConfig}
        style={{ width: '100%', height: '100%' }}
        useResizeHandler={true}
      />
    );
  };

  const renderCorrelationMatrix = () => {
    if (!correlationData) return null;

    return (
      <Plot
        data={[{
          type: 'surface' as const,
          z: correlationData.z,
          colorscale: COLOR_SCALES.correlation,
          colorbar: {
            title: { text: 'ρ', font: { size: 12, color: accentColor } },
            ...colorbarConfig,
          },
          hovertemplate: '<b>CORRELATION:</b> %{z:.3f}<extra></extra>'
        } as any]}
        layout={{
          ...baseLayout,
          scene: {
            ...sceneConfig,
            xaxis: { ...sceneConfig.xaxis, title: 'ASSET PAIR INDEX' },
            yaxis: { ...sceneConfig.yaxis, title: 'TIME (DAYS)' },
            zaxis: { ...sceneConfig.zaxis, title: 'CORRELATION COEFF' },
          }
        } as any}
        config={plotConfig}
        style={{ width: '100%', height: '100%' }}
        useResizeHandler={true}
      />
    );
  };

  const renderYieldCurve = () => {
    if (!yieldCurveData) return null;

    return (
      <Plot
        data={[{
          type: 'surface' as const,
          x: yieldCurveData.maturities,
          y: yieldCurveData.timePoints,
          z: yieldCurveData.z,
          colorscale: COLOR_SCALES.yieldCurve,
          colorbar: {
            title: { text: 'YIELD %', font: { size: 11, color: accentColor } },
            ...colorbarConfig,
          },
          hovertemplate: '<b>MATURITY:</b> %{x}M<br><b>YIELD:</b> %{z:.2f}%<extra></extra>'
        } as any]}
        layout={{
          ...baseLayout,
          scene: {
            ...sceneConfig,
            xaxis: { ...sceneConfig.xaxis, title: 'MATURITY (MONTHS)' },
            yaxis: { ...sceneConfig.yaxis, title: 'HISTORICAL DAYS' },
            zaxis: { ...sceneConfig.zaxis, title: 'YIELD (%)' },
          }
        } as any}
        config={plotConfig}
        style={{ width: '100%', height: '100%' }}
        useResizeHandler={true}
      />
    );
  };

  const renderPCA = () => {
    if (!pcaData) return null;

    return (
      <Plot
        data={[{
          type: 'surface' as const,
          z: pcaData.z,
          colorscale: COLOR_SCALES.pca,
          colorbar: {
            title: { text: 'LOADING', font: { size: 11, color: accentColor } },
            ...colorbarConfig,
          },
          hovertemplate: '<b>LOADING:</b> %{z:.3f}<extra></extra>'
        } as any]}
        layout={{
          ...baseLayout,
          scene: {
            ...sceneConfig,
            xaxis: { ...sceneConfig.xaxis, title: 'PRINCIPAL COMPONENT' },
            yaxis: { ...sceneConfig.yaxis, title: 'ASSET INDEX' },
            zaxis: { ...sceneConfig.zaxis, title: 'FACTOR LOADING' },
          }
        } as any}
        config={plotConfig}
        style={{ width: '100%', height: '100%' }}
        useResizeHandler={true}
      />
    );
  };

  const getChartTitle = (): string => {
    switch (chartType) {
      case 'volatility':
        return volatilityData
          ? `${volatilityData.underlying} OPTIONS - IMPLIED VOLATILITY SURFACE`
          : 'IMPLIED VOLATILITY SURFACE';
      case 'correlation':
        return correlationData
          ? `MULTI-ASSET CORRELATION MATRIX - ${correlationData.window} DAY ROLLING`
          : 'MULTI-ASSET CORRELATION MATRIX';
      case 'yield-curve':
        return 'U.S. TREASURY YIELD CURVE - HISTORICAL EVOLUTION';
      case 'pca':
        return 'PRINCIPAL COMPONENT ANALYSIS - FACTOR LOADINGS';
      default:
        return '';
    }
  };

  const getDataSource = (): string => {
    if (isDemoMode) {
      return 'DEMO DATA (SYNTHETIC)';
    }
    switch (chartType) {
      case 'volatility':
        return volatilityData ? `${volatilityData.underlying} | DATABENTO OPRA` : 'DATABENTO OPRA';
      case 'correlation':
        return 'MULTI-ASSET | DATABENTO';
      case 'yield-curve':
        return yieldCurveData?.source || 'UST | FEDERAL RESERVE';
      case 'pca':
        return 'PORTFOLIO | COMPUTED';
      default:
        return '';
    }
  };

  const renderContent = () => {
    if (loading) {
      return (
        <div style={{
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          justifyContent: 'center',
          height: '100%',
        }}>
          <RefreshCw size={32} className="animate-spin" style={{ color: accentColor }} />
          <span style={{
            marginTop: '16px',
            fontSize: TYPOGRAPHY.BODY_SIZE,
            color: FINCEPT_COLORS.MUTED,
            fontFamily: TYPOGRAPHY.FONT_FAMILY,
            letterSpacing: '0.5px',
          }}>
            LOADING DATA...
          </span>
        </div>
      );
    }

    // Check if we have data to display
    const hasData = (chartType === 'volatility' && volatilityData) ||
                    (chartType === 'correlation' && correlationData) ||
                    (chartType === 'yield-curve' && yieldCurveData) ||
                    (chartType === 'pca' && pcaData);

    // If we have data, show the chart (even if there was an error - demo data fallback)
    if (hasData) {
      return (
        <Suspense
          fallback={
            <div style={{
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              height: '100%',
            }}>
              <RefreshCw size={24} className="animate-spin" style={{ color: accentColor }} />
            </div>
          }
        >
          {chartType === 'volatility' && renderVolatilitySurface()}
          {chartType === 'correlation' && renderCorrelationMatrix()}
          {chartType === 'yield-curve' && renderYieldCurve()}
          {chartType === 'pca' && renderPCA()}
        </Suspense>
      );
    }

    // Only show error if we have no data
    if (error) {
      // Parse error for user-friendly messages
      const isAuthError = error.toLowerCase().includes('401') || error.toLowerCase().includes('auth');
      const isAccessError = error.toLowerCase().includes('403') || error.toLowerCase().includes('access');
      const isNotFoundError = error.toLowerCase().includes('404') || error.toLowerCase().includes('not found');

      let errorTitle = 'FAILED TO LOAD DATA';
      let errorHelp = '';

      if (isAuthError) {
        errorTitle = 'AUTHENTICATION FAILED';
        errorHelp = 'Your Databento API key may be invalid, expired, or missing required permissions. Please verify your key at databento.com/portal/api-keys';
      } else if (isAccessError) {
        errorTitle = 'ACCESS DENIED';
        errorHelp = 'Your API key may not have access to this dataset. OPRA options data requires specific permissions.';
      } else if (isNotFoundError) {
        errorTitle = 'SYMBOL NOT FOUND';
        errorHelp = 'The requested symbol was not found. Try a different symbol like SPY, AAPL, or QQQ.';
      }

      return (
        <div style={{
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          justifyContent: 'center',
          height: '100%',
          padding: '24px',
        }}>
          <div style={{
            width: '48px',
            height: '48px',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            backgroundColor: `${FINCEPT_COLORS.RED}20`,
            borderRadius: '50%',
            marginBottom: '16px',
          }}>
            <AlertCircle size={24} style={{ color: FINCEPT_COLORS.RED }} />
          </div>
          <div style={{
            fontSize: TYPOGRAPHY.HEADER_SIZE,
            fontWeight: 700,
            color: FINCEPT_COLORS.RED,
            marginBottom: '8px',
          }}>
            {errorTitle}
          </div>
          {errorHelp && (
            <span style={{
              fontSize: TYPOGRAPHY.BODY_SIZE,
              color: FINCEPT_COLORS.YELLOW,
              textAlign: 'center',
              maxWidth: '500px',
              lineHeight: '1.5',
              marginBottom: '12px',
            }}>
              {errorHelp}
            </span>
          )}
          <span style={{
            fontSize: '9px',
            color: FINCEPT_COLORS.MUTED,
            textAlign: 'center',
            maxWidth: '500px',
            lineHeight: '1.5',
            padding: '8px',
            backgroundColor: FINCEPT_COLORS.DARK_BG,
            border: `1px solid ${FINCEPT_COLORS.BORDER}`,
            borderRadius: '2px',
            fontFamily: 'monospace',
          }}>
            {error}
          </span>
        </div>
      );
    }

    // Show empty state if no data and no error
    return (
      <div style={{
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        justifyContent: 'center',
        height: '100%',
        color: FINCEPT_COLORS.MUTED,
        fontSize: TYPOGRAPHY.BODY_SIZE,
        textAlign: 'center',
      }}>
        <TrendingUp size={48} style={{ marginBottom: '16px', opacity: 0.3 }} />
        <div style={{ fontWeight: 700, marginBottom: '8px' }}>NO DATA LOADED</div>
        <span>Click REFRESH to fetch data</span>
      </div>
    );
  };

  return (
    <div style={{
      flex: 1,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden',
      minWidth: 0,
    }}>
      {/* Chart Header */}
      <div
        style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          padding: '8px 12px',
          backgroundColor: FINCEPT_COLORS.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT_COLORS.BORDER}`,
          flexShrink: 0,
        }}
      >
        <div style={{
          fontSize: TYPOGRAPHY.LABEL_SIZE,
          fontWeight: 700,
          color: accentColor,
          letterSpacing: '0.5px',
          fontFamily: TYPOGRAPHY.FONT_FAMILY,
        }}>
          {getChartTitle()}
        </div>
        <div style={{
          fontSize: TYPOGRAPHY.LABEL_SIZE,
          color: FINCEPT_COLORS.CYAN,
          fontFamily: TYPOGRAPHY.FONT_FAMILY,
        }}>
          {getDataSource()}
        </div>
      </div>

      {/* Chart Area */}
      <div style={{
        flex: 1,
        overflow: 'hidden',
        backgroundColor: FINCEPT_COLORS.BLACK,
      }}>
        {renderContent()}
      </div>
    </div>
  );
};
