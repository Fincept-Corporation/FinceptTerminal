import React, { useState, useEffect } from 'react';

interface StockAnalysis {
  ticker: string;
  company: string;
  sector: string;
  price: number;
  target: number;
  rating: 'STRONG_BUY' | 'BUY' | 'HOLD' | 'SELL' | 'STRONG_SELL';
  analyst: string;
  firm: string;
  updated: string;
  keyPoints: string[];
  financials: {
    pe: number;
    eps: number;
    revenue: string;
    margin: number;
    debt: string;
  };
  catalysts: string[];
  risks: string[];
}

interface Valuation {
  method: string;
  value: number;
  weight: number;
}

const EquityResearchTab: React.FC = () => {
  const [currentTime, setCurrentTime] = useState(new Date());
  const [selectedStock, setSelectedStock] = useState('AAPL');
  const [activeView, setActiveView] = useState('OVERVIEW');
  const [researchCount] = useState(8947);

  // Bloomberg color scheme
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
    }, 1000);
    return () => clearInterval(timer);
  }, []);

  const researchReports: StockAnalysis[] = [
    {
      ticker: 'AAPL',
      company: 'Apple Inc.',
      sector: 'Technology',
      price: 184.92,
      target: 225.00,
      rating: 'STRONG_BUY',
      analyst: 'John Smith',
      firm: 'Goldman Sachs',
      updated: '2024-01-15',
      keyPoints: [
        'iPhone 15 cycle showing stronger-than-expected demand with Pro models mix at 65%',
        'Services revenue trajectory accelerating - $22.3B quarterly (30% margin) growing 18% YoY',
        'Vision Pro spatial computing launch represents $50B+ TAM over 5 years',
        'Apple Silicon transition complete - Mac margins expanded 400bps to 38%',
        'India manufacturing ramp reducing China dependency - now 12% of iPhone production',
        '$90B annual buyback supporting EPS growth - reduced share count by 3.5% in FY2023'
      ],
      financials: {
        pe: 29.8,
        eps: 6.42,
        revenue: '$383.9B',
        margin: 26.4,
        debt: '$109.3B'
      },
      catalysts: [
        'Q1 2024 earnings beat expected (Jan 25) - iPhone revenue $69B vs $65B est',
        'Vision Pro launch Feb 2024 - pre-orders exceeding supply',
        'AI features integration across ecosystem announced at WWDC June 2024',
        'Services attach rate improvement - ARPU up 15% YoY to $24/month',
        'Potential dividend increase (15%+) and buyback expansion to $100B'
      ],
      risks: [
        'China revenue exposure (18%) - geopolitical tensions and Huawei competition',
        'Regulatory scrutiny on App Store fees (EU Digital Markets Act)',
        'iPhone upgrade cycles lengthening - 4.2 years vs 3.5 years historically',
        'Valuation premium vs peers - trading at 29x vs MSFT 35x, GOOGL 25x'
      ]
    },
    {
      ticker: 'NVDA',
      company: 'NVIDIA Corporation',
      sector: 'Semiconductors',
      price: 501.23,
      target: 750.00,
      rating: 'STRONG_BUY',
      analyst: 'Sarah Chen',
      firm: 'Morgan Stanley',
      updated: '2024-01-14',
      keyPoints: [
        'Data center GPU revenue $18.4B in Q3 (up 279% YoY) - H100 demand exceeding supply by 3x',
        'AI infrastructure buildout driving 5-year supercycle - TAM expanding to $1T by 2030',
        'Gross margins sustained at 75%+ despite competitive pressure from AMD/Intel',
        'CUDA software moat strengthens - 4M developers, 40K companies using platform',
        'Automotive/robotics revenue inflecting - $300M quarterly growing 50% YoY',
        'Next-gen Blackwell architecture (2024) promises 4x performance at same power'
      ],
      financials: {
        pe: 67.2,
        eps: 7.89,
        revenue: '$60.9B',
        margin: 54.8,
        debt: '$9.7B'
      },
      catalysts: [
        'Q4 earnings (Feb 21) - guidance raise expected to $22B revenue (vs $20B est)',
        'GTC conference March 2024 - Blackwell GPU unveiling, major cloud partnerships',
        'Hyperscaler capex acceleration - MSFT/GOOGL/AMZN combined $200B for AI infrastructure',
        'China export restriction easing potential - could add $5B+ annual revenue',
        'Automotive design wins - Mercedes, Tesla, BYD partnerships ramping'
      ],
      risks: [
        'Competitive threats from custom chips (Google TPU, Amazon Trainium, Tesla Dojo)',
        'Export controls to China limiting 20-25% of datacenter TAM',
        'Hyperscaler in-house chip development reducing NVDA dependency',
        'Valuation stretched - trading at 25x forward sales vs 10-year avg of 15x'
      ]
    },
    {
      ticker: 'MSFT',
      company: 'Microsoft Corporation',
      sector: 'Technology',
      price: 378.45,
      target: 450.00,
      rating: 'BUY',
      analyst: 'David Park',
      firm: 'JP Morgan',
      updated: '2024-01-16',
      keyPoints: [
        'Azure cloud revenue $31.8B quarterly (+29% YoY) - gaining share vs AWS',
        'OpenAI partnership driving Azure AI services to $1B+ quarterly run rate',
        'Office 365 Copilot rollout - early adopters showing 30% productivity gains',
        'Gaming segment stable post-Activision merger - $7.1B quarterly revenue',
        'Operating leverage improving - op margins expanded to 47% (up 200bps YoY)',
        'LinkedIn revenue acceleration - $4B quarterly growing 8% (improving from 5%)'
      ],
      financials: {
        pe: 35.2,
        eps: 11.23,
        revenue: '$227.6B',
        margin: 42.1,
        debt: '$97.8B'
      },
      catalysts: [
        'Copilot monetization inflection - targeting 40% of Office 365 users by Q2',
        'Azure OpenAI Service expanding - 18K organizations, up from 2.5K in Q1',
        'Gaming growth reacceleration - Call of Duty integration boosting Game Pass',
        'Windows 11 AI PC refresh cycle starting - Copilot+ PCs launching Q2 2024'
      ],
      risks: [
        'OpenAI investment creating dependency - $10B committed, exclusivity concerns',
        'Azure growth deceleration risk if AI workloads don\'t sustain',
        'Gaming integration challenges - Activision culture/talent retention',
        'Regulatory scrutiny on AI partnerships and gaming consolidation'
      ]
    },
    {
      ticker: 'TSLA',
      company: 'Tesla Inc.',
      sector: 'Automotive',
      price: 251.78,
      target: 320.00,
      rating: 'BUY',
      analyst: 'Michael Zhang',
      firm: 'BofA Securities',
      updated: '2024-01-13',
      keyPoints: [
        'Q4 deliveries beat at 484K units (up 20% YoY) - Model Y best-selling vehicle globally',
        'Cybertruck production ramping - targeting 200K units in 2024 at 25% gross margin',
        'Energy storage business inflecting - 15 GWh deployed in 2023 (up 125% YoY)',
        'FSD Beta v12 neural net showing step-function improvement - intervention rate down 80%',
        'Cost reduction initiatives - $4,000 per vehicle saved through manufacturing improvements',
        'Gigafactory Mexico groundbreaking - next-gen $25K vehicle production start 2025'
      ],
      financials: {
        pe: 68.4,
        eps: 3.68,
        revenue: '$96.8B',
        margin: 17.6,
        debt: '$9.4B'
      },
      catalysts: [
        'Q4 earnings (Jan 24) - guidance for 2M+ deliveries in 2024',
        'Model 2 ($25K vehicle) reveal expected Q2 2024 - addresses mass market',
        'FSD subscription take rate improving - now 15% of fleet (vs 11% in Q1)',
        'Energy storage margins expanding - approaching automotive parity at 20%+',
        'Robotaxi service beta launch potential in select cities by Q4 2024'
      ],
      risks: [
        'EV demand uncertainty amid broader auto slowdown and rate environment',
        'Price cuts impacting margins - 5 reductions in 2023, gross margin compressed to 17.6%',
        'Competition intensifying - BYD, Hyundai/Kia, legacy OEMs launching competitive EVs',
        'Regulatory/legal overhang - FSD classification, Autopilot investigations',
        'Execution risk on multiple initiatives - Cybertruck, Model 2, Robotaxi, Optimus'
      ]
    }
  ];

  const currentStock = researchReports.find(r => r.ticker === selectedStock) || researchReports[0];

  const valuationModels: Valuation[] = [
    { method: 'DCF (10Y)', value: 225.00, weight: 40 },
    { method: 'P/E Multiple', value: 215.00, weight: 25 },
    { method: 'EV/EBITDA', value: 220.00, weight: 20 },
    { method: 'Sum-of-Parts', value: 230.00, weight: 15 }
  ];

  const getRatingColor = (rating: string) => {
    switch (rating) {
      case 'STRONG_BUY': return BLOOMBERG_GREEN;
      case 'BUY': return BLOOMBERG_CYAN;
      case 'HOLD': return BLOOMBERG_YELLOW;
      case 'SELL': return BLOOMBERG_ORANGE;
      case 'STRONG_SELL': return BLOOMBERG_RED;
      default: return BLOOMBERG_GRAY;
    }
  };

  const peerComparison = [
    { ticker: 'AAPL', pe: 29.8, fwdPE: 27.3, pegRatio: 2.1, roe: 147.3, debtEquity: 1.78 },
    { ticker: 'MSFT', pe: 35.2, fwdPE: 31.8, pegRatio: 2.4, roe: 42.1, debtEquity: 0.48 },
    { ticker: 'GOOGL', pe: 25.4, fwdPE: 22.1, pegRatio: 1.9, roe: 28.4, debtEquity: 0.11 },
    { ticker: 'META', pe: 28.7, fwdPE: 24.6, pegRatio: 1.7, roe: 32.8, debtEquity: 0.00 }
  ];

  const recentResearch = [
    { ticker: 'AAPL', action: 'UPGRADE', from: 'BUY', to: 'STRONG_BUY', firm: 'Goldman Sachs', time: '2h ago' },
    { ticker: 'NVDA', action: 'TARGET_RAISE', from: '650', to: '750', firm: 'Morgan Stanley', time: '4h ago' },
    { ticker: 'TSLA', action: 'INITIATE', rating: 'BUY', target: '320', firm: 'BofA Securities', time: '6h ago' },
    { ticker: 'GOOGL', action: 'REITERATE', rating: 'BUY', target: '165', firm: 'JP Morgan', time: '8h ago' },
    { ticker: 'MSFT', action: 'TARGET_RAISE', from: '420', to: '450', firm: 'Barclays', time: '12h ago' }
  ];

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
          <span style={{ color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>FINCEPT EQUITY RESEARCH</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_GREEN }}>● LIVE COVERAGE</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_CYAN }}>REPORTS: {researchCount.toLocaleString()}</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_YELLOW }}>ANALYSTS: 3,847</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_WHITE }}>{currentTime.toISOString().replace('T', ' ').substring(0, 19)} UTC</span>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '11px' }}>
          <span style={{ color: BLOOMBERG_GRAY }}>COVERAGE:</span>
          <span style={{ color: BLOOMBERG_GREEN }}>5,234 STOCKS</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_GRAY }}>TODAY:</span>
          <span style={{ color: BLOOMBERG_GREEN }}>123 UPGRADES</span>
          <span style={{ color: BLOOMBERG_RED }}>45 DOWNGRADES</span>
          <span style={{ color: BLOOMBERG_CYAN }}>234 TARGET CHANGES</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_GRAY }}>CONSENSUS:</span>
          <span style={{ color: BLOOMBERG_GREEN }}>BULLISH 67%</span>
          <span style={{ color: BLOOMBERG_YELLOW }}>NEUTRAL 25%</span>
          <span style={{ color: BLOOMBERG_RED }}>BEARISH 8%</span>
        </div>
      </div>

      {/* Function Keys */}
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
        padding: '2px 4px',
        flexShrink: 0
      }}>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(12, 1fr)', gap: '2px' }}>
          {[
            { key: "F1", label: "OVERVIEW", view: "OVERVIEW" },
            { key: "F2", label: "VALUATION", view: "VALUATION" },
            { key: "F3", label: "FINANCIALS", view: "FINANCIALS" },
            { key: "F4", label: "CATALYSTS", view: "CATALYSTS" },
            { key: "F5", label: "RISKS", view: "RISKS" },
            { key: "F6", label: "PEERS", view: "PEERS" },
            { key: "F7", label: "ESTIMATE", view: "ESTIMATE" },
            { key: "F8", label: "MODEL", view: "MODEL" },
            { key: "F9", label: "SEARCH", view: "SEARCH" },
            { key: "F10", label: "ALERTS", view: "ALERTS" },
            { key: "F11", label: "EXPORT", view: "EXPORT" },
            { key: "F12", label: "SETTINGS", view: "SETTINGS" }
          ].map(item => (
            <button key={item.key}
              onClick={() => item.view && setActiveView(item.view)}
              style={{
                backgroundColor: activeView === item.view ? BLOOMBERG_ORANGE : BLOOMBERG_DARK_BG,
                border: `1px solid ${BLOOMBERG_GRAY}`,
                color: activeView === item.view ? BLOOMBERG_DARK_BG : BLOOMBERG_WHITE,
                padding: '2px 4px',
                fontSize: '9px',
                height: '16px',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                fontWeight: activeView === item.view ? 'bold' : 'normal',
                cursor: 'pointer'
              }}>
              <span style={{ color: BLOOMBERG_YELLOW }}>{item.key}:</span>
              <span style={{ marginLeft: '2px' }}>{item.label}</span>
            </button>
          ))}
        </div>
      </div>

      {/* Main Content */}
      <div style={{ flex: 1, overflow: 'auto', padding: '4px', backgroundColor: '#050505' }}>
        <div style={{ display: 'flex', gap: '4px', height: '100%' }}>

          {/* Left Panel - Stock List */}
          <div style={{
            width: '280px',
            backgroundColor: BLOOMBERG_PANEL_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            padding: '4px',
            overflow: 'auto'
          }}>
            <div style={{ color: BLOOMBERG_ORANGE, fontSize: '11px', fontWeight: 'bold', marginBottom: '4px' }}>
              RESEARCH COVERAGE
            </div>
            <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '8px' }}></div>

            {researchReports.map((stock, index) => (
              <button key={index}
                onClick={() => setSelectedStock(stock.ticker)}
                style={{
                  width: '100%',
                  padding: '6px',
                  marginBottom: '3px',
                  backgroundColor: selectedStock === stock.ticker ? 'rgba(255,165,0,0.1)' : 'rgba(255,255,255,0.02)',
                  border: `1px solid ${selectedStock === stock.ticker ? BLOOMBERG_ORANGE : 'transparent'}`,
                  textAlign: 'left',
                  cursor: 'pointer',
                  fontSize: '10px'
                }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                  <span style={{ color: BLOOMBERG_WHITE, fontWeight: 'bold' }}>{stock.ticker}</span>
                  <span style={{ color: getRatingColor(stock.rating), fontSize: '8px' }}>{stock.rating.replace('_', ' ')}</span>
                </div>
                <div style={{ color: BLOOMBERG_GRAY, fontSize: '9px', marginBottom: '2px' }}>{stock.company}</div>
                <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '9px' }}>
                  <span style={{ color: BLOOMBERG_CYAN }}>${stock.price.toFixed(2)}</span>
                  <span style={{ color: BLOOMBERG_GREEN }}>PT: ${stock.target.toFixed(2)}</span>
                </div>
                <div style={{ fontSize: '8px', color: BLOOMBERG_GRAY, marginTop: '2px' }}>
                  Upside: {((stock.target / stock.price - 1) * 100).toFixed(1)}%
                </div>
              </button>
            ))}

            <div style={{ borderTop: `1px solid ${BLOOMBERG_GRAY}`, marginTop: '12px', paddingTop: '8px' }}>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
                RECENT UPDATES
              </div>
              {recentResearch.map((item, index) => (
                <div key={index} style={{
                  padding: '3px',
                  marginBottom: '3px',
                  backgroundColor: 'rgba(255,255,255,0.02)',
                  fontSize: '8px',
                  lineHeight: '1.3'
                }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                    <span style={{ color: BLOOMBERG_CYAN }}>{item.ticker}</span>
                    <span style={{ color: BLOOMBERG_YELLOW }}>{item.time}</span>
                  </div>
                  <div style={{ color: BLOOMBERG_WHITE, marginBottom: '1px' }}>
                    {item.action === 'UPGRADE' && `${item.from} → ${item.to}`}
                    {item.action === 'TARGET_RAISE' && `Target: $${item.from} → $${item.to}`}
                    {item.action === 'INITIATE' && `Initiate: ${item.rating} PT $${item.target}`}
                    {item.action === 'REITERATE' && `Reiterate: ${item.rating} PT $${item.target}`}
                  </div>
                  <div style={{ color: BLOOMBERG_GRAY }}>{item.firm}</div>
                </div>
              ))}
            </div>

            <div style={{ borderTop: `1px solid ${BLOOMBERG_GRAY}`, marginTop: '12px', paddingTop: '8px' }}>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
                TOP SECTORS
              </div>
              {[
                { sector: 'Technology', rating: 'OVERWEIGHT', coverage: 847 },
                { sector: 'Financials', rating: 'NEUTRAL', coverage: 623 },
                { sector: 'Healthcare', rating: 'OVERWEIGHT', coverage: 512 },
                { sector: 'Energy', rating: 'OVERWEIGHT', coverage: 289 },
                { sector: 'Consumer', rating: 'UNDERWEIGHT', coverage: 734 }
              ].map((item, index) => (
                <div key={index} style={{
                  padding: '2px',
                  marginBottom: '2px',
                  fontSize: '9px',
                  display: 'flex',
                  justifyContent: 'space-between'
                }}>
                  <span style={{ color: BLOOMBERG_WHITE }}>{item.sector}</span>
                  <div>
                    <span style={{
                      color: item.rating === 'OVERWEIGHT' ? BLOOMBERG_GREEN :
                             item.rating === 'NEUTRAL' ? BLOOMBERG_YELLOW : BLOOMBERG_RED,
                      marginRight: '4px'
                    }}>{item.rating}</span>
                    <span style={{ color: BLOOMBERG_GRAY }}>({item.coverage})</span>
                  </div>
                </div>
              ))}
            </div>
          </div>

          {/* Center Panel - Research Report */}
          <div style={{
            flex: 1,
            backgroundColor: BLOOMBERG_PANEL_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            padding: '8px',
            overflow: 'auto'
          }}>
            {/* Stock Header */}
            <div style={{ marginBottom: '12px' }}>
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: '6px' }}>
                <div>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '4px' }}>
                    <span style={{ color: BLOOMBERG_ORANGE, fontSize: '18px', fontWeight: 'bold' }}>{currentStock.ticker}</span>
                    <span style={{ color: BLOOMBERG_WHITE, fontSize: '14px' }}>{currentStock.company}</span>
                    <span style={{ color: BLOOMBERG_BLUE, fontSize: '10px', padding: '2px 6px', backgroundColor: 'rgba(100,150,250,0.2)' }}>
                      {currentStock.sector}
                    </span>
                  </div>
                  <div style={{ display: 'flex', gap: '16px', fontSize: '11px' }}>
                    <span style={{ color: BLOOMBERG_GRAY }}>ANALYST:</span>
                    <span style={{ color: BLOOMBERG_CYAN }}>{currentStock.analyst}</span>
                    <span style={{ color: BLOOMBERG_GRAY }}>|</span>
                    <span style={{ color: BLOOMBERG_GRAY }}>FIRM:</span>
                    <span style={{ color: BLOOMBERG_WHITE }}>{currentStock.firm}</span>
                    <span style={{ color: BLOOMBERG_GRAY }}>|</span>
                    <span style={{ color: BLOOMBERG_GRAY }}>UPDATED:</span>
                    <span style={{ color: BLOOMBERG_YELLOW }}>{currentStock.updated}</span>
                  </div>
                </div>
                <div style={{ textAlign: 'right' }}>
                  <div style={{ fontSize: '22px', fontWeight: 'bold', color: BLOOMBERG_WHITE, marginBottom: '2px' }}>
                    ${currentStock.price.toFixed(2)}
                  </div>
                  <div style={{ fontSize: '10px', color: BLOOMBERG_GRAY, marginBottom: '4px' }}>
                    CURRENT PRICE
                  </div>
                </div>
              </div>

              <div style={{ display: 'flex', gap: '20px', padding: '8px', backgroundColor: 'rgba(255,255,255,0.02)', borderRadius: '4px' }}>
                <div>
                  <div style={{ color: BLOOMBERG_GRAY, fontSize: '9px', marginBottom: '2px' }}>RATING</div>
                  <div style={{ color: getRatingColor(currentStock.rating), fontSize: '12px', fontWeight: 'bold' }}>
                    {currentStock.rating.replace('_', ' ')}
                  </div>
                </div>
                <div>
                  <div style={{ color: BLOOMBERG_GRAY, fontSize: '9px', marginBottom: '2px' }}>PRICE TARGET</div>
                  <div style={{ color: BLOOMBERG_GREEN, fontSize: '12px', fontWeight: 'bold' }}>
                    ${currentStock.target.toFixed(2)}
                  </div>
                </div>
                <div>
                  <div style={{ color: BLOOMBERG_GRAY, fontSize: '9px', marginBottom: '2px' }}>UPSIDE</div>
                  <div style={{ color: BLOOMBERG_GREEN, fontSize: '12px', fontWeight: 'bold' }}>
                    {((currentStock.target / currentStock.price - 1) * 100).toFixed(1)}%
                  </div>
                </div>
                <div>
                  <div style={{ color: BLOOMBERG_GRAY, fontSize: '9px', marginBottom: '2px' }}>P/E RATIO</div>
                  <div style={{ color: BLOOMBERG_CYAN, fontSize: '12px', fontWeight: 'bold' }}>
                    {currentStock.financials.pe.toFixed(1)}x
                  </div>
                </div>
                <div>
                  <div style={{ color: BLOOMBERG_GRAY, fontSize: '9px', marginBottom: '2px' }}>EPS (TTM)</div>
                  <div style={{ color: BLOOMBERG_CYAN, fontSize: '12px', fontWeight: 'bold' }}>
                    ${currentStock.financials.eps.toFixed(2)}
                  </div>
                </div>
                <div>
                  <div style={{ color: BLOOMBERG_GRAY, fontSize: '9px', marginBottom: '2px' }}>REVENUE</div>
                  <div style={{ color: BLOOMBERG_CYAN, fontSize: '12px', fontWeight: 'bold' }}>
                    {currentStock.financials.revenue}
                  </div>
                </div>
                <div>
                  <div style={{ color: BLOOMBERG_GRAY, fontSize: '9px', marginBottom: '2px' }}>NET MARGIN</div>
                  <div style={{ color: BLOOMBERG_GREEN, fontSize: '12px', fontWeight: 'bold' }}>
                    {currentStock.financials.margin.toFixed(1)}%
                  </div>
                </div>
              </div>
            </div>

            <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '8px' }}></div>

            {/* Investment Thesis */}
            <div style={{ marginBottom: '16px' }}>
              <div style={{ color: BLOOMBERG_ORANGE, fontSize: '12px', fontWeight: 'bold', marginBottom: '6px' }}>
                INVESTMENT THESIS & KEY POINTS
              </div>
              <div style={{ fontSize: '10px', lineHeight: '1.6' }}>
                {currentStock.keyPoints.map((point, index) => (
                  <div key={index} style={{
                    marginBottom: '6px',
                    padding: '6px',
                    backgroundColor: 'rgba(255,255,255,0.02)',
                    borderLeft: `3px solid ${BLOOMBERG_GREEN}`
                  }}>
                    <span style={{ color: BLOOMBERG_YELLOW, marginRight: '6px' }}>▸</span>
                    <span style={{ color: BLOOMBERG_WHITE }}>{point}</span>
                  </div>
                ))}
              </div>
            </div>

            {/* Catalysts */}
            <div style={{ marginBottom: '16px' }}>
              <div style={{ color: BLOOMBERG_GREEN, fontSize: '12px', fontWeight: 'bold', marginBottom: '6px' }}>
                KEY CATALYSTS & POSITIVE DRIVERS
              </div>
              <div style={{ fontSize: '10px', lineHeight: '1.5' }}>
                {currentStock.catalysts.map((catalyst, index) => (
                  <div key={index} style={{
                    marginBottom: '4px',
                    padding: '4px 6px',
                    backgroundColor: 'rgba(0,200,0,0.05)',
                    borderLeft: `2px solid ${BLOOMBERG_GREEN}`
                  }}>
                    <span style={{ color: BLOOMBERG_GREEN, marginRight: '6px' }}>✓</span>
                    <span style={{ color: BLOOMBERG_WHITE }}>{catalyst}</span>
                  </div>
                ))}
              </div>
            </div>

            {/* Risks */}
            <div style={{ marginBottom: '16px' }}>
              <div style={{ color: BLOOMBERG_RED, fontSize: '12px', fontWeight: 'bold', marginBottom: '6px' }}>
                KEY RISKS & CONCERNS
              </div>
              <div style={{ fontSize: '10px', lineHeight: '1.5' }}>
                {currentStock.risks.map((risk, index) => (
                  <div key={index} style={{
                    marginBottom: '4px',
                    padding: '4px 6px',
                    backgroundColor: 'rgba(255,0,0,0.05)',
                    borderLeft: `2px solid ${BLOOMBERG_RED}`
                  }}>
                    <span style={{ color: BLOOMBERG_RED, marginRight: '6px' }}>⚠</span>
                    <span style={{ color: BLOOMBERG_WHITE }}>{risk}</span>
                  </div>
                ))}
              </div>
            </div>

            {/* Valuation Summary */}
            <div>
              <div style={{ color: BLOOMBERG_CYAN, fontSize: '12px', fontWeight: 'bold', marginBottom: '6px' }}>
                VALUATION ANALYSIS
              </div>
              <div style={{
                display: 'grid',
                gridTemplateColumns: '2fr 1fr 1fr',
                gap: '2px',
                fontSize: '10px',
                marginBottom: '6px'
              }}>
                <div style={{ color: BLOOMBERG_GRAY, fontWeight: 'bold', padding: '4px', backgroundColor: 'rgba(255,255,255,0.05)' }}>
                  METHODOLOGY
                </div>
                <div style={{ color: BLOOMBERG_GRAY, fontWeight: 'bold', padding: '4px', backgroundColor: 'rgba(255,255,255,0.05)' }}>
                  VALUE
                </div>
                <div style={{ color: BLOOMBERG_GRAY, fontWeight: 'bold', padding: '4px', backgroundColor: 'rgba(255,255,255,0.05)' }}>
                  WEIGHT
                </div>
              </div>
              {valuationModels.map((model, index) => (
                <div key={index} style={{
                  display: 'grid',
                  gridTemplateColumns: '2fr 1fr 1fr',
                  gap: '2px',
                  fontSize: '10px',
                  marginBottom: '2px'
                }}>
                  <div style={{ color: BLOOMBERG_WHITE, padding: '4px', backgroundColor: 'rgba(255,255,255,0.02)' }}>
                    {model.method}
                  </div>
                  <div style={{ color: BLOOMBERG_CYAN, padding: '4px', backgroundColor: 'rgba(255,255,255,0.02)' }}>
                    ${model.value.toFixed(2)}
                  </div>
                  <div style={{ color: BLOOMBERG_YELLOW, padding: '4px', backgroundColor: 'rgba(255,255,255,0.02)' }}>
                    {model.weight}%
                  </div>
                </div>
              ))}
              <div style={{
                display: 'grid',
                gridTemplateColumns: '2fr 1fr 1fr',
                gap: '2px',
                fontSize: '11px',
                marginTop: '6px',
                fontWeight: 'bold'
              }}>
                <div style={{ color: BLOOMBERG_ORANGE, padding: '6px', backgroundColor: 'rgba(255,165,0,0.1)' }}>
                  WEIGHTED AVERAGE TARGET
                </div>
                <div style={{ color: BLOOMBERG_GREEN, padding: '6px', backgroundColor: 'rgba(255,165,0,0.1)' }}>
                  ${currentStock.target.toFixed(2)}
                </div>
                <div style={{ color: BLOOMBERG_GREEN, padding: '6px', backgroundColor: 'rgba(255,165,0,0.1)' }}>
                  100%
                </div>
              </div>
            </div>
          </div>

          {/* Right Panel - Additional Data */}
          <div style={{
            width: '320px',
            backgroundColor: BLOOMBERG_PANEL_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            padding: '4px',
            overflow: 'auto'
          }}>
            <div style={{ color: BLOOMBERG_ORANGE, fontSize: '11px', fontWeight: 'bold', marginBottom: '4px' }}>
              PEER COMPARISON
            </div>
            <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '8px' }}></div>

            <div style={{
              display: 'grid',
              gridTemplateColumns: '1fr 1fr 1fr 1fr 1fr 1fr',
              gap: '2px',
              fontSize: '8px',
              marginBottom: '4px'
            }}>
              <div style={{ color: BLOOMBERG_GRAY, fontWeight: 'bold' }}>TICKER</div>
              <div style={{ color: BLOOMBERG_GRAY, fontWeight: 'bold' }}>P/E</div>
              <div style={{ color: BLOOMBERG_GRAY, fontWeight: 'bold' }}>FWD P/E</div>
              <div style={{ color: BLOOMBERG_GRAY, fontWeight: 'bold' }}>PEG</div>
              <div style={{ color: BLOOMBERG_GRAY, fontWeight: 'bold' }}>ROE</div>
              <div style={{ color: BLOOMBERG_GRAY, fontWeight: 'bold' }}>D/E</div>
            </div>

            {peerComparison.map((peer, index) => (
              <div key={index} style={{
                display: 'grid',
                gridTemplateColumns: '1fr 1fr 1fr 1fr 1fr 1fr',
                gap: '2px',
                fontSize: '9px',
                marginBottom: '2px',
                padding: '2px',
                backgroundColor: peer.ticker === selectedStock ? 'rgba(255,165,0,0.1)' : 'rgba(255,255,255,0.02)'
              }}>
                <div style={{ color: BLOOMBERG_WHITE, fontWeight: 'bold' }}>{peer.ticker}</div>
                <div style={{ color: BLOOMBERG_CYAN }}>{peer.pe.toFixed(1)}</div>
                <div style={{ color: BLOOMBERG_CYAN }}>{peer.fwdPE.toFixed(1)}</div>
                <div style={{ color: BLOOMBERG_YELLOW }}>{peer.pegRatio.toFixed(1)}</div>
                <div style={{ color: BLOOMBERG_GREEN }}>{peer.roe.toFixed(1)}%</div>
                <div style={{ color: BLOOMBERG_WHITE }}>{peer.debtEquity.toFixed(2)}</div>
              </div>
            ))}

            <div style={{ borderTop: `1px solid ${BLOOMBERG_GRAY}`, marginTop: '12px', paddingTop: '8px' }}>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '6px' }}>
                ANALYST CONSENSUS
              </div>
              <div style={{ fontSize: '9px', lineHeight: '1.5' }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '3px' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>Strong Buy:</span>
                  <span style={{ color: BLOOMBERG_GREEN }}>18 (62%)</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '3px' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>Buy:</span>
                  <span style={{ color: BLOOMBERG_CYAN }}>8 (28%)</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '3px' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>Hold:</span>
                  <span style={{ color: BLOOMBERG_YELLOW }}>3 (10%)</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '3px' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>Sell:</span>
                  <span style={{ color: BLOOMBERG_ORANGE }}>0 (0%)</span>
                </div>
                <div style={{ borderTop: `1px solid ${BLOOMBERG_GRAY}`, marginTop: '6px', paddingTop: '6px' }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '3px' }}>
                    <span style={{ color: BLOOMBERG_CYAN }}>Avg Target:</span>
                    <span style={{ color: BLOOMBERG_GREEN, fontWeight: 'bold' }}>${currentStock.target.toFixed(2)}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '3px' }}>
                    <span style={{ color: BLOOMBERG_CYAN }}>High Target:</span>
                    <span style={{ color: BLOOMBERG_GREEN }}>${(currentStock.target * 1.15).toFixed(2)}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: BLOOMBERG_CYAN }}>Low Target:</span>
                    <span style={{ color: BLOOMBERG_RED }}>${(currentStock.target * 0.85).toFixed(2)}</span>
                  </div>
                </div>
              </div>
            </div>

            <div style={{ borderTop: `1px solid ${BLOOMBERG_GRAY}`, marginTop: '12px', paddingTop: '8px' }}>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '6px' }}>
                EARNINGS ESTIMATES
              </div>
              <div style={{ fontSize: '9px' }}>
                <div style={{ marginBottom: '6px' }}>
                  <div style={{ color: BLOOMBERG_GRAY, marginBottom: '2px' }}>Q1 2024 (Current)</div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: BLOOMBERG_WHITE }}>EPS Est:</span>
                    <span style={{ color: BLOOMBERG_CYAN }}>$1.62</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: BLOOMBERG_WHITE }}>Revenue Est:</span>
                    <span style={{ color: BLOOMBERG_CYAN }}>$96.5B</span>
                  </div>
                </div>
                <div style={{ marginBottom: '6px' }}>
                  <div style={{ color: BLOOMBERG_GRAY, marginBottom: '2px' }}>FY 2024</div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: BLOOMBERG_WHITE }}>EPS Est:</span>
                    <span style={{ color: BLOOMBERG_CYAN }}>$6.78</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: BLOOMBERG_WHITE }}>Revenue Est:</span>
                    <span style={{ color: BLOOMBERG_CYAN }}>$402.8B</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: BLOOMBERG_WHITE }}>Growth:</span>
                    <span style={{ color: BLOOMBERG_GREEN }}>+12.4%</span>
                  </div>
                </div>
                <div>
                  <div style={{ color: BLOOMBERG_GRAY, marginBottom: '2px' }}>FY 2025</div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: BLOOMBERG_WHITE }}>EPS Est:</span>
                    <span style={{ color: BLOOMBERG_CYAN }}>$7.48</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: BLOOMBERG_WHITE }}>Revenue Est:</span>
                    <span style={{ color: BLOOMBERG_CYAN }}>$445.2B</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: BLOOMBERG_WHITE }}>Growth:</span>
                    <span style={{ color: BLOOMBERG_GREEN }}>+10.5%</span>
                  </div>
                </div>
              </div>
            </div>

            <div style={{ borderTop: `1px solid ${BLOOMBERG_GRAY}`, marginTop: '12px', paddingTop: '8px' }}>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '6px' }}>
                INSTITUTIONAL OWNERSHIP
              </div>
              <div style={{ fontSize: '9px', lineHeight: '1.5' }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>Total Institutions:</span>
                  <span style={{ color: BLOOMBERG_WHITE }}>4,847</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>% Outstanding:</span>
                  <span style={{ color: BLOOMBERG_CYAN }}>62.3%</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>Q/Q Change:</span>
                  <span style={{ color: BLOOMBERG_GREEN }}>+2.4%</span>
                </div>
                <div style={{ color: BLOOMBERG_GRAY, marginTop: '6px', marginBottom: '4px' }}>Top Holders:</div>
                <div style={{ fontSize: '8px' }}>
                  <div>1. Vanguard Group (8.2%)</div>
                  <div>2. BlackRock (6.7%)</div>
                  <div>3. State Street (4.1%)</div>
                  <div>4. Berkshire Hathaway (3.8%)</div>
                  <div>5. Fidelity (2.9%)</div>
                </div>
              </div>
            </div>
          </div>
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
            <span>Fincept Equity Research Terminal v3.2.1 | Professional analyst coverage</span>
            <span>Coverage: 5,234 stocks | Reports: {researchCount.toLocaleString()} | Analysts: 3,847</span>
          </div>
          <div style={{ display: 'flex', gap: '16px' }}>
            <span>Viewing: {selectedStock}</span>
            <span>Section: {activeView}</span>
            <span style={{ color: BLOOMBERG_GREEN }}>DATA: LIVE</span>
          </div>
        </div>
      </div>
    </div>
  );
};

export default EquityResearchTab;