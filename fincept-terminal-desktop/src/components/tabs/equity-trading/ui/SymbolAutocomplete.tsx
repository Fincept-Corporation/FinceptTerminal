/**
 * Symbol Autocomplete Component
 * Provides autocomplete suggestions for stock symbols
 */

import React, { useState, useEffect, useRef } from 'react';
import { Search, TrendingUp } from 'lucide-react';
import { symbolMasterService } from '../services/SymbolMasterService';

const BLOOMBERG = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  INPUT_BG: '#0A0A0A',
  CYAN: '#00E5FF',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
};

interface SymbolAutocompleteProps {
  value: string;
  onChange: (value: string) => void;
  onSelect?: (symbol: string) => void;
  placeholder?: string;
  style?: React.CSSProperties;
}

const SymbolAutocomplete: React.FC<SymbolAutocompleteProps> = ({
  value,
  onChange,
  onSelect,
  placeholder = 'Search symbols...',
  style = {},
}) => {
  const [suggestions, setSuggestions] = useState<Array<{ ticker: string; name: string; shortName: string }>>([]);
  const [showDropdown, setShowDropdown] = useState(false);
  const [selectedIndex, setSelectedIndex] = useState(0);
  const [isServiceReady, setIsServiceReady] = useState(false);
  const dropdownRef = useRef<HTMLDivElement>(null);
  const inputRef = useRef<HTMLInputElement>(null);

  // Initialize symbol master service
  useEffect(() => {
    const initService = async () => {
      await symbolMasterService.initialize();
      setIsServiceReady(true);
      console.log(`[SymbolAutocomplete] Service ready with ${symbolMasterService.getSymbolsCount()} symbols`);
    };

    initService();
  }, []);

  // Search for symbols when value changes
  useEffect(() => {
    if (!isServiceReady) return;

    if (value.length >= 1) {
      const results = symbolMasterService.searchSymbols(value, 10);
      setSuggestions(results);
      setShowDropdown(results.length > 0);
      setSelectedIndex(0);
    } else {
      setSuggestions([]);
      setShowDropdown(false);
    }
  }, [value, isServiceReady]);

  // Handle click outside to close dropdown
  useEffect(() => {
    const handleClickOutside = (event: MouseEvent) => {
      if (
        dropdownRef.current &&
        !dropdownRef.current.contains(event.target as Node) &&
        !inputRef.current?.contains(event.target as Node)
      ) {
        setShowDropdown(false);
      }
    };

    document.addEventListener('mousedown', handleClickOutside);
    return () => document.removeEventListener('mousedown', handleClickOutside);
  }, []);

  const handleInputChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const newValue = e.target.value.toUpperCase();
    onChange(newValue);
  };

  const handleSelectSymbol = (ticker: string) => {
    onChange(ticker);
    setShowDropdown(false);
    if (onSelect) {
      onSelect(ticker);
    }
  };

  const handleKeyDown = (e: React.KeyboardEvent) => {
    if (!showDropdown || suggestions.length === 0) return;

    switch (e.key) {
      case 'ArrowDown':
        e.preventDefault();
        setSelectedIndex((prev) => (prev + 1) % suggestions.length);
        break;
      case 'ArrowUp':
        e.preventDefault();
        setSelectedIndex((prev) => (prev - 1 + suggestions.length) % suggestions.length);
        break;
      case 'Enter':
        e.preventDefault();
        if (suggestions[selectedIndex]) {
          handleSelectSymbol(suggestions[selectedIndex].ticker);
        }
        break;
      case 'Escape':
        setShowDropdown(false);
        break;
    }
  };

  return (
    <div style={{ position: 'relative', width: '100%' }}>
      <div style={{ position: 'relative' }}>
        <input
          ref={inputRef}
          type="text"
          value={value}
          onChange={handleInputChange}
          onKeyDown={handleKeyDown}
          placeholder={placeholder}
          style={{
            width: '100%',
            padding: '8px 12px 8px 32px',
            backgroundColor: BLOOMBERG.INPUT_BG,
            border: `1px solid ${BLOOMBERG.BORDER}`,
            color: BLOOMBERG.WHITE,
            fontSize: '11px',
            fontWeight: 600,
            fontFamily: 'monospace',
            outline: 'none',
            transition: 'border-color 0.2s',
            ...style,
          }}
          onFocus={(e) => {
            e.currentTarget.style.borderColor = BLOOMBERG.CYAN;
            if (value.length >= 1 && suggestions.length > 0) setShowDropdown(true);
          }}
          onBlur={(e) => {
            e.currentTarget.style.borderColor = BLOOMBERG.BORDER;
          }}
        />
        <Search
          size={14}
          color={BLOOMBERG.GRAY}
          style={{
            position: 'absolute',
            left: '10px',
            top: '50%',
            transform: 'translateY(-50%)',
            pointerEvents: 'none',
          }}
        />
      </div>

      {/* Autocomplete Dropdown */}
      {showDropdown && suggestions.length > 0 && (
        <div
          ref={dropdownRef}
          style={{
            position: 'absolute',
            top: '100%',
            left: 0,
            right: 0,
            marginTop: '4px',
            backgroundColor: BLOOMBERG.PANEL_BG,
            border: `1px solid ${BLOOMBERG.CYAN}`,
            borderRadius: '4px',
            maxHeight: '300px',
            overflowY: 'auto',
            zIndex: 1000,
            boxShadow: `0 4px 12px rgba(0, 229, 255, 0.2)`,
          }}
        >
          {suggestions.map((suggestion, index) => (
            <div
              key={suggestion.ticker}
              onClick={() => handleSelectSymbol(suggestion.ticker)}
              style={{
                padding: '10px 12px',
                cursor: 'pointer',
                backgroundColor: index === selectedIndex ? BLOOMBERG.HOVER : 'transparent',
                borderBottom: index < suggestions.length - 1 ? `1px solid ${BLOOMBERG.BORDER}` : 'none',
                transition: 'background-color 0.15s',
              }}
              onMouseEnter={(e) => {
                setSelectedIndex(index);
                e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER;
              }}
              onMouseLeave={(e) => {
                e.currentTarget.style.backgroundColor = index === selectedIndex ? BLOOMBERG.HOVER : 'transparent';
              }}
            >
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <TrendingUp size={12} color={BLOOMBERG.CYAN} />
                <div style={{ flex: 1 }}>
                  <div style={{
                    fontSize: '12px',
                    fontWeight: 700,
                    color: BLOOMBERG.WHITE,
                    fontFamily: 'monospace',
                  }}>
                    {suggestion.ticker}
                  </div>
                  <div style={{
                    fontSize: '9px',
                    color: BLOOMBERG.GRAY,
                    marginTop: '2px',
                    overflow: 'hidden',
                    textOverflow: 'ellipsis',
                    whiteSpace: 'nowrap',
                  }}>
                    {suggestion.name}
                  </div>
                </div>
              </div>
            </div>
          ))}
        </div>
      )}
    </div>
  );
};

export default SymbolAutocomplete;
