import React, { useState, useRef, useEffect, useCallback } from 'react';
import {
  FileText,
  BarChart3,
  Image as ImageIcon,
} from 'lucide-react';
import { ReportComponent } from '@/services/core/reportService';
import { DocumentCanvasProps, PageTheme, ThemeBackgroundStyle } from './types';
import {
  LiveDataTable,
  KPICardsRow,
  Sparkline,
  DynamicChart,
} from './DataComponents';
import {
  QuoteBox,
  ListComponent,
  TableOfContents,
  SignatureBlock,
  DisclaimerSection,
  QRCodeComponent,
  Watermark,
} from './AdditionalComponents';

// Get theme background style
export const getThemeBackground = (pageTheme: PageTheme, customBgColor: string): ThemeBackgroundStyle => {
  switch (pageTheme) {
    case 'modern':
      return {
        background: 'linear-gradient(135deg, #667eea 0%, #764ba2 100%)',
        backgroundSize: 'cover',
        color: '#ffffff',
      };
    case 'geometric':
      return {
        backgroundColor: '#ffffff',
        backgroundImage: `
          linear-gradient(30deg, rgba(255,165,0,0.08) 12%, transparent 12.5%, transparent 87%, rgba(255,165,0,0.08) 87.5%, rgba(255,165,0,0.08)),
          linear-gradient(150deg, rgba(255,165,0,0.08) 12%, transparent 12.5%, transparent 87%, rgba(255,165,0,0.08) 87.5%, rgba(255,165,0,0.08)),
          linear-gradient(30deg, rgba(255,165,0,0.05) 12%, transparent 12.5%, transparent 87%, rgba(255,165,0,0.05) 87.5%, rgba(255,165,0,0.05)),
          linear-gradient(150deg, rgba(255,165,0,0.05) 12%, transparent 12.5%, transparent 87%, rgba(255,165,0,0.05) 87.5%, rgba(255,165,0,0.05))
        `,
        backgroundSize: '80px 140px',
        backgroundPosition: '0 0, 0 0, 40px 70px, 40px 70px',
        color: '#000000',
      };
    case 'gradient':
      return {
        background: 'linear-gradient(135deg, #1a1a2e 0%, #16213e 50%, #0f3460 100%)',
        color: '#ffffff',
      };
    case 'minimal':
      return {
        backgroundColor: '#ffffff',
        backgroundImage: `
          linear-gradient(rgba(200,200,200,0.3) 1px, transparent 1px),
          linear-gradient(90deg, rgba(200,200,200,0.3) 1px, transparent 1px)
        `,
        backgroundSize: '20px 20px',
        color: '#000000',
      };
    case 'pattern':
      return {
        backgroundColor: '#ffffff',
        backgroundImage: `
          repeating-linear-gradient(45deg, transparent, transparent 10px, rgba(255,165,0,0.02) 10px, rgba(255,165,0,0.02) 20px),
          repeating-linear-gradient(-45deg, transparent, transparent 10px, rgba(255,165,0,0.02) 10px, rgba(255,165,0,0.02) 20px),
          radial-gradient(circle at 10% 10%, rgba(255,165,0,0.03) 0%, transparent 50%),
          radial-gradient(circle at 90% 90%, rgba(255,165,0,0.03) 0%, transparent 50%)
        `,
        backgroundSize: 'cover',
        color: '#000000',
      };
    case 'classic':
    default:
      return {
        backgroundColor: customBgColor,
        color: '#000000',
      };
  }
};

