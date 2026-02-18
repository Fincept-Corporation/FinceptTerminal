// Dashboard Template Definitions
// Each template defines a pre-configured widget layout for a specific user profile.
// Shown once on first launch; user can change anytime via "TEMPLATE" toolbar button.

import { Layout } from 'react-grid-layout';
import { WidgetConfig } from './widgets';

export interface WidgetInstance extends WidgetConfig {
  layout: Layout;
}

export interface DashboardTemplate {
  id: string;
  name: string;
  description: string;
  tag: string;         // Short role label shown on card
  icon: string;        // Emoji icon for the card
  region: string;      // Target region/audience
  widgets: WidgetInstance[];
}

// ============================================================================
// Template 1 — Portfolio Manager
// ============================================================================
const portfolioManagerTemplate: DashboardTemplate = {
  id: 'portfolio-manager',
  name: 'Portfolio Manager',
  description: 'Track portfolio performance, risk exposure, macro data and regulatory news. Built for active portfolio managers overseeing multiple assets.',
  tag: 'BUY-SIDE',
  icon: '📊',
  region: 'Global',
  widgets: [
    { id: 't1-perf',    type: 'performance',  title: 'Performance Tracker',   config: {},                                        layout: { i: 't1-perf',    x: 0, y: 0,  w: 4, h: 5, minW: 3, minH: 4 } },
    { id: 't1-port',    type: 'portfolio',    title: 'Portfolio Summary',     config: {},                                        layout: { i: 't1-port',    x: 4, y: 0,  w: 4, h: 5, minW: 3, minH: 4 } },
    { id: 't1-risk',    type: 'riskmetrics',  title: 'Risk Metrics',          config: {},                                        layout: { i: 't1-risk',    x: 8, y: 0,  w: 4, h: 5, minW: 3, minH: 4 } },
    { id: 't1-idx',     type: 'indices',      title: 'Global Indices',        config: {},                                        layout: { i: 't1-idx',     x: 0, y: 5,  w: 4, h: 4, minW: 3, minH: 3 } },
    { id: 't1-forex',   type: 'forex',        title: 'Forex',                 config: {},                                        layout: { i: 't1-forex',   x: 4, y: 5,  w: 4, h: 4, minW: 3, minH: 3 } },
    { id: 't1-cal',     type: 'calendar',     title: 'Economic Calendar',     config: { country: 'US', limit: 10 },              layout: { i: 't1-cal',     x: 8, y: 5,  w: 4, h: 4, minW: 3, minH: 3 } },
    { id: 't1-news',    type: 'news',         title: 'Market & Regulatory News', config: { newsCategory: 'MARKETS', newsLimit: 8 }, layout: { i: 't1-news',    x: 0, y: 9,  w: 4, h: 5, minW: 2, minH: 3 } },
    { id: 't1-eco',     type: 'economic',     title: 'Economic Indicators',   config: {},                                        layout: { i: 't1-eco',     x: 4, y: 9,  w: 4, h: 5, minW: 2, minH: 3 } },
    { id: 't1-alerts',  type: 'watchlistalerts', title: 'Watchlist Alerts',  config: { alertThreshold: 3 },                     layout: { i: 't1-alerts',  x: 8, y: 9,  w: 4, h: 5, minW: 2, minH: 3 } },
  ]
};

