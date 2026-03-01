import React, { useState, useEffect } from 'react';
import {
  AlignLeft,
  AlignCenter,
  AlignRight,
  AlignJustify,
  Copy,
  Check,
} from 'lucide-react';
import {
  brandKitService,
  BrandKit,
  HeaderFooterTemplate,
  ParagraphStyles,
} from '@/services/core/brandKitService';
import { toast } from '@/components/ui/terminal-toast';
import { useTerminalTheme } from '@/contexts/ThemeContext';

// ============ Header/Footer Tab ============
export interface HeaderFooterTabProps {
  brandKit: BrandKit | null;
  onUpdate: (header: HeaderFooterTemplate, footer: HeaderFooterTemplate) => void;
}

export const HeaderFooterTab: React.FC<HeaderFooterTabProps> = ({ brandKit, onUpdate }) => {
  const { colors } = useTerminalTheme();
  const [editingSection, setEditingSection] = useState<'header' | 'footer'>('header');

  if (!brandKit) {
    return (
      <div className="text-center py-8 text-gray-500 text-xs">
        Select a brand kit first
      </div>
    );
  }

  const template = editingSection === 'header' ? brandKit.headerTemplate : brandKit.footerTemplate;

  const updateTemplate = (updates: Partial<HeaderFooterTemplate>) => {
    const newHeader = editingSection === 'header'
      ? { ...brandKit.headerTemplate!, ...updates }
      : brandKit.headerTemplate!;
    const newFooter = editingSection === 'footer'
      ? { ...brandKit.footerTemplate!, ...updates }
      : brandKit.footerTemplate!;
    onUpdate(newHeader, newFooter);
  };

  return (
    <div className="space-y-4">
      <label className="text-xs font-semibold" style={{ color: colors.primary }}>
        HEADER & FOOTER
      </label>

      {/* Section Toggle */}
      <div className="flex gap-2">
        <button
          onClick={() => setEditingSection('header')}
          className={`flex-1 py-2 text-xs rounded transition-colors ${
            editingSection === 'header'
              ? 'bg-[#FFA500] text-black font-semibold'
              : 'bg-[#0a0a0a] text-gray-400'
          }`}
        >
          Header
        </button>
        <button
          onClick={() => setEditingSection('footer')}
          className={`flex-1 py-2 text-xs rounded transition-colors ${
            editingSection === 'footer'
              ? 'bg-[#FFA500] text-black font-semibold'
              : 'bg-[#0a0a0a] text-gray-400'
          }`}
        >
          Footer
        </button>
      </div>

      {template && (
        <div className="space-y-3">
          {/* Enable Toggle */}
          <div className="flex items-center justify-between">
            <label className="text-xs text-gray-300">Enable {editingSection}</label>
            <button
              onClick={() => updateTemplate({ enabled: !template.enabled })}
              className={`w-10 h-5 rounded-full transition-colors ${
                template.enabled ? 'bg-[#FFA500]' : 'bg-[#333333]'
              }`}
            >
              <div
                className={`w-4 h-4 rounded-full bg-white transition-transform ${
                  template.enabled ? 'translate-x-5' : 'translate-x-0.5'
                }`}
              />
            </button>
          </div>

          {template.enabled && (
            <>
              {/* Content */}
              <div className="space-y-1">
                <label className="text-[10px] text-gray-500">
                  Content (use {'{company}'}, {'{date}'}, {'{title}'})
                </label>
                <input
                  type="text"
                  value={template.content}
                  onChange={(e) => updateTemplate({ content: e.target.value })}
                  className="w-full px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none text-white"
                  placeholder="Enter content..."
                />
              </div>

              {/* Alignment */}
              <div className="space-y-1">
                <label className="text-[10px] text-gray-500">Text Alignment</label>
                <div className="flex gap-2">
                  {(['left', 'center', 'right'] as const).map(align => (
                    <button
                      key={align}
                      onClick={() => updateTemplate({ alignment: align })}
                      className={`flex-1 py-2 rounded transition-colors ${
                        template.alignment === align
                          ? 'bg-[#FFA500] text-black'
                          : 'bg-[#0a0a0a] text-gray-400'
                      }`}
                    >
                      {align === 'left' && <AlignLeft size={14} className="mx-auto" />}
                      {align === 'center' && <AlignCenter size={14} className="mx-auto" />}
                      {align === 'right' && <AlignRight size={14} className="mx-auto" />}
                    </button>
                  ))}
                </div>
              </div>

              {/* Page Numbers */}
              <div className="space-y-2">
                <div className="flex items-center justify-between">
                  <label className="text-xs text-gray-300">Show Page Numbers</label>
                  <button
                    onClick={() => updateTemplate({ showPageNumbers: !template.showPageNumbers })}
                    className={`w-10 h-5 rounded-full transition-colors ${
                      template.showPageNumbers ? 'bg-[#FFA500]' : 'bg-[#333333]'
                    }`}
                  >
                    <div
                      className={`w-4 h-4 rounded-full bg-white transition-transform ${
                        template.showPageNumbers ? 'translate-x-5' : 'translate-x-0.5'
                      }`}
                    />
                  </button>
                </div>

                {template.showPageNumbers && (
                  <div className="grid grid-cols-2 gap-2">
                    <div>
                      <label className="text-[10px] text-gray-500">Format</label>
                      <select
                        value={template.pageNumberFormat}
                        onChange={(e) => updateTemplate({ pageNumberFormat: e.target.value as any })}
                        className="w-full px-2 py-1 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none text-white"
                      >
                        <option value="numeric">1, 2, 3...</option>
                        <option value="roman">I, II, III...</option>
                        <option value="alphanumeric">A, B, C...</option>
                      </select>
                    </div>
                    <div>
                      <label className="text-[10px] text-gray-500">Position</label>
                      <select
                        value={template.pageNumberPosition}
                        onChange={(e) => updateTemplate({ pageNumberPosition: e.target.value as any })}
                        className="w-full px-2 py-1 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none text-white"
                      >
                        <option value="left">Left</option>
                        <option value="center">Center</option>
                        <option value="right">Right</option>
                      </select>
                    </div>
                  </div>
                )}
              </div>

              {/* Colors */}
              <div className="grid grid-cols-2 gap-2">
                <div className="space-y-1">
                  <label className="text-[10px] text-gray-500">Background</label>
                  <div className="flex gap-2">
                    <input
                      type="color"
                      value={template.backgroundColor || '#FFFFFF'}
                      onChange={(e) => updateTemplate({ backgroundColor: e.target.value })}
                      className="w-8 h-8 rounded cursor-pointer"
                    />
                    <input
                      type="text"
                      value={template.backgroundColor || '#FFFFFF'}
                      onChange={(e) => updateTemplate({ backgroundColor: e.target.value })}
                      className="flex-1 px-2 py-1 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none text-white"
                    />
                  </div>
                </div>
                <div className="space-y-1">
                  <label className="text-[10px] text-gray-500">Text Color</label>
                  <div className="flex gap-2">
                    <input
                      type="color"
                      value={template.textColor || '#666666'}
                      onChange={(e) => updateTemplate({ textColor: e.target.value })}
                      className="w-8 h-8 rounded cursor-pointer"
                    />
                    <input
                      type="text"
                      value={template.textColor || '#666666'}
                      onChange={(e) => updateTemplate({ textColor: e.target.value })}
                      className="flex-1 px-2 py-1 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none text-white"
                    />
                  </div>
                </div>
              </div>

              {/* Font Size & Height */}
              <div className="grid grid-cols-2 gap-2">
                <div className="space-y-1">
                  <label className="text-[10px] text-gray-500">Font Size (pt)</label>
                  <input
                    type="text"
                    inputMode="numeric"
                    value={template.fontSize || 10}
                    onChange={(e) => {
                      const v = e.target.value;
                      if (v === '' || /^\d*\.?\d*$/.test(v)) {
                        updateTemplate({ fontSize: v === '' ? 10 : Number(v) });
                      }
                    }}
                    className="w-full px-2 py-1 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none text-white"
                  />
                </div>
                <div className="space-y-1">
                  <label className="text-[10px] text-gray-500">Height (px)</label>
                  <input
                    type="text"
                    inputMode="numeric"
                    value={template.height || 30}
                    onChange={(e) => {
                      const v = e.target.value;
                      if (v === '' || /^\d*\.?\d*$/.test(v)) {
                        updateTemplate({ height: v === '' ? 30 : Number(v) });
                      }
                    }}
                    className="w-full px-2 py-1 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none text-white"
                  />
                </div>
              </div>

              {/* Border */}
              <div className="flex items-center justify-between">
                <label className="text-xs text-gray-300">
                  {editingSection === 'header' ? 'Border Bottom' : 'Border Top'}
                </label>
                <button
                  onClick={() => {
                    if (editingSection === 'header') {
                      updateTemplate({ borderBottom: !template.borderBottom });
                    } else {
                      updateTemplate({ borderTop: !template.borderTop });
                    }
                  }}
                  className={`w-10 h-5 rounded-full transition-colors ${
                    (editingSection === 'header' ? template.borderBottom : template.borderTop)
                      ? 'bg-[#FFA500]'
                      : 'bg-[#333333]'
                  }`}
                >
                  <div
                    className={`w-4 h-4 rounded-full bg-white transition-transform ${
                      (editingSection === 'header' ? template.borderBottom : template.borderTop)
                        ? 'translate-x-5'
                        : 'translate-x-0.5'
                    }`}
                  />
                </button>
              </div>

              {/* Show Date */}
              <div className="flex items-center justify-between">
                <label className="text-xs text-gray-300">Show Date</label>
                <button
                  onClick={() => updateTemplate({ showDate: !template.showDate })}
                  className={`w-10 h-5 rounded-full transition-colors ${
                    template.showDate ? 'bg-[#FFA500]' : 'bg-[#333333]'
                  }`}
                >
                  <div
                    className={`w-4 h-4 rounded-full bg-white transition-transform ${
                      template.showDate ? 'translate-x-5' : 'translate-x-0.5'
                    }`}
                  />
                </button>
              </div>
            </>
          )}
        </div>
      )}

      {/* Preview */}
      <div className="space-y-2">
        <label className="text-[10px] text-gray-500">Preview</label>
        <div className="bg-white p-4 rounded">
          {brandKit.headerTemplate?.enabled && (
            <div
              className="mb-4 pb-2"
              style={{
                borderBottom: brandKit.headerTemplate.borderBottom ? '1px solid #E0E0E0' : 'none',
                fontSize: `${brandKit.headerTemplate.fontSize}pt`,
                color: brandKit.headerTemplate.textColor,
                backgroundColor: brandKit.headerTemplate.backgroundColor,
                textAlign: brandKit.headerTemplate.alignment,
                minHeight: brandKit.headerTemplate.height,
                display: 'flex',
                alignItems: 'center',
                justifyContent: brandKit.headerTemplate.alignment === 'center' ? 'center' :
                  brandKit.headerTemplate.alignment === 'right' ? 'flex-end' : 'flex-start',
              }}
            >
              <span>{brandKit.headerTemplate.content.replace('{company}', 'Your Company')}</span>
              {brandKit.headerTemplate.showPageNumbers && brandKit.headerTemplate.pageNumberPosition !== brandKit.headerTemplate.alignment && (
                <span className="ml-auto">Page 1</span>
              )}
            </div>
          )}
          <div className="h-20 bg-gray-100 rounded flex items-center justify-center text-gray-400 text-xs">
            Report Content
          </div>
          {brandKit.footerTemplate?.enabled && (
            <div
              className="mt-4 pt-2"
              style={{
                borderTop: brandKit.footerTemplate.borderTop ? '1px solid #E0E0E0' : 'none',
                fontSize: `${brandKit.footerTemplate.fontSize}pt`,
                color: brandKit.footerTemplate.textColor,
                backgroundColor: brandKit.footerTemplate.backgroundColor,
                textAlign: brandKit.footerTemplate.alignment,
                minHeight: brandKit.footerTemplate.height,
                display: 'flex',
                alignItems: 'center',
                justifyContent: brandKit.footerTemplate.alignment === 'center' ? 'center' :
                  brandKit.footerTemplate.alignment === 'right' ? 'flex-end' : 'flex-start',
              }}
            >
              <span>{brandKit.footerTemplate.content.replace('{company}', 'Your Company')}</span>
              {brandKit.footerTemplate.showPageNumbers && (
                <span className={brandKit.footerTemplate.pageNumberPosition === 'center' ? '' : 'ml-auto'}>
                  {brandKit.footerTemplate.pageNumberPosition === 'center' ? ' - Page 1 -' : 'Page 1'}
                </span>
              )}
            </div>
          )}
        </div>
      </div>
    </div>
  );
};

