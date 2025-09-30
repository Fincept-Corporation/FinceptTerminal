import React, { useState } from 'react';
import { Book, Search, Code, ChevronRight } from 'lucide-react';
import { DOC_SECTIONS } from './content';
import { DocSubsection } from './types';
import { COLORS } from './constants';

export default function DocsTab() {
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
        .docs-scroll::-webkit-scrollbar { width: 8px; }
        .docs-scroll::-webkit-scrollbar-track { background: #1a1a1a; }
        .docs-scroll::-webkit-scrollbar-thumb { background: #404040; border-radius: 4px; }
        .docs-scroll::-webkit-scrollbar-thumb:hover { background: #525252; }

        .docs-content h1 { color: ${COLORS.ORANGE}; font-size: 24px; margin: 24px 0 16px 0; }
        .docs-content h2 { color: ${COLORS.CYAN}; font-size: 18px; margin: 20px 0 12px 0; }
        .docs-content p { line-height: 1.8; margin: 12px 0; }
        .docs-content ul { margin: 12px 0; padding-left: 24px; }
        .docs-content li { margin: 6px 0; line-height: 1.6; }
      `}</style>

      {/* Header */}
      <div
        style={{
          backgroundColor: COLORS.PANEL_BG,
          borderBottom: `2px solid ${COLORS.GRAY}`,
          padding: '8px 16px',
          flexShrink: 0
        }}
      >
        <div
          style={{
            display: 'flex',
            alignItems: 'center',
            gap: '12px',
            flexWrap: 'wrap'
          }}
        >
          <Book size={20} style={{ color: COLORS.ORANGE }} />
          <span style={{ color: COLORS.ORANGE, fontWeight: 'bold', fontSize: '16px' }}>DOCUMENTATION</span>
          <span style={{ color: COLORS.GRAY }}>|</span>
          <span style={{ color: COLORS.CYAN, fontSize: '12px' }}>Complete Guide & API Reference</span>
        </div>
      </div>

      {/* Search Bar */}
      <div
        style={{
          backgroundColor: COLORS.PANEL_BG,
          borderBottom: `1px solid ${COLORS.GRAY}`,
          padding: '12px 16px',
          flexShrink: 0
        }}
      >
        <div
          style={{
            display: 'flex',
            alignItems: 'center',
            gap: '8px',
            backgroundColor: COLORS.DARK_BG,
            border: `1px solid ${COLORS.GRAY}`,
            padding: '8px 12px'
          }}
        >
          <Search size={16} style={{ color: COLORS.GRAY, flexShrink: 0 }} />
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
              fontSize: '13px',
              fontFamily: 'Consolas, monospace',
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
            width: '280px',
            borderRight: `1px solid ${COLORS.GRAY}`,
            backgroundColor: COLORS.PANEL_BG,
            overflowY: 'auto',
            padding: '12px',
            flexShrink: 0
          }}
        >
          {filteredDocs.map((section) => (
            <div key={section.id} style={{ marginBottom: '8px' }}>
              <div
                onClick={() => toggleSection(section.id)}
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '8px',
                  padding: '8px',
                  backgroundColor: COLORS.DARK_BG,
                  cursor: 'pointer',
                  border: `1px solid ${COLORS.GRAY}`
                }}
              >
                <section.icon size={14} style={{ color: COLORS.ORANGE }} />
                <span style={{ flex: 1, fontSize: '12px', fontWeight: 'bold', color: COLORS.WHITE }}>
                  {section.title}
                </span>
                <ChevronRight
                  size={14}
                  style={{
                    color: COLORS.GRAY,
                    transform: expandedSections.includes(section.id) ? 'rotate(90deg)' : 'rotate(0deg)',
                    transition: 'transform 0.2s'
                  }}
                />
              </div>
              {expandedSections.includes(section.id) && (
                <div style={{ marginTop: '4px', marginLeft: '12px' }}>
                  {section.subsections.map((sub) => (
                    <div
                      key={sub.id}
                      onClick={() => setSelectedSection(sub.id)}
                      style={{
                        padding: '8px 12px',
                        fontSize: '11px',
                        color: selectedSection === sub.id ? COLORS.ORANGE : COLORS.GRAY,
                        backgroundColor: selectedSection === sub.id ? COLORS.DARK_BG : 'transparent',
                        cursor: 'pointer',
                        borderLeft: `2px solid ${selectedSection === sub.id ? COLORS.ORANGE : 'transparent'}`
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
        <div className="docs-scroll" style={{ flex: 1, overflowY: 'auto', padding: '24px', backgroundColor: '#050505' }}>
          {activeSubsection ? (
            <div style={{ maxWidth: '900px', margin: '0 auto' }}>
              <h1
                style={{
                  color: COLORS.ORANGE,
                  fontSize: '28px',
                  marginBottom: '16px',
                  borderBottom: `2px solid ${COLORS.ORANGE}`,
                  paddingBottom: '12px'
                }}
              >
                {activeSubsection.title}
              </h1>
              <div className="docs-content" style={{ fontSize: '14px', lineHeight: '1.8', color: COLORS.WHITE }}>
                {activeSubsection.content.split('\n\n').map((para, idx) => (
                  <p key={idx} style={{ marginBottom: '16px', whiteSpace: 'pre-wrap' }}>
                    {para.split('\n').map((line, lineIdx) => {
                      if (line.startsWith('**') && line.endsWith('**')) {
                        return (
                          <div
                            key={lineIdx}
                            style={{ color: COLORS.CYAN, fontWeight: 'bold', marginTop: '16px', marginBottom: '8px' }}
                          >
                            {line.replace(/\*\*/g, '')}
                          </div>
                        );
                      }
                      if (line.startsWith('•')) {
                        return (
                          <li key={lineIdx} style={{ marginLeft: '20px', marginBottom: '4px' }}>
                            {line.substring(1).trim()}
                          </li>
                        );
                      }
                      return (
                        <span key={lineIdx}>
                          {line}
                          <br />
                        </span>
                      );
                    })}
                  </p>
                ))}
              </div>

              {activeSubsection.codeExample && (
                <div style={{ marginTop: '24px' }}>
                  <div
                    style={{
                      color: COLORS.ORANGE,
                      fontSize: '14px',
                      fontWeight: 'bold',
                      marginBottom: '8px',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '8px'
                    }}
                  >
                    <Code size={16} />
                    CODE EXAMPLE
                  </div>
                  <pre
                    style={{
                      backgroundColor: COLORS.CODE_BG,
                      border: `1px solid ${COLORS.GRAY}`,
                      padding: '16px',
                      borderRadius: '4px',
                      overflowX: 'auto',
                      fontSize: '13px',
                      lineHeight: '1.6',
                      color: COLORS.GREEN
                    }}
                  >
                    <code>{activeSubsection.codeExample}</code>
                  </pre>
                </div>
              )}
            </div>
          ) : (
            <div style={{ textAlign: 'center', paddingTop: '60px', color: COLORS.GRAY }}>
              <Book size={48} style={{ marginBottom: '16px', opacity: 0.5 }} />
              <div style={{ fontSize: '16px' }}>Select a topic from the sidebar</div>
            </div>
          )}
        </div>
      </div>

      {/* Footer */}
      <div
        style={{
          borderTop: `2px solid ${COLORS.ORANGE}`,
          backgroundColor: COLORS.PANEL_BG,
          padding: '8px 16px',
          fontSize: '11px',
          flexShrink: 0
        }}
      >
        <div
          style={{
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            flexWrap: 'wrap',
            gap: '12px'
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', gap: '16px', flexWrap: 'wrap' }}>
            <span style={{ color: COLORS.ORANGE, fontWeight: 'bold' }}>FINCEPT DOCS v3.0</span>
            <span style={{ color: COLORS.GRAY }}>|</span>
            <span style={{ color: COLORS.CYAN }}>FinScript Language Reference</span>
            <span style={{ color: COLORS.GRAY }}>|</span>
            <span style={{ color: COLORS.BLUE }}>API Documentation</span>
          </div>
          <div style={{ color: COLORS.GRAY }}>
            {activeSubsection ? activeSubsection.title : 'Browse Documentation'}
          </div>
        </div>
      </div>
    </div>
  );
}
