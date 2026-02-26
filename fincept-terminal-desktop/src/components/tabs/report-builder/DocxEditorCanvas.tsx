import React, { useRef, useEffect, useState } from 'react';
import { EditorContent } from '@tiptap/react';
import { AlertTriangle, PanelTop, PanelBottom } from 'lucide-react';
import { DocxEditorCanvasProps } from './types';
import './docx-editor.css';

// ── Fincept Design Tokens ──────────────────────────────────────────
const F = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  BORDER: '#2A2A2A',
  MUTED: '#4A4A4A',
};

// Lazy-load docx-preview to avoid bundling it when not needed
let renderAsyncFn: ((data: any, container: HTMLElement, styleContainer?: HTMLElement, options?: any) => Promise<any>) | null = null;

const loadDocxPreview = async () => {
  if (!renderAsyncFn) {
    const docxPreview = await import('docx-preview');
    renderAsyncFn = docxPreview.renderAsync;
  }
  return renderAsyncFn!;
};

// ── Watermark Overlay ──────────────────────────────────────────────
const WatermarkOverlay: React.FC<{ text: string }> = ({ text }) => (
  <div
    style={{
      position: 'absolute',
      inset: 0,
      pointerEvents: 'none',
      overflow: 'hidden',
      zIndex: 1,
    }}
  >
    <div
      style={{
        position: 'absolute',
        top: '50%',
        left: '50%',
        transform: 'translate(-50%, -50%) rotate(-45deg)',
        fontSize: '64px',
        fontWeight: 700,
        color: 'rgba(200, 200, 200, 0.15)',
        whiteSpace: 'nowrap',
        userSelect: 'none',
        letterSpacing: '8px',
        textTransform: 'uppercase',
        fontFamily: '"IBM Plex Mono", monospace',
      }}
    >
      {text}
    </div>
  </div>
);

// ── Editable Header Zone ───────────────────────────────────────────
// Always visible: when enabled shows editable text, when disabled shows
// a clickable placeholder to enable it (like Word's "Double-click to edit header")
const HeaderZone: React.FC<{
  text: string;
  enabled: boolean;
  compact?: boolean;
  onToggle?: () => void;
  onTextChange?: (text: string) => void;
}> = ({ text, enabled, compact, onToggle, onTextChange }) => {
  if (!enabled) {
    // Show a subtle clickable area to enable header
    return (
      <div
        onClick={onToggle}
        style={{
          textAlign: 'center',
          padding: compact ? '3px 8px' : '4px 16px',
          fontSize: compact ? '7px' : '8px',
          color: '#ccc',
          minHeight: compact ? '14px' : '18px',
          backgroundColor: '#f9f9f9',
          borderBottom: '1px dashed #e0e0e0',
          cursor: 'pointer',
          fontFamily: '"IBM Plex Mono", monospace',
          fontStyle: 'italic',
          opacity: 0.6,
          transition: 'opacity 0.2s',
        }}
        onMouseEnter={(e) => { e.currentTarget.style.opacity = '1'; }}
        onMouseLeave={(e) => { e.currentTarget.style.opacity = '0.6'; }}
        title="Click to enable header"
      >
        <PanelTop size={8} style={{ display: 'inline', verticalAlign: 'middle', marginRight: '4px' }} />
        Click to add header
      </div>
    );
  }

  return (
    <div
      style={{
        borderBottom: '1px solid #e0e0e0',
        padding: compact ? '4px 8px' : '6px 16px',
        minHeight: compact ? '20px' : '28px',
        backgroundColor: '#fafafa',
        display: 'flex',
        alignItems: 'center',
        gap: '6px',
      }}
    >
      <span title="Click to disable header" onClick={onToggle} style={{ cursor: 'pointer', flexShrink: 0, display: 'flex' }}>
        <PanelTop size={compact ? 8 : 10} style={{ color: '#bbb' }} />
      </span>
      <input
        type="text"
        value={text}
        onChange={(e) => onTextChange?.(e.target.value)}
        placeholder="Type header text..."
        style={{
          flex: 1,
          border: 'none',
          outline: 'none',
          backgroundColor: 'transparent',
          fontSize: compact ? '8px' : '9px',
          color: '#666',
          fontFamily: '"IBM Plex Mono", monospace',
          textAlign: 'center',
          padding: 0,
        }}
      />
    </div>
  );
};

