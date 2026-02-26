import React, { useState, useMemo } from 'react';
import { BookOpen, ChevronDown, ChevronRight, Search, X, ExternalLink } from 'lucide-react';
import { finscriptLanguageDoc } from '../docs/content/finscriptLanguage';
import { useNavigation } from '@/contexts/NavigationContext';

const F = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  BORDER: '#2A2A2A',
  CYAN: '#00E5FF',
};

const FONT = '"IBM Plex Mono", "Consolas", monospace';

interface FinScriptDocsPanelProps {
  onClose: () => void;
  onInsertExample?: (code: string) => void;
}

export const FinScriptDocsPanel: React.FC<FinScriptDocsPanelProps> = ({ onClose, onInsertExample }) => {
  const { setActiveTab } = useNavigation();
  const [searchQuery, setSearchQuery] = useState('');
  const [expandedSections, setExpandedSections] = useState<Record<string, boolean>>({});
  const [activeSubsection, setActiveSubsection] = useState<string>('fs-overview');

  const subsections = finscriptLanguageDoc.subsections;

  const filteredSections = useMemo(() => {
    if (!searchQuery.trim()) return subsections;
    const q = searchQuery.toLowerCase();
    return subsections.filter(sub =>
      sub.title.toLowerCase().includes(q) ||
      sub.content.toLowerCase().includes(q) ||
      sub.codeExample?.toLowerCase().includes(q)
    );
  }, [searchQuery, subsections]);

  const activeContent = subsections.find(s => s.id === activeSubsection);

  // Format content with markdown-like rendering
  const renderContent = (content: string) => {
    const lines = content.split('\n');
    const elements: React.ReactElement[] = [];
    let i = 0;

    while (i < lines.length) {
      const line = lines[i];

      if (line.trim() === '') {
        elements.push(<div key={i} style={{ height: '6px' }} />);
        i++;
        continue;
      }

      if (line.trimStart().startsWith('•') || line.trimStart().startsWith('-')) {
        const bulletItems: string[] = [];
        while (i < lines.length && (lines[i].trimStart().startsWith('•') || lines[i].trimStart().startsWith('-'))) {
          bulletItems.push(lines[i].trimStart().replace(/^[•-]\s*/, ''));
          i++;
        }
        elements.push(
          <ul key={`ul-${i}`} style={{ margin: '6px 0', padding: 0, listStyle: 'none' }}>
            {bulletItems.map((item, idx) => (
              <li key={idx} style={{
                margin: '3px 0',
                paddingLeft: '14px',
                fontSize: '10px',
                lineHeight: '1.5',
                position: 'relative',
                color: F.WHITE
              }}>
                <span style={{ position: 'absolute', left: '3px', color: F.ORANGE, fontSize: '6px', top: '4px' }}>&#9632;</span>
                <span dangerouslySetInnerHTML={{ __html: formatInlineText(item) }} />
              </li>
            ))}
          </ul>
        );
        continue;
      }

      elements.push(
        <p key={i} style={{ margin: '0 0 6px 0', fontSize: '10px', lineHeight: '1.6', color: F.WHITE }}>
          <span dangerouslySetInnerHTML={{ __html: formatInlineText(line) }} />
        </p>
      );
      i++;
    }

    return elements;
  };

  const formatInlineText = (text: string): string => {
    let result = text;
    // Bold **text**
    result = result.replace(/\*\*(.+?)\*\*/g, `<span style="color:${F.CYAN};font-weight:bold">$1</span>`);
    // Code `text`
    result = result.replace(/`(.+?)`/g, `<code style="background:rgba(255,165,0,0.1);padding:1px 4px;border-radius:2px;font-size:10px;color:${F.ORANGE}">$1</code>`);
    return result;
  };

  return (
    <div style={{
      width: '100%',
      height: '100%',
      backgroundColor: F.DARK_BG,
      display: 'flex',
      flexDirection: 'column',
      fontFamily: FONT,
    }}>
      {/* Header */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        padding: '6px 12px',
        backgroundColor: F.PANEL_BG,
        borderBottom: `1px solid ${F.BORDER}`,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <BookOpen size={12} style={{ color: F.ORANGE }} />
          <span style={{ fontSize: '10px', fontWeight: 'bold', color: F.ORANGE, letterSpacing: '0.5px' }}>
            FINSCRIPT DOCS
          </span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
          <button
            onClick={() => setActiveTab?.('docs')}
            title="Open Full Documentation Tab"
            style={{
              background: 'none',
              border: `1px solid ${F.BORDER}`,
              borderRadius: '2px',
              color: F.CYAN,
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '3px',
              padding: '3px 6px',
              fontSize: '8px',
              fontWeight: 'bold',
              letterSpacing: '0.5px',
            }}
          >
            <ExternalLink size={9} />
            FULL DOCS
          </button>
          <button
            onClick={onClose}
            style={{
              background: 'none',
              border: 'none',
              color: F.GRAY,
              cursor: 'pointer',
              display: 'flex',
              padding: '2px',
            }}
          >
            <X size={14} />
          </button>
        </div>
      </div>

      {/* Search */}
      <div style={{ padding: '8px 12px', borderBottom: `1px solid ${F.BORDER}` }}>
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: '6px',
          backgroundColor: F.PANEL_BG,
          border: `1px solid ${F.BORDER}`,
          borderRadius: '2px',
          padding: '4px 8px',
        }}>
          <Search size={10} style={{ color: F.GRAY }} />
          <input
            type="text"
            placeholder="Search docs..."
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            style={{
              flex: 1,
              background: 'none',
              border: 'none',
              outline: 'none',
              color: F.WHITE,
              fontSize: '10px',
              fontFamily: FONT,
            }}
          />
          {searchQuery && (
            <button
              onClick={() => setSearchQuery('')}
              style={{
                background: 'none',
                border: 'none',
                color: F.GRAY,
                cursor: 'pointer',
                padding: 0,
                display: 'flex',
              }}
            >
              <X size={10} />
            </button>
          )}
        </div>
      </div>

      {/* Content */}
      <div style={{ flex: 1, display: 'flex', minHeight: 0 }}>
        {/* Sidebar Navigation */}
        <div style={{
          width: '140px',
          borderRight: `1px solid ${F.BORDER}`,
          overflowY: 'auto',
          backgroundColor: F.PANEL_BG,
        }} className="ce-scroll">
          {filteredSections.map(sub => {
            const isActive = sub.id === activeSubsection;
            return (
              <div
                key={sub.id}
                onClick={() => setActiveSubsection(sub.id)}
                style={{
                  padding: '6px 8px',
                  cursor: 'pointer',
                  fontSize: '9px',
                  color: isActive ? F.ORANGE : F.WHITE,
                  backgroundColor: isActive ? 'rgba(255,136,0,0.1)' : 'transparent',
                  borderLeft: isActive ? `2px solid ${F.ORANGE}` : '2px solid transparent',
                  lineHeight: '1.4',
                  transition: 'all 0.2s',
                }}
                onMouseEnter={(e) => {
                  if (!isActive) e.currentTarget.style.backgroundColor = 'rgba(255,136,0,0.05)';
                }}
                onMouseLeave={(e) => {
                  if (!isActive) e.currentTarget.style.backgroundColor = 'transparent';
                }}
              >
                {sub.title}
              </div>
            );
          })}
        </div>

        {/* Content Area */}
        <div style={{
          flex: 1,
          overflowY: 'auto',
          padding: '12px',
        }} className="ce-scroll">
          {activeContent ? (
            <div>
              <h3 style={{
                fontSize: '12px',
                fontWeight: 'bold',
                color: F.ORANGE,
                margin: '0 0 10px 0',
                letterSpacing: '0.3px',
                borderBottom: `1px solid ${F.BORDER}`,
                paddingBottom: '6px',
              }}>
                {activeContent.title}
              </h3>

              <div>{renderContent(activeContent.content)}</div>

              {activeContent.codeExample && (
                <div style={{ marginTop: '12px' }}>
                  <div style={{
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'space-between',
                    marginBottom: '4px',
                  }}>
                    <span style={{ fontSize: '9px', color: F.CYAN, fontWeight: 'bold', letterSpacing: '0.5px' }}>
                      EXAMPLE
                    </span>
                    {onInsertExample && (
                      <button
                        onClick={() => onInsertExample(activeContent.codeExample!)}
                        style={{
                          padding: '2px 6px',
                          fontSize: '8px',
                          fontWeight: 'bold',
                          backgroundColor: F.ORANGE,
                          color: F.DARK_BG,
                          border: 'none',
                          borderRadius: '2px',
                          cursor: 'pointer',
                          letterSpacing: '0.5px',
                        }}
                      >
                        INSERT
                      </button>
                    )}
                  </div>
                  <pre style={{
                    margin: 0,
                    padding: '8px',
                    backgroundColor: '#0a0a0a',
                    border: `1px solid ${F.BORDER}`,
                    borderRadius: '2px',
                    fontSize: '9px',
                    lineHeight: '1.5',
                    color: F.WHITE,
                    overflowX: 'auto',
                    whiteSpace: 'pre-wrap',
                    wordBreak: 'break-word',
                  }}>
                    {activeContent.codeExample}
                  </pre>
                </div>
              )}
            </div>
          ) : (
            <div style={{
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              height: '100%',
              color: F.GRAY,
              fontSize: '10px',
            }}>
              {searchQuery ? 'No results found' : 'Select a topic'}
            </div>
          )}
        </div>
      </div>
    </div>
  );
};
