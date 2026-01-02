/**
 * Functime Analytics Panel - Time Series Forecasting
 * Bloomberg Professional Design
 * Integrated with functime library via PyO3
 */

import React, { useState, useEffect, useMemo } from 'react';
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
  Database
} from 'lucide-react';
import {
  functimeService,
  type ForecastResult,
  type ForecastConfig,
  type PreprocessConfig,
  type FullPipelineResponse
} from '@/services/aiQuantLab/functimeService';

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

export function FunctimePanel() {
  const [isFunctimeAvailable, setIsFunctimeAvailable] = useState<boolean | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [analysisResult, setAnalysisResult] = useState<ForecastAnalysisResult | null>(null);
  const [dataInput, setDataInput] = useState(JSON.stringify(SAMPLE_DATA, null, 2));

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
    chart: true
  });

  const models = functimeService.getAvailableModels();
  const frequencies = functimeService.getAvailableFrequencies();

  // Check functime availability on mount
  useEffect(() => {
    checkFunctimeStatus();
  }, []);

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
          {/* Data Input */}
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

          {/* Model Configuration */}
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
        </div>

        {/* Run Button */}
        <div className="p-4 border-t" style={{ borderColor: BLOOMBERG.BORDER }}>
          <button
            onClick={runForecast}
            disabled={isLoading}
            className="w-full py-3 rounded font-bold text-sm uppercase flex items-center justify-center gap-2 disabled:opacity-50"
            style={{ backgroundColor: BLOOMBERG.ORANGE, color: BLOOMBERG.DARK_BG }}
          >
            {isLoading ? (
              <>
                <RefreshCw size={16} className="animate-spin" />
                FORECASTING...
              </>
            ) : (
              <>
                <Play size={16} />
                RUN FORECAST
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

        {!analysisResult && !error && (
          <div className="flex items-center justify-center h-full">
            <div className="text-center">
              <LineChart size={64} color={BLOOMBERG.MUTED} className="mx-auto mb-4" />
              <p className="text-sm font-mono" style={{ color: BLOOMBERG.GRAY }}>
                Configure model and click "Run Forecast" to see predictions
              </p>
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
                        {analysisResult.forecast.slice(0, 20).map((row, idx) => (
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
                    {analysisResult.forecast.length > 20 && (
                      <div
                        className="px-3 py-2 text-xs font-mono text-center"
                        style={{ backgroundColor: BLOOMBERG.HEADER_BG, color: BLOOMBERG.GRAY }}
                      >
                        + {analysisResult.forecast.length - 20} more rows
                      </div>
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

// Forecast Chart Component - SVG-based line chart
interface ChartDataPoint {
  time: string;
  value: number;
  type: 'historical' | 'forecast';
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
  const chartData = useMemo(() => {
    const historical: ChartDataPoint[] = historicalData.map(d => ({
      time: d.time,
      value: d.value,
      type: 'historical' as const
    }));

    const forecast: ChartDataPoint[] = forecastData.map(d => ({
      time: d.time,
      value: d.value,
      type: 'forecast' as const
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

  const width = 800;
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

  return (
    <div className="w-full overflow-x-auto">
      <svg
        width="100%"
        height={height}
        viewBox={`0 0 ${width} ${height}`}
        preserveAspectRatio="xMidYMid meet"
        style={{ backgroundColor: BLOOMBERG.DARK_BG }}
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

        {/* Historical data points */}
        {historicalData.map((d, i) => (
          <circle
            key={`hist-${i}`}
            cx={xScale(i)}
            cy={yScale(d.value)}
            r={3}
            fill={BLOOMBERG.CYAN}
          />
        ))}

        {/* Forecast data points */}
        {forecastData.map((d, i) => (
          <circle
            key={`forecast-${i}`}
            cx={xScale(historicalData.length + i)}
            cy={yScale(d.value)}
            r={4}
            fill={BLOOMBERG.ORANGE}
            stroke={BLOOMBERG.DARK_BG}
            strokeWidth={1}
          />
        ))}

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
    </div>
  );
}