// ── Editable Footer Zone ───────────────────────────────────────────
const FooterZone: React.FC<{
  text: string;
  enabled: boolean;
  compact?: boolean;
  onToggle?: () => void;
  onTextChange?: (text: string) => void;
}> = ({ text, enabled, compact, onToggle, onTextChange }) => {
  if (!enabled) {
    return (
      <div
        onClick={onToggle}
        style={{
          textAlign: 'center',
          padding: compact ? '3px 8px' : '4px 16px',
          fontSize: compact ? '7px' : '8px',
          color: '#ccc',
          minHeight: compact ? '14px' : '18px',
          backgroundColor: '#f9f9f9',
          borderTop: '1px dashed #e0e0e0',
          cursor: 'pointer',
          fontFamily: '"IBM Plex Mono", monospace',
          fontStyle: 'italic',
          opacity: 0.6,
          transition: 'opacity 0.2s',
        }}
        onMouseEnter={(e) => { e.currentTarget.style.opacity = '1'; }}
        onMouseLeave={(e) => { e.currentTarget.style.opacity = '0.6'; }}
        title="Click to enable footer"
      >
        <PanelBottom size={8} style={{ display: 'inline', verticalAlign: 'middle', marginRight: '4px' }} />
        Click to add footer
      </div>
    );
  }

  return (
    <div
      style={{
        borderTop: '1px solid #e0e0e0',
        padding: compact ? '4px 8px' : '6px 16px',
        minHeight: compact ? '20px' : '28px',
        backgroundColor: '#fafafa',
        display: 'flex',
        alignItems: 'center',
        gap: '6px',
      }}
    >
      <span title="Click to disable footer" onClick={onToggle} style={{ cursor: 'pointer', flexShrink: 0, display: 'flex' }}>
        <PanelBottom size={compact ? 8 : 10} style={{ color: '#bbb' }} />
      </span>
      <input
        type="text"
        value={text}
        onChange={(e) => onTextChange?.(e.target.value)}
        placeholder="Type footer text..."
        style={{
          flex: 1,
          border: 'none',
          outline: 'none',
          backgroundColor: 'transparent',
          fontSize: compact ? '8px' : '9px',
          color: '#666',
          fontFamily: '"IBM Plex Mono", monospace',
          textAlign: 'center',
          padding: 0,
        }}
      />
    </div>
  );
};

// ── Page Break Divider ─────────────────────────────────────────────
// Visual gap between A4 pages (like Google Docs / Word)
const PageGap: React.FC<{ pageNumber: number; compact?: boolean }> = ({ pageNumber, compact }) => (
  <div
    style={{
      width: '100%',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      padding: compact ? '4px 0' : '10px 0',
      position: 'relative',
    }}
  >
    <div
      style={{
        position: 'absolute',
        left: '50%',
        transform: 'translateX(-50%)',
        backgroundColor: F.DARK_BG,
        padding: '0 10px',
        fontSize: compact ? '7px' : '8px',
        color: F.MUTED,
        fontFamily: '"IBM Plex Mono", monospace',
        letterSpacing: '1px',
        zIndex: 1,
      }}
    >
      PAGE {pageNumber}
    </div>
    <div style={{ width: '100%', borderTop: `1px dashed ${F.MUTED}` }} />
  </div>
);

// ── Preview Panel (docx-preview) ───────────────────────────────────
const DocxPreviewPanel: React.FC<{ arrayBuffer: ArrayBuffer | null; compact?: boolean }> = ({ arrayBuffer, compact }) => {
  const containerRef = useRef<HTMLDivElement>(null);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);

  useEffect(() => {
    if (!arrayBuffer || !containerRef.current) return;

    let cancelled = false;
    setLoading(true);
    setError(null);

    (async () => {
      try {
        const render = await loadDocxPreview();
        if (cancelled || !containerRef.current) return;
        containerRef.current.innerHTML = '';

        await render(arrayBuffer, containerRef.current, undefined, {
          className: 'docx',
          inWrapper: true,
          ignoreWidth: compact,
          ignoreHeight: false,
          ignoreFonts: false,
          breakPages: !compact,
          ignoreLastRenderedPageBreak: true,
          experimental: true,
          trimXmlDeclaration: true,
          useBase64URL: true,
          renderChanges: false,
          renderHeaders: true,
          renderFooters: true,
          renderFootnotes: true,
          renderEndnotes: true,
          renderComments: false,
          debug: false,
        });
      } catch (err) {
        if (!cancelled) {
          console.error('[DocxPreview] Render failed:', err);
          setError('Failed to render DOCX preview');
        }
      } finally {
        if (!cancelled) setLoading(false);
      }
    })();

    return () => { cancelled = true; };
  }, [arrayBuffer, compact]);

  if (!arrayBuffer) {
    return (
      <div
        style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          height: '100%',
          color: F.MUTED,
          fontSize: '10px',
          fontFamily: '"IBM Plex Mono", monospace',
        }}
      >
        No DOCX file loaded for preview
      </div>
    );
  }

  return (
    <div className={`docx-preview-container ${compact ? 'docx-preview-compact' : ''}`} style={{ height: '100%', overflowY: 'auto' }}>
      {loading && (
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', padding: '24px', color: F.GRAY, fontSize: '10px', fontFamily: '"IBM Plex Mono", monospace' }}>
          Rendering preview...
        </div>
      )}
      {error && (
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', padding: '24px', color: F.ORANGE, fontSize: '10px', fontFamily: '"IBM Plex Mono", monospace' }}>
          {error}
        </div>
      )}
      <div ref={containerRef} />
    </div>
  );
};

