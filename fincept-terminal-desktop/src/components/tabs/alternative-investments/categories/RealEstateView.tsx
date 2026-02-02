import React, { useState } from 'react';
import { AlternativeInvestmentApi } from '@/services/alternativeInvestments/api/analyticsApi';
import { RealEstateParams, REITParams, RealEstateType } from '@/services/alternativeInvestments/api/types';
import VerdictPanel from '../components/VerdictPanel';

type RealEstateInvestmentType = 'real-estate' | 'reit' | 'infrastructure';

export const RealEstateView: React.FC = () => {
  const [selected, setSelected] = useState<RealEstateInvestmentType>('real-estate');
  const [isAnalyzing, setIsAnalyzing] = useState(false);
  const [analysis, setAnalysis] = useState<any>(null);
  const [error, setError] = useState<string | null>(null);

  const handleAnalyze = async (data: any) => {
    setIsAnalyzing(true);
    setError(null);

    try {
      let result;
      switch (selected) {
        case 'real-estate':
          result = await AlternativeInvestmentApi.analyzeRealEstate(data);
          break;
        case 'reit':
          result = await AlternativeInvestmentApi.analyzeREIT(data);
          break;
        case 'infrastructure':
          result = await AlternativeInvestmentApi.analyzeRealEstate({...data, property_type: 'infrastructure' as any});
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
        <h3 className="text-lg font-semibold mb-4">Select Investment Type</h3>
        <div className="grid grid-cols-3 gap-3">
          {(['real-estate', 'reit', 'infrastructure'] as const).map((type) => (
            <button
              key={type}
              onClick={() => setSelected(type)}
              className={`p-4 rounded-lg border-2 transition-all ${
                selected === type ? 'bg-green-900/30 border-green-500' : 'bg-gray-900/30 border-gray-600'
              }`}
            >
              <div className="text-2xl mb-2">
                {type === 'real-estate' ? '🏠' : type === 'reit' ? '🏢' : '🌉'}
              </div>
              <div className="text-sm font-medium capitalize">{type.replace('-', ' ')}</div>
            </button>
          ))}
        </div>
      </div>

      <div className="bg-gray-800/50 rounded-lg p-6 border border-gray-700">
        <h3 className="text-lg font-semibold mb-4">Investment Details</h3>
        {selected === 'real-estate' && <RealEstateForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />}
        {selected === 'reit' && <REITForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />}
        {selected === 'infrastructure' && <RealEstateForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />}
      </div>

      {error && <div className="bg-red-900/20 border border-red-500 rounded-lg p-4 text-red-400">❌ {error}</div>}
      {analysis?.success && analysis.metrics && <VerdictPanel verdict={analysis.metrics} />}
    </div>
  );
};

const RealEstateForm: React.FC<{onSubmit: any; isLoading: boolean}> = ({ onSubmit, isLoading }) => {
  const [formData, setFormData] = useState({
    name: 'Sample Property',
    property_type: 'multifamily' as RealEstateType,
    acquisition_price: 1000000,
    current_market_value: 1200000,
    annual_noi: 80000,
    ltv_ratio: 0.75,
  });

  return (
    <form onSubmit={(e) => { e.preventDefault(); onSubmit({...formData, asset_class: 'real_estate', currency: 'USD'}); }} className="space-y-4">
      <div className="grid grid-cols-2 gap-4">
        <div>
          <label className="block text-sm font-medium text-gray-300 mb-2">Property Type</label>
          <select value={formData.property_type} onChange={(e) => setFormData({...formData, property_type: e.target.value as RealEstateType})} className="w-full bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white">
            <option value="office">Office</option>
            <option value="retail">Retail</option>
            <option value="industrial">Industrial</option>
            <option value="multifamily">Multifamily</option>
            <option value="hotel">Hotel</option>
          </select>
        </div>
        <input type="text" placeholder="Property Name" value={formData.name} onChange={(e) => setFormData({...formData, name: e.target.value})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" placeholder="Acquisition Price ($)" value={formData.acquisition_price} onChange={(e) => setFormData({...formData, acquisition_price: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" placeholder="Current Value ($)" value={formData.current_market_value} onChange={(e) => setFormData({...formData, current_market_value: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" placeholder="Annual NOI ($)" value={formData.annual_noi} onChange={(e) => setFormData({...formData, annual_noi: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.01" placeholder="LTV Ratio" value={formData.ltv_ratio} onChange={(e) => setFormData({...formData, ltv_ratio: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
      </div>
      <button type="submit" disabled={isLoading} className="w-full bg-green-600 hover:bg-green-500 disabled:bg-gray-600 text-white font-semibold py-3 px-4 rounded-lg">
        {isLoading ? 'Analyzing...' : '🔍 Analyze Property'}
      </button>
    </form>
  );
};

const REITForm: React.FC<{onSubmit: any; isLoading: boolean}> = ({ onSubmit, isLoading }) => {
  const [formData, setFormData] = useState({
    name: 'Sample REIT',
    share_price: 50,
    dividend_yield: 0.045,
    ffo_per_share: 3.5,
    occupancy_rate: 0.92,
  });

  return (
    <form onSubmit={(e) => { e.preventDefault(); onSubmit({...formData, asset_class: 'reit', currency: 'USD'}); }} className="space-y-4">
      <div className="grid grid-cols-2 gap-4">
        <input type="text" placeholder="REIT Name" value={formData.name} onChange={(e) => setFormData({...formData, name: e.target.value})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" placeholder="Share Price ($)" value={formData.share_price} onChange={(e) => setFormData({...formData, share_price: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.001" placeholder="Dividend Yield" value={formData.dividend_yield} onChange={(e) => setFormData({...formData, dividend_yield: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.1" placeholder="FFO per Share" value={formData.ffo_per_share} onChange={(e) => setFormData({...formData, ffo_per_share: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
      </div>
      <button type="submit" disabled={isLoading} className="w-full bg-green-600 hover:bg-green-500 disabled:bg-gray-600 text-white font-semibold py-3 px-4 rounded-lg">
        {isLoading ? 'Analyzing...' : '🔍 Analyze REIT'}
      </button>
    </form>
  );
};

export default RealEstateView;
