/**
 * DataMappingTab - State, Reducer, and Constants
 */

import React from 'react';
import { Network, Layers, GitMerge, Zap, Play } from 'lucide-react';
import { v4 as uuidv4 } from 'uuid';
import {
  DataMappingConfig,
  APIConfig,
  CreateStep,
  UnifiedSchemaType,
  CustomField,
  FieldMapping,
  MappingExecutionResult,
} from './types';

export type View = 'list' | 'create' | 'templates';

// State management
export interface DataMappingState {
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

export type DataMappingAction =
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

export const defaultApiConfig: APIConfig = {
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

export const initialState: DataMappingState = {
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

export function dataMappingReducer(state: DataMappingState, action: DataMappingAction): DataMappingState {
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

export const steps: CreateStep[] = ['api-config', 'schema-select', 'field-mapping', 'cache-settings', 'test-save'];

export const stepMeta: Record<CreateStep, { label: string; shortLabel: string; icon: React.ComponentType<{ size?: number; color?: string }> }> = {
  'api-config':    { label: 'API Config',    shortLabel: 'API',     icon: Network },
  'schema-select': { label: 'Schema',        shortLabel: 'SCHEMA',  icon: Layers },
  'field-mapping': { label: 'Field Mapping', shortLabel: 'MAPPING', icon: GitMerge },
  'cache-settings':{ label: 'Cache',         shortLabel: 'CACHE',   icon: Zap },
  'test-save':     { label: 'Test & Save',   shortLabel: 'TEST',    icon: Play },
};
