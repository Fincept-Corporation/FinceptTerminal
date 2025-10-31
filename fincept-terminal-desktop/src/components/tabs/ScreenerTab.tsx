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
      setApiKeyConfigured(!!apiKey);
    } catch (error) {
      console.error('Failed to check FRED API key:', error);
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
    try {
      const apiKey = await sqliteService.getApiKey('FRED');
      if (!apiKey) return;

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
  const addSeriesFromBrowser = (id: string) => {
    if (!getCurrentSeriesIds().includes(id)) {
      setSeriesIds(prev => prev ? `${prev},${id}` : id);
    }
  };

  const fetchData = async () => {
    setLoading(true);
    setError(null);

    // Use setTimeout to make it non-blocking
    setTimeout(async () => {
      try {
        const apiKey = await sqliteService.getApiKey('FRED');
        if (!apiKey) {
          setError('FRED API key not configured. Please add it in Settings > Credentials.');
          setApiKeyConfigured(false);
          setLoading(false);
          return;
        }

        // Remove duplicates and trim
        const ids = [...new Set(seriesIds.split(',').map(s => s.trim()).filter(Boolean))];

        if (ids.length === 0) {
          setError('Please enter at least one series ID');
          setLoading(false);
          return;
        }

        const args = ['multiple', ...ids, startDate, endDate];

        const result = await invoke<string>('execute_python_script', {
          scriptName: 'fred_data.py',
          args,
          env: { FRED_API_KEY: apiKey }
        });

        const parsed = JSON.parse(result);
        if (parsed.error) {
          setError(parsed.error);
        } else {
          setData(parsed);
        }
      } catch (err: any) {
        setError(err.message || 'Failed to fetch data');
      } finally {
        setLoading(false);
      }
    }, 0);
  };

  // Helper to get current series IDs as array
  const getCurrentSeriesIds = () => {
    return seriesIds.split(',').map(s => s.trim()).filter(Boolean);
  };

  const addPopularSeries = (id: string) => {
    if (!getCurrentSeriesIds().includes(id)) {
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

    return sortedDates.map(date => {
      const point: any = { date };
      data.forEach(series => {
        const obs = series.observations.find(o => o.date === date);
        point[series.series_id] = obs?.value || null;
      });
      return point;
    });
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
    <div style={{ width: '100%', height: '100%', display: 'flex', flexDirection: 'column', backgroundColor: 'colors.background', overflow: 'hidden' }}>
      <style>{`
        *::-webkit-scrollbar { width: 8px; height: 8px; }
        *::-webkit-scrollbar-track { background: colors.panel; }
        *::-webkit-scrollbar-thumb { background: #2a2a2a; border-radius: 4px; }
        *::-webkit-scrollbar-thumb:hover { background: #3a3a3a; }
      `}</style>

      {/* Series Browser Modal */}
      {showBrowser && (
        <div style={{
          position: 'fixed',
          top: 0,
          left: 0,
          right: 0,
          bottom: 0,
          background: 'rgba(0,0,0,0.9)',
          zIndex: 1000,
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          padding: '20px'
        }}>
          <div style={{
            background: 'colors.panel',
            border: '2px solid #ea580c',
            borderRadius: '8px',
            width: '90%',
            maxWidth: '1200px',
            height: '85vh',
            display: 'flex',
            flexDirection: 'column',
            overflow: 'hidden'
          }}>
            {/* Modal Header */}
            <div style={{
              borderBottom: '2px solid #ea580c',
              padding: '16px 20px',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
              background: 'linear-gradient(180deg, #1a1a1a 0%, colors.panel 100%)'
            }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                <FolderTree size={20} color="#ea580c" />
                <span style={{ color: '#ea580c', fontSize: '16px', fontWeight: 'bold', letterSpacing: '1px' }}>
                  FRED SERIES BROWSER
                </span>
              </div>
              <button
                onClick={() => setShowBrowser(false)}
                style={{
                  background: 'transparent',
                  border: 'none',
                  color: '#888',
                  cursor: 'pointer',
                  padding: '4px'
                }}
              >
                <X size={24} />
              </button>
            </div>

            {/* Search Bar */}
            <div style={{ padding: '16px 20px', borderBottom: '1px solid #1a1a1a' }}>
              <div style={{ position: 'relative' }}>
                <Search size={16} color="#666" style={{ position: 'absolute', left: '12px', top: '50%', transform: 'translateY(-50%)' }} />
                <input
                  type="text"
                  placeholder="Search FRED series by keyword (e.g., 'unemployment', 'inflation', 'GDP')..."
                  value={searchQuery}
                  onChange={(e) => setSearchQuery(e.target.value)}
                  style={{
                    width: '100%',
                    background: 'colors.background',
                    border: '1px solid #2a2a2a',
                    color: 'colors.text',
                    padding: '12px 12px 12px 40px',
                    fontSize: '11px',
                    borderRadius: '4px'
                  }}
                />
                {searchLoading && (
                  <Loader2 size={16} className="animate-spin" style={{ position: 'absolute', right: '12px', top: '50%', transform: 'translateY(-50%)', color: '#ea580c' }} />
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
                {searchQuery ? (
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
        borderBottom: '2px solid #ea580c',
        padding: '12px 16px',
        background: 'linear-gradient(180deg, #1a1a1a 0%, colors.panel 100%)',
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <BarChart3 size={20} color="#ea580c" />
          <span style={{ color: '#ea580c', fontSize: '16px', fontWeight: 'bold', letterSpacing: '1px' }}>
            FRED DATA EXPLORER
          </span>
        </div>
        <div style={{ display: 'flex', gap: '8px' }}>
          <button
            onClick={() => setChartType(chartType === 'line' ? 'area' : 'line')}
            style={{
              background: 'colors.panel',
              border: '1px solid #2a2a2a',
              color: '#ea580c',
              padding: '6px 12px',
              fontSize: '10px',
              cursor: 'pointer',
              borderRadius: '3px',
              fontWeight: 'bold'
            }}
          >
            {chartType === 'line' ? 'AREA' : 'LINE'}
          </button>
          <button
            onClick={exportToCSV}
            disabled={data.length === 0}
            style={{
              background: 'colors.panel',
              border: '1px solid #2a2a2a',
              color: data.length > 0 ? '#00ff00' : '#666',
              padding: '6px 12px',
              fontSize: '10px',
              cursor: data.length > 0 ? 'pointer' : 'not-allowed',
              borderRadius: '3px',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              fontWeight: 'bold'
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
          background: '#3a1a0a',
          border: '1px solid #ea580c',
          padding: '12px 16px',
          display: 'flex',
          alignItems: 'center',
          gap: '12px',
          flexShrink: 0
        }}>
          <AlertCircle size={20} color="#ea580c" />
          <div style={{ flex: 1 }}>
            <p style={{ color: '#ea580c', fontSize: '11px', fontWeight: 'bold', marginBottom: '4px' }}>
              FRED API Key Required
            </p>
            <p style={{ color: '#888', fontSize: '10px' }}>
              Please add your FRED API key in Settings → Credentials to use this feature. Get your free key at <span style={{ color: '#ea580c' }}>research.stlouisfed.org/useraccount/apikey</span>
            </p>
          </div>
        </div>
      )}

      {/* Query Panel */}
      <div style={{
        borderBottom: '1px solid #1a1a1a',
        padding: '16px',
        background: 'colors.panel',
        flexShrink: 0
      }}>
        <div style={{ display: 'grid', gridTemplateColumns: '2fr 1fr 1fr auto', gap: '12px', marginBottom: '12px' }}>
          <div>
            <label style={{ color: '#888', fontSize: '9px', display: 'block', marginBottom: '4px' }}>
              FRED SERIES IDs (comma-separated)
            </label>
            <input
              type="text"
              value={seriesIds}
              onChange={(e) => setSeriesIds(e.target.value)}
              placeholder="GDP,UNRATE,CPIAUCSL"
              style={{
                width: '100%',
                background: 'colors.background',
                border: '1px solid #2a2a2a',
                color: 'colors.text',
                padding: '8px',
                fontSize: '10px',
                borderRadius: '3px'
              }}
            />
          </div>
          <div>
            <label style={{ color: '#888', fontSize: '9px', display: 'block', marginBottom: '4px' }}>
              START DATE
            </label>
            <input
              type="date"
              value={startDate}
              onChange={(e) => setStartDate(e.target.value)}
              style={{
                width: '100%',
                background: 'colors.background',
                border: '1px solid #2a2a2a',
                color: 'colors.text',
                padding: '8px',
                fontSize: '10px',
                borderRadius: '3px'
              }}
            />
          </div>
          <div>
            <label style={{ color: '#888', fontSize: '9px', display: 'block', marginBottom: '4px' }}>
              END DATE
            </label>
            <input
              type="date"
              value={endDate}
              onChange={(e) => setEndDate(e.target.value)}
              style={{
                width: '100%',
                background: 'colors.background',
                border: '1px solid #2a2a2a',
                color: 'colors.text',
                padding: '8px',
                fontSize: '10px',
                borderRadius: '3px'
              }}
            />
          </div>
          <button
            onClick={fetchData}
            disabled={loading}
            style={{
              background: '#ea580c',
              border: 'none',
              color: 'colors.text',
              padding: '0 20px',
              fontSize: '10px',
              cursor: loading ? 'not-allowed' : 'pointer',
              borderRadius: '3px',
              fontWeight: 'bold',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              alignSelf: 'end',
              opacity: loading ? 0.5 : 1
            }}
          >
            {loading ? <Loader2 size={14} className="animate-spin" /> : <Search size={14} />}
            {loading ? 'LOADING' : 'FETCH'}
          </button>
        </div>

        {/* Popular Series Quick Add & Browse Button */}
        <div style={{ display: 'flex', alignItems: 'flex-end', gap: '12px' }}>
          <div style={{ flex: 1 }}>
            <label style={{ color: '#888', fontSize: '9px', marginBottom: '6px', display: 'block' }}>
              QUICK ADD:
            </label>
            <div style={{ display: 'flex', gap: '6px', flexWrap: 'wrap' }}>
              {POPULAR_SERIES.map(series => (
                <button
                  key={series.id}
                  onClick={() => addPopularSeries(series.id)}
                  style={{
                    background: currentSeriesIdsSet.has(series.id) ? '#1a3a1a' : 'colors.panel',
                    border: `1px solid ${currentSeriesIdsSet.has(series.id) ? '#00ff00' : '#2a2a2a'}`,
                    color: currentSeriesIdsSet.has(series.id) ? '#00ff00' : '#666',
                    padding: '4px 8px',
                    fontSize: '9px',
                    cursor: 'pointer',
                    borderRadius: '3px'
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
              background: 'linear-gradient(135deg, #ea580c 0%, #c2410c 100%)',
              border: '1px solid #ea580c',
              color: 'colors.text',
              padding: '8px 16px',
              fontSize: '10px',
              cursor: apiKeyConfigured ? 'pointer' : 'not-allowed',
              borderRadius: '3px',
              fontWeight: 'bold',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              whiteSpace: 'nowrap',
              opacity: apiKeyConfigured ? 1 : 0.5,
              boxShadow: apiKeyConfigured ? '0 2px 8px rgba(234, 88, 12, 0.3)' : 'none'
            }}
          >
            <FolderTree size={14} />
            BROWSE ALL SERIES
          </button>
        </div>
      </div>

      {/* Main Content */}
      <div style={{ flex: 1, display: 'flex', gap: '16px', padding: '16px', overflow: 'hidden', minHeight: 0 }}>

        {/* Chart Section */}
        <div style={{ flex: 2, display: 'flex', flexDirection: 'column', minWidth: 0 }}>
          <div style={{
            background: 'colors.panel',
            border: '1px solid #1a1a1a',
            borderRadius: '4px',
            padding: '16px',
            height: '100%',
            display: 'flex',
            flexDirection: 'column'
          }}>
            <h3 style={{ color: 'colors.text', fontSize: '12px', fontWeight: 'bold', marginBottom: '12px' }}>
              TIME SERIES CHART
            </h3>

            {error && (
              <div style={{
                background: '#3a0a0a',
                border: '1px solid #ff0000',
                color: '#ff0000',
                padding: '12px',
                borderRadius: '3px',
                fontSize: '10px',
                marginBottom: '12px'
              }}>
                {error}
              </div>
            )}

            {data.length === 0 && !loading && !error && (
              <div style={{
                flex: 1,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                color: '#666',
                fontSize: '11px',
                flexDirection: 'column',
                gap: '8px'
              }}>
                <TrendingUp size={40} color="#333" />
                <p>Enter series IDs and click FETCH to visualize data</p>
              </div>
            )}

            {data.length > 0 && (
              <div style={{ flex: 1, minHeight: 0 }}>
                <ResponsiveContainer width="100%" height="100%">
                  {chartType === 'line' ? (
                    <LineChart data={prepareChartData()} margin={{ top: 5, right: 30, left: 20, bottom: 5 }}>
                      <CartesianGrid strokeDasharray="3 3" stroke="#1a1a1a" />
                      <XAxis dataKey="date" stroke="#666" style={{ fontSize: '10px' }} />
                      <YAxis stroke="#666" style={{ fontSize: '10px' }} />
                      <Tooltip
                        contentStyle={{ background: 'colors.panel', border: '1px solid #2a2a2a', borderRadius: '3px', fontSize: '10px' }}
                        labelStyle={{ color: '#ea580c' }}
                      />
                      <Legend wrapperStyle={{ fontSize: '10px' }} />
                      {data.map((series, idx) => (
                        <Line
                          key={series.series_id}
                          type="monotone"
                          dataKey={series.series_id}
                          stroke={['#ea580c', '#00ff00', '#00ffff', '#ff00ff', 'colors.textf00'][idx % 5]}
                          strokeWidth={2}
                          dot={false}
                        />
                      ))}
                    </LineChart>
                  ) : (
                    <AreaChart data={prepareChartData()} margin={{ top: 5, right: 30, left: 20, bottom: 5 }}>
                      <CartesianGrid strokeDasharray="3 3" stroke="#1a1a1a" />
                      <XAxis dataKey="date" stroke="#666" style={{ fontSize: '10px' }} />
                      <YAxis stroke="#666" style={{ fontSize: '10px' }} />
                      <Tooltip
                        contentStyle={{ background: 'colors.panel', border: '1px solid #2a2a2a', borderRadius: '3px', fontSize: '10px' }}
                        labelStyle={{ color: '#ea580c' }}
                      />
                      <Legend wrapperStyle={{ fontSize: '10px' }} />
                      {data.map((series, idx) => (
                        <Area
                          key={series.series_id}
                          type="monotone"
                          dataKey={series.series_id}
                          stroke={['#ea580c', '#00ff00', '#00ffff', '#ff00ff', 'colors.textf00'][idx % 5]}
                          fill={['#ea580c33', '#00ff0033', '#00ffff33', '#ff00ff33', 'colors.textf0033'][idx % 5]}
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
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', minWidth: 0, overflowY: 'auto' }}>
          {data.map(series => (
            <div
              key={series.series_id}
              style={{
                background: 'colors.panel',
                border: '1px solid #1a1a1a',
                borderRadius: '4px',
                padding: '12px',
                marginBottom: '12px',
                flexShrink: 0
              }}
            >
              <div style={{ borderBottom: '1px solid #1a1a1a', paddingBottom: '8px', marginBottom: '8px' }}>
                <h4 style={{ color: '#ea580c', fontSize: '11px', fontWeight: 'bold', marginBottom: '4px' }}>
                  {series.series_id}
                </h4>
                <p style={{ color: '#888', fontSize: '9px', marginBottom: '4px' }}>{series.title}</p>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '4px', fontSize: '9px', color: '#666' }}>
                  <span>Units: {series.units}</span>
                  <span>Freq: {series.frequency}</span>
                  <span>Observations: {series.observation_count}</span>
                  <span>Updated: {series.last_updated.split('T')[0]}</span>
                </div>
              </div>

              <div style={{ maxHeight: '200px', overflowY: 'auto' }}>
                <table style={{ width: '100%', fontSize: '9px', borderCollapse: 'collapse' }}>
                  <thead style={{ position: 'sticky', top: 0, background: 'colors.panel' }}>
                    <tr style={{ borderBottom: '1px solid #1a1a1a' }}>
                      <th style={{ color: '#888', textAlign: 'left', padding: '4px' }}>Date</th>
                      <th style={{ color: '#888', textAlign: 'right', padding: '4px' }}>Value</th>
                    </tr>
                  </thead>
                  <tbody>
                    {series.observations.slice(-20).reverse().map((obs, idx) => (
                      <tr key={idx} style={{ borderBottom: '1px solid colors.panel' }}>
                        <td style={{ color: '#666', padding: '4px' }}>{obs.date}</td>
                        <td style={{ color: 'colors.text', textAlign: 'right', padding: '4px', fontFamily: 'monospace' }}>
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
        borderTop: '1px solid #1a1a1a',
        padding: '8px 16px',
        background: 'colors.panel',
        fontSize: '9px',
        color: '#666',
        display: 'flex',
        justifyContent: 'space-between',
        flexShrink: 0
      }}>
        <span>FRED Economic Data | Federal Reserve Bank of St. Louis</span>
        <span>{data.length} series loaded | {data.reduce((acc, s) => acc + s.observation_count, 0)} total observations</span>
      </div>
    </div>
  );
}
