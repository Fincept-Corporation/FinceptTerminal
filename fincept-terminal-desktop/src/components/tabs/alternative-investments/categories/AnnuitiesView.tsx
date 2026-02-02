import React, { useState } from 'react';
import { AlternativeInvestmentApi } from '@/services/alternativeInvestments/api/analyticsApi';
import { FixedAnnuityParams, VariableAnnuityParams, EquityIndexedAnnuityParams } from '@/services/alternativeInvestments/api/types';
import VerdictPanel from '../components/VerdictPanel';

type AnnuityType = 'fixed' | 'variable' | 'equity-indexed';

export const AnnuitiesView: React.FC = () => {
  const [selected, setSelected] = useState<AnnuityType>('fixed');
  const [isAnalyzing, setIsAnalyzing] = useState(false);
  const [analysis, setAnalysis] = useState<any>(null);
  const [error, setError] = useState<string | null>(null);

  const handleAnalyze = async (data: any) => {
    setIsAnalyzing(true);
    setError(null);

    try {
      let result;
      switch (selected) {
        case 'fixed':
          result = await AlternativeInvestmentApi.analyzeFixedAnnuity(data);
          break;
        case 'variable':
          result = await AlternativeInvestmentApi.analyzeVariableAnnuity(data);
          break;
        case 'equity-indexed':
          result = await AlternativeInvestmentApi.analyzeEquityIndexedAnnuity(data);
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
        <h3 className="text-lg font-semibold mb-4">Select Annuity Type</h3>
        <div className="grid grid-cols-3 gap-3">
          {(['fixed', 'variable', 'equity-indexed'] as const).map((type) => (
            <button
              key={type}
              onClick={() => setSelected(type)}
              className={`p-4 rounded-lg border-2 transition-all ${
                selected === type ? 'bg-cyan-900/30 border-cyan-500' : 'bg-gray-900/30 border-gray-600'
              }`}
            >
              <div className="text-2xl mb-2">
                {type === 'fixed' ? '📅' : type === 'variable' ? '📊' : '📈'}
              </div>
              <div className="text-sm font-medium capitalize">{type.replace('-', ' ')}</div>
            </button>
          ))}
        </div>
      </div>

      <div className="bg-gray-800/50 rounded-lg p-6 border border-gray-700">
        <h3 className="text-lg font-semibold mb-4">Annuity Details</h3>
        {selected === 'fixed' && <FixedAnnuityForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />}
        {selected === 'variable' && <VariableAnnuityForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />}
        {selected === 'equity-indexed' && <EquityIndexedAnnuityForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />}
      </div>

      {error && <div className="bg-red-900/20 border border-red-500 rounded-lg p-4 text-red-400">❌ {error}</div>}
      {analysis?.success && analysis.metrics && <VerdictPanel verdict={analysis.metrics} />}
    </div>
  );
};

const FixedAnnuityForm: React.FC<{onSubmit: any; isLoading: boolean}> = ({ onSubmit, isLoading }) => {
  const [formData, setFormData] = useState({
    name: 'Sample Fixed Annuity',
    premium: 100000,
    guaranteed_rate: 0.04,
    term_years: 10,
    surrender_charge_years: 7,
  });

  return (
    <form onSubmit={(e) => { e.preventDefault(); onSubmit({...formData, asset_class: 'annuity', currency: 'USD'}); }} className="space-y-4">
      <div className="grid grid-cols-2 gap-4">
        <input type="text" placeholder="Annuity Name" value={formData.name} onChange={(e) => setFormData({...formData, name: e.target.value})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" placeholder="Premium ($)" value={formData.premium} onChange={(e) => setFormData({...formData, premium: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.01" placeholder="Guaranteed Rate (%)" value={formData.guaranteed_rate * 100} onChange={(e) => setFormData({...formData, guaranteed_rate: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" placeholder="Term (Years)" value={formData.term_years} onChange={(e) => setFormData({...formData, term_years: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" placeholder="Surrender Charge Period (Years)" value={formData.surrender_charge_years} onChange={(e) => setFormData({...formData, surrender_charge_years: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
      </div>
      <button type="submit" disabled={isLoading} className="w-full bg-cyan-600 hover:bg-cyan-500 disabled:bg-gray-600 text-white font-semibold py-3 px-4 rounded-lg">
        {isLoading ? 'Analyzing...' : '🔍 Analyze Fixed Annuity'}
      </button>
    </form>
  );
};

const VariableAnnuityForm: React.FC<{onSubmit: any; isLoading: boolean}> = ({ onSubmit, isLoading }) => {
  const [formData, setFormData] = useState({
    name: 'Sample Variable Annuity',
    premium: 100000,
    annual_fee: 0.015,
    subaccount_returns: 0.08,
    mortality_expense: 0.012,
    admin_fee: 0.003,
  });

  return (
    <form onSubmit={(e) => { e.preventDefault(); onSubmit({...formData, asset_class: 'annuity', currency: 'USD'}); }} className="space-y-4">
      <div className="grid grid-cols-2 gap-4">
        <input type="text" placeholder="Annuity Name" value={formData.name} onChange={(e) => setFormData({...formData, name: e.target.value})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" placeholder="Premium ($)" value={formData.premium} onChange={(e) => setFormData({...formData, premium: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.001" placeholder="Annual Fee (%)" value={formData.annual_fee * 100} onChange={(e) => setFormData({...formData, annual_fee: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.01" placeholder="Expected Subaccount Return (%)" value={formData.subaccount_returns * 100} onChange={(e) => setFormData({...formData, subaccount_returns: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.001" placeholder="M&E Expense (%)" value={formData.mortality_expense * 100} onChange={(e) => setFormData({...formData, mortality_expense: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.001" placeholder="Admin Fee (%)" value={formData.admin_fee * 100} onChange={(e) => setFormData({...formData, admin_fee: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
      </div>
      <button type="submit" disabled={isLoading} className="w-full bg-cyan-600 hover:bg-cyan-500 disabled:bg-gray-600 text-white font-semibold py-3 px-4 rounded-lg">
        {isLoading ? 'Analyzing...' : '🔍 Analyze Variable Annuity'}
      </button>
    </form>
  );
};

const EquityIndexedAnnuityForm: React.FC<{onSubmit: any; isLoading: boolean}> = ({ onSubmit, isLoading }) => {
  const [formData, setFormData] = useState({
    name: 'Sample Equity-Indexed Annuity',
    premium: 100000,
    participation_rate: 0.80,
    cap_rate: 0.10,
    floor_rate: 0.01,
    annual_fee: 0.015,
  });

  return (
    <form onSubmit={(e) => { e.preventDefault(); onSubmit({...formData, asset_class: 'annuity', currency: 'USD'}); }} className="space-y-4">
      <div className="grid grid-cols-2 gap-4">
        <input type="text" placeholder="Annuity Name" value={formData.name} onChange={(e) => setFormData({...formData, name: e.target.value})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" placeholder="Premium ($)" value={formData.premium} onChange={(e) => setFormData({...formData, premium: Number(e.target.value)})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.01" placeholder="Participation Rate (%)" value={formData.participation_rate * 100} onChange={(e) => setFormData({...formData, participation_rate: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.01" placeholder="Cap Rate (%)" value={formData.cap_rate * 100} onChange={(e) => setFormData({...formData, cap_rate: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.01" placeholder="Floor Rate (%)" value={formData.floor_rate * 100} onChange={(e) => setFormData({...formData, floor_rate: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
        <input type="number" step="0.001" placeholder="Annual Fee (%)" value={formData.annual_fee * 100} onChange={(e) => setFormData({...formData, annual_fee: Number(e.target.value) / 100})} className="bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white" />
      </div>
      <button type="submit" disabled={isLoading} className="w-full bg-cyan-600 hover:bg-cyan-500 disabled:bg-gray-600 text-white font-semibold py-3 px-4 rounded-lg">
        {isLoading ? 'Analyzing...' : '🔍 Analyze Equity-Indexed Annuity'}
      </button>
    </form>
  );
};

export default AnnuitiesView;