// ============================================================================
// Template 2 — Hedge Fund Manager
// ============================================================================
const hedgeFundTemplate: DashboardTemplate = {
  id: 'hedge-fund',
  name: 'Hedge Fund Manager',
  description: 'Alpha generation, live signals, backtesting results, market sentiment and geopolitical risk. For sophisticated multi-strategy fund managers.',
  tag: 'ALPHA',
  icon: '🏦',
  region: 'Global',
  widgets: [
    { id: 't2-alpha',   type: 'alphaarena',   title: 'Alpha Arena',           config: {},                                        layout: { i: 't2-alpha',   x: 0, y: 0,  w: 4, h: 5, minW: 3, minH: 4 } },
    { id: 't2-signals', type: 'livesignals',  title: 'Live AI Signals',       config: {},                                        layout: { i: 't2-signals', x: 4, y: 0,  w: 4, h: 5, minW: 3, minH: 4 } },
    { id: 't2-risk',    type: 'riskmetrics',  title: 'Risk Metrics',          config: {},                                        layout: { i: 't2-risk',    x: 8, y: 0,  w: 4, h: 5, minW: 3, minH: 4 } },
    { id: 't2-bt',      type: 'backtestsummary', title: 'Backtest Summary',   config: {},                                        layout: { i: 't2-bt',      x: 0, y: 5,  w: 4, h: 4, minW: 3, minH: 3 } },
    { id: 't2-sent',    type: 'sentiment',    title: 'Market Sentiment',      config: {},                                        layout: { i: 't2-sent',    x: 4, y: 5,  w: 4, h: 4, minW: 3, minH: 3 } },
    { id: 't2-movers',  type: 'topmovers',    title: 'Top Movers',            config: {},                                        layout: { i: 't2-movers',  x: 8, y: 5,  w: 4, h: 4, minW: 3, minH: 3 } },
    { id: 't2-news',    type: 'news',         title: 'Markets & Earnings News', config: { newsCategory: 'MARKETS', newsLimit: 8 }, layout: { i: 't2-news',    x: 0, y: 9,  w: 4, h: 5, minW: 2, minH: 3 } },
    { id: 't2-geo',     type: 'geopolitics',  title: 'Geopolitical Risk',     config: {},                                        layout: { i: 't2-geo',     x: 4, y: 9,  w: 4, h: 5, minW: 2, minH: 3 } },
    { id: 't2-screen',  type: 'screener',     title: 'Stock Screener',        config: { screenerPreset: 'momentum' },            layout: { i: 't2-screen',  x: 8, y: 9,  w: 4, h: 5, minW: 2, minH: 3 } },
  ]
};

// ============================================================================
// Template 3 — Crypto Trader
// ============================================================================
const cryptoTraderTemplate: DashboardTemplate = {
  id: 'crypto-trader',
  name: 'Crypto Trader',
  description: 'Real-time crypto markets, Polymarket prediction feeds, sentiment and breaking crypto news. For full-time digital asset traders.',
  tag: 'CRYPTO',
  icon: '₿',
  region: 'Global',
  widgets: [
    { id: 't3-crypto',  type: 'crypto',       title: 'Cryptocurrency Markets', config: {},                                       layout: { i: 't3-crypto',  x: 0, y: 0,  w: 8, h: 5, minW: 4, minH: 4 } },
    { id: 't3-sent',    type: 'sentiment',    title: 'Market Sentiment',       config: {},                                       layout: { i: 't3-sent',    x: 8, y: 0,  w: 4, h: 5, minW: 3, minH: 4 } },
    { id: 't3-poly',    type: 'polymarket',   title: 'Polymarket Predictions', config: {},                                       layout: { i: 't3-poly',    x: 0, y: 5,  w: 4, h: 4, minW: 3, minH: 3 } },
    { id: 't3-qt',      type: 'quicktrade',   title: 'Quick Trade',            config: {},                                       layout: { i: 't3-qt',      x: 4, y: 5,  w: 4, h: 4, minW: 3, minH: 3 } },
    { id: 't3-watch',   type: 'watchlist',    title: 'My Watchlist',           config: { watchlistName: 'Default' },             layout: { i: 't3-watch',   x: 8, y: 5,  w: 4, h: 4, minW: 2, minH: 3 } },
    { id: 't3-news',    type: 'news',         title: 'Crypto News',            config: { newsCategory: 'CRYPTO', newsLimit: 10 }, layout: { i: 't3-news',    x: 0, y: 9,  w: 6, h: 5, minW: 3, minH: 3 } },
    { id: 't3-forex',   type: 'forex',        title: 'Forex',                  config: {},                                       layout: { i: 't3-forex',   x: 6, y: 9,  w: 6, h: 5, minW: 3, minH: 3 } },
  ]
};

