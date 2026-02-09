import { useState, useCallback } from 'react';
import type { ScanResult } from '../types';
import { runAlgoScan } from '../services/algoTradingService';

export function useScanner() {
  const [results, setResults] = useState<ScanResult[]>([]);
  const [scanned, setScanned] = useState(0);
  const [scanning, setScanning] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const scan = useCallback(async (conditions: string, symbols: string, timeframe?: string) => {
    setScanning(true);
    setError(null);
    setResults([]);
    setScanned(0);

    const result = await runAlgoScan(conditions, symbols, timeframe);
    setScanning(false);

    if (result.success && result.data) {
      setResults(result.data.matches);
      setScanned(result.data.scanned);
    } else {
      setError(result.error || 'Scan failed');
    }

    return result;
  }, []);

  const clear = useCallback(() => {
    setResults([]);
    setScanned(0);
    setError(null);
  }, []);

  return { results, scanned, scanning, error, scan, clear };
}
