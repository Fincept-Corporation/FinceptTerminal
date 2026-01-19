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
    <div className="flex h-full" style={{ backgroundColor: FINCEPT.DARK_BG }}>
      <div
        className="w-80 flex-shrink-0 border-r overflow-y-auto"
        style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.BORDER }}
      >
        <div className="p-4 border-b" style={{ borderColor: FINCEPT.BORDER }}>
          <div className="flex items-center gap-2 mb-3">
            <Activity size={16} color={FINCEPT.ORANGE} />
            <span className="text-xs font-bold" style={{ color: FINCEPT.ORANGE }}>DATA SOURCE</span>
          </div>

          <div className="flex gap-2 mb-3">
            <button
              onClick={() => setDataSourceType('symbol')}
              className="flex-1 py-1.5 text-xs font-semibold rounded"
              style={{
                backgroundColor: dataSourceType === 'symbol' ? FINCEPT.ORANGE : FINCEPT.DARK_BG,
                color: dataSourceType === 'symbol' ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                border: `1px solid ${FINCEPT.BORDER}`
              }}
            >
              SYMBOL
            </button>
            <button
              onClick={() => setDataSourceType('manual')}
              className="flex-1 py-1.5 text-xs font-semibold rounded"
              style={{
                backgroundColor: dataSourceType === 'manual' ? FINCEPT.ORANGE : FINCEPT.DARK_BG,
                color: dataSourceType === 'manual' ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                border: `1px solid ${FINCEPT.BORDER}`
              }}
            >
              MANUAL
            </button>
          </div>

          {dataSourceType === 'symbol' ? (
            <div className="space-y-2">
              <div className="flex gap-2">
                <input
                  type="text"
                  value={symbolInput}
                  onChange={(e) => setSymbolInput(e.target.value.toUpperCase())}
                  placeholder="AAPL"
                  className="flex-1 px-2 py-1.5 text-xs font-mono rounded"
                  style={{
                    backgroundColor: FINCEPT.DARK_BG,
                    color: FINCEPT.WHITE,
                    border: `1px solid ${FINCEPT.BORDER}`
                  }}
                />
                <button
                  onClick={fetchSymbolData}
                  disabled={priceDataLoading}
                  className="px-3 py-1.5 rounded"
                  style={{ backgroundColor: FINCEPT.BLUE, color: FINCEPT.WHITE }}
                >
                  {priceDataLoading ? <RefreshCw size={12} className="animate-spin" /> : <Search size={12} />}
                </button>
              </div>
              <div className="flex items-center gap-2">
                <span className="text-xs" style={{ color: FINCEPT.GRAY }}>Days:</span>
                <select
                  value={historicalDays}
                  onChange={(e) => setHistoricalDays(parseInt(e.target.value))}
                  className="flex-1 px-2 py-1 text-xs rounded"
                  style={{
                    backgroundColor: FINCEPT.DARK_BG,
                    color: FINCEPT.WHITE,
                    border: `1px solid ${FINCEPT.BORDER}`
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
                <div className="text-xs" style={{ color: FINCEPT.GREEN }}>
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
              className="w-full px-2 py-1.5 text-xs font-mono rounded"
              style={{
                backgroundColor: FINCEPT.DARK_BG,
                color: FINCEPT.WHITE,
                border: `1px solid ${FINCEPT.BORDER}`
              }}
            />
          )}
        </div>

        <div className="p-4 border-b" style={{ borderColor: FINCEPT.BORDER }}>
          <div className="flex items-center gap-2 mb-3">
            <BarChart2 size={16} color={FINCEPT.ORANGE} />
            <span className="text-xs font-bold" style={{ color: FINCEPT.ORANGE }}>CATEGORY</span>
          </div>
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
            className="w-full px-2 py-1.5 text-xs rounded"
            style={{
              backgroundColor: FINCEPT.DARK_BG,
              color: FINCEPT.WHITE,
              border: `1px solid ${FINCEPT.BORDER}`
            }}
          >
            {Object.entries(categoryLabels).map(([key, { label }]) => (
              <option key={key} value={key}>{label}</option>
            ))}
          </select>
        </div>

        <div className="p-4 border-b" style={{ borderColor: FINCEPT.BORDER }}>
          <div className="flex items-center gap-2 mb-3">
            <Calculator size={16} color={FINCEPT.ORANGE} />
            <span className="text-xs font-bold" style={{ color: FINCEPT.ORANGE }}>ANALYSIS</span>
          </div>
          <select
            value={selectedAnalysis}
            onChange={(e) => {
              setSelectedAnalysis(e.target.value);
              setAnalysisParams({});
            }}
            className="w-full px-2 py-1.5 text-xs rounded"
            style={{
              backgroundColor: FINCEPT.DARK_BG,
              color: FINCEPT.WHITE,
              border: `1px solid ${FINCEPT.BORDER}`
            }}
          >
            {analysisTypes
              .filter(a => a.category === selectedCategory)
              .map(a => (
                <option key={a.id} value={a.id}>{a.name}</option>
              ))}
          </select>
          <div className="mt-2 text-xs" style={{ color: FINCEPT.GRAY }}>
            {analysisTypes.find(a => a.id === selectedAnalysis)?.description}
          </div>
          {selectedAnalysis === 'logit' && (
            <div className="mt-2 p-2 rounded text-xs" style={{ backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.ORANGE }}>
              <strong>Note:</strong> Logit models binary outcomes (0/1). Price data will be auto-converted to returns direction (up=1, down=0).
            </div>
          )}
        </div>

        {getParamFields(selectedAnalysis).length > 0 && (
          <div className="p-4 border-b" style={{ borderColor: FINCEPT.BORDER }}>
            <div className="flex items-center gap-2 mb-3">
              <ChevronDown size={16} color={FINCEPT.ORANGE} />
              <span className="text-xs font-bold" style={{ color: FINCEPT.ORANGE }}>PARAMETERS</span>
            </div>
            <div className="space-y-3">
              {getParamFields(selectedAnalysis).map(field => (
                <div key={field.key}>
                  <label className="text-xs block mb-1" style={{ color: FINCEPT.GRAY }}>{field.label}</label>
                  {field.type === 'select' ? (
                    <select
                      value={analysisParams[field.key] || ''}
                      onChange={(e) => setAnalysisParams(prev => ({ ...prev, [field.key]: e.target.value }))}
                      className="w-full px-2 py-1.5 text-xs rounded"
                      style={{
                        backgroundColor: FINCEPT.DARK_BG,
                        color: FINCEPT.WHITE,
                        border: `1px solid ${FINCEPT.BORDER}`
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
                      className="w-full px-2 py-1.5 text-xs font-mono rounded"
                      style={{
                        backgroundColor: FINCEPT.DARK_BG,
                        color: FINCEPT.WHITE,
                        border: `1px solid ${FINCEPT.BORDER}`
                      }}
                    />
                  )}
                </div>
              ))}
            </div>
          </div>
        )}

        <div className="p-4">
          <button
            onClick={runAnalysis}
            disabled={isLoading}
            className="w-full py-2.5 text-xs font-bold rounded flex items-center justify-center gap-2"
            style={{
              backgroundColor: isLoading ? FINCEPT.MUTED : FINCEPT.ORANGE,
              color: FINCEPT.DARK_BG
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

      <div className="flex-1 overflow-y-auto p-6">
        <div className="mb-6">
          <h2 className="text-lg font-bold mb-1" style={{ color: FINCEPT.WHITE }}>
            STATSMODELS ANALYTICS
          </h2>
          <p className="text-xs" style={{ color: FINCEPT.GRAY }}>
            Statistical modeling and econometric analysis powered by statsmodels library
          </p>
        </div>

        <div className="grid grid-cols-4 gap-3 mb-6">
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
                className="p-3 rounded text-left transition-all"
                style={{
                  backgroundColor: isSelected ? FINCEPT.ORANGE : FINCEPT.PANEL_BG,
                  border: `1px solid ${isSelected ? FINCEPT.ORANGE : FINCEPT.BORDER}`
                }}
              >
                <Icon size={16} color={isSelected ? FINCEPT.DARK_BG : FINCEPT.GRAY} />
                <div
                  className="text-xs font-semibold mt-2"
                  style={{ color: isSelected ? FINCEPT.DARK_BG : FINCEPT.WHITE }}
                >
                  {label}
                </div>
                <div
                  className="text-xs"
                  style={{ color: isSelected ? FINCEPT.DARK_BG : FINCEPT.MUTED }}
                >
                  {count} analyses
                </div>
              </button>
            );
          })}
        </div>

        <div className="mb-6">
          <div className="text-xs font-bold mb-3" style={{ color: FINCEPT.ORANGE }}>
            AVAILABLE IN {categoryLabels[selectedCategory].label.toUpperCase()}
          </div>
          <div className="grid grid-cols-3 gap-2">
            {analysisTypes
              .filter(a => a.category === selectedCategory)
              .map(a => (
                <button
                  key={a.id}
                  onClick={() => {
                    setSelectedAnalysis(a.id);
                    setAnalysisParams({});
                  }}
                  className="p-2.5 rounded text-left"
                  style={{
                    backgroundColor: selectedAnalysis === a.id ? FINCEPT.BLUE : FINCEPT.HEADER_BG,
                    border: `1px solid ${FINCEPT.BORDER}`
                  }}
                >
                  <div className="text-xs font-semibold" style={{ color: FINCEPT.WHITE }}>{a.name}</div>
                  <div className="text-xs mt-1" style={{ color: FINCEPT.GRAY }}>{a.description}</div>
                </button>
              ))}
          </div>
        </div>

        {error && (
          <div
            className="p-3 rounded mb-4 flex items-center gap-2"
            style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.RED}` }}
          >
            <AlertCircle size={16} color={FINCEPT.RED} />
            <span className="text-xs" style={{ color: FINCEPT.RED }}>{error}</span>
          </div>
        )}

        {renderResult()}

        {!analysisResult && !error && (
          <div
            className="p-8 rounded text-center"
            style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}` }}
          >
            <Database size={48} color={FINCEPT.MUTED} className="mx-auto mb-4" />
            <div className="text-sm" style={{ color: FINCEPT.GRAY }}>
              Select an analysis and click RUN ANALYSIS to begin
            </div>
            <div className="text-xs mt-2" style={{ color: FINCEPT.MUTED }}>
              {getDataArray().length > 0
                ? `${getDataArray().length} data points ready`
                : 'No data loaded - fetch symbol data or enter manual values'}
            </div>
          </div>
        )}
      </div>
    </div>
  );
}

export default StatsmodelsPanel;
