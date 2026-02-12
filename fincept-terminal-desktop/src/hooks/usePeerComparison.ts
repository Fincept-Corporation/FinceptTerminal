// usePeerComparison Hook - Peer Comparison State Management
// Manages peer comparison data fetching and state

import { useState, useCallback, useEffect } from 'react';
import type {
  PeerCompany,
  PeerComparisonResult,
  CompanyMetrics,
  PercentileRanking,
} from '../types/peer';

interface UsePeerComparisonReturn {
  targetSymbol: string;
  selectedPeers: PeerCompany[];
  availablePeers: PeerCompany[];
  comparisonData: PeerComparisonResult | null;
  loading: boolean;
  error: string | null;
  setTargetSymbol: (symbol: string) => void;
  addPeer: (peer: PeerCompany) => void;
  removePeer: (symbol: string) => void;
  fetchPeers: () => Promise<void>;
  runComparison: () => Promise<void>;
}

// Mock data generator for development
function generateMockCompanyMetrics(symbol: string): CompanyMetrics {
  const randomValue = (min: number, max: number) =>
    Math.random() * (max - min) + min;

  return {
    symbol,
    name: `${symbol} Inc.`,
    valuation: {
      peRatio: randomValue(10, 50),
      pbRatio: randomValue(1, 10),
      psRatio: randomValue(1, 15),
      evToEbitda: randomValue(5, 25),
      pegRatio: randomValue(0.5, 3),
    },
    profitability: {
      roe: randomValue(0.05, 0.35),
      roa: randomValue(0.02, 0.2),
      roic: randomValue(0.05, 0.25),
      grossMargin: randomValue(0.2, 0.8),
      operatingMargin: randomValue(0.05, 0.4),
      netMargin: randomValue(0.02, 0.3),
    },
    growth: {
      revenueGrowth: randomValue(-0.1, 0.5),
      earningsGrowth: randomValue(-0.2, 0.6),
      fcfGrowth: randomValue(-0.15, 0.4),
    },
    leverage: {
      debtToEquity: randomValue(0, 2),
      debtToAssets: randomValue(0.1, 0.6),
      interestCoverage: randomValue(2, 20),
    },
  };
}

function generateMockPeerCompany(symbol: string, index: number): PeerCompany {
  return {
    symbol,
    name: `${symbol} Corporation`,
    sector: 'Information Technology',
    industry: 'Software',
    marketCap: Math.random() * 500_000_000_000,
    marketCapCategory: 'Large Cap',
    similarityScore: 0.7 + Math.random() * 0.3,
    matchDetails: {
      sectorMatch: true,
      industryMatch: index < 3,
      marketCapSimilarity: 0.6 + Math.random() * 0.4,
      geographicMatch: true,
    },
  };
}

