import { useState, useCallback } from 'react';
import { getCandleCache } from '../services/algoTradingService';

export function useCandleData() {
  const [candles, setCandles] = useState<unknown[]>([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const fetch = useCallback(async (symbol: string, timeframe?: string, limit?: number) => {
    setLoading(true);
    setError(null);

    const result = await getCandleCache(symbol, timeframe, limit);
    setLoading(false);

    if (result.success && result.data) {
      setCandles(result.data);
    } else {
      setError(result.error || 'Failed to fetch candles');
    }

    return result;
  }, []);

  const clear = useCallback(() => {
    setCandles([]);
    setError(null);
  }, []);

  return { candles, loading, error, fetch, clear };
}
