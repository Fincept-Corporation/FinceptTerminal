import { useTerminalTheme } from '@/contexts/ThemeContext';
import React, { useState, useEffect, useMemo } from 'react';
import { Search, Download, TrendingUp, BarChart3, Loader2, AlertCircle, FolderTree, X, Plus, ArrowLeft, ChevronRight } from 'lucide-react';
import { LineChart, Line, AreaChart, Area, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer } from 'recharts';
import { sqliteService } from '@/services/sqliteService';
import { invoke } from '@tauri-apps/api/core';

interface SeriesData {
  date: string;
  value: number;
}

interface FREDSeries {
  series_id: string;
  title: string;
  units: string;
  frequency: string;
  seasonal_adjustment: string;
  last_updated: string;
  observations: SeriesData[];
  observation_count: number;
}

interface SearchResult {
  id: string;
  title: string;
  frequency: string;
  units: string;
  seasonal_adjustment: string;
  last_updated: string;
  popularity: number;
}

interface Category {
  id: number;
  name: string;
  parent_id: number;
}

const POPULAR_SERIES = [
  { id: 'GDP', name: 'Gross Domestic Product', cat: 'Economic' },
  { id: 'UNRATE', name: 'Unemployment Rate', cat: 'Labor' },
  { id: 'CPIAUCSL', name: 'Consumer Price Index', cat: 'Inflation' },
  { id: 'FEDFUNDS', name: 'Federal Funds Rate', cat: 'Interest Rates' },
  { id: 'DGS10', name: '10-Year Treasury Rate', cat: 'Bonds' },
  { id: 'DEXUSEU', name: 'USD/EUR Exchange Rate', cat: 'Forex' },
  { id: 'DCOILWTICO', name: 'WTI Crude Oil Price', cat: 'Commodities' },
  { id: 'GOLDAMGBD228NLBM', name: 'Gold Price', cat: 'Commodities' }
];

const POPULAR_CATEGORIES = [
  { id: 32991, name: '💰 Money, Banking, & Finance', desc: 'Money supply, interest rates, banking data' },
  { id: 10, name: '📊 Population, Employment, Labor', desc: 'Employment, unemployment, labor statistics' },
  { id: 32455, name: '💵 Prices', desc: 'Inflation, CPI, PPI, commodity prices' },
  { id: 1, name: '🏭 Production & Business', desc: 'Industrial production, capacity utilization' },
  { id: 32263, name: '🌍 International Data', desc: 'Trade, exchange rates, foreign markets' },
  { id: 33060, name: '🏛️ U.S. Regional Data', desc: 'State and metro area statistics' },
];

