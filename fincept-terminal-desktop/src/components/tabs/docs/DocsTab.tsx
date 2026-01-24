import React, { useState, useMemo, useCallback, useRef } from 'react';
import { Search, ChevronRight, ChevronDown, Copy, Check, ArrowUp, Play, Square, Terminal, AlertTriangle, TrendingUp, TrendingDown } from 'lucide-react';
import { invoke } from '@tauri-apps/api/core';
import { getColors } from './constants';
import { DOC_SECTIONS } from './content';
import { DocSection, DocSubsection } from './types';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { TabFooter } from '@/components/common/TabFooter';

// ─── Types ──────────────────────────────────────────────────────────────────

interface FinScriptResult {
  success: boolean;
  output: string;
  signals: Array<{
    signal_type: string;
    message: string;
    timestamp: string;
    price?: number;
  }>;
  plots: Array<{
    plot_type: string;
    label: string;
    data: Array<{
      timestamp: number;
      value?: number;
      open?: number;
      high?: number;
      low?: number;
      close?: number;
      volume?: number;
    }>;
    color?: string;
  }>;
  errors: string[];
  execution_time_ms: number;
}

// ─── Syntax Highlighting ────────────────────────────────────────────────────

function highlightCode(code: string, colors: ReturnType<typeof getColors>): string {
  const lines = code.split('\n');
  return lines.map(line => {
    if (line.trimStart().startsWith('//')) {
      return `<span style="color:${colors.GRAY};font-style:italic">${escapeHtml(line)}</span>`;
    }

    let result = escapeHtml(line);

    result = result.replace(/"([^"]*?)"/g, `<span style="color:#98c379">"$1"</span>`);
    result = result.replace(/\b(\d+\.?\d*)\b/g, `<span style="color:#d19a66">$1</span>`);

    const keywords = ['if', 'else', 'for', 'while', 'in', 'fn', 'return', 'break', 'continue',
      'buy', 'sell', 'plot', 'plot_candlestick', 'plot_line', 'plot_histogram', 'plot_shape',
      'and', 'or', 'not', 'true', 'false', 'na', 'switch', 'strategy', 'input',
      'struct', 'import', 'export', 'alert', 'alertcondition', 'print', 'request', 'color',
      'bgcolor', 'hline'];
    keywords.forEach(kw => {
      const regex = new RegExp(`\\b(${kw})\\b`, 'g');
      result = result.replace(regex, `<span style="color:${colors.ORANGE};font-weight:bold">$1</span>`);
    });

    const builtins = ['ema', 'sma', 'wma', 'rsi', 'macd', 'bollinger', 'stochastic', 'atr',
      'obv', 'vwap', 'cci', 'mfi', 'adx', 'close', 'open', 'high', 'low', 'volume',
      'last', 'nz', 'math_abs', 'math_max', 'math_min', 'math_sqrt', 'math_pow',
      'math_round', 'math_floor', 'math_ceil', 'math_log', 'array_new', 'array_push',
      'array_pop', 'array_get', 'array_set', 'array_size', 'str_length', 'str_contains',
      'str_replace', 'str_tostring', 'map_new', 'map_put', 'map_get', 'map_contains',
      'matrix_new', 'matrix_get', 'matrix_set', 'matrix_rows', 'matrix_cols'];
    builtins.forEach(fn => {
      const regex = new RegExp(`\\b(${fn})\\b(?=\\s*[\\(])`, 'g');
      result = result.replace(regex, `<span style="color:${colors.CYAN}">$1</span>`);
    });

    result = result.replace(/\b([A-Z]{2,5})\b(?!<\/span>)/g, (match, p1) => {
      if (keywords.includes(p1.toLowerCase())) return match;
      return `<span style="color:#e5c07b">${p1}</span>`;
    });

    return result;
  }).join('\n');
}

