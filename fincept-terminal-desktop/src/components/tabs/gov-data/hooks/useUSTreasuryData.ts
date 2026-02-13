// useUSTreasuryData Hook - Fetch and manage US Treasury data

import { useState, useCallback } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { getDateFromPreset } from '@/components/common/charts';
import type { DateRangePreset } from '@/components/common/charts';
import type {
  USTreasuryEndpoint,
  SecurityTypeFilter,
  TreasuryPricesResponse,
  TreasuryAuctionsResponse,
  TreasurySummaryResponse,
} from '../types';

interface UseUSTreasuryDataReturn {
  // Endpoint selection
  activeEndpoint: USTreasuryEndpoint;
  setActiveEndpoint: (endpoint: USTreasuryEndpoint) => void;

  // Unified date range (matches economics tab pattern)
  dateRangePreset: DateRangePreset;
  setDateRangePreset: (preset: DateRangePreset) => void;
  startDate: string;
  endDate: string;
  setStartDate: (date: string) => void;
  setEndDate: (date: string) => void;

  // Prices state
  pricesData: TreasuryPricesResponse | null;
  pricesSecurityType: SecurityTypeFilter;
  setPricesSecurityType: (type: SecurityTypeFilter) => void;
  pricesCusip: string;
  setPricesCusip: (cusip: string) => void;
  fetchPrices: () => Promise<void>;

  // Auctions state
  auctionsData: TreasuryAuctionsResponse | null;
  auctionsSecurityType: string;
  setAuctionsSecurityType: (type: string) => void;
  auctionsPageSize: number;
  setAuctionsPageSize: (size: number) => void;
  auctionsPageNum: number;
  setAuctionsPageNum: (page: number) => void;
  fetchAuctions: () => Promise<void>;

  // Summary state
  summaryData: TreasurySummaryResponse | null;
  fetchSummary: () => Promise<void>;

  // Export (CSV only — image uses shared useExport in Provider)
  exportCSV: () => void;

  // Shared state
  loading: boolean;
  error: string | null;
}

const getDefaultDate = (): string => {
  const now = new Date();
  // Treasury prices are published EOD, so today's data won't exist yet.
  // Default to the previous business day.
  now.setDate(now.getDate() - 1);
  const day = now.getDay();
  if (day === 0) now.setDate(now.getDate() - 2); // Sunday -> Friday
  else if (day === 6) now.setDate(now.getDate() - 1); // Saturday -> Friday
  return now.toISOString().split('T')[0];
};

