import React, { useState } from 'react';
import { AlternativeInvestmentApi } from '@/services/alternativeInvestments/api/analyticsApi';
import { RealEstateParams, REITParams, RealEstateType } from '@/services/alternativeInvestments/api/types';

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

type RealEstateInvestmentType = 'real-estate' | 'reit';

const TYPES = [
  { id: 'real-estate', name: 'Direct Property' },
  { id: 'reit', name: 'REITs' },
];

export const RealEstateView: React.FC = () => {
  const [selected, setSelected] = useState<RealEstateInvestmentType>('real-estate');
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
        case 'real-estate':
          result = await AlternativeInvestmentApi.analyzeRealEstate(data);
          break;
        case 'reit':
          result = await AlternativeInvestmentApi.analyzeREIT(data);
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
          <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.MUTED, marginBottom: '8px', letterSpacing: '0.5px' }}>INVESTMENT TYPE</div>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '6px' }}>
            {TYPES.map((type) => (
              <button
                key={type.id}
                onClick={() => {
                  setSelected(type.id as RealEstateInvestmentType);
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
                INVESTMENT DETAILS
              </span>
            </div>
            <div style={{ padding: '20px' }}>
              {selected === 'real-estate' && <RealEstateForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />}
              {selected === 'reit' && <REITForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />}
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
                Fill in the investment details and click "ANALYZE"
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
              <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Running real estate analysis</div>
            </div>
          )}
        </div>
      </div>

    </div>
  );
};

const RealEstateForm: React.FC<{onSubmit: any; isLoading: boolean}> = ({ onSubmit, isLoading }) => {
  const [formData, setFormData] = useState({
    name: 'Sample Property',
    property_type: 'multifamily' as RealEstateType,
    acquisition_price: 1000000,
    current_market_value: 1200000,
    annual_noi: 80000,
    ltv_ratio: 0.75,
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
    <form onSubmit={(e) => { e.preventDefault(); onSubmit({...formData, asset_class: 'real_estate', currency: 'USD'}); }} style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '12px' }}>
        <div style={{ gridColumn: '1 / -1' }}>
          <label style={labelStyle}>PROPERTY NAME</label>
          <input type="text" value={formData.name} onChange={(e) => setFormData({...formData, name: e.target.value})} style={inputStyle} />
        </div>
        <div>
          <label style={labelStyle}>PROPERTY TYPE</label>
          <select value={formData.property_type} onChange={(e) => setFormData({...formData, property_type: e.target.value as RealEstateType})} style={inputStyle}>
            <option value="office">Office</option>
            <option value="retail">Retail</option>
            <option value="industrial">Industrial</option>
            <option value="multifamily">Multifamily</option>
            <option value="hotel">Hotel</option>
          </select>
        </div>
        <div>
          <label style={labelStyle}>LTV RATIO</label>
          <input type="number" step="0.01" value={formData.ltv_ratio} onChange={(e) => setFormData({...formData, ltv_ratio: Number(e.target.value)})} style={inputStyle} />
        </div>
        <div>
          <label style={labelStyle}>ACQUISITION PRICE ($)</label>
          <input type="number" value={formData.acquisition_price} onChange={(e) => setFormData({...formData, acquisition_price: Number(e.target.value)})} style={inputStyle} />
        </div>
        <div>
          <label style={labelStyle}>CURRENT MARKET VALUE ($)</label>
          <input type="number" value={formData.current_market_value} onChange={(e) => setFormData({...formData, current_market_value: Number(e.target.value)})} style={inputStyle} />
        </div>
        <div style={{ gridColumn: '1 / -1' }}>
          <label style={labelStyle}>ANNUAL NOI ($)</label>
          <input type="number" value={formData.annual_noi} onChange={(e) => setFormData({...formData, annual_noi: Number(e.target.value)})} style={inputStyle} />
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
        {isLoading ? 'ANALYZING...' : 'ANALYZE PROPERTY'}
      </button>
    </form>
  );
};

const REITForm: React.FC<{onSubmit: any; isLoading: boolean}> = ({ onSubmit, isLoading }) => {
  const [formData, setFormData] = useState({
    name: 'Sample REIT',
    share_price: 50,
    dividend_yield: 0.045,
    ffo_per_share: 3.5,
    occupancy_rate: 0.92,
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
    <form onSubmit={(e) => { e.preventDefault(); onSubmit({...formData, asset_class: 'reit', currency: 'USD'}); }} style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '12px' }}>
        <div style={{ gridColumn: '1 / -1' }}>
          <label style={labelStyle}>REIT NAME</label>
          <input type="text" value={formData.name} onChange={(e) => setFormData({...formData, name: e.target.value})} style={inputStyle} />
        </div>
        <div>
          <label style={labelStyle}>SHARE PRICE ($)</label>
          <input type="number" value={formData.share_price} onChange={(e) => setFormData({...formData, share_price: Number(e.target.value)})} style={inputStyle} />
        </div>
        <div>
          <label style={labelStyle}>DIVIDEND YIELD (%)</label>
          <input type="number" step="0.001" value={formData.dividend_yield * 100} onChange={(e) => setFormData({...formData, dividend_yield: Number(e.target.value) / 100})} style={inputStyle} />
        </div>
        <div>
          <label style={labelStyle}>FFO PER SHARE ($)</label>
          <input type="number" step="0.1" value={formData.ffo_per_share} onChange={(e) => setFormData({...formData, ffo_per_share: Number(e.target.value)})} style={inputStyle} />
        </div>
        <div>
          <label style={labelStyle}>OCCUPANCY RATE (%)</label>
          <input type="number" step="0.01" value={formData.occupancy_rate * 100} onChange={(e) => setFormData({...formData, occupancy_rate: Number(e.target.value) / 100})} style={inputStyle} />
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
        {isLoading ? 'ANALYZING...' : 'ANALYZE REIT'}
      </button>
    </form>
  );
};

export default RealEstateView;