export default function ScreenerTab() {
  const { colors, fontSize, fontFamily, fontWeight, fontStyle } = useTerminalTheme();
  const [seriesIds, setSeriesIds] = useState('GDP');
  const [startDate, setStartDate] = useState('2000-01-01');
  const [endDate, setEndDate] = useState(new Date().toISOString().split('T')[0]);
  const [data, setData] = useState<FREDSeries[]>([]);
  const [loading, setLoading] = useState(false);
  const [loadingMessage, setLoadingMessage] = useState('');
  const [error, setError] = useState<string | null>(null);
  const [chartType, setChartType] = useState<'line' | 'area'>('line');
  const [apiKeyConfigured, setApiKeyConfigured] = useState<boolean>(false);

  // Series browser modal state
  const [showBrowser, setShowBrowser] = useState(false);
  const [searchQuery, setSearchQuery] = useState('');
  const [searchResults, setSearchResults] = useState<SearchResult[]>([]);
  const [searchLoading, setSearchLoading] = useState(false);
  const [categories, setCategories] = useState<Category[]>([]);
  const [currentCategory, setCurrentCategory] = useState<number>(0);
  const [categoryPath, setCategoryPath] = useState<{id: number, name: string}[]>([{id: 0, name: 'All Categories'}]);
  const [categoryLoading, setCategoryLoading] = useState(false);
  const [categorySeriesResults, setCategorySeriesResults] = useState<SearchResult[]>([]);
  const [categorySeriesLoading, setCategorySeriesLoading] = useState(false);
  const [normalizeData, setNormalizeData] = useState(false);

  useEffect(() => {
    checkApiKey();
    // Auto-load GDP data on mount
    const autoLoadData = async () => {
      const apiKey = await sqliteService.getApiKey('FRED');
      if (apiKey) {
        fetchData();
      }
    };
    autoLoadData();
  }, []);

  const checkApiKey = async () => {
    try {
      const apiKey = await sqliteService.getApiKey('FRED');
      console.log('[ScreenerTab] FRED API key check:', apiKey ? `Found (length: ${apiKey.length})` : 'Not found');
      setApiKeyConfigured(!!apiKey);

      // Also check all API keys for debugging
      const allKeys = await sqliteService.getAllApiKeys();
      console.log('[ScreenerTab] All configured API keys:', Object.keys(allKeys));
    } catch (error) {
      console.error('[ScreenerTab] Failed to check FRED API key:', error);
      setApiKeyConfigured(false);
    }
  };

  // Search series
  const searchSeries = async (query: string) => {
    if (!query.trim()) {
      setSearchResults([]);
      return;
    }

    setSearchLoading(true);
    try {
      const apiKey = await sqliteService.getApiKey('FRED');
      if (!apiKey) return;

      const result = await invoke<string>('execute_python_script', {
        scriptName: 'fred_data.py',
        args: ['search', query, '50'],
        env: { FRED_API_KEY: apiKey }
      });

      const parsed = JSON.parse(result);
      if (!parsed.error && parsed.series) {
        setSearchResults(parsed.series);
      } else {
        setSearchResults([]);
      }
    } catch (err) {
      setSearchResults([]);
    } finally {
      setSearchLoading(false);
    }
  };

  // Load categories
  const loadCategories = async (categoryId: number = 0) => {
    setCategoryLoading(true);
    try {
      const apiKey = await sqliteService.getApiKey('FRED');
      if (!apiKey) return;

      const result = await invoke<string>('execute_python_script', {
        scriptName: 'fred_data.py',
        args: ['categories', categoryId.toString()],
        env: { FRED_API_KEY: apiKey }
      });

      const parsed = JSON.parse(result);
      if (!parsed.error && parsed.categories) {
        setCategories(parsed.categories);
      } else {
        setCategories([]);
      }
    } catch (err) {
      setCategories([]);
    } finally {
      setCategoryLoading(false);
    }
  };

  // Load series in a category
  const loadCategorySeries = async (categoryId: number) => {
    setCategorySeriesLoading(true);
    try {
      const apiKey = await sqliteService.getApiKey('FRED');
      if (!apiKey) {
        setCategorySeriesLoading(false);
        return;
      }

      const result = await invoke<string>('execute_python_script', {
        scriptName: 'fred_data.py',
        args: ['category_series', categoryId.toString(), '50'],
        env: { FRED_API_KEY: apiKey }
      });

      const parsed = JSON.parse(result);
      if (!parsed.error && parsed.series) {
        setCategorySeriesResults(parsed.series);
      } else {
        setCategorySeriesResults([]);
      }
    } catch (err) {
      setCategorySeriesResults([]);
    } finally {
      setCategorySeriesLoading(false);
    }
  };

  // Navigate to category
  const navigateToCategory = async (cat: {id: number, name: string}) => {
    setCurrentCategory(cat.id);
    setCategoryPath([...categoryPath, cat]);
    setSearchQuery('');
    setSearchResults([]);
    setCategorySeriesResults([]);

    // Load both subcategories and series
    await Promise.all([
      loadCategories(cat.id),
      loadCategorySeries(cat.id)
    ]);
  };

  // Navigate back
  const navigateBack = () => {
    if (categoryPath.length > 1) {
      const newPath = categoryPath.slice(0, -1);
      setCategoryPath(newPath);
      const parentId = newPath[newPath.length - 1].id;
      setCurrentCategory(parentId);
      loadCategories(parentId);
    }
  };

  // Open browser modal
  const openBrowser = () => {
    setShowBrowser(true);
    if (categories.length === 0) {
      loadCategories(0);
    }
  };

  // Add series from browser
  const addSeriesFromBrowser = async (id: string) => {
    if (!getCurrentSeriesIds().includes(id)) {
      const newIds = seriesIds ? `${seriesIds},${id}` : id;
      setSeriesIds(newIds);

      // Auto-fetch data after adding series
      setTimeout(async () => {
        try {
          setLoading(true);
          setError(null);
          setLoadingMessage('Initializing...');

          const apiKey = await sqliteService.getApiKey('FRED');
          if (!apiKey) {
            setError('FRED API key not configured. Please add it in Settings.');
            setLoading(false);
            setLoadingMessage('');
            return;
          }

          const ids = [...new Set(newIds.split(',').map(s => s.trim()).filter(Boolean))];

          if (ids.length === 0) {
            setError('Please enter at least one series ID');
            setLoading(false);
            setLoadingMessage('');
            return;
          }

          setLoadingMessage(`Fetching ${ids.length} series from FRED API...`);
          const args = ['multiple', ...ids, startDate, endDate];

          const result = await invoke<string>('execute_python_script', {
            scriptName: 'fred_data.py',
            args,
            env: { FRED_API_KEY: apiKey }
          });

          setLoadingMessage('Processing data...');
          const parsed = JSON.parse(result);

          if (parsed.error) {
            setError(`API Error: ${parsed.error}`);
          } else if (Array.isArray(parsed)) {
            const errorSeries = parsed.find(s => s.error);
            if (errorSeries) {
              setError(`Failed to fetch ${errorSeries.series_id}: ${errorSeries.error}`);
            } else {
              setData(parsed);
              setError(null);
              setLoadingMessage(`Successfully loaded ${parsed.length} series!`);
              setTimeout(() => setLoadingMessage(''), 2000);
            }
          } else {
            setData(parsed);
          }
        } catch (err: any) {
          setError(err.message || 'Failed to fetch data');
        } finally {
          setLoading(false);
        }
      }, 100);
    }
  };

  const fetchData = async () => {
    setLoading(true);
    setError(null);
    setLoadingMessage('Initializing...');

    try {
      const apiKey = await sqliteService.getApiKey('FRED');
      if (!apiKey) {
        setError('FRED API key not configured. Please add it in Settings > Credentials.');
        setApiKeyConfigured(false);
        setLoading(false);
        setLoadingMessage('');
        return;
      }

      // Remove duplicates and trim
      const ids = [...new Set(seriesIds.split(',').map(s => s.trim()).filter(Boolean))];

      if (ids.length === 0) {
        setError('Please enter at least one series ID');
        setLoading(false);
        setLoadingMessage('');
        return;
      }

      setLoadingMessage(`Fetching ${ids.length} series from FRED API...`);
      const args = ['multiple', ...ids, startDate, endDate];

      const result = await invoke<string>('execute_python_script', {
        scriptName: 'fred_data.py',
        args,
        env: { FRED_API_KEY: apiKey }
      });

      setLoadingMessage('Processing data...');
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(`API Error: ${parsed.error}`);
      } else if (Array.isArray(parsed)) {
        // Check if any series has an error
        const errorSeries = parsed.find(s => s.error);
        if (errorSeries) {
          setError(`Failed to fetch ${errorSeries.series_id}: ${errorSeries.error}`);
        } else {
          setData(parsed);
          setError(null);
          setLoadingMessage(`Successfully loaded ${parsed.length} series!`);
          setTimeout(() => setLoadingMessage(''), 2000);
        }
      } else {
        setData(parsed);
      }
    } catch (err: any) {
      setError(err.message || 'Failed to fetch data');
    } finally {
      setLoading(false);
    }
  };

  // Helper to get current series IDs as array
  const getCurrentSeriesIds = () => {
    return seriesIds.split(',').map(s => s.trim()).filter(Boolean);
  };

  const addPopularSeries = (id: string) => {
    const currentIds = getCurrentSeriesIds();
    if (currentIds.includes(id)) {
      // Remove if already added
      const newIds = currentIds.filter(sid => sid !== id);
      setSeriesIds(newIds.join(','));
    } else {
      // Add if not present
      setSeriesIds(prev => prev ? `${prev},${id}` : id);
    }
  };

  const prepareChartData = () => {
    if (data.length === 0) return [];

    const allDates = new Set<string>();
    data.forEach(series => {
      series.observations.forEach(obs => allDates.add(obs.date));
    });

    const sortedDates = Array.from(allDates).sort();

    // First pass: collect all values
    const rawData = sortedDates.map(date => {
      const point: any = { date };
      data.forEach(series => {
        const obs = series.observations.find(o => o.date === date);
        point[series.series_id] = obs?.value || null;
      });
      return point;
    });

    // If normalization is enabled, normalize each series to 0-100 scale
    if (normalizeData && data.length > 1) {
      // Find min/max for each series
      const seriesStats: Record<string, { min: number, max: number }> = {};

      data.forEach(series => {
        const values = series.observations.map(obs => parseFloat(obs.value.toString())).filter(v => !isNaN(v) && v !== null);
        if (values.length > 0) {
          seriesStats[series.series_id] = {
            min: Math.min(...values),
            max: Math.max(...values)
          };
        }
      });

      // Normalize the data
      return rawData.map(point => {
        const normalizedPoint: any = { date: point.date };
        data.forEach(series => {
          const value = point[series.series_id];
          if (value !== null && value !== undefined && seriesStats[series.series_id]) {
            const { min, max } = seriesStats[series.series_id];
            const range = max - min;
            normalizedPoint[series.series_id] = range > 0 ? ((value - min) / range) * 100 : 50;
          } else {
            normalizedPoint[series.series_id] = null;
          }
        });
        return normalizedPoint;
      });
    }

    return rawData;
  };

  const exportToCSV = () => {
    const chartData = prepareChartData();
    const headers = ['Date', ...data.map(s => s.series_id)].join(',');
    const rows = chartData.map(row => {
      return [row.date, ...data.map(s => row[s.series_id] || '')].join(',');
    });
    const csv = [headers, ...rows].join('\n');

    const blob = new Blob([csv], { type: 'text/csv' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `fred_data_${new Date().toISOString()}.csv`;
    a.click();
  };

  // Memoize current series IDs set for performance
  const currentSeriesIdsSet = useMemo(() => {
    return new Set(getCurrentSeriesIds());
  }, [seriesIds]);

  // Use effect for search debounce
  useEffect(() => {
    const timer = setTimeout(() => {
      if (searchQuery && showBrowser) {
        searchSeries(searchQuery);
      }
    }, 500);
    return () => clearTimeout(timer);
  }, [searchQuery, showBrowser]);

  return (
    <div style={{
      width: '100%',
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: colors.background,
      color: colors.text,
      fontFamily: fontFamily,
      fontWeight: fontWeight,
      fontStyle: fontStyle,
      overflow: 'hidden',
      fontSize: fontSize.body
    }}>
      <style>{`
        *::-webkit-scrollbar { width: 6px; height: 6px; }
        *::-webkit-scrollbar-track { background: ${colors.background}; }
        *::-webkit-scrollbar-thumb { background: ${colors.panel}; border-radius: 3px; }
        *::-webkit-scrollbar-thumb:hover { background: ${colors.textMuted}; }
      `}</style>

      {/* Series Browser Modal */}
      {showBrowser && (
        <div style={{
          position: 'fixed',
          top: 0,
          left: 0,
          right: 0,
          bottom: 0,
          background: 'rgba(0,0,0,0.95)',
          zIndex: 1000,
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          padding: '20px',
          backdropFilter: 'blur(4px)'
        }}>
          <div style={{
            background: colors.background,
            border: `1px solid ${colors.primary}`,
            borderRadius: '4px',
            width: '90%',
            maxWidth: '1200px',
            height: '85vh',
            display: 'flex',
            flexDirection: 'column',
            overflow: 'hidden',
            boxShadow: `0 8px 32px ${colors.primary}33`
          }}>
            {/* Modal Header */}
            <div style={{
              borderBottom: `1px solid ${colors.primary}`,
              padding: '12px 20px',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
              background: colors.panel
            }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
                <FolderTree size={16} color={colors.primary} />
                <span style={{ color: colors.primary, fontSize: fontSize.heading, fontWeight: 'bold', letterSpacing: '0.5px' }}>
                  FRED SERIES BROWSER
                </span>
              </div>
              <button
                onClick={() => setShowBrowser(false)}
                style={{
                  background: 'transparent',
                  border: 'none',
                  color: colors.textMuted,
                  cursor: 'pointer',
                  padding: '4px',
                  display: 'flex',
                  alignItems: 'center'
                }}
              >
                <X size={18} />
              </button>
            </div>

            {/* Search Bar */}
            <div style={{ padding: '12px 20px', borderBottom: `1px solid ${colors.panel}` }}>
              <div style={{ position: 'relative' }}>
                <Search size={14} color={colors.textMuted} style={{ position: 'absolute', left: '12px', top: '50%', transform: 'translateY(-50%)' }} />
                <input
                  type="text"
                  placeholder="Search FRED series by keyword (e.g., 'unemployment', 'inflation', 'GDP')..."
                  value={searchQuery}
                  onChange={(e) => setSearchQuery(e.target.value)}
                  style={{
                    width: '100%',
                    background: colors.panel,
                    border: `1px solid ${colors.textMuted}33`,
                    color: colors.text,
                    padding: '10px 40px 10px 40px',
                    fontSize: fontSize.small,
                    borderRadius: '3px',
                    outline: 'none',
                    transition: 'border-color 0.2s'
                  }}
                  onFocus={(e) => e.target.style.borderColor = colors.primary}
                  onBlur={(e) => e.target.style.borderColor = `${colors.textMuted}33`}
                />
                {searchLoading && (
                  <Loader2 size={14} className="animate-spin" style={{ position: 'absolute', right: '12px', top: '50%', transform: 'translateY(-50%)', color: colors.primary }} />
                )}
              </div>
            </div>

            {/* Content Area */}
            <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
              {/* Left Panel - Categories or Popular */}
              {!searchQuery && (
                <div style={{
                  width: '350px',
                  borderRight: '1px solid #1a1a1a',
                  display: 'flex',
                  flexDirection: 'column',
                  background: '#050505'
                }}>
                  {/* Breadcrumb */}
                  <div style={{
                    padding: '12px 16px',
                    borderBottom: '1px solid #1a1a1a',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '8px'
                  }}>
                    {categoryPath.length > 1 && (
                      <button
                        onClick={navigateBack}
                        style={{
                          background: 'colors.panel',
                          border: '1px solid #2a2a2a',
                          color: '#ea580c',
                          padding: '4px 8px',
                          fontSize: '10px',
                          cursor: 'pointer',
                          borderRadius: '3px',
                          display: 'flex',
                          alignItems: 'center',
                          gap: '4px'
                        }}
                      >
                        <ArrowLeft size={12} />
                        BACK
                      </button>
                    )}
                    <span style={{ color: '#888', fontSize: '10px', flex: 1 }}>
                      {categoryPath.map((p, i) => (
                        <span key={`${p.id}-${i}`}>
                          {i > 0 && ' > '}
                          {p.name}
                        </span>
                      ))}
                    </span>
                  </div>

                  {/* Popular Categories (only at root) */}
                  {currentCategory === 0 && (
                    <div style={{ padding: '12px', borderBottom: '1px solid #1a1a1a', maxHeight: '200px', overflowY: 'auto' }}>
                      <h4 style={{ color: '#ea580c', fontSize: '10px', marginBottom: '8px', fontWeight: 'bold' }}>
                        POPULAR CATEGORIES
                      </h4>
                      <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                        {POPULAR_CATEGORIES.map((cat, idx) => (
                          <button
                            key={`pop-${cat.id}-${idx}`}
                            onClick={() => navigateToCategory({ id: cat.id, name: cat.name.replace(/[^\w\s]/gi, '').trim() })}
                            style={{
                              background: 'colors.panel',
                              border: '1px solid #2a2a2a',
                              color: 'colors.text',
                              padding: '6px 8px',
                              fontSize: '9px',
                              cursor: 'pointer',
                              borderRadius: '3px',
                              textAlign: 'left',
                              display: 'flex',
                              alignItems: 'center',
                              justifyContent: 'space-between'
                            }}
                          >
                            <div style={{ flex: 1, minWidth: 0 }}>
                              <div style={{ fontWeight: 'bold', marginBottom: '2px', fontSize: '9px' }}>{cat.name}</div>
                            </div>
                            <ChevronRight size={12} color="#666" />
                          </button>
                        ))}
                      </div>
                    </div>
                  )}

                  {/* Category List */}
                  <div style={{ flex: 1, overflowY: 'auto', padding: '12px' }}>
                    <h4 style={{ color: '#888', fontSize: '9px', marginBottom: '8px', fontWeight: 'bold' }}>
                      {categoryLoading ? 'LOADING...' : `SUBCATEGORIES (${categories.length})`}
                    </h4>
                    {categories.length === 0 && !categoryLoading && currentCategory !== 0 && (
                      <p style={{ color: '#666', fontSize: '10px' }}>No subcategories found</p>
                    )}
                    <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                      {categories.map((cat, idx) => (
                        <button
                          key={`cat-${cat.id}-${idx}`}
                          onClick={() => navigateToCategory(cat)}
                          style={{
                            background: 'colors.panel',
                            border: '1px solid #2a2a2a',
                            color: 'colors.text',
                            padding: '8px 10px',
                            fontSize: '10px',
                            cursor: 'pointer',
                            borderRadius: '3px',
                            textAlign: 'left',
                            display: 'flex',
                            alignItems: 'center',
                            justifyContent: 'space-between'
                          }}
                        >
                          <span>{cat.name}</span>
                          <ChevronRight size={12} color="#666" />
                        </button>
                      ))}
                    </div>
                  </div>
                </div>
              )}

              {/* Right Panel - Search Results or Series in Category */}
              <div style={{ flex: 1, overflowY: 'auto', padding: '16px 20px' }}>
                {categorySeriesLoading && !searchQuery ? (
                  // Loading Category Series
                  <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%', gap: '16px' }}>
                    <Loader2 size={32} color={colors.primary} className="animate-spin" />
                    <p style={{ color: colors.textMuted, fontSize: fontSize.small }}>
                      Loading series in category...
                    </p>
                  </div>
                ) : searchQuery ? (
                  // Search Results
                  <>
                    <h3 style={{ color: '#ea580c', fontSize: '11px', marginBottom: '12px', fontWeight: 'bold' }}>
                      SEARCH RESULTS ({searchResults.length})
                    </h3>
                    {searchResults.length === 0 && !searchLoading && (
                      <p style={{ color: '#666', fontSize: '10px' }}>No results found</p>
                    )}
                    <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                      {searchResults.map((result) => (
                        <div
                          key={result.id}
                          style={{
                            background: 'colors.panel',
                            border: '1px solid #1a1a1a',
                            borderRadius: '4px',
                            padding: '12px',
                            display: 'flex',
                            alignItems: 'flex-start',
                            justifyContent: 'space-between',
                            gap: '12px'
                          }}
                        >
                          <div style={{ flex: 1, minWidth: 0 }}>
                            <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '4px' }}>
                              <span style={{ color: '#ea580c', fontSize: '11px', fontWeight: 'bold', fontFamily: 'monospace' }}>
                                {result.id}
                              </span>
                              <span style={{ color: '#666', fontSize: '9px' }}>
                                Pop: {result.popularity}
                              </span>
                            </div>
                            <p style={{ color: 'colors.text', fontSize: '10px', marginBottom: '4px' }}>
                              {result.title}
                            </p>
                            <div style={{ display: 'flex', gap: '12px', fontSize: '9px', color: '#666' }}>
                              <span>Freq: {result.frequency}</span>
                              <span>Units: {result.units}</span>
                              <span>Updated: {result.last_updated.split('T')[0]}</span>
                            </div>
                          </div>
                          <button
                            onClick={() => addSeriesFromBrowser(result.id)}
                            disabled={currentSeriesIdsSet.has(result.id)}
                            style={{
                              background: currentSeriesIdsSet.has(result.id) ? '#1a3a1a' : '#ea580c',
                              border: 'none',
                              color: 'colors.text',
                              padding: '6px 12px',
                              fontSize: '10px',
                              cursor: currentSeriesIdsSet.has(result.id) ? 'not-allowed' : 'pointer',
                              borderRadius: '3px',
                              fontWeight: 'bold',
                              display: 'flex',
                              alignItems: 'center',
                              gap: '4px',
                              whiteSpace: 'nowrap',
                              opacity: currentSeriesIdsSet.has(result.id) ? 0.5 : 1
                            }}
                          >
                            <Plus size={12} />
                            {currentSeriesIdsSet.has(result.id) ? 'ADDED' : 'ADD'}
                          </button>
                        </div>
                      ))}
                    </div>
                  </>
                ) : categorySeriesResults.length > 0 ? (
                  // Category Series Results
                  <>
                    <h3 style={{ color: '#ea580c', fontSize: '11px', marginBottom: '12px', fontWeight: 'bold' }}>
                      SERIES IN THIS CATEGORY ({categorySeriesResults.length})
                    </h3>
                    <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                      {categorySeriesResults.map((result) => (
                        <div
                          key={result.id}
                          style={{
                            background: 'colors.panel',
                            border: '1px solid #1a1a1a',
                            borderRadius: '4px',
                            padding: '12px',
                            display: 'flex',
                            alignItems: 'flex-start',
                            justifyContent: 'space-between',
                            gap: '12px'
                          }}
                        >
                          <div style={{ flex: 1, minWidth: 0 }}>
                            <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '4px' }}>
                              <span style={{ color: '#ea580c', fontSize: '11px', fontWeight: 'bold', fontFamily: 'monospace' }}>
                                {result.id}
                              </span>
                              <span style={{ color: '#666', fontSize: '9px' }}>
                                Pop: {result.popularity}
                              </span>
                            </div>
                            <p style={{ color: 'colors.text', fontSize: '10px', marginBottom: '4px' }}>
                              {result.title}
                            </p>
                            <div style={{ display: 'flex', gap: '12px', fontSize: '9px', color: '#666' }}>
                              <span>Freq: {result.frequency}</span>
                              <span>Units: {result.units}</span>
                              <span>Updated: {result.last_updated.split('T')[0]}</span>
                            </div>
                          </div>
                          <button
                            onClick={() => addSeriesFromBrowser(result.id)}
                            disabled={currentSeriesIdsSet.has(result.id)}
                            style={{
                              background: currentSeriesIdsSet.has(result.id) ? '#1a3a1a' : '#ea580c',
                              border: 'none',
                              color: 'colors.text',
                              padding: '6px 12px',
                              fontSize: '10px',
                              cursor: currentSeriesIdsSet.has(result.id) ? 'not-allowed' : 'pointer',
                              borderRadius: '3px',
                              fontWeight: 'bold',
                              display: 'flex',
                              alignItems: 'center',
                              gap: '4px',
                              whiteSpace: 'nowrap',
                              opacity: currentSeriesIdsSet.has(result.id) ? 0.5 : 1
                            }}
                          >
                            <Plus size={12} />
                            {currentSeriesIdsSet.has(result.id) ? 'ADDED' : 'ADD'}
                          </button>
                        </div>
                      ))}
                    </div>
                  </>
                ) : (
                  // Instructions or Category Info
                  <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%', gap: '16px' }}>
                    <TrendingUp size={48} color="#333" />
                    <div style={{ textAlign: 'center', maxWidth: '500px' }}>
                      <p style={{ color: '#888', fontSize: '12px', marginBottom: '8px' }}>
                        <strong>Search for any economic data series</strong>
                      </p>
                      <p style={{ color: '#666', fontSize: '10px', lineHeight: '1.5' }}>
                        Try searching for terms like "unemployment", "inflation", "GDP", "interest rates", "housing", etc.
                        Or browse through categories on the left to discover available data series.
                      </p>
                    </div>
                  </div>
                )}
              </div>
            </div>

            {/* Modal Footer */}
            <div style={{
              borderTop: '1px solid #1a1a1a',
              padding: '12px 20px',
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center',
              background: 'colors.panel'
            }}>
              <span style={{ color: '#666', fontSize: '9px' }}>
                Federal Reserve Economic Data (FRED) | 800,000+ series available
              </span>
              <button
                onClick={() => setShowBrowser(false)}
                style={{
                  background: '#ea580c',
                  border: 'none',
                  color: 'colors.text',
                  padding: '8px 16px',
                  fontSize: '10px',
                  cursor: 'pointer',
                  borderRadius: '3px',
                  fontWeight: 'bold'
                }}
              >
                DONE
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Header */}
      <div style={{
        borderBottom: `1px solid ${colors.primary}`,
        padding: '10px 16px',
        background: colors.panel,
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
          <BarChart3 size={16} color={colors.primary} />
          <span style={{ color: colors.primary, fontSize: fontSize.heading, fontWeight: 'bold', letterSpacing: '0.5px' }}>
            FRED DATA EXPLORER
          </span>
        </div>
        <div style={{ display: 'flex', gap: '6px' }}>
          <button
            onClick={() => setChartType(chartType === 'line' ? 'area' : 'line')}
            style={{
              background: colors.background,
              border: `1px solid ${colors.primary}33`,
              color: colors.primary,
              padding: '6px 12px',
              fontSize: fontSize.small,
              cursor: 'pointer',
              borderRadius: '3px',
              fontWeight: 'bold',
              transition: 'all 0.2s'
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.background = colors.primary;
              e.currentTarget.style.color = colors.background;
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.background = colors.background;
              e.currentTarget.style.color = colors.primary;
            }}
          >
            {chartType === 'line' ? 'AREA' : 'LINE'}
          </button>
          <button
            onClick={() => setNormalizeData(!normalizeData)}
            disabled={data.length <= 1}
            style={{
              background: normalizeData ? colors.primary : colors.background,
              border: `1px solid ${colors.primary}33`,
              color: normalizeData ? colors.background : colors.primary,
              padding: '6px 12px',
              fontSize: fontSize.small,
              cursor: data.length > 1 ? 'pointer' : 'not-allowed',
              borderRadius: '3px',
              fontWeight: 'bold',
              opacity: data.length > 1 ? 1 : 0.5,
              transition: 'all 0.2s'
            }}
            onMouseEnter={(e) => {
              if (data.length > 1 && !normalizeData) {
                e.currentTarget.style.background = colors.primary;
                e.currentTarget.style.color = colors.background;
              }
            }}
            onMouseLeave={(e) => {
              if (data.length > 1 && !normalizeData) {
                e.currentTarget.style.background = colors.background;
                e.currentTarget.style.color = colors.primary;
              }
            }}
          >
            NORMALIZE
          </button>
          <button
            onClick={exportToCSV}
            disabled={data.length === 0}
            style={{
              background: data.length > 0 ? colors.background : colors.panel,
              border: `1px solid ${data.length > 0 ? colors.success : colors.textMuted}33`,
              color: data.length > 0 ? colors.success : colors.textMuted,
              padding: '6px 12px',
              fontSize: fontSize.small,
              cursor: data.length > 0 ? 'pointer' : 'not-allowed',
              borderRadius: '3px',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              fontWeight: 'bold',
              opacity: data.length > 0 ? 1 : 0.5,
              transition: 'opacity 0.2s'
            }}
            onMouseEnter={(e) => {
              if (data.length > 0) {
                e.currentTarget.style.opacity = '0.7';
              }
            }}
            onMouseLeave={(e) => {
              if (data.length > 0) {
                e.currentTarget.style.opacity = '1';
              }
            }}
          >
            <Download size={12} />
            CSV
          </button>
        </div>
      </div>

      {/* API Key Warning */}
      {!apiKeyConfigured && (
        <div style={{
          background: `${colors.warning}11`,
          border: `1px solid ${colors.warning}`,
          padding: '12px 16px',
          display: 'flex',
          alignItems: 'flex-start',
          gap: '12px',
          flexShrink: 0
        }}>
          <AlertCircle size={20} color={colors.warning} style={{ flexShrink: 0, marginTop: '2px' }} />
          <div style={{ flex: 1 }}>
            <p style={{ color: colors.warning, fontSize: fontSize.body, fontWeight: 'bold', marginBottom: '6px' }}>
              API KEY REQUIRED
            </p>
            <p style={{ color: colors.text, fontSize: fontSize.small, lineHeight: '1.5', marginBottom: '8px' }}>
              Configure your FRED API key in <strong>Settings → Credentials</strong> to access economic data.
            </p>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
              <p style={{ color: colors.textMuted, fontSize: fontSize.tiny }}>
                <strong style={{ color: colors.warning }}>1.</strong> Get free API key at{' '}
                <a
                  href="https://research.stlouisfed.org/useraccount/apikey"
                  target="_blank"
                  rel="noopener noreferrer"
                  style={{ color: colors.info, textDecoration: 'underline' }}
                >
                  research.stlouisfed.org
                </a>
              </p>
              <p style={{ color: colors.textMuted, fontSize: fontSize.tiny }}>
                <strong style={{ color: colors.warning }}>2.</strong> Add to <strong>Settings → Credentials → FRED_API_KEY</strong>
              </p>
              <p style={{ color: colors.textMuted, fontSize: fontSize.tiny }}>
                <strong style={{ color: colors.warning }}>3.</strong> Save and reload this tab
              </p>
            </div>
          </div>
        </div>
      )}

      {/* Query Panel */}
      <div style={{
        borderBottom: `1px solid ${colors.panel}`,
        padding: '12px 16px',
        background: colors.panel,
        flexShrink: 0
      }}>
        <div style={{ display: 'grid', gridTemplateColumns: '2fr 1fr 1fr auto', gap: '10px', marginBottom: '10px' }}>
          <div>
            <label style={{ color: colors.textMuted, fontSize: fontSize.tiny, display: 'block', marginBottom: '4px', fontWeight: 'bold' }}>
              SERIES IDs
            </label>
            <input
              type="text"
              value={seriesIds}
              onChange={(e) => setSeriesIds(e.target.value)}
              placeholder="GDP,UNRATE,CPIAUCSL"
              style={{
                width: '100%',
                background: colors.background,
                border: `1px solid ${colors.textMuted}33`,
                color: colors.text,
                padding: '8px 10px',
                fontSize: fontSize.small,
                borderRadius: '3px',
                outline: 'none'
              }}
              onFocus={(e) => e.target.style.borderColor = colors.primary}
              onBlur={(e) => e.target.style.borderColor = `${colors.textMuted}33`}
            />
          </div>
          <div>
            <label style={{ color: colors.textMuted, fontSize: fontSize.tiny, display: 'block', marginBottom: '4px', fontWeight: 'bold' }}>
              START DATE
            </label>
            <input
              type="date"
              value={startDate}
              onChange={(e) => setStartDate(e.target.value)}
              style={{
                width: '100%',
                background: colors.background,
                border: `1px solid ${colors.textMuted}33`,
                color: colors.text,
                padding: '8px 10px',
                fontSize: fontSize.small,
                borderRadius: '3px',
                outline: 'none'
              }}
              onFocus={(e) => e.target.style.borderColor = colors.primary}
              onBlur={(e) => e.target.style.borderColor = `${colors.textMuted}33`}
            />
          </div>
          <div>
            <label style={{ color: colors.textMuted, fontSize: fontSize.tiny, display: 'block', marginBottom: '4px', fontWeight: 'bold' }}>
              END DATE
            </label>
            <input
              type="date"
              value={endDate}
              onChange={(e) => setEndDate(e.target.value)}
              style={{
                width: '100%',
                background: colors.background,
                border: `1px solid ${colors.textMuted}33`,
                color: colors.text,
                padding: '8px 10px',
                fontSize: fontSize.small,
                borderRadius: '3px',
                outline: 'none'
              }}
              onFocus={(e) => e.target.style.borderColor = colors.primary}
              onBlur={(e) => e.target.style.borderColor = `${colors.textMuted}33`}
            />
          </div>
          <button
            onClick={fetchData}
            disabled={loading}
            style={{
              background: loading ? colors.panel : colors.primary,
              border: 'none',
              color: colors.background,
              padding: '0 18px',
              fontSize: fontSize.small,
              cursor: loading ? 'not-allowed' : 'pointer',
              borderRadius: '3px',
              fontWeight: 'bold',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              alignSelf: 'end',
              opacity: loading ? 0.6 : 1,
              transition: 'all 0.2s'
            }}
            onMouseEnter={(e) => !loading && (e.currentTarget.style.opacity = '0.8')}
            onMouseLeave={(e) => !loading && (e.currentTarget.style.opacity = '1')}
          >
            {loading ? <Loader2 size={12} className="animate-spin" /> : <Search size={12} />}
            {loading ? 'LOADING' : 'FETCH'}
          </button>
        </div>

        {/* Popular Series Quick Add & Browse Button */}
        <div style={{ display: 'flex', alignItems: 'flex-end', gap: '10px' }}>
          <div style={{ flex: 1 }}>
            <label style={{ color: colors.textMuted, fontSize: fontSize.tiny, marginBottom: '6px', display: 'block', fontWeight: 'bold' }}>
              QUICK ADD
            </label>
            <div style={{ display: 'flex', gap: '4px', flexWrap: 'wrap' }}>
              {POPULAR_SERIES.map(series => (
                <button
                  key={series.id}
                  onClick={() => addPopularSeries(series.id)}
                  style={{
                    background: currentSeriesIdsSet.has(series.id) ? `${colors.success}22` : colors.background,
                    border: `1px solid ${currentSeriesIdsSet.has(series.id) ? colors.success : colors.textMuted}44`,
                    color: currentSeriesIdsSet.has(series.id) ? colors.success : colors.textMuted,
                    padding: '4px 10px',
                    fontSize: fontSize.tiny,
                    cursor: 'pointer',
                    borderRadius: '3px',
                    fontWeight: currentSeriesIdsSet.has(series.id) ? 'bold' : 'normal',
                    transition: 'all 0.2s'
                  }}
                  onMouseEnter={(e) => {
                    if (!currentSeriesIdsSet.has(series.id)) {
                      e.currentTarget.style.borderColor = colors.primary;
                      e.currentTarget.style.color = colors.primary;
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (!currentSeriesIdsSet.has(series.id)) {
                      e.currentTarget.style.borderColor = `${colors.textMuted}44`;
                      e.currentTarget.style.color = colors.textMuted;
                    }
                  }}
                >
                  {series.id}
                </button>
              ))}
            </div>
          </div>
          <button
            onClick={openBrowser}
            disabled={!apiKeyConfigured}
            style={{
              background: apiKeyConfigured ? colors.primary : colors.panel,
              border: `1px solid ${apiKeyConfigured ? colors.primary : colors.textMuted}44`,
              color: apiKeyConfigured ? colors.background : colors.textMuted,
              padding: '8px 14px',
              fontSize: fontSize.small,
              cursor: apiKeyConfigured ? 'pointer' : 'not-allowed',
              borderRadius: '3px',
              fontWeight: 'bold',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              whiteSpace: 'nowrap',
              opacity: apiKeyConfigured ? 1 : 0.5,
              transition: 'opacity 0.2s'
            }}
            onMouseEnter={(e) => apiKeyConfigured && (e.currentTarget.style.opacity = '0.8')}
            onMouseLeave={(e) => apiKeyConfigured && (e.currentTarget.style.opacity = '1')}
          >
            <FolderTree size={12} />
            BROWSE SERIES
          </button>
        </div>
      </div>

      {/* Main Content */}
      <div style={{ flex: 1, display: 'flex', gap: '12px', padding: '12px 16px', overflow: 'hidden', minHeight: 0 }}>

        {/* Chart Section */}
        <div style={{ flex: 2, display: 'flex', flexDirection: 'column', minWidth: 0 }}>
          <div style={{
            background: colors.panel,
            border: `1px solid ${colors.primary}33`,
            borderRadius: '4px',
            padding: '14px',
            height: '100%',
            display: 'flex',
            flexDirection: 'column'
          }}>
            <h3 style={{ color: colors.primary, fontSize: fontSize.body, fontWeight: 'bold', marginBottom: '10px', letterSpacing: '0.5px' }}>
              TIME SERIES CHART
            </h3>

            {loading && (
              <div style={{
                flex: 1,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                flexDirection: 'column',
                gap: '16px'
              }}>
                <Loader2 size={48} color={colors.primary} className="animate-spin" />
                <div style={{ textAlign: 'center' }}>
                  <p style={{ color: colors.primary, fontSize: fontSize.body, fontWeight: 'bold', marginBottom: '6px' }}>
                    {loadingMessage}
                  </p>
                  <p style={{ color: colors.textMuted, fontSize: fontSize.small }}>
                    Please wait while we fetch data from FRED...
                  </p>
                </div>
              </div>
            )}

            {!loading && error && (
              <div style={{
                background: '#3a0a0a',
                border: '2px solid #ff0000',
                color: '#ff0000',
                padding: '16px',
                borderRadius: '4px',
                fontSize: '11px',
                marginBottom: '12px',
                display: 'flex',
                alignItems: 'flex-start',
                gap: '12px',
                boxShadow: '0 2px 8px rgba(255, 0, 0, 0.2)'
              }}>
                <AlertCircle size={20} style={{ flexShrink: 0, marginTop: '2px' }} />
                <div style={{ flex: 1 }}>
                  <p style={{ fontWeight: 'bold', marginBottom: '4px' }}>Error Fetching Data</p>
                  <p style={{ color: '#ffaaaa', fontSize: '10px' }}>{error}</p>
                  {!apiKeyConfigured && (
                    <p style={{ color: '#ffaaaa', fontSize: '10px', marginTop: '8px' }}>
                      💡 Tip: Make sure you've configured your FRED API key in Settings → Credentials
                    </p>
                  )}
                </div>
              </div>
            )}

            {data.length === 0 && !loading && !error && (
              <div style={{
                flex: 1,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                color: colors.textMuted,
                fontSize: fontSize.body,
                flexDirection: 'column',
                gap: '10px'
              }}>
                <TrendingUp size={48} color={colors.textMuted} opacity={0.3} />
                <p style={{ color: colors.text }}>Enter series IDs and click FETCH to visualize data</p>
                <p style={{ fontSize: fontSize.small, color: colors.textMuted }}>Try searching for GDP, UNRATE, or CPIAUCSL</p>
              </div>
            )}

            {data.length > 0 && (
              <div style={{ flex: 1, minHeight: 0 }}>
                <ResponsiveContainer width="100%" height="100%">
                  {chartType === 'line' ? (
                    <LineChart data={prepareChartData()} margin={{ top: 5, right: 20, left: 10, bottom: 5 }}>
                      <CartesianGrid strokeDasharray="3 3" stroke={`${colors.textMuted}22`} />
                      <XAxis dataKey="date" stroke={colors.textMuted} style={{ fontSize: fontSize.tiny, fill: colors.textMuted }} />
                      <YAxis
                        stroke={colors.textMuted}
                        style={{ fontSize: fontSize.tiny, fill: colors.textMuted }}
                        label={normalizeData ? { value: 'Normalized (0-100)', angle: -90, position: 'insideLeft', style: { fontSize: fontSize.tiny, fill: colors.textMuted } } : undefined}
                      />
                      <Tooltip
                        contentStyle={{ background: colors.background, border: `1px solid ${colors.primary}`, borderRadius: '3px', fontSize: fontSize.small }}
                        labelStyle={{ color: colors.primary, fontSize: fontSize.tiny }}
                        itemStyle={{ color: colors.text, fontSize: fontSize.tiny }}
                      />
                      <Legend wrapperStyle={{ fontSize: fontSize.tiny }} />
                      {data.map((series, idx) => (
                        <Line
                          key={series.series_id}
                          type="monotone"
                          dataKey={series.series_id}
                          stroke={[colors.primary, colors.success, colors.info, colors.warning, colors.alert][idx % 5]}
                          strokeWidth={2}
                          dot={false}
                        />
                      ))}
                    </LineChart>
                  ) : (
                    <AreaChart data={prepareChartData()} margin={{ top: 5, right: 20, left: 10, bottom: 5 }}>
                      <CartesianGrid strokeDasharray="3 3" stroke={`${colors.textMuted}22`} />
                      <XAxis dataKey="date" stroke={colors.textMuted} style={{ fontSize: fontSize.tiny, fill: colors.textMuted }} />
                      <YAxis
                        stroke={colors.textMuted}
                        style={{ fontSize: fontSize.tiny, fill: colors.textMuted }}
                        label={normalizeData ? { value: 'Normalized (0-100)', angle: -90, position: 'insideLeft', style: { fontSize: fontSize.tiny, fill: colors.textMuted } } : undefined}
                      />
                      <Tooltip
                        contentStyle={{ background: colors.background, border: `1px solid ${colors.primary}`, borderRadius: '3px', fontSize: fontSize.small }}
                        labelStyle={{ color: colors.primary, fontSize: fontSize.tiny }}
                        itemStyle={{ color: colors.text, fontSize: fontSize.tiny }}
                      />
                      <Legend wrapperStyle={{ fontSize: fontSize.tiny }} />
                      {data.map((series, idx) => (
                        <Area
                          key={series.series_id}
                          type="monotone"
                          dataKey={series.series_id}
                          stroke={[colors.primary, colors.success, colors.info, colors.warning, colors.alert][idx % 5]}
                          fill={`${[colors.primary, colors.success, colors.info, colors.warning, colors.alert][idx % 5]}33`}
                        />
                      ))}
                    </AreaChart>
                  )}
                </ResponsiveContainer>
              </div>
            )}
          </div>
        </div>

        {/* Data Table */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', minWidth: 0, overflowY: 'auto', gap: '10px' }}>
          {data.map(series => (
            <div
              key={series.series_id}
              style={{
                background: colors.panel,
                border: `1px solid ${colors.primary}33`,
                borderRadius: '4px',
                padding: '12px',
                flexShrink: 0
              }}
            >
              <div style={{ borderBottom: `1px solid ${colors.textMuted}33`, paddingBottom: '8px', marginBottom: '8px' }}>
                <h4 style={{ color: colors.primary, fontSize: fontSize.body, fontWeight: 'bold', marginBottom: '4px' }}>
                  {series.series_id}
                </h4>
                <p style={{ color: colors.text, fontSize: fontSize.tiny, marginBottom: '6px' }}>{series.title}</p>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '4px', fontSize: fontSize.tiny, color: colors.textMuted }}>
                  <span>Units: {series.units}</span>
                  <span>Freq: {series.frequency}</span>
                  <span>Obs: {series.observation_count}</span>
                  <span>Updated: {series.last_updated.split('T')[0]}</span>
                </div>
              </div>

              <div style={{ maxHeight: '200px', overflowY: 'auto' }}>
                <table style={{ width: '100%', fontSize: fontSize.tiny, borderCollapse: 'collapse' }}>
                  <thead style={{ position: 'sticky', top: 0, background: colors.panel }}>
                    <tr style={{ borderBottom: `1px solid ${colors.textMuted}33` }}>
                      <th style={{ color: colors.textMuted, textAlign: 'left', padding: '6px 4px', fontWeight: 'bold' }}>Date</th>
                      <th style={{ color: colors.textMuted, textAlign: 'right', padding: '6px 4px', fontWeight: 'bold' }}>Value</th>
                    </tr>
                  </thead>
                  <tbody>
                    {series.observations.slice(-20).reverse().map((obs, idx) => (
                      <tr key={idx} style={{ borderBottom: `1px solid ${colors.background}` }}>
                        <td style={{ color: colors.textMuted, padding: '4px' }}>{obs.date}</td>
                        <td style={{ color: colors.text, textAlign: 'right', padding: '4px', fontFamily: 'monospace', fontSize: fontSize.tiny }}>
                          {obs.value.toFixed(2)}
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            </div>
          ))}
        </div>
      </div>

      {/* Footer */}
      <div style={{
        borderTop: `1px solid ${colors.primary}`,
        padding: '8px 16px',
        background: colors.panel,
        fontSize: fontSize.tiny,
        color: colors.textMuted,
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        flexShrink: 0
      }}>
        <span>FRED Economic Data | Federal Reserve Bank of St. Louis</span>
        {data.length > 0 && (
          <span style={{ color: colors.primary, fontWeight: 'bold' }}>
            {data.length} series | {data.reduce((acc, s) => acc + s.observation_count, 0)} observations
          </span>
        )}
      </div>
    </div>
  );
}
