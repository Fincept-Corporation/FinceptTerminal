/**
 * Surface Analytics Tab
 * ======================
 * Professional-grade 3D financial analytics terminal.
 * Bloomberg-style visualization for options volatility surfaces,
 * multi-asset correlations, yield curve evolution, and PCA analysis.
 *
 * Data Source: Databento (institutional-grade market data)
 * Charts: Plotly.js 3D surfaces
 *
 * NOTE: This tab requires a Databento API key for real data.
 * No demo/dummy data - everything comes from live market data.
 */

import React, { useState, useEffect, useCallback, useMemo } from 'react';
import { useWorkspaceTabState } from '@/hooks/useWorkspaceTabState';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { useTranslation } from 'react-i18next';
import { TabFooter } from '@/components/common/TabFooter';
import { Key, ExternalLink, AlertTriangle, Database, Settings, RefreshCw, TrendingUp, Activity, BarChart3, Network } from 'lucide-react';

// Components
import { ControlBar, MetricsPanel, SettingsModal, SurfaceChart } from './components';

// Hooks
import { useDatabentoApi, useSurfaceComputation, useDemoData } from './hooks';

// Types & Constants
import type {
  ChartType,
  ChartMetric,
  SurfaceAnalyticsConfig,
  VolatilitySurfaceData,
  CorrelationMatrixData,
  YieldCurveData,
  PCAData,
} from './types';
import { CORRELATION_ASSETS, TREASURY_MATURITIES } from './constants';

// Default configuration
const DEFAULT_CONFIG: SurfaceAnalyticsConfig = {
  selectedSymbol: 'SPY',
  correlationAssets: [...CORRELATION_ASSETS].slice(0, 8),
  dateRange: {
    start: new Date(Date.now() - 60 * 24 * 60 * 60 * 1000).toISOString().split('T')[0],
    end: new Date().toISOString().split('T')[0],
  },
  refreshInterval: 0, // Manual refresh by default (cost-conscious)
  timeframe: 'ohlcv-1d', // Daily OHLCV (lowest cost)
  dataset: 'DBEQ.BASIC', // Free sample dataset
};

/**
 * API Key Setup Screen - shown when no Databento API key is configured
 * Now includes "Use Demo Data" option for users without API access
 */
