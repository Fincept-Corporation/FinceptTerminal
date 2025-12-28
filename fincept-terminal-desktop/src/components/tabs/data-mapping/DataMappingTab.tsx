// Data Mapping Tab - Bloomberg Terminal Theme UI

import React, { useState, useEffect } from 'react';
import { useTranslation } from 'react-i18next';
import { Plus, List, Save, Play, ArrowLeft, ArrowRight, CheckCircle2, Circle, Bookmark, Database, Trash2, Edit3 } from 'lucide-react';
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
import { TabFooter } from '@/components/common/TabFooter';

type View = 'list' | 'create' | 'templates';

export default function DataMappingTab() {
  const { t } = useTranslation('dataMapping');
  const [view, setView] = useState<View>('list');
  const [currentStep, setCurrentStep] = useState<CreateStep>('api-config');
  const [savedMappings, setSavedMappings] = useState<DataMappingConfig[]>([]);
  const [isLoadingMappings, setIsLoadingMappings] = useState(false);

  // State for creating new mapping
  const [mappingId, setMappingId] = useState<string>('');
  const [mappingName, setMappingName] = useState<string>('');
  const [mappingDescription, setMappingDescription] = useState<string>('');

  // API Configuration
  const [apiConfig, setApiConfig] = useState<APIConfig>({
    id: uuidv4(),
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
  });

  // Schema Configuration
  const [schemaType, setSchemaType] = useState<'predefined' | 'custom'>('predefined');
  const [selectedSchema, setSelectedSchema] = useState<UnifiedSchemaType | undefined>();
  const [customFields, setCustomFields] = useState<CustomField[]>([]);

  // Sample Data
  const [sampleData, setSampleData] = useState<any>(null);

  // Field Mappings
  const [fieldMappings, setFieldMappings] = useState<FieldMapping[]>([]);

  // Test Results
  const [testResult, setTestResult] = useState<MappingExecutionResult | null>(null);
  const [isTesting, setIsTesting] = useState(false);

  const steps: CreateStep[] = [
    'api-config',
    'schema-select',
    'field-mapping',
    'cache-settings',
    'test-save',
  ];
  const stepIndex = steps.indexOf(currentStep);

  useEffect(() => {
    loadSavedMappings();
    initializeDatabase();
  }, []);

  const initializeDatabase = async () => {
    try {
      await mappingDatabase.initialize();
    } catch (error) {
      console.error('Failed to initialize database:', error);
    }
  };

  const loadSavedMappings = async () => {
    setIsLoadingMappings(true);
    try {
      const mappings = await mappingDatabase.getAllMappings();
      setSavedMappings(mappings);
    } catch (error) {
      console.error('Failed to load mappings:', error);
    } finally {
      setIsLoadingMappings(false);
    }
  };

  const handleStartNewMapping = () => {
    // Reset all state
    setMappingId(uuidv4());
    setMappingName('');
    setMappingDescription('');
    setApiConfig({
      id: uuidv4(),
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
    });
    setSchemaType('predefined');
    setSelectedSchema(undefined);
    setCustomFields([]);
    setSampleData(null);
    setFieldMappings([]);
    setTestResult(null);
    setCurrentStep('api-config');
    setView('create');
  };

  const handleTestSuccess = (data: any) => {
    setSampleData(data);
    console.log('[DataMappingTab] Sample data fetched:', data);
  };

  const handleApiConfigChange = (config: APIConfig) => {
    setApiConfig(config);
    // Sync mapping name with API config name if not manually set
    if (!mappingName || mappingName === apiConfig.name) {
      setMappingName(config.name);
    }
    if (!mappingDescription || mappingDescription === apiConfig.description) {
      setMappingDescription(config.description);
    }
  };

  const handleTestMapping = async () => {
    if (!sampleData) {
      alert('Please fetch sample data first');
      return;
    }

    if (fieldMappings.length === 0) {
      alert('Please configure at least one field mapping');
      return;
    }

    setIsTesting(true);

    try {
      const detectDataArray = (obj: any): { rootPath: string; isArray: boolean } => {
        if (Array.isArray(obj)) return { rootPath: '', isArray: true };

        const arrayKeys = ['data', 'results', 'items', 'records', 'response'];
        for (const key of arrayKeys) {
          if (obj[key] && Array.isArray(obj[key]) && obj[key].length > 0) {
            return { rootPath: key, isArray: true };
          }
        }

        return { rootPath: '', isArray: false };
      };

      let extractionConfig;
      if (fieldMappings.length > 0 && fieldMappings[0].source?.path?.startsWith('$')) {
        const { rootPath, isArray } = detectDataArray(sampleData);
        extractionConfig = {
          engine: ParserEngine.JSONPATH,
          rootPath: rootPath ? `$.${rootPath}[*]` : '',
          isArray,
        };
      } else {
        const { rootPath, isArray } = detectDataArray(sampleData);
        extractionConfig = {
          engine: ParserEngine.JSONPATH,
          rootPath: rootPath ? `$.${rootPath}[*]` : '',
          isArray,
        };
      }

      const mappingConfig: DataMappingConfig = {
        id: mappingId,
        name: mappingName || apiConfig.name,
        description: mappingDescription || apiConfig.description,
        source: {
          type: 'sample',
          sampleData,
        },
        target: {
          schemaType,
          schema: schemaType === 'predefined' ? selectedSchema : undefined,
          customFields: schemaType === 'custom' ? customFields : undefined,
        },
        extraction: extractionConfig,
        fieldMappings,
        postProcessing: undefined,
        validation: {
          enabled: true,
          strictMode: false,
          rules: [],
        },
        metadata: {
          createdAt: new Date().toISOString(),
          updatedAt: new Date().toISOString(),
          version: 1,
          tags: [],
          aiGenerated: false,
        },
      };

      const result = await mappingEngine.test(mappingConfig, sampleData);
      setTestResult(result);

      if (!result.success) {
        alert(`Test failed: ${result.errors?.join(', ')}`);
      }
    } catch (error) {
      console.error('Test failed:', error);
      setTestResult({
        success: false,
        errors: [error instanceof Error ? error.message : String(error)],
        metadata: {
          executedAt: new Date().toISOString(),
          duration: 0,
          recordsProcessed: 0,
          recordsReturned: 0,
        },
      });
    } finally {
      setIsTesting(false);
    }
  };

  const handleSaveMapping = async () => {
    if (!testResult?.success) {
      alert('Please test the mapping successfully before saving');
      return;
    }

    if (!mappingName) {
      alert('Please provide a mapping name');
      return;
    }

    try {
      const detectDataArray = (obj: any): { rootPath: string; isArray: boolean } => {
        if (Array.isArray(obj)) return { rootPath: '', isArray: true };

        const arrayKeys = ['data', 'results', 'items', 'records', 'response'];
        for (const key of arrayKeys) {
          if (obj[key] && Array.isArray(obj[key]) && obj[key].length > 0) {
            return { rootPath: key, isArray: true };
          }
        }

        return { rootPath: '', isArray: false };
      };

      let extractionConfig;
      if (fieldMappings.length > 0 && fieldMappings[0].source?.path?.startsWith('$')) {
        const { rootPath, isArray } = detectDataArray(sampleData);
        extractionConfig = {
          engine: ParserEngine.JSONPATH,
          rootPath: rootPath ? `$.${rootPath}[*]` : '',
          isArray,
        };
      } else {
        const { rootPath, isArray } = detectDataArray(sampleData);
        extractionConfig = {
          engine: ParserEngine.JSONPATH,
          rootPath: rootPath ? `$.${rootPath}[*]` : '',
          isArray,
        };
      }

      const mappingConfig: DataMappingConfig = {
        id: mappingId,
        name: mappingName,
        description: mappingDescription,
        source: {
          type: 'api',
          apiConfig,
        },
        target: {
          schemaType,
          schema: schemaType === 'predefined' ? selectedSchema : undefined,
          customFields: schemaType === 'custom' ? customFields : undefined,
        },
        extraction: extractionConfig,
        fieldMappings,
        postProcessing: undefined,
        validation: {
          enabled: true,
          strictMode: false,
          rules: [],
        },
        metadata: {
          createdAt: new Date().toISOString(),
          updatedAt: new Date().toISOString(),
          version: 1,
          tags: [],
          aiGenerated: false,
        },
      };

      await mappingDatabase.saveMapping(mappingConfig);
      alert('Mapping saved successfully!');
      await loadSavedMappings();
      setView('list');
    } catch (error) {
      console.error('Failed to save mapping:', error);
      alert(`Failed to save mapping: ${error instanceof Error ? error.message : String(error)}`);
    }
  };

  const handleDeleteMapping = async (id: string) => {
    if (!confirm('Delete this mapping? This cannot be undone.')) {
      return;
    }

    try {
      await mappingDatabase.deleteMapping(id);
      await loadSavedMappings();
    } catch (error) {
      console.error('Failed to delete mapping:', error);
      alert('Failed to delete mapping');
    }
  };

  const handleLoadMapping = async (mapping: DataMappingConfig) => {
    setMappingId(mapping.id);
    setMappingName(mapping.name);
    setMappingDescription(mapping.description);

    if (mapping.source.type === 'api' && mapping.source.apiConfig) {
      setApiConfig(mapping.source.apiConfig);
    }

    setSchemaType(mapping.target.schemaType);
    if (mapping.target.schemaType === 'predefined') {
      setSelectedSchema(mapping.target.schema as UnifiedSchemaType | undefined);
    } else {
      setCustomFields(mapping.target.customFields || []);
    }

    setFieldMappings(mapping.fieldMappings);
    setCurrentStep('api-config');
    setView('create');
  };

  const handleSelectTemplate = (template: MappingTemplate) => {
    setMappingId(uuidv4());
    setMappingName(template.mappingConfig.name || template.name);
    setMappingDescription(template.mappingConfig.description || template.description);

    if (template.mappingConfig.source?.apiConfig) {
      setApiConfig(template.mappingConfig.source.apiConfig);
    }

    if (template.mappingConfig.target) {
      setSchemaType(template.mappingConfig.target.schemaType);
      if (template.mappingConfig.target.schemaType === 'predefined') {
        setSelectedSchema(template.mappingConfig.target.schema as UnifiedSchemaType);
      } else {
        setCustomFields(template.mappingConfig.target.customFields || []);
      }
    }

    if (template.mappingConfig.fieldMappings) {
      setFieldMappings(template.mappingConfig.fieldMappings);
    }

    setCurrentStep('api-config');
    setView('create');
  };

  const canProceedToNextStep = () => {
    switch (currentStep) {
      case 'api-config':
        return apiConfig.baseUrl && apiConfig.endpoint && apiConfig.name && sampleData !== null;
      case 'schema-select':
        return (schemaType === 'predefined' && selectedSchema) ||
          (schemaType === 'custom' && customFields.length > 0);
      case 'field-mapping':
        return fieldMappings.length > 0;
      case 'cache-settings':
        return true;
      case 'test-save':
        return testResult?.success === true;
      default:
        return false;
    }
  };

  const getStepStatus = (step: CreateStep): 'complete' | 'current' | 'pending' => {
    const targetIndex = steps.indexOf(step);
    if (targetIndex < stepIndex) return 'complete';
    if (targetIndex === stepIndex) return 'current';
    return 'pending';
  };

  const getStepLabel = (step: CreateStep): string => {
    const labels: Record<CreateStep, string> = {
      'api-config': 'API CONFIG',
      'schema-select': 'SCHEMA',
      'field-mapping': 'MAPPING',
      'cache-settings': 'CACHE',
      'test-save': 'TEST & SAVE',
    };
    return labels[step];
  };

  const renderStepContent = () => {
    switch (currentStep) {
      case 'api-config':
        return (
          <div className="p-6 max-w-6xl mx-auto">
            <APIConfigurationPanel
              config={apiConfig}
              onChange={handleApiConfigChange}
              onTestSuccess={handleTestSuccess}
            />
          </div>
        );

      case 'schema-select':
        return (
          <div className="p-6 max-w-full mx-auto">
            <SchemaBuilder
              schemaType={schemaType}
              selectedSchema={selectedSchema}
              customFields={customFields}
              onSchemaTypeChange={setSchemaType}
              onSchemaSelect={(schema) => setSelectedSchema(schema as UnifiedSchemaType)}
              onCustomFieldsChange={setCustomFields}
              sampleData={sampleData}
              onFieldMappingsAutoCreate={(newMappings) => {
                const existingFields = new Set(fieldMappings.map(m => m.targetField));
                const uniqueNewMappings = newMappings.filter(m => !existingFields.has(m.targetField));
                if (uniqueNewMappings.length > 0) {
                  setFieldMappings([...fieldMappings, ...uniqueNewMappings]);
                }
              }}
            />
          </div>
        );

      case 'field-mapping':
        if (!sampleData) {
          return (
            <div className="flex items-center justify-center h-64">
              <div className="text-center">
                <Database className="mx-auto mb-3 text-[#666666]" size={48} />
                <p className="text-[#999999] text-sm">No sample data available</p>
                <p className="text-[#666666] text-xs mt-1">Please fetch sample data from API configuration</p>
              </div>
            </div>
          );
        }

        return (
          <div className="p-6 h-full">
            <VisualFieldMapper
              schemaType={schemaType}
              selectedSchema={selectedSchema}
              customFields={customFields}
              sampleData={sampleData}
              mappings={fieldMappings}
              onMappingsChange={setFieldMappings}
            />
          </div>
        );

      case 'cache-settings':
        return (
          <div className="p-6 max-w-4xl mx-auto">
            <CacheSettings
              cacheEnabled={apiConfig.cacheEnabled}
              cacheTTL={apiConfig.cacheTTL}
              onCacheEnabledChange={(enabled) =>
                setApiConfig({ ...apiConfig, cacheEnabled: enabled })
              }
              onCacheTTLChange={(ttl) => setApiConfig({ ...apiConfig, cacheTTL: ttl })}
              mappingId={mappingId}
            />
          </div>
        );

      case 'test-save':
        return (
          <div className="p-6 max-w-6xl mx-auto space-y-4">
            <div className="flex items-center justify-between mb-2">
              <div>
                <h3 className="text-[#FFA500] text-xs font-bold tracking-wider">TEST & VALIDATION</h3>
                <p className="text-[#999999] text-xs mt-1">Run test to verify mapping configuration</p>
              </div>
              <button
                onClick={handleTestMapping}
                disabled={isTesting}
                className="bg-[#FFA500] hover:bg-[#FF8C00] disabled:bg-[#333333] disabled:text-[#666666] text-black px-4 py-2 rounded text-xs font-bold flex items-center gap-2 transition-colors"
              >
                <Play size={14} />
                {isTesting ? 'TESTING...' : 'RUN TEST'}
              </button>
            </div>

            <LivePreview result={testResult || undefined} isLoading={isTesting} />

            {testResult?.success && (
              <div className="pt-4 border-t border-[#2a2a2a]">
                <button
                  onClick={handleSaveMapping}
                  className="w-full bg-[#00AA00] hover:bg-[#009900] text-white py-3 rounded text-sm font-bold flex items-center justify-center gap-2 transition-colors"
                >
                  <Save size={18} />
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
    <div className="w-full h-full bg-[#0a0a0a] flex flex-col text-white">
      {/* Bloomberg-style Header */}
      <div className="bg-[#1a1a1a] border-b border-[#2a2a2a] px-4 py-2.5">
        <div className="flex items-center justify-between">
          <div className="flex items-center gap-3">
            <Database className="text-[#FFA500]" size={18} />
            <div className="flex items-center gap-3">
              <span className="text-[#FFA500] text-sm font-bold tracking-wider">DATA MAPPING</span>
              <div className="w-px h-4 bg-[#333333]"></div>
              <span className="text-[#999999] text-xs">API Integration & Schema Transformation</span>
            </div>
          </div>

          <div className="flex items-center gap-2">
            <button
              onClick={() => setView('list')}
              className={`px-3 py-1.5 rounded text-xs font-bold flex items-center gap-1.5 transition-all ${
                view === 'list'
                  ? 'bg-[#FFA500] text-black'
                  : 'bg-[#2a2a2a] text-[#999999] hover:bg-[#333333] hover:text-white'
              }`}
            >
              <List size={14} />
              MAPPINGS
            </button>
            <button
              onClick={() => setView('templates')}
              className={`px-3 py-1.5 rounded text-xs font-bold flex items-center gap-1.5 transition-all ${
                view === 'templates'
                  ? 'bg-[#FFA500] text-black'
                  : 'bg-[#2a2a2a] text-[#999999] hover:bg-[#333333] hover:text-white'
              }`}
            >
              <Bookmark size={14} />
              TEMPLATES
            </button>
            <button
              onClick={handleStartNewMapping}
              className={`px-3 py-1.5 rounded text-xs font-bold flex items-center gap-1.5 transition-all ${
                view === 'create'
                  ? 'bg-[#FFA500] text-black'
                  : 'bg-[#2a2a2a] text-[#999999] hover:bg-[#333333] hover:text-white'
              }`}
            >
              <Plus size={14} />
              CREATE
            </button>
          </div>
        </div>
      </div>

      {/* Step Progress Bar - Bloomberg Style */}
      {view === 'create' && (
        <div className="bg-[#1a1a1a] border-b border-[#2a2a2a] px-4 py-3">
          <div className="flex items-center gap-1">
            {steps.map((step, index) => {
              const status = getStepStatus(step);
              const isActive = status === 'current';
              const isComplete = status === 'complete';

              return (
                <React.Fragment key={step}>
                  <div
                    className={`flex-1 cursor-pointer transition-all ${
                      index <= stepIndex ? 'opacity-100' : 'opacity-40'
                    }`}
                    onClick={() => index <= stepIndex && setCurrentStep(step)}
                  >
                    <div className="flex items-center gap-2 mb-2 px-2">
                      {isComplete ? (
                        <CheckCircle2 size={14} className="text-[#00AA00] flex-shrink-0" />
                      ) : (
                        <Circle size={14} className={isActive ? 'text-[#FFA500]' : 'text-[#4a4a4a]'} />
                      )}
                      <span
                        className={`text-[10px] font-bold tracking-wider ${
                          isComplete
                            ? 'text-[#00AA00]'
                            : isActive
                            ? 'text-[#FFA500]'
                            : 'text-[#666666]'
                        }`}
                      >
                        {getStepLabel(step)}
                      </span>
                    </div>
                    <div
                      className={`h-0.5 w-full transition-all ${
                        isComplete
                          ? 'bg-[#00AA00]'
                          : isActive
                          ? 'bg-[#FFA500]'
                          : 'bg-[#2a2a2a]'
                      }`}
                    ></div>
                  </div>
                  {index < steps.length - 1 && (
                    <div className="w-2 h-0.5 bg-[#2a2a2a] mb-5 flex-shrink-0"></div>
                  )}
                </React.Fragment>
              );
            })}
          </div>
        </div>
      )}

      {/* Content Area */}
      <div className="flex-1 overflow-auto bg-[#0a0a0a]">
        {view === 'list' && (
          <div className="p-6">
            {isLoadingMappings ? (
              <div className="flex items-center justify-center h-64">
                <div className="text-center">
                  <div className="inline-block animate-spin rounded-full h-8 w-8 border-b-2 border-[#FFA500] mb-3"></div>
                  <p className="text-[#999999] text-sm">Loading mappings...</p>
                </div>
              </div>
            ) : savedMappings.length === 0 ? (
              <div className="flex items-center justify-center h-64">
                <div className="text-center max-w-md">
                  <Database className="mx-auto mb-4 text-[#666666]" size={48} />
                  <h3 className="text-[#CCCCCC] text-lg font-bold mb-2">NO MAPPINGS CONFIGURED</h3>
                  <p className="text-[#999999] text-sm mb-6">
                    Create your first API data mapping to connect external data sources to Fincept Terminal
                  </p>
                  <button
                    onClick={handleStartNewMapping}
                    className="bg-[#FFA500] hover:bg-[#FF8C00] text-black px-6 py-3 rounded text-sm font-bold flex items-center gap-2 mx-auto transition-colors"
                  >
                    <Plus size={16} />
                    CREATE FIRST MAPPING
                  </button>
                </div>
              </div>
            ) : (
              <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
                {savedMappings.map((mapping) => (
                  <div
                    key={mapping.id}
                    className="bg-[#1a1a1a] border border-[#2a2a2a] rounded hover:border-[#FFA500] transition-all group"
                  >
                    <div className="p-4">
                      <div className="flex items-start justify-between mb-3">
                        <div className="flex-1">
                          <h3 className="text-white font-bold text-sm mb-1">{mapping.name}</h3>
                          <p className="text-[#999999] text-xs line-clamp-2">{mapping.description}</p>
                        </div>
                        <div className="ml-2 px-2 py-1 bg-[#2a2a2a] rounded text-[10px] font-bold text-[#FFA500]">
                          {mapping.target.schemaType === 'predefined'
                            ? mapping.target.schema?.toUpperCase()
                            : 'CUSTOM'}
                        </div>
                      </div>

                      <div className="flex items-center justify-between pt-3 border-t border-[#2a2a2a]">
                        <span className="text-[#666666] text-[10px]">
                          {new Date(mapping.metadata.createdAt).toLocaleDateString()}
                        </span>
                        <div className="flex gap-2 opacity-0 group-hover:opacity-100 transition-opacity">
                          <button
                            onClick={() => handleLoadMapping(mapping)}
                            className="p-1 hover:bg-[#2a2a2a] rounded text-[#999999] hover:text-[#FFA500] transition-colors"
                            title="Edit"
                          >
                            <Edit3 size={14} />
                          </button>
                          <button
                            onClick={() => handleDeleteMapping(mapping.id)}
                            className="p-1 hover:bg-[#2a2a2a] rounded text-[#999999] hover:text-red-500 transition-colors"
                            title="Delete"
                          >
                            <Trash2 size={14} />
                          </button>
                        </div>
                      </div>
                    </div>
                  </div>
                ))}
              </div>
            )}
          </div>
        )}

        {view === 'templates' && (
          <div className="p-6">
            <TemplateLibrary onSelectTemplate={handleSelectTemplate} />
          </div>
        )}

        {view === 'create' && <>{renderStepContent()}</>}
      </div>

      {/* Navigation Footer - Bloomberg Style */}
      {view === 'create' && (
        <div className="bg-[#1a1a1a] border-t border-[#2a2a2a] px-4 py-3 flex justify-between items-center">
          <button
            onClick={() => {
              const prevIndex = stepIndex - 1;
              if (prevIndex >= 0) {
                setCurrentStep(steps[prevIndex]);
              }
            }}
            disabled={stepIndex === 0}
            className="px-4 py-2 rounded text-xs font-bold flex items-center gap-2 bg-[#2a2a2a] text-[#999999] hover:bg-[#333333] hover:text-white disabled:opacity-30 disabled:cursor-not-allowed transition-all"
          >
            <ArrowLeft size={14} />
            PREVIOUS
          </button>

          <div className="text-[#666666] text-xs">
            STEP {stepIndex + 1} OF {steps.length}
          </div>

          <button
            onClick={() => {
              if (!canProceedToNextStep()) {
                alert('Please complete the current step before proceeding');
                return;
              }
              const nextIndex = stepIndex + 1;
              if (nextIndex < steps.length) {
                setCurrentStep(steps[nextIndex]);
              }
            }}
            disabled={stepIndex === steps.length - 1}
            className="px-4 py-2 rounded text-xs font-bold flex items-center gap-2 bg-[#FFA500] hover:bg-[#FF8C00] text-black disabled:opacity-30 disabled:cursor-not-allowed transition-all"
          >
            NEXT
            <ArrowRight size={14} />
          </button>
        </div>
      )}

      <TabFooter
        tabName="DATA MAPPING"
        leftInfo={[
          { label: `TOTAL: ${savedMappings.length}`, color: '#999999' },
          { label: `MODE: ${view.toUpperCase()}`, color: '#999999' },
          ...(view === 'create' ? [{ label: getStepLabel(currentStep), color: '#FFA500' }] : [])
        ]}
        statusInfo={`API TRANSFORMATION • SCHEMA MAPPING • DATA INTEGRATION`}
        backgroundColor="#1a1a1a"
        borderColor="#2a2a2a"
      />
    </div>
  );
}
