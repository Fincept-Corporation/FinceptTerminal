/**
 * GluonTSPanel - Deep Learning Time Series Forecasting
 * Amazon GluonTS: 9 DL models + 3 baselines + evaluation
 *
 * Redesigned UI with:
 * - Clear input labels with help text
 * - How to use guide panel
 * - Model descriptions and tips
 */

import React, { useState } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import {
  TrendingUp, Play, AlertCircle, BarChart3, Zap, Brain, HelpCircle, Info, RefreshCw
} from 'lucide-react';
import {
  ResponsiveContainer, LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, Area, AreaChart
} from 'recharts';

const GLUON_BLUE = '#00A1E4';

interface ForecastResult {
  mean: number[];
  quantiles: { '0.1': number[]; '0.5': number[]; '0.9': number[] };
  prediction_length: number;
  model: string;
}

interface EvalResult {
  aggregate_metrics: {
    MSE: number; RMSE: number; MAE: number;
    MAPE: number; sMAPE: number; MASE: number;
    mean_wQuantileLoss: number;
  };
  num_forecasts: number;
}

// Tooltip component for help text
function TooltipHelp({ text }: { text: string }) {
  const [show, setShow] = useState(false);
  const [position, setPosition] = useState({ top: 0, left: 0 });
  const triggerRef = React.useRef<HTMLDivElement>(null);
  const { colors } = useTerminalTheme();

  const handleMouseEnter = () => {
    if (triggerRef.current) {
      const rect = triggerRef.current.getBoundingClientRect();
      setPosition({
        top: rect.top - 10,
        left: rect.left + rect.width / 2,
      });
    }
    setShow(true);
  };

  return (
    <div
      ref={triggerRef}
      style={{ position: 'relative', display: 'inline-flex', alignItems: 'center' }}
      onMouseEnter={handleMouseEnter}
      onMouseLeave={() => setShow(false)}
    >
      <HelpCircle size={12} color={colors.textMuted} style={{ cursor: 'help' }} />
      {show && (
        <div
          style={{
            position: 'fixed',
            zIndex: 9999,
            padding: '10px 14px',
            backgroundColor: '#1a1a2e',
            border: `1px solid ${GLUON_BLUE}60`,
            borderRadius: '6px',
            color: colors.text,
            fontSize: '12px',
            lineHeight: 1.5,
            whiteSpace: 'normal',
            width: '260px',
            textAlign: 'left',
            boxShadow: '0 8px 24px rgba(0,0,0,0.5)',
            top: position.top,
            left: position.left,
            transform: 'translate(-50%, -100%)',
          }}
        >
          {text}
        </div>
      )}
    </div>
  );
}