// Inline editable text component
const InlineEdit: React.FC<{
  value: string;
  onChange: (value: string) => void;
  isEditing: boolean;
  onStartEdit: () => void;
  onStopEdit: () => void;
  className?: string;
  style?: React.CSSProperties;
  tag?: 'h1' | 'h2' | 'h3' | 'p' | 'span';
  multiline?: boolean;
  placeholder?: string;
}> = ({ value, onChange, isEditing, onStartEdit, onStopEdit, className = '', style, tag: Tag = 'p', multiline = false, placeholder = '' }) => {
  const inputRef = useRef<HTMLTextAreaElement | HTMLInputElement>(null);

  useEffect(() => {
    if (isEditing && inputRef.current) {
      inputRef.current.focus();
      // Select all text
      inputRef.current.select();
    }
  }, [isEditing]);

  if (isEditing) {
    if (multiline) {
      return (
        <textarea
          ref={inputRef as React.RefObject<HTMLTextAreaElement>}
          value={value}
          onChange={(e) => onChange(e.target.value)}
          onBlur={onStopEdit}
          onKeyDown={(e) => {
            if (e.key === 'Escape') onStopEdit();
          }}
          className={`w-full bg-transparent outline-none resize-none border border-orange-400 rounded px-1 ${className}`}
          style={{ ...style, minHeight: '2em' }}
          rows={Math.max(2, value.split('\n').length)}
          placeholder={placeholder}
        />
      );
    }
    return (
      <input
        ref={inputRef as React.RefObject<HTMLInputElement>}
        type="text"
        value={value}
        onChange={(e) => onChange(e.target.value)}
        onBlur={onStopEdit}
        onKeyDown={(e) => {
          if (e.key === 'Enter' || e.key === 'Escape') onStopEdit();
        }}
        className={`w-full bg-transparent outline-none border border-orange-400 rounded px-1 ${className}`}
        style={style}
        placeholder={placeholder}
      />
    );
  }

  return (
    <Tag
      onDoubleClick={(e) => { e.stopPropagation(); onStartEdit(); }}
      className={`${className} cursor-text`}
      style={style}
      title="Double-click to edit"
    >
      {value || placeholder || '\u00A0'}
    </Tag>
  );
};

