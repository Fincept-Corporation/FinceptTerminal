import React, { useState } from 'react';
import { AlternativeInvestmentApi } from '@/services/alternativeInvestments/api/analyticsApi';
import { CoveredCallParams, SRIFundParams } from '@/services/alternativeInvestments/api/types';
import VerdictPanel from '../components/VerdictPanel';

type StrategyType = 'covered-calls' | 'sri-funds';

export const StrategiesView: React.FC = () => {
  const [selected, setSelected] = useState<StrategyType>('covered-calls');
  const [isAnalyzing, setIsAnalyzing] = useState(false);
  const [analysis, setAnalysis] = useState<any>(null);
  const [error, setError] = useState<string | null>(null);

  const handleAnalyze = async (data: any) => {
    setIsAnalyzing(true);
    setError(null);

    try {
      let result;
      switch (selected) {
        case 'covered-calls':
          result = await AlternativeInvestmentApi.analyzeCoveredCall(data);
          break;
        case 'sri-funds':
          result = await AlternativeInvestmentApi.analyzeSRIFund(data);
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
        <h3 className="text-lg font-semibold mb-4">Select Strategy</h3>
        <div className="grid grid-cols-2 gap-3">
          {(['covered-calls', 'sri-funds'] as const).map((type) => (
            <button
              key={type}
              onClick={() => setSelected(type)}
              className={`p-4 rounded-lg border-2 transition-all ${
                selected === type ? 'bg-pink-900/30 border-pink-500' : 'bg-gray-900/30 border-gray-600'
              }`}
            >
              <div className="text-2xl mb-2">
                {type === 'covered-calls' ? '🎯' : '🌱'}
              </div>
              <div className="text-sm font-medium capitalize">{type.replace('-', ' ')}</div>
            </button>
          ))}
        </div>
      </div>

      <div className="bg-gray-800/50 rounded-lg p-6 border border-gray-700">
        <h3 className="text-lg font-semibold mb-4">Strategy Details</h3>
        {selected === 'covered-calls' && <CoveredCallForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />}
        {selected === 'sri-funds' && <SRIFundForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />}
      </div>

      {error && <div className="bg-red-900/20 border border-red-500 rounded-lg p-4 text-red-400">❌ {error}</div>}
      {analysis?.success && analysis.metrics && <VerdictPanel verdict={analysis.metrics} />}
    </div>
  );
};

const CoveredCallForm: React.FC<{onSubmit: any; isLoading: boolean}> = ({ onSubmit, isLoading }) => {
  const [formData, setFormData] = useState({
    name: 'Covered Call Strategy',
    stock_price: 100,
    shares_owned: 100,
    strike_price: 105,
    premium_received: 2.5,
    days_to_expiration: 30,
  });

  return (
    <form onSubmit={(e) => { e.preventDefault(); onSubmit({...formData, asset_class: 'equity', currency: 'USD'}); }} className="space-y-4">
      <div className="grid grid-cols-2 gap-4">
        <input type="text" placeholder="Strategy Name" value={formData.name} onChange={(e) => setFormData({...formData, name: e.target.value})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.01" placeholder="Stock Price ($)" value={formData.stock_price} onChange={(e) => setFormData({...formData, stock_price: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" placeholder="Shares Owned" value={formData.shares_owned} onChange={(e) => setFormData({...formData, shares_owned: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.01" placeholder="Strike Price ($)" value={formData.strike_price} onChange={(e) => setFormData({...formData, strike_price: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.01" placeholder="Premium Received ($)" value={formData.premium_received} onChange={(e) => setFormData({...formData, premium_received: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" placeholder="Days to Expiration" value={formData.days_to_expiration} onChange={(e) => setFormData({...formData, days_to_expiration: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
      </div>
      <button type="submit" disabled={isLoading} className="w-full bg-pink-600 hover:bg-pink-500 disabled:bg-gray-600 text-white font-semibold py-3 px-4 rounded-lg">
        {isLoading ? 'Analyzing...' : '🔍 Analyze Covered Call'}
      </button>
    </form>
  );
};

const SRIFundForm: React.FC<{onSubmit: any; isLoading: boolean}> = ({ onSubmit, isLoading }) => {
  const [formData, setFormData] = useState({
    name: 'ESG Fund',
    expense_ratio: 0.008,
    esg_rating: 8.5,
    annual_return: 0.10,
    tracking_error: 0.015,
  });

  return (
    <form onSubmit={(e) => { e.preventDefault(); onSubmit({...formData, asset_class: 'equity', currency: 'USD'}); }} className="space-y-4">
      <div className="grid grid-cols-2 gap-4">
        <input type="text" placeholder="Fund Name" value={formData.name} onChange={(e) => setFormData({...formData, name: e.target.value})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.0001" placeholder="Expense Ratio (%)" value={formData.expense_ratio * 100} onChange={(e) => setFormData({...formData, expense_ratio: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.1" placeholder="ESG Rating (1-10)" value={formData.esg_rating} onChange={(e) => setFormData({...formData, esg_rating: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.01" placeholder="Annual Return (%)" value={formData.annual_return * 100} onChange={(e) => setFormData({...formData, annual_return: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.001" placeholder="Tracking Error (%)" value={formData.tracking_error * 100} onChange={(e) => setFormData({...formData, tracking_error: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
      </div>
      <button type="submit" disabled={isLoading} className="w-full bg-pink-600 hover:bg-pink-500 disabled:bg-gray-600 text-white font-semibold py-3 px-4 rounded-lg">
        {isLoading ? 'Analyzing...' : '🔍 Analyze SRI Fund'}
      </button>
    </form>
  );
};

export default StrategiesView;
