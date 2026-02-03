import React, { useState } from 'react';
import { AlternativeInvestmentApi } from '@/services/alternativeInvestments/api/analyticsApi';

const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  GREEN: '#00D66F',
  RED: '#FF4444',
};

type StrategyType = 'covered-calls' | 'sri-funds' | 'asset-location' | 'performance' | 'risk';

const TYPES = [
  { id: 'covered-calls' as const, name: 'Covered Calls' },
  { id: 'sri-funds' as const, name: 'SRI Funds' },
  { id: 'asset-location' as const, name: 'Asset Location' },
  { id: 'performance' as const, name: 'Performance' },
  { id: 'risk' as const, name: 'Risk' },
];

export const StrategiesView: React.FC = () => {
  const [selected, setSelected] = useState<StrategyType>('covered-calls');
  const [isAnalyzing, setIsAnalyzing] = useState(false);
  const [analysis, setAnalysis] = useState<any>(null);
  const [error, setError] = useState<string | null>(null);

  const [coveredCallData, setCoveredCallData] = useState({
    name: 'Covered Call Strategy',
    stock_price: 100,
    shares_owned: 100,
    strike_price: 105,
    premium_received: 2.5,
    days_to_expiration: 30,
  });

  const [sriData, setSriData] = useState({
    name: 'ESG Fund',
    expense_ratio: 0.008,
    esg_rating: 8.5,
    annual_return: 0.10,
    tracking_error: 0.015,
  });

  const [assetLocationData, setAssetLocationData] = useState({
    tax_bracket: 0.24,
    portfolio_value: 1000000,
    taxable_account_value: 400000,
    tax_deferred_value: 400000,
    tax_free_value: 200000,
  });

  const [performanceData, setPerformanceData] = useState({
    portfolio_returns: '0.08,0.12,-0.05,0.15,0.10',
    benchmark_returns: '0.07,0.10,-0.03,0.12,0.09',
    risk_free_rate: 0.03,
  });

  const [riskData, setRiskData] = useState({
    portfolio_returns: '0.08,0.12,-0.05,0.15,0.10,-0.02,0.09,0.11',
    confidence_level: 0.95,
    time_horizon: 252,
  });

  const handleAnalyze = async (e: React.FormEvent) => {
    e.preventDefault();
    setIsAnalyzing(true);
    setError(null);
    setAnalysis(null);

    try {
      let result;
      const baseData = { asset_class: 'equity', currency: 'USD' };

      switch (selected) {
        case 'covered-calls':
          result = await AlternativeInvestmentApi.analyzeCoveredCall({ ...coveredCallData, ...baseData });
          break;
        case 'sri-funds':
          result = await AlternativeInvestmentApi.analyzeSRIFund({ ...sriData, ...baseData });
          break;
        case 'asset-location':
          result = await AlternativeInvestmentApi.analyzeAssetLocation(assetLocationData);
          break;
        case 'performance':
          result = await AlternativeInvestmentApi.analyzePerformance(performanceData);
          break;
        case 'risk':
          result = await AlternativeInvestmentApi.analyzeRisk(riskData);
          break;
      }
      setAnalysis(result);
    } catch (err: any) {
      setError(err.message);
    } finally {
      setIsAnalyzing(false);
    }
  };

  const renderVerdict = () => {
    if (!analysis?.success || !analysis.metrics) return null;

    const category = analysis.metrics.analysis_category || 'UNKNOWN';
    const recommendation = analysis.metrics.analysis_recommendation || 'No recommendation';
    const keyProblems = analysis.metrics.key_problems || [];

    let badgeColor = FINCEPT.GRAY;
    if (category === 'BUY' || category === 'STRONG BUY') badgeColor = FINCEPT.GREEN;
    if (category === 'SELL' || category === 'STRONG SELL') badgeColor = FINCEPT.RED;
    if (category === 'HOLD') badgeColor = FINCEPT.ORANGE;

    return (
      <div style={{ padding: '16px', display: 'flex', flexDirection: 'column', gap: '16px' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <div style={{ padding: '4px 10px', backgroundColor: badgeColor, borderRadius: '4px', fontSize: '10px', fontWeight: 700, color: FINCEPT.DARK_BG, letterSpacing: '0.5px' }}>
            {category}
          </div>
        </div>
        <div>
          <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.MUTED, marginBottom: '6px', letterSpacing: '0.5px' }}>RECOMMENDATION</div>
          <div style={{ fontSize: '11px', color: FINCEPT.WHITE, lineHeight: '1.5' }}>
            {recommendation}
          </div>
        </div>
        {keyProblems.length > 0 && (
          <div>
            <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.MUTED, marginBottom: '6px', letterSpacing: '0.5px' }}>KEY CONCERNS</div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
              {keyProblems.map((problem: string, idx: number) => (
                <div key={idx} style={{ fontSize: '10px', color: FINCEPT.GRAY, display: 'flex', alignItems: 'flex-start', gap: '6px' }}>
                  <span style={{ color: FINCEPT.RED, flexShrink: 0 }}>•</span>
                  <span>{problem}</span>
                </div>
              ))}
            </div>
          </div>
        )}
      </div>
    );
  };

  const renderForm = () => {
    if (selected === 'covered-calls') {
      return (
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Strategy Name</label>
            <input
              type="text"
              value={coveredCallData.name}
              onChange={(e) => setCoveredCallData({...coveredCallData, name: e.target.value})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Stock Price ($)</label>
            <input
              type="number"
              step="0.01"
              value={coveredCallData.stock_price}
              onChange={(e) => setCoveredCallData({...coveredCallData, stock_price: Number(e.target.value)})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Shares Owned</label>
            <input
              type="number"
              value={coveredCallData.shares_owned}
              onChange={(e) => setCoveredCallData({...coveredCallData, shares_owned: Number(e.target.value)})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Strike Price ($)</label>
            <input
              type="number"
              step="0.01"
              value={coveredCallData.strike_price}
              onChange={(e) => setCoveredCallData({...coveredCallData, strike_price: Number(e.target.value)})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Premium Received ($)</label>
            <input
              type="number"
              step="0.01"
              value={coveredCallData.premium_received}
              onChange={(e) => setCoveredCallData({...coveredCallData, premium_received: Number(e.target.value)})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Days to Expiration</label>
            <input
              type="number"
              value={coveredCallData.days_to_expiration}
              onChange={(e) => setCoveredCallData({...coveredCallData, days_to_expiration: Number(e.target.value)})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
        </div>
      );
    }

    if (selected === 'sri-funds') {
      return (
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Fund Name</label>
            <input
              type="text"
              value={sriData.name}
              onChange={(e) => setSriData({...sriData, name: e.target.value})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Expense Ratio (%)</label>
            <input
              type="number"
              step="0.0001"
              value={sriData.expense_ratio * 100}
              onChange={(e) => setSriData({...sriData, expense_ratio: Number(e.target.value) / 100})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>ESG Rating (1-10)</label>
            <input
              type="number"
              step="0.1"
              value={sriData.esg_rating}
              onChange={(e) => setSriData({...sriData, esg_rating: Number(e.target.value)})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Annual Return (%)</label>
            <input
              type="number"
              step="0.01"
              value={sriData.annual_return * 100}
              onChange={(e) => setSriData({...sriData, annual_return: Number(e.target.value) / 100})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px', gridColumn: 'span 2' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Tracking Error (%)</label>
            <input
              type="number"
              step="0.001"
              value={sriData.tracking_error * 100}
              onChange={(e) => setSriData({...sriData, tracking_error: Number(e.target.value) / 100})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
        </div>
      );
    }

    if (selected === 'asset-location') {
      return (
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Tax Bracket (%)</label>
            <input
              type="number"
              step="0.01"
              value={assetLocationData.tax_bracket * 100}
              onChange={(e) => setAssetLocationData({...assetLocationData, tax_bracket: Number(e.target.value) / 100})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Total Portfolio Value ($)</label>
            <input
              type="number"
              value={assetLocationData.portfolio_value}
              onChange={(e) => setAssetLocationData({...assetLocationData, portfolio_value: Number(e.target.value)})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Taxable Account ($)</label>
            <input
              type="number"
              value={assetLocationData.taxable_account_value}
              onChange={(e) => setAssetLocationData({...assetLocationData, taxable_account_value: Number(e.target.value)})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Tax-Deferred Account ($)</label>
            <input
              type="number"
              value={assetLocationData.tax_deferred_value}
              onChange={(e) => setAssetLocationData({...assetLocationData, tax_deferred_value: Number(e.target.value)})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px', gridColumn: 'span 2' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Tax-Free Account ($)</label>
            <input
              type="number"
              value={assetLocationData.tax_free_value}
              onChange={(e) => setAssetLocationData({...assetLocationData, tax_free_value: Number(e.target.value)})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
        </div>
      );
    }

    if (selected === 'performance') {
      return (
        <div style={{ display: 'grid', gridTemplateColumns: '1fr', gap: '12px' }}>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Portfolio Returns (comma-separated)</label>
            <input
              type="text"
              value={performanceData.portfolio_returns}
              onChange={(e) => setPerformanceData({...performanceData, portfolio_returns: e.target.value})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
              placeholder="0.08,0.12,-0.05,0.15,0.10"
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Benchmark Returns (comma-separated)</label>
            <input
              type="text"
              value={performanceData.benchmark_returns}
              onChange={(e) => setPerformanceData({...performanceData, benchmark_returns: e.target.value})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
              placeholder="0.07,0.10,-0.03,0.12,0.09"
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Risk-Free Rate (%)</label>
            <input
              type="number"
              step="0.01"
              value={performanceData.risk_free_rate * 100}
              onChange={(e) => setPerformanceData({...performanceData, risk_free_rate: Number(e.target.value) / 100})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
        </div>
      );
    }

    // risk
    return (
      <div style={{ display: 'grid', gridTemplateColumns: '1fr', gap: '12px' }}>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
          <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Portfolio Returns (comma-separated)</label>
          <input
            type="text"
            value={riskData.portfolio_returns}
            onChange={(e) => setRiskData({...riskData, portfolio_returns: e.target.value})}
            style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            placeholder="0.08,0.12,-0.05,0.15,0.10,-0.02,0.09,0.11"
          />
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
          <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Confidence Level (%)</label>
          <input
            type="number"
            step="0.01"
            value={riskData.confidence_level * 100}
            onChange={(e) => setRiskData({...riskData, confidence_level: Number(e.target.value) / 100})}
            style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
          />
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
          <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Time Horizon (Days)</label>
          <input
            type="number"
            value={riskData.time_horizon}
            onChange={(e) => setRiskData({...riskData, time_horizon: Number(e.target.value)})}
            style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
          />
        </div>
      </div>
    );
  };

  return (
    <div style={{ display: 'flex', height: '100%', fontFamily: '"IBM Plex Mono", monospace', backgroundColor: FINCEPT.DARK_BG }}>

      {/* LEFT: Form Section */}
      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden', borderRight: `1px solid ${FINCEPT.BORDER}` }}>

        {/* Type Selector - Compact Horizontal */}
        <div style={{ padding: '12px 16px', backgroundColor: FINCEPT.PANEL_BG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
          <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.MUTED, marginBottom: '8px', letterSpacing: '0.5px' }}>STRATEGY TYPE</div>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(5, 1fr)', gap: '6px' }}>
            {TYPES.map((type) => {
              const isActive = selected === type.id;
              return (
                <button
                  key={type.id}
                  type="button"
                  onClick={() => setSelected(type.id)}
                  style={{
                    padding: '8px 12px',
                    backgroundColor: isActive ? `${FINCEPT.ORANGE}20` : FINCEPT.PANEL_BG,
                    border: `1px solid ${isActive ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                    borderRadius: '4px',
                    fontSize: '10px',
                    fontWeight: 600,
                    color: isActive ? FINCEPT.ORANGE : FINCEPT.GRAY,
                    cursor: 'pointer',
                    transition: 'all 0.2s',
                  }}
                  onMouseEnter={(e) => {
                    if (!isActive) e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                  }}
                  onMouseLeave={(e) => {
                    if (!isActive) e.currentTarget.style.backgroundColor = FINCEPT.PANEL_BG;
                  }}
                >
                  {type.name}
                </button>
              );
            })}
          </div>
        </div>

        {/* Input Form */}
        <form onSubmit={handleAnalyze} style={{ flex: 1, overflow: 'auto', padding: '16px', display: 'flex', flexDirection: 'column', gap: '16px' }}>

          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.MUTED, letterSpacing: '0.5px' }}>STRATEGY DETAILS</div>
            {renderForm()}
          </div>

          <button
            type="submit"
            disabled={isAnalyzing}
            style={{
              marginTop: 'auto',
              padding: '10px',
              backgroundColor: isAnalyzing ? FINCEPT.MUTED : FINCEPT.ORANGE,
              color: FINCEPT.DARK_BG,
              border: 'none',
              borderRadius: '4px',
              fontSize: '11px',
              fontWeight: 700,
              cursor: isAnalyzing ? 'not-allowed' : 'pointer',
              letterSpacing: '0.5px',
            }}
          >
            {isAnalyzing ? 'ANALYZING...' : 'ANALYZE STRATEGY'}
          </button>
        </form>
      </div>

      {/* RIGHT: Results Section */}
      <div style={{ width: '480px', display: 'flex', flexDirection: 'column', overflow: 'hidden', backgroundColor: FINCEPT.PANEL_BG }}>
        <div style={{ padding: '12px 16px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
          <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.MUTED, letterSpacing: '0.5px' }}>ANALYSIS RESULTS</div>
        </div>
        <div style={{ flex: 1, overflow: 'auto' }}>
          {isAnalyzing && (
            <div style={{ padding: '24px', display: 'flex', alignItems: 'center', justifyContent: 'center', flexDirection: 'column', gap: '12px' }}>
              <div style={{ width: '24px', height: '24px', border: `2px solid ${FINCEPT.BORDER}`, borderTopColor: FINCEPT.ORANGE, borderRadius: '50%', animation: 'spin 1s linear infinite' }} />
              <div style={{ fontSize: '10px', color: FINCEPT.GRAY }}>Running strategy analytics...</div>
            </div>
          )}
          {error && (
            <div style={{ margin: '16px', padding: '12px', backgroundColor: `${FINCEPT.RED}20`, border: `1px solid ${FINCEPT.RED}`, borderRadius: '4px' }}>
              <div style={{ fontSize: '10px', color: FINCEPT.RED }}>{error}</div>
            </div>
          )}
          {!isAnalyzing && !error && !analysis && (
            <div style={{ padding: '24px', textAlign: 'center' }}>
              <div style={{ fontSize: '10px', color: FINCEPT.MUTED }}>Select strategy type and enter details to see results</div>
            </div>
          )}
          {renderVerdict()}
        </div>
      </div>

      <style>{`
        @keyframes spin {
          to { transform: rotate(360deg); }
        }
      `}</style>
    </div>
  );
};

export default StrategiesView;
