import React, { useMemo } from 'react';
import {
  FileText,
  User,
  Building2,
  Calendar,
  Camera,
  RotateCcw,
  Trash2,
  Hash,
  Type,
  FileIcon,
  Info,
  Droplets,
  PanelTop,
  Maximize2,
} from 'lucide-react';
import { DocxPropertiesPanelProps } from './types';

// ── Fincept Design Tokens ──────────────────────────────────────────
const F = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
};

// ── Shared Styles ──────────────────────────────────────────────────
const inputStyle: React.CSSProperties = {
  width: '100%',
  padding: '6px 8px',
  backgroundColor: F.DARK_BG,
  color: F.WHITE,
  border: `1px solid ${F.BORDER}`,
  borderRadius: '2px',
  fontSize: '10px',
  fontFamily: '"IBM Plex Mono", "Consolas", monospace',
  outline: 'none',
};

const labelStyle: React.CSSProperties = {
  fontSize: '9px',
  fontWeight: 700,
  color: F.GRAY,
  letterSpacing: '0.5px',
  display: 'flex',
  alignItems: 'center',
  gap: '4px',
  marginBottom: '4px',
};

const sectionHeaderStyle: React.CSSProperties = {
  display: 'flex',
  alignItems: 'center',
  gap: '6px',
  marginBottom: '12px',
  color: F.ORANGE,
  fontSize: '9px',
  fontWeight: 700,
  letterSpacing: '0.5px',
  fontFamily: '"IBM Plex Mono", monospace',
};

const sectionStyle: React.CSSProperties = {
  padding: '12px',
  borderBottom: `1px solid ${F.BORDER}`,
};

const statBoxStyle: React.CSSProperties = {
  padding: '8px',
  backgroundColor: F.DARK_BG,
  border: `1px solid ${F.BORDER}`,
  borderRadius: '2px',
};

// ── Utility Functions ──────────────────────────────────────────────
const countWords = (html: string): number => {
  const text = html.replace(/<[^>]*>/g, ' ').replace(/\s+/g, ' ').trim();
  return text ? text.split(' ').length : 0;
};

const countChars = (html: string): number => {
  const text = html.replace(/<[^>]*>/g, '').trim();
  return text.length;
};

