/**
 * AKShare Data Explorer Tab
 *
 * Comprehensive interface for exploring AKShare data sources including:
 * - Bonds (Chinese & International)
 * - Derivatives (Options, Futures)
 * - China Economics (GDP, CPI, PMI, etc.)
 * - Global Economics (US, EU, UK, Japan, etc.)
 * - Funds (ETFs, Mutual Funds)
 * - Alternative Data (Air Quality, Carbon, Real Estate, etc.)
 */

import React, { useState, useEffect, useCallback, useMemo } from 'react';
import { useTranslation } from 'react-i18next';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { TabFooter } from '@/components/common/TabFooter';
import { AKShareAPI, AKSHARE_DATA_SOURCES, AKShareDataSource, AKShareResponse } from '@/services/akshareApi';
import {
  Search, RefreshCw, Download, ChevronRight, ChevronDown, Database,
  Landmark, LineChart, TrendingUp, Globe, PieChart, Layers, BarChart3,
  AlertCircle, CheckCircle2, Clock, Filter, X, Copy, Table, Code,
  ArrowUpDown, ExternalLink, Bookmark, BookmarkCheck, History, Zap, Building2,
  Activity, DollarSign, Newspaper, Building, Bitcoin, FileText, Users,
  ArrowRightLeft, LayoutGrid, Percent, Flame
} from 'lucide-react';

// Color palette
const COLORS = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0A0A0A',
  HEADER_BG: '#111111',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#3B82F6',
  PURPLE: '#8B5CF6',
  PINK: '#EC4899',
  BORDER: '#1F1F1F',
  HOVER: '#1A1A1A',
  MUTED: '#4A4A4A'
};

// Icon mapping
const IconMap: Record<string, React.FC<{ size?: number; color?: string }>> = {
  Landmark,
  LineChart,
  TrendingUp,
  Globe,
  PieChart,
  Layers,
  BarChart3,
  Building2,
  Activity,
  DollarSign,
  Zap,
  Bitcoin,
  Newspaper,
  Building,
  // Stock data icons
  Clock,
  FileText,
  Users,
  ArrowRightLeft,
  LayoutGrid,
  Percent,
  Flame,
};

interface QueryHistoryItem {
  id: string;
  script: string;
  endpoint: string;
  timestamp: number;
  success: boolean;
  count?: number;
}

interface FavoriteEndpoint {
  script: string;
  endpoint: string;
  name: string;
}

