import { useState, useCallback } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { cacheService, CacheService, TTL } from '../../../../services/cache/cacheService';
import type { StockInfo, HistoricalData, QuoteData, FinancialsData, ChartPeriod, TechnicalsData } from '../types';

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

const CACHE_CATEGORY = 'market-quotes';
const HISTORICAL_CATEGORY = 'market-historical';
const REFERENCE_CATEGORY = 'reference';

export function useStockData(): UseStockDataReturn {
  const [stockInfo, setStockInfo] = useState<StockInfo | null>(null);
  const [quoteData, setQuoteData] = useState<QuoteData | null>(null);
  const [historicalData, setHistoricalData] = useState<HistoricalData[]>([]);
  const [financials, setFinancials] = useState<FinancialsData | null>(null);
  const [technicalsData, setTechnicalsData] = useState<TechnicalsData | null>(null);
  const [loading, setLoading] = useState(false);
  const [technicalsLoading, setTechnicalsLoading] = useState(false);

  const fetchStockData = useCallback(async (symbol: string, period: ChartPeriod, forceRefresh = false) => {
    setLoading(true);
    console.log('Fetching data for:', symbol, '| Force refresh:', forceRefresh);

    const quoteKey = CacheService.key(CACHE_CATEGORY, 'quote', symbol);
    const infoKey = CacheService.key(REFERENCE_CATEGORY, 'stock-info', symbol);
    const historicalKey = CacheService.key(HISTORICAL_CATEGORY, 'historical', symbol, period);
    const financialsKey = CacheService.key(REFERENCE_CATEGORY, 'financials', symbol);

    try {
      // Check cache first (unless force refresh)
      if (!forceRefresh) {
        const [cachedQuote, cachedInfo, cachedHistorical, cachedFinancials] = await Promise.all([
          cacheService.get<QuoteData>(quoteKey),
          cacheService.get<StockInfo>(infoKey),
          cacheService.get<HistoricalData[]>(historicalKey),
          cacheService.get<FinancialsData>(financialsKey),
        ]);

        // If all core data is cached and fresh, use it
        if (cachedQuote && cachedInfo && cachedFinancials) {
          console.log('Using cached data for', symbol);
          setQuoteData(cachedQuote.data);
          setStockInfo(cachedInfo.data);
          setFinancials(cachedFinancials.data);

          if (cachedHistorical) {
            setHistoricalData(cachedHistorical.data);
            setLoading(false);
            return;
          }
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
        await cacheService.set(quoteKey, quoteResponse.data, CACHE_CATEGORY, TTL['5m']);
      }

      // Fetch stock info
      const infoResponse: any = await invoke('get_stock_info', { symbol });
      let newInfo: StockInfo | null = null;
      if (infoResponse.success && infoResponse.data) {
        setStockInfo(infoResponse.data);
        newInfo = infoResponse.data;
        await cacheService.set(infoKey, infoResponse.data, REFERENCE_CATEGORY, TTL['24h']);
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
      if (historicalResponse.success && historicalResponse.data) {
        setHistoricalData(historicalResponse.data);
        await cacheService.set(historicalKey, historicalResponse.data, HISTORICAL_CATEGORY, TTL['1h']);
      }

      // Fetch financials
      const financialsResponse: any = await invoke('get_financials', { symbol });
      if (financialsResponse.success && financialsResponse.data) {
        setFinancials(financialsResponse.data);
        await cacheService.set(financialsKey, financialsResponse.data, REFERENCE_CATEGORY, TTL['24h']);
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

      // Check cache for technicals
      const techKey = CacheService.keyWithHash('market-quotes', 'technicals', historical.map(d => String(d.timestamp)));
      const cached = await cacheService.get<TechnicalsData>(techKey);
      if (cached) {
        console.log('Using cached technicals data');
        setTechnicalsData(cached.data);
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

      let parsedData: TechnicalsData | null = null;

      if (typeof response === 'string') {
        try {
          const parsed = JSON.parse(response);
          if (parsed.success && parsed.data && Array.isArray(parsed.data) && parsed.data.length > 0) {
            parsedData = parsed;
          } else {
            console.error('Error computing technicals:', parsed.error || 'Invalid data structure');
          }
        } catch (parseError) {
          console.error('Error parsing technicals response:', parseError);
        }
      } else if (response && response.success && response.data && Array.isArray(response.data) && response.data.length > 0) {
        parsedData = response;
      } else {
        console.error('Invalid technicals response structure');
      }

      setTechnicalsData(parsedData);

      // Cache the result
      if (parsedData) {
        await cacheService.set(techKey, parsedData, CACHE_CATEGORY, TTL['15m']);
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
