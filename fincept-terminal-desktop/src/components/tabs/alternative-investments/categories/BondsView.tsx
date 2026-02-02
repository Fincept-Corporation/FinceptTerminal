import React, { useState } from 'react';
import { AlternativeInvestmentApi } from '@/services/alternativeInvestments/api/analyticsApi';
import {
  HighYieldBondParams,
  EmergingMarketBondParams,
  ConvertibleBondParams,
  PreferredStockParams,
  CreditRating,
} from '@/services/alternativeInvestments/api/types';
import VerdictPanel from '../components/VerdictPanel';

type BondType = 'high-yield' | 'emerging-market' | 'convertible' | 'preferred-stock';

/**
 * BondsView - Analysis for bond-related alternative investments
 * Covers: High-Yield, Emerging Market, Convertible Bonds, Preferred Stocks
 */
export const BondsView: React.FC = () => {
  const [selectedBond, setSelectedBond] = useState<BondType>('high-yield');
  const [isAnalyzing, setIsAnalyzing] = useState(false);
  const [analysis, setAnalysis] = useState<any>(null);
  const [error, setError] = useState<string | null>(null);

  const handleAnalyze = async (data: any) => {
    setIsAnalyzing(true);
    setError(null);

    try {
      let result;

      switch (selectedBond) {
        case 'high-yield':
          result = await AlternativeInvestmentApi.analyzeHighYieldBond(data as HighYieldBondParams);
          break;
        case 'emerging-market':
          result = await AlternativeInvestmentApi.analyzeEmergingMarketBond(data as EmergingMarketBondParams);
          break;
        case 'convertible':
          result = await AlternativeInvestmentApi.analyzeConvertibleBond(data as ConvertibleBondParams);
          break;
        case 'preferred-stock':
          result = await AlternativeInvestmentApi.analyzePreferredStock(data as PreferredStockParams);
          break;
      }

      setAnalysis(result);
    } catch (err: any) {
      setError(err.message || 'Analysis failed');
      console.error('Analysis error:', err);
    } finally {
      setIsAnalyzing(false);
    }
  };

  return (
    <div className="bonds-view space-y-6">
      {/* Bond Type Selector */}
      <div className="bg-gray-800/50 rounded-lg p-4 border border-gray-700">
        <h3 className="text-lg font-semibold mb-4">Select Bond Type</h3>
        <div className="grid grid-cols-2 md:grid-cols-4 gap-3">
          <BondTypeButton
            type="high-yield"
            label="High-Yield Bonds"
            icon="📊"
            selected={selectedBond === 'high-yield'}
            onClick={() => setSelectedBond('high-yield')}
          />
          <BondTypeButton
            type="emerging-market"
            label="Emerging Market"
            icon="🌍"
            selected={selectedBond === 'emerging-market'}
            onClick={() => setSelectedBond('emerging-market')}
          />
          <BondTypeButton
            type="convertible"
            label="Convertible Bonds"
            icon="🔄"
            selected={selectedBond === 'convertible'}
            onClick={() => setSelectedBond('convertible')}
          />
          <BondTypeButton
            type="preferred-stock"
            label="Preferred Stocks"
            icon="⭐"
            selected={selectedBond === 'preferred-stock'}
            onClick={() => setSelectedBond('preferred-stock')}
          />
        </div>
      </div>

      {/* Input Form */}
      <div className="bg-gray-800/50 rounded-lg p-6 border border-gray-700">
        <h3 className="text-lg font-semibold mb-4">Bond Details</h3>
        {selectedBond === 'high-yield' && (
          <HighYieldBondForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />
        )}
        {selectedBond === 'emerging-market' && (
          <EmergingMarketBondForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />
        )}
        {selectedBond === 'convertible' && (
          <ConvertibleBondForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />
        )}
        {selectedBond === 'preferred-stock' && (
          <PreferredStockForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />
        )}
      </div>

      {/* Error Display */}
      {error && (
        <div className="bg-red-900/20 border border-red-500 rounded-lg p-4">
          <p className="text-red-400">❌ {error}</p>
        </div>
      )}

      {/* Analysis Results */}
      {analysis && analysis.success && analysis.metrics && (
        <VerdictPanel verdict={analysis.metrics} />
      )}
    </div>
  );
};

/**
 * Bond Type Button
 */
interface BondTypeButtonProps {
  type: BondType;
  label: string;
  icon: string;
  selected: boolean;
  onClick: () => void;
}