// ── A4 Page Shell ──────────────────────────────────────────────────
// Renders one A4 page with header, content area, and footer
const A4Page: React.FC<{
  children: React.ReactNode;
  compact?: boolean;
  pageWidth: string;
  watermarkEnabled?: boolean;
  watermarkText?: string;
  headerEnabled?: boolean;
  footerEnabled?: boolean;
  headerText?: string;
  footerText?: string;
  onToggleHeader?: () => void;
  onToggleFooter?: () => void;
  onHeaderTextChange?: (text: string) => void;
  onFooterTextChange?: (text: string) => void;
  mt: number;
  mr: number;
  mb: number;
  ml: number;
  lineSpacing?: string;
  isMainPage?: boolean;
}> = ({
  children, compact, pageWidth, watermarkEnabled, watermarkText,
  headerEnabled, footerEnabled, headerText, footerText,
  onToggleHeader, onToggleFooter, onHeaderTextChange, onFooterTextChange,
  mt, mr, mb, ml, lineSpacing, isMainPage,
}) => (
  <div
    style={{
      width: pageWidth,
      maxWidth: '100%',
      minHeight: compact ? 'auto' : '297mm',
      margin: '0 auto',
      backgroundColor: '#FFFFFF',
      boxShadow: compact
        ? '0 2px 8px rgba(0,0,0,0.4)'
        : `0 4px 16px rgba(0,0,0,0.5), 0 0 0 1px ${F.BORDER}`,
      position: 'relative',
      display: 'flex',
      flexDirection: 'column',
    }}
  >
    {watermarkEnabled && <WatermarkOverlay text={watermarkText || 'DRAFT'} />}
    <HeaderZone
      text={headerText || ''}
      enabled={!!headerEnabled}
      compact={compact}
      onToggle={onToggleHeader}
      onTextChange={onHeaderTextChange}
    />
    <div
      style={{
        flex: 1,
        padding: `${mt}mm ${mr}mm ${mb}mm ${ml}mm`,
        lineHeight: lineSpacing || '1.6',
        position: 'relative',
        zIndex: 2,
      }}
    >
      {children}
    </div>
    <FooterZone
      text={footerText || ''}
      enabled={!!footerEnabled}
      compact={compact}
      onToggle={onToggleFooter}
      onTextChange={onFooterTextChange}
    />
  </div>
);

