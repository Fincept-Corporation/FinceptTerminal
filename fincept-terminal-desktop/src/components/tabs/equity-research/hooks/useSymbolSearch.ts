import { useState, useRef, useEffect, useCallback } from 'react';
import type { SearchResult } from '../types';

interface UseSymbolSearchReturn {
  searchSymbol: string;
  setSearchSymbol: (value: string) => void;
  searchResults: SearchResult[];
  showSearchDropdown: boolean;
  setShowSearchDropdown: (value: boolean) => void;
  searchLoading: boolean;
  handleSearchInput: (query: string) => void;
  handleSelectSymbol: (symbol: string, onSelect: (symbol: string) => void) => void;
  searchContainerRef: React.RefObject<HTMLDivElement>;
}

export function useSymbolSearch(): UseSymbolSearchReturn {
  const [searchSymbol, setSearchSymbol] = useState('');
  const [searchResults, setSearchResults] = useState<SearchResult[]>([]);
  const [showSearchDropdown, setShowSearchDropdown] = useState(false);
  const [searchLoading, setSearchLoading] = useState(false);

  const searchDebounceRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const searchContainerRef = useRef<HTMLDivElement>(null);

  // HTTP Symbol Search function
  const searchSymbolsHttp = async (query: string) => {
    try {
      const response = await fetch(`https://finceptbackend.share.zrok.io/search/symbols?query=${encodeURIComponent(query)}&limit=50`);
      if (response.ok) {
        const data = await response.json();
        console.log('[Search] HTTP results:', data);
        setSearchResults(data.results || []);
        setSearchLoading(false);
        if (data.results?.length > 0) {
          setShowSearchDropdown(true);
        }
      } else {
        console.error('[Search] HTTP error:', response.status);
        setSearchLoading(false);
      }
    } catch (e) {
      console.error('[Search] HTTP fetch error:', e);
      setSearchLoading(false);
    }
  };

  // Cleanup debounce on unmount
  useEffect(() => {
    return () => {
      if (searchDebounceRef.current) {
        clearTimeout(searchDebounceRef.current);
      }
    };
  }, []);

  // Close dropdown when clicking outside
  useEffect(() => {
    const handleClickOutside = (e: MouseEvent) => {
      if (searchContainerRef.current && !searchContainerRef.current.contains(e.target as Node)) {
        setShowSearchDropdown(false);
      }
    };
    document.addEventListener('mousedown', handleClickOutside);
    return () => document.removeEventListener('mousedown', handleClickOutside);
  }, []);

  // Debounced search function (non-blocking)
  const handleSearchInput = useCallback((query: string) => {
    if (searchDebounceRef.current) {
      clearTimeout(searchDebounceRef.current);
    }

    if (query.length < 2) {
      setSearchResults([]);
      setShowSearchDropdown(false);
      setSearchLoading(false);
      return;
    }

    setSearchLoading(true);

    searchDebounceRef.current = setTimeout(() => {
      searchSymbolsHttp(query);
    }, 300);
  }, []);

  // Handle symbol selection from dropdown
  const handleSelectSymbol = useCallback((symbol: string, onSelect: (symbol: string) => void) => {
    setSearchSymbol(symbol);
    setShowSearchDropdown(false);
    setSearchResults([]);
    onSelect(symbol);
  }, []);

  return {
    searchSymbol,
    setSearchSymbol,
    searchResults,
    showSearchDropdown,
    setShowSearchDropdown,
    searchLoading,
    handleSearchInput,
    handleSelectSymbol,
    searchContainerRef: searchContainerRef as React.RefObject<HTMLDivElement>,
  };
}

export default useSymbolSearch;