// ============================================================================
// Template 4 — Equity / Day Trader
// ============================================================================
const equityTraderTemplate: DashboardTemplate = {
  id: 'equity-trader',
  name: 'Equity Trader',
  description: 'Intraday stock moves, watchlist alerts, quick trade execution, top movers and earnings news. For active equity and day traders.',
  tag: 'EQUITY',
  icon: '📈',
  region: 'Global',
  widgets: [
    { id: 't4-quote',   type: 'stockquote',   title: 'Stock Quote',           config: { stockSymbol: 'AAPL' },                   layout: { i: 't4-quote',   x: 0, y: 0,  w: 4, h: 5, minW: 3, minH: 4 } },
    { id: 't4-movers',  type: 'topmovers',    title: 'Top Movers',            config: {},                                        layout: { i: 't4-movers',  x: 4, y: 0,  w: 4, h: 5, minW: 3, minH: 4 } },
    { id: 't4-qt',      type: 'quicktrade',   title: 'Quick Trade',           config: {},                                        layout: { i: 't4-qt',      x: 8, y: 0,  w: 4, h: 5, minW: 3, minH: 4 } },
    { id: 't4-watch',   type: 'watchlist',    title: 'My Watchlist',          config: { watchlistName: 'Default' },              layout: { i: 't4-watch',   x: 0, y: 5,  w: 4, h: 4, minW: 3, minH: 3 } },
    { id: 't4-alerts',  type: 'watchlistalerts', title: 'Watchlist Alerts',  config: { alertThreshold: 2 },                     layout: { i: 't4-alerts',  x: 4, y: 5,  w: 4, h: 4, minW: 3, minH: 3 } },
    { id: 't4-screen',  type: 'screener',     title: 'Momentum Screener',    config: { screenerPreset: 'momentum' },             layout: { i: 't4-screen',  x: 8, y: 5,  w: 4, h: 4, minW: 3, minH: 3 } },
    { id: 't4-idx',     type: 'indices',      title: 'Market Indices',        config: {},                                        layout: { i: 't4-idx',     x: 0, y: 9,  w: 4, h: 5, minW: 2, minH: 3 } },
    { id: 't4-news',    type: 'news',         title: 'Markets & Earnings',    config: { newsCategory: 'MARKETS', newsLimit: 8 }, layout: { i: 't4-news',    x: 4, y: 9,  w: 8, h: 5, minW: 4, minH: 3 } },
  ]
};

// ============================================================================
// Template 5 — Macro Economist
// ============================================================================
const macroEconomistTemplate: DashboardTemplate = {
  id: 'macro-economist',
  name: 'Macro Economist',
  description: 'Central bank feeds, economic indicators, DBnomics data, global indices and forex. For economists, researchers and macro analysts.',
  tag: 'MACRO',
  icon: '🌐',
  region: 'Global',
  widgets: [
    { id: 't5-eco',     type: 'economic',     title: 'Economic Indicators',   config: {},                                        layout: { i: 't5-eco',     x: 0, y: 0,  w: 4, h: 5, minW: 3, minH: 4 } },
    { id: 't5-cal',     type: 'calendar',     title: 'Economic Calendar',     config: { country: 'US', limit: 12 },              layout: { i: 't5-cal',     x: 4, y: 0,  w: 4, h: 5, minW: 3, minH: 4 } },
    { id: 't5-dbn',     type: 'dbnomics',     title: 'DBnomics — Fed Funds', config: { dbnomicsSeriesId: 'FRED/FEDFUNDS' },      layout: { i: 't5-dbn',     x: 8, y: 0,  w: 4, h: 5, minW: 3, minH: 4 } },
    { id: 't5-idx',     type: 'indices',      title: 'Global Indices',        config: {},                                        layout: { i: 't5-idx',     x: 0, y: 5,  w: 4, h: 4, minW: 3, minH: 3 } },
    { id: 't5-forex',   type: 'forex',        title: 'Forex Major Pairs',     config: {},                                        layout: { i: 't5-forex',   x: 4, y: 5,  w: 4, h: 4, minW: 3, minH: 3 } },
    { id: 't5-comm',    type: 'commodities',  title: 'Commodities',           config: {},                                        layout: { i: 't5-comm',    x: 8, y: 5,  w: 4, h: 4, minW: 3, minH: 3 } },
    { id: 't5-news',    type: 'news',         title: 'Economic & Policy News', config: { newsCategory: 'MARKETS', newsLimit: 8 }, layout: { i: 't5-news',    x: 0, y: 9,  w: 4, h: 5, minW: 2, minH: 3 } },
    { id: 't5-geo',     type: 'geopolitics',  title: 'Geopolitical Risk',     config: {},                                        layout: { i: 't5-geo',     x: 4, y: 9,  w: 4, h: 5, minW: 2, minH: 3 } },
    { id: 't5-heat',    type: 'heatmap',      title: 'Sector Heatmap',        config: {},                                        layout: { i: 't5-heat',    x: 8, y: 9,  w: 4, h: 5, minW: 2, minH: 3 } },
  ]
};

