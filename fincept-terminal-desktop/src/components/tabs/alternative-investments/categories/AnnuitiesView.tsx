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
  CYAN: '#00E5FF',
};

type AnnuityType = 'fixed' | 'variable' | 'equity-indexed' | 'inflation-annuity';

const TYPES = [
  { id: 'fixed' as const, name: 'Fixed Annuity' },
  { id: 'variable' as const, name: 'Variable Annuity' },
  { id: 'equity-indexed' as const, name: 'Equity-Indexed' },
  { id: 'inflation-annuity' as const, name: 'Inflation Annuity' },
];

export const AnnuitiesView: React.FC = () => {
  const [selected, setSelected] = useState<AnnuityType>('fixed');
  const [isAnalyzing, setIsAnalyzing] = useState(false);
  const [analysis, setAnalysis] = useState<any>(null);
  const [error, setError] = useState<string | null>(null);

  const [fixedData, setFixedData] = useState({
    name: 'Sample Fixed Annuity',
    premium: 100000,
    guaranteed_rate: 0.04,
    term_years: 10,
    surrender_charge_years: 7,
  });

  const [variableData, setVariableData] = useState({
    name: 'Sample Variable Annuity',
    premium: 100000,
    annual_fee: 0.015,
    subaccount_returns: 0.08,
    mortality_expense: 0.012,
    admin_fee: 0.003,
  });

  const [equityIndexedData, setEquityIndexedData] = useState({
    name: 'Sample Equity-Indexed Annuity',
    premium: 100000,
    participation_rate: 0.80,
    cap_rate: 0.10,
    floor_rate: 0.01,
    annual_fee: 0.015,
  });

  const [inflationAnnuityData, setInflationAnnuityData] = useState({
    name: 'Sample Inflation-Protected Annuity',
    premium: 100000,
    base_payout: 5000,
    inflation_adjustment: 0.025,
    guarantee_years: 20,
    rider_cost: 0.005,
  });

  const handleAnalyze = async (e: React.FormEvent) => {
    e.preventDefault();
    setIsAnalyzing(true);
    setError(null);
    setAnalysis(null);

    try {
      let result;
      const baseData = { asset_class: 'annuity', currency: 'USD' };

      switch (selected) {
        case 'fixed':
          result = await AlternativeInvestmentApi.analyzeFixedAnnuity({ ...fixedData, ...baseData });
          break;
        case 'variable':
          result = await AlternativeInvestmentApi.analyzeVariableAnnuity({ ...variableData, ...baseData });
          break;
        case 'equity-indexed':
          result = await AlternativeInvestmentApi.analyzeEquityIndexedAnnuity({ ...equityIndexedData, ...baseData });
          break;
        case 'inflation-annuity':
          result = await AlternativeInvestmentApi.analyzeInflationAnnuity({ ...inflationAnnuityData, ...baseData });
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
    if (selected === 'fixed') {
      return (
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Annuity Name</label>
            <input
              type="text"
              value={fixedData.name}
              onChange={(e) => setFixedData({...fixedData, name: e.target.value})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Premium ($)</label>
            <input
              type="number"
              value={fixedData.premium}
              onChange={(e) => setFixedData({...fixedData, premium: Number(e.target.value)})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Guaranteed Rate (%)</label>
            <input
              type="number"
              step="0.01"
              value={fixedData.guaranteed_rate * 100}
              onChange={(e) => setFixedData({...fixedData, guaranteed_rate: Number(e.target.value) / 100})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Term (Years)</label>
            <input
              type="number"
              value={fixedData.term_years}
              onChange={(e) => setFixedData({...fixedData, term_years: Number(e.target.value)})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px', gridColumn: 'span 2' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Surrender Charge Period (Years)</label>
            <input
              type="number"
              value={fixedData.surrender_charge_years}
              onChange={(e) => setFixedData({...fixedData, surrender_charge_years: Number(e.target.value)})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
        </div>
      );
    }

    if (selected === 'variable') {
      return (
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Annuity Name</label>
            <input
              type="text"
              value={variableData.name}
              onChange={(e) => setVariableData({...variableData, name: e.target.value})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Premium ($)</label>
            <input
              type="number"
              value={variableData.premium}
              onChange={(e) => setVariableData({...variableData, premium: Number(e.target.value)})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Annual Fee (%)</label>
            <input
              type="number"
              step="0.001"
              value={variableData.annual_fee * 100}
              onChange={(e) => setVariableData({...variableData, annual_fee: Number(e.target.value) / 100})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Expected Subaccount Return (%)</label>
            <input
              type="number"
              step="0.01"
              value={variableData.subaccount_returns * 100}
              onChange={(e) => setVariableData({...variableData, subaccount_returns: Number(e.target.value) / 100})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>M&E Expense (%)</label>
            <input
              type="number"
              step="0.001"
              value={variableData.mortality_expense * 100}
              onChange={(e) => setVariableData({...variableData, mortality_expense: Number(e.target.value) / 100})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Admin Fee (%)</label>
            <input
              type="number"
              step="0.001"
              value={variableData.admin_fee * 100}
              onChange={(e) => setVariableData({...variableData, admin_fee: Number(e.target.value) / 100})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
        </div>
      );
    }

    if (selected === 'equity-indexed') {
      return (
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Annuity Name</label>
            <input
              type="text"
              value={equityIndexedData.name}
              onChange={(e) => setEquityIndexedData({...equityIndexedData, name: e.target.value})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Premium ($)</label>
            <input
              type="number"
              value={equityIndexedData.premium}
              onChange={(e) => setEquityIndexedData({...equityIndexedData, premium: Number(e.target.value)})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Participation Rate (%)</label>
            <input
              type="number"
              step="0.01"
              value={equityIndexedData.participation_rate * 100}
              onChange={(e) => setEquityIndexedData({...equityIndexedData, participation_rate: Number(e.target.value) / 100})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Cap Rate (%)</label>
            <input
              type="number"
              step="0.01"
              value={equityIndexedData.cap_rate * 100}
              onChange={(e) => setEquityIndexedData({...equityIndexedData, cap_rate: Number(e.target.value) / 100})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Floor Rate (%)</label>
            <input
              type="number"
              step="0.01"
              value={equityIndexedData.floor_rate * 100}
              onChange={(e) => setEquityIndexedData({...equityIndexedData, floor_rate: Number(e.target.value) / 100})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Annual Fee (%)</label>
            <input
              type="number"
              step="0.001"
              value={equityIndexedData.annual_fee * 100}
              onChange={(e) => setEquityIndexedData({...equityIndexedData, annual_fee: Number(e.target.value) / 100})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
        </div>
      );
    }

    // inflation-annuity
    return (
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
          <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Annuity Name</label>
          <input
            type="text"
            value={inflationAnnuityData.name}
            onChange={(e) => setInflationAnnuityData({...inflationAnnuityData, name: e.target.value})}
            style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
          />
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
          <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Premium ($)</label>
          <input
            type="number"
            value={inflationAnnuityData.premium}
            onChange={(e) => setInflationAnnuityData({...inflationAnnuityData, premium: Number(e.target.value)})}
            style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
          />
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
          <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Base Payout ($/year)</label>
          <input
            type="number"
            value={inflationAnnuityData.base_payout}
            onChange={(e) => setInflationAnnuityData({...inflationAnnuityData, base_payout: Number(e.target.value)})}
            style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
          />
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
          <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Inflation Adjustment (%)</label>
          <input
            type="number"
            step="0.001"
            value={inflationAnnuityData.inflation_adjustment * 100}
            onChange={(e) => setInflationAnnuityData({...inflationAnnuityData, inflation_adjustment: Number(e.target.value) / 100})}
            style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
          />
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
          <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Guarantee Period (Years)</label>
          <input
            type="number"
            value={inflationAnnuityData.guarantee_years}
            onChange={(e) => setInflationAnnuityData({...inflationAnnuityData, guarantee_years: Number(e.target.value)})}
            style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
          />
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
          <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Rider Cost (%)</label>
          <input
            type="number"
            step="0.001"
            value={inflationAnnuityData.rider_cost * 100}
            onChange={(e) => setInflationAnnuityData({...inflationAnnuityData, rider_cost: Number(e.target.value) / 100})}
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
          <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.MUTED, marginBottom: '8px', letterSpacing: '0.5px' }}>ANNUITY TYPE</div>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '6px' }}>
            {TYPES.map((type) => {
              const isActive = selected === type.id;
              return (
                <button
                  key={type.id}
                  type="button"
                  onClick={() => setSelected(type.id)}
                  style={{
                    padding: '8px 12px',
                    backgroundColor: isActive ? `${FINCEPT.CYAN}20` : FINCEPT.PANEL_BG,
                    border: `1px solid ${isActive ? FINCEPT.CYAN : FINCEPT.BORDER}`,
                    borderRadius: '4px',
                    fontSize: '10px',
                    fontWeight: 600,
                    color: isActive ? FINCEPT.CYAN : FINCEPT.GRAY,
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
            <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.MUTED, letterSpacing: '0.5px' }}>ANNUITY DETAILS</div>
            {renderForm()}
          </div>

          <button
            type="submit"
            disabled={isAnalyzing}
            style={{
              marginTop: 'auto',
              padding: '10px',
              backgroundColor: isAnalyzing ? FINCEPT.MUTED : FINCEPT.CYAN,
              color: FINCEPT.DARK_BG,
              border: 'none',
              borderRadius: '4px',
              fontSize: '11px',
              fontWeight: 700,
              cursor: isAnalyzing ? 'not-allowed' : 'pointer',
              letterSpacing: '0.5px',
            }}
          >
            {isAnalyzing ? 'ANALYZING...' : 'ANALYZE ANNUITY'}
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
              <div style={{ width: '24px', height: '24px', border: `2px solid ${FINCEPT.BORDER}`, borderTopColor: FINCEPT.CYAN, borderRadius: '50%', animation: 'spin 1s linear infinite' }} />
              <div style={{ fontSize: '10px', color: FINCEPT.GRAY }}>Running annuity analytics...</div>
            </div>
          )}
          {error && (
            <div style={{ margin: '16px', padding: '12px', backgroundColor: `${FINCEPT.RED}20`, border: `1px solid ${FINCEPT.RED}`, borderRadius: '4px' }}>
              <div style={{ fontSize: '10px', color: FINCEPT.RED }}>{error}</div>
            </div>
          )}
          {!isAnalyzing && !error && !analysis && (
            <div style={{ padding: '24px', textAlign: 'center' }}>
              <div style={{ fontSize: '10px', color: FINCEPT.MUTED }}>Select annuity type and enter details to see results</div>
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

export default AnnuitiesView;
