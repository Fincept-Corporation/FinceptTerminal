/**
 * QuantStats View — Utilities
 *
 * Constants and formatting helpers extracted from QuantStatsView.tsx.
 */

import { FINCEPT } from '../finceptStyles';

/** Decode a base64 string to a Uint8Array (handles non-ASCII correctly). */
export function base64ToUint8Array(base64: string): Uint8Array {
  const binary = atob(base64);
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i++) {
    bytes[i] = binary.charCodeAt(i);
  }
  return bytes;
}

export const BENCHMARK_SUGGESTIONS = [
  { value: 'SPY', label: 'S&P 500' },
  { value: 'QQQ', label: 'Nasdaq 100' },
  { value: 'IWM', label: 'Russell 2000' },
  { value: 'DIA', label: 'Dow Jones' },
  { value: 'VTI', label: 'Total US Market' },
  { value: 'EFA', label: 'EAFE Intl' },
  { value: 'EEM', label: 'Emerging Mkts' },
  { value: 'AGG', label: 'US Bonds' },
  { value: '^GSPC', label: 'S&P 500 Index' },
  { value: '^NSEI', label: 'Nifty 50' },
  { value: '^BSESN', label: 'Sensex' },
  { value: '^FTSE', label: 'FTSE 100' },
  { value: '^N225', label: 'Nikkei 225' },
  { value: '^HSI', label: 'Hang Seng' },
  { value: '^GDAXI', label: 'DAX' },
  { value: 'GLD', label: 'Gold ETF' },
  { value: 'BTC-USD', label: 'Bitcoin' },
  { value: 'ETH-USD', label: 'Ethereum' },
];

export const PERIODS = [
  { value: '3mo', label: '3M' },
  { value: '6mo', label: '6M' },
  { value: '1y', label: '1Y' },
  { value: '2y', label: '2Y' },
  { value: '5y', label: '5Y' },
  { value: 'max', label: 'MAX' },
];

export const MONTH_NAMES = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];

export function fmt(val: number | null | undefined, decimals = 2, suffix = ''): string {
  if (val === null || val === undefined) return 'N/A';
  return `${(val * 100).toFixed(decimals)}${suffix}`;
}

export function fmtRaw(val: number | null | undefined, decimals = 2): string {
  if (val === null || val === undefined) return 'N/A';
  return val.toFixed(decimals);
}

export function getReturnColor(val: number | null | undefined): string {
  if (val === null || val === undefined) return FINCEPT.GRAY;
  return val >= 0 ? FINCEPT.GREEN : FINCEPT.RED;
}

export function getHeatmapColor(val: number | null | undefined): string {
  if (val === null || val === undefined) return FINCEPT.PANEL_BG;
  const v = val * 100;
  if (v > 5) return 'rgba(0,214,111,0.4)';
  if (v > 2) return 'rgba(0,214,111,0.25)';
  if (v > 0) return 'rgba(0,214,111,0.1)';
  if (v > -2) return 'rgba(255,59,59,0.1)';
  if (v > -5) return 'rgba(255,59,59,0.25)';
  return 'rgba(255,59,59,0.4)';
}
