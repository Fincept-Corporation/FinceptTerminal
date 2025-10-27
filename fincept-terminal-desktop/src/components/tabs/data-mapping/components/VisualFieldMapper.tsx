// Visual Field Mapper Component - Map API response to schema fields

import React, { useState, useEffect } from 'react';
import { ChevronDown, ChevronRight, CheckCircle, AlertCircle, Play, Copy } from 'lucide-react';
import { FieldMapping, CustomField, ParserEngine, UnifiedSchemaType } from '../types';
import { JSONExplorer } from './JSONExplorer';
import { UNIFIED_SCHEMAS } from '../schemas';

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
  const [selectedSourcePath, setSelectedSourcePath] = useState<string[] | null>(null);
  const [selectedTargetField, setSelectedTargetField] = useState<string | null>(null);
  const [currentParser, setCurrentParser] = useState<FieldMapping['parser']>('jsonpath');
  const [previewField, setPreviewField] = useState<string | null>(null);
  const [previewValue, setPreviewValue] = useState<any>(null);
  const [expandedFields, setExpandedFields] = useState<Set<string>>(new Set());

  // Get target fields based on schema type
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
    // Auto-expand all fields initially
    const allFieldNames = targetFields.map(f => f.name);
    setExpandedFields(new Set(allFieldNames));
  }, [targetFields]);

  const handleSourcePathSelect = (path: string[], value: any) => {
    setSelectedSourcePath(path);
  };

  const handleTargetFieldClick = (fieldName: string) => {
    if (selectedTargetField === fieldName) {
      setSelectedTargetField(null);
    } else {
      setSelectedTargetField(fieldName);

      // If there's a selected source path, create the mapping
      if (selectedSourcePath) {
        // Convert path array to JSONPath string
        const pathString = '$' + selectedSourcePath.map(p => `['${p}']`).join('');
        createMapping(fieldName, pathString, currentParser);
        setSelectedSourcePath(null);
      }
    }
  };

  const createMapping = (targetField: string, sourcePath: string, parser: FieldMapping['parser']) => {
    const existingIndex = mappings.findIndex(m => m.targetField === targetField);
    const newMapping: FieldMapping = {
      targetField,
      source: {
        type: 'path',
        path: sourcePath,
      },
      parser,
      transform: [],
    };

    if (existingIndex >= 0) {
      // Update existing mapping
      const updated = [...mappings];
      updated[existingIndex] = newMapping;
      onMappingsChange(updated);
    } else {
      // Add new mapping
      onMappingsChange([...mappings, newMapping]);
    }
  };

  const handleRemoveMapping = (targetField: string) => {
    onMappingsChange(mappings.filter(m => m.targetField !== targetField));
  };

  const handleExpressionChange = (targetField: string, expression: string, parser: FieldMapping['parser']) => {
    const existingIndex = mappings.findIndex(m => m.targetField === targetField);
    const newMapping: FieldMapping = {
      targetField,
      source: {
        type: 'path',
        path: expression,
      },
      parser,
      transform: [],
    };

    if (existingIndex >= 0) {
      const updated = [...mappings];
      updated[existingIndex] = newMapping;
      onMappingsChange(updated);
    } else {
      onMappingsChange([...mappings, newMapping]);
    }
  };

  const handlePreview = async (fieldName: string) => {
    const mapping = mappings.find(m => m.targetField === fieldName);
    if (!mapping || !mapping.source.path) return;

    try {
      // Import parser dynamically based on parser type
      let result: any;

      if (mapping.parser === 'jsonpath') {
        const jsonpathPlus = await import('jsonpath-plus');
        const results = jsonpathPlus.JSONPath({ path: mapping.source.path, json: sampleData });
        result = Array.isArray(results) && results.length > 0 ? results[0] : null;
      } else if (mapping.parser === 'jmespath') {
        const jmespath = await import('jmespath');
        result = jmespath.search(sampleData, mapping.source.path);
      } else if (mapping.parser === 'direct') {
        // Direct property access
        const keys = mapping.source.path.split('.');
        result = keys.reduce((obj: any, key: string) => obj?.[key], sampleData);
      } else if (mapping.parser === 'javascript') {
        // Evaluate JavaScript expression
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

    if (mapping) {
      return { status: 'mapped', color: 'text-green-500', icon: CheckCircle };
    } else if (field?.required) {
      return { status: 'required', color: 'text-red-500', icon: AlertCircle };
    } else {
      return { status: 'optional', color: 'text-gray-500', icon: AlertCircle };
    }
  };

  const toggleFieldExpand = (fieldName: string) => {
    const newExpanded = new Set(expandedFields);
    if (newExpanded.has(fieldName)) {
      newExpanded.delete(fieldName);
    } else {
      newExpanded.add(fieldName);
    }
    setExpandedFields(newExpanded);
  };

  const copyPathToClipboard = (path: string) => {
    navigator.clipboard.writeText(path);
  };

  const parserEngines: { value: FieldMapping['parser']; label: string; description: string }[] = [
    { value: 'jsonpath', label: 'JSONPath', description: '$.data.price' },
    { value: 'jmespath', label: 'JMESPath', description: 'data.price' },
    { value: 'direct', label: 'Direct', description: 'data.price' },
    { value: 'javascript', label: 'JavaScript', description: 'data.price * 2' },
  ];

  return (
    <div className="grid grid-cols-2 gap-4 h-full">
      {/* Left Panel - Source Data */}
      <div className="flex flex-col h-full">
        <div className="mb-3">
          <h3 className="text-xs font-bold text-orange-500 uppercase mb-2">
            Source Data (Sample Response)
          </h3>
          <p className="text-xs text-gray-400">
            Click on any path to select it for mapping
          </p>
        </div>

        <div className="flex-1 bg-zinc-900 border border-zinc-700 rounded p-3 overflow-auto">
          <JSONExplorer
            data={sampleData}
            onFieldSelect={handleSourcePathSelect}
            selectedPath={selectedSourcePath || undefined}
          />
        </div>

        {selectedSourcePath && (
          <div className="mt-3 p-3 bg-blue-900/20 border border-blue-700 rounded">
            <div className="flex items-center justify-between mb-2">
              <div className="text-xs font-bold text-blue-400">SELECTED PATH</div>
              <button
                onClick={() => copyPathToClipboard('$' + selectedSourcePath.map(p => `['${p}']`).join(''))}
                className="text-blue-400 hover:text-blue-300 p-1"
                title="Copy path"
              >
                <Copy size={12} />
              </button>
            </div>
            <div className="text-xs text-blue-300 font-mono break-all">
              {'$' + selectedSourcePath.map(p => `['${p}']`).join('')}
            </div>
            <div className="mt-2">
              <label className="text-xs text-gray-400 block mb-1">Parser Engine</label>
              <select
                value={currentParser}
                onChange={(e) => setCurrentParser(e.target.value as FieldMapping['parser'])}
                className="w-full bg-zinc-800 border border-zinc-700 text-gray-300 px-2 py-1 text-xs rounded focus:outline-none focus:border-orange-500"
              >
                {parserEngines.map(engine => (
                  <option key={engine.value} value={engine.value}>
                    {engine.label} - {engine.description}
                  </option>
                ))}
              </select>
            </div>
            <div className="mt-2 text-xs text-gray-400">
              Click a target field to create mapping →
            </div>
          </div>
        )}
      </div>

      {/* Right Panel - Target Schema */}
      <div className="flex flex-col h-full">
        <div className="mb-3">
          <h3 className="text-xs font-bold text-orange-500 uppercase mb-2">
            Target Schema Fields
          </h3>
          <p className="text-xs text-gray-400">
            {schemaType === 'predefined'
              ? `${selectedSchema?.toUpperCase()} Schema`
              : 'Custom Schema'
            }
          </p>
        </div>

        <div className="flex-1 bg-zinc-900 border border-zinc-700 rounded p-3 overflow-auto">
          {targetFields.length === 0 ? (
            <div className="text-center py-8 text-gray-500">
              <AlertCircle size={32} className="mx-auto mb-2" />
              <p className="text-sm">No fields defined</p>
            </div>
          ) : (
            <div className="space-y-2">
              {targetFields.map(field => {
                const mapping = mappings.find(m => m.targetField === field.name);
                const status = getMappingStatus(field.name);
                const isExpanded = expandedFields.has(field.name);
                const StatusIcon = status.icon;

                return (
                  <div
                    key={field.name}
                    className={`border rounded transition-all ${
                      selectedTargetField === field.name
                        ? 'border-orange-500 bg-orange-900/20'
                        : mapping
                        ? 'border-green-700 bg-green-900/10'
                        : 'border-zinc-700 bg-zinc-800'
                    }`}
                  >
                    {/* Field Header */}
                    <div
                      className="flex items-center justify-between p-2 cursor-pointer hover:bg-zinc-700/50"
                      onClick={() => handleTargetFieldClick(field.name)}
                    >
                      <div className="flex items-center gap-2 flex-1">
                        <button
                          onClick={(e) => {
                            e.stopPropagation();
                            toggleFieldExpand(field.name);
                          }}
                          className="text-gray-400 hover:text-white"
                        >
                          {isExpanded ? <ChevronDown size={14} /> : <ChevronRight size={14} />}
                        </button>
                        <StatusIcon size={14} className={status.color} />
                        <div>
                          <div className="text-xs font-bold text-white font-mono">
                            {field.name}
                            {field.required && (
                              <span className="text-red-500 ml-1">*</span>
                            )}
                          </div>
                          <div className="text-xs text-gray-400">{field.type}</div>
                        </div>
                      </div>

                      {mapping && (
                        <button
                          onClick={(e) => {
                            e.stopPropagation();
                            handlePreview(field.name);
                          }}
                          className="text-blue-400 hover:text-blue-300 p-1 mr-2"
                          title="Preview value"
                        >
                          <Play size={12} />
                        </button>
                      )}
                    </div>

                    {/* Expanded Details */}
                    {isExpanded && (
                      <div className="border-t border-zinc-700 p-2 space-y-2">
                        {field.description && (
                          <div className="text-xs text-gray-400">{field.description}</div>
                        )}

                        {mapping ? (
                          <>
                            {/* Current Mapping */}
                            <div className="bg-zinc-900 border border-zinc-700 rounded p-2">
                              <div className="text-xs font-bold text-green-500 mb-1">
                                MAPPED
                              </div>
                              <div className="text-xs text-gray-400 mb-1">
                                Parser: {mapping.parser}
                              </div>
                              <div className="text-xs text-gray-300 font-mono break-all">
                                {mapping.source.path}
                              </div>
                            </div>

                            {/* Preview */}
                            {previewField === field.name && (
                              <div className="bg-blue-900/20 border border-blue-700 rounded p-2">
                                <div className="text-xs font-bold text-blue-400 mb-1">
                                  PREVIEW VALUE
                                </div>
                                <pre className="text-xs text-blue-300 font-mono whitespace-pre-wrap overflow-auto max-h-24">
                                  {JSON.stringify(previewValue, null, 2)}
                                </pre>
                              </div>
                            )}

                            {/* Edit/Remove */}
                            <div className="flex gap-2">
                              <input
                                type="text"
                                value={mapping.source.path}
                                onChange={(e) =>
                                  handleExpressionChange(field.name, e.target.value, mapping.parser)
                                }
                                placeholder="Expression"
                                className="flex-1 bg-zinc-900 border border-zinc-700 text-gray-300 px-2 py-1 text-xs rounded focus:outline-none focus:border-orange-500 font-mono"
                              />
                              <button
                                onClick={() => handleRemoveMapping(field.name)}
                                className="bg-red-900/30 hover:bg-red-900/50 text-red-400 px-2 py-1 rounded text-xs"
                              >
                                Remove
                              </button>
                            </div>
                          </>
                        ) : (
                          <>
                            {/* Manual Entry */}
                            <div>
                              <label className="text-xs text-gray-400 block mb-1">
                                Or enter expression manually:
                              </label>
                              <div className="flex gap-2">
                                <select
                                  value={currentParser}
                                  onChange={(e) => setCurrentParser(e.target.value as FieldMapping['parser'])}
                                  className="bg-zinc-900 border border-zinc-700 text-gray-300 px-2 py-1 text-xs rounded focus:outline-none focus:border-orange-500"
                                >
                                  {parserEngines.map(engine => (
                                    <option key={engine.value} value={engine.value}>
                                      {engine.label}
                                    </option>
                                  ))}
                                </select>
                                <input
                                  type="text"
                                  placeholder={parserEngines.find(e => e.value === currentParser)?.description}
                                  onKeyDown={(e) => {
                                    if (e.key === 'Enter' && e.currentTarget.value.trim()) {
                                      handleExpressionChange(
                                        field.name,
                                        e.currentTarget.value.trim(),
                                        currentParser
                                      );
                                      e.currentTarget.value = '';
                                    }
                                  }}
                                  className="flex-1 bg-zinc-900 border border-zinc-700 text-gray-300 px-2 py-1 text-xs rounded focus:outline-none focus:border-orange-500 font-mono"
                                />
                              </div>
                              <div className="text-xs text-gray-500 mt-1">
                                Press Enter to create mapping
                              </div>
                            </div>
                          </>
                        )}
                      </div>
                    )}
                  </div>
                );
              })}
            </div>
          )}
        </div>

        {/* Mapping Summary */}
        <div className="mt-3 p-3 bg-zinc-900 border border-zinc-700 rounded">
          <div className="text-xs font-bold text-orange-500 uppercase mb-2">
            Mapping Status
          </div>
          <div className="grid grid-cols-3 gap-2 text-xs">
            <div>
              <span className="text-gray-400">Mapped:</span>{' '}
              <span className="text-green-500 font-bold">{mappings.length}</span>
            </div>
            <div>
              <span className="text-gray-400">Required:</span>{' '}
              <span className="text-yellow-500 font-bold">
                {targetFields.filter(f => f.required).length}
              </span>
            </div>
            <div>
              <span className="text-gray-400">Total:</span>{' '}
              <span className="text-white font-bold">{targetFields.length}</span>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
