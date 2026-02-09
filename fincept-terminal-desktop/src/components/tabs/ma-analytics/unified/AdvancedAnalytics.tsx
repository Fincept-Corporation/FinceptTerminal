/**
 * Advanced Analytics Panel - Monte Carlo Simulation & Regression Valuation
 *
 * Tools: Monte Carlo simulation for probability distributions, Regression-based valuation
 */

import React, { useState } from 'react';
import { Activity, PlayCircle, Loader2, TrendingUp, BarChart3, Target, Percent, Plus, Trash2 } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, COMMON_STYLES } from '../../portfolio-tab/finceptStyles';
import { MAAnalyticsService } from '@/services/maAnalyticsService';
import { showSuccess, showError } from '@/utils/notifications';

type AnalysisType = 'montecarlo' | 'regression';

interface CompanyData {
  name: string;
  ev: number;
  revenue: number;
  ebitda: number;
  growth: number;
}

const DEFAULT_COMPS: CompanyData[] = [
  { name: 'Comp A', ev: 5000, revenue: 1200, ebitda: 350, growth: 15 },
  { name: 'Comp B', ev: 3200, revenue: 850, ebitda: 220, growth: 12 },
  { name: 'Comp C', ev: 7500, revenue: 1800, ebitda: 520, growth: 18 },
  { name: 'Comp D', ev: 4100, revenue: 950, ebitda: 280, growth: 10 },
];