// ── Edit Panel (TipTap Editor) ─────────────────────────────────────
const DocxEditPanel: React.FC<{
  editor: any;
  hasOriginal: boolean;
  compact?: boolean;
  watermarkEnabled?: boolean;
  watermarkText?: string;
  headerEnabled?: boolean;
  footerEnabled?: boolean;
  headerText?: string;
  footerText?: string;
  onToggleHeader?: () => void;
  onToggleFooter?: () => void;
  onHeaderTextChange?: (text: string) => void;
  onFooterTextChange?: (text: string) => void;
  lineSpacing?: string;
  margins?: { top: number; right: number; bottom: number; left: number };
  zoom?: number;
}> = ({
  editor, hasOriginal, compact, watermarkEnabled, watermarkText,
  headerEnabled, footerEnabled, headerText, footerText,
  onToggleHeader, onToggleFooter, onHeaderTextChange, onFooterTextChange,
  lineSpacing, margins, zoom = 100,
}) => {
  if (!editor) return null;

  const pageWidth = compact ? '100%' : '210mm';
  const defaultMargin = compact ? 10 : 20;
  const mt = compact ? Math.min(margins?.top ?? defaultMargin, 10) : (margins?.top ?? 20);
  const mr = compact ? Math.min(margins?.right ?? defaultMargin, 10) : (margins?.right ?? 20);
  const mb = compact ? Math.min(margins?.bottom ?? defaultMargin, 10) : (margins?.bottom ?? 20);
  const ml = compact ? Math.min(margins?.left ?? defaultMargin, 10) : (margins?.left ?? 20);

  const sharedPageProps = {
    compact,
    pageWidth,
    watermarkEnabled,
    watermarkText,
    headerEnabled,
    footerEnabled,
    headerText,
    footerText,
    onToggleHeader,
    onToggleFooter,
    onHeaderTextChange,
    onFooterTextChange,
    mt, mr, mb, ml,
    lineSpacing,
  };

  return (
    <div style={{ height: '100%', overflowY: 'auto', backgroundColor: F.DARK_BG }}>
      {hasOriginal && !compact && (
        <div style={{ maxWidth: pageWidth, margin: '12px auto 0' }}>
          <div
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              padding: '6px 12px',
              backgroundColor: `${F.ORANGE}15`,
              border: `1px solid ${F.ORANGE}40`,
              borderRadius: '2px',
              fontSize: '10px',
              color: F.ORANGE,
              fontFamily: '"IBM Plex Mono", monospace',
            }}
          >
            <AlertTriangle size={12} />
            Edit mode shows converted content. Use Preview to see original formatting.
          </div>
        </div>
      )}

      <div style={{
        padding: compact ? '6px' : '16px',
        transformOrigin: 'top center',
        transform: !compact && zoom !== 100 ? `scale(${zoom / 100})` : undefined,
        width: !compact && zoom !== 100 ? `${10000 / zoom}%` : undefined,
      }}>
        {/* Page 1 — contains the actual editor */}
        <A4Page {...sharedPageProps} isMainPage>
          <EditorContent editor={editor} />
        </A4Page>

        {/* Page separator + Page 2 (visual continuation) */}
        {!compact && (
          <>
            <PageGap pageNumber={2} compact={compact} />
            <A4Page {...sharedPageProps}>
              <div
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  minHeight: '80px',
                  color: '#ccc',
                  fontSize: '11px',
                  fontFamily: '"IBM Plex Mono", monospace',
                  fontStyle: 'italic',
                }}
              >
                Content overflows here automatically...
              </div>
            </A4Page>

            <PageGap pageNumber={3} compact={compact} />
            <A4Page {...sharedPageProps}>
              <div
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  minHeight: '80px',
                  color: '#ddd',
                  fontSize: '10px',
                  fontFamily: '"IBM Plex Mono", monospace',
                  fontStyle: 'italic',
                }}
              >
                &nbsp;
              </div>
            </A4Page>
          </>
        )}
      </div>
    </div>
  );
};

// ── Main Canvas Component ──────────────────────────────────────────
export const DocxEditorCanvas: React.FC<DocxEditorCanvasProps> = ({
  editor,
  viewMode,
  originalArrayBuffer,
  watermarkText,
  watermarkEnabled,
  headerText,
  footerText,
  headerEnabled,
  footerEnabled,
  onToggleHeader,
  onToggleFooter,
  onHeaderTextChange,
  onFooterTextChange,
  lineSpacing,
  margins,
  zoom = 100,
}) => {
  const editProps = {
    editor,
    hasOriginal: !!originalArrayBuffer,
    watermarkEnabled,
    watermarkText,
    headerEnabled,
    footerEnabled,
    headerText,
    footerText,
    onToggleHeader,
    onToggleFooter,
    onHeaderTextChange,
    onFooterTextChange,
    lineSpacing,
    margins,
    zoom,
  };

  if (viewMode === 'edit') {
    return <DocxEditPanel {...editProps} />;
  }

  if (viewMode === 'preview') {
    return <DocxPreviewPanel arrayBuffer={originalArrayBuffer} />;
  }

  // Split view — each child scrolls independently
  return (
    <div style={{ display: 'flex', height: '100%', overflow: 'hidden' }}>
      <div style={{ flex: 1, minWidth: 0, borderRight: `1px solid ${F.BORDER}`, height: '100%', overflow: 'auto' }}>
        <DocxEditPanel {...editProps} compact />
      </div>
      <div style={{ flex: 1, minWidth: 0, height: '100%', overflow: 'auto' }}>
        <DocxPreviewPanel arrayBuffer={originalArrayBuffer} compact />
      </div>
    </div>
  );
};

export default DocxEditorCanvas;
