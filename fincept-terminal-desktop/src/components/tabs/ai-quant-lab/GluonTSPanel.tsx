/**
 * GluonTSPanel - Deep Learning Time Series Forecasting
 * Amazon GluonTS: 9 DL models + 3 baselines + evaluation
 */

import React, { useState } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import {
  TrendingUp, Play, AlertCircle, BarChart3, Zap, Brain
} from 'lucide-react';
import {
  ResponsiveContainer, LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, Area, AreaChart
} from 'recharts';

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

const DL_MODELS = [
  { id: 'forecast_feedforward', label: 'SimpleFeedForward', desc: 'Basic feedforward neural network' },
  { id: 'forecast_deepar', label: 'DeepAR', desc: 'Autoregressive RNN (Amazon)' },
  { id: 'forecast_tft', label: 'Temporal Fusion Transformer', desc: 'Attention-based transformer' },
  { id: 'forecast_wavenet', label: 'WaveNet', desc: 'Dilated causal convolutions' },
  { id: 'forecast_dlinear', label: 'DLinear', desc: 'Decomposition linear model' },
  { id: 'forecast_patchtst', label: 'PatchTST', desc: 'Patch time-series transformer' },
  { id: 'forecast_tide', label: 'TiDE', desc: 'Time-series dense encoder' },
  { id: 'forecast_lagtst', label: 'LagTST', desc: 'Lag-based transformer' },
  { id: 'forecast_deepnpts', label: 'DeepNPTS', desc: 'Non-parametric time series' },
];

const BASELINE_MODELS = [
  { id: 'predict_seasonal_naive', label: 'Seasonal Naive', desc: 'Repeats last season' },
  { id: 'predict_mean', label: 'Mean', desc: 'Predicts historical mean' },
  { id: 'predict_constant', label: 'Constant', desc: 'Predicts a fixed value' },
];

