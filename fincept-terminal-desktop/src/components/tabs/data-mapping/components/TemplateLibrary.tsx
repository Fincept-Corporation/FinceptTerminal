// Template Library - Pre-built mapping templates

import React, { useState } from 'react';
import { Star, CheckCircle, Copy, Eye } from 'lucide-react';
import { MappingTemplate } from '../types';
import { INDIAN_BROKER_TEMPLATES } from '../templates/indian-brokers';

interface TemplateLibraryProps {
  onSelectTemplate: (template: MappingTemplate) => void;
}

export function TemplateLibrary({ onSelectTemplate }: TemplateLibraryProps) {
  const [selectedTemplate, setSelectedTemplate] = useState<MappingTemplate | null>(null);
  const [searchTerm, setSearchTerm] = useState('');
  const [categoryFilter, setCategoryFilter] = useState<string>('all');

  const allTemplates = [...INDIAN_BROKER_TEMPLATES];

  const filteredTemplates = allTemplates.filter((template) => {
    const matchesSearch =
      template.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
      template.description.toLowerCase().includes(searchTerm.toLowerCase()) ||
      template.tags.some((tag) => tag.toLowerCase().includes(searchTerm.toLowerCase()));

    const matchesCategory = categoryFilter === 'all' || template.tags.includes(categoryFilter);

    return matchesSearch && matchesCategory;
  });

  // Get unique tags for filtering
  const allTags = Array.from(new Set(allTemplates.flatMap((t) => t.tags)));
  const brokerTags = allTags.filter((tag) =>
    ['upstox', 'fyers', 'dhan', 'zerodha', 'angelone', 'shipnext'].includes(tag)
  );

  const handleUseTemplate = (template: MappingTemplate) => {
    onSelectTemplate(template);
  };

  return (
    <div className="space-y-4">
      {/* Header */}
      <div>
        <h3 className="text-xs font-bold text-orange-500 uppercase mb-2">Template Library</h3>
        <p className="text-xs text-gray-500">Pre-built mappings for popular brokers and data sources</p>
      </div>

      {/* Search & Filter */}
      <div className="flex gap-3">
        <input
          type="text"
          placeholder="Search templates..."
          value={searchTerm}
          onChange={(e) => setSearchTerm(e.target.value)}
          className="flex-1 bg-zinc-900 border border-zinc-700 text-gray-300 px-3 py-2 text-xs rounded focus:outline-none focus:border-orange-500"
        />

        <select
          value={categoryFilter}
          onChange={(e) => setCategoryFilter(e.target.value)}
          className="bg-zinc-900 border border-zinc-700 text-gray-300 px-3 py-2 text-xs rounded focus:outline-none focus:border-orange-500"
        >
          <option value="all">All Brokers</option>
          {brokerTags.map((tag) => (
            <option key={tag} value={tag}>
              {tag.toUpperCase()}
            </option>
          ))}
        </select>
      </div>

      {/* Templates Grid */}
      <div className="grid grid-cols-1 gap-3 max-h-[600px] overflow-auto pr-2">
        {filteredTemplates.map((template) => (
          <div
            key={template.id}
            className={`bg-zinc-900 border rounded-lg p-4 cursor-pointer transition-all ${
              selectedTemplate?.id === template.id
                ? 'border-orange-500 bg-orange-900/10'
                : 'border-zinc-700 hover:border-zinc-600'
            }`}
            onClick={() => setSelectedTemplate(template)}
          >
            {/* Header */}
            <div className="flex items-start justify-between mb-3">
              <div className="flex-1">
                <div className="flex items-center gap-2 mb-1">
                  <h4 className="text-sm font-bold text-white">{template.name}</h4>
                  {template.verified && (
                    <CheckCircle size={14} className="text-green-500" aria-label="Verified" />
                  )}
                  {template.official && <Star size={14} className="text-yellow-500" aria-label="Official" />}
                </div>
                <p className="text-xs text-gray-400">{template.description}</p>
              </div>
            </div>

            {/* Tags */}
            <div className="flex flex-wrap gap-2 mb-3">
              {template.tags.map((tag) => (
                <span
                  key={tag}
                  className="text-xs px-2 py-1 bg-zinc-800 text-gray-400 rounded font-mono uppercase"
                >
                  {tag}
                </span>
              ))}
            </div>

            {/* Stats */}
            <div className="flex items-center gap-4 text-xs text-gray-500 mb-3">
              {template.usageCount !== undefined && (
                <div>
                  <Copy size={12} className="inline mr-1" />
                  {template.usageCount} uses
                </div>
              )}
              {template.rating !== undefined && (
                <div>
                  <Star size={12} className="inline mr-1 text-yellow-500" />
                  {template.rating}/5
                </div>
              )}
            </div>

            {/* Actions */}
            <div className="flex gap-2">
              <button
                onClick={(e) => {
                  e.stopPropagation();
                  setSelectedTemplate(template);
                }}
                className="flex-1 bg-zinc-800 hover:bg-zinc-700 text-gray-300 py-2 px-3 rounded text-xs font-bold flex items-center justify-center gap-2"
              >
                <Eye size={12} />
                PREVIEW
              </button>
              <button
                onClick={(e) => {
                  e.stopPropagation();
                  handleUseTemplate(template);
                }}
                className="flex-1 bg-orange-600 hover:bg-orange-700 text-white py-2 px-3 rounded text-xs font-bold"
              >
                USE TEMPLATE
              </button>
            </div>
          </div>
        ))}

        {filteredTemplates.length === 0 && (
          <div className="text-center py-12 text-gray-500">
            <p className="text-sm">No templates found</p>
            <p className="text-xs mt-2">Try adjusting your search or filter</p>
          </div>
        )}
      </div>

      {/* Template Preview */}
      {selectedTemplate && (
        <div className="bg-zinc-900 border border-orange-700 rounded-lg p-4">
          <div className="flex items-center justify-between mb-3">
            <h4 className="text-xs font-bold text-orange-500 uppercase">Template Details</h4>
            <button
              onClick={() => setSelectedTemplate(null)}
              className="text-xs text-gray-400 hover:text-white"
            >
              Close
            </button>
          </div>

          {/* Instructions */}
          {selectedTemplate.instructions && (
            <div className="mb-4">
              <div className="text-xs font-bold text-gray-400 mb-2">USAGE INSTRUCTIONS</div>
              <pre className="text-xs text-gray-400 whitespace-pre-wrap bg-zinc-800 p-3 rounded">
                {selectedTemplate.instructions}
              </pre>
            </div>
          )}

          {/* Field Mappings Preview */}
          {selectedTemplate.mappingConfig.fieldMappings && (
            <div>
              <div className="text-xs font-bold text-gray-400 mb-2">FIELD MAPPINGS</div>
              <div className="space-y-2">
                {selectedTemplate.mappingConfig.fieldMappings.map((mapping, idx) => (
                  <div key={idx} className="bg-zinc-800 p-2 rounded text-xs">
                    <div className="flex items-center justify-between">
                      <span className="text-orange-400 font-mono">{mapping.targetField}</span>
                      <span className="text-gray-500">←</span>
                      <span className="text-green-400 font-mono flex-1 text-right truncate">
                        {mapping.sourceExpression}
                      </span>
                    </div>
                    {mapping.transform && (
                      <div className="text-purple-400 mt-1">
                        Transform: {mapping.transform}
                      </div>
                    )}
                  </div>
                ))}
              </div>
            </div>
          )}

          {/* Use Button */}
          <button
            onClick={() => handleUseTemplate(selectedTemplate)}
            className="w-full mt-4 bg-orange-600 hover:bg-orange-700 text-white py-3 rounded font-bold text-xs"
          >
            USE THIS TEMPLATE
          </button>
        </div>
      )}
    </div>
  );
}
