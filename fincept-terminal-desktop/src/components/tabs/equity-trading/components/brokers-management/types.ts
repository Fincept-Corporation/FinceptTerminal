import type { Region } from '@/brokers/stocks/types';

// Fincept color palette
export const COLORS = {
  ORANGE: '#FF8800',
  GREEN: '#00D66F',
  RED: '#FF3B3B',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CARD_BG: '#141414',
  BORDER: '#2A2A2A',
  GRAY: '#787878',
  MUTED: '#4A4A4A',
  WHITE: '#FFFFFF',
};

export type ConnectionStatus = 'connected' | 'expired' | 'configured' | 'not_configured';

export interface BrokerStatus {
  brokerId: string;
  metadata: import('@/brokers/stocks/types').StockBrokerMetadata;
  status: ConnectionStatus;
  lastAuthTime?: Date;
  hasCredentials: boolean;
}

// API credential URLs for brokers
export const API_KEYS_URLS: Record<string, string> = {
  zerodha: 'https://developers.kite.trade/',
  fyers: 'https://myapi.fyers.in/dashboard',
  angelone: 'https://smartapi.angelbroking.com/publisher-login',
  angel: 'https://smartapi.angelbroking.com/',
  dhan: 'https://dhanhq.co/api',
  upstox: 'https://upstox.com/developer/api/',
  kotak: 'https://napi.kotaksecurities.com/devportal/',
  saxobank: 'https://www.developer.saxo/openapi/appmanagement',
  alpaca: 'https://app.alpaca.markets/brokerage/new-account',
  ibkr: 'https://www.interactivebrokers.com/en/trading/ib-api.php',
  tradier: 'https://developer.tradier.com/',
};

// Region labels
export const REGION_LABELS: Record<Region, string> = {
  india: 'India',
  us: 'United States',
  europe: 'Europe',
  uk: 'United Kingdom',
  asia: 'Asia Pacific',
  australia: 'Australia',
  canada: 'Canada',
};