function escapeHtml(text: string): string {
  return text.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

// ─── Search ─────────────────────────────────────────────────────────────────

function searchDocs(sections: DocSection[], query: string): Array<{
  sectionId: string;
  sectionTitle: string;
  subsection: DocSubsection;
  matchContext: string;
}> {
  if (!query.trim()) return [];
  const q = query.toLowerCase();
  const results: Array<{
    sectionId: string;
    sectionTitle: string;
    subsection: DocSubsection;
    matchContext: string;
  }> = [];

  sections.forEach(section => {
    section.subsections.forEach(sub => {
      const titleMatch = sub.title.toLowerCase().includes(q);
      const contentMatch = sub.content.toLowerCase().includes(q);
      const codeMatch = sub.codeExample?.toLowerCase().includes(q);

      if (titleMatch || contentMatch || codeMatch) {
        let matchContext = '';
        if (titleMatch) {
          matchContext = sub.title;
        } else if (contentMatch) {
          const idx = sub.content.toLowerCase().indexOf(q);
          const start = Math.max(0, idx - 30);
          const end = Math.min(sub.content.length, idx + query.length + 30);
          matchContext = (start > 0 ? '...' : '') + sub.content.slice(start, end) + (end < sub.content.length ? '...' : '');
        } else if (codeMatch) {
          matchContext = 'Found in code example';
        }
        results.push({ sectionId: section.id, sectionTitle: section.title, subsection: sub, matchContext });
      }
    });
  });

  return results;
}

// ─── Content Renderer ───────────────────────────────────────────────────────

function ContentRenderer({ content, colors }: { content: string; colors: ReturnType<typeof getColors> }) {
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
        bulletItems.push(lines[i].trimStart().replace(/^[•\-]\s*/, ''));
        i++;
      }
      elements.push(
        <ul key={`ul-${i}`} style={{ margin: '6px 0', padding: 0, listStyle: 'none' }}>
          {bulletItems.map((item, idx) => (
            <li key={idx} style={{
              margin: '3px 0',
              paddingLeft: '14px',
              fontSize: '11px',
              lineHeight: '1.5',
              position: 'relative',
              color: colors.WHITE
            }}>
              <span style={{ position: 'absolute', left: '3px', color: colors.ORANGE, fontSize: '6px', top: '5px' }}>&#9632;</span>
              <InlineText text={item} colors={colors} />
            </li>
          ))}
        </ul>
      );
      continue;
    }

    elements.push(
      <p key={i} style={{ margin: '0 0 6px 0', fontSize: '11px', lineHeight: '1.6', color: colors.WHITE }}>
        <InlineText text={line} colors={colors} />
      </p>
    );
    i++;
  }

  return <>{elements}</>;
}

