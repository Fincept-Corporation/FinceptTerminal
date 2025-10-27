// Data Mapping Tab - Complete Rewrite with Standalone API Integration

import React, { useState, useEffect } from 'react';
import { Plus, List, Save, Play, ArrowLeft, ArrowRight, CheckCircle, Circle, Bookmark } from 'lucide-react';
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

type View = 'list' | 'create' | 'templates';

export default function DataMappingTab() {
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
      // Detect if data has common array structure (only if not already configured from template)
      const detectDataArray = (obj: any): { rootPath: string; isArray: boolean } => {
        if (Array.isArray(obj)) return { rootPath: '', isArray: true };

        // Common patterns: data, results, items, records
        const arrayKeys = ['data', 'results', 'items', 'records', 'response'];
        for (const key of arrayKeys) {
          if (obj[key] && Array.isArray(obj[key]) && obj[key].length > 0) {
            return { rootPath: key, isArray: true };
          }
        }

        return { rootPath: '', isArray: false };
      };

      // Use existing extraction config if available (from template), otherwise auto-detect
      let extractionConfig;
      if (fieldMappings.length > 0 && fieldMappings[0].source?.path?.startsWith('$')) {
        // Template has configured extraction, detect from sample data
        const { rootPath, isArray } = detectDataArray(sampleData);
        extractionConfig = {
          engine: ParserEngine.JSONPATH,
          rootPath: rootPath ? `$.${rootPath}[*]` : '',
          isArray,
        };
      } else {
        // No template, auto-detect
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
      // Detect if data has common array structure (same logic as test)
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

      // Use existing extraction config if available (from template), otherwise auto-detect
      let extractionConfig;
      if (fieldMappings.length > 0 && fieldMappings[0].source?.path?.startsWith('$')) {
        // Template has configured extraction, detect from sample data
        const { rootPath, isArray } = detectDataArray(sampleData);
        extractionConfig = {
          engine: ParserEngine.JSONPATH,
          rootPath: rootPath ? `$.${rootPath}[*]` : '',
          isArray,
        };
      } else {
        // No template, auto-detect
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
    // Load template into create view
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

  const renderStepContent = () => {
    switch (currentStep) {
      case 'api-config':
        return (
          <div className="p-6 max-w-4xl mx-auto">
            <h2 className="text-lg font-bold text-white mb-4">Configure API Connection</h2>
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
            <h2 className="text-lg font-bold text-white mb-4">Choose Target Schema</h2>
            <SchemaBuilder
              schemaType={schemaType}
              selectedSchema={selectedSchema}
              customFields={customFields}
              onSchemaTypeChange={setSchemaType}
              onSchemaSelect={(schema) => setSelectedSchema(schema as UnifiedSchemaType)}
              onCustomFieldsChange={setCustomFields}
              sampleData={sampleData}
              onFieldMappingsAutoCreate={(newMappings) => {
                // Only add mappings for fields that don't already have mappings
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
            <div className="p-6 text-center">
              <p className="text-red-400">Please fetch sample data first</p>
            </div>
          );
        }

        return (
          <div className="p-6 h-full">
            <h2 className="text-lg font-bold text-white mb-4">Map API Fields to Schema</h2>
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
            <h2 className="text-lg font-bold text-white mb-4">Cache & Security Settings</h2>
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
          <div className="p-6 max-w-4xl mx-auto space-y-4">
            <div className="flex items-center justify-between">
              <h2 className="text-lg font-bold text-white">Test & Save Mapping</h2>
              <button
                onClick={handleTestMapping}
                disabled={isTesting}
                className="bg-green-600 hover:bg-green-700 disabled:bg-zinc-700 text-white px-4 py-2 rounded font-bold flex items-center gap-2"
              >
                <Play size={16} />
                {isTesting ? 'TESTING...' : 'RUN TEST'}
              </button>
            </div>

            <LivePreview result={testResult || undefined} isLoading={isTesting} />

            {testResult?.success && (
              <button
                onClick={handleSaveMapping}
                className="w-full bg-orange-600 hover:bg-orange-700 text-white py-3 rounded font-bold flex items-center justify-center gap-2"
              >
                <Save size={18} />
                SAVE MAPPING
              </button>
            )}
          </div>
        );

      default:
        return null;
    }
  };

  return (
    <div className="w-full h-full bg-zinc-950 flex flex-col">
      {/* Header */}
      <div className="bg-zinc-900 border-b border-zinc-800 p-3">
        <div className="flex items-center justify-between">
          <div className="flex items-center gap-3">
            <span className="text-orange-500 text-sm font-bold tracking-wide">
              DATA MAPPING
            </span>
            <div className="w-px h-5 bg-zinc-700"></div>
            <span className="text-gray-500 text-xs">
              Connect any API directly to Fincept Terminal
            </span>
          </div>

          <div className="flex gap-2">
            <button
              onClick={() => setView('list')}
              className={`px-3 py-1.5 rounded text-xs font-bold flex items-center gap-1 ${
                view === 'list'
                  ? 'bg-orange-600 text-white'
                  : 'bg-transparent text-gray-400 border border-zinc-700 hover:border-zinc-600'
              }`}
            >
              <List size={12} />
              MY MAPPINGS
            </button>
            <button
              onClick={() => setView('templates')}
              className={`px-3 py-1.5 rounded text-xs font-bold flex items-center gap-1 ${
                view === 'templates'
                  ? 'bg-orange-600 text-white'
                  : 'bg-transparent text-gray-400 border border-zinc-700 hover:border-zinc-600'
              }`}
            >
              <Bookmark size={12} />
              TEMPLATES
            </button>
            <button
              onClick={handleStartNewMapping}
              className={`px-3 py-1.5 rounded text-xs font-bold flex items-center gap-1 ${
                view === 'create'
                  ? 'bg-orange-600 text-white'
                  : 'bg-transparent text-gray-400 border border-zinc-700 hover:border-zinc-600'
              }`}
            >
              <Plus size={12} />
              CREATE NEW
            </button>
          </div>
        </div>
      </div>

      {/* Step Progress */}
      {view === 'create' && (
        <div className="bg-zinc-900 border-b border-zinc-800 p-3">
          <div className="flex items-center gap-2">
            {steps.map((step, index) => {
              const status = getStepStatus(step);
              const Icon = status === 'complete' ? CheckCircle : Circle;

              return (
                <React.Fragment key={step}>
                  <div
                    className={`flex-1 flex flex-col items-center cursor-pointer ${
                      index <= stepIndex ? 'opacity-100' : 'opacity-40'
                    }`}
                    onClick={() => index <= stepIndex && setCurrentStep(step)}
                  >
                    <div className="flex items-center gap-2 mb-1">
                      <Icon
                        size={14}
                        className={
                          status === 'complete'
                            ? 'text-green-500'
                            : status === 'current'
                            ? 'text-orange-500'
                            : 'text-gray-600'
                        }
                      />
                      <span
                        className={`text-xs font-bold uppercase ${
                          status === 'complete'
                            ? 'text-green-500'
                            : status === 'current'
                            ? 'text-orange-500'
                            : 'text-gray-600'
                        }`}
                      >
                        {step.replace('-', ' ')}
                      </span>
                    </div>
                    <div
                      className={`h-1 w-full rounded ${
                        index <= stepIndex ? 'bg-orange-500' : 'bg-zinc-700'
                      }`}
                    ></div>
                  </div>
                  {index < steps.length - 1 && (
                    <ArrowRight size={12} className="text-zinc-700 flex-shrink-0" />
                  )}
                </React.Fragment>
              );
            })}
          </div>
        </div>
      )}

      {/* Content */}
      <div className="flex-1 overflow-auto">
        {view === 'list' && (
          <div className="p-6">
            {isLoadingMappings ? (
              <div className="text-center py-12 text-gray-400">Loading mappings...</div>
            ) : savedMappings.length === 0 ? (
              <div className="text-center py-12">
                <h3 className="text-gray-400 mb-3">No mappings yet</h3>
                <p className="text-gray-600 text-sm mb-6">
                  Create your first API mapping to get started
                </p>
                <button
                  onClick={handleStartNewMapping}
                  className="bg-orange-600 hover:bg-orange-700 text-white px-6 py-3 rounded font-bold flex items-center gap-2 mx-auto"
                >
                  <Plus size={16} />
                  CREATE FIRST MAPPING
                </button>
              </div>
            ) : (
              <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
                {savedMappings.map((mapping) => (
                  <div
                    key={mapping.id}
                    className="bg-zinc-900 border border-zinc-700 rounded p-4 hover:border-zinc-600 transition-colors"
                  >
                    <h3 className="text-white font-bold mb-1">{mapping.name}</h3>
                    <p className="text-xs text-gray-400 mb-3">{mapping.description}</p>
                    <div className="flex items-center justify-between text-xs">
                      <span className="text-gray-500">
                        {mapping.target.schemaType === 'predefined'
                          ? mapping.target.schema?.toUpperCase()
                          : 'CUSTOM'}
                      </span>
                      <div className="flex gap-2">
                        <button
                          onClick={() => handleLoadMapping(mapping)}
                          className="text-blue-400 hover:text-blue-300"
                        >
                          Edit
                        </button>
                        <button
                          onClick={() => handleDeleteMapping(mapping.id)}
                          className="text-red-400 hover:text-red-300"
                        >
                          Delete
                        </button>
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

      {/* Navigation Footer */}
      {view === 'create' && (
        <div className="bg-zinc-900 border-t border-zinc-800 p-3 flex justify-between">
          <button
            onClick={() => {
              const prevIndex = stepIndex - 1;
              if (prevIndex >= 0) {
                setCurrentStep(steps[prevIndex]);
              }
            }}
            disabled={stepIndex === 0}
            className="px-4 py-2 rounded text-xs font-bold flex items-center gap-2 bg-transparent text-gray-400 border border-zinc-700 hover:border-zinc-600 disabled:opacity-50 disabled:cursor-not-allowed"
          >
            <ArrowLeft size={14} />
            PREVIOUS
          </button>

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
            className="px-4 py-2 rounded text-xs font-bold flex items-center gap-2 bg-orange-600 hover:bg-orange-700 text-white disabled:opacity-50 disabled:cursor-not-allowed"
          >
            NEXT
            <ArrowRight size={14} />
          </button>
        </div>
      )}
    </div>
  );
}
