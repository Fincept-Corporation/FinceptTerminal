// Schema Builder - Terminal UI/UX

import React, { useState } from 'react';
import { Plus, Trash2, Database, Edit3, Sparkles, AlertCircle, Layers, CheckCircle2 } from 'lucide-react';
import { CustomField, FieldType } from '../types';
import { SchemaSelector } from './SchemaSelector';
import { JSONExplorer } from './JSONExplorer';
import { showWarning } from '@/utils/notifications';
import { useTerminalTheme } from '@/contexts/ThemeContext';

interface SchemaBuilderProps {
  schemaType: 'predefined' | 'custom';
  selectedSchema?: string;
  customFields: CustomField[];
  onSchemaTypeChange: (type: 'predefined' | 'custom') => void;
  onSchemaSelect: (schema: string) => void;
  onCustomFieldsChange: (fields: CustomField[]) => void;
  sampleData?: any;
  onFieldMappingsAutoCreate?: (mappings: any[]) => void;
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
  const { colors } = useTerminalTheme();
  const [editingField, setEditingField] = useState<number | null>(null);
  const [selectedJsonPath, setSelectedJsonPath] = useState<string[] | undefined>();

  const borderColor = 'var(--ft-border-color, #2A2A2A)';

  const detectFieldType = (value: any): FieldType => {
    if (value === null || value === undefined) return 'string';
    if (Array.isArray(value)) return 'array';
    if (typeof value === 'object') return 'object';
    if (typeof value === 'boolean') return 'boolean';
    if (typeof value === 'number') return 'number';
    if (typeof value === 'string') {
      if (!isNaN(Date.parse(value)) && value.match(/^\d{4}-\d{2}-\d{2}/)) return 'datetime';
      return 'string';
    }
    return 'string';
  };

  const generateFieldNameFromPath = (path: string[]): string => {
    const cleanPath = path[0] === 'root' ? path.slice(1) : path;
    return cleanPath.map(s => s.replace(/^\[|\]$/g, '').replace(/\[(\d+)\]/, '_$1')).join('_');
  };

  const createFieldMapping = (fieldName: string, path: string[], isRequired: boolean = false) => {
    const jsonPathExpression = path.length === 0 ? '$' : path.length === 1 ? `$.${path[0]}` : '$.' + path.join('.');
    return { targetField: fieldName, source: { type: 'path' as const, path: jsonPathExpression }, parser: 'jsonpath' as const, required: isRequired };
  };

  const handleFieldSelectFromJson = (path: string[], value: any) => {
    const fullPath = path.join('.');
    const fieldName = generateFieldNameFromPath(path);
    const fieldType = detectFieldType(value);
    const existingIndex = customFields.findIndex(f => f.description?.includes(fullPath) || f.name === fieldName);
    if (existingIndex >= 0) { setEditingField(existingIndex); return; }
    if (fieldType === 'object' || fieldType === 'array') { showWarning('Please expand this field and select a specific property inside it.'); return; }
    const newField: CustomField = { name: fieldName, type: fieldType, description: `Path: ${fullPath}`, required: false, defaultValue: value === null || value === undefined ? undefined : value };
    const updatedFields = [...customFields, newField];
    onCustomFieldsChange(updatedFields);
    if (onFieldMappingsAutoCreate) onFieldMappingsAutoCreate([createFieldMapping(fieldName, path, newField.required)]);
    setEditingField(customFields.length);
    setSelectedJsonPath(path);
  };

  const handleAddField = () => {
    onCustomFieldsChange([...customFields, { name: '', type: 'string', description: '', required: false }]);
    setEditingField(customFields.length);
  };

