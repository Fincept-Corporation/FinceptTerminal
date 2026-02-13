// useCongressGovData Hook - Fetch and manage Congress.gov data

import { useState, useCallback } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { sqliteService } from '@/services/core/sqliteService';
import { getDateFromPreset } from '@/components/common/charts';
import type { DateRangePreset } from '@/components/common/charts';
import type {
  CongressGovView,
  CongressBill,
  CongressBillsResponse,
  CongressBillDetail,
  CongressBillSummary,
} from '../types';

interface UseCongressGovDataReturn {
  // View
  activeView: CongressGovView;
  setActiveView: (view: CongressGovView) => void;

  // API key
  apiKey: string;
  apiKeyLoaded: boolean;
  loadApiKey: () => Promise<void>;
  saveApiKey: (key: string) => Promise<void>;

  // Date range
  dateRangePreset: DateRangePreset;
  setDateRangePreset: (preset: DateRangePreset) => void;
  startDate: string;
  endDate: string;
  setStartDate: (date: string) => void;
  setEndDate: (date: string) => void;

  // Bills list
  bills: CongressBill[];
  billsTotalCount: number;
  congressNumber: number;
  setCongressNumber: (n: number) => void;
  billType: string;
  setBillType: (t: string) => void;
  billsOffset: number;
  setBillsOffset: (n: number) => void;
  fetchBills: () => Promise<void>;

  // Bill detail
  billDetail: CongressBillDetail | null;
  fetchBillDetail: (billUrl: string) => Promise<void>;
  selectedBillTitle: string;

  // Summary
  billSummary: CongressBillSummary | null;
  fetchSummary: () => Promise<void>;

  // Export
  exportCSV: () => void;

  // Shared
  loading: boolean;
  error: string | null;
}

const currentCongressNumber = (): number => {
  const year = new Date().getFullYear();
  return 74 + Math.floor((year - 1935) / 2);
};

export function useCongressGovData(): UseCongressGovDataReturn {
  const [activeView, setActiveView] = useState<CongressGovView>('bills');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // API key
  const [apiKey, setApiKey] = useState('');
  const [apiKeyLoaded, setApiKeyLoaded] = useState(false);

  // Date range
  const initialDates = getDateFromPreset('1Y');
  const [dateRangePreset, setDateRangePresetState] = useState<DateRangePreset>('1Y');
  const [startDate, setStartDate] = useState(initialDates.start);
  const [endDate, setEndDate] = useState(new Date().toISOString().split('T')[0]);

  const setDateRangePreset = useCallback((preset: DateRangePreset) => {
    setDateRangePresetState(preset);
    const dates = getDateFromPreset(preset);
    setStartDate(dates.start);
    setEndDate(new Date().toISOString().split('T')[0]);
  }, []);

  // Bills
  const [bills, setBills] = useState<CongressBill[]>([]);
  const [billsTotalCount, setBillsTotalCount] = useState(0);
  const [congressNumber, setCongressNumber] = useState(currentCongressNumber());
  const [billType, setBillType] = useState('');
  const [billsOffset, setBillsOffset] = useState(0);

  // Bill detail
  const [billDetail, setBillDetail] = useState<CongressBillDetail | null>(null);
  const [selectedBillTitle, setSelectedBillTitle] = useState('');

  // Summary
  const [billSummary, setBillSummary] = useState<CongressBillSummary | null>(null);

  const loadApiKey = useCallback(async () => {
    try {
      const key = await sqliteService.getSetting('CONGRESS_GOV_API_KEY');
      if (key) {
        setApiKey(key);
      }
    } catch {
      // ignore
    } finally {
      setApiKeyLoaded(true);
    }
  }, []);

  const saveApiKey = useCallback(async (key: string) => {
    try {
      await sqliteService.saveSetting('CONGRESS_GOV_API_KEY', key, 'api_keys');
      setApiKey(key);
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to save API key: ${msg}`);
    }
  }, []);

  const fetchBills = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const args: string[] = [
        String(congressNumber),
        billType || '',
        startDate,
        endDate,
        '100',          // limit
        String(billsOffset),
        'desc',
        'false',
      ];

      const env: Record<string, string> = {};
      if (apiKey) env['CONGRESS_GOV_API_KEY'] = apiKey;

      const result = await invoke('execute_congress_gov_command', {
        command: 'congress_bills',
        args,
      }) as string;

      const parsed: CongressBillsResponse = JSON.parse(result);
      if (!parsed.success) {
        setError(parsed.error || 'Failed to fetch bills');
        setBills([]);
        setBillsTotalCount(0);
      } else {
        setBills(parsed.bills || []);
        setBillsTotalCount(parsed.pagination?.count || parsed.total_bills || 0);
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to fetch bills: ${msg}`);
      setBills([]);
    } finally {
      setLoading(false);
    }
  }, [congressNumber, billType, startDate, endDate, billsOffset, apiKey]);

  const fetchBillDetail = useCallback(async (billUrl: string) => {
    setLoading(true);
    setError(null);
    try {
      const result = await invoke('execute_congress_gov_command', {
        command: 'bill_info',
        args: [billUrl],
      }) as string;

      const parsed: CongressBillDetail = JSON.parse(result);
      if (!parsed.success) {
        setError(parsed.error || 'Failed to fetch bill detail');
        setBillDetail(null);
      } else {
        setBillDetail(parsed);
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to fetch bill detail: ${msg}`);
      setBillDetail(null);
    } finally {
      setLoading(false);
    }
  }, []);

  const fetchSummary = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const result = await invoke('execute_congress_gov_command', {
        command: 'summary',
        args: [String(congressNumber), '10'],
      }) as string;

      const parsed: CongressBillSummary = JSON.parse(result);
      if (!parsed.success) {
        setError(parsed.error || 'Failed to fetch summary');
        setBillSummary(null);
      } else {
        setBillSummary(parsed);
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to fetch summary: ${msg}`);
      setBillSummary(null);
    } finally {
      setLoading(false);
    }
  }, [congressNumber]);

  const exportCSV = useCallback(() => {
    if (activeView !== 'bills' || bills.length === 0) return;

    const csv = [
      `# Congress.gov Bills - ${congressNumber}th Congress`,
      '',
      'Congress,Type,Number,Title,Chamber,Update Date,Latest Action Date,Latest Action',
      ...bills.map(b =>
        `${b.congress},${b.bill_type},${b.bill_number},"${(b.title || '').replace(/"/g, '""')}",${b.origin_chamber},${b.update_date},${b.latest_action_date || ''},"${(b.latest_action || '').replace(/"/g, '""')}"`
      ),
    ].join('\n');

    const blob = new Blob([csv], { type: 'text/csv' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `fincept_congress_bills_${congressNumber}.csv`;
    a.click();
    URL.revokeObjectURL(url);
  }, [activeView, bills, congressNumber]);

  return {
    activeView, setActiveView,
    apiKey, apiKeyLoaded, loadApiKey, saveApiKey,
    dateRangePreset, setDateRangePreset,
    startDate, endDate, setStartDate, setEndDate,
    bills, billsTotalCount, congressNumber, setCongressNumber,
    billType, setBillType, billsOffset, setBillsOffset,
    fetchBills,
    billDetail, fetchBillDetail, selectedBillTitle,
    billSummary, fetchSummary,
    exportCSV,
    loading, error,
  };
}

export default useCongressGovData;