export const AdvancedAnalytics: React.FC = () => {
  const [analysisType, setAnalysisType] = useState<AnalysisType>('montecarlo');
  const [loading, setLoading] = useState(false);
  const [result, setResult] = useState<any>(null);

  // Monte Carlo inputs
  const [mcInputs, setMcInputs] = useState({
    baseValuation: 1000,
    revenueGrowthMean: 15,
    revenueGrowthStd: 5,
    marginMean: 25,
    marginStd: 3,
    discountRate: 10,
    simulations: 10000,
  });

  // Regression inputs
  const [regressionType, setRegressionType] = useState<'ols' | 'multiple'>('ols');
  const [compData, setCompData] = useState<CompanyData[]>(DEFAULT_COMPS);
  const [subjectMetrics, setSubjectMetrics] = useState({
    revenue: 1000,
    ebitda: 300,
    growth: 14,
  });

  const runAnalysis = async () => {
    setLoading(true);
    setResult(null);
    try {
      let data;
      if (analysisType === 'montecarlo') {
        data = await MAAnalyticsService.AdvancedAnalytics.runMonteCarloValuation(
          mcInputs.baseValuation,
          mcInputs.revenueGrowthMean,
          mcInputs.revenueGrowthStd,
          mcInputs.marginMean,
          mcInputs.marginStd,
          mcInputs.discountRate,
          mcInputs.simulations
        );
      } else {
        data = await MAAnalyticsService.AdvancedAnalytics.runRegressionValuation(
          compData,
          subjectMetrics,
          regressionType
        );
      }
      setResult(data);
      showSuccess(`${analysisType === 'montecarlo' ? 'Monte Carlo' : 'Regression'} analysis completed`);
    } catch (error) {
      showError(`Analysis failed: ${error}`);
    } finally {
      setLoading(false);
    }
  };

  const addComp = () => {
    setCompData([...compData, { name: `Comp ${String.fromCharCode(65 + compData.length)}`, ev: 0, revenue: 0, ebitda: 0, growth: 0 }]);
  };

  const removeComp = (idx: number) => {
    setCompData(compData.filter((_, i) => i !== idx));
  };

  const updateComp = (idx: number, field: keyof CompanyData, value: string | number) => {
    const updated = [...compData];
    updated[idx] = { ...updated[idx], [field]: field === 'name' ? value : parseFloat(String(value)) || 0 };
    setCompData(updated);
  };

  const renderMonteCarloInputs = () => (
    <div className="space-y-4">
      <div className="grid grid-cols-2 gap-3">
        <div>
          <label className="text-xs text-gray-400 mb-1 block">Base Valuation ($M)</label>
          <input
            type="number"
            value={mcInputs.baseValuation}
            onChange={e => setMcInputs({ ...mcInputs, baseValuation: parseFloat(e.target.value) || 0 })}
            className="w-full px-3 py-2 rounded text-sm text-white"
            style={{ backgroundColor: FINCEPT.CHARCOAL }}
          />
        </div>
        <div>
          <label className="text-xs text-gray-400 mb-1 block">Discount Rate (%)</label>
          <input
            type="number"
            value={mcInputs.discountRate}
            onChange={e => setMcInputs({ ...mcInputs, discountRate: parseFloat(e.target.value) || 0 })}
            className="w-full px-3 py-2 rounded text-sm text-white"
            style={{ backgroundColor: FINCEPT.CHARCOAL }}
          />
        </div>
      </div>

      <div className="p-3 rounded" style={{ backgroundColor: FINCEPT.CHARCOAL }}>
        <h4 className="text-xs font-medium text-white mb-3">Revenue Growth Distribution</h4>
        <div className="grid grid-cols-2 gap-3">
          <div>
            <label className="text-xs text-gray-400 mb-1 block">Mean (%)</label>
            <input
              type="number"
              value={mcInputs.revenueGrowthMean}
              onChange={e => setMcInputs({ ...mcInputs, revenueGrowthMean: parseFloat(e.target.value) || 0 })}
              className="w-full px-3 py-2 rounded text-sm text-white"
              style={{ backgroundColor: FINCEPT.PANEL_BG }}
            />
          </div>
          <div>
            <label className="text-xs text-gray-400 mb-1 block">Std Dev (%)</label>
            <input
              type="number"
              value={mcInputs.revenueGrowthStd}
              onChange={e => setMcInputs({ ...mcInputs, revenueGrowthStd: parseFloat(e.target.value) || 0 })}
              className="w-full px-3 py-2 rounded text-sm text-white"
              style={{ backgroundColor: FINCEPT.PANEL_BG }}
            />
          </div>
        </div>
      </div>

      <div className="p-3 rounded" style={{ backgroundColor: FINCEPT.CHARCOAL }}>
        <h4 className="text-xs font-medium text-white mb-3">Margin Distribution</h4>
        <div className="grid grid-cols-2 gap-3">
          <div>
            <label className="text-xs text-gray-400 mb-1 block">Mean (%)</label>
            <input
              type="number"
              value={mcInputs.marginMean}
              onChange={e => setMcInputs({ ...mcInputs, marginMean: parseFloat(e.target.value) || 0 })}
              className="w-full px-3 py-2 rounded text-sm text-white"
              style={{ backgroundColor: FINCEPT.PANEL_BG }}
            />
          </div>
          <div>
            <label className="text-xs text-gray-400 mb-1 block">Std Dev (%)</label>
            <input
              type="number"
              value={mcInputs.marginStd}
              onChange={e => setMcInputs({ ...mcInputs, marginStd: parseFloat(e.target.value) || 0 })}
              className="w-full px-3 py-2 rounded text-sm text-white"
              style={{ backgroundColor: FINCEPT.PANEL_BG }}
            />
          </div>
        </div>
      </div>

      <div>
        <label className="text-xs text-gray-400 mb-1 block">Number of Simulations</label>
        <select
          value={mcInputs.simulations}
          onChange={e => setMcInputs({ ...mcInputs, simulations: parseInt(e.target.value) })}
          className="w-full px-3 py-2 rounded text-sm text-white"
          style={{ backgroundColor: FINCEPT.CHARCOAL }}
        >
          <option value={1000}>1,000</option>
          <option value={5000}>5,000</option>
          <option value={10000}>10,000</option>
          <option value={50000}>50,000</option>
          <option value={100000}>100,000</option>
        </select>
      </div>
    </div>
  );

  const renderRegressionInputs = () => (
    <div className="space-y-4">
      <div>
        <label className="text-xs text-gray-400 mb-1 block">Regression Type</label>
        <div className="flex gap-2">
          <button
            onClick={() => setRegressionType('ols')}
            className={`flex-1 px-3 py-2 rounded text-xs font-medium transition-all ${
              regressionType === 'ols' ? 'text-white' : 'text-gray-400'
            }`}
            style={{ backgroundColor: regressionType === 'ols' ? FINCEPT.ORANGE + '30' : FINCEPT.CHARCOAL }}
          >
            OLS (Simple)
          </button>
          <button
            onClick={() => setRegressionType('multiple')}
            className={`flex-1 px-3 py-2 rounded text-xs font-medium transition-all ${
              regressionType === 'multiple' ? 'text-white' : 'text-gray-400'
            }`}
            style={{ backgroundColor: regressionType === 'multiple' ? FINCEPT.ORANGE + '30' : FINCEPT.CHARCOAL }}
          >
            Multiple Regression
          </button>
        </div>
      </div>

      <div className="p-3 rounded" style={{ backgroundColor: FINCEPT.CHARCOAL }}>
        <div className="flex items-center justify-between mb-3">
          <h4 className="text-xs font-medium text-white">Comparable Companies</h4>
          <button
            onClick={addComp}
            className="flex items-center gap-1 px-2 py-1 rounded text-xs"
            style={{ backgroundColor: FINCEPT.ORANGE + '20', color: FINCEPT.ORANGE }}
          >
            <Plus size={12} /> Add
          </button>
        </div>
        <div className="space-y-2 max-h-48 overflow-y-auto">
          {compData.map((comp, idx) => (
            <div key={idx} className="grid grid-cols-6 gap-2 items-center">
              <input
                type="text"
                value={comp.name}
                onChange={e => updateComp(idx, 'name', e.target.value)}
                className="px-2 py-1 rounded text-xs text-white"
                style={{ backgroundColor: FINCEPT.PANEL_BG }}
                placeholder="Name"
              />
              <input
                type="number"
                value={comp.ev}
                onChange={e => updateComp(idx, 'ev', e.target.value)}
                className="px-2 py-1 rounded text-xs text-white"
                style={{ backgroundColor: FINCEPT.PANEL_BG }}
                placeholder="EV"
              />
              <input
                type="number"
                value={comp.revenue}
                onChange={e => updateComp(idx, 'revenue', e.target.value)}
                className="px-2 py-1 rounded text-xs text-white"
                style={{ backgroundColor: FINCEPT.PANEL_BG }}
                placeholder="Revenue"
              />
              <input
                type="number"
                value={comp.ebitda}
                onChange={e => updateComp(idx, 'ebitda', e.target.value)}
                className="px-2 py-1 rounded text-xs text-white"
                style={{ backgroundColor: FINCEPT.PANEL_BG }}
                placeholder="EBITDA"
              />
              <input
                type="number"
                value={comp.growth}
                onChange={e => updateComp(idx, 'growth', e.target.value)}
                className="px-2 py-1 rounded text-xs text-white"
                style={{ backgroundColor: FINCEPT.PANEL_BG }}
                placeholder="Growth %"
              />
              <button
                onClick={() => removeComp(idx)}
                className="p-1 rounded hover:bg-red-500/20"
                disabled={compData.length <= 2}
              >
                <Trash2 size={12} className={compData.length <= 2 ? 'text-gray-600' : 'text-red-400'} />
              </button>
            </div>
          ))}
        </div>
        <div className="mt-2 grid grid-cols-6 gap-2 text-xs text-gray-500">
          <span>Name</span>
          <span>EV ($M)</span>
          <span>Rev ($M)</span>
          <span>EBITDA</span>
          <span>Growth %</span>
          <span></span>
        </div>
      </div>

      <div className="p-3 rounded" style={{ backgroundColor: FINCEPT.CHARCOAL }}>
        <h4 className="text-xs font-medium text-white mb-3">Subject Company Metrics</h4>
        <div className="grid grid-cols-3 gap-3">
          <div>
            <label className="text-xs text-gray-400 mb-1 block">Revenue ($M)</label>
            <input
              type="number"
              value={subjectMetrics.revenue}
              onChange={e => setSubjectMetrics({ ...subjectMetrics, revenue: parseFloat(e.target.value) || 0 })}
              className="w-full px-3 py-2 rounded text-sm text-white"
              style={{ backgroundColor: FINCEPT.PANEL_BG }}
            />
          </div>
          <div>
            <label className="text-xs text-gray-400 mb-1 block">EBITDA ($M)</label>
            <input
              type="number"
              value={subjectMetrics.ebitda}
              onChange={e => setSubjectMetrics({ ...subjectMetrics, ebitda: parseFloat(e.target.value) || 0 })}
              className="w-full px-3 py-2 rounded text-sm text-white"
              style={{ backgroundColor: FINCEPT.PANEL_BG }}
            />
          </div>
          <div>
            <label className="text-xs text-gray-400 mb-1 block">Growth (%)</label>
            <input
              type="number"
              value={subjectMetrics.growth}
              onChange={e => setSubjectMetrics({ ...subjectMetrics, growth: parseFloat(e.target.value) || 0 })}
              className="w-full px-3 py-2 rounded text-sm text-white"
              style={{ backgroundColor: FINCEPT.PANEL_BG }}
            />
          </div>
        </div>
      </div>
    </div>
  );

  const renderMonteCarloResults = () => {
    if (!result) return null;

    return (
      <div className="space-y-4">
        {/* Key Statistics */}
        <div className="grid grid-cols-2 md:grid-cols-4 gap-3">
          <div className="p-3 rounded text-center" style={{ backgroundColor: FINCEPT.CHARCOAL }}>
            <div className="text-xs text-gray-400">Mean Valuation</div>
            <div className="text-lg font-bold text-white">${(result.mean || 0).toFixed(0)}M</div>
          </div>
          <div className="p-3 rounded text-center" style={{ backgroundColor: FINCEPT.CHARCOAL }}>
            <div className="text-xs text-gray-400">Median Valuation</div>
            <div className="text-lg font-bold text-white">${(result.median || 0).toFixed(0)}M</div>
          </div>
          <div className="p-3 rounded text-center" style={{ backgroundColor: FINCEPT.CHARCOAL }}>
            <div className="text-xs text-gray-400">Std Deviation</div>
            <div className="text-lg font-bold text-white">${(result.std || 0).toFixed(0)}M</div>
          </div>
          <div className="p-3 rounded text-center" style={{ backgroundColor: FINCEPT.CHARCOAL }}>
            <div className="text-xs text-gray-400">CV</div>
            <div className="text-lg font-bold text-white">{((result.std / result.mean) * 100 || 0).toFixed(1)}%</div>
          </div>
        </div>

        {/* Percentiles */}
        <div className="p-4 rounded" style={{ backgroundColor: FINCEPT.CHARCOAL }}>
          <h4 className="text-xs font-medium text-white mb-3">Valuation Distribution</h4>
          <div className="space-y-2">
            {[
              { label: '5th Percentile', key: 'p5', color: '#ef4444' },
              { label: '25th Percentile', key: 'p25', color: '#f97316' },
              { label: '50th Percentile (Median)', key: 'median', color: FINCEPT.ORANGE },
              { label: '75th Percentile', key: 'p75', color: '#22c55e' },
              { label: '95th Percentile', key: 'p95', color: '#10b981' },
            ].map(item => (
              <div key={item.key} className="flex items-center justify-between">
                <span className="text-xs text-gray-400">{item.label}</span>
                <div className="flex items-center gap-2">
                  <div className="w-24 h-2 rounded-full overflow-hidden" style={{ backgroundColor: FINCEPT.PANEL_BG }}>
                    <div
                      className="h-full rounded-full"
                      style={{
                        backgroundColor: item.color,
                        width: `${Math.min(100, ((result[item.key] || 0) / (result.p95 || 1)) * 100)}%`,
                      }}
                    />
                  </div>
                  <span className="text-sm font-medium text-white w-20 text-right">
                    ${(result[item.key] || 0).toFixed(0)}M
                  </span>
                </div>
              </div>
            ))}
          </div>
        </div>

        {/* Range */}
        <div className="p-4 rounded" style={{ backgroundColor: FINCEPT.CHARCOAL }}>
          <h4 className="text-xs font-medium text-white mb-2">90% Confidence Interval</h4>
          <div className="flex items-center justify-center gap-4">
            <span className="text-lg font-medium" style={{ color: FINCEPT.ORANGE }}>
              ${(result.p5 || 0).toFixed(0)}M
            </span>
            <span className="text-gray-500">—</span>
            <span className="text-lg font-medium" style={{ color: FINCEPT.ORANGE }}>
              ${(result.p95 || 0).toFixed(0)}M
            </span>
          </div>
        </div>
      </div>
    );
  };

  const renderRegressionResults = () => {
    if (!result) return null;

    return (
      <div className="space-y-4">
        {/* Implied Valuation */}
        <div className="p-4 rounded text-center" style={{ backgroundColor: FINCEPT.ORANGE + '20' }}>
          <div className="text-xs text-gray-400 mb-1">Implied Enterprise Value</div>
          <div className="text-3xl font-bold" style={{ color: FINCEPT.ORANGE }}>
            ${(result.implied_ev || 0).toFixed(0)}M
          </div>
        </div>

        {/* Regression Stats */}
        <div className="grid grid-cols-2 gap-3">
          <div className="p-3 rounded text-center" style={{ backgroundColor: FINCEPT.CHARCOAL }}>
            <div className="text-xs text-gray-400">R-Squared</div>
            <div className="text-lg font-bold text-white">{((result.r_squared || 0) * 100).toFixed(1)}%</div>
          </div>
          <div className="p-3 rounded text-center" style={{ backgroundColor: FINCEPT.CHARCOAL }}>
            <div className="text-xs text-gray-400">Adj. R-Squared</div>
            <div className="text-lg font-bold text-white">{((result.adj_r_squared || 0) * 100).toFixed(1)}%</div>
          </div>
        </div>

        {/* Coefficients */}
        {result.coefficients && (
          <div className="p-4 rounded" style={{ backgroundColor: FINCEPT.CHARCOAL }}>
            <h4 className="text-xs font-medium text-white mb-3">Regression Coefficients</h4>
            <div className="space-y-2">
              {Object.entries(result.coefficients).map(([key, value]) => (
                <div key={key} className="flex items-center justify-between">
                  <span className="text-xs text-gray-400">{key.replace(/_/g, ' ')}</span>
                  <span className="text-sm font-medium text-white">{(value as number).toFixed(4)}</span>
                </div>
              ))}
            </div>
          </div>
        )}

        {/* Implied Multiples */}
        {result.implied_multiples && (
          <div className="p-4 rounded" style={{ backgroundColor: FINCEPT.CHARCOAL }}>
            <h4 className="text-xs font-medium text-white mb-3">Implied Multiples</h4>
            <div className="grid grid-cols-2 gap-3">
              {Object.entries(result.implied_multiples).map(([key, value]) => (
                <div key={key} className="flex items-center justify-between p-2 rounded" style={{ backgroundColor: FINCEPT.PANEL_BG }}>
                  <span className="text-xs text-gray-400">{key.toUpperCase()}</span>
                  <span className="text-sm font-bold" style={{ color: FINCEPT.ORANGE }}>{(value as number).toFixed(1)}x</span>
                </div>
              ))}
            </div>
          </div>
        )}
      </div>
    );
  };

  return (
    <div className="h-full flex flex-col" style={{ backgroundColor: FINCEPT.CHARCOAL }}>
      {/* Header */}
      <div className="flex items-center justify-between p-4 border-b" style={{ borderColor: FINCEPT.PANEL_BG }}>
        <div className="flex items-center gap-3">
          <div className="p-2 rounded-lg" style={{ backgroundColor: FINCEPT.ORANGE + '20' }}>
            <Activity size={20} style={{ color: FINCEPT.ORANGE }} />
          </div>
          <div>
            <h2 className={TYPOGRAPHY.HEADING}>Advanced Analytics</h2>
            <p className="text-xs text-gray-400">Monte Carlo simulation and regression-based valuation</p>
          </div>
        </div>
        <button
          onClick={runAnalysis}
          disabled={loading}
          className="flex items-center gap-2 px-4 py-2 rounded-lg text-sm font-medium transition-all hover:opacity-90 disabled:opacity-50"
          style={{ backgroundColor: FINCEPT.ORANGE, color: '#000' }}
        >
          {loading ? <Loader2 size={16} className="animate-spin" /> : <PlayCircle size={16} />}
          Run Analysis
        </button>
      </div>

      {/* Analysis Type Tabs */}
      <div className="flex items-center gap-2 px-4 py-2 border-b" style={{ borderColor: FINCEPT.PANEL_BG }}>
        <button
          onClick={() => { setAnalysisType('montecarlo'); setResult(null); }}
          className={`flex items-center gap-2 px-4 py-2 rounded-lg text-sm font-medium transition-all ${
            analysisType === 'montecarlo' ? 'text-white' : 'text-gray-400 hover:text-white'
          }`}
          style={{ backgroundColor: analysisType === 'montecarlo' ? FINCEPT.PANEL_BG : 'transparent' }}
        >
          <Target size={16} /> Monte Carlo
        </button>
        <button
          onClick={() => { setAnalysisType('regression'); setResult(null); }}
          className={`flex items-center gap-2 px-4 py-2 rounded-lg text-sm font-medium transition-all ${
            analysisType === 'regression' ? 'text-white' : 'text-gray-400 hover:text-white'
          }`}
          style={{ backgroundColor: analysisType === 'regression' ? FINCEPT.PANEL_BG : 'transparent' }}
        >
          <TrendingUp size={16} /> Regression
        </button>
      </div>

      {/* Content */}
      <div className="flex-1 overflow-auto p-4">
        <div className="grid grid-cols-1 lg:grid-cols-2 gap-4">
          {/* Input Panel */}
          <div className="p-4 rounded-lg" style={{ backgroundColor: FINCEPT.PANEL_BG }}>
            <h3 className="text-sm font-medium text-white mb-4">
              {analysisType === 'montecarlo' ? 'Monte Carlo Parameters' : 'Regression Inputs'}
            </h3>
            {analysisType === 'montecarlo' ? renderMonteCarloInputs() : renderRegressionInputs()}
          </div>

          {/* Results Panel */}
          <div className="p-4 rounded-lg" style={{ backgroundColor: FINCEPT.PANEL_BG }}>
            <h3 className="text-sm font-medium text-white mb-4">Analysis Results</h3>
            {loading ? (
              <div className="flex items-center justify-center h-48">
                <Loader2 size={24} className="animate-spin" style={{ color: FINCEPT.ORANGE }} />
              </div>
            ) : result ? (
              analysisType === 'montecarlo' ? renderMonteCarloResults() : renderRegressionResults()
            ) : (
              <div className="flex flex-col items-center justify-center h-48 text-gray-500 text-sm">
                <Activity size={32} className="mb-2 opacity-50" />
                <p>Click "Run Analysis" to start</p>
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
};

export default AdvancedAnalytics;
