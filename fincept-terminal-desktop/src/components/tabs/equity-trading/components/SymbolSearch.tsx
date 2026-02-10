/**
 * SymbolSearch Component
 *
 * Professional symbol search dropdown using master contract database.
 * Provides fast, offline-capable symbol search with relevance ranking.
 */

import React, { useState, useEffect, useRef, useCallback } from 'react';
import { Search, Download, RefreshCw, ChevronDown, X, AlertCircle, Check, Loader2, Globe } from 'lucide-react';
import {
  masterContractService,
  type SymbolSearchResult,
  type SupportedBroker,
  type DownloadResult
} from '@/services/trading/masterContractService';
import type { StockExchange } from '@/brokers/stocks/types';

// Fincept Professional Color Palette
const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
};

interface SymbolSearchProps {
  /** Currently selected symbol */
  selectedSymbol: string;
  /** Currently selected exchange */
  selectedExchange: StockExchange;
  /** Callback when symbol is selected */
  onSymbolSelect: (symbol: string, exchange: StockExchange, result?: SymbolSearchResult) => void;
  /** Active broker ID */
  brokerId: SupportedBroker;
  /** Placeholder text */
  placeholder?: string;
  /** Whether to show download progress */
  showDownloadStatus?: boolean;
  /** Filter by exchange */
  exchangeFilter?: string;
  /** Filter by instrument type */
  instrumentTypeFilter?: string;
  /**
   * Use direct search via adapter instead of master contract service.
   * When true, onDirectSearch must be provided.
   * Used for brokers like yfinance that don't have master contract data.
   */
  useDirectSearch?: boolean;
  /** Direct search function - called when useDirectSearch is true */
  onDirectSearch?: (query: string) => Promise<SymbolSearchResult[]>;
}

