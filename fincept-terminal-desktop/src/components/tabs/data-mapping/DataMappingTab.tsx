// Data Mapping Tab - Main component

import React, { useState } from 'react';
import { Plus, List, Save, Play, ArrowLeft, ArrowRight } from 'lucide-react';
import { DataMappingConfig, ParserEngine, MappingExecutionResult, MappingTemplate } from './types';
import { DataSourceConnection } from '../data-sources/types';
import { JSONExplorer } from './components/JSONExplorer';
import { ExpressionBuilder } from './components/ExpressionBuilder';
import { LivePreview } from './components/LivePreview';
import { SchemaSelector } from './components/SchemaSelector';
import { ConnectionSelector } from './components/ConnectionSelector';
import { TemplateLibrary } from './components/TemplateLibrary';
import { MappingEngine } from './engine/MappingEngine';
import { getSchema } from './schemas';

type View = 'gallery' | 'create' | 'list';
type CreateStep = 'template' | 'connection' | 'schema' | 'data' | 'mapping' | 'test';

export default function DataMappingTab() {
  const [view, setView] = useState<View>('gallery');
  const [currentStep, setCurrentStep] = useState<CreateStep>('template');

  // State for creating new mapping
  const [selectedConnection, setSelectedConnection] = useState<DataSourceConnection | null>(null);
  const [selectedSchema, setSelectedSchema] = useState<string>('');
  const [endpoint, setEndpoint] = useState('');
  const [sampleData, setSampleData] = useState<any>(null);
  const [sampleDataJson, setSampleDataJson] = useState('');
  const [mappingName, setMappingName] = useState('');
  const [extractionEngine, setExtractionEngine] = useState<ParserEngine>(ParserEngine.JSONPATH);
  const [extractionExpression, setExtractionExpression] = useState('');
  const [fieldMappings, setFieldMappings] = useState<any[]>([]);
  const [testResult, setTestResult] = useState<MappingExecutionResult | null>(null);
  const [isTesting, setIsTesting] = useState(false);
  const [jsonError, setJsonError] = useState<string | null>(null);

  const steps: CreateStep[] = ['template', 'connection', 'schema', 'data', 'mapping', 'test'];
  const stepIndex = steps.indexOf(currentStep);

  const mappingEngine = new MappingEngine();

  const handleTemplateSelect = (template: MappingTemplate) => {
    // Load template configuration
    const config = template.mappingConfig;

    setMappingName(config.name || template.name);
    if (config.target?.schema) {
      setSelectedSchema(config.target.schema);
    }
    if (config.extraction) {
      setExtractionEngine(config.extraction.engine);
      setExtractionExpression(config.extraction.rootPath || '');
    }
    if (config.fieldMappings) {
      setFieldMappings(config.fieldMappings);
    }

    // Skip to connection step
    setCurrentStep('connection');
  };

  const handleConnectionSelect = (connection: DataSourceConnection) => {
    setSelectedConnection(connection);
  };

  const handleSchemaSelect = (schemaName: string) => {
    setSelectedSchema(schemaName);
  };

  const handleFetchSampleData = async () => {
    try {
      const data = JSON.parse(sampleDataJson);
      setSampleData(data);
      setJsonError(null);
    } catch (error) {
      const errorMessage = error instanceof Error ? error.message : 'Invalid JSON format';
      setJsonError(errorMessage);
      alert(`JSON Error: ${errorMessage}`);
    }
  };

  const handleAddFieldMapping = () => {
    setFieldMappings([
      ...fieldMappings,
      {
        targetField: '',
        sourceExpression: '',
        required: false,
      },
    ]);
  };

  const handleUpdateFieldMapping = (index: number, field: string, value: any) => {
    const updated = [...fieldMappings];
    updated[index] = { ...updated[index], [field]: value };
    setFieldMappings(updated);
  };

  const handleRemoveFieldMapping = (index: number) => {
    setFieldMappings(fieldMappings.filter((_, i) => i !== index));
  };

  const handleTestMapping = async () => {
    if (!sampleData || !selectedSchema) {
      alert('Please provide sample data and select a schema');
      return;
    }

    setIsTesting(true);

    try {
      const mappingConfig: DataMappingConfig = {
        id: 'test_mapping',
        name: mappingName || 'Test Mapping',
        description: '',
        source: {
          connectionId: selectedConnection?.id || '',
          connection: selectedConnection || undefined,
          sampleData,
        },
        target: {
          schema: selectedSchema,
        },
        extraction: {
          engine: extractionEngine,
          rootPath: extractionExpression,
          isArray: true,
        },
        fieldMappings: fieldMappings.filter((m) => m.targetField && m.sourceExpression),
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

  const handleSaveMapping = () => {
    // TODO: Save to DuckDB
    alert('Mapping saved! (Storage implementation pending)');
    setView('list');
  };

  const renderStepContent = () => {
    switch (currentStep) {
      case 'template':
        return (
          <div className="p-6">
            <TemplateLibrary onSelectTemplate={handleTemplateSelect} />
            <div className="mt-6 flex justify-center">
              <button
                onClick={() => setCurrentStep('connection')}
                className="bg-zinc-800 hover:bg-zinc-700 text-white px-6 py-2 rounded text-sm font-bold"
              >
                SKIP - CREATE FROM SCRATCH
              </button>
            </div>
          </div>
        );

      case 'connection':
        return (
          <div className="p-6">
            <ConnectionSelector
              selectedConnectionId={selectedConnection?.id}
              onSelect={handleConnectionSelect}
            />
          </div>
        );

      case 'schema':
        return (
          <div className="p-6">
            <SchemaSelector selectedSchema={selectedSchema} onSelect={handleSchemaSelect} />
          </div>
        );

      case 'data':
        return (
          <div className="p-6 space-y-4">
            <div>
              <h3 className="text-xs font-bold text-orange-500 uppercase mb-2">Endpoint Configuration</h3>
              <input
                type="text"
                placeholder="/historical-candle/{symbol}/day/{from}/{to}"
                value={endpoint}
                onChange={(e) => setEndpoint(e.target.value)}
                className="w-full bg-zinc-900 border border-zinc-700 text-gray-300 px-3 py-2 text-sm rounded focus:outline-none focus:border-orange-500"
              />
            </div>

            <div>
              <h3 className="text-xs font-bold text-orange-500 uppercase mb-2">Sample Data (JSON)</h3>
              <textarea
                value={sampleDataJson}
                onChange={(e) => setSampleDataJson(e.target.value)}
                placeholder='{"data": {"candles": [[timestamp, o, h, l, c, v]]}}'
                rows={12}
                className="w-full bg-zinc-900 border border-zinc-700 text-gray-300 p-3 text-xs font-mono rounded focus:outline-none focus:border-orange-500"
              />
              <button
                onClick={handleFetchSampleData}
                className="mt-2 w-full bg-green-600 hover:bg-green-700 text-white py-2 rounded text-sm font-bold"
              >
                PARSE & PREVIEW
              </button>
            </div>

            {sampleData && (
              <div>
                <h3 className="text-xs font-bold text-orange-500 uppercase mb-2">JSON Structure</h3>
                <JSONExplorer data={sampleData} />
              </div>
            )}
          </div>
        );

      case 'mapping':
        const schema = selectedSchema ? getSchema(selectedSchema) : null;

        return (
          <div className="p-6 space-y-4">
            <div>
              <h3 className="text-xs font-bold text-orange-500 uppercase mb-2">Mapping Name</h3>
              <input
                type="text"
                placeholder="My Mapping Name"
                value={mappingName}
                onChange={(e) => setMappingName(e.target.value)}
                className="w-full bg-zinc-900 border border-zinc-700 text-gray-300 px-3 py-2 text-sm rounded focus:outline-none focus:border-orange-500"
              />
            </div>

            <div>
              <h3 className="text-xs font-bold text-orange-500 uppercase mb-2">Data Extraction</h3>
              <ExpressionBuilder
                initialEngine={extractionEngine}
                initialExpression={extractionExpression}
                testData={sampleData}
                onExpressionChange={(engine, expression) => {
                  setExtractionEngine(engine);
                  setExtractionExpression(expression);
                }}
              />
            </div>

            <div>
              <div className="flex items-center justify-between mb-3">
                <h3 className="text-xs font-bold text-orange-500 uppercase">Field Mappings</h3>
                <button
                  onClick={handleAddFieldMapping}
                  className="bg-orange-600 hover:bg-orange-700 text-white px-3 py-1 rounded text-xs font-bold flex items-center gap-1"
                >
                  <Plus size={12} />
                  ADD FIELD
                </button>
              </div>

              <div className="space-y-2">
                {schema &&
                  Object.entries(schema.fields).map(([fieldName, fieldSpec]) => {
                    const existingMapping = fieldMappings.find((m) => m.targetField === fieldName);
                    const index = fieldMappings.indexOf(existingMapping);

                    return (
                      <div key={fieldName} className="bg-zinc-900 border border-zinc-700 rounded p-3">
                        <div className="grid grid-cols-2 gap-3">
                          <div>
                            <label className="text-xs text-gray-400 block mb-1">
                              Target Field {fieldSpec.required && <span className="text-orange-500">*</span>}
                            </label>
                            <input
                              type="text"
                              value={fieldName}
                              disabled
                              className="w-full bg-zinc-800 border border-zinc-700 text-gray-400 px-2 py-1 text-xs rounded font-mono"
                            />
                          </div>
                          <div>
                            <label className="text-xs text-gray-400 block mb-1">Source Expression</label>
                            <input
                              type="text"
                              placeholder="$.data.field"
                              value={existingMapping?.sourceExpression || ''}
                              onChange={(e) => {
                                if (existingMapping) {
                                  handleUpdateFieldMapping(index, 'sourceExpression', e.target.value);
                                } else {
                                  setFieldMappings([
                                    ...fieldMappings,
                                    {
                                      targetField: fieldName,
                                      sourceExpression: e.target.value,
                                      required: fieldSpec.required,
                                    },
                                  ]);
                                }
                              }}
                              className="w-full bg-zinc-800 border border-zinc-700 text-gray-300 px-2 py-1 text-xs rounded font-mono focus:outline-none focus:border-orange-500"
                            />
                          </div>
                        </div>
                      </div>
                    );
                  })}
              </div>
            </div>
          </div>
        );

      case 'test':
        return (
          <div className="p-6 space-y-4">
            <div className="flex items-center justify-between">
              <h3 className="text-xs font-bold text-orange-500 uppercase">Test Mapping</h3>
              <button
                onClick={handleTestMapping}
                disabled={isTesting}
                className="bg-green-600 hover:bg-green-700 disabled:bg-zinc-700 text-white px-4 py-2 rounded text-sm font-bold flex items-center gap-2"
              >
                <Play size={14} />
                {isTesting ? 'TESTING...' : 'RUN TEST'}
              </button>
            </div>

            <LivePreview result={testResult || undefined} isLoading={isTesting} />

            {testResult?.success && (
              <button
                onClick={handleSaveMapping}
                className="w-full bg-orange-600 hover:bg-orange-700 text-white py-3 rounded font-bold flex items-center justify-center gap-2"
              >
                <Save size={16} />
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
    <div style={{ width: '100%', height: '100%', backgroundColor: '#0a0a0a', display: 'flex', flexDirection: 'column' }}>
      {/* Header */}
      <div
        style={{
          backgroundColor: '#1a1a1a',
          borderBottom: '1px solid #2d2d2d',
          padding: '12px 16px',
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <span
              style={{
                color: '#ea580c',
                fontSize: '14px',
                fontWeight: 'bold',
                letterSpacing: '0.5px',
              }}
            >
              DATA MAPPING
            </span>
            <div style={{ width: '1px', height: '20px', backgroundColor: '#404040' }}></div>
            <span style={{ color: '#737373', fontSize: '11px' }}>Transform any data source to unified schemas</span>
          </div>

          {/* View Switcher */}
          <div style={{ display: 'flex', gap: '8px' }}>
            <button
              onClick={() => setView('gallery')}
              style={{
                backgroundColor: view === 'gallery' ? '#ea580c' : 'transparent',
                color: view === 'gallery' ? 'white' : '#a3a3a3',
                border: `1px solid ${view === 'gallery' ? '#ea580c' : '#404040'}`,
                padding: '6px 12px',
                fontSize: '11px',
                cursor: 'pointer',
                borderRadius: '3px',
                fontWeight: 'bold',
              }}
            >
              TEMPLATES
            </button>
            <button
              onClick={() => {
                setView('create');
                setCurrentStep('template');
              }}
              style={{
                backgroundColor: view === 'create' ? '#ea580c' : 'transparent',
                color: view === 'create' ? 'white' : '#a3a3a3',
                border: `1px solid ${view === 'create' ? '#ea580c' : '#404040'}`,
                padding: '6px 12px',
                fontSize: '11px',
                cursor: 'pointer',
                borderRadius: '3px',
                fontWeight: 'bold',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
              }}
            >
              <Plus size={12} />
              CREATE NEW
            </button>
            <button
              onClick={() => setView('list')}
              style={{
                backgroundColor: view === 'list' ? '#ea580c' : 'transparent',
                color: view === 'list' ? 'white' : '#a3a3a3',
                border: `1px solid ${view === 'list' ? '#ea580c' : '#404040'}`,
                padding: '6px 12px',
                fontSize: '11px',
                cursor: 'pointer',
                borderRadius: '3px',
                fontWeight: 'bold',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
              }}
            >
              <List size={12} />
              MY MAPPINGS
            </button>
          </div>
        </div>
      </div>

      {/* Step Progress (for create view) */}
      {view === 'create' && (
        <div style={{ backgroundColor: '#1a1a1a', borderBottom: '1px solid #2d2d2d', padding: '12px 16px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            {steps.map((step, index) => (
              <React.Fragment key={step}>
                <div
                  style={{
                    flex: 1,
                    textAlign: 'center',
                    cursor: index <= stepIndex ? 'pointer' : 'default',
                  }}
                  onClick={() => index <= stepIndex && setCurrentStep(step)}
                >
                  <div
                    style={{
                      height: '4px',
                      backgroundColor: index <= stepIndex ? '#ea580c' : '#404040',
                      borderRadius: '2px',
                      marginBottom: '4px',
                    }}
                  ></div>
                  <span
                    style={{
                      fontSize: '10px',
                      color: index <= stepIndex ? '#ea580c' : '#737373',
                      fontWeight: 'bold',
                      textTransform: 'uppercase',
                    }}
                  >
                    {step}
                  </span>
                </div>
                {index < steps.length - 1 && (
                  <ArrowRight size={12} style={{ color: '#404040' }} />
                )}
              </React.Fragment>
            ))}
          </div>
        </div>
      )}

      {/* Content */}
      <div style={{ flex: 1, overflow: 'auto' }}>
        {view === 'gallery' && (
          <div style={{ padding: '24px' }}>
            <TemplateLibrary
              onSelectTemplate={(template) => {
                handleTemplateSelect(template);
                setView('create');
              }}
            />
          </div>
        )}

        {view === 'create' && (
          <>
            {renderStepContent()}

            {/* Navigation Buttons */}
            {currentStep !== 'template' && (
              <div
                style={{
                  position: 'sticky',
                  bottom: 0,
                  backgroundColor: '#1a1a1a',
                  borderTop: '1px solid #2d2d2d',
                  padding: '16px',
                  display: 'flex',
                  gap: '12px',
                  justifyContent: 'space-between',
                }}
              >
                <button
                  onClick={() => {
                    const prevIndex = stepIndex - 1;
                    if (prevIndex >= 0) {
                      setCurrentStep(steps[prevIndex]);
                    }
                  }}
                  disabled={stepIndex === 0}
                  style={{
                    backgroundColor: 'transparent',
                    color: '#a3a3a3',
                    border: '1px solid #404040',
                    padding: '8px 16px',
                    fontSize: '11px',
                    cursor: stepIndex === 0 ? 'not-allowed' : 'pointer',
                    borderRadius: '4px',
                    fontWeight: 'bold',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '8px',
                    opacity: stepIndex === 0 ? 0.5 : 1,
                  }}
                >
                  <ArrowLeft size={14} />
                  PREVIOUS
                </button>

                <button
                  onClick={() => {
                    const nextIndex = stepIndex + 1;
                    if (nextIndex < steps.length) {
                      setCurrentStep(steps[nextIndex]);
                    }
                  }}
                  disabled={stepIndex === steps.length - 1}
                  style={{
                    backgroundColor: '#ea580c',
                    color: 'white',
                    border: 'none',
                    padding: '8px 16px',
                    fontSize: '11px',
                    cursor: stepIndex === steps.length - 1 ? 'not-allowed' : 'pointer',
                    borderRadius: '4px',
                    fontWeight: 'bold',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '8px',
                    opacity: stepIndex === steps.length - 1 ? 0.5 : 1,
                  }}
                >
                  NEXT
                  <ArrowRight size={14} />
                </button>
              </div>
            )}
          </>
        )}

        {view === 'list' && (
          <div style={{ padding: '24px', textAlign: 'center' }}>
            <h3 style={{ color: '#a3a3a3', marginBottom: '12px' }}>My Mappings</h3>
            <p style={{ color: '#737373', fontSize: '12px' }}>
              No saved mappings yet. Create your first mapping to get started.
            </p>
          </div>
        )}
      </div>
    </div>
  );
}
