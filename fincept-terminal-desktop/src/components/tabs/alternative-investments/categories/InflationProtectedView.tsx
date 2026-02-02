import React, { useState } from 'react';
import { AlternativeInvestmentApi } from '@/services/alternativeInvestments/api/analyticsApi';
import { InflationProtectedParams } from '@/services/alternativeInvestments/api/types';
import VerdictPanel from '../components/VerdictPanel';

type InflationType = 'tips' | 'i-bonds' | 'stable-value';

export const InflationProtectedView: React.FC = () => {
  const [selected, setSelected] = useState<InflationType>('tips');
  const [isAnalyzing, setIsAnalyzing] = useState(false);
  const [analysis, setAnalysis] = useState<any>(null);
  const [error, setError] = useState<string | null>(null);

  const handleAnalyze = async (data: any) => {
    setIsAnalyzing(true);
    setError(null);

    try {
      const result = await AlternativeInvestmentApi.analyzeInflationProtected(data);
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
        <h3 className="text-lg font-semibold mb-4">Select Inflation-Protected Security</h3>
        <div className="grid grid-cols-3 gap-3">
          {(['tips', 'i-bonds', 'stable-value'] as const).map((type) => (
            <button
              key={type}
              onClick={() => setSelected(type)}
              className={`p-4 rounded-lg border-2 transition-all ${
                selected === type ? 'bg-blue-900/30 border-blue-500' : 'bg-gray-900/30 border-gray-600'
              }`}
            >
              <div className="text-2xl mb-2">
                {type === 'tips' ? '🛡️' : type === 'i-bonds' ? '💵' : '📊'}
              </div>
              <div className="text-sm font-medium uppercase">{type.replace('-', ' ')}</div>
            </button>
          ))}
        </div>
      </div>

      <div className="bg-gray-800/50 rounded-lg p-6 border border-gray-700">
        <h3 className="text-lg font-semibold mb-4">Security Details</h3>
        {selected === 'tips' && <TIPSForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />}
        {selected === 'i-bonds' && <IBondsForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />}
        {selected === 'stable-value' && <StableValueForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />}
      </div>

      {error && <div className="bg-red-900/20 border border-red-500 rounded-lg p-4 text-red-400">❌ {error}</div>}
      {analysis?.success && analysis.metrics && <VerdictPanel verdict={analysis.metrics} />}
    </div>
  );
};

const TIPSForm: React.FC<{onSubmit: any; isLoading: boolean}> = ({ onSubmit, isLoading }) => {
  const [formData, setFormData] = useState({
    name: 'TIPS 10-Year',
    bond_type: 'TIPS',
    face_value: 10000,
    real_yield: 0.015,
    maturity_years: 10,
    current_cpi: 0.03,
  });

  return (
    <form onSubmit={(e) => { e.preventDefault(); onSubmit({...formData, asset_class: 'fixed_income', currency: 'USD'}); }} className="space-y-4">
      <div className="grid grid-cols-2 gap-4">
        <input type="text" placeholder="Bond Name" value={formData.name} onChange={(e) => setFormData({...formData, name: e.target.value})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" placeholder="Face Value ($)" value={formData.face_value} onChange={(e) => setFormData({...formData, face_value: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.001" placeholder="Real Yield (%)" value={formData.real_yield * 100} onChange={(e) => setFormData({...formData, real_yield: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" placeholder="Maturity (Years)" value={formData.maturity_years} onChange={(e) => setFormData({...formData, maturity_years: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.001" placeholder="Current CPI Rate (%)" value={formData.current_cpi * 100} onChange={(e) => setFormData({...formData, current_cpi: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
      </div>
      <button type="submit" disabled={isLoading} className="w-full bg-blue-600 hover:bg-blue-500 disabled:bg-gray-600 text-white font-semibold py-3 px-4 rounded-lg">
        {isLoading ? 'Analyzing...' : '🔍 Analyze TIPS'}
      </button>
    </form>
  );
};

const IBondsForm: React.FC<{onSubmit: any; isLoading: boolean}> = ({ onSubmit, isLoading }) => {
  const [formData, setFormData] = useState({
    name: 'Series I Savings Bond',
    bond_type: 'I_BONDS',
    face_value: 10000,
    fixed_rate: 0.005,
    current_inflation_rate: 0.032,
    holding_period_months: 60,
  });

  return (
    <form onSubmit={(e) => { e.preventDefault(); onSubmit({...formData, asset_class: 'fixed_income', currency: 'USD'}); }} className="space-y-4">
      <div className="grid grid-cols-2 gap-4">
        <input type="text" placeholder="Bond Name" value={formData.name} onChange={(e) => setFormData({...formData, name: e.target.value})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" placeholder="Face Value ($)" value={formData.face_value} onChange={(e) => setFormData({...formData, face_value: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.001" placeholder="Fixed Rate (%)" value={formData.fixed_rate * 100} onChange={(e) => setFormData({...formData, fixed_rate: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.001" placeholder="Current Inflation Rate (%)" value={formData.current_inflation_rate * 100} onChange={(e) => setFormData({...formData, current_inflation_rate: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" placeholder="Holding Period (Months)" value={formData.holding_period_months} onChange={(e) => setFormData({...formData, holding_period_months: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
      </div>
      <button type="submit" disabled={isLoading} className="w-full bg-blue-600 hover:bg-blue-500 disabled:bg-gray-600 text-white font-semibold py-3 px-4 rounded-lg">
        {isLoading ? 'Analyzing...' : '🔍 Analyze I-Bonds'}
      </button>
    </form>
  );
};

const StableValueForm: React.FC<{onSubmit: any; isLoading: boolean}> = ({ onSubmit, isLoading }) => {
  const [formData, setFormData] = useState({
    name: 'Stable Value Fund',
    bond_type: 'STABLE_VALUE',
    face_value: 50000,
    credited_rate: 0.025,
    book_value: 50000,
    market_value: 49500,
  });

  return (
    <form onSubmit={(e) => { e.preventDefault(); onSubmit({...formData, asset_class: 'fixed_income', currency: 'USD'}); }} className="space-y-4">
      <div className="grid grid-cols-2 gap-4">
        <input type="text" placeholder="Fund Name" value={formData.name} onChange={(e) => setFormData({...formData, name: e.target.value})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" placeholder="Investment Amount ($)" value={formData.face_value} onChange={(e) => setFormData({...formData, face_value: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.001" placeholder="Credited Rate (%)" value={formData.credited_rate * 100} onChange={(e) => setFormData({...formData, credited_rate: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" placeholder="Book Value ($)" value={formData.book_value} onChange={(e) => setFormData({...formData, book_value: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" placeholder="Market Value ($)" value={formData.market_value} onChange={(e) => setFormData({...formData, market_value: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
      </div>
      <button type="submit" disabled={isLoading} className="w-full bg-blue-600 hover:bg-blue-500 disabled:bg-gray-600 text-white font-semibold py-3 px-4 rounded-lg">
        {isLoading ? 'Analyzing...' : '🔍 Analyze Stable Value'}
      </button>
    </form>
  );
};

export default InflationProtectedView;
