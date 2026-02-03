import React, { useState } from 'react';
import { AlternativeInvestmentApi } from '@/services/alternativeInvestments/api/analyticsApi';
import { PreciousMetalsParams, NaturalResourceParams, MetalType } from '@/services/alternativeInvestments/api/types';

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

type CommodityType = 'precious-metals' | 'natural-resources';

const COMMODITY_TYPES = [
  { id: 'precious-metals', name: 'Precious Metals' },
  { id: 'natural-resources', name: 'Natural Resources' },
];

export const CommoditiesView: React.FC = () => {
  const [selected, setSelected] = useState<CommodityType>('precious-metals');
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
        case 'precious-metals':
          result = await AlternativeInvestmentApi.analyzePreciousMetals(data);
          break;
        case 'natural-resources':
          result = await AlternativeInvestmentApi.analyzeNaturalResources(data);
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
          <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.MUTED, marginBottom: '8px', letterSpacing: '0.5px' }}>COMMODITY TYPE</div>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '6px' }}>
            {COMMODITY_TYPES.map((type) => (
              <button
                key={type.id}
                onClick={() => {
                  setSelected(type.id as CommodityType);
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
                COMMODITY DETAILS
              </span>
            </div>
            <div style={{ padding: '20px' }}>
              {selected === 'precious-metals' && <PreciousMetalForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />}
              {selected === 'natural-resources' && <NaturalResourceForm onSubmit={handleAnalyze} isLoading={isAnalyzing} />}
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
                Fill in the commodity details and click "ANALYZE" to get professional analysis
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
              <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Running commodity analysis</div>
            </div>
          )}
        </div>
      </div>

    </div>
  );
};

const PreciousMetalForm: React.FC<{onSubmit: any; isLoading: boolean}> = ({ onSubmit, isLoading }) => {
  const [formData, setFormData] = useState({
    name: 'Gold Investment',
    metal_type: 'gold' as MetalType,
    purchase_price: 1800,
    current_price: 1900,
    weight_oz: 10,
    storage_cost_annual: 100,
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
    <form onSubmit={(e) => { e.preventDefault(); onSubmit({...formData, asset_class: 'commodities', currency: 'USD'}); }} style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '12px' }}>
        <div style={{ gridColumn: '1 / -1' }}>
          <label style={labelStyle}>INVESTMENT NAME</label>
          <input type="text" value={formData.name} onChange={(e) => setFormData({...formData, name: e.target.value})} style={inputStyle} />
        </div>
        <div>
          <label style={labelStyle}>METAL TYPE</label>
          <select value={formData.metal_type} onChange={(e) => setFormData({...formData, metal_type: e.target.value as MetalType})} style={inputStyle}>
            <option value="gold">Gold</option>
            <option value="silver">Silver</option>
            <option value="platinum">Platinum</option>
            <option value="palladium">Palladium</option>
          </select>
        </div>
        <div>
          <label style={labelStyle}>WEIGHT (OZ)</label>
          <input type="number" step="0.1" value={formData.weight_oz} onChange={(e) => setFormData({...formData, weight_oz: Number(e.target.value)})} style={inputStyle} />
        </div>
        <div>
          <label style={labelStyle}>PURCHASE PRICE ($/OZ)</label>
          <input type="number" value={formData.purchase_price} onChange={(e) => setFormData({...formData, purchase_price: Number(e.target.value)})} style={inputStyle} />
        </div>
        <div>
          <label style={labelStyle}>CURRENT PRICE ($/OZ)</label>
          <input type="number" value={formData.current_price} onChange={(e) => setFormData({...formData, current_price: Number(e.target.value)})} style={inputStyle} />
        </div>
        <div style={{ gridColumn: '1 / -1' }}>
          <label style={labelStyle}>ANNUAL STORAGE COST ($)</label>
          <input type="number" value={formData.storage_cost_annual} onChange={(e) => setFormData({...formData, storage_cost_annual: Number(e.target.value)})} style={inputStyle} />
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
        {isLoading ? 'ANALYZING...' : 'ANALYZE PRECIOUS METAL'}
      </button>
    </form>
  );
};

const NaturalResourceForm: React.FC<{onSubmit: any; isLoading: boolean}> = ({ onSubmit, isLoading }) => {
  const [formData, setFormData] = useState({
    name: 'Oil & Gas Investment',
    resource_type: 'energy',
    purchase_price: 50000,
    current_value: 55000,
    annual_yield: 0.08,
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
    <form onSubmit={(e) => { e.preventDefault(); onSubmit({...formData, asset_class: 'natural_resources', currency: 'USD'}); }} style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '12px' }}>
        <div>
          <label style={labelStyle}>INVESTMENT NAME</label>
          <input type="text" value={formData.name} onChange={(e) => setFormData({...formData, name: e.target.value})} style={inputStyle} />
        </div>
        <div>
          <label style={labelStyle}>RESOURCE TYPE</label>
          <select value={formData.resource_type} onChange={(e) => setFormData({...formData, resource_type: e.target.value})} style={inputStyle}>
            <option value="energy">Energy (Oil/Gas)</option>
            <option value="agriculture">Agriculture</option>
            <option value="timber">Timber</option>
            <option value="water">Water Rights</option>
          </select>
        </div>
        <div>
          <label style={labelStyle}>PURCHASE PRICE ($)</label>
          <input type="number" value={formData.purchase_price} onChange={(e) => setFormData({...formData, purchase_price: Number(e.target.value)})} style={inputStyle} />
        </div>
        <div>
          <label style={labelStyle}>CURRENT VALUE ($)</label>
          <input type="number" value={formData.current_value} onChange={(e) => setFormData({...formData, current_value: Number(e.target.value)})} style={inputStyle} />
        </div>
        <div style={{ gridColumn: '1 / -1' }}>
          <label style={labelStyle}>ANNUAL YIELD (%)</label>
          <input type="number" step="0.01" value={formData.annual_yield * 100} onChange={(e) => setFormData({...formData, annual_yield: Number(e.target.value) / 100})} style={inputStyle} />
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
        {isLoading ? 'ANALYZING...' : 'ANALYZE NATURAL RESOURCE'}
      </button>
    </form>
  );
};

export default CommoditiesView;
