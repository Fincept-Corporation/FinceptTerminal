// Valuation Signal Calculator
// Percentile-based investment signal generation (from PeerComparisonPanel logic)

import type { CompanyInfoV2, PeerCompany, ValuationSignal } from '../types';

/**
 * Calculate percentile rank of a value within a sorted array.
 * Returns 0-100 where 0 = lowest, 100 = highest.
 */
function calculatePercentile(value: number, values: number[]): number {
  if (values.length === 0 || isNaN(value)) return 50;
  const sorted = [...values].sort((a, b) => a - b);
  const below = sorted.filter(v => v < value).length;
  return (below / sorted.length) * 100;
}

/**
 * Calculate valuation signal for a company relative to its peers.
 * Uses percentile-based scoring across multiple metrics.
 *
 * Scoring Logic:
 * - Valuation metrics (P/E, P/B, P/S): lower percentile = cheaper = +score
 * - Profitability (ROE, margins): higher percentile = stronger = +score
 * - Growth: higher percentile = faster growing = +score
 */
export function calculateValuationSignal(
  company: CompanyInfoV2 | PeerCompany,
  peers: PeerCompany[]
): ValuationSignal {
  // Need minimum peers for reliable signal
  if (peers.length < 3) {
    return {
      status: 'FAIRLY_VALUED',
      action: 'HOLD',
      confidence: 'LOW',
      score: 0,
      reasoning: ['Insufficient peers for reliable valuation signal'],
      percentiles: { pe: 50, pb: 50, ps: 50, roe: 50, growth: 50 },
    };
  }

  const reasoning: string[] = [];
  let score = 0;

  // Collect peer metrics for percentile calculation
  const peerPE = peers.filter(p => p.pe_ratio > 0).map(p => p.pe_ratio);
  const peerPB = peers.filter(p => p.price_to_book > 0).map(p => p.price_to_book);
  const peerPS = peers.filter(p => p.price_to_sales > 0).map(p => p.price_to_sales);
  const peerROE = peers.filter(p => p.roe !== 0).map(p => p.roe);
  const peerGrowth = peers.map(p => p.revenue_growth);

  // Calculate percentiles for the target company
  const c = company as CompanyInfoV2 & PeerCompany;
  const pe = c.pe_ratio || 0;
  const pb = c.price_to_book || 0;
  const ps = (c as CompanyInfoV2).price_to_sales || 0;
  const roe = c.roe || 0;
  const growth = c.revenue_growth || 0;

  const pePercentile = pe > 0 ? calculatePercentile(pe, peerPE) : 50;
  const pbPercentile = pb > 0 ? calculatePercentile(pb, peerPB) : 50;
  const psPercentile = ps > 0 ? calculatePercentile(ps, peerPS) : 50;
  const roePercentile = roe !== 0 ? calculatePercentile(roe, peerROE) : 50;
  const growthPercentile = calculatePercentile(growth, peerGrowth);

  // Valuation scoring: lower percentile = cheaper = better
  if (pePercentile < 33 && pe > 0) {
    score += 1;
    reasoning.push(`P/E ratio (${pe.toFixed(1)}) is in bottom third vs peers - cheap`);
  } else if (pePercentile > 66) {
    score -= 1;
    reasoning.push(`P/E ratio (${pe.toFixed(1)}) is in top third vs peers - expensive`);
  }

  if (pbPercentile < 33 && pb > 0) {
    score += 1;
    reasoning.push(`P/B ratio (${pb.toFixed(1)}) is in bottom third - undervalued on book`);
  } else if (pbPercentile > 66) {
    score -= 1;
    reasoning.push(`P/B ratio (${pb.toFixed(1)}) is in top third - premium to book`);
  }

  if (psPercentile < 33 && ps > 0) {
    score += 1;
    reasoning.push(`P/S ratio (${ps.toFixed(1)}) is in bottom third - revenue undervalued`);
  } else if (psPercentile > 66) {
    score -= 1;
    reasoning.push(`P/S ratio (${ps.toFixed(1)}) is in top third - expensive on revenue`);
  }

  // Profitability scoring: higher = better
  if (roePercentile > 66) {
    score += 1;
    reasoning.push(`ROE (${(roe * 100).toFixed(1)}%) is top third - strong profitability`);
  } else if (roePercentile < 33 && roe > 0) {
    score -= 0.5;
    reasoning.push(`ROE (${(roe * 100).toFixed(1)}%) is bottom third - weak profitability`);
  }

  // Growth scoring: higher = better
  if (growthPercentile > 66) {
    score += 0.5;
    reasoning.push(`Revenue growth (${(growth * 100).toFixed(1)}%) is top third - fast growing`);
  } else if (growthPercentile < 33) {
    score -= 0.5;
    reasoning.push(`Revenue growth (${(growth * 100).toFixed(1)}%) is bottom third - slow growth`);
  }

  // Determine status and action
  let status: ValuationSignal['status'];
  let action: ValuationSignal['action'];

  if (score >= 2) {
    status = 'UNDERVALUED';
    action = 'BUY';
  } else if (score >= 1) {
    status = 'POTENTIALLY_UNDERVALUED';
    action = 'BUY';
  } else if (score <= -2) {
    status = 'OVERVALUED';
    action = 'SELL';
  } else if (score <= -1) {
    status = 'POTENTIALLY_OVERVALUED';
    action = 'SELL';
  } else {
    status = 'FAIRLY_VALUED';
    action = 'HOLD';
  }

  // Determine confidence
  const metricsAvailable = [
    pe > 0 ? 1 : 0,
    pb > 0 ? 1 : 0,
    ps > 0 ? 1 : 0,
    roe !== 0 ? 1 : 0,
    growth !== 0 ? 1 : 0,
  ].reduce((a, b) => a + b, 0);

  let confidence: ValuationSignal['confidence'];
  if (metricsAvailable >= 4 && peers.length >= 5) {
    confidence = 'HIGH';
  } else if (metricsAvailable >= 3 && peers.length >= 3) {
    confidence = 'MEDIUM';
  } else {
    confidence = 'LOW';
  }

  return {
    status,
    action,
    confidence,
    score,
    reasoning,
    percentiles: {
      pe: pePercentile,
      pb: pbPercentile,
      ps: psPercentile,
      roe: roePercentile,
      growth: growthPercentile,
    },
  };
}