// ============================================================================
// Template 6 — Geopolitics / Intelligence Analyst
// ============================================================================
const geopoliticsAnalystTemplate: DashboardTemplate = {
  id: 'geopolitics-analyst',
  name: 'Geopolitics Analyst',
  description: 'Country risk, supply chain monitoring, maritime tracking, China markets and geopolitical intelligence feeds.',
  tag: 'INTEL',
  icon: '🗺️',
  region: 'Global',
  widgets: [
    { id: 't6-geo',     type: 'geopolitics',  title: 'Geopolitical Risk',     config: {},                                        layout: { i: 't6-geo',     x: 0, y: 0,  w: 4, h: 5, minW: 3, minH: 4 } },
    { id: 't6-maritime',type: 'maritime',     title: 'Maritime Tracking',     config: {},                                        layout: { i: 't6-maritime', x: 4, y: 0,  w: 4, h: 5, minW: 3, minH: 4 } },
    { id: 't6-ak',      type: 'akshare',      title: 'China Markets',         config: {},                                        layout: { i: 't6-ak',      x: 8, y: 0,  w: 4, h: 5, minW: 3, minH: 4 } },
    { id: 't6-idx',     type: 'indices',      title: 'Global Indices',        config: {},                                        layout: { i: 't6-idx',     x: 0, y: 5,  w: 4, h: 4, minW: 3, minH: 3 } },
    { id: 't6-forex',   type: 'forex',        title: 'Forex',                 config: {},                                        layout: { i: 't6-forex',   x: 4, y: 5,  w: 4, h: 4, minW: 3, minH: 3 } },
    { id: 't6-poly',    type: 'polymarket',   title: 'Polymarket',            config: {},                                        layout: { i: 't6-poly',    x: 8, y: 5,  w: 4, h: 4, minW: 3, minH: 3 } },
    { id: 't6-news',    type: 'news',         title: 'Geopolitics News',      config: { newsCategory: 'GEOPOLITICS', newsLimit: 10 }, layout: { i: 't6-news', x: 0, y: 9, w: 6, h: 5, minW: 3, minH: 3 } },
    { id: 't6-comm',    type: 'commodities',  title: 'Commodities',           config: {},                                        layout: { i: 't6-comm',    x: 6, y: 9,  w: 6, h: 5, minW: 3, minH: 3 } },
  ]
};