// How to Use Panel
function HowToUsePanel() {
  const { colors } = useTerminalTheme();
  const [expanded, setExpanded] = useState(true);

  return (
    <div
      style={{
        backgroundColor: `${GLUON_BLUE}08`,
        border: `1px solid ${GLUON_BLUE}25`,
        borderRadius: '6px',
        marginBottom: '16px',
      }}
    >
      {/* Header */}
      <div
        style={{
          display: 'flex',
          alignItems: 'center',
          gap: '10px',
          padding: '12px 16px',
          backgroundColor: `${GLUON_BLUE}15`,
          cursor: 'pointer',
          borderBottom: expanded ? `1px solid ${GLUON_BLUE}20` : 'none',
        }}
        onClick={() => setExpanded(!expanded)}
      >
        <Info size={16} color={GLUON_BLUE} />
        <span style={{ color: colors.text, fontWeight: 600, fontSize: '13px', flex: 1 }}>
          GluonTS Time Series Forecasting
        </span>
        <span style={{ color: colors.textMuted, fontSize: '11px' }}>
          {expanded ? '▼ Hide guide' : '▶ How to use this'}
        </span>
      </div>

      {expanded && (
        <div style={{ padding: '16px', display: 'flex', flexDirection: 'column', gap: '16px', maxHeight: '300px', overflowY: 'scroll' }}>
          {/* What is it */}
          <div style={{ color: colors.text, fontSize: '12px', lineHeight: 1.5 }}>
            Forecast future values using Amazon's GluonTS deep learning library. Choose from 9 neural network models or 3 statistical baselines.
          </div>

          {/* Two columns */}
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
            {/* Where to get data */}
            <div style={{ backgroundColor: colors.background, border: '1px solid #333', borderRadius: '4px', padding: '12px' }}>
              <div style={{ color: GLUON_BLUE, fontSize: '11px', fontWeight: 600, marginBottom: '8px', textTransform: 'uppercase' }}>
                Where to Get the Data
              </div>
              <ol style={{ margin: 0, paddingLeft: '18px', color: colors.textMuted, fontSize: '11px', lineHeight: 1.6 }}>
                <li style={{ marginBottom: '4px' }}>Get historical prices from Yahoo Finance or your broker</li>
                <li style={{ marginBottom: '4px' }}>Use closing prices, sales figures, or any time series</li>
                <li style={{ marginBottom: '4px' }}>Need at least 10 data points (30+ recommended)</li>
                <li style={{ marginBottom: '4px' }}>Enter values separated by commas</li>
              </ol>
            </div>

            {/* What you will get */}
            <div style={{ backgroundColor: colors.background, border: '1px solid #333', borderRadius: '4px', padding: '12px' }}>
              <div style={{ color: GLUON_BLUE, fontSize: '11px', fontWeight: 600, marginBottom: '8px', textTransform: 'uppercase' }}>
                Output You Will Get
              </div>
              <ul style={{ margin: 0, paddingLeft: '18px', color: colors.textMuted, fontSize: '11px', lineHeight: 1.6 }}>
                <li style={{ marginBottom: '4px' }}>Mean forecast - Expected future values</li>
                <li style={{ marginBottom: '4px' }}>10-90% quantiles - Uncertainty range</li>
                <li style={{ marginBottom: '4px' }}>Interactive chart with historical + forecast</li>
                <li style={{ marginBottom: '4px' }}>Evaluation metrics (MSE, RMSE, MAE, MAPE)</li>
              </ul>
            </div>
          </div>

          {/* Example use case */}
          <div style={{
            backgroundColor: `${GLUON_BLUE}10`,
            padding: '10px 12px',
            borderRadius: '4px',
            borderLeft: `3px solid ${GLUON_BLUE}`,
            flexShrink: 0,
          }}>
            <div style={{ color: GLUON_BLUE, fontSize: '10px', fontWeight: 600, textTransform: 'uppercase', marginBottom: '4px' }}>
              Example Use Case:
            </div>
            <div style={{ color: colors.text, fontSize: '11px', lineHeight: 1.5 }}>
              You have 30 days of stock prices and want to predict the next 10 days. Use DeepAR for probabilistic forecasts with confidence intervals.
            </div>
          </div>
        </div>
      )}
    </div>
  );
}

const DL_MODELS = [
  { id: 'forecast_feedforward', label: 'SimpleFeedForward', desc: 'Basic feedforward neural network - fast training, good baseline', category: 'Basic' },
  { id: 'forecast_deepar', label: 'DeepAR', desc: 'Amazon\'s autoregressive RNN - best for probabilistic forecasts', category: 'RNN' },
  { id: 'forecast_tft', label: 'Temporal Fusion Transformer', desc: 'Attention-based transformer - handles multiple inputs', category: 'Transformer' },
  { id: 'forecast_wavenet', label: 'WaveNet', desc: 'Dilated causal convolutions - captures long-range patterns', category: 'CNN' },
  { id: 'forecast_dlinear', label: 'DLinear', desc: 'Decomposition linear model - simple but effective', category: 'Linear' },
  { id: 'forecast_patchtst', label: 'PatchTST', desc: 'Patch time-series transformer - state-of-the-art accuracy', category: 'Transformer' },
  { id: 'forecast_tide', label: 'TiDE', desc: 'Time-series dense encoder - efficient for long sequences', category: 'MLP' },
  { id: 'forecast_lagtst', label: 'LagTST', desc: 'Lag-based transformer - uses lagged features', category: 'Transformer' },
  { id: 'forecast_deepnpts', label: 'DeepNPTS', desc: 'Non-parametric time series - flexible distributions', category: 'Advanced' },
];

const BASELINE_MODELS = [
  { id: 'predict_seasonal_naive', label: 'Seasonal Naive', desc: 'Repeats last season - good baseline for seasonal data', category: 'Statistical' },
  { id: 'predict_mean', label: 'Mean', desc: 'Predicts historical mean - simplest baseline', category: 'Statistical' },
  { id: 'predict_constant', label: 'Constant', desc: 'Predicts last value - random walk baseline', category: 'Statistical' },
];

