import { useState, useRef, useEffect, useCallback } from 'react';
import { invoke } from '@tauri-apps/api/core';
import type { StockInfo, HistoricalData, QuoteData, FinancialsData, ChartPeriod, TechnicalsData } from '../types';

interface DataCache {
  stockInfo: StockInfo;
  quoteData: QuoteData;
  historicalData: Record<string, HistoricalData[]>;
  financials: FinancialsData;
  timestamp: number;
}

interface UseStockDataReturn {
  stockInfo: StockInfo | null;
  quoteData: QuoteData | null;
  historicalData: HistoricalData[];
  financials: FinancialsData | null;
  technicalsData: TechnicalsData | null;
  loading: boolean;
  technicalsLoading: boolean;
  fetchStockData: (symbol: string, period: ChartPeriod, forceRefresh?: boolean) => Promise<void>;
  computeTechnicals: (historical: HistoricalData[]) => Promise<void>;
}

const CACHE_DURATION = 5 * 60 * 1000; // 5 minutes

export function useStockData(): UseStockDataReturn {
  const [stockInfo, setStockInfo] = useState<StockInfo | null>(null);
  const [quoteData, setQuoteData] = useState<QuoteData | null>(null);
  const [historicalData, setHistoricalData] = useState<HistoricalData[]>([]);
  const [financials, setFinancials] = useState<FinancialsData | null>(null);
  const [technicalsData, setTechnicalsData] = useState<TechnicalsData | null>(null);
  const [loading, setLoading] = useState(false);
  const [technicalsLoading, setTechnicalsLoading] = useState(false);

  const dataCache = useRef<Record<string, DataCache>>({});

  const fetchStockData = useCallback(async (symbol: string, period: ChartPeriod, forceRefresh = false) => {
    setLoading(true);
    console.log('Fetching data for:', symbol, '| Force refresh:', forceRefresh);

    try {
      // Check cache first
      const cached = dataCache.current[symbol];
      const now = Date.now();

      if (cached && !forceRefresh && (now - cached.timestamp < CACHE_DURATION)) {
        console.log('Using cached data for', symbol);
        setStockInfo(cached.stockInfo);
        setQuoteData(cached.quoteData);
        setFinancials(cached.financials);

        // Check if we have historical data for this period
        if (cached.historicalData[period]) {
          setHistoricalData(cached.historicalData[period]);
          setLoading(false);
          return;
        }
      }

      // Fetch fresh data
      console.log('Fetching fresh data for', symbol);

      // Fetch quote
      const quoteResponse: any = await invoke('get_market_quote', { symbol });
      let newQuote: QuoteData | null = null;
      if (quoteResponse.success && quoteResponse.data) {
        setQuoteData(quoteResponse.data);
        newQuote = quoteResponse.data;
      }

      // Fetch stock info
      const infoResponse: any = await invoke('get_stock_info', { symbol });
      let newInfo: StockInfo | null = null;
      if (infoResponse.success && infoResponse.data) {
        setStockInfo(infoResponse.data);
        newInfo = infoResponse.data;
      }

      // Fetch historical data
      const endDate = new Date();
      const startDate = new Date();
      switch (period) {
        case '1M': startDate.setMonth(endDate.getMonth() - 1); break;
        case '3M': startDate.setMonth(endDate.getMonth() - 3); break;
        case '6M': startDate.setMonth(endDate.getMonth() - 6); break;
        case '1Y': startDate.setFullYear(endDate.getFullYear() - 1); break;
        case '5Y': startDate.setFullYear(endDate.getFullYear() - 5); break;
      }

      const historicalResponse: any = await invoke('get_historical_data', {
        symbol,
        startDate: startDate.toISOString().split('T')[0],
        endDate: endDate.toISOString().split('T')[0],
      });
      let newHistorical: HistoricalData[] = [];
      if (historicalResponse.success && historicalResponse.data) {
        setHistoricalData(historicalResponse.data);
        newHistorical = historicalResponse.data;
      }

      // Fetch financials
      const financialsResponse: any = await invoke('get_financials', { symbol });
      let newFinancials: FinancialsData | null = null;
      if (financialsResponse.success && financialsResponse.data) {
        setFinancials(financialsResponse.data);
        newFinancials = financialsResponse.data;
      }

      // Update cache
      if (newInfo && newQuote && newFinancials) {
        dataCache.current[symbol] = {
          stockInfo: newInfo,
          quoteData: newQuote,
          historicalData: {
            ...((cached?.historicalData) || {}),
            [period]: newHistorical,
          },
          financials: newFinancials,
          timestamp: now,
        };
      }
    } catch (error) {
      console.error('Error fetching stock data:', error);
    } finally {
      setLoading(false);
    }
  }, []);

  const computeTechnicals = useCallback(async (historical: HistoricalData[]) => {
    setTechnicalsLoading(true);
    try {
      // Validate historical data
      if (!historical || !Array.isArray(historical) || historical.length === 0) {
        console.error('Invalid historical data for technicals computation');
        setTechnicalsData(null);
        return;
      }

      // Convert historical data to format expected by Python script
      const historicalJson = JSON.stringify(historical.map(d => ({
        timestamp: d.timestamp,
        open: d.open,
        high: d.high,
        low: d.low,
        close: d.close,
        volume: d.volume
      })));

      console.log('Sending historical data for technicals computation:', historical.length, 'data points');

      const response: any = await invoke('compute_all_technicals', {
        historicalData: historicalJson
      });

      console.log('Technicals response:', response);

      if (typeof response === 'string') {
        try {
          const parsed = JSON.parse(response);
          if (parsed.success && parsed.data && Array.isArray(parsed.data) && parsed.data.length > 0) {
            setTechnicalsData(parsed);
          } else {
            console.error('Error computing technicals:', parsed.error || 'Invalid data structure');
            setTechnicalsData(null);
          }
        } catch (parseError) {
          console.error('Error parsing technicals response:', parseError);
          setTechnicalsData(null);
        }
      } else if (response && response.success && response.data && Array.isArray(response.data) && response.data.length > 0) {
        setTechnicalsData(response);
      } else {
        console.error('Invalid technicals response structure');
        setTechnicalsData(null);
      }
    } catch (error) {
      console.error('Error computing technicals:', error);
      setTechnicalsData(null);
    } finally {
      setTechnicalsLoading(false);
    }
  }, []);

  return {
    stockInfo,
    quoteData,
    historicalData,
    financials,
    technicalsData,
    loading,
    technicalsLoading,
    fetchStockData,
    computeTechnicals,
  };
}

export default useStockData;
