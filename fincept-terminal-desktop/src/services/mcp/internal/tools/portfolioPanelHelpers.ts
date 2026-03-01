// Portfolio Panel MCP Tools — Shared Helpers
// Used by portfolioPanelTools1.ts and portfolioPanelTools2.ts

import { invoke } from '@tauri-apps/api/core';

/** Safely invoke a Tauri/Rust command — returns null on any error. */
export async function si<T>(cmd: string, args?: Record<string, any>): Promise<T | null> {
  try { return await invoke<T>(cmd, args); } catch { return null; }
}

/** Parse a JSON string that may be returned by Rust commands. */
export function safeParse(raw: any): any {
  if (raw === null || raw === undefined) return null;
  if (typeof raw !== 'string') return raw;
  try { return JSON.parse(raw); } catch { return raw; }
}

/** Build symbol → fractional-weight map from holdings array. */
export function toWeights(holdings: any[]): Record<string, number> {
  const out: Record<string, number> = {};
  for (const h of holdings) {
    const w = typeof h.weight === 'number' ? h.weight
      : typeof h.portfolio_weight_percent === 'number' ? h.portfolio_weight_percent : 0;
    out[h.symbol] = w / 100;
  }
  return out;
}

/** Build { symbol: day_change_percent/100 } returns map from holdings. */
export function toReturns(holdings: any[]): Record<string, number> {
  const out: Record<string, number> = {};
  for (const h of holdings) out[h.symbol] = (h.day_change_percent ?? 0) / 100;
  return out;
}

/** Build { symbol: unrealized_pnl_percent/100 } returns map from holdings. */
export function toPnlReturns(holdings: any[]): Record<string, number> {
  const out: Record<string, number> = {};
  for (const h of holdings) out[h.symbol] = (h.unrealized_pnl_percent ?? 0) / 100;
  return out;
}
