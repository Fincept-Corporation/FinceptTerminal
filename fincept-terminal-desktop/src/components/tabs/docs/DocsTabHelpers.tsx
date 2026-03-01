/**
 * DocsTab - Helper functions, types, and sub-components
 */

import React, { useState, useMemo, useCallback } from 'react';
import { Copy, Check, Play, Square, Terminal, AlertTriangle, TrendingUp, TrendingDown } from 'lucide-react';
import { getColors } from './constants';
import { DocSection, DocSubsection } from './types';

// ─── Types ──────────────────────────────────────────────────────────────────

export interface FinScriptResult {
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

export function highlightCode(code: string, colors: ReturnType<typeof getColors>): string {
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

export function escapeHtml(text: string): string {
  return text.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

// ─── Search ─────────────────────────────────────────────────────────────────

export function searchDocs(sections: DocSection[], query: string): Array<{
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

export function ContentRenderer({ content, colors }: { content: string; colors: ReturnType<typeof getColors> }) {
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

export function InlineText({ text, colors }: { text: string; colors: ReturnType<typeof getColors> }) {
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

export function CodeBlock({ code, colors }: { code: string; colors: ReturnType<typeof getColors> }) {
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

export function OutputPanel({ result, isRunning, colors }: {
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

// ─── RunnableCodeBlock ───────────────────────────────────────────────────────
// Extracted as a proper component so hooks (useState, useMemo) are valid

export interface RunnableCodeBlockProps {
  code: string;
  colors: ReturnType<typeof getColors>;
  activeCode: string | null;
  isRunning: boolean;
  onRun: (code: string) => void;
}

export function RunnableCodeBlock({ code, colors: c, activeCode, isRunning, onRun }: RunnableCodeBlockProps) {
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
            onClick={() => onRun(code)}
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
}
