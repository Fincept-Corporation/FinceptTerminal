// Schema Selector - Choose target schema

import React from 'react';
import { Database, TrendingUp, ShoppingCart, Briefcase, FileText } from 'lucide-react';
import { UNIFIED_SCHEMAS, getSchemasByCategory } from '../schemas';
import { UnifiedSchema } from '../types';

interface SchemaSelectorProps {
  selectedSchema?: string;
  onSelect: (schemaName: string) => void;
}

export function SchemaSelector({ selectedSchema, onSelect }: SchemaSelectorProps) {
  const categories = ['market-data', 'trading', 'portfolio', 'reference', 'custom'] as const;

  const getCategoryIcon = (category: typeof categories[number]) => {
    switch (category) {
      case 'market-data':
        return <TrendingUp size={16} />;
      case 'trading':
        return <ShoppingCart size={16} />;
      case 'portfolio':
        return <Briefcase size={16} />;
      case 'reference':
        return <FileText size={16} />;
      default:
        return <Database size={16} />;
    }
  };

  const getCategoryColor = (category: typeof categories[number]) => {
    switch (category) {
      case 'market-data':
        return 'text-blue-500 border-blue-700 bg-blue-900/20';
      case 'trading':
        return 'text-green-500 border-green-700 bg-green-900/20';
      case 'portfolio':
        return 'text-purple-500 border-purple-700 bg-purple-900/20';
      case 'reference':
        return 'text-yellow-500 border-yellow-700 bg-yellow-900/20';
      default:
        return 'text-gray-500 border-gray-700 bg-gray-900/20';
    }
  };

  return (
    <div className="space-y-6">
      <div>
        <h3 className="text-xs font-bold text-orange-500 mb-2 uppercase">Select Target Schema</h3>
        <p className="text-xs text-gray-500">
          Choose the standardized format that your source data will be mapped to
        </p>
      </div>

      {categories.map((category) => {
        const schemas = getSchemasByCategory(category);
        if (schemas.length === 0) return null;

        return (
          <div key={category}>
            <div className="flex items-center gap-2 mb-3">
              {getCategoryIcon(category)}
              <h4 className="text-xs font-bold text-gray-400 uppercase">
                {category.replace('-', ' ')}
              </h4>
            </div>

            <div className="grid grid-cols-1 gap-3">
              {schemas.map((schema) => {
                const schemaName = Object.keys(UNIFIED_SCHEMAS).find(
                  (key) => UNIFIED_SCHEMAS[key] === schema
                );
                const isSelected = selectedSchema === schemaName;

                return (
                  <button
                    key={schemaName}
                    onClick={() => onSelect(schemaName!)}
                    className={`text-left p-4 rounded-lg border transition-all ${
                      isSelected
                        ? 'border-orange-500 bg-orange-900/20 shadow-lg'
                        : 'border-zinc-700 bg-zinc-900 hover:border-zinc-600'
                    }`}
                  >
                    <div className="flex items-start justify-between mb-2">
                      <div>
                        <h5 className="text-sm font-bold text-white mb-1">{schema.name}</h5>
                        <p className="text-xs text-gray-400">{schema.description}</p>
                      </div>
                      {isSelected && (
                        <span className="text-xs bg-orange-600 text-white px-2 py-1 rounded font-bold">
                          SELECTED
                        </span>
                      )}
                    </div>

                    {/* Field Preview */}
                    <div className="mt-3 pt-3 border-t border-zinc-800">
                      <div className="text-xs text-gray-500 mb-2">
                        {Object.keys(schema.fields).length} Fields:
                      </div>
                      <div className="flex flex-wrap gap-2">
                        {Object.entries(schema.fields)
                          .slice(0, 8)
                          .map(([fieldName, fieldSpec]) => (
                            <span
                              key={fieldName}
                              className={`text-xs px-2 py-1 rounded font-mono ${
                                fieldSpec.required
                                  ? 'bg-orange-900/30 text-orange-400'
                                  : 'bg-zinc-800 text-gray-400'
                              }`}
                            >
                              {fieldName}
                              {fieldSpec.required && '*'}
                            </span>
                          ))}
                        {Object.keys(schema.fields).length > 8 && (
                          <span className="text-xs text-gray-600">
                            +{Object.keys(schema.fields).length - 8} more
                          </span>
                        )}
                      </div>
                    </div>
                  </button>
                );
              })}
            </div>
          </div>
        );
      })}

      {/* Selected Schema Details */}
      {selectedSchema && UNIFIED_SCHEMAS[selectedSchema] && (
        <div className="bg-zinc-900 rounded-lg border border-orange-700 p-4">
          <h4 className="text-xs font-bold text-orange-500 mb-3 uppercase">Schema Details</h4>

          <div className="space-y-2">
            {Object.entries(UNIFIED_SCHEMAS[selectedSchema].fields).map(([fieldName, fieldSpec]) => (
              <div
                key={fieldName}
                className="flex items-start justify-between p-2 bg-zinc-800 rounded"
              >
                <div className="flex-1">
                  <div className="flex items-center gap-2">
                    <span className="text-xs font-bold text-white font-mono">{fieldName}</span>
                    {fieldSpec.required && (
                      <span className="text-xs text-orange-500 font-bold">*required</span>
                    )}
                  </div>
                  <p className="text-xs text-gray-400 mt-1">{fieldSpec.description}</p>
                </div>
                <span className="text-xs text-gray-500 font-mono ml-4">{fieldSpec.type}</span>
              </div>
            ))}
          </div>
        </div>
      )}
    </div>
  );
}