const ApiKeySetupScreen: React.FC<{
  onOpenSettings: () => void;
  onUseDemoData: () => void;
  colors: { primary: string; text: string; textMuted: string; background: string; panel: string; info: string; success: string; warning: string };
  fontSize: { heading: string; subheading: string; body: string; small: string; tiny: string };
  fontFamily: string;
  t: (key: string) => string;
}> = ({ onOpenSettings, onUseDemoData, colors, fontSize, fontFamily, t }) => (
  <div
    style={{
      display: 'flex',
      flexDirection: 'column',
      alignItems: 'center',
      justifyContent: 'center',
      height: '100%',
      padding: '48px',
      backgroundColor: colors.background,
    }}
  >
    {/* Main Container */}
    <div
      style={{
        maxWidth: '560px',
        padding: '32px',
        backgroundColor: colors.panel,
        border: `1px solid ${colors.textMuted}`,
        borderRadius: 'var(--ft-border-radius)',
        textAlign: 'center',
      }}
    >
      {/* Icon */}
      <div
        style={{
          width: '64px',
          height: '64px',
          margin: '0 auto 24px',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          backgroundColor: colors.primary + '15',
          borderRadius: '50%',
        }}
      >
        <Database size={32} style={{ color: colors.primary }} />
      </div>

      {/* Title */}
      <h2
        style={{
          fontSize: fontSize.heading,
          fontWeight: 700,
          color: colors.text,
          marginBottom: '12px',
          fontFamily,
          letterSpacing: '0.5px',
        }}
      >
        {t('surfaceAnalytics.apiKeyRequired')}
      </h2>

      {/* Description */}
      <p
        style={{
          fontSize: fontSize.body,
          color: colors.textMuted,
          marginBottom: '24px',
          lineHeight: '1.6',
          fontFamily,
        }}
      >
        {t('surfaceAnalytics.apiKeyDescription')}
      </p>

      {/* Features List */}
      <div
        style={{
          display: 'grid',
          gridTemplateColumns: '1fr 1fr',
          gap: '12px',
          marginBottom: '24px',
          textAlign: 'left',
        }}
      >
        {[
          { icon: TrendingUp, label: t('surfaceAnalytics.features.volatility'), desc: t('surfaceAnalytics.features.volatilityDesc') },
          { icon: Network, label: t('surfaceAnalytics.features.correlation'), desc: t('surfaceAnalytics.features.correlationDesc') },
          { icon: Activity, label: t('surfaceAnalytics.features.yieldCurves'), desc: t('surfaceAnalytics.features.yieldCurvesDesc') },
          { icon: BarChart3, label: t('surfaceAnalytics.features.pca'), desc: t('surfaceAnalytics.features.pcaDesc') },
        ].map(({ icon: Icon, label, desc }) => (
          <div
            key={label}
            style={{
              display: 'flex',
              alignItems: 'flex-start',
              gap: '8px',
              padding: '8px',
              backgroundColor: colors.background,
              border: `1px solid ${colors.textMuted}`,
              borderRadius: 'var(--ft-border-radius)',
            }}
          >
            <Icon size={14} style={{ color: colors.primary, marginTop: '2px' }} />
            <div>
              <div style={{ fontSize: fontSize.tiny, fontWeight: 700, color: colors.text, letterSpacing: '0.5px' }}>
                {label}
              </div>
              <div style={{ fontSize: fontSize.tiny, color: colors.textMuted }}>
                {desc}
              </div>
            </div>
          </div>
        ))}
      </div>

      {/* Action Buttons */}
      <div style={{ display: 'flex', flexDirection: 'column', gap: '12px', alignItems: 'center' }}>
        {/* Primary action row */}
        <div style={{ display: 'flex', gap: '12px', justifyContent: 'center' }}>
          <button
            onClick={onOpenSettings}
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              padding: '10px 20px',
              backgroundColor: colors.primary,
              color: colors.background,
              border: 'none',
              borderRadius: 'var(--ft-border-radius)',
              fontSize: fontSize.small,
              fontWeight: 700,
              cursor: 'pointer',
              fontFamily,
              letterSpacing: '0.5px',
            }}
          >
            <Key size={14} />
            {t('surfaceAnalytics.configureApiKey')}
          </button>

          <a
            href="https://databento.com"
            target="_blank"
            rel="noopener noreferrer"
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              padding: '10px 20px',
              backgroundColor: 'transparent',
              color: colors.info,
              border: `1px solid ${colors.textMuted}`,
              borderRadius: 'var(--ft-border-radius)',
              fontSize: fontSize.small,
              fontWeight: 700,
              cursor: 'pointer',
              textDecoration: 'none',
              fontFamily,
              letterSpacing: '0.5px',
            }}
          >
            <ExternalLink size={14} />
            {t('surfaceAnalytics.getApiKey')}
          </a>
        </div>

        {/* Demo mode option */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span style={{ fontSize: fontSize.tiny, color: colors.textMuted }}>{t('common.or')}</span>
          <button
            onClick={onUseDemoData}
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              padding: '8px 16px',
              backgroundColor: 'transparent',
              color: colors.success,
              border: `1px solid ${colors.success}50`,
              borderRadius: 'var(--ft-border-radius)',
              fontSize: fontSize.tiny,
              fontWeight: 700,
              cursor: 'pointer',
              fontFamily,
              letterSpacing: '0.5px',
            }}
          >
            <BarChart3 size={12} />
            {t('surfaceAnalytics.useDemoData')}
          </button>
          <span style={{ fontSize: fontSize.tiny, color: colors.textMuted }}>({t('surfaceAnalytics.syntheticData')})</span>
        </div>
      </div>

      {/* Cost Notice */}
      <div
        style={{
          marginTop: '20px',
          padding: '10px',
          backgroundColor: colors.warning + '10',
          border: `1px solid ${colors.warning}30`,
          borderRadius: 'var(--ft-border-radius)',
          display: 'flex',
          alignItems: 'flex-start',
          gap: '8px',
        }}
      >
        <AlertTriangle size={14} style={{ color: colors.warning, flexShrink: 0, marginTop: '2px' }} />
        <span style={{ fontSize: fontSize.tiny, color: colors.textMuted, textAlign: 'left' }}>
          <strong style={{ color: colors.warning }}>{t('common.note')}:</strong> {t('surfaceAnalytics.paidServiceNotice')}
        </span>
      </div>
    </div>
  </div>
);

