/**
 * Data Mapping Tab - TERMINAL UI/UX (AI Quant Lab Style)
 * Professional terminal-grade API integration & schema transformation platform
 * 3-panel layout: left nav | center content | right status
 */

import React, { useState, useEffect, useReducer } from 'react';
import {
  Database,
  Plus,
  List,
  Bookmark,
  ArrowLeft,
  ArrowRight,
  CheckCircle2,
  Save,
  Play,
  Trash2,
  Edit3,
  RefreshCw,
  ChevronDown,
  ChevronUp,
  Activity,
  Link,
  Layers,
  GitMerge,
  Zap,
  Server,
  Shield,
  AlertCircle,
  FileJson,
  Network,
} from 'lucide-react';
import { showConfirm, showSuccess, showError, showWarning } from '@/utils/notifications';
import {
  DataMappingConfig,
  APIConfig,
  CreateStep,
  UnifiedSchemaType,
  CustomField,
  FieldMapping,
  MappingExecutionResult,
  ParserEngine,
  MappingTemplate,
} from './types';
import { APIConfigurationPanel } from './components/APIConfigurationPanel';
import { SchemaBuilder } from './components/SchemaBuilder';
import { VisualFieldMapper } from './components/VisualFieldMapper';
import { CacheSettings } from './components/CacheSettings';
import { LivePreview } from './components/LivePreview';
import { TemplateLibrary } from './components/TemplateLibrary';
import { mappingEngine } from './engine/MappingEngine';
import { mappingDatabase } from './services/MappingDatabase';
import { v4 as uuidv4 } from 'uuid';
import { useTerminalTheme } from '@/contexts/ThemeContext';

type View = 'list' | 'create' | 'templates';

// State management
interface DataMappingState {
  view: View;
  currentStep: CreateStep;
  savedMappings: DataMappingConfig[];
  isLoadingMappings: boolean;
  mappingId: string;
  mappingName: string;
  mappingDescription: string;
  apiConfig: APIConfig;
  schemaType: 'predefined' | 'custom';
  selectedSchema?: UnifiedSchemaType;
  customFields: CustomField[];
  sampleData: any;
  fieldMappings: FieldMapping[];
  testResult: MappingExecutionResult | null;
  isTesting: boolean;
  isLeftPanelMinimized: boolean;
  isRightPanelMinimized: boolean;
  showStepDropdown: boolean;
}

type DataMappingAction =
  | { type: 'SET_VIEW'; payload: View }
  | { type: 'SET_STEP'; payload: CreateStep }
  | { type: 'SET_MAPPINGS'; payload: DataMappingConfig[] }
  | { type: 'SET_LOADING'; payload: boolean }
  | { type: 'SET_TESTING'; payload: boolean }
  | { type: 'SET_SAMPLE_DATA'; payload: any }
  | { type: 'SET_FIELD_MAPPINGS'; payload: FieldMapping[] }
  | { type: 'SET_TEST_RESULT'; payload: MappingExecutionResult | null }
  | { type: 'SET_API_CONFIG'; payload: APIConfig }
  | { type: 'SET_SCHEMA_TYPE'; payload: 'predefined' | 'custom' }
  | { type: 'SET_SELECTED_SCHEMA'; payload: UnifiedSchemaType | undefined }
  | { type: 'SET_CUSTOM_FIELDS'; payload: CustomField[] }
  | { type: 'SET_MAPPING_META'; payload: { id: string; name: string; description: string } }
  | { type: 'TOGGLE_LEFT_PANEL' }
  | { type: 'TOGGLE_RIGHT_PANEL' }
  | { type: 'TOGGLE_STEP_DROPDOWN' }
  | { type: 'CLOSE_STEP_DROPDOWN' }
  | { type: 'RESET_CREATE' };

const defaultApiConfig: APIConfig = {
  id: '',
  name: '',
  description: '',
  baseUrl: '',
  endpoint: '',
  method: 'GET',
  authentication: { type: 'none' },
  headers: {},
  queryParams: {},
  timeout: 30000,
  cacheEnabled: true,
  cacheTTL: 300,
};

const initialState: DataMappingState = {
  view: 'list',
  currentStep: 'api-config',
  savedMappings: [],
  isLoadingMappings: false,
  mappingId: '',
  mappingName: '',
  mappingDescription: '',
  apiConfig: { ...defaultApiConfig, id: uuidv4() },
  schemaType: 'predefined',
  selectedSchema: undefined,
  customFields: [],
  sampleData: null,
  fieldMappings: [],
  testResult: null,
  isTesting: false,
  isLeftPanelMinimized: false,
  isRightPanelMinimized: false,
  showStepDropdown: false,
};

function dataMappingReducer(state: DataMappingState, action: DataMappingAction): DataMappingState {
  switch (action.type) {
    case 'SET_VIEW': return { ...state, view: action.payload, showStepDropdown: false };
    case 'SET_STEP': return { ...state, currentStep: action.payload, showStepDropdown: false };
    case 'SET_MAPPINGS': return { ...state, savedMappings: action.payload };
    case 'SET_LOADING': return { ...state, isLoadingMappings: action.payload };
    case 'SET_TESTING': return { ...state, isTesting: action.payload };
    case 'SET_SAMPLE_DATA': return { ...state, sampleData: action.payload };
    case 'SET_FIELD_MAPPINGS': return { ...state, fieldMappings: action.payload };
    case 'SET_TEST_RESULT': return { ...state, testResult: action.payload };
    case 'SET_API_CONFIG': return { ...state, apiConfig: action.payload };
    case 'SET_SCHEMA_TYPE': return { ...state, schemaType: action.payload };
    case 'SET_SELECTED_SCHEMA': return { ...state, selectedSchema: action.payload };
    case 'SET_CUSTOM_FIELDS': return { ...state, customFields: action.payload };
    case 'SET_MAPPING_META': return {
      ...state,
      mappingId: action.payload.id,
      mappingName: action.payload.name,
      mappingDescription: action.payload.description,
    };
    case 'TOGGLE_LEFT_PANEL': return { ...state, isLeftPanelMinimized: !state.isLeftPanelMinimized };
    case 'TOGGLE_RIGHT_PANEL': return { ...state, isRightPanelMinimized: !state.isRightPanelMinimized };
    case 'TOGGLE_STEP_DROPDOWN': return { ...state, showStepDropdown: !state.showStepDropdown };
    case 'CLOSE_STEP_DROPDOWN': return { ...state, showStepDropdown: false };
    case 'RESET_CREATE':
      return {
        ...state,
        mappingId: uuidv4(),
        mappingName: '',
        mappingDescription: '',
        apiConfig: { ...defaultApiConfig, id: uuidv4() },
        schemaType: 'predefined',
        selectedSchema: undefined,
        customFields: [],
        sampleData: null,
        fieldMappings: [],
        testResult: null,
        currentStep: 'api-config',
        view: 'create',
      };
    default: return state;
  }
}

const steps: CreateStep[] = ['api-config', 'schema-select', 'field-mapping', 'cache-settings', 'test-save'];

