// useEconomicsData Hook - Fetch and manage economic data

import { useState, useCallback, useEffect } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { sqliteService } from '@/services/core/sqliteService';
import { getDateFromPreset } from '@/components/common/charts';
import type { DateRangePreset } from '@/components/common/charts';
import {
  COUNTRIES,
  CFTC_MARKETS,
  EIA_INDICATORS,
  ADB_INDICATORS,
  FED_INDICATORS,
  BLS_INDICATORS,
  UNESCO_INDICATORS,
  FISCALDATA_INDICATORS,
  BEA_INDICATORS,
  getIndicatorsForSource,
  getDefaultIndicator,
  DATA_SOURCES,
} from '../constants';
import type { DataSource, IndicatorData, DataPoint, ChartStats } from '../types';

export type { DateRangePreset };

interface UseEconomicsDataReturn {
  dataSource: DataSource;
  setDataSource: (source: DataSource) => void;
  selectedCountry: string;
  setSelectedCountry: (code: string) => void;
  selectedIndicator: string;
  setSelectedIndicator: (id: string) => void;
  data: IndicatorData | null;
  loading: boolean;
  error: string | null;
  fetchData: () => Promise<void>;
  stats: ChartStats | null;
  // Date range for pagination
  dateRangePreset: DateRangePreset;
  setDateRangePreset: (preset: DateRangePreset) => void;
  startDate: string;
  endDate: string;
  setStartDate: (date: string) => void;
  setEndDate: (date: string) => void;
  // API Keys
  wtoApiKey: string;
  setWtoApiKey: (key: string) => void;
  eiaApiKey: string;
  setEiaApiKey: (key: string) => void;
  blsApiKey: string;
  setBlsApiKey: (key: string) => void;
  beaApiKey: string;
  setBeaApiKey: (key: string) => void;
  showApiKeyInput: boolean;
  setShowApiKeyInput: (show: boolean) => void;
  apiKeySaving: boolean;
  saveApiKey: (keyName: string, keyValue: string) => Promise<void>;
}