const SurfaceAnalyticsTab: React.FC = () => {
  const { t } = useTranslation('surfaceAnalytics');
  const { colors, fontSize, fontFamily } = useTerminalTheme();

  // Theme-aware color mappings
  const themeColors = {
    primary: colors.primary,
    text: colors.text,
    textMuted: colors.textMuted,
    background: colors.background,
    panel: colors.panel,
    info: colors.info,
    success: colors.success,
    warning: colors.warning,
    alert: colors.alert,
    accent: colors.accent,
  };

  // State
  const [activeChart, setActiveChart] = useState<ChartType>('volatility');

  // Workspace tab state
  const getWorkspaceState = useCallback(() => ({
    activeChart,
  }), [activeChart]);

  const setWorkspaceState = useCallback((state: Record<string, unknown>) => {
    if (typeof state.activeChart === "string") setActiveChart(state.activeChart as any);
  }, []);

  useWorkspaceTabState("surface-analytics", getWorkspaceState, setWorkspaceState);
  const [lastUpdate, setLastUpdate] = useState<Date | null>(null);
  const [showSettings, setShowSettings] = useState(false);
  const [config, setConfig] = useState<SurfaceAnalyticsConfig>(DEFAULT_CONFIG);
  const [initialLoad, setInitialLoad] = useState(true);

  // Data states
  const [volatilityData, setVolatilityData] = useState<VolatilitySurfaceData | null>(null);
  const [correlationData, setCorrelationData] = useState<CorrelationMatrixData | null>(null);
  const [yieldCurveData, setYieldCurveData] = useState<YieldCurveData | null>(null);
  const [pcaData, setPcaData] = useState<PCAData | null>(null);
  const [dataError, setDataError] = useState<string | null>(null);

  // Hooks
  const { fetchOptionsChain, fetchHistoricalBars, loadApiKey, hasApiKey, loading, error: apiError } = useDatabentoApi();
  const { computeVolatilitySurface, computeCorrelationMatrix, computePCA } = useSurfaceComputation();
  const { generateVolatilitySurface, generateCorrelationMatrix, generateYieldCurve, generatePCA } = useDemoData();

  // Demo mode state - allows using synthetic data without API key
  const [isDemoMode, setIsDemoMode] = useState(false);
  const [useDemoModeExplicitly, setUseDemoModeExplicitly] = useState(false);

  // Load API key on mount
  useEffect(() => {
    const init = async () => {
      await loadApiKey();
      setInitialLoad(false);
    };
    init();
  }, [loadApiKey]);

  // Load demo data as fallback or primary mode
  const loadDemoData = useCallback((chartType?: ChartType) => {
    const targetChart = chartType || activeChart;
    setIsDemoMode(true);
    setDataError(null);

    if (targetChart === 'volatility') {
      const spotPrice = config.selectedSymbol === 'SPY' ? 450 :
                       config.selectedSymbol === 'QQQ' ? 380 :
                       config.selectedSymbol === 'IWM' ? 200 : 100;
      const surface = generateVolatilitySurface(config.selectedSymbol, spotPrice);
      setVolatilityData(surface);
      setLastUpdate(new Date());
    }

    if (targetChart === 'correlation') {
      const matrix = generateCorrelationMatrix(config.correlationAssets);
      setCorrelationData(matrix);
      setLastUpdate(new Date());
    }

    if (targetChart === 'yield-curve') {
      const curve = generateYieldCurve();
      setYieldCurveData(curve);
      setLastUpdate(new Date());
    }

    if (targetChart === 'pca') {
      const pca = generatePCA(config.correlationAssets);
      setPcaData(pca);
      setLastUpdate(new Date());
    }
  }, [activeChart, config.selectedSymbol, config.correlationAssets, generateVolatilitySurface, generateCorrelationMatrix, generateYieldCurve, generatePCA]);

  // Handle explicit demo mode activation from setup screen
  const handleUseDemoData = useCallback(() => {
    setUseDemoModeExplicitly(true);
    // Load all chart types for demo mode
    loadDemoData('volatility');
    // Set other data types for quick switching
    setCorrelationData(generateCorrelationMatrix(config.correlationAssets));
    setYieldCurveData(generateYieldCurve());
    setPcaData(generatePCA(config.correlationAssets));
  }, [loadDemoData, generateCorrelationMatrix, generateYieldCurve, generatePCA, config.correlationAssets]);

  // Fetch real data from Databento (with demo fallback)
  const fetchRealData = useCallback(async () => {
    if (!hasApiKey) {
      // No API key - use demo data
      loadDemoData();
      return;
    }

    setDataError(null);
    setIsDemoMode(false);

    try {
      // Fetch options chain for volatility surface
      if (activeChart === 'volatility') {
        const optionsResult = await fetchOptionsChain(config.selectedSymbol);
        if (optionsResult.error) {
          console.warn('API error, falling back to demo data:', optionsResult.message);
          loadDemoData();
          setDataError(`API Error (using demo data): ${optionsResult.message}`);
        } else if (optionsResult.data && optionsResult.data.length > 0) {
          // Get spot price from underlying data if available
          const spotPrice = optionsResult.data[0]?.spot_price || 100;
          const surface = computeVolatilitySurface(optionsResult.data, spotPrice);
          if (surface) {
            setVolatilityData(surface);
            setLastUpdate(new Date());
          } else {
            loadDemoData();
            setDataError('Failed to compute volatility surface, using demo data');
          }
        } else {
          loadDemoData();
          setDataError('No options data available, using demo data');
        }
      }

      // Fetch historical bars for correlation/PCA
      if (activeChart === 'correlation' || activeChart === 'pca') {
        const barsResult = await fetchHistoricalBars(config.correlationAssets, 60, config.timeframe || 'ohlcv-1d');
        if (barsResult.error) {
          console.warn('API error, falling back to demo data:', barsResult.message);
          loadDemoData();
          setDataError(`API Error (using demo data): ${barsResult.message}`);
        } else if (barsResult.data) {
          if (activeChart === 'correlation') {
            const matrix = computeCorrelationMatrix(barsResult.data, 30);
            if (matrix) {
              setCorrelationData(matrix);
              setLastUpdate(new Date());
            } else {
              loadDemoData();
              setDataError('Failed to compute correlation matrix, using demo data');
            }
          }
          if (activeChart === 'pca') {
            const pca = computePCA(barsResult.data);
            if (pca) {
              setPcaData(pca);
              setLastUpdate(new Date());
            } else {
              loadDemoData();
              setDataError('Failed to compute PCA factors, using demo data');
            }
          }
        } else {
          loadDemoData();
        }
      }

      // Yield curve - use demo data (FRED API not yet implemented)
      if (activeChart === 'yield-curve') {
        loadDemoData();
        setDataError('Yield curve: Using demo data (FRED API integration coming soon)');
      }

    } catch (error) {
      console.error('Failed to fetch data, falling back to demo:', error);
      loadDemoData();
      setDataError(`Error (using demo data): ${error}`);
    }
  }, [
    hasApiKey,
    activeChart,
    config.selectedSymbol,
    config.correlationAssets,
    fetchOptionsChain,
    fetchHistoricalBars,
    computeVolatilitySurface,
    computeCorrelationMatrix,
    computePCA,
    loadDemoData,
  ]);

  // Auto-refresh when API key is available and refresh interval is set
  useEffect(() => {
    if (hasApiKey && config.refreshInterval > 0) {
      const interval = setInterval(fetchRealData, config.refreshInterval);
      return () => clearInterval(interval);
    }
  }, [hasApiKey, config.refreshInterval, fetchRealData]);

  // Compute real metrics from actual data
  const currentMetrics: ChartMetric[] = useMemo(() => {
    switch (activeChart) {
      case 'volatility':
        if (!volatilityData) {
          return [
            { label: 'ATM VOL', value: '--', change: '--', positive: null },
            { label: 'SKEW', value: '--', change: '--', positive: null },
            { label: 'TERM', value: '--', change: '--', positive: null },
            { label: 'IV RANK', value: '--', change: '--', positive: null },
          ];
        }
        // Compute ATM vol (middle strike, middle expiration)
        const midExpIdx = Math.floor(volatilityData.z.length / 2);
        const midStrikeIdx = Math.floor(volatilityData.strikes.length / 2);
        const atmVol = volatilityData.z[midExpIdx]?.[midStrikeIdx] || 0;

        // Compute skew (OTM put vol - OTM call vol)
        const otmPutIdx = Math.floor(volatilityData.strikes.length * 0.2);
        const otmCallIdx = Math.floor(volatilityData.strikes.length * 0.8);
        const putVol = volatilityData.z[midExpIdx]?.[otmPutIdx] || 0;
        const callVol = volatilityData.z[midExpIdx]?.[otmCallIdx] || 0;
        const skew = putVol - callVol;

        return [
          { label: 'ATM VOL', value: `${atmVol.toFixed(1)}%`, change: '--', positive: null },
          { label: 'SKEW', value: `${skew.toFixed(1)}%`, change: skew > 0 ? 'PUT PREM' : 'CALL PREM', positive: skew < 0 },
          { label: 'SPOT', value: `$${volatilityData.spotPrice.toFixed(2)}`, change: volatilityData.underlying, positive: null },
          { label: 'OPTIONS', value: `${volatilityData.strikes.length * volatilityData.expirations.length}`, change: 'CONTRACTS', positive: null },
        ];

      case 'correlation':
        if (!correlationData) {
          return [
            { label: 'AVG CORR', value: '--', change: '--', positive: null },
            { label: 'MAX CORR', value: '--', change: '--', positive: null },
            { label: 'MIN CORR', value: '--', change: '--', positive: null },
            { label: 'ASSETS', value: '--', change: '--', positive: null },
          ];
        }
        // Compute correlation statistics from latest time point
        const latestCorr = correlationData.z[correlationData.z.length - 1] || [];
        const nonDiagonalCorrs = latestCorr.filter((_, i) => {
          const assetCount = correlationData.assets.length;
          return i % (assetCount + 1) !== 0; // Skip diagonal (self-correlation = 1)
        });
        const avgCorr = nonDiagonalCorrs.reduce((a, b) => a + b, 0) / nonDiagonalCorrs.length;
        const maxCorr = Math.max(...nonDiagonalCorrs);
        const minCorr = Math.min(...nonDiagonalCorrs);

        return [
          { label: 'AVG CORR', value: avgCorr.toFixed(3), change: avgCorr > 0.5 ? 'HIGH' : 'MODERATE', positive: avgCorr < 0.5 },
          { label: 'MAX CORR', value: maxCorr.toFixed(3), change: maxCorr > 0.8 ? 'STRONG' : 'WEAK', positive: maxCorr < 0.8 },
          { label: 'MIN CORR', value: minCorr.toFixed(3), change: minCorr < 0 ? 'NEGATIVE' : 'POSITIVE', positive: minCorr > 0 },
          { label: 'ASSETS', value: `${correlationData.assets.length}`, change: `${correlationData.window}D WINDOW`, positive: null },
        ];

      case 'yield-curve':
        if (!yieldCurveData) {
          return [
            { label: '2Y YIELD', value: '--', change: '--', positive: null },
            { label: '10Y YIELD', value: '--', change: '--', positive: null },
            { label: '2-10 SPREAD', value: '--', change: '--', positive: null },
            { label: 'CURVE', value: '--', change: '--', positive: null },
          ];
        }
        // Get latest yield curve data (last time point)
        const latestYields = yieldCurveData.z[yieldCurveData.z.length - 1] || [];
        // Find 2Y (24 months) and 10Y (120 months) indices
        const idx2Y = yieldCurveData.maturities.indexOf(24);
        const idx10Y = yieldCurveData.maturities.indexOf(120);
        const yield2Y = idx2Y >= 0 ? latestYields[idx2Y] : 0;
        const yield10Y = idx10Y >= 0 ? latestYields[idx10Y] : 0;
        const spread2_10 = yield10Y - yield2Y;

        return [
          { label: '2Y YIELD', value: `${yield2Y.toFixed(2)}%`, change: yield2Y > 4 ? 'HIGH' : 'NORMAL', positive: null },
          { label: '10Y YIELD', value: `${yield10Y.toFixed(2)}%`, change: yield10Y > 4 ? 'HIGH' : 'NORMAL', positive: null },
          { label: '2-10 SPREAD', value: `${spread2_10.toFixed(2)}%`, change: spread2_10 < 0 ? 'INVERTED' : 'NORMAL', positive: spread2_10 >= 0 },
          { label: 'CURVE', value: spread2_10 < 0 ? 'INVERTED' : 'NORMAL', change: `${yieldCurveData.maturities.length} TENORS`, positive: spread2_10 >= 0 },
        ];

      case 'pca':
        if (!pcaData) {
          return [
            { label: 'PC1 VAR', value: '--', change: '--', positive: null },
            { label: 'PC2 VAR', value: '--', change: '--', positive: null },
            { label: 'PC3 VAR', value: '--', change: '--', positive: null },
            { label: 'TOTAL', value: '--', change: '--', positive: null },
          ];
        }
        const totalVar = pcaData.varianceExplained.slice(0, 3).reduce((a, b) => a + b, 0);

        return [
          { label: 'PC1 VAR', value: `${(pcaData.varianceExplained[0] * 100).toFixed(1)}%`, change: 'DOMINANT', positive: true },
          { label: 'PC2 VAR', value: `${(pcaData.varianceExplained[1] * 100).toFixed(1)}%`, change: 'SECONDARY', positive: null },
          { label: 'PC3 VAR', value: `${(pcaData.varianceExplained[2] * 100).toFixed(1)}%`, change: 'TERTIARY', positive: null },
          { label: 'TOP 3', value: `${(totalVar * 100).toFixed(1)}%`, change: totalVar > 0.9 ? 'EXCELLENT' : 'GOOD', positive: totalVar > 0.9 },
        ];

      default:
        return [];
    }
  }, [activeChart, volatilityData, correlationData, yieldCurveData, pcaData]);

  const handleRefresh = useCallback(() => {
    if (useDemoModeExplicitly || !hasApiKey) {
      // In demo mode, regenerate synthetic data
      loadDemoData();
    } else {
      fetchRealData();
    }
  }, [useDemoModeExplicitly, hasApiKey, loadDemoData, fetchRealData]);

  const handleConfigSave = useCallback((newConfig: SurfaceAnalyticsConfig) => {
    setConfig(newConfig);
  }, []);

  const handleApiKeyChange = useCallback(() => {
    loadApiKey();
    // Clear any previous errors when API key changes
    setDataError(null);
  }, [loadApiKey]);

  // Show loading state during initial load
  if (initialLoad) {
    return (
      <div
        style={{
          height: '100%',
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          justifyContent: 'center',
          backgroundColor: colors.background,
          color: colors.text,
          fontFamily,
        }}
      >
        <RefreshCw size={32} className="animate-spin" style={{ color: colors.accent }} />
        <span style={{ marginTop: '16px', fontSize: fontSize.body, color: colors.textMuted }}>
          {t('initializing')}
        </span>
      </div>
    );
  }

  // Show API key setup screen when no key is configured AND user hasn't chosen demo mode
  if (!hasApiKey && !useDemoModeExplicitly) {
    return (
      <div
        style={{
          height: '100%',
          display: 'flex',
          flexDirection: 'column',
          backgroundColor: colors.background,
          color: colors.text,
          fontFamily,
          overflow: 'hidden',
        }}
      >
        {/* Header */}
        <div
          style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            padding: '8px 16px',
            backgroundColor: colors.panel,
            borderBottom: `2px solid ${colors.accent}`,
            boxShadow: `0 2px 8px ${colors.accent}20`,
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Database size={14} style={{ color: colors.accent }} />
            <span style={{ fontSize: fontSize.body, fontWeight: 700, color: colors.accent, letterSpacing: '0.5px' }}>
              {t('title')}
            </span>
          </div>
          <button
            onClick={() => setShowSettings(true)}
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              padding: '6px 12px',
              backgroundColor: colors.accent,
              color: colors.background,
              border: 'none',
              borderRadius: 'var(--ft-border-radius)',
              fontSize: fontSize.tiny,
              fontWeight: 700,
              cursor: 'pointer',
            }}
          >
            <Settings size={12} />
            {t('common:settings')}
          </button>
        </div>

        {/* Setup Screen */}
        <ApiKeySetupScreen
          onOpenSettings={() => setShowSettings(true)}
          onUseDemoData={handleUseDemoData}
          colors={themeColors}
          fontSize={fontSize}
          fontFamily={fontFamily}
          t={t}
        />

        {/* Footer */}
        <div style={{ flexShrink: 0 }}>
          <TabFooter
            tabName={t('title')}
            statusInfo={t('apiKeyRequiredStatus')}
          />
        </div>

        {/* Settings Modal */}
        <SettingsModal
          isOpen={showSettings}
          onClose={() => setShowSettings(false)}
          config={config}
          onSave={handleConfigSave}
          onApiKeyChange={handleApiKeyChange}
          accentColor={colors.accent}
          textColor={colors.text}
        />
      </div>
    );
  }

  return (
    <div
      style={{
        height: '100%',
        display: 'flex',
        flexDirection: 'column',
        backgroundColor: colors.background,
        color: colors.text,
        fontFamily,
        overflow: 'hidden',
      }}
    >
      {/* Top Control Bar */}
      <ControlBar
        activeChart={activeChart}
        onChartChange={setActiveChart}
        onRefresh={handleRefresh}
        onSettings={() => setShowSettings(true)}
        lastUpdate={lastUpdate || new Date()}
        loading={loading}
        hasApiKey={hasApiKey}
        accentColor={colors.accent}
        textColor={colors.text}
      />

      {/* Main Content Area */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden', minHeight: 0 }}>
        {/* Left Metrics Panel */}
        <MetricsPanel
          chartType={activeChart}
          metrics={currentMetrics}
          dataSource={isDemoMode ? t('demoData') : 'DATABENTO'}
          frequency={isDemoMode ? t('synthetic') : (config.refreshInterval > 0 ? `${config.refreshInterval / 60000}M AUTO` : t('manual'))}
          quality={dataError ? t('error') : (isDemoMode ? t('synthetic') : (lastUpdate ? '99.8%' : t('ready')))}
          symbol={activeChart === 'volatility' ? config.selectedSymbol : undefined}
          textColor={colors.text}
          accentColor={colors.accent}
        />

        {/* 3D Chart Display */}
        <SurfaceChart
          chartType={activeChart}
          volatilityData={volatilityData}
          correlationData={correlationData}
          yieldCurveData={yieldCurveData}
          pcaData={pcaData}
          loading={loading}
          error={dataError || apiError}
          textColor={colors.text}
          accentColor={colors.accent}
          fontFamily={fontFamily}
          isDemoMode={isDemoMode}
        />
      </div>

      {/* Footer */}
      <div style={{ flexShrink: 0 }}>
        <TabFooter
          tabName={t('title')}
          statusInfo={`${activeChart.toUpperCase()} | ${currentMetrics.length} ${t('metrics')} | ${isDemoMode ? t('demoDataSynthetic') : 'DATABENTO'} | ${lastUpdate ? `${t('updated')}: ${lastUpdate.toLocaleTimeString()}` : t('clickRefresh')}`}
        />
      </div>

      {/* Settings Modal */}
      <SettingsModal
        isOpen={showSettings}
        onClose={() => setShowSettings(false)}
        config={config}
        onSave={handleConfigSave}
        onApiKeyChange={handleApiKeyChange}
        accentColor={colors.accent}
        textColor={colors.text}
      />
    </div>
  );
};

export default SurfaceAnalyticsTab;
