import React, { useState, useEffect } from 'react';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer, BarChart, Bar, PieChart, Pie, Cell, AreaChart, Area, ScatterChart, Scatter, RadarChart, PolarGrid, PolarAngleAxis, PolarRadiusAxis, Radar, ComposedChart, ReferenceLine, Treemap, FunnelChart, Funnel, LabelList } from 'recharts';
import { useTerminalTheme } from '@/contexts/ThemeContext';

const AnalyticsTab: React.FC = () => {
  const { colors, fontSize, fontFamily, fontWeight, fontStyle } = useTerminalTheme();
  const [currentTime, setCurrentTime] = useState(new Date());
  const [dataUpdateCount, setDataUpdateCount] = useState(0);
  const [alertCount, setAlertCount] = useState(23);

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
      backgroundColor: colors.panel,
      border: `1px solid ${colors.textMuted}`,
      padding: '4px',
      height,
      width,
      overflow: 'hidden'
    }}>
      <div style={{
        color: colors.primary,
        fontSize: '10px',
        fontWeight: 'bold',
        marginBottom: '2px',
        textAlign: 'center',
        borderBottom: `1px solid ${colors.textMuted}`,
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
      backgroundColor: colors.background,
      color: colors.text,
      fontFamily: 'Consolas, monospace',
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column',
      fontSize: '12px'
    }}>
      {/* Header */}
      <div style={{
        backgroundColor: colors.panel,
        borderBottom: `1px solid ${colors.textMuted}`,
        padding: '4px 8px',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '13px', marginBottom: '2px' }}>
          <span style={{ color: colors.primary, fontWeight: 'bold' }}>FINANCIAL ANALYTICS DASHBOARD</span>
          <span style={{ color: colors.text }}>|</span>
          <span style={{ color: colors.secondary, fontWeight: 'bold' }}>PORTFOLIO VALUE: $2.45M</span>
          <span style={{ color: colors.text }}>|</span>
          <span style={{ color: colors.info }}>P&L TODAY: +$12,456 (+0.51%)</span>
          <span style={{ color: colors.text }}>|</span>
          <span style={{ color: colors.warning }}>ALERTS: {alertCount}</span>
          <span style={{ color: colors.text }}>|</span>
          <span style={{ color: colors.secondary }}>● LIVE DATA</span>
          <span style={{ color: colors.text }}>|</span>
          <span style={{ color: colors.text }}>{currentTime.toISOString().replace('T', ' ').substring(0, 19)} UTC</span>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '11px' }}>
          <span style={{ color: colors.textMuted }}>ANALYTICS:</span>
          <span style={{ color: colors.secondary }}>RISK●</span>
          <span style={{ color: colors.secondary }}>PERFORMANCE●</span>
          <span style={{ color: colors.secondary }}>ATTRIBUTION●</span>
          <span style={{ color: colors.secondary }}>TECHNICAL●</span>
          <span style={{ color: colors.text }}>|</span>
          <span style={{ color: colors.textMuted }}>MODELS:</span>
          <span style={{ color: colors.info }}>VaR: $45K</span>
          <span style={{ color: colors.warning }}>Sharpe: 1.84</span>
          <span style={{ color: colors.purple }}>Beta: 0.97</span>
          <span style={{ color: colors.text }}>|</span>
          <span style={{ color: colors.textMuted }}>UPDATES:</span>
          <span style={{ color: colors.accent }}>{dataUpdateCount}</span>
        </div>
      </div>

      {/* Function Keys Bar */}
      <div style={{
        backgroundColor: colors.panel,
        borderBottom: `1px solid ${colors.textMuted}`,
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
              backgroundColor: colors.background,
              border: `1px solid ${colors.textMuted}`,
              color: colors.text,
              padding: '2px 4px',
              fontSize: '9px',
              height: '16px',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center'
            }}>
              <span style={{ color: colors.warning }}>{item.key}:</span>
              <span style={{ marginLeft: '2px' }}>{item.label}</span>
              <span style={{
                marginLeft: '2px',
                color: item.status === '●' ? colors.secondary : colors.warning
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
                <CartesianGrid strokeDasharray="1 1" stroke={colors.textMuted} opacity={0.3} />
                <XAxis dataKey="time" stroke={colors.text} fontSize={8} />
                <YAxis stroke={colors.text} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: colors.background, border: `1px solid ${colors.textMuted}`, fontSize: '9px' }} />
                <Line type="monotone" dataKey="price" stroke={colors.info} strokeWidth={1} dot={false} />
                <Bar dataKey="volume" fill={colors.textMuted} opacity={0.3} />
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
                  fill={colors.info}
                >
                  {sectorData.map((entry, index) => (
                    <Cell key={`cell-${index}`} fill={[colors.info, colors.secondary, colors.warning, colors.alert, colors.purple, colors.accent][index % 6]} />
                  ))}
                </Pie>
                <Tooltip contentStyle={{ backgroundColor: colors.background, border: `1px solid ${colors.textMuted}`, fontSize: '9px' }} />
              </PieChart>
            </ResponsiveContainer>
          </ChartBox>

          <ChartBox title="CORRELATION SCATTER">
            <ResponsiveContainer width="100%" height="100%">
              <ScatterChart data={correlationData}>
                <CartesianGrid strokeDasharray="1 1" stroke={colors.textMuted} opacity={0.3} />
                <XAxis stroke={colors.text} fontSize={8} />
                <YAxis stroke={colors.text} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: colors.background, border: `1px solid ${colors.textMuted}`, fontSize: '9px' }} />
                <Scatter dataKey="y" fill={colors.secondary} />
              </ScatterChart>
            </ResponsiveContainer>
          </ChartBox>

          <ChartBox title="PORTFOLIO COMPOSITION">
            <ResponsiveContainer width="100%" height="100%">
              <BarChart data={portfolioData}>
                <CartesianGrid strokeDasharray="1 1" stroke={colors.textMuted} opacity={0.3} />
                <XAxis dataKey="asset" stroke={colors.text} fontSize={8} />
                <YAxis stroke={colors.text} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: colors.background, border: `1px solid ${colors.textMuted}`, fontSize: '9px' }} />
                <Bar dataKey="allocation" fill={colors.primary} />
              </BarChart>
            </ResponsiveContainer>
          </ChartBox>

          <ChartBox title="RISK METRICS">
            <ResponsiveContainer width="100%" height="100%">
              <LineChart data={riskMetrics.slice(0, 20)}>
                <CartesianGrid strokeDasharray="1 1" stroke={colors.textMuted} opacity={0.3} />
                <XAxis dataKey="day" stroke={colors.text} fontSize={8} />
                <YAxis stroke={colors.text} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: colors.background, border: `1px solid ${colors.textMuted}`, fontSize: '9px' }} />
                <Line type="monotone" dataKey="var" stroke={colors.alert} strokeWidth={1} dot={false} />
                <Line type="monotone" dataKey="sharpe" stroke={colors.secondary} strokeWidth={1} dot={false} />
              </LineChart>
            </ResponsiveContainer>
          </ChartBox>

          {/* Row 2 */}
          <ChartBox title="MOMENTUM ANALYSIS">
            <ResponsiveContainer width="100%" height="100%">
              <BarChart data={momentumData.slice(0, 10)}>
                <CartesianGrid strokeDasharray="1 1" stroke={colors.textMuted} opacity={0.3} />
                <XAxis dataKey="stock" stroke={colors.text} fontSize={8} />
                <YAxis stroke={colors.text} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: colors.background, border: `1px solid ${colors.textMuted}`, fontSize: '9px' }} />
                <Bar dataKey="momentum" fill={colors.secondary} />
              </BarChart>
            </ResponsiveContainer>
          </ChartBox>

          <ChartBox title="INTRADAY LIQUIDITY">
            <ResponsiveContainer width="100%" height="100%">
              <AreaChart data={liquidityData}>
                <CartesianGrid strokeDasharray="1 1" stroke={colors.textMuted} opacity={0.3} />
                <XAxis dataKey="hour" stroke={colors.text} fontSize={8} />
                <YAxis stroke={colors.text} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: colors.background, border: `1px solid ${colors.textMuted}`, fontSize: '9px' }} />
                <Area type="monotone" dataKey="volume" stroke={colors.accent} fill={colors.accent} fillOpacity={0.3} />
              </AreaChart>
            </ResponsiveContainer>
          </ChartBox>

          <ChartBox title="TECHNICAL INDICATORS">
            <ResponsiveContainer width="100%" height="100%">
              <LineChart data={technicalData}>
                <CartesianGrid strokeDasharray="1 1" stroke={colors.textMuted} opacity={0.3} />
                <XAxis dataKey="period" stroke={colors.text} fontSize={8} />
                <YAxis stroke={colors.text} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: colors.background, border: `1px solid ${colors.textMuted}`, fontSize: '9px' }} />
                <Line type="monotone" dataKey="price" stroke={colors.text} strokeWidth={2} dot={false} />
                <Line type="monotone" dataKey="sma20" stroke={colors.warning} strokeWidth={1} dot={false} />
                <Line type="monotone" dataKey="ema12" stroke={colors.secondary} strokeWidth={1} dot={false} />
              </LineChart>
            </ResponsiveContainer>
          </ChartBox>

          <ChartBox title="FUNDAMENTAL RATIOS">
            <ResponsiveContainer width="100%" height="100%">
              <BarChart data={fundamentalData} layout="horizontal">
                <CartesianGrid strokeDasharray="1 1" stroke={colors.textMuted} opacity={0.3} />
                <XAxis type="number" stroke={colors.text} fontSize={8} />
                <YAxis type="category" dataKey="metric" stroke={colors.text} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: colors.background, border: `1px solid ${colors.textMuted}`, fontSize: '9px' }} />
                <Bar dataKey="value" fill={colors.info} />
                <Bar dataKey="benchmark" fill={colors.textMuted} opacity={0.5} />
              </BarChart>
            </ResponsiveContainer>
          </ChartBox>

          <ChartBox title="OPTIONS FLOW">
            <ResponsiveContainer width="100%" height="100%">
              <ComposedChart data={optionsData}>
                <CartesianGrid strokeDasharray="1 1" stroke={colors.textMuted} opacity={0.3} />
                <XAxis dataKey="strike" stroke={colors.text} fontSize={8} />
                <YAxis stroke={colors.text} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: colors.background, border: `1px solid ${colors.textMuted}`, fontSize: '9px' }} />
                <Bar dataKey="calls" fill={colors.secondary} />
                <Bar dataKey="puts" fill={colors.alert} />
                <Line type="monotone" dataKey="iv" stroke={colors.warning} strokeWidth={1} />
              </ComposedChart>
            </ResponsiveContainer>
          </ChartBox>

          {/* Row 3 */}
          <ChartBox title="VOLATILITY SURFACE">
            <ResponsiveContainer width="100%" height="100%">
              <AreaChart data={volatilityData.slice(0, 30)}>
                <CartesianGrid strokeDasharray="1 1" stroke={colors.textMuted} opacity={0.3} />
                <XAxis dataKey="day" stroke={colors.text} fontSize={8} />
                <YAxis stroke={colors.text} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: colors.background, border: `1px solid ${colors.textMuted}`, fontSize: '9px' }} />
                <Area type="monotone" dataKey="realized" stackId="1" stroke={colors.secondary} fill={colors.secondary} fillOpacity={0.6} />
                <Area type="monotone" dataKey="implied" stackId="1" stroke={colors.warning} fill={colors.warning} fillOpacity={0.6} />
              </AreaChart>
            </ResponsiveContainer>
          </ChartBox>

          <ChartBox title="ECONOMIC INDICATORS">
            <ResponsiveContainer width="100%" height="100%">
              <BarChart data={economicData}>
                <CartesianGrid strokeDasharray="1 1" stroke={colors.textMuted} opacity={0.3} />
                <XAxis dataKey="indicator" stroke={colors.text} fontSize={7} angle={-45} />
                <YAxis stroke={colors.text} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: colors.background, border: `1px solid ${colors.textMuted}`, fontSize: '9px' }} />
                <Bar dataKey="current" fill={colors.info} />
                <Bar dataKey="forecast" fill={colors.primary} />
              </BarChart>
            </ResponsiveContainer>
          </ChartBox>

          <ChartBox title="MARKET SENTIMENT">
            <ResponsiveContainer width="100%" height="100%">
              <AreaChart data={sentimentData.slice(0, 15)}>
                <CartesianGrid strokeDasharray="1 1" stroke={colors.textMuted} opacity={0.3} />
                <XAxis dataKey="date" stroke={colors.text} fontSize={8} />
                <YAxis stroke={colors.text} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: colors.background, border: `1px solid ${colors.textMuted}`, fontSize: '9px' }} />
                <Area type="monotone" dataKey="bullish" stackId="1" stroke={colors.secondary} fill={colors.secondary} fillOpacity={0.4} />
                <Area type="monotone" dataKey="bearish" stackId="1" stroke={colors.alert} fill={colors.alert} fillOpacity={0.4} />
              </AreaChart>
            </ResponsiveContainer>
          </ChartBox>

          <ChartBox title="FUND FLOWS">
            <ResponsiveContainer width="100%" height="100%">
              <ComposedChart data={flowsData}>
                <CartesianGrid strokeDasharray="1 1" stroke={colors.textMuted} opacity={0.3} />
                <XAxis dataKey="date" stroke={colors.text} fontSize={8} />
                <YAxis stroke={colors.text} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: colors.background, border: `1px solid ${colors.textMuted}`, fontSize: '9px' }} />
                <Bar dataKey="inflows" fill={colors.secondary} />
                <Bar dataKey="outflows" fill={colors.alert} />
                <Line type="monotone" dataKey="net" stroke={colors.warning} strokeWidth={2} />
              </ComposedChart>
            </ResponsiveContainer>
          </ChartBox>

          <ChartBox title="PERFORMANCE ATTRIBUTION">
            <ResponsiveContainer width="100%" height="100%">
              <BarChart data={performanceData}>
                <CartesianGrid strokeDasharray="1 1" stroke={colors.textMuted} opacity={0.3} />
                <XAxis dataKey="period" stroke={colors.text} fontSize={8} />
                <YAxis stroke={colors.text} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: colors.background, border: `1px solid ${colors.textMuted}`, fontSize: '9px' }} />
                <Bar dataKey="return" fill={colors.secondary} />
                <Bar dataKey="benchmark" fill={colors.textMuted} opacity={0.5} />
              </BarChart>
            </ResponsiveContainer>
          </ChartBox>

          {/* Row 4 */}
          <ChartBox title="FACTOR ATTRIBUTION">
            <ResponsiveContainer width="100%" height="100%">
              <BarChart data={attribution} layout="horizontal">
                <CartesianGrid strokeDasharray="1 1" stroke={colors.textMuted} opacity={0.3} />
                <XAxis type="number" stroke={colors.text} fontSize={8} />
                <YAxis type="category" dataKey="factor" stroke={colors.text} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: colors.background, border: `1px solid ${colors.textMuted}`, fontSize: '9px' }} />
                <Bar dataKey="contribution" fill={colors.secondary} />
              </BarChart>
            </ResponsiveContainer>
          </ChartBox>

          <ChartBox title="MONEY FLOW INDEX">
            <ResponsiveContainer width="100%" height="100%">
              <LineChart data={moneyFlowData}>
                <CartesianGrid strokeDasharray="1 1" stroke={colors.textMuted} opacity={0.3} />
                <XAxis dataKey="day" stroke={colors.text} fontSize={8} />
                <YAxis stroke={colors.text} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: colors.background, border: `1px solid ${colors.textMuted}`, fontSize: '9px' }} />
                <Line type="monotone" dataKey="mfi" stroke={colors.purple} strokeWidth={1} dot={false} />
                <ReferenceLine y={20} stroke={colors.alert} strokeDasharray="2 2" />
                <ReferenceLine y={80} stroke={colors.alert} strokeDasharray="2 2" />
              </LineChart>
            </ResponsiveContainer>
          </ChartBox>

          <ChartBox title="GLOBAL MARKETS">
            <ResponsiveContainer width="100%" height="100%">
              <RadarChart data={marketData}>
                <PolarGrid stroke={colors.textMuted} />
                <PolarAngleAxis dataKey="market" tick={{ fontSize: 9, fill: colors.text }} />
                <PolarRadiusAxis tick={{ fontSize: 8, fill: colors.textMuted }} />
                <Radar
                  name="Return"
                  dataKey="return"
                  stroke={colors.secondary}
                  fill={colors.secondary}
                  fillOpacity={0.3}
                />
                <Radar
                  name="Volatility"
                  dataKey="volatility"
                  stroke={colors.alert}
                  fill={colors.alert}
                  fillOpacity={0.2}
                />
                <Tooltip contentStyle={{ backgroundColor: colors.background, border: `1px solid ${colors.textMuted}`, fontSize: '9px' }} />
              </RadarChart>
            </ResponsiveContainer>
          </ChartBox>

          <ChartBox title="YIELD CURVE">
            <ResponsiveContainer width="100%" height="100%">
              <AreaChart data={bondData}>
                <CartesianGrid strokeDasharray="1 1" stroke={colors.textMuted} opacity={0.3} />
                <XAxis dataKey="maturity" stroke={colors.text} fontSize={8} />
                <YAxis stroke={colors.text} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: colors.background, border: `1px solid ${colors.textMuted}`, fontSize: '9px' }} />
                <Area type="monotone" dataKey="yield" stroke={colors.warning} fill={colors.warning} fillOpacity={0.3} />
              </AreaChart>
            </ResponsiveContainer>
          </ChartBox>

          <ChartBox title="STRESS SCENARIOS">
            <ResponsiveContainer width="100%" height="100%">
              <ComposedChart data={stressData}>
                <CartesianGrid strokeDasharray="1 1" stroke={colors.textMuted} opacity={0.3} />
                <XAxis dataKey="scenario" stroke={colors.text} fontSize={7} angle={-45} />
                <YAxis stroke={colors.text} fontSize={8} />
                <Tooltip contentStyle={{ backgroundColor: colors.background, border: `1px solid ${colors.textMuted}`, fontSize: '9px' }} />
                <Bar dataKey="probability" fill={colors.info} />
                <Line type="monotone" dataKey="impact" stroke={colors.alert} strokeWidth={2} />
              </ComposedChart>
            </ResponsiveContainer>
          </ChartBox>
        </div>
      </div>

      {/* Status Bar */}
      <div style={{
        borderTop: `1px solid ${colors.textMuted}`,
        backgroundColor: colors.panel,
        padding: '2px 8px',
        fontSize: '10px',
        color: colors.textMuted,
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
            <span style={{ color: colors.secondary }}>SYSTEM: ACTIVE</span>
          </div>
        </div>
      </div>
    </div>
  );
};

export default AnalyticsTab;