  const handleAutoAddAllFields = () => {
    if (!sampleData) return;
    const fieldPaths: { field: CustomField; path: string[] }[] = [];
    const detectDataArray = (obj: any): { data: any } => {
      if (Array.isArray(obj)) return { data: obj };
      for (const key of ['data', 'results', 'items', 'records', 'response']) {
        if (obj[key] && Array.isArray(obj[key]) && obj[key].length > 0) return { data: obj[key] };
      }
      return { data: obj };
    };
    const extractFields = (obj: any, pathArray: string[] = []): CustomField[] => {
      const fields: CustomField[] = [];
      if (typeof obj !== 'object' || obj === null) return fields;
      if (Array.isArray(obj)) return obj.length > 0 ? extractFields(obj[0], pathArray) : fields;
      for (const [key, value] of Object.entries(obj)) {
        const currentPath = [...pathArray, key];
        const fieldType = detectFieldType(value);
        if (fieldType !== 'object' && fieldType !== 'array') {
          const actualFieldName = currentPath.length === 1 ? key : currentPath.join('_');
          const field: CustomField = { name: actualFieldName, type: fieldType, description: `Path: ${currentPath.join('.')}`, required: false };
          fields.push(field);
          fieldPaths.push({ field, path: currentPath });
        } else if (fieldType === 'object' && value !== null) {
          fields.push(...extractFields(value, currentPath));
        } else if (fieldType === 'array' && Array.isArray(value) && value.length > 0) {
          const firstItem = value[0];
          if (typeof firstItem === 'object' && firstItem !== null) fields.push(...extractFields(firstItem, currentPath));
        }
      }
      return fields;
    };
    const { data: dataToProcess } = detectDataArray(sampleData);
    const autoFields = extractFields(dataToProcess);
    const uniqueFields = autoFields.filter((field, index, self) => index === self.findIndex(f => f.name === field.name));
    const limitedFields = uniqueFields.slice(0, 50);
    if (uniqueFields.length > 50) showWarning(`Found ${uniqueFields.length} fields. Adding first 50.`);
    onCustomFieldsChange(limitedFields);
    if (onFieldMappingsAutoCreate) {
      const mappings = fieldPaths.filter(({ field }) => limitedFields.some(f => f.name === field.name)).map(({ field, path }) => createFieldMapping(field.name, path, field.required));
      onFieldMappingsAutoCreate(mappings);
    }
  };

  const handleRemoveField = (index: number) => {
    onCustomFieldsChange(customFields.filter((_, i) => i !== index));
    if (editingField === index) setEditingField(null);
  };

  const handleFieldChange = (index: number, field: keyof CustomField, value: any) => {
    const updated = [...customFields]; updated[index] = { ...updated[index], [field]: value }; onCustomFieldsChange(updated);
  };

  const fieldTypeOptions: FieldType[] = ['string', 'number', 'boolean', 'datetime', 'array', 'object'];
  const typeColors: Record<string, string> = {
    string: colors.success,
    number: colors.primary,
    boolean: colors.warning || '#F59E0B',
    datetime: colors.info || '#0088FF',
    array: colors.alert,
    object: colors.textMuted,
  };

  const inputStyle = (focused?: boolean) => ({
    backgroundColor: colors.background,
    border: `1px solid ${focused ? colors.primary : borderColor}`,
    color: colors.text,
    outline: 'none',
  });

