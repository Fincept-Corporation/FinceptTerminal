// Visual Field Mapper - Terminal UI/UX

import React, { useState, useEffect } from 'react';
import { ChevronDown, ChevronRight, CheckCircle2, AlertCircle, Play, Copy, GitMerge, X, Activity } from 'lucide-react';
import { FieldMapping, CustomField, ParserEngine, UnifiedSchemaType } from '../types';
import { JSONExplorer } from './JSONExplorer';
import { UNIFIED_SCHEMAS } from '../schemas';
import { useTerminalTheme } from '@/contexts/ThemeContext';

interface VisualFieldMapperProps {
  schemaType: 'predefined' | 'custom';
  selectedSchema?: UnifiedSchemaType;
  customFields: CustomField[];
  sampleData: any;
  mappings: FieldMapping[];
  onMappingsChange: (mappings: FieldMapping[]) => void;
}

export function VisualFieldMapper({
  schemaType,
  selectedSchema,
  customFields,
  sampleData,
  mappings,
  onMappingsChange,
}: VisualFieldMapperProps) {
  const { colors } = useTerminalTheme();
  const [selectedSourcePath, setSelectedSourcePath] = useState<string[] | null>(null);
  const [selectedTargetField, setSelectedTargetField] = useState<string | null>(null);
  const [currentParser, setCurrentParser] = useState<FieldMapping['parser']>('jsonpath');
  const [previewField, setPreviewField] = useState<string | null>(null);
  const [previewValue, setPreviewValue] = useState<any>(null);
  const [expandedFields, setExpandedFields] = useState<Set<string>>(new Set());

  const targetFields: CustomField[] = schemaType === 'predefined' && selectedSchema
    ? Object.entries(UNIFIED_SCHEMAS[selectedSchema].fields).map(([name, spec]) => ({
        name,
        type: spec.type,
        description: spec.description,
        required: spec.required,
        defaultValue: spec.defaultValue,
        validation: spec.validation,
      }))
    : customFields;

  useEffect(() => {
    setExpandedFields(new Set(targetFields.map(f => f.name)));
  }, [targetFields.length]);

  const mappedCount = mappings.length;
  const requiredCount = targetFields.filter(f => f.required).length;
  const requiredMapped = mappings.filter(m => {
    const field = targetFields.find(f => f.name === m.targetField);
    return field?.required;
  }).length;

  const handleSourcePathSelect = (path: string[], _value: any) => {
    setSelectedSourcePath(path);
  };

  const handleTargetFieldClick = (fieldName: string) => {
    if (selectedTargetField === fieldName) {
      setSelectedTargetField(null);
    } else {
      setSelectedTargetField(fieldName);
      if (selectedSourcePath) {
        const pathString = '$' + selectedSourcePath.map(p => `['${p}']`).join('');
        createMapping(fieldName, pathString, currentParser);
        setSelectedSourcePath(null);
        setSelectedTargetField(null);
      }
    }
  };

  const createMapping = (targetField: string, sourcePath: string, parser: FieldMapping['parser']) => {
    const newMapping: FieldMapping = { targetField, source: { type: 'path', path: sourcePath }, parser, transform: [] };
    const existingIndex = mappings.findIndex(m => m.targetField === targetField);
    if (existingIndex >= 0) {
      const updated = [...mappings];
      updated[existingIndex] = newMapping;
      onMappingsChange(updated);
    } else {
      onMappingsChange([...mappings, newMapping]);
    }
  };

  const handleRemoveMapping = (targetField: string) => {
    onMappingsChange(mappings.filter(m => m.targetField !== targetField));
  };

  const handleExpressionChange = (targetField: string, expression: string, parser: FieldMapping['parser']) => {
    const newMapping: FieldMapping = { targetField, source: { type: 'path', path: expression }, parser, transform: [] };
    const existingIndex = mappings.findIndex(m => m.targetField === targetField);
    if (existingIndex >= 0) {
      const updated = [...mappings]; updated[existingIndex] = newMapping; onMappingsChange(updated);
    } else {
      onMappingsChange([...mappings, newMapping]);
    }
  };

  const handlePreview = async (fieldName: string) => {
    const mapping = mappings.find(m => m.targetField === fieldName);
    if (!mapping?.source.path) return;
    try {
      let result: any;
      if (mapping.parser === 'jsonpath') {
        const jsonpathPlus = await import('jsonpath-plus');
        const results = jsonpathPlus.JSONPath({ path: mapping.source.path, json: sampleData });
        result = Array.isArray(results) && results.length > 0 ? results[0] : null;
      } else if (mapping.parser === 'jmespath') {
        const jmespath = await import('jmespath');
        result = jmespath.search(sampleData, mapping.source.path);
      } else if (mapping.parser === 'direct') {
        result = mapping.source.path.split('.').reduce((obj: any, key: string) => obj?.[key], sampleData);
      } else if (mapping.parser === 'javascript') {
        const func = new Function('data', `return ${mapping.source.path}`);
        result = func(sampleData);
      }
      setPreviewField(fieldName);
      setPreviewValue(result);
    } catch (error) {
      setPreviewField(fieldName);
      setPreviewValue({ error: error instanceof Error ? error.message : String(error) });
    }
  };

  const getMappingStatus = (fieldName: string) => {
    const mapping = mappings.find(m => m.targetField === fieldName);
    const field = targetFields.find(f => f.name === fieldName);
    if (mapping) return { status: 'mapped', color: colors.success };
    if (field?.required) return { status: 'required', color: colors.alert };
    return { status: 'optional', color: colors.textMuted };
  };

  const toggleFieldExpand = (fieldName: string) => {
    const next = new Set(expandedFields);
    if (next.has(fieldName)) next.delete(fieldName); else next.add(fieldName);
    setExpandedFields(next);
  };

  const copyPath = (path: string) => navigator.clipboard.writeText(path);

  const parserEngines = [
    { value: 'jsonpath' as const, label: 'JSONPath', example: '$.data.price' },
    { value: 'jmespath' as const, label: 'JMESPath', example: 'data.price' },
    { value: 'direct' as const, label: 'Direct', example: 'data.price' },
    { value: 'javascript' as const, label: 'JavaScript', example: 'data.price * 2' },
  ];

  const borderColor = 'var(--ft-border-color, #2A2A2A)';

  return (
    <div className="flex flex-col h-full">
      {/* ── Stats Bar ── */}
      <div
        className="flex items-center justify-between px-3 py-2 mb-3 flex-shrink-0"
        style={{ backgroundColor: colors.panel, border: `1px solid ${borderColor}` }}
      >
        <div className="flex items-center gap-2">
          <GitMerge size={13} color={colors.primary} />
          <span className="text-xs font-bold tracking-wider" style={{ color: colors.text }}>FIELD MAPPER</span>
          <div className="h-3 w-px mx-1" style={{ backgroundColor: borderColor }} />
          <span className="text-[11px]" style={{ color: colors.text }}>
            {schemaType === 'predefined' ? selectedSchema?.toUpperCase() + ' SCHEMA' : 'CUSTOM SCHEMA'}
          </span>
        </div>
        <div className="flex items-center gap-4 text-[10px] font-mono">
          <span style={{ color: colors.success }}>
            MAPPED: <span className="font-bold">{mappedCount}</span>
          </span>
          <span style={{ color: colors.alert }}>
            REQ: <span className="font-bold">{requiredMapped}/{requiredCount}</span>
          </span>
          <span style={{ color: colors.textMuted }}>
            TOTAL: <span className="font-bold" style={{ color: colors.text }}>{targetFields.length}</span>
          </span>
        </div>
      </div>

      {/* ── Main split ── */}
      <div className="flex flex-1 gap-3 overflow-hidden min-h-0">

        {/* LEFT: Source Data */}
        <div className="flex flex-col flex-1 overflow-hidden">
          {/* Header */}
          <div
            className="flex items-center justify-between px-3 py-2 flex-shrink-0"
            style={{ backgroundColor: colors.panel, borderBottom: `1px solid ${borderColor}`, border: `1px solid ${borderColor}` }}
          >
            <div className="flex items-center gap-2">
              <Activity size={12} color={colors.primary} />
              <span className="text-[11px] font-bold tracking-wider" style={{ color: colors.text }}>SOURCE DATA</span>
            </div>
            <span className="text-[10px]" style={{ color: colors.text }}>Click path to select</span>
          </div>

          {/* JSON Explorer */}
          <div
            className="flex-1 overflow-auto p-2"
            style={{ backgroundColor: colors.background, border: `1px solid ${borderColor}`, borderTop: 'none' }}
          >
            <JSONExplorer
              data={sampleData}
              onFieldSelect={handleSourcePathSelect}
              selectedPath={selectedSourcePath || undefined}
            />
          </div>

          {/* Selected Path Indicator */}
          {selectedSourcePath && (
            <div
              className="flex-shrink-0 p-3 mt-2"
              style={{ backgroundColor: `${colors.info || '#0088FF'}10`, border: `1px solid ${colors.info || '#0088FF'}40` }}
            >
              <div className="flex items-center justify-between mb-2">
                <span className="text-[10px] font-bold tracking-wider" style={{ color: colors.info || '#0088FF' }}>SELECTED PATH</span>
                <div className="flex items-center gap-2">
                  <button
                    onClick={() => copyPath('$' + selectedSourcePath.map(p => `['${p}']`).join(''))}
                    className="flex items-center gap-1 text-[10px]"
                    style={{ color: colors.textMuted }}
                  >
                    <Copy size={10} /> COPY
                  </button>
                  <button onClick={() => setSelectedSourcePath(null)} style={{ color: colors.textMuted }}>
                    <X size={12} />
                  </button>
                </div>
              </div>
              <div className="text-[11px] font-mono mb-2" style={{ color: colors.info || '#0088FF' }}>
                {'$' + selectedSourcePath.map(p => `['${p}']`).join('')}
              </div>
              <div className="flex items-center gap-2">
                <select
                  value={currentParser}
                  onChange={(e) => setCurrentParser(e.target.value as FieldMapping['parser'])}
                  className="flex-1 px-2 py-1 text-[11px] font-mono"
                  style={{ backgroundColor: colors.background, border: `1px solid ${borderColor}`, color: colors.text, outline: 'none' }}
                >
                  {parserEngines.map(e => (
                    <option key={e.value} value={e.value}>{e.label}</option>
                  ))}
                </select>
              </div>
              <div className="text-[10px] mt-1.5" style={{ color: colors.text }}>
                Now click a target field to create mapping →
              </div>
            </div>
          )}
        </div>

        {/* RIGHT: Target Schema */}
        <div className="flex flex-col flex-1 overflow-hidden">
          {/* Header */}
          <div
            className="flex items-center justify-between px-3 py-2 flex-shrink-0"
            style={{ backgroundColor: colors.panel, border: `1px solid ${borderColor}` }}
          >
            <div className="flex items-center gap-2">
              <GitMerge size={12} color={colors.primary} />
              <span className="text-[11px] font-bold tracking-wider" style={{ color: colors.text }}>TARGET SCHEMA</span>
            </div>
            <span className="text-[10px] font-mono" style={{ color: colors.textMuted }}>
              {selectedTargetField ? (
                <span style={{ color: colors.primary }}>
                  SELECTING: {selectedTargetField}
                </span>
              ) : (
                `${mappedCount}/${targetFields.length} MAPPED`
              )}
            </span>
          </div>

          {/* Fields List */}
          <div
            className="flex-1 overflow-auto"
            style={{ backgroundColor: colors.background, border: `1px solid ${borderColor}`, borderTop: 'none' }}
          >
            {targetFields.length === 0 ? (
              <div className="flex items-center justify-center h-32">
                <div className="text-center">
                  <AlertCircle size={24} color={colors.textMuted} className="mx-auto mb-2" />
                  <p className="text-xs" style={{ color: colors.textMuted }}>No fields defined</p>
                </div>
              </div>
            ) : (
              <div className="p-2 space-y-1">
                {targetFields.map(field => {
                  const mapping = mappings.find(m => m.targetField === field.name);
                  const status = getMappingStatus(field.name);
                  const isExpanded = expandedFields.has(field.name);
                  const isSelected = selectedTargetField === field.name;
                  const isAwaitingMapping = !!selectedSourcePath;

                  let borderCol = borderColor;
                  if (isSelected) borderCol = colors.primary;
                  else if (mapping) borderCol = `${colors.success}60`;
                  else if (field.required) borderCol = `${colors.alert}40`;

                  return (
                    <div
                      key={field.name}
                      className="transition-all"
                      style={{
                        backgroundColor: isSelected ? `${colors.primary}10` : mapping ? `${colors.success}05` : colors.panel,
                        border: `1px solid ${borderCol}`,
                      }}
                    >
                      {/* Field Row */}
                      <div
                        className="flex items-center justify-between p-2 cursor-pointer transition-all"
                        onClick={() => handleTargetFieldClick(field.name)}
                        style={{
                          backgroundColor: isAwaitingMapping && !mapping ? `${colors.primary}05` : 'transparent',
                        }}
                        onMouseEnter={(e) => { if (!isSelected) (e.currentTarget as HTMLDivElement).style.backgroundColor = '#1F1F1F30'; }}
                        onMouseLeave={(e) => { (e.currentTarget as HTMLDivElement).style.backgroundColor = isAwaitingMapping && !mapping ? `${colors.primary}05` : 'transparent'; }}
                      >
                        <div className="flex items-center gap-2 flex-1 min-w-0">
                          <button
                            onClick={(e) => { e.stopPropagation(); toggleFieldExpand(field.name); }}
                            className="flex-shrink-0"
                            style={{ color: colors.textMuted }}
                          >
                            {isExpanded ? <ChevronDown size={12} /> : <ChevronRight size={12} />}
                          </button>

                          {/* Status dot */}
                          <div
                            className="w-2 h-2 rounded-full flex-shrink-0"
                            style={{ backgroundColor: status.color }}
                          />

                          <div className="min-w-0 flex-1">
                            <div className="flex items-center gap-1.5">
                              <span className="text-xs font-bold font-mono" style={{ color: colors.text }}>
                                {field.name}
                              </span>
                              {field.required && (
                                <span className="text-[10px] font-bold" style={{ color: colors.alert }}>*</span>
                              )}
                              <span
                                className="text-[10px] px-1.5 py-0.5 font-mono"
                                style={{ backgroundColor: `${colors.textMuted}15`, color: colors.textMuted }}
                              >
                                {field.type}
                              </span>
                            </div>
                          </div>
                        </div>

                        <div className="flex items-center gap-1 flex-shrink-0 ml-2">
                          {mapping && (
                            <>
                              <span
                                className="text-[10px] font-bold tracking-wider px-1.5 py-0.5"
                                style={{ backgroundColor: `${colors.success}15`, color: colors.success }}
                              >
                                MAPPED
                              </span>
                              <button
                                onClick={(e) => { e.stopPropagation(); handlePreview(field.name); }}
                                className="p-1 transition-colors"
                                style={{ color: colors.textMuted }}
                                onMouseEnter={(e) => e.currentTarget.style.color = colors.primary}
                                onMouseLeave={(e) => e.currentTarget.style.color = colors.textMuted}
                                title="Preview value"
                              >
                                <Play size={11} />
                              </button>
                            </>
                          )}
                          {!mapping && isAwaitingMapping && (
                            <span className="text-[10px] font-bold" style={{ color: colors.primary }}>← MAP</span>
                          )}
                        </div>
                      </div>

                      {/* Expanded Details */}
                      {isExpanded && (
                        <div
                          className="border-t p-2 space-y-2"
                          style={{ borderColor: 'var(--ft-border-color, #2A2A2A)', backgroundColor: colors.background }}
                        >
                          {field.description && (
                            <div className="text-[10px]" style={{ color: colors.text }}>{field.description}</div>
                          )}

                          {mapping ? (
                            <>
                              {/* Mapping Info */}
                              <div className="space-y-1">
                                <div className="flex items-center justify-between text-[10px]">
                                  <span className="font-bold" style={{ color: colors.text }}>PARSER</span>
                                  <span
                                    className="px-1.5 py-0.5 font-bold"
                                    style={{ backgroundColor: `${colors.primary}15`, color: colors.primary }}
                                  >
                                    {mapping.parser?.toUpperCase()}
                                  </span>
                                </div>
                                <div className="text-[11px] font-mono break-all p-2" style={{ backgroundColor: colors.panel, color: colors.text }}>
                                  {mapping.source.path}
                                </div>
                              </div>

                              {/* Preview value */}
                              {previewField === field.name && (
                                <div
                                  className="p-2"
                                  style={{ backgroundColor: `${colors.info || '#0088FF'}10`, border: `1px solid ${colors.info || '#0088FF'}30` }}
                                >
                                  <div className="text-[10px] font-bold mb-1" style={{ color: colors.info || '#0088FF' }}>PREVIEW</div>
                                  <pre className="text-[11px] font-mono max-h-20 overflow-auto" style={{ color: colors.text }}>
                                    {JSON.stringify(previewValue, null, 2)}
                                  </pre>
                                </div>
                              )}

                              {/* Edit row */}
                              <div className="flex gap-1">
                                <input
                                  type="text"
                                  value={mapping.source.path}
                                  onChange={(e) => handleExpressionChange(field.name, e.target.value, mapping.parser)}
                                  className="flex-1 px-2 py-1 text-[11px] font-mono"
                                  style={{ backgroundColor: colors.panel, border: `1px solid ${borderColor}`, color: colors.text, outline: 'none' }}
                                />
                                <button
                                  onClick={() => handleRemoveMapping(field.name)}
                                  className="px-2 py-1 text-[10px] font-bold transition-all"
                                  style={{ backgroundColor: `${colors.alert}15`, color: colors.alert, border: `1px solid ${colors.alert}30` }}
                                  onMouseEnter={(e) => e.currentTarget.style.backgroundColor = `${colors.alert}25`}
                                  onMouseLeave={(e) => e.currentTarget.style.backgroundColor = `${colors.alert}15`}
                                >
                                  <X size={12} />
                                </button>
                              </div>
                            </>
                          ) : (
                            /* Manual entry */
                            <div>
                              <div className="text-[10px] mb-1.5" style={{ color: colors.text }}>
                                Or enter expression manually:
                              </div>
                              <div className="flex gap-1">
                                <select
                                  value={currentParser}
                                  onChange={(e) => setCurrentParser(e.target.value as FieldMapping['parser'])}
                                  className="px-2 py-1 text-[10px] font-mono"
                                  style={{ backgroundColor: colors.panel, border: `1px solid ${borderColor}`, color: colors.text, outline: 'none' }}
                                >
                                  {parserEngines.map(e => (
                                    <option key={e.value} value={e.value}>{e.label}</option>
                                  ))}
                                </select>
                                <input
                                  type="text"
                                  placeholder={parserEngines.find(e => e.value === currentParser)?.example}
                                  onKeyDown={(e) => {
                                    if (e.key === 'Enter' && e.currentTarget.value.trim()) {
                                      handleExpressionChange(field.name, e.currentTarget.value.trim(), currentParser);
                                      e.currentTarget.value = '';
                                    }
                                  }}
                                  className="flex-1 px-2 py-1 text-[11px] font-mono"
                                  style={{ backgroundColor: colors.panel, border: `1px solid ${borderColor}`, color: colors.text, outline: 'none' }}
                                />
                              </div>
                              <div className="text-[10px] mt-1" style={{ color: colors.text }}>Press Enter to create mapping</div>
                            </div>
                          )}
                        </div>
                      )}
                    </div>
                  );
                })}
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}
