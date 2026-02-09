/**
 * Stock Broker Registry
 *
 * Central registry for all stock brokers globally
 * Enables broker discovery, switching, and capability detection
 */

import type { StockBrokerMetadata, Region, IStockBrokerAdapter } from './types';

// ============================================================================
// NIFTY 50 COMPONENTS (Jan 2026)
// ============================================================================

const NIFTY_50_SYMBOLS = [
  // Financials (~35% weight)
  'HDFCBANK', 'ICICIBANK', 'SBIN', 'KOTAKBANK', 'AXISBANK',
  'BAJFINANCE', 'BAJAJFINSV', 'HDFCLIFE', 'SBILIFE', 'INDUSINDBK',
  // IT (~13% weight)
  'TCS', 'INFY', 'WIPRO', 'HCLTECH', 'TECHM', 'LTIM',
  // Oil & Gas / Energy (~12% weight)
  'RELIANCE', 'ONGC', 'NTPC', 'POWERGRID', 'ADANIPORTS', 'ADANIENT',
  // Auto (~7% weight)
  'MARUTI', 'TATAMOTORS', 'M&M', 'BAJAJ-AUTO', 'HEROMOTOCO', 'EICHERMOT',
  // FMCG (~8% weight)
  'HINDUNILVR', 'ITC', 'NESTLEIND', 'BRITANNIA', 'TATACONSUM',
  // Metals & Mining (~4% weight)
  'TATASTEEL', 'JSWSTEEL', 'HINDALCO', 'COALINDIA',
  // Pharma & Healthcare (~4% weight)
  'SUNPHARMA', 'DRREDDY', 'CIPLA', 'APOLLOHOSP', 'DIVISLAB',
  // Infrastructure & Others (~17% weight)
  'LT', 'BHARTIARTL', 'ULTRACEMCO', 'GRASIM', 'TITAN',
  'ASIANPAINT', 'BPCL', 'SHRIRAMFIN', 'TRENT',
];

// ============================================================================
// BROKER REGISTRY
// ============================================================================

