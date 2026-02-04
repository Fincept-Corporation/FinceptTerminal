import React, { useState, useEffect, useCallback } from 'react';
import {
  Activity,
  TrendingUp,
  BarChart2,
  Calculator,
  RefreshCw,
  AlertCircle,
  CheckCircle2,
  ChevronDown,
  ChevronUp,
  LineChart,
  Percent,
  Search,
  Play,
  Download,
  Database,
  Layers,
  GitBranch,
  Sigma,
  Eye,
  Copy
} from 'lucide-react';
import { statsmodelsService, type StatsmodelsResult } from '@/services/aiQuantLab/statsmodelsService';
import { yfinanceService } from '@/services/markets/yfinanceService';

const FINCEPT = {
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
  TEAL: '#00C9A7', // Unique theme color for Statsmodels
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
};

type AnalysisCategory = 'time_series' | 'stationarity' | 'regression' | 'glm' | 'tests' | 'power' | 'multivariate' | 'nonparametric';
type DataSourceType = 'manual' | 'symbol';

interface AnalysisType {
  id: string;
  name: string;
  description: string;
  category: AnalysisCategory;
}

const analysisTypes: AnalysisType[] = [
  { id: 'arima', name: 'ARIMA', description: 'Autoregressive Integrated Moving Average', category: 'time_series' },
  { id: 'arima_forecast', name: 'ARIMA Forecast', description: 'Forecast with ARIMA model', category: 'time_series' },
  { id: 'sarimax', name: 'SARIMAX', description: 'Seasonal ARIMA with exogenous variables', category: 'time_series' },
  { id: 'exponential_smoothing', name: 'Exp Smoothing', description: 'Holt-Winters exponential smoothing', category: 'time_series' },
  { id: 'stl_decompose', name: 'STL Decompose', description: 'Seasonal-Trend decomposition', category: 'time_series' },
  { id: 'adf_test', name: 'ADF Test', description: 'Augmented Dickey-Fuller stationarity test', category: 'stationarity' },
  { id: 'kpss_test', name: 'KPSS Test', description: 'KPSS stationarity test', category: 'stationarity' },
  { id: 'acf', name: 'ACF', description: 'Autocorrelation function', category: 'stationarity' },
  { id: 'pacf', name: 'PACF', description: 'Partial autocorrelation function', category: 'stationarity' },
  { id: 'ljung_box', name: 'Ljung-Box', description: 'Test for autocorrelation', category: 'stationarity' },
  { id: 'ols', name: 'OLS', description: 'Ordinary Least Squares regression', category: 'regression' },
  { id: 'regression_diagnostics', name: 'Diagnostics', description: 'Regression diagnostics', category: 'regression' },
  { id: 'logit', name: 'Logit', description: 'Logistic regression', category: 'glm' },
  { id: 'ttest_ind', name: 'T-Test (Ind)', description: 'Independent samples t-test', category: 'tests' },
  { id: 'ttest_1samp', name: 'T-Test (1 Sample)', description: 'One-sample t-test', category: 'tests' },
  { id: 'jarque_bera', name: 'Jarque-Bera', description: 'Normality test', category: 'tests' },
  { id: 'descriptive', name: 'Descriptive Stats', description: 'Descriptive statistics', category: 'tests' },
  { id: 'ttest_power', name: 'T-Test Power', description: 'Power analysis for t-test', category: 'power' },
  { id: 'pca', name: 'PCA', description: 'Principal Component Analysis', category: 'multivariate' },
  { id: 'kde', name: 'KDE', description: 'Kernel Density Estimation', category: 'nonparametric' },
  { id: 'lowess', name: 'LOWESS', description: 'Locally weighted regression', category: 'nonparametric' },
];

const categoryLabels: Record<AnalysisCategory, { label: string; icon: React.ElementType }> = {
  time_series: { label: 'Time Series', icon: TrendingUp },
  stationarity: { label: 'Stationarity Tests', icon: Activity },
  regression: { label: 'Regression', icon: LineChart },
  glm: { label: 'GLM Models', icon: GitBranch },
  tests: { label: 'Statistical Tests', icon: Calculator },
  power: { label: 'Power Analysis', icon: Percent },
  multivariate: { label: 'Multivariate', icon: Layers },
  nonparametric: { label: 'Nonparametric', icon: Sigma },
};

interface ParamField {
  key: string;
  label: string;
  placeholder: string;
  type: 'text' | 'number' | 'select';
  options?: { value: string; label: string }[];
}

