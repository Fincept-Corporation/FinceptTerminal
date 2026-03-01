import React, { useState, useMemo, useCallback, useRef } from 'react';
import { Search, ChevronRight, ChevronDown, ArrowUp } from 'lucide-react';
import { invoke } from '@tauri-apps/api/core';
import { getColors } from './constants';
import { DOC_SECTIONS } from './content';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { TabFooter } from '@/components/common/TabFooter';
import {
  FinScriptResult, searchDocs,
  ContentRenderer, CodeBlock, OutputPanel, RunnableCodeBlock,
} from './DocsTabHelpers';

// ─── Main Component ─────────────────────────────────────────────────────────

export default function DocsTab() {
  useTerminalTheme();
  const COLORS = getColors();

  const [expandedSections, setExpandedSections] = useState<Record<string, boolean>>(() => {
    const initial: Record<string, boolean> = {};
    DOC_SECTIONS.forEach(s => { initial[s.id] = s.id === 'finscript'; });
    return initial;
  });
  const [activeSubsection, setActiveSubsection] = useState<string>(
    DOC_SECTIONS[0]?.subsections[0]?.id || ''
  );
  const [activeSectionId, setActiveSectionId] = useState<string>(DOC_SECTIONS[0]?.id || '');
  const [searchQuery, setSearchQuery] = useState('');
  const [showSearch, setShowSearch] = useState(false);
  const [codeResult, setCodeResult] = useState<FinScriptResult | null>(null);
  const [isRunning, setIsRunning] = useState(false);
  const [activeCode, setActiveCode] = useState<string>('');
  const contentRef = useRef<HTMLDivElement>(null);

  const totalSections = DOC_SECTIONS.length;
  const totalTopics = DOC_SECTIONS.reduce((acc, s) => acc + s.subsections.length, 0);

  const searchResults = useMemo(() => searchDocs(DOC_SECTIONS, searchQuery), [searchQuery]);

  const currentContent = useMemo(() => {
    const section = DOC_SECTIONS.find(s => s.id === activeSectionId);
    if (!section) return null;
    return section.subsections.find(sub => sub.id === activeSubsection) || null;
  }, [activeSectionId, activeSubsection]);

  const currentSection = useMemo(() => DOC_SECTIONS.find(s => s.id === activeSectionId), [activeSectionId]);

  // Check if current section is FinScript (code is runnable)
  const isFinScriptSection = activeSectionId === 'finscript';

  const toggleSection = useCallback((sectionId: string) => {
    setExpandedSections(prev => ({ ...prev, [sectionId]: !prev[sectionId] }));
  }, []);

  const navigateTo = useCallback((sectionId: string, subsectionId: string) => {
    setActiveSectionId(sectionId);
    setActiveSubsection(subsectionId);
    setExpandedSections(prev => ({ ...prev, [sectionId]: true }));
    setSearchQuery('');
    setShowSearch(false);
    setCodeResult(null);
    setActiveCode('');
    if (contentRef.current) {
      contentRef.current.scrollTop = 0;
    }
  }, []);

  const navigateRelative = useCallback((direction: 'next' | 'prev') => {
    const allSubs: { sectionId: string; subId: string }[] = [];
    DOC_SECTIONS.forEach(s => {
      s.subsections.forEach(sub => {
        allSubs.push({ sectionId: s.id, subId: sub.id });
      });
    });
    const currentIdx = allSubs.findIndex(x => x.sectionId === activeSectionId && x.subId === activeSubsection);
    if (currentIdx === -1) return;
    const nextIdx = direction === 'next' ? currentIdx + 1 : currentIdx - 1;
    if (nextIdx >= 0 && nextIdx < allSubs.length) {
      navigateTo(allSubs[nextIdx].sectionId, allSubs[nextIdx].subId);
    }
  }, [activeSectionId, activeSubsection, navigateTo]);

  const scrollToTop = useCallback(() => {
    if (contentRef.current) {
      contentRef.current.scrollTop = 0;
    }
  }, []);

  // Run FinScript code
  const runCode = useCallback(async (code: string) => {
    setIsRunning(true);
    setActiveCode(code);
    setCodeResult(null);

    try {
      const result = await invoke<FinScriptResult>('execute_finscript', { code });
      setCodeResult(result);
    } catch (error) {
      setCodeResult({
        success: false,
        output: '',
        signals: [],
        plots: [],
        errors: [String(error)],
        execution_time_ms: 0,
      });
    } finally {
      setIsRunning(false);
    }
  }, []);

  // Runnable code block component
  // RunnableCodeBlock is defined as a proper component above DocsTab (hooks-safe)

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
        .docs-scrollbar::-webkit-scrollbar {
          width: 5px;
          height: 5px;
        }
        .docs-scrollbar::-webkit-scrollbar-track {
          background: #0a0a0a;
        }
        .docs-scrollbar::-webkit-scrollbar-thumb {
          background: #333;
          border-radius: 3px;
        }
        .docs-scrollbar::-webkit-scrollbar-thumb:hover {
          background: #444;
        }
        .nav-item:hover {
          background: rgba(255, 165, 0, 0.05) !important;
        }
        .nav-item-active {
          background: rgba(255, 165, 0, 0.12) !important;
          border-left: 2px solid ${COLORS.ORANGE} !important;
        }
        .search-result:hover {
          background: rgba(255, 165, 0, 0.08) !important;
        }
        @keyframes docs-spin {
          from { transform: rotate(0deg); }
          to { transform: rotate(360deg); }
        }
      `}</style>

      {/* Header Bar */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        padding: '5px 12px',
        backgroundColor: COLORS.PANEL_BG,
        borderBottom: `1px solid ${COLORS.GRAY}`,
        fontSize: '11px',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <span style={{ color: COLORS.ORANGE, fontWeight: 'bold', fontSize: '12px' }}>FINCEPT</span>
          <span style={{ color: COLORS.WHITE, fontSize: '11px' }}>DOCS</span>
          {currentSection && (
            <>
              <span style={{ color: COLORS.GRAY }}>|</span>
              <span style={{ color: COLORS.CYAN, fontSize: '10px' }}>{currentSection.title.toUpperCase()}</span>
            </>
          )}
          {currentContent && (
            <>
              <span style={{ color: COLORS.GRAY }}>&gt;</span>
              <span style={{ color: COLORS.WHITE, fontSize: '10px' }}>{currentContent.title}</span>
            </>
          )}
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <Search size={12} style={{ color: COLORS.GRAY }} />
          <input
            type="text"
            placeholder="Search..."
            value={searchQuery}
            onChange={(e) => {
              setSearchQuery(e.target.value);
              setShowSearch(e.target.value.length > 0);
            }}
            onFocus={() => { if (searchQuery) setShowSearch(true); }}
            style={{
              width: '160px',
              backgroundColor: '#0d0d0d',
              border: `1px solid #333`,
              color: COLORS.WHITE,
              fontSize: '10px',
              fontFamily: 'Consolas, monospace',
              padding: '3px 6px',
              outline: 'none',
              borderRadius: '2px'
            }}
          />
          {searchQuery && (
            <button
              onClick={() => { setSearchQuery(''); setShowSearch(false); }}
              style={{
                backgroundColor: '#333',
                color: COLORS.WHITE,
                border: 'none',
                padding: '2px 6px',
                fontSize: '9px',
                cursor: 'pointer',
                borderRadius: '2px'
              }}
            >
              X
            </button>
          )}
          <span style={{ color: COLORS.GRAY, fontSize: '9px' }}>
            {totalSections}S/{totalTopics}T
          </span>
        </div>
      </div>

      {/* Main Content Area - Three Panels */}
      <div style={{ flex: 1, display: 'flex', minHeight: 0, overflow: 'hidden', position: 'relative' }}>

        {/* Left: Sidebar Navigation */}
        <div className="docs-scrollbar" style={{
          width: '200px',
          borderRight: `1px solid #222`,
          backgroundColor: '#0a0a0a',
          overflowY: 'auto',
          flexShrink: 0
        }}>
          {DOC_SECTIONS.map(section => {
            const Icon = section.icon;
            const isExpanded = expandedSections[section.id];
            const isSectionActive = section.id === activeSectionId;

            return (
              <div key={section.id}>
                <div
                  onClick={() => toggleSection(section.id)}
                  style={{
                    display: 'flex',
                    alignItems: 'center',
                    gap: '5px',
                    padding: '6px 8px',
                    cursor: 'pointer',
                    borderBottom: `1px solid #151515`,
                    backgroundColor: isSectionActive ? 'rgba(255,165,0,0.04)' : 'transparent'
                  }}
                  className="nav-item"
                >
                  {isExpanded ? <ChevronDown size={10} style={{ color: COLORS.ORANGE }} /> : <ChevronRight size={10} style={{ color: '#444' }} />}
                  <Icon size={11} style={{ color: isSectionActive ? COLORS.ORANGE : '#555' }} />
                  <span style={{
                    fontSize: '10px',
                    fontWeight: 'bold',
                    color: isSectionActive ? COLORS.ORANGE : COLORS.WHITE,
                    letterSpacing: '0.2px',
                    flex: 1
                  }}>
                    {section.title.toUpperCase()}
                  </span>
                  <span style={{ fontSize: '8px', color: '#444' }}>{section.subsections.length}</span>
                </div>

                {isExpanded && section.subsections.map(sub => {
                  const isActive = sub.id === activeSubsection && section.id === activeSectionId;
                  return (
                    <div
                      key={sub.id}
                      onClick={() => navigateTo(section.id, sub.id)}
                      className={isActive ? 'nav-item-active' : 'nav-item'}
                      style={{
                        padding: '4px 8px 4px 28px',
                        cursor: 'pointer',
                        fontSize: '10px',
                        color: isActive ? COLORS.ORANGE : '#bbb',
                        borderLeft: isActive ? `2px solid ${COLORS.ORANGE}` : '2px solid transparent',
                        lineHeight: '1.4'
                      }}
                    >
                      {sub.title}
                    </div>
                  );
                })}
              </div>
            );
          })}
        </div>

        {/* Center: Content Area */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden', position: 'relative' }}>
          {/* Search Results Overlay */}
          {showSearch && searchResults.length > 0 && (
            <div className="docs-scrollbar" style={{
              position: 'absolute',
              top: 0,
              left: 0,
              right: 0,
              bottom: 0,
              backgroundColor: '#0a0a0a',
              zIndex: 10,
              overflowY: 'auto',
              padding: '12px 16px'
            }}>
              <div style={{ fontSize: '11px', color: COLORS.ORANGE, fontWeight: 'bold', marginBottom: '8px' }}>
                RESULTS ({searchResults.length})
              </div>
              {searchResults.map((result, idx) => (
                <div
                  key={idx}
                  className="search-result"
                  onClick={() => navigateTo(result.sectionId, result.subsection.id)}
                  style={{
                    padding: '8px 10px',
                    cursor: 'pointer',
                    borderBottom: `1px solid #1a1a1a`,
                    borderRadius: '2px',
                    marginBottom: '2px'
                  }}
                >
                  <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '2px' }}>
                    <span style={{ fontSize: '9px', color: COLORS.GRAY }}>{result.sectionTitle}</span>
                    <span style={{ color: '#333' }}>&gt;</span>
                    <span style={{ fontSize: '10px', color: COLORS.CYAN, fontWeight: 'bold' }}>{result.subsection.title}</span>
                  </div>
                  <div style={{ fontSize: '9px', color: '#555', lineHeight: '1.3' }}>
                    {result.matchContext}
                  </div>
                </div>
              ))}
            </div>
          )}

          {showSearch && searchResults.length === 0 && searchQuery.length > 0 && (
            <div style={{
              position: 'absolute',
              top: 0, left: 0, right: 0, bottom: 0,
              backgroundColor: '#0a0a0a',
              zIndex: 10,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center'
            }}>
              <div style={{ textAlign: 'center', color: '#444', fontSize: '11px' }}>No results for "{searchQuery}"</div>
            </div>
          )}

          {/* Document Content */}
          <div
            ref={contentRef}
            className="docs-scrollbar"
            style={{
              flex: 1,
              overflowY: 'auto',
              padding: '14px 20px 40px 20px',
              backgroundColor: '#0a0a0a'
            }}
          >
            {currentContent ? (
              <div>
                {/* Title */}
                <h1 style={{
                  fontSize: '15px',
                  fontWeight: 'bold',
                  color: COLORS.ORANGE,
                  margin: '0 0 12px 0',
                  letterSpacing: '0.3px',
                  borderBottom: `1px solid #1a1a1a`,
                  paddingBottom: '8px'
                }}>
                  {currentContent.title}
                </h1>

                {/* Content */}
                <ContentRenderer content={currentContent.content} colors={COLORS} />

                {/* Code Example */}
                {currentContent.codeExample && (
                  <div style={{ marginTop: '12px' }}>
                    <div style={{ fontSize: '10px', color: COLORS.CYAN, fontWeight: 'bold', marginBottom: '4px' }}>
                      {isFinScriptSection ? 'RUNNABLE EXAMPLE' : 'EXAMPLE'}
                    </div>
                    {isFinScriptSection ? (
                      <RunnableCodeBlock
                        code={currentContent.codeExample}
                        colors={COLORS}
                        activeCode={activeCode}
                        isRunning={isRunning}
                        onRun={runCode}
                      />
                    ) : (
                      <CodeBlock code={currentContent.codeExample} colors={COLORS} />
                    )}
                  </div>
                )}

                {/* Navigation buttons */}
                <div style={{
                  display: 'flex',
                  justifyContent: 'space-between',
                  marginTop: '20px',
                  paddingTop: '10px',
                  borderTop: `1px solid #1a1a1a`
                }}>
                  <button
                    onClick={() => navigateRelative('prev')}
                    style={{
                      backgroundColor: '#141414',
                      border: `1px solid #222`,
                      color: COLORS.WHITE,
                      padding: '4px 10px',
                      fontSize: '10px',
                      cursor: 'pointer',
                      borderRadius: '2px',
                      fontFamily: 'Consolas, monospace'
                    }}
                  >
                    &lt; PREV
                  </button>
                  <button
                    onClick={() => navigateRelative('next')}
                    style={{
                      backgroundColor: '#141414',
                      border: `1px solid #222`,
                      color: COLORS.WHITE,
                      padding: '4px 10px',
                      fontSize: '10px',
                      cursor: 'pointer',
                      borderRadius: '2px',
                      fontFamily: 'Consolas, monospace'
                    }}
                  >
                    NEXT &gt;
                  </button>
                </div>
              </div>
            ) : (
              <div style={{ textAlign: 'center', paddingTop: '60px' }}>
                <div style={{ fontSize: '14px', color: COLORS.ORANGE, fontWeight: 'bold', marginBottom: '8px' }}>
                  FINCEPT DOCUMENTATION
                </div>
                <div style={{ fontSize: '10px', color: '#444' }}>
                  Select a topic from navigation
                </div>
              </div>
            )}
          </div>

          {/* Scroll to top */}
          <button
            onClick={scrollToTop}
            style={{
              position: 'absolute',
              bottom: '8px',
              right: '8px',
              width: '24px',
              height: '24px',
              borderRadius: '3px',
              backgroundColor: '#1a1a1a',
              border: `1px solid #333`,
              color: COLORS.ORANGE,
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              zIndex: 5
            }}
          >
            <ArrowUp size={12} />
          </button>
        </div>

        {/* Right: Output Panel */}
        <div className="docs-scrollbar" style={{
          width: '280px',
          borderLeft: `1px solid #222`,
          backgroundColor: '#080808',
          overflowY: 'auto',
          flexShrink: 0,
          display: 'flex',
          flexDirection: 'column'
        }}>
          {/* Output header */}
          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            padding: '6px 8px',
            borderBottom: `1px solid #1a1a1a`,
            backgroundColor: '#0c0c0c',
            flexShrink: 0
          }}>
            <span style={{ fontSize: '10px', color: COLORS.ORANGE, fontWeight: 'bold', letterSpacing: '0.3px' }}>
              OUTPUT
            </span>
            {codeResult && (
              <button
                onClick={() => { setCodeResult(null); setActiveCode(''); }}
                style={{
                  marginLeft: 'auto',
                  background: 'none',
                  border: 'none',
                  color: COLORS.GRAY,
                  fontSize: '9px',
                  cursor: 'pointer',
                  padding: '1px 4px'
                }}
              >
                CLEAR
              </button>
            )}
          </div>

          {/* Output content */}
          <div style={{ flex: 1, overflowY: 'auto' }}>
            <OutputPanel result={codeResult} isRunning={isRunning} colors={COLORS} />
          </div>

          {/* Active code preview at bottom */}
          {activeCode && !isRunning && (
            <div style={{
              borderTop: `1px solid #1a1a1a`,
              padding: '4px 8px',
              flexShrink: 0
            }}>
              <div style={{ fontSize: '8px', color: '#444', marginBottom: '2px' }}>LAST EXECUTED</div>
              <pre style={{
                margin: 0,
                fontSize: '8px',
                color: '#555',
                lineHeight: '1.3',
                maxHeight: '60px',
                overflow: 'hidden',
                whiteSpace: 'pre-wrap'
              }}>
                {activeCode.slice(0, 200)}{activeCode.length > 200 ? '...' : ''}
              </pre>
            </div>
          )}
        </div>
      </div>

      <TabFooter
        tabName="DOCUMENTATION"
        leftInfo={[
          { label: `${totalSections} Sections`, color: COLORS.CYAN },
          { label: `${totalTopics} Topics`, color: COLORS.GREEN },
          { label: currentContent ? currentContent.title : 'Ready', color: COLORS.WHITE },
        ]}
        statusInfo={isRunning ? 'Executing...' : (codeResult?.success ? 'Last run: OK' : 'Online')}
        backgroundColor={COLORS.PANEL_BG}
        borderColor={COLORS.GRAY}
      />
    </div>
  );
}