// Render individual component based on type
const RenderComponent: React.FC<{
  component: ReportComponent;
  isSelected: boolean;
  onClick: () => void;
  onUpdate: (id: string, updates: Partial<ReportComponent>) => void;
  template: any;
  tocItems: any[];
}> = ({ component, isSelected, onClick, onUpdate, template, tocItems }) => {
  const [editingField, setEditingField] = useState<string | null>(null);

  const startEdit = useCallback((field: string) => {
    setEditingField(field);
  }, []);

  const stopEdit = useCallback(() => {
    setEditingField(null);
  }, []);

  const updateContent = useCallback((value: string) => {
    onUpdate(component.id, { content: value });
  }, [component.id, onUpdate]);

  const updateConfig = useCallback((key: string, value: any) => {
    onUpdate(component.id, { config: { ...component.config, [key]: value } });
  }, [component.id, component.config, onUpdate]);

  return (
    <div
      onClick={onClick}
      className={`group relative cursor-pointer transition-all rounded ${
        isSelected
          ? 'ring-2 ring-orange-500 ring-offset-2 ring-offset-white'
          : 'hover:ring-1 hover:ring-orange-300 hover:ring-offset-1 hover:ring-offset-white'
      }`}
      style={{ padding: '2px' }}
    >
      {/* Floating toolbar on hover/select */}
      {isSelected && (
        <div className="absolute -top-3 right-2 z-10 flex items-center gap-1 bg-[#1a1a1a] rounded shadow-lg border border-[#333] px-1 py-0.5">
          <span className="text-[9px] text-orange-400 font-semibold px-1 capitalize">{component.type}</span>
        </div>
      )}

      {component.type === 'heading' && (
        <InlineEdit
          value={String(component.content || '')}
          onChange={updateContent}
          isEditing={editingField === 'content'}
          onStartEdit={() => startEdit('content')}
          onStopEdit={stopEdit}
          className={`font-bold text-2xl`}
          style={{ textAlign: (component.config.alignment as any) || 'left' }}
          tag="h2"
          placeholder="New Heading"
        />
      )}
      {component.type === 'subheading' && (
        <InlineEdit
          value={String(component.content || '')}
          onChange={updateContent}
          isEditing={editingField === 'content'}
          onStartEdit={() => startEdit('content')}
          onStopEdit={stopEdit}
          className={`font-semibold text-xl`}
          style={{ textAlign: (component.config.alignment as any) || 'left' }}
          tag="h3"
          placeholder="New Subheading"
        />
      )}
      {component.type === 'text' && (
        <InlineEdit
          value={String(component.content || '')}
          onChange={updateContent}
          isEditing={editingField === 'content'}
          onStartEdit={() => startEdit('content')}
          onStopEdit={stopEdit}
          style={{ textAlign: (component.config.alignment as any) || 'left' }}
          tag="p"
          multiline
          placeholder="Enter text here..."
        />
      )}
      {component.type === 'chart' && (
        <div className="border border-gray-300 rounded p-8 bg-gray-50 text-center">
          <BarChart3 className="w-16 h-16 mx-auto text-gray-400" />
          <p className="text-sm text-gray-500 mt-2">Chart Placeholder</p>
        </div>
      )}
      {component.type === 'table' && (
        <div className="border border-gray-300 rounded overflow-hidden">
          <table className="w-full">
            <thead className="bg-gray-100">
              <tr>
                {(component.config.columns || ['Column 1', 'Column 2', 'Column 3']).map((col: string, idx: number) => (
                  <th key={idx} className="px-4 py-2 text-left text-sm font-semibold">{col}</th>
                ))}
              </tr>
            </thead>
            <tbody>
              <tr className="border-t border-gray-200">
                {(component.config.columns || ['Column 1', 'Column 2', 'Column 3']).map((_: any, idx: number) => (
                  <td key={idx} className="px-4 py-2 text-sm">Sample data</td>
                ))}
              </tr>
            </tbody>
          </table>
        </div>
      )}
      {component.type === 'image' && (
        <div className="border border-gray-300 rounded p-4 bg-gray-50">
          {component.config.imageUrl ? (
            <img
              src={`file://${component.config.imageUrl}`}
              alt="Report Image"
              className="max-w-full h-auto"
            />
          ) : (
            <div className="text-center py-8 text-gray-400">
              <ImageIcon className="w-12 h-12 mx-auto mb-2" />
              <p className="text-sm">No image selected — use Properties panel to upload</p>
            </div>
          )}
        </div>
      )}
      {component.type === 'code' && (
        <div className="relative">
          {editingField === 'content' ? (
            <textarea
              autoFocus
              value={String(component.content || '')}
              onChange={(e) => updateContent(e.target.value)}
              onBlur={stopEdit}
              onKeyDown={(e) => { if (e.key === 'Escape') stopEdit(); }}
              className="w-full bg-gray-900 text-green-400 p-4 rounded text-sm font-mono border border-orange-400 outline-none resize-none"
              rows={Math.max(4, String(component.content || '').split('\n').length)}
              placeholder="// Enter code here..."
            />
          ) : (
            <pre
              className="bg-gray-900 text-green-400 p-4 rounded overflow-x-auto text-sm font-mono cursor-text"
              onDoubleClick={(e) => { e.stopPropagation(); startEdit('content'); }}
              title="Double-click to edit"
            >
              <code>{component.content || '// Code block'}</code>
            </pre>
          )}
        </div>
      )}
      {component.type === 'divider' && (
        <hr className="border-t-2 my-4" style={{ borderColor: component.config.color || '#333333' }} />
      )}
      {component.type === 'section' && (
        <div className="border-l-4 p-4 my-4" style={{
          borderColor: component.config.borderColor || '#FFA500',
          backgroundColor: component.config.backgroundColor || '#f8f9fa'
        }}>
          <InlineEdit
            value={String(component.config.sectionTitle || '')}
            onChange={(val) => updateConfig('sectionTitle', val)}
            isEditing={editingField === 'sectionTitle'}
            onStartEdit={() => startEdit('sectionTitle')}
            onStopEdit={stopEdit}
            className="font-bold text-lg mb-2"
            style={{ color: component.config.borderColor || '#FFA500' }}
            tag="h3"
            placeholder="Section Title"
          />
          <p className="text-sm text-gray-600">Section content area</p>
        </div>
      )}
      {component.type === 'columns' && (
        <div className={`grid gap-4 my-4 ${component.config.columnCount === 3 ? 'grid-cols-3' : 'grid-cols-2'}`}>
          {Array.from({ length: component.config.columnCount || 2 }).map((_, idx) => (
            <div key={idx} className="border border-gray-300 rounded p-4 bg-gray-50">
              <p className="text-sm text-gray-500 text-center">Column {idx + 1}</p>
            </div>
          ))}
        </div>
      )}
      {component.type === 'coverpage' && (
        <div className="min-h-[400px] flex flex-col items-center justify-center text-center p-12 rounded" style={{
          backgroundColor: component.config.backgroundColor || '#0a0a0a',
          color: '#FFA500'
        }}>
          {component.config.logo && (
            <img
              src={`file://${component.config.logo}`}
              alt="Logo"
              className="max-w-[200px] h-auto mb-8"
            />
          )}
          <InlineEdit
            value={String(component.content || template.metadata.title || '')}
            onChange={updateContent}
            isEditing={editingField === 'content'}
            onStartEdit={() => startEdit('content')}
            onStopEdit={stopEdit}
            className="text-4xl font-bold mb-4"
            tag="h1"
            placeholder="Report Title"
          />
          <InlineEdit
            value={String(component.config.subtitle || '')}
            onChange={(val) => updateConfig('subtitle', val)}
            isEditing={editingField === 'subtitle'}
            onStartEdit={() => startEdit('subtitle')}
            onStopEdit={stopEdit}
            className="text-xl mb-8"
            tag="p"
            placeholder="Subtitle"
          />
          <div className="text-sm text-gray-400 mt-auto">
            {template.metadata.company && <>{template.metadata.company}<br /></>}
            {template.metadata.author && <>{template.metadata.author}<br /></>}
            {template.metadata.date}
          </div>
        </div>
      )}
      {component.type === 'pagebreak' && (
        <div className="my-8 py-4 border-t-2 border-dashed border-gray-400 text-center">
          <span className="text-xs text-gray-400 bg-white px-3 py-1 rounded-full">Page Break</span>
        </div>
      )}
      {component.type === 'quote' && (
        <div onDoubleClick={(e) => { e.stopPropagation(); startEdit('content'); }}>
          {editingField === 'content' ? (
            <div className="p-4 rounded-r-lg my-4 bg-gray-50 border-l-4 border-gray-500">
              <textarea
                autoFocus
                value={String(component.content || '')}
                onChange={(e) => updateContent(e.target.value)}
                onBlur={stopEdit}
                onKeyDown={(e) => { if (e.key === 'Escape') stopEdit(); }}
                className="w-full bg-transparent text-sm text-gray-700 italic outline-none resize-none border border-orange-400 rounded p-1"
                rows={3}
                placeholder="Enter quote text..."
              />
            </div>
          ) : (
            <QuoteBox
              content={String(component.content || 'Enter quote text here...')}
              author={component.config.author}
              type={component.config.quoteType || 'quote'}
            />
          )}
        </div>
      )}
      {component.type === 'list' && (
        <ListComponent
          items={component.config.items || ['Item 1', 'Item 2']}
          ordered={component.config.ordered}
        />
      )}
      {component.type === 'toc' && (
        <TableOfContents
          items={tocItems}
          showPageNumbers={component.config.showPageNumbers}
        />
      )}
      {component.type === 'kpi' && (
        <KPICardsRow
          cards={(component.config.kpis || []).map((k: any) => ({
            title: k.label,
            value: k.value,
            change: k.change,
            trend: k.trend,
          }))}
        />
      )}
      {component.type === 'sparkline' && (
        <div className="inline-block" style={{ minWidth: '200px', minHeight: '50px' }}>
          <Sparkline
            config={{
              data: component.config.data as number[] || [10, 20, 15, 25],
              color: component.config.color || '#FFA500',
              height: 50,
            }}
          />
        </div>
      )}
      {component.type === 'liveTable' && (
        <LiveDataTable
          config={{
            source: component.config.dataSource as 'portfolio' | 'watchlist' || 'portfolio',
            columns: component.config.columns,
          }}
        />
      )}
      {component.type === 'dynamicChart' && (
        <DynamicChart
          config={{
            type: (component.config.chartType || 'line') as 'line' | 'bar' | 'area' | 'pie',
            data: component.config.data as Array<{ name: string; value: number }> || [],
            xKey: 'name',
            yKey: 'value',
            height: 250,
          }}
        />
      )}
      {component.type === 'signature' && (
        <div onDoubleClick={(e) => { e.stopPropagation(); startEdit('name'); }}>
          {editingField === 'name' ? (
            <div className="mt-8 inline-block min-w-[200px]">
              {component.config.showLine !== false && <div className="border-b border-gray-400 h-8 mb-2" />}
              <input
                autoFocus
                type="text"
                value={component.config.name || ''}
                onChange={(e) => updateConfig('name', e.target.value)}
                onBlur={stopEdit}
                onKeyDown={(e) => { if (e.key === 'Enter' || e.key === 'Escape') stopEdit(); }}
                className="text-sm font-semibold text-gray-800 bg-transparent outline-none border border-orange-400 rounded px-1 w-full"
                placeholder="Name"
              />
              <input
                type="text"
                value={component.config.title || ''}
                onChange={(e) => updateConfig('title', e.target.value)}
                onBlur={stopEdit}
                className="text-xs text-gray-600 bg-transparent outline-none border border-orange-400 rounded px-1 w-full mt-1"
                placeholder="Title"
              />
            </div>
          ) : (
            <SignatureBlock
              name={component.config.name || 'Name'}
              title={component.config.title}
              date={template.metadata.date}
              showLine={component.config.showLine}
            />
          )}
        </div>
      )}
      {component.type === 'disclaimer' && (
        <div onDoubleClick={(e) => { e.stopPropagation(); startEdit('content'); }}>
          {editingField === 'content' ? (
            <div className="mt-8 p-4 rounded text-xs bg-gray-100 border border-gray-200">
              <h4 className="font-bold text-gray-700 mb-2 uppercase text-[10px]">Disclaimer</h4>
              <textarea
                autoFocus
                value={String(component.content || '')}
                onChange={(e) => updateContent(e.target.value)}
                onBlur={stopEdit}
                onKeyDown={(e) => { if (e.key === 'Escape') stopEdit(); }}
                className="w-full bg-transparent text-xs text-gray-600 outline-none resize-none border border-orange-400 rounded p-1"
                rows={4}
                placeholder="Enter disclaimer text..."
              />
            </div>
          ) : (
            <DisclaimerSection
              content={String(component.content || 'This report is for informational purposes only.')}
              type={component.config.disclaimerType || 'standard'}
            />
          )}
        </div>
      )}
      {component.type === 'qrcode' && (
        <QRCodeComponent
          value={component.config.value || 'https://example.com'}
          size={component.config.size || 100}
          label={component.config.label}
        />
      )}
      {component.type === 'watermark' && (
        <Watermark
          text={component.config.text || 'CONFIDENTIAL'}
          opacity={component.config.opacity || 0.1}
          rotation={component.config.rotation || -45}
        />
      )}
    </div>
  );
};