const getParamFields = (analysisId: string): ParamField[] => {
  const paramMap: Record<string, ParamField[]> = {
    arima: [
      { key: 'order', label: 'Order (p,d,q)', placeholder: '[1,1,1]', type: 'text' },
    ],
    arima_forecast: [
      { key: 'order', label: 'Order (p,d,q)', placeholder: '[1,1,1]', type: 'text' },
      { key: 'steps', label: 'Forecast Steps', placeholder: '10', type: 'number' },
    ],
    sarimax: [
      { key: 'order', label: 'Order (p,d,q)', placeholder: '[1,1,1]', type: 'text' },
      { key: 'seasonal_order', label: 'Seasonal (P,D,Q,s)', placeholder: '[1,1,1,12]', type: 'text' },
    ],
    exponential_smoothing: [
      { key: 'trend', label: 'Trend', placeholder: 'add', type: 'select', options: [{ value: '', label: 'None' }, { value: 'add', label: 'Additive' }, { value: 'mul', label: 'Multiplicative' }] },
      { key: 'seasonal', label: 'Seasonal', placeholder: 'add', type: 'select', options: [{ value: '', label: 'None' }, { value: 'add', label: 'Additive' }, { value: 'mul', label: 'Multiplicative' }] },
      { key: 'seasonal_periods', label: 'Seasonal Periods', placeholder: '12', type: 'number' },
    ],
    stl_decompose: [
      { key: 'period', label: 'Period', placeholder: '12', type: 'number' },
      { key: 'seasonal', label: 'Seasonal Window', placeholder: '7', type: 'number' },
    ],
    acf: [
      { key: 'nlags', label: 'Number of Lags', placeholder: '40', type: 'number' },
    ],
    pacf: [
      { key: 'nlags', label: 'Number of Lags', placeholder: '40', type: 'number' },
    ],
    ljung_box: [
      { key: 'lags', label: 'Lags', placeholder: '10', type: 'number' },
    ],
    ttest_1samp: [
      { key: 'popmean', label: 'Population Mean', placeholder: '0', type: 'number' },
    ],
    ttest_power: [
      { key: 'effect_size', label: 'Effect Size', placeholder: '0.5', type: 'number' },
      { key: 'alpha', label: 'Alpha', placeholder: '0.05', type: 'number' },
      { key: 'power', label: 'Power (optional)', placeholder: '0.8', type: 'number' },
      { key: 'nobs', label: 'Sample Size (optional)', placeholder: '', type: 'number' },
    ],
    pca: [
      { key: 'n_components', label: 'Components', placeholder: '2', type: 'number' },
    ],
    lowess: [
      { key: 'frac', label: 'Fraction', placeholder: '0.3', type: 'number' },
    ],
  };
  return paramMap[analysisId] || [];
};

