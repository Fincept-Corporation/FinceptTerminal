/**
 * Functime Analytics Panel - Time Series Forecasting
 * Bloomberg Professional Design
 * Integrated with functime library via PyO3
 * Enhanced with portfolio integration and yfinance data fetching
 */

import React, { useState, useEffect, useMemo, useCallback } from 'react';
import {
  TrendingUp,
  TrendingDown,
  Activity,
  BarChart2,
  Calendar,
  RefreshCw,
  AlertCircle,
  CheckCircle2,
  ChevronDown,
  ChevronUp,
  LineChart,
  Play,
  Settings,
  Zap,
  Target,
  Clock,
  Layers,
  GitBranch,
  Database,
  Briefcase,
  Search,
  AlertTriangle,
  Waves,
  BarChart3,
  History,
  Shuffle,
  Shield
} from 'lucide-react';
import {
  functimeService,
  type ForecastResult,
  type ForecastConfig,
  type PreprocessConfig,
  type FullPipelineResponse,
  type AnomalyDetectionConfig,
  type SeasonalityConfig,
  type ConfidenceIntervalsConfig,
  type BacktestConfig
} from '@/services/aiQuantLab/functimeService';
import {
  portfolioFunctimeService,
  type PortfolioPriceData,
  type PortfolioForecastResult,
  type PortfolioAnomalyResult,
  type PortfolioSeasonalityResult
} from '@/services/aiQuantLab/portfolioFunctimeService';
import { portfolioService, type Portfolio } from '@/services/portfolioService';

// Bloomberg Professional Color Palette
const BLOOMBERG = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
};

// Sample panel data for demonstration
const SAMPLE_DATA = [
  { entity_id: 'AAPL', time: '2024-01-02', value: 185.64 },
  { entity_id: 'AAPL', time: '2024-01-03', value: 184.25 },
  { entity_id: 'AAPL', time: '2024-01-04', value: 181.91 },
  { entity_id: 'AAPL', time: '2024-01-05', value: 181.18 },
  { entity_id: 'AAPL', time: '2024-01-08', value: 185.56 },
  { entity_id: 'AAPL', time: '2024-01-09', value: 185.14 },
  { entity_id: 'AAPL', time: '2024-01-10', value: 186.19 },
  { entity_id: 'AAPL', time: '2024-01-11', value: 185.59 },
  { entity_id: 'AAPL', time: '2024-01-12', value: 185.92 },
  { entity_id: 'AAPL', time: '2024-01-16', value: 183.63 },
  { entity_id: 'AAPL', time: '2024-01-17', value: 182.68 },
  { entity_id: 'AAPL', time: '2024-01-18', value: 188.63 },
  { entity_id: 'AAPL', time: '2024-01-19', value: 191.56 },
  { entity_id: 'AAPL', time: '2024-01-22', value: 193.89 },
  { entity_id: 'AAPL', time: '2024-01-23', value: 195.18 },
  { entity_id: 'AAPL', time: '2024-01-24', value: 194.50 },
  { entity_id: 'AAPL', time: '2024-01-25', value: 194.17 },
  { entity_id: 'AAPL', time: '2024-01-26', value: 192.42 },
  { entity_id: 'AAPL', time: '2024-01-29', value: 191.73 },
  { entity_id: 'AAPL', time: '2024-01-30', value: 188.04 }
];

interface ForecastAnalysisResult {
  forecast?: ForecastResult[];
  model?: string;
  horizon?: number;
  frequency?: string;
  best_params?: Record<string, unknown>;
  evaluation_metrics?: {
    mae?: number;
    rmse?: number;
    smape?: number;
  };
  data_summary?: {
    total_rows: number;
    entities: number;
    train_size: number;
    test_size: number;
  };
}

// Data source types
type DataSourceType = 'manual' | 'portfolio' | 'symbol';

// Analysis mode types
type AnalysisMode = 'forecast' | 'anomaly' | 'seasonality' | 'backtest';

