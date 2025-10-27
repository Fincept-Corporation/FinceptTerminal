// Data Mapping System - Main exports

// Main component
export { default as DataMappingTab } from './DataMappingTab';

// Types
export * from './types';

// Schemas
export * from './schemas';

// Parsers
export * from './engine/parsers';

// Engine
export { MappingEngine, mappingEngine } from './engine/MappingEngine';

// Services
export { apiClient } from './services/APIClient';
export { mappingDatabase } from './services/MappingDatabase';
export { encryptionService } from './services/EncryptionService';

// Components
export { JSONExplorer } from './components/JSONExplorer';
export { ExpressionBuilder } from './components/ExpressionBuilder';
export { LivePreview } from './components/LivePreview';
export { SchemaSelector } from './components/SchemaSelector';
export { TemplateLibrary } from './components/TemplateLibrary';

// Utilities
export * from './utils/transformFunctions';