export const STOCK_BROKER_REGISTRY: Record<string, StockBrokerMetadata> = {
  // ============================================================================
  // INDIAN BROKERS
  // ============================================================================

  zerodha: {
    id: 'zerodha',
    name: 'zerodha',
    displayName: 'Zerodha (Kite)',
    logo: 'https://fincept.in/brokers/zerodha.png',
    website: 'https://zerodha.com',

    region: 'india',
    country: 'IN',
    currency: 'INR',
    exchanges: ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'MCX'],
    marketHours: {
      open: '09:15',
      close: '15:30',
      timezone: 'Asia/Kolkata',
      preMarketOpen: '09:00',
      preMarketClose: '09:08',
    },

    features: {
      webSocket: true,
      amo: true,
      gtt: true,
      bracketOrder: true,
      coverOrder: true,
      marginCalculator: true,
      optionsChain: true,
      paperTrading: false,
    },

    tradingFeatures: {
      marketOrders: true,
      limitOrders: true,
      stopOrders: true,
      stopLimitOrders: true,
      trailingStopOrders: false,
    },

    productTypes: ['CASH', 'INTRADAY', 'MARGIN'],

    authType: 'oauth',

    rateLimit: {
      ordersPerSecond: 10,
      quotesPerSecond: 5,
    },

    fees: {
      equity: { brokerage: 0, stt: 0.001 },
      intraday: { brokerage: 0.0003 },
      fno: { brokerage: 20 },
    },

    defaultSymbols: NIFTY_50_SYMBOLS,
  },

  fyers: {
    id: 'fyers',
    name: 'fyers',
    displayName: 'Fyers',
    logo: 'https://fincept.in/brokers/fyers.png',
    website: 'https://fyers.in',

    region: 'india',
    country: 'IN',
    currency: 'INR',
    exchanges: ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'MCX'],
    marketHours: {
      open: '09:15',
      close: '15:30',
      timezone: 'Asia/Kolkata',
      preMarketOpen: '09:00',
      preMarketClose: '09:08',
    },

    features: {
      webSocket: true,
      amo: true,
      gtt: false,
      bracketOrder: true,
      coverOrder: true,
      marginCalculator: false,
      optionsChain: true,
      paperTrading: false,
    },

    tradingFeatures: {
      marketOrders: true,
      limitOrders: true,
      stopOrders: true,
      stopLimitOrders: true,
      trailingStopOrders: false,
    },

    productTypes: ['CASH', 'INTRADAY', 'MARGIN'],

    authType: 'oauth',

    rateLimit: {
      ordersPerSecond: 10,
      quotesPerSecond: 5,
    },

    fees: {
      equity: { brokerage: 0, stt: 0.001 },
      intraday: { brokerage: 0.0003 },
      fno: { brokerage: 20 },
    },

    defaultSymbols: NIFTY_50_SYMBOLS,
  },

  angelone: {
    id: 'angelone',
    name: 'angelone',
    displayName: 'Angel One',
    logo: 'https://fincept.in/brokers/angelone.png',
    website: 'https://www.angelone.in',

    region: 'india',
    country: 'IN',
    currency: 'INR',
    exchanges: ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'MCX'],
    marketHours: {
      open: '09:15',
      close: '15:30',
      timezone: 'Asia/Kolkata',
      preMarketOpen: '09:00',
      preMarketClose: '09:08',
    },

    features: {
      webSocket: true,
      amo: true,
      gtt: true,
      bracketOrder: true,
      coverOrder: true,
      marginCalculator: true,
      optionsChain: true,
      paperTrading: false,
    },

    tradingFeatures: {
      marketOrders: true,
      limitOrders: true,
      stopOrders: true,
      stopLimitOrders: true,
      trailingStopOrders: false,
    },

    productTypes: ['CASH', 'INTRADAY', 'MARGIN'],

    authType: 'totp',

    rateLimit: {
      ordersPerSecond: 10,
      quotesPerSecond: 1, // Angel One has 1 request per second for quotes
    },

    fees: {
      equity: { brokerage: 0, stt: 0.001 },
      intraday: { brokerage: 20 },
      fno: { brokerage: 20 },
    },

    defaultSymbols: NIFTY_50_SYMBOLS,
  },

  upstox: {
    id: 'upstox',
    name: 'upstox',
    displayName: 'Upstox',
    logo: 'https://fincept.in/brokers/upstox.png',
    website: 'https://upstox.com',

    region: 'india',
    country: 'IN',
    currency: 'INR',
    exchanges: ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'MCX'],
    marketHours: {
      open: '09:15',
      close: '15:30',
      timezone: 'Asia/Kolkata',
      preMarketOpen: '09:00',
      preMarketClose: '09:08',
    },

    features: {
      webSocket: true,
      amo: true,
      gtt: true,
      bracketOrder: false,
      coverOrder: false,
      marginCalculator: true,
      optionsChain: true,
      paperTrading: false,
    },

    tradingFeatures: {
      marketOrders: true,
      limitOrders: true,
      stopOrders: true,
      stopLimitOrders: true,
      trailingStopOrders: false,
    },

    productTypes: ['CASH', 'INTRADAY', 'MARGIN'],

    authType: 'oauth',

    rateLimit: {
      ordersPerSecond: 10,
      quotesPerSecond: 10,
    },

    fees: {
      equity: { brokerage: 0, stt: 0.001 },
      intraday: { brokerage: 20 },
      fno: { brokerage: 20 },
    },

    defaultSymbols: NIFTY_50_SYMBOLS,
  },

  dhan: {
    id: 'dhan',
    name: 'dhan',
    displayName: 'Dhan',
    logo: 'https://fincept.in/brokers/dhan.png',
    website: 'https://dhan.co',

    region: 'india',
    country: 'IN',
    currency: 'INR',
    exchanges: ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'MCX'],
    marketHours: {
      open: '09:15',
      close: '15:30',
      timezone: 'Asia/Kolkata',
      preMarketOpen: '09:00',
      preMarketClose: '09:08',
    },

    features: {
      webSocket: true,
      amo: true,
      gtt: true,
      bracketOrder: false,
      coverOrder: false,
      marginCalculator: true,
      optionsChain: true,
      paperTrading: false,
    },

    tradingFeatures: {
      marketOrders: true,
      limitOrders: true,
      stopOrders: true,
      stopLimitOrders: true,
      trailingStopOrders: false,
    },

    productTypes: ['CASH', 'INTRADAY', 'MARGIN'],

    authType: 'oauth',

    rateLimit: {
      ordersPerSecond: 10,
      quotesPerSecond: 1, // Dhan has strict rate limits
    },

    fees: {
      equity: { brokerage: 0, stt: 0.001 },
      intraday: { brokerage: 20 },
      fno: { brokerage: 20 },
    },

    defaultSymbols: NIFTY_50_SYMBOLS,
  },

  kotak: {
    id: 'kotak',
    name: 'kotak',
    displayName: 'Kotak Neo',
    logo: 'https://fincept.in/brokers/kotak.png',
    website: 'https://www.kotaksecurities.com',

    region: 'india',
    country: 'IN',
    currency: 'INR',
    exchanges: ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'BCD', 'MCX'],
    marketHours: {
      open: '09:15',
      close: '15:30',
      timezone: 'Asia/Kolkata',
      preMarketOpen: '09:00',
      preMarketClose: '09:08',
    },

    features: {
      webSocket: true,
      amo: true,
      gtt: false, // Kotak Neo doesn't support GTT
      bracketOrder: true,
      coverOrder: true,
      marginCalculator: true,
      optionsChain: true,
      paperTrading: false,
    },

    tradingFeatures: {
      marketOrders: true,
      limitOrders: true,
      stopOrders: true,
      stopLimitOrders: true,
      trailingStopOrders: false,
    },

    productTypes: ['CASH', 'INTRADAY', 'MARGIN'],

    authType: 'totp', // Kotak uses TOTP + MPIN

    rateLimit: {
      ordersPerSecond: 10,
      quotesPerSecond: 5,
    },

    fees: {
      equity: { brokerage: 0, stt: 0.001 },
      intraday: { brokerage: 20 },
      fno: { brokerage: 20 },
    },

    defaultSymbols: NIFTY_50_SYMBOLS,
  },

  groww: {
    id: 'groww',
    name: 'groww',
    displayName: 'Groww',
    logo: 'https://fincept.in/brokers/groww.png',
    website: 'https://groww.in',

    region: 'india',
    country: 'IN',
    currency: 'INR',
    exchanges: ['NSE', 'BSE', 'NFO', 'BFO'],
    marketHours: {
      open: '09:15',
      close: '15:30',
      timezone: 'Asia/Kolkata',
      preMarketOpen: '09:00',
      preMarketClose: '09:08',
    },

    features: {
      webSocket: false, // Groww uses REST API polling
      amo: true,
      gtt: false,
      bracketOrder: false,
      coverOrder: false,
      marginCalculator: false,
      optionsChain: true,
      paperTrading: false,
    },

    tradingFeatures: {
      marketOrders: true,
      limitOrders: true,
      stopOrders: true,
      stopLimitOrders: true,
      trailingStopOrders: false,
    },

    productTypes: ['CASH', 'INTRADAY', 'MARGIN'],

    authType: 'totp',

    rateLimit: {
      ordersPerSecond: 10,
      quotesPerSecond: 5,
    },

    fees: {
      equity: { brokerage: 0, stt: 0.001 },
      intraday: { brokerage: 20 },
      fno: { brokerage: 20 },
    },

    defaultSymbols: NIFTY_50_SYMBOLS,
  },

  aliceblue: {
    id: 'aliceblue',
    name: 'aliceblue',
    displayName: 'Alice Blue',
    logo: 'https://fincept.in/brokers/aliceblue.png',
    website: 'https://aliceblueonline.com',

    region: 'india',
    country: 'IN',
    currency: 'INR',
    exchanges: ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'MCX'],
    marketHours: {
      open: '09:15',
      close: '15:30',
      timezone: 'Asia/Kolkata',
      preMarketOpen: '09:00',
      preMarketClose: '09:08',
    },

    features: {
      webSocket: true,
      amo: true,
      gtt: false,
      bracketOrder: true,
      coverOrder: true,
      marginCalculator: true,
      optionsChain: true,
      paperTrading: false,
    },

    tradingFeatures: {
      marketOrders: true,
      limitOrders: true,
      stopOrders: true,
      stopLimitOrders: true,
      trailingStopOrders: false,
    },

    productTypes: ['CASH', 'INTRADAY', 'MARGIN'],

    authType: 'enckey', // SHA256 checksum-based auth with encryption key

    rateLimit: {
      ordersPerSecond: 10,
      quotesPerSecond: 5,
    },

    fees: {
      equity: { brokerage: 15, stt: 0.001 },
      intraday: { brokerage: 15 },
      fno: { brokerage: 15 },
    },

    defaultSymbols: NIFTY_50_SYMBOLS,
  },

  fivepaisa: {
    id: 'fivepaisa',
    name: 'fivepaisa',
    displayName: '5Paisa',
    logo: 'https://fincept.in/brokers/5paisa.png',
    website: 'https://www.5paisa.com',

    region: 'india',
    country: 'IN',
    currency: 'INR',
    exchanges: ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'MCX'],
    marketHours: {
      open: '09:15',
      close: '15:30',
      timezone: 'Asia/Kolkata',
      preMarketOpen: '09:00',
      preMarketClose: '09:08',
    },

    features: {
      webSocket: true,
      amo: true,
      gtt: false,
      bracketOrder: true,
      coverOrder: true,
      marginCalculator: true,
      optionsChain: true,
      paperTrading: false,
    },

    tradingFeatures: {
      marketOrders: true,
      limitOrders: true,
      stopOrders: true,
      stopLimitOrders: true,
      trailingStopOrders: false,
    },

    productTypes: ['CASH', 'INTRADAY', 'MARGIN'],

    authType: 'totp', // Two-step TOTP authentication

    rateLimit: {
      ordersPerSecond: 10,
      quotesPerSecond: 5,
    },

    fees: {
      equity: { brokerage: 20, stt: 0.001 },
      intraday: { brokerage: 20 },
      fno: { brokerage: 20 },
    },

    defaultSymbols: NIFTY_50_SYMBOLS,
  },

  iifl: {
    id: 'iifl',
    name: 'iifl',
    displayName: 'IIFL Securities',
    logo: 'https://fincept.in/brokers/iiflsec.png',
    website: 'https://www.iifl.com',

    region: 'india',
    country: 'IN',
    currency: 'INR',
    exchanges: ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'MCX'],
    marketHours: {
      open: '09:15',
      close: '15:30',
      timezone: 'Asia/Kolkata',
      preMarketOpen: '09:00',
      preMarketClose: '09:08',
    },

    features: {
      webSocket: true,
      amo: true,
      gtt: false,
      bracketOrder: true,
      coverOrder: true,
      marginCalculator: true,
      optionsChain: true,
      paperTrading: false,
    },

    tradingFeatures: {
      marketOrders: true,
      limitOrders: true,
      stopOrders: true,
      stopLimitOrders: true,
      trailingStopOrders: false,
    },

    productTypes: ['CASH', 'INTRADAY', 'MARGIN'],

    authType: 'oauth', // API Key + Secret authentication

    rateLimit: {
      ordersPerSecond: 10,
      quotesPerSecond: 50, // XTS allows 50 instruments per request
    },

    fees: {
      equity: { brokerage: 20, stt: 0.001 },
      intraday: { brokerage: 20 },
      fno: { brokerage: 20 },
    },

    defaultSymbols: NIFTY_50_SYMBOLS,
  },

  motilal: {
    id: 'motilal',
    name: 'motilal',
    displayName: 'Motilal Oswal',
    logo: 'https://fincept.in/brokers/motilaloswal.png',
    website: 'https://www.motilaloswal.com',

    region: 'india',
    country: 'IN',
    currency: 'INR',
    exchanges: ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'MCX'],
    marketHours: {
      open: '09:15',
      close: '15:30',
      timezone: 'Asia/Kolkata',
      preMarketOpen: '09:00',
      preMarketClose: '09:08',
    },

    features: {
      webSocket: true,
      amo: true,
      gtt: false,
      bracketOrder: false,
      coverOrder: false,
      marginCalculator: true,
      optionsChain: true,
      paperTrading: false,
    },

    tradingFeatures: {
      marketOrders: true,
      limitOrders: true,
      stopOrders: true,
      stopLimitOrders: true,
      trailingStopOrders: false,
    },

    productTypes: ['CASH', 'INTRADAY', 'MARGIN'],

    authType: 'password', // Password + 2FA + optional TOTP

    rateLimit: {
      ordersPerSecond: 10,
      quotesPerSecond: 20,
    },

    fees: {
      equity: { brokerage: 20, stt: 0.001 },
      intraday: { brokerage: 20 },
      fno: { brokerage: 20 },
    },

    defaultSymbols: NIFTY_50_SYMBOLS,
  },

  shoonya: {
    id: 'shoonya',
    name: 'shoonya',
    displayName: 'Shoonya (Finvasia)',
    logo: 'https://fincept.in/brokers/shoonya.png',
    website: 'https://shoonya.com',

    region: 'india',
    country: 'IN',
    currency: 'INR',
    exchanges: ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'MCX'],
    marketHours: {
      open: '09:15',
      close: '15:30',
      timezone: 'Asia/Kolkata',
      preMarketOpen: '09:00',
      preMarketClose: '09:08',
    },

    features: {
      webSocket: true,
      amo: true,
      gtt: false,
      bracketOrder: true,
      coverOrder: true,
      marginCalculator: true,
      optionsChain: true,
      paperTrading: false,
    },

    tradingFeatures: {
      marketOrders: true,
      limitOrders: true,
      stopOrders: true,
      stopLimitOrders: true,
      trailingStopOrders: false,
    },

    productTypes: ['CASH', 'INTRADAY', 'MARGIN'],

    authType: 'totp', // TOTP-based authentication with SHA256

    rateLimit: {
      ordersPerSecond: 10,
      quotesPerSecond: 10,
    },

    fees: {
      equity: { brokerage: 0, stt: 0.001 },   // Zero brokerage
      intraday: { brokerage: 0 },              // Zero brokerage
      fno: { brokerage: 0 },                   // Zero brokerage
    },

    defaultSymbols: NIFTY_50_SYMBOLS,
  },

  flattrade: {
    id: 'flattrade',
    name: 'flattrade',
    displayName: 'Flattrade',
    logo: 'https://fincept.in/brokers/flattrade.png',
    website: 'https://flattrade.in',

    region: 'india',
    country: 'IN',
    currency: 'INR',
    exchanges: ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'MCX'],
    marketHours: {
      open: '09:15',
      close: '15:30',
      timezone: 'Asia/Kolkata',
      preMarketOpen: '09:00',
      preMarketClose: '09:08',
    },

    features: {
      webSocket: true,
      amo: true,
      gtt: false,
      bracketOrder: true,
      coverOrder: true,
      marginCalculator: true,
      optionsChain: true,
      paperTrading: false,
    },

    tradingFeatures: {
      marketOrders: true,
      limitOrders: true,
      stopOrders: true,
      stopLimitOrders: true,
      trailingStopOrders: false,
    },

    productTypes: ['CASH', 'INTRADAY', 'MARGIN'],

    authType: 'oauth', // OAuth redirect + SHA256 hash

    rateLimit: {
      ordersPerSecond: 10,
      quotesPerSecond: 5,
    },

    fees: {
      equity: { brokerage: 0, stt: 0.001 },   // Zero brokerage
      intraday: { brokerage: 0 },              // Zero brokerage
      fno: { brokerage: 0 },                   // Zero brokerage
    },

    defaultSymbols: NIFTY_50_SYMBOLS,
  },

  // ============================================================================
  // US BROKERS
  // ============================================================================

  alpaca: {
    id: 'alpaca',
    name: 'alpaca',
    displayName: 'Alpaca',
    logo: 'https://fincept.in/brokers/alpaca.png',
    website: 'https://alpaca.markets',

    region: 'us',
    country: 'US',
    currency: 'USD',
    exchanges: ['NYSE', 'NASDAQ', 'AMEX', 'ARCA', 'BATS'],
    marketHours: {
      open: '09:30',
      close: '16:00',
      timezone: 'America/New_York',
      preMarketOpen: '04:00',
      preMarketClose: '09:30',
      postMarketOpen: '16:00',
      postMarketClose: '20:00',
    },

    features: {
      webSocket: true,
      amo: false, // US doesn't use AMO concept
      gtt: true, // GTC orders
      bracketOrder: true,
      coverOrder: false,
      marginCalculator: true,
      optionsChain: false,
      paperTrading: true, // Alpaca has built-in paper trading!
    },

    tradingFeatures: {
      marketOrders: true,
      limitOrders: true,
      stopOrders: true,
      stopLimitOrders: true,
      trailingStopOrders: true, // Alpaca supports trailing stops
    },

    productTypes: ['CASH', 'MARGIN'],

    authType: 'api_key', // API Key + Secret

    rateLimit: {
      ordersPerSecond: 10,
      quotesPerSecond: 200, // Alpaca has generous rate limits
    },

    fees: {
      equity: { brokerage: 0, stt: 0 }, // Commission-free
      intraday: { brokerage: 0 },
      fno: { brokerage: 0 },
    },

    defaultSymbols: [
      'AAPL', 'MSFT', 'GOOGL', 'AMZN', 'NVDA',
      'META', 'TSLA', 'BRK.B', 'JPM', 'V',
      'UNH', 'JNJ', 'XOM', 'PG', 'MA',
    ],
  },

  tradier: {
    id: 'tradier',
    name: 'tradier',
    displayName: 'Tradier',
    logo: 'https://fincept.in/brokers/tradier.png',
    website: 'https://tradier.com',

    region: 'us',
    country: 'US',
    currency: 'USD',
    exchanges: ['NYSE', 'NASDAQ', 'AMEX', 'ARCA', 'BATS'],
    marketHours: {
      open: '09:30',
      close: '16:00',
      timezone: 'America/New_York',
      preMarketOpen: '04:00',
      preMarketClose: '09:30',
      postMarketOpen: '16:00',
      postMarketClose: '20:00',
    },

    features: {
      webSocket: true,
      amo: false, // US doesn't use AMO concept
      gtt: true, // GTC orders supported
      bracketOrder: false, // Basic bracket not directly supported
      coverOrder: false,
      marginCalculator: false, // No margin preview API
      optionsChain: true, // Full options support!
      paperTrading: true, // Sandbox environment
    },

    tradingFeatures: {
      marketOrders: true,
      limitOrders: true,
      stopOrders: true,
      stopLimitOrders: true,
      trailingStopOrders: false, // Not supported
    },

    productTypes: ['CASH', 'MARGIN'],

    authType: 'api_key', // Bearer token

    rateLimit: {
      ordersPerSecond: 10,
      quotesPerSecond: 120, // 120 requests per minute
    },

    fees: {
      equity: { brokerage: 0, stt: 0 }, // Commission-free stocks
      intraday: { brokerage: 0 },
      fno: { brokerage: 0.35 }, // $0.35 per contract for options
    },

    defaultSymbols: [
      'AAPL', 'MSFT', 'GOOGL', 'AMZN', 'NVDA',
      'META', 'TSLA', 'BRK.B', 'JPM', 'V',
      'UNH', 'JNJ', 'XOM', 'PG', 'MA',
    ],
  },

  ibkr: {
    id: 'ibkr',
    name: 'ibkr',
    displayName: 'Interactive Brokers',
    logo: 'https://fincept.in/brokers/ibkr.png',
    website: 'https://www.interactivebrokers.com',

    region: 'us',
    country: 'US',
    currency: 'USD',
    exchanges: ['NYSE', 'NASDAQ', 'AMEX', 'ARCA', 'BATS', 'IEX', 'CBOE', 'PHLX', 'ISE'],
    marketHours: {
      open: '09:30',
      close: '16:00',
      timezone: 'America/New_York',
      preMarketOpen: '04:00',
      preMarketClose: '09:30',
      postMarketOpen: '16:00',
      postMarketClose: '20:00',
    },

    features: {
      webSocket: true,
      amo: false, // US doesn't use AMO concept
      gtt: true, // GTC orders supported
      bracketOrder: true,
      coverOrder: false,
      marginCalculator: true,
      optionsChain: true, // IBKR has full options support
      paperTrading: true, // IBKR has built-in paper trading (DU accounts)
    },

    tradingFeatures: {
      marketOrders: true,
      limitOrders: true,
      stopOrders: true,
      stopLimitOrders: true,
      trailingStopOrders: true, // IBKR supports trailing stops
    },

    productTypes: ['CASH', 'MARGIN'],

    authType: 'oauth', // OAuth or Client Portal Gateway

    rateLimit: {
      ordersPerSecond: 50, // IBKR is generous with order rate limits
      quotesPerSecond: 100, // Market data rate limits
    },

    fees: {
      equity: { brokerage: 0.005, stt: 0 }, // $0.005 per share, min $1
      intraday: { brokerage: 0.005 },
      fno: { brokerage: 0.65 }, // $0.65 per contract for options
    },

    defaultSymbols: [
      'AAPL', 'MSFT', 'GOOGL', 'AMZN', 'NVDA',
      'META', 'TSLA', 'BRK.B', 'JPM', 'V',
      'UNH', 'JNJ', 'XOM', 'PG', 'MA',
    ],
  },

  // ============================================================================
  // EUROPE BROKERS (Placeholder)
  // ============================================================================


  saxobank: {
    id: 'saxobank',
    name: 'saxobank',
    displayName: 'Saxo Bank',
    logo: 'https://fincept.in/brokers/saxobank.png',
    website: 'https://www.home.saxo',

    region: 'europe',
    country: 'DK',
    currency: 'EUR',
    exchanges: [
      'XPAR', 'XETR', 'XFRA', 'XMAD', 'XAMS', 'XBRU', 'XLIS', 'XMIL',
      'XLON', 'XSWX', 'XCSE', 'XSTO', 'XHEL', 'XOSL', 'XWAR', 'XPRA',
      'XBUD', 'XIST', 'XATH', 'XDUB',
      'XNYS', 'XNAS', 'XASE', 'ARCX',
      'XTKS', 'XHKG', 'XASX', 'XSES',
    ],
    marketHours: {
      open: '09:00',
      close: '17:30',
      timezone: 'Europe/Copenhagen',
      preMarketOpen: '08:00',
      preMarketClose: '09:00',
      postMarketOpen: '17:30',
      postMarketClose: '20:00',
    },

    features: {
      webSocket: true,
      amo: true,
      gtt: true,
      bracketOrder: true,
      coverOrder: false,
      marginCalculator: true,
      optionsChain: true,
      paperTrading: true,
    },

    tradingFeatures: {
      marketOrders: true,
      limitOrders: true,
      stopOrders: true,
      stopLimitOrders: true,
      trailingStopOrders: true,
    },

    productTypes: ['CASH', 'MARGIN'],

    authType: 'oauth',

    rateLimit: {
      ordersPerSecond: 1,
      quotesPerSecond: 2,
    },

    fees: {
      equity: { brokerage: 0.1, stt: 0 },
      intraday: { brokerage: 0.05 },
      fno: { brokerage: 0.05 },
    },

    defaultSymbols: [
      'ASML:XAMS', 'SAP:XETR', 'LVMH:XPAR', 'NESN:XSWX', 'NOVO:XCSE',
      'SHEL:XLON', 'SAN:XMAD', 'ENEL:XMIL', 'OR:XPAR', 'SIE:XETR',
      'AZN:XLON', 'MC:XPAR', 'ROG:XSWX', 'NOVN:XSWX', 'ULVR:XLON',
    ],
  },

  // ============================================================================
  // PAPER TRADING / DATA PROVIDERS (No Real Broker - Free Data + Paper Trading)
  // ============================================================================
  // NOTE: yfinance is categorized under 'us' region but supports global markets
  // It's free, requires NO credentials, and is ONLY for paper trading

  yfinance: {
    id: 'yfinance',
    name: 'yfinance',
    displayName: 'YFinance (Free Paper Trading)',
    logo: 'https://fincept.in/brokers/yfinance.png',
    website: 'https://finance.yahoo.com',

    region: 'us', // Primary region, but supports global markets
    country: 'US',
    currency: 'USD',
    exchanges: ['NYSE', 'NASDAQ', 'AMEX', 'NSE', 'BSE', 'LSE', 'XETRA', 'TSX', 'ASX', 'HKEX'],
    marketHours: {
      open: '09:30',
      close: '16:00',
      timezone: 'America/New_York',
      preMarketOpen: '04:00',
      preMarketClose: '09:30',
      postMarketOpen: '16:00',
      postMarketClose: '20:00',
    },

    features: {
      webSocket: false, // No real-time streaming - poll-based quotes
      amo: false,
      gtt: false,
      bracketOrder: false,
      coverOrder: false,
      marginCalculator: false,
      optionsChain: false,
      paperTrading: true, // ONLY paper trading - no live trading support
    },

    tradingFeatures: {
      marketOrders: true,
      limitOrders: true,
      stopOrders: true,
      stopLimitOrders: true,
      trailingStopOrders: false,
    },

    productTypes: ['CASH'],

    authType: 'api_key', // NOTE: This is a placeholder - no actual credentials needed

    rateLimit: {
      ordersPerSecond: 10,
      quotesPerSecond: 1,
    },

    fees: {
      equity: { brokerage: 0, stt: 0 },
      intraday: { brokerage: 0 },
      fno: { brokerage: 0 },
    },

    defaultSymbols: [
      'AAPL', 'MSFT', 'GOOGL', 'AMZN', 'NVDA',
      'META', 'TSLA', 'BRK-B', 'JPM', 'V',
      'UNH', 'JNJ', 'XOM', 'PG', 'MA',
      'NFLX', 'ADBE', 'CRM', 'INTC', 'AMD',
      'RELIANCE.NS', 'TCS.NS', 'INFY.NS', 'HDFCBANK.NS', 'ICICIBANK.NS',
    ],
  },
};

