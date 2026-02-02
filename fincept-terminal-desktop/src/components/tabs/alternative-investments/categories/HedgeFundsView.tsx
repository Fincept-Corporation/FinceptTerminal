import React, { useState } from 'react';
import { AlternativeInvestmentApi } from '@/services/alternativeInvestments/api/analyticsApi';
import { HedgeFundParams, ManagedFuturesParams, MarketNeutralParams, HedgeFundStrategy } from '@/services/alternativeInvestments/api/types';
import VerdictPanel from '../components/VerdictPanel';

type HedgeFundType = 'hedge-fund' | 'managed-futures' | 'market-neutral';

export const HedgeFundsView: React.FC = () => {
  const [selected, setSelected] = useState<HedgeFundType>('hedge-fund');
  const [isAnalyzing, setIsAnalyzing] = useState(false);
  const [analysis, setAnalysis] = useState<any>(null);
  const [error, setError] = useState<string | null>(null);

  const handleAnalyze = async (data: any) => {
    setIsAnalyzing(true);
    setError(null);

    try {
      let result;
      switch (selected) {
        case 'hedge-fund':
          result = await AlternativeInvestmentApi.analyzeHedgeFund(data);
          break;
        case 'managed-futures':
          result = await AlternativeInvestmentApi.analyzeManagedFutures(data);
          break;
        case 'market-neutral':
          result = await AlternativeInvestmentApi.analyzeMarketNeutral(data);
          break;
      }
      setAnalysis(result);
    } catch (err: any) {
      setError(err.message);
    } finally {
      setIsAnalyzing(false);
    }
  };

  return (
    <div className="space-y-6">
      <div className="bg-gray-800/50 rounded-lg p-4 border border-gray-700">
        <h3 className="text-lg font-semibold mb-4">Select Strategy Type</h3>
        <div className="grid grid-cols-3 gap-3">
          {(['hedge-fund', 'managed-futures', 'market-neutral'] as const).map((type) => (
            <button
              key={type}
              onClick={() => setSelected(type)}
              className={`p-4 rounded-lg border-2 transition-all ${
                selected === type ? 'bg-purple-900/30 border-purple-500' : 'bg-gray-900/30 border-gray-600'
              }`}
            >
              <div className="text-2xl mb-2">
                {type === 'hedge-fund' ? '🛡️' : type === 'managed-futures' ? '📊' : '⚖️'}
              </div>
              <div className="text-sm font-medium capitalize">{type.replace('-', ' ')}</div>
            </button>
          ))}
        </div>
      </div>

      <div className="bg-gray-800/50 rounded-lg p-6 border border-gray-700">
        <h3 className="text-lg font-semibold mb-4">Strategy Details</h3>
        {selected === 'hedge-fund' && <HedgeFundForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />}
        {selected === 'managed-futures' && <ManagedFuturesForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />}
        {selected === 'market-neutral' && <MarketNeutralForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />}
      </div>

      {error && <div className="bg-red-900/20 border border-red-500 rounded-lg p-4 text-red-400">❌ {error}</div>}
      {analysis?.success && analysis.metrics && <VerdictPanel verdict={analysis.metrics} />}
    </div>
  );
};