export function GluonTSPanel() {
  const { colors } = useTerminalTheme();

  // Config state
  const [dataInput, setDataInput] = useState('100, 102, 101, 105, 108, 107, 110, 112, 115, 118, 120, 119, 122, 125, 128, 130, 127, 132, 135, 138, 140, 142, 145, 148, 150');
  const [selectedModel, setSelectedModel] = useState('forecast_deepar');
  const [predictionLength, setPredictionLength] = useState('10');
  const [epochs, setEpochs] = useState('5');
  const [freq, setFreq] = useState('D');

  // Results state
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [forecastResult, setForecastResult] = useState<ForecastResult | null>(null);
  const [evalResult, setEvalResult] = useState<EvalResult | null>(null);

  const parseData = (): number[] => {
    return dataInput.split(',').map(s => parseFloat(s.trim())).filter(n => !isNaN(n));
  };

  const runForecast = async () => {
    setLoading(true); setError(null); setEvalResult(null);
    try {
      const data = parseData();
      if (data.length < 10) throw new Error('Need at least 10 data points');

      const commandName = `gluonts_${selectedModel}`;
      const predLen = parseInt(predictionLength) || 10;
      const ep = parseInt(epochs) || 5;
      let resultStr: string;

      if (['forecast_feedforward', 'forecast_dlinear'].includes(selectedModel)) {
        resultStr = await invoke<string>(commandName, { data, predictionLength: predLen, epochs: ep });
      } else if (selectedModel === 'predict_seasonal_naive') {
        resultStr = await invoke<string>(commandName, { data, predictionLength: predLen, seasonLength: 7 });
      } else if (selectedModel === 'predict_mean') {
        resultStr = await invoke<string>(commandName, { data, predictionLength: predLen });
      } else if (selectedModel === 'predict_constant') {
        resultStr = await invoke<string>(commandName, { data, predictionLength: predLen, constantValue: data[data.length - 1] });
      } else {
        resultStr = await invoke<string>(commandName, { data, predictionLength: predLen, freq, epochs: ep });
      }

      setForecastResult(JSON.parse(resultStr));
    } catch (e: any) {
      setError(e.toString());
    } finally {
      setLoading(false);
    }
  };

  const runEvaluation = async () => {
    setLoading(true); setError(null);
    try {
      const data = parseData();
      const splitIdx = Math.floor(data.length * 0.8);
      const trainData = data.slice(0, splitIdx);
      const testData = data.slice(splitIdx);
      const predLen = parseInt(predictionLength) || 10;

      const resultStr = await invoke<string>('gluonts_evaluate', {
        trainData, testData, predictionLength: predLen, freq
      });
      setEvalResult(JSON.parse(resultStr));
    } catch (e: any) {
      setError(e.toString());
    } finally {
      setLoading(false);
    }
  };

  // Build chart data
  const buildChartData = () => {
    if (!forecastResult) return [];
    const data = parseData();
    const items: any[] = [];

    data.forEach((val, i) => {
      items.push({ idx: i, historical: val, mean: null, q10: null, q90: null });
    });

    const lastVal = data[data.length - 1];
    items[items.length - 1] = {
      ...items[items.length - 1],
      mean: lastVal,
      q10: lastVal,
      q90: lastVal,
    };

    forecastResult.mean.forEach((val, i) => {
      items.push({
        idx: data.length + i,
        historical: null,
        mean: val,
        q10: forecastResult.quantiles['0.1'][i],
        q90: forecastResult.quantiles['0.9'][i],
      });
    });

    return items;
  };

  const allModels = [...DL_MODELS, ...BASELINE_MODELS];
  const currentModel = allModels.find(m => m.id === selectedModel);
  const isDL = DL_MODELS.some(m => m.id === selectedModel);

  const inputStyle: React.CSSProperties = {
    backgroundColor: colors.background,
    border: '1px solid #333',
    color: colors.text,
    padding: '8px 10px',
    fontSize: '13px',
    fontFamily: 'monospace',
    width: '100%',
    outline: 'none',
    borderRadius: '2px',
  };

  const labelStyle: React.CSSProperties = {
    color: colors.textMuted,
    fontSize: '11px',
    fontWeight: 600,
    textTransform: 'uppercase' as const,
    letterSpacing: '0.5px',
    marginBottom: '6px',
    display: 'flex',
    alignItems: 'center',
    gap: '6px',
  };

  const cardStyle: React.CSSProperties = {
    backgroundColor: colors.background,
    border: '1px solid #333',
    padding: '12px',
    borderRadius: '4px',
  };

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
      {/* Mode Tabs - Header */}
      <div
        style={{
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          padding: '8px 12px',
          backgroundColor: colors.panel,
          borderBottom: '1px solid #333',
        }}
      >
        <Brain size={16} color={GLUON_BLUE} />
        <span style={{ color: colors.text, fontWeight: 700, fontSize: '12px', letterSpacing: '0.5px' }}>
          GLUONTS FORECASTING
        </span>
        <span style={{ color: colors.textMuted, fontSize: '11px', marginLeft: 'auto' }}>
          {parseData().length} data points | {currentModel?.category || 'Model'}
        </span>
      </div>

      {/* Content Area */}
      <div style={{ flex: 1, overflow: 'auto', padding: '12px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
        {/* How to Use Guide */}
        <HowToUsePanel />

        {/* Main Grid: Inputs Left, Results Right */}
        <div style={{ display: 'grid', gridTemplateColumns: '350px 1fr', gap: '16px', flex: 1, minHeight: 0 }}>
          {/* LEFT: Configuration */}
          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px', overflowY: 'auto' }}>
            {/* Time Series Data Input */}
            <div style={cardStyle}>
              <div style={labelStyle}>
                <span>Time Series Data</span>
                <TooltipHelp text="Enter your historical values separated by commas. These can be stock prices, sales figures, temperatures, or any sequential data." />
              </div>
              <textarea
                value={dataInput}
                onChange={(e) => setDataInput(e.target.value)}
                rows={3}
                style={{ ...inputStyle, resize: 'vertical' }}
                placeholder="100, 102, 105, 108, 110, ..."
              />
              <div style={{ color: colors.textMuted, fontSize: '10px', marginTop: '4px' }}>
                {parseData().length} data points detected (minimum 10 required)
              </div>
            </div>

            {/* Model Selection */}
            <div style={cardStyle}>
              <div style={labelStyle}>
                <span>Forecasting Model</span>
                <TooltipHelp text="Deep Learning models learn patterns from data. Baselines are simple statistical methods for comparison." />
              </div>
              <select
                value={selectedModel}
                onChange={(e) => setSelectedModel(e.target.value)}
                style={inputStyle}
              >
                <optgroup label="Deep Learning Models">
                  {DL_MODELS.map(m => <option key={m.id} value={m.id}>{m.label}</option>)}
                </optgroup>
                <optgroup label="Statistical Baselines">
                  {BASELINE_MODELS.map(m => <option key={m.id} value={m.id}>{m.label}</option>)}
                </optgroup>
              </select>
              {currentModel && (
                <div style={{
                  color: GLUON_BLUE,
                  fontSize: '11px',
                  marginTop: '6px',
                  padding: '8px',
                  backgroundColor: `${GLUON_BLUE}10`,
                  borderRadius: '4px',
                  borderLeft: `2px solid ${GLUON_BLUE}`,
                }}>
                  {currentModel.desc}
                </div>
              )}
            </div>

            {/* Parameters */}
            <div style={cardStyle}>
              <div style={labelStyle}>
                <span>Parameters</span>
              </div>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
                <div>
                  <div style={{ ...labelStyle, fontSize: '10px', marginBottom: '4px' }}>
                    <span>Prediction Length</span>
                    <TooltipHelp text="How many future time steps to predict. E.g., 10 for next 10 days if using daily data." />
                  </div>
                  <input
                    type="number"
                    value={predictionLength}
                    onChange={(e) => setPredictionLength(e.target.value)}
                    style={inputStyle}
                    placeholder="10"
                  />
                </div>
                <div>
                  <div style={{ ...labelStyle, fontSize: '10px', marginBottom: '4px' }}>
                    <span>Frequency</span>
                    <TooltipHelp text="Time interval between data points. Must match your input data frequency." />
                  </div>
                  <select value={freq} onChange={(e) => setFreq(e.target.value)} style={inputStyle}>
                    <option value="D">Daily</option>
                    <option value="W">Weekly</option>
                    <option value="M">Monthly</option>
                    <option value="h">Hourly</option>
                  </select>
                </div>
                {isDL && (
                  <div style={{ gridColumn: '1 / -1' }}>
                    <div style={{ ...labelStyle, fontSize: '10px', marginBottom: '4px' }}>
                      <span>Training Epochs</span>
                      <TooltipHelp text="Number of times to train on the data. More epochs = better fit but slower. Start with 5-10." />
                    </div>
                    <input
                      type="number"
                      value={epochs}
                      onChange={(e) => setEpochs(e.target.value)}
                      style={inputStyle}
                      placeholder="5"
                    />
                  </div>
                )}
              </div>
            </div>

            {/* Action Buttons */}
            <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
              <button
                onClick={runForecast}
                disabled={loading}
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  gap: '6px',
                  padding: '10px 16px',
                  backgroundColor: loading ? `${GLUON_BLUE}60` : GLUON_BLUE,
                  color: '#fff',
                  border: 'none',
                  borderRadius: '4px',
                  cursor: loading ? 'not-allowed' : 'pointer',
                  fontSize: '12px',
                  fontWeight: 600,
                }}
              >
                {loading ? <RefreshCw size={14} className="animate-spin" /> : <Play size={14} />}
                {loading ? 'RUNNING...' : 'RUN FORECAST'}
              </button>
              <button
                onClick={runEvaluation}
                disabled={loading}
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  gap: '6px',
                  padding: '10px 16px',
                  backgroundColor: 'transparent',
                  color: loading ? colors.textMuted : GLUON_BLUE,
                  border: `1px solid ${loading ? '#333' : GLUON_BLUE}`,
                  borderRadius: '4px',
                  cursor: loading ? 'not-allowed' : 'pointer',
                  fontSize: '12px',
                  fontWeight: 600,
                }}
              >
                <BarChart3 size={14} />
                EVALUATE (80/20 SPLIT)
              </button>
            </div>
          </div>

          {/* RIGHT: Results */}
          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px', overflowY: 'auto' }}>
            {/* Error */}
            {error && (
              <div style={{
                display: 'flex',
                alignItems: 'flex-start',
                gap: '8px',
                padding: '12px',
                backgroundColor: `${colors.alert}15`,
                border: `1px solid ${colors.alert}`,
                borderRadius: '4px',
              }}>
                <AlertCircle size={14} color={colors.alert} style={{ flexShrink: 0, marginTop: 2 }} />
                <div style={{ color: colors.alert, fontSize: '12px', fontFamily: 'monospace' }}>{error}</div>
              </div>
            )}

            {/* Forecast Chart */}
            {forecastResult && (
              <div style={cardStyle}>
                <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '12px' }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                    <TrendingUp size={14} color={GLUON_BLUE} />
                    <span style={{ color: colors.text, fontWeight: 600, fontSize: '12px' }}>
                      {forecastResult.model} FORECAST
                    </span>
                  </div>
                  <span style={{ color: colors.textMuted, fontSize: '11px' }}>
                    {forecastResult.prediction_length} steps ahead
                  </span>
                </div>
                <ResponsiveContainer width="100%" height={250}>
                  <AreaChart data={buildChartData()}>
                    <CartesianGrid strokeDasharray="3 3" stroke="#333" strokeOpacity={0.5} />
                    <XAxis dataKey="idx" stroke={colors.textMuted} tick={{ fill: colors.textMuted, fontSize: 10 }} />
                    <YAxis stroke={colors.textMuted} tick={{ fill: colors.textMuted, fontSize: 10 }} />
                    <Tooltip
                      contentStyle={{ backgroundColor: colors.background, border: `1px solid ${GLUON_BLUE}`, borderRadius: '4px', fontSize: 11 }}
                      labelStyle={{ color: GLUON_BLUE }}
                    />
                    <Area type="monotone" dataKey="q90" stackId="1" stroke="none" fill={GLUON_BLUE} fillOpacity={0.15} />
                    <Area type="monotone" dataKey="q10" stackId="2" stroke="none" fill={colors.background} fillOpacity={1} />
                    <Line type="monotone" dataKey="historical" stroke={colors.text} strokeWidth={1.5} dot={false} connectNulls={false} />
                    <Line type="monotone" dataKey="mean" stroke={GLUON_BLUE} strokeWidth={2} dot={false} connectNulls={false} />
                  </AreaChart>
                </ResponsiveContainer>
                <div style={{ display: 'flex', gap: '16px', marginTop: '8px', fontSize: '11px', color: colors.textMuted }}>
                  <span><span style={{ color: colors.text }}>---</span> Historical</span>
                  <span><span style={{ color: GLUON_BLUE }}>---</span> Forecast Mean</span>
                  <span><span style={{ color: GLUON_BLUE, opacity: 0.3 }}>|||</span> 10-90% Confidence</span>
                </div>
              </div>
            )}

            {/* Forecast Values Table */}
            {forecastResult && (
              <div style={cardStyle}>
                <div style={{ ...labelStyle, color: GLUON_BLUE, marginBottom: '8px' }}>FORECAST VALUES</div>
                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(5, 1fr)', gap: '4px', fontSize: '11px' }}>
                  <span style={{ color: colors.textMuted, fontWeight: 600, padding: '4px 0' }}>Step</span>
                  <span style={{ color: colors.textMuted, fontWeight: 600, padding: '4px 0' }}>Q10</span>
                  <span style={{ color: colors.textMuted, fontWeight: 600, padding: '4px 0' }}>Mean</span>
                  <span style={{ color: colors.textMuted, fontWeight: 600, padding: '4px 0' }}>Q50</span>
                  <span style={{ color: colors.textMuted, fontWeight: 600, padding: '4px 0' }}>Q90</span>
                </div>
                <div style={{ maxHeight: '150px', overflowY: 'auto' }}>
                  {forecastResult.mean.map((val, i) => (
                    <div key={i} style={{ display: 'grid', gridTemplateColumns: 'repeat(5, 1fr)', gap: '4px', padding: '4px 0', borderBottom: '1px solid #222', fontSize: '11px' }}>
                      <span style={{ color: colors.textMuted }}>t+{i + 1}</span>
                      <span style={{ color: colors.warning }}>{forecastResult.quantiles['0.1'][i]?.toFixed(2)}</span>
                      <span style={{ color: GLUON_BLUE, fontWeight: 700 }}>{val.toFixed(2)}</span>
                      <span style={{ color: colors.accent }}>{forecastResult.quantiles['0.5'][i]?.toFixed(2)}</span>
                      <span style={{ color: colors.warning }}>{forecastResult.quantiles['0.9'][i]?.toFixed(2)}</span>
                    </div>
                  ))}
                </div>
              </div>
            )}

            {/* Evaluation Results */}
            {evalResult && (
              <div style={cardStyle}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '12px' }}>
                  <BarChart3 size={14} color={colors.success} />
                  <span style={{ color: colors.text, fontWeight: 600, fontSize: '12px' }}>EVALUATION METRICS</span>
                  <span style={{ color: colors.textMuted, fontSize: '11px' }}>({evalResult.num_forecasts} forecasts)</span>
                </div>
                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '8px' }}>
                  {Object.entries(evalResult.aggregate_metrics).map(([key, val]) => (
                    <div key={key} style={{ backgroundColor: colors.panel, padding: '10px', borderRadius: '4px', border: '1px solid #333' }}>
                      <div style={{ fontSize: '10px', color: colors.textMuted, marginBottom: '4px', fontWeight: 600 }}>{key}</div>
                      <div style={{ fontSize: '14px', color: GLUON_BLUE, fontWeight: 700 }}>{val.toFixed(4)}</div>
                    </div>
                  ))}
                </div>
              </div>
            )}

            {/* Empty state */}
            {!forecastResult && !evalResult && !error && !loading && (
              <div style={{
                flex: 1,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                backgroundColor: colors.panel,
                border: '1px solid #333',
                borderRadius: '4px',
                minHeight: '200px',
              }}>
                <div style={{ textAlign: 'center' }}>
                  <Zap size={32} color="#333" style={{ marginBottom: '8px' }} />
                  <div style={{ color: colors.textMuted, fontSize: '12px' }}>
                    Enter time series data and click RUN FORECAST
                  </div>
                  <div style={{ color: colors.textMuted, fontSize: '11px', marginTop: '4px' }}>
                    Or use EVALUATE to test model accuracy on your data
                  </div>
                </div>
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}

export default GluonTSPanel;