// ============================================================================
// Template 7 — Individual Investor (India)
// ============================================================================
const indiaInvestorTemplate: DashboardTemplate = {
  id: 'india-investor',
  name: 'Individual Investor — India',
  description: 'NSE/BSE indices, Indian market news, economic calendar and personal portfolio tracker. Designed for Indian retail investors.',
  tag: 'INDIA',
  icon: '🇮🇳',
  region: 'India',
  widgets: [
    { id: 't7-idx',     type: 'indices',      title: 'Market Indices',        config: {},                                        layout: { i: 't7-idx',     x: 0, y: 0,  w: 4, h: 5, minW: 3, minH: 4 } },
    { id: 't7-perf',    type: 'performance',  title: 'My Portfolio',          config: {},                                        layout: { i: 't7-perf',    x: 4, y: 0,  w: 4, h: 5, minW: 3, minH: 4 } },
    { id: 't7-port',    type: 'portfolio',    title: 'Portfolio Summary',     config: {},                                        layout: { i: 't7-port',    x: 8, y: 0,  w: 4, h: 5, minW: 3, minH: 4 } },
    { id: 't7-watch',   type: 'watchlist',    title: 'My Watchlist',          config: { watchlistName: 'Default' },              layout: { i: 't7-watch',   x: 0, y: 5,  w: 4, h: 4, minW: 3, minH: 3 } },
    { id: 't7-quote',   type: 'stockquote',   title: 'Stock Quote',           config: { stockSymbol: 'RELIANCE.NS' },            layout: { i: 't7-quote',   x: 4, y: 5,  w: 4, h: 4, minW: 3, minH: 3 } },
    { id: 't7-cal',     type: 'calendar',     title: 'Economic Calendar',     config: { country: 'IN', limit: 10 },              layout: { i: 't7-cal',     x: 8, y: 5,  w: 4, h: 4, minW: 3, minH: 3 } },
    { id: 't7-news',    type: 'news',         title: 'Market News',           config: { newsCategory: 'MARKETS', newsLimit: 8 }, layout: { i: 't7-news',    x: 0, y: 9,  w: 4, h: 5, minW: 2, minH: 3 } },
    { id: 't7-eco',     type: 'economic',     title: 'Economic Indicators',   config: {},                                        layout: { i: 't7-eco',     x: 4, y: 9,  w: 4, h: 5, minW: 2, minH: 3 } },
    { id: 't7-comm',    type: 'commodities',  title: 'Commodities (Gold/Oil)', config: {},                                       layout: { i: 't7-comm',    x: 8, y: 9,  w: 4, h: 5, minW: 2, minH: 3 } },
  ]
};

// ============================================================================
// Template 8 — Individual Investor (US)
// ============================================================================
const usInvestorTemplate: DashboardTemplate = {
  id: 'us-investor',
  name: 'Individual Investor — USA',
  description: 'S&P 500, NASDAQ, Dow Jones, earnings news, stock screener and personal portfolio. For US-based retail investors.',
  tag: 'USA',
  icon: '🇺🇸',
  region: 'United States',
  widgets: [
    { id: 't8-idx',     type: 'indices',      title: 'US Indices',            config: {},                                        layout: { i: 't8-idx',     x: 0, y: 0,  w: 4, h: 5, minW: 3, minH: 4 } },
    { id: 't8-perf',    type: 'performance',  title: 'My Portfolio',          config: {},                                        layout: { i: 't8-perf',    x: 4, y: 0,  w: 4, h: 5, minW: 3, minH: 4 } },
    { id: 't8-screen',  type: 'screener',     title: 'Value Screener',        config: { screenerPreset: 'value' },               layout: { i: 't8-screen',  x: 8, y: 0,  w: 4, h: 5, minW: 3, minH: 4 } },
    { id: 't8-quote',   type: 'stockquote',   title: 'Stock Quote',           config: { stockSymbol: 'AAPL' },                   layout: { i: 't8-quote',   x: 0, y: 5,  w: 4, h: 4, minW: 3, minH: 3 } },
    { id: 't8-watch',   type: 'watchlist',    title: 'My Watchlist',          config: { watchlistName: 'Default' },              layout: { i: 't8-watch',   x: 4, y: 5,  w: 4, h: 4, minW: 3, minH: 3 } },
    { id: 't8-cal',     type: 'calendar',     title: 'Economic Calendar',     config: { country: 'US', limit: 10 },              layout: { i: 't8-cal',     x: 8, y: 5,  w: 4, h: 4, minW: 3, minH: 3 } },
    { id: 't8-news',    type: 'news',         title: 'Markets & Earnings',    config: { newsCategory: 'MARKETS', newsLimit: 8 }, layout: { i: 't8-news',    x: 0, y: 9,  w: 4, h: 5, minW: 2, minH: 3 } },
    { id: 't8-eco',     type: 'economic',     title: 'Economic Indicators',   config: {},                                        layout: { i: 't8-eco',     x: 4, y: 9,  w: 4, h: 5, minW: 2, minH: 3 } },
    { id: 't8-port',    type: 'portfolio',    title: 'Portfolio Summary',     config: {},                                        layout: { i: 't8-port',    x: 8, y: 9,  w: 4, h: 5, minW: 2, minH: 3 } },
  ]
};

