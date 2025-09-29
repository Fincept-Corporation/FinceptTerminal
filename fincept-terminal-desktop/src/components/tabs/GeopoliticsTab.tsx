import React, { useState, useEffect } from 'react';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer, BarChart, Bar, PieChart, Pie, Cell, AreaChart, Area, ScatterChart, Scatter, RadarChart, PolarGrid, PolarAngleAxis, PolarRadiusAxis, Radar } from 'recharts';

const GeopoliticsTab: React.FC = () => {
  const [currentTime, setCurrentTime] = useState(new Date());
  const [activePanel, setActivePanel] = useState('overview');
  const [alertCount, setAlertCount] = useState(127);
  const [dataUpdateCount, setDataUpdateCount] = useState(0);

  // Bloomberg color scheme
  const BLOOMBERG_ORANGE = '#FFA500';
  const BLOOMBERG_WHITE = '#FFFFFF';
  const BLOOMBERG_RED = '#FF0000';
  const BLOOMBERG_GREEN = '#00C800';
  const BLOOMBERG_YELLOW = '#FFFF00';
  const BLOOMBERG_GRAY = '#787878';
  const BLOOMBERG_BLUE = '#64B4FF';
  const BLOOMBERG_PURPLE = '#C864FF';
  const BLOOMBERG_CYAN = '#00FFFF';
  const BLOOMBERG_DARK_BG = '#000000';
  const BLOOMBERG_PANEL_BG = '#0a0a0a';

  // Risk colors
  const RISK_CRITICAL = '#FF0000';
  const RISK_HIGH = '#FF6400';
  const RISK_MEDIUM = '#FFC800';
  const RISK_LOW = '#64FF64';

  useEffect(() => {
    const timer = setInterval(() => {
      setCurrentTime(new Date());
      setDataUpdateCount(prev => prev + 1);
      if (Math.random() > 0.7) setAlertCount(prev => prev + 1);
    }, 2000);
    return () => clearInterval(timer);
  }, []);

  // Complex chart data
  const riskForecastData = Array.from({ length: 90 }, (_, i) => ({
    day: i + 1,
    baseRisk: 7.2 + Math.sin(i * 0.1) * 0.8 + Math.random() * 0.4,
    militaryRisk: 8.1 + Math.cos(i * 0.08) * 0.6 + Math.random() * 0.3,
    economicRisk: 6.8 + Math.sin(i * 0.12) * 0.9 + Math.random() * 0.5,
    cyberRisk: 7.5 + Math.cos(i * 0.15) * 0.7 + Math.random() * 0.4,
    nuclearRisk: 7.0 + Math.sin(i * 0.05) * 0.5 + Math.random() * 0.2,
    volatility: 2.1 + Math.random() * 1.8
  }));

  const correlationMatrix = [
    { asset: 'Energy', military: 0.89, economic: 0.76, cyber: 0.45, nuclear: 0.67, volatility: 3.2 },
    { asset: 'Tech', military: 0.23, economic: 0.84, cyber: 0.91, nuclear: 0.34, volatility: 2.8 },
    { asset: 'Defense', military: 0.94, economic: 0.56, cyber: 0.67, nuclear: 0.78, volatility: 2.1 },
    { asset: 'Finance', military: 0.45, economic: 0.91, cyber: 0.72, nuclear: 0.43, volatility: 2.4 },
    { asset: 'Materials', military: 0.78, economic: 0.69, cyber: 0.34, nuclear: 0.56, volatility: 3.1 },
    { asset: 'Crypto', military: 0.12, economic: 0.67, cyber: 0.89, nuclear: 0.23, volatility: 4.8 },
    { asset: 'Commodities', military: 0.82, economic: 0.74, cyber: 0.28, nuclear: 0.61, volatility: 3.6 }
  ];

  const threatRadarData = [
    { threat: 'Military', current: 8.1, max: 10, trend: 'up' },
    { threat: 'Economic', current: 7.4, max: 10, trend: 'up' },
    { threat: 'Cyber', current: 7.8, max: 10, trend: 'up' },
    { threat: 'Nuclear', current: 7.2, max: 10, trend: 'stable' },
    { threat: 'Climate', current: 6.4, max: 10, trend: 'up' },
    { threat: 'Terror', current: 5.8, max: 10, trend: 'down' }
  ];

  const countryRiskData = [
    { country: 'Russia', military: 9.2, economic: 8.1, political: 4.0, total: 7.1, trend: 'up', sanctions: 1247 },
    { country: 'China', military: 8.5, economic: 7.8, political: 6.5, total: 7.6, trend: 'up', sanctions: 89 },
    { country: 'Iran', military: 7.9, economic: 8.3, political: 3.2, total: 6.5, trend: 'stable', sanctions: 891 },
    { country: 'N. Korea', military: 8.7, economic: 6.2, political: 2.1, total: 5.7, trend: 'up', sanctions: 456 },
    { country: 'Pakistan', military: 6.8, economic: 8.1, political: 3.8, total: 6.2, trend: 'up', sanctions: 34 },
    { country: 'India', military: 5.2, economic: 4.1, political: 6.8, total: 5.4, trend: 'stable', sanctions: 0 },
    { country: 'USA', military: 4.1, economic: 5.2, political: 7.0, total: 5.4, trend: 'stable', sanctions: 0 },
    { country: 'EU', military: 4.8, economic: 6.1, political: 7.3, total: 6.1, trend: 'down', sanctions: 12 }
  ];

  const marketImpactData = [
    { sector: 'Energy', correlation: 0.89, impact: 3.8, volume: '2.3B', change: '+2.4%' },
    { sector: 'Defense', correlation: 0.94, impact: 4.2, volume: '890M', change: '+3.1%' },
    { sector: 'Tech', correlation: 0.84, impact: -2.1, volume: '4.7B', change: '-1.8%' },
    { sector: 'Finance', correlation: 0.71, impact: -1.6, volume: '3.2B', change: '-0.9%' },
    { sector: 'Materials', correlation: 0.80, impact: 2.8, volume: '1.8B', change: '+2.2%' },
    { sector: 'Healthcare', correlation: 0.34, impact: 0.2, volume: '1.4B', change: '+0.1%' },
    { sector: 'Utilities', correlation: 0.45, impact: 1.1, volume: '780M', change: '+0.8%' },
    { sector: 'Crypto', correlation: 0.67, impact: -4.2, volume: '890M', change: '-3.7%' }
  ];

  const intelligenceStream = [
    { time: "16:47:23", priority: "FLASH", source: "SIGINT", classification: "TS/SCI", region: "LAC", event: "PLA forces movement detected near Galwan Valley - 3 battalions repositioning", confidence: 95 },
    { time: "16:45:12", priority: "URGENT", source: "HUMINT", classification: "SECRET", region: "PAK", event: "ISI-China joint operation planning meeting confirmed in Islamabad", confidence: 88 },
    { time: "16:43:56", priority: "FLASH", source: "CYBINT", classification: "TS", region: "IND", event: "State-sponsored APT targeting Indian critical infrastructure - Power grid vulnerabilities", confidence: 92 },
    { time: "16:42:31", priority: "IMMEDIATE", source: "ECONINT", classification: "SECRET", region: "CHN", event: "Major capital outflows from China $2.8B in 24hrs - Real estate sector collapse", confidence: 97 },
    { time: "16:41:08", priority: "URGENT", source: "OSINT", classification: "CONFIDENTIAL", region: "RUS", event: "Wagner Group remnants regrouping in Belarus - 2000+ personnel", confidence: 78 },
    { time: "16:39:45", priority: "FLASH", source: "GEOINT", classification: "TS", region: "NK", event: "Satellite imagery confirms new ICBM facility construction - 78% complete", confidence: 94 },
    { time: "16:38:22", priority: "ROUTINE", source: "HUMINT", classification: "SECRET", region: "IRN", event: "IRGC naval exercises in Strait of Hormuz - 12 vessels deployed", confidence: 85 },
    { time: "16:37:01", priority: "URGENT", source: "SIGINT", classification: "TS/SCI", region: "UKR", event: "Russian communications indicate winter offensive preparations", confidence: 91 }
  ];

  const economicIndicators = [
    { name: "VIX Fear Index", current: 18.45, change: -1.2, trend: "down", impact: "Medium", alert: false },
    { name: "DXY Dollar Index", current: 104.78, change: 0.3, trend: "up", impact: "High", alert: false },
    { name: "Gold Spot", current: 2047.80, change: 18.3, trend: "up", impact: "Medium", alert: true },
    { name: "Oil Brent", current: 82.45, change: 2.1, trend: "up", impact: "Critical", alert: true },
    { name: "Bitcoin", current: 67234, change: -1234, trend: "down", impact: "Low", alert: false },
    { name: "India Nifty 50", current: 24867, change: 234, trend: "up", impact: "High", alert: false },
    { name: "INR/USD", current: 83.14, change: -0.08, trend: "stable", impact: "Medium", alert: false },
    { name: "Yuan/USD", current: 7.234, change: 0.045, trend: "up", impact: "High", alert: true },
    { name: "Ruble/USD", current: 92.45, change: -2.34, trend: "down", impact: "Critical", alert: true },
    { name: "Baltic Dry Index", current: 1567, change: -89, trend: "down", impact: "Medium", alert: false }
  ];

  const tradeRouteRisk = [
    { route: "Strait of Hormuz", risk: 8.7, throughput: "21.0mbpd", threat: "Military", status: "Critical", india: "40% oil imports" },
    { route: "Strait of Malacca", risk: 6.8, throughput: "$3.8B/day", threat: "Piracy", status: "Medium", india: "Major trade route" },
    { route: "Suez Canal", risk: 7.2, throughput: "$10.2B/day", threat: "Regional", status: "High", india: "Europe trade 25%" },
    { route: "South China Sea", risk: 8.1, throughput: "$5.8B/day", threat: "Military", status: "Critical", india: "ASEAN trade 18%" },
    { route: "Bab el-Mandeb", risk: 7.8, throughput: "$2.4B/day", threat: "Terrorism", status: "High", india: "Energy route 15%" },
    { route: "IMEC Corridor", risk: 4.5, throughput: "$1.2B/day", threat: "Development", status: "Opportunity", india: "Alternative route" }
  ];

  const getRiskColor = (level: number) => {
    if (level >= 8.5) return RISK_CRITICAL;
    if (level >= 7.0) return RISK_HIGH;
    if (level >= 5.5) return RISK_MEDIUM;
    return RISK_LOW;
  };

  const getPriorityColor = (priority: string) => {
    switch (priority) {
      case "FLASH": return '#FF0000';
      case "URGENT": return '#FF6400';
      case "IMMEDIATE": return '#FFC800';
      case "ROUTINE": return '#00C800';
      default: return BLOOMBERG_GRAY;
    }
  };

  const createMatrixDisplay = () => (
    <div style={{
      backgroundColor: BLOOMBERG_PANEL_BG,
      border: `1px solid ${BLOOMBERG_GRAY}`,
      padding: '6px',
      height: '220px',
      overflow: 'auto'
    }}>
      <div style={{ color: BLOOMBERG_ORANGE, fontSize: '11px', fontWeight: 'bold', marginBottom: '6px' }}>
        CORRELATION MATRIX - GEOPOLITICAL RISK vs MARKETS
      </div>
      <div style={{
        display: 'grid',
        gridTemplateColumns: '80px repeat(5, 1fr)',
        gap: '2px',
        fontSize: '10px',
        marginBottom: '4px'
      }}>
        <div style={{ color: BLOOMBERG_WHITE, fontWeight: 'bold' }}>Asset</div>
        <div style={{ color: BLOOMBERG_RED, fontWeight: 'bold' }}>MIL</div>
        <div style={{ color: BLOOMBERG_YELLOW, fontWeight: 'bold' }}>ECO</div>
        <div style={{ color: BLOOMBERG_BLUE, fontWeight: 'bold' }}>CYB</div>
        <div style={{ color: BLOOMBERG_PURPLE, fontWeight: 'bold' }}>NUC</div>
        <div style={{ color: BLOOMBERG_CYAN, fontWeight: 'bold' }}>VOL</div>
      </div>
      {correlationMatrix.map((item, index) => (
        <div key={index} style={{
          display: 'grid',
          gridTemplateColumns: '80px repeat(5, 1fr)',
          gap: '2px',
          fontSize: '10px',
          marginBottom: '2px',
          backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.05)' : 'transparent'
        }}>
          <div style={{ color: BLOOMBERG_WHITE }}>{item.asset}</div>
          <div style={{ color: item.military > 0.8 ? RISK_CRITICAL : item.military > 0.6 ? RISK_HIGH : RISK_MEDIUM }}>
            {item.military.toFixed(2)}
          </div>
          <div style={{ color: item.economic > 0.8 ? RISK_CRITICAL : item.economic > 0.6 ? RISK_HIGH : RISK_MEDIUM }}>
            {item.economic.toFixed(2)}
          </div>
          <div style={{ color: item.cyber > 0.8 ? RISK_CRITICAL : item.cyber > 0.6 ? RISK_HIGH : RISK_MEDIUM }}>
            {item.cyber.toFixed(2)}
          </div>
          <div style={{ color: item.nuclear > 0.8 ? RISK_CRITICAL : item.nuclear > 0.6 ? RISK_HIGH : RISK_MEDIUM }}>
            {item.nuclear.toFixed(2)}
          </div>
          <div style={{ color: getRiskColor(item.volatility) }}>
            {item.volatility.toFixed(1)}
          </div>
        </div>
      ))}
    </div>
  );

  return (
    <div style={{
      height: '100%',
      backgroundColor: BLOOMBERG_DARK_BG,
      color: BLOOMBERG_WHITE,
      fontFamily: 'Consolas, monospace',
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column',
      fontSize: '12px'
    }}>
      {/* Complex Header with Multiple Status Lines */}
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
        padding: '4px 8px',
        flexShrink: 0
      }}>
        {/* Main Status Line */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '13px', marginBottom: '2px' }}>
          <span style={{ color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>GEOPOLITICAL INTELLIGENCE SYSTEM</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: RISK_HIGH, fontWeight: 'bold' }}>GLOBAL RISK: 7.34/10</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_RED }}>ACTIVE CONFLICTS: 15</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_YELLOW }}>SANCTIONS: 3,127 (+3)</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: alertCount > 125 ? BLOOMBERG_RED : BLOOMBERG_YELLOW }}>ALERTS: {alertCount}</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_GREEN, animation: 'blink 1s infinite' }}>● INTEL: LIVE</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_WHITE }}>{currentTime.toISOString().replace('T', ' ').substring(0, 19)} UTC</span>
        </div>

        {/* Secondary Status Line */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '11px', marginBottom: '2px' }}>
          <span style={{ color: BLOOMBERG_GRAY }}>FEEDS:</span>
          <span style={{ color: BLOOMBERG_GREEN }}>SIGINT●</span>
          <span style={{ color: BLOOMBERG_GREEN }}>HUMINT●</span>
          <span style={{ color: BLOOMBERG_GREEN }}>CYBINT●</span>
          <span style={{ color: BLOOMBERG_GREEN }}>ECONINT●</span>
          <span style={{ color: BLOOMBERG_YELLOW }}>GEOINT◐</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_GRAY }}>LATENCY:</span>
          <span style={{ color: BLOOMBERG_GREEN }}>89ms</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_GRAY }}>UPDATES:</span>
          <span style={{ color: BLOOMBERG_BLUE }}>{dataUpdateCount}</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_GRAY }}>CLASSIFICATION:</span>
          <span style={{ color: BLOOMBERG_RED }}>TOP SECRET//SCI</span>
        </div>

        {/* Market Ticker */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px', fontSize: '11px' }}>
          <span style={{ color: BLOOMBERG_GRAY }}>MARKETS:</span>
          <span style={{ color: BLOOMBERG_GREEN }}>NIFTY: 24,867 (+234)</span>
          <span style={{ color: BLOOMBERG_BLUE }}>INR: 83.14 (-0.08)</span>
          <span style={{ color: BLOOMBERG_GREEN }}>BRENT: $82.45 (+2.1)</span>
          <span style={{ color: BLOOMBERG_YELLOW }}>GOLD: $2,047 (+18.3)</span>
          <span style={{ color: BLOOMBERG_RED }}>VIX: 18.45 (-1.2)</span>
          <span style={{ color: BLOOMBERG_GREEN }}>DXY: 104.78 (+0.3)</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_GRAY }}>ENERGY:</span>
          <span style={{ color: BLOOMBERG_GREEN }}>+3.8%</span>
          <span style={{ color: BLOOMBERG_GRAY }}>DEFENSE:</span>
          <span style={{ color: BLOOMBERG_GREEN }}>+4.2%</span>
          <span style={{ color: BLOOMBERG_GRAY }}>TECH:</span>
          <span style={{ color: BLOOMBERG_RED }}>-2.1%</span>
        </div>
      </div>

      {/* Function Keys with Sub-categories */}
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
        padding: '3px 6px',
        flexShrink: 0
      }}>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(12, 1fr)', gap: '2px' }}>
          {[
            { key: "F1", label: "INDIA", status: "●" },
            { key: "F2", label: "CHINA", status: "●" },
            { key: "F3", label: "PAKISTAN", status: "◐" },
            { key: "F4", label: "USA", status: "●" },
            { key: "F5", label: "RUSSIA", status: "●" },
            { key: "F6", label: "LAC", status: "◐" },
            { key: "F7", label: "QUAD", status: "○" },
            { key: "F8", label: "BRICS", status: "○" },
            { key: "F9", label: "TRADE", status: "●" },
            { key: "F10", label: "DEFENSE", status: "●" },
            { key: "F11", label: "ENERGY", status: "●" },
            { key: "F12", label: "CYBER", status: "●" }
          ].map(item => (
            <button key={item.key} style={{
              backgroundColor: BLOOMBERG_DARK_BG,
              border: `1px solid ${BLOOMBERG_GRAY}`,
              color: BLOOMBERG_WHITE,
              padding: '2px 4px',
              fontSize: '12px',
              height: '18px',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center'
            }}>
              <span style={{ color: BLOOMBERG_YELLOW }}>{item.key}:</span>
              <span style={{ marginLeft: '2px' }}>{item.label}</span>
              <span style={{
                marginLeft: '2px',
                color: item.status === '●' ? BLOOMBERG_GREEN :
                       item.status === '◐' ? BLOOMBERG_YELLOW : BLOOMBERG_GRAY
              }}>
                {item.status}
              </span>
            </button>
          ))}
        </div>
      </div>

      {/* Dense Main Content */}
      <div style={{ flex: 1, overflow: 'auto', padding: '4px', backgroundColor: '#050505' }}>
        {/* Top Row - Intelligence Stream & Risk Assessment */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '4px', marginBottom: '4px', height: '180px' }}>
          {/* Real-time Intelligence Stream */}
          <div style={{
            backgroundColor: BLOOMBERG_PANEL_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            padding: '4px',
            overflow: 'auto'
          }}>
            <div style={{ color: BLOOMBERG_ORANGE, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
              REAL-TIME INTELLIGENCE STREAM [CLASSIFICATION: TS//SCI]
            </div>
            <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '4px' }}></div>

            {intelligenceStream.map((intel, index) => (
              <div key={index} style={{
                marginBottom: '4px',
                fontSize: '12px',
                borderLeft: `2px solid ${getPriorityColor(intel.priority)}`,
                paddingLeft: '4px'
              }}>
                <div style={{ display: 'flex', gap: '4px', marginBottom: '1px' }}>
                  <span style={{ color: BLOOMBERG_GRAY, minWidth: '50px' }}>{intel.time}</span>
                  <span style={{ color: getPriorityColor(intel.priority), fontWeight: 'bold', minWidth: '60px' }}>
                    [{intel.priority}]
                  </span>
                  <span style={{ color: BLOOMBERG_BLUE, minWidth: '50px' }}>[{intel.source}]</span>
                  <span style={{ color: BLOOMBERG_PURPLE, minWidth: '30px' }}>{intel.region}</span>
                  <span style={{ color: BLOOMBERG_YELLOW, minWidth: '30px' }}>{intel.confidence}%</span>
                </div>
                <div style={{ color: BLOOMBERG_WHITE, lineHeight: '1.1', marginBottom: '1px' }}>
                  {intel.event}
                </div>
                <div style={{ color: BLOOMBERG_GRAY, fontSize: '7px' }}>
                  Classification: {intel.classification}
                </div>
              </div>
            ))}
          </div>

          {/* Country Risk Matrix */}
          <div style={{
            backgroundColor: BLOOMBERG_PANEL_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            padding: '4px',
            overflow: 'auto'
          }}>
            <div style={{ color: BLOOMBERG_ORANGE, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
              COUNTRY RISK ASSESSMENT MATRIX
            </div>
            <div style={{
              display: 'grid',
              gridTemplateColumns: '50px repeat(6, 1fr)',
              gap: '2px',
              fontSize: '12px',
              marginBottom: '4px'
            }}>
              <div style={{ color: BLOOMBERG_WHITE, fontWeight: 'bold' }}>CTRY</div>
              <div style={{ color: BLOOMBERG_RED, fontWeight: 'bold' }}>MIL</div>
              <div style={{ color: BLOOMBERG_YELLOW, fontWeight: 'bold' }}>ECO</div>
              <div style={{ color: BLOOMBERG_BLUE, fontWeight: 'bold' }}>POL</div>
              <div style={{ color: BLOOMBERG_PURPLE, fontWeight: 'bold' }}>TOT</div>
              <div style={{ color: BLOOMBERG_CYAN, fontWeight: 'bold' }}>TRD</div>
              <div style={{ color: BLOOMBERG_GRAY, fontWeight: 'bold' }}>SAN</div>
            </div>
            {countryRiskData.map((country, index) => (
              <div key={index} style={{
                display: 'grid',
                gridTemplateColumns: '50px repeat(6, 1fr)',
                gap: '2px',
                fontSize: '11px',
                marginBottom: '1px',
                backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.03)' : 'transparent'
              }}>
                <div style={{ color: BLOOMBERG_WHITE, fontWeight: 'bold' }}>{country.country}</div>
                <div style={{ color: getRiskColor(country.military) }}>{country.military.toFixed(1)}</div>
                <div style={{ color: getRiskColor(country.economic) }}>{country.economic.toFixed(1)}</div>
                <div style={{ color: getRiskColor(country.political) }}>{country.political.toFixed(1)}</div>
                <div style={{ color: getRiskColor(country.total) }}>{country.total.toFixed(1)}</div>
                <div style={{
                  color: country.trend === 'up' ? BLOOMBERG_RED :
                         country.trend === 'down' ? BLOOMBERG_GREEN : BLOOMBERG_YELLOW
                }}>
                  {country.trend === 'up' ? '↑' : country.trend === 'down' ? '↓' : '→'}
                </div>
                <div style={{ color: country.sanctions > 500 ? BLOOMBERG_RED : country.sanctions > 50 ? BLOOMBERG_YELLOW : BLOOMBERG_GREEN }}>
                  {country.sanctions}
                </div>
              </div>
            ))}
          </div>

          {/* Economic Indicators Dashboard */}
          <div style={{
            backgroundColor: BLOOMBERG_PANEL_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            padding: '4px',
            overflow: 'auto'
          }}>
            <div style={{ color: BLOOMBERG_ORANGE, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
              ECONOMIC THREAT INDICATORS
            </div>
            <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '4px' }}></div>

            {economicIndicators.map((indicator, index) => (
              <div key={index} style={{
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center',
                marginBottom: '2px',
                fontSize: '12px',
                backgroundColor: indicator.alert ? 'rgba(255,0,0,0.1)' : 'transparent',
                padding: '1px 2px'
              }}>
                <div style={{ flex: 1 }}>
                  <span style={{ color: BLOOMBERG_WHITE, fontWeight: indicator.alert ? 'bold' : 'normal' }}>
                    {indicator.name}
                  </span>
                  {indicator.alert && <span style={{ color: BLOOMBERG_RED, marginLeft: '2px' }}>⚠</span>}
                </div>
                <div style={{
                  color: indicator.change > 0 ? BLOOMBERG_GREEN :
                         indicator.change < 0 ? BLOOMBERG_RED : BLOOMBERG_GRAY,
                  minWidth: '60px',
                  textAlign: 'right'
                }}>
                  {indicator.current.toLocaleString()}
                </div>
                <div style={{
                  color: indicator.change > 0 ? BLOOMBERG_GREEN :
                         indicator.change < 0 ? BLOOMBERG_RED : BLOOMBERG_GRAY,
                  minWidth: '40px',
                  textAlign: 'right'
                }}>
                  {indicator.change > 0 ? '+' : ''}{indicator.change}
                </div>
                <div style={{
                  color: indicator.impact === 'Critical' ? BLOOMBERG_RED :
                         indicator.impact === 'High' ? BLOOMBERG_ORANGE :
                         indicator.impact === 'Medium' ? BLOOMBERG_YELLOW : BLOOMBERG_GREEN,
                  minWidth: '20px',
                  textAlign: 'center',
                  fontSize: '11px'
                }}>
                  {indicator.impact.charAt(0)}
                </div>
              </div>
            ))}
          </div>
        </div>

        {/* Middle Row - Charts Section */}
        <div style={{ display: 'grid', gridTemplateColumns: '2fr 1fr 1fr', gap: '4px', marginBottom: '4px', height: '240px' }}>
          {/* Complex Risk Forecast Chart */}
          <div style={{
            backgroundColor: BLOOMBERG_PANEL_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            padding: '4px'
          }}>
            <div style={{ color: BLOOMBERG_ORANGE, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
              MULTI-VECTOR RISK FORECAST - 90 DAY PREDICTIVE MODEL
            </div>
            <ResponsiveContainer width="100%" height={200}>
              <LineChart data={riskForecastData.slice(0, 30)}>
                <CartesianGrid strokeDasharray="1 1" stroke={BLOOMBERG_GRAY} opacity={0.3} />
                <XAxis dataKey="day" stroke={BLOOMBERG_WHITE} fontSize={10} />
                <YAxis stroke={BLOOMBERG_WHITE} fontSize={10} domain={[5, 10]} />
                <Tooltip
                  contentStyle={{
                    backgroundColor: BLOOMBERG_DARK_BG,
                    border: `1px solid ${BLOOMBERG_GRAY}`,
                    color: BLOOMBERG_WHITE,
                    fontSize: '10px'
                  }}
                />
                <Legend wrapperStyle={{ fontSize: '10px' }} />
                <Line type="monotone" dataKey="baseRisk" stroke={BLOOMBERG_GREEN} strokeWidth={1} name="Base Risk" dot={false} />
                <Line type="monotone" dataKey="militaryRisk" stroke={BLOOMBERG_RED} strokeWidth={1} name="Military" dot={false} />
                <Line type="monotone" dataKey="economicRisk" stroke={BLOOMBERG_YELLOW} strokeWidth={1} name="Economic" dot={false} />
                <Line type="monotone" dataKey="cyberRisk" stroke={BLOOMBERG_BLUE} strokeWidth={1} name="Cyber" dot={false} />
                <Line type="monotone" dataKey="nuclearRisk" stroke={BLOOMBERG_PURPLE} strokeWidth={1} name="Nuclear" dot={false} />
              </LineChart>
            </ResponsiveContainer>
          </div>

          {/* Threat Radar Chart */}
          <div style={{
            backgroundColor: BLOOMBERG_PANEL_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            padding: '4px'
          }}>
            <div style={{ color: BLOOMBERG_ORANGE, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
              THREAT RADAR
            </div>
            <ResponsiveContainer width="100%" height={200}>
              <RadarChart data={threatRadarData}>
                <PolarGrid stroke={BLOOMBERG_GRAY} />
                <PolarAngleAxis dataKey="threat" tick={{ fontSize: 10, fill: BLOOMBERG_WHITE }} />
                <PolarRadiusAxis angle={90} domain={[0, 10]} tick={{ fontSize: 9, fill: BLOOMBERG_GRAY }} />
                <Radar
                  name="Current Threat"
                  dataKey="current"
                  stroke={BLOOMBERG_RED}
                  fill={BLOOMBERG_RED}
                  fillOpacity={0.3}
                  strokeWidth={2}
                />
              </RadarChart>
            </ResponsiveContainer>
          </div>

          {/* Correlation Matrix Display */}
          {createMatrixDisplay()}
        </div>

        {/* Bottom Row - Market Impact & Trade Routes */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '4px', height: '200px' }}>
          {/* Market Impact Analysis */}
          <div style={{
            backgroundColor: BLOOMBERG_PANEL_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            padding: '4px',
            overflow: 'auto'
          }}>
            <div style={{ color: BLOOMBERG_ORANGE, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
              MARKET IMPACT ANALYSIS
            </div>
            <div style={{
              display: 'grid',
              gridTemplateColumns: '60px 40px 40px 50px 40px',
              gap: '2px',
              fontSize: '12px',
              marginBottom: '4px'
            }}>
              <div style={{ color: BLOOMBERG_WHITE, fontWeight: 'bold' }}>SECTOR</div>
              <div style={{ color: BLOOMBERG_BLUE, fontWeight: 'bold' }}>CORR</div>
              <div style={{ color: BLOOMBERG_GREEN, fontWeight: 'bold' }}>IMPL</div>
              <div style={{ color: BLOOMBERG_YELLOW, fontWeight: 'bold' }}>VOLUME</div>
              <div style={{ color: BLOOMBERG_PURPLE, fontWeight: 'bold' }}>CHG</div>
            </div>
            {marketImpactData.map((market, index) => (
              <div key={index} style={{
                display: 'grid',
                gridTemplateColumns: '60px 40px 40px 50px 40px',
                gap: '2px',
                fontSize: '11px',
                marginBottom: '1px',
                backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.03)' : 'transparent'
              }}>
                <div style={{ color: BLOOMBERG_WHITE }}>{market.sector}</div>
                <div style={{ color: market.correlation > 0.8 ? BLOOMBERG_RED : market.correlation > 0.6 ? BLOOMBERG_YELLOW : BLOOMBERG_GREEN }}>
                  {market.correlation.toFixed(2)}
                </div>
                <div style={{ color: market.impact > 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED }}>
                  {market.impact > 0 ? '+' : ''}{market.impact.toFixed(1)}
                </div>
                <div style={{ color: BLOOMBERG_GRAY }}>{market.volume}</div>
                <div style={{ color: market.change.startsWith('+') ? BLOOMBERG_GREEN : BLOOMBERG_RED }}>
                  {market.change}
                </div>
              </div>
            ))}
          </div>

          {/* Critical Trade Routes */}
          <div style={{
            backgroundColor: BLOOMBERG_PANEL_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            padding: '4px',
            overflow: 'auto'
          }}>
            <div style={{ color: BLOOMBERG_ORANGE, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
              CRITICAL TRADE ROUTE RISK
            </div>
            <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '4px' }}></div>

            {tradeRouteRisk.map((route, index) => (
              <div key={index} style={{ marginBottom: '6px', fontSize: '8px' }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                  <span style={{ color: BLOOMBERG_WHITE, fontWeight: 'bold', fontSize: '9px' }}>
                    {route.route}
                  </span>
                  <span style={{ color: getRiskColor(route.risk) }}>
                    {route.risk.toFixed(1)}
                  </span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>Throughput:</span>
                  <span style={{ color: BLOOMBERG_BLUE }}>{route.throughput}</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>Threat:</span>
                  <span style={{ color: BLOOMBERG_YELLOW }}>{route.threat}</span>
                </div>
                <div style={{ color: BLOOMBERG_CYAN, fontSize: '7px', lineHeight: '1.1' }}>
                  India: {route.india}
                </div>
              </div>
            ))}
          </div>

          {/* India Strategic Dashboard */}
          <div style={{
            backgroundColor: BLOOMBERG_PANEL_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            padding: '4px',
            overflow: 'auto'
          }}>
            <div style={{ color: BLOOMBERG_ORANGE, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
              INDIA STRATEGIC POSITION ANALYSIS
            </div>
            <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '4px' }}></div>

            <div style={{ marginBottom: '6px' }}>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '9px', fontWeight: 'bold', marginBottom: '2px' }}>
                KEY METRICS
              </div>
              <div style={{ fontSize: '8px', lineHeight: '1.2' }}>
                <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>GDP Growth:</span>
                  <span style={{ color: BLOOMBERG_GREEN }}>6.7% YoY</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>FDI Inflows:</span>
                  <span style={{ color: BLOOMBERG_GREEN }}>$83.6B</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>Defense Budget:</span>
                  <span style={{ color: BLOOMBERG_BLUE }}>$75.6B</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>Border Infra:</span>
                  <span style={{ color: BLOOMBERG_YELLOW }}>67% Complete</span>
                </div>
              </div>
            </div>

            <div style={{ marginBottom: '6px' }}>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '9px', fontWeight: 'bold', marginBottom: '2px' }}>
                THREAT MATRIX
              </div>
              <div style={{ fontSize: '7px' }}>
                {[
                  { threat: "LAC Tensions", level: 7.8, status: "ACTIVE" },
                  { threat: "Pakistan Terror", level: 6.9, status: "MEDIUM" },
                  { threat: "Cyber Warfare", level: 6.5, status: "HIGH" },
                  { threat: "Energy Security", level: 5.8, status: "WATCH" },
                  { threat: "Trade Disruption", level: 5.2, status: "LOW" }
                ].map((item, index) => (
                  <div key={index} style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                    <span style={{ color: BLOOMBERG_WHITE }}>{item.threat}:</span>
                    <span style={{ color: getRiskColor(item.level) }}>{item.level} [{item.status}]</span>
                  </div>
                ))}
              </div>
            </div>

            <div>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '9px', fontWeight: 'bold', marginBottom: '2px' }}>
                STRATEGIC INITIATIVES
              </div>
              <div style={{ fontSize: '7px', color: BLOOMBERG_GREEN }}>
                <div>✓ Quad Alliance Operationalization</div>
                <div>✓ IMEC Corridor Development</div>
                <div>✓ Semiconductor Self-Reliance</div>
                <div>✓ Digital India 2.0 Rollout</div>
                <div>✓ Defense Export Push</div>
              </div>
            </div>
          </div>
        </div>
      </div>

      {/* Enhanced Status Bar */}
      <div style={{
        borderTop: `1px solid ${BLOOMBERG_GRAY}`,
        backgroundColor: BLOOMBERG_PANEL_BG,
        padding: '2px 8px',
        fontSize: '10px',
        color: BLOOMBERG_GRAY,
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <div style={{ display: 'flex', gap: '16px' }}>
            <span>Geopolitical Intelligence System v3.2.1 | Real-time threat analysis</span>
            <span>Sources: 47 Active | Confidence: 94.2% | Processing: 2.1M events/min</span>
          </div>
          <div style={{ display: 'flex', gap: '16px' }}>
            <span>Classification: TOP SECRET//SCI//NOFORN</span>
            <span>Session: {currentTime.toTimeString().substring(0, 8)}</span>
            <span style={{ color: BLOOMBERG_GREEN }}>SCIF: AUTHORIZED</span>
          </div>
        </div>
      </div>
    </div>
  );
};

export default GeopoliticsTab;