const BondTypeButton: React.FC<BondTypeButtonProps> = ({
  label,
  icon,
  selected,
  onClick,
}) => {
  return (
    <button
      onClick={onClick}
      className={`
        p-4 rounded-lg border-2 transition-all
        ${
          selected
            ? 'bg-blue-900/30 border-blue-500 shadow-lg'
            : 'bg-gray-900/30 border-gray-600 hover:border-gray-500'
        }
      `}
    >
      <div className="text-2xl mb-2">{icon}</div>
      <div className="text-sm font-medium">{label}</div>
    </button>
  );
};

/**
 * High-Yield Bond Input Form
 */
interface BondFormProps {
  onSubmit: (data: any) => void;
  isLoading: boolean;
}

const HighYieldBondForm: React.FC<BondFormProps> = ({ onSubmit, isLoading }) => {
  const [formData, setFormData] = useState({
    name: 'Sample High-Yield Bond',
    face_value: 1000,
    coupon_rate: 0.08,
    current_price: 950,
    credit_rating: 'BB' as CreditRating,
    maturity_years: 5,
    sector: 'Industrial',
  });

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    onSubmit({
      ...formData,
      asset_class: 'fixed_income',
      currency: 'USD',
    } as HighYieldBondParams);
  };

  return (
    <form onSubmit={handleSubmit} className="space-y-4">
      <div className="grid grid-cols-2 gap-4">
        <FormInput
          label="Bond Name"
          value={formData.name}
          onChange={(e) => setFormData({ ...formData, name: e.target.value })}
        />
        <FormInput
          label="Face Value ($)"
          type="number"
          value={formData.face_value}
          onChange={(e) => setFormData({ ...formData, face_value: Number(e.target.value) })}
        />
        <FormInput
          label="Coupon Rate (%)"
          type="number"
          step="0.01"
          value={formData.coupon_rate * 100}
          onChange={(e) => setFormData({ ...formData, coupon_rate: Number(e.target.value) / 100 })}
        />
        <FormInput
          label="Current Price ($)"
          type="number"
          value={formData.current_price}
          onChange={(e) => setFormData({ ...formData, current_price: Number(e.target.value) })}
        />
        <div>
          <label className="block text-sm font-medium text-gray-300 mb-2">Credit Rating</label>
          <select
            value={formData.credit_rating}
            onChange={(e) => setFormData({ ...formData, credit_rating: e.target.value as CreditRating })}
            className="w-full bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white"
          >
            <option value="BBB">BBB (Investment Grade)</option>
            <option value="BB">BB (High Yield)</option>
            <option value="B">B (High Yield)</option>
            <option value="CCC">CCC (High Yield)</option>
          </select>
        </div>
        <FormInput
          label="Maturity (Years)"
          type="number"
          value={formData.maturity_years}
          onChange={(e) => setFormData({ ...formData, maturity_years: Number(e.target.value) })}
        />
      </div>

      <button
        type="submit"
        disabled={isLoading}
        className="w-full bg-blue-600 hover:bg-blue-500 disabled:bg-gray-600 disabled:cursor-not-allowed text-white font-semibold py-3 px-4 rounded-lg transition-colors"
      >
        {isLoading ? 'Analyzing...' : '🔍 Analyze Bond'}
      </button>
    </form>
  );
};

/**
 * Placeholder forms for other bond types (to be implemented)
 */
const EmergingMarketBondForm: React.FC<BondFormProps> = ({ onSubmit, isLoading }) => (
  <PlaceholderForm type="Emerging Market Bond" onSubmit={onSubmit} isLoading={isLoading} />
);

const ConvertibleBondForm: React.FC<BondFormProps> = ({ onSubmit, isLoading }) => (
  <PlaceholderForm type="Convertible Bond" onSubmit={onSubmit} isLoading={isLoading} />
);

const PreferredStockForm: React.FC<BondFormProps> = ({ onSubmit, isLoading }) => (
  <PlaceholderForm type="Preferred Stock" onSubmit={onSubmit} isLoading={isLoading} />
);

const PlaceholderForm: React.FC<{ type: string; onSubmit: any; isLoading: boolean }> = ({ type }) => (
  <div className="text-center p-8 text-gray-400">
    <p>{type} form will be implemented in the next phase</p>
  </div>
);

/**
 * Reusable Form Input Component
 */
interface FormInputProps {
  label: string;
  value: string | number;
  onChange: (e: React.ChangeEvent<HTMLInputElement>) => void;
  type?: string;
  step?: string;
}

const FormInput: React.FC<FormInputProps> = ({ label, value, onChange, type = 'text', step }) => (
  <div>
    <label className="block text-sm font-medium text-gray-300 mb-2">{label}</label>
    <input
      type={type}
      value={value}
      onChange={onChange}
      step={step}
      className="w-full bg-gray-900 border border-gray-600 rounded-lg px-3 py-2 text-white focus:border-blue-500 focus:outline-none"
    />
  </div>
);

export default BondsView;