function InlineText({ text, colors }: { text: string; colors: ReturnType<typeof getColors> }) {
  const parts: React.ReactNode[] = [];
  let remaining = text;
  let key = 0;

  while (remaining.length > 0) {
    const boldMatch = remaining.match(/^\*\*(.+?)\*\*/);
    if (boldMatch) {
      parts.push(<span key={key++} style={{ color: colors.CYAN, fontWeight: 'bold' }}>{boldMatch[1]}</span>);
      remaining = remaining.slice(boldMatch[0].length);
      continue;
    }

    const codeMatch = remaining.match(/^`(.+?)`/);
    if (codeMatch) {
      parts.push(
        <code key={key++} style={{
          background: 'rgba(255,165,0,0.1)',
          padding: '1px 4px',
          borderRadius: '2px',
          fontSize: '11px',
          color: colors.ORANGE
        }}>{codeMatch[1]}</code>
      );
      remaining = remaining.slice(codeMatch[0].length);
      continue;
    }

    const nextSpecial = remaining.search(/\*\*|`/);
    if (nextSpecial === -1) {
      parts.push(<span key={key++}>{remaining}</span>);
      break;
    } else if (nextSpecial === 0) {
      parts.push(<span key={key++}>{remaining[0]}</span>);
      remaining = remaining.slice(1);
    } else {
      parts.push(<span key={key++}>{remaining.slice(0, nextSpecial)}</span>);
      remaining = remaining.slice(nextSpecial);
    }
  }

  return <>{parts}</>;
}

// ─── Code Block (non-runnable, for non-finscript examples) ──────────────────

function CodeBlock({ code, colors }: { code: string; colors: ReturnType<typeof getColors> }) {
  const [copied, setCopied] = useState(false);

  const handleCopy = useCallback(() => {
    navigator.clipboard.writeText(code).then(() => {
      setCopied(true);
      setTimeout(() => setCopied(false), 2000);
    });
  }, [code]);

  const highlighted = useMemo(() => highlightCode(code, colors), [code, colors]);

  return (
    <div style={{
      margin: '8px 0',
      backgroundColor: '#0d0d0d',
      border: `1px solid #222`,
      borderRadius: '3px',
      overflow: 'hidden'
    }}>
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        padding: '3px 8px',
        backgroundColor: '#141414',
        borderBottom: `1px solid #222`
      }}>
        <span style={{ fontSize: '9px', color: colors.GRAY, letterSpacing: '0.5px' }}>CODE</span>
        <button
          onClick={handleCopy}
          style={{
            display: 'flex',
            alignItems: 'center',
            gap: '3px',
            background: 'none',
            border: 'none',
            color: copied ? colors.GREEN : colors.GRAY,
            fontSize: '9px',
            cursor: 'pointer',
            padding: '1px 4px'
          }}
        >
          {copied ? <Check size={9} /> : <Copy size={9} />}
          {copied ? 'COPIED' : 'COPY'}
        </button>
      </div>
      <pre style={{
        margin: 0,
        padding: '8px',
        overflowX: 'auto',
        fontSize: '10px',
        lineHeight: '1.5',
        fontFamily: 'Consolas, monospace',
        color: colors.WHITE
      }}>
        <code dangerouslySetInnerHTML={{ __html: highlighted }} />
      </pre>
    </div>
  );
}

// ─── Output Panel (Right Side) ──────────────────────────────────────────────

