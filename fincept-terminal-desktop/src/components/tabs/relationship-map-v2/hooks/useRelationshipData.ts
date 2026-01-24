// Progressive Data Loading Hook for Relationship Map V2

import { useState, useCallback, useRef } from 'react';
import { relationshipMapServiceV2 } from '../../../../services/geopolitics/relationshipMapServiceV2';
import type { RelationshipMapDataV2, LoadingPhase } from '../types';

interface UseRelationshipDataReturn {
  data: RelationshipMapDataV2 | null;
  loading: boolean;
  error: string | null;
  phase: LoadingPhase;
  fetchData: (ticker: string) => Promise<void>;
  clearData: () => void;
}

export function useRelationshipData(): UseRelationshipDataReturn {
  const [data, setData] = useState<RelationshipMapDataV2 | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [phase, setPhase] = useState<LoadingPhase>({
    phase: 'idle',
    progress: 0,
    message: '',
  });
  const abortRef = useRef(false);

  const fetchData = useCallback(async (ticker: string) => {
    if (!ticker.trim()) return;

    abortRef.current = false;
    setLoading(true);
    setError(null);
    setData(null);
    setPhase({ phase: 'company', progress: 0, message: 'Starting...' });

    try {
      const result = await relationshipMapServiceV2.getRelationshipMap(
        ticker,
        (loadingPhase) => {
          if (abortRef.current) return;
          setPhase(loadingPhase);
        }
      );

      if (!abortRef.current) {
        setData(result);
        setPhase({ phase: 'complete', progress: 100, message: 'Complete' });
      }
    } catch (err) {
      if (!abortRef.current) {
        const message = err instanceof Error ? err.message : 'Failed to load data';
        setError(message);
        setPhase({ phase: 'error', progress: 0, message });
      }
    } finally {
      if (!abortRef.current) {
        setLoading(false);
      }
    }
  }, []);

  const clearData = useCallback(() => {
    abortRef.current = true;
    setData(null);
    setLoading(false);
    setError(null);
    setPhase({ phase: 'idle', progress: 0, message: '' });
  }, []);

  return { data, loading, error, phase, fetchData, clearData };
}