const AkShareDataTab: React.FC = () => {
  const { t } = useTranslation();
  const { colors } = useTerminalTheme();

  // State
  const [selectedSource, setSelectedSource] = useState<AKShareDataSource | null>(null);
  const [endpoints, setEndpoints] = useState<string[]>([]);
  const [categories, setCategories] = useState<Record<string, string[]>>({});
  const [selectedEndpoint, setSelectedEndpoint] = useState<string | null>(null);
  const [searchQuery, setSearchQuery] = useState('');
  const [expandedCategories, setExpandedCategories] = useState<Set<string>>(new Set());

  // Data state
  const [loading, setLoading] = useState(false);
  const [loadingEndpoints, setLoadingEndpoints] = useState(false);
  const [data, setData] = useState<any[] | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [responseInfo, setResponseInfo] = useState<{ count: number; timestamp: number; source?: string } | null>(null);

  // View state
  const [viewMode, setViewMode] = useState<'table' | 'json'>('table');
  const [sortColumn, setSortColumn] = useState<string | null>(null);
  const [sortDirection, setSortDirection] = useState<'asc' | 'desc'>('asc');

  // History & Favorites
  const [queryHistory, setQueryHistory] = useState<QueryHistoryItem[]>([]);
  const [favorites, setFavorites] = useState<FavoriteEndpoint[]>([]);
  const [showHistory, setShowHistory] = useState(false);
  const [showFavorites, setShowFavorites] = useState(false);

  // Load endpoints when source changes
  useEffect(() => {
    if (selectedSource) {
      loadEndpoints(selectedSource.script);
    }
  }, [selectedSource]);

  // Load favorites from localStorage
  useEffect(() => {
    const saved = localStorage.getItem('akshare_favorites');
    if (saved) {
      try {
        setFavorites(JSON.parse(saved));
      } catch (e) {
        console.error('Failed to load favorites:', e);
      }
    }

    const savedHistory = localStorage.getItem('akshare_history');
    if (savedHistory) {
      try {
        setQueryHistory(JSON.parse(savedHistory).slice(0, 50));
      } catch (e) {
        console.error('Failed to load history:', e);
      }
    }
  }, []);

  const loadEndpoints = async (script: string) => {
    setLoadingEndpoints(true);
    setEndpoints([]);
    setCategories({});
    try {
      const response = await AKShareAPI.getEndpoints(script);
      if (response.success && response.data) {
        const endpointData = response.data as any;
        setEndpoints(endpointData.available_endpoints || []);
        setCategories(endpointData.categories || {});
        // Auto-expand first category
        if (endpointData.categories) {
          const firstCategory = Object.keys(endpointData.categories)[0];
          if (firstCategory) {
            setExpandedCategories(new Set([firstCategory]));
          }
        }
      }
    } catch (err) {
      console.error('Failed to load endpoints:', err);
    } finally {
      setLoadingEndpoints(false);
    }
  };

  const executeQuery = async (script: string, endpoint: string) => {
    setLoading(true);
    setError(null);
    setData(null);
    setResponseInfo(null);
    setSelectedEndpoint(endpoint);

    try {
      const response = await AKShareAPI.query(script, endpoint);

      // Add to history
      const historyItem: QueryHistoryItem = {
        id: `${Date.now()}`,
        script,
        endpoint,
        timestamp: Date.now(),
        success: response.success,
        count: response.count
      };
      const newHistory = [historyItem, ...queryHistory.slice(0, 49)];
      setQueryHistory(newHistory);
      localStorage.setItem('akshare_history', JSON.stringify(newHistory));

      if (response.success && response.data) {
        const dataArray = Array.isArray(response.data) ? response.data : [response.data];
        setData(dataArray);
        setResponseInfo({
          count: response.count || dataArray.length,
          timestamp: response.timestamp || Date.now(),
          source: response.source
        });
      } else {
        setError(typeof response.error === 'string' ? response.error : JSON.stringify(response.error));
      }
    } catch (err: any) {
      setError(err.message || 'Query failed');
    } finally {
      setLoading(false);
    }
  };

  const toggleFavorite = (script: string, endpoint: string) => {
    const exists = favorites.some(f => f.script === script && f.endpoint === endpoint);
    let newFavorites: FavoriteEndpoint[];

    if (exists) {
      newFavorites = favorites.filter(f => !(f.script === script && f.endpoint === endpoint));
    } else {
      newFavorites = [...favorites, { script, endpoint, name: endpoint }];
    }

    setFavorites(newFavorites);
    localStorage.setItem('akshare_favorites', JSON.stringify(newFavorites));
  };

  const isFavorite = (script: string, endpoint: string) => {
    return favorites.some(f => f.script === script && f.endpoint === endpoint);
  };

  const toggleCategory = (category: string) => {
    const newExpanded = new Set(expandedCategories);
    if (newExpanded.has(category)) {
      newExpanded.delete(category);
    } else {
      newExpanded.add(category);
    }
    setExpandedCategories(newExpanded);
  };

  // Filter endpoints by search query
  const filteredEndpoints = useMemo(() => {
    if (!searchQuery) return endpoints;
    const query = searchQuery.toLowerCase();
    return endpoints.filter(e => e.toLowerCase().includes(query));
  }, [endpoints, searchQuery]);

  // Get filtered categories
  const filteredCategories = useMemo(() => {
    if (!searchQuery) return categories;
    const query = searchQuery.toLowerCase();
    const filtered: Record<string, string[]> = {};

    for (const [cat, eps] of Object.entries(categories)) {
      const matchingEps = eps.filter(e => e.toLowerCase().includes(query));
      if (matchingEps.length > 0 || cat.toLowerCase().includes(query)) {
        filtered[cat] = matchingEps.length > 0 ? matchingEps : eps;
      }
    }
    return filtered;
  }, [categories, searchQuery]);

  // Sort data
  const sortedData = useMemo(() => {
    if (!data || !sortColumn) return data;
    return [...data].sort((a, b) => {
      const aVal = a[sortColumn];
      const bVal = b[sortColumn];
      if (aVal === bVal) return 0;
      if (aVal === null || aVal === undefined) return 1;
      if (bVal === null || bVal === undefined) return -1;
      const comparison = aVal < bVal ? -1 : 1;
      return sortDirection === 'asc' ? comparison : -comparison;
    });
  }, [data, sortColumn, sortDirection]);

  // Get table columns
  const columns = useMemo(() => {
    if (!data || data.length === 0) return [];
    return Object.keys(data[0]);
  }, [data]);

  const copyToClipboard = (text: string) => {
    navigator.clipboard.writeText(text);
  };

  const downloadCSV = () => {
    if (!data || data.length === 0) return;

    const headers = columns.join(',');
    const rows = data.map(row =>
      columns.map(col => {
        const val = row[col];
        if (typeof val === 'string' && (val.includes(',') || val.includes('"'))) {
          return `"${val.replace(/"/g, '""')}"`;
        }
        return val;
      }).join(',')
    );

    const csv = [headers, ...rows].join('\n');
    const blob = new Blob([csv], { type: 'text/csv' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `akshare_${selectedEndpoint}_${Date.now()}.csv`;
    a.click();
    URL.revokeObjectURL(url);
  };

  const downloadJSON = () => {
    if (!data) return;
    const json = JSON.stringify(data, null, 2);
    const blob = new Blob([json], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `akshare_${selectedEndpoint}_${Date.now()}.json`;
    a.click();
    URL.revokeObjectURL(url);
  };

  return (
    <div className="flex flex-col h-full" style={{ background: COLORS.DARK_BG }}>
      {/* Header */}
      <div className="flex items-center justify-between px-4 py-3 border-b" style={{ borderColor: COLORS.BORDER, background: COLORS.HEADER_BG }}>
        <div className="flex items-center gap-3">
          <Database size={20} color={COLORS.ORANGE} />
          <span className="font-semibold text-white">AKShare Data Explorer</span>
          <span className="text-xs px-2 py-0.5 rounded" style={{ background: COLORS.ORANGE + '20', color: COLORS.ORANGE }}>
            400+ Endpoints
          </span>
        </div>
        <div className="flex items-center gap-2">
          <button
            onClick={() => setShowFavorites(!showFavorites)}
            className="p-2 rounded hover:bg-white/10 transition-colors"
            title="Favorites"
          >
            {showFavorites ? <BookmarkCheck size={18} color={COLORS.YELLOW} /> : <Bookmark size={18} color={COLORS.MUTED} />}
          </button>
          <button
            onClick={() => setShowHistory(!showHistory)}
            className="p-2 rounded hover:bg-white/10 transition-colors"
            title="History"
          >
            <History size={18} color={showHistory ? COLORS.CYAN : COLORS.MUTED} />
          </button>
        </div>
      </div>

      <div className="flex flex-1 overflow-hidden">
        {/* Left Panel - Data Sources */}
        <div className="w-64 border-r flex flex-col" style={{ borderColor: COLORS.BORDER, background: COLORS.PANEL_BG }}>
          <div className="p-3 border-b" style={{ borderColor: COLORS.BORDER }}>
            <span className="text-xs font-medium" style={{ color: COLORS.MUTED }}>DATA SOURCES</span>
          </div>
          <div className="flex-1 overflow-y-auto">
            {AKSHARE_DATA_SOURCES.map(source => {
              const IconComponent = IconMap[source.icon] || Database;
              const isSelected = selectedSource?.id === source.id;

              return (
                <button
                  key={source.id}
                  onClick={() => setSelectedSource(source)}
                  className="w-full px-3 py-3 flex items-start gap-3 hover:bg-white/5 transition-colors text-left"
                  style={{
                    background: isSelected ? COLORS.HOVER : 'transparent',
                    borderLeft: isSelected ? `2px solid ${source.color}` : '2px solid transparent'
                  }}
                >
                  <IconComponent size={18} color={source.color} />
                  <div className="flex-1 min-w-0">
                    <div className="font-medium text-white text-sm">{source.name}</div>
                    <div className="text-xs mt-0.5 line-clamp-2" style={{ color: COLORS.MUTED }}>
                      {source.description}
                    </div>
                  </div>
                </button>
              );
            })}
          </div>
        </div>

        {/* Middle Panel - Endpoints */}
        <div className="w-80 border-r flex flex-col" style={{ borderColor: COLORS.BORDER, background: COLORS.PANEL_BG }}>
          {selectedSource ? (
            <>
              <div className="p-3 border-b" style={{ borderColor: COLORS.BORDER }}>
                <div className="relative">
                  <Search size={16} className="absolute left-3 top-1/2 -translate-y-1/2" color={COLORS.MUTED} />
                  <input
                    type="text"
                    value={searchQuery}
                    onChange={e => setSearchQuery(e.target.value)}
                    placeholder="Search endpoints..."
                    className="w-full pl-9 pr-8 py-2 rounded text-sm"
                    style={{
                      background: COLORS.DARK_BG,
                      border: `1px solid ${COLORS.BORDER}`,
                      color: COLORS.WHITE
                    }}
                  />
                  {searchQuery && (
                    <button
                      onClick={() => setSearchQuery('')}
                      className="absolute right-2 top-1/2 -translate-y-1/2"
                    >
                      <X size={14} color={COLORS.MUTED} />
                    </button>
                  )}
                </div>
                <div className="mt-2 text-xs" style={{ color: COLORS.MUTED }}>
                  {filteredEndpoints.length} endpoints available
                </div>
              </div>

              <div className="flex-1 overflow-y-auto">
                {loadingEndpoints ? (
                  <div className="flex items-center justify-center py-8">
                    <RefreshCw size={20} className="animate-spin" color={COLORS.ORANGE} />
                  </div>
                ) : Object.keys(filteredCategories).length > 0 ? (
                  Object.entries(filteredCategories).map(([category, categoryEndpoints]) => (
                    <div key={category}>
                      <button
                        onClick={() => toggleCategory(category)}
                        className="w-full px-3 py-2 flex items-center gap-2 hover:bg-white/5 text-left"
                        style={{ borderBottom: `1px solid ${COLORS.BORDER}` }}
                      >
                        {expandedCategories.has(category) ? (
                          <ChevronDown size={14} color={COLORS.MUTED} />
                        ) : (
                          <ChevronRight size={14} color={COLORS.MUTED} />
                        )}
                        <span className="text-xs font-medium" style={{ color: COLORS.ORANGE }}>
                          {category}
                        </span>
                        <span className="text-xs ml-auto" style={{ color: COLORS.MUTED }}>
                          {categoryEndpoints.length}
                        </span>
                      </button>

                      {expandedCategories.has(category) && (
                        <div className="py-1">
                          {categoryEndpoints.map(endpoint => {
                            const isActive = selectedEndpoint === endpoint;
                            const isFav = isFavorite(selectedSource.script, endpoint);

                            return (
                              <div
                                key={endpoint}
                                className="flex items-center gap-1 px-2"
                              >
                                <button
                                  onClick={() => executeQuery(selectedSource.script, endpoint)}
                                  className="flex-1 px-3 py-1.5 text-left text-xs rounded hover:bg-white/5 transition-colors truncate"
                                  style={{
                                    color: isActive ? COLORS.CYAN : COLORS.WHITE,
                                    background: isActive ? COLORS.CYAN + '10' : 'transparent'
                                  }}
                                >
                                  {endpoint}
                                </button>
                                <button
                                  onClick={() => toggleFavorite(selectedSource.script, endpoint)}
                                  className="p-1 rounded hover:bg-white/10"
                                >
                                  {isFav ? (
                                    <BookmarkCheck size={12} color={COLORS.YELLOW} />
                                  ) : (
                                    <Bookmark size={12} color={COLORS.MUTED} />
                                  )}
                                </button>
                              </div>
                            );
                          })}
                        </div>
                      )}
                    </div>
                  ))
                ) : (
                  // Show flat list if no categories
                  <div className="py-1">
                    {filteredEndpoints.map(endpoint => {
                      const isActive = selectedEndpoint === endpoint;
                      const isFav = isFavorite(selectedSource.script, endpoint);

                      return (
                        <div key={endpoint} className="flex items-center gap-1 px-2">
                          <button
                            onClick={() => executeQuery(selectedSource.script, endpoint)}
                            className="flex-1 px-3 py-1.5 text-left text-xs rounded hover:bg-white/5 transition-colors truncate"
                            style={{
                              color: isActive ? COLORS.CYAN : COLORS.WHITE,
                              background: isActive ? COLORS.CYAN + '10' : 'transparent'
                            }}
                          >
                            {endpoint}
                          </button>
                          <button
                            onClick={() => toggleFavorite(selectedSource.script, endpoint)}
                            className="p-1 rounded hover:bg-white/10"
                          >
                            {isFav ? (
                              <BookmarkCheck size={12} color={COLORS.YELLOW} />
                            ) : (
                              <Bookmark size={12} color={COLORS.MUTED} />
                            )}
                          </button>
                        </div>
                      );
                    })}
                  </div>
                )}
              </div>
            </>
          ) : (
            <div className="flex-1 flex items-center justify-center p-4">
              <div className="text-center">
                <Database size={32} color={COLORS.MUTED} className="mx-auto mb-3" />
                <div className="text-sm" style={{ color: COLORS.MUTED }}>
                  Select a data source to browse endpoints
                </div>
              </div>
            </div>
          )}
        </div>

        {/* Right Panel - Data Display */}
        <div className="flex-1 flex flex-col overflow-hidden">
          {/* Toolbar */}
          <div className="flex items-center justify-between px-4 py-2 border-b" style={{ borderColor: COLORS.BORDER }}>
            <div className="flex items-center gap-4">
              {selectedEndpoint && (
                <>
                  <span className="font-mono text-sm" style={{ color: COLORS.CYAN }}>
                    {selectedEndpoint}
                  </span>
                  {responseInfo && (
                    <span className="text-xs" style={{ color: COLORS.MUTED }}>
                      {responseInfo.count} records
                    </span>
                  )}
                </>
              )}
            </div>
            <div className="flex items-center gap-2">
              {/* View Mode Toggle */}
              <div className="flex items-center rounded overflow-hidden" style={{ border: `1px solid ${COLORS.BORDER}` }}>
                <button
                  onClick={() => setViewMode('table')}
                  className="px-2 py-1 flex items-center gap-1"
                  style={{
                    background: viewMode === 'table' ? COLORS.HOVER : 'transparent',
                    color: viewMode === 'table' ? COLORS.WHITE : COLORS.MUTED
                  }}
                >
                  <Table size={14} />
                  <span className="text-xs">Table</span>
                </button>
                <button
                  onClick={() => setViewMode('json')}
                  className="px-2 py-1 flex items-center gap-1"
                  style={{
                    background: viewMode === 'json' ? COLORS.HOVER : 'transparent',
                    color: viewMode === 'json' ? COLORS.WHITE : COLORS.MUTED
                  }}
                >
                  <Code size={14} />
                  <span className="text-xs">JSON</span>
                </button>
              </div>

              {data && (
                <>
                  <button
                    onClick={downloadCSV}
                    className="p-2 rounded hover:bg-white/10 transition-colors"
                    title="Download CSV"
                  >
                    <Download size={16} color={COLORS.MUTED} />
                  </button>
                  <button
                    onClick={() => copyToClipboard(JSON.stringify(data, null, 2))}
                    className="p-2 rounded hover:bg-white/10 transition-colors"
                    title="Copy JSON"
                  >
                    <Copy size={16} color={COLORS.MUTED} />
                  </button>
                </>
              )}

              {selectedEndpoint && selectedSource && (
                <button
                  onClick={() => executeQuery(selectedSource.script, selectedEndpoint)}
                  disabled={loading}
                  className="px-3 py-1.5 rounded flex items-center gap-2 text-sm font-medium"
                  style={{ background: COLORS.ORANGE, color: COLORS.WHITE }}
                >
                  <RefreshCw size={14} className={loading ? 'animate-spin' : ''} />
                  Refresh
                </button>
              )}
            </div>
          </div>

          {/* Data Content */}
          <div className="flex-1 overflow-auto p-4">
            {loading ? (
              <div className="flex items-center justify-center h-full">
                <div className="text-center">
                  <RefreshCw size={32} className="animate-spin mx-auto mb-3" color={COLORS.ORANGE} />
                  <div className="text-sm" style={{ color: COLORS.MUTED }}>Loading data...</div>
                </div>
              </div>
            ) : error ? (
              <div className="flex items-center justify-center h-full">
                <div className="text-center max-w-md">
                  <AlertCircle size={32} color={COLORS.RED} className="mx-auto mb-3" />
                  <div className="text-sm font-medium text-white mb-2">Query Failed</div>
                  <div className="text-xs p-3 rounded font-mono" style={{ background: COLORS.RED + '20', color: COLORS.RED }}>
                    {error}
                  </div>
                </div>
              </div>
            ) : data && data.length > 0 ? (
              viewMode === 'table' ? (
                <div className="overflow-auto">
                  <table className="w-full text-xs">
                    <thead>
                      <tr style={{ background: COLORS.HEADER_BG }}>
                        {columns.map(col => (
                          <th
                            key={col}
                            className="px-3 py-2 text-left font-medium cursor-pointer hover:bg-white/5"
                            style={{ color: COLORS.MUTED, borderBottom: `1px solid ${COLORS.BORDER}` }}
                            onClick={() => {
                              if (sortColumn === col) {
                                setSortDirection(sortDirection === 'asc' ? 'desc' : 'asc');
                              } else {
                                setSortColumn(col);
                                setSortDirection('asc');
                              }
                            }}
                          >
                            <div className="flex items-center gap-1">
                              <span className="truncate max-w-[150px]" title={col}>{col}</span>
                              {sortColumn === col && (
                                <ArrowUpDown size={12} color={COLORS.ORANGE} />
                              )}
                            </div>
                          </th>
                        ))}
                      </tr>
                    </thead>
                    <tbody>
                      {(sortedData || data).slice(0, 500).map((row, idx) => (
                        <tr key={idx} className="hover:bg-white/5" style={{ borderBottom: `1px solid ${COLORS.BORDER}` }}>
                          {columns.map(col => {
                            const value = row[col];
                            const isNumber = typeof value === 'number';
                            const isNegative = isNumber && value < 0;

                            return (
                              <td
                                key={col}
                                className="px-3 py-2"
                                style={{
                                  color: isNegative ? COLORS.RED : isNumber ? COLORS.GREEN : COLORS.WHITE
                                }}
                              >
                                <span className="truncate block max-w-[200px]" title={String(value)}>
                                  {isNumber ? value.toLocaleString() : String(value ?? '-')}
                                </span>
                              </td>
                            );
                          })}
                        </tr>
                      ))}
                    </tbody>
                  </table>
                  {data.length > 500 && (
                    <div className="text-center py-4 text-xs" style={{ color: COLORS.MUTED }}>
                      Showing first 500 of {data.length} records
                    </div>
                  )}
                </div>
              ) : (
                <pre
                  className="text-xs font-mono p-4 rounded overflow-auto"
                  style={{ background: COLORS.HEADER_BG, color: COLORS.WHITE }}
                >
                  {JSON.stringify(data.slice(0, 100), null, 2)}
                  {data.length > 100 && `\n\n... and ${data.length - 100} more records`}
                </pre>
              )
            ) : !selectedEndpoint ? (
              <div className="flex items-center justify-center h-full">
                <div className="text-center max-w-lg">
                  <Zap size={48} color={COLORS.ORANGE} className="mx-auto mb-4" />
                  <div className="text-xl font-semibold text-white mb-2">AKShare Data Explorer</div>
                  <div className="text-sm mb-6" style={{ color: COLORS.MUTED }}>
                    Access 400+ financial data endpoints covering bonds, derivatives, economics, funds, and alternative data from Chinese and global markets.
                  </div>
                  <div className="grid grid-cols-3 gap-3 text-xs">
                    {AKSHARE_DATA_SOURCES.slice(0, 6).map(source => (
                      <button
                        key={source.id}
                        onClick={() => setSelectedSource(source)}
                        className="p-3 rounded text-left hover:bg-white/5 transition-colors"
                        style={{ border: `1px solid ${COLORS.BORDER}` }}
                      >
                        <div className="font-medium text-white">{source.name}</div>
                        <div style={{ color: COLORS.MUTED }}>{source.categories.length} categories</div>
                      </button>
                    ))}
                  </div>
                </div>
              </div>
            ) : (
              <div className="flex items-center justify-center h-full">
                <div className="text-center">
                  <CheckCircle2 size={32} color={COLORS.GREEN} className="mx-auto mb-3" />
                  <div className="text-sm" style={{ color: COLORS.MUTED }}>No data returned</div>
                </div>
              </div>
            )}
          </div>
        </div>

        {/* Favorites Panel */}
        {showFavorites && (
          <div className="w-64 border-l flex flex-col" style={{ borderColor: COLORS.BORDER, background: COLORS.PANEL_BG }}>
            <div className="p-3 border-b flex items-center justify-between" style={{ borderColor: COLORS.BORDER }}>
              <span className="text-xs font-medium" style={{ color: COLORS.YELLOW }}>FAVORITES</span>
              <button onClick={() => setShowFavorites(false)}>
                <X size={14} color={COLORS.MUTED} />
              </button>
            </div>
            <div className="flex-1 overflow-y-auto">
              {favorites.length === 0 ? (
                <div className="p-4 text-center text-xs" style={{ color: COLORS.MUTED }}>
                  No favorites yet. Click the bookmark icon on any endpoint to add it here.
                </div>
              ) : (
                favorites.map((fav, idx) => (
                  <button
                    key={idx}
                    onClick={() => {
                      const source = AKSHARE_DATA_SOURCES.find(s => s.script === fav.script);
                      if (source) {
                        setSelectedSource(source);
                        executeQuery(fav.script, fav.endpoint);
                      }
                    }}
                    className="w-full px-3 py-2 text-left text-xs hover:bg-white/5 border-b"
                    style={{ borderColor: COLORS.BORDER }}
                  >
                    <div className="text-white truncate">{fav.endpoint}</div>
                    <div style={{ color: COLORS.MUTED }}>{fav.script.replace('akshare_', '').replace('.py', '')}</div>
                  </button>
                ))
              )}
            </div>
          </div>
        )}

        {/* History Panel */}
        {showHistory && (
          <div className="w-64 border-l flex flex-col" style={{ borderColor: COLORS.BORDER, background: COLORS.PANEL_BG }}>
            <div className="p-3 border-b flex items-center justify-between" style={{ borderColor: COLORS.BORDER }}>
              <span className="text-xs font-medium" style={{ color: COLORS.CYAN }}>QUERY HISTORY</span>
              <button onClick={() => setShowHistory(false)}>
                <X size={14} color={COLORS.MUTED} />
              </button>
            </div>
            <div className="flex-1 overflow-y-auto">
              {queryHistory.length === 0 ? (
                <div className="p-4 text-center text-xs" style={{ color: COLORS.MUTED }}>
                  No queries yet
                </div>
              ) : (
                queryHistory.map((item) => (
                  <button
                    key={item.id}
                    onClick={() => {
                      const source = AKSHARE_DATA_SOURCES.find(s => s.script === item.script);
                      if (source) {
                        setSelectedSource(source);
                        executeQuery(item.script, item.endpoint);
                      }
                    }}
                    className="w-full px-3 py-2 text-left text-xs hover:bg-white/5 border-b"
                    style={{ borderColor: COLORS.BORDER }}
                  >
                    <div className="flex items-center gap-2">
                      {item.success ? (
                        <CheckCircle2 size={10} color={COLORS.GREEN} />
                      ) : (
                        <AlertCircle size={10} color={COLORS.RED} />
                      )}
                      <span className="text-white truncate">{item.endpoint}</span>
                    </div>
                    <div className="flex items-center justify-between mt-1" style={{ color: COLORS.MUTED }}>
                      <span>{new Date(item.timestamp).toLocaleTimeString()}</span>
                      {item.count !== undefined && <span>{item.count} rows</span>}
                    </div>
                  </button>
                ))
              )}
            </div>
          </div>
        )}
      </div>

      <TabFooter tabName="AKShare Data" />
    </div>
  );
};

export default AkShareDataTab;
