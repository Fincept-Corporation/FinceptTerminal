import { useTerminalTheme } from '@/contexts/ThemeContext';
import React, { useState } from 'react';
import { RefreshCw, Download, Globe, TrendingUp, BarChart3, PieChart } from 'lucide-react';
import { invoke } from '@tauri-apps/api/core';
import bisDataService from '@/services/bisDataService';

interface Indicator {
  code: string;
  name: string;
  description: string;
}

interface Country {
  code: string;
  name: string;
}

interface Observation {
  date: string;
  value: number;
}

interface IndicatorData {
  indicator: string;
  country: string;
  transform: string;
  observations: Observation[];
  observation_count?: number;
  metadata: any;
}

interface ProfileData {
  country: string;
  indicators: Record<string, { date: string; value: number }>;
}

export default function EconomicsTab() {
  const { colors, fontSize, fontFamily, fontWeight, fontStyle } = useTerminalTheme();
  const [dataSource, setDataSource] = useState<'econdb' | 'bis'>('econdb');
  const [view, setView] = useState<'indicator' | 'profile' | 'countries'>('indicator');
  const [loading, setLoading] = useState(false);
  const [status, setStatus] = useState('Ready');

  // Indicator view state
  const [selectedIndicator, setSelectedIndicator] = useState('GDP');
  const [selectedCountry, setSelectedCountry] = useState('US');
  const [selectedTransform, setSelectedTransform] = useState('level');
  const [indicatorData, setIndicatorData] = useState<IndicatorData | null>(null);

  // BIS-specific state
  const [bisDataType, setBisDataType] = useState<
    'policy_rates' | 'effective_exchange' | 'property_prices' | 'total_credit' |
    'consumer_prices' | 'credit_gap' | 'debt_service' | 'cb_assets'
  >('policy_rates');
  const [bisStartDate, setBisStartDate] = useState(() => {
    const date = new Date();
    date.setFullYear(date.getFullYear() - 1); // 1 year ago
    return date.toISOString().split('T')[0];
  });
  const [bisEndDate, setBisEndDate] = useState(() => new Date().toISOString().split('T')[0]);

  // BIS available countries (only major economies with data)
  const bisCountries = [
    { code: 'US', name: 'United States' },
    { code: 'CN', name: 'China' },
    { code: 'JP', name: 'Japan' },
    { code: 'GB', name: 'United Kingdom' },
    { code: 'IN', name: 'India' },
    { code: 'BR', name: 'Brazil' },
    { code: 'CA', name: 'Canada' },
    { code: 'KR', name: 'South Korea' },
    { code: 'RU', name: 'Russia' },
    { code: 'AU', name: 'Australia' },
    { code: 'MX', name: 'Mexico' },
    { code: 'ID', name: 'Indonesia' },
    { code: 'SA', name: 'Saudi Arabia' },
    { code: 'TR', name: 'Turkey' },
    { code: 'CH', name: 'Switzerland' },
    { code: 'ZA', name: 'South Africa' },
    { code: 'AR', name: 'Argentina' },
    { code: 'SE', name: 'Sweden' },
    { code: 'NO', name: 'Norway' },
    { code: 'PL', name: 'Poland' },
    { code: 'TH', name: 'Thailand' },
    { code: 'MY', name: 'Malaysia' },
    { code: 'SG', name: 'Singapore' },
    { code: 'HK', name: 'Hong Kong' },
    { code: 'NZ', name: 'New Zealand' },
  ];

  // Profile view state
  const [profileCountry, setProfileCountry] = useState('US');
  const [profileData, setProfileData] = useState<ProfileData | null>(null);

  // Available options
  const indicators: Indicator[] = [
    { code: 'GDP', name: 'GDP (Nominal)', description: 'Gross Domestic Product' },
    { code: 'RGDP', name: 'GDP (Real)', description: 'Real GDP' },
    { code: 'CPI', name: 'CPI', description: 'Consumer Price Index' },
    { code: 'PPI', name: 'PPI', description: 'Producer Price Index' },
    { code: 'CORE', name: 'Core CPI', description: 'Core Inflation' },
    { code: 'URATE', name: 'Unemployment', description: 'Unemployment Rate' },
    { code: 'EMP', name: 'Employment', description: 'Employment Level' },
    { code: 'IP', name: 'Industrial Production', description: 'Industrial Production Index' },
    { code: 'RETA', name: 'Retail Sales', description: 'Retail Sales' },
    { code: 'CONF', name: 'Consumer Confidence', description: 'Consumer Confidence Index' },
    { code: 'POLIR', name: 'Policy Rate', description: 'Policy Interest Rate' },
    { code: 'Y10YD', name: '10Y Yield', description: '10-Year Government Bond Yield' },
    { code: 'M3YD', name: '3M Yield', description: '3-Month Treasury Yield' },
    { code: 'GDEBT', name: 'Government Debt', description: 'Government Debt' },
    { code: 'CA', name: 'Current Account', description: 'Current Account Balance' },
    { code: 'TB', name: 'Trade Balance', description: 'Trade Balance' },
    { code: 'POP', name: 'Population', description: 'Total Population' },
  ];

  const countries: Country[] = [
    { code: 'US', name: 'United States' },
    { code: 'CN', name: 'China' },
    { code: 'JP', name: 'Japan' },
    { code: 'DE', name: 'Germany' },
    { code: 'UK', name: 'United Kingdom' },
    { code: 'FR', name: 'France' },
    { code: 'IN', name: 'India' },
    { code: 'IT', name: 'Italy' },
    { code: 'BR', name: 'Brazil' },
    { code: 'CA', name: 'Canada' },
    { code: 'KR', name: 'South Korea' },
    { code: 'RU', name: 'Russia' },
    { code: 'AU', name: 'Australia' },
    { code: 'ES', name: 'Spain' },
    { code: 'MX', name: 'Mexico' },
    { code: 'ID', name: 'Indonesia' },
    { code: 'NL', name: 'Netherlands' },
    { code: 'SA', name: 'Saudi Arabia' },
    { code: 'TR', name: 'Turkey' },
    { code: 'CH', name: 'Switzerland' },
  ];

  const transforms = [
    { code: 'level', name: 'Level', description: 'No transformation' },
    { code: 'toya', name: 'YoY %', description: 'Year-over-year change' },
    { code: 'tpop', name: 'QoQ %', description: 'Quarter-over-quarter change' },
    { code: 'tusd', name: 'USD', description: 'Convert to USD' },
    { code: 'tpgp', name: '% of GDP', description: 'As percent of GDP' },
  ];

  const fetchIndicator = async () => {
    setLoading(true);
    setStatus(`Fetching data for ${selectedCountry}...`);

    try {
      if (dataSource === 'bis') {
        // Fetch from BIS based on selected data type with date range
        let result;

        // Map data type to BIS dataset and frequency
        const bisDatasetMap: Record<string, { flow: string; freq: string }> = {
          policy_rates: { flow: 'WS_CBPOL', freq: 'D' },
          effective_exchange: { flow: 'WS_EER', freq: 'M' },
          property_prices: { flow: 'WS_SPP', freq: 'Q' },
          total_credit: { flow: 'WS_TC', freq: 'Q' },
          consumer_prices: { flow: 'WS_LONG_CPI', freq: 'M' },
          credit_gap: { flow: 'WS_CREDIT_GAP', freq: 'Q' },
          debt_service: { flow: 'WS_DSR', freq: 'Q' },
          cb_assets: { flow: 'WS_CBTA', freq: 'M' },
        };

        const dataset = bisDatasetMap[bisDataType];

        if (bisDataType === 'policy_rates') {
          result = await bisDataService.getCentralBankPolicyRates([selectedCountry], bisStartDate, bisEndDate);
        } else if (bisDataType === 'effective_exchange') {
          result = await bisDataService.getEffectiveExchangeRates([selectedCountry], bisStartDate, bisEndDate);
        } else {
          // Use generic get_data for other datasets
          result = await bisDataService.getData(
            dataset.flow,
            `${dataset.freq}.${selectedCountry}`,
            bisStartDate,
            bisEndDate
          );
        }

        if (result.success && result.data) {
          // Transform BIS SDMX-JSON data to match indicatorData format
          let observations: Observation[] = [];

          try {
            // BIS returns SDMX-JSON format: data.data.dataSets[0].series["0:0"].observations
            const dataSets = result.data.data?.dataSets || [];
            if (dataSets.length > 0) {
              const dataSet = dataSets[0];
              const series = dataSet.series;
              const seriesKey = Object.keys(series || {})[0];

              if (seriesKey && series[seriesKey]) {
                const obsData = series[seriesKey].observations;

                // Try to get TIME_PERIOD dimension values from structure
                const timeDimension = result.data.structure?.dimensions?.observation?.find(
                  (d: any) => d.id === 'TIME_PERIOD'
                );
                const timeValues = timeDimension?.values || [];

                // Convert SDMX observations to our format
                observations = Object.entries(obsData || {}).map(([index, value]: [string, any]) => {
                  const timeIndex = parseInt(index);
                  let dateValue: string;

                  // Use TIME_PERIOD values if available, otherwise generate from reportingBegin
                  if (timeValues.length > timeIndex) {
                    dateValue = timeValues[timeIndex].id || timeValues[timeIndex].name;
                  } else {
                    // Fallback: generate date from reportingBegin
                    const reportingBegin = dataSet.reportingBegin || '2020-01-01';
                    const startDate = new Date(reportingBegin.split('T')[0]);
                    const obsDate = new Date(startDate);
                    obsDate.setDate(obsDate.getDate() + timeIndex);
                    dateValue = obsDate.toISOString().split('T')[0];
                  }

                  const numValue = Array.isArray(value) ? parseFloat(value[0]) : parseFloat(value);

                  return {
                    date: dateValue,
                    value: isNaN(numValue) ? 0 : numValue
                  };
                });
              }
            }
          } catch (parseError) {
            console.error('Error parsing BIS data:', parseError);
            setStatus(`Error parsing BIS data: ${parseError}`);
            return;
          }

          if (observations.length === 0) {
            setStatus('No observations found in BIS data');
            return;
          }

          setIndicatorData({
            indicator: bisDataType,
            country: selectedCountry,
            transform: 'level',
            observations: observations,
            observation_count: observations.length,
            metadata: result.data
          });
          setStatus(`Loaded ${observations.length} observations from BIS`);
        } else {
          setStatus(`Error: ${result.error || 'Failed to fetch BIS data'}`);
        }
      } else {
        // Fetch from EconDB
        const result = await invoke('execute_python_script', {
          scriptName: 'econdb_data.py',
          args: ['indicator', selectedIndicator, selectedCountry, '', '', selectedTransform]
        }) as string;

        const data = JSON.parse(result);

        if (data.error) {
          setStatus(`Error: ${data.error}`);
        } else {
          setIndicatorData(data);
          setStatus(`Loaded ${data.observation_count} observations from EconDB`);
        }
      }
    } catch (error) {
      setStatus(`Error: ${error}`);
    } finally {
      setLoading(false);
    }
  };

  const fetchProfile = async () => {
    setLoading(true);
    setStatus(`Fetching profile for ${profileCountry}...`);

    try {
      const result = await invoke('execute_python_script', {
        scriptName: 'econdb_data.py',
        args: ['profile', profileCountry, 'true']
      }) as string;

      const data = JSON.parse(result);

      if (data.error) {
        setStatus(`Error: ${data.error}`);
      } else {
        setProfileData(data);
        setStatus(`Profile loaded for ${profileCountry}`);
      }
    } catch (error) {
      setStatus(`Error: ${error}`);
    } finally {
      setLoading(false);
    }
  };

  const exportData = () => {
    if (!indicatorData) return;

    const csv = [
      'Date,Value',
      ...indicatorData.observations.map(obs => `${obs.date},${obs.value}`)
    ].join('\n');

    const blob = new Blob([csv], { type: 'text/csv' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `econdb_${selectedIndicator}_${selectedCountry}_${Date.now()}.csv`;
    a.click();
    URL.revokeObjectURL(url);
    setStatus('Data exported');
  };

  const renderChart = (data: Observation[]) => {
    if (data.length === 0) return null;

    const w = 800;
    const h = 300;
    const ml = 70;
    const mr = 20;
    const mt = 20;
    const mb = 50;
    const pw = w - ml - mr;
    const ph = h - mt - mb;

    const values = data.map(d => d.value);
    const yMin = Math.min(...values) * 0.95;
    const yMax = Math.max(...values) * 1.05;
    const yRange = yMax - yMin;

    return (
      <svg width={w} height={h} style={{ backgroundColor: 'colors.panel', width: '100%', height: 'auto' }}>
        {/* Axes */}
        <line x1={ml} y1={mt} x2={ml} y2={h - mb} stroke="#404040" strokeWidth="1" />
        <line x1={ml} y1={h - mb} x2={w - mr} y2={h - mb} stroke="#404040" strokeWidth="1" />

        {/* Y-axis grid and labels */}
        {[0, 1, 2, 3, 4].map(i => {
          const val = yMin + (yRange * i) / 4;
          const y = h - mb - (ph * i) / 4;
          return (
            <g key={i}>
              <line x1={ml} y1={y} x2={w - mr} y2={y} stroke="#1a1a1a" strokeDasharray="2,2" />
              <text x={ml - 10} y={y + 4} textAnchor="end" fill="#737373" fontSize="10" fontFamily="monospace">
                {val.toFixed(2)}
              </text>
            </g>
          );
        })}

        {/* Line and points */}
        {data.map((obs, idx) => {
          const x = ml + (pw * idx) / (data.length - 1 || 1);
          const y = h - mb - (ph * ((obs.value - yMin) / yRange));

          return (
            <g key={idx}>
              <circle cx={x} cy={y} r="3" fill="#ea580c" />
              {idx < data.length - 1 && (
                <line
                  x1={x}
                  y1={y}
                  x2={ml + (pw * (idx + 1)) / (data.length - 1 || 1)}
                  y2={h - mb - (ph * ((data[idx + 1].value - yMin) / yRange))}
                  stroke="#ea580c"
                  strokeWidth="2"
                />
              )}
            </g>
          );
        })}

        {/* X-axis labels */}
        {data.map((obs, idx) => {
          if (idx % Math.max(1, Math.floor(data.length / 8)) !== 0 && idx !== data.length - 1) return null;
          const x = ml + (pw * idx) / (data.length - 1 || 1);
          return (
            <text key={idx} x={x} y={h - mb + 20} textAnchor="middle" fill="#737373" fontSize="9" fontFamily="monospace">
              {obs.date.substring(0, 10)}
            </text>
          );
        })}
      </svg>
    );
  };

  return (
    <div style={{ width: '100%', height: '100%', backgroundColor: 'colors.background', color: '#d4d4d4', fontFamily: 'Consolas, monospace', fontSize: '11px', display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
      {/* Header */}
      <div style={{ padding: '8px 12px', borderBottom: '1px solid #404040', display: 'flex', alignItems: 'center', gap: '12px', flexShrink: 0 }}>
        <span style={{ color: '#ea580c', fontWeight: 'bold' }}>ECONOMICS - {dataSource.toUpperCase()}</span>
        <span style={{ color: '#10b981', fontSize: '10px' }}>● {status}</span>
        <div style={{ flex: 1 }}></div>

        {/* Data Source Toggle */}
        <div style={{ display: 'flex', gap: '4px', marginRight: '12px' }}>
          <button
            onClick={() => setDataSource('econdb')}
            style={{
              padding: '4px 8px',
              backgroundColor: dataSource === 'econdb' ? '#3b82f6' : '#2d2d2d',
              color: colors.text,
              border: 'none',
              cursor: 'pointer',
              fontSize: '9px',
              borderRadius: '2px'
            }}
          >
            EconDB
          </button>
          <button
            onClick={() => setDataSource('bis')}
            style={{
              padding: '4px 8px',
              backgroundColor: dataSource === 'bis' ? '#3b82f6' : '#2d2d2d',
              color: colors.text,
              border: 'none',
              cursor: 'pointer',
              fontSize: '9px',
              borderRadius: '2px'
            }}
          >
            BIS
          </button>
        </div>

        <button onClick={() => setView('indicator')} style={{ padding: '4px 10px', backgroundColor: view === 'indicator' ? '#ea580c' : '#2d2d2d', color: 'colors.text', border: 'none', cursor: 'pointer', fontSize: '9px' }}>
          <BarChart3 size={10} style={{ display: 'inline', marginRight: '4px' }} />INDICATOR
        </button>
        <button onClick={() => setView('profile')} style={{ padding: '4px 10px', backgroundColor: view === 'profile' ? '#ea580c' : '#2d2d2d', color: 'colors.text', border: 'none', cursor: 'pointer', fontSize: '9px' }}>
          <PieChart size={10} style={{ display: 'inline', marginRight: '4px' }} />PROFILE
        </button>
        <button onClick={() => setView('countries')} style={{ padding: '4px 10px', backgroundColor: view === 'countries' ? '#ea580c' : '#2d2d2d', color: 'colors.text', border: 'none', cursor: 'pointer', fontSize: '9px' }}>
          <Globe size={10} style={{ display: 'inline', marginRight: '4px' }} />COUNTRIES
        </button>
      </div>

      {/* Main Content */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* Left Panel - Controls */}
        <div style={{ width: '300px', borderRight: '1px solid #404040', display: 'flex', flexDirection: 'column', backgroundColor: 'colors.panel', overflowY: 'auto' }}>
          {view === 'indicator' && (
            <div style={{ padding: '12px' }}>
              {dataSource === 'bis' ? (
                <>
                  {/* BIS-specific controls */}
                  <div style={{ color: '#ea580c', fontSize: '10px', marginBottom: '8px' }}>BIS DATA TYPE</div>
                  <select value={bisDataType} onChange={(e) => setBisDataType(e.target.value as any)} style={{ width: '100%', padding: '6px', backgroundColor: '#1a1a1a', color: '#d4d4d4', border: '1px solid #404040', marginBottom: '12px', fontSize: '10px' }}>
                    <optgroup label="Interest Rates & FX">
                      <option value="policy_rates">Central Bank Policy Rates (Daily)</option>
                      <option value="effective_exchange">Effective Exchange Rates (Monthly)</option>
                    </optgroup>
                    <optgroup label="Property & Credit">
                      <option value="property_prices">Property Prices (Quarterly)</option>
                      <option value="total_credit">Total Credit (Quarterly)</option>
                      <option value="credit_gap">Credit-to-GDP Gaps (Quarterly)</option>
                      <option value="debt_service">Debt Service Ratios (Quarterly)</option>
                    </optgroup>
                    <optgroup label="Other Indicators">
                      <option value="consumer_prices">Consumer Prices (Monthly)</option>
                      <option value="cb_assets">Central Bank Assets (Monthly)</option>
                    </optgroup>
                  </select>

                  <div style={{ color: '#ea580c', fontSize: '10px', marginBottom: '8px' }}>COUNTRY (G20 + Major Economies)</div>
                  <select value={selectedCountry} onChange={(e) => setSelectedCountry(e.target.value)} style={{ width: '100%', padding: '6px', backgroundColor: '#1a1a1a', color: '#d4d4d4', border: '1px solid #404040', marginBottom: '12px', fontSize: '10px' }}>
                    {bisCountries.map(c => (
                      <option key={c.code} value={c.code}>{c.name}</option>
                    ))}
                  </select>

                  <div style={{ color: '#ea580c', fontSize: '10px', marginBottom: '8px' }}>START DATE</div>
                  <input
                    type="date"
                    value={bisStartDate}
                    onChange={(e) => setBisStartDate(e.target.value)}
                    style={{ width: '100%', padding: '6px', backgroundColor: '#1a1a1a', color: '#d4d4d4', border: '1px solid #404040', marginBottom: '12px', fontSize: '10px' }}
                  />

                  <div style={{ color: '#ea580c', fontSize: '10px', marginBottom: '8px' }}>END DATE</div>
                  <input
                    type="date"
                    value={bisEndDate}
                    onChange={(e) => setBisEndDate(e.target.value)}
                    style={{ width: '100%', padding: '6px', backgroundColor: '#1a1a1a', color: '#d4d4d4', border: '1px solid #404040', marginBottom: '12px', fontSize: '10px' }}
                  />

                  <div style={{ padding: '8px', backgroundColor: '#1a1a1a', border: '1px solid #404040', fontSize: '9px', marginBottom: '12px' }}>
                    <div style={{ color: '#3b82f6', marginBottom: '4px' }}>ℹ️ BIS Data Info</div>
                    <div style={{ color: '#737373', lineHeight: '1.4' }}>
                      Limit date range to avoid slow loading. Recommended: 1-2 years max.
                    </div>
                  </div>
                </>
              ) : (
                <>
                  {/* EconDB controls */}
                  <div style={{ color: '#ea580c', fontSize: '10px', marginBottom: '8px' }}>INDICATOR</div>
                  <select value={selectedIndicator} onChange={(e) => setSelectedIndicator(e.target.value)} style={{ width: '100%', padding: '6px', backgroundColor: '#1a1a1a', color: '#d4d4d4', border: '1px solid #404040', marginBottom: '12px', fontSize: '10px' }}>
                    {indicators.map(ind => (
                      <option key={ind.code} value={ind.code}>{ind.name}</option>
                    ))}
                  </select>

                  <div style={{ color: '#ea580c', fontSize: '10px', marginBottom: '8px' }}>COUNTRY</div>
                  <select value={selectedCountry} onChange={(e) => setSelectedCountry(e.target.value)} style={{ width: '100%', padding: '6px', backgroundColor: '#1a1a1a', color: '#d4d4d4', border: '1px solid #404040', marginBottom: '12px', fontSize: '10px' }}>
                    {countries.map(c => (
                      <option key={c.code} value={c.code}>{c.name}</option>
                    ))}
                  </select>

                  <div style={{ color: '#ea580c', fontSize: '10px', marginBottom: '8px' }}>TRANSFORM</div>
                  <select value={selectedTransform} onChange={(e) => setSelectedTransform(e.target.value)} style={{ width: '100%', padding: '6px', backgroundColor: '#1a1a1a', color: '#d4d4d4', border: '1px solid #404040', marginBottom: '16px', fontSize: '10px' }}>
                    {transforms.map(t => (
                      <option key={t.code} value={t.code}>{t.name}</option>
                    ))}
                  </select>
                </>
              )}

              <button onClick={fetchIndicator} disabled={loading} style={{ width: '100%', padding: '8px', backgroundColor: '#ea580c', color: 'colors.text', border: 'none', cursor: loading ? 'not-allowed' : 'pointer', fontSize: '10px', marginBottom: '8px' }}>
                {loading ? 'Loading...' : 'FETCH DATA'}
              </button>

              <button onClick={exportData} disabled={!indicatorData} style={{ width: '100%', padding: '8px', backgroundColor: '#2d2d2d', color: '#d4d4d4', border: '1px solid #404040', cursor: !indicatorData ? 'not-allowed' : 'pointer', fontSize: '10px' }}>
                <Download size={10} style={{ display: 'inline', marginRight: '4px' }} />EXPORT CSV
              </button>

              {indicatorData && (
                <div style={{ marginTop: '16px', padding: '8px', backgroundColor: '#1a1a1a', border: '1px solid #404040', fontSize: '9px' }}>
                  <div style={{ color: '#fbbf24', marginBottom: '4px' }}>METADATA</div>
                  <div style={{ color: '#737373' }}>Country: {indicatorData.country}</div>
                  <div style={{ color: '#737373' }}>Indicator: {indicatorData.indicator}</div>
                  <div style={{ color: '#737373' }}>Transform: {indicatorData.transform}</div>
                  <div style={{ color: '#737373' }}>Points: {indicatorData.observation_count}</div>
                </div>
              )}
            </div>
          )}

          {view === 'profile' && (
            <div style={{ padding: '12px' }}>
              <div style={{ color: '#ea580c', fontSize: '10px', marginBottom: '8px' }}>SELECT COUNTRY</div>
              <select value={profileCountry} onChange={(e) => setProfileCountry(e.target.value)} style={{ width: '100%', padding: '6px', backgroundColor: '#1a1a1a', color: '#d4d4d4', border: '1px solid #404040', marginBottom: '16px', fontSize: '10px' }}>
                {countries.map(c => (
                  <option key={c.code} value={c.code}>{c.name}</option>
                ))}
              </select>

              <button onClick={fetchProfile} disabled={loading} style={{ width: '100%', padding: '8px', backgroundColor: '#ea580c', color: 'colors.text', border: 'none', cursor: loading ? 'not-allowed' : 'pointer', fontSize: '10px' }}>
                {loading ? 'Loading...' : 'LOAD PROFILE'}
              </button>
            </div>
          )}

          {view === 'countries' && (
            <div style={{ padding: '12px' }}>
              <div style={{ color: '#ea580c', fontSize: '10px', marginBottom: '8px' }}>AVAILABLE COUNTRIES ({countries.length})</div>
              {countries.map(c => (
                <div key={c.code} style={{ padding: '6px 8px', backgroundColor: '#1a1a1a', border: '1px solid #404040', marginBottom: '4px', fontSize: '10px' }}>
                  <span style={{ color: '#fbbf24' }}>{c.code}</span> - {c.name}
                </div>
              ))}
            </div>
          )}
        </div>

        {/* Right Panel - Data Display */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
          <div style={{ flex: 1, overflowY: 'auto', padding: '16px' }}>
            {view === 'indicator' && indicatorData && (
              <>
                <div style={{ marginBottom: '16px' }}>
                  <div style={{ color: '#ea580c', fontSize: '12px', marginBottom: '8px' }}>
                    {indicatorData.metadata.description || indicatorData.indicator}
                  </div>
                  <div style={{ color: '#737373', fontSize: '10px' }}>
                    {indicatorData.country} • {indicatorData.observation_count} observations
                  </div>
                </div>

                {renderChart(indicatorData.observations)}

                <div style={{ marginTop: '24px' }}>
                  <div style={{ color: '#ea580c', fontSize: '11px', marginBottom: '8px' }}>DATA TABLE</div>
                  <div style={{ maxHeight: '400px', overflowY: 'auto', border: '1px solid #404040', backgroundColor: 'colors.panel' }}>
                    <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '10px' }}>
                      <thead style={{ position: 'sticky', top: 0, backgroundColor: '#1a1a1a' }}>
                        <tr>
                          <th style={{ padding: '8px', textAlign: 'left', color: '#ea580c', borderBottom: '2px solid #404040' }}>Date</th>
                          <th style={{ padding: '8px', textAlign: 'right', color: '#ea580c', borderBottom: '2px solid #404040' }}>Value</th>
                        </tr>
                      </thead>
                      <tbody>
                        {indicatorData.observations.slice().reverse().map((obs, idx) => (
                          <tr key={idx} style={{ borderBottom: '1px solid #1a1a1a' }}>
                            <td style={{ padding: '6px 8px', color: '#d4d4d4' }}>{obs.date}</td>
                            <td style={{ padding: '6px 8px', textAlign: 'right', color: '#a3a3a3' }}>{obs.value.toFixed(4)}</td>
                          </tr>
                        ))}
                      </tbody>
                    </table>
                  </div>
                </div>
              </>
            )}

            {view === 'profile' && profileData && (
              <div>
                <div style={{ color: '#ea580c', fontSize: '12px', marginBottom: '16px' }}>
                  ECONOMIC PROFILE - {profileData.country}
                </div>
                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(300px, 1fr))', gap: '12px' }}>
                  {Object.entries(profileData.indicators).map(([name, data]) => (
                    <div key={name} style={{ padding: '12px', backgroundColor: 'colors.panel', border: '1px solid #404040' }}>
                      <div style={{ color: '#fbbf24', fontSize: '10px', marginBottom: '8px' }}>{name}</div>
                      <div style={{ color: 'colors.text', fontSize: '20px', fontWeight: 'bold', marginBottom: '4px' }}>
                        {typeof data.value === 'number' ? data.value.toFixed(4) : data.value}
                      </div>
                      <div style={{ color: '#737373', fontSize: '9px' }}>{data.date}</div>
                    </div>
                  ))}
                </div>
              </div>
            )}

            {!indicatorData && !profileData && view !== 'countries' && (
              <div style={{ textAlign: 'center', color: '#737373', paddingTop: '80px' }}>
                <TrendingUp size={48} style={{ margin: '0 auto 16px', opacity: 0.3 }} />
                <div>Select parameters and fetch data to begin</div>
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}
