import React, { useState } from 'react';
import { AlternativeInvestmentApi } from '@/services/alternativeInvestments/api/analyticsApi';
import { HighYieldBondParams, EmergingMarketBondParams, ConvertibleBondParams, PreferredStockParams, CreditRating } from '@/services/alternativeInvestments/api/types';

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

type BondType = 'high-yield' | 'emerging-market' | 'convertible' | 'preferred-stock';

const BOND_TYPES = [
  { id: 'high-yield', name: 'High-Yield Bonds', desc: 'Below investment-grade corporate bonds' },
  { id: 'emerging-market', name: 'Emerging Market Bonds', desc: 'Government bonds from developing countries' },
  { id: 'convertible', name: 'Convertible Bonds', desc: 'Bonds convertible to company stock' },
  { id: 'preferred-stock', name: 'Preferred Stocks', desc: 'Hybrid equity with fixed dividends' },
];

export const BondsView: React.FC = () => {
  const [selectedBond, setSelectedBond] = useState<BondType>('high-yield');
  const [isAnalyzing, setIsAnalyzing] = useState(false);
  const [analysis, setAnalysis] = useState<any>(null);
  const [error, setError] = useState<string | null>(null);

  const handleAnalyze = async (data: any) => {
    setIsAnalyzing(true);
    setError(null);
    setAnalysis(null);

    try {
      let result;
      switch (selectedBond) {
        case 'high-yield':
          result = await AlternativeInvestmentApi.analyzeHighYieldBond(data);
          break;
        case 'emerging-market':
          result = await AlternativeInvestmentApi.analyzeEmergingMarketBond(data);
          break;
        case 'convertible':
          result = await AlternativeInvestmentApi.analyzeConvertibleBond(data);
          break;
        case 'preferred-stock':
          result = await AlternativeInvestmentApi.analyzePreferredStock(data);
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

        {/* Bond Type Selector - Compact Horizontal */}
        <div style={{ padding: '12px 16px', backgroundColor: FINCEPT.PANEL_BG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
          <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.MUTED, marginBottom: '8px', letterSpacing: '0.5px' }}>BOND TYPE</div>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '6px' }}>
            {BOND_TYPES.map((type) => (
              <button
                key={type.id}
                onClick={() => {
                  setSelectedBond(type.id as BondType);
                  setAnalysis(null);
                  setError(null);
                }}
                style={{
                  padding: '8px 10px',
                  backgroundColor: selectedBond === type.id ? FINCEPT.HOVER : FINCEPT.DARK_BG,
                  border: `1px solid ${selectedBond === type.id ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                  borderTop: `2px solid ${selectedBond === type.id ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  cursor: 'pointer',
                  transition: 'all 0.2s',
                  textAlign: 'center',
                }}
                onMouseEnter={(e) => {
                  if (selectedBond !== type.id) e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                }}
                onMouseLeave={(e) => {
                  if (selectedBond !== type.id) e.currentTarget.style.backgroundColor = FINCEPT.DARK_BG;
                }}
              >
                <div style={{ fontSize: '10px', fontWeight: 600, color: selectedBond === type.id ? FINCEPT.ORANGE : FINCEPT.WHITE }}>
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
                BOND DETAILS
              </span>
            </div>
            <div style={{ padding: '20px' }}>
              {selectedBond === 'high-yield' && <HighYieldBondForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />}
              {selectedBond === 'emerging-market' && <EmergingMarketBondForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />}
              {selectedBond === 'convertible' && <ConvertibleBondForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />}
              {selectedBond === 'preferred-stock' && <PreferredStockForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />}
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
          {/* Error */}
          {error && (
            <div style={{ backgroundColor: '#FF444410', border: '1px solid #FF4444', borderRadius: '2px', padding: '16px' }}>
              <div style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.RED, marginBottom: '6px', letterSpacing: '0.5px' }}>ERROR</div>
              <div style={{ fontSize: '10px', color: FINCEPT.RED, lineHeight: '1.5' }}>{error}</div>
            </div>
          )}

          {/* Analysis Result */}
          {analysis && analysis.success && analysis.metrics && (
            <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>

              {/* Verdict Badge */}
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

              {/* Recommendation */}
              <div style={{ padding: '16px', backgroundColor: FINCEPT.DARK_BG, borderRadius: '2px', borderLeft: `3px solid ${FINCEPT.ORANGE}` }}>
                <div style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.MUTED, marginBottom: '8px', letterSpacing: '0.5px' }}>
                  RECOMMENDATION
                </div>
                <div style={{ fontSize: '12px', color: FINCEPT.WHITE, lineHeight: '1.6' }}>
                  {analysis.metrics.analysis_recommendation}
                </div>
              </div>

              {/* Key Problems */}
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

              {/* Bond Metrics */}
              <div style={{ padding: '16px', backgroundColor: FINCEPT.DARK_BG, borderRadius: '2px' }}>
                <div style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.MUTED, marginBottom: '12px', letterSpacing: '0.5px' }}>
                  BOND METRICS
                </div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
                  {analysis.metrics.credit_rating && (
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '8px 0', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                      <div style={{ fontSize: '10px', color: FINCEPT.GRAY }}>Credit Rating</div>
                      <div style={{ fontSize: '12px', color: FINCEPT.WHITE, fontWeight: 600 }}>{analysis.metrics.credit_rating}</div>
                    </div>
                  )}
                  {analysis.metrics.yield_to_maturity && (
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '8px 0', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                      <div style={{ fontSize: '10px', color: FINCEPT.GRAY }}>Yield to Maturity</div>
                      <div style={{ fontSize: '12px', color: FINCEPT.WHITE, fontWeight: 600 }}>{(analysis.metrics.yield_to_maturity * 100).toFixed(2)}%</div>
                    </div>
                  )}
                  {analysis.metrics.maturity_years && (
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '8px 0', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                      <div style={{ fontSize: '10px', color: FINCEPT.GRAY }}>Maturity</div>
                      <div style={{ fontSize: '12px', color: FINCEPT.WHITE, fontWeight: 600 }}>{analysis.metrics.maturity_years} years</div>
                    </div>
                  )}
                  {analysis.metrics.default_analysis && (
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '8px 0' }}>
                      <div style={{ fontSize: '10px', color: FINCEPT.GRAY }}>Default Risk (5yr)</div>
                      <div style={{ fontSize: '12px', color: FINCEPT.RED, fontWeight: 600 }}>
                        {(analysis.metrics.default_analysis.default_probability_5yr * 100).toFixed(2)}%
                      </div>
                    </div>
                  )}
                </div>
              </div>

            </div>
          )}

          {/* Help Text */}
          {!analysis && !isAnalyzing && !error && (
            <div style={{
              padding: '24px',
              backgroundColor: FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '2px',
              textAlign: 'center',
            }}>
              <div style={{ fontSize: '10px', color: FINCEPT.GRAY, lineHeight: '1.6' }}>
                Fill in the bond details and click "ANALYZE" to get professional CFA-level analysis
              </div>
            </div>
          )}

          {/* Loading State */}
          {isAnalyzing && (
            <div style={{
              padding: '24px',
              backgroundColor: FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.ORANGE}`,
              borderRadius: '2px',
              textAlign: 'center',
            }}>
              <div style={{ fontSize: '11px', color: FINCEPT.ORANGE, fontWeight: 600, marginBottom: '8px' }}>ANALYZING...</div>
              <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Running CFA-level bond analysis</div>
            </div>
          )}
        </div>
      </div>

    </div>
  );
};

const HighYieldBondForm: React.FC<{ onSubmit: any; isLoading: boolean }> = ({ onSubmit, isLoading }) => {
  const [formData, setFormData] = useState({
    name: 'Sample High-Yield Bond',
    face_value: 1000,
    coupon_rate: 0.08,
    current_price: 950,
    credit_rating: 'BB' as CreditRating,
    maturity_years: 5,
    sector: 'Industrial'
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
    <form onSubmit={(e) => { e.preventDefault(); onSubmit({ ...formData, asset_class: 'fixed_income', currency: 'USD' }); }} style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '12px' }}>
        <div style={{ gridColumn: '1 / -1' }}>
          <label style={labelStyle}>BOND NAME</label>
          <input type="text" value={formData.name} onChange={(e) => setFormData({ ...formData, name: e.target.value })} style={inputStyle} />
        </div>
        <div>
          <label style={labelStyle}>FACE VALUE ($)</label>
          <input type="number" value={formData.face_value} onChange={(e) => setFormData({ ...formData, face_value: Number(e.target.value) })} style={inputStyle} />
        </div>
        <div>
          <label style={labelStyle}>COUPON RATE (%)</label>
          <input type="number" step="0.01" value={formData.coupon_rate * 100} onChange={(e) => setFormData({ ...formData, coupon_rate: Number(e.target.value) / 100 })} style={inputStyle} />
        </div>
        <div>
          <label style={labelStyle}>CURRENT PRICE ($)</label>
          <input type="number" value={formData.current_price} onChange={(e) => setFormData({ ...formData, current_price: Number(e.target.value) })} style={inputStyle} />
        </div>
        <div>
          <label style={labelStyle}>CREDIT RATING</label>
          <select value={formData.credit_rating} onChange={(e) => setFormData({ ...formData, credit_rating: e.target.value as CreditRating })} style={inputStyle}>
            <option value="BBB">BBB (Investment Grade)</option>
            <option value="BB">BB (High Yield)</option>
            <option value="B">B (High Yield)</option>
            <option value="CCC">CCC (High Yield)</option>
          </select>
        </div>
        <div style={{ gridColumn: '1 / -1' }}>
          <label style={labelStyle}>MATURITY (YEARS)</label>
          <input type="number" value={formData.maturity_years} onChange={(e) => setFormData({ ...formData, maturity_years: Number(e.target.value) })} style={inputStyle} />
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
        {isLoading ? 'ANALYZING...' : 'ANALYZE BOND'}
      </button>
    </form>
  );
};

const EmergingMarketBondForm: React.FC<{ onSubmit: any; isLoading: boolean }> = ({ onSubmit, isLoading }) => {
  const [formData, setFormData] = useState({ name: 'EM Bond', face_value: 1000, coupon_rate: 0.06, current_price: 950, sovereign_rating: 'BB' as CreditRating, maturity_years: 10, country: 'Brazil' });
  const inputStyle = { backgroundColor: FINCEPT.DARK_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', padding: '12px', color: FINCEPT.WHITE, fontSize: '11px', width: '100%', outline: 'none' };
  const labelStyle = { display: 'block', fontSize: '10px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '8px', letterSpacing: '0.5px' };
  return (
    <form onSubmit={(e) => { e.preventDefault(); onSubmit({ ...formData, asset_class: 'fixed_income', currency: 'USD' }); }} style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '12px' }}>
        <div><label style={labelStyle}>BOND NAME</label><input type="text" value={formData.name} onChange={(e) => setFormData({ ...formData, name: e.target.value })} style={inputStyle} /></div>
        <div><label style={labelStyle}>COUNTRY</label><input type="text" value={formData.country} onChange={(e) => setFormData({ ...formData, country: e.target.value })} style={inputStyle} /></div>
        <div><label style={labelStyle}>FACE VALUE ($)</label><input type="number" value={formData.face_value} onChange={(e) => setFormData({ ...formData, face_value: Number(e.target.value) })} style={inputStyle} /></div>
        <div><label style={labelStyle}>COUPON RATE (%)</label><input type="number" step="0.01" value={formData.coupon_rate * 100} onChange={(e) => setFormData({ ...formData, coupon_rate: Number(e.target.value) / 100 })} style={inputStyle} /></div>
        <div><label style={labelStyle}>CURRENT PRICE ($)</label><input type="number" value={formData.current_price} onChange={(e) => setFormData({ ...formData, current_price: Number(e.target.value) })} style={inputStyle} /></div>
        <div><label style={labelStyle}>MATURITY (YEARS)</label><input type="number" value={formData.maturity_years} onChange={(e) => setFormData({ ...formData, maturity_years: Number(e.target.value) })} style={inputStyle} /></div>
      </div>
      <button type="submit" disabled={isLoading} style={{ width: '100%', backgroundColor: isLoading ? FINCEPT.MUTED : FINCEPT.ORANGE, color: FINCEPT.WHITE, fontWeight: 700, padding: '16px', borderRadius: '2px', border: 'none', cursor: isLoading ? 'not-allowed' : 'pointer', fontSize: '12px', letterSpacing: '0.5px' }}>{isLoading ? 'ANALYZING...' : 'ANALYZE BOND'}</button>
    </form>
  );
};

const ConvertibleBondForm: React.FC<{ onSubmit: any; isLoading: boolean }> = ({ onSubmit, isLoading }) => {
  const [formData, setFormData] = useState({ name: 'Convertible Bond', face_value: 1000, coupon_rate: 0.03, current_price: 1050, conversion_ratio: 25, stock_price: 40, maturity_years: 5 });
  const inputStyle = { backgroundColor: FINCEPT.DARK_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', padding: '12px', color: FINCEPT.WHITE, fontSize: '11px', width: '100%', outline: 'none' };
  const labelStyle = { display: 'block', fontSize: '10px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '8px', letterSpacing: '0.5px' };
  return (
    <form onSubmit={(e) => { e.preventDefault(); onSubmit({ ...formData, asset_class: 'fixed_income', currency: 'USD' }); }} style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '12px' }}>
        <div style={{ gridColumn: '1 / -1' }}><label style={labelStyle}>BOND NAME</label><input type="text" value={formData.name} onChange={(e) => setFormData({ ...formData, name: e.target.value })} style={inputStyle} /></div>
        <div><label style={labelStyle}>FACE VALUE ($)</label><input type="number" value={formData.face_value} onChange={(e) => setFormData({ ...formData, face_value: Number(e.target.value) })} style={inputStyle} /></div>
        <div><label style={labelStyle}>COUPON RATE (%)</label><input type="number" step="0.01" value={formData.coupon_rate * 100} onChange={(e) => setFormData({ ...formData, coupon_rate: Number(e.target.value) / 100 })} style={inputStyle} /></div>
        <div><label style={labelStyle}>CURRENT PRICE ($)</label><input type="number" value={formData.current_price} onChange={(e) => setFormData({ ...formData, current_price: Number(e.target.value) })} style={inputStyle} /></div>
        <div><label style={labelStyle}>CONVERSION RATIO</label><input type="number" value={formData.conversion_ratio} onChange={(e) => setFormData({ ...formData, conversion_ratio: Number(e.target.value) })} style={inputStyle} /></div>
        <div><label style={labelStyle}>STOCK PRICE ($)</label><input type="number" value={formData.stock_price} onChange={(e) => setFormData({ ...formData, stock_price: Number(e.target.value) })} style={inputStyle} /></div>
        <div><label style={labelStyle}>MATURITY (YEARS)</label><input type="number" value={formData.maturity_years} onChange={(e) => setFormData({ ...formData, maturity_years: Number(e.target.value) })} style={inputStyle} /></div>
      </div>
      <button type="submit" disabled={isLoading} style={{ width: '100%', backgroundColor: isLoading ? FINCEPT.MUTED : FINCEPT.ORANGE, color: FINCEPT.WHITE, fontWeight: 700, padding: '16px', borderRadius: '2px', border: 'none', cursor: isLoading ? 'not-allowed' : 'pointer', fontSize: '12px', letterSpacing: '0.5px' }}>{isLoading ? 'ANALYZING...' : 'ANALYZE BOND'}</button>
    </form>
  );
};

const PreferredStockForm: React.FC<{ onSubmit: any; isLoading: boolean }> = ({ onSubmit, isLoading }) => {
  const [formData, setFormData] = useState({ name: 'Preferred Stock', par_value: 100, dividend_rate: 0.06, current_price: 95, call_price: 105, years_to_call: 5 });
  const inputStyle = { backgroundColor: FINCEPT.DARK_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', padding: '12px', color: FINCEPT.WHITE, fontSize: '11px', width: '100%', outline: 'none' };
  const labelStyle = { display: 'block', fontSize: '10px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '8px', letterSpacing: '0.5px' };
  return (
    <form onSubmit={(e) => { e.preventDefault(); onSubmit({ ...formData, asset_class: 'equity', currency: 'USD' }); }} style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '12px' }}>
        <div style={{ gridColumn: '1 / -1' }}><label style={labelStyle}>STOCK NAME</label><input type="text" value={formData.name} onChange={(e) => setFormData({ ...formData, name: e.target.value })} style={inputStyle} /></div>
        <div><label style={labelStyle}>PAR VALUE ($)</label><input type="number" value={formData.par_value} onChange={(e) => setFormData({ ...formData, par_value: Number(e.target.value) })} style={inputStyle} /></div>
        <div><label style={labelStyle}>DIVIDEND RATE (%)</label><input type="number" step="0.01" value={formData.dividend_rate * 100} onChange={(e) => setFormData({ ...formData, dividend_rate: Number(e.target.value) / 100 })} style={inputStyle} /></div>
        <div><label style={labelStyle}>CURRENT PRICE ($)</label><input type="number" value={formData.current_price} onChange={(e) => setFormData({ ...formData, current_price: Number(e.target.value) })} style={inputStyle} /></div>
        <div><label style={labelStyle}>CALL PRICE ($)</label><input type="number" value={formData.call_price} onChange={(e) => setFormData({ ...formData, call_price: Number(e.target.value) })} style={inputStyle} /></div>
        <div><label style={labelStyle}>YEARS TO CALL</label><input type="number" value={formData.years_to_call} onChange={(e) => setFormData({ ...formData, years_to_call: Number(e.target.value) })} style={inputStyle} /></div>
      </div>
      <button type="submit" disabled={isLoading} style={{ width: '100%', backgroundColor: isLoading ? FINCEPT.MUTED : FINCEPT.ORANGE, color: FINCEPT.WHITE, fontWeight: 700, padding: '16px', borderRadius: '2px', border: 'none', cursor: isLoading ? 'not-allowed' : 'pointer', fontSize: '12px', letterSpacing: '0.5px' }}>{isLoading ? 'ANALYZING...' : 'ANALYZE STOCK'}</button>
    </form>
  );
};

export default BondsView;
