import React, { useCallback } from 'react';
import {
  Bold,
  Italic,
  Underline as UnderlineIcon,
  Strikethrough,
  AlignLeft,
  AlignCenter,
  AlignRight,
  AlignJustify,
  List,
  ListOrdered,
  Image as ImageIcon,
  Table as TableIcon,
  Link as LinkIcon,
  Minus,
  Type,
  Undo2,
  Redo2,
  Eye,
  Edit3,
  Columns,
  Palette,
  Highlighter,
  ChevronDown,
  Unlink,
  Trash2,
  Plus,
  Superscript,
  Subscript,
  Quote,
  Code,
  IndentIncrease,
  IndentDecrease,
  RemoveFormatting,
  SeparatorHorizontal,
  Droplets,
  ArrowUpDown,
  PanelTop,
  PanelBottom,
  ZoomIn,
  ZoomOut,
} from 'lucide-react';
import { DocxEditorToolbarProps } from './types';
import { FONT_FAMILIES, PRESET_COLORS } from './constants';

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
};

const FONT_SIZES = [
  '8', '9', '10', '10.5', '11', '12', '14', '16', '18', '20', '24', '28', '32', '36', '48', '72',
];

const HEADING_OPTIONS = [
  { level: 0, label: 'Normal' },
  { level: 1, label: 'Heading 1' },
  { level: 2, label: 'Heading 2' },
  { level: 3, label: 'Heading 3' },
  { level: 4, label: 'Heading 4' },
  { level: 5, label: 'Heading 5' },
  { level: 6, label: 'Heading 6' },
];

const LINE_SPACING_OPTIONS = [
  { value: '1', label: '1.0' },
  { value: '1.15', label: '1.15' },
  { value: '1.5', label: '1.5' },
  { value: '2', label: '2.0' },
  { value: '2.5', label: '2.5' },
  { value: '3', label: '3.0' },
];

// ── Toolbar Button ─────────────────────────────────────────────────
const Btn: React.FC<{
  onClick: () => void;
  isActive?: boolean;
  disabled?: boolean;
  title: string;
  children: React.ReactNode;
}> = ({ onClick, isActive, disabled, title, children }) => (
  <button
    onClick={onClick}
    disabled={disabled}
    title={title}
    style={{
      padding: '4px',
      borderRadius: '2px',
      border: 'none',
      backgroundColor: isActive ? F.ORANGE : 'transparent',
      color: isActive ? F.DARK_BG : F.WHITE,
      cursor: disabled ? 'default' : 'pointer',
      opacity: disabled ? 0.3 : 1,
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      transition: 'all 0.2s',
    }}
    onMouseEnter={(e) => { if (!isActive && !disabled) e.currentTarget.style.backgroundColor = F.HOVER; }}
    onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = isActive ? F.ORANGE : 'transparent'; }}
  >
    {children}
  </button>
);

// ── Separator ──────────────────────────────────────────────────────
const Sep: React.FC = () => (
  <div style={{ width: '1px', height: '16px', backgroundColor: F.BORDER, margin: '0 2px', flexShrink: 0 }} />
);

// ── Dropdown ───────────────────────────────────────────────────────
const Dropdown: React.FC<{
  trigger: React.ReactNode;
  open: boolean;
  onToggle: () => void;
  children: React.ReactNode;
  align?: 'left' | 'right';
}> = ({ trigger, open, onToggle, children, align = 'left' }) => {
  const ref = React.useRef<HTMLDivElement>(null);
  React.useEffect(() => {
    if (!open) return;
    const handler = (e: MouseEvent) => {
      if (ref.current && !ref.current.contains(e.target as Node)) onToggle();
    };
    document.addEventListener('mousedown', handler);
    return () => document.removeEventListener('mousedown', handler);
  }, [open, onToggle]);

  return (
    <div ref={ref} style={{ position: 'relative' }}>
      <div onClick={onToggle}>{trigger}</div>
      {open && (
        <div
          style={{
            position: 'absolute',
            top: '100%',
            marginTop: '4px',
            padding: '4px 0',
            borderRadius: '2px',
            boxShadow: '0 4px 12px rgba(0,0,0,0.6)',
            border: `1px solid ${F.BORDER}`,
            backgroundColor: F.HEADER_BG,
            minWidth: '140px',
            zIndex: 50,
            [align === 'right' ? 'right' : 'left']: 0,
          }}
        >
          {children}
        </div>
      )}
    </div>
  );
};

