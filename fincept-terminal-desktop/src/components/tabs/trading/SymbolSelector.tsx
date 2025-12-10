// File: src/components/tabs/trading/SymbolSelector.tsx
// Dynamic symbol selector that adapts to active broker

import React, { useState, useEffect, useMemo } from 'react';
import { useBrokerContext } from '../../../contexts/BrokerContext';
import { Search } from 'lucide-react';

const BLOOMBERG_COLORS = {
  ORANGE: '#ea580c',
  WHITE: '#ffffff',
  GRAY: '#525252',
  DARK_BG: '#0d0d0d',
  PANEL_BG: '#1a1a1a',
};

interface SymbolSelectorProps {
  selectedSymbol: string;
  onSymbolChange: (symbol: string) => void;
}

export function SymbolSelector({ selectedSymbol, onSymbolChange }: SymbolSelectorProps) {
  const { activeBroker, activeBrokerMetadata, defaultSymbols } = useBrokerContext();
  const [searchValue, setSearchValue] = useState('');
  const [isFocused, setIsFocused] = useState(false);

  const { ORANGE, WHITE, GRAY, PANEL_BG } = BLOOMBERG_COLORS;

  // Update search value when selected symbol changes
  useEffect(() => {
    setSearchValue(selectedSymbol);
  }, [selectedSymbol]);

  // Filter symbols based on search
  const filteredSymbols = useMemo(() => {
    if (!searchValue || searchValue === selectedSymbol) {
      return defaultSymbols;
    }
    return defaultSymbols.filter(symbol =>
      symbol.toLowerCase().includes(searchValue.toLowerCase())
    );
  }, [defaultSymbols, searchValue, selectedSymbol]);

  // Get placeholder based on broker
  const placeholder = useMemo(() => {
    if (!activeBrokerMetadata) return 'Search symbol...';

    if (activeBroker === 'kraken') {
      return 'e.g. BTC/USD';
    } else if (activeBroker === 'hyperliquid') {
      return 'e.g. BTC/USDC:USDC';
    }
    return 'Search symbol...';
  }, [activeBroker, activeBrokerMetadata]);

  const handleSymbolClick = (symbol: string) => {
    setSearchValue(symbol);
    onSymbolChange(symbol);
    setIsFocused(false);
  };

  const handleInputChange = (value: string) => {
    setSearchValue(value.toUpperCase());
  };

  const handleInputKeyPress = (e: React.KeyboardEvent<HTMLInputElement>) => {
    if (e.key === 'Enter') {
      if (filteredSymbols.length > 0) {
        handleSymbolClick(filteredSymbols[0]);
      } else if (searchValue) {
        onSymbolChange(searchValue);
        setIsFocused(false);
      }
    }
  };

  return (
    <div style={{
      display: 'flex',
      alignItems: 'center',
      gap: '8px',
      backgroundColor: PANEL_BG,
      padding: '8px 12px',
      borderBottom: `1px solid ${GRAY}`,
      position: 'relative',
    }}>
      <span style={{
        fontSize: '10px',
        color: GRAY,
        fontWeight: 'bold',
        textTransform: 'uppercase',
      }}>
        {activeBroker === 'kraken' ? 'Pair:' : 'Symbol:'}
      </span>

      {/* Popular Symbols */}
      <div style={{ display: 'flex', gap: '6px', flexWrap: 'wrap' }}>
        {defaultSymbols.map(symbol => (
          <button
            key={symbol}
            onClick={() => handleSymbolClick(symbol)}
            style={{
              padding: '4px 12px',
              fontSize: '11px',
              fontWeight: 'bold',
              textTransform: 'uppercase',
              backgroundColor: selectedSymbol === symbol ? ORANGE : '#2d2d2d',
              color: selectedSymbol === symbol ? '#000' : GRAY,
              border: selectedSymbol === symbol ? `1px solid ${ORANGE}` : '1px solid #404040',
              cursor: 'pointer',
              transition: 'all 0.2s',
              borderRadius: '2px',
            }}
            onMouseEnter={(e) => {
              if (selectedSymbol !== symbol) {
                e.currentTarget.style.backgroundColor = '#404040';
                e.currentTarget.style.borderColor = GRAY;
              }
            }}
            onMouseLeave={(e) => {
              if (selectedSymbol !== symbol) {
                e.currentTarget.style.backgroundColor = '#2d2d2d';
                e.currentTarget.style.borderColor = '#404040';
              }
            }}
          >
            {symbol}
          </button>
        ))}
      </div>

      {/* Search Input */}
      <div style={{ position: 'relative', marginLeft: '12px' }}>
        <div style={{
          display: 'flex',
          alignItems: 'center',
          backgroundColor: '#2d2d2d',
          border: `1px solid ${isFocused ? ORANGE : '#404040'}`,
          borderRadius: '2px',
          padding: '4px 8px',
          transition: 'border-color 0.2s',
        }}>
          <Search size={12} style={{ color: GRAY, marginRight: '6px' }} />
          <input
            type="text"
            placeholder={placeholder}
            value={searchValue}
            onChange={(e) => handleInputChange(e.target.value)}
            onFocus={() => setIsFocused(true)}
            onBlur={() => setTimeout(() => setIsFocused(false), 200)}
            onKeyPress={handleInputKeyPress}
            style={{
              backgroundColor: 'transparent',
              border: 'none',
              outline: 'none',
              color: WHITE,
              fontSize: '11px',
              width: '150px',
              fontFamily: 'Consolas, monospace',
            }}
          />
        </div>

        {/* Dropdown Suggestions */}
        {isFocused && filteredSymbols.length > 0 && searchValue !== selectedSymbol && (
          <div style={{
            position: 'absolute',
            top: '100%',
            left: 0,
            right: 0,
            marginTop: '4px',
            backgroundColor: '#1a1a1a',
            border: `1px solid ${GRAY}`,
            borderRadius: '2px',
            maxHeight: '200px',
            overflowY: 'auto',
            zIndex: 1000,
            boxShadow: '0 4px 6px rgba(0, 0, 0, 0.3)',
          }}>
            {filteredSymbols.map(symbol => (
              <button
                key={symbol}
                onClick={() => handleSymbolClick(symbol)}
                style={{
                  display: 'block',
                  width: '100%',
                  padding: '8px 12px',
                  fontSize: '11px',
                  fontWeight: 'bold',
                  textAlign: 'left',
                  backgroundColor: 'transparent',
                  color: GRAY,
                  border: 'none',
                  cursor: 'pointer',
                  transition: 'background-color 0.2s',
                }}
                onMouseEnter={(e) => {
                  e.currentTarget.style.backgroundColor = '#2d2d2d';
                  e.currentTarget.style.color = WHITE;
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.backgroundColor = 'transparent';
                  e.currentTarget.style.color = GRAY;
                }}
              >
                {symbol}
              </button>
            ))}
          </div>
        )}
      </div>
    </div>
  );
}