// ============================================================================
// Template 9 — Individual Investor (China / Asia)
// ============================================================================
const asiaInvestorTemplate: DashboardTemplate = {
  id: 'asia-investor',
  name: 'Individual Investor — Asia',
  description: 'China A-shares, Asian indices, AkShare data, forex and geopolitical risk. For investors focused on Chinese and broader Asian markets.',
  tag: 'ASIA',
  icon: '🇨🇳',
  region: 'China / Asia',
  widgets: [
    { id: 't9-ak',      type: 'akshare',      title: 'China Markets',         config: {},                                        layout: { i: 't9-ak',      x: 0, y: 0,  w: 4, h: 5, minW: 3, minH: 4 } },
    { id: 't9-idx',     type: 'indices',      title: 'Global Indices',        config: {},                                        layout: { i: 't9-idx',     x: 4, y: 0,  w: 4, h: 5, minW: 3, minH: 4 } },
    { id: 't9-perf',    type: 'performance',  title: 'My Portfolio',          config: {},                                        layout: { i: 't9-perf',    x: 8, y: 0,  w: 4, h: 5, minW: 3, minH: 4 } },
    { id: 't9-forex',   type: 'forex',        title: 'Forex',                 config: {},                                        layout: { i: 't9-forex',   x: 0, y: 5,  w: 4, h: 4, minW: 3, minH: 3 } },
    { id: 't9-geo',     type: 'geopolitics',  title: 'Geopolitical Risk',     config: {},                                        layout: { i: 't9-geo',     x: 4, y: 5,  w: 4, h: 4, minW: 3, minH: 3 } },
    { id: 't9-cal',     type: 'calendar',     title: 'Economic Calendar',     config: { country: 'CN', limit: 10 },              layout: { i: 't9-cal',     x: 8, y: 5,  w: 4, h: 4, minW: 3, minH: 3 } },
    { id: 't9-eco',     type: 'economic',     title: 'Economic Indicators',   config: {},                                        layout: { i: 't9-eco',     x: 0, y: 9,  w: 4, h: 5, minW: 2, minH: 3 } },
    { id: 't9-news',    type: 'news',         title: 'Market News',           config: { newsCategory: 'MARKETS', newsLimit: 8 }, layout: { i: 't9-news',    x: 4, y: 9,  w: 4, h: 5, minW: 2, minH: 3 } },
    { id: 't9-comm',    type: 'commodities',  title: 'Commodities',           config: {},                                        layout: { i: 't9-comm',    x: 8, y: 9,  w: 4, h: 5, minW: 2, minH: 3 } },
  ]
};

// ============================================================================
// Template 10 — Blank / Custom
// ============================================================================
const blankTemplate: DashboardTemplate = {
  id: 'custom',
  name: 'Custom / Blank',
  description: 'Start with an empty dashboard and build your own layout from scratch. Add any widgets you need from the widget library.',
  tag: 'CUSTOM',
  icon: '⚙️',
  region: 'Global',
  widgets: []
};

// ============================================================================
// All Templates (exported)
// ============================================================================
export const DASHBOARD_TEMPLATES: DashboardTemplate[] = [
  portfolioManagerTemplate,
  hedgeFundTemplate,
  cryptoTraderTemplate,
  equityTraderTemplate,
  macroEconomistTemplate,
  geopoliticsAnalystTemplate,
  indiaInvestorTemplate,
  usInvestorTemplate,
  asiaInvestorTemplate,
  blankTemplate,
];

export const TEMPLATE_PICKER_SEEN_KEY = 'dashboard-template-picker-seen';
export const SELECTED_TEMPLATE_KEY   = 'dashboard-selected-template';