export function usePeerComparison(initialSymbol?: string): UsePeerComparisonReturn {
  const [targetSymbol, setTargetSymbol] = useState<string>(initialSymbol || '');
  const [selectedPeers, setSelectedPeers] = useState<PeerCompany[]>([]);
  const [availablePeers, setAvailablePeers] = useState<PeerCompany[]>([]);
  const [comparisonData, setComparisonData] = useState<PeerComparisonResult | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // Add a peer to selected peers
  const addPeer = useCallback((peer: PeerCompany) => {
    setSelectedPeers((prev) => {
      if (prev.find((p) => p.symbol === peer.symbol)) {
        return prev;
      }
      return [...prev, peer];
    });
  }, []);

  // Remove a peer from selected peers
  const removePeer = useCallback((symbol: string) => {
    setSelectedPeers((prev) => prev.filter((p) => p.symbol !== symbol));
  }, []);

  // Fetch available peers for the target symbol
  const fetchPeers = useCallback(async () => {
    if (!targetSymbol) {
      setError('Please enter a target symbol');
      return;
    }

    setLoading(true);
    setError(null);

    try {
      // TODO: Replace with actual API call
      // For now, generate mock peers
      await new Promise((resolve) => setTimeout(resolve, 500));

      const mockPeers: PeerCompany[] = [
        'MSFT', 'GOOGL', 'META', 'AMZN', 'NVDA', 'CRM', 'ORCL', 'ADBE'
      ]
        .filter((s) => s !== targetSymbol.toUpperCase())
        .slice(0, 8)
        .map((symbol, index) => generateMockPeerCompany(symbol, index));

      setAvailablePeers(mockPeers);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to fetch peers');
    } finally {
      setLoading(false);
    }
  }, [targetSymbol]);

  // Run comparison between target and selected peers
  const runComparison = useCallback(async () => {
    if (!targetSymbol) {
      setError('Please enter a target symbol');
      return;
    }

    if (selectedPeers.length === 0) {
      setError('Please select at least one peer');
      return;
    }

    setLoading(true);
    setError(null);

    try {
      // TODO: Replace with actual API call
      await new Promise((resolve) => setTimeout(resolve, 800));

      const targetMetrics = generateMockCompanyMetrics(targetSymbol);
      const peerMetrics = selectedPeers.map((peer) =>
        generateMockCompanyMetrics(peer.symbol)
      );

      // Calculate percentile rankings
      const percentileRanks: Record<string, PercentileRanking> = {};

      const calculatePercentile = (targetValue: number, peerValues: number[]): number => {
        const allValues = [targetValue, ...peerValues];
        const sorted = [...allValues].sort((a, b) => a - b);
        const rank = sorted.indexOf(targetValue);
        return (rank / (allValues.length - 1)) * 100;
      };

      if (targetMetrics.valuation.peRatio) {
        const peerValues = peerMetrics
          .map((p) => p.valuation.peRatio)
          .filter((v): v is number => v !== undefined);
        if (peerValues.length > 0) {
          const percentile = calculatePercentile(targetMetrics.valuation.peRatio, peerValues);
          const mean = peerValues.reduce((a, b) => a + b, 0) / peerValues.length;
          const variance = peerValues.reduce((sum, v) => sum + Math.pow(v - mean, 2), 0) / peerValues.length;
          const stdDev = Math.sqrt(variance);
          percentileRanks['P/E Ratio'] = {
            metricName: 'P/E Ratio',
            value: targetMetrics.valuation.peRatio,
            percentile,
            zScore: stdDev > 0 ? (targetMetrics.valuation.peRatio - mean) / stdDev : 0,
            peerMedian: [...peerValues].sort((a, b) => a - b)[Math.floor(peerValues.length / 2)],
            peerMean: mean,
            peerStdDev: stdDev,
          };
        }
      }

      // Calculate summary scores
      const valuationScore = Math.random() * 2;
      const profitabilityScore = Math.random() * 2;
      const growthScore = Math.random() * 2;

      const result: PeerComparisonResult = {
        target: targetMetrics,
        peers: peerMetrics,
        benchmarks: {
          sector: 'Information Technology',
          medianPe: peerMetrics.reduce((sum, p) => sum + (p.valuation.peRatio || 0), 0) / peerMetrics.length,
          medianPb: peerMetrics.reduce((sum, p) => sum + (p.valuation.pbRatio || 0), 0) / peerMetrics.length,
          medianRoe: peerMetrics.reduce((sum, p) => sum + (p.profitability.roe || 0), 0) / peerMetrics.length * 100,
          medianRevenueGrowth: peerMetrics.reduce((sum, p) => sum + (p.growth.revenueGrowth || 0), 0) / peerMetrics.length * 100,
          medianDebtToEquity: peerMetrics.reduce((sum, p) => sum + (p.leverage.debtToEquity || 0), 0) / peerMetrics.length,
        },
        percentileRanks,
        summary: {
          valuationScore,
          profitabilityScore,
          growthScore,
          overallRating: valuationScore + profitabilityScore + growthScore,
        },
      };

      setComparisonData(result);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to run comparison');
    } finally {
      setLoading(false);
    }
  }, [targetSymbol, selectedPeers]);

  // Auto-fetch peers when target symbol changes
  useEffect(() => {
    if (targetSymbol && targetSymbol.length >= 1) {
      fetchPeers();
    }
  }, [targetSymbol, fetchPeers]);

  return {
    targetSymbol,
    selectedPeers,
    availablePeers,
    comparisonData,
    loading,
    error,
    setTargetSymbol,
    addPeer,
    removePeer,
    fetchPeers,
    runComparison,
  };
}

export default usePeerComparison;