export function useUSTreasuryData(): UseUSTreasuryDataReturn {
  const [activeEndpoint, setActiveEndpoint] = useState<USTreasuryEndpoint>('treasury_prices');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // Unified date range — same pattern as economics tab
  const initialDates = getDateFromPreset('1M');
  const [dateRangePreset, setDateRangePresetState] = useState<DateRangePreset>('1M');
  const [startDate, setStartDate] = useState(initialDates.start);
  const [endDate, setEndDate] = useState(getDefaultDate());

  const setDateRangePreset = useCallback((preset: DateRangePreset) => {
    setDateRangePresetState(preset);
    const dates = getDateFromPreset(preset);
    setStartDate(dates.start);
    setEndDate(getDefaultDate());
  }, []);

  // Prices state
  const [pricesData, setPricesData] = useState<TreasuryPricesResponse | null>(null);
  const [pricesSecurityType, setPricesSecurityType] = useState<SecurityTypeFilter>('all');
  const [pricesCusip, setPricesCusip] = useState('');

  // Auctions state
  const [auctionsData, setAuctionsData] = useState<TreasuryAuctionsResponse | null>(null);
  const [auctionsSecurityType, setAuctionsSecurityType] = useState('all');
  const [auctionsPageSize, setAuctionsPageSize] = useState(50);
  const [auctionsPageNum, setAuctionsPageNum] = useState(1);

  // Summary state
  const [summaryData, setSummaryData] = useState<TreasurySummaryResponse | null>(null);

  const fetchPrices = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      // Prices use endDate as the single date parameter
      const args: string[] = [endDate];
      if (pricesCusip.trim()) {
        args.push(pricesCusip.trim());
      }
      if (pricesSecurityType !== 'all') {
        if (!pricesCusip.trim()) args.push('');
        args.push(pricesSecurityType);
      }

      const result = await invoke('execute_government_us_command', {
        command: 'treasury_prices',
        args,
      }) as string;

      const parsed: TreasuryPricesResponse = JSON.parse(result);
      if (!parsed.success) {
        setError(parsed.error || 'Failed to fetch treasury prices');
        setPricesData(null);
      } else {
        setPricesData(parsed);
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to fetch prices: ${msg}`);
      setPricesData(null);
    } finally {
      setLoading(false);
    }
  }, [endDate, pricesSecurityType, pricesCusip]);

  const fetchAuctions = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const args: string[] = [startDate, endDate];
      if (auctionsSecurityType !== 'all') {
        args.push(auctionsSecurityType);
      } else {
        args.push('');
      }
      args.push(String(auctionsPageSize));
      args.push(String(auctionsPageNum));

      const result = await invoke('execute_government_us_command', {
        command: 'treasury_auctions',
        args,
      }) as string;

      const parsed: TreasuryAuctionsResponse = JSON.parse(result);
      if (!parsed.success) {
        setError(parsed.error || 'Failed to fetch auctions');
        setAuctionsData(null);
      } else {
        setAuctionsData(parsed);
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to fetch auctions: ${msg}`);
      setAuctionsData(null);
    } finally {
      setLoading(false);
    }
  }, [startDate, endDate, auctionsSecurityType, auctionsPageSize, auctionsPageNum]);

  const fetchSummary = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const result = await invoke('execute_government_us_command', {
        command: 'summary',
        args: [endDate],
      }) as string;

      const parsed: TreasurySummaryResponse = JSON.parse(result);
      if (!parsed.success) {
        setError(parsed.error || 'Failed to fetch summary');
        setSummaryData(null);
      } else {
        setSummaryData(parsed);
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to fetch summary: ${msg}`);
      setSummaryData(null);
    } finally {
      setLoading(false);
    }
  }, [endDate]);

  // CSV export — browser download
  const exportCSV = useCallback(() => {
    let csv = '';
    let filename = 'fincept_govt_';

    if (activeEndpoint === 'treasury_prices' && pricesData) {
      csv = [
        `# US Treasury Prices - ${pricesData.date}`,
        `# Records: ${pricesData.total_records}`,
        '',
        'CUSIP,Security Type,Rate,Maturity Date,Bid,Offer,EOD Price',
        ...pricesData.data.map(r =>
          `${r.cusip},${r.security_type},${r.rate ?? ''},${r.maturity_date ?? ''},${r.bid ?? ''},${r.offer ?? ''},${r.eod_price ?? ''}`
        ),
      ].join('\n');
      filename += `prices_${pricesData.date}.csv`;
    } else if (activeEndpoint === 'treasury_auctions' && auctionsData) {
      const keys = ['cusip', 'securityType', 'securityTerm', 'auctionDate', 'issueDate', 'maturityDate',
        'highDiscountRate', 'highPrice', 'bidToCoverRatio', 'offeringAmount', 'totalAccepted', 'allocationPercentage'];
      csv = [
        `# US Treasury Auctions - ${auctionsData.filters.start_date} to ${auctionsData.filters.end_date}`,
        `# Records: ${auctionsData.total_records}`,
        '',
        keys.join(','),
        ...auctionsData.data.map(r => keys.map(k => r[k] ?? '').join(',')),
      ].join('\n');
      filename += `auctions_${auctionsData.filters.start_date}_${auctionsData.filters.end_date}.csv`;
    } else if (activeEndpoint === 'summary' && summaryData) {
      csv = [
        `# US Treasury Summary - ${summaryData.date}`,
        `# Total Securities: ${summaryData.total_securities}`,
        '',
        'Metric,Value',
        `Total Securities,${summaryData.total_securities}`,
        `Min Rate,${summaryData.yield_summary.min_rate}`,
        `Max Rate,${summaryData.yield_summary.max_rate}`,
        `Avg Rate,${summaryData.yield_summary.avg_rate}`,
        `Min Price,${summaryData.price_summary.min_price}`,
        `Max Price,${summaryData.price_summary.max_price}`,
        `Avg Price,${summaryData.price_summary.avg_price}`,
        '',
        'Security Type,Count',
        ...Object.entries(summaryData.security_types).map(([k, v]) => `${k},${v}`),
      ].join('\n');
      filename += `summary_${summaryData.date}.csv`;
    } else {
      return;
    }

    const blob = new Blob([csv], { type: 'text/csv' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    a.click();
    URL.revokeObjectURL(url);
  }, [activeEndpoint, pricesData, auctionsData, summaryData]);

  return {
    activeEndpoint,
    setActiveEndpoint,
    dateRangePreset,
    setDateRangePreset,
    startDate,
    endDate,
    setStartDate,
    setEndDate,
    pricesData,
    pricesSecurityType,
    setPricesSecurityType,
    pricesCusip,
    setPricesCusip,
    fetchPrices,
    auctionsData,
    auctionsSecurityType,
    setAuctionsSecurityType,
    auctionsPageSize,
    setAuctionsPageSize,
    auctionsPageNum,
    setAuctionsPageNum,
    fetchAuctions,
    summaryData,
    fetchSummary,
    exportCSV,
    loading,
    error,
  };
}

export default useUSTreasuryData;
