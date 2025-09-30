import React, { useState, useEffect } from 'react';

interface ForumPost {
  id: string;
  time: string;
  author: string;
  title: string;
  content: string;
  category: string;
  tags: string[];
  views: number;
  replies: number;
  likes: number;
  sentiment: 'BULLISH' | 'BEARISH' | 'NEUTRAL';
  priority: 'HOT' | 'TRENDING' | 'NORMAL';
  verified: boolean;
}

interface ForumUser {
  username: string;
  reputation: number;
  posts: number;
  joined: string;
  status: 'ONLINE' | 'OFFLINE';
}

const ForumTab: React.FC = () => {
  const [currentTime, setCurrentTime] = useState(new Date());
  const [activeCategory, setActiveCategory] = useState('ALL');
  const [sortBy, setSortBy] = useState('LATEST');
  const [onlineUsers, setOnlineUsers] = useState(1247);

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
      setOnlineUsers(prev => prev + Math.floor(Math.random() * 10) - 5);
    }, 5000);
    return () => clearInterval(timer);
  }, []);

  const forumPosts: ForumPost[] = [
    {
      id: '1',
      time: '16:52:18',
      author: 'QuantTrader_Pro',
      title: 'Fed Rate Decision Analysis - 75bps Cut Implications for Q2 2024',
      content: 'Deep analysis of the Federal Reserve\'s emergency rate cut decision. Historical precedents from 2008 and 2020 suggest this could trigger a sustained equity rally. My quant models show S&P 500 has 78% probability of hitting 4,950 within 30 days. Key sectors to watch: Tech (AI infrastructure), Regional Banks (benefiting from rate cuts), Real Estate (commercial REITs). However, watch for potential stagflation risks if inflation doesn\'t respond as expected. Technical analysis shows strong support at 4,750 with RSI indicating oversold conditions in previous weeks. Options market is pricing in 15% move in either direction - suggesting high uncertainty. My personal position: Long QQQ calls, Short VIX, Hedged with TLT puts.',
      category: 'MARKETS',
      tags: ['FED', 'RATES', 'ANALYSIS', 'S&P500'],
      views: 8924,
      replies: 156,
      likes: 342,
      sentiment: 'BULLISH',
      priority: 'HOT',
      verified: true
    },
    {
      id: '2',
      time: '16:48:33',
      author: 'TechInvestor88',
      title: 'NVIDIA Q4 Earnings Preview - AI Chip Demand Analysis',
      content: 'NVDA earnings expected next week. My channel checks with data center operators suggest GPU orders are up 40% QoQ. H100 chips still have 6-month waiting lists. Competition from AMD and custom chips (Google TPU, Amazon Trainium) not materially impacting market share yet. Guidance will be key - looking for $30B+ revenue guidance for FY2024. Stock trading at 45x forward earnings which seems reasonable given 50%+ revenue growth. Potential risks: export restrictions to China (20% of revenue), datacenter capex slowdown, margin compression from next-gen Blackwell chips. My play: Selling $650 puts, buying $750 calls for earnings week. High IV makes this attractive. What are your thoughts on the competitive landscape?',
      category: 'TECH',
      tags: ['NVDA', 'EARNINGS', 'AI', 'SEMICONDUCTORS'],
      views: 6234,
      replies: 89,
      likes: 278,
      sentiment: 'BULLISH',
      priority: 'TRENDING',
      verified: true
    },
    {
      id: '3',
      time: '16:45:12',
      author: 'CryptoWhale_2024',
      title: 'Bitcoin ETF Approval - Historical Analysis of Gold ETF Launch 2004',
      content: 'Now that spot BTC ETFs are approved, I did historical analysis of what happened when Gold ETF (GLD) launched in Nov 2004. Gold was at $450/oz and rallied to $1,900/oz over next 7 years (322% gain). Key differences: Gold already had massive institutional adoption, BTC is earlier in adoption curve. Similarities: Both are "digital gold" narratives, both faced regulatory skepticism, both had futures products before ETFs. My projection: BTC could see $100K within 12-18 months as institutional allocations begin. Fidelity alone has $4.5T AUM - even 1% allocation would be $45B into BTC market. However, watch for: Grayscale GBTC redemptions (discount now closed), Mt. Gox distribution, regulatory changes. I\'m holding long-term, targeting $150K by end of 2025.',
      category: 'CRYPTO',
      tags: ['BTC', 'ETF', 'INSTITUTIONS', 'GOLD'],
      views: 5823,
      replies: 142,
      likes: 401,
      sentiment: 'BULLISH',
      priority: 'HOT',
      verified: true
    },
    {
      id: '4',
      time: '16:41:45',
      author: 'EnergyAnalyst_TX',
      title: 'Saudi Arabia Production Cuts - Oil Supply Dynamics Through 2024',
      content: 'Saudi extending voluntary 1M bpd cuts through Q2. This is critical because global oil inventories are already at 5-year lows. US SPR still depleted (365M barrels vs 650M historical). Demand side: China reopening could add 1.5M bpd demand, US summer driving season approaching. Supply side: US shale growth plateauing at 13M bpd, OPEC+ discipline stronger than expected. My price targets: Brent $95-105 range for Q2/Q3, potential spike to $120 if geopolitical tensions escalate (Middle East, Russia). How to play: Long XLE (energy sector ETF), individual picks - CVX and XOM for stability + dividends, smaller E&Ps for leverage (DVN, FANG). Alternative plays: Refined products (crack spreads widening), tanker stocks (shipping rates up 40% YoY). Risks: Demand destruction above $100/bbl, surprise OPEC production increases.',
      category: 'ENERGY',
      tags: ['OIL', 'OPEC', 'COMMODITIES', 'ENERGY'],
      views: 4156,
      replies: 67,
      likes: 189,
      sentiment: 'BULLISH',
      priority: 'TRENDING',
      verified: true
    },
    {
      id: '5',
      time: '16:38:22',
      author: 'BankingInsider',
      title: 'JPMorgan Q4 Beat - What It Means for Regional Banks',
      content: 'JPM crushed earnings with $15.2B profit. Key takeaway: Net Interest Margin held up despite rate uncertainty. Credit quality remains strong (provisions down 34%). But here\'s what matters for regional banks (KRE, RF, CFG): JPM benefited from deposit flight FROM regional banks after SVB collapse. They gained $200B+ in deposits at near-zero cost. Regional banks face opposite dynamic: deposit costs up 200bps, commercial real estate exposure (office vacancy at 20%+ in major cities), wholesale funding pressure. My contrarian take: Some regional banks are now oversold. Look at tangible book value - many trading at 0.7x TBV. Potential acquisitions by larger banks could unlock value. Playing this with: Long USB and TFC (strong balance sheets), short SBNY and PACW (CRE exposure). Rate cuts actually help net interest income for banks - flattening curve is positive.',
      category: 'BANKING',
      tags: ['JPM', 'BANKS', 'EARNINGS', 'FINANCIALS'],
      views: 3642,
      replies: 78,
      likes: 156,
      sentiment: 'NEUTRAL',
      priority: 'NORMAL',
      verified: true
    },
    {
      id: '6',
      time: '16:35:01',
      author: 'GlobalMacro_Trader',
      title: 'China $2.8T Infrastructure Plan - Supply Chain and Commodity Implications',
      content: 'China announced massive Belt & Road 2.0 initiative. This is bigger than most realize. $2.8T over 5 years = $560B annually (compare to China\'s 2023 GDP of $18T, this is 3% of GDP). Focus areas: AI infrastructure, renewable energy (solar/wind), digital connectivity, ports/logistics. Commodity implications: Steel demand up (already seeing Chinese rebar futures +8%), Copper demand massive (EV charging, grid infrastructure), Rare earths (tech components), Lithium (battery supply chain). Equity plays: Chinese infrastructure stocks (obviously), but ALSO emerging market infrastructure plays (Indonesia, Vietnam, Pakistan). Commodity plays: FCX (copper), ALB (lithium), MP (rare earths). Currency implications: Yuan strength short-term as capital flows in, but medium-term weakness as China exports inflation via commodity demand. This is 2008-2010 China stimulus playbook 2.0. Worked then, could work again.',
      category: 'GLOBAL',
      tags: ['CHINA', 'INFRASTRUCTURE', 'COMMODITIES', 'EM'],
      views: 3198,
      replies: 54,
      likes: 134,
      sentiment: 'BULLISH',
      priority: 'NORMAL',
      verified: false
    },
    {
      id: '7',
      time: '16:32:18',
      author: 'OptionsFlow_Alert',
      title: 'Massive Call Sweeps in Tech Stocks - Someone Knows Something',
      content: 'Unusual options activity detected across multiple tech names. MSFT: 50,000 calls at $400 strike expiring March (premium paid: $45M). GOOGL: 30,000 calls at $150 strike Feb expiry ($22M premium). META: 25,000 calls at $380 strike March ($31M). These are NOT retail traders - this is institutional flow, paying up for upside. Timing suspicious: 2 days before big tech earnings start. Either hedge funds positioning for earnings beats, or insiders know something about AI monetization that market doesn\'t. Historical precedent: Similar unusual activity preceded NVDA\'s 60% rally last year. My interpretation: Smart money is betting on AI revenue acceleration across cloud/advertising. Playing this with: Long dated calls on QQQ and XLK, selling near-term puts to finance. Risk/reward heavily skewed to upside if thesis correct. Monitoring daily options flow for any reversals.',
      category: 'OPTIONS',
      tags: ['OPTIONS', 'TECH', 'FLOW', 'UNUSUAL_ACTIVITY'],
      views: 7234,
      replies: 134,
      likes: 423,
      sentiment: 'BULLISH',
      priority: 'HOT',
      verified: true
    },
    {
      id: '8',
      time: '16:28:45',
      author: 'MacroHedgeFund',
      title: 'Yield Curve Un-Inversion - Historical Recession Indicator Analysis',
      content: 'The 2-10 year Treasury yield curve just un-inverted after 18 months inverted. Historical data: Every recession since 1970 was preceded by curve inversion FOLLOWED by un-inversion. Average lag between un-inversion and recession: 6-12 months. Current situation: Curve un-inverting due to Fed rate cuts (short end falling faster than long end). Question is whether this is "soft landing" scenario or delayed recession. Bull case: Inflation under control, employment strong, consumer spending resilient, corporate earnings growing. Bear case: Leading indicators weak (PMI, LEI, yield curve), commercial real estate stress, consumer credit deteriorating, geopolitical risks. My positioning: Cautiously bullish equities but hedged with TLT (long duration treasuries rally in recession), Gold (safe haven), defensive sectors (utilities, consumer staples). Time horizon matters - 6 month view bullish, 12-18 month view increasingly cautious.',
      category: 'MACRO',
      tags: ['BONDS', 'RECESSION', 'YIELD_CURVE', 'MACRO'],
      views: 4523,
      replies: 89,
      likes: 267,
      sentiment: 'NEUTRAL',
      priority: 'TRENDING',
      verified: true
    }
  ];

  const topContributors: ForumUser[] = [
    { username: 'QuantTrader_Pro', reputation: 8947, posts: 1234, joined: '2021-03-15', status: 'ONLINE' },
    { username: 'CryptoWhale_2024', reputation: 7823, posts: 892, joined: '2020-11-22', status: 'ONLINE' },
    { username: 'OptionsFlow_Alert', reputation: 7456, posts: 1567, joined: '2022-01-08', status: 'ONLINE' },
    { username: 'MacroHedgeFund', reputation: 6789, posts: 743, joined: '2021-07-19', status: 'OFFLINE' },
    { username: 'TechInvestor88', reputation: 6234, posts: 654, joined: '2022-05-12', status: 'ONLINE' },
    { username: 'EnergyAnalyst_TX', reputation: 5892, posts: 521, joined: '2021-09-30', status: 'ONLINE' }
  ];

  const categories = [
    { name: 'ALL', count: 8947, color: BLOOMBERG_WHITE },
    { name: 'MARKETS', count: 2341, color: BLOOMBERG_BLUE },
    { name: 'TECH', count: 1876, color: BLOOMBERG_PURPLE },
    { name: 'CRYPTO', count: 1654, color: BLOOMBERG_CYAN },
    { name: 'OPTIONS', count: 1234, color: BLOOMBERG_YELLOW },
    { name: 'BANKING', count: 987, color: BLOOMBERG_GREEN },
    { name: 'ENERGY', count: 876, color: BLOOMBERG_ORANGE },
    { name: 'MACRO', count: 654, color: BLOOMBERG_RED },
    { name: 'GLOBAL', count: 543, color: BLOOMBERG_PURPLE }
  ];

  const getPriorityColor = (priority: string) => {
    switch (priority) {
      case 'HOT': return BLOOMBERG_RED;
      case 'TRENDING': return BLOOMBERG_ORANGE;
      case 'NORMAL': return BLOOMBERG_GRAY;
      default: return BLOOMBERG_GRAY;
    }
  };

  const getSentimentColor = (sentiment: string) => {
    switch (sentiment) {
      case 'BULLISH': return BLOOMBERG_GREEN;
      case 'BEARISH': return BLOOMBERG_RED;
      case 'NEUTRAL': return BLOOMBERG_YELLOW;
      default: return BLOOMBERG_GRAY;
    }
  };

  const filteredPosts = activeCategory === 'ALL'
    ? forumPosts
    : forumPosts.filter(post => post.category === activeCategory);

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
          <span style={{ color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>FINCEPT GLOBAL FORUM</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_GREEN }}>● LIVE</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_YELLOW }}>USERS ONLINE: {onlineUsers.toLocaleString()}</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_CYAN }}>POSTS TODAY: 2,847</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_WHITE }}>{currentTime.toISOString().replace('T', ' ').substring(0, 19)} UTC</span>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '11px' }}>
          <span style={{ color: BLOOMBERG_GRAY }}>TRENDING:</span>
          <span style={{ color: BLOOMBERG_RED }}>FED_RATES</span>
          <span style={{ color: BLOOMBERG_CYAN }}>BTC_ETF</span>
          <span style={{ color: BLOOMBERG_PURPLE }}>NVDA_EARNINGS</span>
          <span style={{ color: BLOOMBERG_ORANGE }}>OIL_SUPPLY</span>
          <span style={{ color: BLOOMBERG_GREEN }}>AI_REVENUE</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_GRAY }}>SENTIMENT:</span>
          <span style={{ color: BLOOMBERG_GREEN }}>BULLISH 68%</span>
          <span style={{ color: BLOOMBERG_RED }}>BEARISH 18%</span>
          <span style={{ color: BLOOMBERG_YELLOW }}>NEUTRAL 14%</span>
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
            { key: "F1", label: "ALL", filter: "ALL" },
            { key: "F2", label: "MARKETS", filter: "MARKETS" },
            { key: "F3", label: "TECH", filter: "TECH" },
            { key: "F4", label: "CRYPTO", filter: "CRYPTO" },
            { key: "F5", label: "OPTIONS", filter: "OPTIONS" },
            { key: "F6", label: "BANKING", filter: "BANKING" },
            { key: "F7", label: "ENERGY", filter: "ENERGY" },
            { key: "F8", label: "MACRO", filter: "MACRO" },
            { key: "F9", label: "POST", filter: "POST" },
            { key: "F10", label: "SEARCH", filter: "SEARCH" },
            { key: "F11", label: "PROFILE", filter: "PROFILE" },
            { key: "F12", label: "SETTINGS", filter: "SETTINGS" }
          ].map(item => (
            <button key={item.key}
              onClick={() => item.filter !== 'POST' && item.filter !== 'SEARCH' && item.filter !== 'PROFILE' && item.filter !== 'SETTINGS' && setActiveCategory(item.filter)}
              style={{
                backgroundColor: activeCategory === item.filter ? BLOOMBERG_ORANGE : BLOOMBERG_DARK_BG,
                border: `1px solid ${BLOOMBERG_GRAY}`,
                color: activeCategory === item.filter ? BLOOMBERG_DARK_BG : BLOOMBERG_WHITE,
                padding: '2px 4px',
                fontSize: '9px',
                height: '16px',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                fontWeight: activeCategory === item.filter ? 'bold' : 'normal',
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

          {/* Left Panel - Categories & Stats */}
          <div style={{
            width: '280px',
            backgroundColor: BLOOMBERG_PANEL_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            padding: '4px',
            overflow: 'auto'
          }}>
            <div style={{ color: BLOOMBERG_ORANGE, fontSize: '11px', fontWeight: 'bold', marginBottom: '4px' }}>
              FORUM CATEGORIES
            </div>
            <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '4px' }}></div>

            {categories.map((cat, index) => (
              <button key={index}
                onClick={() => setActiveCategory(cat.name)}
                style={{
                  width: '100%',
                  display: 'flex',
                  justifyContent: 'space-between',
                  padding: '4px',
                  marginBottom: '2px',
                  backgroundColor: activeCategory === cat.name ? 'rgba(255,165,0,0.1)' : 'transparent',
                  border: `1px solid ${activeCategory === cat.name ? BLOOMBERG_ORANGE : 'transparent'}`,
                  color: cat.color,
                  fontSize: '10px',
                  cursor: 'pointer',
                  textAlign: 'left'
                }}>
                <span>{cat.name}</span>
                <span style={{ color: BLOOMBERG_CYAN }}>{cat.count}</span>
              </button>
            ))}

            <div style={{ borderTop: `1px solid ${BLOOMBERG_GRAY}`, marginTop: '8px', paddingTop: '8px' }}>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
                TOP CONTRIBUTORS
              </div>
              {topContributors.map((user, index) => (
                <div key={index} style={{
                  padding: '3px',
                  marginBottom: '2px',
                  backgroundColor: 'rgba(255,255,255,0.02)',
                  fontSize: '9px'
                }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                    <span style={{ color: BLOOMBERG_WHITE }}>{user.username}</span>
                    <span style={{ color: user.status === 'ONLINE' ? BLOOMBERG_GREEN : BLOOMBERG_GRAY }}>
                      ●
                    </span>
                  </div>
                  <div style={{ display: 'flex', gap: '6px', color: BLOOMBERG_GRAY }}>
                    <span>REP: {user.reputation}</span>
                    <span>POSTS: {user.posts}</span>
                  </div>
                </div>
              ))}
            </div>

            <div style={{ borderTop: `1px solid ${BLOOMBERG_GRAY}`, marginTop: '8px', paddingTop: '8px' }}>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
                FORUM STATISTICS
              </div>
              <div style={{ fontSize: '9px', lineHeight: '1.4', color: BLOOMBERG_GRAY }}>
                <div>Total Posts: 894,234</div>
                <div>Total Users: 47,832</div>
                <div>Posts Today: 2,847</div>
                <div>Active Now: {onlineUsers}</div>
                <div style={{ color: BLOOMBERG_GREEN }}>Avg Response: 12 min</div>
              </div>
            </div>
          </div>

          {/* Center Panel - Forum Posts */}
          <div style={{
            flex: 1,
            backgroundColor: BLOOMBERG_PANEL_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            padding: '4px',
            overflow: 'auto'
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '4px' }}>
              <div style={{ color: BLOOMBERG_ORANGE, fontSize: '11px', fontWeight: 'bold' }}>
                FORUM POSTS [{activeCategory}]
              </div>
              <div style={{ display: 'flex', gap: '4px', fontSize: '9px' }}>
                <button onClick={() => setSortBy('LATEST')} style={{
                  padding: '2px 6px',
                  backgroundColor: sortBy === 'LATEST' ? BLOOMBERG_ORANGE : BLOOMBERG_DARK_BG,
                  color: sortBy === 'LATEST' ? BLOOMBERG_DARK_BG : BLOOMBERG_WHITE,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  cursor: 'pointer'
                }}>LATEST</button>
                <button onClick={() => setSortBy('HOT')} style={{
                  padding: '2px 6px',
                  backgroundColor: sortBy === 'HOT' ? BLOOMBERG_ORANGE : BLOOMBERG_DARK_BG,
                  color: sortBy === 'HOT' ? BLOOMBERG_DARK_BG : BLOOMBERG_WHITE,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  cursor: 'pointer'
                }}>HOT</button>
                <button onClick={() => setSortBy('TOP')} style={{
                  padding: '2px 6px',
                  backgroundColor: sortBy === 'TOP' ? BLOOMBERG_ORANGE : BLOOMBERG_DARK_BG,
                  color: sortBy === 'TOP' ? BLOOMBERG_DARK_BG : BLOOMBERG_WHITE,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  cursor: 'pointer'
                }}>TOP</button>
              </div>
            </div>
            <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '8px' }}></div>

            {filteredPosts.map((post, index) => (
              <div key={post.id} style={{
                marginBottom: '6px',
                padding: '6px',
                backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent',
                borderLeft: `3px solid ${getPriorityColor(post.priority)}`,
                paddingLeft: '6px'
              }}>
                <div style={{ display: 'flex', gap: '4px', alignItems: 'center', marginBottom: '2px', fontSize: '8px' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>{post.time}</span>
                  <span style={{ color: getPriorityColor(post.priority), fontWeight: 'bold' }}>[{post.priority}]</span>
                  <span style={{ color: BLOOMBERG_BLUE }}>[{post.category}]</span>
                  <span style={{ color: getSentimentColor(post.sentiment) }}>{post.sentiment}</span>
                  {post.verified && <span style={{ color: BLOOMBERG_CYAN }}>✓ VERIFIED</span>}
                </div>

                <div style={{ display: 'flex', gap: '4px', alignItems: 'center', marginBottom: '2px', fontSize: '9px' }}>
                  <span style={{ color: BLOOMBERG_CYAN }}>@{post.author}</span>
                  <span style={{ color: BLOOMBERG_GRAY }}>•</span>
                  <span style={{ color: BLOOMBERG_WHITE, fontWeight: 'bold' }}>{post.title}</span>
                </div>

                <div style={{ color: BLOOMBERG_GRAY, fontSize: '9px', lineHeight: '1.3', marginBottom: '3px' }}>
                  {post.content}
                </div>

                <div style={{ display: 'flex', gap: '6px', fontSize: '8px', marginBottom: '2px' }}>
                  {post.tags.map((tag, i) => (
                    <span key={i} style={{
                      color: BLOOMBERG_YELLOW,
                      backgroundColor: 'rgba(255,255,0,0.1)',
                      padding: '1px 4px',
                      borderRadius: '2px'
                    }}>#{tag}</span>
                  ))}
                </div>

                <div style={{ display: 'flex', gap: '10px', fontSize: '8px' }}>
                  <span style={{ color: BLOOMBERG_CYAN }}>👁️ {post.views.toLocaleString()}</span>
                  <span style={{ color: BLOOMBERG_PURPLE }}>💬 {post.replies}</span>
                  <span style={{ color: BLOOMBERG_GREEN }}>👍 {post.likes}</span>
                  <button style={{
                    color: BLOOMBERG_ORANGE,
                    backgroundColor: 'transparent',
                    border: 'none',
                    cursor: 'pointer',
                    fontSize: '8px'
                  }}>REPLY</button>
                  <button style={{
                    color: BLOOMBERG_BLUE,
                    backgroundColor: 'transparent',
                    border: 'none',
                    cursor: 'pointer',
                    fontSize: '8px'
                  }}>SHARE</button>
                </div>
              </div>
            ))}
          </div>

          {/* Right Panel - Activity & Trending */}
          <div style={{
            width: '280px',
            backgroundColor: BLOOMBERG_PANEL_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            padding: '4px',
            overflow: 'auto'
          }}>
            <div style={{ color: BLOOMBERG_ORANGE, fontSize: '11px', fontWeight: 'bold', marginBottom: '4px' }}>
              TRENDING TOPICS (1H)
            </div>
            <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '8px' }}></div>

            {[
              { topic: '#FED_RATES', mentions: 847, sentiment: 'BULLISH', change: '+234%' },
              { topic: '#BTC_ETF', mentions: 723, sentiment: 'BULLISH', change: '+189%' },
              { topic: '#NVDA', mentions: 612, sentiment: 'BULLISH', change: '+156%' },
              { topic: '#OIL_SUPPLY', mentions: 487, sentiment: 'BULLISH', change: '+123%' },
              { topic: '#BANKING_CRISIS', mentions: 356, sentiment: 'BEARISH', change: '+89%' },
              { topic: '#AI_REVENUE', mentions: 298, sentiment: 'BULLISH', change: '+67%' }
            ].map((item, index) => (
              <div key={index} style={{
                padding: '3px',
                marginBottom: '3px',
                backgroundColor: 'rgba(255,255,255,0.02)',
                fontSize: '9px'
              }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                  <span style={{ color: BLOOMBERG_YELLOW }}>{item.topic}</span>
                  <span style={{ color: BLOOMBERG_GREEN }}>{item.change}</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', color: BLOOMBERG_GRAY }}>
                  <span>{item.mentions} mentions</span>
                  <span style={{ color: getSentimentColor(item.sentiment) }}>{item.sentiment}</span>
                </div>
              </div>
            ))}

            <div style={{ borderTop: `1px solid ${BLOOMBERG_GRAY}`, marginTop: '12px', paddingTop: '8px' }}>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
                RECENT ACTIVITY
              </div>
              {[
                { user: 'QuantTrader_Pro', action: 'posted', target: 'Fed Rate Analysis', time: '2 min ago' },
                { user: 'CryptoWhale_2024', action: 'replied to', target: 'BTC ETF Thread', time: '5 min ago' },
                { user: 'OptionsFlow_Alert', action: 'liked', target: 'NVDA Earnings Post', time: '8 min ago' },
                { user: 'MacroHedgeFund', action: 'posted', target: 'Yield Curve Analysis', time: '12 min ago' },
                { user: 'TechInvestor88', action: 'replied to', target: 'AI Infrastructure', time: '15 min ago' }
              ].map((activity, index) => (
                <div key={index} style={{
                  padding: '2px',
                  marginBottom: '2px',
                  fontSize: '8px',
                  color: BLOOMBERG_GRAY,
                  lineHeight: '1.3'
                }}>
                  <span style={{ color: BLOOMBERG_CYAN }}>@{activity.user}</span>
                  {' '}<span>{activity.action}</span>{' '}
                  <span style={{ color: BLOOMBERG_WHITE }}>{activity.target}</span>
                  {' '}<span style={{ color: BLOOMBERG_YELLOW }}>• {activity.time}</span>
                </div>
              ))}
            </div>

            <div style={{ borderTop: `1px solid ${BLOOMBERG_GRAY}`, marginTop: '12px', paddingTop: '8px' }}>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
                SENTIMENT ANALYSIS
              </div>
              <div style={{ fontSize: '9px' }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>Bullish Posts:</span>
                  <span style={{ color: BLOOMBERG_GREEN }}>68%</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>Bearish Posts:</span>
                  <span style={{ color: BLOOMBERG_RED }}>18%</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>Neutral Posts:</span>
                  <span style={{ color: BLOOMBERG_YELLOW }}>14%</span>
                </div>
                <div style={{ borderTop: `1px solid ${BLOOMBERG_GRAY}`, marginTop: '4px', paddingTop: '4px' }}>
                  <span style={{ color: BLOOMBERG_CYAN }}>Overall Sentiment:</span>
                  <span style={{ color: BLOOMBERG_GREEN, fontWeight: 'bold', marginLeft: '4px' }}>+0.62 BULLISH</span>
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
            <span>Fincept Global Forum v2.4.1 | Professional trading community</span>
            <span>Posts: 894,234 | Users: 47,832 | Active: {onlineUsers}</span>
          </div>
          <div style={{ display: 'flex', gap: '16px' }}>
            <span>Category: {activeCategory}</span>
            <span>Sort: {sortBy}</span>
            <span style={{ color: BLOOMBERG_GREEN }}>STATUS: LIVE</span>
          </div>
        </div>
      </div>
    </div>
  );
};

export default ForumTab;