  return (
    <div className="space-y-4">

      {/* ── Schema Type Selector ── */}
      <div>
        <div className="text-[10px] font-bold tracking-wider mb-2" style={{ color: colors.text }}>SCHEMA TYPE</div>
        <div className="grid grid-cols-2 gap-2">
          {[
            { id: 'predefined' as const, label: 'Predefined', sub: 'Built-in schemas (OHLCV, QUOTE, ORDER...)', icon: Database, iconColor: colors.primary },
            { id: 'custom' as const, label: 'Custom', sub: 'Define your own fields for any data', icon: Edit3, iconColor: colors.info || '#0088FF' },
          ].map(({ id, label, sub, icon: Icon, iconColor }) => {
            const isActive = schemaType === id;
            return (
              <button
                key={id}
                onClick={() => onSchemaTypeChange(id)}
                className="flex items-start gap-3 p-3 text-left transition-all"
                style={{
                  backgroundColor: isActive ? `${iconColor}10` : colors.panel,
                  border: `1px solid ${isActive ? iconColor : borderColor}`,
                }}
                onMouseEnter={(e) => { if (!isActive) (e.currentTarget as HTMLButtonElement).style.borderColor = `${iconColor}50`; }}
                onMouseLeave={(e) => { if (!isActive) (e.currentTarget as HTMLButtonElement).style.borderColor = borderColor; }}
              >
                <div
                  className="w-8 h-8 flex items-center justify-center flex-shrink-0"
                  style={{ backgroundColor: `${iconColor}15`, border: `1px solid ${iconColor}30` }}
                >
                  <Icon size={16} color={iconColor} />
                </div>
                <div className="min-w-0">
                  <div className="flex items-center gap-1.5 mb-0.5">
                    <span className="text-xs font-bold" style={{ color: isActive ? iconColor : colors.text }}>{label}</span>
                    {isActive && <CheckCircle2 size={11} color={iconColor} />}
                  </div>
                  <span className="text-[10px]" style={{ color: colors.textMuted }}>{sub}</span>
                </div>
              </button>
            );
          })}
        </div>
      </div>

      {/* ── Predefined Schema Selection ── */}
      {schemaType === 'predefined' && (
        <div>
          <div className="text-[10px] font-bold tracking-wider mb-2" style={{ color: colors.text }}>SELECT SCHEMA</div>
          <SchemaSelector selectedSchema={selectedSchema} onSelect={onSchemaSelect} />
        </div>
      )}

      {/* ── Custom Schema Builder ── */}
      {schemaType === 'custom' && (
        <div className="space-y-3">
          {/* Info Banner */}
          {sampleData ? (
            <div
              className="flex items-start gap-3 p-3"
              style={{ backgroundColor: `${colors.info || '#0088FF'}08`, border: `1px solid ${colors.info || '#0088FF'}30` }}
            >
              <Sparkles size={14} color={colors.info || '#0088FF'} className="flex-shrink-0 mt-0.5" />
              <div className="text-[11px] space-y-0.5" style={{ color: colors.text }}>
                <div className="font-bold" style={{ color: colors.info || '#0088FF' }}>SMART FIELD EXTRACTION</div>
                <div>Click <span style={{ color: colors.success }} className="font-bold">AUTO-ADD ALL</span> to import all fields from your API response automatically</div>
                <div>Or click individual leaf fields in the JSON tree on the left to add them one by one</div>
              </div>
            </div>
          ) : (
            <div
              className="flex items-start gap-3 p-3"
              style={{ backgroundColor: `${colors.warning || '#F59E0B'}08`, border: `1px solid ${colors.warning || '#F59E0B'}30` }}
            >
              <AlertCircle size={14} color={colors.warning || '#F59E0B'} className="flex-shrink-0 mt-0.5" />
              <div className="text-[11px]" style={{ color: colors.text }}>
                <span className="font-bold" style={{ color: colors.warning || '#F59E0B' }}>NO SAMPLE DATA — </span>
                Go back to Step 1 and fetch sample data to auto-detect fields
              </div>
            </div>
          )}

          {/* Two-column layout */}
          <div className="grid grid-cols-2 gap-3">
            {/* LEFT: JSON Explorer */}
            <div>
              <div className="flex items-center justify-between mb-2">
                <div className="flex items-center gap-2">
                  <Layers size={12} color={colors.success} />
                  <span className="text-[11px] font-bold tracking-wider" style={{ color: colors.success }}>API RESPONSE</span>
                </div>
                {sampleData && (
                  <button
                    onClick={handleAutoAddAllFields}
                    className="flex items-center gap-1 px-2 py-1 text-[10px] font-bold tracking-wide transition-all"
                    style={{ backgroundColor: colors.success, color: '#000' }}
                    onMouseEnter={(e) => e.currentTarget.style.opacity = '0.85'}
                    onMouseLeave={(e) => e.currentTarget.style.opacity = '1'}
                    title="Auto-add all primitive fields"
                  >
                    <Sparkles size={11} />
                    AUTO-ADD ALL
                  </button>
                )}
              </div>

              {sampleData ? (
                <div style={{ border: `1px solid ${borderColor}`, backgroundColor: colors.background }}>
                  <JSONExplorer data={sampleData} onFieldSelect={handleFieldSelectFromJson} selectedPath={selectedJsonPath} />
                </div>
              ) : (
                <div
                  className="flex items-center justify-center p-8"
                  style={{ border: `1px solid ${borderColor}`, backgroundColor: colors.background }}
                >
                  <div className="text-center">
                    <AlertCircle size={24} color={colors.textMuted} className="mx-auto mb-2 opacity-40" />
                    <p className="text-xs font-bold tracking-wider" style={{ color: colors.textMuted }}>NO SAMPLE DATA</p>
                    <p className="text-[10px] mt-1" style={{ color: colors.textMuted }}>Fetch data in Step 1</p>
                  </div>
                </div>
              )}
            </div>

            {/* RIGHT: Schema Fields */}
            <div>
              <div className="flex items-center justify-between mb-2">
                <div className="flex items-center gap-2">
                  <Edit3 size={12} color={colors.primary} />
                  <span className="text-[11px] font-bold tracking-wider" style={{ color: colors.primary }}>
                    SCHEMA FIELDS
                  </span>
                  <span
                    className="text-[10px] px-1.5 py-0.5 font-bold font-mono"
                    style={{ backgroundColor: `${colors.primary}20`, color: colors.primary }}
                  >
                    {customFields.length}
                  </span>
                </div>
                <button
                  onClick={handleAddField}
                  className="flex items-center gap-1 px-2 py-1 text-[10px] font-bold tracking-wide transition-all"
                  style={{ backgroundColor: `${colors.primary}15`, color: colors.primary, border: `1px solid ${colors.primary}40` }}
                  onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = `${colors.primary}25`; }}
                  onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = `${colors.primary}15`; }}
                >
                  <Plus size={11} />
                  ADD
                </button>
              </div>

              <div
                className="max-h-96 overflow-y-auto"
                style={{ border: `1px solid ${borderColor}`, backgroundColor: colors.background }}
              >
                {customFields.length === 0 ? (
                  <div className="flex items-center justify-center p-8">
                    <div className="text-center">
                      <Edit3 size={24} color={colors.textMuted} className="mx-auto mb-2 opacity-40" />
                      <p className="text-xs font-bold tracking-wider" style={{ color: colors.textMuted }}>NO FIELDS YET</p>
                      <p className="text-[10px] mt-1" style={{ color: colors.textMuted }}>
                        {sampleData ? 'Click fields in the JSON tree to add them' : 'Add fields manually'}
                      </p>
                    </div>
                  </div>
                ) : (
                  <div className="p-2 space-y-1.5">
                    {customFields.map((field, index) => {
                      const isEditing = editingField === index;
                      return (
                        <div
                          key={index}
                          className="transition-all"
                          style={{
                            backgroundColor: isEditing ? `${colors.primary}08` : colors.panel,
                            border: `1px solid ${isEditing ? colors.primary : borderColor}`,
                          }}
                        >
                          <div className="p-2 space-y-1.5">
                            {/* Row: Name + Type */}
                            <div className="grid grid-cols-[1fr_auto] gap-1.5">
                              <input
                                type="text"
                                value={field.name}
                                onChange={(e) => handleFieldChange(index, 'name', e.target.value)}
                                onFocus={() => setEditingField(index)}
                                placeholder="field_name"
                                className="px-2 py-1 text-xs font-mono w-full transition-all"
                                style={inputStyle(isEditing)}
                              />
                              <select
                                value={field.type}
                                onChange={(e) => handleFieldChange(index, 'type', e.target.value as FieldType)}
                                className="px-2 py-1 text-[10px] font-mono font-bold transition-all"
                                style={{
                                  backgroundColor: `${typeColors[field.type] || colors.textMuted}15`,
                                  border: `1px solid ${typeColors[field.type] || colors.textMuted}40`,
                                  color: typeColors[field.type] || colors.textMuted,
                                  outline: 'none',
                                }}
                              >
                                {fieldTypeOptions.map((t) => (
                                  <option key={t} value={t} style={{ backgroundColor: colors.panel, color: colors.text }}>{t}</option>
                                ))}
                              </select>
                            </div>

                            {/* Description (only when editing) */}
                            {isEditing && (
                              <input
                                type="text"
                                value={field.description || ''}
                                onChange={(e) => handleFieldChange(index, 'description', e.target.value)}
                                placeholder="Description (optional)"
                                className="w-full px-2 py-1 text-[11px] transition-all"
                                style={inputStyle()}
                              />
                            )}

                            {/* Footer row */}
                            <div className="flex items-center justify-between">
                              <label className="flex items-center gap-1.5 cursor-pointer">
                                <div
                                  className="w-3 h-3 flex items-center justify-center transition-all"
                                  style={{
                                    backgroundColor: field.required ? colors.alert : 'transparent',
                                    border: `1px solid ${field.required ? colors.alert : borderColor}`,
                                  }}
                                >
                                  {field.required && <CheckCircle2 size={10} color="#000" />}
                                </div>
                                <input
                                  type="checkbox"
                                  checked={field.required}
                                  onChange={(e) => handleFieldChange(index, 'required', e.target.checked)}
                                  className="sr-only"
                                />
                                <span className="text-[10px]" style={{ color: field.required ? colors.alert : colors.textMuted }}>Required</span>
                              </label>
                              <button
                                onClick={() => handleRemoveField(index)}
                                className="p-1 transition-all"
                                style={{ color: colors.textMuted }}
                                onMouseEnter={(e) => { e.currentTarget.style.color = colors.alert; e.currentTarget.style.backgroundColor = `${colors.alert}10`; }}
                                onMouseLeave={(e) => { e.currentTarget.style.color = colors.textMuted; e.currentTarget.style.backgroundColor = 'transparent'; }}
                                title="Remove field"
                              >
                                <Trash2 size={12} />
                              </button>
                            </div>
                          </div>
                        </div>
                      );
                    })}
                  </div>
                )}
              </div>
            </div>
          </div>

          {/* Fields count banner */}
          {customFields.length > 0 && (
            <div
              className="flex items-center gap-2 px-3 py-2"
              style={{ backgroundColor: `${colors.success}08`, border: `1px solid ${colors.success}30` }}
            >
              <CheckCircle2 size={13} color={colors.success} />
              <span className="text-[11px] font-bold" style={{ color: colors.success }}>
                {customFields.length} field{customFields.length !== 1 ? 's' : ''} added
              </span>
              <span className="text-[11px]" style={{ color: colors.textMuted }}>
                — field mappings auto-created. Review them in Step 3.
              </span>
            </div>
          )}
        </div>
      )}
    </div>
  );
}
