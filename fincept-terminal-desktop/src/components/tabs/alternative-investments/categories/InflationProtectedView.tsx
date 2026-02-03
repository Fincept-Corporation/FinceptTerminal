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

type InflationType = 'tips' | 'i-bonds' | 'stable-value';

const TYPES = [
  { id: 'tips' as const, name: 'TIPS' },
  { id: 'i-bonds' as const, name: 'I-Bonds' },
  { id: 'stable-value' as const, name: 'Stable Value' },
];

export const InflationProtectedView: React.FC = () => {
  const [selected, setSelected] = useState<InflationType>('tips');
  const [isAnalyzing, setIsAnalyzing] = useState(false);
  const [analysis, setAnalysis] = useState<any>(null);
  const [error, setError] = useState<string | null>(null);

  const [tipsData, setTipsData] = useState({
    name: 'TIPS 10-Year',
    bond_type: 'TIPS',
    face_value: 10000,
    real_yield: 0.015,
    maturity_years: 10,
    current_cpi: 0.03,
  });

  const [ibondsData, setIbondsData] = useState({
    name: 'Series I Savings Bond',
    bond_type: 'I_BONDS',
    face_value: 10000,
    fixed_rate: 0.005,
    current_inflation_rate: 0.032,
    holding_period_months: 60,
  });

  const [stableValueData, setStableValueData] = useState({
    name: 'Stable Value Fund',
    bond_type: 'STABLE_VALUE',
    face_value: 50000,
    credited_rate: 0.025,
    book_value: 50000,
    market_value: 49500,
  });

  const handleAnalyze = async (e: React.FormEvent) => {
    e.preventDefault();
    setIsAnalyzing(true);
    setError(null);
    setAnalysis(null);

    try {
      let data;
      const baseData = { asset_class: 'fixed_income', currency: 'USD' };

      switch (selected) {
        case 'tips':
          data = { ...tipsData, ...baseData };
          break;
        case 'i-bonds':
          data = { ...ibondsData, ...baseData };
          break;
        case 'stable-value':
          data = { ...stableValueData, ...baseData };
          break;
      }

      const result = await AlternativeInvestmentApi.analyzeInflationProtected(data);
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
    if (selected === 'tips') {
      return (
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Bond Name</label>
            <input
              type="text"
              value={tipsData.name}
              onChange={(e) => setTipsData({...tipsData, name: e.target.value})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Face Value ($)</label>
            <input
              type="number"
              value={tipsData.face_value}
              onChange={(e) => setTipsData({...tipsData, face_value: Number(e.target.value)})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Real Yield (%)</label>
            <input
              type="number"
              step="0.001"
              value={tipsData.real_yield * 100}
              onChange={(e) => setTipsData({...tipsData, real_yield: Number(e.target.value) / 100})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Maturity (Years)</label>
            <input
              type="number"
              value={tipsData.maturity_years}
              onChange={(e) => setTipsData({...tipsData, maturity_years: Number(e.target.value)})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px', gridColumn: 'span 2' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Current CPI Rate (%)</label>
            <input
              type="number"
              step="0.001"
              value={tipsData.current_cpi * 100}
              onChange={(e) => setTipsData({...tipsData, current_cpi: Number(e.target.value) / 100})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
        </div>
      );
    }

    if (selected === 'i-bonds') {
      return (
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Bond Name</label>
            <input
              type="text"
              value={ibondsData.name}
              onChange={(e) => setIbondsData({...ibondsData, name: e.target.value})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Face Value ($)</label>
            <input
              type="number"
              value={ibondsData.face_value}
              onChange={(e) => setIbondsData({...ibondsData, face_value: Number(e.target.value)})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Fixed Rate (%)</label>
            <input
              type="number"
              step="0.001"
              value={ibondsData.fixed_rate * 100}
              onChange={(e) => setIbondsData({...ibondsData, fixed_rate: Number(e.target.value) / 100})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Current Inflation Rate (%)</label>
            <input
              type="number"
              step="0.001"
              value={ibondsData.current_inflation_rate * 100}
              onChange={(e) => setIbondsData({...ibondsData, current_inflation_rate: Number(e.target.value) / 100})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px', gridColumn: 'span 2' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Holding Period (Months)</label>
            <input
              type="number"
              value={ibondsData.holding_period_months}
              onChange={(e) => setIbondsData({...ibondsData, holding_period_months: Number(e.target.value)})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
        </div>
      );
    }

    // stable-value
    return (
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
          <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Fund Name</label>
          <input
            type="text"
            value={stableValueData.name}
            onChange={(e) => setStableValueData({...stableValueData, name: e.target.value})}
            style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
          />
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
          <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Investment Amount ($)</label>
          <input
            type="number"
            value={stableValueData.face_value}
            onChange={(e) => setStableValueData({...stableValueData, face_value: Number(e.target.value)})}
            style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
          />
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
          <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Credited Rate (%)</label>
          <input
            type="number"
            step="0.001"
            value={stableValueData.credited_rate * 100}
            onChange={(e) => setStableValueData({...stableValueData, credited_rate: Number(e.target.value) / 100})}
            style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
          />
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
          <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Book Value ($)</label>
          <input
            type="number"
            value={stableValueData.book_value}
            onChange={(e) => setStableValueData({...stableValueData, book_value: Number(e.target.value)})}
            style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
          />
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '4px', gridColumn: 'span 2' }}>
          <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Market Value ($)</label>
          <input
            type="number"
            value={stableValueData.market_value}
            onChange={(e) => setStableValueData({...stableValueData, market_value: Number(e.target.value)})}
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
          <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.MUTED, marginBottom: '8px', letterSpacing: '0.5px' }}>SECURITY TYPE</div>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '6px' }}>
            {TYPES.map((type) => {
              const isActive = selected === type.id;
              return (
                <button
                  key={type.id}
                  type="button"
                  onClick={() => setSelected(type.id)}
                  style={{
                    padding: '8px 12px',
                    backgroundColor: isActive ? `${FINCEPT.GREEN}20` : FINCEPT.PANEL_BG,
                    border: `1px solid ${isActive ? FINCEPT.GREEN : FINCEPT.BORDER}`,
                    borderRadius: '4px',
                    fontSize: '10px',
                    fontWeight: 600,
                    color: isActive ? FINCEPT.GREEN : FINCEPT.GRAY,
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
            <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.MUTED, letterSpacing: '0.5px' }}>SECURITY DETAILS</div>
            {renderForm()}
          </div>

          <button
            type="submit"
            disabled={isAnalyzing}
            style={{
              marginTop: 'auto',
              padding: '10px',
              backgroundColor: isAnalyzing ? FINCEPT.MUTED : FINCEPT.GREEN,
              color: FINCEPT.DARK_BG,
              border: 'none',
              borderRadius: '4px',
              fontSize: '11px',
              fontWeight: 700,
              cursor: isAnalyzing ? 'not-allowed' : 'pointer',
              letterSpacing: '0.5px',
            }}
          >
            {isAnalyzing ? 'ANALYZING...' : 'ANALYZE SECURITY'}
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
              <div style={{ width: '24px', height: '24px', border: `2px solid ${FINCEPT.BORDER}`, borderTopColor: FINCEPT.GREEN, borderRadius: '50%', animation: 'spin 1s linear infinite' }} />
              <div style={{ fontSize: '10px', color: FINCEPT.GRAY }}>Running inflation-protected analytics...</div>
            </div>
          )}
          {error && (
            <div style={{ margin: '16px', padding: '12px', backgroundColor: `${FINCEPT.RED}20`, border: `1px solid ${FINCEPT.RED}`, borderRadius: '4px' }}>
              <div style={{ fontSize: '10px', color: FINCEPT.RED }}>{error}</div>
            </div>
          )}
          {!isAnalyzing && !error && !analysis && (
            <div style={{ padding: '24px', textAlign: 'center' }}>
              <div style={{ fontSize: '10px', color: FINCEPT.MUTED }}>Select security type and enter details to see results</div>
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

export default InflationProtectedView;
