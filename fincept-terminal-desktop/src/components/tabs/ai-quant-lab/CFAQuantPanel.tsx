import React, { useState, useEffect, useCallback, useMemo } from 'react';
import {
  TrendingUp,
  Calculator,
  RefreshCw,
  AlertCircle,
  CheckCircle2,
  ChevronRight,
  ChevronLeft,
  Search,
  Play,
  Download,
  Database,
  Brain,
  Shuffle,
  Target,
  BarChart3,
  Info,
  Sparkles,
  ArrowUpRight,
  ArrowDownRight,
  Minus,
  Copy,
  HelpCircle,
  Zap,
  LineChart,
  Activity,
  TrendingDown,
  ZoomIn,
  ZoomOut,
  RotateCcw
} from 'lucide-react';
import {
  LineChart as RechartsLineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  ResponsiveContainer,
  Area,
  AreaChart,
  ReferenceLine,
  ComposedChart,
  Bar,
  Scatter,
  Brush,
  ReferenceArea
} from 'recharts';
import { quantAnalyticsService, type QuantAnalyticsResult } from '@/services/aiQuantLab/quantAnalyticsService';
import { yfinanceService } from '@/services/yfinanceService';

// Bloomberg-style color palette
const BB = {
  // Core Bloomberg colors
  black: '#000000',
  darkBg: '#0a0a0a',
  panelBg: '#121212',
  cardBg: '#1a1a1a',
  borderDark: '#2a2a2a',
  borderLight: '#3a3a3a',

  // Accent colors
  amber: '#FF8C00',
  amberLight: '#FFA500',
  amberDim: '#CC7000',

  // Status colors
  green: '#00C853',
  greenDim: '#00A040',
  red: '#FF3D00',
  redDim: '#CC3000',

  // Text colors
  textPrimary: '#FFFFFF',
  textSecondary: '#B0B0B0',
  textMuted: '#707070',
  textAmber: '#FF9500',

  // Chart colors
  chartLine: '#00D4FF',
  chartArea: 'rgba(0, 212, 255, 0.1)',
  chartGrid: '#1f1f1f',

  // Bloomberg blue
  blue: '#0066CC',
  blueLight: '#0088FF',
};

type Step = 'data' | 'analysis' | 'results';
type DataSourceType = 'symbol' | 'manual';

interface AnalysisConfig {
  id: string;
  name: string;
  shortName: string;
  description: string;
  icon: React.ElementType;
  category: 'time_series' | 'machine_learning' | 'sampling' | 'data_quality';
  color: string;
  minDataPoints: number;
  helpText: string;
}

const analysisConfigs: AnalysisConfig[] = [
  {
    id: 'trend_analysis',
    name: 'Trend Analysis',
    shortName: 'TREND',
    description: 'Detect upward/downward trends',
    icon: TrendingUp,
    category: 'time_series',
    color: BB.green,
    minDataPoints: 5,
    helpText: 'Linear regression to identify significant trends in price movements.'
  },
  {
    id: 'stationarity_test',
    name: 'Stationarity Test',
    shortName: 'ADF',
    description: 'Unit root test (ADF)',
    icon: BarChart3,
    category: 'time_series',
    color: BB.blue,
    minDataPoints: 20,
    helpText: 'Tests if time series has constant mean/variance. Important for ARIMA modeling.'
  },
  {
    id: 'arima_model',
    name: 'ARIMA Forecast',
    shortName: 'ARIMA',
    description: 'Time series prediction',
    icon: LineChart,
    category: 'time_series',
    color: BB.amber,
    minDataPoints: 20,
    helpText: 'AutoRegressive Integrated Moving Average for forecasting.'
  },
  {
    id: 'forecasting',
    name: 'Exp. Smoothing',
    shortName: 'ETS',
    description: 'Simple forecasting',
    icon: Sparkles,
    category: 'time_series',
    color: BB.amberLight,
    minDataPoints: 10,
    helpText: 'Exponential smoothing methods for quick predictions.'
  },
  {
    id: 'supervised_learning',
    name: 'ML Prediction',
    shortName: 'ML',
    description: 'Machine learning models',
    icon: Brain,
    category: 'machine_learning',
    color: BB.blueLight,
    minDataPoints: 30,
    helpText: 'Compare Ridge, Lasso, Random Forest for predictions.'
  },
  {
    id: 'unsupervised_learning',
    name: 'Pattern Discovery',
    shortName: 'PCA/K',
    description: 'PCA and clustering',
    icon: Target,
    category: 'machine_learning',
    color: BB.amber,
    minDataPoints: 20,
    helpText: 'Find hidden patterns with PCA and K-Means clustering.'
  },
  {
    id: 'resampling_methods',
    name: 'Bootstrap',
    shortName: 'BOOT',
    description: 'Confidence intervals',
    icon: Shuffle,
    category: 'sampling',
    color: BB.green,
    minDataPoints: 10,
    helpText: 'Bootstrap/jackknife for robust statistical inference.'
  },
  {
    id: 'validate_data',
    name: 'Data Quality',
    shortName: 'QA',
    description: 'Validate data quality',
    icon: CheckCircle2,
    category: 'data_quality',
    color: BB.greenDim,
    minDataPoints: 5,
    helpText: 'Check for missing values, outliers, and data issues.'
  },
];