// ── Dropdown Menu Item ─────────────────────────────────────────────
const MenuItem: React.FC<{
  onClick: () => void;
  color?: string;
  children: React.ReactNode;
}> = ({ onClick, color, children }) => (
  <button
    onClick={onClick}
    style={{
      width: '100%',
      textAlign: 'left',
      padding: '6px 12px',
      fontSize: '10px',
      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
      color: color || F.WHITE,
      backgroundColor: 'transparent',
      border: 'none',
      cursor: 'pointer',
      display: 'flex',
      alignItems: 'center',
      gap: '8px',
      transition: 'all 0.2s',
    }}
    onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = F.HOVER; }}
    onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = 'transparent'; }}
  >
    {children}
  </button>
);

const MenuDivider: React.FC = () => (
  <div style={{ borderTop: `1px solid ${F.BORDER}`, margin: '4px 0' }} />
);

// ── Color Picker ───────────────────────────────────────────────────
const ColorPicker: React.FC<{
  currentColor: string;
  onColorChange: (color: string) => void;
  icon: React.ReactNode;
  title: string;
}> = ({ currentColor, onColorChange, icon, title }) => {
  const [open, setOpen] = React.useState(false);
  const ref = React.useRef<HTMLDivElement>(null);

  React.useEffect(() => {
    if (!open) return;
    const handler = (e: MouseEvent) => {
      if (ref.current && !ref.current.contains(e.target as Node)) setOpen(false);
    };
    document.addEventListener('mousedown', handler);
    return () => document.removeEventListener('mousedown', handler);
  }, [open]);

  return (
    <div ref={ref} style={{ position: 'relative' }}>
      <button
        onClick={() => setOpen(!open)}
        title={title}
        style={{
          padding: '4px',
          borderRadius: '2px',
          border: 'none',
          backgroundColor: 'transparent',
          color: F.GRAY,
          cursor: 'pointer',
          display: 'flex',
          alignItems: 'center',
          gap: '2px',
          transition: 'all 0.2s',
        }}
        onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = F.HOVER; }}
        onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = 'transparent'; }}
      >
        {icon}
        <div style={{ width: '12px', height: '4px', borderRadius: '1px', backgroundColor: currentColor || '#000' }} />
      </button>
      {open && (
        <div
          style={{
            position: 'absolute',
            top: '100%',
            left: 0,
            marginTop: '4px',
            padding: '8px',
            borderRadius: '2px',
            boxShadow: '0 4px 12px rgba(0,0,0,0.6)',
            border: `1px solid ${F.BORDER}`,
            backgroundColor: F.HEADER_BG,
            width: '180px',
            zIndex: 50,
          }}
        >
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(8, 1fr)', gap: '4px' }}>
            {PRESET_COLORS.map((color) => (
              <button
                key={color}
                onClick={() => { onColorChange(color); setOpen(false); }}
                style={{
                  width: '16px',
                  height: '16px',
                  borderRadius: '2px',
                  border: `1px solid ${F.MUTED}`,
                  backgroundColor: color,
                  cursor: 'pointer',
                  transition: 'transform 0.15s',
                }}
                title={color}
                onMouseEnter={(e) => { e.currentTarget.style.transform = 'scale(1.2)'; }}
                onMouseLeave={(e) => { e.currentTarget.style.transform = 'scale(1)'; }}
              />
            ))}
          </div>
          <div style={{ marginTop: '8px', display: 'flex', alignItems: 'center', gap: '8px' }}>
            <input
              type="color"
              value={currentColor || '#000000'}
              onChange={(e) => { onColorChange(e.target.value); setOpen(false); }}
              style={{ width: '20px', height: '20px', cursor: 'pointer', border: 'none', background: 'transparent' }}
            />
            <span style={{ fontSize: '9px', color: F.GRAY, fontFamily: '"IBM Plex Mono", monospace' }}>CUSTOM</span>
          </div>
        </div>
      )}
    </div>
  );
};

