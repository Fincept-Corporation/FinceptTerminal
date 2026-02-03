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

type StructuredType = 'structured-product' | 'leveraged-fund';

const TYPES = [
  { id: 'structured-product' as const, name: 'Structured Product' },
  { id: 'leveraged-fund' as const, name: 'Leveraged Fund' },
];

export const StructuredProductsView: React.FC = () => {
  const [selected, setSelected] = useState<StructuredType>('structured-product');
  const [isAnalyzing, setIsAnalyzing] = useState(false);
  const [analysis, setAnalysis] = useState<any>(null);
  const [error, setError] = useState<string | null>(null);

  const [structuredData, setStructuredData] = useState({
    name: 'Sample Structured Note',
    principal: 100000,
    term_years: 5,
    coupon_rate: 0.06,
    barrier_level: 0.70,
    participation_rate: 1.0,
    issuer_credit_rating: 'A',
  });

  const [leveragedData, setLeveragedData] = useState({
    name: 'Sample 3x Leveraged ETF',
    leverage_ratio: 3.0,
    expense_ratio: 0.0095,
    underlying_index: 'S&P 500',
    daily_reset: true,
  });

  const handleAnalyze = async (e: React.FormEvent) => {
    e.preventDefault();
    setIsAnalyzing(true);
    setError(null);
    setAnalysis(null);

    try {
      let result;
      const baseData = { currency: 'USD' };

      switch (selected) {
        case 'structured-product':
          result = await AlternativeInvestmentApi.analyzeStructuredProduct({
            ...structuredData,
            asset_class: 'structured_product',
            ...baseData,
          });
          break;
        case 'leveraged-fund':
          result = await AlternativeInvestmentApi.analyzeLeveragedFund({
            ...leveragedData,
            asset_class: 'leveraged_fund',
            ...baseData,
          });
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
    if (selected === 'structured-product') {
      return (
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Product Name</label>
            <input
              type="text"
              value={structuredData.name}
              onChange={(e) => setStructuredData({...structuredData, name: e.target.value})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Principal ($)</label>
            <input
              type="number"
              value={structuredData.principal}
              onChange={(e) => setStructuredData({...structuredData, principal: Number(e.target.value)})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Term (Years)</label>
            <input
              type="number"
              value={structuredData.term_years}
              onChange={(e) => setStructuredData({...structuredData, term_years: Number(e.target.value)})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Coupon Rate (%)</label>
            <input
              type="number"
              step="0.01"
              value={structuredData.coupon_rate * 100}
              onChange={(e) => setStructuredData({...structuredData, coupon_rate: Number(e.target.value) / 100})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Barrier Level (%)</label>
            <input
              type="number"
              step="0.01"
              value={structuredData.barrier_level * 100}
              onChange={(e) => setStructuredData({...structuredData, barrier_level: Number(e.target.value) / 100})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Participation Rate (%)</label>
            <input
              type="number"
              step="0.01"
              value={structuredData.participation_rate * 100}
              onChange={(e) => setStructuredData({...structuredData, participation_rate: Number(e.target.value) / 100})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px', gridColumn: 'span 2' }}>
            <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Issuer Credit Rating</label>
            <input
              type="text"
              value={structuredData.issuer_credit_rating}
              onChange={(e) => setStructuredData({...structuredData, issuer_credit_rating: e.target.value})}
              style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
            />
          </div>
        </div>
      );
    }

    // leveraged-fund
    return (
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
          <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Fund Name</label>
          <input
            type="text"
            value={leveragedData.name}
            onChange={(e) => setLeveragedData({...leveragedData, name: e.target.value})}
            style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
          />
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
          <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Leverage Ratio (e.g., 3.0 for 3x)</label>
          <input
            type="number"
            step="0.1"
            value={leveragedData.leverage_ratio}
            onChange={(e) => setLeveragedData({...leveragedData, leverage_ratio: Number(e.target.value)})}
            style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
          />
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
          <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Expense Ratio (%)</label>
          <input
            type="number"
            step="0.0001"
            value={leveragedData.expense_ratio * 100}
            onChange={(e) => setLeveragedData({...leveragedData, expense_ratio: Number(e.target.value) / 100})}
            style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
          />
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
          <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Underlying Index</label>
          <input
            type="text"
            value={leveragedData.underlying_index}
            onChange={(e) => setLeveragedData({...leveragedData, underlying_index: e.target.value})}
            style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '8px', fontSize: '11px', color: FINCEPT.WHITE }}
          />
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', gridColumn: 'span 2' }}>
          <input
            type="checkbox"
            checked={leveragedData.daily_reset}
            onChange={(e) => setLeveragedData({...leveragedData, daily_reset: e.target.checked})}
            style={{ width: '16px', height: '16px', cursor: 'pointer' }}
          />
          <label style={{ fontSize: '10px', color: FINCEPT.GRAY }}>Daily Reset (Most leveraged funds reset daily)</label>
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
          <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.MUTED, marginBottom: '8px', letterSpacing: '0.5px' }}>PRODUCT TYPE</div>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '6px' }}>
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
            <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.MUTED, letterSpacing: '0.5px' }}>PRODUCT DETAILS</div>
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
            {isAnalyzing ? 'ANALYZING...' : 'ANALYZE PRODUCT'}
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
              <div style={{ fontSize: '10px', color: FINCEPT.GRAY }}>Running structured product analytics...</div>
            </div>
          )}
          {error && (
            <div style={{ margin: '16px', padding: '12px', backgroundColor: `${FINCEPT.RED}20`, border: `1px solid ${FINCEPT.RED}`, borderRadius: '4px' }}>
              <div style={{ fontSize: '10px', color: FINCEPT.RED }}>{error}</div>
            </div>
          )}
          {!isAnalyzing && !error && !analysis && (
            <div style={{ padding: '24px', textAlign: 'center' }}>
              <div style={{ fontSize: '10px', color: FINCEPT.MUTED }}>Select product type and enter details to see results</div>
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

export default StructuredProductsView;
