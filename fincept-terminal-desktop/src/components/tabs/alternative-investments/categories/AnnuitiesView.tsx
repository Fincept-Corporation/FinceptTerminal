import React, { useState, useReducer, useRef, useEffect } from 'react';
import { AlternativeInvestmentApi } from '@/services/alternativeInvestments/api/analyticsApi';
import { withTimeout } from '@/services/core/apiUtils';
import { ErrorBoundary } from '@/components/common/ErrorBoundary';

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

type AnalysisState =
  | { status: 'idle' }
  | { status: 'loading' }
  | { status: 'success'; data: any }
  | { status: 'error'; message: string };
type AnalysisAction =
  | { type: 'START' }
  | { type: 'SUCCESS'; payload: any }
  | { type: 'ERROR'; payload: string }
  | { type: 'RESET' };
function analysisReducer(state: AnalysisState, action: AnalysisAction): AnalysisState {
  switch (action.type) {
    case 'START': return { status: 'loading' };
    case 'SUCCESS': return { status: 'success', data: action.payload };
    case 'ERROR': return { status: 'error', message: action.payload };
    case 'RESET': return { status: 'idle' };
    default: return state;
  }
}

export const AnnuitiesView: React.FC = () => {
  const [selected, setSelected] = useState<AnnuityType>('fixed');
  const [state, dispatch] = useReducer(analysisReducer, { status: 'idle' });
  const mountedRef = useRef(true);
  useEffect(() => { return () => { mountedRef.current = false; }; }, []);

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
    if (state.status === 'loading') return;
    dispatch({ type: 'START' });

    try {
      let result;
      const baseData = { asset_class: 'annuity' as const, currency: 'USD' };

      switch (selected) {
        case 'fixed':
          result = await withTimeout(AlternativeInvestmentApi.analyzeFixedAnnuity({ ...fixedData, ...baseData }), 30000);
          break;
        case 'variable':
          result = await withTimeout(AlternativeInvestmentApi.analyzeVariableAnnuity({ ...variableData, ...baseData }), 30000);
          break;
        case 'equity-indexed':
          result = await withTimeout(AlternativeInvestmentApi.analyzeEquityIndexedAnnuity({ ...equityIndexedData, ...baseData }), 30000);
          break;
        case 'inflation-annuity':
          result = await withTimeout(AlternativeInvestmentApi.analyzeInflationAnnuity({ ...inflationAnnuityData, ...baseData }), 30000);
          break;
      }
      if (!mountedRef.current) return;
      dispatch({ type: 'SUCCESS', payload: result });
    } catch (err: any) {
      if (!mountedRef.current) return;
      dispatch({ type: 'ERROR', payload: err.message || 'Analysis failed' });
    }
  };

  const renderVerdict = () => {
    if (state.status !== 'success' || !state.data?.success || !state.data?.metrics) return null;

    const category = state.data.metrics.analysis_category || 'UNKNOWN';
    const recommendation = state.data.metrics.analysis_recommendation || 'No recommendation';
    const keyProblems = state.data.metrics.key_problems || [];

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
              type="text"
              inputMode="decimal"
              value={fixedData.premium}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setFixedData({...fixedData, premium: Number(v)}); }}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Guaranteed Rate (%)</label>
            <input
              type="text"
              inputMode="decimal"
              value={fixedData.guaranteed_rate * 100}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setFixedData({...fixedData, guaranteed_rate: (Number(v) || 0) / 100}); }}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Term (Years)</label>
            <input
              type="text"
              inputMode="decimal"
              value={fixedData.term_years}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setFixedData({...fixedData, term_years: Number(v)}); }}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px', gridColumn: 'span 2' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Surrender Charge Period (Years)</label>
            <input
              type="text"
              inputMode="decimal"
              value={fixedData.surrender_charge_years}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setFixedData({...fixedData, surrender_charge_years: Number(v)}); }}
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
              type="text"
              inputMode="decimal"
              value={variableData.premium}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setVariableData({...variableData, premium: Number(v)}); }}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Annual Fee (%)</label>
            <input
              type="text"
              inputMode="decimal"
              value={variableData.annual_fee * 100}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setVariableData({...variableData, annual_fee: (Number(v) || 0) / 100}); }}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Expected Subaccount Return (%)</label>
            <input
              type="text"
              inputMode="decimal"
              value={variableData.subaccount_returns * 100}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setVariableData({...variableData, subaccount_returns: (Number(v) || 0) / 100}); }}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>M&E Expense (%)</label>
            <input
              type="text"
              inputMode="decimal"
              value={variableData.mortality_expense * 100}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setVariableData({...variableData, mortality_expense: (Number(v) || 0) / 100}); }}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Admin Fee (%)</label>
            <input
              type="text"
              inputMode="decimal"
              value={variableData.admin_fee * 100}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setVariableData({...variableData, admin_fee: (Number(v) || 0) / 100}); }}
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
              type="text"
              inputMode="decimal"
              value={equityIndexedData.premium}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setEquityIndexedData({...equityIndexedData, premium: Number(v)}); }}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Participation Rate (%)</label>
            <input
              type="text"
              inputMode="decimal"
              value={equityIndexedData.participation_rate * 100}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setEquityIndexedData({...equityIndexedData, participation_rate: (Number(v) || 0) / 100}); }}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Cap Rate (%)</label>
            <input
              type="text"
              inputMode="decimal"
              value={equityIndexedData.cap_rate * 100}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setEquityIndexedData({...equityIndexedData, cap_rate: (Number(v) || 0) / 100}); }}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Floor Rate (%)</label>
            <input
              type="text"
              inputMode="decimal"
              value={equityIndexedData.floor_rate * 100}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setEquityIndexedData({...equityIndexedData, floor_rate: (Number(v) || 0) / 100}); }}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Annual Fee (%)</label>
            <input
              type="text"
              inputMode="decimal"
              value={equityIndexedData.annual_fee * 100}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setEquityIndexedData({...equityIndexedData, annual_fee: (Number(v) || 0) / 100}); }}
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
            type="text"
            inputMode="decimal"
            value={inflationAnnuityData.premium}
            onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setInflationAnnuityData({...inflationAnnuityData, premium: Number(v)}); }}
            style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
          />
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
          <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Base Payout ($/year)</label>
          <input
            type="text"
            inputMode="decimal"
            value={inflationAnnuityData.base_payout}
            onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setInflationAnnuityData({...inflationAnnuityData, base_payout: Number(v)}); }}
            style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
          />
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
          <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Inflation Adjustment (%)</label>
          <input
            type="text"
            inputMode="decimal"
            value={inflationAnnuityData.inflation_adjustment * 100}
            onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setInflationAnnuityData({...inflationAnnuityData, inflation_adjustment: (Number(v) || 0) / 100}); }}
            style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
          />
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
          <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Guarantee Period (Years)</label>
          <input
            type="text"
            inputMode="decimal"
            value={inflationAnnuityData.guarantee_years}
            onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setInflationAnnuityData({...inflationAnnuityData, guarantee_years: Number(v)}); }}
            style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
          />
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
          <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Rider Cost (%)</label>
          <input
            type="text"
            inputMode="decimal"
            value={inflationAnnuityData.rider_cost * 100}
            onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setInflationAnnuityData({...inflationAnnuityData, rider_cost: (Number(v) || 0) / 100}); }}
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
            disabled={state.status === 'loading'}
            style={{
              marginTop: 'auto',
              padding: '10px',
              backgroundColor: state.status === 'loading' ? FINCEPT.MUTED : FINCEPT.CYAN,
              color: FINCEPT.DARK_BG,
              border: 'none',
              borderRadius: '4px',
              fontSize: '11px',
              fontWeight: 700,
              cursor: state.status === 'loading' ? 'not-allowed' : 'pointer',
              letterSpacing: '0.5px',
            }}
          >
            {state.status === 'loading' ? 'ANALYZING...' : 'ANALYZE ANNUITY'}
          </button>
        </form>
      </div>

      {/* RIGHT: Results Section */}
      <div style={{ width: '480px', display: 'flex', flexDirection: 'column', overflow: 'hidden', backgroundColor: FINCEPT.PANEL_BG }}>
        <div style={{ padding: '12px 16px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
          <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.MUTED, letterSpacing: '0.5px' }}>ANALYSIS RESULTS</div>
        </div>
        <div style={{ flex: 1, overflow: 'auto' }}>
          {state.status === 'loading' && (
            <div style={{ padding: '24px', display: 'flex', alignItems: 'center', justifyContent: 'center', flexDirection: 'column', gap: '12px' }}>
              <div style={{ width: '24px', height: '24px', border: `2px solid ${FINCEPT.BORDER}`, borderTopColor: FINCEPT.CYAN, borderRadius: '50%', animation: 'spin 1s linear infinite' }} />
              <div style={{ fontSize: '10px', color: FINCEPT.GRAY }}>Running annuity analytics...</div>
            </div>
          )}
          {state.status === 'error' && (
            <div style={{ margin: '16px', padding: '12px', backgroundColor: `${FINCEPT.RED}20`, border: `1px solid ${FINCEPT.RED}`, borderRadius: '4px' }}>
              <div style={{ fontSize: '10px', color: FINCEPT.RED }}>{state.status === 'error' && state.message}</div>
            </div>
          )}
          {state.status === 'idle' && (
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

const AnnuitiesViewWithBoundary: React.FC = () => (
  <ErrorBoundary name="AnnuitiesView">
    <AnnuitiesView />
  </ErrorBoundary>
);

export default AnnuitiesViewWithBoundary;