// ── Select styling (fixes white bg + white text issue) ─────────────
const selectStyle: React.CSSProperties = {
  backgroundColor: F.DARK_BG,
  color: F.WHITE,
  border: `1px solid ${F.BORDER}`,
  borderRadius: '2px',
  padding: '2px 4px',
  fontSize: '10px',
  fontFamily: '"IBM Plex Mono", "Consolas", monospace',
  cursor: 'pointer',
  outline: 'none',
};

// ── Main Toolbar Component ─────────────────────────────────────────
export const DocxEditorToolbar: React.FC<DocxEditorToolbarProps> = ({
  editor,
  viewMode,
  onViewModeChange,
  onInsertImage,
  onToggleWatermark,
  watermarkEnabled,
  onToggleHeader,
  onToggleFooter,
  headerEnabled,
  footerEnabled,
  lineSpacing,
  onLineSpacingChange,
  zoom,
  onZoomChange,
}) => {
  const [showHeadingMenu, setShowHeadingMenu] = React.useState(false);
  const [showTableMenu, setShowTableMenu] = React.useState(false);
  const [showInsertMenu, setShowInsertMenu] = React.useState(false);
  const [showSpacingMenu, setShowSpacingMenu] = React.useState(false);

  const setLink = useCallback(() => {
    if (!editor) return;
    const previousUrl = editor.getAttributes('link').href;
    const url = window.prompt('URL', previousUrl);
    if (url === null) return;
    if (url === '') {
      editor.chain().focus().extendMarkRange('link').unsetLink().run();
      return;
    }
    editor.chain().focus().extendMarkRange('link').setLink({ href: url }).run();
  }, [editor]);

  if (!editor) return null;

  const currentHeading = HEADING_OPTIONS.find(h =>
    h.level > 0 ? editor.isActive('heading', { level: h.level }) : !editor.isActive('heading')
  ) || HEADING_OPTIONS[0];

  return (
    <div
      style={{
        display: 'flex',
        flexWrap: 'wrap',
        alignItems: 'center',
        gap: '2px',
        padding: '6px 12px',
        backgroundColor: F.HEADER_BG,
        borderBottom: `1px solid ${F.BORDER}`,
        flexShrink: 0,
        fontFamily: '"IBM Plex Mono", "Consolas", monospace',
      }}
    >
      {/* Undo / Redo */}
      <Btn onClick={() => editor.chain().focus().undo().run()} disabled={!editor.can().undo()} title="Undo (Ctrl+Z)">
        <Undo2 size={13} />
      </Btn>
      <Btn onClick={() => editor.chain().focus().redo().run()} disabled={!editor.can().redo()} title="Redo (Ctrl+Y)">
        <Redo2 size={13} />
      </Btn>

      <Sep />

      {/* Heading dropdown */}
      <Dropdown
        open={showHeadingMenu}
        onToggle={() => setShowHeadingMenu(!showHeadingMenu)}
        trigger={
          <button
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              padding: '2px 6px',
              borderRadius: '2px',
              border: `1px solid ${F.BORDER}`,
              backgroundColor: 'transparent',
              color: F.WHITE,
              fontSize: '10px',
              fontFamily: '"IBM Plex Mono", monospace',
              cursor: 'pointer',
              minWidth: '80px',
              transition: 'all 0.2s',
            }}
            onMouseEnter={(e) => { e.currentTarget.style.borderColor = F.ORANGE; }}
            onMouseLeave={(e) => { e.currentTarget.style.borderColor = F.BORDER; }}
            title="Paragraph style"
          >
            <Type size={11} />
            <span style={{ overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{currentHeading.label}</span>
            <ChevronDown size={9} />
          </button>
        }
      >
        {HEADING_OPTIONS.map((opt) => (
          <button
            key={opt.level}
            onClick={() => {
              if (opt.level === 0) editor.chain().focus().setParagraph().run();
              else editor.chain().focus().toggleHeading({ level: opt.level as 1|2|3|4|5|6 }).run();
              setShowHeadingMenu(false);
            }}
            style={{
              width: '100%',
              textAlign: 'left',
              padding: '6px 12px',
              fontSize: opt.level === 0 ? '10px' : `${15 - opt.level}px`,
              fontWeight: opt.level > 0 ? 700 : 400,
              fontFamily: '"IBM Plex Mono", monospace',
              color: currentHeading.level === opt.level ? F.ORANGE : F.WHITE,
              backgroundColor: 'transparent',
              border: 'none',
              cursor: 'pointer',
              transition: 'all 0.2s',
            }}
            onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = F.HOVER; }}
            onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = 'transparent'; }}
          >
            {opt.label}
          </button>
        ))}
      </Dropdown>

      {/* Font family */}
      <select
        value={editor.getAttributes('textStyle').fontFamily || ''}
        onChange={(e) => {
          if (e.target.value) editor.chain().focus().setFontFamily(e.target.value).run();
          else editor.chain().focus().unsetFontFamily().run();
        }}
        style={{ ...selectStyle, maxWidth: '90px' }}
        title="Font family"
      >
        <option value="" style={{ backgroundColor: F.DARK_BG, color: F.WHITE }}>Default</option>
        {FONT_FAMILIES.map((f) => (
          <option key={f.value} value={f.value} style={{ backgroundColor: F.DARK_BG, color: F.WHITE }}>{f.label}</option>
        ))}
      </select>

      {/* Font size */}
      <select
        value="11"
        onChange={(e) => {
          editor.chain().focus().setMark('textStyle', { fontSize: `${e.target.value}pt` } as any).run();
        }}
        style={{ ...selectStyle, width: '42px' }}
        title="Font size"
      >
        {FONT_SIZES.map((s) => (
          <option key={s} value={s} style={{ backgroundColor: F.DARK_BG, color: F.WHITE }}>{s}</option>
        ))}
      </select>

      <Sep />

      {/* Text formatting */}
      <Btn onClick={() => editor.chain().focus().toggleBold().run()} isActive={editor.isActive('bold')} title="Bold (Ctrl+B)">
        <Bold size={13} />
      </Btn>
      <Btn onClick={() => editor.chain().focus().toggleItalic().run()} isActive={editor.isActive('italic')} title="Italic (Ctrl+I)">
        <Italic size={13} />
      </Btn>
      <Btn onClick={() => editor.chain().focus().toggleUnderline().run()} isActive={editor.isActive('underline')} title="Underline (Ctrl+U)">
        <UnderlineIcon size={13} />
      </Btn>
      <Btn onClick={() => editor.chain().focus().toggleStrike().run()} isActive={editor.isActive('strike')} title="Strikethrough">
        <Strikethrough size={13} />
      </Btn>
      <Btn onClick={() => editor.chain().focus().toggleSuperscript().run()} isActive={editor.isActive('superscript')} title="Superscript">
        <Superscript size={13} />
      </Btn>
      <Btn onClick={() => editor.chain().focus().toggleSubscript().run()} isActive={editor.isActive('subscript')} title="Subscript">
        <Subscript size={13} />
      </Btn>

      {/* Colors */}
      <ColorPicker
        currentColor={editor.getAttributes('textStyle').color || '#000000'}
        onColorChange={(color) => editor.chain().focus().setColor(color).run()}
        icon={<Palette size={13} />}
        title="Text color"
      />
      <ColorPicker
        currentColor={editor.getAttributes('highlight').color || '#ffff00'}
        onColorChange={(color) => editor.chain().focus().toggleHighlight({ color }).run()}
        icon={<Highlighter size={13} />}
        title="Highlight color"
      />

      {/* Clear formatting */}
      <Btn
        onClick={() => { editor.chain().focus().unsetAllMarks().run(); editor.chain().focus().clearNodes().run(); }}
        title="Clear formatting"
      >
        <RemoveFormatting size={13} />
      </Btn>

      <Sep />

      {/* Alignment */}
      <Btn onClick={() => editor.chain().focus().setTextAlign('left').run()} isActive={editor.isActive({ textAlign: 'left' })} title="Align left">
        <AlignLeft size={13} />
      </Btn>
      <Btn onClick={() => editor.chain().focus().setTextAlign('center').run()} isActive={editor.isActive({ textAlign: 'center' })} title="Align center">
        <AlignCenter size={13} />
      </Btn>
      <Btn onClick={() => editor.chain().focus().setTextAlign('right').run()} isActive={editor.isActive({ textAlign: 'right' })} title="Align right">
        <AlignRight size={13} />
      </Btn>
      <Btn onClick={() => editor.chain().focus().setTextAlign('justify').run()} isActive={editor.isActive({ textAlign: 'justify' })} title="Justify">
        <AlignJustify size={13} />
      </Btn>

      {/* Line spacing dropdown */}
      <Dropdown
        open={showSpacingMenu}
        onToggle={() => setShowSpacingMenu(!showSpacingMenu)}
        trigger={
          <Btn onClick={() => {}} title="Line spacing">
            <ArrowUpDown size={13} />
          </Btn>
        }
      >
        {LINE_SPACING_OPTIONS.map((opt) => (
          <MenuItem
            key={opt.value}
            onClick={() => { onLineSpacingChange?.(opt.value); setShowSpacingMenu(false); }}
            color={lineSpacing === opt.value ? F.ORANGE : F.WHITE}
          >
            {opt.label}
          </MenuItem>
        ))}
      </Dropdown>

      <Sep />

      {/* Lists & Indent */}
      <Btn onClick={() => editor.chain().focus().toggleBulletList().run()} isActive={editor.isActive('bulletList')} title="Bullet list">
        <List size={13} />
      </Btn>
      <Btn onClick={() => editor.chain().focus().toggleOrderedList().run()} isActive={editor.isActive('orderedList')} title="Numbered list">
        <ListOrdered size={13} />
      </Btn>
      <Btn onClick={() => editor.chain().focus().sinkListItem('listItem').run()} disabled={!editor.can().sinkListItem('listItem')} title="Increase indent">
        <IndentIncrease size={13} />
      </Btn>
      <Btn onClick={() => editor.chain().focus().liftListItem('listItem').run()} disabled={!editor.can().liftListItem('listItem')} title="Decrease indent">
        <IndentDecrease size={13} />
      </Btn>

      <Sep />

      {/* Blockquote & Code */}
      <Btn onClick={() => editor.chain().focus().toggleBlockquote().run()} isActive={editor.isActive('blockquote')} title="Blockquote">
        <Quote size={13} />
      </Btn>
      <Btn onClick={() => editor.chain().focus().toggleCodeBlock().run()} isActive={editor.isActive('codeBlock')} title="Code block">
        <Code size={13} />
      </Btn>

      <Sep />

      {/* Insert menu */}
      <Dropdown
        open={showInsertMenu}
        onToggle={() => setShowInsertMenu(!showInsertMenu)}
        trigger={
          <button
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: '2px',
              padding: '2px 6px',
              borderRadius: '2px',
              border: 'none',
              backgroundColor: 'transparent',
              color: F.WHITE,
              fontSize: '9px',
              fontWeight: 700,
              fontFamily: '"IBM Plex Mono", monospace',
              letterSpacing: '0.5px',
              cursor: 'pointer',
              transition: 'all 0.2s',
            }}
            onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = F.HOVER; }}
            onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = 'transparent'; }}
            title="Insert"
          >
            <Plus size={11} />
            INSERT
            <ChevronDown size={9} />
          </button>
        }
      >
        <MenuItem onClick={() => { onInsertImage(); setShowInsertMenu(false); }}>
          <ImageIcon size={12} /> Image
        </MenuItem>
        <MenuItem onClick={() => { setLink(); setShowInsertMenu(false); }}>
          <LinkIcon size={12} /> Link
        </MenuItem>
        {editor.isActive('link') && (
          <MenuItem onClick={() => { editor.chain().focus().unsetLink().run(); setShowInsertMenu(false); }} color={F.RED}>
            <Unlink size={12} /> Remove Link
          </MenuItem>
        )}
        <MenuDivider />
        <MenuItem onClick={() => { editor.chain().focus().setHorizontalRule().run(); setShowInsertMenu(false); }}>
          <Minus size={12} /> Horizontal Rule
        </MenuItem>
        <MenuItem onClick={() => { editor.chain().focus().setHardBreak().run(); setShowInsertMenu(false); }}>
          <SeparatorHorizontal size={12} /> Page Break
        </MenuItem>
        <MenuDivider />
        <MenuItem onClick={() => { onToggleHeader?.(); setShowInsertMenu(false); }} color={headerEnabled ? F.ORANGE : F.WHITE}>
          <PanelTop size={12} /> Header {headerEnabled ? '(On)' : '(Off)'}
        </MenuItem>
        <MenuItem onClick={() => { onToggleFooter?.(); setShowInsertMenu(false); }} color={footerEnabled ? F.ORANGE : F.WHITE}>
          <PanelBottom size={12} /> Footer {footerEnabled ? '(On)' : '(Off)'}
        </MenuItem>
        <MenuDivider />
        <MenuItem onClick={() => { onToggleWatermark?.(); setShowInsertMenu(false); }} color={watermarkEnabled ? F.ORANGE : F.WHITE}>
          <Droplets size={12} /> Watermark {watermarkEnabled ? '(On)' : '(Off)'}
        </MenuItem>
      </Dropdown>

      {/* Table dropdown */}
      <Dropdown
        open={showTableMenu}
        onToggle={() => setShowTableMenu(!showTableMenu)}
        trigger={
          <button
            style={{
              padding: '4px',
              borderRadius: '2px',
              border: 'none',
              backgroundColor: 'transparent',
              color: editor.isActive('table') ? F.ORANGE : F.WHITE,
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '2px',
              transition: 'all 0.2s',
            }}
            onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = F.HOVER; }}
            onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = 'transparent'; }}
            title="Table"
          >
            <TableIcon size={13} />
            <ChevronDown size={8} />
          </button>
        }
      >
        <MenuItem onClick={() => { editor.chain().focus().insertTable({ rows: 3, cols: 3, withHeaderRow: true }).run(); setShowTableMenu(false); }}>
          <Plus size={11} /> Insert Table (3x3)
        </MenuItem>
        <MenuItem onClick={() => { editor.chain().focus().insertTable({ rows: 5, cols: 5, withHeaderRow: true }).run(); setShowTableMenu(false); }}>
          <Plus size={11} /> Insert Table (5x5)
        </MenuItem>
        {editor.isActive('table') && (
          <>
            <MenuDivider />
            <MenuItem onClick={() => { editor.chain().focus().addColumnBefore().run(); setShowTableMenu(false); }}>
              Add Column Before
            </MenuItem>
            <MenuItem onClick={() => { editor.chain().focus().addColumnAfter().run(); setShowTableMenu(false); }}>
              Add Column After
            </MenuItem>
            <MenuItem onClick={() => { editor.chain().focus().addRowBefore().run(); setShowTableMenu(false); }}>
              Add Row Before
            </MenuItem>
            <MenuItem onClick={() => { editor.chain().focus().addRowAfter().run(); setShowTableMenu(false); }}>
              Add Row After
            </MenuItem>
            <MenuItem onClick={() => { editor.chain().focus().mergeCells().run(); setShowTableMenu(false); }}>
              Merge Cells
            </MenuItem>
            <MenuItem onClick={() => { editor.chain().focus().splitCell().run(); setShowTableMenu(false); }}>
              Split Cell
            </MenuItem>
            <MenuDivider />
            <MenuItem onClick={() => { editor.chain().focus().deleteColumn().run(); setShowTableMenu(false); }} color={F.RED}>
              Delete Column
            </MenuItem>
            <MenuItem onClick={() => { editor.chain().focus().deleteRow().run(); setShowTableMenu(false); }} color={F.RED}>
              Delete Row
            </MenuItem>
            <MenuItem onClick={() => { editor.chain().focus().deleteTable().run(); setShowTableMenu(false); }} color={F.RED}>
              <Trash2 size={11} /> Delete Table
            </MenuItem>
          </>
        )}
      </Dropdown>

      <Sep />

      {/* Header / Footer / Watermark toggles — always visible */}
      <Btn
        onClick={() => onToggleHeader?.()}
        isActive={headerEnabled}
        title={headerEnabled ? 'Header (On) — click to disable' : 'Header (Off) — click to enable'}
      >
        <PanelTop size={13} />
      </Btn>
      <Btn
        onClick={() => onToggleFooter?.()}
        isActive={footerEnabled}
        title={footerEnabled ? 'Footer (On) — click to disable' : 'Footer (Off) — click to enable'}
      >
        <PanelBottom size={13} />
      </Btn>
      <Btn
        onClick={() => onToggleWatermark?.()}
        isActive={watermarkEnabled}
        title={watermarkEnabled ? 'Watermark (On) — click to disable' : 'Watermark (Off) — click to enable'}
      >
        <Droplets size={13} />
      </Btn>

      <Sep />

      {/* Zoom controls */}
      <Btn onClick={() => onZoomChange(Math.max(25, zoom - 10))} title="Zoom out (−10%)">
        <ZoomOut size={13} />
      </Btn>
      <button
        onClick={() => onZoomChange(100)}
        title="Reset zoom to 100%"
        style={{
          padding: '2px 4px',
          borderRadius: '2px',
          border: `1px solid ${F.BORDER}`,
          backgroundColor: 'transparent',
          color: F.WHITE,
          fontSize: '9px',
          fontFamily: '"IBM Plex Mono", monospace',
          cursor: 'pointer',
          minWidth: '38px',
          textAlign: 'center',
          transition: 'all 0.2s',
        }}
        onMouseEnter={(e) => { e.currentTarget.style.borderColor = F.ORANGE; }}
        onMouseLeave={(e) => { e.currentTarget.style.borderColor = F.BORDER; }}
      >
        {zoom}%
      </button>
      <Btn onClick={() => onZoomChange(Math.min(200, zoom + 10))} title="Zoom in (+10%)">
        <ZoomIn size={13} />
      </Btn>

      {/* Spacer */}
      <div style={{ flex: 1, minWidth: '8px' }} />

      {/* View mode toggle */}
      <div style={{ display: 'flex', alignItems: 'center', border: `1px solid ${F.BORDER}`, borderRadius: '2px', overflow: 'hidden', flexShrink: 0 }}>
        {([
          { mode: 'edit' as const, icon: <Edit3 size={9} />, label: 'EDIT' },
          { mode: 'preview' as const, icon: <Eye size={9} />, label: 'PREVIEW' },
          { mode: 'split' as const, icon: <Columns size={9} />, label: 'SPLIT' },
        ]).map(({ mode, icon, label }) => (
          <button
            key={mode}
            onClick={() => onViewModeChange(mode)}
            style={{
              padding: '4px 8px',
              fontSize: '9px',
              fontWeight: 700,
              fontFamily: '"IBM Plex Mono", monospace',
              letterSpacing: '0.5px',
              border: 'none',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '3px',
              backgroundColor: viewMode === mode ? F.ORANGE : 'transparent',
              color: viewMode === mode ? F.DARK_BG : F.GRAY,
              transition: 'all 0.2s',
            }}
            title={`${label} mode`}
          >
            {icon} {label}
          </button>
        ))}
      </div>
    </div>
  );
};

export default DocxEditorToolbar;
