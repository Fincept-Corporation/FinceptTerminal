import React, { useState } from 'react';
import { AlternativeInvestmentApi } from '@/services/alternativeInvestments/api/analyticsApi';
import { HedgeFundParams, ManagedFuturesParams, HedgeFundStrategy } from '@/services/alternativeInvestments/api/types';

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

type HedgeFundType = 'hedge-fund' | 'managed-futures' | 'market-neutral';

const TYPES = [
  { id: 'hedge-fund', name: 'Hedge Funds' },
  { id: 'managed-futures', name: 'Managed Futures' },
  { id: 'market-neutral', name: 'Market Neutral' },
];

export const HedgeFundsView: React.FC = () => {
  const [selected, setSelected] = useState<HedgeFundType>('hedge-fund');
  const [isAnalyzing, setIsAnalyzing] = useState(false);
  const [analysis, setAnalysis] = useState<any>(null);
  const [error, setError] = useState<string | null>(null);

  const handleAnalyze = async (data: any) => {
    setIsAnalyzing(true);
    setError(null);
    setAnalysis(null);

    try {
      let result;
      switch (selected) {
        case 'hedge-fund':
          result = await AlternativeInvestmentApi.analyzeHedgeFund(data);
          break;
        case 'managed-futures':
          result = await AlternativeInvestmentApi.analyzeManagedFutures(data);
          break;
        case 'market-neutral':
          result = await AlternativeInvestmentApi.analyzeMarketNeutral(data);
          break;
      }
      setAnalysis(result);
    } catch (err: any) {
      setError(err.message || 'Analysis failed');
    } finally {
      setIsAnalyzing(false);
    }
  };

  return (
    <div style={{ display: 'flex', height: '100%', fontFamily: '"IBM Plex Mono", monospace', backgroundColor: FINCEPT.DARK_BG }}>

      {/* LEFT: Form Section */}
      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden', borderRight: `1px solid ${FINCEPT.BORDER}` }}>

        {/* Type Selector */}
        <div style={{ padding: '12px 16px', backgroundColor: FINCEPT.PANEL_BG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
          <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.MUTED, marginBottom: '8px', letterSpacing: '0.5px' }}>STRATEGY TYPE</div>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '6px' }}>
            {TYPES.map((type) => (
              <button
                key={type.id}
                onClick={() => {
                  setSelected(type.id as HedgeFundType);
                  setAnalysis(null);
                  setError(null);
                }}
                style={{
                  padding: '8px 10px',
                  backgroundColor: selected === type.id ? FINCEPT.HOVER : FINCEPT.DARK_BG,
                  border: `1px solid ${selected === type.id ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                  borderTop: `2px solid ${selected === type.id ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  cursor: 'pointer',
                  transition: 'all 0.2s',
                  textAlign: 'center',
                }}
                onMouseEnter={(e) => {
                  if (selected !== type.id) e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                }}
                onMouseLeave={(e) => {
                  if (selected !== type.id) e.currentTarget.style.backgroundColor = FINCEPT.DARK_BG;
                }}
              >
                <div style={{ fontSize: '10px', fontWeight: 600, color: selected === type.id ? FINCEPT.ORANGE : FINCEPT.WHITE }}>
                  {type.name}
                </div>
              </button>
            ))}
          </div>
        </div>

        {/* Input Form */}
        <div style={{ flex: 1, overflow: 'auto', padding: '16px' }}>
          <div style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px' }}>
            <div style={{ padding: '14px 16px', backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
              <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE, letterSpacing: '0.5px' }}>
                STRATEGY DETAILS
              </span>
            </div>
            <div style={{ padding: '20px' }}>
              {selected === 'hedge-fund' && <HedgeFundForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />}
              {selected === 'managed-futures' && <ManagedFuturesForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />}
              {selected === 'market-neutral' && <MarketNeutralForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />}
            </div>
          </div>
        </div>
      </div>

      {/* RIGHT: Results Section */}
      <div style={{ width: '480px', display: 'flex', flexDirection: 'column', overflow: 'hidden', backgroundColor: FINCEPT.PANEL_BG }}>
        <div style={{ padding: '14px 16px', backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
          <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE, letterSpacing: '0.5px' }}>
            ANALYSIS RESULTS
          </span>
        </div>

        <div style={{ flex: 1, overflow: 'auto', padding: '16px' }}>
          {error && (
            <div style={{ backgroundColor: '#FF444410', border: '1px solid #FF4444', borderRadius: '2px', padding: '16px' }}>
              <div style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.RED, marginBottom: '6px', letterSpacing: '0.5px' }}>ERROR</div>
              <div style={{ fontSize: '10px', color: FINCEPT.RED, lineHeight: '1.5' }}>{error}</div>
            </div>
          )}

          {analysis && analysis.success && analysis.metrics && (
            <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
              <div style={{
                padding: '12px 20px',
                backgroundColor: analysis.metrics.analysis_category === 'GOOD' ? FINCEPT.GREEN + '20' :
                                 analysis.metrics.analysis_category === 'FLAWED' ? FINCEPT.ORANGE + '20' :
                                 FINCEPT.RED + '20',
                border: `2px solid ${analysis.metrics.analysis_category === 'GOOD' ? FINCEPT.GREEN :
                                     analysis.metrics.analysis_category === 'FLAWED' ? FINCEPT.ORANGE :
                                     FINCEPT.RED}`,
                borderRadius: '2px',
                textAlign: 'center',
              }}>
                <div style={{ fontSize: '14px', fontWeight: 700, letterSpacing: '1.5px', color:
                  analysis.metrics.analysis_category === 'GOOD' ? FINCEPT.GREEN :
                  analysis.metrics.analysis_category === 'FLAWED' ? FINCEPT.ORANGE :
                  FINCEPT.RED
                }}>
                  {analysis.metrics.analysis_category}
                </div>
              </div>

              <div style={{ padding: '16px', backgroundColor: FINCEPT.DARK_BG, borderRadius: '2px', borderLeft: `3px solid ${FINCEPT.ORANGE}` }}>
                <div style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.MUTED, marginBottom: '8px', letterSpacing: '0.5px' }}>
                  RECOMMENDATION
                </div>
                <div style={{ fontSize: '12px', color: FINCEPT.WHITE, lineHeight: '1.6' }}>
                  {analysis.metrics.analysis_recommendation}
                </div>
              </div>

              {analysis.metrics.key_problems && analysis.metrics.key_problems.length > 0 && (
                <div style={{ padding: '16px', backgroundColor: FINCEPT.DARK_BG, borderRadius: '2px' }}>
                  <div style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.MUTED, marginBottom: '12px', letterSpacing: '0.5px' }}>
                    KEY CONCERNS
                  </div>
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
                    {analysis.metrics.key_problems.map((problem: string, i: number) => (
                      <div key={i} style={{ display: 'flex', gap: '10px', alignItems: 'flex-start' }}>
                        <div style={{
                          fontSize: '11px',
                          color: FINCEPT.ORANGE,
                          fontWeight: 700,
                          backgroundColor: FINCEPT.ORANGE + '20',
                          padding: '4px 8px',
                          borderRadius: '2px',
                          minWidth: '28px',
                          textAlign: 'center',
                        }}>
                          {i + 1}
                        </div>
                        <div style={{ fontSize: '11px', color: FINCEPT.WHITE, flex: 1, lineHeight: '1.5' }}>{problem}</div>
                      </div>
                    ))}
                  </div>
                </div>
              )}
            </div>
          )}

          {!analysis && !isAnalyzing && !error && (
            <div style={{
              padding: '24px',
              backgroundColor: FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '2px',
              textAlign: 'center',
            }}>
              <div style={{ fontSize: '10px', color: FINCEPT.GRAY, lineHeight: '1.6' }}>
                Fill in the strategy details and click "ANALYZE"
              </div>
            </div>
          )}

          {isAnalyzing && (
            <div style={{
              padding: '24px',
              backgroundColor: FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.ORANGE}`,
              borderRadius: '2px',
              textAlign: 'center',
            }}>
              <div style={{ fontSize: '11px', color: FINCEPT.ORANGE, fontWeight: 600, marginBottom: '8px' }}>ANALYZING...</div>
              <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Running hedge fund analysis</div>
            </div>
          )}
        </div>
      </div>

    </div>
  );
};

const HedgeFundForm: React.FC<{onSubmit: any; isLoading: boolean}> = ({ onSubmit, isLoading }) => {
  const [formData, setFormData] = useState({
    name: 'Sample Hedge Fund',
    strategy: 'equity_long_short' as HedgeFundStrategy,
    aum: 100000000,
    annual_return: 0.12,
    volatility: 0.15,
    management_fee: 0.02,
    performance_fee: 0.20,
  });

  const inputStyle = {
    backgroundColor: FINCEPT.DARK_BG,
    border: `1px solid ${FINCEPT.BORDER}`,
    borderRadius: '2px',
    padding: '12px',
    color: FINCEPT.WHITE,
    fontSize: '11px',
    width: '100%',
    outline: 'none',
  };

  const labelStyle = {
    display: 'block',
    fontSize: '10px',
    fontWeight: 700,
    color: FINCEPT.GRAY,
    marginBottom: '8px',
    letterSpacing: '0.5px',
  };

  return (
    <form onSubmit={(e) => { e.preventDefault(); onSubmit({...formData, asset_class: 'hedge_fund', currency: 'USD'}); }} style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '12px' }}>
        <div style={{ gridColumn: '1 / -1' }}>
          <label style={labelStyle}>FUND NAME</label>
          <input type="text" value={formData.name} onChange={(e) => setFormData({...formData, name: e.target.value})} style={inputStyle} />
        </div>
        <div>
          <label style={labelStyle}>STRATEGY</label>
          <select value={formData.strategy} onChange={(e) => setFormData({...formData, strategy: e.target.value as HedgeFundStrategy})} style={inputStyle}>
            <option value="equity_long_short">Equity Long/Short</option>
            <option value="event_driven">Event Driven</option>
            <option value="global_macro">Global Macro</option>
            <option value="relative_value">Relative Value</option>
          </select>
        </div>
        <div>
          <label style={labelStyle}>AUM ($)</label>
          <input type="number" value={formData.aum} onChange={(e) => setFormData({...formData, aum: Number(e.target.value)})} style={inputStyle} />
        </div>
        <div>
          <label style={labelStyle}>ANNUAL RETURN (%)</label>
          <input type="number" step="0.01" value={formData.annual_return * 100} onChange={(e) => setFormData({...formData, annual_return: Number(e.target.value) / 100})} style={inputStyle} />
        </div>
        <div>
          <label style={labelStyle}>VOLATILITY (%)</label>
          <input type="number" step="0.01" value={formData.volatility * 100} onChange={(e) => setFormData({...formData, volatility: Number(e.target.value) / 100})} style={inputStyle} />
        </div>
        <div>
          <label style={labelStyle}>MANAGEMENT FEE (%)</label>
          <input type="number" step="0.01" value={formData.management_fee * 100} onChange={(e) => setFormData({...formData, management_fee: Number(e.target.value) / 100})} style={inputStyle} />
        </div>
        <div>
          <label style={labelStyle}>PERFORMANCE FEE (%)</label>
          <input type="number" step="0.01" value={formData.performance_fee * 100} onChange={(e) => setFormData({...formData, performance_fee: Number(e.target.value) / 100})} style={inputStyle} />
        </div>
      </div>
      <button type="submit" disabled={isLoading} style={{
        width: '100%',
        backgroundColor: isLoading ? FINCEPT.MUTED : FINCEPT.ORANGE,
        color: FINCEPT.WHITE,
        fontWeight: 700,
        padding: '16px',
        borderRadius: '2px',
        border: 'none',
        cursor: isLoading ? 'not-allowed' : 'pointer',
        fontSize: '12px',
        letterSpacing: '0.5px',
      }}>
        {isLoading ? 'ANALYZING...' : 'ANALYZE HEDGE FUND'}
      </button>
    </form>
  );
};

const ManagedFuturesForm: React.FC<{onSubmit: any; isLoading: boolean}> = ({ onSubmit, isLoading }) => {
  const [formData, setFormData] = useState({
    name: 'Sample CTA Fund',
    annual_return: 0.10,
    volatility: 0.18,
    management_fee: 0.02,
    performance_fee: 0.20,
  });

  const inputStyle = {
    backgroundColor: FINCEPT.DARK_BG,
    border: `1px solid ${FINCEPT.BORDER}`,
    borderRadius: '2px',
    padding: '12px',
    color: FINCEPT.WHITE,
    fontSize: '11px',
    width: '100%',
    outline: 'none',
  };

  const labelStyle = {
    display: 'block',
    fontSize: '10px',
    fontWeight: 700,
    color: FINCEPT.GRAY,
    marginBottom: '8px',
    letterSpacing: '0.5px',
  };

  return (
    <form onSubmit={(e) => { e.preventDefault(); onSubmit({...formData, asset_class: 'managed_futures', currency: 'USD'}); }} style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '12px' }}>
        <div style={{ gridColumn: '1 / -1' }}>
          <label style={labelStyle}>FUND NAME</label>
          <input type="text" value={formData.name} onChange={(e) => setFormData({...formData, name: e.target.value})} style={inputStyle} />
        </div>
        <div>
          <label style={labelStyle}>ANNUAL RETURN (%)</label>
          <input type="number" step="0.01" value={formData.annual_return * 100} onChange={(e) => setFormData({...formData, annual_return: Number(e.target.value) / 100})} style={inputStyle} />
        </div>
        <div>
          <label style={labelStyle}>VOLATILITY (%)</label>
          <input type="number" step="0.01" value={formData.volatility * 100} onChange={(e) => setFormData({...formData, volatility: Number(e.target.value) / 100})} style={inputStyle} />
        </div>
        <div>
          <label style={labelStyle}>MANAGEMENT FEE (%)</label>
          <input type="number" step="0.01" value={formData.management_fee * 100} onChange={(e) => setFormData({...formData, management_fee: Number(e.target.value) / 100})} style={inputStyle} />
        </div>
        <div>
          <label style={labelStyle}>PERFORMANCE FEE (%)</label>
          <input type="number" step="0.01" value={formData.performance_fee * 100} onChange={(e) => setFormData({...formData, performance_fee: Number(e.target.value) / 100})} style={inputStyle} />
        </div>
      </div>
      <button type="submit" disabled={isLoading} style={{
        width: '100%',
        backgroundColor: isLoading ? FINCEPT.MUTED : FINCEPT.ORANGE,
        color: FINCEPT.WHITE,
        fontWeight: 700,
        padding: '16px',
        borderRadius: '2px',
        border: 'none',
        cursor: isLoading ? 'not-allowed' : 'pointer',
        fontSize: '12px',
        letterSpacing: '0.5px',
      }}>
        {isLoading ? 'ANALYZING...' : 'ANALYZE CTA FUND'}
      </button>
    </form>
  );
};

const MarketNeutralForm: React.FC<{onSubmit: any; isLoading: boolean}> = ({ onSubmit, isLoading }) => {
  const [formData, setFormData] = useState({
    name: 'Sample Market Neutral Fund',
    annual_return: 0.08,
    volatility: 0.06,
    market_beta: 0.1,
    management_fee: 0.015,
  });

  const inputStyle = {
    backgroundColor: FINCEPT.DARK_BG,
    border: `1px solid ${FINCEPT.BORDER}`,
    borderRadius: '2px',
    padding: '12px',
    color: FINCEPT.WHITE,
    fontSize: '11px',
    width: '100%',
    outline: 'none',
  };

  const labelStyle = {
    display: 'block',
    fontSize: '10px',
    fontWeight: 700,
    color: FINCEPT.GRAY,
    marginBottom: '8px',
    letterSpacing: '0.5px',
  };

  return (
    <form onSubmit={(e) => { e.preventDefault(); onSubmit({...formData, asset_class: 'market_neutral', currency: 'USD'}); }} style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '12px' }}>
        <div style={{ gridColumn: '1 / -1' }}>
          <label style={labelStyle}>FUND NAME</label>
          <input type="text" value={formData.name} onChange={(e) => setFormData({...formData, name: e.target.value})} style={inputStyle} />
        </div>
        <div>
          <label style={labelStyle}>ANNUAL RETURN (%)</label>
          <input type="number" step="0.01" value={formData.annual_return * 100} onChange={(e) => setFormData({...formData, annual_return: Number(e.target.value) / 100})} style={inputStyle} />
        </div>
        <div>
          <label style={labelStyle}>VOLATILITY (%)</label>
          <input type="number" step="0.01" value={formData.volatility * 100} onChange={(e) => setFormData({...formData, volatility: Number(e.target.value) / 100})} style={inputStyle} />
        </div>
        <div>
          <label style={labelStyle}>MARKET BETA</label>
          <input type="number" step="0.01" value={formData.market_beta} onChange={(e) => setFormData({...formData, market_beta: Number(e.target.value)})} style={inputStyle} />
        </div>
        <div>
          <label style={labelStyle}>MANAGEMENT FEE (%)</label>
          <input type="number" step="0.01" value={formData.management_fee * 100} onChange={(e) => setFormData({...formData, management_fee: Number(e.target.value) / 100})} style={inputStyle} />
        </div>
      </div>
      <button type="submit" disabled={isLoading} style={{
        width: '100%',
        backgroundColor: isLoading ? FINCEPT.MUTED : FINCEPT.ORANGE,
        color: FINCEPT.WHITE,
        fontWeight: 700,
        padding: '16px',
        borderRadius: '2px',
        border: 'none',
        cursor: isLoading ? 'not-allowed' : 'pointer',
        fontSize: '12px',
        letterSpacing: '0.5px',
      }}>
        {isLoading ? 'ANALYZING...' : 'ANALYZE MARKET NEUTRAL'}
      </button>
    </form>
  );
};

export default HedgeFundsView;