function OutputPanel({ result, isRunning, colors }: {
  result: FinScriptResult | null;
  isRunning: boolean;
  colors: ReturnType<typeof getColors>;
}) {
  if (isRunning) {
    return (
      <div style={{ padding: '16px', textAlign: 'center' }}>
        <div className="docs-spinner" style={{
          width: '20px',
          height: '20px',
          border: `2px solid #333`,
          borderTop: `2px solid ${colors.ORANGE}`,
          borderRadius: '50%',
          margin: '0 auto 8px',
          animation: 'docs-spin 0.8s linear infinite'
        }} />
        <div style={{ fontSize: '10px', color: colors.GRAY }}>EXECUTING...</div>
      </div>
    );
  }

  if (!result) {
    return (
      <div style={{ padding: '16px', textAlign: 'center' }}>
        <Terminal size={24} style={{ color: '#333', margin: '0 auto 8px' }} />
        <div style={{ fontSize: '10px', color: '#444' }}>Run a code example to see output</div>
        <div style={{ fontSize: '9px', color: '#333', marginTop: '4px' }}>Click the RUN button on any code block</div>
      </div>
    );
  }

  return (
    <div style={{ padding: '8px', fontSize: '10px' }}>
      {/* Status bar */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: '6px',
        padding: '4px 6px',
        marginBottom: '6px',
        backgroundColor: result.success ? 'rgba(0,255,0,0.05)' : 'rgba(255,0,0,0.05)',
        border: `1px solid ${result.success ? '#1a3a1a' : '#3a1a1a'}`,
        borderRadius: '2px'
      }}>
        <span style={{
          width: '6px',
          height: '6px',
          borderRadius: '50%',
          backgroundColor: result.success ? colors.GREEN : '#ff4444',
          flexShrink: 0
        }} />
        <span style={{ color: result.success ? colors.GREEN : '#ff4444', fontWeight: 'bold', fontSize: '9px' }}>
          {result.success ? 'SUCCESS' : 'ERROR'}
        </span>
        <span style={{ color: colors.GRAY, fontSize: '9px', marginLeft: 'auto' }}>
          {result.execution_time_ms}ms
        </span>
      </div>

      {/* Errors */}
      {result.errors.length > 0 && (
        <div style={{ marginBottom: '8px' }}>
          <div style={{ fontSize: '9px', color: '#ff4444', fontWeight: 'bold', marginBottom: '4px', display: 'flex', alignItems: 'center', gap: '4px' }}>
            <AlertTriangle size={10} /> ERRORS
          </div>
          {result.errors.map((err, idx) => (
            <div key={idx} style={{
              padding: '4px 6px',
              backgroundColor: 'rgba(255,0,0,0.05)',
              border: '1px solid #3a1a1a',
              borderRadius: '2px',
              fontSize: '10px',
              color: '#ff6666',
              marginBottom: '3px',
              lineHeight: '1.4',
              wordBreak: 'break-word'
            }}>
              {err}
            </div>
          ))}
        </div>
      )}

      {/* Output text */}
      {result.output && (
        <div style={{ marginBottom: '8px' }}>
          <div style={{ fontSize: '9px', color: colors.CYAN, fontWeight: 'bold', marginBottom: '4px' }}>OUTPUT</div>
          <pre style={{
            margin: 0,
            padding: '6px',
            backgroundColor: '#0a0a0a',
            border: '1px solid #1a1a1a',
            borderRadius: '2px',
            fontSize: '10px',
            lineHeight: '1.5',
            color: colors.WHITE,
            whiteSpace: 'pre-wrap',
            wordBreak: 'break-word',
            maxHeight: '200px',
            overflowY: 'auto'
          }}>
            {result.output}
          </pre>
        </div>
      )}

      {/* Signals */}
      {result.signals.length > 0 && (
        <div style={{ marginBottom: '8px' }}>
          <div style={{ fontSize: '9px', color: colors.CYAN, fontWeight: 'bold', marginBottom: '4px' }}>
            SIGNALS ({result.signals.length})
          </div>
          {result.signals.map((signal, idx) => (
            <div key={idx} style={{
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              padding: '4px 6px',
              backgroundColor: signal.signal_type === 'Buy' ? 'rgba(0,255,0,0.04)' : 'rgba(255,0,0,0.04)',
              border: `1px solid ${signal.signal_type === 'Buy' ? '#1a3a1a' : '#3a1a1a'}`,
              borderRadius: '2px',
              marginBottom: '3px'
            }}>
              {signal.signal_type === 'Buy' ? (
                <TrendingUp size={10} style={{ color: colors.GREEN, flexShrink: 0 }} />
              ) : (
                <TrendingDown size={10} style={{ color: '#ff4444', flexShrink: 0 }} />
              )}
              <span style={{
                fontSize: '9px',
                fontWeight: 'bold',
                color: signal.signal_type === 'Buy' ? colors.GREEN : '#ff4444',
                flexShrink: 0
              }}>
                {signal.signal_type.toUpperCase()}
              </span>
              <span style={{ fontSize: '9px', color: colors.WHITE, flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                {signal.message}
              </span>
              {signal.price && (
                <span style={{ fontSize: '9px', color: colors.GRAY, flexShrink: 0 }}>${signal.price.toFixed(2)}</span>
              )}
            </div>
          ))}
        </div>
      )}

      {/* Plots info */}
      {result.plots.length > 0 && (
        <div>
          <div style={{ fontSize: '9px', color: colors.CYAN, fontWeight: 'bold', marginBottom: '4px' }}>
            PLOTS ({result.plots.length})
          </div>
          {result.plots.map((plot, idx) => (
            <div key={idx} style={{
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              padding: '3px 6px',
              backgroundColor: '#0a0a0a',
              border: '1px solid #1a1a1a',
              borderRadius: '2px',
              marginBottom: '2px'
            }}>
              <span style={{
                width: '8px',
                height: '2px',
                backgroundColor: plot.color || colors.CYAN,
                flexShrink: 0
              }} />
              <span style={{ fontSize: '9px', color: colors.WHITE }}>{plot.label}</span>
              <span style={{ fontSize: '8px', color: colors.GRAY, marginLeft: 'auto' }}>
                {plot.plot_type} ({plot.data.length} pts)
              </span>
            </div>
          ))}
        </div>
      )}

      {/* Empty result */}
      {!result.output && result.signals.length === 0 && result.plots.length === 0 && result.errors.length === 0 && (
        <div style={{ color: colors.GRAY, fontSize: '10px', textAlign: 'center', padding: '8px' }}>
          Script executed with no output
        </div>
      )}
    </div>
  );
}

// ─── Main Component ─────────────────────────────────────────────────────────

export default function DocsTab() {
  const { colors: themeColors } = useTerminalTheme();
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
  const RunnableCodeBlock = useCallback(({ code, colors: c }: { code: string; colors: ReturnType<typeof getColors> }) => {
    const [copied, setCopied] = useState(false);
    const highlighted = useMemo(() => highlightCode(code, c), [code, c]);
    const isActive = activeCode === code;

    const handleCopy = () => {
      navigator.clipboard.writeText(code).then(() => {
        setCopied(true);
        setTimeout(() => setCopied(false), 2000);
      });
    };

    return (
      <div style={{
        margin: '8px 0',
        backgroundColor: '#0d0d0d',
        border: `1px solid ${isActive ? c.ORANGE : '#222'}`,
        borderRadius: '3px',
        overflow: 'hidden',
        transition: 'border-color 0.2s'
      }}>
        <div style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          padding: '3px 8px',
          backgroundColor: '#141414',
          borderBottom: `1px solid #222`
        }}>
          <span style={{ fontSize: '9px', color: c.ORANGE, letterSpacing: '0.5px', fontWeight: 'bold' }}>FINSCRIPT</span>
          <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
            <button
              onClick={handleCopy}
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '3px',
                background: 'none',
                border: 'none',
                color: copied ? c.GREEN : c.GRAY,
                fontSize: '9px',
                cursor: 'pointer',
                padding: '2px 5px'
              }}
            >
              {copied ? <Check size={9} /> : <Copy size={9} />}
              {copied ? 'COPIED' : 'COPY'}
            </button>
            <button
              onClick={() => runCode(code)}
              disabled={isRunning}
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '3px',
                backgroundColor: isRunning ? '#333' : c.ORANGE,
                color: isRunning ? c.GRAY : '#000',
                border: 'none',
                fontSize: '9px',
                fontWeight: 'bold',
                cursor: isRunning ? 'not-allowed' : 'pointer',
                padding: '2px 8px',
                borderRadius: '2px'
              }}
            >
              {isRunning ? <Square size={8} /> : <Play size={8} />}
              {isRunning ? 'RUNNING' : 'RUN'}
            </button>
          </div>
        </div>
        <pre style={{
          margin: 0,
          padding: '8px',
          overflowX: 'auto',
          fontSize: '10px',
          lineHeight: '1.5',
          fontFamily: 'Consolas, monospace',
          color: c.WHITE,
          maxHeight: '300px',
          overflowY: 'auto'
        }}>
          <code dangerouslySetInnerHTML={{ __html: highlighted }} />
        </pre>
      </div>
    );
  }, [activeCode, isRunning, runCode]);

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
                      <RunnableCodeBlock code={currentContent.codeExample} colors={COLORS} />
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
            <Terminal size={11} style={{ color: COLORS.ORANGE }} />
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