export function CFAQuantPanel() {
  const [currentStep, setCurrentStep] = useState<Step>('data');
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [analysisResult, setAnalysisResult] = useState<QuantAnalyticsResult<Record<string, unknown>> | null>(null);

  const [dataSourceType, setDataSourceType] = useState<DataSourceType>('symbol');
  const [symbolInput, setSymbolInput] = useState('AAPL');
  const [historicalDays, setHistoricalDays] = useState(365);
  const [manualData, setManualData] = useState('');
  const [priceData, setPriceData] = useState<number[]>([]);
  const [priceDates, setPriceDates] = useState<string[]>([]);
  const [priceDataLoading, setPriceDataLoading] = useState(false);
  const [dataStats, setDataStats] = useState<{ min: number; max: number; mean: number; std: number; change: number; changePercent: number } | null>(null);

  const [selectedAnalysis, setSelectedAnalysis] = useState<string>('trend_analysis');
  const [showHelp, setShowHelp] = useState<string | null>(null);

  // Zoom state for charts
  const [dataChartZoom, setDataChartZoom] = useState<{ startIndex: number; endIndex: number } | null>(null);
  const [resultsChartZoom, setResultsChartZoom] = useState<{ startIndex: number; endIndex: number } | null>(null);
  const [refAreaLeft, setRefAreaLeft] = useState<number | null>(null);
  const [refAreaRight, setRefAreaRight] = useState<number | null>(null);
  const [isSelecting, setIsSelecting] = useState(false);
  const [activeChart, setActiveChart] = useState<'data' | 'results' | null>(null);

  // Zoom helper functions
  const handleZoomIn = useCallback((chartType: 'data' | 'results') => {
    const currentZoom = chartType === 'data' ? dataChartZoom : resultsChartZoom;
    const setZoom = chartType === 'data' ? setDataChartZoom : setResultsChartZoom;
    const dataLength = chartType === 'results' && analysisResult ?
      (getResultChartDataLength()) : priceData.length;

    if (dataLength <= 5) return;

    const currentStart = currentZoom?.startIndex ?? 0;
    const currentEnd = currentZoom?.endIndex ?? dataLength - 1;
    const range = currentEnd - currentStart;
    const center = Math.floor((currentStart + currentEnd) / 2);
    const newRange = Math.max(5, Math.floor(range * 0.7));
    const newStart = Math.max(0, center - Math.floor(newRange / 2));
    const newEnd = Math.min(dataLength - 1, newStart + newRange);

    setZoom({ startIndex: newStart, endIndex: newEnd });
  }, [priceData.length, dataChartZoom, resultsChartZoom, analysisResult]);

  const handleZoomOut = useCallback((chartType: 'data' | 'results') => {
    const currentZoom = chartType === 'data' ? dataChartZoom : resultsChartZoom;
    const setZoom = chartType === 'data' ? setDataChartZoom : setResultsChartZoom;
    const dataLength = chartType === 'results' && analysisResult ?
      (getResultChartDataLength()) : priceData.length;

    if (!currentZoom) return;

    const currentStart = currentZoom.startIndex;
    const currentEnd = currentZoom.endIndex;
    const range = currentEnd - currentStart;
    const center = Math.floor((currentStart + currentEnd) / 2);
    const newRange = Math.min(dataLength - 1, Math.floor(range * 1.5));
    const newStart = Math.max(0, center - Math.floor(newRange / 2));
    const newEnd = Math.min(dataLength - 1, newStart + newRange);

    if (newStart === 0 && newEnd === dataLength - 1) {
      setZoom(null);
    } else {
      setZoom({ startIndex: newStart, endIndex: newEnd });
    }
  }, [priceData.length, dataChartZoom, resultsChartZoom, analysisResult]);

  const handleResetZoom = useCallback((chartType: 'data' | 'results') => {
    const setZoom = chartType === 'data' ? setDataChartZoom : setResultsChartZoom;
    setZoom(null);
  }, []);

  // Pan navigation functions
  const handlePanLeft = useCallback((chartType: 'data' | 'results') => {
    const currentZoom = chartType === 'data' ? dataChartZoom : resultsChartZoom;
    const setZoom = chartType === 'data' ? setDataChartZoom : setResultsChartZoom;

    if (!currentZoom) return;

    const range = currentZoom.endIndex - currentZoom.startIndex;
    const step = Math.max(1, Math.floor(range * 0.2)); // Pan by 20% of visible range
    const newStart = Math.max(0, currentZoom.startIndex - step);
    const newEnd = newStart + range;

    setZoom({ startIndex: newStart, endIndex: newEnd });
  }, [dataChartZoom, resultsChartZoom]);

  const handlePanRight = useCallback((chartType: 'data' | 'results') => {
    const currentZoom = chartType === 'data' ? dataChartZoom : resultsChartZoom;
    const setZoom = chartType === 'data' ? setDataChartZoom : setResultsChartZoom;
    const dataLength = chartType === 'results' && analysisResult ?
      getResultChartDataLength() : priceData.length;

    if (!currentZoom) return;

    const range = currentZoom.endIndex - currentZoom.startIndex;
    const step = Math.max(1, Math.floor(range * 0.2)); // Pan by 20% of visible range
    const newEnd = Math.min(dataLength - 1, currentZoom.endIndex + step);
    const newStart = newEnd - range;

    setZoom({ startIndex: Math.max(0, newStart), endIndex: newEnd });
  }, [dataChartZoom, resultsChartZoom, priceData.length, analysisResult]);

  // Check if we can pan in each direction
  const canPanLeft = useCallback((chartType: 'data' | 'results') => {
    const currentZoom = chartType === 'data' ? dataChartZoom : resultsChartZoom;
    return currentZoom !== null && currentZoom.startIndex > 0;
  }, [dataChartZoom, resultsChartZoom]);

  const canPanRight = useCallback((chartType: 'data' | 'results') => {
    const currentZoom = chartType === 'data' ? dataChartZoom : resultsChartZoom;
    const dataLength = chartType === 'results' && analysisResult ?
      getResultChartDataLength() : priceData.length;
    return currentZoom !== null && currentZoom.endIndex < dataLength - 1;
  }, [dataChartZoom, resultsChartZoom, priceData.length, analysisResult]);

  const getResultChartDataLength = useCallback(() => {
    if (!analysisResult?.result) return priceData.length;
    if (selectedAnalysis === 'forecasting') {
      const forecasts = (analysisResult.result as Record<string, unknown>).value as number[];
      return priceData.length + (forecasts?.length || 0);
    }
    if (selectedAnalysis === 'trend_analysis') {
      // Account for 5-period projection
      return priceData.length + 5;
    }
    if (selectedAnalysis === 'arima_model') {
      // Account for forecast projection (5 periods default or actual forecasts length)
      const result = analysisResult.result as Record<string, unknown>;
      const value = result.value as Record<string, unknown>;
      const forecasts = value?.forecasts as number[];
      return priceData.length + (forecasts?.length || 5);
    }
    return priceData.length;
  }, [analysisResult, priceData.length, selectedAnalysis]);

  // Mouse selection handlers for drag zoom
  const handleMouseDown = useCallback((e: any, chartType: 'data' | 'results') => {
    const activeLabel = typeof e.activeLabel === 'string' ? parseInt(e.activeLabel, 10) : e.activeLabel;
    if (activeLabel !== undefined && !isNaN(activeLabel)) {
      setRefAreaLeft(activeLabel);
      setIsSelecting(true);
      setActiveChart(chartType);
    }
  }, []);

  const handleMouseMove = useCallback((e: any) => {
    const activeLabel = typeof e.activeLabel === 'string' ? parseInt(e.activeLabel, 10) : e.activeLabel;
    if (isSelecting && activeLabel !== undefined && !isNaN(activeLabel)) {
      setRefAreaRight(activeLabel);
    }
  }, [isSelecting]);

  const handleMouseUp = useCallback(() => {
    if (refAreaLeft !== null && refAreaRight !== null && activeChart) {
      const left = Math.min(refAreaLeft, refAreaRight);
      const right = Math.max(refAreaLeft, refAreaRight);

      if (right - left >= 2) {
        const setZoom = activeChart === 'data' ? setDataChartZoom : setResultsChartZoom;
        setZoom({ startIndex: left, endIndex: right });
      }
    }
    setRefAreaLeft(null);
    setRefAreaRight(null);
    setIsSelecting(false);
    setActiveChart(null);
  }, [refAreaLeft, refAreaRight, activeChart]);

  // Mouse wheel handler for zoom and horizontal scroll
  const handleWheel = useCallback((e: React.WheelEvent, chartType: 'data' | 'results') => {
    e.preventDefault();

    // Shift + wheel = horizontal pan
    if (e.shiftKey) {
      if (e.deltaY > 0) {
        handlePanRight(chartType);
      } else if (e.deltaY < 0) {
        handlePanLeft(chartType);
      }
      return;
    }

    // Ctrl + wheel = zoom in/out
    if (e.ctrlKey || e.metaKey) {
      if (e.deltaY < 0) {
        handleZoomIn(chartType);
      } else if (e.deltaY > 0) {
        handleZoomOut(chartType);
      }
      return;
    }

    // Plain wheel = horizontal pan (more intuitive for time series)
    if (e.deltaY > 0) {
      handlePanRight(chartType);
    } else if (e.deltaY < 0) {
      handlePanLeft(chartType);
    }
  }, [handlePanLeft, handlePanRight, handleZoomIn, handleZoomOut]);

  const calculateStats = useCallback((data: number[]) => {
    if (data.length === 0) return null;
    const mean = data.reduce((a, b) => a + b, 0) / data.length;
    const variance = data.reduce((a, b) => a + Math.pow(b - mean, 2), 0) / data.length;
    const first = data[0];
    const last = data[data.length - 1];
    const change = last - first;
    const changePercent = (change / first) * 100;
    return {
      min: Math.min(...data),
      max: Math.max(...data),
      mean,
      std: Math.sqrt(variance),
      change,
      changePercent,
    };
  }, []);

  const chartData = useMemo(() => {
    return priceData.map((value, index) => ({
      index,
      value,
      date: priceDates[index] || `Day ${index + 1}`,
    }));
  }, [priceData, priceDates]);

  // Helper to get date for a given index
  const getDateForIndex = useCallback((index: number): string => {
    if (index >= 0 && index < priceDates.length) {
      return priceDates[index];
    }
    return `Day ${index + 1}`;
  }, [priceDates]);

  // Zoomed chart data for data step
  const zoomedChartData = useMemo(() => {
    if (!dataChartZoom) return chartData;
    return chartData.slice(dataChartZoom.startIndex, dataChartZoom.endIndex + 1);
  }, [chartData, dataChartZoom]);

  // Zoom controls component with navigation
  const ZoomControls = ({ chartType, isZoomed }: { chartType: 'data' | 'results'; isZoomed: boolean }) => {
    const canGoLeft = canPanLeft(chartType);
    const canGoRight = canPanRight(chartType);

    return (
      <div className="flex items-center gap-1">
        {/* Pan Left */}
        <button
          onClick={() => handlePanLeft(chartType)}
          disabled={!canGoLeft}
          className="p-1.5 transition-colors hover:opacity-80"
          style={{
            backgroundColor: BB.cardBg,
            border: `1px solid ${BB.borderDark}`,
            opacity: canGoLeft ? 1 : 0.4,
            cursor: canGoLeft ? 'pointer' : 'not-allowed'
          }}
          title="Pan Left"
        >
          <ChevronLeft size={14} style={{ color: canGoLeft ? BB.amber : BB.textSecondary }} />
        </button>
        {/* Zoom In */}
        <button
          onClick={() => handleZoomIn(chartType)}
          className="p-1.5 transition-colors hover:opacity-80"
          style={{ backgroundColor: BB.cardBg, border: `1px solid ${BB.borderDark}` }}
          title="Zoom In"
        >
          <ZoomIn size={14} style={{ color: BB.textSecondary }} />
        </button>
        {/* Zoom Out */}
        <button
          onClick={() => handleZoomOut(chartType)}
          disabled={!isZoomed}
          className="p-1.5 transition-colors hover:opacity-80"
          style={{
            backgroundColor: BB.cardBg,
            border: `1px solid ${BB.borderDark}`,
            opacity: isZoomed ? 1 : 0.4,
            cursor: isZoomed ? 'pointer' : 'not-allowed'
          }}
          title="Zoom Out"
        >
          <ZoomOut size={14} style={{ color: BB.textSecondary }} />
        </button>
        {/* Reset */}
        <button
          onClick={() => handleResetZoom(chartType)}
          disabled={!isZoomed}
          className="p-1.5 transition-colors hover:opacity-80"
          style={{
            backgroundColor: BB.cardBg,
            border: `1px solid ${BB.borderDark}`,
            opacity: isZoomed ? 1 : 0.4,
            cursor: isZoomed ? 'pointer' : 'not-allowed'
          }}
          title="Reset Zoom"
        >
          <RotateCcw size={14} style={{ color: BB.textSecondary }} />
        </button>
        {/* Pan Right */}
        <button
          onClick={() => handlePanRight(chartType)}
          disabled={!canGoRight}
          className="p-1.5 transition-colors hover:opacity-80"
          style={{
            backgroundColor: BB.cardBg,
            border: `1px solid ${BB.borderDark}`,
            opacity: canGoRight ? 1 : 0.4,
            cursor: canGoRight ? 'pointer' : 'not-allowed'
          }}
          title="Pan Right"
        >
          <ChevronRight size={14} style={{ color: canGoRight ? BB.amber : BB.textSecondary }} />
        </button>
      </div>
    );
  };

  const fetchSymbolData = useCallback(async () => {
    if (!symbolInput.trim()) return;

    setPriceDataLoading(true);
    setError(null);

    try {
      const endDate = new Date();
      const startDate = new Date();
      startDate.setDate(startDate.getDate() - historicalDays);

      const data = await yfinanceService.getHistoricalData(
        symbolInput.toUpperCase(),
        startDate.toISOString().split('T')[0],
        endDate.toISOString().split('T')[0]
      );

      if (data && data.length > 0) {
        const closes = data.map(d => d.close);
        const dates = data.map(d => {
          const date = new Date(d.timestamp);
          return date.toLocaleDateString('en-US', { month: 'short', day: 'numeric', year: '2-digit' });
        });
        setPriceData(closes);
        setPriceDates(dates);
        setDataStats(calculateStats(closes));
        setDataChartZoom(null); // Reset zoom when new data loads
      } else {
        setError('No data returned for symbol');
      }
    } catch (err) {
      setError(`Failed to fetch data: ${err}`);
    } finally {
      setPriceDataLoading(false);
    }
  }, [symbolInput, historicalDays, calculateStats]);

  useEffect(() => {
    if (dataSourceType === 'manual' && manualData) {
      const parsed = manualData.split(',').map(v => parseFloat(v.trim())).filter(v => !isNaN(v));
      setPriceData(parsed);
      setDataStats(calculateStats(parsed));
    }
  }, [manualData, dataSourceType, calculateStats]);

  const getDataArray = (): number[] => {
    if (dataSourceType === 'manual') {
      return manualData.split(',').map(v => parseFloat(v.trim())).filter(v => !isNaN(v));
    }
    return priceData;
  };

  const runAnalysis = async () => {
    setIsLoading(true);
    setError(null);
    setAnalysisResult(null);

    try {
      const data = getDataArray();
      const config = analysisConfigs.find(a => a.id === selectedAnalysis);

      if (data.length === 0) {
        setError('No data available');
        setIsLoading(false);
        return;
      }

      if (config && data.length < config.minDataPoints) {
        setError(`Need ${config.minDataPoints} data points (have ${data.length})`);
        setIsLoading(false);
        return;
      }

      const params: Record<string, unknown> = { data };
      const result = await quantAnalyticsService.executeCustomCommand(selectedAnalysis, params);
      setAnalysisResult(result as QuantAnalyticsResult<Record<string, unknown>>);

      if (result.success) {
        setCurrentStep('results');
      } else if (result.error) {
        setError(result.error);
      }
    } catch (err) {
      setError(`Analysis failed: ${err}`);
    } finally {
      setIsLoading(false);
    }
  };

  const exportResults = () => {
    if (!analysisResult) return;
    const content = JSON.stringify(analysisResult, null, 2);
    const blob = new Blob([content], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `${selectedAnalysis}_${symbolInput}_results.json`;
    a.click();
    URL.revokeObjectURL(url);
  };

  const copyToClipboard = (text: string) => {
    navigator.clipboard.writeText(text);
  };

  const formatNumber = (val: number, decimals = 4): string => {
    if (val === null || val === undefined || isNaN(val)) return '—';
    return val.toFixed(decimals);
  };

  const formatPrice = (val: number): string => {
    if (val === null || val === undefined || isNaN(val)) return '—';
    return val.toLocaleString('en-US', { minimumFractionDigits: 2, maximumFractionDigits: 2 });
  };

  // Bloomberg-style header
  const renderHeader = () => (
    <div className="flex items-center justify-between px-4 py-2" style={{ backgroundColor: BB.black, borderBottom: `1px solid ${BB.borderDark}` }}>
      <div className="flex items-center gap-4">
        <div className="flex items-center gap-2">
          <Activity size={18} style={{ color: BB.amber }} />
          <span className="text-sm font-bold tracking-wider" style={{ color: BB.textAmber }}>CFA QUANT ANALYTICS</span>
        </div>
        <div className="h-4 w-px" style={{ backgroundColor: BB.borderLight }} />
        <div className="flex items-center gap-2">
          {(['data', 'analysis', 'results'] as Step[]).map((step, idx) => {
            const isActive = currentStep === step;
            const isPast = ['data', 'analysis', 'results'].indexOf(currentStep) > idx;
            const labels = { data: '1 DATA', analysis: '2 MODEL', results: '3 OUTPUT' };
            return (
              <React.Fragment key={step}>
                <button
                  onClick={() => {
                    if (step === 'results' && !analysisResult) return;
                    setCurrentStep(step);
                  }}
                  className="px-3 py-1 text-xs font-mono transition-colors"
                  style={{
                    backgroundColor: isActive ? BB.amber : 'transparent',
                    color: isActive ? BB.black : isPast ? BB.textAmber : BB.textMuted,
                    cursor: step === 'results' && !analysisResult ? 'not-allowed' : 'pointer',
                  }}
                >
                  {labels[step]}
                </button>
                {idx < 2 && <span style={{ color: BB.textMuted }}>›</span>}
              </React.Fragment>
            );
          })}
        </div>
      </div>
      {dataSourceType === 'symbol' && symbolInput && (
        <div className="flex items-center gap-3">
          <span className="text-sm font-mono font-bold" style={{ color: BB.textPrimary }}>{symbolInput.toUpperCase()}</span>
          {dataStats && (
            <>
              <span className="text-sm font-mono" style={{ color: BB.textSecondary }}>{formatPrice(dataStats.mean)}</span>
              <span className="text-sm font-mono" style={{ color: dataStats.changePercent >= 0 ? BB.green : BB.red }}>
                {dataStats.changePercent >= 0 ? '▲' : '▼'} {Math.abs(dataStats.changePercent).toFixed(2)}%
              </span>
            </>
          )}
        </div>
      )}
    </div>
  );

  // Data Step - Bloomberg style
  const renderDataStep = () => (
    <div className="flex h-full">
      {/* Left Panel - Input Controls */}
      <div className="w-80 flex-shrink-0 p-4 overflow-y-auto" style={{ backgroundColor: BB.panelBg, borderRight: `1px solid ${BB.borderDark}` }}>
        <div className="mb-4">
          <div className="text-xs font-mono mb-2" style={{ color: BB.textAmber }}>DATA SOURCE</div>
          <div className="flex gap-2">
            <button
              onClick={() => setDataSourceType('symbol')}
              className="flex-1 px-3 py-2 text-xs font-mono transition-colors"
              style={{
                backgroundColor: dataSourceType === 'symbol' ? BB.amber : BB.cardBg,
                color: dataSourceType === 'symbol' ? BB.black : BB.textSecondary,
                border: `1px solid ${dataSourceType === 'symbol' ? BB.amber : BB.borderDark}`,
              }}
            >
              <Database size={14} className="inline mr-2" />
              MARKET
            </button>
            <button
              onClick={() => setDataSourceType('manual')}
              className="flex-1 px-3 py-2 text-xs font-mono transition-colors"
              style={{
                backgroundColor: dataSourceType === 'manual' ? BB.amber : BB.cardBg,
                color: dataSourceType === 'manual' ? BB.black : BB.textSecondary,
                border: `1px solid ${dataSourceType === 'manual' ? BB.amber : BB.borderDark}`,
              }}
            >
              <Calculator size={14} className="inline mr-2" />
              MANUAL
            </button>
          </div>
        </div>

        {dataSourceType === 'symbol' ? (
          <div className="space-y-4">
            <div>
              <label className="block text-xs font-mono mb-1" style={{ color: BB.textMuted }}>TICKER</label>
              <input
                type="text"
                value={symbolInput}
                onChange={(e) => setSymbolInput(e.target.value.toUpperCase())}
                placeholder="AAPL"
                className="w-full px-3 py-2 text-sm font-mono"
                style={{
                  backgroundColor: BB.black,
                  color: BB.textPrimary,
                  border: `1px solid ${BB.borderLight}`,
                }}
              />
            </div>
            <div>
              <label className="block text-xs font-mono mb-1" style={{ color: BB.textMuted }}>LOOKBACK (DAYS)</label>
              <input
                type="number"
                value={historicalDays}
                onChange={(e) => setHistoricalDays(parseInt(e.target.value) || 365)}
                className="w-full px-3 py-2 text-sm font-mono"
                style={{
                  backgroundColor: BB.black,
                  color: BB.textPrimary,
                  border: `1px solid ${BB.borderLight}`,
                }}
              />
            </div>
            <button
              onClick={fetchSymbolData}
              disabled={priceDataLoading}
              className="w-full py-2 text-sm font-mono font-bold flex items-center justify-center gap-2"
              style={{
                backgroundColor: BB.amber,
                color: BB.black,
                opacity: priceDataLoading ? 0.7 : 1,
              }}
            >
              {priceDataLoading ? (
                <>
                  <RefreshCw size={14} className="animate-spin" />
                  LOADING...
                </>
              ) : (
                <>
                  <Search size={14} />
                  FETCH DATA
                </>
              )}
            </button>
          </div>
        ) : (
          <div>
            <label className="block text-xs font-mono mb-1" style={{ color: BB.textMuted }}>VALUES (COMMA SEP)</label>
            <textarea
              value={manualData}
              onChange={(e) => setManualData(e.target.value)}
              placeholder="100, 102, 98, 105..."
              rows={8}
              className="w-full px-3 py-2 text-sm font-mono"
              style={{
                backgroundColor: BB.black,
                color: BB.textPrimary,
                border: `1px solid ${BB.borderLight}`,
              }}
            />
          </div>
        )}

        {/* Statistics Panel */}
        {priceData.length > 0 && dataStats && (
          <div className="mt-4 p-3" style={{ backgroundColor: BB.cardBg, border: `1px solid ${BB.borderDark}` }}>
            <div className="text-xs font-mono mb-3" style={{ color: BB.textAmber }}>STATISTICS</div>
            <div className="grid grid-cols-2 gap-2 text-xs font-mono">
              <div>
                <div style={{ color: BB.textMuted }}>COUNT</div>
                <div style={{ color: BB.textPrimary }}>{priceData.length}</div>
              </div>
              <div>
                <div style={{ color: BB.textMuted }}>MEAN</div>
                <div style={{ color: BB.textPrimary }}>{formatPrice(dataStats.mean)}</div>
              </div>
              <div>
                <div style={{ color: BB.textMuted }}>LOW</div>
                <div style={{ color: BB.red }}>{formatPrice(dataStats.min)}</div>
              </div>
              <div>
                <div style={{ color: BB.textMuted }}>HIGH</div>
                <div style={{ color: BB.green }}>{formatPrice(dataStats.max)}</div>
              </div>
              <div>
                <div style={{ color: BB.textMuted }}>STD DEV</div>
                <div style={{ color: BB.textPrimary }}>{formatNumber(dataStats.std, 2)}</div>
              </div>
              <div>
                <div style={{ color: BB.textMuted }}>CHANGE</div>
                <div style={{ color: dataStats.changePercent >= 0 ? BB.green : BB.red }}>
                  {dataStats.changePercent >= 0 ? '+' : ''}{dataStats.changePercent.toFixed(2)}%
                </div>
              </div>
            </div>
          </div>
        )}

        {error && (
          <div className="mt-4 p-3 text-xs font-mono" style={{ backgroundColor: `${BB.red}20`, border: `1px solid ${BB.red}`, color: BB.red }}>
            <AlertCircle size={14} className="inline mr-2" />
            {error}
          </div>
        )}

        <button
          onClick={() => setCurrentStep('analysis')}
          disabled={priceData.length === 0}
          className="w-full mt-4 py-2 text-sm font-mono font-bold flex items-center justify-center gap-2"
          style={{
            backgroundColor: priceData.length > 0 ? BB.green : BB.cardBg,
            color: priceData.length > 0 ? BB.black : BB.textMuted,
            cursor: priceData.length > 0 ? 'pointer' : 'not-allowed',
          }}
        >
          CONTINUE <ChevronRight size={14} />
        </button>
      </div>

      {/* Right Panel - Chart */}
      <div className="flex-1 p-4" style={{ backgroundColor: BB.darkBg }}>
        {priceData.length > 0 ? (
          <div className="h-full flex flex-col">
            <div className="flex items-center justify-between mb-2">
              <div className="flex items-center gap-3">
                <div className="text-xs font-mono" style={{ color: BB.textAmber }}>PRICE CHART</div>
                {dataChartZoom && (
                  <div className="text-xs font-mono px-2 py-0.5" style={{ backgroundColor: BB.amber + '20', color: BB.amber }}>
                    {getDateForIndex(dataChartZoom.startIndex)} - {getDateForIndex(dataChartZoom.endIndex)}
                  </div>
                )}
              </div>
              <div className="flex items-center gap-3">
                <ZoomControls chartType="data" isZoomed={dataChartZoom !== null} />
                <div className="text-xs font-mono" style={{ color: BB.textMuted }}>{priceData.length} POINTS</div>
              </div>
            </div>
            <div
              className="flex-1"
              style={{ minHeight: 300 }}
              onWheel={(e) => handleWheel(e, 'data')}
            >
              <ResponsiveContainer width="100%" height="100%">
                <AreaChart
                  data={zoomedChartData}
                  margin={{ top: 10, right: 10, left: 0, bottom: 30 }}
                  onMouseDown={(e) => handleMouseDown(e, 'data')}
                  onMouseMove={handleMouseMove}
                  onMouseUp={handleMouseUp}
                  onMouseLeave={handleMouseUp}
                >
                  <defs>
                    <linearGradient id="colorValue" x1="0" y1="0" x2="0" y2="1">
                      <stop offset="5%" stopColor={dataStats && dataStats.changePercent >= 0 ? BB.green : BB.red} stopOpacity={0.3}/>
                      <stop offset="95%" stopColor={dataStats && dataStats.changePercent >= 0 ? BB.green : BB.red} stopOpacity={0}/>
                    </linearGradient>
                  </defs>
                  <CartesianGrid strokeDasharray="3 3" stroke={BB.chartGrid} />
                  <XAxis
                    dataKey="date"
                    tick={{ fill: BB.textMuted, fontSize: 9 }}
                    axisLine={{ stroke: BB.borderDark }}
                    tickLine={{ stroke: BB.borderDark }}
                    interval="preserveStartEnd"
                  />
                  <YAxis
                    tick={{ fill: BB.textMuted, fontSize: 10 }}
                    axisLine={{ stroke: BB.borderDark }}
                    tickLine={{ stroke: BB.borderDark }}
                    domain={['auto', 'auto']}
                    tickFormatter={(val) => formatPrice(val)}
                  />
                  <Tooltip
                    contentStyle={{
                      backgroundColor: BB.cardBg,
                      border: `1px solid ${BB.borderLight}`,
                      borderRadius: 0,
                    }}
                    labelFormatter={(label) => `Date: ${label}`}
                    labelStyle={{ color: BB.textAmber, fontSize: 11, fontWeight: 'bold' }}
                    itemStyle={{ color: BB.textPrimary, fontSize: 12 }}
                    formatter={(value: number) => [formatPrice(value), 'Price']}
                  />
                  <ReferenceLine y={dataStats?.mean} stroke={BB.amber} strokeDasharray="5 5" />
                  <Area
                    type="monotone"
                    dataKey="value"
                    stroke={dataStats && dataStats.changePercent >= 0 ? BB.green : BB.red}
                    fillOpacity={1}
                    fill="url(#colorValue)"
                    strokeWidth={1.5}
                  />
                  {/* Brush for horizontal pan/zoom */}
                  {!dataChartZoom && (
                    <Brush
                      dataKey="index"
                      height={20}
                      stroke={BB.amber}
                      fill={BB.cardBg}
                      tickFormatter={(val) => `${val}`}
                      onChange={(e) => {
                        if (e.startIndex !== undefined && e.endIndex !== undefined && e.startIndex !== e.endIndex) {
                          setDataChartZoom({ startIndex: e.startIndex, endIndex: e.endIndex });
                        }
                      }}
                    />
                  )}
                  {/* Selection area indicator */}
                  {isSelecting && activeChart === 'data' && refAreaLeft !== null && refAreaRight !== null && (
                    <ReferenceArea
                      x1={refAreaLeft}
                      x2={refAreaRight}
                      strokeOpacity={0.3}
                      fill={BB.amber}
                      fillOpacity={0.2}
                    />
                  )}
                </AreaChart>
              </ResponsiveContainer>
            </div>
            <div className="text-xs font-mono mt-1 text-center" style={{ color: BB.textMuted }}>
              Scroll to pan | Ctrl+Scroll to zoom | Drag to select range
            </div>
          </div>
        ) : (
          <div className="h-full flex items-center justify-center">
            <div className="text-center">
              <Database size={48} style={{ color: BB.textMuted, margin: '0 auto 16px' }} />
              <div className="text-sm font-mono" style={{ color: BB.textMuted }}>NO DATA LOADED</div>
              <div className="text-xs font-mono mt-1" style={{ color: BB.textMuted }}>Fetch market data or enter values manually</div>
            </div>
          </div>
        )}
      </div>
    </div>
  );

  // Analysis Step - Bloomberg style
  const renderAnalysisStep = () => {
    const selectedConfig = analysisConfigs.find(a => a.id === selectedAnalysis);
    const canRun = priceData.length >= (selectedConfig?.minDataPoints || 0);

    return (
      <div className="flex h-full">
        {/* Left Panel - Analysis Selection */}
        <div className="w-80 flex-shrink-0 p-4 overflow-y-auto" style={{ backgroundColor: BB.panelBg, borderRight: `1px solid ${BB.borderDark}` }}>
          <div className="text-xs font-mono mb-3" style={{ color: BB.textAmber }}>SELECT MODEL</div>

          <div className="space-y-1">
            {analysisConfigs.map((config) => {
              const isSelected = selectedAnalysis === config.id;
              const hasEnoughData = priceData.length >= config.minDataPoints;
              const Icon = config.icon;

              return (
                <button
                  key={config.id}
                  onClick={() => hasEnoughData && setSelectedAnalysis(config.id)}
                  className="w-full px-3 py-2 text-left flex items-center gap-3 transition-colors"
                  style={{
                    backgroundColor: isSelected ? BB.cardBg : 'transparent',
                    borderLeft: `3px solid ${isSelected ? config.color : 'transparent'}`,
                    opacity: hasEnoughData ? 1 : 0.4,
                    cursor: hasEnoughData ? 'pointer' : 'not-allowed',
                  }}
                >
                  <Icon size={16} style={{ color: config.color }} />
                  <div className="flex-1 min-w-0">
                    <div className="text-xs font-mono font-bold" style={{ color: BB.textPrimary }}>{config.shortName}</div>
                    <div className="text-xs font-mono truncate" style={{ color: BB.textMuted }}>{config.name}</div>
                  </div>
                  {!hasEnoughData && (
                    <span className="text-xs font-mono" style={{ color: BB.red }}>
                      {config.minDataPoints}+
                    </span>
                  )}
                </button>
              );
            })}
          </div>

          <div className="mt-4 flex gap-2">
            <button
              onClick={() => setCurrentStep('data')}
              className="flex-1 py-2 text-xs font-mono"
              style={{ backgroundColor: BB.cardBg, color: BB.textSecondary, border: `1px solid ${BB.borderDark}` }}
            >
              <ChevronLeft size={12} className="inline" /> BACK
            </button>
            <button
              onClick={runAnalysis}
              disabled={isLoading || !canRun}
              className="flex-1 py-2 text-xs font-mono font-bold"
              style={{
                backgroundColor: canRun ? BB.green : BB.cardBg,
                color: canRun ? BB.black : BB.textMuted,
                cursor: canRun ? 'pointer' : 'not-allowed',
              }}
            >
              {isLoading ? 'RUNNING...' : 'RUN'} <Zap size={12} className="inline ml-1" />
            </button>
          </div>

          {error && (
            <div className="mt-4 p-3 text-xs font-mono" style={{ backgroundColor: `${BB.red}20`, border: `1px solid ${BB.red}`, color: BB.red }}>
              {error}
            </div>
          )}
        </div>

        {/* Right Panel - Model Details */}
        <div className="flex-1 p-4" style={{ backgroundColor: BB.darkBg }}>
          {selectedConfig && (
            <div className="h-full flex flex-col">
              <div className="mb-4 p-4" style={{ backgroundColor: BB.cardBg, border: `1px solid ${BB.borderDark}` }}>
                <div className="flex items-center gap-3 mb-3">
                  <div className="p-2" style={{ backgroundColor: `${selectedConfig.color}20` }}>
                    <selectedConfig.icon size={24} style={{ color: selectedConfig.color }} />
                  </div>
                  <div>
                    <div className="text-sm font-mono font-bold" style={{ color: BB.textPrimary }}>{selectedConfig.name}</div>
                    <div className="text-xs font-mono" style={{ color: BB.textMuted }}>{selectedConfig.category.replace('_', ' ').toUpperCase()}</div>
                  </div>
                </div>
                <div className="text-xs font-mono" style={{ color: BB.textSecondary }}>{selectedConfig.helpText}</div>

                <div className="mt-3 flex items-center gap-4 text-xs font-mono">
                  <div>
                    <span style={{ color: BB.textMuted }}>MIN POINTS: </span>
                    <span style={{ color: priceData.length >= selectedConfig.minDataPoints ? BB.green : BB.red }}>
                      {selectedConfig.minDataPoints}
                    </span>
                  </div>
                  <div>
                    <span style={{ color: BB.textMuted }}>YOUR DATA: </span>
                    <span style={{ color: BB.textPrimary }}>{priceData.length}</span>
                  </div>
                </div>
              </div>

              {/* Mini Chart Preview */}
              <div className="flex-1" style={{ minHeight: 200 }}>
                <div className="flex items-center justify-between mb-2">
                  <div className="text-xs font-mono" style={{ color: BB.textAmber }}>DATA PREVIEW</div>
                  {dataChartZoom && (
                    <button
                      onClick={() => setDataChartZoom(null)}
                      className="text-xs font-mono px-2 py-0.5 flex items-center gap-1"
                      style={{ backgroundColor: BB.cardBg, color: BB.textMuted, border: `1px solid ${BB.borderDark}` }}
                    >
                      <RotateCcw size={10} /> RESET ZOOM
                    </button>
                  )}
                </div>
                <ResponsiveContainer width="100%" height="100%">
                  <RechartsLineChart data={zoomedChartData} margin={{ top: 5, right: 5, left: 0, bottom: 5 }}>
                    <CartesianGrid strokeDasharray="3 3" stroke={BB.chartGrid} />
                    <XAxis dataKey="index" hide />
                    <YAxis hide domain={['auto', 'auto']} />
                    <Line
                      type="monotone"
                      dataKey="value"
                      stroke={selectedConfig.color}
                      dot={false}
                      strokeWidth={1.5}
                    />
                  </RechartsLineChart>
                </ResponsiveContainer>
              </div>
            </div>
          )}
        </div>
      </div>
    );
  };

  // Results Step - Bloomberg style
  const renderResultsStep = () => {
    if (!analysisResult?.result) return null;

    const config = analysisConfigs.find(a => a.id === selectedAnalysis);
    const result = analysisResult.result as Record<string, unknown>;

    // Extract key metrics based on analysis type
    const getKeyMetrics = () => {
      const metrics: { label: string; value: string; color: string; icon?: React.ElementType }[] = [];

      if (selectedAnalysis === 'trend_analysis' && result.metadata) {
        const meta = result.metadata as Record<string, unknown>;
        const value = result.value as Record<string, unknown>;
        const slope = value?.slope as number;
        const intercept = value?.intercept as number;
        const rSquared = meta?.r_squared as number;
        const significant = meta?.trend_significant as boolean;
        const direction = meta?.trend_direction as string;
        const pValue = meta?.slope_p_value as number;
        const tStat = meta?.slope_t_statistic as number;

        // Main result - direction with icon
        metrics.push({
          label: 'DIRECTION',
          value: direction === 'upward' ? 'UP' : direction === 'downward' ? 'DOWN' : 'FLAT',
          color: direction === 'upward' ? BB.green : direction === 'downward' ? BB.red : BB.textMuted,
          icon: direction === 'upward' ? ArrowUpRight : direction === 'downward' ? ArrowDownRight : Minus,
        });

        // Statistical significance
        metrics.push({
          label: 'SIGNIFICANT',
          value: significant ? 'YES' : 'NO',
          color: significant ? BB.green : BB.textMuted,
          icon: significant ? CheckCircle2 : undefined
        });

        // Model fit
        metrics.push({
          label: 'R-SQUARED',
          value: `${(rSquared * 100).toFixed(1)}%`,
          color: rSquared > 0.7 ? BB.green : rSquared > 0.4 ? BB.amber : BB.textMuted
        });

        // P-value with color coding
        if (pValue !== undefined && pValue !== null) {
          metrics.push({
            label: 'P-VALUE',
            value: pValue < 0.001 ? '<0.001' : formatNumber(pValue, 4),
            color: pValue < 0.01 ? BB.green : pValue < 0.05 ? BB.greenDim : pValue < 0.1 ? BB.amber : BB.red
          });
        }

        // T-statistic
        if (tStat !== undefined && tStat !== null) {
          metrics.push({
            label: 'T-STATISTIC',
            value: formatNumber(tStat, 2),
            color: Math.abs(tStat) > 2 ? BB.green : BB.textMuted
          });
        }

        // Regression parameters
        metrics.push({ label: 'SLOPE', value: formatNumber(slope, 6), color: BB.textPrimary });
        if (intercept !== undefined && intercept !== null) {
          metrics.push({ label: 'INTERCEPT', value: formatNumber(intercept, 2), color: BB.textSecondary });
        }
      } else if (selectedAnalysis === 'stationarity_test' && result.value) {
        const value = result.value as Record<string, unknown>;
        const meta = result.metadata as Record<string, unknown>;
        const isStationary = value?.is_stationary as boolean;
        const pValue = value?.p_value as number;
        const testStat = value?.test_statistic as number;
        const criticalValues = value?.critical_values as Record<string, number>;
        const nullHypothesis = value?.null_hypothesis as string;

        // Main result
        metrics.push({
          label: 'STATIONARY',
          value: isStationary ? 'YES' : 'NO',
          color: isStationary ? BB.green : BB.red,
          icon: isStationary ? CheckCircle2 : AlertCircle
        });

        // Test statistics
        metrics.push({ label: 'TEST STAT', value: formatNumber(testStat, 4), color: BB.textPrimary });
        metrics.push({
          label: 'P-VALUE',
          value: formatNumber(pValue, 4),
          color: pValue < 0.01 ? BB.green : pValue < 0.05 ? BB.greenDim : pValue < 0.1 ? BB.amber : BB.red
        });

        // Critical values comparison
        if (criticalValues) {
          const cv1 = criticalValues['1%'];
          const cv5 = criticalValues['5%'];
          const cv10 = criticalValues['10%'];

          if (cv1 !== undefined) {
            const beats1 = testStat < cv1;
            metrics.push({ label: 'CV 1%', value: `${formatNumber(cv1, 2)} ${beats1 ? 'PASS' : ''}`, color: beats1 ? BB.green : BB.textMuted });
          }
          if (cv5 !== undefined) {
            const beats5 = testStat < cv5;
            metrics.push({ label: 'CV 5%', value: `${formatNumber(cv5, 2)} ${beats5 ? 'PASS' : ''}`, color: beats5 ? BB.green : BB.textMuted });
          }
          if (cv10 !== undefined) {
            const beats10 = testStat < cv10;
            metrics.push({ label: 'CV 10%', value: `${formatNumber(cv10, 2)} ${beats10 ? 'PASS' : ''}`, color: beats10 ? BB.amber : BB.textMuted });
          }
        }

        // Metadata - stability measures
        if (meta) {
          const rollingMeanStability = meta.rolling_mean_stability as number;
          const rollingStdStability = meta.rolling_std_stability as number;
          const meanReversionStrength = meta.mean_reversion_strength as number;

          if (rollingMeanStability !== undefined) {
            metrics.push({
              label: 'MEAN STABILITY',
              value: formatNumber(rollingMeanStability, 2),
              color: rollingMeanStability < 1 ? BB.green : rollingMeanStability < 5 ? BB.amber : BB.red
            });
          }
          if (rollingStdStability !== undefined) {
            metrics.push({
              label: 'VOL STABILITY',
              value: formatNumber(rollingStdStability, 2),
              color: rollingStdStability < 0.5 ? BB.green : rollingStdStability < 2 ? BB.amber : BB.red
            });
          }
          if (meanReversionStrength !== undefined) {
            metrics.push({
              label: 'MEAN REVERSION',
              value: `${(meanReversionStrength * 100).toFixed(1)}%`,
              color: meanReversionStrength > 0.3 ? BB.green : meanReversionStrength > 0.1 ? BB.amber : BB.textMuted
            });
          }
        }
      } else if (selectedAnalysis === 'arima_model' && result.metadata) {
        const meta = result.metadata as Record<string, unknown>;
        const params = result.parameters as Record<string, unknown>;
        const value = result.value as Record<string, unknown>;
        const residuals = value?.residuals as number[];

        // Model order
        const order = params?.order as number[];
        if (order && order.length === 3) {
          metrics.push({
            label: 'MODEL ORDER',
            value: `(${order[0]},${order[1]},${order[2]})`,
            color: BB.amber
          });
        }

        // Seasonal order if present
        const seasonalOrder = params?.seasonal_order as number[];
        if (seasonalOrder && seasonalOrder.length === 4) {
          metrics.push({
            label: 'SEASONAL',
            value: `(${seasonalOrder[0]},${seasonalOrder[1]},${seasonalOrder[2]},${seasonalOrder[3]})`,
            color: BB.blueLight
          });
        }

        // Information criteria (lower is better)
        const aic = meta?.aic as number;
        const bic = meta?.bic as number;
        metrics.push({ label: 'AIC', value: formatNumber(aic, 2), color: BB.textPrimary });
        metrics.push({ label: 'BIC', value: formatNumber(bic, 2), color: BB.textPrimary });

        // Log-likelihood
        const logLik = meta?.log_likelihood as number;
        if (logLik !== undefined && logLik !== null) {
          metrics.push({ label: 'LOG-LIKELIHOOD', value: formatNumber(logLik, 2), color: BB.textSecondary });
        }

        // Ljung-Box test for residual autocorrelation
        const ljungBoxPValue = meta?.ljung_box_p_value as number;
        const residualsAutocorrelated = meta?.residuals_autocorrelated as boolean;

        metrics.push({
          label: 'RESIDUALS OK',
          value: !residualsAutocorrelated ? 'YES' : 'NO',
          color: !residualsAutocorrelated ? BB.green : BB.red,
          icon: !residualsAutocorrelated ? CheckCircle2 : AlertCircle
        });

        if (ljungBoxPValue !== undefined && ljungBoxPValue !== null) {
          metrics.push({
            label: 'LJUNG-BOX P',
            value: formatNumber(ljungBoxPValue, 4),
            color: ljungBoxPValue > 0.05 ? BB.green : ljungBoxPValue > 0.01 ? BB.amber : BB.red
          });
        }

        // Residual statistics
        if (residuals && residuals.length > 0) {
          const residMean = residuals.reduce((a, b) => a + b, 0) / residuals.length;
          const residStd = Math.sqrt(residuals.reduce((a, b) => a + Math.pow(b - residMean, 2), 0) / residuals.length);
          metrics.push({ label: 'RESID MEAN', value: formatNumber(residMean, 4), color: Math.abs(residMean) < 0.01 ? BB.green : BB.textMuted });
          metrics.push({ label: 'RESID STD', value: formatNumber(residStd, 4), color: BB.textSecondary });
        }
      } else if (selectedAnalysis === 'forecasting' && result.metadata) {
        const meta = result.metadata as Record<string, unknown>;
        const params = result.parameters as Record<string, unknown>;
        const forecasts = result.value as number[];
        const trainSize = meta?.train_data_size as number;
        const testSize = meta?.test_data_size as number;
        const horizon = meta?.forecast_horizon as number;
        const method = params?.method as string;
        const mae = meta?.mae as number;
        const rmse = meta?.rmse as number;
        const mape = meta?.mape as number;

        // Method with icon
        if (method) {
          const methodLabels: Record<string, string> = {
            'simple_exponential': 'EXP SMOOTH',
            'linear_trend': 'LINEAR TREND',
            'moving_average': 'MOVING AVG'
          };
          const methodDescriptions: Record<string, string> = {
            'simple_exponential': 'Level',
            'linear_trend': 'Trend',
            'moving_average': 'Average'
          };
          metrics.push({
            label: 'METHOD',
            value: methodLabels[method] || method.toUpperCase(),
            color: BB.amber,
            icon: method === 'linear_trend' ? TrendingUp : method === 'simple_exponential' ? Sparkles : Activity
          });
          metrics.push({ label: 'MODEL TYPE', value: methodDescriptions[method] || 'Custom', color: BB.textSecondary });
        }

        // Forecast accuracy rating based on MAPE
        if (mape !== undefined && mape !== null) {
          let accuracy = 'EXCELLENT';
          let accuracyColor = BB.green;
          if (mape > 30) { accuracy = 'POOR'; accuracyColor = BB.red; }
          else if (mape > 20) { accuracy = 'FAIR'; accuracyColor = BB.amber; }
          else if (mape > 10) { accuracy = 'GOOD'; accuracyColor = BB.greenDim; }
          metrics.push({ label: 'ACCURACY', value: accuracy, color: accuracyColor, icon: mape <= 10 ? CheckCircle2 : mape <= 20 ? AlertCircle : undefined });
        }

        // Forecast value info
        if (forecasts && forecasts.length > 0) {
          const forecastValue = forecasts[0];
          metrics.push({ label: 'FORECAST VALUE', value: formatPrice(forecastValue), color: BB.green });

          // If we have test data, show comparison
          if (testSize > 0 && priceData.length > 0) {
            const testStartIdx = trainSize;
            const lastActual = priceData[priceData.length - 1];
            const diff = forecastValue - lastActual;
            const diffPct = (diff / lastActual) * 100;
            metrics.push({
              label: 'VS LAST ACTUAL',
              value: `${diffPct >= 0 ? '+' : ''}${diffPct.toFixed(2)}%`,
              color: diffPct >= 0 ? BB.green : BB.red,
              icon: diffPct >= 0 ? ArrowUpRight : ArrowDownRight
            });
          }
        }

        // Data split info with visual representation
        if (trainSize && testSize) {
          const trainPct = ((trainSize / (trainSize + testSize)) * 100).toFixed(0);
          metrics.push({ label: 'TRAIN/TEST', value: `${trainPct}/${100 - parseInt(trainPct)}%`, color: BB.blueLight });
        }
        if (trainSize) metrics.push({ label: 'TRAIN SIZE', value: String(trainSize), color: BB.textSecondary });
        if (testSize) metrics.push({ label: 'TEST SIZE', value: String(testSize), color: BB.textSecondary });
        if (horizon) metrics.push({ label: 'HORIZON', value: `${horizon} periods`, color: BB.green });

        // Accuracy metrics with color coding
        if (mae !== undefined && mae !== null) {
          metrics.push({ label: 'MAE', value: formatNumber(mae, 2), color: BB.textPrimary });
        }
        if (rmse !== undefined && rmse !== null) {
          metrics.push({ label: 'RMSE', value: formatNumber(rmse, 2), color: BB.textPrimary });
          // RMSE to MAE ratio (closer to 1 means less outliers)
          if (mae !== undefined && mae !== null && mae > 0) {
            const rmseToMae = rmse / mae;
            metrics.push({
              label: 'RMSE/MAE',
              value: formatNumber(rmseToMae, 2),
              color: rmseToMae < 1.2 ? BB.green : rmseToMae < 1.5 ? BB.amber : BB.red
            });
          }
        }
        if (mape !== undefined && mape !== null) {
          metrics.push({
            label: 'MAPE',
            value: `${formatNumber(mape, 2)}%`,
            color: mape < 10 ? BB.green : mape < 20 ? BB.amber : BB.red
          });
        }
      } else if (selectedAnalysis === 'supervised_learning' && result.value) {
        const meta = result.metadata as Record<string, unknown>;
        const value = result.value as Record<string, Record<string, unknown>>;
        const bestAlgo = meta?.best_algorithm as string;
        const nFeatures = meta?.n_features as number;
        const yTest = meta?.y_test as number[];

        // Best model highlight
        if (bestAlgo) {
          metrics.push({ label: 'BEST MODEL', value: bestAlgo.toUpperCase().replace('_', ' '), color: BB.green, icon: CheckCircle2 });
        }

        // Best model detailed metrics
        if (bestAlgo && value[bestAlgo]) {
          const bestResult = value[bestAlgo];
          if (bestResult.r2_score !== undefined) {
            const r2 = bestResult.r2_score as number;
            const r2Quality = r2 > 0.8 ? 'Excellent' : r2 > 0.6 ? 'Good' : r2 > 0.4 ? 'Fair' : 'Poor';
            metrics.push({ label: 'R² SCORE', value: `${(r2 * 100).toFixed(1)}%`, color: r2 > 0.7 ? BB.green : r2 > 0.5 ? BB.amber : BB.red });
            metrics.push({ label: 'FIT QUALITY', value: r2Quality.toUpperCase(), color: r2 > 0.7 ? BB.green : r2 > 0.5 ? BB.amber : BB.red });
          }
          if (bestResult.rmse !== undefined) {
            metrics.push({ label: 'RMSE', value: formatNumber(bestResult.rmse as number, 4), color: BB.textPrimary });
          }
          if (bestResult.mse !== undefined) {
            metrics.push({ label: 'MSE', value: formatNumber(bestResult.mse as number, 4), color: BB.textSecondary });
          }
        }

        // Compare all algorithms
        const algoNames = Object.keys(value).filter(k => !value[k].error);
        if (algoNames.length > 1) {
          metrics.push({ label: 'MODELS TESTED', value: String(algoNames.length), color: BB.blueLight });

          // Show comparison of R² scores
          algoNames.forEach(algo => {
            if (algo !== bestAlgo && value[algo]?.r2_score !== undefined) {
              const r2 = value[algo].r2_score as number;
              metrics.push({
                label: algo.toUpperCase().replace('_', ' ').substring(0, 12),
                value: `R²: ${(r2 * 100).toFixed(1)}%`,
                color: BB.textMuted
              });
            }
          });
        }

        // Data split info
        if (meta?.train_size) {
          metrics.push({ label: 'TRAIN SIZE', value: String(meta.train_size), color: BB.textMuted });
        }
        if (meta?.test_size) {
          metrics.push({ label: 'TEST SIZE', value: String(meta.test_size), color: BB.textMuted });
        }
        if (nFeatures) {
          metrics.push({ label: 'FEATURES', value: String(nFeatures), color: BB.textMuted });
        }

        // Calculate prediction error statistics if y_test is available
        if (yTest && bestAlgo && value[bestAlgo]?.predictions) {
          const predictions = value[bestAlgo].predictions as number[];
          if (predictions.length === yTest.length) {
            const errors = yTest.map((actual, i) => actual - predictions[i]);
            const meanError = errors.reduce((a, b) => a + b, 0) / errors.length;
            const maxAbsError = Math.max(...errors.map(Math.abs));

            metrics.push({ label: 'MEAN ERROR', value: formatNumber(meanError, 4), color: Math.abs(meanError) < 0.01 ? BB.green : BB.textMuted });
            metrics.push({ label: 'MAX |ERROR|', value: formatNumber(maxAbsError, 4), color: BB.textSecondary });
          }
        }
      } else if (selectedAnalysis === 'unsupervised_learning' && result.value) {
        const value = result.value as Record<string, Record<string, unknown>>;
        const meta = result.metadata as Record<string, unknown>;

        if (value.pca) {
          const pca = value.pca;
          const explainedVariance = pca.explained_variance_ratio as number[];
          const componentsFor95 = pca.components_for_95_variance as number;

          if (explainedVariance && explainedVariance.length > 0) {
            const firstComponentVar = (explainedVariance[0] * 100).toFixed(1);
            const totalComponents = explainedVariance.length;
            metrics.push({ label: 'PC1 VARIANCE', value: `${firstComponentVar}%`, color: BB.green });
            metrics.push({ label: 'COMPONENTS (95%)', value: String(componentsFor95), color: BB.amber });
            metrics.push({ label: 'TOTAL PCs', value: String(totalComponents), color: BB.textSecondary });
          }
        }

        if (value.kmeans) {
          const kmeans = value.kmeans;
          const optimalK = kmeans.optimal_k as number;
          const silhouetteByK = kmeans.silhouette_by_k as [number, number][];

          metrics.push({ label: 'OPTIMAL K', value: String(optimalK), color: BB.green });

          if (silhouetteByK && silhouetteByK.length > 0) {
            const optimalSilhouette = silhouetteByK.find(([k]) => k === optimalK);
            if (optimalSilhouette) {
              const score = optimalSilhouette[1];
              const quality = score > 0.7 ? 'STRONG' : score > 0.5 ? 'GOOD' : score > 0.25 ? 'FAIR' : 'WEAK';
              metrics.push({ label: 'SILHOUETTE', value: formatNumber(score, 3), color: score > 0.5 ? BB.green : BB.amber });
              metrics.push({ label: 'CLUSTERING', value: quality, color: score > 0.5 ? BB.green : BB.amber });
            }
          }

          const clusterLabels = kmeans.cluster_labels as number[];
          if (clusterLabels) {
            metrics.push({ label: 'DATA POINTS', value: String(clusterLabels.length), color: BB.textSecondary });
          }
        }

        if (meta?.n_samples) {
          metrics.push({ label: 'SAMPLES', value: String(meta.n_samples), color: BB.textMuted });
        }
        if (meta?.n_features) {
          metrics.push({ label: 'FEATURES', value: String(meta.n_features), color: BB.textMuted });
        }
      } else if (selectedAnalysis === 'resampling_methods' && result.value) {
        const value = result.value as Record<string, Record<string, unknown>>;
        const meta = result.metadata as Record<string, unknown>;

        // Bootstrap metrics
        if (value.bootstrap) {
          const bootstrap = value.bootstrap;
          const mean = bootstrap.mean as number;
          const std = bootstrap.std as number;
          const bias = bootstrap.bias as number;
          const ci = bootstrap.confidence_interval as [number, number];

          metrics.push({ label: 'BOOT MEAN', value: formatNumber(mean, 4), color: BB.green });
          metrics.push({ label: 'BOOT STD', value: formatNumber(std, 4), color: BB.textPrimary });
          if (bias !== undefined && bias !== null) {
            metrics.push({ label: 'BIAS', value: formatNumber(bias, 6), color: Math.abs(bias) < 0.01 ? BB.green : BB.amber });
          }
          if (ci && ci.length === 2) {
            metrics.push({ label: '95% CI LOW', value: formatNumber(ci[0], 4), color: BB.textSecondary });
            metrics.push({ label: '95% CI HIGH', value: formatNumber(ci[1], 4), color: BB.textSecondary });
          }
        }

        // Jackknife metrics
        if (value.jackknife) {
          const jackknife = value.jackknife;
          const biasCorr = jackknife.bias_corrected_estimate as number;
          const stdErr = jackknife.std_error as number;
          const jackBias = jackknife.bias as number;

          if (biasCorr !== undefined) {
            metrics.push({ label: 'JACK ESTIMATE', value: formatNumber(biasCorr, 4), color: BB.amber });
          }
          if (stdErr !== undefined) {
            metrics.push({ label: 'STD ERROR', value: formatNumber(stdErr, 4), color: BB.textPrimary });
          }
          if (jackBias !== undefined) {
            metrics.push({ label: 'JACK BIAS', value: formatNumber(jackBias, 6), color: Math.abs(jackBias) < 0.01 ? BB.green : BB.amber });
          }
        }

        // Permutation test metrics
        if (value.permutation) {
          const perm = value.permutation;
          const pValue = perm.p_value as number;
          const significant = perm.significant as boolean;

          metrics.push({ label: 'P-VALUE', value: formatNumber(pValue, 4), color: pValue < 0.05 ? BB.green : BB.textMuted });
          metrics.push({ label: 'SIGNIFICANT', value: significant ? 'YES' : 'NO', color: significant ? BB.green : BB.textMuted });
        }

        // Metadata
        if (meta?.n_bootstrap) {
          metrics.push({ label: 'ITERATIONS', value: String(meta.n_bootstrap), color: BB.textMuted });
        }
        if (meta?.data_size) {
          metrics.push({ label: 'DATA SIZE', value: String(meta.data_size), color: BB.textMuted });
        }
      } else if (selectedAnalysis === 'validate_data' && result) {
        // Data Quality metrics
        const qualityScore = result.quality_score as number;
        const issues = result.issues as { type: string; severity: string; description: string }[];
        const warnings = result.warnings as { type: string; message: string }[];
        const statistics = result.statistics as Record<string, unknown>;
        const recommendations = result.recommendations as string[];

        // Quality Score with color coding
        if (qualityScore !== undefined && qualityScore !== null) {
          let scoreColor = BB.green;
          let scoreLabel = 'EXCELLENT';
          if (qualityScore < 50) { scoreColor = BB.red; scoreLabel = 'POOR'; }
          else if (qualityScore < 70) { scoreColor = BB.amber; scoreLabel = 'FAIR'; }
          else if (qualityScore < 90) { scoreColor = BB.green; scoreLabel = 'GOOD'; }
          metrics.push({ label: 'QUALITY SCORE', value: `${qualityScore.toFixed(0)}/100`, color: scoreColor });
          metrics.push({ label: 'STATUS', value: scoreLabel, color: scoreColor });
        }

        // Statistics
        if (statistics) {
          if (statistics.total_observations !== undefined) {
            metrics.push({ label: 'OBSERVATIONS', value: String(statistics.total_observations), color: BB.textPrimary });
          }
          if (statistics.missing_data_ratio !== undefined) {
            const missingRatio = statistics.missing_data_ratio as number;
            const missingPct = (missingRatio * 100).toFixed(1);
            metrics.push({
              label: 'MISSING DATA',
              value: `${missingPct}%`,
              color: missingRatio > 0.05 ? BB.red : missingRatio > 0.01 ? BB.amber : BB.green
            });
          }
          if (statistics.mean !== undefined) {
            metrics.push({ label: 'MEAN', value: formatNumber(statistics.mean as number, 2), color: BB.textSecondary });
          }
          if (statistics.std !== undefined) {
            metrics.push({ label: 'STD DEV', value: formatNumber(statistics.std as number, 2), color: BB.textSecondary });
          }
        }

        // Issues count by severity
        if (issues && issues.length > 0) {
          const criticalCount = issues.filter(i => i.severity === 'critical').length;
          const highCount = issues.filter(i => i.severity === 'high').length;
          const mediumCount = issues.filter(i => i.severity === 'medium').length;
          const totalIssues = issues.length;

          metrics.push({ label: 'TOTAL ISSUES', value: String(totalIssues), color: totalIssues > 0 ? BB.amber : BB.green });
          if (criticalCount > 0) {
            metrics.push({ label: 'CRITICAL', value: String(criticalCount), color: BB.red });
          }
          if (highCount > 0) {
            metrics.push({ label: 'HIGH SEV', value: String(highCount), color: BB.red });
          }
        } else {
          metrics.push({ label: 'ISSUES', value: 'NONE', color: BB.green });
        }

        // Warnings count
        if (warnings && warnings.length > 0) {
          metrics.push({ label: 'WARNINGS', value: String(warnings.length), color: BB.amber });
        }

        // Recommendations count
        if (recommendations && recommendations.length > 0) {
          metrics.push({ label: 'RECOMMENDATIONS', value: String(recommendations.length), color: BB.blueLight });
        }
      }

      return metrics;
    };

    // Build chart data for results
    const getResultChartData = () => {
      if (selectedAnalysis === 'trend_analysis') {
        const meta = result.metadata as Record<string, unknown>;
        const value = result.value as Record<string, unknown>;
        const fitted = meta?.fitted_values as number[];
        const residuals = meta?.residuals as number[];
        const slope = value?.slope as number;
        const intercept = value?.intercept as number;

        if (fitted && fitted.length > 0) {
          // Calculate standard error for confidence bands
          const n = priceData.length;
          const residualsArr = residuals || priceData.map((v, i) => v - fitted[i]);
          const mse = residualsArr.reduce((sum, r) => sum + r * r, 0) / (n - 2);
          const se = Math.sqrt(mse);

          // Mean of time index for confidence interval calculation
          const meanX = (n - 1) / 2;
          const sumXX = Array.from({ length: n }, (_, i) => Math.pow(i - meanX, 2)).reduce((a, b) => a + b, 0);

          // Build enhanced chart data with confidence bands
          const enhancedData = chartData.map((d, i) => {
            // Standard error of prediction at each point
            const sePred = se * Math.sqrt(1 + 1/n + Math.pow(i - meanX, 2) / sumXX);
            const ciWidth = 1.96 * sePred; // 95% confidence

            return {
              ...d,
              fitted: fitted[i],
              residual: residualsArr[i],
              upperBand: fitted[i] + ciWidth,
              lowerBand: fitted[i] - ciWidth,
            };
          });

          // Add future projection (5 periods)
          if (slope !== undefined && intercept !== undefined) {
            const projectionPeriods = 5;
            for (let i = 0; i < projectionPeriods; i++) {
              const futureIndex = n + i;
              const projectedValue = intercept + slope * futureIndex;
              const sePred = se * Math.sqrt(1 + 1/n + Math.pow(futureIndex - meanX, 2) / sumXX);
              const ciWidth = 1.96 * sePred;

              enhancedData.push({
                index: futureIndex,
                value: null as unknown as number,
                date: `+${i + 1}d`,
                fitted: null as unknown as number,
                residual: null as unknown as number,
                upperBand: null as unknown as number,
                lowerBand: null as unknown as number,
                projection: projectedValue,
                projectionUpper: projectedValue + ciWidth,
                projectionLower: projectedValue - ciWidth,
              } as any);
            }
          }

          return enhancedData;
        }
      } else if (selectedAnalysis === 'stationarity_test') {
        // Create rolling mean and rolling std visualization
        const windowSize = Math.min(20, Math.floor(priceData.length / 4));
        if (windowSize < 2) return chartData;

        // Calculate rolling statistics
        const rollingMean: (number | null)[] = [];
        const rollingStd: (number | null)[] = [];
        const upperBand: (number | null)[] = [];
        const lowerBand: (number | null)[] = [];

        for (let i = 0; i < priceData.length; i++) {
          if (i < windowSize - 1) {
            rollingMean.push(null);
            rollingStd.push(null);
            upperBand.push(null);
            lowerBand.push(null);
          } else {
            const windowData = priceData.slice(i - windowSize + 1, i + 1);
            const mean = windowData.reduce((a, b) => a + b, 0) / windowData.length;
            const variance = windowData.reduce((a, b) => a + Math.pow(b - mean, 2), 0) / windowData.length;
            const std = Math.sqrt(variance);

            rollingMean.push(mean);
            rollingStd.push(std);
            upperBand.push(mean + 2 * std);
            lowerBand.push(mean - 2 * std);
          }
        }

        // Overall mean for reference
        const overallMean = priceData.reduce((a, b) => a + b, 0) / priceData.length;

        return chartData.map((d, i) => ({
          ...d,
          rollingMean: rollingMean[i],
          upperBand: upperBand[i],
          lowerBand: lowerBand[i],
          overallMean,
        }));
      } else if (selectedAnalysis === 'arima_model') {
        const value = result.value as Record<string, unknown>;
        const meta = result.metadata as Record<string, unknown>;
        const fitted = value?.fitted_values as number[];
        const residuals = value?.residuals as number[];
        const forecasts = value?.forecasts as number[];
        const forecastCI = value?.forecast_confidence_interval as { lower: number[]; upper: number[] };

        if (fitted && fitted.length > 0) {
          const n = priceData.length;

          // Calculate residual standard deviation for confidence bands
          const residualsArr = residuals || priceData.map((v, i) => v - (fitted[i] || v));
          const residMean = residualsArr.reduce((a, b) => a + b, 0) / residualsArr.length;
          const residStd = Math.sqrt(residualsArr.reduce((a, b) => a + Math.pow(b - residMean, 2), 0) / residualsArr.length);

          // Build enhanced chart data with fitted values, residuals, and confidence bands
          const enhancedData = chartData.map((d, i) => {
            const fittedVal = fitted[i];
            const residual = residualsArr[i];
            // 95% confidence band around fitted values
            const ciWidth = 1.96 * residStd;

            return {
              ...d,
              fitted: fittedVal,
              residual: residual,
              upperBand: fittedVal !== undefined ? fittedVal + ciWidth : null,
              lowerBand: fittedVal !== undefined ? fittedVal - ciWidth : null,
              // Residual bar height (scaled for visibility)
              residualBar: residual,
            };
          });

          // Add future forecast points if available
          if (forecasts && forecasts.length > 0) {
            forecasts.forEach((forecast, i) => {
              const ciLower = forecastCI?.lower?.[i];
              const ciUpper = forecastCI?.upper?.[i];
              // Default CI if not provided (widening uncertainty)
              const defaultCiWidth = 1.96 * residStd * Math.sqrt(1 + (i + 1) * 0.1);

              enhancedData.push({
                index: n + i,
                value: null as unknown as number,
                date: `+${i + 1}d`,
                fitted: null as unknown as number,
                residual: null as unknown as number,
                upperBand: null as unknown as number,
                lowerBand: null as unknown as number,
                residualBar: null as unknown as number,
                forecast: forecast,
                forecastUpper: ciUpper ?? (forecast + defaultCiWidth),
                forecastLower: ciLower ?? (forecast - defaultCiWidth),
              } as any);
            });
          } else {
            // Generate simple forecast projection if not provided by backend
            // Use last fitted value as base and project forward
            const lastFitted = fitted[fitted.length - 1];
            const forecastHorizon = 5;

            for (let i = 0; i < forecastHorizon; i++) {
              // Simple projection using last fitted value (ARIMA(0,1,0) like behavior)
              const forecastVal = lastFitted;
              const ciWidth = 1.96 * residStd * Math.sqrt(1 + (i + 1) * 0.2);

              enhancedData.push({
                index: n + i,
                value: null as unknown as number,
                date: `+${i + 1}d`,
                fitted: null as unknown as number,
                residual: null as unknown as number,
                upperBand: null as unknown as number,
                lowerBand: null as unknown as number,
                residualBar: null as unknown as number,
                forecast: forecastVal,
                forecastUpper: forecastVal + ciWidth,
                forecastLower: forecastVal - ciWidth,
              } as any);
            }
          }

          return enhancedData;
        }
      } else if (selectedAnalysis === 'forecasting') {
        const forecasts = result.value as number[];
        const meta = result.metadata as Record<string, unknown>;
        const params = result.parameters as Record<string, unknown>;
        const trainSize = (meta?.train_data_size as number) || Math.floor(chartData.length * 0.8);
        const testSize = (meta?.test_data_size as number) || chartData.length - trainSize;
        const horizon = (meta?.forecast_horizon as number) || forecasts?.length || 5;
        const rmse = meta?.rmse as number;
        const method = params?.method as string;

        if (forecasts && forecasts.length > 0) {
          // For ETS forecasting, show:
          // 1. Training data (gray line)
          // 2. Test actual data (amber line with dots)
          // 3. Forecast level through test period (dashed blue)
          // 4. Future forecast projection with confidence bands (green)

          const forecastValue = forecasts[0]; // For simple ETS, all forecasts are the same

          // Calculate standard deviation from training data for confidence bands
          const trainData = priceData.slice(0, trainSize);
          const trainMean = trainData.reduce((a, b) => a + b, 0) / trainData.length;
          const trainStd = Math.sqrt(trainData.reduce((a, b) => a + Math.pow(b - trainMean, 2), 0) / trainData.length);

          // Use RMSE if available, otherwise estimate from training std
          const errorStd = rmse || trainStd * 0.3;

          // Calculate test period errors for visualization
          const testData = priceData.slice(trainSize);
          const testErrors = testData.map(actual => actual - forecastValue);
          const meanTestError = testErrors.length > 0 ? testErrors.reduce((a, b) => a + b, 0) / testErrors.length : 0;

          const enhancedData = chartData.map((d, i) => {
            const isTrainData = i < trainSize;
            const isTestData = i >= trainSize;
            const testIdx = i - trainSize;

            // Calculate error for test points
            const error = isTestData && testIdx < testErrors.length ? testErrors[testIdx] : null;
            const absError = error !== null ? Math.abs(error) : null;

            return {
              ...d,
              trainValue: isTrainData ? d.value : null,
              testActual: isTestData ? d.value : null,
              // Show forecast level as a reference line through the test period
              forecastLevel: isTestData ? forecastValue : null,
              // Confidence bands in test period (95% CI)
              forecastUpper: isTestData ? forecastValue + 1.96 * errorStd : null,
              forecastLower: isTestData ? forecastValue - 1.96 * errorStd : null,
              // Error bar (absolute value for visualization)
              forecastError: error,
              absError: absError,
            };
          });

          // Add future forecast points beyond the historical data with widening confidence bands
          const futureForecasts = forecasts.map((v, i) => {
            // Confidence bands widen over time for future forecasts
            const ciWidth = 1.96 * errorStd * Math.sqrt(1 + (i + 1) * 0.15);

            return {
              index: chartData.length + i,
              value: null,
              trainValue: null,
              testActual: null,
              forecastLevel: null,
              forecastUpper: null,
              forecastLower: null,
              forecastError: null,
              absError: null,
              forecast: v,
              futureForecastUpper: v + ciWidth,
              futureForecastLower: v - ciWidth,
              date: `+${i + 1}d`,
            };
          });

          // Add a connecting point at the last training data point
          if (enhancedData.length > 0 && trainSize > 0) {
            const lastTrainIdx = trainSize - 1;
            enhancedData[lastTrainIdx] = {
              ...enhancedData[lastTrainIdx],
              forecastStart: enhancedData[lastTrainIdx].value,
            } as any;
          }

          return [...enhancedData, ...futureForecasts];
        }
      } else if (selectedAnalysis === 'supervised_learning') {
        const meta = result.metadata as Record<string, unknown>;
        const value = result.value as Record<string, Record<string, unknown>>;
        const bestAlgo = meta?.best_algorithm as string;
        if (bestAlgo && value[bestAlgo]) {
          const predictions = value[bestAlgo].predictions as number[];
          const yTest = meta?.y_test as number[];
          const rmse = value[bestAlgo].rmse as number;

          if (predictions && predictions.length > 0) {
            const nLags = 5;
            const totalSamples = chartData.length - nLags;
            const trainSize = (meta?.train_size as number) || Math.floor(totalSamples * 0.8);
            const testStartIndex = nLags + trainSize;

            // Calculate prediction standard error for confidence bands
            const predictionErrors = yTest ? yTest.map((actual, i) => actual - predictions[i]) : [];
            const predStd = rmse || (predictionErrors.length > 0 ?
              Math.sqrt(predictionErrors.reduce((sum, e) => sum + e * e, 0) / predictionErrors.length) : 0);

            return chartData.map((d, i) => {
              const testIdx = i - testStartIndex;
              const isTestPoint = testIdx >= 0 && testIdx < predictions.length;
              const isTrainPoint = i >= nLags && i < testStartIndex;
              const pred = isTestPoint ? predictions[testIdx] : null;
              const actual = isTestPoint && yTest ? yTest[testIdx] : null;
              const error = (pred !== null && actual !== null) ? actual - pred : null;

              // 95% confidence band (approximately ±2 * RMSE)
              const ciWidth = 1.96 * predStd;

              return {
                ...d,
                trainValue: isTrainPoint ? d.value : null,
                prediction: pred,
                testActual: actual,
                predictionError: error,
                upperBand: pred !== null ? pred + ciWidth : null,
                lowerBand: pred !== null ? pred - ciWidth : null,
                // Zero line for error reference
                zeroLine: isTestPoint ? 0 : null,
              };
            });
          }
        }
      } else if (selectedAnalysis === 'unsupervised_learning') {
        const value = result.value as Record<string, Record<string, unknown>>;
        if (value?.kmeans) {
          const clusterLabels = value.kmeans.cluster_labels as number[];
          if (clusterLabels && clusterLabels.length > 0) {
            return chartData.map((d, i) => ({
              ...d,
              cluster: i < clusterLabels.length ? clusterLabels[i] : 0,
            }));
          }
        }
        return chartData;
      } else if (selectedAnalysis === 'resampling_methods') {
        const value = result.value as Record<string, Record<string, unknown>>;
        if (value?.bootstrap) {
          const bootstrap = value.bootstrap;
          const resampledStats = bootstrap.resampled_statistics as number[];
          const mean = bootstrap.mean as number;
          const ci = bootstrap.confidence_interval as [number, number];

          if (resampledStats && resampledStats.length > 0) {
            // Create histogram data from resampled statistics
            const sortedStats = [...resampledStats].sort((a, b) => a - b);
            const min = sortedStats[0];
            const max = sortedStats[sortedStats.length - 1];
            const binCount = Math.min(30, Math.ceil(Math.sqrt(sortedStats.length)));
            const binWidth = (max - min) / binCount;

            // Find bin indices for mean and CI
            const findBinIndex = (val: number) => {
              const binIdx = Math.floor((val - min) / binWidth);
              return Math.max(0, Math.min(binCount - 1, binIdx));
            };

            const meanBinIdx = findBinIndex(mean);
            const ciLowIdx = ci ? findBinIndex(ci[0]) : 0;
            const ciHighIdx = ci ? findBinIndex(ci[1]) : binCount - 1;

            const bins: { binCenter: number; count: number; frequency: number; inCI: boolean; isMean: boolean }[] = [];
            for (let i = 0; i < binCount; i++) {
              const binStart = min + i * binWidth;
              const binEnd = binStart + binWidth;
              const binCenter = (binStart + binEnd) / 2;
              const count = sortedStats.filter(v => v >= binStart && (i === binCount - 1 ? v <= binEnd : v < binEnd)).length;
              const inCI = i >= ciLowIdx && i <= ciHighIdx;
              const isMean = i === meanBinIdx;
              bins.push({
                binCenter,
                count,
                frequency: count / sortedStats.length,
                inCI,
                isMean,
              });
            }

            return bins.map((bin, i) => ({
              index: i,
              value: bin.count,
              binCenter: bin.binCenter,
              frequency: bin.frequency,
              inCI: bin.inCI,
              isMean: bin.isMean,
              date: formatNumber(bin.binCenter, 2),
              meanBinIdx,
              ciLowIdx,
              ciHighIdx,
            }));
          }
        }
        // If no bootstrap data, show original data with jackknife leave-one-out visualization
        return chartData;
      } else if (selectedAnalysis === 'validate_data') {
        // Data Quality visualization: show data with outlier highlighting
        // Compute z-scores for outlier detection
        if (priceData.length < 3) return chartData;

        const mean = priceData.reduce((a, b) => a + b, 0) / priceData.length;
        const variance = priceData.reduce((a, b) => a + Math.pow(b - mean, 2), 0) / priceData.length;
        const std = Math.sqrt(variance);

        const outlierThreshold = 3.0; // Z-score threshold for outliers

        return chartData.map((d, i) => {
          const zScore = std > 0 ? Math.abs((d.value - mean) / std) : 0;
          const isOutlier = zScore > outlierThreshold;
          const isSuspicious = zScore > 2.0 && zScore <= outlierThreshold;

          return {
            ...d,
            zScore,
            isOutlier,
            isSuspicious,
            normalValue: !isOutlier && !isSuspicious ? d.value : null,
            suspiciousValue: isSuspicious ? d.value : null,
            outlierValue: isOutlier ? d.value : null,
            mean,
          };
        });
      }
      return chartData;
    };

    const keyMetrics = getKeyMetrics();
    const resultChartData = getResultChartData();

    // Compute cluster boundary separately (not in chart data to avoid breaking the chart)
    let clusterBoundary: number | null = null;
    if (selectedAnalysis === 'unsupervised_learning' && analysisResult?.result) {
      const result = analysisResult.result as Record<string, unknown>;
      const value = result.value as Record<string, Record<string, unknown>>;
      if (value?.kmeans) {
        const clusterLabels = value.kmeans.cluster_labels as number[];
        if (clusterLabels && clusterLabels.length > 0 && priceData.length > 0) {
          const clusterSums: Record<number, { sum: number; count: number }> = {};
          priceData.forEach((price, i) => {
            const cluster = i < clusterLabels.length ? clusterLabels[i] : 0;
            if (!clusterSums[cluster]) clusterSums[cluster] = { sum: 0, count: 0 };
            clusterSums[cluster].sum += price;
            clusterSums[cluster].count += 1;
          });
          const means = Object.values(clusterSums).map(s => s.sum / s.count).sort((a, b) => a - b);
          if (means.length >= 2) {
            clusterBoundary = (means[0] + means[1]) / 2;
          }
        }
      }
    }

    // Compute Bootstrap reference values separately
    let bootstrapMean: number | null = null;
    let bootstrapCiLow: number | null = null;
    let bootstrapCiHigh: number | null = null;
    let bootstrapMeanIdx: number | null = null;
    let bootstrapCiLowIdx: number | null = null;
    let bootstrapCiHighIdx: number | null = null;
    if (selectedAnalysis === 'resampling_methods' && analysisResult?.result) {
      const result = analysisResult.result as Record<string, unknown>;
      const value = result.value as Record<string, Record<string, unknown>>;
      if (value?.bootstrap) {
        bootstrapMean = value.bootstrap.mean as number;
        const ci = value.bootstrap.confidence_interval as [number, number];
        if (ci && ci.length === 2) {
          bootstrapCiLow = ci[0];
          bootstrapCiHigh = ci[1];
        }
        // Get indices from chart data
        if (resultChartData.length > 0 && (resultChartData[0] as any)?.meanBinIdx !== undefined) {
          bootstrapMeanIdx = (resultChartData[0] as any).meanBinIdx as number;
          bootstrapCiLowIdx = (resultChartData[0] as any).ciLowIdx as number;
          bootstrapCiHighIdx = (resultChartData[0] as any).ciHighIdx as number;
        }
      }
    }

    return (
      <div className="flex h-full">
        {/* Left Panel - Metrics */}
        <div className="w-80 flex-shrink-0 p-4 overflow-y-auto" style={{ backgroundColor: BB.panelBg, borderRight: `1px solid ${BB.borderDark}` }}>
          <div className="flex items-center justify-between mb-3">
            <div className="text-xs font-mono" style={{ color: BB.textAmber }}>RESULTS</div>
            <div className="flex gap-2">
              <button onClick={() => copyToClipboard(JSON.stringify(analysisResult, null, 2))} className="p-1.5" style={{ backgroundColor: BB.cardBg }}>
                <Copy size={14} style={{ color: BB.textMuted }} />
              </button>
              <button onClick={exportResults} className="p-1.5" style={{ backgroundColor: BB.cardBg }}>
                <Download size={14} style={{ color: BB.textMuted }} />
              </button>
            </div>
          </div>

          {/* Status Banner */}
          <div className="p-3 mb-4" style={{ backgroundColor: `${BB.green}10`, border: `1px solid ${BB.green}` }}>
            <div className="flex items-center gap-2">
              <CheckCircle2 size={16} style={{ color: BB.green }} />
              <span className="text-xs font-mono font-bold" style={{ color: BB.green }}>COMPLETE</span>
            </div>
            <div className="text-xs font-mono mt-1" style={{ color: BB.textMuted }}>
              {config?.name} • {analysisResult.timestamp ? new Date(analysisResult.timestamp).toLocaleTimeString() : 'Just now'}
            </div>
          </div>

          {/* Key Metrics */}
          {keyMetrics.length > 0 && (
            <div className="space-y-2 mb-4">
              {keyMetrics.map((metric, i) => (
                <div key={i} className="flex items-center justify-between p-3" style={{ backgroundColor: BB.cardBg, border: `1px solid ${BB.borderDark}` }}>
                  <span className="text-xs font-mono" style={{ color: BB.textMuted }}>{metric.label}</span>
                  <div className="flex items-center gap-2">
                    {metric.icon && <metric.icon size={14} style={{ color: metric.color }} />}
                    <span className="text-sm font-mono font-bold" style={{ color: metric.color }}>{metric.value}</span>
                  </div>
                </div>
              ))}
            </div>
          )}

          {/* Raw Data Section */}
          <div className="mb-4">
            <div className="text-xs font-mono mb-2" style={{ color: BB.textAmber }}>RAW OUTPUT</div>
            <div className="p-3 text-xs font-mono overflow-x-auto" style={{ backgroundColor: BB.black, border: `1px solid ${BB.borderDark}`, maxHeight: 200, overflowY: 'auto' }}>
              {Object.entries(result).map(([key, value]) => {
                if (key === 'model_summary' || (Array.isArray(value) && value.length > 20)) return null;
                return (
                  <div key={key} className="flex gap-2 py-1" style={{ borderBottom: `1px solid ${BB.borderDark}` }}>
                    <span style={{ color: BB.textMuted }}>{key}:</span>
                    <span style={{ color: BB.textPrimary }} className="truncate">
                      {typeof value === 'object' ? JSON.stringify(value).substring(0, 50) + '...' : String(value)}
                    </span>
                  </div>
                );
              })}
            </div>
          </div>

          <div className="flex gap-2">
            <button
              onClick={() => setCurrentStep('analysis')}
              className="flex-1 py-2 text-xs font-mono"
              style={{ backgroundColor: BB.cardBg, color: BB.textSecondary, border: `1px solid ${BB.borderDark}` }}
            >
              <ChevronLeft size={12} className="inline" /> MODELS
            </button>
            <button
              onClick={() => {
                setCurrentStep('data');
                setAnalysisResult(null);
              }}
              className="flex-1 py-2 text-xs font-mono font-bold"
              style={{ backgroundColor: BB.amber, color: BB.black }}
            >
              NEW <Play size={12} className="inline ml-1" />
            </button>
          </div>
        </div>

        {/* Right Panel - Chart with Results */}
        <div className="flex-1 p-4" style={{ backgroundColor: BB.darkBg }}>
          <div className="h-full flex flex-col">
            <div className="flex items-center justify-between mb-2">
              <div className="flex items-center gap-3">
                <div className="text-xs font-mono" style={{ color: BB.textAmber }}>{config?.shortName} OUTPUT</div>
                {resultsChartZoom && (
                  <div className="text-xs font-mono px-2 py-0.5" style={{ backgroundColor: BB.amber + '20', color: BB.amber }}>
                    {getDateForIndex(resultsChartZoom.startIndex)} - {getDateForIndex(resultsChartZoom.endIndex)}
                  </div>
                )}
              </div>
              <div className="flex items-center gap-3">
                <ZoomControls chartType="results" isZoomed={resultsChartZoom !== null} />
                <div className="text-xs font-mono" style={{ color: BB.textMuted }}>{symbolInput.toUpperCase()}</div>
              </div>
            </div>
            <div
              className="flex-1"
              style={{ minHeight: 300 }}
              onWheel={(e) => handleWheel(e, 'results')}
            >
              <ResponsiveContainer width="100%" height="100%">
                <ComposedChart
                  data={resultsChartZoom ? resultChartData.slice(resultsChartZoom.startIndex, resultsChartZoom.endIndex + 1) : resultChartData}
                  margin={{ top: 10, right: 10, left: 0, bottom: 30 }}
                  onMouseDown={(e) => handleMouseDown(e, 'results')}
                  onMouseMove={handleMouseMove}
                  onMouseUp={handleMouseUp}
                  onMouseLeave={handleMouseUp}
                >
                  <CartesianGrid strokeDasharray="3 3" stroke={BB.chartGrid} />
                  <XAxis
                    dataKey={selectedAnalysis === 'resampling_methods' ? 'index' : 'date'}
                    tick={{ fill: BB.textMuted, fontSize: 9 }}
                    axisLine={{ stroke: BB.borderDark }}
                    interval={selectedAnalysis === 'resampling_methods' ? 'preserveStartEnd' : 'preserveStartEnd'}
                    tickFormatter={(val) => {
                      if (selectedAnalysis === 'resampling_methods' && (resultChartData[val] as any)?.binCenter !== undefined) {
                        return formatNumber((resultChartData[val] as any).binCenter as number, 1);
                      }
                      return val;
                    }}
                  />
                  <YAxis
                    tick={{ fill: BB.textMuted, fontSize: 10 }}
                    axisLine={{ stroke: BB.borderDark }}
                    domain={['auto', 'auto']}
                    tickFormatter={(val) => selectedAnalysis === 'resampling_methods' ? String(Math.round(val)) : formatPrice(val)}
                    label={selectedAnalysis === 'resampling_methods' ? { value: 'COUNT', angle: -90, position: 'insideLeft', fill: BB.textMuted, fontSize: 9 } : undefined}
                  />
                  <Tooltip
                    contentStyle={{
                      backgroundColor: BB.cardBg,
                      border: `1px solid ${BB.borderLight}`,
                      borderRadius: 0,
                      fontSize: 11,
                    }}
                    labelFormatter={(label) => selectedAnalysis === 'resampling_methods' ? `Value: ${label}` : `Date: ${label}`}
                    labelStyle={{ color: BB.textAmber, fontWeight: 'bold' }}
                    formatter={(value: number, name: string, props: { payload?: { frequency?: number; inCI?: boolean; binCenter?: number; zScore?: number; isOutlier?: boolean; isSuspicious?: boolean; value?: number } }) => {
                      if (selectedAnalysis === 'resampling_methods' && props.payload) {
                        const freq = props.payload.frequency;
                        const inCI = props.payload.inCI;
                        return [
                          <span key="val">
                            Count: <strong>{value}</strong> ({freq ? (freq * 100).toFixed(1) : 0}%)
                            {inCI && <span style={{ color: BB.blueLight, marginLeft: 8 }}>In 95% CI</span>}
                          </span>,
                          ''
                        ];
                      }
                      if (selectedAnalysis === 'validate_data' && props.payload) {
                        const zScore = props.payload.zScore;
                        const isOutlier = props.payload.isOutlier;
                        const isSuspicious = props.payload.isSuspicious;
                        const priceVal = props.payload.value;
                        let status = 'Normal';
                        let statusColor = BB.green;
                        if (isOutlier) { status = 'OUTLIER'; statusColor = BB.red; }
                        else if (isSuspicious) { status = 'Suspicious'; statusColor = BB.amber; }
                        return [
                          <span key="val">
                            <span style={{ color: BB.textPrimary }}>Price: <strong>{priceVal !== undefined ? formatPrice(priceVal) : formatPrice(value)}</strong></span>
                            <br />
                            <span style={{ color: BB.textSecondary }}>Z-Score: {zScore !== undefined ? zScore.toFixed(2) : '—'}</span>
                            <br />
                            <span style={{ color: statusColor }}>Status: {status}</span>
                          </span>,
                          ''
                        ];
                      }
                      return [value, name];
                    }}
                  />
                  {/* Original data (for non-forecasting/non-unsupervised/non-bootstrap/non-validate/non-stationarity analyses) */}
                  {selectedAnalysis !== 'forecasting' && selectedAnalysis !== 'unsupervised_learning' && selectedAnalysis !== 'resampling_methods' && selectedAnalysis !== 'validate_data' && selectedAnalysis !== 'stationarity_test' && (
                    <Line
                      type="monotone"
                      dataKey="value"
                      stroke={BB.textSecondary}
                      strokeWidth={1}
                      dot={false}
                      name="Actual"
                    />
                  )}
                  {/* Stationarity Test: Price line */}
                  {selectedAnalysis === 'stationarity_test' && (
                    <Line
                      type="monotone"
                      dataKey="value"
                      stroke={BB.textSecondary}
                      strokeWidth={1}
                      dot={false}
                      name="Price"
                    />
                  )}
                  {/* Stationarity Test: Upper Bollinger Band */}
                  {selectedAnalysis === 'stationarity_test' && (
                    <Line
                      type="monotone"
                      dataKey="upperBand"
                      stroke={BB.amber}
                      strokeWidth={1}
                      strokeDasharray="3 3"
                      dot={false}
                      name="Upper Band (+2σ)"
                      connectNulls={false}
                    />
                  )}
                  {/* Stationarity Test: Lower Bollinger Band */}
                  {selectedAnalysis === 'stationarity_test' && (
                    <Line
                      type="monotone"
                      dataKey="lowerBand"
                      stroke={BB.amber}
                      strokeWidth={1}
                      strokeDasharray="3 3"
                      dot={false}
                      name="Lower Band (-2σ)"
                      connectNulls={false}
                    />
                  )}
                  {/* Stationarity Test: Rolling Mean */}
                  {selectedAnalysis === 'stationarity_test' && (
                    <Line
                      type="monotone"
                      dataKey="rollingMean"
                      stroke={BB.green}
                      strokeWidth={2}
                      dot={false}
                      name="Rolling Mean"
                      connectNulls={false}
                    />
                  )}
                  {/* Stationarity Test: Overall Mean Reference */}
                  {selectedAnalysis === 'stationarity_test' && resultChartData.length > 0 && (resultChartData[0] as any).overallMean !== undefined && (
                    <ReferenceLine
                      y={(resultChartData[0] as any).overallMean}
                      stroke={BB.blueLight}
                      strokeWidth={2}
                      strokeDasharray="5 5"
                      label={{ value: 'OVERALL MEAN', fill: BB.blueLight, fontSize: 9, position: 'right' }}
                    />
                  )}
                  {/* Data Quality: Normal data line */}
                  {selectedAnalysis === 'validate_data' && (
                    <Line
                      type="monotone"
                      dataKey="normalValue"
                      stroke={BB.green}
                      strokeWidth={1.5}
                      dot={false}
                      name="Normal"
                      connectNulls={false}
                    />
                  )}
                  {/* Data Quality: Suspicious data (2-3 std dev) */}
                  {selectedAnalysis === 'validate_data' && (
                    <Scatter
                      dataKey="suspiciousValue"
                      fill={BB.amber}
                      name="Suspicious"
                      shape={(props: any) => {
                        if (!props.cx || !props.cy) return <g />;
                        return (
                          <circle
                            cx={props.cx}
                            cy={props.cy}
                            r={5}
                            fill={BB.amber}
                            stroke={BB.black}
                            strokeWidth={1}
                          />
                        );
                      }}
                    />
                  )}
                  {/* Data Quality: Outlier data (>3 std dev) */}
                  {selectedAnalysis === 'validate_data' && (
                    <Scatter
                      dataKey="outlierValue"
                      fill={BB.red}
                      name="Outlier"
                      shape={(props: any) => {
                        if (!props.cx || !props.cy) return <g />;
                        return (
                          <g>
                            <circle
                              cx={props.cx}
                              cy={props.cy}
                              r={7}
                              fill={BB.red}
                              stroke={BB.black}
                              strokeWidth={1}
                            />
                            <text
                              x={props.cx}
                              y={props.cy + 3}
                              textAnchor="middle"
                              fill={BB.textPrimary}
                              fontSize={10}
                              fontWeight="bold"
                            >
                              !
                            </text>
                          </g>
                        );
                      }}
                    />
                  )}
                  {/* Data Quality: Mean reference line */}
                  {selectedAnalysis === 'validate_data' && resultChartData.length > 0 && (resultChartData[0] as any).mean !== undefined && (
                    <ReferenceLine
                      y={(resultChartData[0] as any).mean}
                      stroke={BB.blueLight}
                      strokeWidth={2}
                      strokeDasharray="5 5"
                      label={{ value: 'MEAN', fill: BB.blueLight, fontSize: 10, position: 'right' }}
                    />
                  )}
                  {/* Bootstrap: CI shaded area */}
                  {selectedAnalysis === 'resampling_methods' && typeof bootstrapCiLowIdx === 'number' && typeof bootstrapCiHighIdx === 'number' && (
                    <ReferenceArea
                      x1={bootstrapCiLowIdx}
                      x2={bootstrapCiHighIdx}
                      fill={BB.blueLight}
                      fillOpacity={0.15}
                      stroke={BB.blueLight}
                      strokeOpacity={0.3}
                    />
                  )}
                  {/* Bootstrap: Histogram bars with conditional coloring */}
                  {selectedAnalysis === 'resampling_methods' && (
                    <Bar
                      dataKey="value"
                      name="Frequency"
                      shape={(props: any) => {
                        const { x, y, width, height, payload } = props;
                        const inCI = payload?.inCI;
                        const isMean = payload?.isMean;
                        let fill = BB.green;
                        let opacity = 0.5;
                        if (inCI) {
                          fill = BB.green;
                          opacity = 0.8;
                        }
                        if (isMean) {
                          fill = BB.amber;
                          opacity = 1;
                        }
                        return (
                          <rect
                            x={x}
                            y={y}
                            width={width}
                            height={height}
                            fill={fill}
                            fillOpacity={opacity}
                            stroke={isMean ? BB.amber : 'none'}
                            strokeWidth={isMean ? 2 : 0}
                          />
                        );
                      }}
                    />
                  )}
                  {/* Bootstrap: Mean reference line */}
                  {selectedAnalysis === 'resampling_methods' && typeof bootstrapMeanIdx === 'number' && (
                    <ReferenceLine
                      x={bootstrapMeanIdx}
                      stroke={BB.amber}
                      strokeWidth={2}
                      label={{ value: `MEAN: ${formatNumber(bootstrapMean || 0, 2)}`, fill: BB.amber, fontSize: 10, position: 'top' }}
                    />
                  )}
                  {/* Unsupervised Learning: Cluster boundary line */}
                  {selectedAnalysis === 'unsupervised_learning' && typeof clusterBoundary === 'number' && (
                    <ReferenceLine
                      y={clusterBoundary}
                      stroke={BB.amber}
                      strokeWidth={2}
                      strokeDasharray="6 3"
                    />
                  )}
                  {/* Unsupervised Learning: Price line with cluster colors */}
                  {selectedAnalysis === 'unsupervised_learning' && (
                    <Line
                      type="monotone"
                      dataKey="value"
                      stroke={BB.textMuted}
                      strokeWidth={1}
                      strokeOpacity={0.5}
                      dot={(props) => {
                        const p = props as { cx: number; cy: number; payload: { cluster?: number } };
                        if (!p.cx || !p.cy) return <g />;
                        const colors = [BB.green, BB.amber, BB.blueLight, BB.red];
                        const c = p.payload?.cluster ?? 0;
                        return <circle cx={p.cx} cy={p.cy} r={4} fill={colors[c % colors.length]} />;
                      }}
                      name="Clustered Data"
                    />
                  )}
                  {/* ETS/Forecasting: Training data */}
                  <Line
                    type="monotone"
                    dataKey="trainValue"
                    stroke={BB.textSecondary}
                    dot={false}
                    strokeWidth={1.5}
                    name="Training Data"
                  />
                  {/* ETS/Forecasting: Test period actual values */}
                  <Line
                    type="monotone"
                    dataKey="testActual"
                    stroke={BB.amber}
                    dot={{ fill: BB.amber, r: 2 }}
                    strokeWidth={2}
                    name="Actual (Test)"
                  />
                  {/* ETS/Forecasting: Forecast level line (shows expected value) */}
                  <Line
                    type="monotone"
                    dataKey="forecastLevel"
                    stroke={BB.blueLight}
                    dot={false}
                    strokeWidth={2}
                    strokeDasharray="5 5"
                    name="Forecast Level"
                  />
                  {/* ETS/Forecasting: Upper confidence band in test period */}
                  {selectedAnalysis === 'forecasting' && (
                    <Line
                      type="monotone"
                      dataKey="forecastUpper"
                      stroke={BB.amber}
                      strokeWidth={1}
                      strokeDasharray="3 3"
                      dot={false}
                      name="95% CI Upper"
                      connectNulls={false}
                    />
                  )}
                  {/* ETS/Forecasting: Lower confidence band in test period */}
                  {selectedAnalysis === 'forecasting' && (
                    <Line
                      type="monotone"
                      dataKey="forecastLower"
                      stroke={BB.amber}
                      strokeWidth={1}
                      strokeDasharray="3 3"
                      dot={false}
                      name="95% CI Lower"
                      connectNulls={false}
                    />
                  )}
                  {/* ETS/Forecasting: Future forecast upper band (widening) */}
                  {selectedAnalysis === 'forecasting' && (
                    <Line
                      type="monotone"
                      dataKey="futureForecastUpper"
                      stroke={BB.greenDim}
                      strokeWidth={1}
                      strokeDasharray="3 3"
                      dot={false}
                      name="Forecast CI Upper"
                      connectNulls={false}
                    />
                  )}
                  {/* ETS/Forecasting: Future forecast lower band (widening) */}
                  {selectedAnalysis === 'forecasting' && (
                    <Line
                      type="monotone"
                      dataKey="futureForecastLower"
                      stroke={BB.greenDim}
                      strokeWidth={1}
                      strokeDasharray="3 3"
                      dot={false}
                      name="Forecast CI Lower"
                      connectNulls={false}
                    />
                  )}
                  {/* Trend Analysis: Upper confidence band */}
                  {selectedAnalysis === 'trend_analysis' && (
                    <Line
                      type="monotone"
                      dataKey="upperBand"
                      stroke={BB.amber}
                      strokeWidth={1}
                      strokeDasharray="3 3"
                      dot={false}
                      name="95% CI Upper"
                      connectNulls={false}
                    />
                  )}
                  {/* Trend Analysis: Lower confidence band */}
                  {selectedAnalysis === 'trend_analysis' && (
                    <Line
                      type="monotone"
                      dataKey="lowerBand"
                      stroke={BB.amber}
                      strokeWidth={1}
                      strokeDasharray="3 3"
                      dot={false}
                      name="95% CI Lower"
                      connectNulls={false}
                    />
                  )}
                  {/* Fitted/trend line */}
                  <Line
                    type="monotone"
                    dataKey="fitted"
                    stroke={config?.color || BB.amber}
                    dot={false}
                    strokeWidth={2}
                    strokeDasharray="5 5"
                    name="Fitted"
                  />
                  {/* Trend Analysis: Future projection line */}
                  {selectedAnalysis === 'trend_analysis' && (
                    <Line
                      type="monotone"
                      dataKey="projection"
                      stroke={BB.green}
                      strokeWidth={2}
                      dot={{ fill: BB.green, r: 4 }}
                      name="Projection"
                      connectNulls={false}
                    />
                  )}
                  {/* Trend Analysis: Projection upper band */}
                  {selectedAnalysis === 'trend_analysis' && (
                    <Line
                      type="monotone"
                      dataKey="projectionUpper"
                      stroke={BB.greenDim}
                      strokeWidth={1}
                      strokeDasharray="3 3"
                      dot={false}
                      name="Projection CI Upper"
                      connectNulls={false}
                    />
                  )}
                  {/* Trend Analysis: Projection lower band */}
                  {selectedAnalysis === 'trend_analysis' && (
                    <Line
                      type="monotone"
                      dataKey="projectionLower"
                      stroke={BB.greenDim}
                      strokeWidth={1}
                      strokeDasharray="3 3"
                      dot={false}
                      name="Projection CI Lower"
                      connectNulls={false}
                    />
                  )}
                  {/* ARIMA: Upper confidence band */}
                  {selectedAnalysis === 'arima_model' && (
                    <Line
                      type="monotone"
                      dataKey="upperBand"
                      stroke={BB.amber}
                      strokeWidth={1}
                      strokeDasharray="3 3"
                      dot={false}
                      name="95% CI Upper"
                      connectNulls={false}
                    />
                  )}
                  {/* ARIMA: Lower confidence band */}
                  {selectedAnalysis === 'arima_model' && (
                    <Line
                      type="monotone"
                      dataKey="lowerBand"
                      stroke={BB.amber}
                      strokeWidth={1}
                      strokeDasharray="3 3"
                      dot={false}
                      name="95% CI Lower"
                      connectNulls={false}
                    />
                  )}
                  {/* ARIMA: Forecast projection line */}
                  {selectedAnalysis === 'arima_model' && (
                    <Line
                      type="monotone"
                      dataKey="forecast"
                      stroke={BB.green}
                      strokeWidth={2}
                      dot={{ fill: BB.green, r: 4 }}
                      name="ARIMA Forecast"
                      connectNulls={false}
                    />
                  )}
                  {/* ARIMA: Forecast upper band */}
                  {selectedAnalysis === 'arima_model' && (
                    <Line
                      type="monotone"
                      dataKey="forecastUpper"
                      stroke={BB.greenDim}
                      strokeWidth={1}
                      strokeDasharray="3 3"
                      dot={false}
                      name="Forecast CI Upper"
                      connectNulls={false}
                    />
                  )}
                  {/* ARIMA: Forecast lower band */}
                  {selectedAnalysis === 'arima_model' && (
                    <Line
                      type="monotone"
                      dataKey="forecastLower"
                      stroke={BB.greenDim}
                      strokeWidth={1}
                      strokeDasharray="3 3"
                      dot={false}
                      name="Forecast CI Lower"
                      connectNulls={false}
                    />
                  )}
                  {/* Future Forecast */}
                  <Line
                    type="monotone"
                    dataKey="forecast"
                    stroke={BB.green}
                    dot={{ fill: BB.green, r: 4 }}
                    strokeWidth={2}
                    name="Forecast"
                  />
                  {/* ML Prediction: Training data line */}
                  {selectedAnalysis === 'supervised_learning' && (
                    <Line
                      type="monotone"
                      dataKey="trainValue"
                      stroke={BB.textSecondary}
                      strokeWidth={1}
                      dot={false}
                      name="Training Data"
                      connectNulls={false}
                    />
                  )}
                  {/* ML Prediction: Upper confidence band */}
                  {selectedAnalysis === 'supervised_learning' && (
                    <Line
                      type="monotone"
                      dataKey="upperBand"
                      stroke={BB.amber}
                      strokeWidth={1}
                      strokeDasharray="3 3"
                      dot={false}
                      name="95% CI Upper"
                      connectNulls={false}
                    />
                  )}
                  {/* ML Prediction: Lower confidence band */}
                  {selectedAnalysis === 'supervised_learning' && (
                    <Line
                      type="monotone"
                      dataKey="lowerBand"
                      stroke={BB.amber}
                      strokeWidth={1}
                      strokeDasharray="3 3"
                      dot={false}
                      name="95% CI Lower"
                      connectNulls={false}
                    />
                  )}
                  {/* ML Prediction: Test actual values */}
                  {selectedAnalysis === 'supervised_learning' && (
                    <Line
                      type="monotone"
                      dataKey="testActual"
                      stroke={BB.green}
                      strokeWidth={2}
                      dot={{ fill: BB.green, r: 3 }}
                      name="Test Actual"
                      connectNulls={false}
                    />
                  )}
                  {/* ML Prediction */}
                  {selectedAnalysis === 'supervised_learning' && (
                    <Line
                      type="monotone"
                      dataKey="prediction"
                      stroke={BB.blueLight}
                      dot={{ fill: BB.blueLight, r: 3 }}
                      strokeWidth={2}
                      name="ML Prediction"
                      connectNulls={false}
                    />
                  )}
                  {/* ML Prediction (for other analysis types that use it) */}
                  {selectedAnalysis !== 'supervised_learning' && (
                    <Line
                      type="monotone"
                      dataKey="prediction"
                      stroke={BB.blueLight}
                      dot={{ fill: BB.blueLight, r: 3 }}
                      strokeWidth={2}
                      name="ML Prediction"
                    />
                  )}
                  {/* Brush for horizontal pan/zoom */}
                  {!resultsChartZoom && (
                    <Brush
                      dataKey="index"
                      height={20}
                      stroke={BB.amber}
                      fill={BB.cardBg}
                      tickFormatter={(val) => `${val}`}
                      onChange={(e) => {
                        if (e.startIndex !== undefined && e.endIndex !== undefined && e.startIndex !== e.endIndex) {
                          setResultsChartZoom({ startIndex: e.startIndex, endIndex: e.endIndex });
                        }
                      }}
                    />
                  )}
                  {/* Selection area indicator */}
                  {isSelecting && activeChart === 'results' && refAreaLeft !== null && refAreaRight !== null && (
                    <ReferenceArea
                      x1={refAreaLeft}
                      x2={refAreaRight}
                      strokeOpacity={0.3}
                      fill={BB.amber}
                      fillOpacity={0.2}
                    />
                  )}
                </ComposedChart>
              </ResponsiveContainer>
            </div>

            {/* Legend */}
            <div className="flex items-center justify-between mt-2 px-2">
              <div className="flex items-center gap-4 flex-wrap">
                {selectedAnalysis === 'forecasting' && (
                  <>
                    <div className="flex items-center gap-2">
                      <div className="w-4 h-0.5" style={{ backgroundColor: BB.textSecondary }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>TRAIN DATA</span>
                    </div>
                    <div className="flex items-center gap-2">
                      <div className="w-2 h-2 rounded-full" style={{ backgroundColor: BB.amber }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>TEST ACTUAL</span>
                    </div>
                    <div className="flex items-center gap-2">
                      <div className="w-4 border-t-2 border-dashed" style={{ borderColor: BB.blueLight }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>ETS LEVEL</span>
                    </div>
                    <div className="flex items-center gap-2">
                      <div className="w-4 border-t border-dashed" style={{ borderColor: BB.amber }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>95% CI</span>
                    </div>
                    <div className="flex items-center gap-2">
                      <div className="w-2 h-2 rounded-full" style={{ backgroundColor: BB.green }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>FORECAST</span>
                    </div>
                    <div className="flex items-center gap-2">
                      <div className="w-4 border-t border-dashed" style={{ borderColor: BB.greenDim }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>FORECAST CI</span>
                    </div>
                  </>
                )}
                {selectedAnalysis === 'resampling_methods' && (
                  <>
                    <div className="flex items-center gap-2">
                      <div className="w-3 h-3" style={{ backgroundColor: BB.green, opacity: 0.7 }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>DISTRIBUTION</span>
                    </div>
                    <div className="flex items-center gap-2">
                      <div className="w-4 h-0.5" style={{ backgroundColor: BB.amber }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>MEAN</span>
                    </div>
                    <div className="flex items-center gap-2">
                      <div className="w-4 h-3" style={{ backgroundColor: BB.blueLight, opacity: 0.2 }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>95% CI</span>
                    </div>
                  </>
                )}
                {selectedAnalysis === 'unsupervised_learning' && (
                  <>
                    <div className="flex items-center gap-2">
                      <div className="w-4 h-0.5" style={{ backgroundColor: BB.textSecondary }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>PRICE</span>
                    </div>
                    <div className="flex items-center gap-2">
                      <div className="w-4 border-t-2 border-dashed" style={{ borderColor: BB.amber }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>BOUNDARY</span>
                    </div>
                    <div className="flex items-center gap-2">
                      <div className="w-2 h-2 rounded-full" style={{ backgroundColor: BB.green }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>CLUSTER 0</span>
                    </div>
                    <div className="flex items-center gap-2">
                      <div className="w-2 h-2 rounded-full" style={{ backgroundColor: BB.amber }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>CLUSTER 1</span>
                    </div>
                  </>
                )}
                {selectedAnalysis === 'supervised_learning' && (
                  <>
                    <div className="flex items-center gap-2">
                      <div className="w-4 h-0.5" style={{ backgroundColor: BB.textSecondary }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>TRAIN DATA</span>
                    </div>
                    <div className="flex items-center gap-2">
                      <div className="w-2 h-2 rounded-full" style={{ backgroundColor: BB.green }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>TEST ACTUAL</span>
                    </div>
                    <div className="flex items-center gap-2">
                      <div className="w-2 h-2 rounded-full" style={{ backgroundColor: BB.blueLight }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>PREDICTION</span>
                    </div>
                    <div className="flex items-center gap-2">
                      <div className="w-4 border-t border-dashed" style={{ borderColor: BB.amber }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>95% CI</span>
                    </div>
                  </>
                )}
                {selectedAnalysis === 'validate_data' && (
                  <>
                    <div className="flex items-center gap-2">
                      <div className="w-4 h-0.5" style={{ backgroundColor: BB.green }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>NORMAL DATA</span>
                    </div>
                    <div className="flex items-center gap-2">
                      <div className="w-2 h-2 rounded-full" style={{ backgroundColor: BB.amber }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>SUSPICIOUS (2-3σ)</span>
                    </div>
                    <div className="flex items-center gap-2">
                      <div className="w-2 h-2 rounded-full" style={{ backgroundColor: BB.red }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>OUTLIER (&gt;3σ)</span>
                    </div>
                    <div className="flex items-center gap-2">
                      <div className="w-4 border-t-2 border-dashed" style={{ borderColor: BB.blueLight }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>MEAN</span>
                    </div>
                  </>
                )}
                {selectedAnalysis === 'stationarity_test' && (
                  <>
                    <div className="flex items-center gap-2">
                      <div className="w-4 h-0.5" style={{ backgroundColor: BB.textSecondary }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>PRICE</span>
                    </div>
                    <div className="flex items-center gap-2">
                      <div className="w-4 h-0.5" style={{ backgroundColor: BB.green }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>ROLLING MEAN</span>
                    </div>
                    <div className="flex items-center gap-2">
                      <div className="w-4 border-t-2 border-dashed" style={{ borderColor: BB.amber }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>±2σ BANDS</span>
                    </div>
                    <div className="flex items-center gap-2">
                      <div className="w-4 border-t-2 border-dashed" style={{ borderColor: BB.blueLight }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>OVERALL MEAN</span>
                    </div>
                  </>
                )}
                {selectedAnalysis === 'trend_analysis' && (
                  <>
                    <div className="flex items-center gap-2">
                      <div className="w-4 h-0.5" style={{ backgroundColor: BB.textSecondary }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>ACTUAL</span>
                    </div>
                    <div className="flex items-center gap-2">
                      <div className="w-4 border-t-2 border-dashed" style={{ borderColor: config?.color || BB.amber }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>TREND</span>
                    </div>
                    <div className="flex items-center gap-2">
                      <div className="w-4 border-t border-dashed" style={{ borderColor: BB.amber }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>95% CI</span>
                    </div>
                    <div className="flex items-center gap-2">
                      <div className="w-2 h-2 rounded-full" style={{ backgroundColor: BB.green }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>PROJECTION</span>
                    </div>
                  </>
                )}
                {selectedAnalysis === 'arima_model' && (
                  <>
                    <div className="flex items-center gap-2">
                      <div className="w-4 h-0.5" style={{ backgroundColor: BB.textSecondary }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>ACTUAL</span>
                    </div>
                    <div className="flex items-center gap-2">
                      <div className="w-4 border-t-2 border-dashed" style={{ borderColor: BB.amber }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>FITTED</span>
                    </div>
                    <div className="flex items-center gap-2">
                      <div className="w-4 border-t border-dashed" style={{ borderColor: BB.amber }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>95% CI</span>
                    </div>
                    <div className="flex items-center gap-2">
                      <div className="w-2 h-2 rounded-full" style={{ backgroundColor: BB.green }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>FORECAST</span>
                    </div>
                    <div className="flex items-center gap-2">
                      <div className="w-4 border-t border-dashed" style={{ borderColor: BB.greenDim }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>FORECAST CI</span>
                    </div>
                  </>
                )}
                {selectedAnalysis !== 'forecasting' && selectedAnalysis !== 'resampling_methods' && selectedAnalysis !== 'unsupervised_learning' && selectedAnalysis !== 'supervised_learning' && selectedAnalysis !== 'validate_data' && selectedAnalysis !== 'stationarity_test' && selectedAnalysis !== 'trend_analysis' && selectedAnalysis !== 'arima_model' && (
                  <>
                    <div className="flex items-center gap-2">
                      <div className="w-4 h-0.5" style={{ backgroundColor: BB.textSecondary }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>ACTUAL</span>
                    </div>
                    <div className="flex items-center gap-2">
                      <div className="w-4 h-0.5" style={{ backgroundColor: config?.color, borderStyle: 'dashed', borderWidth: 1, borderColor: config?.color }} />
                      <span className="text-xs font-mono" style={{ color: BB.textMuted }}>FITTED</span>
                    </div>
                  </>
                )}
              </div>
              <div className="text-xs font-mono" style={{ color: BB.textMuted }}>
                Scroll to pan | Ctrl+Scroll to zoom | Drag to select
              </div>
            </div>
          </div>
        </div>
      </div>
    );
  };

  return (
    <div className="h-full flex flex-col" style={{ backgroundColor: BB.black }}>
      {renderHeader()}
      <div className="flex-1 overflow-hidden">
        {currentStep === 'data' && renderDataStep()}
        {currentStep === 'analysis' && renderAnalysisStep()}
        {currentStep === 'results' && renderResultsStep()}
      </div>
    </div>
  );
}

export default CFAQuantPanel;