export function useEconomicsData(): UseEconomicsDataReturn {
  const [dataSource, setDataSourceState] = useState<DataSource>('worldbank');
  const [selectedCountry, setSelectedCountry] = useState('USA');
  const [selectedIndicator, setSelectedIndicator] = useState('NY.GDP.MKTP.CD');
  const [data, setData] = useState<IndicatorData | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // Date range state for pagination
  const [dateRangePreset, setDateRangePresetState] = useState<DateRangePreset>('5Y');
  const initialDates = getDateFromPreset('5Y');
  const [startDate, setStartDate] = useState(initialDates.start);
  const [endDate, setEndDate] = useState(initialDates.end);

  // Update dates when preset changes
  const setDateRangePreset = useCallback((preset: DateRangePreset) => {
    setDateRangePresetState(preset);
    const dates = getDateFromPreset(preset);
    setStartDate(dates.start);
    setEndDate(dates.end);
  }, []);

  // API Keys
  const [wtoApiKey, setWtoApiKey] = useState<string>('');
  const [eiaApiKey, setEiaApiKey] = useState<string>('');
  const [blsApiKey, setBlsApiKey] = useState<string>('');
  const [beaApiKey, setBeaApiKey] = useState<string>('');
  const [showApiKeyInput, setShowApiKeyInput] = useState(false);
  const [apiKeySaving, setApiKeySaving] = useState(false);

  // Load API keys on mount
  useEffect(() => {
    const loadApiKeys = async () => {
      try {
        const wtoKey = await sqliteService.getApiKey('WTO_API_KEY');
        if (wtoKey) setWtoApiKey(wtoKey);

        const eiaKey = await sqliteService.getApiKey('EIA_API_KEY');
        if (eiaKey) setEiaApiKey(eiaKey);

        const blsKey = await sqliteService.getApiKey('BLS_API_KEY');
        if (blsKey) setBlsApiKey(blsKey);

        const beaKey = await sqliteService.getApiKey('BEA_API_KEY');
        if (beaKey) setBeaApiKey(beaKey);
      } catch (err) {
        console.error('Failed to load API keys:', err);
      }
    };
    loadApiKeys();
  }, []);

  // Reset indicator and country when data source changes
  const setDataSource = useCallback((source: DataSource) => {
    setDataSourceState(source);
    setSelectedIndicator(getDefaultIndicator(source));
    // Set appropriate default country for source
    if (source === 'adb') {
      // ADB is Asia-Pacific only - default to India
      setSelectedCountry('IND');
    } else if (source === 'fred' || source === 'fed' || source === 'bls' || source === 'fiscaldata' || source === 'bea') {
      // FRED, Federal Reserve, BLS, FiscalData, and BEA are US-only
      setSelectedCountry('USA');
    }
    setData(null);
    setError(null);
  }, []);

  // Save API key
  const saveApiKey = useCallback(async (keyName: string, keyValue: string) => {
    if (!keyValue.trim()) return;
    setApiKeySaving(true);
    try {
      await sqliteService.setApiKey(keyName, keyValue);
      setShowApiKeyInput(false);
    } catch (err) {
      setError('Failed to save API key');
    } finally {
      setApiKeySaving(false);
    }
  }, []);

  // Fetch data
  const fetchData = useCallback(async () => {
    setLoading(true);
    setError(null);

    try {
      const country = COUNTRIES.find(c => c.code === selectedCountry);
      const indicators = getIndicatorsForSource(dataSource);
      const currentSourceConfig = DATA_SOURCES.find(s => s.id === dataSource)!;
      let result: string;
      let countryCode = selectedCountry;

      // Get appropriate country code for data source
      if (dataSource === 'bis' && country?.bis) {
        countryCode = country.bis;
      } else if (dataSource === 'imf' && country?.imf) {
        countryCode = country.imf;
      } else if (dataSource === 'fred') {
        countryCode = 'US'; // FRED is US-only
      } else if (dataSource === 'oecd' && country?.oecd) {
        countryCode = country.oecd;
      } else if (dataSource === 'wto' && country?.wto) {
        countryCode = country.wto;
      } else if (dataSource === 'adb' && country?.adb) {
        countryCode = country.adb;
      }

      // Check if API key is required
      if (dataSource === 'wto' && !wtoApiKey) {
        setError('WTO API Key required. Click the key icon to configure.');
        setShowApiKeyInput(true);
        setLoading(false);
        return;
      }

      if (dataSource === 'eia' && selectedIndicator.startsWith('steo_') && !eiaApiKey) {
        setError('EIA API Key required for STEO data. Click the key icon to configure.');
        setShowApiKeyInput(true);
        setLoading(false);
        return;
      }

      if (dataSource === 'bls' && !blsApiKey) {
        setError('BLS API Key required. Get one at https://data.bls.gov/registrationEngine/. Click the key icon to configure.');
        setShowApiKeyInput(true);
        setLoading(false);
        return;
      }

      if (dataSource === 'bea' && !beaApiKey) {
        setError('BEA API Key required. Register free at https://apps.bea.gov/API/signup/. Click the key icon to configure.');
        setShowApiKeyInput(true);
        setLoading(false);
        return;
      }

      // Fetch data based on source
      switch (dataSource) {
        case 'worldbank':
          result = await invoke('execute_python_script', {
            scriptName: 'worldbank_data.py',
            args: ['indicators', countryCode, selectedIndicator],
            env: {},
          }) as string;
          break;

        case 'bis':
          result = await invoke('execute_python_script', {
            scriptName: 'bis_data.py',
            args: ['fetch', selectedIndicator, countryCode],
            env: {},
          }) as string;
          break;

        case 'imf':
          result = await invoke('execute_python_script', {
            scriptName: 'imf_data.py',
            args: ['weo', countryCode, selectedIndicator],
            env: {},
          }) as string;
          break;

        case 'fred':
          result = await invoke('execute_python_script', {
            scriptName: 'fred_data.py',
            args: ['series', selectedIndicator],
            env: {},
          }) as string;
          break;

        case 'oecd':
          result = await invoke('execute_python_script', {
            scriptName: 'oecd_data.py',
            args: ['fetch', selectedIndicator, countryCode],
            env: {},
          }) as string;
          break;

        case 'wto':
          result = await invoke('execute_python_script', {
            scriptName: 'wto_data.py',
            args: ['timeseries_data', '--i', selectedIndicator, '--reporters', countryCode, wtoApiKey],
            env: {},
          }) as string;
          break;

        case 'cftc':
          result = await invoke('execute_python_script', {
            scriptName: 'cftc_data.py',
            args: ['cot_historical_trend', selectedIndicator, 'legacy', '104'],
            env: {},
          }) as string;
          break;

        case 'eia':
          if (selectedIndicator.startsWith('steo_')) {
            // STEO data requires API key
            const tableNum = selectedIndicator.replace('steo_', '');
            result = await invoke('execute_python_script', {
              scriptName: 'eia_data.py',
              args: ['get_steo', tableNum, '', 'month'],
              env: { EIA_API_KEY: eiaApiKey },
            }) as string;
          } else {
            // Petroleum status report data (no API key needed)
            result = await invoke('execute_python_script', {
              scriptName: 'eia_data.py',
              args: ['get_petroleum', selectedIndicator],
              env: {},
            }) as string;
          }
          break;

        case 'adb':
          // Check if country has ADB code
          if (!country?.adb) {
            throw new Error(`${country?.name || selectedCountry} is not available in ADB database. ADB covers Asia-Pacific region only.`);
          }
          // Extract years from date range for ADB API (it only accepts years)
          const adbStartYear = startDate.substring(0, 4);
          const adbEndYear = endDate.substring(0, 4);

          // Map indicators to ADB dataflows and commands
          // ADB uses two main dataflows:
          // - PPL_POP: Population data (indicators starting with LP_, SP_, SM_)
          // - EO_NA: Economy & Output (indicators starting with NGDP, NYG, NC_, NI_, NEGS_, NIGS_, NGDPVA_)

          // Population dataflow (PPL_POP)
          if (selectedIndicator.startsWith('LP_') || selectedIndicator.startsWith('SP_') || selectedIndicator.startsWith('SM_')) {
            result = await invoke('execute_python_script', {
              scriptName: 'adb_data.py',
              args: ['get_population', countryCode, selectedIndicator, adbStartYear, adbEndYear],
              env: {},
            }) as string;
          }
          // Economy & Output dataflow (EO_NA) - this covers GDP, GNI, expenditure, and sector output
          else {
            result = await invoke('execute_python_script', {
              scriptName: 'adb_data.py',
              args: ['get_gdp', countryCode, selectedIndicator, adbStartYear, adbEndYear],
              env: {},
            }) as string;
          }
          break;

        case 'fed':
          // Federal Reserve data - US only, no country selection needed
          // Use user-selected date range for pagination
          // Map indicator IDs to Python script commands with user date range
          if (selectedIndicator === 'federal_funds_rate') {
            result = await invoke('execute_python_script', {
              scriptName: 'federal_reserve_data.py',
              args: ['federal_funds_rate', startDate, endDate],
              env: {},
            }) as string;
          } else if (selectedIndicator === 'sofr_rate') {
            result = await invoke('execute_python_script', {
              scriptName: 'federal_reserve_data.py',
              args: ['sofr_rate', startDate, endDate],
              env: {},
            }) as string;
          } else if (selectedIndicator === 'treasury_rates') {
            result = await invoke('execute_python_script', {
              scriptName: 'federal_reserve_data.py',
              args: ['treasury_rates', startDate, endDate],
              env: {},
            }) as string;
          } else if (selectedIndicator === 'yield_curve') {
            // Get latest yield curve (single date)
            result = await invoke('execute_python_script', {
              scriptName: 'federal_reserve_data.py',
              args: ['yield_curve', endDate],
              env: {},
            }) as string;
          } else if (selectedIndicator === 'money_measures') {
            result = await invoke('execute_python_script', {
              scriptName: 'federal_reserve_data.py',
              args: ['money_measures', startDate, endDate, 'false'],
              env: {},
            }) as string;
          } else if (selectedIndicator === 'money_measures_adjusted') {
            result = await invoke('execute_python_script', {
              scriptName: 'federal_reserve_data.py',
              args: ['money_measures', startDate, endDate, 'true'],
              env: {},
            }) as string;
          } else if (selectedIndicator === 'market_overview') {
            // Composite - already limited internally
            result = await invoke('execute_python_script', {
              scriptName: 'federal_reserve_data.py',
              args: ['market_overview'],
              env: {},
            }) as string;
          } else {
            // Default - direct mapping with user date range
            result = await invoke('execute_python_script', {
              scriptName: 'federal_reserve_data.py',
              args: [selectedIndicator, startDate, endDate],
              env: {},
            }) as string;
          }
          break;

        case 'bls':
          // BLS uses series IDs directly, pass start/end years
          result = await invoke('execute_python_script', {
            scriptName: 'bls_data.py',
            args: ['get_series', selectedIndicator, startDate, endDate],
            env: { BLS_API_KEY: blsApiKey },
          }) as string;
          break;

        case 'unesco':
          // UNESCO UIS uses 3-letter ISO country codes and year-based dates
          const unescoStartYear = startDate.substring(0, 4);
          const unescoEndYear = endDate.substring(0, 4);
          result = await invoke('execute_python_script', {
            scriptName: 'unesco_data.py',
            args: ['fetch', selectedIndicator, countryCode, unescoStartYear, unescoEndYear],
            env: {},
          }) as string;
          break;

        case 'fiscaldata':
          // FiscalData is US-only, uses full date range (YYYY-MM-DD)
          result = await invoke('execute_python_script', {
            scriptName: 'fiscal_data.py',
            args: ['fetch', selectedIndicator, startDate, endDate],
            env: {},
          }) as string;
          break;

        case 'bea':
          // BEA is US-only, uses full date range (YYYY-MM-DD), requires API key
          result = await invoke('execute_python_script', {
            scriptName: 'bea_data.py',
            args: ['fetch', selectedIndicator, startDate, endDate],
            env: { BEA_API_KEY: beaApiKey },
          }) as string;
          break;

        default:
          throw new Error('Unknown data source');
      }

      const parsed = JSON.parse(result);

      // Check for error in response
      if (parsed.error || parsed.success === false) {
        // Handle error that could be a string or an object
        let errorMessage: string;
        if (typeof parsed.error === 'string') {
          errorMessage = parsed.error;
        } else if (typeof parsed.error === 'object' && parsed.error !== null) {
          errorMessage = parsed.error.error || parsed.error.message || JSON.stringify(parsed.error);
        } else if (parsed.message) {
          errorMessage = parsed.message;
        } else {
          errorMessage = 'An error occurred while fetching data';
        }
        setError(errorMessage);
        setData(null);
      } else {
        // Handle different response formats
        let rawData: any[] = [];
        let indicatorName = '';
        let countryName = '';

        if (dataSource === 'wto') {
          const wtoData = parsed.data?.Dataset || parsed.Dataset || [];
          if (Array.isArray(wtoData) && wtoData.length > 0) {
            rawData = wtoData.map((d: any) => ({
              date: String(d.Year || d.year || ''),
              value: d.Value ?? d.value,
            }));
            indicatorName = wtoData[0]?.Indicator || wtoData[0]?.IndicatorCode || '';
            countryName = wtoData[0]?.ReportingEconomy || wtoData[0]?.Reporter || '';
          }
        } else if (dataSource === 'cftc') {
          const cftcData = parsed.data || [];
          if (Array.isArray(cftcData) && cftcData.length > 0) {
            rawData = cftcData.map((d: any) => ({
              date: String(d.date || ''),
              value: d.non_commercial_net ?? d.commercial_net ?? d.open_interest ?? 0,
            }));
            const marketInfo = CFTC_MARKETS.find(m => m.id === selectedIndicator);
            indicatorName = marketInfo ? `${marketInfo.name} - Net Speculator Position` : selectedIndicator;
            countryName = 'Global';
          }
        } else if (dataSource === 'eia') {
          const eiaData = parsed.data || [];
          if (Array.isArray(eiaData) && eiaData.length > 0) {
            rawData = eiaData.map((d: any) => ({
              date: String(d.date || d.period || ''),
              value: d.value,
            }));
            const eiaIndicator = EIA_INDICATORS.find(i => i.id === selectedIndicator);
            indicatorName = eiaIndicator?.name || parsed.category || selectedIndicator;
            countryName = 'United States';
          }
        } else if (dataSource === 'adb') {
          // ADB returns SDMX-JSON format parsed into simpler structure
          const adbData = parsed.data || [];
          if (Array.isArray(adbData) && adbData.length > 0) {
            rawData = adbData.map((d: any) => ({
              date: String(d.time_period || d.date || ''),
              value: d.value,
            }));
            const adbIndicator = ADB_INDICATORS.find(i => i.id === selectedIndicator);
            indicatorName = adbIndicator?.name || parsed.metadata?.description || selectedIndicator;
            countryName = parsed.metadata?.economy_name || country?.name || '';
          } else {
            // Sometimes ADB returns nested structure
            const metadata = parsed.metadata || {};
            indicatorName = metadata.description || selectedIndicator;
            countryName = metadata.economy_name || country?.name || '';
          }
        } else if (dataSource === 'fed') {
          // Federal Reserve data - various formats depending on endpoint
          const fedData = parsed.data || [];
          const fedIndicator = FED_INDICATORS.find(i => i.id === selectedIndicator);
          indicatorName = fedIndicator?.name || selectedIndicator;
          countryName = 'United States';

          if (Array.isArray(fedData) && fedData.length > 0) {
            // Handle different Fed data structures
            if (selectedIndicator === 'federal_funds_rate' || selectedIndicator === 'sofr_rate') {
              // Interest rate data - has 'rate' field
              rawData = fedData.map((d: any) => ({
                date: String(d.date || ''),
                value: d.rate !== null && d.rate !== undefined ? d.rate * 100 : null, // Convert to percentage
              }));
            } else if (selectedIndicator === 'treasury_rates') {
              // Treasury rates - use 10-year as default display value
              rawData = fedData.map((d: any) => ({
                date: String(d.date || ''),
                value: d.year_10 !== null && d.year_10 !== undefined ? d.year_10 * 100 : null,
              }));
              indicatorName = '10-Year Treasury Rate';
            } else if (selectedIndicator === 'yield_curve') {
              // Yield curve data - filter for specific maturity or use rate field
              rawData = fedData
                .filter((d: any) => d.maturity === 'year_10')
                .map((d: any) => ({
                  date: String(d.date || ''),
                  value: d.rate !== null && d.rate !== undefined ? d.rate * 100 : null,
                }));
            } else if (selectedIndicator === 'money_measures' || selectedIndicator === 'money_measures_adjusted') {
              // Money supply data - use M2 as default display
              rawData = fedData.map((d: any) => ({
                date: String(d.month || d.date || ''),
                value: d.M2 || d.m2 || null,
              }));
              indicatorName = selectedIndicator === 'money_measures_adjusted'
                ? 'M2 Money Supply (Seasonally Adjusted)'
                : 'M2 Money Supply';
            } else if (selectedIndicator === 'market_overview') {
              // Market overview - extract federal funds rate from nested structure
              const ffr = parsed.endpoints?.federal_funds_rate?.data || [];
              if (Array.isArray(ffr) && ffr.length > 0) {
                rawData = ffr.map((d: any) => ({
                  date: String(d.date || ''),
                  value: d.rate !== null && d.rate !== undefined ? d.rate * 100 : null,
                }));
                indicatorName = 'Federal Funds Rate (from Market Overview)';
              }
            } else {
              // Generic fallback
              rawData = fedData.map((d: any) => ({
                date: String(d.date || d.month || ''),
                value: d.value ?? d.rate ?? d.M2 ?? null,
              }));
            }
          } else if (parsed.endpoints) {
            // Composite endpoint like market_overview or comprehensive_monetary_data
            const ffr = parsed.endpoints?.federal_funds_rate?.data || [];
            if (Array.isArray(ffr) && ffr.length > 0) {
              rawData = ffr.map((d: any) => ({
                date: String(d.date || ''),
                value: d.rate !== null && d.rate !== undefined ? d.rate * 100 : null,
              }));
              indicatorName = 'Federal Funds Rate';
            }
          }
        } else if (dataSource === 'bls') {
          // BLS returns { data: { series_data: [...], metadata: {...} } }
          const blsSeriesData = parsed.data?.series_data || [];
          if (Array.isArray(blsSeriesData) && blsSeriesData.length > 0) {
            rawData = blsSeriesData.map((d: any) => ({
              date: String(d.date || ''),
              value: d.value,
            }));
            const blsIndicator = BLS_INDICATORS.find(i => i.id === selectedIndicator);
            // Use title from API metadata or fall back to our constant
            indicatorName = blsSeriesData[0]?.title || blsIndicator?.name || selectedIndicator;
            countryName = 'United States';
          }
        } else if (dataSource === 'unesco') {
          // UNESCO returns { success, data: [{date, value}], metadata: {indicator_name, country} }
          const unescoData = parsed.data || [];
          if (Array.isArray(unescoData) && unescoData.length > 0) {
            rawData = unescoData.map((d: any) => ({
              date: String(d.date || ''),
              value: d.value,
            }));
            const unescoIndicator = UNESCO_INDICATORS.find(i => i.id === selectedIndicator);
            indicatorName = parsed.metadata?.indicator_name || unescoIndicator?.name || selectedIndicator;
            countryName = country?.name || parsed.metadata?.country || '';
          }
        } else if (dataSource === 'fiscaldata') {
          // FiscalData returns { success, data: [{date, value}], metadata: {indicator_name, country} }
          const fiscalData = parsed.data || [];
          if (Array.isArray(fiscalData) && fiscalData.length > 0) {
            rawData = fiscalData.map((d: any) => ({
              date: String(d.date || ''),
              value: d.value,
            }));
            const fiscalIndicator = FISCALDATA_INDICATORS.find(i => i.id === selectedIndicator);
            indicatorName = parsed.metadata?.indicator_name || fiscalIndicator?.name || selectedIndicator;
            countryName = 'United States';
          }
        } else if (dataSource === 'bea') {
          // BEA returns { success, data: [{date, value}], metadata: {indicator_name, country, source} }
          const beaData = parsed.data || [];
          if (Array.isArray(beaData) && beaData.length > 0) {
            rawData = beaData.map((d: any) => ({
              date: String(d.date || ''),
              value: d.value,
            }));
            const beaIndicator = BEA_INDICATORS.find(i => i.id === selectedIndicator);
            indicatorName = parsed.metadata?.indicator_name || beaIndicator?.name || selectedIndicator;
            countryName = 'United States';
          }
        } else if (parsed.data && Array.isArray(parsed.data)) {
          rawData = parsed.data.map((d: any) => ({
            date: d.date || d.period || d.year || '',
            value: d.value,
          }));
          indicatorName = parsed.data[0]?.indicator_name || parsed.indicator_name || '';
          countryName = parsed.data[0]?.country_name || parsed.country_name || '';
        }

        // Filter and transform data
        let dataPoints: DataPoint[] = rawData
          .filter((d: any) => d.value !== null && d.value !== undefined)
          .map((d: any) => ({
            date: String(d.date),
            value: typeof d.value === 'number' ? d.value : parseFloat(d.value),
          }))
          .filter((d: DataPoint) => !isNaN(d.value) && d.date)
          .sort((a: DataPoint, b: DataPoint) => a.date.localeCompare(b.date));

        // Apply date range filter for all providers (client-side filtering)
        // This ensures consistent pagination across all data sources
        if (startDate && endDate) {
          dataPoints = dataPoints.filter((d: DataPoint) => {
            // Handle various date formats: YYYY, YYYY-MM, YYYY-MM-DD
            const dateStr = d.date;
            // Normalize to comparable format
            const normalizedDate = dateStr.length === 4
              ? `${dateStr}-01-01` // Year only -> Jan 1
              : dateStr.length === 7
                ? `${dateStr}-01` // Year-Month -> 1st of month
                : dateStr; // Full date
            return normalizedDate >= startDate && normalizedDate <= endDate;
          });
        }

        if (dataPoints.length === 0) {
          setError('No data available for this selection');
          setData(null);
        } else {
          const indicatorInfo = indicators.find((i) => i.id === selectedIndicator);

          setData({
            indicator: selectedIndicator,
            country: selectedCountry,
            data: dataPoints,
            metadata: {
              indicator_name: indicatorName || indicatorInfo?.name,
              country_name: countryName || country?.name,
              source: currentSourceConfig.name,
            },
          });
        }
      }
    } catch (err) {
      console.error('Fetch error:', err);
      // Ensure error is always a string
      let errorMessage: string;
      if (err instanceof Error) {
        errorMessage = err.message;
      } else if (typeof err === 'string') {
        errorMessage = err;
      } else if (typeof err === 'object' && err !== null) {
        errorMessage = (err as any).message || (err as any).error || JSON.stringify(err);
      } else {
        errorMessage = 'An unknown error occurred';
      }
      setError(`Failed to fetch data: ${errorMessage}`);
      setData(null);
    } finally {
      setLoading(false);
    }
  }, [dataSource, selectedCountry, selectedIndicator, wtoApiKey, eiaApiKey, blsApiKey, beaApiKey, startDate, endDate]);

  // Calculate stats
  const stats: ChartStats | null = data && data.data.length > 0 ? (() => {
    const values = data.data.map((d) => d.value);
    const latest = values[values.length - 1];
    const previous = values.length > 1 ? values[values.length - 2] : latest;
    const change = previous !== 0 ? ((latest - previous) / Math.abs(previous)) * 100 : 0;
    const min = Math.min(...values);
    const max = Math.max(...values);
    const avg = values.reduce((a, b) => a + b, 0) / values.length;
    return { latest, change, min, max, avg, count: values.length };
  })() : null;

  return {
    dataSource,
    setDataSource,
    selectedCountry,
    setSelectedCountry,
    selectedIndicator,
    setSelectedIndicator,
    data,
    loading,
    error,
    fetchData,
    stats,
    // Date range
    dateRangePreset,
    setDateRangePreset,
    startDate,
    endDate,
    setStartDate,
    setEndDate,
    // API Keys
    wtoApiKey,
    setWtoApiKey,
    eiaApiKey,
    setEiaApiKey,
    blsApiKey,
    setBlsApiKey,
    beaApiKey,
    setBeaApiKey,
    showApiKeyInput,
    setShowApiKeyInput,
    apiKeySaving,
    saveApiKey,
  };
}

export default useEconomicsData;
