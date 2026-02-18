// Template Library - Terminal UI/UX

import React, { useState } from 'react';
import { Star, CheckCircle2, Copy, Eye, Bookmark, Search, Filter, ArrowRight, X, GitMerge, Play } from 'lucide-react';
import { MappingTemplate } from '../types';
import { INDIAN_BROKER_TEMPLATES } from '../templates/indian-brokers';
import { useTerminalTheme } from '@/contexts/ThemeContext';

interface TemplateLibraryProps {
  onSelectTemplate: (template: MappingTemplate) => void;
}

export function TemplateLibrary({ onSelectTemplate }: TemplateLibraryProps) {
  const { colors } = useTerminalTheme();
  const [selectedTemplate, setSelectedTemplate] = useState<MappingTemplate | null>(null);
  const [searchTerm, setSearchTerm] = useState('');
  const [categoryFilter, setCategoryFilter] = useState<string>('all');

  const allTemplates = [...INDIAN_BROKER_TEMPLATES];

  const filteredTemplates = allTemplates.filter((template) => {
    const matchesSearch =
      template.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
      template.description.toLowerCase().includes(searchTerm.toLowerCase()) ||
      template.tags.some((tag) => tag.toLowerCase().includes(searchTerm.toLowerCase()));
    const matchesCategory = categoryFilter === 'all' || template.tags.includes(categoryFilter);
    return matchesSearch && matchesCategory;
  });

  const allTags = Array.from(new Set(allTemplates.flatMap((t) => t.tags)));
  const brokerTags = allTags.filter((tag) =>
    ['upstox', 'fyers', 'dhan', 'zerodha', 'angelone', 'shipnext'].includes(tag)
  );

  const borderColor = 'var(--ft-border-color, #2A2A2A)';

  return (
    <div className="flex flex-col h-full" style={{ fontFamily: 'inherit' }}>

      {/* ── Search & Filter Bar ── */}
      <div
        className="flex items-center gap-2 px-3 py-2 mb-3 flex-shrink-0"
        style={{ backgroundColor: colors.panel, border: `1px solid ${borderColor}` }}
      >
        <div className="flex items-center gap-2 flex-1" style={{ border: `1px solid ${borderColor}`, backgroundColor: colors.background, padding: '0 8px' }}>
          <Search size={12} color={colors.textMuted} />
          <input
            type="text"
            placeholder="Search templates..."
            value={searchTerm}
            onChange={(e) => setSearchTerm(e.target.value)}
            className="flex-1 py-1.5 text-xs font-mono bg-transparent outline-none"
            style={{ color: colors.text }}
          />
          {searchTerm && (
            <button onClick={() => setSearchTerm('')} style={{ color: colors.textMuted }}>
              <X size={11} />
            </button>
          )}
        </div>

        <div className="flex items-center gap-1.5" style={{ border: `1px solid ${borderColor}`, backgroundColor: colors.background, padding: '0 8px' }}>
          <Filter size={11} color={colors.textMuted} />
          <select
            value={categoryFilter}
            onChange={(e) => setCategoryFilter(e.target.value)}
            className="py-1.5 text-xs font-mono bg-transparent outline-none"
            style={{ color: colors.text, minWidth: '100px' }}
          >
            <option value="all" style={{ backgroundColor: colors.panel }}>ALL BROKERS</option>
            {brokerTags.map((tag) => (
              <option key={tag} value={tag} style={{ backgroundColor: colors.panel }}>
                {tag.toUpperCase()}
              </option>
            ))}
          </select>
        </div>

        <div className="text-[10px] font-mono flex-shrink-0" style={{ color: colors.textMuted }}>
          {filteredTemplates.length}/{allTemplates.length} RESULTS
        </div>
      </div>

      {/* ── Main Layout: Grid + Detail Panel ── */}
      <div className="flex flex-1 gap-3 overflow-hidden min-h-0">

        {/* Left: Templates Grid */}
        <div className="flex-1 overflow-auto space-y-2 pr-1">
          {filteredTemplates.length === 0 ? (
            <div
              className="flex items-center justify-center h-48"
              style={{ backgroundColor: colors.background, border: `1px solid ${borderColor}` }}
            >
              <div className="text-center">
                <Bookmark size={32} color={colors.textMuted} className="mx-auto mb-2 opacity-30" />
                <p className="text-xs font-bold tracking-wider" style={{ color: colors.textMuted }}>NO TEMPLATES FOUND</p>
                <p className="text-[11px] mt-1" style={{ color: colors.textMuted }}>Try adjusting your search or filter</p>
              </div>
            </div>
          ) : (
            filteredTemplates.map((template) => {
              const isSelected = selectedTemplate?.id === template.id;
              return (
                <div
                  key={template.id}
                  className="group transition-all cursor-pointer"
                  style={{
                    backgroundColor: isSelected ? `${colors.primary}10` : colors.panel,
                    border: `1px solid ${isSelected ? colors.primary : borderColor}`,
                  }}
                  onClick={() => setSelectedTemplate(isSelected ? null : template)}
                  onMouseEnter={(e) => { if (!isSelected) (e.currentTarget as HTMLDivElement).style.borderColor = `${colors.primary}60`; }}
                  onMouseLeave={(e) => { if (!isSelected) (e.currentTarget as HTMLDivElement).style.borderColor = borderColor; }}
                >
                  {/* Top accent bar */}
                  <div className="h-0.5 w-full" style={{ backgroundColor: isSelected ? colors.primary : `${colors.primary}40` }} />

                  <div className="p-3">
                    {/* Row 1: Name + badges */}
                    <div className="flex items-start justify-between mb-1.5">
                      <div className="flex items-center gap-2 flex-1 min-w-0">
                        <span className="text-xs font-bold truncate" style={{ color: colors.text }}>{template.name}</span>
                        {template.verified && (
                          <CheckCircle2 size={12} color={colors.success} className="flex-shrink-0" />
                        )}
                        {template.official && (
                          <Star size={12} color={colors.warning || '#F59E0B'} className="flex-shrink-0" />
                        )}
                      </div>
                      <div className="flex items-center gap-1 flex-shrink-0 ml-2">
                        {template.rating !== undefined && (
                          <div className="flex items-center gap-0.5 text-[10px] font-mono" style={{ color: colors.warning || '#F59E0B' }}>
                            <Star size={10} />
                            {template.rating}
                          </div>
                        )}
                      </div>
                    </div>

                    {/* Row 2: Description */}
                    <p className="text-[11px] mb-2 line-clamp-1" style={{ color: colors.textMuted }}>{template.description}</p>

                    {/* Row 3: Tags + Stats */}
                    <div className="flex items-center justify-between">
                      <div className="flex flex-wrap gap-1">
                        {template.tags.slice(0, 4).map((tag) => (
                          <span
                            key={tag}
                            className="text-[10px] px-1.5 py-0.5 font-mono font-bold"
                            style={{
                              backgroundColor: categoryFilter === tag ? `${colors.primary}20` : `${colors.textMuted}10`,
                              color: categoryFilter === tag ? colors.primary : colors.textMuted,
                            }}
                          >
                            {tag.toUpperCase()}
                          </span>
                        ))}
                      </div>
                      {template.usageCount !== undefined && (
                        <div className="flex items-center gap-1 text-[10px] font-mono" style={{ color: colors.textMuted }}>
                          <Copy size={10} />
                          {template.usageCount}
                        </div>
                      )}
                    </div>
                  </div>

                  {/* Action Row */}
                  <div
                    className="flex items-center justify-between px-3 py-2 border-t"
                    style={{ borderColor }}
                  >
                    <div className="flex items-center gap-1.5">
                      {template.mappingConfig.fieldMappings && (
                        <div className="flex items-center gap-1 text-[10px] font-mono" style={{ color: colors.textMuted }}>
                          <GitMerge size={10} color={colors.textMuted} />
                          {template.mappingConfig.fieldMappings.length} fields
                        </div>
                      )}
                    </div>
                    <div className="flex gap-1.5 opacity-0 group-hover:opacity-100 transition-opacity">
                      <button
                        onClick={(e) => { e.stopPropagation(); setSelectedTemplate(template); }}
                        className="flex items-center gap-1 px-2 py-1 text-[10px] font-bold tracking-wide transition-all"
                        style={{ backgroundColor: `${colors.textMuted}10`, color: colors.textMuted, border: `1px solid ${borderColor}` }}
                        onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = '#1F1F1F'; e.currentTarget.style.color = colors.text; }}
                        onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = `${colors.textMuted}10`; e.currentTarget.style.color = colors.textMuted; }}
                      >
                        <Eye size={10} />
                        PREVIEW
                      </button>
                      <button
                        onClick={(e) => { e.stopPropagation(); onSelectTemplate(template); }}
                        className="flex items-center gap-1 px-2 py-1 text-[10px] font-bold tracking-wide transition-all"
                        style={{ backgroundColor: colors.primary, color: colors.background }}
                        onMouseEnter={(e) => e.currentTarget.style.backgroundColor = '#FF8C00'}
                        onMouseLeave={(e) => e.currentTarget.style.backgroundColor = colors.primary}
                      >
                        USE
                        <ArrowRight size={10} />
                      </button>
                    </div>
                  </div>
                </div>
              );
            })
          )}
        </div>

        {/* Right: Detail / Preview Panel */}
        {selectedTemplate && (
          <div
            className="flex-shrink-0 flex flex-col overflow-hidden"
            style={{ width: '320px', backgroundColor: colors.panel, border: `1px solid ${colors.primary}` }}
          >
            {/* Detail header */}
            <div
              className="flex items-center justify-between px-3 py-2.5 flex-shrink-0"
              style={{ backgroundColor: `${colors.primary}15`, borderBottom: `1px solid ${colors.primary}30` }}
            >
              <div className="flex items-center gap-2">
                <Bookmark size={13} color={colors.primary} />
                <span className="text-[11px] font-bold tracking-wider" style={{ color: colors.primary }}>TEMPLATE DETAILS</span>
              </div>
              <button
                onClick={() => setSelectedTemplate(null)}
                className="p-0.5 transition-colors"
                style={{ color: colors.textMuted }}
                onMouseEnter={(e) => e.currentTarget.style.color = colors.text}
                onMouseLeave={(e) => e.currentTarget.style.color = colors.textMuted}
              >
                <X size={13} />
              </button>
            </div>

            <div className="flex-1 overflow-auto p-3 space-y-3">
              {/* Name & badges */}
              <div>
                <div className="flex items-center gap-2 mb-1">
                  <span className="text-xs font-bold" style={{ color: colors.text }}>{selectedTemplate.name}</span>
                  {selectedTemplate.verified && <CheckCircle2 size={12} color={colors.success} />}
                  {selectedTemplate.official && <Star size={12} color={colors.warning || '#F59E0B'} />}
                </div>
                <p className="text-[11px]" style={{ color: colors.text }}>{selectedTemplate.description}</p>
              </div>

              {/* Tags */}
              <div className="flex flex-wrap gap-1">
                {selectedTemplate.tags.map((tag) => (
                  <span
                    key={tag}
                    className="text-[10px] px-1.5 py-0.5 font-mono font-bold"
                    style={{ backgroundColor: `${colors.primary}15`, color: colors.primary }}
                  >
                    {tag.toUpperCase()}
                  </span>
                ))}
              </div>

              {/* Stats */}
              <div className="grid grid-cols-2 gap-2">
                {[
                  { label: 'Usage', value: selectedTemplate.usageCount ?? '—' },
                  { label: 'Rating', value: selectedTemplate.rating ? `${selectedTemplate.rating}/5` : '—' },
                  { label: 'Fields', value: selectedTemplate.mappingConfig.fieldMappings?.length ?? 0 },
                  { label: 'Schema', value: selectedTemplate.mappingConfig.target?.schema?.toUpperCase() ?? 'CUSTOM' },
                ].map(({ label, value }) => (
                  <div key={label} className="p-2" style={{ backgroundColor: colors.background, border: `1px solid ${borderColor}` }}>
                    <div className="text-[10px]" style={{ color: colors.text }}>{label}</div>
                    <div className="text-xs font-bold font-mono" style={{ color: colors.text }}>{String(value)}</div>
                  </div>
                ))}
              </div>

              {/* Instructions */}
              {selectedTemplate.instructions && (
                <div>
                  <div className="text-[10px] font-bold tracking-wider mb-1.5" style={{ color: colors.text }}>USAGE INSTRUCTIONS</div>
                  <div
                    className="p-2.5 text-[11px] font-mono whitespace-pre-wrap max-h-32 overflow-auto"
                    style={{ backgroundColor: colors.background, border: `1px solid ${borderColor}`, color: colors.text }}
                  >
                    {selectedTemplate.instructions}
                  </div>
                </div>
              )}

              {/* Field Mappings Preview */}
              {selectedTemplate.mappingConfig.fieldMappings && selectedTemplate.mappingConfig.fieldMappings.length > 0 && (
                <div>
                  <div className="text-[10px] font-bold tracking-wider mb-1.5" style={{ color: colors.text }}>
                    FIELD MAPPINGS ({selectedTemplate.mappingConfig.fieldMappings.length})
                  </div>
                  <div className="space-y-1 max-h-48 overflow-auto">
                    {selectedTemplate.mappingConfig.fieldMappings.map((mapping, idx) => (
                      <div
                        key={idx}
                        className="flex items-center gap-1 px-2 py-1.5 text-[10px] font-mono"
                        style={{ backgroundColor: colors.background, border: `1px solid ${borderColor}` }}
                      >
                        <span style={{ color: colors.primary }} className="truncate flex-1">{mapping.targetField}</span>
                        <span style={{ color: colors.textMuted }}>←</span>
                        <span style={{ color: colors.success }} className="truncate flex-1 text-right">
                          {mapping.sourceExpression || mapping.source?.path || '—'}
                        </span>
                      </div>
                    ))}
                  </div>
                </div>
              )}
            </div>

            {/* Use button */}
            <div className="p-3 flex-shrink-0" style={{ borderTop: `1px solid ${borderColor}` }}>
              <button
                onClick={() => onSelectTemplate(selectedTemplate)}
                className="w-full flex items-center justify-center gap-2 py-2.5 text-xs font-bold tracking-wide transition-all"
                style={{ backgroundColor: colors.primary, color: colors.background }}
                onMouseEnter={(e) => e.currentTarget.style.backgroundColor = '#FF8C00'}
                onMouseLeave={(e) => e.currentTarget.style.backgroundColor = colors.primary}
              >
                <Play size={13} />
                USE THIS TEMPLATE
                <ArrowRight size={13} />
              </button>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}