const stepMeta: Record<CreateStep, { label: string; shortLabel: string; icon: React.ComponentType<{ size?: number; color?: string }> }> = {
  'api-config':    { label: 'API Config',    shortLabel: 'API',     icon: Network },
  'schema-select': { label: 'Schema',        shortLabel: 'SCHEMA',  icon: Layers },
  'field-mapping': { label: 'Field Mapping', shortLabel: 'MAPPING', icon: GitMerge },
  'cache-settings':{ label: 'Cache',         shortLabel: 'CACHE',   icon: Zap },
  'test-save':     { label: 'Test & Save',   shortLabel: 'TEST',    icon: Play },
};

export default function DataMappingTab() {
  const { colors, fontFamily } = useTerminalTheme();
  const [state, dispatch] = useReducer(dataMappingReducer, initialState);
  const [currentTime, setCurrentTime] = useState(new Date());

  const stepIndex = steps.indexOf(state.currentStep);
  const activeStep = stepMeta[state.currentStep];

  // Clock
  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  // Close dropdown on outside click
  useEffect(() => {
    if (state.showStepDropdown) {
      const handleClick = () => dispatch({ type: 'CLOSE_STEP_DROPDOWN' });
      document.addEventListener('click', handleClick);
      return () => document.removeEventListener('click', handleClick);
    }
  }, [state.showStepDropdown]);

  useEffect(() => {
    initializeDatabase();
    loadSavedMappings();
  }, []);

  const initializeDatabase = async () => {
    try { await mappingDatabase.initialize(); } catch { /* db init failed, will retry on next use */ }
  };

  const loadSavedMappings = async () => {
    dispatch({ type: 'SET_LOADING', payload: true });
    try {
      const mappings = await mappingDatabase.getAllMappings();
      dispatch({ type: 'SET_MAPPINGS', payload: mappings });
    } catch { /* failed to load mappings, start with empty state */ }
    finally { dispatch({ type: 'SET_LOADING', payload: false }); }
  };

  const handleApiConfigChange = (config: APIConfig) => {
    dispatch({ type: 'SET_API_CONFIG', payload: config });
    if (!state.mappingName || state.mappingName === state.apiConfig.name) {
      dispatch({ type: 'SET_MAPPING_META', payload: { id: state.mappingId, name: config.name, description: config.description } });
    }
  };

  const handleTestMapping = async () => {
    if (!state.sampleData) { showWarning('Please fetch sample data first'); return; }
    if (state.fieldMappings.length === 0) { showWarning('Please configure at least one field mapping'); return; }

    dispatch({ type: 'SET_TESTING', payload: true });
    try {
      const detectDataArray = (obj: any): { rootPath: string; isArray: boolean } => {
        if (Array.isArray(obj)) return { rootPath: '', isArray: true };
        for (const key of ['data', 'results', 'items', 'records', 'response']) {
          if (obj[key] && Array.isArray(obj[key]) && obj[key].length > 0) return { rootPath: key, isArray: true };
        }
        return { rootPath: '', isArray: false };
      };
      const { rootPath, isArray } = detectDataArray(state.sampleData);
      const mappingConfig: DataMappingConfig = {
        id: state.mappingId,
        name: state.mappingName || state.apiConfig.name,
        description: state.mappingDescription || state.apiConfig.description,
        source: { type: 'sample', sampleData: state.sampleData },
        target: {
          schemaType: state.schemaType,
          schema: state.schemaType === 'predefined' ? state.selectedSchema : undefined,
          customFields: state.schemaType === 'custom' ? state.customFields : undefined,
        },
        extraction: { engine: ParserEngine.JSONPATH, rootPath: rootPath ? `$.${rootPath}[*]` : '', isArray },
        fieldMappings: state.fieldMappings,
        postProcessing: undefined,
        validation: { enabled: true, strictMode: false, rules: [] },
        metadata: { createdAt: new Date().toISOString(), updatedAt: new Date().toISOString(), version: 1, tags: [], aiGenerated: false },
      };
      const result = await mappingEngine.test(mappingConfig, state.sampleData);
      dispatch({ type: 'SET_TEST_RESULT', payload: result });
      if (!result.success) showError('Test failed', [{ label: 'ERRORS', value: result.errors?.join(', ') || 'Unknown error' }]);
    } catch (error) {
      dispatch({ type: 'SET_TEST_RESULT', payload: {
        success: false,
        errors: [error instanceof Error ? error.message : String(error)],
        metadata: { executedAt: new Date().toISOString(), duration: 0, recordsProcessed: 0, recordsReturned: 0 },
      }});
    } finally {
      dispatch({ type: 'SET_TESTING', payload: false });
    }
  };

  const handleSaveMapping = async () => {
    if (!state.testResult?.success) { showWarning('Please test the mapping successfully before saving'); return; }
    if (!state.mappingName) { showWarning('Please provide a mapping name'); return; }
    try {
      const detectDataArray = (obj: any): { rootPath: string; isArray: boolean } => {
        if (Array.isArray(obj)) return { rootPath: '', isArray: true };
        for (const key of ['data', 'results', 'items', 'records', 'response']) {
          if (obj[key] && Array.isArray(obj[key]) && obj[key].length > 0) return { rootPath: key, isArray: true };
        }
        return { rootPath: '', isArray: false };
      };
      const { rootPath, isArray } = detectDataArray(state.sampleData);
      const mappingConfig: DataMappingConfig = {
        id: state.mappingId,
        name: state.mappingName,
        description: state.mappingDescription,
        source: { type: 'api', apiConfig: state.apiConfig },
        target: {
          schemaType: state.schemaType,
          schema: state.schemaType === 'predefined' ? state.selectedSchema : undefined,
          customFields: state.schemaType === 'custom' ? state.customFields : undefined,
        },
        extraction: { engine: ParserEngine.JSONPATH, rootPath: rootPath ? `$.${rootPath}[*]` : '', isArray },
        fieldMappings: state.fieldMappings,
        postProcessing: undefined,
        validation: { enabled: true, strictMode: false, rules: [] },
        metadata: { createdAt: new Date().toISOString(), updatedAt: new Date().toISOString(), version: 1, tags: [], aiGenerated: false },
      };
      await mappingDatabase.saveMapping(mappingConfig);
      showSuccess('Mapping saved successfully!');
      await loadSavedMappings();
      dispatch({ type: 'SET_VIEW', payload: 'list' });
    } catch (error) {
      showError('Failed to save mapping', [{ label: 'ERROR', value: error instanceof Error ? error.message : String(error) }]);
    }
  };

  const handleDeleteMapping = async (id: string) => {
    const confirmed = await showConfirm('This action cannot be undone.', { title: 'Delete this mapping?', type: 'danger' });
    if (!confirmed) return;
    try {
      await mappingDatabase.deleteMapping(id);
      await loadSavedMappings();
      showSuccess('Mapping deleted successfully');
    } catch { showError('Failed to delete mapping'); }
  };

  const handleLoadMapping = (mapping: DataMappingConfig) => {
    dispatch({ type: 'SET_MAPPING_META', payload: { id: mapping.id, name: mapping.name, description: mapping.description } });
    if (mapping.source.type === 'api' && mapping.source.apiConfig) {
      dispatch({ type: 'SET_API_CONFIG', payload: mapping.source.apiConfig });
    }
    dispatch({ type: 'SET_SCHEMA_TYPE', payload: mapping.target.schemaType });
    if (mapping.target.schemaType === 'predefined') {
      dispatch({ type: 'SET_SELECTED_SCHEMA', payload: mapping.target.schema as UnifiedSchemaType | undefined });
    } else {
      dispatch({ type: 'SET_CUSTOM_FIELDS', payload: mapping.target.customFields || [] });
    }
    dispatch({ type: 'SET_FIELD_MAPPINGS', payload: mapping.fieldMappings });
    dispatch({ type: 'SET_STEP', payload: 'api-config' });
    dispatch({ type: 'SET_VIEW', payload: 'create' });
  };

  const handleSelectTemplate = (template: MappingTemplate) => {
    const newId = uuidv4();
    dispatch({ type: 'SET_MAPPING_META', payload: {
      id: newId,
      name: template.mappingConfig.name || template.name,
      description: template.mappingConfig.description || template.description,
    }});
    if (template.mappingConfig.source?.apiConfig) dispatch({ type: 'SET_API_CONFIG', payload: template.mappingConfig.source.apiConfig });
    if (template.mappingConfig.target) {
      dispatch({ type: 'SET_SCHEMA_TYPE', payload: template.mappingConfig.target.schemaType });
      if (template.mappingConfig.target.schemaType === 'predefined') {
        dispatch({ type: 'SET_SELECTED_SCHEMA', payload: template.mappingConfig.target.schema as UnifiedSchemaType });
      } else {
        dispatch({ type: 'SET_CUSTOM_FIELDS', payload: template.mappingConfig.target.customFields || [] });
      }
    }
    if (template.mappingConfig.fieldMappings) dispatch({ type: 'SET_FIELD_MAPPINGS', payload: template.mappingConfig.fieldMappings });
    dispatch({ type: 'SET_STEP', payload: 'api-config' });
    dispatch({ type: 'SET_VIEW', payload: 'create' });
  };

  const canProceedToNextStep = () => {
    switch (state.currentStep) {
      case 'api-config': return !!(state.apiConfig.baseUrl && state.apiConfig.endpoint && state.apiConfig.name && state.sampleData !== null);
      case 'schema-select': return (state.schemaType === 'predefined' && !!state.selectedSchema) || (state.schemaType === 'custom' && state.customFields.length > 0);
      case 'field-mapping': return state.fieldMappings.length > 0;
      case 'cache-settings': return true;
      case 'test-save': return state.testResult?.success === true;
      default: return false;
    }
  };

  const getStepStatus = (step: CreateStep): 'complete' | 'current' | 'pending' => {
    const targetIndex = steps.indexOf(step);
    if (targetIndex < stepIndex) return 'complete';
    if (targetIndex === stepIndex) return 'current';
    return 'pending';
  };

  const mappedCount = state.fieldMappings.length;
  const totalMappings = state.savedMappings.length;

  const renderStepContent = () => {
    switch (state.currentStep) {
      case 'api-config':
        return (
          <div className="h-full overflow-auto p-4">
            <APIConfigurationPanel
              config={state.apiConfig}
              onChange={handleApiConfigChange}
              onTestSuccess={(data) => dispatch({ type: 'SET_SAMPLE_DATA', payload: data })}
            />
          </div>
        );
      case 'schema-select':
        return (
          <div className="h-full overflow-auto p-4">
            <SchemaBuilder
              schemaType={state.schemaType}
              selectedSchema={state.selectedSchema}
              customFields={state.customFields}
              onSchemaTypeChange={(t) => dispatch({ type: 'SET_SCHEMA_TYPE', payload: t })}
              onSchemaSelect={(schema) => dispatch({ type: 'SET_SELECTED_SCHEMA', payload: schema as UnifiedSchemaType })}
              onCustomFieldsChange={(fields) => dispatch({ type: 'SET_CUSTOM_FIELDS', payload: fields })}
              sampleData={state.sampleData}
              onFieldMappingsAutoCreate={(newMappings) => {
                const existingFields = new Set(state.fieldMappings.map(m => m.targetField));
                const unique = newMappings.filter(m => !existingFields.has(m.targetField));
                if (unique.length > 0) dispatch({ type: 'SET_FIELD_MAPPINGS', payload: [...state.fieldMappings, ...unique] });
              }}
            />
          </div>
        );
      case 'field-mapping':
        if (!state.sampleData) {
          return (
            <div className="flex items-center justify-center h-full">
              <div className="text-center">
                <Database size={48} color={colors.textMuted} className="mx-auto mb-3" />
                <p className="text-xs font-bold tracking-wider" style={{ color: colors.textMuted }}>NO SAMPLE DATA</p>
                <p className="text-xs mt-1" style={{ color: colors.textMuted }}>Return to API Config and fetch sample data first</p>
              </div>
            </div>
          );
        }
        return (
          <div className="h-full p-4">
            <VisualFieldMapper
              schemaType={state.schemaType}
              selectedSchema={state.selectedSchema}
              customFields={state.customFields}
              sampleData={state.sampleData}
              mappings={state.fieldMappings}
              onMappingsChange={(mappings) => dispatch({ type: 'SET_FIELD_MAPPINGS', payload: mappings })}
            />
          </div>
        );
      case 'cache-settings':
        return (
          <div className="h-full overflow-auto p-4">
            <CacheSettings
              cacheEnabled={state.apiConfig.cacheEnabled}
              cacheTTL={state.apiConfig.cacheTTL}
              onCacheEnabledChange={(enabled) => dispatch({ type: 'SET_API_CONFIG', payload: { ...state.apiConfig, cacheEnabled: enabled } })}
              onCacheTTLChange={(ttl) => dispatch({ type: 'SET_API_CONFIG', payload: { ...state.apiConfig, cacheTTL: ttl } })}
              mappingId={state.mappingId}
            />
          </div>
        );
      case 'test-save':
        return (
          <div className="h-full overflow-auto p-4 space-y-4">
            {/* Test Header */}
            <div
              className="flex items-center justify-between p-3"
              style={{ backgroundColor: colors.panel, border: `1px solid var(--ft-border-color, #2A2A2A)` }}
            >
              <div>
                <div className="flex items-center gap-2">
                  <Play size={14} color={colors.primary} />
                  <span className="text-xs font-bold tracking-wider" style={{ color: colors.primary }}>TEST & VALIDATION</span>
                </div>
                <p className="text-[11px] mt-0.5" style={{ color: colors.textMuted }}>Run test to verify mapping configuration</p>
              </div>
              <button
                onClick={handleTestMapping}
                disabled={state.isTesting}
                className="flex items-center gap-2 px-4 py-2 text-xs font-bold tracking-wide transition-all"
                style={{
                  backgroundColor: state.isTesting ? colors.panel : colors.primary,
                  color: state.isTesting ? colors.textMuted : colors.background,
                  border: `1px solid ${state.isTesting ? 'var(--ft-border-color, #2A2A2A)' : colors.primary}`,
                  cursor: state.isTesting ? 'not-allowed' : 'pointer',
                }}
                onMouseEnter={(e) => { if (!state.isTesting) e.currentTarget.style.backgroundColor = '#FF8C00'; }}
                onMouseLeave={(e) => { if (!state.isTesting) e.currentTarget.style.backgroundColor = state.isTesting ? colors.panel : colors.primary; }}
              >
                {state.isTesting ? <RefreshCw size={14} className="animate-spin" /> : <Play size={14} />}
                {state.isTesting ? 'TESTING...' : 'RUN TEST'}
              </button>
            </div>

            <LivePreview result={state.testResult || undefined} isLoading={state.isTesting} />

            {state.testResult?.success && (
              <div className="pt-3 border-t" style={{ borderColor: 'var(--ft-border-color, #2A2A2A)' }}>
                <button
                  onClick={handleSaveMapping}
                  className="w-full flex items-center justify-center gap-2 py-3 text-sm font-bold tracking-wide transition-all"
                  style={{
                    backgroundColor: colors.success,
                    color: '#000',
                    border: `1px solid ${colors.success}`,
                  }}
                  onMouseEnter={(e) => e.currentTarget.style.opacity = '0.9'}
                  onMouseLeave={(e) => e.currentTarget.style.opacity = '1'}
                >
                  <Save size={16} />
                  SAVE MAPPING CONFIGURATION
                </button>
              </div>
            )}
          </div>
        );
      default:
        return null;
    }
  };

  return (
    <div className="flex flex-col h-full" style={{ backgroundColor: colors.background, fontFamily }}>

      {/* ===== TOP NAVIGATION BAR ===== */}
      <div
        className="flex items-center justify-between px-3 py-2.5 border-b flex-shrink-0"
        style={{ backgroundColor: colors.panel, borderColor: 'var(--ft-border-color, #2A2A2A)', height: '48px' }}
      >
        {/* Left: Branding + View Selector */}
        <div className="flex items-center gap-3">
          {/* Brand */}
          <div className="flex items-center gap-2 px-3 py-1.5" style={{ backgroundColor: colors.primary }}>
            <Database size={14} color={colors.background} />
            <span className="font-bold text-xs tracking-wider" style={{ color: colors.background }}>DATA MAPPING</span>
          </div>

          <div className="h-5 w-px" style={{ backgroundColor: 'var(--ft-border-color, #2A2A2A)' }} />

          {/* View Selector */}
          <div className="flex items-center gap-1">
            {([
              { id: 'list' as View, label: 'MAPPINGS', icon: List },
              { id: 'templates' as View, label: 'TEMPLATES', icon: Bookmark },
              { id: 'create' as View, label: 'CREATE', icon: Plus },
            ] as const).map(({ id, label, icon: Icon }) => (
              <button
                key={id}
                onClick={() => {
                  if (id === 'create') dispatch({ type: 'RESET_CREATE' });
                  else dispatch({ type: 'SET_VIEW', payload: id });
                }}
                className="flex items-center gap-1.5 px-3 py-1.5 text-xs font-semibold tracking-wide transition-colors"
                style={{
                  backgroundColor: state.view === id ? `${colors.primary}20` : 'transparent',
                  color: state.view === id ? colors.primary : colors.textMuted,
                  borderLeft: state.view === id ? `2px solid ${colors.primary}` : '2px solid transparent',
                }}
                onMouseEnter={(e) => { if (state.view !== id) e.currentTarget.style.backgroundColor = '#1F1F1F'; }}
                onMouseLeave={(e) => { if (state.view !== id) e.currentTarget.style.backgroundColor = 'transparent'; }}
              >
                <Icon size={13} />
                <span>{label}</span>
              </button>
            ))}
          </div>

          {/* Step Selector Dropdown (create mode only) */}
          {state.view === 'create' && (
            <>
              <div className="h-5 w-px" style={{ backgroundColor: 'var(--ft-border-color, #2A2A2A)' }} />
              <div className="relative">
                <button
                  onClick={(e) => { e.stopPropagation(); dispatch({ type: 'TOGGLE_STEP_DROPDOWN' }); }}
                  className="flex items-center gap-2 px-3 py-1.5 text-xs font-semibold tracking-wide transition-colors"
                  style={{
                    backgroundColor: colors.panel,
                    color: colors.primary,
                    border: `1px solid var(--ft-border-color, #2A2A2A)`,
                  }}
                  onMouseEnter={(e) => e.currentTarget.style.backgroundColor = '#1F1F1F'}
                  onMouseLeave={(e) => e.currentTarget.style.backgroundColor = colors.panel}
                >
                  <activeStep.icon size={13} />
                  <span>{activeStep.shortLabel}</span>
                  <ChevronDown size={11} />
                </button>

                {state.showStepDropdown && (
                  <div
                    className="absolute top-full left-0 mt-1 z-50"
                    style={{
                      backgroundColor: colors.panel,
                      border: `1px solid var(--ft-border-color, #2A2A2A)`,
                      minWidth: '200px',
                      boxShadow: `0 4px 12px ${colors.background}80`,
                    }}
                  >
                    {steps.map((step, idx) => {
                      const meta = stepMeta[step];
                      const status = getStepStatus(step);
                      const StepIcon = meta.icon;
                      return (
                        <button
                          key={step}
                          onClick={() => idx <= stepIndex && dispatch({ type: 'SET_STEP', payload: step })}
                          className="w-full flex items-center gap-3 px-3 py-2 text-xs transition-colors"
                          style={{
                            backgroundColor: step === state.currentStep ? '#1F1F1F' : 'transparent',
                            color: status === 'complete' ? colors.success : step === state.currentStep ? colors.primary : colors.textMuted,
                            borderLeft: step === state.currentStep ? `2px solid ${colors.primary}` : '2px solid transparent',
                            cursor: idx <= stepIndex ? 'pointer' : 'not-allowed',
                            opacity: idx > stepIndex ? 0.4 : 1,
                          }}
                        >
                          {status === 'complete' ? <CheckCircle2 size={13} color={colors.success} /> : <StepIcon size={13} />}
                          <span className="flex-1 text-left">{meta.label}</span>
                          <span className="text-[10px]" style={{ color: colors.textMuted }}>{idx + 1}/{steps.length}</span>
                        </button>
                      );
                    })}
                  </div>
                )}
              </div>
            </>
          )}
        </div>

        {/* Right: Status + Actions */}
        <div className="flex items-center gap-3">
          {/* Clock */}
          <div className="text-xs font-mono px-2" style={{ color: colors.textMuted }}>
            {currentTime.toLocaleTimeString('en-US', { hour12: false })}
          </div>

          <div className="h-5 w-px" style={{ backgroundColor: 'var(--ft-border-color, #2A2A2A)' }} />

          {/* Mapping count badge */}
          <div className="flex items-center gap-1.5 px-2.5 py-1 text-xs font-mono"
            style={{ border: `1px solid var(--ft-border-color, #2A2A2A)`, backgroundColor: `${colors.primary}10` }}>
            <FileJson size={12} color={colors.primary} />
            <span style={{ color: colors.primary }}>{totalMappings} MAPPINGS</span>
          </div>

          {/* DB Status */}
          <div className="flex items-center gap-1.5 px-2.5 py-1 text-xs font-mono"
            style={{ border: `1px solid ${colors.success}`, backgroundColor: `${colors.success}10` }}>
            <Shield size={12} color={colors.success} />
            <span style={{ color: colors.success }}>ENCRYPTED</span>
          </div>

          {/* Reload */}
          <button
            onClick={loadSavedMappings}
            className="p-1.5 transition-colors"
            style={{ backgroundColor: 'transparent' }}
            title="Refresh mappings"
            onMouseEnter={(e) => e.currentTarget.style.backgroundColor = '#1F1F1F'}
            onMouseLeave={(e) => e.currentTarget.style.backgroundColor = 'transparent'}
          >
            <RefreshCw size={14} color={colors.textMuted} className={state.isLoadingMappings ? 'animate-spin' : ''} />
          </button>
        </div>
      </div>

      {/* ===== STEP PROGRESS (create mode) ===== */}
      {state.view === 'create' && (
        <div
          className="flex items-center px-4 flex-shrink-0"
          style={{ backgroundColor: colors.panel, borderBottom: `1px solid var(--ft-border-color, #2A2A2A)`, height: '36px' }}
        >
          {steps.map((step, index) => {
            const meta = stepMeta[step];
            const status = getStepStatus(step);
            const StepIcon = meta.icon;
            return (
              <React.Fragment key={step}>
                <button
                  onClick={() => index <= stepIndex && dispatch({ type: 'SET_STEP', payload: step })}
                  className="flex items-center gap-1.5 h-full px-3 text-[11px] font-bold tracking-wider transition-all relative"
                  style={{
                    color: status === 'complete' ? colors.success : status === 'current' ? colors.primary : colors.textMuted,
                    opacity: index > stepIndex ? 0.4 : 1,
                    cursor: index <= stepIndex ? 'pointer' : 'not-allowed',
                    borderBottom: status === 'current' ? `2px solid ${colors.primary}` : '2px solid transparent',
                  }}
                >
                  {status === 'complete' ? (
                    <CheckCircle2 size={12} color={colors.success} />
                  ) : (
                    <StepIcon size={12} color={status === 'current' ? colors.primary : colors.textMuted} />
                  )}
                  <span>{meta.shortLabel}</span>
                </button>
                {index < steps.length - 1 && (
                  <div className="w-4 h-px" style={{ backgroundColor: 'var(--ft-border-color, #2A2A2A)' }} />
                )}
              </React.Fragment>
            );
          })}
        </div>
      )}

      {/* ===== MAIN LAYOUT ===== */}
      <div className="flex flex-1 overflow-hidden">

        {/* LEFT PANEL: Context Nav */}
        {!state.isLeftPanelMinimized && (
          <div
            className="flex flex-col border-r flex-shrink-0"
            style={{ backgroundColor: colors.panel, borderColor: 'var(--ft-border-color, #2A2A2A)', width: '200px' }}
          >
            <div className="flex items-center justify-between px-3 py-2 border-b" style={{ borderColor: 'var(--ft-border-color, #2A2A2A)' }}>
              <span className="text-[11px] font-bold tracking-wider" style={{ color: colors.textMuted }}>
                {state.view === 'create' ? 'WIZARD STEPS' : 'NAVIGATION'}
              </span>
              <button onClick={() => dispatch({ type: 'TOGGLE_LEFT_PANEL' })} className="p-0.5">
                <ChevronUp size={12} color={colors.textMuted} />
              </button>
            </div>

            <div className="flex-1 overflow-y-auto p-2">
              {state.view === 'create' ? (
                // Step navigation in create mode
                <div className="space-y-1">
                  <div className="px-2 py-1 text-[10px] font-bold tracking-wider" style={{ color: colors.textMuted }}>STEPS</div>
                  {steps.map((step, idx) => {
                    const meta = stepMeta[step];
                    const status = getStepStatus(step);
                    const StepIcon = meta.icon;
                    const isActive = step === state.currentStep;
                    return (
                      <button
                        key={step}
                        onClick={() => idx <= stepIndex && dispatch({ type: 'SET_STEP', payload: step })}
                        className="w-full flex items-center gap-2 px-2 py-1.5 text-xs transition-all"
                        style={{
                          backgroundColor: isActive ? `${colors.primary}20` : 'transparent',
                          color: status === 'complete' ? colors.success : isActive ? colors.primary : colors.textMuted,
                          borderLeft: isActive ? `2px solid ${colors.primary}` : '2px solid transparent',
                          opacity: idx > stepIndex ? 0.4 : 1,
                          cursor: idx <= stepIndex ? 'pointer' : 'not-allowed',
                        }}
                        onMouseEnter={(e) => { if (!isActive && idx <= stepIndex) e.currentTarget.style.backgroundColor = '#1F1F1F'; }}
                        onMouseLeave={(e) => { if (!isActive) e.currentTarget.style.backgroundColor = 'transparent'; }}
                      >
                        {status === 'complete' ? <CheckCircle2 size={13} color={colors.success} /> : <StepIcon size={13} />}
                        <span className="flex-1 text-left truncate">{meta.label}</span>
                        {status === 'complete' && <CheckCircle2 size={11} color={colors.success} />}
                      </button>
                    );
                  })}
                </div>
              ) : (
                // View navigation
                <div className="space-y-1">
                  <div className="px-2 py-1 text-[10px] font-bold tracking-wider" style={{ color: colors.textMuted }}>VIEWS</div>
                  {([
                    { id: 'list' as View, label: 'My Mappings', icon: List },
                    { id: 'templates' as View, label: 'Templates', icon: Bookmark },
                    { id: 'create' as View, label: 'New Mapping', icon: Plus },
                  ] as const).map(({ id, label, icon: Icon }) => (
                    <button
                      key={id}
                      onClick={() => {
                        if (id === 'create') dispatch({ type: 'RESET_CREATE' });
                        else dispatch({ type: 'SET_VIEW', payload: id });
                      }}
                      className="w-full flex items-center gap-2 px-2 py-1.5 text-xs transition-all"
                      style={{
                        backgroundColor: state.view === id ? `${colors.primary}20` : 'transparent',
                        color: state.view === id ? colors.primary : colors.textMuted,
                        borderLeft: state.view === id ? `2px solid ${colors.primary}` : '2px solid transparent',
                      }}
                      onMouseEnter={(e) => { if (state.view !== id) e.currentTarget.style.backgroundColor = '#1F1F1F'; }}
                      onMouseLeave={(e) => { if (state.view !== id) e.currentTarget.style.backgroundColor = 'transparent'; }}
                    >
                      <Icon size={13} />
                      <span className="flex-1 text-left">{label}</span>
                      {id === 'list' && (
                        <span className="text-[10px] px-1.5 py-0.5 font-bold" style={{ backgroundColor: `${colors.primary}20`, color: colors.primary }}>
                          {totalMappings}
                        </span>
                      )}
                    </button>
                  ))}
                </div>
              )}

              {/* Quick Stats */}
              <div className="mt-4 pt-3 border-t" style={{ borderColor: 'var(--ft-border-color, #2A2A2A)' }}>
                <div className="px-2 py-1 text-[10px] font-bold tracking-wider mb-1" style={{ color: colors.textMuted }}>STATS</div>
                <div className="space-y-1.5 px-2">
                  {[
                    { label: 'Saved', value: String(totalMappings), color: colors.primary },
                    { label: 'Fields', value: state.view === 'create' ? String(mappedCount) : '—', color: colors.success },
                    { label: 'Schema', value: state.view === 'create' ? (state.selectedSchema?.toUpperCase() || 'CUSTOM') : '—', color: colors.info || '#0088FF' },
                  ].map(({ label, value, color }) => (
                    <div key={label} className="flex justify-between text-[10px]">
                      <span style={{ color: colors.textMuted }}>{label}:</span>
                      <span style={{ color }} className="font-bold">{value}</span>
                    </div>
                  ))}
                </div>
              </div>
            </div>
          </div>
        )}

        {/* Left panel collapsed */}
        {state.isLeftPanelMinimized && (
          <div className="flex flex-col items-center border-r flex-shrink-0"
            style={{ backgroundColor: colors.panel, borderColor: 'var(--ft-border-color, #2A2A2A)', width: '32px' }}>
            <button onClick={() => dispatch({ type: 'TOGGLE_LEFT_PANEL' })} className="p-1 mt-1">
              <ChevronDown size={12} color={colors.textMuted} />
            </button>
          </div>
        )}

        {/* CENTER: Main Content */}
        <div className="flex-1 flex flex-col overflow-hidden">
          {/* Content sub-header */}
          <div
            className="flex items-center justify-between px-3 py-2 border-b flex-shrink-0"
            style={{ backgroundColor: colors.panel, borderColor: 'var(--ft-border-color, #2A2A2A)' }}
          >
            <div className="flex items-center gap-2">
              {state.view === 'create' ? (
                <>
                  <activeStep.icon size={14} color={colors.primary} />
                  <span className="text-xs font-bold tracking-wide" style={{ color: colors.text }}>{activeStep.label}</span>
                  <div className="h-3 w-px mx-1" style={{ backgroundColor: 'var(--ft-border-color, #2A2A2A)' }} />
                  <span className="text-[11px]" style={{ color: colors.textMuted }}>
                    {state.mappingName || 'Untitled Mapping'}
                  </span>
                </>
              ) : state.view === 'list' ? (
                <>
                  <List size={14} color={colors.primary} />
                  <span className="text-xs font-bold tracking-wide" style={{ color: colors.text }}>SAVED MAPPINGS</span>
                </>
              ) : (
                <>
                  <Bookmark size={14} color={colors.primary} />
                  <span className="text-xs font-bold tracking-wide" style={{ color: colors.text }}>TEMPLATE LIBRARY</span>
                </>
              )}
            </div>
            <div className="flex items-center gap-2">
              {state.view === 'create' && (
                <div className="text-[11px] font-mono" style={{ color: colors.textMuted }}>
                  STEP {stepIndex + 1}/{steps.length}
                </div>
              )}
              {state.view === 'list' && (
                <button
                  onClick={() => dispatch({ type: 'RESET_CREATE' })}
                  className="flex items-center gap-1.5 px-3 py-1 text-xs font-bold tracking-wide transition-all"
                  style={{ backgroundColor: colors.primary, color: colors.background }}
                  onMouseEnter={(e) => e.currentTarget.style.backgroundColor = '#FF8C00'}
                  onMouseLeave={(e) => e.currentTarget.style.backgroundColor = colors.primary}
                >
                  <Plus size={12} />
                  NEW
                </button>
              )}
            </div>
          </div>

          {/* Content Area */}
          <div className="flex-1 overflow-hidden" style={{ backgroundColor: colors.background }}>

            {/* LIST VIEW */}
            {state.view === 'list' && (
              <div className="h-full overflow-auto p-4">
                {state.isLoadingMappings ? (
                  <div className="flex items-center justify-center h-64">
                    <div className="text-center">
                      <RefreshCw size={32} color={colors.primary} className="animate-spin mx-auto mb-3" />
                      <p className="text-xs font-bold tracking-wider" style={{ color: colors.textMuted }}>LOADING MAPPINGS...</p>
                    </div>
                  </div>
                ) : state.savedMappings.length === 0 ? (
                  <div className="flex items-center justify-center h-full">
                    <div className="text-center max-w-md">
                      <div
                        className="inline-flex items-center justify-center w-16 h-16 mb-4"
                        style={{ border: `2px solid var(--ft-border-color, #2A2A2A)` }}
                      >
                        <Database size={32} color={colors.textMuted} />
                      </div>
                      <h3 className="text-sm font-bold tracking-wider mb-2" style={{ color: colors.text }}>NO MAPPINGS CONFIGURED</h3>
                      <p className="text-xs mb-6" style={{ color: colors.textMuted }}>
                        Create your first API data mapping to connect external data sources to Fincept Terminal
                      </p>
                      <button
                        onClick={() => dispatch({ type: 'RESET_CREATE' })}
                        className="flex items-center gap-2 px-6 py-3 text-sm font-bold tracking-wide mx-auto transition-all"
                        style={{ backgroundColor: colors.primary, color: colors.background }}
                        onMouseEnter={(e) => e.currentTarget.style.backgroundColor = '#FF8C00'}
                        onMouseLeave={(e) => e.currentTarget.style.backgroundColor = colors.primary}
                      >
                        <Plus size={16} />
                        CREATE FIRST MAPPING
                      </button>
                    </div>
                  </div>
                ) : (
                  <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-3">
                    {state.savedMappings.map((mapping) => (
                      <div
                        key={mapping.id}
                        className="group flex flex-col transition-all"
                        style={{
                          backgroundColor: colors.panel,
                          border: `1px solid var(--ft-border-color, #2A2A2A)`,
                        }}
                        onMouseEnter={(e) => (e.currentTarget as HTMLDivElement).style.borderColor = colors.primary}
                        onMouseLeave={(e) => (e.currentTarget as HTMLDivElement).style.borderColor = 'var(--ft-border-color, #2A2A2A)'}
                      >
                        {/* Card top accent */}
                        <div className="h-0.5 w-full" style={{ backgroundColor: colors.primary }} />

                        <div className="p-3 flex-1">
                          <div className="flex items-start justify-between mb-2">
                            <div className="flex-1 min-w-0">
                              <h3 className="text-xs font-bold truncate" style={{ color: colors.text }}>{mapping.name}</h3>
                              <p className="text-[11px] mt-0.5 line-clamp-2" style={{ color: colors.textMuted }}>{mapping.description}</p>
                            </div>
                            <div
                              className="ml-2 px-2 py-0.5 text-[10px] font-bold tracking-wider flex-shrink-0"
                              style={{ backgroundColor: `${colors.primary}20`, color: colors.primary }}
                            >
                              {mapping.target.schemaType === 'predefined' ? mapping.target.schema?.toUpperCase() : 'CUSTOM'}
                            </div>
                          </div>

                          {/* Stats row */}
                          <div className="flex items-center gap-3 text-[10px] mb-3" style={{ color: colors.textMuted }}>
                            <span>{mapping.fieldMappings?.length || 0} fields</span>
                            <span>•</span>
                            <span>{mapping.source.type?.toUpperCase()}</span>
                            <span>•</span>
                            <span>{new Date(mapping.metadata.createdAt).toLocaleDateString()}</span>
                          </div>

                          {/* Method tag */}
                          {mapping.source.apiConfig && (
                            <div className="flex items-center gap-2 text-[10px] font-mono" style={{ color: colors.textMuted }}>
                              <span
                                className="px-1.5 py-0.5 font-bold"
                                style={{ backgroundColor: `${colors.success}15`, color: colors.success }}
                              >
                                {mapping.source.apiConfig.method}
                              </span>
                              <span className="truncate" style={{ color: colors.textMuted }}>
                                {mapping.source.apiConfig.baseUrl}{mapping.source.apiConfig.endpoint}
                              </span>
                            </div>
                          )}
                        </div>

                        {/* Card Footer Actions */}
                        <div
                          className="flex items-center justify-between px-3 py-2 border-t"
                          style={{ borderColor: 'var(--ft-border-color, #2A2A2A)' }}
                        >
                          <div className="flex items-center gap-1.5">
                            <Activity size={11} color={colors.success} />
                            <span className="text-[10px]" style={{ color: colors.success }}>ACTIVE</span>
                          </div>
                          <div className="flex gap-1 opacity-0 group-hover:opacity-100 transition-opacity">
                            <button
                              onClick={() => handleLoadMapping(mapping)}
                              className="p-1.5 transition-colors"
                              style={{ backgroundColor: 'transparent', color: colors.textMuted }}
                              onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = '#1F1F1F'; e.currentTarget.style.color = colors.primary; }}
                              onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = 'transparent'; e.currentTarget.style.color = colors.textMuted; }}
                              title="Edit"
                            >
                              <Edit3 size={13} />
                            </button>
                            <button
                              onClick={() => handleDeleteMapping(mapping.id)}
                              className="p-1.5 transition-colors"
                              style={{ backgroundColor: 'transparent', color: colors.textMuted }}
                              onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = '#1F1F1F'; e.currentTarget.style.color = colors.alert; }}
                              onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = 'transparent'; e.currentTarget.style.color = colors.textMuted; }}
                              title="Delete"
                            >
                              <Trash2 size={13} />
                            </button>
                          </div>
                        </div>
                      </div>
                    ))}
                  </div>
                )}
              </div>
            )}

            {/* TEMPLATES VIEW */}
            {state.view === 'templates' && (
              <div className="h-full overflow-auto p-4">
                <TemplateLibrary onSelectTemplate={handleSelectTemplate} />
              </div>
            )}

            {/* CREATE VIEW */}
            {state.view === 'create' && (
              <div className="h-full">
                {renderStepContent()}
              </div>
            )}
          </div>
        </div>

        {/* RIGHT PANEL: System Info */}
        {!state.isRightPanelMinimized && (
          <div
            className="flex flex-col border-l flex-shrink-0"
            style={{ backgroundColor: colors.panel, borderColor: 'var(--ft-border-color, #2A2A2A)', width: '210px' }}
          >
            <div className="flex items-center justify-between px-3 py-2 border-b" style={{ borderColor: 'var(--ft-border-color, #2A2A2A)' }}>
              <span className="text-[11px] font-bold tracking-wider" style={{ color: colors.textMuted }}>SYSTEM</span>
              <button onClick={() => dispatch({ type: 'TOGGLE_RIGHT_PANEL' })} className="p-0.5">
                <ChevronUp size={12} color={colors.textMuted} />
              </button>
            </div>

            <div className="flex-1 overflow-y-auto p-3 space-y-3">
              {/* Engine Status */}
              <div className="p-2.5" style={{ backgroundColor: colors.background, border: `1px solid ${colors.success}` }}>
                <div className="flex items-center gap-2 mb-2">
                  <Server size={13} color={colors.success} />
                  <span className="text-[11px] font-bold" style={{ color: colors.text }}>Mapping Engine</span>
                </div>
                <div className="space-y-1">
                  {[
                    { label: 'Status', value: 'ONLINE', color: colors.success },
                    { label: 'Parsers', value: '6 Active', color: colors.text },
                    { label: 'Schemas', value: '7 Built-in', color: colors.text },
                  ].map(({ label, value, color }) => (
                    <div key={label} className="flex justify-between text-[10px]">
                      <span style={{ color: colors.textMuted }}>{label}:</span>
                      <span style={{ color }} className="font-bold">{value}</span>
                    </div>
                  ))}
                </div>
              </div>

              {/* Security */}
              <div className="p-2.5" style={{ backgroundColor: colors.background, border: `1px solid ${colors.primary}` }}>
                <div className="flex items-center gap-2 mb-2">
                  <Shield size={13} color={colors.primary} />
                  <span className="text-[11px] font-bold" style={{ color: colors.text }}>Security</span>
                </div>
                <div className="space-y-1">
                  {[
                    { label: 'Encryption', value: 'AES-256', color: colors.success },
                    { label: 'Credentials', value: 'Secured', color: colors.success },
                    { label: 'Cache', value: state.apiConfig.cacheEnabled ? 'ON' : 'OFF', color: state.apiConfig.cacheEnabled ? colors.success : colors.textMuted },
                  ].map(({ label, value, color }) => (
                    <div key={label} className="flex justify-between text-[10px]">
                      <span style={{ color: colors.textMuted }}>{label}:</span>
                      <span style={{ color }} className="font-bold">{value}</span>
                    </div>
                  ))}
                </div>
              </div>

              {/* Current Mapping Info (create mode) */}
              {state.view === 'create' && (
                <div className="p-2.5" style={{ backgroundColor: colors.background, border: `1px solid var(--ft-border-color, #2A2A2A)` }}>
                  <div className="flex items-center gap-2 mb-2">
                    <Link size={13} color={colors.primary} />
                    <span className="text-[11px] font-bold" style={{ color: colors.text }}>Current Mapping</span>
                  </div>
                  <div className="space-y-1">
                    {[
                      { label: 'Name', value: state.mappingName || '—' },
                      { label: 'Method', value: state.apiConfig.method },
                      { label: 'Schema', value: state.selectedSchema?.toUpperCase() || (state.schemaType === 'custom' ? 'CUSTOM' : '—') },
                      { label: 'Fields', value: String(mappedCount) },
                      { label: 'Auth', value: state.apiConfig.authentication.type.toUpperCase() },
                    ].map(({ label, value }) => (
                      <div key={label} className="flex justify-between text-[10px]">
                        <span style={{ color: colors.textMuted }}>{label}:</span>
                        <span style={{ color: colors.text }} className="font-bold truncate ml-2 max-w-[90px]">{value}</span>
                      </div>
                    ))}
                  </div>
                </div>
              )}

              {/* Test Result Summary */}
              {state.testResult && (
                <div
                  className="p-2.5"
                  style={{
                    backgroundColor: colors.background,
                    border: `1px solid ${state.testResult.success ? colors.success : colors.alert}`,
                  }}
                >
                  <div className="flex items-center gap-2 mb-2">
                    {state.testResult.success ? (
                      <CheckCircle2 size={13} color={colors.success} />
                    ) : (
                      <AlertCircle size={13} color={colors.alert} />
                    )}
                    <span className="text-[11px] font-bold" style={{ color: colors.text }}>Last Test</span>
                  </div>
                  <div className="space-y-1">
                    {[
                      { label: 'Result', value: state.testResult.success ? 'PASSED' : 'FAILED', color: state.testResult.success ? colors.success : colors.alert },
                      { label: 'Duration', value: `${state.testResult.metadata.duration}ms` },
                      { label: 'Records', value: String(state.testResult.metadata.recordsReturned) },
                    ].map(({ label, value, color }) => (
                      <div key={label} className="flex justify-between text-[10px]">
                        <span style={{ color: colors.textMuted }}>{label}:</span>
                        <span style={{ color: color || colors.text }} className="font-bold">{value}</span>
                      </div>
                    ))}
                  </div>
                </div>
              )}

              {/* Parsers Available */}
              <div className="pt-2 border-t" style={{ borderColor: 'var(--ft-border-color, #2A2A2A)' }}>
                <div className="text-[10px] font-bold tracking-wider mb-2" style={{ color: colors.textMuted }}>PARSER ENGINES</div>
                <div className="space-y-1">
                  {['JSONPath', 'JSONata', 'JMESPath', 'Direct', 'JS Expr', 'Regex'].map((parser) => (
                    <div key={parser} className="flex items-center justify-between text-[10px]">
                      <span style={{ color: colors.textMuted }}>{parser}</span>
                      <span className="font-bold" style={{ color: colors.success }}>READY</span>
                    </div>
                  ))}
                </div>
              </div>
            </div>
          </div>
        )}

        {/* Right panel collapsed */}
        {state.isRightPanelMinimized && (
          <div className="flex flex-col items-center border-l flex-shrink-0"
            style={{ backgroundColor: colors.panel, borderColor: 'var(--ft-border-color, #2A2A2A)', width: '32px' }}>
            <button onClick={() => dispatch({ type: 'TOGGLE_RIGHT_PANEL' })} className="p-1 mt-1">
              <ChevronDown size={12} color={colors.textMuted} />
            </button>
          </div>
        )}
      </div>

      {/* ===== NAVIGATION FOOTER (create mode) ===== */}
      {state.view === 'create' && (
        <div
          className="flex items-center justify-between px-4 py-2.5 border-t flex-shrink-0"
          style={{ backgroundColor: colors.panel, borderColor: 'var(--ft-border-color, #2A2A2A)', height: '48px' }}
        >
          <button
            onClick={() => {
              const prev = stepIndex - 1;
              if (prev >= 0) dispatch({ type: 'SET_STEP', payload: steps[prev] });
            }}
            disabled={stepIndex === 0}
            className="flex items-center gap-2 px-4 py-2 text-xs font-bold tracking-wide transition-all"
            style={{
              backgroundColor: 'transparent',
              color: stepIndex === 0 ? colors.textMuted : colors.text,
              border: `1px solid ${stepIndex === 0 ? 'var(--ft-border-color, #2A2A2A)' : colors.textMuted}`,
              opacity: stepIndex === 0 ? 0.4 : 1,
              cursor: stepIndex === 0 ? 'not-allowed' : 'pointer',
            }}
            onMouseEnter={(e) => { if (stepIndex > 0) e.currentTarget.style.backgroundColor = '#1F1F1F'; }}
            onMouseLeave={(e) => e.currentTarget.style.backgroundColor = 'transparent'}
          >
            <ArrowLeft size={14} />
            PREVIOUS
          </button>

          <div className="text-[11px] font-mono" style={{ color: colors.textMuted }}>
            {stepIndex + 1} / {steps.length} — {activeStep.label.toUpperCase()}
          </div>

          <button
            onClick={() => {
              if (!canProceedToNextStep()) { showWarning('Please complete the current step before proceeding'); return; }
              const next = stepIndex + 1;
              if (next < steps.length) dispatch({ type: 'SET_STEP', payload: steps[next] });
            }}
            disabled={stepIndex === steps.length - 1}
            className="flex items-center gap-2 px-4 py-2 text-xs font-bold tracking-wide transition-all"
            style={{
              backgroundColor: stepIndex === steps.length - 1 ? colors.panel : colors.primary,
              color: stepIndex === steps.length - 1 ? colors.textMuted : colors.background,
              border: `1px solid ${stepIndex === steps.length - 1 ? 'var(--ft-border-color, #2A2A2A)' : colors.primary}`,
              opacity: stepIndex === steps.length - 1 ? 0.4 : 1,
              cursor: stepIndex === steps.length - 1 ? 'not-allowed' : 'pointer',
            }}
            onMouseEnter={(e) => { if (stepIndex < steps.length - 1) e.currentTarget.style.backgroundColor = '#FF8C00'; }}
            onMouseLeave={(e) => { if (stepIndex < steps.length - 1) e.currentTarget.style.backgroundColor = colors.primary; }}
          >
            NEXT
            <ArrowRight size={14} />
          </button>
        </div>
      )}

      {/* ===== BOTTOM STATUS BAR ===== */}
      <div
        className="flex items-center justify-between px-4 py-1.5 border-t text-[10px] font-mono flex-shrink-0"
        style={{ backgroundColor: colors.panel, borderColor: 'var(--ft-border-color, #2A2A2A)', height: '28px' }}
      >
        <div className="flex items-center gap-4">
          <span style={{ color: colors.textMuted }}>Fincept Data Mapping v3.3.1</span>
          <span style={{ color: colors.textMuted }}>•</span>
          <span style={{ color: colors.textMuted }}>Mappings: <span style={{ color: colors.primary }}>{totalMappings}</span></span>
          <span style={{ color: colors.textMuted }}>•</span>
          <span style={{ color: colors.textMuted }}>Engine: <span style={{ color: colors.success }}>ONLINE</span></span>
          {state.view === 'create' && (
            <>
              <span style={{ color: colors.textMuted }}>•</span>
              <span style={{ color: colors.textMuted }}>Step: <span style={{ color: colors.primary }}>{activeStep.shortLabel}</span></span>
            </>
          )}
        </div>
        <div className="flex items-center gap-3">
          <span style={{ color: colors.textMuted }}>
            View: <span style={{ color: colors.primary }}>{state.view.toUpperCase()}</span>
          </span>
          <span style={{ color: colors.textMuted }}>•</span>
          <span style={{ color: colors.textMuted }}>{currentTime.toLocaleDateString()}</span>
        </div>
      </div>
    </div>
  );
}
