import React, { useState } from 'react';
import { AlternativeInvestmentApi } from '@/services/alternativeInvestments/api/analyticsApi';
import { PreciousMetalsParams, NaturalResourceParams, MetalType } from '@/services/alternativeInvestments/api/types';
import VerdictPanel from '../components/VerdictPanel';

type CommodityType = 'precious-metals' | 'natural-resources';

export const CommoditiesView: React.FC = () => {
  const [selected, setSelected] = useState<CommodityType>('precious-metals');
  const [isAnalyzing, setIsAnalyzing] = useState(false);
  const [analysis, setAnalysis] = useState<any>(null);
  const [error, setError] = useState<string | null>(null);

  const handleAnalyze = async (data: any) => {
    setIsAnalyzing(true);
    setError(null);

    try {
      let result;
      switch (selected) {
        case 'precious-metals':
          result = await AlternativeInvestmentApi.analyzePreciousMetals(data);
          break;
        case 'natural-resources':
          result = await AlternativeInvestmentApi.analyzeNaturalResources(data);
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
        <h3 className="text-lg font-semibold mb-4">Select Commodity Type</h3>
        <div className="grid grid-cols-2 gap-3">
          {(['precious-metals', 'natural-resources'] as const).map((type) => (
            <button
              key={type}
              onClick={() => setSelected(type)}
              className={`p-4 rounded-lg border-2 transition-all ${
                selected === type ? 'bg-yellow-900/30 border-yellow-500' : 'bg-gray-900/30 border-gray-600'
              }`}
            >
              <div className="text-2xl mb-2">
                {type === 'precious-metals' ? '💎' : '🛢️'}
              </div>
              <div className="text-sm font-medium capitalize">{type.replace('-', ' ')}</div>
            </button>
          ))}
        </div>
      </div>

      <div className="bg-gray-800/50 rounded-lg p-6 border border-gray-700">
        <h3 className="text-lg font-semibold mb-4">Commodity Details</h3>
        {selected === 'precious-metals' && <PreciousMetalForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />}
        {selected === 'natural-resources' && <NaturalResourceForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />}
      </div>

      {error && <div className="bg-red-900/20 border border-red-500 rounded-lg p-4 text-red-400">❌ {error}</div>}
      {analysis?.success && analysis.metrics && <VerdictPanel verdict={analysis.metrics} />}
    </div>
  );
};

const PreciousMetalForm: React.FC<{onSubmit: any; isLoading: boolean}> = ({ onSubmit, isLoading }) => {
  const [formData, setFormData] = useState({
    name: 'Gold Investment',
    metal_type: 'gold' as MetalType,
    purchase_price: 1800,
    current_price: 1900,
    weight_oz: 10,
    storage_cost_annual: 100,
  });

  return (
    <form onSubmit={(e) => { e.preventDefault(); onSubmit({...formData, asset_class: 'commodities', currency: 'USD'}); }} className="space-y-4">
      <div className="grid grid-cols-2 gap-4">
        <input type="text" placeholder="Investment Name" value={formData.name} onChange={(e) => setFormData({...formData, name: e.target.value})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <div>
          <label className="block text-sm font-medium text-gray-300 mb-2">Metal Type</label>
          <select value={formData.metal_type} onChange={(e) => setFormData({...formData, metal_type: e.target.value as MetalType})} className="w-full bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white">
            <option value="gold">Gold</option>
            <option value="silver">Silver</option>
            <option value="platinum">Platinum</option>
            <option value="palladium">Palladium</option>
          </select>
        </div>
        <input type="number" placeholder="Purchase Price ($/oz)" value={formData.purchase_price} onChange={(e) => setFormData({...formData, purchase_price: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" placeholder="Current Price ($/oz)" value={formData.current_price} onChange={(e) => setFormData({...formData, current_price: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.1" placeholder="Weight (oz)" value={formData.weight_oz} onChange={(e) => setFormData({...formData, weight_oz: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" placeholder="Annual Storage Cost ($)" value={formData.storage_cost_annual} onChange={(e) => setFormData({...formData, storage_cost_annual: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
      </div>
      <button type="submit" disabled={isLoading} className="w-full bg-yellow-600 hover:bg-yellow-500 disabled:bg-gray-600 text-white font-semibold py-3 px-4 rounded-lg">
        {isLoading ? 'Analyzing...' : '🔍 Analyze Precious Metal'}
      </button>
    </form>
  );
};

const NaturalResourceForm: React.FC<{onSubmit: any; isLoading: boolean}> = ({ onSubmit, isLoading }) => {
  const [formData, setFormData] = useState({
    name: 'Oil & Gas Investment',
    resource_type: 'energy',
    purchase_price: 50000,
    current_value: 55000,
    annual_yield: 0.08,
  });

  return (
    <form onSubmit={(e) => { e.preventDefault(); onSubmit({...formData, asset_class: 'natural_resources', currency: 'USD'}); }} className="space-y-4">
      <div className="grid grid-cols-2 gap-4">
        <input type="text" placeholder="Investment Name" value={formData.name} onChange={(e) => setFormData({...formData, name: e.target.value})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <div>
          <label className="block text-sm font-medium text-gray-300 mb-2">Resource Type</label>
          <select value={formData.resource_type} onChange={(e) => setFormData({...formData, resource_type: e.target.value})} className="w-full bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white">
            <option value="energy">Energy (Oil/Gas)</option>
            <option value="agriculture">Agriculture</option>
            <option value="timber">Timber</option>
            <option value="water">Water Rights</option>
          </select>
        </div>
        <input type="number" placeholder="Purchase Price ($)" value={formData.purchase_price} onChange={(e) => setFormData({...formData, purchase_price: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" placeholder="Current Value ($)" value={formData.current_value} onChange={(e) => setFormData({...formData, current_value: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.01" placeholder="Annual Yield (%)" value={formData.annual_yield * 100} onChange={(e) => setFormData({...formData, annual_yield: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
      </div>
      <button type="submit" disabled={isLoading} className="w-full bg-yellow-600 hover:bg-yellow-500 disabled:bg-gray-600 text-white font-semibold py-3 px-4 rounded-lg">
        {isLoading ? 'Analyzing...' : '🔍 Analyze Natural Resource'}
      </button>
    </form>
  );
};

export default CommoditiesView;