export function GluonTSPanel() {
  const { colors, fontFamily } = useTerminalTheme();
  // Derived colors for properties not in ColorTheme
  const borderColor = colors.textMuted;
  const textSecondary = colors.textMuted;

  // Config state
  const [dataInput, setDataInput] = useState('100,102,101,105,108,107,110,112,115,118,120,119,122,125,128,130,127,132,135,138,140,142,145,148,150');
  const [selectedModel, setSelectedModel] = useState('forecast_deepar');
  const [predictionLength, setPredictionLength] = useState(10);
  const [epochs, setEpochs] = useState(5);
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
      let resultStr: string;

      // Commands with freq param vs without
      if (['forecast_feedforward', 'forecast_dlinear'].includes(selectedModel)) {
        resultStr = await invoke<string>(commandName, { data, predictionLength, epochs });
      } else if (selectedModel === 'predict_seasonal_naive') {
        resultStr = await invoke<string>(commandName, { data, predictionLength, seasonLength: 7 });
      } else if (selectedModel === 'predict_mean') {
        resultStr = await invoke<string>(commandName, { data, predictionLength });
      } else if (selectedModel === 'predict_constant') {
        resultStr = await invoke<string>(commandName, { data, predictionLength, constantValue: data[data.length - 1] });
      } else {
        resultStr = await invoke<string>(commandName, { data, predictionLength, freq, epochs });
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

      const resultStr = await invoke<string>('gluonts_evaluate', {
        trainData, testData, predictionLength, freq
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

    // Historical data
    data.forEach((val, i) => {
      items.push({ idx: i, historical: val, mean: null, q10: null, q90: null });
    });

    // Forecast data (connect to last historical)
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

  return (
    <div className="flex flex-col h-full gap-3 p-3" style={{ fontFamily }}>
      {/* Header */}
      <div className="flex items-center justify-between px-3 py-2 rounded" style={{ backgroundColor: colors.panel, border: `1px solid ${borderColor}` }}>
        <div className="flex items-center gap-2">
          <Brain size={16} style={{ color: colors.primary }} />
          <span className="font-bold text-xs tracking-wider" style={{ color: colors.text }}>GLUONTS TIME SERIES FORECASTING</span>
        </div>
        <span className="text-xs" style={{ color: textSecondary }}>9 DL Models + 3 Baselines</span>
      </div>

      {/* Main Grid */}
      <div className="grid grid-cols-3 gap-3 flex-1 min-h-0">
        {/* LEFT: Config */}
        <div className="flex flex-col gap-3" style={{ overflow: 'auto' }}>
          {/* Data Input */}
          <div className="p-3 rounded" style={{ backgroundColor: colors.panel, border: `1px solid ${borderColor}` }}>
            <label className="block text-xs font-bold mb-1 tracking-wider" style={{ color: textSecondary }}>TIME SERIES DATA (comma-separated)</label>
            <textarea
              value={dataInput}
              onChange={(e) => setDataInput(e.target.value)}
              rows={3}
              className="w-full p-2 rounded text-xs"
              style={{ backgroundColor: colors.background, color: colors.text, border: `1px solid ${borderColor}`, fontFamily, resize: 'vertical' }}
            />
            <div className="text-xs mt-1" style={{ color: textSecondary }}>
              {parseData().length} data points
            </div>
          </div>

          {/* Model Selector */}
          <div className="p-3 rounded" style={{ backgroundColor: colors.panel, border: `1px solid ${borderColor}` }}>
            <label className="block text-xs font-bold mb-1 tracking-wider" style={{ color: textSecondary }}>MODEL</label>
            <select
              value={selectedModel}
              onChange={(e) => setSelectedModel(e.target.value)}
              className="w-full p-2 rounded text-xs"
              style={{ backgroundColor: colors.background, color: colors.text, border: `1px solid ${borderColor}`, fontFamily }}
            >
              <optgroup label="Deep Learning">
                {DL_MODELS.map(m => <option key={m.id} value={m.id}>{m.label}</option>)}
              </optgroup>
              <optgroup label="Baselines">
                {BASELINE_MODELS.map(m => <option key={m.id} value={m.id}>{m.label}</option>)}
              </optgroup>
            </select>
            {currentModel && (
              <div className="text-xs mt-1" style={{ color: colors.accent }}>{currentModel.desc}</div>
            )}
          </div>

          {/* Parameters */}
          <div className="p-3 rounded" style={{ backgroundColor: colors.panel, border: `1px solid ${borderColor}` }}>
            <label className="block text-xs font-bold mb-2 tracking-wider" style={{ color: textSecondary }}>PARAMETERS</label>
            <div className="grid grid-cols-2 gap-2">
              <div>
                <label className="block text-xs mb-1" style={{ color: textSecondary }}>Prediction Length</label>
                <input type="number" value={predictionLength} onChange={(e) => setPredictionLength(parseInt(e.target.value) || 10)}
                  className="w-full p-1.5 rounded text-xs" style={{ backgroundColor: colors.background, color: colors.text, border: `1px solid ${borderColor}`, fontFamily }} />
              </div>
              {isDL && (
                <div>
                  <label className="block text-xs mb-1" style={{ color: textSecondary }}>Epochs</label>
                  <input type="number" value={epochs} onChange={(e) => setEpochs(parseInt(e.target.value) || 5)}
                    className="w-full p-1.5 rounded text-xs" style={{ backgroundColor: colors.background, color: colors.text, border: `1px solid ${borderColor}`, fontFamily }} />
                </div>
              )}
              <div>
                <label className="block text-xs mb-1" style={{ color: textSecondary }}>Frequency</label>
                <select value={freq} onChange={(e) => setFreq(e.target.value)}
                  className="w-full p-1.5 rounded text-xs" style={{ backgroundColor: colors.background, color: colors.text, border: `1px solid ${borderColor}`, fontFamily }}>
                  <option value="D">Daily</option>
                  <option value="W">Weekly</option>
                  <option value="M">Monthly</option>
                  <option value="h">Hourly</option>
                </select>
              </div>
            </div>
          </div>

          {/* Buttons */}
          <div className="flex flex-col gap-2">
            <button onClick={runForecast} disabled={loading}
              className="w-full flex items-center justify-center gap-2 py-2 rounded font-bold text-xs tracking-wider"
              style={{
                backgroundColor: loading ? borderColor : colors.primary,
                color: colors.background,
                opacity: loading ? 0.6 : 1,
                cursor: loading ? 'not-allowed' : 'pointer',
                border: 'none', fontFamily,
              }}>
              <Play size={12} />
              {loading ? 'RUNNING...' : 'RUN FORECAST'}
            </button>
            <button onClick={runEvaluation} disabled={loading}
              className="w-full flex items-center justify-center gap-2 py-2 rounded font-bold text-xs tracking-wider"
              style={{
                backgroundColor: 'transparent',
                color: loading ? textSecondary : colors.accent,
                border: `1px solid ${loading ? borderColor : colors.accent}`,
                opacity: loading ? 0.6 : 1,
                cursor: loading ? 'not-allowed' : 'pointer',
                fontFamily,
              }}>
              <BarChart3 size={12} />
              EVALUATE (80/20 SPLIT)
            </button>
          </div>
        </div>

        {/* CENTER + RIGHT: Results */}
        <div className="col-span-2 flex flex-col gap-3" style={{ overflow: 'auto' }}>
          {/* Error */}
          {error && (
            <div className="flex items-center gap-2 p-3 rounded" style={{ backgroundColor: `${colors.alert}20`, border: `1px solid ${colors.alert}` }}>
              <AlertCircle size={14} style={{ color: colors.alert }} />
              <span className="text-xs" style={{ color: colors.alert }}>{error}</span>
            </div>
          )}

          {/* Forecast Chart */}
          {forecastResult && (
            <div className="p-3 rounded" style={{ backgroundColor: colors.panel, border: `1px solid ${borderColor}` }}>
              <div className="flex items-center justify-between mb-2">
                <div className="flex items-center gap-2">
                  <TrendingUp size={14} style={{ color: colors.primary }} />
                  <span className="font-bold text-xs tracking-wider" style={{ color: colors.text }}>
                    {forecastResult.model} FORECAST
                  </span>
                </div>
                <span className="text-xs" style={{ color: textSecondary }}>
                  {forecastResult.prediction_length} steps ahead
                </span>
              </div>
              <ResponsiveContainer width="100%" height={280}>
                <AreaChart data={buildChartData()}>
                  <CartesianGrid strokeDasharray="3 3" stroke={borderColor} strokeOpacity={0.3} />
                  <XAxis dataKey="idx" stroke={textSecondary} tick={{ fill: textSecondary, fontSize: 9 }} />
                  <YAxis stroke={textSecondary} tick={{ fill: textSecondary, fontSize: 9 }} />
                  <Tooltip
                    contentStyle={{ backgroundColor: colors.panel, border: `1px solid ${colors.accent}`, borderRadius: '2px', fontSize: 10 }}
                    labelStyle={{ color: colors.accent }}
                  />
                  <Area type="monotone" dataKey="q90" stackId="1" stroke="none" fill={colors.accent} fillOpacity={0.1} />
                  <Area type="monotone" dataKey="q10" stackId="2" stroke="none" fill={colors.panel} fillOpacity={1} />
                  <Line type="monotone" dataKey="historical" stroke={colors.text} strokeWidth={1.5} dot={false} connectNulls={false} />
                  <Line type="monotone" dataKey="mean" stroke={colors.primary} strokeWidth={2} dot={false} connectNulls={false} />
                </AreaChart>
              </ResponsiveContainer>
              <div className="flex gap-4 mt-2 text-xs" style={{ color: textSecondary }}>
                <span><span style={{ color: colors.text }}>---</span> Historical</span>
                <span><span style={{ color: colors.primary }}>---</span> Forecast Mean</span>
                <span><span style={{ color: colors.accent, opacity: 0.3 }}>|||</span> 10-90% Quantile</span>
              </div>
            </div>
          )}

          {/* Forecast Values Table */}
          {forecastResult && (
            <div className="p-3 rounded" style={{ backgroundColor: colors.panel, border: `1px solid ${borderColor}` }}>
              <span className="font-bold text-xs tracking-wider" style={{ color: textSecondary }}>FORECAST VALUES</span>
              <div className="grid grid-cols-5 gap-1 mt-2 text-xs" style={{ color: textSecondary }}>
                <span className="font-bold">Step</span>
                <span className="font-bold">Q10</span>
                <span className="font-bold">Mean</span>
                <span className="font-bold">Q50</span>
                <span className="font-bold">Q90</span>
              </div>
              {forecastResult.mean.map((val, i) => (
                <div key={i} className="grid grid-cols-5 gap-1 py-0.5 text-xs" style={{ borderBottom: `1px solid ${borderColor}` }}>
                  <span style={{ color: textSecondary }}>t+{i + 1}</span>
                  <span style={{ color: colors.warning }}>{forecastResult.quantiles['0.1'][i]?.toFixed(2)}</span>
                  <span style={{ color: colors.primary, fontWeight: 700 }}>{val.toFixed(2)}</span>
                  <span style={{ color: colors.accent }}>{forecastResult.quantiles['0.5'][i]?.toFixed(2)}</span>
                  <span style={{ color: colors.warning }}>{forecastResult.quantiles['0.9'][i]?.toFixed(2)}</span>
                </div>
              ))}
            </div>
          )}

          {/* Evaluation Results */}
          {evalResult && (
            <div className="p-3 rounded" style={{ backgroundColor: colors.panel, border: `1px solid ${borderColor}` }}>
              <div className="flex items-center gap-2 mb-2">
                <BarChart3 size={14} style={{ color: colors.success }} />
                <span className="font-bold text-xs tracking-wider" style={{ color: colors.text }}>EVALUATION METRICS</span>
                <span className="text-xs" style={{ color: textSecondary }}>({evalResult.num_forecasts} forecasts)</span>
              </div>
              <div className="grid grid-cols-4 gap-2">
                {Object.entries(evalResult.aggregate_metrics).map(([key, val]) => (
                  <div key={key} className="p-2 rounded" style={{ backgroundColor: colors.background }}>
                    <div className="text-xs font-bold mb-1" style={{ color: textSecondary }}>{key}</div>
                    <div className="text-sm font-bold" style={{ color: colors.accent }}>{val.toFixed(4)}</div>
                  </div>
                ))}
              </div>
            </div>
          )}

          {/* Empty state */}
          {!forecastResult && !evalResult && !error && !loading && (
            <div className="flex-1 flex items-center justify-center rounded" style={{ backgroundColor: colors.panel, border: `1px solid ${borderColor}` }}>
              <div className="text-center">
                <Zap size={32} style={{ color: borderColor, margin: '0 auto 8px' }} />
                <div className="text-xs" style={{ color: textSecondary }}>
                  Select a model and click RUN FORECAST to generate predictions
                </div>
              </div>
            </div>
          )}
        </div>
      </div>
    </div>
  );
}

export default GluonTSPanel;
