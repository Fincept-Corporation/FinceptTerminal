import React, { useState, useEffect } from 'react';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer, BarChart, Bar, PieChart, Pie, Cell, AreaChart, Area, ScatterChart, Scatter, RadarChart, PolarGrid, PolarAngleAxis, PolarRadiusAxis, Radar, ComposedChart, ReferenceLine, Treemap, FunnelChart, Funnel, LabelList } from 'recharts';

const AnalyticsTab: React.FC = () => {
  const [currentTime, setCurrentTime] = useState(new Date());
  const [dataUpdateCount, setDataUpdateCount] = useState(0);
  const [alertCount, setAlertCount] = useState(23);

  // Bloomberg color scheme (exact from DearPyGUI)
  const BLOOMBERG_ORANGE = '#FFA500';
  const BLOOMBERG_WHITE = '#FFFFFF';
  const BLOOMBERG_RED = '#FF0000';
  const BLOOMBERG_GREEN = '#00C800';
  const BLOOMBERG_YELLOW = '#FFFF00';
  const BLOOMBERG_GRAY = '#787878';
  const BLOOMBERG_BLUE = '#6496FA';
  const BLOOMBERG_PURPLE = '#C864FF';
  const BLOOMBERG_CYAN = '#00FFFF';
  const BLOOMBERG_DARK_BG = '#000000';
  const BLOOMBERG_PANEL_BG = '#0a0a0a';

  useEffect(() => {
    const timer = setInterval(() => {
      setCurrentTime(new Date());
      setDataUpdateCount(prev => prev + 1);
      if (Math.random() > 0.8) setAlertCount(prev => prev + 1);
    }, 2000);
    return () => clearInterval(timer);
  }, []);

  // Generate various chart data
  const priceData = Array.from({ length: 30 }, (_, i) => ({
    time: i + 1,
    price: 150 + Math.sin(i * 0.1) * 20 + Math.random() * 10,
    volume: Math.random() * 1000000 + 500000,
    high: 150 + Math.sin(i * 0.1) * 20 + Math.random() * 15 + 5,
    low: 150 + Math.sin(i * 0.1) * 20 + Math.random() * 15 - 5,
    volatility: Math.random() * 5 + 2
  }));

  const sectorData = [
    { sector: 'Technology', value: 28.5, growth: 12.4, pe: 24.8 },
    { sector: 'Healthcare', value: 15.2, growth: 8.7, pe: 18.9 },
    { sector: 'Finance', value: 18.9, growth: 6.3, pe: 14.2 },
    { sector: 'Energy', value: 12.1, growth: 15.8, pe: 12.5 },
    { sector: 'Consumer', value: 14.7, growth: 4.2, pe: 22.1 },
    { sector: 'Industrial', value: 10.6, growth: 7.9, pe: 16.7 }
  ];

  const correlationData = Array.from({ length: 20 }, (_, i) => ({
    x: Math.random() * 10,
    y: Math.random() * 10,
    size: Math.random() * 100 + 20
  }));

  const portfolioData = [
    { asset: 'Stocks', allocation: 65, returns: 14.2, risk: 18.5 },
    { asset: 'Bonds', allocation: 25, returns: 4.8, risk: 6.2 },
    { asset: 'Commodities', allocation: 7, returns: 8.9, risk: 22.1 },
    { asset: 'Cash', allocation: 3, returns: 2.1, risk: 0.5 }
  ];

  const riskMetrics = Array.from({ length: 90 }, (_, i) => ({
    day: i + 1,
    var: Math.random() * 5 + 2,
    sharpe: Math.random() * 2 + 0.5,
    beta: Math.random() * 1.5 + 0.5,
    alpha: Math.random() * 4 - 2,
    drawdown: Math.random() * 15
  }));

  const momentumData = Array.from({ length: 50 }, (_, i) => ({
    stock: i + 1,
    momentum: Math.random() * 100 - 50,
    rsi: Math.random() * 100,
    macd: Math.random() * 10 - 5
  }));

  const liquidityData = Array.from({ length: 24 }, (_, i) => ({
    hour: i,
    volume: Math.random() * 1000000 + 200000,
    spread: Math.random() * 0.5 + 0.1,
    depth: Math.random() * 50000 + 10000
  }));

  const technicalData = Array.from({ length: 30 }, (_, i) => ({
    period: i + 1,
    price: 100 + Math.sin(i * 0.2) * 15 + Math.random() * 5,
    sma20: 100 + Math.sin((i - 2) * 0.2) * 12,
    ema12: 100 + Math.sin((i - 1) * 0.2) * 13,
    bb_upper: 100 + Math.sin(i * 0.2) * 15 + 10,
    bb_lower: 100 + Math.sin(i * 0.2) * 15 - 10
  }));

  const fundamentalData = [
    { metric: 'P/E Ratio', value: 22.8, benchmark: 18.5 },
    { metric: 'EV/EBITDA', value: 14.2, benchmark: 12.1 },
    { metric: 'P/B Ratio', value: 3.4, benchmark: 2.8 },
    { metric: 'ROE', value: 18.7, benchmark: 15.2 },
    { metric: 'ROA', value: 8.9, benchmark: 7.1 },
    { metric: 'Debt/Equity', value: 0.65, benchmark: 0.8 }
  ];

  const optionsData = Array.from({ length: 10 }, (_, i) => ({
    strike: 140 + i * 5,
    calls: Math.random() * 1000 + 100,
    puts: Math.random() * 800 + 50,
    iv: Math.random() * 40 + 20,
    delta: 1 - (i / 10),
    gamma: Math.random() * 0.1,
    theta: -Math.random() * 0.5
  }));

  const volatilityData = Array.from({ length: 252 }, (_, i) => ({
    day: i + 1,
    realized: Math.random() * 40 + 10,
    implied: Math.random() * 45 + 15,
    historical: Math.random() * 35 + 12
  }));

  const economicData = [
    { indicator: 'GDP Growth', current: 6.7, forecast: 6.4, impact: 'High' },
    { indicator: 'Inflation', current: 4.2, forecast: 3.8, impact: 'Critical' },
    { indicator: 'Unemployment', current: 3.8, forecast: 4.1, impact: 'Medium' },
    { indicator: 'Interest Rate', current: 5.25, forecast: 5.5, impact: 'Critical' },
    { indicator: 'Oil Price', current: 82.45, forecast: 85.2, impact: 'High' }
  ];

  const sentimentData = Array.from({ length: 30 }, (_, i) => ({
    date: i + 1,
    bullish: Math.random() * 100,
    bearish: Math.random() * 100,
    neutral: Math.random() * 100,
    fear: Math.random() * 100,
    greed: Math.random() * 100
  }));

  const flowsData = Array.from({ length: 20 }, (_, i) => ({
    date: i + 1,
    inflows: Math.random() * 2000 - 1000,
    outflows: Math.random() * 1500 - 750,
    net: Math.random() * 1000 - 500
  }));

  const performanceData = [
    { period: '1D', return: 1.2, benchmark: 0.8 },
    { period: '1W', return: 3.4, benchmark: 2.1 },
    { period: '1M', return: 8.7, benchmark: 6.2 },
    { period: '3M', return: 15.2, benchmark: 12.8 },
    { period: '6M', return: 24.6, benchmark: 18.9 },
    { period: '1Y', return: 34.8, benchmark: 28.4 }
  ];

  const attribution = [
    { factor: 'Stock Selection', contribution: 4.8 },
    { factor: 'Sector Allocation', contribution: 2.3 },
    { factor: 'Market Timing', contribution: -1.2 },
    { factor: 'Currency', contribution: 0.8 },
    { factor: 'Other', contribution: 1.4 }
  ];

  const moneyFlowData = Array.from({ length: 30 }, (_, i) => ({
    day: i + 1,
    mfi: Math.random() * 100,
    obv: Math.random() * 1000000,
    adl: Math.random() * 50000 - 25000,
    cmf: Math.random() * 0.4 - 0.2
  }));

  const marketData = [
    { market: 'US', weight: 55.2, return: 12.4, volatility: 16.8 },
    { market: 'Europe', weight: 18.7, return: 8.9, volatility: 19.2 },
    { market: 'Asia', weight: 15.8, return: 15.6, volatility: 22.1 },
    { market: 'Emerging', weight: 10.3, return: 18.2, volatility: 28.4 }
  ];

  const bondData = Array.from({ length: 30 }, (_, i) => ({
    maturity: i / 2 + 0.25,
    yield: 3.5 + (i * 0.1) + Math.random() * 0.5,
    duration: i / 3 + 1,
    spread: Math.random() * 200 + 50
  }));

  const derivativesData = Array.from({ length: 15 }, (_, i) => ({
    type: ['Futures', 'Options', 'Swaps', 'Forwards'][i % 4],
    notional: Math.random() * 10000000 + 1000000,
    pnl: Math.random() * 100000 - 50000,
    risk: Math.random() * 50000 + 10000
  }));

  const stressData = [
    { scenario: 'Base Case', probability: 60, impact: 0 },
    { scenario: 'Recession', probability: 25, impact: -25 },
    { scenario: 'Market Crash', probability: 10, impact: -45 },
    { scenario: 'Black Swan', probability: 5, impact: -65 }
  ];

  // Chart component wrapper with Bloomberg styling
  const ChartBox = ({ title, children, width = "100%", height = "160px" }: {
    title: string,
    children: React.ReactNode,
    width?: string,
    height?: string
  }) => (
    <div style={{
      backgroundColor: BLOOMBERG_PANEL_BG,
      border: `1px solid ${BLOOMBERG_GRAY}`,
      padding: '4px',
      height,
      width,
      overflow: 'hidden'
    }}>
      <div style={{
        color: BLOOMBERG_ORANGE,
        fontSize: '10px',
        fontWeight: 'bold',
        marginBottom: '2px',
        textAlign: 'center',
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
        paddingBottom: '2px'
      }}>
        {title}
      </div>
      <div style={{ height: 'calc(100% - 20px)' }}>
        {children}
      </div>
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
      {/* Header */}
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
        padding: '4px 8px',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '13px', marginBottom: '2px' }}>
          <span style={{ color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>FINANCIAL ANALYTICS DASHBOARD</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_GREEN, fontWeight: 'bold' }}>PORTFOLIO VALUE: $2.45M</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_BLUE }}>P&L TODAY: +$12,456 (+0.51%)</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_YELLOW }}>ALERTS: {alertCount}</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_GREEN }}>● LIVE DATA</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_WHITE }}>{currentTime.toISOString().replace('T', ' ').substring(0, 19)} UTC</span>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '11px' }}>
          <span style={{ color: BLOOMBERG_GRAY }}>ANALYTICS:</span>
          <span style={{ color: BLOOMBERG_GREEN }}>RISK●</span>
          <span style={{ color: BLOOMBERG_GREEN }}>PERFORMANCE●</span>
          <span style={{ color: BLOOMBERG_GREEN }}>ATTRIBUTION●</span>
          <span style={{ color: BLOOMBERG_GREEN }}>TECHNICAL●</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_GRAY }}>MODELS:</span>
          <span style={{ color: BLOOMBERG_BLUE }}>VaR: $45K</span>
          <span style={{ color: BLOOMBERG_YELLOW }}>Sharpe: 1.84</span>
          <span style={{ color: BLOOMBERG_PURPLE }}>Beta: 0.97</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_GRAY }}>UPDATES:</span>
          <span style={{ color: BLOOMBERG_CYAN }}>{dataUpdateCount}</span>
        </div>
      </div>

      {/* Function Keys Bar */}
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
        padding: '2px 4px',
        flexShrink: 0
      }}>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(10, 1fr)', gap: '2px' }}>
          {[
            { key: "F1", label: "RISK", status: "●" },
            { key: "F2", label: "PERF", status: "●" },
            { key: "F3", label: "TECH", status: "●" },
            { key: "F4", label: "FUND", status: "●" },
            { key: "F5", label: "OPTIONS", status: "●" },
            { key: "F6", label: "FLOWS", status: "●" },
            { key: "F7", label: "STRESS", status: "◐" },
            { key: "F8", label: "DERIV", status: "●" },
            { key: "F9", label: "MACRO", status: "●" },
            { key: "F10", label: "ALERT", status: "●" }
          ].map(item => (
            <button key={item.key} style={{
              backgroundColor: BLOOMBERG_DARK_BG,
              border: `1px solid ${BLOOMBERG_GRAY}`,
              color: BLOOMBERG_WHITE,
              padding: '2px 4px',
              fontSize: '9px',
              height: '16px',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center'
            }}>
              <span style={{ color: BLOOMBERG_YELLOW }}>{item.key}:</span>
              <span style={{ marginLeft: '2px' }}>{item.label}</span>
              <span style={{
                marginLeft: '2px',
                color: item.status === '●' ? BLOOMBERG_GREEN : BLOOMBERG_YELLOW
              }}>
                {item.status}
              </span>
            </button>
          ))}
        </div>
      </div>

      {/* Main Content - 4x5 Grid (20 charts) */}
      <div style={{ flex: 1, overflow: 'auto', padding: '2px', backgroundColor: '#050505' }}>
        <div style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(5, 1fr)',
          gridTemplateRows: 'repeat(4, 1fr)',
          gap: '3px',
          height: '100%'
        }}>
          {/* Row 1 */}
          <ChartBox title="PRICE & VOLUME">
            <ResponsiveContainer width="100%" height="100%">
              <ComposedChart data={priceData.slice(0, 15)}>
                <CartesianGrid strokeDasharray="1 1" stroke={BLOOMBERG_GRAY} opacity={0.3} />
                <XAxis dataKey="time" stroke={BLOOMBERG_WHITE} fontSize={8} />
                <YAxis stroke={BLOOMBERG_WHITE} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: BLOOMBERG_DARK_BG, border: `1px solid ${BLOOMBERG_GRAY}`, fontSize: '9px' }} />
                <Line type="monotone" dataKey="price" stroke={BLOOMBERG_BLUE} strokeWidth={1} dot={false} />
                <Bar dataKey="volume" fill={BLOOMBERG_GRAY} opacity={0.3} />
              </ComposedChart>
            </ResponsiveContainer>
          </ChartBox>

          <ChartBox title="SECTOR ALLOCATION">
            <ResponsiveContainer width="100%" height="100%">
              <PieChart>
                <Pie
                  data={sectorData}
                  dataKey="value"
                  nameKey="sector"
                  cx="50%"
                  cy="50%"
                  outerRadius={50}
                  fill={BLOOMBERG_BLUE}
                >
                  {sectorData.map((entry, index) => (
                    <Cell key={`cell-${index}`} fill={[BLOOMBERG_BLUE, BLOOMBERG_GREEN, BLOOMBERG_YELLOW, BLOOMBERG_RED, BLOOMBERG_PURPLE, BLOOMBERG_CYAN][index % 6]} />
                  ))}
                </Pie>
                <Tooltip contentStyle={{ backgroundColor: BLOOMBERG_DARK_BG, border: `1px solid ${BLOOMBERG_GRAY}`, fontSize: '9px' }} />
              </PieChart>
            </ResponsiveContainer>
          </ChartBox>

          <ChartBox title="CORRELATION SCATTER">
            <ResponsiveContainer width="100%" height="100%">
              <ScatterChart data={correlationData}>
                <CartesianGrid strokeDasharray="1 1" stroke={BLOOMBERG_GRAY} opacity={0.3} />
                <XAxis stroke={BLOOMBERG_WHITE} fontSize={8} />
                <YAxis stroke={BLOOMBERG_WHITE} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: BLOOMBERG_DARK_BG, border: `1px solid ${BLOOMBERG_GRAY}`, fontSize: '9px' }} />
                <Scatter dataKey="y" fill={BLOOMBERG_GREEN} />
              </ScatterChart>
            </ResponsiveContainer>
          </ChartBox>

          <ChartBox title="PORTFOLIO COMPOSITION">
            <ResponsiveContainer width="100%" height="100%">
              <BarChart data={portfolioData}>
                <CartesianGrid strokeDasharray="1 1" stroke={BLOOMBERG_GRAY} opacity={0.3} />
                <XAxis dataKey="asset" stroke={BLOOMBERG_WHITE} fontSize={8} />
                <YAxis stroke={BLOOMBERG_WHITE} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: BLOOMBERG_DARK_BG, border: `1px solid ${BLOOMBERG_GRAY}`, fontSize: '9px' }} />
                <Bar dataKey="allocation" fill={BLOOMBERG_ORANGE} />
              </BarChart>
            </ResponsiveContainer>
          </ChartBox>

          <ChartBox title="RISK METRICS">
            <ResponsiveContainer width="100%" height="100%">
              <LineChart data={riskMetrics.slice(0, 20)}>
                <CartesianGrid strokeDasharray="1 1" stroke={BLOOMBERG_GRAY} opacity={0.3} />
                <XAxis dataKey="day" stroke={BLOOMBERG_WHITE} fontSize={8} />
                <YAxis stroke={BLOOMBERG_WHITE} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: BLOOMBERG_DARK_BG, border: `1px solid ${BLOOMBERG_GRAY}`, fontSize: '9px' }} />
                <Line type="monotone" dataKey="var" stroke={BLOOMBERG_RED} strokeWidth={1} dot={false} />
                <Line type="monotone" dataKey="sharpe" stroke={BLOOMBERG_GREEN} strokeWidth={1} dot={false} />
              </LineChart>
            </ResponsiveContainer>
          </ChartBox>

          {/* Row 2 */}
          <ChartBox title="MOMENTUM ANALYSIS">
            <ResponsiveContainer width="100%" height="100%">
              <BarChart data={momentumData.slice(0, 10)}>
                <CartesianGrid strokeDasharray="1 1" stroke={BLOOMBERG_GRAY} opacity={0.3} />
                <XAxis dataKey="stock" stroke={BLOOMBERG_WHITE} fontSize={8} />
                <YAxis stroke={BLOOMBERG_WHITE} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: BLOOMBERG_DARK_BG, border: `1px solid ${BLOOMBERG_GRAY}`, fontSize: '9px' }} />
                <Bar dataKey="momentum" fill={(entry) => entry.momentum > 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED} />
              </BarChart>
            </ResponsiveContainer>
          </ChartBox>

          <ChartBox title="INTRADAY LIQUIDITY">
            <ResponsiveContainer width="100%" height="100%">
              <AreaChart data={liquidityData}>
                <CartesianGrid strokeDasharray="1 1" stroke={BLOOMBERG_GRAY} opacity={0.3} />
                <XAxis dataKey="hour" stroke={BLOOMBERG_WHITE} fontSize={8} />
                <YAxis stroke={BLOOMBERG_WHITE} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: BLOOMBERG_DARK_BG, border: `1px solid ${BLOOMBERG_GRAY}`, fontSize: '9px' }} />
                <Area type="monotone" dataKey="volume" stroke={BLOOMBERG_CYAN} fill={BLOOMBERG_CYAN} fillOpacity={0.3} />
              </AreaChart>
            </ResponsiveContainer>
          </ChartBox>

          <ChartBox title="TECHNICAL INDICATORS">
            <ResponsiveContainer width="100%" height="100%">
              <LineChart data={technicalData}>
                <CartesianGrid strokeDasharray="1 1" stroke={BLOOMBERG_GRAY} opacity={0.3} />
                <XAxis dataKey="period" stroke={BLOOMBERG_WHITE} fontSize={8} />
                <YAxis stroke={BLOOMBERG_WHITE} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: BLOOMBERG_DARK_BG, border: `1px solid ${BLOOMBERG_GRAY}`, fontSize: '9px' }} />
                <Line type="monotone" dataKey="price" stroke={BLOOMBERG_WHITE} strokeWidth={2} dot={false} />
                <Line type="monotone" dataKey="sma20" stroke={BLOOMBERG_YELLOW} strokeWidth={1} dot={false} />
                <Line type="monotone" dataKey="ema12" stroke={BLOOMBERG_GREEN} strokeWidth={1} dot={false} />
              </LineChart>
            </ResponsiveContainer>
          </ChartBox>

          <ChartBox title="FUNDAMENTAL RATIOS">
            <ResponsiveContainer width="100%" height="100%">
              <BarChart data={fundamentalData} layout="horizontal">
                <CartesianGrid strokeDasharray="1 1" stroke={BLOOMBERG_GRAY} opacity={0.3} />
                <XAxis type="number" stroke={BLOOMBERG_WHITE} fontSize={8} />
                <YAxis type="category" dataKey="metric" stroke={BLOOMBERG_WHITE} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: BLOOMBERG_DARK_BG, border: `1px solid ${BLOOMBERG_GRAY}`, fontSize: '9px' }} />
                <Bar dataKey="value" fill={BLOOMBERG_BLUE} />
                <Bar dataKey="benchmark" fill={BLOOMBERG_GRAY} opacity={0.5} />
              </BarChart>
            </ResponsiveContainer>
          </ChartBox>

          <ChartBox title="OPTIONS FLOW">
            <ResponsiveContainer width="100%" height="100%">
              <ComposedChart data={optionsData}>
                <CartesianGrid strokeDasharray="1 1" stroke={BLOOMBERG_GRAY} opacity={0.3} />
                <XAxis dataKey="strike" stroke={BLOOMBERG_WHITE} fontSize={8} />
                <YAxis stroke={BLOOMBERG_WHITE} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: BLOOMBERG_DARK_BG, border: `1px solid ${BLOOMBERG_GRAY}`, fontSize: '9px' }} />
                <Bar dataKey="calls" fill={BLOOMBERG_GREEN} />
                <Bar dataKey="puts" fill={BLOOMBERG_RED} />
                <Line type="monotone" dataKey="iv" stroke={BLOOMBERG_YELLOW} strokeWidth={1} />
              </ComposedChart>
            </ResponsiveContainer>
          </ChartBox>

          {/* Row 3 */}
          <ChartBox title="VOLATILITY SURFACE">
            <ResponsiveContainer width="100%" height="100%">
              <AreaChart data={volatilityData.slice(0, 30)}>
                <CartesianGrid strokeDasharray="1 1" stroke={BLOOMBERG_GRAY} opacity={0.3} />
                <XAxis dataKey="day" stroke={BLOOMBERG_WHITE} fontSize={8} />
                <YAxis stroke={BLOOMBERG_WHITE} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: BLOOMBERG_DARK_BG, border: `1px solid ${BLOOMBERG_GRAY}`, fontSize: '9px' }} />
                <Area type="monotone" dataKey="realized" stackId="1" stroke={BLOOMBERG_GREEN} fill={BLOOMBERG_GREEN} fillOpacity={0.6} />
                <Area type="monotone" dataKey="implied" stackId="1" stroke={BLOOMBERG_YELLOW} fill={BLOOMBERG_YELLOW} fillOpacity={0.6} />
              </AreaChart>
            </ResponsiveContainer>
          </ChartBox>

          <ChartBox title="ECONOMIC INDICATORS">
            <ResponsiveContainer width="100%" height="100%">
              <BarChart data={economicData}>
                <CartesianGrid strokeDasharray="1 1" stroke={BLOOMBERG_GRAY} opacity={0.3} />
                <XAxis dataKey="indicator" stroke={BLOOMBERG_WHITE} fontSize={7} angle={-45} />
                <YAxis stroke={BLOOMBERG_WHITE} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: BLOOMBERG_DARK_BG, border: `1px solid ${BLOOMBERG_GRAY}`, fontSize: '9px' }} />
                <Bar dataKey="current" fill={BLOOMBERG_BLUE} />
                <Bar dataKey="forecast" fill={BLOOMBERG_ORANGE} />
              </BarChart>
            </ResponsiveContainer>
          </ChartBox>

          <ChartBox title="MARKET SENTIMENT">
            <ResponsiveContainer width="100%" height="100%">
              <AreaChart data={sentimentData.slice(0, 15)}>
                <CartesianGrid strokeDasharray="1 1" stroke={BLOOMBERG_GRAY} opacity={0.3} />
                <XAxis dataKey="date" stroke={BLOOMBERG_WHITE} fontSize={8} />
                <YAxis stroke={BLOOMBERG_WHITE} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: BLOOMBERG_DARK_BG, border: `1px solid ${BLOOMBERG_GRAY}`, fontSize: '9px' }} />
                <Area type="monotone" dataKey="bullish" stackId="1" stroke={BLOOMBERG_GREEN} fill={BLOOMBERG_GREEN} fillOpacity={0.4} />
                <Area type="monotone" dataKey="bearish" stackId="1" stroke={BLOOMBERG_RED} fill={BLOOMBERG_RED} fillOpacity={0.4} />
              </AreaChart>
            </ResponsiveContainer>
          </ChartBox>

          <ChartBox title="FUND FLOWS">
            <ResponsiveContainer width="100%" height="100%">
              <ComposedChart data={flowsData}>
                <CartesianGrid strokeDasharray="1 1" stroke={BLOOMBERG_GRAY} opacity={0.3} />
                <XAxis dataKey="date" stroke={BLOOMBERG_WHITE} fontSize={8} />
                <YAxis stroke={BLOOMBERG_WHITE} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: BLOOMBERG_DARK_BG, border: `1px solid ${BLOOMBERG_GRAY}`, fontSize: '9px' }} />
                <Bar dataKey="inflows" fill={BLOOMBERG_GREEN} />
                <Bar dataKey="outflows" fill={BLOOMBERG_RED} />
                <Line type="monotone" dataKey="net" stroke={BLOOMBERG_YELLOW} strokeWidth={2} />
              </ComposedChart>
            </ResponsiveContainer>
          </ChartBox>

          <ChartBox title="PERFORMANCE ATTRIBUTION">
            <ResponsiveContainer width="100%" height="100%">
              <BarChart data={performanceData}>
                <CartesianGrid strokeDasharray="1 1" stroke={BLOOMBERG_GRAY} opacity={0.3} />
                <XAxis dataKey="period" stroke={BLOOMBERG_WHITE} fontSize={8} />
                <YAxis stroke={BLOOMBERG_WHITE} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: BLOOMBERG_DARK_BG, border: `1px solid ${BLOOMBERG_GRAY}`, fontSize: '9px' }} />
                <Bar dataKey="return" fill={BLOOMBERG_GREEN} />
                <Bar dataKey="benchmark" fill={BLOOMBERG_GRAY} opacity={0.5} />
              </BarChart>
            </ResponsiveContainer>
          </ChartBox>

          {/* Row 4 */}
          <ChartBox title="FACTOR ATTRIBUTION">
            <ResponsiveContainer width="100%" height="100%">
              <BarChart data={attribution} layout="horizontal">
                <CartesianGrid strokeDasharray="1 1" stroke={BLOOMBERG_GRAY} opacity={0.3} />
                <XAxis type="number" stroke={BLOOMBERG_WHITE} fontSize={8} />
                <YAxis type="category" dataKey="factor" stroke={BLOOMBERG_WHITE} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: BLOOMBERG_DARK_BG, border: `1px solid ${BLOOMBERG_GRAY}`, fontSize: '9px' }} />
                <Bar dataKey="contribution" fill={(entry) => entry.contribution > 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED} />
              </BarChart>
            </ResponsiveContainer>
          </ChartBox>

          <ChartBox title="MONEY FLOW INDEX">
            <ResponsiveContainer width="100%" height="100%">
              <LineChart data={moneyFlowData}>
                <CartesianGrid strokeDasharray="1 1" stroke={BLOOMBERG_GRAY} opacity={0.3} />
                <XAxis dataKey="day" stroke={BLOOMBERG_WHITE} fontSize={8} />
                <YAxis stroke={BLOOMBERG_WHITE} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: BLOOMBERG_DARK_BG, border: `1px solid ${BLOOMBERG_GRAY}`, fontSize: '9px' }} />
                <Line type="monotone" dataKey="mfi" stroke={BLOOMBERG_PURPLE} strokeWidth={1} dot={false} />
                <ReferenceLine y={20} stroke={BLOOMBERG_RED} strokeDasharray="2 2" />
                <ReferenceLine y={80} stroke={BLOOMBERG_RED} strokeDasharray="2 2" />
              </LineChart>
            </ResponsiveContainer>
          </ChartBox>

          <ChartBox title="GLOBAL MARKETS">
            <ResponsiveContainer width="100%" height="100%">
              <RadarChart data={marketData}>
                <PolarGrid stroke={BLOOMBERG_GRAY} />
                <PolarAngleAxis dataKey="market" tick={{ fontSize: 9, fill: BLOOMBERG_WHITE }} />
                <PolarRadiusAxis tick={{ fontSize: 8, fill: BLOOMBERG_GRAY }} />
                <Radar
                  name="Return"
                  dataKey="return"
                  stroke={BLOOMBERG_GREEN}
                  fill={BLOOMBERG_GREEN}
                  fillOpacity={0.3}
                />
                <Radar
                  name="Volatility"
                  dataKey="volatility"
                  stroke={BLOOMBERG_RED}
                  fill={BLOOMBERG_RED}
                  fillOpacity={0.2}
                />
                <Tooltip contentStyle={{ backgroundColor: BLOOMBERG_DARK_BG, border: `1px solid ${BLOOMBERG_GRAY}`, fontSize: '9px' }} />
              </RadarChart>
            </ResponsiveContainer>
          </ChartBox>

          <ChartBox title="YIELD CURVE">
            <ResponsiveContainer width="100%" height="100%">
              <AreaChart data={bondData}>
                <CartesianGrid strokeDasharray="1 1" stroke={BLOOMBERG_GRAY} opacity={0.3} />
                <XAxis dataKey="maturity" stroke={BLOOMBERG_WHITE} fontSize={8} />
                <YAxis stroke={BLOOMBERG_WHITE} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: BLOOMBERG_DARK_BG, border: `1px solid ${BLOOMBERG_GRAY}`, fontSize: '9px' }} />
                <Area type="monotone" dataKey="yield" stroke={BLOOMBERG_YELLOW} fill={BLOOMBERG_YELLOW} fillOpacity={0.3} />
              </AreaChart>
            </ResponsiveContainer>
          </ChartBox>

          <ChartBox title="STRESS SCENARIOS">
            <ResponsiveContainer width="100%" height="100%">
              <ComposedChart data={stressData}>
                <CartesianGrid strokeDasharray="1 1" stroke={BLOOMBERG_GRAY} opacity={0.3} />
                <XAxis dataKey="scenario" stroke={BLOOMBERG_WHITE} fontSize={7} angle={-45} />
                <YAxis stroke={BLOOMBERG_WHITE} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: BLOOMBERG_DARK_BG, border: `1px solid ${BLOOMBERG_GRAY}`, fontSize: '9px' }} />
                <Bar dataKey="probability" fill={BLOOMBERG_BLUE} />
                <Line type="monotone" dataKey="impact" stroke={BLOOMBERG_RED} strokeWidth={2} />
              </ComposedChart>
            </ResponsiveContainer>
          </ChartBox>
        </div>
      </div>

      {/* Status Bar */}
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
            <span>Financial Analytics System v2.8.1 | Real-time portfolio analysis</span>
            <span>Models: 47 Active | Data Sources: 23 | Processing: 1.2M ticks/min</span>
          </div>
          <div style={{ display: 'flex', gap: '16px' }}>
            <span>Risk: VaR $45K | Sharpe: 1.84 | Beta: 0.97</span>
            <span>Session: {currentTime.toTimeString().substring(0, 8)}</span>
            <span style={{ color: BLOOMBERG_GREEN }}>SYSTEM: ACTIVE</span>
          </div>
        </div>
      </div>
    </div>
  );
};

export default AnalyticsTab;