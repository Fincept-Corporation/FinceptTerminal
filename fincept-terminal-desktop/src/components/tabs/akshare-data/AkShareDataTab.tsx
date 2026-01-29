/**
 * AKShare Data Explorer Tab
 *
 * Comprehensive interface for exploring 26 AKShare data sources with 1000+ endpoints:
 * - Bonds (Chinese & International) - 28 endpoints
 * - Derivatives (Options, Futures) - 46 endpoints
 * - China Economics (GDP, CPI, PMI, etc.) - 85 endpoints
 * - Global Economics (US, EU, UK, Japan, etc.) - 35 endpoints
 * - Funds (ETFs, Mutual Funds) - 70+ endpoints
 * - Stocks (Realtime, Historical, Financial) - 150+ endpoints
 * - Alternative Data (Air Quality, Carbon, Real Estate, etc.) - 14 endpoints
 * - Macro Global (96 endpoints) - Multi-country economic indicators
 * - Miscellaneous (129 endpoints) - AMAC, FRED, Car Sales, Movies, etc.
 */

import React, { useState, useEffect, useCallback, useMemo } from 'react';
import { useTranslation } from 'react-i18next';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { useLanguage } from '@/contexts/LanguageContext';
import { AKShareAPI, AKSHARE_DATA_SOURCES, AKShareDataSource, AKShareResponse } from '@/services/akshareApi';
import { parseAKShareResponse, isValidParsedData, getDataSummary } from '@/lib/akshareDataParser';
import {
  Search, RefreshCw, Download, ChevronRight, ChevronDown, Database,
  Landmark, LineChart, TrendingUp, Globe, PieChart, Layers, BarChart3,
  AlertCircle, CheckCircle2, Clock, Filter, X, Copy, Table, Code,
  ArrowUpDown, ExternalLink, Bookmark, BookmarkCheck, History, Zap, Building2,
  Activity, DollarSign, Newspaper, Building, Bitcoin, FileText, Users,
  ArrowRightLeft, LayoutGrid, Percent, Flame, ChevronLeft, PanelLeftClose, PanelLeftOpen
} from 'lucide-react';

// Fincept Terminal Color Palette
const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
};

