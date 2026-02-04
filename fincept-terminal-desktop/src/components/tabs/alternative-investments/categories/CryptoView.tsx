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

export const CryptoView: React.FC = () => {
  const [state, dispatch] = useReducer(analysisReducer, { status: 'idle' });
  const mountedRef = useRef(true);
  useEffect(() => { return () => { mountedRef.current = false; }; }, []);

  const [formData, setFormData] = useState({
    name: 'Bitcoin',
    asset_type: 'cryptocurrency',
    purchase_price: 40000,
    current_price: 45000,
    amount_held: 0.5,
    volatility: 0.65,
    market_cap_billions: 850,
  });

  const handleAnalyze = async (e: React.FormEvent) => {
    e.preventDefault();
    if (state.status === 'loading') return;
    dispatch({ type: 'START' });

    try {
      const result = await withTimeout(AlternativeInvestmentApi.analyzeDigitalAsset({
        ...formData,
        asset_class: 'digital_assets',
        currency: 'USD',
      }), 30000);
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

  return (
    <div style={{ display: 'flex', height: '100%', fontFamily: '"IBM Plex Mono", monospace', backgroundColor: FINCEPT.DARK_BG }}>

      {/* LEFT: Form Section */}
      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden', borderRight: `1px solid ${FINCEPT.BORDER}` }}>

        {/* Input Form */}
        <form onSubmit={handleAnalyze} style={{ flex: 1, overflow: 'auto', padding: '16px', display: 'flex', flexDirection: 'column', gap: '16px' }}>

          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.MUTED, letterSpacing: '0.5px' }}>DIGITAL ASSET DETAILS</div>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
              <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Asset Name</label>
                <input
                  type="text"
                  value={formData.name}
                  onChange={(e) => setFormData({...formData, name: e.target.value})}
                  style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
                />
              </div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Asset Type</label>
                <select
                  value={formData.asset_type}
                  onChange={(e) => setFormData({...formData, asset_type: e.target.value})}
                  style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
                >
                  <option value="cryptocurrency">Cryptocurrency</option>
                  <option value="token">Token</option>
                  <option value="nft">NFT</option>
                  <option value="defi">DeFi Asset</option>
                </select>
              </div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Purchase Price ($)</label>
                <input
                  type="text"
                  inputMode="decimal"
                  value={formData.purchase_price}
                  onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) { setFormData({...formData, purchase_price: Number(v) || 0}); } }}
                  style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
                />
              </div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Current Price ($)</label>
                <input
                  type="text"
                  inputMode="decimal"
                  value={formData.current_price}
                  onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) { setFormData({...formData, current_price: Number(v) || 0}); } }}
                  style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
                />
              </div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Amount Held</label>
                <input
                  type="text"
                  inputMode="decimal"
                  value={formData.amount_held}
                  onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) { setFormData({...formData, amount_held: Number(v) || 0}); } }}
                  style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
                />
              </div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Volatility (%)</label>
                <input
                  type="text"
                  inputMode="decimal"
                  value={formData.volatility * 100}
                  onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) { setFormData({...formData, volatility: (Number(v) || 0) / 100}); } }}
                  style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
                />
              </div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: '4px', gridColumn: 'span 2' }}>
                <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Market Cap (Billions $)</label>
                <input
                  type="text"
                  inputMode="decimal"
                  value={formData.market_cap_billions}
                  onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) { setFormData({...formData, market_cap_billions: Number(v) || 0}); } }}
                  style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
                />
              </div>
            </div>
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
            {state.status === 'loading' ? 'ANALYZING...' : 'ANALYZE DIGITAL ASSET'}
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
              <div style={{ fontSize: '10px', color: FINCEPT.GRAY }}>Running digital asset analytics...</div>
            </div>
          )}
          {state.status === 'error' && (
            <div style={{ margin: '16px', padding: '12px', backgroundColor: `${FINCEPT.RED}20`, border: `1px solid ${FINCEPT.RED}`, borderRadius: '4px' }}>
              <div style={{ fontSize: '10px', color: FINCEPT.RED }}>{state.status === 'error' && state.message}</div>
            </div>
          )}
          {state.status === 'idle' && (
            <div style={{ padding: '24px', textAlign: 'center' }}>
              <div style={{ fontSize: '10px', color: FINCEPT.MUTED }}>Enter digital asset details to see results</div>
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

const CryptoViewWithBoundary: React.FC = () => (
  <ErrorBoundary name="CryptoView">
    <CryptoView />
  </ErrorBoundary>
);

export default CryptoViewWithBoundary;
