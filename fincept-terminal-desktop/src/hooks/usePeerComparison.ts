// Custom React Hook for Peer Comparison
// Manages state and API calls for peer analysis

import { useState, useCallback, useEffect } from 'react';
import { invoke } from '@tauri-apps/api/core';
import type {
  PeerCompany,
  PeerComparisonResult,
  PeerSearchInput,
  ComparisonInput,
  PeerCommandResponse,
  MetricRanking,
  PercentileInput,
} from '../types/peer';

export interface UsePeerComparisonReturn {
  // State
  targetSymbol: string;
  selectedPeers: PeerCompany[];
  availablePeers: PeerCompany[];
  comparisonData: PeerComparisonResult | null;
  loading: boolean;
  error: string | null;

  // Actions
  setTargetSymbol: (symbol: string) => void;
  fetchPeers: (input: PeerSearchInput) => Promise<void>;
  addPeer: (peer: PeerCompany) => void;
  removePeer: (symbol: string) => void;
  selectPeer: (symbol: string) => void;
  deselectPeer: (symbol: string) => void;
  selectAllPeers: () => void;
  clearPeers: () => void;
  runComparison: () => Promise<void>;
  calculatePercentiles: (input: PercentileInput) => Promise<MetricRanking | null>;
  reset: () => void;
}

export function usePeerComparison(initialSymbol?: string): UsePeerComparisonReturn {
  const [targetSymbol, setTargetSymbol] = useState<string>(initialSymbol || '');
  const [selectedPeers, setSelectedPeers] = useState<PeerCompany[]>([]);
  const [availablePeers, setAvailablePeers] = useState<PeerCompany[]>([]);
  const [comparisonData, setComparisonData] = useState<PeerComparisonResult | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // Fetch peer companies
  const fetchPeers = useCallback(async (input: PeerSearchInput) => {
    console.log('[usePeerComparison] Fetching peers with input:', input);
    setLoading(true);
    setError(null);

    try {
      console.log('[usePeerComparison] Calling invoke find_peers...');
      const response = await invoke<PeerCommandResponse<PeerCompany[]>>('find_peers', { input });
      console.log('[usePeerComparison] Response:', response);

      if (response.success && response.data) {
        console.log('[usePeerComparison] Found', response.data.length, 'peers');
        setAvailablePeers(response.data);
        // Auto-select top 5 peers by similarity
        const topPeers = response.data.slice(0, 5);
        setSelectedPeers(topPeers);
        console.log('[usePeerComparison] Auto-selected', topPeers.length, 'peers');
      } else {
        console.error('[usePeerComparison] Failed:', response.error);
        setError(response.error || 'Failed to fetch peers');
      }
    } catch (err) {
      console.error('[usePeerComparison] Error fetching peers:', err);
      setError(err instanceof Error ? err.message : 'Unknown error occurred');
    } finally {
      setLoading(false);
    }
  }, []);

  // Add a peer to selected list
  const addPeer = useCallback((peer: PeerCompany) => {
    setSelectedPeers((prev) => {
      if (prev.find((p) => p.symbol === peer.symbol)) {
        return prev;
      }
      return [...prev, peer];
    });
  }, []);

  // Remove a peer from selected list
  const removePeer = useCallback((symbol: string) => {
    setSelectedPeers((prev) => prev.filter((p) => p.symbol !== symbol));
  }, []);

  // Select a peer from available list
  const selectPeer = useCallback((symbol: string) => {
    const peer = availablePeers.find((p) => p.symbol === symbol);
    if (peer) {
      addPeer(peer);
    }
  }, [availablePeers, addPeer]);

  // Deselect a peer
  const deselectPeer = useCallback((symbol: string) => {
    removePeer(symbol);
  }, [removePeer]);

  // Select all available peers
  const selectAllPeers = useCallback(() => {
    setSelectedPeers(availablePeers);
  }, [availablePeers]);

  // Clear all selected peers
  const clearPeers = useCallback(() => {
    setSelectedPeers([]);
  }, []);

  // Run peer comparison
  const runComparison = useCallback(async () => {
    if (!targetSymbol || selectedPeers.length === 0) {
      setError('Please select a target symbol and at least one peer');
      return;
    }

    setLoading(true);
    setError(null);

    try {
      const input: ComparisonInput = {
        targetSymbol,
        peerSymbols: selectedPeers.map((p) => p.symbol),
        metrics: [
          'peRatio',
          'pbRatio',
          'psRatio',
          'roe',
          'roa',
          'netMargin',
          'revenueGrowth',
          'earningsGrowth',
          'debtToEquity',
        ],
      };

      const response = await invoke<PeerCommandResponse<PeerComparisonResult>>('compare_peers', { input });

      if (response.success && response.data) {
        setComparisonData(response.data);
      } else {
        setError(response.error || 'Failed to run comparison');
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unknown error occurred');
      console.error('Error running comparison:', err);
    } finally {
      setLoading(false);
    }
  }, [targetSymbol, selectedPeers]);

  // Calculate percentile ranking for a metric
  const calculatePercentiles = useCallback(
    async (input: PercentileInput): Promise<MetricRanking | null> => {
      try {
        const response = await invoke<PeerCommandResponse<MetricRanking>>('calculate_peer_percentiles', { input });

        if (response.success && response.data) {
          return response.data;
        } else {
          console.error('Failed to calculate percentiles:', response.error);
          return null;
        }
      } catch (err) {
        console.error('Error calculating percentiles:', err);
        return null;
      }
    },
    []
  );

  // Reset all state
  const reset = useCallback(() => {
    setTargetSymbol('');
    setSelectedPeers([]);
    setAvailablePeers([]);
    setComparisonData(null);
    setError(null);
  }, []);

  // Auto-fetch peers when target symbol changes
  useEffect(() => {
    console.log('[usePeerComparison] useEffect triggered. targetSymbol:', targetSymbol, 'availablePeers.length:', availablePeers.length);
    if (targetSymbol && targetSymbol.length >= 1) {
      // Create a mock search input for demo purposes
      // In production, you'd fetch company info first
      const mockInput: PeerSearchInput = {
        symbol: targetSymbol,
        sector: 'Information Technology',
        industry: 'Software',
        marketCap: 2000000000000,
        minSimilarity: 0.5,
        maxPeers: 15,
      };

      // Only fetch if we don't have peers yet
      if (availablePeers.length === 0) {
        console.log('[usePeerComparison] Triggering fetchPeers...');
        fetchPeers(mockInput);
      } else {
        console.log('[usePeerComparison] Peers already loaded, skipping fetch');
      }
    }
  }, [targetSymbol]); // Intentionally not including fetchPeers and availablePeers to avoid loops

  return {
    // State
    targetSymbol,
    selectedPeers,
    availablePeers,
    comparisonData,
    loading,
    error,

    // Actions
    setTargetSymbol,
    fetchPeers,
    addPeer,
    removePeer,
    selectPeer,
    deselectPeer,
    selectAllPeers,
    clearPeers,
    runComparison,
    calculatePercentiles,
    reset,
  };
}
