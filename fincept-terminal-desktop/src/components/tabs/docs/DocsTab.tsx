import React, { useState, useMemo } from 'react';
import { Book, Search, Code, ChevronRight, Hash, FileText } from 'lucide-react';
import { DOC_SECTIONS } from './content';
import { DocSubsection } from './types';
import { getColors } from './constants';
import { useTerminalTheme } from '@/contexts/ThemeContext';

export default function DocsTab() {
  const { colors: themeColors, fontSize, fontFamily, fontWeight, fontStyle } = useTerminalTheme();
  const COLORS = getColors();
  const [selectedSection, setSelectedSection] = useState('getting-started');
  const [searchQuery, setSearchQuery] = useState('');
  const [expandedSections, setExpandedSections] = useState<string[]>(['finscript']);

  const toggleSection = (sectionId: string) => {
    setExpandedSections((prev) =>
      prev.includes(sectionId) ? prev.filter((id) => id !== sectionId) : [...prev, sectionId]
    );
  };

  const filteredDocs = DOC_SECTIONS.map((section) => ({
    ...section,
    subsections: section.subsections.filter(
      (sub) =>
        searchQuery === '' ||
        sub.title.toLowerCase().includes(searchQuery.toLowerCase()) ||
        sub.content.toLowerCase().includes(searchQuery.toLowerCase())
    )
  })).filter((section) => section.subsections.length > 0);

  const activeSubsection: DocSubsection | undefined = DOC_SECTIONS.flatMap((s) => s.subsections).find(
    (sub) => sub.id === selectedSection
  );

  return (
    <div
      style={{
        height: '100%',
        backgroundColor: COLORS.DARK_BG,
        color: COLORS.WHITE,
        fontFamily: 'Consolas, monospace',
        overflow: 'hidden',
        display: 'flex',
        flexDirection: 'column',
        fontSize: '12px'
      }}
    >
      <style>{`
        .docs-scroll::-webkit-scrollbar { width: 10px; }
        .docs-scroll::-webkit-scrollbar-track { background: #0a0a0a; }
        .docs-scroll::-webkit-scrollbar-thumb { background: #404040; border-radius: 5px; }
        .docs-scroll::-webkit-scrollbar-thumb:hover { background: #525252; }

        .docs-content h1 {
          color: ${COLORS.ORANGE};
          font-size: 32px;
          font-weight: 700;
          margin: 0 0 24px 0;
          font-family: 'Inter', 'Segoe UI', system-ui, -apple-system, sans-serif;
          letter-spacing: -0.5px;
        }
        .docs-content h2 {
          color: ${COLORS.CYAN};
          font-size: 22px;
          font-weight: 600;
          margin: 32px 0 16px 0;
          font-family: 'Inter', 'Segoe UI', system-ui, -apple-system, sans-serif;
          display: flex;
          align-items: center;
          gap: 8px;
        }
        .docs-content h3 {
          color: ${COLORS.WHITE};
          font-size: 18px;
          font-weight: 600;
          margin: 24px 0 12px 0;
          font-family: 'Inter', 'Segoe UI', system-ui, -apple-system, sans-serif;
        }
        .docs-content p {
          line-height: 1.8;
          margin: 0 0 16px 0;
          font-size: 15px;
          font-family: 'Inter', 'Segoe UI', system-ui, -apple-system, sans-serif;
          color: rgba(255, 255, 255, 0.9);
        }
        .docs-content ul {
          margin: 16px 0;
          padding-left: 0;
          list-style: none;
        }
        .docs-content li {
          margin: 8px 0;
          padding-left: 28px;
          line-height: 1.7;
          font-size: 15px;
          font-family: 'Inter', 'Segoe UI', system-ui, -apple-system, sans-serif;
          position: relative;
          color: rgba(255, 255, 255, 0.85);
        }
        .docs-content li::before {
          content: '•';
          position: absolute;
          left: 12px;
          color: ${COLORS.ORANGE};
          font-weight: bold;
        }
        .docs-content strong {
          color: ${COLORS.CYAN};
          font-weight: 600;
        }
        .docs-content code {
          background: rgba(255, 165, 0, 0.1);
          padding: 2px 6px;
          border-radius: 3px;
          font-family: 'JetBrains Mono', 'Fira Code', 'Consolas', monospace;
          font-size: 14px;
          color: ${COLORS.ORANGE};
        }
        .section-badge {
          display: inline-block;
          padding: 4px 10px;
          background: rgba(255, 165, 0, 0.15);
          border: 1px solid ${COLORS.ORANGE};
          border-radius: 4px;
          font-size: 11px;
          font-weight: 600;
          color: ${COLORS.ORANGE};
          text-transform: uppercase;
          letter-spacing: 0.5px;
          margin-bottom: 16px;
        }
      `}</style>

      {/* Header */}
      <div
        style={{
          backgroundColor: COLORS.PANEL_BG,
          borderBottom: `2px solid ${COLORS.ORANGE}`,
          padding: '12px 20px',
          flexShrink: 0
        }}
      >
        <div
          style={{
            display: 'flex',
            alignItems: 'center',
            gap: '16px',
            flexWrap: 'wrap'
          }}
        >
          <Book size={24} style={{ color: COLORS.ORANGE }} />
          <span style={{
            color: COLORS.ORANGE,
            fontWeight: '700',
            fontSize: '18px',
            fontFamily: "'Inter', 'Segoe UI', system-ui, -apple-system, sans-serif"
          }}>
            DOCUMENTATION
          </span>
          <span style={{ color: COLORS.GRAY }}>|</span>
          <span style={{
            color: COLORS.CYAN,
            fontSize: '14px',
            fontFamily: "'Inter', 'Segoe UI', system-ui, -apple-system, sans-serif"
          }}>
            Complete Guide & API Reference
          </span>
        </div>
      </div>

      {/* Search Bar */}
      <div
        style={{
          backgroundColor: COLORS.PANEL_BG,
          borderBottom: `1px solid ${COLORS.GRAY}`,
          padding: '14px 20px',
          flexShrink: 0
        }}
      >
        <div
          style={{
            display: 'flex',
            alignItems: 'center',
            gap: '10px',
            backgroundColor: COLORS.DARK_BG,
            border: `2px solid ${COLORS.GRAY}`,
            padding: '10px 16px',
            borderRadius: '6px',
            transition: 'border-color 0.2s'
          }}
          onFocus={(e) => e.currentTarget.style.borderColor = COLORS.ORANGE}
          onBlur={(e) => e.currentTarget.style.borderColor = COLORS.GRAY}
        >
          <Search size={18} style={{ color: COLORS.ORANGE, flexShrink: 0 }} />
          <input
            type="text"
            placeholder="Search documentation..."
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            style={{
              flex: 1,
              backgroundColor: 'transparent',
              border: 'none',
              outline: 'none',
              color: COLORS.WHITE,
              fontSize: '14px',
              fontFamily: "'Inter', 'Segoe UI', system-ui, -apple-system, sans-serif",
              minWidth: '150px'
            }}
          />
        </div>
      </div>

      {/* Main Content Area */}
      <div style={{ flex: 1, display: 'flex', minHeight: 0 }}>
        {/* Sidebar Navigation */}
        <div
          className="docs-scroll"
          style={{
            width: '320px',
            borderRight: `2px solid ${COLORS.GRAY}`,
            backgroundColor: COLORS.PANEL_BG,
            overflowY: 'auto',
            padding: '16px',
            flexShrink: 0
          }}
        >
          {filteredDocs.map((section) => (
            <div key={section.id} style={{ marginBottom: '12px' }}>
              <div
                onClick={() => toggleSection(section.id)}
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '10px',
                  padding: '12px 14px',
                  backgroundColor: expandedSections.includes(section.id) ? COLORS.DARK_BG : 'transparent',
                  cursor: 'pointer',
                  border: `1px solid ${expandedSections.includes(section.id) ? COLORS.ORANGE : COLORS.GRAY}`,
                  borderRadius: '6px',
                  transition: 'all 0.2s',
                  fontFamily: "'Inter', 'Segoe UI', system-ui, -apple-system, sans-serif"
                }}
              >
                <section.icon size={16} style={{ color: COLORS.ORANGE }} />
                <span style={{ flex: 1, fontSize: '13px', fontWeight: '600', color: COLORS.WHITE }}>
                  {section.title}
                </span>
                <ChevronRight
                  size={16}
                  style={{
                    color: COLORS.ORANGE,
                    transform: expandedSections.includes(section.id) ? 'rotate(90deg)' : 'rotate(0deg)',
                    transition: 'transform 0.2s'
                  }}
                />
              </div>
              {expandedSections.includes(section.id) && (
                <div style={{ marginTop: '6px', marginLeft: '8px' }}>
                  {section.subsections.map((sub) => (
                    <div
                      key={sub.id}
                      onClick={() => setSelectedSection(sub.id)}
                      style={{
                        padding: '10px 16px',
                        fontSize: '13px',
                        color: selectedSection === sub.id ? COLORS.ORANGE : COLORS.GRAY,
                        backgroundColor: selectedSection === sub.id ? 'rgba(255, 165, 0, 0.1)' : 'transparent',
                        cursor: 'pointer',
                        borderLeft: `3px solid ${selectedSection === sub.id ? COLORS.ORANGE : 'transparent'}`,
                        borderRadius: '4px',
                        marginBottom: '4px',
                        transition: 'all 0.15s',
                        fontFamily: "'Inter', 'Segoe UI', system-ui, -apple-system, sans-serif",
                        fontWeight: selectedSection === sub.id ? '500' : '400'
                      }}
                      onMouseEnter={(e) => {
                        if (selectedSection !== sub.id) {
                          e.currentTarget.style.backgroundColor = 'rgba(255, 255, 255, 0.05)';
                        }
                      }}
                      onMouseLeave={(e) => {
                        if (selectedSection !== sub.id) {
                          e.currentTarget.style.backgroundColor = 'transparent';
                        }
                      }}
                    >
                      {sub.title}
                    </div>
                  ))}
                </div>
              )}
            </div>
          ))}
        </div>

        {/* Content Area */}
        <div className="docs-scroll" style={{ flex: 1, overflowY: 'auto', padding: '40px 60px', backgroundColor: '#0a0a0a' }}>
          {activeSubsection ? (
            <div style={{ maxWidth: '100%', width: '100%' }}>
              {/* Section Category Badge */}
              <div className="section-badge">
                {DOC_SECTIONS.find(s => s.subsections.some(sub => sub.id === activeSubsection.id))?.title || 'Documentation'}
              </div>

              <h1 className="docs-content">
                {activeSubsection.title}
              </h1>

              <div className="docs-content" style={{ fontSize: '15px', lineHeight: '1.8', color: COLORS.WHITE }}>
                {activeSubsection.content.split('\n\n').map((para, idx) => {
                  const lines = para.split('\n');
                  const isListSection = lines.some(line => line.startsWith('•'));

                  if (isListSection) {
                    // Find the header line (if exists) and the list items
                    const headerLine = lines.find(line => line.startsWith('**') && line.endsWith('**'));
                    const listItems = lines.filter(line => line.startsWith('•'));

                    return (
                      <div key={idx} style={{ marginBottom: '24px' }}>
                        {headerLine && (
                          <h3 style={{
                            color: COLORS.CYAN,
                            fontWeight: '600',
                            marginBottom: '12px',
                            fontSize: '18px',
                            fontFamily: "'Inter', 'Segoe UI', system-ui, -apple-system, sans-serif"
                          }}>
                            {headerLine.replace(/\*\*/g, '')}
                          </h3>
                        )}
                        <ul style={{ margin: '0', paddingLeft: '0', listStyle: 'none' }}>
                          {listItems.map((item, itemIdx) => (
                            <li key={itemIdx}>
                              {item.substring(1).trim()}
                            </li>
                          ))}
                        </ul>
                      </div>
                    );
                  } else {
                    // Regular paragraph
                    return (
                      <p key={idx} style={{
                        marginBottom: '20px',
                        fontFamily: "'Inter', 'Segoe UI', system-ui, -apple-system, sans-serif",
                        fontSize: '15px',
                        lineHeight: '1.8'
                      }}>
                        {para}
                      </p>
                    );
                  }
                })}
              </div>

              {activeSubsection.codeExample && (
                <div style={{ marginTop: '40px' }}>
                  <div
                    style={{
                      color: COLORS.ORANGE,
                      fontSize: '16px',
                      fontWeight: '600',
                      marginBottom: '12px',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '10px',
                      fontFamily: "'Inter', 'Segoe UI', system-ui, -apple-system, sans-serif"
                    }}
                  >
                    <Code size={18} />
                    Code Example
                  </div>
                  <pre
                    style={{
                      backgroundColor: '#0d0d0d',
                      border: `2px solid ${COLORS.GRAY}`,
                      padding: '24px',
                      borderRadius: '8px',
                      overflowX: 'auto',
                      fontSize: '14px',
                      lineHeight: '1.7',
                      color: COLORS.GREEN,
                      fontFamily: "'JetBrains Mono', 'Fira Code', 'Consolas', monospace",
                      boxShadow: '0 4px 12px rgba(0, 0, 0, 0.3)'
                    }}
                  >
                    <code>{activeSubsection.codeExample}</code>
                  </pre>
                </div>
              )}
            </div>
          ) : (
            <div style={{
              textAlign: 'center',
              paddingTop: '100px',
              color: COLORS.GRAY,
              maxWidth: '600px',
              margin: '0 auto'
            }}>
              <Book size={64} style={{ marginBottom: '24px', opacity: 0.3, color: COLORS.ORANGE }} />
              <div style={{
                fontSize: '24px',
                fontWeight: '600',
                marginBottom: '12px',
                color: COLORS.WHITE,
                fontFamily: "'Inter', 'Segoe UI', system-ui, -apple-system, sans-serif"
              }}>
                Welcome to Fincept Terminal Documentation
              </div>
              <div style={{
                fontSize: '15px',
                lineHeight: '1.6',
                marginBottom: '32px',
                fontFamily: "'Inter', 'Segoe UI', system-ui, -apple-system, sans-serif"
              }}>
                Select a topic from the sidebar to get started with the comprehensive guide and API reference.
              </div>
              <div style={{
                display: 'grid',
                gridTemplateColumns: 'repeat(2, 1fr)',
                gap: '16px',
                textAlign: 'left'
              }}>
                {DOC_SECTIONS.slice(0, 4).map((section) => (
                  <div
                    key={section.id}
                    onClick={() => {
                      toggleSection(section.id);
                      setSelectedSection(section.subsections[0]?.id);
                    }}
                    style={{
                      padding: '20px',
                      backgroundColor: COLORS.PANEL_BG,
                      border: `1px solid ${COLORS.GRAY}`,
                      borderRadius: '8px',
                      cursor: 'pointer',
                      transition: 'all 0.2s'
                    }}
                    onMouseEnter={(e) => {
                      e.currentTarget.style.borderColor = COLORS.ORANGE;
                      e.currentTarget.style.backgroundColor = COLORS.DARK_BG;
                    }}
                    onMouseLeave={(e) => {
                      e.currentTarget.style.borderColor = COLORS.GRAY;
                      e.currentTarget.style.backgroundColor = COLORS.PANEL_BG;
                    }}
                  >
                    <section.icon size={24} style={{ color: COLORS.ORANGE, marginBottom: '12px' }} />
                    <div style={{
                      fontSize: '16px',
                      fontWeight: '600',
                      color: COLORS.WHITE,
                      marginBottom: '8px',
                      fontFamily: "'Inter', 'Segoe UI', system-ui, -apple-system, sans-serif"
                    }}>
                      {section.title}
                    </div>
                    <div style={{
                      fontSize: '13px',
                      color: COLORS.GRAY,
                      fontFamily: "'Inter', 'Segoe UI', system-ui, -apple-system, sans-serif"
                    }}>
                      {section.subsections.length} topics
                    </div>
                  </div>
                ))}
              </div>
            </div>
          )}
        </div>
      </div>

      {/* Footer */}
      <div
        style={{
          borderTop: `2px solid ${COLORS.ORANGE}`,
          backgroundColor: COLORS.PANEL_BG,
          padding: '10px 20px',
          fontSize: '12px',
          flexShrink: 0
        }}
      >
        <div
          style={{
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            flexWrap: 'wrap',
            gap: '16px'
          }}
        >
          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: '16px',
            flexWrap: 'wrap',
            fontFamily: "'Inter', 'Segoe UI', system-ui, -apple-system, sans-serif"
          }}>
            <span style={{ color: COLORS.ORANGE, fontWeight: '600' }}>FINCEPT DOCS v3.0</span>
            <span style={{ color: COLORS.GRAY }}>|</span>
            <span style={{ color: COLORS.CYAN }}>FinScript Language Reference</span>
            <span style={{ color: COLORS.GRAY }}>|</span>
            <span style={{ color: COLORS.BLUE }}>API Documentation</span>
          </div>
          <div style={{
            color: COLORS.GRAY,
            fontFamily: "'Inter', 'Segoe UI', system-ui, -apple-system, sans-serif"
          }}>
            {activeSubsection ? `📖 ${activeSubsection.title}` : 'Browse Documentation'}
          </div>
        </div>
      </div>
    </div>
  );
}
