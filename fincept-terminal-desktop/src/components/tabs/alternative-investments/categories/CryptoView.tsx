import React, { useState } from 'react';
import { AlternativeInvestmentApi } from '@/services/alternativeInvestments/api/analyticsApi';
import { DigitalAssetParams } from '@/services/alternativeInvestments/api/types';
import VerdictPanel from '../components/VerdictPanel';

export const CryptoView: React.FC = () => {
  const [isAnalyzing, setIsAnalyzing] = useState(false);
  const [analysis, setAnalysis] = useState<any>(null);
  const [error, setError] = useState<string | null>(null);

  const handleAnalyze = async (data: any) => {
    setIsAnalyzing(true);
    setError(null);

    try {
      const result = await AlternativeInvestmentApi.analyzeDigitalAsset(data);
      setAnalysis(result);
    } catch (err: any) {
      setError(err.message);
    } finally {
      setIsAnalyzing(false);
    }
  };

  return (
    <div className="space-y-6">
      <div className="bg-gray-800/50 rounded-lg p-6 border border-gray-700">
        <h3 className="text-lg font-semibold mb-4">Digital Asset Investment Details</h3>
        <DigitalAssetForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />
      </div>

      {error && <div className="bg-red-900/20 border border-red-500 rounded-lg p-4 text-red-400">❌ {error}</div>}
      {analysis?.success && analysis.metrics && <VerdictPanel verdict={analysis.metrics} />}
    </div>
  );
};

const DigitalAssetForm: React.FC<{onSubmit: any; isLoading: boolean}> = ({ onSubmit, isLoading }) => {
  const [formData, setFormData] = useState({
    name: 'Bitcoin',
    asset_type: 'cryptocurrency',
    purchase_price: 40000,
    current_price: 45000,
    amount_held: 0.5,
    volatility: 0.65,
    market_cap_billions: 850,
  });

  return (
    <form onSubmit={(e) => { e.preventDefault(); onSubmit({...formData, asset_class: 'digital_assets', currency: 'USD'}); }} className="space-y-4">
      <div className="grid grid-cols-2 gap-4">
        <input type="text" placeholder="Asset Name (e.g., Bitcoin, Ethereum)" value={formData.name} onChange={(e) => setFormData({...formData, name: e.target.value})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <div>
          <label className="block text-sm font-medium text-gray-300 mb-2">Asset Type</label>
          <select value={formData.asset_type} onChange={(e) => setFormData({...formData, asset_type: e.target.value})} className="w-full bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white">
            <option value="cryptocurrency">Cryptocurrency</option>
            <option value="token">Token</option>
            <option value="nft">NFT</option>
            <option value="defi">DeFi Asset</option>
          </select>
        </div>
        <input type="number" step="0.01" placeholder="Purchase Price ($)" value={formData.purchase_price} onChange={(e) => setFormData({...formData, purchase_price: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.01" placeholder="Current Price ($)" value={formData.current_price} onChange={(e) => setFormData({...formData, current_price: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.00001" placeholder="Amount Held" value={formData.amount_held} onChange={(e) => setFormData({...formData, amount_held: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.01" placeholder="Volatility (%)" value={formData.volatility * 100} onChange={(e) => setFormData({...formData, volatility: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="1" placeholder="Market Cap (Billions $)" value={formData.market_cap_billions} onChange={(e) => setFormData({...formData, market_cap_billions: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
      </div>
      <button type="submit" disabled={isLoading} className="w-full bg-violet-600 hover:bg-violet-500 disabled:bg-gray-600 text-white font-semibold py-3 px-4 rounded-lg">
        {isLoading ? 'Analyzing...' : '🔍 Analyze Digital Asset'}
      </button>
    </form>
  );
};

export default CryptoView;
