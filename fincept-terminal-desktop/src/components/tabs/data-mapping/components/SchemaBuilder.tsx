// Schema Builder Component - Choose predefined or create custom schema

import React, { useState } from 'react';
import { Plus, Trash2, Database, Edit3, Sparkles, AlertCircle } from 'lucide-react';
import { CustomField, FieldType } from '../types';
import { SchemaSelector } from './SchemaSelector';
import { JSONExplorer } from './JSONExplorer';

interface SchemaBuilderProps {
  schemaType: 'predefined' | 'custom';
  selectedSchema?: string;
  customFields: CustomField[];
  onSchemaTypeChange: (type: 'predefined' | 'custom') => void;
  onSchemaSelect: (schema: string) => void;
  onCustomFieldsChange: (fields: CustomField[]) => void;
  sampleData?: any;
  onFieldMappingsAutoCreate?: (mappings: any[]) => void; // NEW: Auto-create field mappings
}

export function SchemaBuilder({
  schemaType,
  selectedSchema,
  customFields,
  onSchemaTypeChange,
  onSchemaSelect,
  onCustomFieldsChange,
  sampleData,
  onFieldMappingsAutoCreate,
}: SchemaBuilderProps) {
  const [editingField, setEditingField] = useState<number | null>(null);
  const [selectedJsonPath, setSelectedJsonPath] = useState<string[] | undefined>();

  // Auto-detect field type from value
  const detectFieldType = (value: any): FieldType => {
    if (value === null || value === undefined) return 'string';
    if (Array.isArray(value)) return 'array';
    if (typeof value === 'object') return 'object';
    if (typeof value === 'boolean') return 'boolean';
    if (typeof value === 'number') return 'number';
    if (typeof value === 'string') {
      // Check if it's a date string
      if (!isNaN(Date.parse(value)) && value.match(/^\d{4}-\d{2}-\d{2}/)) {
        return 'datetime';
      }
      return 'string';
    }
    return 'string';
  };

  // Generate unique field name from full path
  const generateFieldNameFromPath = (path: string[]): string => {
    // Remove 'root' prefix if present
    const cleanPath = path[0] === 'root' ? path.slice(1) : path;

    // Clean up each segment
    const cleanedSegments = cleanPath.map(segment =>
      segment
        .replace(/^\[|\]$/g, '') // Remove array brackets
        .replace(/\[(\d+)\]/, '_$1') // Convert [0] to _0
    );

    // Join with underscore for nested paths
    return cleanedSegments.join('_');
  };

  // Create field mapping from path
  const createFieldMapping = (fieldName: string, path: string[], isRequired: boolean = false) => {
    // Convert path to JSONPath expression
    // For paths from auto-detection, we assume rootPath has already extracted the array
    // So we just need the relative path from the current item
    let jsonPathExpression: string;

    if (path.length === 0) {
      jsonPathExpression = '$';
    } else if (path.length === 1) {
      // Simple field at root level
      jsonPathExpression = `$.${path[0]}`;
    } else {
      // Nested field - build path without root
      jsonPathExpression = '$.' + path.join('.');
    }

    return {
      targetField: fieldName,
      source: {
        type: 'path' as const,
        path: jsonPathExpression,
      },
      parser: 'jsonpath' as const,
      required: isRequired,
    };
  };

  // Handle field selection from JSON tree
  const handleFieldSelectFromJson = (path: string[], value: any) => {
    const fullPath = path.join('.');
    const fieldName = generateFieldNameFromPath(path);
    const fieldType = detectFieldType(value);

    // Check if field already exists by full path
    const existingIndex = customFields.findIndex(f =>
      f.description?.includes(fullPath) || f.name === fieldName
    );

    if (existingIndex >= 0) {
      // Field exists, highlight it
      setEditingField(existingIndex);
      return;
    }

    // Don't add complex objects/arrays directly (only leaf values)
    if (fieldType === 'object' || fieldType === 'array') {
      alert('Please expand this field and select a specific property inside it.');
      return;
    }

    // Create new field with auto-detected properties
    const newField: CustomField = {
      name: fieldName,
      type: fieldType,
      description: `Path: ${fullPath}`,
      required: false,
      defaultValue: value === null || value === undefined ? undefined : value,
    };

    const updatedFields = [...customFields, newField];
    onCustomFieldsChange(updatedFields);

    // Auto-create field mapping
    if (onFieldMappingsAutoCreate) {
      const newMapping = createFieldMapping(fieldName, path, newField.required);
      onFieldMappingsAutoCreate([newMapping]);
    }

    setEditingField(customFields.length);
    setSelectedJsonPath(path);
  };

  const handleAddField = () => {
    const newField: CustomField = {
      name: '',
      type: 'string',
      description: '',
      required: false,
    };
    onCustomFieldsChange([...customFields, newField]);
    setEditingField(customFields.length);
  };

  // Auto-add all primitive fields from sample data
  const handleAutoAddAllFields = () => {
    if (!sampleData) return;

    // Store field paths for mapping creation
    const fieldPaths: { field: CustomField; path: string[] }[] = [];

    // Detect if data has a common array structure (e.g., data: [...])
    const detectDataArray = (obj: any): { data: any; arrayKey: string | null } => {
      if (Array.isArray(obj)) return { data: obj, arrayKey: null };

      // Common patterns: data, results, items, records
      const arrayKeys = ['data', 'results', 'items', 'records', 'response'];
      for (const key of arrayKeys) {
        if (obj[key] && Array.isArray(obj[key]) && obj[key].length > 0) {
          return { data: obj[key], arrayKey: key };
        }
      }

      return { data: obj, arrayKey: null };
    };

    const extractFields = (obj: any, pathArray: string[] = []): CustomField[] => {
      const fields: CustomField[] = [];

      if (typeof obj !== 'object' || obj === null) return fields;

      // Handle array - use first item as template
      if (Array.isArray(obj)) {
        if (obj.length > 0) {
          return extractFields(obj[0], pathArray);
        }
        return fields;
      }

      // Handle object - extract all leaf nodes
      for (const [key, value] of Object.entries(obj)) {
        const currentPath = [...pathArray, key];
        const fieldType = detectFieldType(value);

        // Only add primitive fields (leaf nodes)
        if (fieldType !== 'object' && fieldType !== 'array') {
          const fieldName = key; // Use simple key name for single-level fields
          const nestedFieldName = currentPath.join('_'); // Use underscore for nested
          const actualFieldName = currentPath.length === 1 ? fieldName : nestedFieldName;
          const fullPath = currentPath.join('.');

          const field: CustomField = {
            name: actualFieldName,
            type: fieldType,
            description: `Path: ${fullPath}`,
            required: false,
          };

          fields.push(field);
          fieldPaths.push({ field, path: currentPath });
        } else if (fieldType === 'object' && value !== null) {
          // Recursively extract from nested objects
          fields.push(...extractFields(value, currentPath));
        } else if (fieldType === 'array' && Array.isArray(value) && value.length > 0) {
          // Extract from first array item if it's an array of objects
          const firstItem = value[0];
          if (typeof firstItem === 'object' && firstItem !== null) {
            fields.push(...extractFields(firstItem, currentPath));
          }
        }
      }

      return fields;
    };

    // Try to detect the main data array
    const { data: dataToProcess } = detectDataArray(sampleData);
    const autoFields = extractFields(dataToProcess);

    // Remove duplicates based on field name
    const uniqueFields = autoFields.filter((field, index, self) =>
      index === self.findIndex(f => f.name === field.name)
    );

    // Limit to reasonable number (prevent UI overload)
    const limitedFields = uniqueFields.slice(0, 50);

    if (uniqueFields.length > 50) {
      alert(`Found ${uniqueFields.length} fields. Adding first 50 to prevent UI overload. You can add more manually.`);
    }

    onCustomFieldsChange(limitedFields);

    // Auto-create field mappings for all added fields
    if (onFieldMappingsAutoCreate) {
      const mappings = fieldPaths
        .filter(({ field }) => limitedFields.some(f => f.name === field.name))
        .map(({ field, path }) => createFieldMapping(field.name, path, field.required));

      onFieldMappingsAutoCreate(mappings);
    }
  };

  const handleRemoveField = (index: number) => {
    onCustomFieldsChange(customFields.filter((_, i) => i !== index));
    if (editingField === index) {
      setEditingField(null);
    }
  };

  const handleFieldChange = (
    index: number,
    field: keyof CustomField,
    value: any
  ) => {
    const updated = [...customFields];
    updated[index] = { ...updated[index], [field]: value };
    onCustomFieldsChange(updated);
  };

  const fieldTypeOptions: FieldType[] = [
    'string',
    'number',
    'boolean',
    'datetime',
    'array',
    'object',
  ];

  return (
    <div className="space-y-4">
      {/* Schema Type Selector */}
      <div>
        <h3 className="text-xs font-bold text-orange-500 uppercase mb-3">
          Choose Schema Type
        </h3>

        <div className="grid grid-cols-2 gap-3">
          <button
            onClick={() => onSchemaTypeChange('predefined')}
            className={`p-4 rounded-lg border transition-all ${
              schemaType === 'predefined'
                ? 'border-orange-500 bg-orange-900/20'
                : 'border-zinc-700 bg-zinc-900 hover:border-zinc-600'
            }`}
          >
            <Database size={24} className="mx-auto mb-2 text-orange-500" />
            <div className="text-sm font-bold text-white mb-1">
              Predefined Schema
            </div>
            <div className="text-xs text-gray-400">
              Use built-in schemas (OHLCV, QUOTE, ORDER, etc.)
            </div>
          </button>

          <button
            onClick={() => onSchemaTypeChange('custom')}
            className={`p-4 rounded-lg border transition-all ${
              schemaType === 'custom'
                ? 'border-orange-500 bg-orange-900/20'
                : 'border-zinc-700 bg-zinc-900 hover:border-zinc-600'
            }`}
          >
            <Edit3 size={24} className="mx-auto mb-2 text-purple-500" />
            <div className="text-sm font-bold text-white mb-1">
              Custom Schema
            </div>
            <div className="text-xs text-gray-400">
              Define your own fields for any data type
            </div>
          </button>
        </div>
      </div>

      {/* Predefined Schema Selection */}
      {schemaType === 'predefined' && (
        <div>
          <SchemaSelector
            selectedSchema={selectedSchema}
            onSelect={onSchemaSelect}
          />
        </div>
      )}

      {/* Custom Schema Builder */}
      {schemaType === 'custom' && (
        <div>
          {/* Instructions Banner */}
          {sampleData ? (
            <div className="mb-4 p-4 bg-blue-900/20 border border-blue-700 rounded-lg">
              <div className="flex items-start gap-3">
                <Sparkles size={20} className="text-blue-400 flex-shrink-0 mt-0.5" />
                <div className="flex-1">
                  <h4 className="text-sm font-bold text-blue-400 mb-2">
                    ✨ Smart Field Extraction
                  </h4>
                  <div className="text-xs text-blue-300 space-y-1">
                    <p><strong>1. Auto-Add All:</strong> Click the green "AUTO-ADD ALL" button to add all fields from your data array automatically</p>
                    <p><strong>2. Manual Select:</strong> Expand the tree (left), click any <strong>leaf field</strong> (not objects/arrays) to add it</p>
                    <p><strong>3. Unique Names:</strong> Nested fields get full path names (e.g., <code className="bg-blue-900/40 px-1 rounded">country_name</code>)</p>
                    <p><strong>4. Smart Detection:</strong> Automatically finds your data array (looks for "data", "results", "items")</p>
                  </div>
                </div>
              </div>
            </div>
          ) : (
            <div className="mb-4 p-4 bg-yellow-900/20 border border-yellow-700 rounded-lg">
              <div className="flex items-start gap-3">
                <AlertCircle size={20} className="text-yellow-400 flex-shrink-0 mt-0.5" />
                <div>
                  <h4 className="text-sm font-bold text-yellow-400 mb-1">
                    No Sample Data Available
                  </h4>
                  <p className="text-xs text-yellow-300">
                    Go back to Step 1 and click "Fetch Sample Data" to see your API response here.
                    This will make it much easier to create your schema!
                  </p>
                </div>
              </div>
            </div>
          )}

          {/* Two-Column Layout */}
          <div className="grid grid-cols-2 gap-4">
            {/* LEFT: JSON Response Explorer */}
            <div>
              <div className="flex items-center justify-between mb-2">
                <h3 className="text-xs font-bold text-green-500 uppercase">
                  API Response
                </h3>
                {sampleData && (
                  <button
                    onClick={handleAutoAddAllFields}
                    className="bg-green-600 hover:bg-green-700 text-white px-2 py-1 rounded text-xs font-bold flex items-center gap-1"
                    title="Auto-add all primitive fields"
                  >
                    <Sparkles size={12} />
                    AUTO-ADD ALL
                  </button>
                )}
              </div>

              {sampleData ? (
                <JSONExplorer
                  data={sampleData}
                  onFieldSelect={handleFieldSelectFromJson}
                  selectedPath={selectedJsonPath}
                />
              ) : (
                <div className="bg-zinc-900 rounded border border-zinc-700 p-8 text-center">
                  <AlertCircle size={32} className="mx-auto mb-2 text-gray-600" />
                  <p className="text-sm text-gray-400">No sample data</p>
                  <p className="text-xs text-gray-500 mt-1">
                    Fetch sample data in Step 1 to see it here
                  </p>
                </div>
              )}
            </div>

            {/* RIGHT: Schema Fields */}
            <div>
              <div className="flex items-center justify-between mb-2">
                <h3 className="text-xs font-bold text-orange-500 uppercase">
                  Schema Fields ({customFields.length})
                </h3>
                <button
                  onClick={handleAddField}
                  className="bg-orange-600 hover:bg-orange-700 text-white px-2 py-1 rounded text-xs font-bold flex items-center gap-1"
                >
                  <Plus size={12} />
                  ADD MANUALLY
                </button>
              </div>

              <div className="bg-zinc-900 rounded border border-zinc-700 p-3 max-h-[600px] overflow-y-auto">
                {customFields.length === 0 ? (
                  <div className="text-center py-12">
                    <Edit3 size={32} className="mx-auto mb-2 text-gray-600" />
                    <p className="text-sm text-gray-400 mb-1">No fields yet</p>
                    <p className="text-xs text-gray-500">
                      {sampleData
                        ? 'Click fields in the API response to add them'
                        : 'Add fields manually or fetch sample data first'}
                    </p>
                  </div>
                ) : (
                  <div className="space-y-2">
                    {customFields.map((field, index) => (
                      <div
                        key={index}
                        className={`bg-zinc-800 border rounded p-2 transition-all ${
                          editingField === index
                            ? 'border-orange-500 shadow-lg shadow-orange-900/20'
                            : 'border-zinc-700'
                        }`}
                      >
                        <div className="grid grid-cols-2 gap-2 mb-2">
                          <div>
                            <label className="text-xs text-gray-400 block mb-1">
                              Field Name *
                            </label>
                            <input
                              type="text"
                              value={field.name}
                              onChange={(e) =>
                                handleFieldChange(index, 'name', e.target.value)
                              }
                              onFocus={() => setEditingField(index)}
                              placeholder="field_name"
                              className="w-full bg-zinc-900 border border-zinc-700 text-gray-300 px-2 py-1 text-xs rounded focus:outline-none focus:border-orange-500 font-mono"
                            />
                          </div>

                          <div>
                            <label className="text-xs text-gray-400 block mb-1">
                              Type
                            </label>
                            <select
                              value={field.type}
                              onChange={(e) =>
                                handleFieldChange(
                                  index,
                                  'type',
                                  e.target.value as FieldType
                                )
                              }
                              className="w-full bg-zinc-900 border border-zinc-700 text-gray-300 px-2 py-1 text-xs rounded focus:outline-none focus:border-orange-500"
                            >
                              {fieldTypeOptions.map((type) => (
                                <option key={type} value={type}>
                                  {type}
                                </option>
                              ))}
                            </select>
                          </div>
                        </div>

                        <div className="mb-2">
                          <label className="text-xs text-gray-400 block mb-1">
                            Description
                          </label>
                          <input
                            type="text"
                            value={field.description}
                            onChange={(e) =>
                              handleFieldChange(index, 'description', e.target.value)
                            }
                            placeholder="Field description"
                            className="w-full bg-zinc-900 border border-zinc-700 text-gray-300 px-2 py-1 text-xs rounded focus:outline-none focus:border-orange-500"
                          />
                        </div>

                        <div className="flex items-center justify-between">
                          <label className="flex items-center gap-2 text-xs text-gray-300 cursor-pointer">
                            <input
                              type="checkbox"
                              checked={field.required}
                              onChange={(e) =>
                                handleFieldChange(index, 'required', e.target.checked)
                              }
                              className="rounded border-zinc-700 bg-zinc-800 text-orange-600 focus:ring-orange-500"
                            />
                            Required
                          </label>

                          <button
                            onClick={() => handleRemoveField(index)}
                            className="text-red-500 hover:text-red-400 p-1 hover:bg-zinc-900 rounded transition-colors"
                            title="Remove field"
                          >
                            <Trash2 size={14} />
                          </button>
                        </div>
                      </div>
                    ))}
                  </div>
                )}
              </div>
            </div>
          </div>

          {/* Helper Text */}
          {customFields.length > 0 && (
            <div className="mt-3 text-xs text-green-500 bg-green-900/20 border border-green-700 rounded p-3">
              <strong>✓ {customFields.length} field{customFields.length !== 1 ? 's' : ''} added!</strong>
              {' '}Field mappings have been created automatically. You can review them in Step 3 (Field Mapping).
            </div>
          )}
        </div>
      )}
    </div>
  );
}