export function FunctimePanel() {
  const [isFunctimeAvailable, setIsFunctimeAvailable] = useState<boolean | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [analysisResult, setAnalysisResult] = useState<ForecastAnalysisResult | null>(null);
  const [dataInput, setDataInput] = useState(JSON.stringify(SAMPLE_DATA, null, 2));

  // Data source configuration
  const [dataSourceType, setDataSourceType] = useState<DataSourceType>('manual');
  const [portfolios, setPortfolios] = useState<Portfolio[]>([]);
  const [selectedPortfolioId, setSelectedPortfolioId] = useState<string>('');
  const [symbolInput, setSymbolInput] = useState('AAPL');
  const [historicalDays, setHistoricalDays] = useState(365);
  const [priceDataLoading, setPriceDataLoading] = useState(false);
  const [priceData, setPriceData] = useState<PortfolioPriceData | null>(null);

  // Analysis mode
  const [analysisMode, setAnalysisMode] = useState<AnalysisMode>('forecast');

  // Forecast configuration
  const [selectedModel, setSelectedModel] = useState('linear');
  const [horizon, setHorizon] = useState(7);
  const [frequency, setFrequency] = useState('1d');
  const [alpha, setAlpha] = useState(1.0);
  const [testSize, setTestSize] = useState(5);
  const [preprocess, setPreprocess] = useState<'none' | 'scale' | 'difference'>('none');

  const [expandedSections, setExpandedSections] = useState({
    config: true,
    forecast: true,
    metrics: true,
    summary: true,
    chart: true,
    dataSource: true,
    anomaly: true,
    seasonality: true,
    backtest: true
  });

  // Anomaly detection state
  const [anomalyMethod, setAnomalyMethod] = useState<'zscore' | 'iqr' | 'isolation_forest'>('zscore');
  const [anomalyThreshold, setAnomalyThreshold] = useState(3.0);
  const [anomalyResult, setAnomalyResult] = useState<PortfolioAnomalyResult | null>(null);

  // Seasonality state
  const [seasonalityResult, setSeasonalityResult] = useState<PortfolioSeasonalityResult | null>(null);

  // Expandable table state
  const [forecastTableExpanded, setForecastTableExpanded] = useState(false);
  const INITIAL_ROWS = 10;
  const EXPANDED_ROWS = 50;

  // Advanced models
  const advancedModels = functimeService.getAdvancedModels();
  const anomalyMethods = functimeService.getAnomalyMethods();

  const models = functimeService.getAvailableModels();
  const frequencies = functimeService.getAvailableFrequencies();

  // Check functime availability on mount
  useEffect(() => {
    checkFunctimeStatus();
    loadPortfolios();
  }, []);

  const loadPortfolios = async () => {
    try {
      const portfolioList = await portfolioFunctimeService.getAvailablePortfolios();
      setPortfolios(portfolioList);
      if (portfolioList.length > 0 && !selectedPortfolioId) {
        setSelectedPortfolioId(portfolioList[0].id);
      }
    } catch (err) {
      console.error('Failed to load portfolios:', err);
    }
  };

  const checkFunctimeStatus = async () => {
    try {
      const status = await functimeService.checkStatus();
      setIsFunctimeAvailable(status.available);
      if (!status.available) {
        setError('Functime library not installed. Run: pip install functime polars');
      }
    } catch (err) {
      setIsFunctimeAvailable(false);
      setError('Failed to check functime status');
    }
  };

  // Load data based on source type
  const loadDataFromSource = useCallback(async () => {
    if (dataSourceType === 'manual') {
      return; // Data already in dataInput
    }

    setPriceDataLoading(true);
    setError(null);

    try {
      if (dataSourceType === 'portfolio' && selectedPortfolioId) {
        const summary = await portfolioFunctimeService.getPortfolioSummary(selectedPortfolioId);
        if (!summary || summary.holdings.length === 0) {
          setError('Portfolio has no holdings');
          return;
        }

        const data = await portfolioFunctimeService.fetchHistoricalPrices(
          summary.holdings,
          historicalDays
        );

        if (data) {
          setPriceData(data);
          setDataInput(JSON.stringify(data.panelData, null, 2));
        } else {
          setError('Failed to fetch historical data for portfolio');
        }
      } else if (dataSourceType === 'symbol' && symbolInput) {
        const data = await portfolioFunctimeService.fetchSymbolData(symbolInput, historicalDays);
        if (data) {
          setDataInput(JSON.stringify(data, null, 2));
          setPriceData({
            panelData: data,
            assets: [symbolInput],
            nDataPoints: data.length,
            startDate: data[0]?.time || '',
            endDate: data[data.length - 1]?.time || ''
          });
        } else {
          setError(`Failed to fetch data for ${symbolInput}`);
        }
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to load data');
    } finally {
      setPriceDataLoading(false);
    }
  }, [dataSourceType, selectedPortfolioId, symbolInput, historicalDays]);

  // Run analysis based on mode
  const runAnalysis = async () => {
    setIsLoading(true);
    setError(null);

    try {
      const data = JSON.parse(dataInput);

      if (analysisMode === 'forecast') {
        await runForecast();
      } else if (analysisMode === 'anomaly') {
        await runAnomalyDetection(data);
      } else if (analysisMode === 'seasonality') {
        await runSeasonalityAnalysis(data);
      } else if (analysisMode === 'backtest') {
        // For backtest, we use the forecast function with more settings
        await runForecast();
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Analysis failed');
    } finally {
      setIsLoading(false);
    }
  };

  // Anomaly detection
  const runAnomalyDetection = async (data: any[]) => {
    const config: AnomalyDetectionConfig = {
      method: anomalyMethod,
      threshold: anomalyThreshold
    };

    const result = await functimeService.detectAnomalies(data, config);

    if (!result.success) {
      setError(result.error || 'Anomaly detection failed');
      return;
    }

    // Format result for portfolio view
    const anomalies: Record<string, any> = {};
    if (result.results) {
      Object.entries(result.results).forEach(([symbol, data]) => {
        const anomalyList = data.anomalies?.filter((a: any) => a.is_anomaly) || [];
        anomalies[symbol] = {
          count: data.n_anomalies,
          rate: data.anomaly_rate,
          dates: anomalyList.map((a: any) => a.time),
          values: anomalyList.map((a: any) => a.value)
        };
      });
    }

    setAnomalyResult({
      success: true,
      portfolio: portfolios.find(p => p.id === selectedPortfolioId) || {} as Portfolio,
      anomalies
    });
  };

  // Seasonality analysis
  const runSeasonalityAnalysis = async (data: any[]) => {
    const config: SeasonalityConfig = {
      operation: 'metrics'
    };

    const result = await functimeService.analyzeSeasonality(data, config);

    if (!result.success) {
      setError(result.error || 'Seasonality analysis failed');
      return;
    }

    // Format result
    const seasonality: Record<string, any> = {};
    if (result.metrics) {
      Object.entries(result.metrics).forEach(([symbol, metrics]) => {
        seasonality[symbol] = {
          period: metrics.detected_period || 0,
          strength: metrics.seasonal_strength,
          is_seasonal: metrics.is_seasonal,
          trend_strength: metrics.trend_strength
        };
      });
    }

    setSeasonalityResult({
      success: true,
      portfolio: portfolios.find(p => p.id === selectedPortfolioId) || {} as Portfolio,
      seasonality
    });
  };

  const runForecast = async () => {
    setIsLoading(true);
    setError(null);

    try {
      const data = JSON.parse(dataInput);

      // Run full pipeline
      const result = await functimeService.fullPipeline(
        data,
        selectedModel,
        horizon,
        frequency,
        testSize,
        preprocess === 'none' ? undefined : preprocess
      );

      if (!result.success) {
        setError(result.error || 'Forecast failed');
        return;
      }

      setAnalysisResult({
        forecast: result.forecast,
        model: result.model,
        horizon: result.horizon,
        frequency: result.frequency,
        best_params: result.best_params,
        evaluation_metrics: result.evaluation_metrics,
        data_summary: result.data_summary
      });
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Forecast failed');
    } finally {
      setIsLoading(false);
    }
  };

  const toggleSection = (section: keyof typeof expandedSections) => {
    setExpandedSections(prev => ({ ...prev, [section]: !prev[section] }));
  };

  const formatMetric = (value: number | null | undefined, decimals = 4): string => {
    if (value === null || value === undefined || isNaN(value)) return 'N/A';
    return value.toFixed(decimals);
  };

  // Parse historical data from input for chart visualization
  const historicalData = useMemo(() => {
    try {
      const data = JSON.parse(dataInput);
      if (Array.isArray(data)) {
        return data.map(d => ({
          time: d.time,
          value: d.value
        }));
      }
      return [];
    } catch {
      return [];
    }
  }, [dataInput]);

  // Loading state
  if (isFunctimeAvailable === null) {
    return (
      <div className="flex items-center justify-center h-full" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
        <RefreshCw size={32} color={BLOOMBERG.ORANGE} className="animate-spin" />
      </div>
    );
  }

  // Not available state
  if (!isFunctimeAvailable) {
    return (
      <div className="flex items-center justify-center h-full p-8" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
        <div className="text-center max-w-md">
          <AlertCircle size={48} color={BLOOMBERG.RED} className="mx-auto mb-4" />
          <h3 className="text-lg font-bold uppercase mb-2" style={{ color: BLOOMBERG.WHITE }}>
            Functime Library Not Installed
          </h3>
          <p className="text-sm mb-4" style={{ color: BLOOMBERG.GRAY }}>
            Install functime and polars for ML time series forecasting
          </p>
          <code
            className="block p-4 rounded text-sm font-mono mt-4"
            style={{ backgroundColor: BLOOMBERG.PANEL_BG, color: BLOOMBERG.ORANGE }}
          >
            pip install functime polars
          </code>
          <button
            onClick={checkFunctimeStatus}
            className="mt-6 px-6 py-2 rounded font-bold text-sm uppercase flex items-center gap-2 mx-auto"
            style={{ backgroundColor: BLOOMBERG.ORANGE, color: BLOOMBERG.DARK_BG }}
          >
            <RefreshCw size={14} />
            Check Again
          </button>
        </div>
      </div>
    );
  }

  return (
    <div className="flex h-full" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
      {/* Left Panel - Configuration */}
      <div
        className="w-96 flex flex-col border-r"
        style={{ backgroundColor: BLOOMBERG.PANEL_BG, borderColor: BLOOMBERG.BORDER }}
      >
        {/* Input Header */}
        <div
          className="px-4 py-3 border-b flex items-center gap-2"
          style={{ backgroundColor: BLOOMBERG.HEADER_BG, borderColor: BLOOMBERG.BORDER }}
        >
          <Zap size={16} color={BLOOMBERG.ORANGE} />
          <span className="text-xs font-bold uppercase tracking-wide" style={{ color: BLOOMBERG.WHITE }}>
            Time Series Forecasting
          </span>
        </div>

        {/* Scrollable Content */}
        <div className="flex-1 overflow-auto p-4 space-y-4">
          {/* Data Source Selection */}
          <div
            className="rounded overflow-hidden"
            style={{ border: `1px solid ${BLOOMBERG.BORDER}` }}
          >
            <button
              onClick={() => toggleSection('dataSource')}
              className="w-full px-3 py-2 flex items-center justify-between"
              style={{ backgroundColor: BLOOMBERG.HEADER_BG }}
            >
              <div className="flex items-center gap-2">
                <Database size={14} color={BLOOMBERG.CYAN} />
                <span className="text-xs font-bold uppercase" style={{ color: BLOOMBERG.WHITE }}>
                  Data Source
                </span>
              </div>
              {expandedSections.dataSource ? (
                <ChevronUp size={14} color={BLOOMBERG.GRAY} />
              ) : (
                <ChevronDown size={14} color={BLOOMBERG.GRAY} />
              )}
            </button>

            {expandedSections.dataSource && (
              <div className="p-3 space-y-3">
                {/* Source Type Selection */}
                <div className="flex gap-2">
                  <button
                    onClick={() => setDataSourceType('manual')}
                    className={`flex-1 px-2 py-1.5 rounded text-xs font-mono ${dataSourceType === 'manual' ? 'border-2' : ''}`}
                    style={{
                      backgroundColor: dataSourceType === 'manual' ? BLOOMBERG.HEADER_BG : BLOOMBERG.DARK_BG,
                      borderColor: BLOOMBERG.ORANGE,
                      color: dataSourceType === 'manual' ? BLOOMBERG.ORANGE : BLOOMBERG.GRAY
                    }}
                  >
                    Manual
                  </button>
                  <button
                    onClick={() => setDataSourceType('portfolio')}
                    className={`flex-1 px-2 py-1.5 rounded text-xs font-mono ${dataSourceType === 'portfolio' ? 'border-2' : ''}`}
                    style={{
                      backgroundColor: dataSourceType === 'portfolio' ? BLOOMBERG.HEADER_BG : BLOOMBERG.DARK_BG,
                      borderColor: BLOOMBERG.ORANGE,
                      color: dataSourceType === 'portfolio' ? BLOOMBERG.ORANGE : BLOOMBERG.GRAY
                    }}
                  >
                    <Briefcase size={12} className="inline mr-1" />
                    Portfolio
                  </button>
                  <button
                    onClick={() => setDataSourceType('symbol')}
                    className={`flex-1 px-2 py-1.5 rounded text-xs font-mono ${dataSourceType === 'symbol' ? 'border-2' : ''}`}
                    style={{
                      backgroundColor: dataSourceType === 'symbol' ? BLOOMBERG.HEADER_BG : BLOOMBERG.DARK_BG,
                      borderColor: BLOOMBERG.ORANGE,
                      color: dataSourceType === 'symbol' ? BLOOMBERG.ORANGE : BLOOMBERG.GRAY
                    }}
                  >
                    <Search size={12} className="inline mr-1" />
                    Symbol
                  </button>
                </div>

                {/* Portfolio Selection */}
                {dataSourceType === 'portfolio' && (
                  <div>
                    <label className="block text-xs font-mono mb-1" style={{ color: BLOOMBERG.GRAY }}>
                      SELECT PORTFOLIO
                    </label>
                    <select
                      value={selectedPortfolioId}
                      onChange={(e) => setSelectedPortfolioId(e.target.value)}
                      className="w-full p-2 rounded text-xs font-mono"
                      style={{
                        backgroundColor: BLOOMBERG.DARK_BG,
                        color: BLOOMBERG.WHITE,
                        border: `1px solid ${BLOOMBERG.BORDER}`
                      }}
                    >
                      {portfolios.length === 0 ? (
                        <option value="">No portfolios found</option>
                      ) : (
                        portfolios.map((p) => (
                          <option key={p.id} value={p.id}>
                            {p.name}
                          </option>
                        ))
                      )}
                    </select>
                  </div>
                )}

                {/* Symbol Input */}
                {dataSourceType === 'symbol' && (
                  <div>
                    <label className="block text-xs font-mono mb-1" style={{ color: BLOOMBERG.GRAY }}>
                      TICKER SYMBOL
                    </label>
                    <input
                      type="text"
                      value={symbolInput}
                      onChange={(e) => setSymbolInput(e.target.value.toUpperCase())}
                      placeholder="AAPL"
                      className="w-full p-2 rounded text-xs font-mono"
                      style={{
                        backgroundColor: BLOOMBERG.DARK_BG,
                        color: BLOOMBERG.WHITE,
                        border: `1px solid ${BLOOMBERG.BORDER}`
                      }}
                    />
                  </div>
                )}

                {/* Historical Days */}
                {dataSourceType !== 'manual' && (
                  <div>
                    <label className="block text-xs font-mono mb-1" style={{ color: BLOOMBERG.GRAY }}>
                      HISTORICAL DAYS
                    </label>
                    <input
                      type="number"
                      min={30}
                      max={1825}
                      value={historicalDays}
                      onChange={(e) => setHistoricalDays(parseInt(e.target.value) || 365)}
                      className="w-full p-2 rounded text-xs font-mono"
                      style={{
                        backgroundColor: BLOOMBERG.DARK_BG,
                        color: BLOOMBERG.WHITE,
                        border: `1px solid ${BLOOMBERG.BORDER}`
                      }}
                    />
                  </div>
                )}

                {/* Load Data Button */}
                {dataSourceType !== 'manual' && (
                  <button
                    onClick={loadDataFromSource}
                    disabled={priceDataLoading}
                    className="w-full py-2 rounded font-bold text-xs uppercase flex items-center justify-center gap-2"
                    style={{ backgroundColor: BLOOMBERG.CYAN, color: BLOOMBERG.DARK_BG }}
                  >
                    {priceDataLoading ? (
                      <>
                        <RefreshCw size={12} className="animate-spin" />
                        Loading...
                      </>
                    ) : (
                      <>
                        <Database size={12} />
                        Fetch Data
                      </>
                    )}
                  </button>
                )}

                {/* Data Info */}
                {priceData && dataSourceType !== 'manual' && (
                  <div className="p-2 rounded text-xs font-mono" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
                    <div className="flex justify-between" style={{ color: BLOOMBERG.GRAY }}>
                      <span>Assets:</span>
                      <span style={{ color: BLOOMBERG.CYAN }}>{priceData.assets.length}</span>
                    </div>
                    <div className="flex justify-between" style={{ color: BLOOMBERG.GRAY }}>
                      <span>Data Points:</span>
                      <span style={{ color: BLOOMBERG.WHITE }}>{priceData.nDataPoints}</span>
                    </div>
                    <div className="flex justify-between" style={{ color: BLOOMBERG.GRAY }}>
                      <span>Range:</span>
                      <span style={{ color: BLOOMBERG.WHITE }}>{priceData.startDate} to {priceData.endDate}</span>
                    </div>
                  </div>
                )}
              </div>
            )}
          </div>

          {/* Analysis Mode Tabs */}
          <div className="flex gap-1 p-1 rounded" style={{ backgroundColor: BLOOMBERG.HEADER_BG }}>
            <button
              onClick={() => setAnalysisMode('forecast')}
              className={`flex-1 px-2 py-1.5 rounded text-xs font-mono flex items-center justify-center gap-1`}
              style={{
                backgroundColor: analysisMode === 'forecast' ? BLOOMBERG.ORANGE : 'transparent',
                color: analysisMode === 'forecast' ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY
              }}
            >
              <TrendingUp size={12} />
              Forecast
            </button>
            <button
              onClick={() => setAnalysisMode('anomaly')}
              className={`flex-1 px-2 py-1.5 rounded text-xs font-mono flex items-center justify-center gap-1`}
              style={{
                backgroundColor: analysisMode === 'anomaly' ? BLOOMBERG.ORANGE : 'transparent',
                color: analysisMode === 'anomaly' ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY
              }}
            >
              <AlertTriangle size={12} />
              Anomaly
            </button>
            <button
              onClick={() => setAnalysisMode('seasonality')}
              className={`flex-1 px-2 py-1.5 rounded text-xs font-mono flex items-center justify-center gap-1`}
              style={{
                backgroundColor: analysisMode === 'seasonality' ? BLOOMBERG.ORANGE : 'transparent',
                color: analysisMode === 'seasonality' ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY
              }}
            >
              <Waves size={12} />
              Season
            </button>
          </div>

          {/* Data Input - Only for Manual */}
          {dataSourceType === 'manual' && (
            <div>
              <label className="block text-xs font-mono mb-2" style={{ color: BLOOMBERG.GRAY }}>
                PANEL DATA (JSON)
              </label>
              <textarea
                value={dataInput}
                onChange={(e) => setDataInput(e.target.value)}
                className="w-full h-32 p-3 rounded text-xs font-mono resize-none"
                style={{
                  backgroundColor: BLOOMBERG.DARK_BG,
                  color: BLOOMBERG.WHITE,
                  border: `1px solid ${BLOOMBERG.BORDER}`
                }}
                placeholder='[{"entity_id": "A", "time": "2024-01-01", "value": 100}, ...]'
              />
            </div>
          )}

          {/* Model Configuration - Shown for forecast mode */}
          {analysisMode === 'forecast' && (
          <div
            className="rounded overflow-hidden"
            style={{ border: `1px solid ${BLOOMBERG.BORDER}` }}
          >
            <button
              onClick={() => toggleSection('config')}
              className="w-full px-3 py-2 flex items-center justify-between"
              style={{ backgroundColor: BLOOMBERG.HEADER_BG }}
            >
              <div className="flex items-center gap-2">
                <Settings size={14} color={BLOOMBERG.CYAN} />
                <span className="text-xs font-bold uppercase" style={{ color: BLOOMBERG.WHITE }}>
                  Model Configuration
                </span>
              </div>
              {expandedSections.config ? (
                <ChevronUp size={14} color={BLOOMBERG.GRAY} />
              ) : (
                <ChevronDown size={14} color={BLOOMBERG.GRAY} />
              )}
            </button>

            {expandedSections.config && (
              <div className="p-3 space-y-3">
                {/* Model Selection */}
                <div>
                  <label className="block text-xs font-mono mb-1" style={{ color: BLOOMBERG.GRAY }}>
                    FORECAST MODEL
                  </label>
                  <select
                    value={selectedModel}
                    onChange={(e) => setSelectedModel(e.target.value)}
                    className="w-full p-2 rounded text-xs font-mono"
                    style={{
                      backgroundColor: BLOOMBERG.DARK_BG,
                      color: BLOOMBERG.WHITE,
                      border: `1px solid ${BLOOMBERG.BORDER}`
                    }}
                  >
                    {models.map((m) => (
                      <option key={m.id} value={m.id}>
                        {m.name}
                      </option>
                    ))}
                  </select>
                </div>

                {/* Horizon */}
                <div>
                  <label className="block text-xs font-mono mb-1" style={{ color: BLOOMBERG.GRAY }}>
                    FORECAST HORIZON
                  </label>
                  <input
                    type="number"
                    min={1}
                    max={365}
                    value={horizon}
                    onChange={(e) => setHorizon(parseInt(e.target.value) || 7)}
                    className="w-full p-2 rounded text-xs font-mono"
                    style={{
                      backgroundColor: BLOOMBERG.DARK_BG,
                      color: BLOOMBERG.WHITE,
                      border: `1px solid ${BLOOMBERG.BORDER}`
                    }}
                  />
                </div>

                {/* Frequency */}
                <div>
                  <label className="block text-xs font-mono mb-1" style={{ color: BLOOMBERG.GRAY }}>
                    DATA FREQUENCY
                  </label>
                  <select
                    value={frequency}
                    onChange={(e) => setFrequency(e.target.value)}
                    className="w-full p-2 rounded text-xs font-mono"
                    style={{
                      backgroundColor: BLOOMBERG.DARK_BG,
                      color: BLOOMBERG.WHITE,
                      border: `1px solid ${BLOOMBERG.BORDER}`
                    }}
                  >
                    {frequencies.map((f) => (
                      <option key={f.id} value={f.id}>
                        {f.name}
                      </option>
                    ))}
                  </select>
                </div>

                {/* Alpha (for regularized models) */}
                {['lasso', 'ridge', 'elasticnet', 'auto_lasso', 'auto_ridge', 'auto_elasticnet'].includes(selectedModel) && (
                  <div>
                    <label className="block text-xs font-mono mb-1" style={{ color: BLOOMBERG.GRAY }}>
                      ALPHA (REGULARIZATION)
                    </label>
                    <input
                      type="number"
                      step="0.1"
                      min={0}
                      value={alpha}
                      onChange={(e) => setAlpha(parseFloat(e.target.value) || 1.0)}
                      className="w-full p-2 rounded text-xs font-mono"
                      style={{
                        backgroundColor: BLOOMBERG.DARK_BG,
                        color: BLOOMBERG.WHITE,
                        border: `1px solid ${BLOOMBERG.BORDER}`
                      }}
                    />
                  </div>
                )}

                {/* Test Size */}
                <div>
                  <label className="block text-xs font-mono mb-1" style={{ color: BLOOMBERG.GRAY }}>
                    TEST SIZE (FOR EVALUATION)
                  </label>
                  <input
                    type="number"
                    min={1}
                    value={testSize}
                    onChange={(e) => setTestSize(parseInt(e.target.value) || 5)}
                    className="w-full p-2 rounded text-xs font-mono"
                    style={{
                      backgroundColor: BLOOMBERG.DARK_BG,
                      color: BLOOMBERG.WHITE,
                      border: `1px solid ${BLOOMBERG.BORDER}`
                    }}
                  />
                </div>

                {/* Preprocessing */}
                <div>
                  <label className="block text-xs font-mono mb-1" style={{ color: BLOOMBERG.GRAY }}>
                    PREPROCESSING
                  </label>
                  <select
                    value={preprocess}
                    onChange={(e) => setPreprocess(e.target.value as 'none' | 'scale' | 'difference')}
                    className="w-full p-2 rounded text-xs font-mono"
                    style={{
                      backgroundColor: BLOOMBERG.DARK_BG,
                      color: BLOOMBERG.WHITE,
                      border: `1px solid ${BLOOMBERG.BORDER}`
                    }}
                  >
                    <option value="none">None</option>
                    <option value="scale">Standard Scaling</option>
                    <option value="difference">Differencing</option>
                  </select>
                </div>
              </div>
            )}
          </div>
          )}

          {/* Anomaly Detection Configuration */}
          {analysisMode === 'anomaly' && (
          <div
            className="rounded overflow-hidden"
            style={{ border: `1px solid ${BLOOMBERG.BORDER}` }}
          >
            <div
              className="px-3 py-2 flex items-center gap-2"
              style={{ backgroundColor: BLOOMBERG.HEADER_BG }}
            >
              <AlertTriangle size={14} color={BLOOMBERG.YELLOW} />
              <span className="text-xs font-bold uppercase" style={{ color: BLOOMBERG.WHITE }}>
                Anomaly Detection Settings
              </span>
            </div>
            <div className="p-3 space-y-3">
              <div>
                <label className="block text-xs font-mono mb-1" style={{ color: BLOOMBERG.GRAY }}>
                  DETECTION METHOD
                </label>
                <select
                  value={anomalyMethod}
                  onChange={(e) => setAnomalyMethod(e.target.value as 'zscore' | 'iqr' | 'isolation_forest')}
                  className="w-full p-2 rounded text-xs font-mono"
                  style={{
                    backgroundColor: BLOOMBERG.DARK_BG,
                    color: BLOOMBERG.WHITE,
                    border: `1px solid ${BLOOMBERG.BORDER}`
                  }}
                >
                  <option value="zscore">Z-Score</option>
                  <option value="iqr">IQR (Interquartile Range)</option>
                  <option value="isolation_forest">Isolation Forest</option>
                </select>
              </div>
              <div>
                <label className="block text-xs font-mono mb-1" style={{ color: BLOOMBERG.GRAY }}>
                  THRESHOLD
                </label>
                <input
                  type="number"
                  step="0.5"
                  min={1}
                  max={5}
                  value={anomalyThreshold}
                  onChange={(e) => setAnomalyThreshold(parseFloat(e.target.value) || 3.0)}
                  className="w-full p-2 rounded text-xs font-mono"
                  style={{
                    backgroundColor: BLOOMBERG.DARK_BG,
                    color: BLOOMBERG.WHITE,
                    border: `1px solid ${BLOOMBERG.BORDER}`
                  }}
                />
              </div>
            </div>
          </div>
          )}

          {/* Seasonality Analysis Configuration */}
          {analysisMode === 'seasonality' && (
          <div
            className="rounded overflow-hidden"
            style={{ border: `1px solid ${BLOOMBERG.BORDER}` }}
          >
            <div
              className="px-3 py-2 flex items-center gap-2"
              style={{ backgroundColor: BLOOMBERG.HEADER_BG }}
            >
              <Waves size={14} color={BLOOMBERG.CYAN} />
              <span className="text-xs font-bold uppercase" style={{ color: BLOOMBERG.WHITE }}>
                Seasonality Analysis
              </span>
            </div>
            <div className="p-3">
              <p className="text-xs font-mono" style={{ color: BLOOMBERG.GRAY }}>
                Analyzes time series for seasonal patterns, trend strength, and periodicity.
                Uses FFT and ACF for automatic period detection.
              </p>
            </div>
          </div>
          )}
        </div>

        {/* Run Button */}
        <div className="p-4 border-t" style={{ borderColor: BLOOMBERG.BORDER }}>
          <button
            onClick={runAnalysis}
            disabled={isLoading}
            className="w-full py-3 rounded font-bold text-sm uppercase flex items-center justify-center gap-2 disabled:opacity-50"
            style={{ backgroundColor: BLOOMBERG.ORANGE, color: BLOOMBERG.DARK_BG }}
          >
            {isLoading ? (
              <>
                <RefreshCw size={16} className="animate-spin" />
                {analysisMode === 'forecast' ? 'FORECASTING...' :
                 analysisMode === 'anomaly' ? 'DETECTING...' : 'ANALYZING...'}
              </>
            ) : (
              <>
                <Play size={16} />
                {analysisMode === 'forecast' ? 'RUN FORECAST' :
                 analysisMode === 'anomaly' ? 'DETECT ANOMALIES' : 'ANALYZE SEASONALITY'}
              </>
            )}
          </button>
        </div>
      </div>

      {/* Right Panel - Results */}
      <div className="flex-1 overflow-auto p-4">
        {error && (
          <div
            className="mb-4 p-4 rounded flex items-center gap-3"
            style={{ backgroundColor: BLOOMBERG.PANEL_BG, borderLeft: `3px solid ${BLOOMBERG.RED}` }}
          >
            <AlertCircle size={20} color={BLOOMBERG.RED} />
            <span className="text-sm font-mono" style={{ color: BLOOMBERG.RED }}>{error}</span>
          </div>
        )}

        {!analysisResult && !anomalyResult && !seasonalityResult && !error && (
          <div className="flex items-center justify-center h-full">
            <div className="text-center">
              <LineChart size={64} color={BLOOMBERG.MUTED} className="mx-auto mb-4" />
              <p className="text-sm font-mono" style={{ color: BLOOMBERG.GRAY }}>
                {analysisMode === 'forecast'
                  ? 'Configure model and click "Run Forecast" to see predictions'
                  : analysisMode === 'anomaly'
                  ? 'Click "Detect Anomalies" to find outliers in your data'
                  : 'Click "Analyze Seasonality" to discover patterns'}
              </p>
            </div>
          </div>
        )}

        {/* Anomaly Detection Results */}
        {anomalyResult && anomalyResult.success && anomalyResult.anomalies && (
          <div className="space-y-4">
            <div
              className="rounded overflow-hidden"
              style={{ backgroundColor: BLOOMBERG.PANEL_BG, border: `1px solid ${BLOOMBERG.BORDER}` }}
            >
              <div
                className="px-4 py-3 flex items-center gap-2"
                style={{ backgroundColor: BLOOMBERG.HEADER_BG }}
              >
                <AlertTriangle size={16} color={BLOOMBERG.YELLOW} />
                <span className="text-xs font-bold uppercase tracking-wide" style={{ color: BLOOMBERG.WHITE }}>
                  Anomaly Detection Results
                </span>
              </div>
              <div className="p-4">
                <div className="grid grid-cols-2 gap-4">
                  {Object.entries(anomalyResult.anomalies).map(([symbol, data]) => (
                    <div
                      key={symbol}
                      className="p-3 rounded"
                      style={{ backgroundColor: BLOOMBERG.DARK_BG }}
                    >
                      <div className="flex justify-between items-center mb-2">
                        <span className="text-sm font-bold" style={{ color: BLOOMBERG.CYAN }}>
                          {symbol}
                        </span>
                        <span
                          className="text-xs px-2 py-0.5 rounded"
                          style={{
                            backgroundColor: data.count > 0 ? BLOOMBERG.RED : BLOOMBERG.GREEN,
                            color: BLOOMBERG.WHITE
                          }}
                        >
                          {data.count} anomalies
                        </span>
                      </div>
                      <div className="text-xs font-mono" style={{ color: BLOOMBERG.GRAY }}>
                        <div>Rate: {(data.rate * 100).toFixed(2)}%</div>
                        {data.dates?.length > 0 && (
                          <div className="mt-1">
                            Latest: {data.dates[data.dates.length - 1]}
                          </div>
                        )}
                      </div>
                    </div>
                  ))}
                </div>
              </div>
            </div>
          </div>
        )}

        {/* Seasonality Results */}
        {seasonalityResult && seasonalityResult.success && seasonalityResult.seasonality && (
          <div className="space-y-4">
            <div
              className="rounded overflow-hidden"
              style={{ backgroundColor: BLOOMBERG.PANEL_BG, border: `1px solid ${BLOOMBERG.BORDER}` }}
            >
              <div
                className="px-4 py-3 flex items-center gap-2"
                style={{ backgroundColor: BLOOMBERG.HEADER_BG }}
              >
                <Waves size={16} color={BLOOMBERG.CYAN} />
                <span className="text-xs font-bold uppercase tracking-wide" style={{ color: BLOOMBERG.WHITE }}>
                  Seasonality Analysis Results
                </span>
              </div>
              <div className="p-4">
                <div className="grid grid-cols-2 gap-4">
                  {Object.entries(seasonalityResult.seasonality).map(([symbol, data]) => (
                    <div
                      key={symbol}
                      className="p-3 rounded"
                      style={{ backgroundColor: BLOOMBERG.DARK_BG }}
                    >
                      <div className="flex justify-between items-center mb-2">
                        <span className="text-sm font-bold" style={{ color: BLOOMBERG.CYAN }}>
                          {symbol}
                        </span>
                        <span
                          className="text-xs px-2 py-0.5 rounded"
                          style={{
                            backgroundColor: data.is_seasonal ? BLOOMBERG.GREEN : BLOOMBERG.MUTED,
                            color: BLOOMBERG.WHITE
                          }}
                        >
                          {data.is_seasonal ? 'Seasonal' : 'Non-Seasonal'}
                        </span>
                      </div>
                      <div className="text-xs font-mono space-y-1" style={{ color: BLOOMBERG.GRAY }}>
                        <div className="flex justify-between">
                          <span>Period:</span>
                          <span style={{ color: BLOOMBERG.WHITE }}>{data.period} days</span>
                        </div>
                        <div className="flex justify-between">
                          <span>Seasonal Strength:</span>
                          <span style={{ color: BLOOMBERG.ORANGE }}>{(data.strength * 100).toFixed(1)}%</span>
                        </div>
                        {data.trend_strength !== undefined && (
                          <div className="flex justify-between">
                            <span>Trend Strength:</span>
                            <span style={{ color: BLOOMBERG.GREEN }}>{(data.trend_strength * 100).toFixed(1)}%</span>
                          </div>
                        )}
                      </div>
                    </div>
                  ))}
                </div>
              </div>
            </div>
          </div>
        )}

        {analysisResult && (
          <div className="space-y-4">
            {/* Data Summary Section */}
            {analysisResult.data_summary && (
              <div
                className="rounded overflow-hidden"
                style={{ backgroundColor: BLOOMBERG.PANEL_BG, border: `1px solid ${BLOOMBERG.BORDER}` }}
              >
                <button
                  onClick={() => toggleSection('summary')}
                  className="w-full px-4 py-3 flex items-center justify-between"
                  style={{ backgroundColor: BLOOMBERG.HEADER_BG }}
                >
                  <div className="flex items-center gap-2">
                    <Database size={16} color={BLOOMBERG.CYAN} />
                    <span className="text-xs font-bold uppercase tracking-wide" style={{ color: BLOOMBERG.WHITE }}>
                      Data Summary
                    </span>
                  </div>
                  {expandedSections.summary ? (
                    <ChevronUp size={16} color={BLOOMBERG.GRAY} />
                  ) : (
                    <ChevronDown size={16} color={BLOOMBERG.GRAY} />
                  )}
                </button>

                {expandedSections.summary && (
                  <div className="p-4 grid grid-cols-4 gap-4">
                    <MetricCard
                      label="Total Rows"
                      value={String(analysisResult.data_summary.total_rows)}
                      color={BLOOMBERG.WHITE}
                      icon={<Database size={14} />}
                    />
                    <MetricCard
                      label="Entities"
                      value={String(analysisResult.data_summary.entities)}
                      color={BLOOMBERG.CYAN}
                      icon={<Layers size={14} />}
                    />
                    <MetricCard
                      label="Train Size"
                      value={String(analysisResult.data_summary.train_size)}
                      color={BLOOMBERG.GREEN}
                      icon={<GitBranch size={14} />}
                    />
                    <MetricCard
                      label="Test Size"
                      value={String(analysisResult.data_summary.test_size)}
                      color={BLOOMBERG.YELLOW}
                      icon={<Target size={14} />}
                    />
                  </div>
                )}
              </div>
            )}

            {/* Evaluation Metrics Section */}
            {analysisResult.evaluation_metrics && (
              <div
                className="rounded overflow-hidden"
                style={{ backgroundColor: BLOOMBERG.PANEL_BG, border: `1px solid ${BLOOMBERG.BORDER}` }}
              >
                <button
                  onClick={() => toggleSection('metrics')}
                  className="w-full px-4 py-3 flex items-center justify-between"
                  style={{ backgroundColor: BLOOMBERG.HEADER_BG }}
                >
                  <div className="flex items-center gap-2">
                    <Target size={16} color={BLOOMBERG.GREEN} />
                    <span className="text-xs font-bold uppercase tracking-wide" style={{ color: BLOOMBERG.WHITE }}>
                      Evaluation Metrics
                    </span>
                  </div>
                  {expandedSections.metrics ? (
                    <ChevronUp size={16} color={BLOOMBERG.GRAY} />
                  ) : (
                    <ChevronDown size={16} color={BLOOMBERG.GRAY} />
                  )}
                </button>

                {expandedSections.metrics && (
                  <div className="p-4 grid grid-cols-3 gap-4">
                    <MetricCard
                      label="MAE"
                      value={formatMetric(analysisResult.evaluation_metrics.mae)}
                      color={BLOOMBERG.CYAN}
                      icon={<BarChart2 size={14} />}
                    />
                    <MetricCard
                      label="RMSE"
                      value={formatMetric(analysisResult.evaluation_metrics.rmse)}
                      color={BLOOMBERG.YELLOW}
                      icon={<Activity size={14} />}
                    />
                    <MetricCard
                      label="SMAPE"
                      value={formatMetric(analysisResult.evaluation_metrics.smape)}
                      color={BLOOMBERG.PURPLE}
                      icon={<TrendingUp size={14} />}
                    />
                  </div>
                )}
              </div>
            )}

            {/* Forecast Chart Visualization */}
            {analysisResult.forecast && analysisResult.forecast.length > 0 && (
              <div
                className="rounded overflow-hidden"
                style={{ backgroundColor: BLOOMBERG.PANEL_BG, border: `1px solid ${BLOOMBERG.BORDER}` }}
              >
                <button
                  onClick={() => toggleSection('chart')}
                  className="w-full px-4 py-3 flex items-center justify-between"
                  style={{ backgroundColor: BLOOMBERG.HEADER_BG }}
                >
                  <div className="flex items-center gap-2">
                    <LineChart size={16} color={BLOOMBERG.CYAN} />
                    <span className="text-xs font-bold uppercase tracking-wide" style={{ color: BLOOMBERG.WHITE }}>
                      Forecast Chart
                    </span>
                  </div>
                  {expandedSections.chart ? (
                    <ChevronUp size={16} color={BLOOMBERG.GRAY} />
                  ) : (
                    <ChevronDown size={16} color={BLOOMBERG.GRAY} />
                  )}
                </button>

                {expandedSections.chart && (
                  <div className="p-4">
                    <ForecastChart
                      historicalData={historicalData}
                      forecastData={analysisResult.forecast}
                      height={350}
                    />
                  </div>
                )}
              </div>
            )}

            {/* Forecast Results Section */}
            <div
              className="rounded overflow-hidden"
              style={{ backgroundColor: BLOOMBERG.PANEL_BG, border: `1px solid ${BLOOMBERG.BORDER}` }}
            >
              <button
                onClick={() => toggleSection('forecast')}
                className="w-full px-4 py-3 flex items-center justify-between"
                style={{ backgroundColor: BLOOMBERG.HEADER_BG }}
              >
                <div className="flex items-center gap-2">
                  <TrendingUp size={16} color={BLOOMBERG.ORANGE} />
                  <span className="text-xs font-bold uppercase tracking-wide" style={{ color: BLOOMBERG.WHITE }}>
                    Forecast Results
                  </span>
                  {analysisResult.model && (
                    <span
                      className="ml-2 px-2 py-0.5 rounded text-xs font-mono"
                      style={{ backgroundColor: BLOOMBERG.ORANGE, color: BLOOMBERG.DARK_BG }}
                    >
                      {analysisResult.model.toUpperCase()}
                    </span>
                  )}
                </div>
                {expandedSections.forecast ? (
                  <ChevronUp size={16} color={BLOOMBERG.GRAY} />
                ) : (
                  <ChevronDown size={16} color={BLOOMBERG.GRAY} />
                )}
              </button>

              {expandedSections.forecast && analysisResult.forecast && (
                <div className="p-4">
                  {/* Forecast info */}
                  <div className="mb-4 flex gap-4 text-xs font-mono" style={{ color: BLOOMBERG.GRAY }}>
                    <span>Horizon: <span style={{ color: BLOOMBERG.WHITE }}>{analysisResult.horizon}</span></span>
                    <span>Frequency: <span style={{ color: BLOOMBERG.WHITE }}>{analysisResult.frequency}</span></span>
                    {analysisResult.best_params && (
                      <span>Auto-tuned: <CheckCircle2 size={12} color={BLOOMBERG.GREEN} className="inline ml-1" /></span>
                    )}
                  </div>

                  {/* Forecast table */}
                  <div
                    className="rounded overflow-hidden"
                    style={{ border: `1px solid ${BLOOMBERG.BORDER}` }}
                  >
                    <table className="w-full text-xs font-mono">
                      <thead>
                        <tr style={{ backgroundColor: BLOOMBERG.HEADER_BG }}>
                          <th className="px-3 py-2 text-left" style={{ color: BLOOMBERG.GRAY }}>ENTITY</th>
                          <th className="px-3 py-2 text-left" style={{ color: BLOOMBERG.GRAY }}>TIME</th>
                          <th className="px-3 py-2 text-right" style={{ color: BLOOMBERG.GRAY }}>FORECAST</th>
                        </tr>
                      </thead>
                      <tbody>
                        {analysisResult.forecast.slice(0, forecastTableExpanded ? EXPANDED_ROWS : INITIAL_ROWS).map((row, idx) => (
                          <tr
                            key={idx}
                            style={{
                              backgroundColor: idx % 2 === 0 ? BLOOMBERG.DARK_BG : BLOOMBERG.PANEL_BG
                            }}
                          >
                            <td className="px-3 py-2" style={{ color: BLOOMBERG.CYAN }}>
                              {row.entity_id}
                            </td>
                            <td className="px-3 py-2" style={{ color: BLOOMBERG.WHITE }}>
                              {row.time}
                            </td>
                            <td className="px-3 py-2 text-right" style={{ color: BLOOMBERG.GREEN }}>
                              {typeof row.value === 'number' ? row.value.toFixed(2) : row.value}
                            </td>
                          </tr>
                        ))}
                      </tbody>
                    </table>
                    {analysisResult.forecast.length > INITIAL_ROWS && (
                      <button
                        onClick={() => setForecastTableExpanded(!forecastTableExpanded)}
                        className="w-full px-3 py-2 text-xs font-mono text-center flex items-center justify-center gap-2 hover:opacity-80 transition-opacity"
                        style={{ backgroundColor: BLOOMBERG.HEADER_BG, color: BLOOMBERG.ORANGE }}
                      >
                        {forecastTableExpanded ? (
                          <>
                            <ChevronUp size={12} />
                            Show Less ({INITIAL_ROWS} rows)
                          </>
                        ) : (
                          <>
                            <ChevronDown size={12} />
                            Show More ({Math.min(analysisResult.forecast.length, EXPANDED_ROWS)} of {analysisResult.forecast.length} rows)
                          </>
                        )}
                      </button>
                    )}
                  </div>
                </div>
              )}
            </div>

            {/* Best Params (if auto-tuned) */}
            {analysisResult.best_params && Object.keys(analysisResult.best_params).length > 0 && (
              <div
                className="rounded p-4"
                style={{ backgroundColor: BLOOMBERG.PANEL_BG, border: `1px solid ${BLOOMBERG.BORDER}` }}
              >
                <div className="flex items-center gap-2 mb-3">
                  <Zap size={16} color={BLOOMBERG.YELLOW} />
                  <span className="text-xs font-bold uppercase" style={{ color: BLOOMBERG.WHITE }}>
                    Best Parameters (Auto-tuned)
                  </span>
                </div>
                <div className="grid grid-cols-3 gap-2">
                  {Object.entries(analysisResult.best_params).map(([key, value]) => (
                    <div key={key} className="flex justify-between p-2 rounded" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
                      <span className="text-xs font-mono" style={{ color: BLOOMBERG.GRAY }}>{key}</span>
                      <span className="text-xs font-mono" style={{ color: BLOOMBERG.CYAN }}>
                        {typeof value === 'number' ? value.toFixed(4) : String(value)}
                      </span>
                    </div>
                  ))}
                </div>
              </div>
            )}
          </div>
        )}
      </div>
    </div>
  );
}

// Metric Card Component
function MetricCard({
  label,
  value,
  color,
  icon,
  large = false
}: {
  label: string;
  value: string;
  color: string;
  icon: React.ReactNode;
  large?: boolean;
}) {
  return (
    <div
      className={`p-3 rounded ${large ? 'col-span-3' : ''}`}
      style={{ backgroundColor: BLOOMBERG.DARK_BG }}
    >
      <div className="flex items-center gap-2 mb-1">
        <span style={{ color: BLOOMBERG.GRAY }}>{icon}</span>
        <span className="text-xs font-mono uppercase" style={{ color: BLOOMBERG.GRAY }}>
          {label}
        </span>
      </div>
      <span
        className={`font-bold font-mono ${large ? 'text-2xl' : 'text-lg'}`}
        style={{ color }}
      >
        {value}
      </span>
    </div>
  );
}

// Forecast Chart Component - Interactive SVG-based line chart with tooltips
interface ChartDataPoint {
  time: string;
  value: number;
  type: 'historical' | 'forecast';
  entity_id?: string;
}

interface TooltipState {
  visible: boolean;
  x: number;
  y: number;
  data: ChartDataPoint | null;
}

function ForecastChart({
  historicalData,
  forecastData,
  height = 300
}: {
  historicalData: { time: string; value: number }[];
  forecastData: ForecastResult[];
  height?: number;
}) {
  const [tooltip, setTooltip] = useState<TooltipState>({ visible: false, x: 0, y: 0, data: null });
  const [zoomLevel, setZoomLevel] = useState(1);
  const [panOffset, setPanOffset] = useState(0);
  const svgRef = React.useRef<SVGSVGElement>(null);

  const chartData = useMemo(() => {
    const historical: ChartDataPoint[] = historicalData.map(d => ({
      time: d.time,
      value: d.value,
      type: 'historical' as const
    }));

    const forecast: ChartDataPoint[] = forecastData.map(d => ({
      time: d.time,
      value: d.value,
      type: 'forecast' as const,
      entity_id: d.entity_id
    }));

    return [...historical, ...forecast];
  }, [historicalData, forecastData]);

  if (chartData.length === 0) {
    return (
      <div
        className="flex items-center justify-center"
        style={{ height, backgroundColor: BLOOMBERG.DARK_BG }}
      >
        <span className="text-sm font-mono" style={{ color: BLOOMBERG.GRAY }}>
          No data to display
        </span>
      </div>
    );
  }

  const baseWidth = 800;
  const width = baseWidth * zoomLevel;
  const padding = { top: 20, right: 60, bottom: 40, left: 60 };
  const chartWidth = width - padding.left - padding.right;
  const chartHeight = height - padding.top - padding.bottom;

  // Calculate scales
  const values = chartData.map(d => d.value);
  const minValue = Math.min(...values);
  const maxValue = Math.max(...values);
  const valueRange = maxValue - minValue || 1;
  const yMin = minValue - valueRange * 0.1;
  const yMax = maxValue + valueRange * 0.1;

  const xScale = (index: number) => padding.left + (index / (chartData.length - 1 || 1)) * chartWidth;
  const yScale = (value: number) => padding.top + chartHeight - ((value - yMin) / (yMax - yMin)) * chartHeight;

  // Find split point between historical and forecast
  const historicalEndIndex = historicalData.length - 1;

  // Generate path for historical data
  const historicalPath = historicalData
    .map((d, i) => {
      const x = xScale(i);
      const y = yScale(d.value);
      return `${i === 0 ? 'M' : 'L'} ${x} ${y}`;
    })
    .join(' ');

  // Generate path for forecast data (starts from last historical point)
  const forecastPath = forecastData.length > 0
    ? [
        historicalData.length > 0
          ? `M ${xScale(historicalEndIndex)} ${yScale(historicalData[historicalEndIndex]?.value ?? forecastData[0].value)}`
          : `M ${xScale(0)} ${yScale(forecastData[0].value)}`,
        ...forecastData.map((d, i) => {
          const x = xScale(historicalData.length + i);
          const y = yScale(d.value);
          return `L ${x} ${y}`;
        })
      ].join(' ')
    : '';

  // Y-axis ticks
  const yTicks = Array.from({ length: 5 }, (_, i) => {
    const value = yMin + (yMax - yMin) * (i / 4);
    return { value, y: yScale(value) };
  });

  // X-axis labels (show first, middle, last, and split point)
  const xLabels = [
    { index: 0, label: chartData[0]?.time.split('T')[0] || '' },
    { index: historicalEndIndex, label: chartData[historicalEndIndex]?.time.split('T')[0] || '' },
    { index: chartData.length - 1, label: chartData[chartData.length - 1]?.time.split('T')[0] || '' }
  ].filter((item, index, arr) =>
    arr.findIndex(t => t.index === item.index) === index && item.label
  );

  // Handle mouse events for interactive tooltips
  const handleMouseEnter = (data: ChartDataPoint, x: number, y: number) => {
    setTooltip({ visible: true, x, y, data });
  };

  const handleMouseLeave = () => {
    setTooltip({ visible: false, x: 0, y: 0, data: null });
  };

  // Zoom controls
  const handleZoomIn = () => setZoomLevel(prev => Math.min(prev + 0.5, 3));
  const handleZoomOut = () => setZoomLevel(prev => Math.max(prev - 0.5, 1));
  const handleResetZoom = () => { setZoomLevel(1); setPanOffset(0); };

  return (
    <div className="w-full">
      {/* Zoom Controls */}
      <div className="flex justify-end gap-2 mb-2">
        <button
          onClick={handleZoomOut}
          className="px-2 py-1 rounded text-xs font-mono"
          style={{ backgroundColor: BLOOMBERG.HEADER_BG, color: BLOOMBERG.WHITE, border: `1px solid ${BLOOMBERG.BORDER}` }}
          disabled={zoomLevel <= 1}
        >
          −
        </button>
        <span className="px-2 py-1 text-xs font-mono" style={{ color: BLOOMBERG.GRAY }}>
          {Math.round(zoomLevel * 100)}%
        </span>
        <button
          onClick={handleZoomIn}
          className="px-2 py-1 rounded text-xs font-mono"
          style={{ backgroundColor: BLOOMBERG.HEADER_BG, color: BLOOMBERG.WHITE, border: `1px solid ${BLOOMBERG.BORDER}` }}
          disabled={zoomLevel >= 3}
        >
          +
        </button>
        {zoomLevel !== 1 && (
          <button
            onClick={handleResetZoom}
            className="px-2 py-1 rounded text-xs font-mono"
            style={{ backgroundColor: BLOOMBERG.HEADER_BG, color: BLOOMBERG.ORANGE, border: `1px solid ${BLOOMBERG.BORDER}` }}
          >
            Reset
          </button>
        )}
      </div>

      {/* Chart Container with horizontal scroll */}
      <div className="overflow-x-auto" style={{ position: 'relative' }}>
        <svg
          ref={svgRef}
          width={width}
          height={height}
          viewBox={`0 0 ${width} ${height}`}
          style={{ backgroundColor: BLOOMBERG.DARK_BG, minWidth: baseWidth }}
        >
          {/* Grid lines */}
          {yTicks.map((tick, i) => (
            <line
              key={i}
              x1={padding.left}
              y1={tick.y}
              x2={width - padding.right}
              y2={tick.y}
              stroke={BLOOMBERG.BORDER}
              strokeWidth={1}
              strokeDasharray="4,4"
            />
          ))}

          {/* Forecast region background */}
          {forecastData.length > 0 && (
            <rect
              x={xScale(historicalEndIndex)}
              y={padding.top}
              width={chartWidth - xScale(historicalEndIndex) + padding.left}
              height={chartHeight}
              fill={BLOOMBERG.ORANGE}
              opacity={0.05}
            />
          )}

          {/* Vertical line at forecast start */}
          {forecastData.length > 0 && historicalData.length > 0 && (
            <line
              x1={xScale(historicalEndIndex)}
              y1={padding.top}
              x2={xScale(historicalEndIndex)}
              y2={height - padding.bottom}
              stroke={BLOOMBERG.ORANGE}
              strokeWidth={1}
              strokeDasharray="6,4"
              opacity={0.6}
            />
          )}

          {/* Historical line */}
          {historicalPath && (
            <path
              d={historicalPath}
              fill="none"
              stroke={BLOOMBERG.CYAN}
              strokeWidth={2}
            />
          )}

          {/* Forecast line */}
          {forecastPath && (
            <path
              d={forecastPath}
              fill="none"
              stroke={BLOOMBERG.ORANGE}
              strokeWidth={2}
              strokeDasharray="6,3"
            />
          )}

          {/* Historical data points - Interactive */}
          {historicalData.map((d, i) => {
            const cx = xScale(i);
            const cy = yScale(d.value);
            const dataPoint: ChartDataPoint = { time: d.time, value: d.value, type: 'historical' };
            return (
              <g key={`hist-${i}`}>
                {/* Larger invisible hit area */}
                <circle
                  cx={cx}
                  cy={cy}
                  r={12}
                  fill="transparent"
                  style={{ cursor: 'pointer' }}
                  onMouseEnter={() => handleMouseEnter(dataPoint, cx, cy)}
                  onMouseLeave={handleMouseLeave}
                />
                {/* Visible point */}
                <circle
                  cx={cx}
                  cy={cy}
                  r={tooltip.data?.time === d.time && tooltip.data?.type === 'historical' ? 6 : 3}
                  fill={BLOOMBERG.CYAN}
                  style={{ transition: 'r 0.15s ease', pointerEvents: 'none' }}
                />
              </g>
            );
          })}

          {/* Forecast data points - Interactive */}
          {forecastData.map((d, i) => {
            const cx = xScale(historicalData.length + i);
            const cy = yScale(d.value);
            const dataPoint: ChartDataPoint = { time: d.time, value: d.value, type: 'forecast', entity_id: d.entity_id };
            return (
              <g key={`forecast-${i}`}>
                {/* Larger invisible hit area */}
                <circle
                  cx={cx}
                  cy={cy}
                  r={12}
                  fill="transparent"
                  style={{ cursor: 'pointer' }}
                  onMouseEnter={() => handleMouseEnter(dataPoint, cx, cy)}
                  onMouseLeave={handleMouseLeave}
                />
                {/* Visible point */}
                <circle
                  cx={cx}
                  cy={cy}
                  r={tooltip.data?.time === d.time && tooltip.data?.type === 'forecast' ? 7 : 4}
                  fill={BLOOMBERG.ORANGE}
                  stroke={BLOOMBERG.DARK_BG}
                  strokeWidth={1}
                  style={{ transition: 'r 0.15s ease', pointerEvents: 'none' }}
                />
              </g>
            );
          })}

          {/* Y-axis labels */}
          {yTicks.map((tick, i) => (
            <text
              key={i}
              x={padding.left - 10}
              y={tick.y + 4}
              textAnchor="end"
              fontSize={10}
              fontFamily="monospace"
              fill={BLOOMBERG.GRAY}
            >
              {tick.value.toFixed(2)}
            </text>
          ))}

          {/* X-axis labels */}
          {xLabels.map((item, i) => (
            <text
              key={i}
              x={xScale(item.index)}
              y={height - padding.bottom + 20}
              textAnchor="middle"
              fontSize={10}
              fontFamily="monospace"
              fill={BLOOMBERG.GRAY}
            >
              {item.label}
            </text>
          ))}

          {/* Legend */}
          <g transform={`translate(${width - padding.right - 120}, ${padding.top})`}>
            <rect x={0} y={0} width={120} height={50} fill={BLOOMBERG.PANEL_BG} rx={4} />
            <line x1={10} y1={15} x2={35} y2={15} stroke={BLOOMBERG.CYAN} strokeWidth={2} />
            <text x={42} y={18} fontSize={10} fontFamily="monospace" fill={BLOOMBERG.WHITE}>Historical</text>
            <line x1={10} y1={35} x2={35} y2={35} stroke={BLOOMBERG.ORANGE} strokeWidth={2} strokeDasharray="6,3" />
            <text x={42} y={38} fontSize={10} fontFamily="monospace" fill={BLOOMBERG.WHITE}>Forecast</text>
          </g>

          {/* Forecast label */}
          {forecastData.length > 0 && historicalData.length > 0 && (
            <text
              x={xScale(historicalEndIndex) + 5}
              y={padding.top + 15}
              fontSize={10}
              fontFamily="monospace"
              fill={BLOOMBERG.ORANGE}
            >
              ▼ Forecast
            </text>
          )}
        </svg>

        {/* Tooltip */}
        {tooltip.visible && tooltip.data && (
          <div
            className="absolute pointer-events-none px-3 py-2 rounded shadow-lg z-50"
            style={{
              left: Math.min(tooltip.x + 10, width - 180),
              top: Math.max(tooltip.y - 60, 10),
              backgroundColor: BLOOMBERG.PANEL_BG,
              border: `1px solid ${tooltip.data.type === 'forecast' ? BLOOMBERG.ORANGE : BLOOMBERG.CYAN}`,
              minWidth: 160
            }}
          >
            <div className="text-xs font-mono font-bold mb-1" style={{ color: tooltip.data.type === 'forecast' ? BLOOMBERG.ORANGE : BLOOMBERG.CYAN }}>
              {tooltip.data.type === 'forecast' ? '📈 FORECAST' : '📊 HISTORICAL'}
            </div>
            <div className="text-xs font-mono" style={{ color: BLOOMBERG.GRAY }}>
              Date: <span style={{ color: BLOOMBERG.WHITE }}>{tooltip.data.time.split('T')[0]}</span>
            </div>
            <div className="text-xs font-mono" style={{ color: BLOOMBERG.GRAY }}>
              Value: <span style={{ color: BLOOMBERG.GREEN, fontWeight: 'bold' }}>{tooltip.data.value.toFixed(4)}</span>
            </div>
            {tooltip.data.entity_id && (
              <div className="text-xs font-mono" style={{ color: BLOOMBERG.GRAY }}>
                Entity: <span style={{ color: BLOOMBERG.CYAN }}>{tooltip.data.entity_id}</span>
              </div>
            )}
          </div>
        )}
      </div>
    </div>
  );
}
