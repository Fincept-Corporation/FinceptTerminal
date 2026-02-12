// File: src/components/command-bar/CommandBar.tsx
// Fincept-style command bar with autocomplete

import React, { useState, useRef, useEffect } from 'react';
import { createPortal } from 'react-dom';
import { Terminal, ChevronRight } from 'lucide-react';
import { searchCommands, getCommand, Command } from './commandRegistry';

interface CommandBarProps {
  onExecuteCommand: (tabId: string) => void;
}

export const CommandBar: React.FC<CommandBarProps> = ({ onExecuteCommand }) => {
  const [input, setInput] = useState('');
  const [suggestions, setSuggestions] = useState<Command[]>([]);
  const [selectedIndex, setSelectedIndex] = useState(0);
  const [showSuggestions, setShowSuggestions] = useState(false);
  const [dropdownPosition, setDropdownPosition] = useState({ top: 0, left: 0, width: 0 });
  const inputRef = useRef<HTMLInputElement>(null);
  const containerRef = useRef<HTMLDivElement>(null);
  const suggestionsRef = useRef<HTMLDivElement>(null);

  // Update suggestions when input changes
  useEffect(() => {
    if (input.trim().length === 0) {
      setSuggestions([]);
      setShowSuggestions(false);
      setSelectedIndex(0);
      return;
    }

    const results = searchCommands(input);
    setSuggestions(results.slice(0, 10)); // Show top 10 results
    setShowSuggestions(results.length > 0);
    setSelectedIndex(0);
  }, [input]);

  // Handle keyboard navigation
  const handleKeyDown = (e: React.KeyboardEvent<HTMLInputElement>) => {
    if (!showSuggestions || suggestions.length === 0) {
      if (e.key === 'Enter') {
        executeCommand(input);
      }
      return;
    }

    switch (e.key) {
      case 'ArrowDown':
        e.preventDefault();
        setSelectedIndex(prev => (prev + 1) % suggestions.length);
        break;
      case 'ArrowUp':
        e.preventDefault();
        setSelectedIndex(prev => (prev - 1 + suggestions.length) % suggestions.length);
        break;
      case 'Tab':
        e.preventDefault();
        if (suggestions[selectedIndex]) {
          setInput(suggestions[selectedIndex].aliases[0]);
          setShowSuggestions(false);
        }
        break;
      case 'Enter':
        e.preventDefault();
        if (suggestions[selectedIndex]) {
          executeCommand(suggestions[selectedIndex].aliases[0]);
        } else {
          executeCommand(input);
        }
        break;
      case 'Escape':
        setShowSuggestions(false);
        setInput('');
        inputRef.current?.blur();
        break;
    }
  };

  // Execute command
  const executeCommand = (commandInput: string) => {
    const cmd = getCommand(commandInput.trim());
    if (cmd && typeof cmd.action === 'string') {
      onExecuteCommand(cmd.action);
      setInput('');
      setShowSuggestions(false);
      inputRef.current?.blur();
    } else if (cmd && typeof cmd.action === 'function') {
      cmd.action();
      setInput('');
      setShowSuggestions(false);
      inputRef.current?.blur();
    }
  };

  // Click outside to close suggestions
  useEffect(() => {
    const handleClickOutside = (event: MouseEvent) => {
      if (
        suggestionsRef.current &&
        !suggestionsRef.current.contains(event.target as Node) &&
        containerRef.current &&
        !containerRef.current.contains(event.target as Node)
      ) {
        setShowSuggestions(false);
      }
    };

    document.addEventListener('mousedown', handleClickOutside);
    return () => document.removeEventListener('mousedown', handleClickOutside);
  }, []);

  // Update dropdown position when showing suggestions
  useEffect(() => {
    if (showSuggestions && containerRef.current) {
      const rect = containerRef.current.getBoundingClientRect();
      setDropdownPosition({
        top: rect.bottom + 2,
        left: rect.left,
        width: Math.max(rect.width, 280) // Minimum width for better readability
      });
    }
  }, [showSuggestions, input]);

  // Render suggestions dropdown using portal to avoid overflow clipping
  const renderSuggestions = () => {
    if (!showSuggestions || suggestions.length === 0) return null;

    const dropdown = (
      <div
        ref={suggestionsRef}
        style={{
          position: 'fixed',
          top: dropdownPosition.top,
          left: dropdownPosition.left,
          width: dropdownPosition.width,
          backgroundColor: '#1a1a1a',
          border: '1px solid #404040',
          borderRadius: '2px',
          maxHeight: '300px',
          overflowY: 'auto',
          zIndex: 99999,
          boxShadow: '0 4px 12px rgba(0,0,0,0.5)'
        }}
      >
        {suggestions.map((cmd, index) => (
          <div
            key={cmd.id}
            onClick={() => {
              setInput(cmd.aliases[0]);
              executeCommand(cmd.aliases[0]);
            }}
            style={{
              padding: '8px 12px',
              cursor: 'pointer',
              backgroundColor: index === selectedIndex ? '#2a2a2a' : 'transparent',
              borderLeft: index === selectedIndex ? '3px solid #ea580c' : '3px solid transparent',
              display: 'flex',
              alignItems: 'flex-start',
              gap: '12px',
              transition: 'all 0.1s'
            }}
            onMouseEnter={() => setSelectedIndex(index)}
          >
            <div style={{ flex: 1 }}>
              <div style={{
                display: 'flex',
                alignItems: 'center',
                gap: '8px',
                marginBottom: '2px'
              }}>
                <span style={{
                  color: '#ffffff',
                  fontSize: '11px',
                  fontFamily: 'Consolas, "Courier New", monospace',
                  fontWeight: 'bold'
                }}>
                  {cmd.aliases[0].toUpperCase()}
                </span>
                <ChevronRight size={10} color="#666" />
                <span style={{
                  color: '#a3a3a3',
                  fontSize: '11px'
                }}>
                  {cmd.name}
                </span>
                {cmd.shortcut && (
                  <span style={{
                    marginLeft: 'auto',
                    color: '#666',
                    fontSize: '10px',
                    fontFamily: 'Consolas, "Courier New", monospace'
                  }}>
                    {cmd.shortcut}
                  </span>
                )}
              </div>
              <div style={{
                color: '#737373',
                fontSize: '10px',
                marginLeft: '0'
              }}>
                {cmd.description}
              </div>
            </div>
          </div>
        ))}
      </div>
    );

    return createPortal(dropdown, document.body);
  };

  return (
    <div ref={containerRef} style={{ position: 'relative', width: '180px', minWidth: '120px', maxWidth: '200px', flex: '0 1 180px' }}>
      <div style={{
        display: 'flex',
        alignItems: 'center',
        backgroundColor: '#1a1a1a',
        border: showSuggestions ? '1px solid #ea580c' : '1px solid #404040',
        borderRadius: '2px',
        padding: '0 6px',
        height: '20px',
        transition: 'border-color 0.2s'
      }}>
        <Terminal size={10} color="#ea580c" style={{ marginRight: '4px', flexShrink: 0 }} />
        <input
          ref={inputRef}
          type="text"
          value={input}
          onChange={(e) => setInput(e.target.value)}
          onKeyDown={handleKeyDown}
          onFocus={() => {
            if (input.trim().length > 0) {
              setShowSuggestions(true);
            }
          }}
          placeholder="Enter Command..."
          style={{
            flex: 1,
            backgroundColor: 'transparent',
            border: 'none',
            outline: 'none',
            color: '#ffffff',
            fontSize: '10px',
            fontFamily: 'Consolas, "Courier New", monospace',
            padding: '0',
            minWidth: 0
          }}
        />
      </div>

      {/* Autocomplete Suggestions - rendered via portal */}
      {renderSuggestions()}
    </div>
  );
};

export default CommandBar;