const HedgeFundForm: React.FC<{onSubmit: any; isLoading: boolean}> = ({ onSubmit, isLoading }) => {
  const [formData, setFormData] = useState({
    name: 'Sample Hedge Fund',
    strategy: 'equity_long_short' as HedgeFundStrategy,
    aum: 100000000,
    annual_return: 0.12,
    volatility: 0.15,
    management_fee: 0.02,
    performance_fee: 0.20,
  });

  return (
    <form onSubmit={(e) => { e.preventDefault(); onSubmit({...formData, asset_class: 'hedge_fund', currency: 'USD'}); }} className="space-y-4">
      <div className="grid grid-cols-2 gap-4">
        <input type="text" placeholder="Fund Name" value={formData.name} onChange={(e) => setFormData({...formData, name: e.target.value})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <div>
          <label className="block text-sm font-medium text-gray-300 mb-2">Strategy</label>
          <select value={formData.strategy} onChange={(e) => setFormData({...formData, strategy: e.target.value as HedgeFundStrategy})} className="w-full bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white">
            <option value="equity_long_short">Equity Long/Short</option>
            <option value="event_driven">Event Driven</option>
            <option value="global_macro">Global Macro</option>
            <option value="relative_value">Relative Value</option>
          </select>
        </div>
        <input type="number" placeholder="AUM ($)" value={formData.aum} onChange={(e) => setFormData({...formData, aum: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.01" placeholder="Annual Return (%)" value={formData.annual_return * 100} onChange={(e) => setFormData({...formData, annual_return: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.01" placeholder="Volatility (%)" value={formData.volatility * 100} onChange={(e) => setFormData({...formData, volatility: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.01" placeholder="Management Fee (%)" value={formData.management_fee * 100} onChange={(e) => setFormData({...formData, management_fee: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.01" placeholder="Performance Fee (%)" value={formData.performance_fee * 100} onChange={(e) => setFormData({...formData, performance_fee: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
      </div>
      <button type="submit" disabled={isLoading} className="w-full bg-purple-600 hover:bg-purple-500 disabled:bg-gray-600 text-white font-semibold py-3 px-4 rounded-lg">
        {isLoading ? 'Analyzing...' : '🔍 Analyze Hedge Fund'}
      </button>
    </form>
  );
};

const ManagedFuturesForm: React.FC<{onSubmit: any; isLoading: boolean}> = ({ onSubmit, isLoading }) => {
  const [formData, setFormData] = useState({
    name: 'Sample CTA Fund',
    annual_return: 0.10,
    volatility: 0.18,
    management_fee: 0.02,
    performance_fee: 0.20,
  });

  return (
    <form onSubmit={(e) => { e.preventDefault(); onSubmit({...formData, asset_class: 'managed_futures', currency: 'USD'}); }} className="space-y-4">
      <div className="grid grid-cols-2 gap-4">
        <input type="text" placeholder="CTA Fund Name" value={formData.name} onChange={(e) => setFormData({...formData, name: e.target.value})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.01" placeholder="Annual Return (%)" value={formData.annual_return * 100} onChange={(e) => setFormData({...formData, annual_return: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.01" placeholder="Volatility (%)" value={formData.volatility * 100} onChange={(e) => setFormData({...formData, volatility: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.01" placeholder="Management Fee (%)" value={formData.management_fee * 100} onChange={(e) => setFormData({...formData, management_fee: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.01" placeholder="Performance Fee (%)" value={formData.performance_fee * 100} onChange={(e) => setFormData({...formData, performance_fee: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
      </div>
      <button type="submit" disabled={isLoading} className="w-full bg-purple-600 hover:bg-purple-500 disabled:bg-gray-600 text-white font-semibold py-3 px-4 rounded-lg">
        {isLoading ? 'Analyzing...' : '🔍 Analyze CTA Fund'}
      </button>
    </form>
  );
};

const MarketNeutralForm: React.FC<{onSubmit: any; isLoading: boolean}> = ({ onSubmit, isLoading }) => {
  const [formData, setFormData] = useState({
    name: 'Sample Market Neutral Fund',
    annual_return: 0.08,
    volatility: 0.06,
    market_beta: 0.1,
    management_fee: 0.015,
  });

  return (
    <form onSubmit={(e) => { e.preventDefault(); onSubmit({...formData, asset_class: 'market_neutral', currency: 'USD'}); }} className="space-y-4">
      <div className="grid grid-cols-2 gap-4">
        <input type="text" placeholder="Fund Name" value={formData.name} onChange={(e) => setFormData({...formData, name: e.target.value})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.01" placeholder="Annual Return (%)" value={formData.annual_return * 100} onChange={(e) => setFormData({...formData, annual_return: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.01" placeholder="Volatility (%)" value={formData.volatility * 100} onChange={(e) => setFormData({...formData, volatility: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.01" placeholder="Market Beta" value={formData.market_beta} onChange={(e) => setFormData({...formData, market_beta: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.01" placeholder="Management Fee (%)" value={formData.management_fee * 100} onChange={(e) => setFormData({...formData, management_fee: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
      </div>
      <button type="submit" disabled={isLoading} className="w-full bg-purple-600 hover:bg-purple-500 disabled:bg-gray-600 text-white font-semibold py-3 px-4 rounded-lg">
        {isLoading ? 'Analyzing...' : '🔍 Analyze Market Neutral'}
      </button>
    </form>
  );
};

export default HedgeFundsView;