// Terminal CSS styles
const TERMINAL_STYLES = `
  .akshare-scrollbar::-webkit-scrollbar { width: 6px; height: 6px; }
  .akshare-scrollbar::-webkit-scrollbar-track { background: ${FINCEPT.DARK_BG}; }
  .akshare-scrollbar::-webkit-scrollbar-thumb { background: ${FINCEPT.BORDER}; border-radius: 3px; }
  .akshare-scrollbar::-webkit-scrollbar-thumb:hover { background: ${FINCEPT.MUTED}; }
  .akshare-hover-row:hover { background: ${FINCEPT.HOVER} !important; }
`;

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
  const { currentLanguage } = useLanguage();

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
  const [translatedColumns, setTranslatedColumns] = useState<Record<string, string>>({});
  const [translatedData, setTranslatedData] = useState<any[] | null>(null);
  const [translating, setTranslating] = useState(false);

  // View state
  const [viewMode, setViewMode] = useState<'table' | 'json'>('table');
  const [sortColumn, setSortColumn] = useState<string | null>(null);
  const [sortDirection, setSortDirection] = useState<'asc' | 'desc'>('asc');

  // Pagination state
  const [currentPage, setCurrentPage] = useState(1);
  const [pageSize, setPageSize] = useState(10);
  const [translatedPages, setTranslatedPages] = useState<Map<number, any[]>>(new Map());

  // Panel collapse state
  const [isSourcesPanelCollapsed, setIsSourcesPanelCollapsed] = useState(false);
  const [isEndpointsPanelCollapsed, setIsEndpointsPanelCollapsed] = useState(false);

  // History & Favorites
  const [queryHistory, setQueryHistory] = useState<QueryHistoryItem[]>([]);
  const [favorites, setFavorites] = useState<FavoriteEndpoint[]>([]);
  const [showHistory, setShowHistory] = useState(false);
  const [showFavorites, setShowFavorites] = useState(false);

  // Query Parameters
  const [symbol, setSymbol] = useState('000001');
  const [startDate, setStartDate] = useState('');
  const [endDate, setEndDate] = useState('');
  const [period, setPeriod] = useState('daily');
  const [market, setMarket] = useState('sh');
  const [adjust, setAdjust] = useState('');
  const [showParameters, setShowParameters] = useState(false);

  // Debug: Log language on mount and changes
  useEffect(() => {
    console.log('AkShare Tab - Current Language:', currentLanguage);
  }, [currentLanguage]);

  // Translate current page when page changes or language changes
  useEffect(() => {
    const translateCurrentPage = async () => {
      if (!data || data.length === 0) return;

      const langCode = currentLanguage === 'zh' ? 'zh-CN' : (currentLanguage || 'en');
      if (langCode === 'zh-CN') return; // No translation needed for Chinese

      // Check if this page is already translated
      if (translatedPages.has(currentPage)) {
        console.log(`Page ${currentPage} already translated, using cached version`);
        return;
      }

      setTranslating(true);
      try {
        const startIdx = (currentPage - 1) * pageSize;
        const endIdx = startIdx + pageSize;
        const pageData = data.slice(startIdx, endIdx);

        console.log(`Translating page ${currentPage} (rows ${startIdx}-${endIdx})...`);
        const translatedPageData = await translatePageData(pageData, langCode);

        // Cache the translated page
        const newTranslatedPages = new Map(translatedPages);
        newTranslatedPages.set(currentPage, translatedPageData);
        setTranslatedPages(newTranslatedPages);
      } catch (err) {
        console.error('Failed to translate page:', err);
      } finally {
        setTranslating(false);
      }
    };

    translateCurrentPage();
  }, [currentPage, data, currentLanguage, pageSize, translatedPages]);

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

  // Helper function to build parameters based on endpoint name
  const buildParametersForEndpoint = (endpoint: string): string[] | undefined => {
    const params: string[] = [];

    // Detect if endpoint needs symbol/stock parameter
    const needsSymbol = endpoint.includes('stock') || endpoint.includes('holder') ||
                       endpoint.includes('fund') || endpoint.includes('esg') ||
                       endpoint.includes('comment') || endpoint.includes('individual');

    // Detect if endpoint needs date parameters
    const needsDates = endpoint.includes('hist') || endpoint.includes('historical');

    // Detect if endpoint needs period parameter
    const needsPeriod = endpoint.includes('hist') && !endpoint.includes('min');

    // Detect if endpoint needs market parameter
    const needsMarket = endpoint.includes('individual_fund_flow');

    if (needsSymbol && symbol) {
      params.push(symbol);
    }

    if (needsPeriod && period) {
      params.push(period);
    }

    if (needsDates) {
      if (startDate) params.push(startDate);
      if (endDate) params.push(endDate);
    }

    if (adjust) {
      params.push(adjust);
    }

    if (needsMarket && market) {
      params.push(market);
    }

    return params.length > 0 ? params : undefined;
  };

  const loadEndpoints = async (script: string) => {
    setLoadingEndpoints(true);
    setEndpoints([]);
    setCategories({});
    try {
      const response = await AKShareAPI.getEndpoints(script);
      console.log('Endpoints raw response:', response);

      if (response.success && response.data) {
        // Don't use the parser for endpoints - they have a special structure
        // Handle nested data structure from Python directly
        let endpointData = response.data as any;

        // Handle nested data.data structure (common from Python)
        if (endpointData.data && typeof endpointData.data === 'object') {
          console.log('Found nested data.data structure');
          endpointData = endpointData.data;
        }

        // Extract endpoints and categories
        const availableEndpoints = endpointData.available_endpoints || endpointData.endpoints || [];
        const categoriesData = endpointData.categories || {};

        console.log('Extracted endpoints:', availableEndpoints);
        console.log('Extracted categories:', categoriesData);

        setEndpoints(Array.isArray(availableEndpoints) ? availableEndpoints : []);
        setCategories(categoriesData || {});

        // Auto-expand first category
        if (Object.keys(categoriesData).length > 0) {
          const firstCategory = Object.keys(categoriesData)[0];
          if (firstCategory) {
            setExpandedCategories(new Set([firstCategory]));
          }
        }
      } else {
        console.error('Failed to load endpoints:', response.error);
      }
    } catch (err) {
      console.error('Failed to load endpoints:', err);
    } finally {
      setLoadingEndpoints(false);
    }
  };

  const translatePageData = async (pageData: any[], targetLang: string) => {
    try {
      // Collect text values that need translation from this page only
      const textsToTranslate: string[] = [];
      const textPositions: Array<{rowIdx: number, col: string}> = [];

      pageData.forEach((row, rowIdx) => {
        Object.entries(row).forEach(([col, value]) => {
          if (typeof value === 'string' && value.trim().length > 0) {
            // Check if contains Chinese/non-ASCII characters
            if (/[\u4e00-\u9fa5\u3000-\u303f]/.test(value)) {
              textsToTranslate.push(value);
              textPositions.push({rowIdx, col});
            }
          }
        });
      });

      if (textsToTranslate.length > 0) {
        console.log(`Translating ${textsToTranslate.length} text fields for current page...`);
        const contentResponse = await AKShareAPI.translateBatch(textsToTranslate, targetLang);

        if (contentResponse.success && contentResponse.translations) {
          const newData = JSON.parse(JSON.stringify(pageData)); // Deep clone
          contentResponse.translations.forEach((t: any, idx: number) => {
            const pos = textPositions[idx];
            if (pos && newData[pos.rowIdx]) {
              newData[pos.rowIdx][pos.col] = t.translated || t.original;
            }
          });
          return newData;
        }
      }

      return pageData;
    } catch (err) {
      console.error('Page translation failed:', err);
      return pageData;
    }
  };

  const translateColumnHeaders = async (columns: string[], targetLang: string) => {
    console.log(`Translating ${columns.length} column headers to ${targetLang}...`);
    const headerResponse = await AKShareAPI.translateBatch(columns, targetLang);
    console.log('Header translation response:', headerResponse);

    const translatedHeaders: Record<string, string> = {};
    if (headerResponse.success && headerResponse.translations) {
      headerResponse.translations.forEach((t: any) => {
        translatedHeaders[t.original] = t.translated;
      });
      setTranslatedColumns(translatedHeaders);
      console.log('Translated headers:', translatedHeaders);
    }
  };

  const executeQuery = async (script: string, endpoint: string, customParams?: string[]) => {
    setLoading(true);
    setError(null);
    setData(null);
    setResponseInfo(null);
    setSelectedEndpoint(endpoint);
    setTranslatedColumns({});
    setTranslatedData(null);
    setCurrentPage(1);
    setTranslatedPages(new Map());

    try {
      // Build parameters array if provided
      const params = customParams || buildParametersForEndpoint(endpoint);
      const response = await AKShareAPI.query(script, endpoint, params);

      console.log('Raw AKShare response:', response);

      // Use robust parser to handle any JSON structure
      const parsed = parseAKShareResponse(response);

      console.log('Parsed data:', parsed);

      // Check if parsing was successful
      if (!isValidParsedData(parsed)) {
        const errorMsg = parsed.warnings?.join(', ') || 'Failed to parse response data';
        setError(errorMsg);

        // Still add to history but mark as failed
        const historyItem: QueryHistoryItem = {
          id: `${Date.now()}`,
          script,
          endpoint,
          timestamp: Date.now(),
          success: false,
          count: 0
        };
        const newHistory = [historyItem, ...queryHistory.slice(0, 49)];
        setQueryHistory(newHistory);
        localStorage.setItem('akshare_history', JSON.stringify(newHistory));

        setLoading(false);
        return;
      }

      // Add to history
      const historyItem: QueryHistoryItem = {
        id: `${Date.now()}`,
        script,
        endpoint,
        timestamp: Date.now(),
        success: true,
        count: parsed.count
      };
      const newHistory = [historyItem, ...queryHistory.slice(0, 49)];
      setQueryHistory(newHistory);
      localStorage.setItem('akshare_history', JSON.stringify(newHistory));

      // Set parsed data
      setData(parsed.data);
      setResponseInfo({
        count: parsed.count,
        timestamp: parsed.metadata?.timestamp || Date.now(),
        source: parsed.metadata?.source
      });

      // Show warnings if any
      if (parsed.warnings && parsed.warnings.length > 0) {
        console.warn('Data parsing warnings:', parsed.warnings);
      }

      // TRANSLATION LAYER: Only translate column headers initially
      if (parsed.data.length > 0) {
        const langCode = currentLanguage === 'zh' ? 'zh-CN' : (currentLanguage || 'en');
        console.log(`Current language: ${currentLanguage}, Target translation: ${langCode}`);

        // If English or any non-Chinese language, translate column headers
        if (langCode !== 'zh-CN') {
          await translateColumnHeaders(parsed.columns, langCode);
        } else {
          console.log('Language is Chinese, skipping translation');
        }
      }
    } catch (err: any) {
      console.error('Query execution error:', err);
      setError(err.message || 'Query failed');

      // Add failed query to history
      const historyItem: QueryHistoryItem = {
        id: `${Date.now()}`,
        script,
        endpoint,
        timestamp: Date.now(),
        success: false,
        count: 0
      };
      const newHistory = [historyItem, ...queryHistory.slice(0, 49)];
      setQueryHistory(newHistory);
      localStorage.setItem('akshare_history', JSON.stringify(newHistory));
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

  // Get paginated data with translation
  const paginatedData = useMemo(() => {
    if (!data) return null;

    const startIdx = (currentPage - 1) * pageSize;
    const endIdx = startIdx + pageSize;

    // Check if we have translated version of current page
    const translatedPage = translatedPages.get(currentPage);
    if (translatedPage) {
      return translatedPage;
    }

    // Return original data for current page
    return data.slice(startIdx, endIdx);
  }, [data, currentPage, pageSize, translatedPages]);

  // Calculate total pages
  const totalPages = useMemo(() => {
    if (!data) return 0;
    return Math.ceil(data.length / pageSize);
  }, [data, pageSize]);

  // Sort data
  const sortedData = useMemo(() => {
    if (!paginatedData || !sortColumn) return paginatedData;
    return [...paginatedData].sort((a, b) => {
      const aVal = a[sortColumn];
      const bVal = b[sortColumn];
      if (aVal === bVal) return 0;
      if (aVal === null || aVal === undefined) return 1;
      if (bVal === null || bVal === undefined) return -1;
      const comparison = aVal < bVal ? -1 : 1;
      return sortDirection === 'asc' ? comparison : -comparison;
    });
  }, [paginatedData, sortColumn, sortDirection]);

  // Get table columns
  const columns = useMemo(() => {
    if (!paginatedData || paginatedData.length === 0) return [];
    return Object.keys(paginatedData[0]);
  }, [paginatedData]);

  const copyToClipboard = (text: string) => {
    navigator.clipboard.writeText(text);
  };

  const downloadCSV = () => {
    // Download all data (original, not just current page)
    if (!data || data.length === 0) return;

    const allColumns = Object.keys(data[0]);

    // Use translated column names if available
    const headerNames = allColumns.map(col => translatedColumns[col] || col);
    const headers = headerNames.join(',');

    const rows = data.map(row =>
      allColumns.map(col => {
        const val = row[col];
        if (typeof val === 'string' && (val.includes(',') || val.includes('"'))) {
          return `"${val.replace(/"/g, '""')}"`;
        }
        return val;
      }).join(',')
    );

    const csv = [headers, ...rows].join('\n');
    const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `akshare_${selectedEndpoint}_${Date.now()}.csv`;
    a.click();
    URL.revokeObjectURL(url);
  };

  const downloadJSON = () => {
    // Download all data (original, not just current page)
    if (!data) return;
    const json = JSON.stringify(data, null, 2);
    const blob = new Blob([json], { type: 'application/json;charset=utf-8;' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `akshare_${selectedEndpoint}_${Date.now()}.json`;
    a.click();
    URL.revokeObjectURL(url);
  };

  return (
    <div style={{
      display: 'flex',
      flexDirection: 'column',
      height: '100%',
      backgroundColor: FINCEPT.DARK_BG,
      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
    }}>
      {/* Inject terminal scrollbar styles */}
      <style>{TERMINAL_STYLES}</style>

      {/* Top Navigation Bar */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        padding: '8px 16px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `2px solid ${FINCEPT.ORANGE}`,
        boxShadow: `0 2px 8px ${FINCEPT.ORANGE}20`,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <Database size={20} color={FINCEPT.ORANGE} />
          <span style={{ fontWeight: 700, color: FINCEPT.WHITE, fontSize: '12px', letterSpacing: '0.5px' }}>
            AKSHARE DATA EXPLORER
          </span>
          <span style={{
            padding: '2px 8px',
            backgroundColor: `${FINCEPT.ORANGE}20`,
            color: FINCEPT.ORANGE,
            fontSize: '9px',
            fontWeight: 700,
            borderRadius: '2px',
          }}>
            400+ ENDPOINTS
          </span>
          {/* Status indicators */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginLeft: '16px' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
              <div style={{ width: '6px', height: '6px', borderRadius: '50%', backgroundColor: selectedSource ? FINCEPT.GREEN : FINCEPT.MUTED }} />
              <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>SOURCE</span>
            </div>
            <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
              <div style={{ width: '6px', height: '6px', borderRadius: '50%', backgroundColor: data ? FINCEPT.GREEN : FINCEPT.MUTED }} />
              <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>DATA</span>
            </div>
          </div>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <button
            onClick={() => setShowFavorites(!showFavorites)}
            style={{
              padding: '6px',
              borderRadius: '2px',
              backgroundColor: showFavorites ? `${FINCEPT.YELLOW}15` : 'transparent',
              border: 'none',
              cursor: 'pointer',
              transition: 'all 0.2s',
            }}
            title="Favorites"
          >
            {showFavorites ? <BookmarkCheck size={16} color={FINCEPT.YELLOW} /> : <Bookmark size={16} color={FINCEPT.MUTED} />}
          </button>
          <button
            onClick={() => setShowHistory(!showHistory)}
            style={{
              padding: '6px',
              borderRadius: '2px',
              backgroundColor: showHistory ? `${FINCEPT.CYAN}15` : 'transparent',
              border: 'none',
              cursor: 'pointer',
              transition: 'all 0.2s',
            }}
            title="History"
          >
            <History size={16} color={showHistory ? FINCEPT.CYAN : FINCEPT.MUTED} />
          </button>
        </div>
      </div>

      {/* Main Content Area */}
      <div style={{ display: 'flex', flex: 1, overflow: 'hidden' }}>
        {/* Left Panel - Data Sources (280px fixed) */}
        {!isSourcesPanelCollapsed && (
          <div style={{
            width: '280px',
            borderRight: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            flexDirection: 'column',
            backgroundColor: FINCEPT.PANEL_BG,
          }}>
            {/* Section Header */}
            <div style={{
              padding: '12px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
            }}>
              <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
                DATA SOURCES
              </span>
              <button
                onClick={() => setIsSourcesPanelCollapsed(true)}
                style={{
                  padding: '4px',
                  borderRadius: '2px',
                  backgroundColor: 'transparent',
                  border: 'none',
                  cursor: 'pointer',
                }}
                title="Collapse panel"
              >
                <ChevronLeft size={14} color={FINCEPT.MUTED} />
              </button>
            </div>
            {/* Source List */}
            <div className="akshare-scrollbar" style={{ flex: 1, overflowY: 'auto' }}>
              {AKSHARE_DATA_SOURCES.map(source => {
                const IconComponent = IconMap[source.icon] || Database;
                const isSelected = selectedSource?.id === source.id;

                return (
                  <button
                    key={source.id}
                    onClick={() => setSelectedSource(source)}
                    style={{
                      width: '100%',
                      padding: '10px 12px',
                      display: 'flex',
                      alignItems: 'flex-start',
                      gap: '10px',
                      textAlign: 'left',
                      backgroundColor: isSelected ? `${FINCEPT.ORANGE}15` : 'transparent',
                      borderLeft: isSelected ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                      borderTop: 'none',
                      borderRight: 'none',
                      borderBottom: `1px solid ${FINCEPT.BORDER}`,
                      cursor: 'pointer',
                      transition: 'all 0.2s',
                    }}
                  >
                    <IconComponent size={16} color={isSelected ? FINCEPT.ORANGE : source.color} />
                    <div style={{ flex: 1, minWidth: 0 }}>
                      <div style={{
                        fontWeight: 600,
                        color: isSelected ? FINCEPT.WHITE : FINCEPT.WHITE,
                        fontSize: '10px',
                      }}>{source.name}</div>
                      <div style={{
                        fontSize: '9px',
                        marginTop: '2px',
                        color: FINCEPT.MUTED,
                        overflow: 'hidden',
                        textOverflow: 'ellipsis',
                        display: '-webkit-box',
                        WebkitLineClamp: 2,
                        WebkitBoxOrient: 'vertical' as any,
                      }}>
                        {source.description}
                      </div>
                    </div>
                  </button>
                );
              })}
            </div>
          </div>
        )}

        {/* Collapsed Sources Panel Button */}
        {isSourcesPanelCollapsed && (
          <div style={{
            width: '36px',
            borderRight: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            flexDirection: 'column',
            backgroundColor: FINCEPT.PANEL_BG,
          }}>
            <button
              onClick={() => setIsSourcesPanelCollapsed(false)}
              style={{
                padding: '10px',
                backgroundColor: 'transparent',
                border: 'none',
                cursor: 'pointer',
              }}
              title="Expand Data Sources"
            >
              <ChevronRight size={14} color={FINCEPT.ORANGE} />
            </button>
          </div>
        )}

        {/* Middle Panel - Endpoints (300px fixed) */}
        {!isEndpointsPanelCollapsed && (
          <div style={{
            width: '300px',
            borderRight: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            flexDirection: 'column',
            backgroundColor: FINCEPT.PANEL_BG,
          }}>
            {selectedSource ? (
              <>
                {/* Section Header with Search */}
                <div style={{
                  padding: '12px',
                  backgroundColor: FINCEPT.HEADER_BG,
                  borderBottom: `1px solid ${FINCEPT.BORDER}`,
                }}>
                  <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '8px' }}>
                    <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
                      ENDPOINTS
                    </span>
                    <button
                      onClick={() => setIsEndpointsPanelCollapsed(true)}
                      style={{
                        padding: '4px',
                        borderRadius: '2px',
                        backgroundColor: 'transparent',
                        border: 'none',
                        cursor: 'pointer',
                      }}
                      title="Collapse panel"
                    >
                      <ChevronLeft size={14} color={FINCEPT.MUTED} />
                    </button>
                  </div>
                  <div style={{ position: 'relative' }}>
                    <Search size={14} style={{ position: 'absolute', left: '10px', top: '50%', transform: 'translateY(-50%)' }} color={FINCEPT.MUTED} />
                    <input
                      type="text"
                      value={searchQuery}
                      onChange={e => setSearchQuery(e.target.value)}
                      placeholder="Search endpoints..."
                      style={{
                        width: '100%',
                        padding: '8px 10px 8px 32px',
                        backgroundColor: FINCEPT.DARK_BG,
                        border: `1px solid ${FINCEPT.BORDER}`,
                        borderRadius: '2px',
                        color: FINCEPT.WHITE,
                        fontSize: '10px',
                        fontFamily: '"IBM Plex Mono", monospace',
                      }}
                    />
                    {searchQuery && (
                      <button
                        onClick={() => setSearchQuery('')}
                        style={{
                          position: 'absolute',
                          right: '8px',
                          top: '50%',
                          transform: 'translateY(-50%)',
                          background: 'none',
                          border: 'none',
                          cursor: 'pointer',
                        }}
                      >
                        <X size={12} color={FINCEPT.MUTED} />
                      </button>
                    )}
                  </div>
                </div>
                {/* Endpoint Count */}
                <div style={{ padding: '8px 12px', fontSize: '9px', color: FINCEPT.MUTED, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                  {filteredEndpoints.length} endpoints available
                </div>

                {/* Endpoint List */}
                <div className="akshare-scrollbar" style={{ flex: 1, overflowY: 'auto' }}>
                  {loadingEndpoints ? (
                    <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', padding: '32px' }}>
                      <RefreshCw size={18} className="animate-spin" color={FINCEPT.ORANGE} />
                    </div>
                  ) : Object.keys(filteredCategories).length > 0 ? (
                    Object.entries(filteredCategories).map(([category, categoryEndpoints]) => (
                      <div key={category}>
                        <button
                          onClick={() => toggleCategory(category)}
                          style={{
                            width: '100%',
                            padding: '8px 12px',
                            display: 'flex',
                            alignItems: 'center',
                            gap: '8px',
                            textAlign: 'left',
                            backgroundColor: FINCEPT.HEADER_BG,
                            border: 'none',
                            borderBottom: `1px solid ${FINCEPT.BORDER}`,
                            cursor: 'pointer',
                          }}
                        >
                          {expandedCategories.has(category) ? (
                            <ChevronDown size={12} color={FINCEPT.MUTED} />
                          ) : (
                            <ChevronRight size={12} color={FINCEPT.MUTED} />
                          )}
                          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.ORANGE, letterSpacing: '0.5px' }}>
                            {category.toUpperCase()}
                          </span>
                          <span style={{ fontSize: '9px', marginLeft: 'auto', color: FINCEPT.MUTED }}>
                            {categoryEndpoints.length}
                          </span>
                        </button>

                        {expandedCategories.has(category) && (
                          <div style={{ padding: '4px 0' }}>
                            {categoryEndpoints.map(endpoint => {
                              const isActive = selectedEndpoint === endpoint;
                              const isFav = isFavorite(selectedSource.script, endpoint);

                              return (
                                <div
                                  key={endpoint}
                                  style={{
                                    display: 'flex',
                                    alignItems: 'center',
                                    gap: '4px',
                                    padding: '0 8px',
                                  }}
                                >
                                  <button
                                    onClick={() => executeQuery(selectedSource.script, endpoint)}
                                    style={{
                                      flex: 1,
                                      padding: '6px 10px',
                                      textAlign: 'left',
                                      fontSize: '10px',
                                      backgroundColor: isActive ? `${FINCEPT.ORANGE}15` : 'transparent',
                                      borderLeft: isActive ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                                      color: isActive ? FINCEPT.CYAN : FINCEPT.WHITE,
                                      borderTop: 'none',
                                      borderRight: 'none',
                                      borderBottom: 'none',
                                      borderRadius: '2px',
                                      cursor: 'pointer',
                                      transition: 'all 0.2s',
                                      overflow: 'hidden',
                                      textOverflow: 'ellipsis',
                                      whiteSpace: 'nowrap',
                                    }}
                                  >
                                    {endpoint}
                                  </button>
                                  <button
                                    onClick={() => toggleFavorite(selectedSource.script, endpoint)}
                                    style={{
                                      padding: '4px',
                                      backgroundColor: 'transparent',
                                      border: 'none',
                                      borderRadius: '2px',
                                      cursor: 'pointer',
                                    }}
                                  >
                                    {isFav ? (
                                      <BookmarkCheck size={12} color={FINCEPT.YELLOW} />
                                    ) : (
                                      <Bookmark size={12} color={FINCEPT.MUTED} />
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
                    <div style={{ padding: '4px 0' }}>
                      {filteredEndpoints.map(endpoint => {
                        const isActive = selectedEndpoint === endpoint;
                        const isFav = isFavorite(selectedSource.script, endpoint);

                        return (
                          <div key={endpoint} style={{ display: 'flex', alignItems: 'center', gap: '4px', padding: '0 8px' }}>
                            <button
                              onClick={() => executeQuery(selectedSource.script, endpoint)}
                              style={{
                                flex: 1,
                                padding: '6px 10px',
                                textAlign: 'left',
                                fontSize: '10px',
                                backgroundColor: isActive ? `${FINCEPT.ORANGE}15` : 'transparent',
                                borderLeft: isActive ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                                color: isActive ? FINCEPT.CYAN : FINCEPT.WHITE,
                                borderTop: 'none',
                                borderRight: 'none',
                                borderBottom: 'none',
                                borderRadius: '2px',
                                cursor: 'pointer',
                                transition: 'all 0.2s',
                                overflow: 'hidden',
                                textOverflow: 'ellipsis',
                                whiteSpace: 'nowrap',
                              }}
                            >
                              {endpoint}
                            </button>
                            <button
                              onClick={() => toggleFavorite(selectedSource.script, endpoint)}
                              style={{
                                padding: '4px',
                                backgroundColor: 'transparent',
                                border: 'none',
                                borderRadius: '2px',
                                cursor: 'pointer',
                              }}
                            >
                              {isFav ? (
                                <BookmarkCheck size={12} color={FINCEPT.YELLOW} />
                              ) : (
                                <Bookmark size={12} color={FINCEPT.MUTED} />
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
              <div style={{
                flex: 1,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                padding: '16px',
              }}>
                <div style={{ textAlign: 'center' }}>
                  <Database size={24} color={FINCEPT.MUTED} style={{ marginBottom: '12px' }} />
                  <div style={{ fontSize: '10px', color: FINCEPT.MUTED }}>
                    Select a data source to browse endpoints
                  </div>
                </div>
              </div>
            )}
          </div>
        )}

        {/* Collapsed Endpoints Panel Button */}
        {isEndpointsPanelCollapsed && (
          <div style={{
            width: '36px',
            borderRight: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            flexDirection: 'column',
            backgroundColor: FINCEPT.PANEL_BG,
          }}>
            <button
              onClick={() => setIsEndpointsPanelCollapsed(false)}
              style={{
                padding: '10px',
                backgroundColor: 'transparent',
                border: 'none',
                cursor: 'pointer',
              }}
              title="Expand Endpoints"
            >
              <ChevronRight size={14} color={FINCEPT.ORANGE} />
            </button>
          </div>
        )}

        {/* Right Panel - Data Display (flex: 1) */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
          {/* Toolbar */}
          <div style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            padding: '8px 16px',
            backgroundColor: FINCEPT.HEADER_BG,
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
          }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
              {/* Panel Toggle Buttons */}
              <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                <button
                  onClick={() => setIsSourcesPanelCollapsed(!isSourcesPanelCollapsed)}
                  style={{
                    padding: '6px',
                    borderRadius: '2px',
                    backgroundColor: 'transparent',
                    border: 'none',
                    cursor: 'pointer',
                  }}
                  title={isSourcesPanelCollapsed ? "Show Data Sources" : "Hide Data Sources"}
                >
                  {isSourcesPanelCollapsed ? (
                    <PanelLeftOpen size={14} color={FINCEPT.ORANGE} />
                  ) : (
                    <PanelLeftClose size={14} color={FINCEPT.MUTED} />
                  )}
                </button>
                <button
                  onClick={() => setIsEndpointsPanelCollapsed(!isEndpointsPanelCollapsed)}
                  style={{
                    padding: '6px',
                    borderRadius: '2px',
                    backgroundColor: 'transparent',
                    border: 'none',
                    cursor: 'pointer',
                  }}
                  title={isEndpointsPanelCollapsed ? "Show Endpoints" : "Hide Endpoints"}
                >
                  {isEndpointsPanelCollapsed ? (
                    <PanelLeftOpen size={14} color={FINCEPT.ORANGE} />
                  ) : (
                    <PanelLeftClose size={14} color={FINCEPT.MUTED} />
                  )}
                </button>
              </div>

              {/* Separator */}
              {selectedEndpoint && (
                <div style={{ width: '1px', height: '20px', backgroundColor: FINCEPT.BORDER }} />
              )}

              {selectedEndpoint && (
                <>
                  <span style={{ fontFamily: '"IBM Plex Mono", monospace', fontSize: '10px', color: FINCEPT.CYAN }}>
                    {selectedEndpoint}
                  </span>
                  {responseInfo && (
                    <span style={{
                      fontSize: '9px',
                      padding: '2px 6px',
                      backgroundColor: `${FINCEPT.GREEN}20`,
                      color: FINCEPT.GREEN,
                      borderRadius: '2px',
                    }}>
                      {responseInfo.count} RECORDS
                    </span>
                  )}
                </>
              )}
            </div>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              {/* View Mode Toggle */}
              <div style={{ display: 'flex', alignItems: 'center', border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', overflow: 'hidden' }}>
                <button
                  onClick={() => setViewMode('table')}
                  style={{
                    padding: '6px 10px',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '4px',
                    backgroundColor: viewMode === 'table' ? FINCEPT.ORANGE : 'transparent',
                    color: viewMode === 'table' ? FINCEPT.DARK_BG : FINCEPT.MUTED,
                    border: 'none',
                    fontSize: '9px',
                    fontWeight: 700,
                    cursor: 'pointer',
                  }}
                >
                  <Table size={12} />
                  TABLE
                </button>
                <button
                  onClick={() => setViewMode('json')}
                  style={{
                    padding: '6px 10px',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '4px',
                    backgroundColor: viewMode === 'json' ? FINCEPT.ORANGE : 'transparent',
                    color: viewMode === 'json' ? FINCEPT.DARK_BG : FINCEPT.MUTED,
                    border: 'none',
                    fontSize: '9px',
                    fontWeight: 700,
                    cursor: 'pointer',
                  }}
                >
                  <Code size={12} />
                  JSON
                </button>
              </div>

              {data && (
                <>
                  <button
                    onClick={downloadCSV}
                    style={{
                      padding: '6px',
                      borderRadius: '2px',
                      backgroundColor: 'transparent',
                      border: `1px solid ${FINCEPT.BORDER}`,
                      cursor: 'pointer',
                    }}
                    title="Download CSV"
                  >
                    <Download size={14} color={FINCEPT.MUTED} />
                  </button>
                  <button
                    onClick={() => copyToClipboard(JSON.stringify(data, null, 2))}
                    style={{
                      padding: '6px',
                      borderRadius: '2px',
                      backgroundColor: 'transparent',
                      border: `1px solid ${FINCEPT.BORDER}`,
                      cursor: 'pointer',
                    }}
                    title="Copy JSON (All Data)"
                  >
                    <Copy size={14} color={FINCEPT.MUTED} />
                  </button>
                </>
              )}

              {/* Parameters Toggle Button */}
              {selectedEndpoint && selectedSource && (
                <button
                  onClick={() => setShowParameters(!showParameters)}
                  style={{
                    padding: '6px 12px',
                    borderRadius: '2px',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '4px',
                    backgroundColor: showParameters ? FINCEPT.PURPLE : 'transparent',
                    color: showParameters ? FINCEPT.WHITE : FINCEPT.MUTED,
                    border: `1px solid ${showParameters ? FINCEPT.PURPLE : FINCEPT.BORDER}`,
                    fontSize: '9px',
                    fontWeight: 700,
                    cursor: 'pointer',
                  }}
                  title="Toggle Parameters"
                >
                  <Filter size={12} />
                  PARAMS
                </button>
              )}

              {selectedEndpoint && selectedSource && (
                <button
                  onClick={() => executeQuery(selectedSource.script, selectedEndpoint)}
                  disabled={loading}
                  style={{
                    padding: '8px 16px',
                    borderRadius: '2px',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '6px',
                    backgroundColor: FINCEPT.ORANGE,
                    color: FINCEPT.DARK_BG,
                    border: 'none',
                    fontSize: '9px',
                    fontWeight: 700,
                    cursor: loading ? 'not-allowed' : 'pointer',
                    opacity: loading ? 0.7 : 1,
                  }}
                >
                  <RefreshCw size={12} className={loading ? 'animate-spin' : ''} />
                  REFRESH
                </button>
              )}
            </div>
          </div>

          {/* Parameters Panel */}
          {showParameters && selectedEndpoint && (
            <div style={{
              backgroundColor: FINCEPT.PANEL_BG,
              borderTop: `1px solid ${FINCEPT.BORDER}`,
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              padding: '16px',
            }}>
              <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.ORANGE, marginBottom: '12px', letterSpacing: '0.5px' }}>
                QUERY PARAMETERS
              </div>
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(180px, 1fr))', gap: '12px' }}>
                {/* Symbol/Stock Input */}
                <div>
                  <label style={{ display: 'block', fontSize: '9px', color: FINCEPT.MUTED, marginBottom: '6px', fontWeight: 600 }}>
                    SYMBOL / STOCK
                  </label>
                  <input
                    type="text"
                    value={symbol}
                    onChange={(e) => setSymbol(e.target.value)}
                    placeholder="e.g., 000001, 600000"
                    style={{
                      width: '100%',
                      padding: '8px 10px',
                      backgroundColor: FINCEPT.DARK_BG,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      borderRadius: '2px',
                      color: FINCEPT.WHITE,
                      fontSize: '10px',
                      fontFamily: '"IBM Plex Mono", monospace',
                      outline: 'none',
                    }}
                  />
                </div>

                {/* Period Selector */}
                <div>
                  <label style={{ display: 'block', fontSize: '9px', color: FINCEPT.MUTED, marginBottom: '6px', fontWeight: 600 }}>
                    PERIOD
                  </label>
                  <select
                    value={period}
                    onChange={(e) => setPeriod(e.target.value)}
                    style={{
                      width: '100%',
                      padding: '8px 10px',
                      backgroundColor: FINCEPT.DARK_BG,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      borderRadius: '2px',
                      color: FINCEPT.WHITE,
                      fontSize: '10px',
                      fontFamily: '"IBM Plex Mono", monospace',
                      outline: 'none',
                      cursor: 'pointer',
                    }}
                  >
                    <option value="daily">Daily</option>
                    <option value="weekly">Weekly</option>
                    <option value="monthly">Monthly</option>
                  </select>
                </div>

                {/* Market Selector */}
                <div>
                  <label style={{ display: 'block', fontSize: '9px', color: FINCEPT.MUTED, marginBottom: '6px', fontWeight: 600 }}>
                    MARKET
                  </label>
                  <select
                    value={market}
                    onChange={(e) => setMarket(e.target.value)}
                    style={{
                      width: '100%',
                      padding: '8px 10px',
                      backgroundColor: FINCEPT.DARK_BG,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      borderRadius: '2px',
                      color: FINCEPT.WHITE,
                      fontSize: '10px',
                      fontFamily: '"IBM Plex Mono", monospace',
                      outline: 'none',
                      cursor: 'pointer',
                    }}
                  >
                    <option value="sh">Shanghai (sh)</option>
                    <option value="sz">Shenzhen (sz)</option>
                  </select>
                </div>

                {/* Adjust Type */}
                <div>
                  <label style={{ display: 'block', fontSize: '9px', color: FINCEPT.MUTED, marginBottom: '6px', fontWeight: 600 }}>
                    ADJUST
                  </label>
                  <select
                    value={adjust}
                    onChange={(e) => setAdjust(e.target.value)}
                    style={{
                      width: '100%',
                      padding: '8px 10px',
                      backgroundColor: FINCEPT.DARK_BG,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      borderRadius: '2px',
                      color: FINCEPT.WHITE,
                      fontSize: '10px',
                      fontFamily: '"IBM Plex Mono", monospace',
                      outline: 'none',
                      cursor: 'pointer',
                    }}
                  >
                    <option value="">No Adjustment</option>
                    <option value="qfq">Forward (qfq)</option>
                    <option value="hfq">Backward (hfq)</option>
                  </select>
                </div>

                {/* Start Date */}
                <div>
                  <label style={{ display: 'block', fontSize: '9px', color: FINCEPT.MUTED, marginBottom: '6px', fontWeight: 600 }}>
                    START DATE
                  </label>
                  <input
                    type="date"
                    value={startDate}
                    onChange={(e) => setStartDate(e.target.value)}
                    style={{
                      width: '100%',
                      padding: '8px 10px',
                      backgroundColor: FINCEPT.DARK_BG,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      borderRadius: '2px',
                      color: FINCEPT.WHITE,
                      fontSize: '10px',
                      fontFamily: '"IBM Plex Mono", monospace',
                      outline: 'none',
                    }}
                  />
                </div>

                {/* End Date */}
                <div>
                  <label style={{ display: 'block', fontSize: '9px', color: FINCEPT.MUTED, marginBottom: '6px', fontWeight: 600 }}>
                    END DATE
                  </label>
                  <input
                    type="date"
                    value={endDate}
                    onChange={(e) => setEndDate(e.target.value)}
                    style={{
                      width: '100%',
                      padding: '8px 10px',
                      backgroundColor: FINCEPT.DARK_BG,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      borderRadius: '2px',
                      color: FINCEPT.WHITE,
                      fontSize: '10px',
                      fontFamily: '"IBM Plex Mono", monospace',
                      outline: 'none',
                    }}
                  />
                </div>
              </div>
              <div style={{ marginTop: '12px', fontSize: '9px', color: FINCEPT.MUTED, fontStyle: 'italic' }}>
                ℹ️ Parameters are automatically selected based on endpoint. Not all parameters are used for every query.
              </div>
            </div>
          )}

          {/* Data Content */}
          <div className="akshare-scrollbar" style={{ flex: 1, overflowX: 'auto', overflowY: 'auto', padding: '16px' }}>
            {loading || translating ? (
              <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: '100%' }}>
                <div style={{ textAlign: 'center' }}>
                  <RefreshCw size={28} className="animate-spin" color={FINCEPT.ORANGE} style={{ marginBottom: '12px' }} />
                  <div style={{ fontSize: '10px', color: FINCEPT.MUTED }}>
                    {translating ? 'Translating data...' : 'Loading data...'}
                  </div>
                </div>
              </div>
            ) : error ? (
              <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: '100%' }}>
                <div style={{ textAlign: 'center', maxWidth: '600px', padding: '20px' }}>
                  <AlertCircle size={28} color={FINCEPT.RED} style={{ marginBottom: '12px' }} />
                  <div style={{ fontSize: '11px', fontWeight: 600, color: FINCEPT.WHITE, marginBottom: '8px' }}>Query Failed</div>
                  <div style={{
                    fontSize: '10px',
                    padding: '12px',
                    borderRadius: '2px',
                    fontFamily: '"IBM Plex Mono", monospace',
                    backgroundColor: `${FINCEPT.RED}20`,
                    color: FINCEPT.RED,
                    marginBottom: '12px',
                    textAlign: 'left',
                    maxHeight: '200px',
                    overflow: 'auto',
                  }}>
                    {error}
                  </div>
                  <div style={{ fontSize: '9px', color: FINCEPT.MUTED, marginTop: '12px' }}>
                    <div style={{ marginBottom: '8px' }}>Common issues:</div>
                    <div style={{ textAlign: 'left', lineHeight: '1.6' }}>
                      • The endpoint may require additional parameters<br/>
                      • The data source might be temporarily unavailable<br/>
                      • The response format may have changed<br/>
                      • Network connectivity issues
                    </div>
                  </div>
                  {selectedEndpoint && selectedSource && (
                    <button
                      onClick={() => executeQuery(selectedSource.script, selectedEndpoint)}
                      style={{
                        marginTop: '16px',
                        padding: '8px 16px',
                        borderRadius: '2px',
                        backgroundColor: FINCEPT.ORANGE,
                        color: FINCEPT.DARK_BG,
                        border: 'none',
                        fontSize: '9px',
                        fontWeight: 700,
                        cursor: 'pointer',
                      }}
                    >
                      TRY AGAIN
                    </button>
                  )}
                </div>
              </div>
            ) : data && data.length > 0 ? (
              viewMode === 'table' ? (
                <div style={{ width: '100%', height: '100%', overflow: 'auto' }}>
                  <table style={{ minWidth: '100%', fontSize: '10px', borderCollapse: 'collapse' }}>
                    <thead style={{ position: 'sticky', top: 0, zIndex: 10 }}>
                      <tr style={{ backgroundColor: FINCEPT.HEADER_BG }}>
                        {columns.map(col => (
                          <th
                            key={col}
                            onClick={() => {
                              if (sortColumn === col) {
                                setSortDirection(sortDirection === 'asc' ? 'desc' : 'asc');
                              } else {
                                setSortColumn(col);
                                setSortDirection('asc');
                              }
                            }}
                            style={{
                              padding: '10px 16px',
                              textAlign: 'left',
                              fontWeight: 700,
                              fontSize: '9px',
                              letterSpacing: '0.5px',
                              color: FINCEPT.GRAY,
                              borderBottom: `1px solid ${FINCEPT.BORDER}`,
                              minWidth: '120px',
                              whiteSpace: 'nowrap',
                              cursor: 'pointer',
                            }}
                          >
                            <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                              <span title={col}>
                                {(translatedColumns[col] || col).toUpperCase()}
                              </span>
                              {sortColumn === col && (
                                <ArrowUpDown size={10} color={FINCEPT.ORANGE} />
                              )}
                            </div>
                          </th>
                        ))}
                      </tr>
                    </thead>
                    <tbody>
                      {(sortedData || []).map((row, idx) => (
                        <tr
                          key={idx}
                          className="akshare-hover-row"
                          style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}
                        >
                          {columns.map(col => {
                            const value = row[col];
                            const isNumber = typeof value === 'number';
                            const isNegative = isNumber && value < 0;

                            return (
                              <td
                                key={col}
                                style={{
                                  padding: '8px 16px',
                                  whiteSpace: 'normal',
                                  color: isNegative ? FINCEPT.RED : isNumber ? FINCEPT.CYAN : FINCEPT.WHITE,
                                  maxWidth: '400px',
                                  fontSize: '10px',
                                }}
                              >
                                <div style={{ wordBreak: 'break-word' }} title={String(value)}>
                                  {isNumber ? value.toLocaleString() : String(value ?? '-')}
                                </div>
                              </td>
                            );
                          })}
                        </tr>
                      ))}
                    </tbody>
                  </table>

                  {/* Pagination Controls */}
                  {data && data.length > 0 && (
                    <div style={{
                      position: 'sticky',
                      bottom: 0,
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'space-between',
                      padding: '10px 16px',
                      borderTop: `1px solid ${FINCEPT.BORDER}`,
                      backgroundColor: FINCEPT.HEADER_BG,
                    }}>
                      <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
                        <span style={{ fontSize: '9px', color: FINCEPT.MUTED }}>
                          Showing {((currentPage - 1) * pageSize) + 1} to {Math.min(currentPage * pageSize, data.length)} of {data.length} rows
                        </span>
                        <select
                          value={pageSize}
                          onChange={(e) => {
                            setPageSize(Number(e.target.value));
                            setCurrentPage(1);
                            setTranslatedPages(new Map());
                          }}
                          style={{
                            padding: '4px 8px',
                            borderRadius: '2px',
                            fontSize: '9px',
                            backgroundColor: FINCEPT.DARK_BG,
                            border: `1px solid ${FINCEPT.BORDER}`,
                            color: FINCEPT.WHITE,
                          }}
                        >
                          <option value={10}>10 rows</option>
                          <option value={25}>25 rows</option>
                          <option value={50}>50 rows</option>
                          <option value={100}>100 rows</option>
                        </select>
                      </div>

                      <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                        <button
                          onClick={() => setCurrentPage(1)}
                          disabled={currentPage === 1}
                          style={{
                            padding: '4px 10px',
                            borderRadius: '2px',
                            fontSize: '9px',
                            fontWeight: 700,
                            backgroundColor: currentPage === 1 ? FINCEPT.MUTED : 'transparent',
                            color: currentPage === 1 ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                            border: `1px solid ${currentPage === 1 ? FINCEPT.MUTED : FINCEPT.BORDER}`,
                            opacity: currentPage === 1 ? 0.5 : 1,
                            cursor: currentPage === 1 ? 'not-allowed' : 'pointer',
                          }}
                        >
                          FIRST
                        </button>
                        <button
                          onClick={() => setCurrentPage(currentPage - 1)}
                          disabled={currentPage === 1}
                          style={{
                            padding: '4px 10px',
                            borderRadius: '2px',
                            fontSize: '9px',
                            fontWeight: 700,
                            backgroundColor: currentPage === 1 ? FINCEPT.MUTED : 'transparent',
                            color: currentPage === 1 ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                            border: `1px solid ${currentPage === 1 ? FINCEPT.MUTED : FINCEPT.BORDER}`,
                            opacity: currentPage === 1 ? 0.5 : 1,
                            cursor: currentPage === 1 ? 'not-allowed' : 'pointer',
                          }}
                        >
                          PREV
                        </button>
                        <span style={{ fontSize: '9px', padding: '0 12px', color: FINCEPT.WHITE }}>
                          Page <span style={{ color: FINCEPT.CYAN }}>{currentPage}</span> of {totalPages}
                        </span>
                        <button
                          onClick={() => setCurrentPage(currentPage + 1)}
                          disabled={currentPage === totalPages}
                          style={{
                            padding: '4px 10px',
                            borderRadius: '2px',
                            fontSize: '9px',
                            fontWeight: 700,
                            backgroundColor: currentPage === totalPages ? FINCEPT.MUTED : 'transparent',
                            color: currentPage === totalPages ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                            border: `1px solid ${currentPage === totalPages ? FINCEPT.MUTED : FINCEPT.BORDER}`,
                            opacity: currentPage === totalPages ? 0.5 : 1,
                            cursor: currentPage === totalPages ? 'not-allowed' : 'pointer',
                          }}
                        >
                          NEXT
                        </button>
                        <button
                          onClick={() => setCurrentPage(totalPages)}
                          disabled={currentPage === totalPages}
                          style={{
                            padding: '4px 10px',
                            borderRadius: '2px',
                            fontSize: '9px',
                            fontWeight: 700,
                            backgroundColor: currentPage === totalPages ? FINCEPT.MUTED : 'transparent',
                            color: currentPage === totalPages ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                            border: `1px solid ${currentPage === totalPages ? FINCEPT.MUTED : FINCEPT.BORDER}`,
                            opacity: currentPage === totalPages ? 0.5 : 1,
                            cursor: currentPage === totalPages ? 'not-allowed' : 'pointer',
                          }}
                        >
                          LAST
                        </button>
                      </div>
                    </div>
                  )}
                </div>
              ) : (
                <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
                  <pre
                    style={{
                      flex: 1,
                      fontSize: '10px',
                      fontFamily: '"IBM Plex Mono", monospace',
                      padding: '16px',
                      borderRadius: '2px',
                      overflow: 'auto',
                      backgroundColor: FINCEPT.HEADER_BG,
                      color: FINCEPT.WHITE,
                    }}
                  >
                    {JSON.stringify(paginatedData, null, 2)}
                  </pre>

                  {/* Pagination Controls for JSON view */}
                  {data && data.length > 0 && (
                    <div style={{
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'space-between',
                      padding: '10px 16px',
                      borderTop: `1px solid ${FINCEPT.BORDER}`,
                      backgroundColor: FINCEPT.HEADER_BG,
                    }}>
                      <span style={{ fontSize: '9px', color: FINCEPT.MUTED }}>
                        Showing page {currentPage} of {totalPages}
                      </span>
                      <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                        <button
                          onClick={() => setCurrentPage(Math.max(1, currentPage - 1))}
                          disabled={currentPage === 1}
                          style={{
                            padding: '4px 12px',
                            borderRadius: '2px',
                            fontSize: '9px',
                            fontWeight: 700,
                            backgroundColor: currentPage === 1 ? FINCEPT.MUTED : 'transparent',
                            color: currentPage === 1 ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                            border: `1px solid ${currentPage === 1 ? FINCEPT.MUTED : FINCEPT.BORDER}`,
                            opacity: currentPage === 1 ? 0.5 : 1,
                            cursor: currentPage === 1 ? 'not-allowed' : 'pointer',
                          }}
                        >
                          PREV
                        </button>
                        <button
                          onClick={() => setCurrentPage(Math.min(totalPages, currentPage + 1))}
                          disabled={currentPage === totalPages}
                          style={{
                            padding: '4px 12px',
                            borderRadius: '2px',
                            fontSize: '9px',
                            fontWeight: 700,
                            backgroundColor: currentPage === totalPages ? FINCEPT.MUTED : 'transparent',
                            color: currentPage === totalPages ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                            border: `1px solid ${currentPage === totalPages ? FINCEPT.MUTED : FINCEPT.BORDER}`,
                            opacity: currentPage === totalPages ? 0.5 : 1,
                            cursor: currentPage === totalPages ? 'not-allowed' : 'pointer',
                          }}
                        >
                          NEXT
                        </button>
                      </div>
                    </div>
                  )}
                </div>
              )
            ) : !selectedEndpoint ? (
              <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: '100%' }}>
                <div style={{ textAlign: 'center', maxWidth: '500px' }}>
                  <Zap size={40} color={FINCEPT.ORANGE} style={{ marginBottom: '16px' }} />
                  <div style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.WHITE, marginBottom: '8px' }}>
                    AKSHARE DATA EXPLORER
                  </div>
                  <div style={{ fontSize: '10px', marginBottom: '24px', color: FINCEPT.MUTED }}>
                    Access 1000+ financial data endpoints across 26 data sources covering bonds, derivatives, economics, stocks, funds, and alternative data from Chinese and global markets.
                  </div>
                  <div style={{
                    display: 'grid',
                    gridTemplateColumns: 'repeat(3, 1fr)',
                    gap: '12px',
                  }}>
                    {AKSHARE_DATA_SOURCES.slice(0, 6).map(source => (
                      <button
                        key={source.id}
                        onClick={() => setSelectedSource(source)}
                        style={{
                          padding: '12px',
                          borderRadius: '2px',
                          textAlign: 'left',
                          backgroundColor: 'transparent',
                          border: `1px solid ${FINCEPT.BORDER}`,
                          cursor: 'pointer',
                          transition: 'all 0.2s',
                        }}
                      >
                        <div style={{ fontWeight: 600, color: FINCEPT.WHITE, fontSize: '10px' }}>{source.name}</div>
                        <div style={{ color: FINCEPT.MUTED, fontSize: '9px', marginTop: '4px' }}>{source.categories.length} categories</div>
                      </button>
                    ))}
                  </div>
                </div>
              </div>
            ) : (
              <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: '100%' }}>
                <div style={{ textAlign: 'center' }}>
                  <CheckCircle2 size={28} color={FINCEPT.GREEN} style={{ marginBottom: '12px' }} />
                  <div style={{ fontSize: '10px', color: FINCEPT.MUTED }}>No data returned</div>
                </div>
              </div>
            )}
          </div>
        </div>

        {/* Favorites Panel (Right side panel - 280px) */}
        {showFavorites && (
          <div style={{
            width: '280px',
            borderLeft: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            flexDirection: 'column',
            backgroundColor: FINCEPT.PANEL_BG,
          }}>
            <div style={{
              padding: '12px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
            }}>
              <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.YELLOW, letterSpacing: '0.5px' }}>FAVORITES</span>
              <button
                onClick={() => setShowFavorites(false)}
                style={{
                  padding: '4px',
                  backgroundColor: 'transparent',
                  border: 'none',
                  cursor: 'pointer',
                }}
              >
                <X size={14} color={FINCEPT.MUTED} />
              </button>
            </div>
            <div className="akshare-scrollbar" style={{ flex: 1, overflowY: 'auto' }}>
              {favorites.length === 0 ? (
                <div style={{ padding: '16px', textAlign: 'center', fontSize: '10px', color: FINCEPT.MUTED }}>
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
                    style={{
                      width: '100%',
                      padding: '10px 12px',
                      textAlign: 'left',
                      fontSize: '10px',
                      backgroundColor: 'transparent',
                      border: 'none',
                      borderBottom: `1px solid ${FINCEPT.BORDER}`,
                      cursor: 'pointer',
                      transition: 'all 0.2s',
                    }}
                  >
                    <div style={{ color: FINCEPT.WHITE, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{fav.endpoint}</div>
                    <div style={{ color: FINCEPT.MUTED, fontSize: '9px', marginTop: '2px' }}>{fav.script.replace('akshare_', '').replace('.py', '')}</div>
                  </button>
                ))
              )}
            </div>
          </div>
        )}

        {/* History Panel (Right side panel - 280px) */}
        {showHistory && (
          <div style={{
            width: '280px',
            borderLeft: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            flexDirection: 'column',
            backgroundColor: FINCEPT.PANEL_BG,
          }}>
            <div style={{
              padding: '12px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
            }}>
              <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.CYAN, letterSpacing: '0.5px' }}>QUERY HISTORY</span>
              <button
                onClick={() => setShowHistory(false)}
                style={{
                  padding: '4px',
                  backgroundColor: 'transparent',
                  border: 'none',
                  cursor: 'pointer',
                }}
              >
                <X size={14} color={FINCEPT.MUTED} />
              </button>
            </div>
            <div className="akshare-scrollbar" style={{ flex: 1, overflowY: 'auto' }}>
              {queryHistory.length === 0 ? (
                <div style={{ padding: '16px', textAlign: 'center', fontSize: '10px', color: FINCEPT.MUTED }}>
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
                    style={{
                      width: '100%',
                      padding: '10px 12px',
                      textAlign: 'left',
                      fontSize: '10px',
                      backgroundColor: 'transparent',
                      border: 'none',
                      borderBottom: `1px solid ${FINCEPT.BORDER}`,
                      cursor: 'pointer',
                      transition: 'all 0.2s',
                    }}
                  >
                    <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                      {item.success ? (
                        <CheckCircle2 size={10} color={FINCEPT.GREEN} />
                      ) : (
                        <AlertCircle size={10} color={FINCEPT.RED} />
                      )}
                      <span style={{ color: FINCEPT.WHITE, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{item.endpoint}</span>
                    </div>
                    <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginTop: '4px', color: FINCEPT.MUTED, fontSize: '9px' }}>
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

      {/* Status Bar */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderTop: `1px solid ${FINCEPT.BORDER}`,
        padding: '4px 16px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        fontSize: '9px',
        color: FINCEPT.GRAY,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <span>
            MODE: <span style={{ color: FINCEPT.ORANGE }}>DATA EXPLORER</span>
          </span>
          <span>
            SOURCE: <span style={{ color: selectedSource ? FINCEPT.CYAN : FINCEPT.MUTED }}>{selectedSource?.name || 'NONE'}</span>
          </span>
          <span>
            ENDPOINT: <span style={{ color: selectedEndpoint ? FINCEPT.CYAN : FINCEPT.MUTED }}>{selectedEndpoint || 'NONE'}</span>
          </span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <span>
            FAVORITES: <span style={{ color: FINCEPT.YELLOW }}>{favorites.length}</span>
          </span>
          <span>
            HISTORY: <span style={{ color: FINCEPT.CYAN }}>{queryHistory.length}</span>
          </span>
          {data && (
            <span>
              RECORDS: <span style={{ color: FINCEPT.GREEN }}>{data.length}</span>
            </span>
          )}
        </div>
      </div>
    </div>
  );
};

export default AkShareDataTab;