// ============================================================================
// ADAPTER REGISTRY (Runtime instances)
// ============================================================================
// ADAPTER REGISTRY (Runtime instances)
// ============================================================================

// Map of broker ID to adapter class constructor
const adapterClasses: Record<string, new () => IStockBrokerAdapter> = {};

/**
 * Register a broker adapter class
 */
export function registerBrokerAdapter(
  brokerId: string,
  adapterClass: new () => IStockBrokerAdapter
): void {
  adapterClasses[brokerId] = adapterClass;
}

/**
 * Create a broker adapter instance
 */
export function createStockBrokerAdapter(brokerId: string): IStockBrokerAdapter {
  const AdapterClass = adapterClasses[brokerId];

  if (!AdapterClass) {
    throw new Error(`Broker adapter not registered: ${brokerId}`);
  }

  return new AdapterClass();
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * Get all registered stock brokers
 */
export function getAllStockBrokers(): StockBrokerMetadata[] {
  return Object.values(STOCK_BROKER_REGISTRY);
}

/**
 * Get brokers by region
 */
export function getStockBrokersByRegion(region: Region): StockBrokerMetadata[] {
  return getAllStockBrokers().filter(broker => broker.region === region);
}

/**
 * Get broker metadata by ID
 */
export function getStockBrokerMetadata(brokerId: string): StockBrokerMetadata | undefined {
  return STOCK_BROKER_REGISTRY[brokerId];
}

/**
 * Check if broker is registered
 */
export function isStockBrokerRegistered(brokerId: string): boolean {
  return brokerId in STOCK_BROKER_REGISTRY;
}

/**
 * Get broker display name
 */
export function getStockBrokerDisplayName(brokerId: string): string {
  return STOCK_BROKER_REGISTRY[brokerId]?.displayName || brokerId.toUpperCase();
}

/**
 * Get default symbols for broker
 */
export function getStockBrokerDefaultSymbols(brokerId: string): string[] {
  return STOCK_BROKER_REGISTRY[brokerId]?.defaultSymbols || [];
}

/**
 * Check if broker supports feature
 */
export function stockBrokerSupportsFeature(
  brokerId: string,
  feature: keyof StockBrokerMetadata['features']
): boolean {
  const metadata = STOCK_BROKER_REGISTRY[brokerId];
  return metadata?.features[feature] === true;
}

/**
 * Check if broker supports trading feature
 */
export function stockBrokerSupportsTradingFeature(
  brokerId: string,
  feature: keyof StockBrokerMetadata['tradingFeatures']
): boolean {
  const metadata = STOCK_BROKER_REGISTRY[brokerId];
  return metadata?.tradingFeatures[feature] === true;
}

/**
 * Get available regions
 */
export function getAvailableRegions(): Region[] {
  const regions = new Set<Region>();
  for (const broker of getAllStockBrokers()) {
    regions.add(broker.region);
  }
  return Array.from(regions);
}

/**
 * Get brokers with specific feature
 */
export function getStockBrokersWithFeature(
  feature: keyof StockBrokerMetadata['features']
): StockBrokerMetadata[] {
  return getAllStockBrokers().filter(broker => broker.features[feature] === true);
}
