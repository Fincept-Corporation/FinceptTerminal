/**
 * CFA Quant Price Data Hook
 * Handles fetching and managing price data
 */

import { useState, useCallback, useEffect, useMemo } from 'react';
import { yfinanceService } from '@/services/markets/yfinanceService';
import { calculateStats } from '../utils';
import type { DataStats, DataSourceType, ChartDataPoint } from '../types';

interface UsePriceDataProps {
  dataSourceType: DataSourceType;
  manualData: string;
}

interface UsePriceDataReturn {
  priceData: number[];
  setPriceData: (data: number[]) => void;
  priceDates: string[];
  setPriceDates: (dates: string[]) => void;
  priceDataLoading: boolean;
  dataStats: DataStats | null;
  setDataStats: (stats: DataStats | null) => void;
  chartData: ChartDataPoint[];
  fetchSymbolData: (symbol: string, days: number) => Promise<void>;
  error: string | null;
  setError: (error: string | null) => void;
  getDataArray: (symbolPriceData: number[], manualData: string, dataSourceType: DataSourceType) => number[];
  getDateForIndex: (index: number) => string;
}

export function usePriceData({
  dataSourceType,
  manualData,
}: UsePriceDataProps): UsePriceDataReturn {
  const [priceData, setPriceData] = useState<number[]>([]);
  const [priceDates, setPriceDates] = useState<string[]>([]);
  const [priceDataLoading, setPriceDataLoading] = useState(false);
  const [dataStats, setDataStats] = useState<DataStats | null>(null);
  const [error, setError] = useState<string | null>(null);

  const chartData = useMemo(() => {
    return priceData.map((value, index) => ({
      index,
      value,
      date: priceDates[index] || `Day ${index + 1}`,
    }));
  }, [priceData, priceDates]);

  const fetchSymbolData = useCallback(async (symbol: string, days: number) => {
    if (!symbol.trim()) return;

    setPriceDataLoading(true);
    setError(null);

    try {
      const endDate = new Date();
      const startDate = new Date();
      startDate.setDate(startDate.getDate() - days);

      const data = await yfinanceService.getHistoricalData(
        symbol.toUpperCase(),
        startDate.toISOString().split('T')[0],
        endDate.toISOString().split('T')[0]
      );

      if (data && data.length > 0) {
        const closes = data.map(d => d.close);
        const dates = data.map(d => {
          const date = new Date(d.timestamp);
          return date.toLocaleDateString('en-US', { month: 'short', day: 'numeric', year: '2-digit' });
        });
        setPriceData(closes);
        setPriceDates(dates);
        setDataStats(calculateStats(closes));
      } else {
        setError('No data returned for symbol');
      }
    } catch (err) {
      setError(`Failed to fetch data: ${err}`);
    } finally {
      setPriceDataLoading(false);
    }
  }, []);

  // Handle manual data parsing
  useEffect(() => {
    if (dataSourceType === 'manual' && manualData) {
      const parsed = manualData.split(',').map(v => parseFloat(v.trim())).filter(v => !isNaN(v));
      setPriceData(parsed);
      setDataStats(calculateStats(parsed));
    }
  }, [manualData, dataSourceType]);

  const getDataArray = useCallback((
    symbolPriceData: number[],
    manualDataStr: string,
    sourceType: DataSourceType
  ): number[] => {
    if (sourceType === 'manual') {
      return manualDataStr.split(',').map(v => parseFloat(v.trim())).filter(v => !isNaN(v));
    }
    return symbolPriceData;
  }, []);

  const getDateForIndex = useCallback((index: number): string => {
    if (index >= 0 && index < priceDates.length) {
      return priceDates[index];
    }
    return `Day ${index + 1}`;
  }, [priceDates]);

  return {
    priceData,
    setPriceData,
    priceDates,
    setPriceDates,
    priceDataLoading,
    dataStats,
    setDataStats,
    chartData,
    fetchSymbolData,
    error,
    setError,
    getDataArray,
    getDateForIndex,
  };
}