export function SymbolSearch({
  selectedSymbol,
  selectedExchange,
  onSymbolSelect,
  brokerId,
  placeholder = 'Search stocks...',
  showDownloadStatus = true,
  exchangeFilter,
  instrumentTypeFilter,
  useDirectSearch = false,
  onDirectSearch,
}: SymbolSearchProps) {
  // State
  const [isOpen, setIsOpen] = useState(false);
  const [query, setQuery] = useState('');
  const [results, setResults] = useState<SymbolSearchResult[]>([]);
  const [isSearching, setIsSearching] = useState(false);
  const [symbolCount, setSymbolCount] = useState(0);
  const [exchanges, setExchanges] = useState<string[]>([]);
  const [selectedFilterExchange, setSelectedFilterExchange] = useState<string | undefined>(exchangeFilter);

  // Download state
  const [isDownloading, setIsDownloading] = useState(false);
  const [downloadStatus, setDownloadStatus] = useState<'idle' | 'downloading' | 'success' | 'error'>('idle');
  const [downloadMessage, setDownloadMessage] = useState('');

  // Refs
  const containerRef = useRef<HTMLDivElement>(null);
  const inputRef = useRef<HTMLInputElement>(null);
  const searchTimeoutRef = useRef<number | null>(null);

  // Check if master contract data exists on mount (skip for direct search brokers)
  useEffect(() => {
    if (useDirectSearch) {
      // Direct search brokers (e.g. yfinance) don't use master contract
      setSymbolCount(-1); // -1 signals "ready" without master contract
      return;
    }

    const checkData = async () => {
      try {
        const count = await masterContractService.getSymbolCount(brokerId);
        setSymbolCount(count);

        if (count > 0) {
          const exchangeList = await masterContractService.getExchanges(brokerId);
          setExchanges(exchangeList);
        }
      } catch (error) {
        console.error('[SymbolSearch] Error checking symbol data:', error);
        setSymbolCount(0);
      }
    };

    checkData();
  }, [brokerId, useDirectSearch]);

  // Close dropdown when clicking outside
  useEffect(() => {
    const handleClickOutside = (event: MouseEvent) => {
      if (containerRef.current && !containerRef.current.contains(event.target as Node)) {
        setIsOpen(false);
      }
    };

    document.addEventListener('mousedown', handleClickOutside);
    return () => document.removeEventListener('mousedown', handleClickOutside);
  }, []);

  // Debounced search
  const performSearch = useCallback(async (searchQuery: string) => {
    if (!searchQuery || searchQuery.length < 1) {
      setResults([]);
      setIsSearching(false);
      return;
    }

    setIsSearching(true);
    try {
      if (useDirectSearch && onDirectSearch) {
        // Direct search via adapter (e.g. yfinance Python subprocess)
        const searchResults = await onDirectSearch(searchQuery);
        setResults(searchResults);
      } else {
        // Standard master contract search
        const searchResults = await masterContractService.safeSearch(brokerId, searchQuery, {
          exchange: selectedFilterExchange,
          instrumentType: instrumentTypeFilter,
          limit: 50
        });
        setResults(searchResults);
      }
    } catch (error) {
      console.error('[SymbolSearch] Search error:', error);
      setResults([]);
    } finally {
      setIsSearching(false);
    }
  }, [brokerId, selectedFilterExchange, instrumentTypeFilter, useDirectSearch, onDirectSearch]);

  // Handle query change with debounce
  const handleQueryChange = (value: string) => {
    setQuery(value);

    if (searchTimeoutRef.current) {
      clearTimeout(searchTimeoutRef.current);
    }

    // Longer debounce for direct search (Python subprocess) vs local DB
    const debounceMs = useDirectSearch ? 400 : 150;
    searchTimeoutRef.current = window.setTimeout(() => {
      performSearch(value);
    }, debounceMs);
  };

  // Handle symbol selection
  const handleSelect = (result: SymbolSearchResult) => {
    onSymbolSelect(result.symbol, result.exchange as StockExchange, result);
    setQuery('');
    setResults([]);
    setIsOpen(false);
  };

  // Download master contract
  const handleDownload = async () => {
    setIsDownloading(true);
    setDownloadStatus('downloading');
    setDownloadMessage('Downloading symbol data...');

    try {
      const result = await masterContractService.downloadMasterContract(brokerId);

      if (result.success) {
        setDownloadStatus('success');
        setDownloadMessage(`Downloaded ${result.symbol_count.toLocaleString()} symbols`);
        setSymbolCount(result.symbol_count);

        // Refresh exchanges
        const exchangeList = await masterContractService.getExchanges(brokerId);
        setExchanges(exchangeList);
      } else {
        setDownloadStatus('error');
        setDownloadMessage(result.message || 'Download failed');
      }
    } catch (error: any) {
      setDownloadStatus('error');
      setDownloadMessage(error.message || 'Download failed');
    } finally {
      setIsDownloading(false);

      // Reset status after 3 seconds
      setTimeout(() => {
        setDownloadStatus('idle');
        setDownloadMessage('');
      }, 3000);
    }
  };

  // Focus input when dropdown opens
  useEffect(() => {
    if (isOpen && inputRef.current) {
      inputRef.current.focus();
    }
  }, [isOpen]);

  return (
    <div ref={containerRef} style={{ position: 'relative', minWidth: '220px' }}>
      {/* Trigger Button */}
      <div
        onClick={() => setIsOpen(!isOpen)}
        style={{
          padding: '6px 12px',
          backgroundColor: FINCEPT.HEADER_BG,
          border: `1px solid ${isOpen ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
          cursor: 'pointer',
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          transition: 'border-color 0.2s'
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span style={{
            fontSize: '13px',
            fontWeight: 700,
            color: selectedSymbol ? FINCEPT.ORANGE : FINCEPT.GRAY
          }}>
            {selectedSymbol || 'Select Symbol'}
          </span>
          {selectedSymbol && (
            <span style={{ fontSize: '10px', color: FINCEPT.GRAY }}>{selectedExchange}</span>
          )}
        </div>
        <ChevronDown
          size={12}
          color={FINCEPT.GRAY}
          style={{
            transform: isOpen ? 'rotate(180deg)' : 'rotate(0deg)',
            transition: 'transform 0.2s'
          }}
        />
      </div>

      {/* Dropdown */}
      {isOpen && (
        <div style={{
          position: 'absolute',
          top: '100%',
          left: 0,
          right: 0,
          marginTop: '4px',
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          maxHeight: '450px',
          overflow: 'hidden',
          zIndex: 1000,
          boxShadow: `0 4px 16px ${FINCEPT.DARK_BG}80`,
          display: 'flex',
          flexDirection: 'column'
        }}>
          {/* Search Input */}
          <div style={{ padding: '8px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={{ position: 'relative' }}>
              <Search
                size={14}
                color={FINCEPT.GRAY}
                style={{ position: 'absolute', left: '8px', top: '50%', transform: 'translateY(-50%)' }}
              />
              <input
                ref={inputRef}
                type="text"
                placeholder={placeholder}
                value={query}
                onChange={(e) => handleQueryChange(e.target.value)}
                style={{
                  width: '100%',
                  padding: '8px 32px 8px 32px',
                  backgroundColor: FINCEPT.HEADER_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  fontSize: '11px',
                  outline: 'none'
                }}
              />
              {query && (
                <X
                  size={14}
                  color={FINCEPT.GRAY}
                  style={{
                    position: 'absolute',
                    right: '8px',
                    top: '50%',
                    transform: 'translateY(-50%)',
                    cursor: 'pointer'
                  }}
                  onClick={(e) => {
                    e.stopPropagation();
                    setQuery('');
                    setResults([]);
                  }}
                />
              )}
            </div>

            {/* Exchange Filter */}
            {exchanges.length > 0 && (
              <div style={{
                marginTop: '8px',
                display: 'flex',
                gap: '4px',
                flexWrap: 'wrap'
              }}>
                <button
                  onClick={() => setSelectedFilterExchange(undefined)}
                  style={{
                    padding: '3px 8px',
                    backgroundColor: !selectedFilterExchange ? FINCEPT.ORANGE : 'transparent',
                    border: `1px solid ${!selectedFilterExchange ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                    color: !selectedFilterExchange ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                    cursor: 'pointer',
                    fontSize: '9px',
                    fontWeight: 600
                  }}
                >
                  ALL
                </button>
                {exchanges.slice(0, 6).map(exch => (
                  <button
                    key={exch}
                    onClick={() => setSelectedFilterExchange(exch === selectedFilterExchange ? undefined : exch)}
                    style={{
                      padding: '3px 8px',
                      backgroundColor: selectedFilterExchange === exch ? FINCEPT.ORANGE : 'transparent',
                      border: `1px solid ${selectedFilterExchange === exch ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                      color: selectedFilterExchange === exch ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                      cursor: 'pointer',
                      fontSize: '9px',
                      fontWeight: 600
                    }}
                  >
                    {exch}
                  </button>
                ))}
              </div>
            )}
          </div>

          {/* Status/Download Bar - hidden for direct search brokers */}
          {showDownloadStatus && !useDirectSearch && (
            <div style={{
              padding: '6px 12px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center',
              fontSize: '9px'
            }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                {downloadStatus === 'downloading' && (
                  <Loader2 size={12} color={FINCEPT.CYAN} className="animate-spin" />
                )}
                {downloadStatus === 'success' && (
                  <Check size={12} color={FINCEPT.GREEN} />
                )}
                {downloadStatus === 'error' && (
                  <AlertCircle size={12} color={FINCEPT.RED} />
                )}

                <span style={{
                  color: downloadStatus === 'error' ? FINCEPT.RED :
                    downloadStatus === 'success' ? FINCEPT.GREEN : FINCEPT.GRAY
                }}>
                  {downloadMessage || (symbolCount > 0
                    ? `${symbolCount.toLocaleString()} symbols loaded`
                    : 'No symbols loaded'
                  )}
                </span>
              </div>

              <button
                onClick={handleDownload}
                disabled={isDownloading}
                style={{
                  padding: '4px 8px',
                  backgroundColor: 'transparent',
                  border: `1px solid ${FINCEPT.BORDER}`,
                  color: isDownloading ? FINCEPT.MUTED : FINCEPT.CYAN,
                  cursor: isDownloading ? 'not-allowed' : 'pointer',
                  fontSize: '9px',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '4px',
                  transition: 'all 0.2s'
                }}
              >
                {isDownloading ? (
                  <RefreshCw size={10} className="animate-spin" />
                ) : (
                  <Download size={10} />
                )}
                {symbolCount > 0 ? 'REFRESH' : 'DOWNLOAD'}
              </button>
            </div>
          )}
          {/* Direct search info bar */}
          {useDirectSearch && (
            <div style={{
              padding: '6px 12px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              fontSize: '9px'
            }}>
              <Globe size={10} color={FINCEPT.CYAN} />
              <span style={{ color: FINCEPT.GRAY }}>
                Search Yahoo Finance — global stocks, ETFs, indices
              </span>
            </div>
          )}

          {/* Results */}
          <div style={{ flex: 1, overflow: 'auto', maxHeight: '340px' }}>
            {isSearching ? (
              <div style={{
                padding: '20px',
                textAlign: 'center',
                color: FINCEPT.GRAY,
                fontSize: '11px'
              }}>
                <Loader2 size={16} className="animate-spin" style={{ margin: '0 auto 8px' }} />
                Searching...
              </div>
            ) : results.length > 0 ? (
              results.map((result, idx) => (
                <div
                  key={`${result.symbol}-${result.exchange}-${result.token}`}
                  onClick={() => handleSelect(result)}
                  style={{
                    padding: '10px 12px',
                    cursor: 'pointer',
                    fontSize: '11px',
                    backgroundColor: 'transparent',
                    borderLeft: '3px solid transparent',
                    borderBottom: `1px solid ${FINCEPT.BORDER}20`
                  }}
                  onMouseEnter={(e) => {
                    e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                    e.currentTarget.style.borderLeftColor = FINCEPT.ORANGE;
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.backgroundColor = 'transparent';
                    e.currentTarget.style.borderLeftColor = 'transparent';
                  }}
                >
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                      <span style={{ color: FINCEPT.WHITE, fontWeight: 700, fontSize: '12px' }}>
                        {result.symbol}
                      </span>
                      <span style={{
                        padding: '2px 4px',
                        backgroundColor: FINCEPT.HEADER_BG,
                        color: FINCEPT.CYAN,
                        fontSize: '8px',
                        fontWeight: 600
                      }}>
                        {result.exchange}
                      </span>
                      {result.instrument_type && result.instrument_type !== 'EQ' && (
                        <span style={{
                          padding: '2px 4px',
                          backgroundColor: FINCEPT.PURPLE + '30',
                          color: FINCEPT.PURPLE,
                          fontSize: '8px',
                          fontWeight: 600
                        }}>
                          {result.instrument_type}
                        </span>
                      )}
                    </div>
                    {result.token && (
                      <span style={{ color: FINCEPT.MUTED, fontSize: '9px', fontFamily: 'monospace' }}>
                        {result.token}
                      </span>
                    )}
                  </div>
                  {result.name && (
                    <div style={{
                      color: FINCEPT.GRAY,
                      fontSize: '10px',
                      marginTop: '4px',
                      whiteSpace: 'nowrap',
                      overflow: 'hidden',
                      textOverflow: 'ellipsis'
                    }}>
                      {result.name}
                    </div>
                  )}
                  {result.expiry && (
                    <div style={{
                      display: 'flex',
                      gap: '12px',
                      marginTop: '4px',
                      fontSize: '9px',
                      color: FINCEPT.MUTED
                    }}>
                      <span>Expiry: {result.expiry}</span>
                      {result.strike && <span>Strike: {result.strike}</span>}
                      {result.lot_size && <span>Lot: {result.lot_size}</span>}
                    </div>
                  )}
                </div>
              ))
            ) : query.length > 0 ? (
              <div style={{
                padding: '20px',
                textAlign: 'center',
                color: FINCEPT.GRAY,
                fontSize: '11px'
              }}>
                No symbols found for "{query}"
              </div>
            ) : useDirectSearch ? (
              <div style={{
                padding: '16px',
                textAlign: 'center',
                color: FINCEPT.MUTED,
                fontSize: '10px'
              }}>
                Type a symbol name (e.g. AAPL, RELIANCE, TSLA)
              </div>
            ) : symbolCount === 0 ? (
              <div style={{
                padding: '30px 20px',
                textAlign: 'center',
                color: FINCEPT.GRAY
              }}>
                <Download size={32} color={FINCEPT.MUTED} style={{ margin: '0 auto 12px' }} />
                <div style={{ fontSize: '12px', marginBottom: '8px' }}>
                  No symbol data available
                </div>
                <div style={{ fontSize: '10px', color: FINCEPT.MUTED, marginBottom: '16px' }}>
                  Download master contract to enable symbol search
                </div>
                <button
                  onClick={handleDownload}
                  disabled={isDownloading}
                  style={{
                    padding: '8px 16px',
                    backgroundColor: FINCEPT.ORANGE,
                    border: 'none',
                    color: FINCEPT.DARK_BG,
                    cursor: isDownloading ? 'not-allowed' : 'pointer',
                    fontSize: '10px',
                    fontWeight: 700,
                    display: 'inline-flex',
                    alignItems: 'center',
                    gap: '6px'
                  }}
                >
                  {isDownloading ? (
                    <RefreshCw size={12} className="animate-spin" />
                  ) : (
                    <Download size={12} />
                  )}
                  DOWNLOAD SYMBOLS
                </button>
              </div>
            ) : (
              <div style={{
                padding: '16px',
                textAlign: 'center',
                color: FINCEPT.MUTED,
                fontSize: '10px'
              }}>
                Type to search {symbolCount.toLocaleString()} symbols
              </div>
            )}
          </div>
        </div>
      )}

      {/* Spinner animation CSS */}
      <style>{`
        @keyframes spin {
          from { transform: rotate(0deg); }
          to { transform: rotate(360deg); }
        }
        .animate-spin {
          animation: spin 1s linear infinite;
        }
      `}</style>
    </div>
  );
}

export default SymbolSearch;