export function StatsmodelsPanel() {
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [analysisResult, setAnalysisResult] = useState<StatsmodelsResult<Record<string, unknown>> | null>(null);

  const [dataSourceType, setDataSourceType] = useState<DataSourceType>('symbol');
  const [symbolInput, setSymbolInput] = useState('AAPL');
  const [historicalDays, setHistoricalDays] = useState(365);
  const [manualData, setManualData] = useState('100, 102, 101, 105, 103, 108, 107, 112, 110, 115');
  const [priceData, setPriceData] = useState<number[]>([]);
  const [priceDataLoading, setPriceDataLoading] = useState(false);

  const [selectedCategory, setSelectedCategory] = useState<AnalysisCategory>('time_series');
  const [selectedAnalysis, setSelectedAnalysis] = useState<string>('arima');
  const [analysisParams, setAnalysisParams] = useState<Record<string, string>>({});
  const [expandedArrays, setExpandedArrays] = useState<Set<string>>(new Set());

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
        setPriceData(closes);
      } else {
        setError('No data returned for symbol');
      }
    } catch (err) {
      setError(`Failed to fetch data: ${err}`);
    } finally {
      setPriceDataLoading(false);
    }
  }, [symbolInput, historicalDays]);

  useEffect(() => {
    if (dataSourceType === 'symbol' && symbolInput) {
      fetchSymbolData();
    }
  }, []);

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
      if (data.length === 0) {
        setError('No data available. Please fetch symbol data or enter manual data.');
        setIsLoading(false);
        return;
      }

      const params: Record<string, unknown> = { data };

      Object.entries(analysisParams).forEach(([key, value]) => {
        if (value) {
          if (key === 'order' || key === 'seasonal_order') {
            try {
              params[key] = JSON.parse(value);
            } catch {
              params[key] = value;
            }
          } else if (['steps', 'nlags', 'lags', 'period', 'seasonal', 'seasonal_periods', 'n_components', 'nobs'].includes(key)) {
            params[key] = parseInt(value);
          } else if (['popmean', 'effect_size', 'alpha', 'power', 'frac'].includes(key)) {
            params[key] = parseFloat(value);
          } else {
            params[key] = value || null;
          }
        }
      });

      const result = await statsmodelsService.executeCustomCommand(selectedAnalysis, params);
      setAnalysisResult(result as StatsmodelsResult<Record<string, unknown>>);

      if (!result.success && result.error) {
        setError(result.error);
      }
    } catch (err) {
      setError(`Analysis failed: ${err}`);
    } finally {
      setIsLoading(false);
    }
  };

  const formatValue = (val: unknown, key?: string): string => {
    if (val === null || val === undefined) return 'N/A';
    if (typeof val === 'number') {
      if (Number.isInteger(val)) return val.toString();
      return val.toFixed(6);
    }
    if (Array.isArray(val)) {
      const isExpanded = key && expandedArrays.has(key);
      if (val.length > 10 && !isExpanded) {
        return `[${val.slice(0, 5).map(v => typeof v === 'number' ? v.toFixed(4) : v).join(', ')}, ... (${val.length} items)]`;
      }
      return `[${val.map(v => typeof v === 'number' ? v.toFixed(4) : v).join(', ')}]`;
    }
    return String(val);
  };

  const toggleArrayExpand = (key: string) => {
    setExpandedArrays(prev => {
      const newSet = new Set(prev);
      if (newSet.has(key)) {
        newSet.delete(key);
      } else {
        newSet.add(key);
      }
      return newSet;
    });
  };

  const exportData = (key: string, value: unknown) => {
    let content: string;
    let filename: string;

    if (Array.isArray(value)) {
      // Export as CSV
      content = value.map((v, i) => `${i},${typeof v === 'number' ? v : JSON.stringify(v)}`).join('\n');
      content = 'index,value\n' + content;
      filename = `${selectedAnalysis}_${key}.csv`;
    } else {
      // Export as JSON
      content = JSON.stringify({ [key]: value }, null, 2);
      filename = `${selectedAnalysis}_${key}.json`;
    }

    const blob = new Blob([content], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    a.click();
    URL.revokeObjectURL(url);
  };

  const exportAllResults = () => {
    if (!analysisResult) return;
    const content = JSON.stringify(analysisResult, null, 2);
    const blob = new Blob([content], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `${selectedAnalysis}_results.json`;
    a.click();
    URL.revokeObjectURL(url);
  };

  const copyToClipboard = (value: unknown) => {
    const text = Array.isArray(value)
      ? value.map(v => typeof v === 'number' ? v.toFixed(6) : v).join('\n')
      : JSON.stringify(value, null, 2);
    navigator.clipboard.writeText(text);
  };

  const renderResult = () => {
    if (!analysisResult) return null;

    return (
      <div
        className="border rounded p-4 mt-4"
        style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.BORDER }}
      >
        <div className="flex items-center gap-2 mb-3">
          {analysisResult.success ? (
            <CheckCircle2 size={16} color={FINCEPT.GREEN} />
          ) : (
            <AlertCircle size={16} color={FINCEPT.RED} />
          )}
          <span
            className="text-xs font-semibold"
            style={{ color: analysisResult.success ? FINCEPT.GREEN : FINCEPT.RED }}
          >
            {analysisResult.success ? 'ANALYSIS COMPLETE' : 'ANALYSIS FAILED'}
          </span>
          {analysisResult.analysis_type && (
            <span className="text-xs" style={{ color: FINCEPT.GRAY }}>
              - {analysisResult.analysis_type.toUpperCase()}
            </span>
          )}
        </div>

        {analysisResult.error && (
          <div className="text-xs p-2 rounded mb-3" style={{ backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.RED }}>
            {analysisResult.error}
            {analysisResult.message && (
              <div className="mt-1" style={{ color: FINCEPT.GRAY }}>{analysisResult.message}</div>
            )}
            {analysisResult.suggestions && Array.isArray(analysisResult.suggestions) && (
              <div className="mt-2" style={{ color: FINCEPT.ORANGE }}>
                <div className="font-semibold mb-1">Suggestions:</div>
                <ul className="list-disc list-inside">
                  {analysisResult.suggestions.map((s: string, i: number) => (
                    <li key={i}>{s}</li>
                  ))}
                </ul>
              </div>
            )}
          </div>
        )}

        {analysisResult.result && (
          <div className="space-y-2">
            {Object.entries(analysisResult.result).map(([key, value]) => {
              const isArray = Array.isArray(value);
              const isLargeArray = isArray && value.length > 10;
              const isExpanded = expandedArrays.has(key);

              return (
                <div key={key} className="py-2 border-b" style={{ borderColor: FINCEPT.BORDER }}>
                  <div className="flex justify-between items-start gap-4">
                    <span className="text-xs font-mono" style={{ color: FINCEPT.ORANGE }}>{key}</span>
                    <div className="flex items-center gap-1">
                      {isLargeArray && (
                        <>
                          <button
                            onClick={() => toggleArrayExpand(key)}
                            className="p-1 rounded hover:bg-opacity-20 hover:bg-white"
                            title={isExpanded ? "Collapse" : "View All"}
                          >
                            {isExpanded ? (
                              <ChevronUp size={12} color={FINCEPT.GRAY} />
                            ) : (
                              <Eye size={12} color={FINCEPT.GRAY} />
                            )}
                          </button>
                          <button
                            onClick={() => copyToClipboard(value)}
                            className="p-1 rounded hover:bg-opacity-20 hover:bg-white"
                            title="Copy to Clipboard"
                          >
                            <Copy size={12} color={FINCEPT.GRAY} />
                          </button>
                          <button
                            onClick={() => exportData(key, value)}
                            className="p-1 rounded hover:bg-opacity-20 hover:bg-white"
                            title="Export as CSV"
                          >
                            <Download size={12} color={FINCEPT.GRAY} />
                          </button>
                        </>
                      )}
                    </div>
                  </div>
                  <div
                    className="text-xs font-mono mt-1"
                    style={{
                      color: FINCEPT.WHITE,
                      maxHeight: isExpanded ? '300px' : 'none',
                      overflowY: isExpanded ? 'auto' : 'visible',
                      wordBreak: 'break-all'
                    }}
                  >
                    {formatValue(value, key)}
                  </div>
                </div>
              );
            })}
          </div>
        )}

        {analysisResult.result && (
          <div className="mt-3 flex justify-end">
            <button
              onClick={exportAllResults}
              className="flex items-center gap-1 px-3 py-1.5 text-xs font-semibold rounded"
              style={{ backgroundColor: FINCEPT.HEADER_BG, color: FINCEPT.ORANGE, border: `1px solid ${FINCEPT.BORDER}` }}
            >
              <Download size={12} />
              Export All Results
            </button>
          </div>
        )}

        {analysisResult.timestamp && (
          <div className="mt-3 text-xs" style={{ color: FINCEPT.MUTED }}>
            Timestamp: {analysisResult.timestamp}
          </div>
        )}
      </div>
    );
  };

  return (
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: FINCEPT.DARK_BG
    }}>
      {/* Terminal-style Header */}
      <div style={{
        padding: '12px 16px',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        backgroundColor: FINCEPT.PANEL_BG,
        display: 'flex',
        alignItems: 'center',
        gap: '12px'
      }}>
        <Activity size={16} color={FINCEPT.TEAL} />
        <span style={{
          color: FINCEPT.TEAL,
          fontSize: '12px',
          fontWeight: 700,
          letterSpacing: '0.5px',
          fontFamily: 'monospace'
        }}>
          STATSMODELS ANALYTICS
        </span>
        <div style={{ flex: 1 }} />
        <div style={{
          fontSize: '10px',
          fontFamily: 'monospace',
          padding: '3px 8px',
          backgroundColor: analysisResult ? FINCEPT.GREEN + '20' : FINCEPT.TEAL + '20',
          border: `1px solid ${analysisResult ? FINCEPT.GREEN : FINCEPT.TEAL}`,
          color: analysisResult ? FINCEPT.GREEN : FINCEPT.TEAL
        }}>
          {analysisResult ? 'RESULTS READY' : 'READY'}
        </div>
      </div>

      <div style={{ display: 'flex', flex: 1, minHeight: 0 }}>
        {/* Left Sidebar - Configuration */}
        <div style={{
          width: '320px',
          borderRight: `1px solid ${FINCEPT.BORDER}`,
          backgroundColor: FINCEPT.PANEL_BG,
          display: 'flex',
          flexDirection: 'column'
        }}>
          <div style={{ flex: 1, overflowY: 'auto' }}>
            {/* Data Source Section */}
            <div style={{
              padding: '10px 12px',
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              fontSize: '10px',
              fontWeight: 700,
              color: FINCEPT.TEAL,
              fontFamily: 'monospace',
              letterSpacing: '0.5px'
            }}>
              DATA SOURCE
            </div>

            <div style={{ padding: '12px' }}>
              <div style={{ display: 'flex', gap: '0', marginBottom: '12px' }}>
                <button
                  onClick={() => setDataSourceType('symbol')}
                  style={{
                    flex: 1,
                    padding: '8px 14px',
                    backgroundColor: dataSourceType === 'symbol' ? FINCEPT.TEAL : FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    color: dataSourceType === 'symbol' ? '#000000' : FINCEPT.WHITE,
                    opacity: dataSourceType === 'symbol' ? 1 : 0.7,
                    fontSize: '10px',
                    fontWeight: 700,
                    fontFamily: 'monospace',
                    letterSpacing: '0.5px',
                    cursor: 'pointer',
                    transition: 'all 0.15s'
                  }}
                >
                  SYMBOL
                </button>
                <button
                  onClick={() => setDataSourceType('manual')}
                  style={{
                    flex: 1,
                    padding: '8px 14px',
                    backgroundColor: dataSourceType === 'manual' ? FINCEPT.TEAL : FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderLeft: '0',
                    marginLeft: '-1px',
                    color: dataSourceType === 'manual' ? '#000000' : FINCEPT.WHITE,
                    opacity: dataSourceType === 'manual' ? 1 : 0.7,
                    fontSize: '10px',
                    fontWeight: 700,
                    fontFamily: 'monospace',
                    letterSpacing: '0.5px',
                    cursor: 'pointer',
                    transition: 'all 0.15s'
                  }}
                >
                  MANUAL
                </button>
              </div>

              {dataSourceType === 'symbol' ? (
                <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
                  <div style={{ display: 'flex', gap: '8px' }}>
                    <input
                      type="text"
                      value={symbolInput}
                      onChange={(e) => setSymbolInput(e.target.value.toUpperCase())}
                      placeholder="AAPL"
                      style={{
                        flex: 1,
                        padding: '8px 10px',
                        fontSize: '11px',
                        fontFamily: 'monospace',
                        backgroundColor: FINCEPT.DARK_BG,
                        color: FINCEPT.WHITE,
                        border: `1px solid ${FINCEPT.BORDER}`,
                        outline: 'none'
                      }}
                    />
                    <button
                      onClick={fetchSymbolData}
                      disabled={priceDataLoading}
                      style={{
                        padding: '8px 12px',
                        backgroundColor: FINCEPT.TEAL,
                        border: 'none',
                        color: '#000000',
                        cursor: priceDataLoading ? 'not-allowed' : 'pointer',
                        opacity: priceDataLoading ? 0.5 : 1,
                        display: 'flex',
                        alignItems: 'center',
                        justifyContent: 'center'
                      }}
                    >
                      {priceDataLoading ? <RefreshCw size={12} className="animate-spin" /> : <Search size={12} />}
                    </button>
                  </div>
                  <div>
                    <label style={{
                      display: 'block',
                      fontSize: '9px',
                      marginBottom: '6px',
                      color: FINCEPT.WHITE,
                      fontFamily: 'monospace',
                      letterSpacing: '0.5px',
                      opacity: 0.7
                    }}>
                      HISTORICAL DAYS
                    </label>
                    <select
                      value={historicalDays}
                      onChange={(e) => setHistoricalDays(parseInt(e.target.value))}
                      style={{
                        width: '100%',
                        padding: '8px 10px',
                        fontSize: '11px',
                        fontFamily: 'monospace',
                        backgroundColor: FINCEPT.DARK_BG,
                        color: FINCEPT.WHITE,
                        border: `1px solid ${FINCEPT.BORDER}`,
                        outline: 'none'
                      }}
                    >
                      <option value={30}>30 Days</option>
                      <option value={90}>90 Days</option>
                      <option value={180}>180 Days</option>
                      <option value={365}>1 Year</option>
                      <option value={730}>2 Years</option>
                      <option value={1825}>5 Years</option>
                    </select>
                  </div>
                  {priceData.length > 0 && (
                    <div style={{
                      fontSize: '10px',
                      fontFamily: 'monospace',
                      color: FINCEPT.GREEN,
                      padding: '6px 10px',
                      backgroundColor: FINCEPT.GREEN + '20',
                      border: `1px solid ${FINCEPT.GREEN}`
                    }}>
                      Loaded {priceData.length} data points
                    </div>
                  )}
                </div>
              ) : (
                <textarea
                  value={manualData}
                  onChange={(e) => setManualData(e.target.value)}
                  placeholder="Enter comma-separated values..."
                  rows={4}
                  style={{
                    width: '100%',
                    padding: '8px 10px',
                    fontSize: '11px',
                    fontFamily: 'monospace',
                    backgroundColor: FINCEPT.DARK_BG,
                    color: FINCEPT.WHITE,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    outline: 'none',
                    resize: 'vertical'
                  }}
                />
              )}
            </div>

            {/* Category Section */}
            <div style={{
              padding: '10px 12px',
              borderTop: `1px solid ${FINCEPT.BORDER}`,
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              fontSize: '10px',
              fontWeight: 700,
              color: FINCEPT.TEAL,
              fontFamily: 'monospace',
              letterSpacing: '0.5px'
            }}>
              CATEGORY
            </div>
            <div style={{ padding: '12px' }}>
              <select
                value={selectedCategory}
                onChange={(e) => {
                  const cat = e.target.value as AnalysisCategory;
                  setSelectedCategory(cat);
                  const first = analysisTypes.find(a => a.category === cat);
                  if (first) {
                    setSelectedAnalysis(first.id);
                    setAnalysisParams({});
                  }
                }}
                style={{
                  width: '100%',
                  padding: '8px 10px',
                  fontSize: '11px',
                  fontFamily: 'monospace',
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  outline: 'none'
                }}
              >
                {Object.entries(categoryLabels).map(([key, { label }]) => (
                  <option key={key} value={key}>{label}</option>
                ))}
              </select>
            </div>

            {/* Analysis Type Section */}
            <div style={{
              padding: '10px 12px',
              borderTop: `1px solid ${FINCEPT.BORDER}`,
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              fontSize: '10px',
              fontWeight: 700,
              color: FINCEPT.TEAL,
              fontFamily: 'monospace',
              letterSpacing: '0.5px'
            }}>
              ANALYSIS TYPE
            </div>
            <div style={{ padding: '12px' }}>
              <select
                value={selectedAnalysis}
                onChange={(e) => {
                  setSelectedAnalysis(e.target.value);
                  setAnalysisParams({});
                }}
                style={{
                  width: '100%',
                  padding: '8px 10px',
                  fontSize: '11px',
                  fontFamily: 'monospace',
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  outline: 'none',
                  marginBottom: '10px'
                }}
              >
                {analysisTypes
                  .filter(a => a.category === selectedCategory)
                  .map(a => (
                    <option key={a.id} value={a.id}>{a.name}</option>
                  ))}
              </select>
              <div style={{
                fontSize: '10px',
                color: FINCEPT.WHITE,
                opacity: 0.6,
                lineHeight: '1.5',
                fontFamily: 'monospace'
              }}>
                {analysisTypes.find(a => a.id === selectedAnalysis)?.description}
              </div>
              {selectedAnalysis === 'logit' && (
                <div style={{
                  marginTop: '10px',
                  padding: '10px',
                  fontSize: '10px',
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.TEAL}`,
                  borderLeft: `3px solid ${FINCEPT.TEAL}`,
                  color: FINCEPT.WHITE,
                  lineHeight: '1.5',
                  fontFamily: 'monospace'
                }}>
                  <strong>Note:</strong> Logit models binary outcomes (0/1). Price data will be auto-converted to returns direction.
                </div>
              )}
            </div>

            {/* Parameters Section */}
            {getParamFields(selectedAnalysis).length > 0 && (
              <>
                <div style={{
                  padding: '10px 12px',
                  borderTop: `1px solid ${FINCEPT.BORDER}`,
                  borderBottom: `1px solid ${FINCEPT.BORDER}`,
                  fontSize: '10px',
                  fontWeight: 700,
                  color: FINCEPT.TEAL,
                  fontFamily: 'monospace',
                  letterSpacing: '0.5px'
                }}>
                  PARAMETERS
                </div>
                <div style={{ padding: '12px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
                  {getParamFields(selectedAnalysis).map(field => (
                    <div key={field.key}>
                      <label style={{
                        display: 'block',
                        fontSize: '9px',
                        marginBottom: '6px',
                        color: FINCEPT.WHITE,
                        fontFamily: 'monospace',
                        letterSpacing: '0.5px',
                        opacity: 0.7
                      }}>
                        {field.label}
                      </label>
                      {field.type === 'select' ? (
                        <select
                          value={analysisParams[field.key] || ''}
                          onChange={(e) => setAnalysisParams(prev => ({ ...prev, [field.key]: e.target.value }))}
                          style={{
                            width: '100%',
                            padding: '8px 10px',
                            fontSize: '11px',
                            fontFamily: 'monospace',
                            backgroundColor: FINCEPT.DARK_BG,
                            color: FINCEPT.WHITE,
                            border: `1px solid ${FINCEPT.BORDER}`,
                            outline: 'none'
                          }}
                        >
                          {field.options?.map(opt => (
                            <option key={opt.value} value={opt.value}>{opt.label}</option>
                          ))}
                        </select>
                      ) : (
                        <input
                          type={field.type}
                          value={analysisParams[field.key] || ''}
                          onChange={(e) => setAnalysisParams(prev => ({ ...prev, [field.key]: e.target.value }))}
                          placeholder={field.placeholder}
                          style={{
                            width: '100%',
                            padding: '8px 10px',
                            fontSize: '11px',
                            fontFamily: 'monospace',
                            backgroundColor: FINCEPT.DARK_BG,
                            color: FINCEPT.WHITE,
                            border: `1px solid ${FINCEPT.BORDER}`,
                            outline: 'none'
                          }}
                        />
                      )}
                    </div>
                  ))}
                </div>
              </>
            )}
          </div>

          {/* Run Button */}
          <div style={{ borderTop: `1px solid ${FINCEPT.BORDER}`, padding: '12px' }}>
            <button
              onClick={runAnalysis}
              disabled={isLoading}
              style={{
                width: '100%',
                padding: '12px 16px',
                backgroundColor: FINCEPT.TEAL,
                border: 'none',
                color: '#000000',
                fontSize: '11px',
                fontWeight: 700,
                fontFamily: 'monospace',
                cursor: isLoading ? 'not-allowed' : 'pointer',
                opacity: isLoading ? 0.6 : 1,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '8px',
                letterSpacing: '0.5px',
                transition: 'all 0.15s'
              }}
            >
              {isLoading ? (
                <>
                  <RefreshCw size={14} className="animate-spin" />
                  RUNNING...
                </>
              ) : (
                <>
                  <Play size={14} />
                  RUN ANALYSIS
                </>
              )}
            </button>
          </div>
        </div>

        {/* Right Panel - Main Content */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', minWidth: 0 }}>
          <div style={{
            padding: '10px 16px',
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            fontSize: '10px',
            fontWeight: 700,
            color: FINCEPT.TEAL,
            fontFamily: 'monospace',
            letterSpacing: '0.5px',
            backgroundColor: FINCEPT.PANEL_BG
          }}>
            STATISTICAL MODELING & ECONOMETRIC ANALYSIS
          </div>

          <div style={{ flex: 1, overflowY: 'auto', padding: '16px' }}>
            {/* Category Grid */}
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '12px', marginBottom: '20px' }}>
              {Object.entries(categoryLabels).map(([key, { label, icon: Icon }]) => {
                const count = analysisTypes.filter(a => a.category === key).length;
                const isSelected = selectedCategory === key;
                return (
                  <button
                    key={key}
                    onClick={() => {
                      setSelectedCategory(key as AnalysisCategory);
                      const first = analysisTypes.find(a => a.category === key);
                      if (first) {
                        setSelectedAnalysis(first.id);
                        setAnalysisParams({});
                      }
                    }}
                    style={{
                      padding: '12px',
                      backgroundColor: isSelected ? FINCEPT.HOVER : FINCEPT.PANEL_BG,
                      border: `1px solid ${isSelected ? FINCEPT.TEAL : FINCEPT.BORDER}`,
                      cursor: 'pointer',
                      textAlign: 'left',
                      transition: 'all 0.15s'
                    }}
                    onMouseEnter={(e) => {
                      if (!isSelected) {
                        e.currentTarget.style.backgroundColor = FINCEPT.DARK_BG;
                        e.currentTarget.style.borderColor = FINCEPT.TEAL;
                      }
                    }}
                    onMouseLeave={(e) => {
                      if (!isSelected) {
                        e.currentTarget.style.backgroundColor = FINCEPT.PANEL_BG;
                        e.currentTarget.style.borderColor = FINCEPT.BORDER;
                      }
                    }}
                  >
                    <Icon size={16} color={FINCEPT.TEAL} />
                    <div style={{
                      fontSize: '11px',
                      fontWeight: 600,
                      marginTop: '8px',
                      color: FINCEPT.WHITE,
                      fontFamily: 'monospace'
                    }}>
                      {label}
                    </div>
                    <div style={{
                      fontSize: '9px',
                      color: FINCEPT.WHITE,
                      opacity: 0.5,
                      marginTop: '4px',
                      fontFamily: 'monospace'
                    }}>
                      {count} analyses
                    </div>
                  </button>
                );
              })}
            </div>

            {/* Available Analysis Types */}
            <div style={{ marginBottom: '20px' }}>
              <div style={{
                fontSize: '10px',
                fontWeight: 700,
                marginBottom: '12px',
                color: FINCEPT.TEAL,
                letterSpacing: '0.5px',
                fontFamily: 'monospace'
              }}>
                AVAILABLE IN {categoryLabels[selectedCategory].label.toUpperCase()}
              </div>
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '10px' }}>
                {analysisTypes
                  .filter(a => a.category === selectedCategory)
                  .map(a => {
                    const isSelected = selectedAnalysis === a.id;
                    return (
                      <button
                        key={a.id}
                        onClick={() => {
                          setSelectedAnalysis(a.id);
                          setAnalysisParams({});
                        }}
                        style={{
                          padding: '10px',
                          backgroundColor: isSelected ? FINCEPT.HOVER : FINCEPT.PANEL_BG,
                          border: `1px solid ${isSelected ? FINCEPT.TEAL : FINCEPT.BORDER}`,
                          cursor: 'pointer',
                          textAlign: 'left',
                          transition: 'all 0.15s'
                        }}
                        onMouseEnter={(e) => {
                          if (!isSelected) {
                            e.currentTarget.style.backgroundColor = FINCEPT.DARK_BG;
                            e.currentTarget.style.borderColor = FINCEPT.TEAL;
                          }
                        }}
                        onMouseLeave={(e) => {
                          if (!isSelected) {
                            e.currentTarget.style.backgroundColor = FINCEPT.PANEL_BG;
                            e.currentTarget.style.borderColor = FINCEPT.BORDER;
                          }
                        }}
                      >
                        <div style={{
                          fontSize: '11px',
                          fontWeight: 600,
                          color: isSelected ? FINCEPT.TEAL : FINCEPT.WHITE,
                          fontFamily: 'monospace',
                          marginBottom: '4px'
                        }}>
                          {a.name}
                        </div>
                        <div style={{
                          fontSize: '10px',
                          color: FINCEPT.WHITE,
                          opacity: 0.6,
                          lineHeight: '1.4',
                          fontFamily: 'monospace'
                        }}>
                          {a.description}
                        </div>
                      </button>
                    );
                  })}
              </div>
            </div>

            {/* Error Display */}
            {error && (
              <div style={{
                padding: '12px 14px',
                backgroundColor: FINCEPT.RED + '15',
                border: `1px solid ${FINCEPT.RED}`,
                borderLeft: `3px solid ${FINCEPT.RED}`,
                color: FINCEPT.RED,
                fontSize: '11px',
                display: 'flex',
                alignItems: 'center',
                gap: '10px',
                fontFamily: 'monospace',
                marginBottom: '16px',
                lineHeight: '1.5'
              }}>
                <AlertCircle size={16} style={{ flexShrink: 0 }} />
                {error}
              </div>
            )}

            {/* Results */}
            {renderResult()}

            {/* Empty State */}
            {!analysisResult && !error && (
              <div style={{
                padding: '40px 20px',
                textAlign: 'center',
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`
              }}>
                <Database size={48} color={FINCEPT.MUTED} style={{ margin: '0 auto 16px' }} />
                <div style={{
                  fontSize: '11px',
                  color: FINCEPT.WHITE,
                  marginBottom: '8px',
                  fontFamily: 'monospace'
                }}>
                  Select an analysis and click RUN ANALYSIS to begin
                </div>
                <div style={{
                  fontSize: '10px',
                  color: FINCEPT.WHITE,
                  opacity: 0.5,
                  fontFamily: 'monospace'
                }}>
                  {getDataArray().length > 0
                    ? `${getDataArray().length} data points ready`
                    : 'No data loaded - fetch symbol data or enter manual values'}
                </div>
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}

export default StatsmodelsPanel;