/**
 * Calculate valuation signals for all peers relative to the group.
 */
export function calculatePeerValuations(
  peers: PeerCompany[]
): Map<string, ValuationSignal> {
  const result = new Map<string, ValuationSignal>();

  for (const peer of peers) {
    // Exclude current peer from comparison group
    const otherPeers = peers.filter(p => p.ticker !== peer.ticker);
    const signal = calculateValuationSignal(peer, otherPeers);
    result.set(peer.ticker, signal);
  }

  return result;
}

/**
 * Get display color for a valuation signal action.
 */
export function getSignalColor(action: ValuationSignal['action']): {
  bg: string;
  border: string;
  text: string;
} {
  switch (action) {
    case 'BUY':
      return { bg: 'rgba(0, 200, 0, 0.2)', border: '#00C800', text: '#00FF41' };
    case 'SELL':
      return { bg: 'rgba(255, 0, 0, 0.2)', border: '#FF0000', text: '#FF4444' };
    case 'HOLD':
    default:
      return { bg: 'rgba(255, 255, 0, 0.15)', border: '#FFFF00', text: '#FFFF00' };
  }
}

/**
 * Get short label for valuation status.
 */
export function getSignalLabel(status: ValuationSignal['status']): string {
  switch (status) {
    case 'UNDERVALUED': return 'UNDERVALUED';
    case 'POTENTIALLY_UNDERVALUED': return 'LIKELY UNDERVALUED';
    case 'OVERVALUED': return 'OVERVALUED';
    case 'POTENTIALLY_OVERVALUED': return 'LIKELY OVERVALUED';
    case 'FAIRLY_VALUED': return 'FAIR VALUE';
  }
}
