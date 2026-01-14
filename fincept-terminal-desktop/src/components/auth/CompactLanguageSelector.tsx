// File: src/components/auth/CompactLanguageSelector.tsx
// Compact language selector for login/auth screens

import React, { useState } from 'react';
import { useLanguage } from '@/contexts/LanguageContext';
import { Globe, Check, ChevronDown } from 'lucide-react';

export const CompactLanguageSelector: React.FC = () => {
  const { currentLanguage, changeLanguage, availableLanguages } = useLanguage();
  const [isOpen, setIsOpen] = useState(false);

  const currentLang = availableLanguages.find(lang => lang.code === currentLanguage);

  return (
    <div style={{ position: 'relative', width: '100%' }}>
      {/* Selector Button */}
      <button
        type="button"
        onClick={() => setIsOpen(!isOpen)}
        style={{
          width: '100%',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          padding: '8px 12px',
          backgroundColor: '#27272a',
          border: '1px solid #3f3f46',
          borderRadius: '6px',
          cursor: 'pointer',
          transition: 'all 0.2s',
          fontSize: '12px',
          color: '#ffffff',
          fontFamily: 'system-ui, -apple-system, sans-serif'
        }}
        onMouseEnter={(e) => {
          e.currentTarget.style.backgroundColor = '#3f3f46';
          e.currentTarget.style.borderColor = '#52525b';
        }}
        onMouseLeave={(e) => {
          e.currentTarget.style.backgroundColor = '#27272a';
          e.currentTarget.style.borderColor = '#3f3f46';
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Globe size={14} color="#a1a1aa" />
          <span style={{ fontSize: '16px' }}>{currentLang?.flag}</span>
          <span style={{ color: '#d4d4d8', fontWeight: 500 }}>
            {currentLang?.nativeName || 'English'}
          </span>
        </div>
        <ChevronDown
          size={14}
          color="#a1a1aa"
          style={{
            transform: isOpen ? 'rotate(180deg)' : 'rotate(0deg)',
            transition: 'transform 0.2s'
          }}
        />
      </button>

      {/* Dropdown Menu */}
      {isOpen && (
        <>
          {/* Backdrop */}
          <div
            onClick={() => setIsOpen(false)}
            style={{
              position: 'fixed',
              top: 0,
              left: 0,
              right: 0,
              bottom: 0,
              zIndex: 40
            }}
          />

          {/* Dropdown */}
          <div
            style={{
              position: 'absolute',
              top: 'calc(100% + 4px)',
              left: 0,
              right: 0,
              maxHeight: '300px',
              overflowY: 'auto',
              backgroundColor: '#27272a',
              border: '1px solid #3f3f46',
              borderRadius: '6px',
              boxShadow: '0 10px 25px rgba(0, 0, 0, 0.5)',
              zIndex: 50,
              animation: 'slideDown 0.2s ease-out',
              fontFamily: 'system-ui, -apple-system, sans-serif'
            }}
          >
            {/* Scrollbar styling */}
            <style>{`
              @keyframes slideDown {
                from {
                  opacity: 0;
                  transform: translateY(-10px);
                }
                to {
                  opacity: 1;
                  transform: translateY(0);
                }
              }

              div[style*="overflowY: auto"]::-webkit-scrollbar {
                width: 6px;
              }
              div[style*="overflowY: auto"]::-webkit-scrollbar-track {
                background: #18181b;
              }
              div[style*="overflowY: auto"]::-webkit-scrollbar-thumb {
                background: #3f3f46;
                border-radius: 3px;
              }
              div[style*="overflowY: auto"]::-webkit-scrollbar-thumb:hover {
                background: #52525b;
              }
            `}</style>

            {availableLanguages.map((lang) => {
              const isActive = currentLanguage === lang.code;

              return (
                <button
                  key={lang.code}
                  type="button"
                  onClick={() => {
                    changeLanguage(lang.code);
                    setIsOpen(false);
                  }}
                  style={{
                    width: '100%',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'space-between',
                    padding: '10px 12px',
                    backgroundColor: isActive ? '#3f3f46' : 'transparent',
                    border: 'none',
                    borderBottom: '1px solid #3f3f46',
                    cursor: 'pointer',
                    transition: 'background-color 0.15s',
                    fontSize: '12px',
                    textAlign: 'left'
                  }}
                  onMouseEnter={(e) => {
                    if (!isActive) {
                      e.currentTarget.style.backgroundColor = '#3f3f46';
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (!isActive) {
                      e.currentTarget.style.backgroundColor = 'transparent';
                    }
                  }}
                >
                  <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
                    <span style={{
                      fontSize: '18px',
                      width: '24px',
                      filter: isActive ? 'brightness(1.2)' : 'brightness(0.8)'
                    }}>
                      {lang.flag}
                    </span>
                    <div>
                      <div style={{
                        color: isActive ? '#ffffff' : '#d4d4d8',
                        fontWeight: isActive ? 600 : 500,
                        marginBottom: '2px'
                      }}>
                        {lang.nativeName}
                      </div>
                      <div style={{
                        color: '#a1a1aa',
                        fontSize: '10px'
                      }}>
                        {lang.name}
                      </div>
                    </div>
                  </div>

                  {isActive && (
                    <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                      <Check size={14} color="#22c55e" />
                      <div style={{
                        width: '6px',
                        height: '6px',
                        borderRadius: '50%',
                        backgroundColor: '#22c55e',
                        boxShadow: '0 0 8px #22c55e'
                      }} />
                    </div>
                  )}
                </button>
              );
            })}
          </div>
        </>
      )}
    </div>
  );
};