export const DocumentCanvas: React.FC<DocumentCanvasProps> = ({
  template,
  selectedComponent,
  onSelectComponent,
  onUpdateComponent,
  pageTheme,
  customBgColor,
  fontFamily,
  defaultFontSize,
  isBold,
  isItalic,
  advancedStyles,
  tocItems,
}) => {
  const themeBackground = getThemeBackground(pageTheme, customBgColor);

  return (
    <div className="w-3/5 overflow-y-auto p-6" style={{ backgroundColor: '#0f0f0f' }}>
      <div
        className="mx-auto shadow-lg"
        style={{
          width: '210mm',
          minHeight: '297mm',
          padding: '20mm',
          fontFamily: fontFamily,
          fontSize: `${defaultFontSize}pt`,
          fontWeight: isBold ? 'bold' : 'normal',
          fontStyle: isItalic ? 'italic' : 'normal',
          ...(advancedStyles.paragraphStyles && {
            lineHeight: advancedStyles.paragraphStyles.lineHeight,
            letterSpacing: `${advancedStyles.paragraphStyles.letterSpacing}px`,
            wordSpacing: `${advancedStyles.paragraphStyles.wordSpacing}px`,
          }),
          ...themeBackground,
        }}
      >
        {/* Header (if enabled) */}
        {advancedStyles.headerTemplate?.enabled && (
          <div
            className="mb-4 -mt-4 -mx-4 px-4"
            style={{
              backgroundColor: advancedStyles.headerTemplate.backgroundColor,
              color: advancedStyles.headerTemplate.textColor,
              fontSize: `${advancedStyles.headerTemplate.fontSize}pt`,
              minHeight: advancedStyles.headerTemplate.height,
              textAlign: advancedStyles.headerTemplate.alignment,
              borderBottom: advancedStyles.headerTemplate.borderBottom ? '1px solid #E0E0E0' : 'none',
              display: 'flex',
              alignItems: 'center',
              justifyContent: advancedStyles.headerTemplate.alignment === 'center' ? 'center' :
                advancedStyles.headerTemplate.alignment === 'right' ? 'flex-end' : 'space-between',
              padding: '8px 20px',
            }}
          >
            <span>
              {advancedStyles.headerTemplate.content
                .replace('{company}', template.metadata.company || 'Company')
                .replace('{title}', template.metadata.title || 'Report')
                .replace('{date}', template.metadata.date || new Date().toLocaleDateString())}
            </span>
            {advancedStyles.headerTemplate.showPageNumbers && (
              <span>Page 1</span>
            )}
          </div>
        )}

        {/* Document Metadata */}
        <div className="mb-8 border-b border-gray-300 pb-4">
          <h1 className="text-3xl font-bold mb-2">{template.metadata.title}</h1>
          {template.metadata.company && (
            <p className="text-lg text-gray-600">{template.metadata.company}</p>
          )}
          {template.metadata.author && (
            <p className="text-sm text-gray-500">By {template.metadata.author}</p>
          )}
          <p className="text-sm text-gray-500">{template.metadata.date}</p>
        </div>

        {/* Components */}
        {template.components.length === 0 ? (
          <div className="text-center py-16 text-gray-400">
            <FileText className="w-16 h-16 mx-auto mb-4 opacity-30" />
            <p className="text-lg">Your report is empty</p>
            <p className="text-sm mt-2">Add components from the left panel to start building your report</p>
          </div>
        ) : (
          <div className="space-y-4">
            {template.components.map((component) => (
              <RenderComponent
                key={component.id}
                component={component}
                isSelected={selectedComponent === component.id}
                onClick={() => onSelectComponent(component.id)}
                onUpdate={onUpdateComponent}
                template={template}
                tocItems={tocItems}
              />
            ))}
          </div>
        )}

        {/* Footer (if enabled) */}
        {advancedStyles.footerTemplate?.enabled && (
          <div
            className="mt-8 -mb-4 -mx-4 px-4"
            style={{
              backgroundColor: advancedStyles.footerTemplate.backgroundColor,
              color: advancedStyles.footerTemplate.textColor,
              fontSize: `${advancedStyles.footerTemplate.fontSize}pt`,
              minHeight: advancedStyles.footerTemplate.height,
              textAlign: advancedStyles.footerTemplate.alignment,
              borderTop: advancedStyles.footerTemplate.borderTop ? '1px solid #E0E0E0' : 'none',
              display: 'flex',
              alignItems: 'center',
              justifyContent: advancedStyles.footerTemplate.alignment === 'center' ? 'center' :
                advancedStyles.footerTemplate.alignment === 'right' ? 'flex-end' : 'space-between',
              padding: '8px 20px',
            }}
          >
            <span>
              {advancedStyles.footerTemplate.content
                .replace('{company}', template.metadata.company || 'Company')
                .replace('{title}', template.metadata.title || 'Report')
                .replace('{date}', template.metadata.date || new Date().toLocaleDateString())}
            </span>
            {advancedStyles.footerTemplate.showPageNumbers && advancedStyles.footerTemplate.pageNumberPosition !== advancedStyles.footerTemplate.alignment && (
              <span>Page 1</span>
            )}
          </div>
        )}
      </div>
    </div>
  );
};

export default DocumentCanvas;