// ============ Typography Tab ============
export interface TypographyTabProps {
  brandKit: BrandKit | null;
  onUpdate: (styles: ParagraphStyles) => void;
}

export const TypographyTab: React.FC<TypographyTabProps> = ({ brandKit, onUpdate }) => {
  const { colors } = useTerminalTheme();
  if (!brandKit) {
    return (
      <div className="text-center py-8 text-gray-500 text-xs">
        Select a brand kit first
      </div>
    );
  }

  const styles = brandKit.paragraphStyles;

  const updateStyles = (updates: Partial<ParagraphStyles>) => {
    onUpdate({ ...styles, ...updates });
  };

  return (
    <div className="space-y-4">
      <label className="text-xs font-semibold" style={{ color: colors.primary }}>
        PARAGRAPH & LINE SPACING
      </label>

      {/* Line Height */}
      <div className="space-y-2">
        <div className="flex items-center justify-between">
          <label className="text-xs text-gray-300">Line Height</label>
          <span className="text-xs text-[#FFA500]">{styles.lineHeight.toFixed(2)}</span>
        </div>
        <input
          type="range"
          min="1"
          max="3"
          step="0.1"
          value={styles.lineHeight}
          onChange={(e) => updateStyles({ lineHeight: Number(e.target.value) })}
          className="w-full"
          style={{ accentColor: colors.primary }}
        />
        <div className="flex justify-between text-[10px] text-gray-500">
          <span>Tight (1.0)</span>
          <span>Normal (1.5)</span>
          <span>Loose (3.0)</span>
        </div>
      </div>

      {/* Paragraph Spacing */}
      <div className="space-y-2">
        <div className="flex items-center justify-between">
          <label className="text-xs text-gray-300">Paragraph Spacing</label>
          <span className="text-xs text-[#FFA500]">{styles.paragraphSpacing}px</span>
        </div>
        <input
          type="range"
          min="0"
          max="48"
          step="4"
          value={styles.paragraphSpacing}
          onChange={(e) => updateStyles({ paragraphSpacing: Number(e.target.value) })}
          className="w-full"
          style={{ accentColor: colors.primary }}
        />
      </div>

      {/* Text Indent */}
      <div className="space-y-2">
        <div className="flex items-center justify-between">
          <label className="text-xs text-gray-300">First Line Indent</label>
          <span className="text-xs text-[#FFA500]">{styles.textIndent}px</span>
        </div>
        <input
          type="range"
          min="0"
          max="50"
          step="5"
          value={styles.textIndent}
          onChange={(e) => updateStyles({ textIndent: Number(e.target.value) })}
          className="w-full"
          style={{ accentColor: colors.primary }}
        />
      </div>

      {/* Letter Spacing */}
      <div className="space-y-2">
        <div className="flex items-center justify-between">
          <label className="text-xs text-gray-300">Letter Spacing</label>
          <span className="text-xs text-[#FFA500]">{styles.letterSpacing}px</span>
        </div>
        <input
          type="range"
          min="-2"
          max="10"
          step="0.5"
          value={styles.letterSpacing}
          onChange={(e) => updateStyles({ letterSpacing: Number(e.target.value) })}
          className="w-full"
          style={{ accentColor: colors.primary }}
        />
      </div>

      {/* Word Spacing */}
      <div className="space-y-2">
        <div className="flex items-center justify-between">
          <label className="text-xs text-gray-300">Word Spacing</label>
          <span className="text-xs text-[#FFA500]">{styles.wordSpacing}px</span>
        </div>
        <input
          type="range"
          min="0"
          max="20"
          step="1"
          value={styles.wordSpacing}
          onChange={(e) => updateStyles({ wordSpacing: Number(e.target.value) })}
          className="w-full"
          style={{ accentColor: colors.primary }}
        />
      </div>

      {/* Text Alignment */}
      <div className="space-y-2">
        <label className="text-xs text-gray-300">Default Text Alignment</label>
        <div className="flex gap-2">
          {([
            { value: 'left', icon: AlignLeft },
            { value: 'center', icon: AlignCenter },
            { value: 'right', icon: AlignRight },
            { value: 'justify', icon: AlignJustify },
          ] as const).map(({ value, icon: Icon }) => (
            <button
              key={value}
              onClick={() => updateStyles({ textAlign: value })}
              className={`flex-1 py-2 rounded transition-colors ${
                styles.textAlign === value
                  ? 'bg-[#FFA500] text-black'
                  : 'bg-[#0a0a0a] text-gray-400 hover:bg-[#2a2a2a]'
              }`}
              title={value.charAt(0).toUpperCase() + value.slice(1)}
            >
              <Icon size={14} className="mx-auto" />
            </button>
          ))}
        </div>
      </div>

      {/* Preview */}
      <div className="space-y-2">
        <label className="text-[10px] text-gray-500">Preview</label>
        <div className="bg-white p-4 rounded text-black">
          <p style={{
            lineHeight: styles.lineHeight,
            marginBottom: `${styles.paragraphSpacing}px`,
            textIndent: `${styles.textIndent}px`,
            letterSpacing: `${styles.letterSpacing}px`,
            wordSpacing: `${styles.wordSpacing}px`,
            textAlign: styles.textAlign,
          }}>
            This is a sample paragraph demonstrating the typography settings. Notice how the line height,
            paragraph spacing, and text alignment affect the overall readability and appearance of the text.
          </p>
          <p style={{
            lineHeight: styles.lineHeight,
            marginBottom: `${styles.paragraphSpacing}px`,
            textIndent: `${styles.textIndent}px`,
            letterSpacing: `${styles.letterSpacing}px`,
            wordSpacing: `${styles.wordSpacing}px`,
            textAlign: styles.textAlign,
          }}>
            A second paragraph shows the spacing between paragraphs. Professional reports often use
            consistent typography to maintain visual coherence throughout the document.
          </p>
        </div>
      </div>

      {/* Presets */}
      <div className="space-y-2">
        <label className="text-xs text-gray-500">Quick Presets</label>
        <div className="grid grid-cols-2 gap-2">
          <button
            onClick={() => updateStyles({
              lineHeight: 1.5,
              paragraphSpacing: 12,
              textIndent: 0,
              letterSpacing: 0,
              wordSpacing: 0,
              textAlign: 'left',
            })}
            className="px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] hover:border-[#FFA500] transition-colors text-gray-300"
          >
            Default
          </button>
          <button
            onClick={() => updateStyles({
              lineHeight: 2.0,
              paragraphSpacing: 16,
              textIndent: 20,
              letterSpacing: 0,
              wordSpacing: 0,
              textAlign: 'justify',
            })}
            className="px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] hover:border-[#FFA500] transition-colors text-gray-300"
          >
            Academic
          </button>
          <button
            onClick={() => updateStyles({
              lineHeight: 1.4,
              paragraphSpacing: 8,
              textIndent: 0,
              letterSpacing: 0.5,
              wordSpacing: 0,
              textAlign: 'left',
            })}
            className="px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] hover:border-[#FFA500] transition-colors text-gray-300"
          >
            Compact
          </button>
          <button
            onClick={() => updateStyles({
              lineHeight: 1.8,
              paragraphSpacing: 20,
              textIndent: 0,
              letterSpacing: 0,
              wordSpacing: 2,
              textAlign: 'left',
            })}
            className="px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] hover:border-[#FFA500] transition-colors text-gray-300"
          >
            Relaxed
          </button>
        </div>
      </div>
    </div>
  );
};