// ── Main Component ─────────────────────────────────────────────────
export const DocxPropertiesPanel: React.FC<DocxPropertiesPanelProps> = ({
  metadata,
  onMetadataChange,
  htmlContent,
  filePath,
  fileId,
  isDirty,
  onCreateSnapshot,
  onRestoreSnapshot,
  onDeleteSnapshot,
  snapshots,
  pageSettings,
  onPageSettingsChange,
}) => {
  const wordCount = useMemo(() => countWords(htmlContent), [htmlContent]);
  const charCount = useMemo(() => countChars(htmlContent), [htmlContent]);
  const pageEstimate = useMemo(() => Math.max(1, Math.ceil(wordCount / 300)), [wordCount]);

  return (
    <div
      style={{
        height: '100%',
        overflowY: 'auto',
        backgroundColor: F.PANEL_BG,
        fontFamily: '"IBM Plex Mono", "Consolas", monospace',
      }}
    >
      {/* Document Properties */}
      <div style={sectionStyle}>
        <div style={sectionHeaderStyle}>
          <FileText size={12} />
          DOCUMENT PROPERTIES
        </div>

        <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
          <div>
            <label style={labelStyle}><Type size={10} /> TITLE</label>
            <input
              type="text"
              value={metadata.title}
              onChange={(e) => onMetadataChange({ title: e.target.value })}
              placeholder="Document title"
              style={inputStyle}
              onFocus={(e) => { e.currentTarget.style.borderColor = F.ORANGE; }}
              onBlur={(e) => { e.currentTarget.style.borderColor = F.BORDER; }}
            />
          </div>

          <div>
            <label style={labelStyle}><User size={10} /> AUTHOR</label>
            <input
              type="text"
              value={metadata.author}
              onChange={(e) => onMetadataChange({ author: e.target.value })}
              placeholder="Author name"
              style={inputStyle}
              onFocus={(e) => { e.currentTarget.style.borderColor = F.ORANGE; }}
              onBlur={(e) => { e.currentTarget.style.borderColor = F.BORDER; }}
            />
          </div>

          <div>
            <label style={labelStyle}><Building2 size={10} /> COMPANY</label>
            <input
              type="text"
              value={metadata.company}
              onChange={(e) => onMetadataChange({ company: e.target.value })}
              placeholder="Company name"
              style={inputStyle}
              onFocus={(e) => { e.currentTarget.style.borderColor = F.ORANGE; }}
              onBlur={(e) => { e.currentTarget.style.borderColor = F.BORDER; }}
            />
          </div>

          <div>
            <label style={labelStyle}><Calendar size={10} /> DATE</label>
            <input
              type="date"
              value={metadata.date}
              onChange={(e) => onMetadataChange({ date: e.target.value })}
              style={{ ...inputStyle, colorScheme: 'dark' }}
              onFocus={(e) => { e.currentTarget.style.borderColor = F.ORANGE; }}
              onBlur={(e) => { e.currentTarget.style.borderColor = F.BORDER; }}
            />
          </div>
        </div>
      </div>

      {/* Statistics */}
      <div style={sectionStyle}>
        <div style={sectionHeaderStyle}>
          <Hash size={12} />
          STATISTICS
        </div>

        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '6px' }}>
          <div style={statBoxStyle}>
            <div style={{ fontSize: '9px', color: F.GRAY, letterSpacing: '0.5px' }}>WORDS</div>
            <div style={{ fontSize: '12px', fontWeight: 700, color: F.CYAN }}>{wordCount.toLocaleString()}</div>
          </div>
          <div style={statBoxStyle}>
            <div style={{ fontSize: '9px', color: F.GRAY, letterSpacing: '0.5px' }}>CHARS</div>
            <div style={{ fontSize: '12px', fontWeight: 700, color: F.CYAN }}>{charCount.toLocaleString()}</div>
          </div>
          <div style={statBoxStyle}>
            <div style={{ fontSize: '9px', color: F.GRAY, letterSpacing: '0.5px' }}>PAGES</div>
            <div style={{ fontSize: '12px', fontWeight: 700, color: F.CYAN }}>{pageEstimate}</div>
          </div>
          <div style={statBoxStyle}>
            <div style={{ fontSize: '9px', color: F.GRAY, letterSpacing: '0.5px' }}>STATUS</div>
            <div style={{ fontSize: '12px', fontWeight: 700, color: isDirty ? F.ORANGE : F.GREEN }}>
              {isDirty ? 'Modified' : 'Saved'}
            </div>
          </div>
        </div>
      </div>

      {/* File Info */}
      {filePath && (
        <div style={sectionStyle}>
          <div style={sectionHeaderStyle}>
            <Info size={12} />
            FILE INFO
          </div>
          <div
            style={{
              padding: '8px',
              backgroundColor: F.DARK_BG,
              border: `1px solid ${F.BORDER}`,
              borderRadius: '2px',
              fontSize: '9px',
              color: F.GRAY,
              wordBreak: 'break-all',
              display: 'flex',
              alignItems: 'flex-start',
              gap: '4px',
            }}
          >
            <FileIcon size={10} style={{ flexShrink: 0, marginTop: '1px' }} />
            {filePath}
          </div>
        </div>
      )}

      {/* Watermark */}
      <div style={sectionStyle}>
        <div style={sectionHeaderStyle}>
          <Droplets size={12} />
          WATERMARK
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
          <label
            style={{ display: 'flex', alignItems: 'center', gap: '8px', cursor: 'pointer', fontSize: '10px', color: F.WHITE }}
          >
            <input
              type="checkbox"
              checked={pageSettings.watermarkEnabled}
              onChange={() => onPageSettingsChange({ watermarkEnabled: !pageSettings.watermarkEnabled })}
              style={{ accentColor: F.ORANGE }}
            />
            Enable watermark
          </label>
          {pageSettings.watermarkEnabled && (
            <input
              type="text"
              value={pageSettings.watermarkText}
              onChange={(e) => onPageSettingsChange({ watermarkText: e.target.value })}
              placeholder="Watermark text"
              style={inputStyle}
              onFocus={(e) => { e.currentTarget.style.borderColor = F.ORANGE; }}
              onBlur={(e) => { e.currentTarget.style.borderColor = F.BORDER; }}
            />
          )}
        </div>
      </div>

      {/* Header & Footer */}
      <div style={sectionStyle}>
        <div style={sectionHeaderStyle}>
          <PanelTop size={12} />
          HEADER & FOOTER
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
          <label
            style={{ display: 'flex', alignItems: 'center', gap: '8px', cursor: 'pointer', fontSize: '10px', color: F.WHITE }}
          >
            <input
              type="checkbox"
              checked={pageSettings.headerEnabled}
              onChange={() => onPageSettingsChange({ headerEnabled: !pageSettings.headerEnabled })}
              style={{ accentColor: F.ORANGE }}
            />
            Enable header
          </label>
          {pageSettings.headerEnabled && (
            <input
              type="text"
              value={pageSettings.headerText}
              onChange={(e) => onPageSettingsChange({ headerText: e.target.value })}
              placeholder="Header text"
              style={inputStyle}
              onFocus={(e) => { e.currentTarget.style.borderColor = F.ORANGE; }}
              onBlur={(e) => { e.currentTarget.style.borderColor = F.BORDER; }}
            />
          )}
          <label
            style={{ display: 'flex', alignItems: 'center', gap: '8px', cursor: 'pointer', fontSize: '10px', color: F.WHITE }}
          >
            <input
              type="checkbox"
              checked={pageSettings.footerEnabled}
              onChange={() => onPageSettingsChange({ footerEnabled: !pageSettings.footerEnabled })}
              style={{ accentColor: F.ORANGE }}
            />
            Enable footer
          </label>
          {pageSettings.footerEnabled && (
            <input
              type="text"
              value={pageSettings.footerText}
              onChange={(e) => onPageSettingsChange({ footerText: e.target.value })}
              placeholder="Footer text"
              style={inputStyle}
              onFocus={(e) => { e.currentTarget.style.borderColor = F.ORANGE; }}
              onBlur={(e) => { e.currentTarget.style.borderColor = F.BORDER; }}
            />
          )}
        </div>
      </div>

      {/* Page Margins */}
      <div style={sectionStyle}>
        <div style={sectionHeaderStyle}>
          <Maximize2 size={12} />
          PAGE MARGINS (mm)
        </div>
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '6px' }}>
          {(['top', 'right', 'bottom', 'left'] as const).map((side) => (
            <div key={side}>
              <label style={{ ...labelStyle, marginBottom: '2px' }}>
                {side.toUpperCase()}
              </label>
              <input
                type="number"
                min={0}
                max={50}
                value={pageSettings.margins[side]}
                onChange={(e) => {
                  const val = parseInt(e.target.value, 10);
                  if (!isNaN(val)) {
                    onPageSettingsChange({ margins: { ...pageSettings.margins, [side]: val } });
                  }
                }}
                style={inputStyle}
                onFocus={(e) => { e.currentTarget.style.borderColor = F.ORANGE; }}
                onBlur={(e) => { e.currentTarget.style.borderColor = F.BORDER; }}
              />
            </div>
          ))}
        </div>
      </div>

      {/* Snapshots */}
      <div style={{ padding: '12px' }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '12px' }}>
          <div style={sectionHeaderStyle}>
            <Camera size={12} />
            SNAPSHOTS
          </div>
          <button
            onClick={onCreateSnapshot}
            style={{
              padding: '4px 8px',
              backgroundColor: F.ORANGE,
              color: F.DARK_BG,
              border: 'none',
              borderRadius: '2px',
              fontSize: '9px',
              fontWeight: 700,
              fontFamily: '"IBM Plex Mono", monospace',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              transition: 'all 0.2s',
            }}
            title="Create snapshot of current state"
          >
            <Camera size={10} />
            CREATE
          </button>
        </div>

        {snapshots.length === 0 ? (
          <div
            style={{
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              padding: '16px',
              color: F.MUTED,
              fontSize: '10px',
              textAlign: 'center',
            }}
          >
            {fileId ? 'No snapshots yet' : 'Save the file first to create snapshots'}
          </div>
        ) : (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '6px', maxHeight: '200px', overflowY: 'auto' }}>
            {snapshots.map((snap) => (
              <div
                key={snap.id}
                style={{
                  padding: '8px',
                  backgroundColor: F.DARK_BG,
                  border: `1px solid ${F.BORDER}`,
                  borderRadius: '2px',
                }}
              >
                <div style={{ fontSize: '10px', fontWeight: 700, color: F.WHITE, marginBottom: '4px' }}>
                  {snap.snapshot_name}
                </div>
                <div style={{ fontSize: '9px', color: F.GRAY }}>
                  {new Date(snap.created_at).toLocaleString()}
                </div>
                <div style={{ display: 'flex', gap: '4px', marginTop: '6px' }}>
                  <button
                    onClick={() => onRestoreSnapshot(snap.id)}
                    style={{
                      padding: '3px 6px',
                      backgroundColor: `${F.GREEN}20`,
                      color: F.GREEN,
                      border: 'none',
                      borderRadius: '2px',
                      fontSize: '9px',
                      fontWeight: 700,
                      cursor: 'pointer',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '3px',
                      transition: 'all 0.2s',
                    }}
                  >
                    <RotateCcw size={9} />
                    RESTORE
                  </button>
                  <button
                    onClick={() => onDeleteSnapshot(snap.id)}
                    style={{
                      padding: '3px 6px',
                      backgroundColor: `${F.RED}20`,
                      color: F.RED,
                      border: 'none',
                      borderRadius: '2px',
                      fontSize: '9px',
                      fontWeight: 700,
                      cursor: 'pointer',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '3px',
                      transition: 'all 0.2s',
                    }}
                  >
                    <Trash2 size={9} />
                    DELETE
                  </button>
                </div>
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
};

export default DocxPropertiesPanel;
