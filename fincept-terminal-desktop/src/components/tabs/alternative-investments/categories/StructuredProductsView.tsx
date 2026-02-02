import React, { useState } from 'react';
import { AlternativeInvestmentApi } from '@/services/alternativeInvestments/api/analyticsApi';
import { StructuredProductParams, LeveragedFundParams } from '@/services/alternativeInvestments/api/types';
import VerdictPanel from '../components/VerdictPanel';

type StructuredType = 'structured-product' | 'leveraged-fund';

export const StructuredProductsView: React.FC = () => {
  const [selected, setSelected] = useState<StructuredType>('structured-product');
  const [isAnalyzing, setIsAnalyzing] = useState(false);
  const [analysis, setAnalysis] = useState<any>(null);
  const [error, setError] = useState<string | null>(null);

  const handleAnalyze = async (data: any) => {
    setIsAnalyzing(true);
    setError(null);

    try {
      let result;
      switch (selected) {
        case 'structured-product':
          result = await AlternativeInvestmentApi.analyzeStructuredProduct(data);
          break;
        case 'leveraged-fund':
          result = await AlternativeInvestmentApi.analyzeLeveragedFund(data);
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
        <h3 className="text-lg font-semibold mb-4">Select Product Type</h3>
        <div className="grid grid-cols-2 gap-3">
          {(['structured-product', 'leveraged-fund'] as const).map((type) => (
            <button
              key={type}
              onClick={() => setSelected(type)}
              className={`p-4 rounded-lg border-2 transition-all ${
                selected === type ? 'bg-orange-900/30 border-orange-500' : 'bg-gray-900/30 border-gray-600'
              }`}
            >
              <div className="text-2xl mb-2">
                {type === 'structured-product' ? '📦' : '⚡'}
              </div>
              <div className="text-sm font-medium capitalize">{type.replace('-', ' ')}</div>
            </button>
          ))}
        </div>
      </div>

      <div className="bg-gray-800/50 rounded-lg p-6 border border-gray-700">
        <h3 className="text-lg font-semibold mb-4">Product Details</h3>
        {selected === 'structured-product' && <StructuredProductForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />}
        {selected === 'leveraged-fund' && <LeveragedFundForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />}
      </div>

      {error && <div className="bg-red-900/20 border border-red-500 rounded-lg p-4 text-red-400">❌ {error}</div>}
      {analysis?.success && analysis.metrics && <VerdictPanel verdict={analysis.metrics} />}
    </div>
  );
};

const StructuredProductForm: React.FC<{onSubmit: any; isLoading: boolean}> = ({ onSubmit, isLoading }) => {
  const [formData, setFormData] = useState({
    name: 'Sample Structured Note',
    principal: 100000,
    term_years: 5,
    coupon_rate: 0.06,
    barrier_level: 0.70,
    participation_rate: 1.0,
    issuer_credit_rating: 'A',
  });

  return (
    <form onSubmit={(e) => { e.preventDefault(); onSubmit({...formData, asset_class: 'structured_product', currency: 'USD'}); }} className="space-y-4">
      <div className="grid grid-cols-2 gap-4">
        <input type="text" placeholder="Product Name" value={formData.name} onChange={(e) => setFormData({...formData, name: e.target.value})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" placeholder="Principal ($)" value={formData.principal} onChange={(e) => setFormData({...formData, principal: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" placeholder="Term (Years)" value={formData.term_years} onChange={(e) => setFormData({...formData, term_years: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.01" placeholder="Coupon Rate (%)" value={formData.coupon_rate * 100} onChange={(e) => setFormData({...formData, coupon_rate: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.01" placeholder="Barrier Level (%)" value={formData.barrier_level * 100} onChange={(e) => setFormData({...formData, barrier_level: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.01" placeholder="Participation Rate (%)" value={formData.participation_rate * 100} onChange={(e) => setFormData({...formData, participation_rate: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="text" placeholder="Issuer Credit Rating" value={formData.issuer_credit_rating} onChange={(e) => setFormData({...formData, issuer_credit_rating: e.target.value})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
      </div>
      <button type="submit" disabled={isLoading} className="w-full bg-orange-600 hover:bg-orange-500 disabled:bg-gray-600 text-white font-semibold py-3 px-4 rounded-lg">
        {isLoading ? 'Analyzing...' : '🔍 Analyze Structured Product'}
      </button>
    </form>
  );
};

const LeveragedFundForm: React.FC<{onSubmit: any; isLoading: boolean}> = ({ onSubmit, isLoading }) => {
  const [formData, setFormData] = useState({
    name: 'Sample 3x Leveraged ETF',
    leverage_ratio: 3.0,
    expense_ratio: 0.0095,
    underlying_index: 'S&P 500',
    daily_reset: true,
  });

  return (
    <form onSubmit={(e) => { e.preventDefault(); onSubmit({...formData, asset_class: 'leveraged_fund', currency: 'USD'}); }} className="space-y-4">
      <div className="grid grid-cols-2 gap-4">
        <input type="text" placeholder="Fund Name" value={formData.name} onChange={(e) => setFormData({...formData, name: e.target.value})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.1" placeholder="Leverage Ratio (e.g., 3.0 for 3x)" value={formData.leverage_ratio} onChange={(e) => setFormData({...formData, leverage_ratio: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.0001" placeholder="Expense Ratio (%)" value={formData.expense_ratio * 100} onChange={(e) => setFormData({...formData, expense_ratio: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="text" placeholder="Underlying Index" value={formData.underlying_index} onChange={(e) => setFormData({...formData, underlying_index: e.target.value})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <div className="flex items-center gap-2 col-span-2">
          <input type="checkbox" checked={formData.daily_reset} onChange={(e) => setFormData({...formData, daily_reset: e.target.checked})} className="w-4 h-4" />
          <label className="text-sm text-gray-300">Daily Reset (Most leveraged funds reset daily)</label>
        </div>
      </div>
      <button type="submit" disabled={isLoading} className="w-full bg-orange-600 hover:bg-orange-500 disabled:bg-gray-600 text-white font-semibold py-3 px-4 rounded-lg">
        {isLoading ? 'Analyzing...' : '🔍 Analyze Leveraged Fund'}
      </button>
    </form>
  );
};

export default StructuredProductsView;