// ============ Custom CSS Tab ============
export interface CustomCSSTabProps {
  brandKit: BrandKit | null;
  onUpdate: (css: string) => void;
}

export const CustomCSSTab: React.FC<CustomCSSTabProps> = ({ brandKit, onUpdate }) => {
  const { colors } = useTerminalTheme();
  const [localCSS, setLocalCSS] = useState(brandKit?.customCSS || '');
  const [showGenerated, setShowGenerated] = useState(false);

  useEffect(() => {
    setLocalCSS(brandKit?.customCSS || '');
  }, [brandKit]);

  if (!brandKit) {
    return (
      <div className="text-center py-8 text-gray-500 text-xs">
        Select a brand kit first
      </div>
    );
  }

  const generatedCSS = brandKitService.generateCSSFromBrandKit(brandKit);

  const handleApply = () => {
    onUpdate(localCSS);
    toast.success('Custom CSS applied');
  };

  return (
    <div className="space-y-4">
      <div className="flex items-center justify-between">
        <label className="text-xs font-semibold" style={{ color: colors.primary }}>
          CUSTOM CSS
        </label>
        <button
          onClick={() => setShowGenerated(!showGenerated)}
          className="text-[10px] px-2 py-1 rounded bg-[#0a0a0a] hover:bg-[#2a2a2a] text-gray-400"
        >
          {showGenerated ? 'Hide Generated' : 'Show Generated'}
        </button>
      </div>

      {showGenerated && (
        <div className="space-y-2">
          <label className="text-[10px] text-gray-500">Generated from Brand Kit (read-only)</label>
          <pre className="p-3 text-[10px] rounded bg-[#0a0a0a] border border-[#333333] text-green-400 overflow-x-auto max-h-48 overflow-y-auto">
            {generatedCSS}
          </pre>
          <button
            onClick={() => {
              navigator.clipboard.writeText(generatedCSS);
              toast.success('CSS copied to clipboard');
            }}
            className="text-[10px] px-2 py-1 rounded bg-[#0a0a0a] hover:bg-[#2a2a2a] text-gray-400 flex items-center gap-1"
          >
            <Copy size={12} />
            Copy Generated CSS
          </button>
        </div>
      )}

      {/* Custom CSS Editor */}
      <div className="space-y-2">
        <label className="text-[10px] text-gray-500">Additional Custom CSS</label>
        <textarea
          value={localCSS}
          onChange={(e) => setLocalCSS(e.target.value)}
          className="w-full h-64 px-3 py-2 text-xs font-mono rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none text-green-400 resize-none"
          placeholder="/* Add your custom CSS here */&#10;&#10;.report-title {&#10;  color: #FFA500;&#10;  font-weight: bold;&#10;}"
          spellCheck={false}
        />
      </div>

      {/* CSS Snippets */}
      <div className="space-y-2">
        <label className="text-[10px] text-gray-500">Quick Snippets</label>
        <div className="grid grid-cols-2 gap-2">
          <button
            onClick={() => setLocalCSS(prev => prev + '\n\n/* Highlight Box */\n.highlight {\n  background: #FFF8E1;\n  border-left: 4px solid #FFA500;\n  padding: 15px;\n  margin: 10px 0;\n}')}
            className="px-2 py-2 text-[10px] rounded bg-[#0a0a0a] border border-[#333333] hover:border-[#FFA500] transition-colors text-gray-300"
          >
            Highlight Box
          </button>
          <button
            onClick={() => setLocalCSS(prev => prev + '\n\n/* Shadow Card */\n.shadow-card {\n  box-shadow: 0 4px 6px rgba(0,0,0,0.1);\n  border-radius: 8px;\n  padding: 20px;\n}')}
            className="px-2 py-2 text-[10px] rounded bg-[#0a0a0a] border border-[#333333] hover:border-[#FFA500] transition-colors text-gray-300"
          >
            Shadow Card
          </button>
          <button
            onClick={() => setLocalCSS(prev => prev + '\n\n/* Striped Table */\ntable tr:nth-child(even) {\n  background-color: #f8f9fa;\n}')}
            className="px-2 py-2 text-[10px] rounded bg-[#0a0a0a] border border-[#333333] hover:border-[#FFA500] transition-colors text-gray-300"
          >
            Striped Table
          </button>
          <button
            onClick={() => setLocalCSS(prev => prev + '\n\n/* Print Styles */\n@media print {\n  .no-print { display: none; }\n  body { font-size: 12pt; }\n}')}
            className="px-2 py-2 text-[10px] rounded bg-[#0a0a0a] border border-[#333333] hover:border-[#FFA500] transition-colors text-gray-300"
          >
            Print Styles
          </button>
        </div>
      </div>

      {/* Apply Button */}
      <button
        onClick={handleApply}
        className="w-full py-2 text-xs font-semibold rounded transition-colors flex items-center justify-center gap-2"
        style={{ backgroundColor: colors.primary, color: '#000' }}
      >
        <Check size={14} />
        Apply Custom CSS
      </button>

      {/* Help Text */}
      <div className="p-3 bg-[#0a0a0a] rounded border border-[#333333]">
        <p className="text-[10px] text-gray-500">
          Custom CSS will be applied to the generated PDF. Use standard CSS properties.
          The generated CSS from your brand kit settings is automatically included.
        </p>
      </div>
    </div>
  );
};
