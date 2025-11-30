import React, { useState, useEffect } from 'react';
import { Book, Search, Code, ChevronRight, FileText, HelpCircle, RefreshCw } from 'lucide-react';
import { DOC_SECTIONS } from './content';
import { DocSubsection } from './types';
import { getColors } from './constants';
import { useTerminalTheme } from '@/contexts/ThemeContext';

export default function DocsTab() {
  const { colors: themeColors } = useTerminalTheme();
  const COLORS = getColors();
  const [currentTime, setCurrentTime] = useState(new Date());
  const [selectedSection, setSelectedSection] = useState('getting-started');
  const [searchQuery, setSearchQuery] = useState('');
  const [expandedSections, setExpandedSections] = useState<string[]>(['getting-started', 'finscript']);

  // Update clock
  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

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

  // Get total count of doc sections
  const totalSections = DOC_SECTIONS.length;
  const totalTopics = DOC_SECTIONS.reduce((acc, section) => acc + section.subsections.length, 0);

  return (
    <div
      style={{
        height: '100%',
        backgroundColor: COLORS.DARK_BG,
        color: COLORS.WHITE,
        fontFamily: 'Consolas, monospace',
        overflow: 'hidden',
        display: 'flex',
        flexDirection: 'column'
      }}
    >
      <style>{`
        *::-webkit-scrollbar {
          width: 8px;
          height: 8px;
        }
        *::-webkit-scrollbar-track {
          background: #1a1a1a;
        }
        *::-webkit-scrollbar-thumb {
          background: #404040;
          border-radius: 4px;
        }
        *::-webkit-scrollbar-thumb:hover {
          background: #525252;
        }

        .docs-content h1 {
          color: ${COLORS.ORANGE};
          font-size: 18px;
          font-weight: bold;
          margin: 0 0 16px 0;
          font-family: 'Consolas', monospace;
          letter-spacing: 0.5px;
          text-transform: uppercase;
        }
        .docs-content h2 {
          color: ${COLORS.CYAN};
          font-size: 14px;
          font-weight: bold;
          margin: 16px 0 8px 0;
          font-family: 'Consolas', monospace;
          text-transform: uppercase;
        }
        .docs-content h3 {
          color: ${COLORS.WHITE};
          font-size: 12px;
          font-weight: bold;
          margin: 12px 0 6px 0;
          font-family: 'Consolas', monospace;
        }
        .docs-content p {
          line-height: 1.6;
          margin: 0 0 12px 0;
          font-size: 11px;
          font-family: 'Consolas', monospace;
          color: ${COLORS.WHITE};
        }
        .docs-content ul {
          margin: 12px 0;
          padding-left: 0;
          list-style: none;
        }
        .docs-content li {
          margin: 6px 0;
          padding-left: 20px;
          line-height: 1.5;
          font-size: 11px;
          font-family: 'Consolas', monospace;
          position: relative;
          color: ${COLORS.WHITE};
        }
        .docs-content li::before {
          content: '■';
          position: absolute;
          left: 8px;
          color: ${COLORS.ORANGE};
          font-size: 8px;
        }
        .docs-content strong {
          color: ${COLORS.CYAN};
          font-weight: bold;
        }
        .docs-content code {
          background: rgba(255, 165, 0, 0.1);
          padding: 2px 4px;
          border-radius: 2px;
          font-family: 'Consolas', monospace;
          font-size: 11px;
          color: ${COLORS.ORANGE};
        }
      `}</style>

      {/* Header Bar */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        padding: '8px 12px',
        backgroundColor: COLORS.PANEL_BG,
        borderBottom: `1px solid ${COLORS.GRAY}`,
        fontSize: '13px',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span style={{ color: COLORS.ORANGE, fontWeight: 'bold' }}>FINCEPT</span>
          <span style={{ color: COLORS.WHITE }}>DOCUMENTATION TERMINAL</span>
          <span style={{ color: COLORS.GRAY }}>|</span>
          <span style={{ color: COLORS.WHITE, fontSize: '11px' }}>
            {currentTime.toISOString().replace('T', ' ').substring(0, 19)}
          </span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span style={{ color: COLORS.GRAY, fontSize: '11px' }}>SECTIONS:</span>
          <span style={{ color: COLORS.CYAN, fontSize: '11px', fontWeight: 'bold' }}>{totalSections}</span>
          <span style={{ color: COLORS.GRAY }}>|</span>
          <span style={{ color: COLORS.GREEN, fontSize: '11px' }}>TOPICS:</span>
          <span style={{ color: COLORS.GREEN, fontSize: '11px', fontWeight: 'bold' }}>{totalTopics}</span>
        </div>
      </div>

      {/* Control Panel */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        padding: '8px 12px',
        backgroundColor: COLORS.PANEL_BG,
        borderBottom: `1px solid ${COLORS.GRAY}`,
        fontSize: '12px',
        gap: '8px',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', flex: 1 }}>
          <Search size={14} style={{ color: COLORS.ORANGE, flexShrink: 0 }} />
          <input
            type="text"
            placeholder="Search documentation..."
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            style={{
              flex: 1,
              backgroundColor: COLORS.DARK_BG,
              border: `1px solid ${COLORS.GRAY}`,
              color: COLORS.WHITE,
              fontSize: '11px',
              fontFamily: 'Consolas, monospace',
              padding: '4px 8px',
              outline: 'none',
              maxWidth: '400px'
            }}
          />
          {searchQuery && (
            <button
              onClick={() => setSearchQuery('')}
              style={{
                backgroundColor: COLORS.GRAY,
                color: 'black',
                border: 'none',
                padding: '4px 8px',
                fontSize: '10px',
                fontWeight: 'bold',
                cursor: 'pointer'
              }}
            >
              CLEAR
            </button>
          )}
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span style={{ color: COLORS.GREEN, fontSize: '14px' }}>●</span>
          <span style={{ color: COLORS.GREEN, fontSize: '11px', fontWeight: 'bold' }}>
            READY
          </span>
        </div>
      </div>

      {/* Main Content Area */}
      <div style={{ flex: 1, display: 'flex', minHeight: 0, overflow: 'hidden' }}>
        {/* Sidebar Navigation */}
        <div style={{
          width: '280px',
          borderRight: `1px solid ${COLORS.GRAY}`,
          backgroundColor: COLORS.PANEL_BG,
          overflowY: 'auto',
          flexShrink: 0
        }}>
          {/* Section Header */}
          <div style={{
            color: COLORS.ORANGE,
            fontSize: '12px',
            fontWeight: 'bold',
            padding: '12px',
            borderBottom: `1px solid ${COLORS.GRAY}`,
            letterSpacing: '0.5px'
          }}>
            NAVIGATION
          </div>

          {/* Navigation Tree */}
          {filteredDocs.map((section) => (
            <div key={section.id} style={{ borderBottom: `1px solid ${COLORS.GRAY}` }}>
              <div
                onClick={() => toggleSection(section.id)}
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '8px',
                  padding: '8px 12px',
                  backgroundColor: expandedSections.includes(section.id) ? COLORS.DARK_BG : 'transparent',
                  cursor: 'pointer',
                  borderLeft: `4px solid ${expandedSections.includes(section.id) ? COLORS.ORANGE : 'transparent'}`,
                  transition: 'all 0.2s'
                }}
                onMouseEnter={(e) => {
                  if (!expandedSections.includes(section.id)) {
                    e.currentTarget.style.backgroundColor = '#1a1a1a';
                  }
                }}
                onMouseLeave={(e) => {
                  if (!expandedSections.includes(section.id)) {
                    e.currentTarget.style.backgroundColor = 'transparent';
                  }
                }}
              >
                <ChevronRight
                  size={12}
                  style={{
                    color: COLORS.ORANGE,
                    transform: expandedSections.includes(section.id) ? 'rotate(90deg)' : 'rotate(0deg)',
                    transition: 'transform 0.2s'
                  }}
                />
                <section.icon size={14} style={{ color: COLORS.ORANGE }} />
                <span style={{ flex: 1, fontSize: '11px', fontWeight: 'bold', color: COLORS.WHITE }}>
                  {section.title.toUpperCase()}
                </span>
                <span style={{ fontSize: '9px', color: COLORS.GRAY }}>
                  ({section.subsections.length})
                </span>
              </div>
              {expandedSections.includes(section.id) && (
                <div style={{ backgroundColor: COLORS.DARK_BG }}>
                  {section.subsections.map((sub) => (
                    <div
                      key={sub.id}
                      onClick={() => setSelectedSection(sub.id)}
                      style={{
                        padding: '6px 12px 6px 40px',
                        fontSize: '10px',
                        color: selectedSection === sub.id ? COLORS.ORANGE : COLORS.GRAY,
                        backgroundColor: selectedSection === sub.id ? 'rgba(255, 165, 0, 0.1)' : 'transparent',
                        cursor: 'pointer',
                        borderLeft: `4px solid ${selectedSection === sub.id ? COLORS.ORANGE : 'transparent'}`,
                        transition: 'all 0.15s'
                      }}
                      onMouseEnter={(e) => {
                        if (selectedSection !== sub.id) {
                          e.currentTarget.style.backgroundColor = 'rgba(255, 255, 255, 0.05)';
                          e.currentTarget.style.color = COLORS.WHITE;
                        }
                      }}
                      onMouseLeave={(e) => {
                        if (selectedSection !== sub.id) {
                          e.currentTarget.style.backgroundColor = 'transparent';
                          e.currentTarget.style.color = COLORS.GRAY;
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
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
          {activeSubsection && (
            <>
              {/* Content Header */}
              <div style={{
                backgroundColor: COLORS.PANEL_BG,
                borderBottom: `1px solid ${COLORS.GRAY}`,
                padding: '8px 12px',
                flexShrink: 0
              }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                  <FileText size={14} style={{ color: COLORS.CYAN }} />
                  <span style={{ color: COLORS.CYAN, fontSize: '12px', fontWeight: 'bold' }}>
                    {activeSubsection.title.toUpperCase()}
                  </span>
                  <span style={{ color: COLORS.GRAY }}>|</span>
                  <span style={{ color: COLORS.GRAY, fontSize: '10px' }}>
                    {DOC_SECTIONS.find(s => s.subsections.some(sub => sub.id === activeSubsection.id))?.title || 'Documentation'}
                  </span>
                </div>
              </div>

              {/* Content Body */}
              <div style={{ flex: 1, overflowY: 'auto', padding: '20px', backgroundColor: '#0a0a0a' }}>
                <div style={{ maxWidth: '900px' }}>
                  {/* Section Badge */}
                  <div style={{
                    display: 'inline-block',
                    padding: '4px 8px',
                    backgroundColor: 'rgba(255, 165, 0, 0.15)',
                    border: `1px solid ${COLORS.ORANGE}`,
                    fontSize: '9px',
                    fontWeight: 'bold',
                    color: COLORS.ORANGE,
                    marginBottom: '12px',
                    letterSpacing: '0.5px'
                  }}>
                    {DOC_SECTIONS.find(s => s.subsections.some(sub => sub.id === activeSubsection.id))?.title.toUpperCase() || 'DOCUMENTATION'}
                  </div>

                  <h1 className="docs-content">
                    {activeSubsection.title}
                  </h1>

                  <div className="docs-content" style={{ fontSize: '11px', lineHeight: '1.6', color: COLORS.WHITE }}>
                    {activeSubsection.content.split('\n\n').map((para, idx) => {
                      const lines = para.split('\n');
                      const isListSection = lines.some(line => line.startsWith('•'));

                      if (isListSection) {
                        const headerLine = lines.find(line => line.startsWith('**') && line.endsWith('**'));
                        const listItems = lines.filter(line => line.startsWith('•'));

                        return (
                          <div key={idx} style={{ marginBottom: '16px' }}>
                            {headerLine && (
                              <h3 style={{
                                color: COLORS.CYAN,
                                fontWeight: 'bold',
                                marginBottom: '8px',
                                fontSize: '12px',
                                fontFamily: 'Consolas, monospace'
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
                        return (
                          <p key={idx} style={{
                            marginBottom: '12px',
                            fontFamily: 'Consolas, monospace',
                            fontSize: '11px',
                            lineHeight: '1.6'
                          }}>
                            {para}
                          </p>
                        );
                      }
                    })}
                  </div>

                  {activeSubsection.codeExample && (
                    <div style={{
                      marginTop: '24px',
                      backgroundColor: COLORS.PANEL_BG,
                      border: `1px solid ${COLORS.GRAY}`,
                      borderLeft: `4px solid ${COLORS.GREEN}`,
                      padding: '16px'
                    }}>
                      <div style={{
                        color: COLORS.ORANGE,
                        fontSize: '11px',
                        fontWeight: 'bold',
                        marginBottom: '8px',
                        display: 'flex',
                        alignItems: 'center',
                        gap: '8px',
                        fontFamily: 'Consolas, monospace'
                      }}>
                        <Code size={14} />
                        CODE EXAMPLE
                      </div>
                      <pre style={{
                        backgroundColor: '#0d0d0d',
                        border: `1px solid ${COLORS.GRAY}`,
                        padding: '12px',
                        overflowX: 'auto',
                        fontSize: '11px',
                        lineHeight: '1.5',
                        color: COLORS.GREEN,
                        fontFamily: 'Consolas, monospace',
                        margin: 0
                      }}>
                        <code>{activeSubsection.codeExample}</code>
                      </pre>
                    </div>
                  )}
                </div>
              </div>
            </>
          )}

          {!activeSubsection && (
            <div style={{
              flex: 1,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              backgroundColor: '#0a0a0a',
              padding: '40px'
            }}>
              <div style={{
                textAlign: 'center',
                maxWidth: '600px'
              }}>
                <HelpCircle size={64} style={{ marginBottom: '24px', opacity: 0.3, color: COLORS.ORANGE }} />
                <div style={{
                  fontSize: '16px',
                  fontWeight: 'bold',
                  marginBottom: '12px',
                  color: COLORS.ORANGE,
                  fontFamily: 'Consolas, monospace',
                  letterSpacing: '0.5px'
                }}>
                  FINCEPT TERMINAL DOCUMENTATION
                </div>
                <div style={{
                  fontSize: '11px',
                  lineHeight: '1.6',
                  marginBottom: '24px',
                  color: COLORS.GRAY,
                  fontFamily: 'Consolas, monospace'
                }}>
                  Select a topic from the navigation panel to view comprehensive guides and API reference.
                </div>

                {/* Quick Links Grid */}
                <div style={{
                  display: 'grid',
                  gridTemplateColumns: 'repeat(2, 1fr)',
                  gap: '12px',
                  textAlign: 'left',
                  marginTop: '32px'
                }}>
                  {DOC_SECTIONS.slice(0, 4).map((section) => (
                    <div
                      key={section.id}
                      onClick={() => {
                        toggleSection(section.id);
                        setSelectedSection(section.subsections[0]?.id);
                      }}
                      style={{
                        padding: '16px',
                        backgroundColor: COLORS.PANEL_BG,
                        border: `1px solid ${COLORS.GRAY}`,
                        borderLeft: `4px solid ${COLORS.ORANGE}`,
                        cursor: 'pointer',
                        transition: 'all 0.2s'
                      }}
                      onMouseEnter={(e) => {
                        e.currentTarget.style.borderColor = COLORS.ORANGE;
                        e.currentTarget.style.backgroundColor = COLORS.DARK_BG;
                      }}
                      onMouseLeave={(e) => {
                        e.currentTarget.style.borderColor = COLORS.GRAY;
                        e.currentTarget.style.borderLeftColor = COLORS.ORANGE;
                        e.currentTarget.style.backgroundColor = COLORS.PANEL_BG;
                      }}
                    >
                      <section.icon size={20} style={{ color: COLORS.ORANGE, marginBottom: '8px' }} />
                      <div style={{
                        fontSize: '11px',
                        fontWeight: 'bold',
                        color: COLORS.WHITE,
                        marginBottom: '4px',
                        fontFamily: 'Consolas, monospace'
                      }}>
                        {section.title.toUpperCase()}
                      </div>
                      <div style={{
                        fontSize: '10px',
                        color: COLORS.GRAY,
                        fontFamily: 'Consolas, monospace'
                      }}>
                        {section.subsections.length} topics available
                      </div>
                    </div>
                  ))}
                </div>
              </div>
            </div>
          )}
        </div>
      </div>

      {/* Status Bar */}
      <div style={{
        borderTop: `1px solid ${COLORS.GRAY}`,
        backgroundColor: COLORS.PANEL_BG,
        padding: '6px 12px',
        fontSize: '10px',
        color: COLORS.GRAY,
        flexShrink: 0,
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <span>Fincept Terminal Documentation v3.0 | Complete API Reference & Guides</span>
          <span>Sections: {totalSections} | Topics: {totalTopics}</span>
        </div>
        <span>{activeSubsection ? `Current: ${activeSubsection.title}` : 'Select a topic to begin'}</span>
      </div>
    </div>
  );
}
